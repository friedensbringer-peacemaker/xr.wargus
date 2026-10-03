#!/usr/bin/env python3
"""Launcher-Icons beider Apps (xr.wargus2 „II“, xr.wargus1 „I“) – eigene, rein prozedurale Grafik.

Kein Fremdmaterial: Formen (abgerundete Kachel, Wappenschild, römische Ziffer) werden hier per
Abstandsfunktion (SDF) mit Kantenglättung gerastert und als PNG geschrieben. Nur Python-Standardbibliothek.

  python tools/launcher-icon/make_icon.py            # schreibt android/src/{stratagus,war1gus}/res/mipmap-*/ic_launcher.png
  python tools/launcher-icon/make_icon.py --preview  # zusätzlich docs/icons/launcher-preview.png

Kachel (Rand, Radius, Verlauf, Schatten) ist am bisherigen Icon in main/res
ausgemessen (SDL-Vorlage), damit die Icons die Proportionen des Android-Standards behalten.
Herkunft und Farbwerte: docs/ICONS.md.
"""
import math, os, struct, sys, zlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SIZES = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}

# Varianten je Flavor: Schildfarbe oben/unten, Randfarbe, Ziffernzahl. Die Flavor-Ressourcen überdecken
# das SDL-Standard-Icon in main/res (bleibt als Rückfall liegen).
VARIANTS = {
    "war1gus":   dict(top=(0x8e, 0x2a, 0x1f), bot=(0x52, 0x14, 0x10), rim=(0xd9, 0xa4, 0x41), numerals=1),
    "stratagus": dict(top=(0x2c, 0x4f, 0x80), bot=(0x16, 0x2a, 0x4a), rim=(0xc8, 0xcf, 0xd8), numerals=2),
}
GOLD_TOP, GOLD_BOT = (0xf6, 0xd3, 0x6b), (0xc9, 0x8a, 0x2a)
OUTLINE = (0x24, 0x0c, 0x08)

# --- Abstandsfunktionen in Einheitskoordinaten (0..1, y nach unten) -----------------------------
def sd_rbox(x, y, x0, y0, x1, y1, r):
    cx, cy, hx, hy = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2 - r, (y1 - y0) / 2 - r
    qx, qy = abs(x - cx) - hx, abs(y - cy) - hy
    return math.hypot(max(qx, 0), max(qy, 0)) + min(max(qx, qy), 0) - r

TILE = (0.065, 0.065, 0.937, 0.937, 0.075)          # wie Hauptapp-Icon (192 px: 12..180, Radius ~14)
SH_CX, SH_W, SH_TOP, SH_MID = 0.5, 0.56, 0.19, 0.40  # Heraldischer Dreieckschild ("heater")

def sd_shield(x, y, grow=0.0):
    w = SH_W
    upper = sd_rbox(x, y, SH_CX - w / 2, SH_TOP, SH_CX + w / 2, SH_MID + 0.05, 0.03)
    c1 = math.hypot(x - (SH_CX + w / 2), y - SH_MID) - w   # linker Bogen
    c2 = math.hypot(x - (SH_CX - w / 2), y - SH_MID) - w   # rechter Bogen
    lower = max(c1, c2, SH_TOP - y)    # Linse liegt oberhalb von SH_MID im Rechteck
    return min(upper, lower) - grow

def sd_numerals(x, y, n):
    """Römische Ziffer I bzw. II mit Serifen, im Schild zentriert."""
    stem_w, serif_w, serif_h, top, bot = 0.075, 0.17, 0.038, 0.31, 0.64
    gap = 0.10
    xs = [SH_CX] if n == 1 else [SH_CX - gap / 2 - stem_w / 2 + 0.005, SH_CX + gap / 2 + stem_w / 2 - 0.005]
    d = 1e9
    for cx in xs:
        sw = serif_w if n == 1 else serif_w * 0.8
        d = min(d,
                sd_rbox(x, y, cx - stem_w / 2, top, cx + stem_w / 2, bot, 0.006),
                sd_rbox(x, y, cx - sw / 2, top, cx + sw / 2, top + serif_h, 0.008),
                sd_rbox(x, y, cx - sw / 2, bot - serif_h, cx + sw / 2, bot, 0.008))
    return d

# --- Rastern ---------------------------------------------------------------------------------------
def cover(d, px):          # SDF (Einheiten) -> Deckung 0..1, 1 px Kantenglättung
    return min(1.0, max(0.0, 0.5 - d * px))

def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))

def over(dst, col, a):     # dst = [r,g,b,a] (vormultipliziert), Quelle col mit Deckung a
    for i in range(3):
        dst[i] = col[i] * a + dst[i] * (1 - a)
    dst[3] = a + dst[3] * (1 - a)

