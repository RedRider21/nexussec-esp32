# Firmware ORIGINALE ESP32-DIV (CiferTech) — per ripristino

Binari **precompilati ufficiali** del produttore, tenuti da parte per poter
**tornare al firmware originale** dalla scheda (opzione di ripristino nel flasher,
accanto ai nostri profili NexusSec).

- `ESP32-DIV-v2-v1.7.2.bin` — build ufficiale per la **ESP32-DIV V2** (la nostra
  scheda), versione **v1.7.2** (1.743.424 byte).

## Fonte e licenza
- Progetto: **CiferTech ESP32-DIV** — <https://github.com/cifertech/ESP32-DIV>
- Cartella origine: `Pre-compiled Bin/` del repo.
- Licenza: **MIT** (ridistribuzione consentita con attribuzione). Copyright dei
  rispettivi autori CiferTech.

## Come si flasha (indirizzo tipico app)
Firmware Arduino/ESP32 monolitico da scrivere all'offset **0x10000** con esptool:
```
esptool.py --chip esp32s3 write_flash 0x10000 ESP32-DIV-v2-v1.7.2.bin
```
> ⚠️ Alcune build ufficiali sono immagini "merged" (bootloader+partizioni+app) da
> scrivere a **0x0**. Verificare sul repo del produttore quale offset usa la
> release prima del flash reale. Il nostro flasher lo gestirà come voce dedicata.

## Uso
Serve solo a **ripristinare lo stato di fabbrica**. Non fa parte del nostro
firmware NexusSec: è materiale di terzi tenuto per comodità dell'utente.
