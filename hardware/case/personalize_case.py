#!/usr/bin/env python3
"""Personalizza un case ESP32-DIV V2 (file di terzi, uso personale) incidendo il
logo NexusSec (emblema + scritta) sul fondo esterno del guscio posteriore.

Uso:  python3 personalize_case.py <back.stl> <out_base>
Output: <out_base>.stl e <out_base>.glb

NB: i file di terzi (DownLord / 0xrphl, licenza MakerWorld) restano in
docs/case/originali/ e NON vanno ridistribuiti; l'output e' per uso personale.
"""
import sys, math
import numpy as np
import trimesh
from shapely.geometry import Polygon, LineString, Point
from shapely.ops import unary_union
from shapely.affinity import translate, scale
from matplotlib.textpath import TextPath
from matplotlib.font_manager import FontProperties

# ---------------- logo 2D ----------------
def text_poly(s, size, weight='normal'):
    tp = TextPath((0, 0), s, size=size, prop=FontProperties(family='DejaVu Sans', weight=weight))
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

def build_logo(target_w=72.0):
    emb = emblem(8.0, 1.6)
    t1 = text_poly("NEXUSSEC", 10, weight='bold')
    t2 = text_poly("ESP32", 6.2, weight='normal')
    # allinea: emblema a sinistra; testo a destra, due righe
    ex0, ey0, ex1, ey1 = bounds_of(emb); ecy = (ey0+ey1)/2
    gap = 4.0
    tx = ex1 + gap
    # NEXUSSEC sopra la mezzeria, ESP32 sotto
    b1 = bounds_of(t1); t1 = translate(t1, tx - b1[0], ecy + 1.0 - b1[1])
    b2 = bounds_of(t2); t2 = translate(t2, tx - b2[0], ecy - 7.0 - b2[1])
    logo = unary_union([emb, t1, t2])
    # centra e scala a target_w
    minx, miny, maxx, maxy = bounds_of(logo)
    logo = translate(logo, -(minx+maxx)/2, -(miny+maxy)/2)
    w = maxx-minx
    s = target_w / w
    logo = scale(logo, xfact=s, yfact=s, origin=(0, 0))
    return logo

# ---------------- incisione sul back ----------------
def personalize(back_path, out_base, depth=0.6, target_w=72.0, mirror=True):
    m = trimesh.load(back_path, force='mesh')
    m.apply_translation(-m.bounds[0])      # min -> origine; fondo esterno a z=0
    bx, by, bz = m.extents
    logo = build_logo(target_w)
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
    depth = float(sys.argv[3]) if len(sys.argv) > 3 else 0.6
    personalize(back, outb, depth=depth)