def render(size, v):
    px = float(size)
    out = []
    for j in range(size):
        row = bytearray()
        for i in range(size):
            x, y = (i + 0.5) / px, (j + 0.5) / px
            p = [0.0, 0.0, 0.0, 0.0]
            # weicher Schatten unter der Kachel
            ds = sd_rbox(x, y - 0.012, *TILE)
            sh = min(1.0, max(0.0, 0.5 - ds / 0.03)) * 0.35
            over(p, (0, 0, 0), sh)
            # Kachel: hellgrauer Rand, weiß -> hellgrau nach unten
            dt = sd_rbox(x, y, *TILE)
            over(p, (0xb8, 0xb8, 0xb8), cover(dt, px))
            t = min(1.0, max(0.0, (y - 0.5) / 0.44))
            over(p, lerp((255, 255, 255), (0xd3, 0xd3, 0xd3), t * t), cover(dt + 1.2 / px, px))
            # Schild: Schatten, Goldrand, Feld mit Verlauf und Glanzkante oben
            over(p, (0, 0, 0), cover(sd_shield(x, y - 0.012, 0.004), px) * 0.25)
            over(p, v["rim"], cover(sd_shield(x, y), px))
            over(p, lerp(v["rim"], (0, 0, 0), 0.45), cover(sd_shield(x, y, -0.026), px))
            t = min(1.0, max(0.0, (y - SH_TOP) / 0.62))
            over(p, lerp(v["top"], v["bot"], t), cover(sd_shield(x, y, -0.034), px))
            hl = max(0.0, 1 - (y - SH_TOP) / 0.2) * 0.18
            over(p, (255, 255, 255), cover(sd_shield(x, y, -0.034), px) * hl)
            # Ziffer: dunkle Kontur, dann Gold mit Verlauf
            dn = sd_numerals(x, y, v["numerals"])
            over(p, OUTLINE, cover(dn - 0.014, px) * 0.85)
            t = min(1.0, max(0.0, (y - 0.31) / 0.33))
            over(p, lerp(GOLD_TOP, GOLD_BOT, t), cover(dn, px))
            a = p[3]
            rgb = [0, 0, 0] if a <= 0 else [round(min(255, c / a)) for c in p[:3]]
            row += bytes(rgb + [round(a * 255)])
        out.append(bytes(row))
    return out

def write_png(path, rows, w, h):
    raw = b"".join(b"\x00" + r for r in rows)
    def chunk(t, c):
        return struct.pack(">I", len(c)) + t + c + struct.pack(">I", zlib.crc32(t + c) & 0xffffffff)
    data = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)

def read_png(path):        # nur 8-bit RGBA, nicht interlaced (wie die Android-Icons)
    d = open(path, "rb").read(); i = 8; idat = b""
    while i < len(d):
        n, = struct.unpack(">I", d[i:i + 4]); t = d[i + 4:i + 8]; c = d[i + 8:i + 8 + n]; i += 12 + n
        if t == b"IHDR": w, h = struct.unpack(">II", c[:8])
        if t == b"IDAT": idat += c
    raw = zlib.decompress(idat); st = w * 4; rows = []; prev = bytearray(st); k = 0
    for _ in range(h):
        f = raw[k]; k += 1; line = bytearray(raw[k:k + st]); k += st
        for x in range(st):
            a = line[x - 4] if x >= 4 else 0; b = prev[x]; c = prev[x - 4] if x >= 4 else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                pp = a + b - c; pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(bytes(line)); prev = line
    return rows

def preview(path, tiles, bg=(0x1c, 0x1e, 0x22)):
    """Icons nebeneinander auf dunklem Grund (ähnlich der Quest-Bibliothek)."""
    pad = 24
    W = pad + sum(len(r) + pad for r in tiles)
    H = pad * 2 + max(len(r) for r in tiles)
    canvas = [bytearray(bytes(bg) * W) for _ in range(H)]
    x0 = pad
    for rows in tiles:
        s = len(rows); y0 = pad + (H - 2 * pad - s)
        for j, r in enumerate(rows):
            for i in range(s):
                sr, sg, sb, sa = r[4 * i:4 * i + 4]; a = sa / 255; o = 3 * (x0 + i)
                line = canvas[y0 + j]
                line[o:o + 3] = bytes(round(c * a + line[o + k] * (1 - a)) for k, c in enumerate((sr, sg, sb)))
        x0 += s + pad
    rgba = [bytes(b for i in range(W) for b in (*line[3 * i:3 * i + 3], 255)) for line in canvas]
    write_png(path, rgba, W, H)

def main():
    for flavor in ("stratagus", "war1gus"):
        for dens, s in SIZES.items():
            write_png(os.path.join(ROOT, f"android/src/{flavor}/res/mipmap-{dens}/ic_launcher.png"),
                      render(s, VARIANTS[flavor]), s, s)
    if "--preview" in sys.argv:
        tiles = []
        for flavor in ("stratagus", "war1gus"):
            tiles += [read_png(os.path.join(ROOT, f"android/src/{flavor}/res/mipmap-{d}/ic_launcher.png"))
                      for d in ("xxxhdpi", "xhdpi", "mdpi")]
        preview(os.path.join(ROOT, "docs/icons/launcher-preview.png"), tiles)
    print("ok")

if __name__ == "__main__":
    main()
