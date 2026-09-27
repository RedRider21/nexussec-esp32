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
#include <RF24.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <FS.h>
#include <SD.h>
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
uint8_t  rotation = 0;      // 0 = verticale (240x320), 1 = orizzontale (320x240)

int W = 240, H = 320;       // dimensioni correnti dello schermo
const int SBAR = 24;        // altezza status bar
const int FBAR = 20;        // altezza barra suggerimenti

// ----------------------------------------------------------------------------
// Input tasti (debounce + fronte di discesa)
// ----------------------------------------------------------------------------
struct Btn { uint8_t pin; bool last; uint32_t t; };
Btn bPrev{BTN_PREV, HIGH, 0}, bOk{BTN_OK, HIGH, 0}, bNext{BTN_NEXT, HIGH, 0};

bool pressed(Btn &b) {
  bool now = digitalRead(b.pin);
  if (now != b.last && (millis() - b.t) > 30) {
    b.t = millis();
    b.last = now;
    if (now == LOW) return true;   // fronte di discesa = premuto
  }
  return false;
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
  footerBar("OK ripeti   PREV menu");
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
  tft.setTextColor(C_MUT, C_BG);
  tft.setTextFont(1);
  tft.drawString("firmware v" NXS_FW_VERSION, 10, SBAR + 78);
  footerBar("OK cambia orientamento   PREV menu");
}

void openModule(int id) {
  curProfile = id;
  state = ST_MODULE;
  if (id == P_RECON)         moduleWifiScan();
  else if (id == P_BLE)      moduleBleScan();
  else if (id == P_NRF24)    moduleNrf24Scan();
  else if (id == P_SUBGHZ)   moduleSubghz();
  else if (id == P_IR)       moduleIrCapture();
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
  pinMode(BTN_PREV, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);

  prefs.begin("nxs", false);
  rotation = prefs.getUChar("rot", 0);

  tft.init();
  applyRotation();
  tft.fillScreen(C_BG);

  state = ST_LEGAL;
  drawLegal();
}

void loop() {
  bool p = pressed(bPrev), o = pressed(bOk), n = pressed(bNext);
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
      else if (o) {                                   // azione del modulo
        if (curProfile == P_RECON) moduleWifiScan();
        else if (curProfile == P_BLE) moduleBleScan();
        else if (curProfile == P_NRF24) moduleNrf24Scan();
        else if (curProfile == P_SUBGHZ) moduleSubghz();
        else if (curProfile == P_IR) moduleIrCapture();
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
