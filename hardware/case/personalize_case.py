#!/usr/bin/env python3
"""Personalizza un case ESP32-DIV V2 (file di terzi, uso personale) incidendo il
logo NexusSec (emblema + scritta) sul fondo esterno del guscio posteriore.

Uso:  python3 personalize_case.py <back.stl> <out_base>
Output: <out_base>.stl e <out_base>.glb

NB: i file di terzi (DownLord / 0xrphl, licenza MakerWorld) restano in
docs/case/originali/ e NON vanno ridistribuiti; l'output e' per uso personale.
"""
import sys, math, os
import numpy as np
import trimesh
from shapely.geometry import Polygon, LineString, Point
from shapely.ops import unary_union
from shapely.affinity import translate, scale, rotate
from matplotlib.textpath import TextPath
from matplotlib.font_manager import FontProperties

# ---------------- logo 2D ----------------
FONTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fonts")
FONT_H = os.path.join(FONTS, "ChakraPetch-Bold.ttf")      # orizzontale
FONT_V = os.path.join(FONTS, "ChakraPetch-Bold.ttf")       # verticale (stesso dell'orizzontale, piu' nitido)

def text_poly(s, size, weight='normal', fname=None):
    prop = FontProperties(fname=fname) if (fname and os.path.exists(fname)) else FontProperties(family='DejaVu Sans', weight=weight)
    tp = TextPath((0, 0), s, size=size, prop=prop)
    geoms = []
    for ring in tp.to_polygons():
        if len(ring) >= 3:
            g = Polygon(ring)
            if not g.is_valid: g = g.buffer(0)
            geoms.append(g)
    res = None
    for g in geoms:                         # even-odd (gestisce i buchi, es. P)
        res = g if res is None else res.symmetric_difference(g)
    return res

def emblem(R=8.0, tw=1.6):
    hexa = lambda r: Polygon([(r*math.cos(math.radians(90+60*k)), r*math.sin(math.radians(90+60*k))) for k in range(6)])
    ring = hexa(R).difference(hexa(R-tw))
    nx, ny = R*0.5, R*0.58
    N = unary_union([
        LineString([(-nx, -ny), (-nx, ny)]).buffer(tw*0.42),
        LineString([(-nx, ny), (nx, -ny)]).buffer(tw*0.42),
        LineString([(nx, -ny), (nx, ny)]).buffer(tw*0.42)])
    nodes = unary_union([Point(p).buffer(tw*0.62) for p in [(-nx, ny), (-nx, -ny), (nx, ny), (nx, -ny)]])
    return unary_union([ring, N, nodes])

def bounds_of(g): return g.bounds  # (minx,miny,maxx,maxy)

def extrude_any(poly, h):
    geoms = list(poly.geoms) if poly.geom_type == 'MultiPolygon' else [poly]
    parts = []
    for g in geoms:
        if g.is_empty or g.area < 1e-6: continue
        if not g.is_valid: g = g.buffer(0)
        parts.append(trimesh.creation.extrude_polygon(g, h))
    return trimesh.util.concatenate(parts)

def _center_scale(logo, target_w):
    minx, miny, maxx, maxy = bounds_of(logo)
    logo = translate(logo, -(minx+maxx)/2, -(miny+maxy)/2)
    return scale(logo, xfact=target_w/(maxx-minx), yfact=target_w/(maxx-minx), origin=(0, 0))

def build_logo(layout='h', target_w=None, font=None):
    emb = emblem(8.0, 1.6)
    if layout == 'h':                       # emblema a sinistra + 2 righe a destra
        t1 = text_poly("NEXUSSEC", 10, 'bold', font); t2 = text_poly("ESP32", 6.2, 'normal', font)
        ex0, ey0, ex1, ey1 = bounds_of(emb); ecy = (ey0+ey1)/2
        tx = ex1 + 4.0
        b1 = bounds_of(t1); t1 = translate(t1, tx - b1[0], ecy + 1.0 - b1[1])
        b2 = bounds_of(t2); t2 = translate(t2, tx - b2[0], ecy - 7.0 - b2[1])
        logo = unary_union([emb, t1, t2])
        return _center_scale(logo, target_w or 72.0)
    else:                                   # verticale: emblema sopra, testo impilato
        t1 = text_poly("NEXUSSEC", 9, 'bold', font); t2 = text_poly("ESP32", 6, 'normal', font)
        eb = bounds_of(emb); ecx = (eb[0]+eb[2])/2
        b1 = bounds_of(t1); t1 = translate(t1, ecx-(b1[0]+b1[2])/2, eb[1]-3.0-b1[3])
        b1 = bounds_of(t1)
        b2 = bounds_of(t2); t2 = translate(t2, ecx-(b2[0]+b2[2])/2, b1[1]-2.0-b2[3])
        logo = unary_union([emb, t1, t2])
        return _center_scale(logo, target_w or 42.0)

# ---------------- incisione sul back ----------------
def personalize(back_path, out_base, depth=1.0, orient='h', mirror=True):
    m = trimesh.load(back_path, force='mesh')
    m.apply_translation(-m.bounds[0])      # min -> origine; fondo esterno a z=0
    bx, by, bz = m.extents
    font = FONT_H if orient == 'h' else FONT_V
    logo = build_logo(orient, None, font)
    if orient == 'v':
        logo = rotate(logo, 90, origin=(0, 0))   # impila lungo il lato lungo (antenne in alto)
    if mirror:
        logo = scale(logo, xfact=-1, yfact=1, origin=(0, 0))  # leggibile da fuori (−z)
    lb = bounds_of(logo); lw = lb[2]-lb[0]; lh = lb[3]-lb[1]
    print("logo %.1f x %.1f mm  su back %.1f x %.1f" % (lw, lh, bx, by))
    solid = extrude_any(logo, depth + 0.4)
    # posiziona: centrato in x,y; incide dal fondo esterno (z=0) verso l'interno
    solid.apply_translation([bx/2, by/2, -0.2])
    out = trimesh.boolean.difference([m, solid])
    out.export(out_base + ".stl")
    out.export(out_base + ".glb")
    print("scritto", out_base + ".stl /.glb")
    return out

if __name__ == "__main__":
    back = sys.argv[1]; outb = sys.argv[2]
    orient = sys.argv[3] if len(sys.argv) > 3 else 'h'
    depth = float(sys.argv[4]) if len(sys.argv) > 4 else 1.0
    personalize(back, outb, depth=depth, orient=orient)
