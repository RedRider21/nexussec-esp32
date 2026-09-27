# NexusSec ESP32 — Manuale

Guida completa al gadget **NexusSec ESP32** (scheda **ESP32-DIV V2**): uso del
dispositivo, ogni modulo, come flashare, sviluppo e integrazione con NexusSec OS.

> **Stato**: firmware **v0.1.0 beta**. Tutto **compila** (simulatore + scheda
> reale) ma **non è ancora stato testato su hardware fisico**. Le parti da
> validare sono segnalate con ⚠️.

---

## 1. Cos'è
Un multi-tool wireless tascabile in stile Flipper, basato su **ESP32-S3**. Non è
un computer con Linux: è un microcontrollore con **più radio a bordo** e un
menu a "profili" su display touch. Fa parte dell'ecosistema **NexusSec**: la
distro NexusSec OS lo **flesha** e ne **importa** i dati (wardriving→HORUS,
handshake→loot).

**Può fare**: scan WiFi/BLE, wardriving, 2,4 GHz (NRF24), sub-GHz (CC1101), IR,
BadUSB/HID, log su microSD. **Non può**: WiFi 5 GHz, monitor-mode completo come
una scheda vera, cracking pesante (quello lo fa il PC).

---

## 2. Hardware (ESP32-DIV V2)
- **MCU**: ESP32-S3, 16 MB flash, USB-C nativa (serve per BadUSB), BLE5.
- **Display**: TFT touch **ILI9341 2,8"**, **240×320** (verticale nativo).
- **Radio**: WiFi + BLE · **3× NRF24L01** (2,4 GHz) · **CC1101** (sub-GHz) · **IR**.
- **Altro**: microSD, 3 tasti fisici (su espansore **PCF8574**), LED RGB, IP5306
  (batteria), connettori SMA. Formato ~53×100 mm.

### Mappa pin (dai sorgenti CiferTech, ramo V2) — in `firmware/src/board_pins.h`
| Blocco | Pin |
|---|---|
| Display (bus SPI dedicato) | MISO 37 · MOSI 35 · SCLK 36 · CS 17 · DC 16 · RST 0 · TOUCH_CS 18 |
| Bus SPI radio+SD | SCK 12 · MISO 13 · MOSI 11 |
| microSD | CS 10 |
| CC1101 | CS 5 · GDO0 6 · GDO2 3 |
| NRF24 (×3) | CE/CSN 15/4 · 47/48 · **14/21** (lo scanner usa 14/21) |
| IR | RX 21 · TX 14 (condivisi col NRF24 mod.3: uso alternato) |
| GPS (UART2, opz.) | RX 5 · TX 6 (condivisi con CC1101: uso alternato) |
| Tasti | su **PCF8574** I2C (0x20–0x27). ⚠️ SDA/SCL V2 **da confermare** |

---

## 3. Uso del dispositivo

### Avvio
All'accensione: **self-test** dei moduli → **avviso legale** (uso solo su target
autorizzati) → si conferma con **OK**. Poi appare il **menu a profili**.

### Navigazione
- **Touch** (sempre attivo): tocca il display — a **zone** orizzontali =
  sinistra **◄ (indietro)**, centro **● (OK)**, destra **► (avanti)**.
- **Tasti fisici**: ◄ indietro · ● OK · ► avanti.
  - Sul **simulatore** sono GPIO 4/5/6; sulla **scheda reale** sono sul PCF8574.
- Nei moduli: **OK** = azione (ripeti scan/esegui) · **◄** = torna al menu ·
  **►** = azione secondaria dove prevista.

### Orientamento
In **Impostazioni** → OK commuta **verticale/orizzontale**; la scelta è salvata
in memoria (NVS) e il layout si adatta da solo.

---

## 4. I moduli (profili)

> Legenda: ✅ implementato e compilato · 🔶 a menu ma "in sviluppo".

### 📡 Recon WiFi ✅
Scan degli access point 2,4 GHz: SSID, canale, cifratura, RSSI. Le reti **aperte**
sono evidenziate. **►** avvia il **Wardriving**.

### 🛰️ Wardriving ✅
Scansione continua (~15 s/sessione): conta le reti (dedup per BSSID) e le salva su
microSD in **`/WARDRIVE.csv`**. Se è collegato un **GPS** (UART2), aggiunge lat/lon.
Formato CSV (per import in HORUS):
```
BSSID,SSID,Enc,Channel,RSSI,Lat,Lon,Timestamp
```
Le righe con `0.0,0.0` = nessun fix GPS.

### 🎯 Attacco WiFi 🔶
Deauth / beacon-spam / rogue-AP / Evil Portal. **A menu ma non ancora
implementato** nel firmware (richiede TX 802.11 raw dedicato). Da fare.

### 🔑 Handshake / PMKID 🔶
Cattura del 4-way handshake / PMKID → file hashcat 22000 su SD. **Da fare.**

### 🦷 Bluetooth / BLE ✅
Scan dei dispositivi BLE vicini (nome, indirizzo, RSSI).

### 📶 NRF24 · 2,4 GHz ✅
Analisi dello spettro 2,4 GHz coi NRF24: attività per canale (0–63) a barre.
⚠️ Dipende dalla presenza del modulo NRF24 sui pin 14/21.

