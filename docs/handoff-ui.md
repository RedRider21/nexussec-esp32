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

## File utili
- `docs/ui-mockup.html` — il mockup (aprire nel browser o via Artifact).
- `docs/prossimi-passi.md` — handoff generale + strade firmware A/B.
- `docs/hardware.md` — hardware e acquisto.
- `CLAUDE.md` — vincoli e stile.
