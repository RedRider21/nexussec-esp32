# NexusSec ESP32 — firmware (Opzione B, nostro)

Firmware **nostro** per la scheda **ESP32-DIV V2** (ESP32-S3 + display ILI9341
320×240). Scaffold v0.1.0: boot con avviso legale, menu a "profili" con **doppio
orientamento**, e primo modulo **reale** (scan WiFi). Gli altri profili sono a
menu come "in sviluppo".

> Non è un sistema operativo: è un **unico programma** nella flash. Tutte le
> funzioni sono **a bordo** (compilate nel firmware), non installate a runtime
> come nella distro NexusSec OS. Aggiornare = **riflashare** l'immagine.

## Struttura
```
firmware/
├── platformio.ini   # ambiente ESP32-S3 + TFT_eSPI (config display via -D flags)
├── src/main.cpp     # boot/avviso, menu profili, orientamento, scan WiFi
├── wokwi.toml       # simulatore Wokwi
└── diagram.json     # schema simulatore: ESP32-S3 + ILI9341 + 3 tasti
```

## Provarlo SENZA scheda (simulatore Wokwi)
1. Installa **PlatformIO** (VS Code) e l'estensione **Wokwi Simulator**.
2. In questa cartella: PlatformIO → **Build** (ambiente `esp32-div-v2`).
3. Apri `diagram.json` e premi **Play**: parte l'ESP32 virtuale col display.
   - Tasti **PREV / OK / NEXT** = navigazione menu.
   - Nel simulatore il WiFi mostra la rete virtuale di Wokwi (prova reale della
     UI e del flusso di scan).

In alternativa: carica la cartella su <https://wokwi.com> (progetto PlatformIO).

## Compilare / flashare su scheda REALE
```bash
pio run                      # compila
pio run -t upload            # flash via USB-C (esptool, incluso in PlatformIO)
pio device monitor           # log seriale 115200
```
> **Pin display**: quelli in `platformio.ini` (`-D TFT_*`) sono i pin del
> **simulatore**. Sulla ESP32-DIV V2 reale vanno messi i **pin veri** del suo
> schema (si cambiano SOLO lì, senza toccare il codice).

## Input
3 tasti fisici (attivi a massa, `INPUT_PULLUP`): `PREV=GPIO4`, `OK=GPIO5`,
`NEXT=GPIO6`. Il **touch** della scheda reale sarà aggiunto come input alternativo.

## Roadmap firmware
- [x] Boot + avviso legale + menu a profili (doppio orientamento, salvato in NVS)
- [x] Modulo reale: **scan WiFi**
- [ ] Wardriving (GPS + CSV su microSD → HORUS)
- [ ] Handshake/PMKID (hashcat 22000 → loot)
- [ ] BLE scan/spam
- [ ] NRF24 (spettro 2.4 GHz), CC1101 (sub-GHz RX/replay), IR (RX/TX)
- [ ] BadUSB/HID (USB nativa ESP32-S3)
- [ ] Touch come input; icone bitmap definitive
- [ ] Tool distro `nxs-esp32` (flash via esptool + import HORUS/loot)

## Legale
Uso consentito **solo su reti e dispositivi autorizzati** (pentest/lab). L'avviso
all'avvio è obbligatorio e va confermato prima di usare i moduli.
