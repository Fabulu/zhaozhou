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
    # 4.5, not 6. A 26-degree-down camera shows mostly the animal's BACK, so
    # a band sized to look right in the side-view drawing dominates the render.
    # Narrowed together with a shallower showcase camera; judged from a render.
    half = 4.5
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
    for y in range(8, 28):
        w = 13.0 * (1.0 - (y - 8) / 20.0) ** 1.3
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
# THE EYES SIT ON THE SIDES (Fabian, asked three times, 2026-08-26: "Eyes
# clearly need to be on the side"). U columns 0 and 32 ARE the side lines of
# the ring (top is 48, belly is 16), so the eye discs are centred exactly
# there, at mid-head V. Each eye rides inside an ORANGE SOCKET, the crescent
# that Front.png shows wrapping the outer edge of the skull.
EYE_ROW = 18     # first texel row of the socket down the head tile
EYE_COL_A = 32   # side line, +Z flank
EYE_COL_B = 0    # side line, -Z flank
EYE_TEX_U = 12   # texels of U for the yellow ball (angle around the head)
EYE_TEX_V = 26   # texels of V for the yellow ball (length along the head)
SOCK_U = 16      # socket footprint, U
SOCK_V = 32      # socket footprint, V


def eye_patch():
    """Sabina drew a better eye than I can model. Take it.

    THE CROP IS TRANSPOSED. On the flank, +U runs VERTICALLY around the body
    and +V runs nose-to-tail -- so the drawing's x axis must land on the tile's
    y axis or the slit pupil comes out horizontal.

    Returns (rgb, alpha) at EYE_TEX_V rows x EYE_TEX_U cols.
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


def paint_eyes(tile, g_orange):
    """Orange socket first, then the drawing's eye on top, on BOTH side lines."""
    rgb, alpha = eye_patch()
    ok = tint(ORANGE, g_orange)
    for col in (EYE_COL_A, EYE_COL_B):
        # the socket: an orange ellipse under the eye, slightly larger
        scy = EYE_ROW + SOCK_V / 2.0 - 0.5
        for j in range(SOCK_V):
            ty = EYE_ROW + j
            if ty < 0 or ty >= TILE:
                continue
            for i in range(SOCK_U):
                tx = (col - SOCK_U // 2 + i) % TILE
                r = np.hypot((i - (SOCK_U - 1) / 2.0) / (SOCK_U / 2.0),
                             (ty - scy) / (SOCK_V / 2.0))
                w = float(np.clip((1.0 - r) * 5.0, 0.0, 1.0))
                if w <= 0.0:
                    continue
                tile[ty, tx] = tile[ty, tx] * (1.0 - w) + ok[ty % TILE, tx] * w
        # the eyeball, centred in the socket
        ey0 = EYE_ROW + (SOCK_V - EYE_TEX_V) // 2
        for j in range(EYE_TEX_V):
            ty = ey0 + j
            if ty < 0 or ty >= TILE:
                continue
            for i in range(EYE_TEX_U):
                tx = (col - EYE_TEX_U // 2 + i) % TILE
                w = alpha[j, i]
                if w <= 0.0:
                    continue
                tile[ty, tx] = tile[ty, tx] * (1.0 - w) + rgb[j, i] * w
    return tile


def head_tile(g_blue, g_green, g_pink, g_orange):
    """The head, laid out the way the SHEETS say (Fabian, 2026-08-26: "you
    made its entire head blue ... you'd know that if you looked at the concept
    art"):

      - BLUE is the FRONT and UNDERSIDE only, and it runs on down the throat
        (the body tile's wedge continues it past this part);
      - the TOP of the skull is PINK -- the dorsal band runs over the crown
        (clear in Side.png), narrowing toward the nose;
      - the rear flanks turn GREEN behind the skull, the same single green as
        the body, so the side view reads green neck / blue throat / pink top;
      - the EYES are on the SIDE LINES (U columns 0 and 32), yellow ball in an
        orange socket, from the drawing itself.
    """
    t = tint(BLUE, g_blue)
    gr = tint(GREEN, g_green)
    pk = tint(PINK, g_pink)
    # widths per V row: the pink cap is WIDE over the skull and narrows to the
    # body band behind it; the throat pinches slightly behind the jaw. What is
    # left between them on the rear rows is the GREEN neck flank -- Side.png
    # shows green riding high on the neck right behind the skull.
    def pink_half(y):
        # the NOSE stays blue: the sheet's pink band fades out before the tip
        if y < 6:
            return 0.0
        if y < 16:
            return 3.0 + (13.0 - 3.0) * (y - 6) / 10.0
        if y < 38:
            return 13.0
        if y < 50:
            return 13.0 - 5.0 * (y - 38) / 12.0
        return 8.0
    def throat_half(y):
        return 12.0 if y < 38 else 10.0
    # green rear flanks: fade in behind the skull; wobbly hand edge
    for x in range(TILE):
        start = 34 + 3.0 * np.sin(x * 0.47) + 2.0 * np.sin(x * 0.13 + 0.7)
        for y in range(max(0, int(start)), TILE):
            dtop = min(abs(x - 48), TILE - abs(x - 48))
            dbel = min(abs(x - 16), TILE - abs(x - 16))
            if dtop <= pink_half(y) or dbel <= throat_half(y):
                continue  # pink cap / blue throat own these columns
            t[y, x] = gr[y, x]
    # pink cap over the crown, narrowing toward the nose and easing back to
    # the body band's width at the tail end of the part
    for y in range(2, TILE):
        half = pink_half(y)
        wob = 1.8 * np.sin(y * 0.23) + 1.1 * np.sin(y * 0.071 + 0.8)
        for x in range(TILE):
            d = min(abs(x - (48 + wob)), TILE - abs(x - (48 + wob)))
            if d < half:
                t[y, x] = pk[y, x]
    t = paint_eyes(t, g_orange)
    # the mouth: Front.png's small white slit on the underside, ink rim
    for y in range(3, 8):
        for x in range(10, 23):
            t[y, x] = INK
    for y in range(4, 7):
        for x in range(11, 22):
            t[y, x] = WHITE
    # V row 0 is the WHOLE nose cap fan's sample row: flat pigment, no streaks
    t[0, :] = BLUE
    return t


def blade_tile(g_green, g_pink, pink_up):
    """A tail blade: PINK on one face, GREEN on the other (Fabian, 2026-08-26:
    "One side of the fin parts at the tail will have the pink, the other the
    green, a bit difficult to texture"). U wraps the blade's flat section, so
    the upper face is the columns around 48 and the lower face the columns
    around 16 -- half the tile each, split with a wobbly hand edge. Two tiles,
    mirrored, so the two blades can disagree about which face is which."""
    gr = tint(GREEN, g_green)
    pk = tint(PINK, g_pink)
    t = np.zeros((TILE, TILE, 3), dtype=np.float64)
    for y in range(TILE):
        wob = 1.6 * np.sin(y * 0.19 + 0.5) + 0.9 * np.sin(y * 0.055 + 1.9)
        for x in range(TILE):
            d = min(abs(x - (48 + wob)), TILE - abs(x - (48 + wob)))
            up = d < 16
            t[y, x] = (pk if up == pink_up else gr)[y, x]
    # cap rows flat so the tip fan cannot streak
    t[0, :] = PINK if pink_up else GREEN
    t[63, :] = PINK if pink_up else GREEN
    return t


def build_tiles():
    g = {k: grain(v[0], v[1], i) for i, (k, v) in enumerate(GRAIN_SOURCES.items())}
    tiles = []
    names = []
    tiles.append(body_tile(g["green"], g["pink"], None))
    names.append("body flank, dorsal band at U=192, throat wedge on the belly")
    tiles.append(head_tile(g["blue"], g["green"], g["pink"], g["orange"]))
    names.append("head: blue front/underside, pink crown, green rear flanks, side eyes")
    tiles.append(tint(YELLOW, g["yellow"]))
    names.append("eye (reserved)")
    tiles.append(tint(ORANGE, g["orange"]))
    names.append("eye rim / pupil (reserved)")
    tiles.append(blade_tile(g["green"], g["pink"], True))
    names.append("tail blade, pink upper face / green lower face")
    tiles.append(blade_tile(g["green"], g["pink"], False))
    names.append("tail blade, green upper face / pink lower face")
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
