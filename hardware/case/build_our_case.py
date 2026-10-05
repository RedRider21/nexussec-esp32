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
Output: docs/case/case-nexussec/nexussec-case-{back,front}[-bat].{stl,glb}
"""
import os, sys
import numpy as np
import trimesh
from shapely.geometry import box
from shapely.affinity import translate as sh_translate, scale as sh_scale, rotate as sh_rotate
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

# --- quote MISURATE dal front originale DownLord (origine = centro) ---
DISP_W, DISP_H, DISP_CX = 60.0, 44.5, -7.25      # finestra display + offset x dal centro
DPAD_CX, DPAD_BS, DPAD_PX, DPAD_PY = 37.0, 6.6, 6.7, 9.0   # croce tasti

def logo_solid(layout, target_w, bc, outer=False, rot=0):
    """Prisma del logo da sottrarre. outer=True (front, faccia +z): niente specchio;
    altrimenti (back, faccia -z): specchio x per leggerlo da fuori. rot: gradi CCW."""
    lg = build_logo(layout, target_w, FONT_H)
    if rot:
        lg = sh_rotate(lg, rot, origin=(0, 0))
    if not outer:
        lg = sh_scale(lg, xfact=-1, yfact=1, origin=(0, 0))
    lg = sh_translate(lg, bc, 0)
    return extrude_any(lg, 1.2)

def build(part, battery=False):
    Lx = L + (BAT_BAY if battery else 0.0)
    bc = -BAT_BAY/2 if battery else 0.0          # centro zona-scheda (verso -x = alto/antenne)
    D = BACK_D if part == 'back' else FRONT_D
    PLATE = WALL
    outer = trimesh.creation.extrude_polygon(rrect(Lx, Wd, R), D)
    inner = trimesh.creation.extrude_polygon(rrect(Lx-2*WALL, Wd-2*WALL, R-WALL), D)
    # back: piastra in basso (z0); front: piastra in alto (z=D) -> vasche contrapposte
    inner.apply_translation([0, 0, WALL if part == 'back' else -PLATE])
    shell = trimesh.boolean.difference([outer, inner])
    cuts, adds = [], []

    # antenne: 4 SMA (max reale) sul lato corto alto (-x), misura 8x7, passo ~11
    z_ant = (D-3.5) if part == 'back' else 3.5   # al seam: bordo alto back / basso front
    for yy in (-16.5, -5.5, 5.5, 16.5):
        cuts.append(bx(WALL*3, 8.0, 7.0, -Lx/2, yy, z_ant))

    if part == 'back':
        for sx in (-1, 1):
            for sy in (-1, 1):
                cuts += [cyl(SCREW_D, D*2, sx*(Lx/2-SCREW_INSET), sy*(Wd/2-SCREW_INSET), D/2),
                         cyl(HEAD_D, 2.0, sx*(Lx/2-SCREW_INSET), sy*(Wd/2-SCREW_INSET), 1.0-0.01)]
        # porte: USB-C (lato lungo -y) e microSD (lato lungo +y), zona scheda
        cuts.append(bx(11, WALL*3, 4.5, bc+40, -Wd/2, WALL+5))
        cuts.append(bx(16, WALL*3, 2.6, bc+20, Wd/2, WALL+4))
        if battery:
            xdiv = Lx/2 - BAT[0] - WALL*1.5
            adds.append(bx(WALL, Wd-2*WALL, D-WALL, xdiv, 0, D/2+WALL/2))
            cuts.append(bx(WALL*3, 7, 5, xdiv, 0, D-4))
        # logo inciso sul fondo esterno (z=0), speculato
        ls = logo_solid('h', 72.0, bc, outer=False); ls.apply_translation([0, 0, -0.2]); cuts.append(ls)
    else:
        # finestra display (misurata) + croce tasti (misurata)
        cuts.append(bx(DISP_W, DISP_H, D*3, bc+DISP_CX, 0, D/2))
        for dx, dy in [(0, 0), (DPAD_PX, 0), (-DPAD_PX, 0), (0, DPAD_PY), (0, -DPAD_PY)]:
            cuts.append(bx(DPAD_BS, DPAD_BS, D*3, bc+DPAD_CX+dx, dy, D/2))
        if battery:
            # divisorio batteria ANCHE sul front (isola la batteria) + passacavo allineato al back
            xdiv = Lx/2 - BAT[0] - WALL*1.5
            adds.append(bx(WALL, Wd-2*WALL, D-PLATE, xdiv, 0, (D-PLATE)/2))
            cuts.append(bx(WALL*3, 7, 5, xdiv, 0, 2.5))     # notch passacavo (al seam col back)
            # logo sul FRONTE area batteria: VERTICALE (leggibile col case in portrait),
            # centrato nell'area estesa cosi' non sborda
            xlogo = (xdiv + Lx/2) / 2.0
            ls = logo_solid('v', 40.0, 0, outer=True, rot=90)
            ls.apply_translation([xlogo, 0, D-0.9]); cuts.append(ls)

    m = trimesh.boolean.union([shell] + adds) if adds else shell
    m = trimesh.boolean.difference([m] + cuts)
    return m

if __name__ == "__main__":
    jobs = [("back", False, "standard/back"), ("front", False, "standard/front"),
            ("back", True, "battery/back"), ("front", True, "battery/front")]
    for part, bat, name in jobs:
        m = build(part, bat)
        p = "docs/case/case-nexussec/%s" % name
        os.makedirs(os.path.dirname(p), exist_ok=True)
        m.export(p+".stl"); m.export(p+".glb")
        print("%-16s watertight=%s dims=%s" % (name, m.is_watertight, np.round(m.extents, 1)))
