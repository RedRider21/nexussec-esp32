#!/usr/bin/env python3
"""Case NexusSec ESP32 — NOSTRO, parametrico (back tray + front frame).
Opera interamente nostra (nessun file di terzi): pubblicabile e scaricabile.

Due varianti:
  - standard:  ~105.5 x 59 x 24 mm (rif. ESP32-DIV V2)
  - battery:   case ALLUNGATO in basso con vano interno per LiPo piatta
               (consigliata 103450: 50x34x10 mm, ~1800-2000 mAh, molto comune)

Feature (finestra display, 6 tasti, antenne, porte) in posizione APPROSSIMATA:
v0 BETA da validare su hardware.

Uso:  python3 build_our_case.py
Output: docs/case-nexussec/nexussec-case-{back,front}[-bat].{stl,glb}
"""
import os, sys
import numpy as np
import trimesh
from shapely.geometry import box
from shapely.affinity import translate as sh_translate, scale as sh_scale
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from personalize_case import build_logo, extrude_any, FONT_H

L, Wd, R = 105.5, 59.0, 5.0        # esterne standard (x,y) + raggio
WALL, BACK_D, FRONT_D = 2.4, 15.4, 8.6
SCREW_INSET, SCREW_D, HEAD_D = 5.5, 3.0, 5.6
BAT_BAY = 56.0                      # allungamento per il vano batteria
BAT = (52.0, 36.0, 11.0)           # vano interno per LiPo 103450 (x,y,z)

def rrect(w, h, r): return box(-w/2+r, -h/2+r, w/2-r, h/2-r).buffer(r, quad_segs=14)
def bx(w, h, d, x, y, z):
    m = trimesh.creation.box(extents=[w, h, d]); m.apply_translation([x, y, z]); return m
def cyl(d, h, x, y, z):
    c = trimesh.creation.cylinder(radius=d/2, height=h, sections=32); c.apply_translation([x, y, z]); return c

def build(part, battery=False):
    Lx = L + (BAT_BAY if battery else 0.0)
    bc = -BAT_BAY/2 if battery else 0.0          # centro zona-scheda (verso -x = alto/antenne)
    D = BACK_D if part == 'back' else FRONT_D
    outer = trimesh.creation.extrude_polygon(rrect(Lx, Wd, R), D)
    inner = trimesh.creation.extrude_polygon(rrect(Lx-2*WALL, Wd-2*WALL, R-WALL), D)
    inner.apply_translation([0, 0, WALL if part == 'back' else -0.01])
    shell = trimesh.boolean.difference([outer, inner])
    cuts, adds = [], []

    # antenne: lato corto alto (-x)
    for yy in np.linspace(-18, 18, 4):
        cuts.append(bx(WALL*3, 6.5, 6.5, -Lx/2, yy, (D-5) if part=='back' else (D-4)))

    if part == 'back':
        # viti ai 4 angoli (svasate sul fondo z=0)
        for sx in (-1, 1):
            for sy in (-1, 1):
                cuts += [cyl(SCREW_D, D*2, sx*(Lx/2-SCREW_INSET), sy*(Wd/2-SCREW_INSET), D/2),
                         cyl(HEAD_D, 2.0, sx*(Lx/2-SCREW_INSET), sy*(Wd/2-SCREW_INSET), 1.0-0.01)]
        # porte: USB-C (lato lungo -y, zona alta) e microSD (lato lungo +y)
        cuts.append(bx(11, WALL*3, 4.5, bc+40, -Wd/2, WALL+5))
        cuts.append(bx(16, WALL*3, 2.6, bc+20, Wd/2, WALL+4))
        if battery:
            # divisorio + vano batteria verso il basso (+x)
            xdiv = Lx/2 - BAT[0] - WALL*1.5
            adds.append(bx(WALL, Wd-2*WALL, D-WALL, xdiv, 0, (D)/2+WALL/2))   # parete divisoria
            # notch passacavo nel divisorio
            cuts.append(bx(WALL*3, 7, 5, xdiv, 0, D-4))
        # logo inciso sul fondo (zona scheda), speculato
        logo = sh_scale(build_logo('h', 72.0, FONT_H), xfact=-1, yfact=1, origin=(0, 0))
        logo = sh_translate(logo, bc, 0)
        lsolid = extrude_any(logo, 1.2); lsolid.apply_translation([0, 0, -0.2])
        cuts.append(lsolid)
    else:
        # finestra display (~58x35) nella zona scheda
        cuts.append(bx(58, 35, D*3, bc-14, 0, D/2))
        # griglia 6 tasti (2x3) a destra della finestra, zona scheda
        for cxo in (34, 44):
            for cyo in (-11, 0, 11):
                cuts.append(bx(7.5, 7.5, D*3, bc+cxo, cyo, D/2))

    m = trimesh.boolean.union([shell] + adds) if adds else shell
    m = trimesh.boolean.difference([m] + cuts)
    return m

if __name__ == "__main__":
    jobs = [("back", False, "standard/back"), ("front", False, "standard/front"),
            ("back", True, "battery/back"), ("front", True, "battery/front")]
    for part, bat, name in jobs:
        m = build(part, bat)
        p = "docs/case-nexussec/%s" % name
        os.makedirs(os.path.dirname(p), exist_ok=True)
        m.export(p+".stl"); m.export(p+".glb")
        print("%-16s watertight=%s dims=%s" % (name, m.is_watertight, np.round(m.extents, 1)))
