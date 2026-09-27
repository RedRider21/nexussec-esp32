# NexusSec ESP32 — handoff sessione UI (2026-09-26)

Ripartenza per la prossima sessione dedicata alle **interfacce grafiche del firmware**.
Apri una sessione in questa cartella e parti da qui.

## Cosa è stato fatto in questa sessione
- Creato **`docs/ui-mockup.html`**: simulatore **interattivo** delle interfacce
  firmware, da usare come riferimento visivo per lo sviluppo sull'hardware.
  - Target: **ESP32-DIV V2** (ESP32-S3, display **touch ILI9341 320×240**, 3×NRF24,
    CC1101, IR, microSD, IP5306, 4 LED RGB WS2812B, buzzer).
  - Stile NexusSec: flat, **accent cyan su fondo scuro**, testi in italiano.
  - Font: Chakra Petch (titoli), IBM Plex Sans (corpo), Share Tech Mono (schermo).
  - Schermo reso a risoluzione reale **320×240** (landscape), con cornice
    dispositivo, 4 LED RGB contestuali (rosso = modalità offensiva) e tasti
    fisici ◄ ● ►.
  - Navigazione: touch sulle voci/riquadri, tasti fisici, frecce tastiera + Esc,
    toggle Tema (chiaro/scuro per la pagina; lo schermo resta scuro).
- **13 schermate** implementate:
  1. `boot` — splash + self-test moduli + **avviso legale** (Accetto)
  2. `home` — menu a 10 profili (griglia)
  3. `wifi_recon` — scan AP (SSID/ch/enc/RSSI, reti aperte in ambra)
  4. `wardriving` — contatori + mini-mappa + log CSV **→ HORUS**
  5. `wifi_attack` — deauth / beacon spam / probe flood / Rogue AP-Evil Portal + STOP
  6. `handshake` — EAPOL/PMKID → **loot** (hashcat 22000)
  7. `ble` — scan dispositivi BLE
  8. `nrf24` — spettro 2.4 GHz (3×NRF24) + rilevo mousejack
  9. `subghz` — CC1101 433.92 MHz OOK, cattura/replay
  10. `ir` — NEC, cattura/invio, DB su SD
  11. `badusb` — payload Ducky da SD, layout IT
  12. `loot` — file su microSD organizzati per import distro
  13. `settings` — preferenze + stato integrazione NexusSec
- **Pubblicato come Artifact** (privato): https://claude.ai/artifact/HiMedM1EVXPCnvkNmuA2To
- **Commit**: `f174e25` — "docs: mockup UI interattivo del firmware (ESP32-DIV V2, 320x240)".

## Hardware confermato dall'utente
- Scheda principale: **ESP32-DIV V2** (link Alibaba fornito:
  product-detail/ESP32DIV-V2-Development-Board-WiFi-NRF24_1601944762661.html).
- L'utente ha previsto anche un **"hardware basico"** (secondo target, più semplice).
  → Da prevedere una **variante UI "profilo base"**: display più piccolo / solo
    WiFi+BLE, senza NRF24/CC1101/IR; il menu profili dovrebbe adattarsi
    all'hardware rilevato (nascondere i moduli assenti).

## Nota operativa importante (classificatore)
- In **modalità auto** il controllo di sicurezza blocca le **scritture** per i
  contenuti di security tooling (deauth/BadUSB): per il resto della sessione.
- Soluzione: lavorare in **modalità permessi predefinita/manuale** (Shift+Tab per
  ciclare le modalità finché sparisce "auto"). In manuale la scrittura passa.

## Prossimi passi proposti (da decidere con l'utente)
1. **Iterare il design** del mockup: confermare orientamento (landscape vs
   portrait reale della scheda), icone bitmap definitive, palette, quali
   schermate aggiungere/semplificare.
2. Aggiungere la **variante "profilo base"** (hardware minimale).
3. Definire i **flussi/stati** dinamici (es. selezione target da recon → attacco;
   avvio/stop reali con animazioni).
4. Decidere **strada A vs B** per il firmware (vedi `docs/prossimi-passi.md`) e,
   se B, iniziare lo scaffold **Arduino-ESP32** riusando questo mockup come
   specifica delle schermate (menu a profili, avviso legale, layout status bar).
5. Lato distro: tool **`nxs-esp32`** (flash esptool + import HORUS/loot).

## Aggiornamento 2026-09-27 — logo + firmware avviato
- **Logo/brand DEFINITIVO**: emblema-chip NexusSec ESP32 (esagono + "N" a nodi
  derivati dal logo NexusSec OS `splash/emblem.png`, resi come **chip**: piedini
  su tutti i lati + segnale WiFi). Applicato a: mockup (header+boot, sorgente
  unica `EMBLEM_ESP`), sito Pages (nav + hero con **due anelli di puntini
  contro-rotanti**, chip piccolo/raffinato scale 0.72). Accent brand `#00e5ff`.
