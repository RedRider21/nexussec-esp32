# NexusSec ESP32 — hardware necessario (promemoria acquisti)

Promemoria per acquistare il materiale del gadget. Obiettivo: un "drone di
ricognizione" WiFi/BLE con menu a schermo, integrato con NexusSec (flash dalla
distro + import dei dati in HORUS/loot). Uso **solo su target autorizzati**.

## Scelta rapida (2 percorsi)

### A) "Tutto‑in‑uno" (consigliato per iniziare — meno saldature)
Un devkit che ha già scheda + schermo (+ spesso batteria/encoder):
- **LilyGO T‑Embed** (ESP32‑S3 + TFT 1.9" + encoder rotativo + LiPo). Variante
  **T‑Embed CC1101** = aggiunge anche il **sub‑GHz** (433/868/915 MHz).
- **LilyGO T‑Display‑S3** (ESP32‑S3 + TFT 1.9" + 2 tasti + USB‑C).
- **M5Stack Core2** o **M5StickC Plus2** (ESP32 + schermo + batteria).
- **CYD "Cheap Yellow Display"** (ESP32 + TFT 2.4/2.8" touch, economico).
> Per il **BadUSB/HID** serve la USB nativa → scegliere modelli **ESP32‑S3** (o S2).

### B) "A moduli" (più libertà, richiede cablaggio)
Scheda + componenti separati (vedi tabella sotto).

## Componenti (percorso B) e a cosa servono
| Componente | Consigliato | A cosa serve | Note |
|---|---|---|---|
| **Scheda** | **ESP32‑S3 DevKitC‑1** con **PSRAM** | WiFi 2.4GHz + BLE5 + **USB nativa** (BadUSB) | Classic ESP32 = niente BadUSB; S2 = niente BT |
| **Display** | TFT SPI **ST7789 1.9"/2.0"** o **ILI9341 2.4"** | menu/mappe/liste | in alternativa OLED **SSD1306 0.96"** (menu minimale) |
| **Input** | **encoder rotativo KY‑040** + 1 tasto | navigare il menu | oppure 3 tasti tattili, o TFT touch |
| **Antenna** | modulo con **connettore u.FL/IPEX** + **antenna 2.4GHz 2–3 dBi** | più portata WiFi/BLE della PCB antenna | scegliere una scheda con u.FL |
| **microSD** | modulo **microSD SPI** + scheda 8–32 GB | log wardriving, handshake, payload | FAT32 |
| **GPS** | **u‑blox NEO‑6M** o **NEO‑M8N** (UART) | coordinate per il wardriving → mappa HORUS | con antenna GPS attiva |
| **Alimentazione** | **LiPo 3.7V 1000–2000 mAh** + modulo di **carica TP4056** | uso portatile | molti devkit A) l'hanno già |
| **Cavi/breadboard** | dupont + breadboard, oppure PCB/saldatura | collegamenti | — |
| Opzionali | RTC DS3231 (timestamp), buzzer/LED, **case 3D** | qualità di vita | — |

## Combo consigliata (equilibrio costo/funzioni)
**ESP32‑S3 + PSAM · TFT ST7789 1.9" · encoder KY‑040 · antenna 2.4GHz su u.FL ·
microSD SPI · GPS NEO‑M8N · LiPo + TP4056**.
Se vuoi la scorciatoia con anche il **sub‑GHz** già a bordo: **LilyGO T‑Embed CC1101**.

## Cosa NON aspettarsi dall'hardware
- **No WiFi 5 GHz**, **no sub‑GHz** senza CC1101, **no** monitor‑mode completo come
  una scheda vera, **no** cracking a bordo (gli handshake si crackano sul PC).
- BadUSB **solo** su ESP32‑S2/S3 (USB nativa).

## Dopo l'acquisto
- Il firmware (Opzione B, nostro) e l'integrazione con la distro (`nxs-esp32`:
  flash con esptool + import in HORUS/loot) sono da sviluppare: vedi
  [`plan.md`](plan.md) e `../README.md`.

## Dove acquistare (venditori + nome esatto del prodotto)

> Non metto URL profondi (cambiano spesso). Uso il **nome esatto** da cercare sul
> sito del venditore. Venditori affidabili con spedizione in Italia/UE.

**Venditori consigliati**
- Ufficiali: **LilyGO** (lilygo.cc) · **M5Stack** (shop.m5stack.com) · **Espressif**.
- Distributori: **Mouser** (mouser.it) · **DigiKey** (digikey.it) · **Adafruit** ·
  **Pimoroni** (pimoroni.com) · **Berrybase** (berrybase.de, UE) · **Botland**
  (botland.it) · **Melopero** (melopero.com, IT) · **Robot Italy** (robot-italy.com, IT).
- Marketplace: **Amazon.it** · **AliExpress** (store ufficiali "LILYGO", "M5Stack").

**Componente → cosa cercare → dove**
| Componente | Nome/ricerca esatta | Dove |
|---|---|---|
| Scheda (consigliata) | **ESP32-S3-DevKitC-1** (o **-1U** con u.FL) | Mouser, DigiKey, Amazon, AliExpress |
| All‑in‑one (schermo+encoder) | **LilyGO T‑Embed** / **T‑Embed CC1101** | lilygo.cc, AliExpress "LILYGO", Berrybase |
| All‑in‑one (schermo+tasti) | **LilyGO T‑Display‑S3** | lilygo.cc, AliExpress, Botland |
| All‑in‑one (case incluso) | **M5Stack Core2** / **M5StickC Plus2** | shop.m5stack.com, Mouser, Melopero, Robot Italy |
| Economico con TFT touch | **ESP32‑2432S028R** ("Cheap Yellow Display") | AliExpress, Amazon |
| Display TFT | **ST7789 1.9"/2.0" SPI** o **ILI9341 2.4" SPI** | Amazon, AliExpress, Berrybase |
| Display OLED (mini) | **SSD1306 0.96" I2C** | Amazon, AliExpress, Adafruit |
| Input | **encoder rotativo KY‑040** | Amazon, AliExpress, Botland |
| Antenna | **antenna 2.4GHz u.FL/IPEX 3 dBi** + pigtail | Amazon, AliExpress |
| microSD | **modulo microSD SPI** + microSD 16–32 GB | Amazon, AliExpress |
| GPS | **u‑blox NEO‑M8N** (o NEO‑6M) UART + antenna | Amazon, AliExpress |
| Batteria | **LiPo 3.7V 1000–2000 mAh** + **modulo TP4056** | Amazon, AliExpress, Adafruit |
| Cavi | dupont + breadboard | Amazon, AliExpress |

## Case (esiste?)
- **M5Stack**: già **con case** (chiuso di fabbrica). Nessun problema.
- **LilyGO T‑Embed / T‑Display‑S3**: vendute con un **guscio/telaio**; in più esistono
  case stampabili — cerca su **Printables.com** e **Thingiverse** "T‑Embed case" /
  "T‑Display‑S3 case".
- **CYD (ESP32‑2432S028R)**: case **stampati 3D** su Printables/Thingiverse
  ("Cheap Yellow Display case" / "ESP32‑2432S028 case").
- **ESP32‑S3 a moduli**: case **stampato 3D** (Printables/Thingiverse "ESP32‑S3 case")
  oppure **acquisto già stampato** su **Etsy** (cerca "ESP32 case" / "T‑Embed case").

## Kit "pronto" più semplice (se vuoi il minimo sforzo)
**LilyGO T‑Embed CC1101** (ESP32‑S3 + schermo + encoder + LiPo + sub‑GHz) + una
**microSD** + un **GPS NEO‑M8N**: copre quasi tutto, poche saldature.
