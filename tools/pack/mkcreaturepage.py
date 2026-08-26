#!/usr/bin/env python3
"""mkcreaturepage.py - the asset-side creature texture converter.

This is the seam that was missing. `raster_tri` already samples CLUT8 through
a `TextureSpan`, and the TMU already does five formats with mips -- but nothing
on the asset side ever BUILT a page, and `tools/pack/` held only a `.gitkeep`.

What it does, deterministically:

  concept scan -> crayon grain -> per-material tiles -> shared 256-entry
  RGB565 palette -> CLUT8 indices -> a C++ header the reel compiles in.

THE CRAYON IS REAL. The grain is not procedural noise and not a photo texture:
it is lifted out of S. Hofer's own scan at native resolution, so the strokes,
the pressure marks and the paper tooth on the model are the ones in the
drawing. That is both the most faithful route to MODELINGGUIDE's "handmade
crayon/pencil surface rather than clean plastic" and much the cheapest.

Tiles are 64x64, which is the format's unit (`Tileset::tiles[256][64*64]`).
That is not a compromise: MODELINGGUIDE is explicit that at 384x240 the budget
goes on what the player actually sees, and a creature 40 px tall does not repay
a 256x256 page.

UV CONVENTION, and it decides where the dorsal stripe lands. U is the ring
angle: vertex k sits at (k*256/segments + align)/256 of a turn, and the ring's
local (x,z) = (rx*cos, rz*sin). Local +Z maps to world -Y (DOWN), so a quarter
turn is DOWN and three quarters is UP. **U = 192/256 is the animal's back**,
i.e. texel column 48 of 64. The pink dorsal band is centred there.

Usage: mkcreaturepage.py [out.h]
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
CONCEPT = HERE.parents[1].parent / "Upheaval" / "creature" / "Zixxtrixx" / "Concept"

TILE = 64

# Pigment, measured. Upheaval/creature/Zixxtrixx/PALETTE.md.
GREEN = (120, 184, 68)
PINK = (233, 188, 206)
BLUE = (3, 145, 205)
YELLOW = (243, 232, 142)
ORANGE = (218, 106, 71)

# Where to lift crayon grain from, in the 2000-wide display space of each
# sheet: a patch of solid, evenly-worked colour.
GRAIN_SOURCES = {
    "green": ("Side", (940, 1020, 1160, 1120)),
    "pink": ("Side", (930, 402, 1210, 424)),
    "blue": ("Side", (1215, 585, 1360, 700)),
    "yellow": ("Front", (1268, 574, 1310, 620)),
    "orange": ("Front", (1228, 562, 1248, 606)),
}


def grain(sheet, box, seed):
    """Lift a TILE x TILE multiplier field from real crayon.

    The pigment colour is already known exactly; what the scan adds is how
    UNEVENLY it was laid down. So the patch is reduced to luminance and
    normalised about its own mean -- a multiplier, not a colour.
    """
    im = Image.open(CONCEPT / f"{sheet}.png").convert("RGB")
    s = im.size[0] / 2000.0
    x0, y0, x1, y1 = box
    crop = im.crop((int(x0 * s), int(y0 * s), int(x1 * s), int(y1 * s)))
    # resize with BOX so we average real texels rather than inventing them
    crop = crop.resize((TILE, TILE), Image.BOX if crop.width > TILE else Image.BICUBIC)
    a = np.asarray(crop).astype(np.float64)
    lum = 0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2]
    sat = a.max(2) - a.min(2)

    # REJECT THE INK. Every form on both sheets is outlined in heavy black, and
    # a grain patch that clips one gets a dark bar baked straight into the
    # tile. Ink is dark AND unsaturated; bare paper is bright AND unsaturated.
    # Both are replaced by the median of the honest crayon around them, so the
    # tile keeps the stroke structure without importing the outline.
    good = (lum > 70) & (lum < 246) & (sat > 18)
    if good.sum() < 64:
        return np.ones((TILE, TILE))
    lum = np.where(good, lum, np.median(lum[good]))

    m = lum.mean()
    if m <= 1:
        return np.ones((TILE, TILE))
    g = lum / m
    # keep the stroke structure, drop what is left of the extremes
    return np.clip(g, 0.84, 1.16)


def tint(base, g):
    """base colour modulated by a grain field -> a TILE x TILE RGB tile."""
    out = np.zeros((TILE, TILE, 3), dtype=np.float64)
    for c in range(3):
        out[..., c] = base[c] * g
    return np.clip(out, 0, 255)


def body_tile(g_green, g_pink, rng):
    """The flank, with the dorsal band painted ON it.

    The concept's pink runs ALONG the animal, which in this UV convention is a
    band at constant U -- texel column 48 is the back. Until now that stripe
    had to be geometry, because V restarted at every rigid part so a
    longitudinal marking could not survive. One chain part fixed that; this
    paints it.

    The boundary is deliberately wobbly. A ruler-straight edge is the one thing
    that would announce this was not drawn by hand.
    """
    t = tint(GREEN, g_green)
    p = tint(PINK, g_pink)
    back = 48  # U = 192/256
    half = 6.0
    for y in range(TILE):
        # a hand-drawn edge: two slow incommensurate waves, no randomness, so
        # the page is byte-identical on every machine
        wob = 2.2 * np.sin(y * 0.21) + 1.4 * np.sin(y * 0.077 + 1.1)
        lo = back - half + wob
        hi = back + half + wob
        for x in range(TILE):
            # wrap-aware distance so the band cannot be cut by the seam
            d = min(abs(x - (lo + hi) / 2), TILE - abs(x - (lo + hi) / 2))
            if d < (hi - lo) / 2:
                t[y, x] = p[y, x]
    return t


def build_tiles():
    g = {k: grain(v[0], v[1], i) for i, (k, v) in enumerate(GRAIN_SOURCES.items())}
    tiles = []
    names = []
    tiles.append(body_tile(g["green"], g["pink"], None))
    names.append("body flank, with the dorsal band painted at U=192")
    tiles.append(tint(BLUE, g["blue"]))
    names.append("head and throat")
    tiles.append(tint(YELLOW, g["yellow"]))
    names.append("eye")
    tiles.append(tint(ORANGE, g["orange"]))
    names.append("eye rim / pupil")
    tiles.append(tint(GREEN, g["green"]))
    names.append("tail blade")
    tiles.append(tint(PINK, g["pink"]))
    names.append("crest / blade edging")
    return tiles, names


def to565(r, g, b):
    return ((int(r) >> 3) << 11) | ((int(g) >> 2) << 5) | (int(b) >> 3)


def main():
    tiles, names = build_tiles()
    n = len(tiles)
    # one shared 256-entry palette across every tile, quantised together so a
    # creature's materials cannot fight each other for entries
    sheet = np.concatenate([t.astype(np.uint8) for t in tiles], axis=0)
    img = Image.fromarray(sheet, "RGB").quantize(colors=256, method=Image.MEDIANCUT, dither=Image.NONE)
    idx = np.asarray(img).reshape(n, TILE, TILE)
    pal = img.getpalette()[: 256 * 3]
    pal565 = [to565(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]) for i in range(256)]
    used = len(set(idx.reshape(-1).tolist()))

    out = []
    out.append("// GENERATED by zhaozhou/tools/pack/mkcreaturepage.py")
    out.append("// DO NOT EDIT BY HAND. Regenerate if the concept sheets change.")
    out.append("//")
    out.append("// Zixxtrixx's texture page. The crayon grain in these tiles is LIFTED")
    out.append("// FROM S. HOFER'S SCAN at native resolution and applied as a multiplier")
    out.append("// over the measured pigment -- so the strokes, pressure marks and paper")
    out.append("// tooth are the ones in the drawing, not procedural noise.")
    out.append("//")
    out.append("// U is the ring angle and local +Z maps to world DOWN, so U = 192/256 is")
    out.append("// the animal's BACK. The dorsal band is painted at texel column 48.")
    out.append("//")
    out.append(f"// {n} tiles of {TILE}x{TILE}, {used} of 256 palette entries used.")
    for i, nm in enumerate(names):
        out.append(f"//   tile {i}: {nm}")
    out.append("")
    out.append(f"constexpr int kPageTiles = {n};")
    out.append("constexpr uint16_t kPagePalette[256] = {")
    for i in range(0, 256, 12):
        out.append("    " + " ".join(f"0x{v:04X}," for v in pal565[i : i + 12]))
    out.append("};")
    out.append(f"constexpr uint8_t kPageTexels[{n}][{TILE} * {TILE}] = {{")
    for t in range(n):
        out.append("    {")
        flat = idx[t].reshape(-1).tolist()
        for i in range(0, len(flat), 32):
            out.append("        " + " ".join(f"{v:3d}," for v in flat[i : i + 32]))
        out.append("    },")
    out.append("};")
    out.append("")

    dst = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE.parents[1] / "tools" / "reel" / "zixxtrixx_page.h"
    dst.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")
    print(f"wrote {dst}")
    print(f"  {n} tiles of {TILE}x{TILE}, {used}/256 palette entries")
    for i, nm in enumerate(names):
        print(f"    tile {i}: {nm}")


if __name__ == "__main__":
    main()
