# Specifica: `nxs-esp32` — integrazione flash/import in NexusSec OS

> **Per chi**: la **sessione NexusSec OS** (`../NexusSec-OS/`). Questo file è il
> contratto da seguire lì. Progetto sorgente del gadget: `nexussec-esp32/`.
> Implementare rispettando le convenzioni della distro (Alpine, OpenRC, Python
> GTK3, `overlay/usr/local/bin/nxs-*`, i18n `nxs_i18n`, palette/accent profilo).

## Obiettivo
Un tool della distro che: (1) **flesha** la scheda ESP32-DIV V2 scegliendo un
**profilo/immagine**, con opzione **ripristino firmware originale**; (2)
**importa** i dati raccolti dal gadget (wardriving → HORUS, handshake/PMKID →
loot).

## Dove va (file da creare in NexusSec-OS)
- `overlay/usr/local/bin/nxs-esp32` — launcher/CLI (shell POSIX o Python).
- `overlay/usr/local/lib/nxs_esp32/` — logica Python (detect/flash/import) se serve.
- Voce nel pannello/menu (Centro di Controllo) con icona dedicata → lancia la GUI
  o `lxterminal -e "nxs-esp32"`.
- Dipendenze APK: `esptool` (o `esptool.py` via pip), `python3`, `py3-serial`.

## CLI proposta
```
nxs-esp32 detect                 # rileva scheda su seriale, mostra porta e chip
nxs-esp32 profiles               # elenca immagini disponibili (profili + originale)
nxs-esp32 flash <profilo>        # scrive l'immagine del profilo scelto
nxs-esp32 restore                # riscrive il firmware ORIGINALE CiferTech
nxs-esp32 import [horus|loot|all]# importa da microSD/seriale
```

### detect
- Cerca porte: `/dev/ttyACM*`, `/dev/ttyUSB*` (ESP32-S3 USB nativa → di solito
  `ttyACM0`). Verifica chip con `esptool --chip esp32s3 chip_id`.

### profiles / flash
- **Immagini** (da `nexussec-esp32`): immagine unica "merged" da scrivere a **0x0**
  con `esptool --chip esp32s3 write_flash 0x0 <img>.bin`.
  - Sorgente immagini: **release GitHub** di `nexussec-esp32` (asset `.bin`) o
    copia locale in `overlay/usr/local/share/nexussec/esp32/`.
  - Profili previsti (build PlatformIO per profilo; finché non ci sono, usare la
    **full**): `full` (tutti i moduli, default) · in futuro `recon`, `rf`, `hid`.
  - Ripristino: `ESP32-DIV-v2-v1.7.2.bin` (già in `nexussec-esp32/firmware/vendor/`,
    licenza MIT CiferTech).
- **Selezione con profilo** (richiesta utente): la GUI mostra le immagini come
  schede/pulsanti (stile selettore profilo distro), con l'accent del profilo.
  Dove non esiste una build per-profilo → mostrare solo **full optionals**.
- Flash con **barra di avanzamento** (parse output esptool) e avviso legale.

### import (formati prodotti dal firmware)
- **Wardriving** → **HORUS**: file microSD `/WARDRIVE.csv`, header:
  `BSSID,SSID,Enc,Channel,RSSI,Lat,Lon,Timestamp`. Importatore nella mappa HORUS
  (`../NexusSec-OS/overlay/usr/local/lib/nxs_horus/`): creare i marker AP con
  lat/lon; ignorare righe con `0.0,0.0` (nessun fix GPS).
- **Handshake/PMKID** → **loot**: file `*.hc22000` / PCAP in `~/NexusSec-loot/`
  (convenzione già usata dai container dei tool per il cracking).
- **Sub-GHz / IR**: catture su microSD (`.sub`, `.ir`) → cartella dedicata
  in loot (per replay futuri).
- Sorgente dati: microSD montata (lettore su PC) **oppure** via seriale se il
  firmware esporrà un comando di dump (fase 2).

## GUI (pannello NexusSec)
- Riusare `nxs_cc.common` (CSS/accent). Schede: **Flash** (scelta immagine +
  Scrivi) e **Import** (Wardriving→HORUS, Loot). Azioni privilegiate in terminale
  (`_run_priv_term`) come gli altri tool.

## Note
- Accesso seriale: l'utente `nexus` deve stare nei gruppi giusti (dialout/uucp)
  o usare `doas`.
- Uso **solo su hardware proprio/autorizzato**; l'avviso legale è già nel firmware.

## Riferimenti
- Firmware e mappa pin: `nexussec-esp32/firmware/` (`src/board_pins.h`, `README.md`).
- Firmware originale (ripristino): `nexussec-esp32/firmware/vendor/`.
- Flasher web standalone (stessa tecnologia ESP Web Tools): `nexussec-esp32/docs/flash/`.
- Memoria: `project_nexussec_esp32`, `project_nexussec_os`, `project_horus_osint_dashboard`.
