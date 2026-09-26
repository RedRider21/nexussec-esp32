# NexusSec ESP32 — avvio della prossima sessione

Documento di **handoff**: apri una nuova sessione **in questa cartella** e parti
da qui. Riassume l'hardware scelto, le due strade possibili per il firmware, i
passi concreti da compiere e le decisioni ancora aperte.

> Stato al 2026-09-26: **scaffold**. Repo su GitHub (`RedRider21/nexussec-esp32`),
> README + sito Pages bilingui online. Firmware **da sviluppare**. Nessun codice
> firmware ancora presente: qui ci sono solo i documenti.

---

## 1. Hardware scelto (scheda di riferimento)

**ESP32-DIV V2** di CiferTech — multi-tool wireless open-source (licenza **MIT**,
<https://github.com/cifertech/ESP32-DIV>). L'utente prevede di acquistare questa.

Specifiche:
- **MCU**: ESP32-S3, 16 MB flash (dual-core, USB nativa → BadUSB/HID, BLE5).
- **Radio**: WiFi + BLE · **3× NRF24L01** (2,4 GHz) · **CC1101** (sub-GHz) ·
  ricetrasmettitore **infrarossi**.
- **Schermo/input**: touch TFT **ILI9341 2,8"**, tasti fisici, 4 LED RGB
  WS2812B + buzzer.
- **Resto**: slot microSD, USB-C, connettori antenna **SMA**, power **IP5306**
  (batteria + boost), header a **pogo-pin** (impilabile).

Conseguenza importante rispetto al vecchio piano: **sub-GHz (CC1101)** e
**2,4 GHz generico (NRF24)** e **IR** ORA CI SONO nell'hardware. Il vecchio
`plan.md` diceva "solo WiFi/BLE, sub-GHz solo con CC1101 esterno": aggiornato.

Capacità realistiche della scheda:
- WiFi: scan, sniffer, wardriving, deauth, beacon/probe spam, rogue AP/Evil
  Portal, PMKID/handshake.
- BLE: scan, spam advertising, sniff (parziale).
- 2,4 GHz (NRF24): analisi/disturbo di canale, mousejack-like.
- Sub-GHz (CC1101): cattura e replay telecomandi (433/315/868/915 MHz).
- IR: cattura e replay.
- BadUSB/HID (ESP32-S3, USB nativa).
- **NON** fa: WiFi 5 GHz, monitor-mode completo come una NIC vera, cracking
  pesante (quello lo fa il PC coi dati raccolti).

---

## 2. Le due strade per il firmware (DECISIONE APERTA)

Mostrate entrambe sul sito. Da scegliere all'inizio della sessione:

### A — Firmware ESP32-DIV esistente (CiferTech, MIT)
- **Pro**: pronto e completo, community attiva, tutte le funzioni della scheda
  subito; noi ci concentriamo sull'**integrazione** con la distro.
- **Contro**: non brandizzato NexusSec; l'import in HORUS/loot va costruito
  sopra (parsing dei file che il firmware salva su microSD/seriale).
- **Lavoro nostro**: fork/patch leggere (menu/branding se serve) + il tool
  `nxs-esp32` lato distro (flash + import).

### B — Firmware nostro NexusSec (Opzione B, scelta 2026-09-14)
- **Pro**: tutto in casa, menu a profili in stile NexusSec, output nel formato
  che vogliamo (import diretto), nessuna dipendenza da terzi.
- **Contro**: da sviluppare da zero, più lavoro e manutenzione.
- **Lavoro nostro**: firmware completo (Arduino-ESP32 o ESP-IDF) + driver
  display touch + le librerie radio (NRF24, CC1101, IR) + il tool lato distro.

> Consiglio pragmatico: **partire da A** per avere subito un dispositivo
> funzionante e collaudare l'integrazione con la distro; valutare B in un secondo
> tempo se serve il pieno controllo/branding. Ma la decisione è dell'utente.

---

## 3. Passi concreti (checklist)

### Fase 0 — hardware in mano
- [ ] Acquistare la ESP32-DIV V2 (vedi `docs/hardware.md` per venditori).
- [ ] Verificare quale variante arriva (S3 16 MB, i moduli montati).

### Fase 1 — far girare qualcosa
- Strada A: [ ] compilare/flashare il firmware ESP32-DIV ufficiale, capire
  quali file salva su microSD e in che formato (WiFi list, PCAP, sub-GHz, IR).
- Strada B: [ ] toolchain (consiglio **Arduino-ESP32** per rapidità), display
  ILI9341 + touch, menu a profili minimale (cyan/scuro), avviso legale all'avvio.

### Fase 2 — integrazione con la distro (vale per A e B)
- [ ] Tool **`nxs-esp32`** lato NexusSec-OS: **flash** via `esptool` (scelta
  profilo/firmware) + **import** dei risultati.
- [ ] **Wardriving → HORUS**: definire il CSV (BSSID, SSID, enc, lat, lon, ts)
  e l'importatore nella mappa HORUS.
- [ ] **Handshake/PMKID → loot**: file hashcat 22000 / PCAP nella cartella loot.
- [ ] Sub-GHz/IR: dove salvare le catture e come rigiocarle.

### Fase 3 — confezionamento
- [ ] Istruzioni build/flash nel README, immagini firmware rilasciabili
  (release GitHub), eventuale case.

---

## 4. Integrazione lato distro (dettagli)

Il tool `nxs-esp32` (da creare in `../NexusSec-OS/overlay/usr/local/bin/`) deve:
- rilevare la scheda su seriale (USB-C), mostrare porta e chip;
- **flashare** un firmware/profilo con `esptool` (già installabile in NexusSec);
- **importare** da microSD o seriale: wardriving in HORUS, handshake in
  `~/NexusSec-loot/`;
- comparire nel menu/pannello della distro (icona dedicata).

HORUS: la mappa OSINT è in `../NexusSec-OS/overlay/usr/local/lib/nxs_horus/`.
loot: convenzione `~/NexusSec-loot/` già usata dai container dei tool.

---

## 5. Decisioni ancora aperte
- **A o B** (firmware esistente vs nostro) — vedi sezione 2.
- Arduino-ESP32 vs ESP-IDF (se strada B).
- Formato preciso dei log per HORUS (concordare i campi CSV).
- Se serve un case/antenne aggiuntive.

## 6. Riferimenti
- Hardware e acquisto: `docs/hardware.md`.
- Piano storico (pre-scheda): `docs/plan.md`.
- Ecosistema: `../NexusSec-OS/` (distro + HORUS), `../vesper/`,
  `../Termux-NexusSEC-OS/`. Memoria: `project_nexussec_esp32`.
- Firmware di riferimento (MIT): <https://github.com/cifertech/ESP32-DIV>.
