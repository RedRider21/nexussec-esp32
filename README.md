# NexusSec ESP32

Gadget hardware di **ricognizione wireless** basato su **ESP32**, **corollario**
dell'ecosistema **NexusSec** (distro NexusSec OS + app Android Termux-NexusSEC-OS
+ DE Vesper). Progetto **a sé ma collegato**: firmware **nostro** (non adottiamo
Marauder/Bruce), brandizzato e integrato con la distro.

> Stato: **scaffold / da avviare**. Qui c'è il piano; il firmware va sviluppato.

## Cos'è (e cosa non è)
L'ESP32 è un microcontrollore: **non** esegue Linux/una distro. Ha però **WiFi
2.4 GHz + BLE** integrati, quindi è perfetto come **piccolo drone di ricognizione**
che la distro prepara (flash) e da cui riprende i dati.

**Fa:** scan WiFi/BLE, sniffer, wardriving, deauth, beacon/probe spam, rogue AP /
Evil Portal, cattura PMKID/handshake, e — su **ESP32‑S2/S3** — **BadUSB/HID**.
**Non fa:** WiFi 5 GHz, sub‑GHz (serve un CC1101), monitor‑mode completo, cracking
pesante (lo fa il PC).

## Menu a "profili" (mini)
Menu semplice su schermo, con profili adattati all'hardware:
- 📡 Ricognizione WiFi · 🎯 Attacco WiFi (autorizzato) · 🔑 Handshake/PMKID ·
  🦷 Bluetooth/BLE · ⌨️ BadUSB (solo S2/S3)

## Integrazione con NexusSec (il "collegamento")
- **Flash con un click** dalla distro (`nxs-esp32`/`vesper-esp32` via `esptool`).
- **Import risultati**: wardriving → mappa **HORUS**; handshake/PMKID → cartella
  **loot** per crackarli con i tool della distro.

## Hardware consigliato
- **Board**: **ESP32‑S3** con PSRAM (dual‑core, USB nativa per BadUSB, BLE5).
- **Devkit pronti**: LilyGO **T‑Display‑S3**, LilyGO **T‑Embed** (encoder; variante
  **CC1101** = sub‑GHz), **M5Stack Core2 / M5StickC Plus2**, **CYD** (TFT touch).
- **Display**: TFT SPI (ST7789/ILI9341) o OLED SSD1306.
- **Input**: encoder rotativo + tasto, o TFT touch.
- **Antenna**: connettore **u.FL/IPEX** + antenna 2.4 GHz (2–3 dBi).
- **microSD** (log/handshake/payload) · **GPS** UART (u‑blox NEO‑6M/M8N) per il
  wardriving · **LiPo + carica** per portabilità.

## Ecosistema NexusSec (la "costellazione")
NexusSec ESP32 è il **gadget hardware** di una famiglia di strumenti che
condividono brand e filosofia (tutto in casa, minimale, in italiano):

- **[NexusSec OS](https://github.com/RedRider21/NexusSec-OS)** — la distro live
  di cybersecurity (Alpine + Openbox + pannello Python). Prepara e **flesha
  l'ESP32** con uno dei profili, e ne **importa i risultati** (wardriving in
  HORUS, handshake/PMKID nel loot).
- **[Vesper](https://github.com/RedRider21/vesper)** — l'ambiente desktop
  Python/GTK estratto da NexusSec.
- **[Termux-NexusSEC-OS](https://github.com/RedRider21/Termux-NexusSEC-OS)** — la
  versione Android (Termux + Kali proot + PWA).

La distro **NexusSec OS** sarà in grado di **flashare direttamente l'ESP32** con
uno dei profili prescelti (Ricognizione / Attacco WiFi / Handshake / Bluetooth /
BadUSB), via `esptool`, e di raccogliere ciò che il gadget cattura.

## Note
- Stack: **Arduino‑ESP32** o **ESP‑IDF** (C/C++).
- Uso **solo su target autorizzati** (pentest/lab): deauth/rogue AP/BadUSB sono
  tecniche offensive.
- Dettagli e fasi: [`docs/plan.md`](docs/plan.md). Istruzioni per l'assistente:
  [`CLAUDE.md`](CLAUDE.md).