- **Sito Pages AGGIORNATO e PUSHATO** su `master` (Pages da /docs).
- **Firmware avviato (Strada B, Arduino-ESP32 + TFT_eSPI, PlatformIO)** in
  `firmware/`: boot+avviso legale, menu profili con **doppio orientamento**
  (NVS), **scan WiFi reale**; file **Wokwi** per provarlo nel browser senza
  scheda. Vedi `firmware/README.md`.
  - ⚠️ **NON ancora compilato/verificato** in questa sessione (qui manca
    PlatformIO/arduino-cli; c'è solo python3). Da fare: `pio run` o simulare in
    Wokwi. Trattare il codice come da collaudare.
- Doppio orientamento nel mockup: default **verticale** per ESP32-DIV V2;
  font più grande in verticale nelle sotto-schermate; griglia menu a pieno schermo.

## Aggiornamento 2026-09-27 (sera) — pinout reale + moduli radio
**PlatformIO installato** (`~/.local/bin/pio`, v6.2.0). Il firmware **compila
pulito** (`pio run -e esp32-div-v2`): ultimo build **1,35 MB**, RAM 20%, Flash
42% dello slot app di default (16 MB reali → ~8%).

**Moduli REALI ora nel firmware** (compilano): WiFi scan, BLE scan, **NRF24
spettro 2.4 GHz**, **IR cattura**. Boot+avviso, menu, doppio orientamento (NVS).

**PINOUT UFFICIALE ESP32-DIV V2** (ricavato dai sorgenti CiferTech, ramo
`BOARD_ESP32_DIV_V2` = `#else`), salvato in `firmware/src/board_pins.h`:
- **Display ILI9341** (bus SPI dedicato, nelle -D di platformio.ini):
  MISO 37, MOSI 35, SCLK 36, CS 17, DC 16, RST 0, TOUCH_CS 18 (no backlight pin).
- **Bus SPI radio+SD condiviso**: SCK 12, MISO 13, MOSI 11.
- **microSD**: CS 10.
- **CC1101**: CS 5, GDO0 6, GDO2 3 (bus 12/13/11).
- **NRF24 ×3**: CE/CSN = 15/4, 47/48, 14/21. Scanner usa modulo 3 (CE14/CSN21).
- **IR**: RX 21, TX 14 (⚠️ CONDIVISI col NRF24 mod.3 — uso alternato, ok).
- **Buzzer**: assente (-1). **Tasti**: su **PCF8574** (I2C, addr auto 0x20-0x27).
  ⚠️ **I2C SDA/SCL della V2 NON nei sorgenti** (Wire default) → DA CONFERMARE
  sullo schema prima di usare i tasti reali.

⚠️ **Non testato su hardware** (nessuna scheda): il codice compila ma va
validato. Il bus SPI display vs radio e i tasti PCF8574 sono i punti da collaudare.

### Da fare (prossima sessione)
1. **CC1101 sub-GHz** (lib SmartRC/ELECHOUSE già in Libraries del repo CiferTech):
   modulo RX/RSSI + replay, con `setSpiPin(12,13,11,5)` e GDO 6/3.
2. **microSD** (CS10, bus 12/13/11): salvataggio catture/log.
3. **Wardriving** (WiFi continuo + GPS UART → CSV per HORUS).
4. **Tasti PCF8574** reali (serve confermare SDA/SCL) + **touch** ILI9341 (TOUCH_CS 18).
5. **BadUSB/HID** (USB nativa S3).
6. Allineare `diagram.json` Wokwi ai pin display reali (35/36/37/17/16/0) se si
   vuole simulare col nuovo pinout; tasti sim su GPIO 4/5/6 (NXS_TARGET_SIM=1).
7. Partition table dedicata 16 MB (app grande + OTA + dati).

### Build / simulazione
- Compilare: `cd firmware && ~/.local/bin/pio run -e esp32-div-v2`
- Toolchain in `~/.platformio` (~2,4 GB, cancellabile). Disco al 93%: attenzione.

## Aggiornamento 2026-09-27 (3) — firmware completo, case in pausa, cleanup
**Firmware**: tutti i 10 profili implementati e compilanti (sim + hw). Aggiunti in
questa sessione: Attacco WiFi (deauth+beacon), Handshake/PMKID (→/loot/pmkid.22000),
layout tastiera **IT** per BadUSB. ⚠️ non testato su hardware.

**Case 3D**:
- Stampabile: `hardware/case/nexussec-esp32-case.scad` (+README) — quote reali
  (case 2KLAB 59×105,5×24, PCB 53,09×100,46), back+fascia a filo, 4 viti, logo inciso.
- **Rendering 3D interattivo** (Three.js): `docs/case-viewer.html` + `docs/case/*.glb/stl`
  + `hardware/case/build_case.py`. Peso ~36 g. Artifact privato `Kxd8twuhCa47Vuh73hfBp8`.
- ⚠️ **IN PAUSA**: rifinire con **misure esatte** + **fessure laterali** per le porte
  (USB-C, microSD, 4×SMA in alto, tasti) — guardare i case in vendita/community
  (Downlord MakerWorld, 2KLAB Cults3D, AliExpress).

**Git/GitHub (richiesta utente)**: pushato tutto TRANNE il **rendering 3D**. Restano
SOLO locali (untracked, non su GitHub): `docs/case-viewer.html`, `docs/case/`,
`hardware/case/build_case.py`.

**Cleanup**: rimosse trimesh/shapely/manifold3d (per rigenerare il GLB: reinstallarle).
Tenuto il compilatore PlatformIO (`~/.platformio` 2,4 GB) + numpy.

**Da fare prossima sessione**: misure case → .scad + fessure porte → rifinire viewer →
pubblicare su Pages il **viewer 3D** e il **mockup interattivo** (`ui-mockup.html`) →
tool distro `nxs-esp32`.

## File utili
- `docs/ui-mockup.html` — il mockup (aprire nel browser o via Artifact).
- `docs/prossimi-passi.md` — handoff generale + strade firmware A/B.
- `docs/hardware.md` — hardware e acquisto.
- `CLAUDE.md` — vincoli e stile.
