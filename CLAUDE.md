# NexusSec ESP32 — istruzioni per Claude

Firmware **nostro** (Opzione B) per gadget **ESP32**, corollario di NexusSec OS
(`../NexusSec-OS/`). Progetto autonomo ma pensato per **integrarsi** con la distro
(flash + import dei risultati in HORUS/loot).

## Lingua e stile
- Contenuti user-visible (menu su schermo, README, log) e commenti in **italiano**
  con accenti corretti.
- UI minimale/flat, accent **cyan** su fondo scuro (coerente col brand NexusSec).

## Hardware di riferimento
- **ESP32‑S3** con PSRAM (dual‑core, **USB nativa** → BadUSB/HID; BLE5).
- Display TFT (ST7789/ILI9341) o OLED (SSD1306); input encoder/tasti o touch.
- Opzionali: microSD (log), GPS UART (wardriving), antenna esterna u.FL, LiPo.
- Il classic ESP32 = WiFi+BT ma **niente BadUSB**; l'ESP32‑S2 = USB ma **niente BT**.

## Stack e vincoli
- **Arduino‑ESP32** o **ESP‑IDF** (C/C++). Preferire librerie mantenute.
- WiFi 2.4 GHz + BLE. **Niente** 5 GHz, **niente** sub‑GHz (a meno di CC1101 esterno,
  es. board T‑Embed CC1101: in quel caso è un modulo separato con la sua antenna).
- Firmware modulare a "profili" (menu). Output (wardriving/handshake) in formato
  pensato per l'**import nella distro** (CSV/PCAP/JSON su seriale o microSD).

## Integrazione con NexusSec
- Flash dalla distro con `esptool` (tool `nxs-esp32`/`vesper-esp32`, da fare lato distro).
- Wardriving → mappa **HORUS**; PMKID/handshake → **loot** per il cracking.

## Etica/legalità
- deauth, rogue AP, Evil Portal, BadUSB: **solo su reti/macchine autorizzate**
  (pentest/lab). Inserire un avviso all'avvio del firmware.

## Cosa NON fare
- Non promettere funzioni impossibili sull'hardware (5 GHz, sub‑GHz senza CC1101,
  cracking a bordo, monitor‑mode completo come una scheda vera).
- Non copiare 1:1 firmware di terzi (facciamo il nostro); ispirarsi è ok, rispettare
  le licenze.