### 📻 Sub-GHz · CC1101 ✅
Rileva il CC1101, imposta 433.92 MHz, misura **RSSI** (medio/max). Base per
cattura/replay (da estendere).

### 🔴 Infrarossi ✅
Attende un codice da telecomando (~5 s) e mostra **protocollo/codice/bit** (NEC,
RC5, Sony, ecc.).

### ⌨️ BadUSB / HID ✅
Esegue script **DuckyScript** (subset) come tastiera HID via USB nativa S3.
- Payload da microSD in **`/payloads/`** (uno per file). **►** scorre, **OK** esegue.
- Se non ci sono payload: **demo integrata** (digita una riga di testo innocua).
- Comandi supportati: `REM`, `STRING`, `DELAY`, `ENTER`, `TAB`, `GUI/CTRL/ALT/SHIFT`
  + combinazioni (es. `GUI r`), tasti freccia, `DELETE`, `BACKSPACE`, `ESC`, `HOME`, `END`.
- ⚠️ Layout tastiera **US** di default (mappa **IT**: da fare). Solo su PC autorizzati.

### 💾 Loot / microSD ✅
Info scheda (capacità/uso) e lista dei file: wardriving, catture, payload.

### ⚙️ Impostazioni ✅
Orientamento (OK per commutare), versione firmware.

---

## 5. Flashare il firmware

### A. Dal browser (più semplice)
Apri **[flash.html](https://redrider21.github.io/nexussec-esp32/flash.html)** con
**Chrome** o **Edge** su **desktop** (serve Web Serial). Collega la scheda via
USB-C e premi:
- **⚡ Flash NexusSec** → installa il nostro firmware (full).
- **↺ Ripristina originale** → rimette il firmware **ESP32-DIV** di CiferTech.

Se la scheda non viene rilevata, tieni premuto **BOOT** mentre la colleghi.

### B. Da riga di comando (esptool)
```bash
# NexusSec (immagine merged, offset 0x0)
esptool.py --chip esp32s3 write_flash 0x0 docs/flash/bin/nexussec-esp32-full.bin
# ripristino originale
esptool.py --chip esp32s3 write_flash 0x0 docs/flash/bin/esp32-div-v2-original-merged.bin
```

### C. Da PlatformIO (per sviluppatori)
```bash
cd firmware && pio run -t upload
```

### D. Dalla distro NexusSec OS
Tool **`nxs-esp32`** (in sviluppo): flash con profilo + import dati. Vedi
`docs/nxs-esp32-integration.md`.

---

## 6. Simulatore (senza scheda)
1. VS Code + **PlatformIO** + estensione **Wokwi**.
2. `cd firmware && pio run`
3. Apri `firmware/diagram.json` → **Play**. Tasti PREV/OK/NEXT nel simulatore.

Il mockup grafico dell'UI è anche navigabile in `docs/ui-mockup.html`
(modalità solo-dispositivo: `#kiosk:<schermata>`, es. `#kiosk:home`).

---

## 7. Sviluppo

### Ambienti PlatformIO
- `esp32-div-v2` — **simulatore** (tasti GPIO 4/5/6, `NXS_TARGET_SIM=1`).
- `esp32-div-v2-hw` — **scheda reale** (tasti PCF8574, `NXS_TARGET_SIM=0`).

I pin del display sono nelle `-D` di `platformio.ini`; tutti gli altri in
`src/board_pins.h`.

### Aggiungere un modulo
1. Scrivi `void moduleXxx()` (disegna con `tft`, usa `statusBar()`/`footerBar()`).
2. Collega il profilo in `openModule()` e nel gestore **OK** dentro `loop()`.
3. `pio run` per verificare.

### Struttura schermo
`statusBar()` in alto, corpo al centro, `footerBar()` in basso; le dimensioni
`W`/`H` si aggiornano con l'orientamento (`applyRotation()`).

---

## 8. Sicurezza, etica, legale
Le funzioni offensive (deauth, rogue AP, BadUSB, jamming) e la cattura di traffico
vanno usate **solo su reti e dispositivi di cui hai il permesso** (test/lab).
L'avviso all'avvio è obbligatorio. Il firmware originale in `firmware/vendor/` è di
**CiferTech** (licenza **MIT**), incluso solo per il ripristino.

---

## 9. Cosa manca / roadmap
- ⚠️ **Validazione su hardware reale** (tutto è solo compilato).
- Confermare **SDA/SCL** del PCF8574 (tasti reali) e **calibrare il touch**.
- Implementare **Attacco WiFi** e **Handshake/PMKID** (TX 802.11 raw).
- Estendere **CC1101** (cattura/replay) e **IR** (invio/DB), salvataggio su SD.
- **BadUSB**: layout tastiera **IT**.
- **Build per-profilo** (oltre alla full) per il flasher.
- **Partition table 16 MB** dedicata (app grande + OTA + dati).
- Tool distro **`nxs-esp32`** (flash + import in HORUS/loot).
- Allineare `diagram.json` Wokwi ai pin display reali per simulare col pinout vero.

---

## 10. Riferimenti
- Sito: https://redrider21.github.io/nexussec-esp32/
- Firmware: `firmware/` · Pin: `firmware/src/board_pins.h`
- Integrazione distro: `docs/nxs-esp32-integration.md`
- Scheda originale: https://github.com/cifertech/ESP32-DIV (MIT)
