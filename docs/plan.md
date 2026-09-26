# NexusSec ESP32 — piano di lavoro

> ⚠️ Aggiornato: la scheda scelta è la **ESP32-DIV V2** (con NRF24/CC1101/IR).
> Per l'avvio della prossima sessione usa **`docs/prossimi-passi.md`** (questo
> file resta come storico del piano iniziale).

Handoff per la sessione dedicata. Decisione utente (2026-09-14): **Opzione B**
(firmware nostro), progetto a parte **collegato** a NexusSec.

## Fasi
1. **Base firmware + menu**: scelta board (ESP32‑S3), toolchain (Arduino‑ESP32 o
   ESP‑IDF), driver display + input, **menu a profili** minimale (cyan/scuro),
   avviso legale all'avvio.
2. **Ricognizione WiFi**: scan AP/client, canali, RSSI; **wardriving** con GPS
   (UART) + salvataggio su **microSD** (CSV compatibile HORUS: BSSID, SSID, enc,
   lat/lon, ts).
3. **Handshake/PMKID**: cattura → file (PCAP/hashcat 22000) su microSD per il crack
   sul PC.
4. **WiFi offensivo (autorizzato)**: deauth, beacon/probe spam, rogue AP / **Evil
   Portal** (captive).
5. **BLE**: scan, spam advertising, sniff.
6. **BadUSB/HID** (solo ESP32‑S2/S3): esecuzione script tipo Ducky; editor payload
   lato distro.
7. **Integrazione distro**: definire il formato di output e il tool lato NexusSec
   (`nxs-esp32`: flash via esptool + import in HORUS/loot).
8. **Confezionamento**: istruzioni build/flash, immagini firmware rilasciabili,
   eventuale case 3D.

## Decisioni prese
- Firmware **nostro** (non Marauder/Bruce), brandizzato NexusSec.
- Target primario **ESP32‑S3 + PSRAM** (per BadUSB e BLE5).
- Solo WiFi 2.4 GHz + BLE (sub‑GHz eventuale solo con CC1101 esterno).
- Output pensato per l'import nella distro (HORUS = mappa wardriving, loot = handshake).

## Da decidere (prossima sessione)
- Board/devkit definitivo (T‑Embed vs T‑Display‑S3 vs M5Stack vs custom).
- Arduino‑ESP32 vs ESP‑IDF.
- Formato preciso dei log per HORUS.

## Riferimenti
- Ecosistema: `../NexusSec-OS/` (distro + HORUS), memoria `project_nexussec_esp32`.
- Progetti gemelli per stile/handoff: Vesper (`../vesper/docs/porting-plan.md`),
  app Android Termux‑NexusSEC‑OS.
