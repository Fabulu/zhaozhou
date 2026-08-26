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
# ART DIRECTION OVERRIDE (Fabian, 2026-08-26): "The pink on the back should be
# like neon pink, it's just not even close to strong enough." The sheet's
# measured pale pink (233,188,206) is therefore NOT what ships on the dorsal
# band -- the owner's call beats the measurement.
PINK = (255, 32, 168)
BLUE = (3, 145, 205)
YELLOW = (243, 232, 142)
ORANGE = (218, 106, 71)
WHITE = (246, 246, 246)
INK = (34, 30, 34)

# Where to lift crayon grain from, in the 2000-wide display space of each
# sheet: a patch of solid, evenly-worked colour.
# Measured 2026-08-26 (grain-box probe): the old green box was only 23%
# honest crayon -- it straddled the form's edge, most of it was bare paper,
# and the paper was replaced by the median, which is exactly why the body
# read as one flat colour. The new green box sits deep inside the upper arch
# (89% crayon, grain sd 0.086 vs 0.057). The PALE pink pigment fails the
# saturation gate outright (sd 0.017 -- crayon laid so lightly there is no
# stroke structure to lift), so the pink band borrows the green box's stroke
# field: same pencil, same hand, same direction as the body it rides on.
GRAIN_SOURCES = {
    "green": ("Side", (700, 600, 900, 900)),
    "pink": ("Side", (700, 600, 900, 900)),
    "blue": ("Side", (1215, 585, 1360, 700)),
    "yellow": ("Side", (1440, 580, 1540, 680)),
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
    # AMPLIFY the stroke structure (Fabian, 2026-08-26: "the texture is
    # completely one-colored instead of the crayon-like texture I expected").
    # The raw multiplier field survives the trip to screen at roughly half its
    # amplitude once the light rig and RGB565 have had their say, so push it
    # here, at the source, where the stroke shapes are still real.
    g = 1.0 + (g - 1.0) * 2.1
    return np.clip(g, 0.74, 1.26)


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
    # THE THROAT RUNS ON PAST THE HEAD (both sheets: the blue continues along
    # the underside behind the skull, a teardrop down the chest in Front.png).
    # The head chain covers V rows 0..~12 of this tile; the wedge takes over
    # where it ends and tapers away by row 21. Belly is U = 64 -> column 16.
    b = tint(BLUE, g_green)
    for y in range(8, 22):
        w = 10.0 * (1.0 - (y - 8) / 14.0) ** 1.3
        if w < 0.6:
            continue
        wob = 1.1 * np.sin(y * 0.31)
        for x in range(TILE):
            d = min(abs(x - (16 + wob)), TILE - abs(x - (16 + wob)))
            if d < w:
                t[y, x] = b[y, x]
    # V row 63 is the tail cap fan's whole sample row (cap apex v = 255 and the
    # end ring's v = 255, so every cap pixel lands here): keep it flat pigment
    # so the cap reads as a solid tip, not a streak.
    t[63, :] = GREEN
    return t



# The eye disc in Side.png, in the 2000-wide display space: the yellow ball,
# its heavy black ring, and the red-orange wavy slit pupil through it.
EYE_BOX = (1412, 556, 1568, 712)
EYE_ROW = 20    # first texel row down the head tile (V runs along the head)
# The eyes sit HIGH: the front sheet has them nearly meeting across the top
# of the face. Top is column 48; these put each eye ~45 deg up its flank.
EYE_COL_A = 54  # upper +Z flank
EYE_COL_B = 42  # upper -Z flank
# TEXEL FOOTPRINT (2026-08-26). The first pass painted the eye 26 x 26 -- but
# U texels measure ANGLE around the head, and 26 of 64 is 40% of the whole
# circumference: two such discs tiled nearly the entire skull with yellow,
# which is most of why the face read as mangled. The measured eye is ~220 mm
# on a ~500 mm skull: 52 deg of arc = 9-10 texels of U, and ~23 texels of V
# along a ~600 mm head chain. The patch is anisotropic because the mapping is.
EYE_TEX_U = 12  # texels of U (angle around the head)
EYE_TEX_V = 26  # texels of V (length along the head)


def eye_patch():
    """Sabina drew a better eye than I can model. Take it.

    The eye is not geometry any more. A yellow ball stuck on the side of the
    head was the obvious thing and it looked exactly like what it was -- a
    sphere glued to a tube. MODELINGGUIDE asks for eyes "integrated into the
    head contour" so they influence the SILHOUETTE rather than sitting on it,
    and the cheapest honest way to do that at 240p is: a shallow LATERAL BULGE
    in the head's own rings, with the drawing's own eye painted onto it.

    THE CROP IS TRANSPOSED. On the flank, +U runs VERTICALLY around the body
    and +V runs nose-to-tail -- so the drawing's x axis must land on the tile's
    y axis or the slit pupil comes out horizontal. Drawing-vertical (the slit)
    -> tile x (U) -> vertical on the model, as drawn.

    Returns (rgb, alpha) at EYE_TEX_V rows x EYE_TEX_U cols. Alpha is the
    disc, taken from the ink ring outward, so the eye composites onto the blue
    head without a square edge.
    """
    im = Image.open(CONCEPT / "Side.png").convert("RGB")
    sc = im.size[0] / 2000.0
    x0, y0, x1, y1 = EYE_BOX
    crop = im.crop((int(x0 * sc), int(y0 * sc), int(x1 * sc), int(y1 * sc)))
    crop = crop.transpose(Image.TRANSPOSE)
    crop = crop.resize((EYE_TEX_U, EYE_TEX_V), Image.BOX)
    a = np.asarray(crop).astype(np.float64)
    yy, xx = np.mgrid[0:EYE_TEX_V, 0:EYE_TEX_U]
    cy = (EYE_TEX_V - 1) / 2.0
    cx = (EYE_TEX_U - 1) / 2.0
    r = np.hypot((xx - cx) / cx, (yy - cy) / cy)
    alpha = np.clip((1.02 - r) * 6.0, 0.0, 1.0)
    return a, alpha


def paint_eye(tile):
    """Composite the eye onto both flanks of the head tile, wrapping in U."""
    rgb, alpha = eye_patch()
    for col in (EYE_COL_A, EYE_COL_B):
        for j in range(EYE_TEX_V):
            ty = EYE_ROW + j
            if ty < 0 or ty >= TILE:
                continue
            for i in range(EYE_TEX_U):
                tx = (col - EYE_TEX_U // 2 + i) % TILE  # U wraps around the head
                w = alpha[j, i]
                if w <= 0.0:
                    continue
                tile[ty, tx] = tile[ty, tx] * (1.0 - w) + rgb[j, i] * w
    return tile


def paint_face(tile):
    """The rest of the face: the nose-cap row and the mouth.

    V row 0 is the WHOLE nose cap: the cap apex carries v = 0 and so does ring
    0, so every pixel of the cap fan samples row 0. Any grain there smears into
    angular streaks on the cap -- keep the row flat pigment.

    The mouth is Front.png's small white slit, on the underside just behind
    the nose (belly is U = 64 -> column 16), with a thin ink rim so it reads
    at 240p the way the drawing's ink line does.
    """
    tile[0, :] = BLUE
    for y in range(3, 8):
        for x in range(10, 23):
            tile[y, x] = INK
    for y in range(4, 7):
        for x in range(11, 22):
            tile[y, x] = WHITE
    return tile


def build_tiles():
    g = {k: grain(v[0], v[1], i) for i, (k, v) in enumerate(GRAIN_SOURCES.items())}
    tiles = []
    names = []
    tiles.append(body_tile(g["green"], g["pink"], None))
    names.append("body flank, with the dorsal band painted at U=192")
    tiles.append(paint_face(paint_eye(tint(BLUE, g["blue"]))))
    names.append("head and throat: eyes from the drawing, mouth, flat nose-cap row")
    tiles.append(tint(YELLOW, g["yellow"]))
    names.append("eye")
    tiles.append(tint(ORANGE, g["orange"]))
    names.append("eye rim / pupil")
    gb = tint(GREEN, g["green"])
    gb[0, :] = GREEN
    gb[63, :] = GREEN
    tiles.append(gb)
    names.append("tail blade, green (cap rows flat)")
    pb = tint(PINK, g["pink"])
    pb[0, :] = PINK
    pb[63, :] = PINK
    tiles.append(pb)
    names.append("tail blade, neon pink / spike (cap rows flat)")
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
