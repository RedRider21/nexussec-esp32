/* ============================================================================
   NexusSec ESP32 — CASE stampabile 3D (parametrico, OpenSCAD)
   ----------------------------------------------------------------------------
   Cover posteriore + fascia perimetrale che arriva "a filo" col fronte del
   dispositivo, 4 fori per fissare il case con le VITI GIA' presenti, e il
   LOGO NexusSec + scritta inciso sul retro.

   ⚠️ QUOTE DA CONFERMARE sulla scheda reale (spessore device, posizione fori,
      tagli per USB-C / microSD / antenne SMA / tasti). I valori qui sono
      ricavati dal render PCB CiferTech (53.086 x 100.457 mm) + stime: vanno
      verificati con un calibro prima della stampa definitiva.

   Render STL:  openscad -o nexussec-esp32-case.stl nexussec-esp32-case.scad
   ============================================================================ */

/* ---------------- Parametri (MISURARE sulla scheda) ----------------
   Riferimenti reali (case community 2KLAB per ESP32-DIV V2):
     ingombro esterno ~ 59.0 x 105.5 x 24.0 mm (senza sporgenza antenne SMA).
   Il nostro guscio "back + fascia a filo" con i valori sotto esce ~58.7 x 106,
   in linea con quello. */
board_w      = 53.09;   // larghezza PCB (render CiferTech)
board_h      = 100.46;  // altezza PCB (render CiferTech)
board_r      = 4;       // raggio angoli PCB
dev_thick    = 21;      // altezza fascia = spessore device a filo col fronte.
                        // Rif. case 2KLAB: back 24 mm / front 20.5 mm -> MISURARE.
screw_inset  = 4.5;     // rientro dei 4 fori dagli spigoli (MISURARE)
screw_d      = 2.9;     // diametro foro vite (M2.5 ~ 2.9 mm)
screw_head_d = 5.4;     // diametro svasatura testa
screw_head_h = 2.0;     // profondita' svasatura

/* ---------------- Costruzione ---------------- */
tol        = 0.4;   // gioco attorno al PCB
wall       = 2.4;   // spessore pareti (fascia)
back       = 2.2;   // spessore fondo
logo_depth = 0.8;   // profondita' incisione logo
logo_on    = true;
$fn        = 64;

inner_w = board_w + 2*tol;
inner_h = board_h + 2*tol;
outer_w = inner_w + 2*wall;
outer_h = inner_h + 2*wall;
band_h  = dev_thick;         // "a filo col fronte"

/* rettangolo con angoli arrotondati */
module rrect(w, h, r) offset(r) offset(-r) square([w, h], center = true);

/* emblema NexusSec 2D: anello esagonale + "N" a nodi */
module emblem2d(R = 8.5, tw = 1.6) {
  nx = R * 0.52; ny = R * 0.60;
  difference() { rotate(90) circle(R, $fn = 6); rotate(90) circle(R - tw, $fn = 6); }
  hull() { translate([-nx, -ny]) circle(tw*0.7); translate([-nx,  ny]) circle(tw*0.7); }  // asta sx
  hull() { translate([-nx,  ny]) circle(tw*0.7); translate([ nx, -ny]) circle(tw*0.7); }  // diagonale
  hull() { translate([ nx, -ny]) circle(tw*0.7); translate([ nx,  ny]) circle(tw*0.7); }  // asta dx
  for (p = [[-nx, ny], [-nx, -ny], [nx, ny], [nx, -ny]]) translate(p) circle(tw*0.9);     // nodi
}

/* logo completo: emblema + scritta */
module logo2d() {
  translate([0,  13]) emblem2d();
  translate([0,  -6]) text("NEXUSSEC", size = 7,   halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
  translate([0, -16]) text("ESP32",    size = 5.5, halign = "center", valign = "center", font = "Liberation Sans");
}

/* case */
module shell() {
  difference() {
    union() {
      // fondo
      linear_extrude(back) rrect(outer_w, outer_h, board_r + wall);
      // fascia perimetrale (fino a filo col fronte)
      linear_extrude(back + band_h)
        difference() { rrect(outer_w, outer_h, board_r + wall); rrect(inner_w, inner_h, board_r); }
    }

    // 4 fori vite con svasatura (testa sul retro esterno, z = 0)
    for (sx = [-1, 1], sy = [-1, 1])
      translate([sx * (board_w/2 - screw_inset), sy * (board_h/2 - screw_inset), 0]) {
        translate([0, 0, -1])    cylinder(h = back + band_h + 2, d = screw_d);
        translate([0, 0, -0.01]) cylinder(h = screw_head_h,      d = screw_head_d);
      }

    // logo inciso sul retro (faccia esterna z = 0), speculato per leggerlo da fuori
    if (logo_on)
      translate([0, 0, -0.01]) linear_extrude(logo_depth) mirror([1, 0, 0]) logo2d();
  }

  // ------------------------------------------------------------------
  // TODO dopo misurazione: aggiungere qui i tagli/finestre, es.
  //  - USB-C (lato corto), microSD (lato lungo), 4x SMA antenne (lato corto)
  //  - finestre per i 3 tasti frontali / passaggio LED
  //  - eventuali colonnine (standoff) se la scheda va sollevata dal fondo
  // Esempio taglio USB-C sul lato alto (quote da verificare):
  //   translate([0, outer_h/2, back + 6]) rotate([90,0,0]) cylinder(h=wall*3, d=10, center=true);
  // ------------------------------------------------------------------
}

shell();
