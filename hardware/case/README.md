# Case 3D — NexusSec ESP32

Case stampabile per la scheda **ESP32-DIV V2**: **cover posteriore + fascia**
perimetrale che arriva **a filo col fronte**, **4 fori** per fissarlo con le
**viti già presenti**, e il **logo NexusSec + scritta inciso sul retro**.

File: [`nexussec-esp32-case.scad`](nexussec-esp32-case.scad) (OpenSCAD, parametrico).

## ⚠️ Prima di stampare: MISURA la scheda
Il case è un **template**: le quote derivano dal render PCB CiferTech
(53.086 × 100.457 mm) + stime. Con un **calibro** verifica e aggiorna in cima al
`.scad`:
- `dev_thick` — spessore **totale** del device (determina l'altezza della fascia)
- `screw_inset` — rientro dei 4 fori dagli spigoli
- `screw_d` / `screw_head_d` / `screw_head_h` — diametro vite e svasatura testa
- posizioni/dimensioni dei **tagli** (USB-C, microSD, antenne SMA, tasti): vedi il
  blocco `TODO dopo misurazione` nel `.scad`.

## Generare l'STL
Con [OpenSCAD](https://openscad.org) (gratuito):
```bash
openscad -o nexussec-esp32-case.stl nexussec-esp32-case.scad
```
Oppure apri il `.scad` in OpenSCAD, regola i parametri e premi **F6** → Export STL.

## Misure reali di riferimento (case community ESP32-DIV V2)
- **Case standard 2KLAB** (Cults3D): FRONT 59,0 × 105,5 × **20,5** mm · BACK
  59,0 × 105,5 × **24,0** mm · ingombro assemblato ~**59 × 105,5 × 24 mm**
  (esclusa la sporgenza dei connettori SMA in alto).
- **PCB**: ~53,09 × 100,46 mm (render CiferTech).
- Il nostro guscio "back + fascia a filo" con i parametri del `.scad` esce
  ~**58,7 × 106** mm, in linea col case standard.
- Altri progetti (riferimento): DownLord "ESP32-DIV2 Enclosure" (MakerWorld),
  battery-mod 18650 `0xrphl/esp32-div-v2-battery-mod` (GitHub). Il **nostro**
  aggiunge il **logo NexusSec** sul retro.

## Aperture da prevedere (dopo misura)
Fori SMA in alto (WiFi/Sub-GHz/2.4G), USB-C (flash/alimentazione), slot microSD,
finestra a filo per il TFT 2,8", passaggio tasti/LED. Vedi blocco `TODO` nel `.scad`.

## Stampa consigliata
- Materiale: **PETG** o **PLA-CF/PLA+** (rigidi, resistono al calore della scheda).
- Ugello 0.4 mm, **layer 0,16 mm** (0,15–0,20), pareti 3 perimetri, **infill 20–30%**.
- **Orientamento**: fondo (retro) sul piatto → il **logo inciso** viene pulito e
  **niente supporti** per la fascia. (Se preferisci il logo **in rilievo** invece
  che inciso, si cambia con una riga nel `.scad` — chiedi.)
- Tolleranza `tol` (gioco attorno al PCB) preimpostata a 0.4 mm: se entra stretto,
  aumpenta a 0.5.

## Note
- I 4 fori sono **svasati** sul retro: le viti originali passano dall'esterno e
  bloccano il case sulla scheda.
- Il logo è **speculato** nel sorgente così risulta dritto guardando il retro.
- Materiale di terzi (scheda) © CiferTech (MIT); il **case e il logo sono NexusSec**.
