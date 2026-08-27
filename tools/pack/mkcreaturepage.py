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
#
# TWO GREENS (Fabian, 2026-08-27: "The concept art has two greens, a darker
# green for the front, and a lighter green for most other things"). Both
# sheets say so once you look: Side.png lays a dark blue-green band between
# the blue throat and the light flank at the neck, and Front.png paints the
# whole front-facing chest in it, while the outer flanks, the rear body and
# the tail run the light yellow-green. Measured (saturated-quartile):
# light ~ (117,196,88) on the arch flank, dark ~ (67,191,105) on the neck
# band -- and then art-directed DARKER for the read: the scanner lightens,
# and at 240p on dark ochre the two must separate at a glance.
GREEN = (122, 192, 70)        # LIGHT green: most of the animal
GREEN_DARK = (44, 146, 86)    # DARK green: the front
# PINK, third pass (Fabian, 2026-08-27): "The pink for the entire creature can
# be a bit less neon and a bit more like on the sketch. A bit neon-y is fine
# though." So: pulled ~40% of the way from the 2026-08-26 neon (255,32,168)
# toward the measured sheet pigment (233,188,206) -- NOT all the way back to
# the pale rose that failed on dark ground at 240p. Judged on a render.
PINK = (246, 94, 183)
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
    # the dark green's own strokes: the bottom curve of the S, 85% honest
    # crayon by the same probe that vetted the light box
    "green_dark": ("Side", (1050, 1150, 1350, 1330)),
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


DEBUG_IDENT = False  # debug fingerprint of the pink sources (leave off)
def body_tile(g_green, g_green_dark, g_pink, rng):
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
    p = tint((255, 0, 0) if DEBUG_IDENT else PINK, g_pink)
    # V ORIGIN MOVED, 2026-08-27 head-only run: the head part is no longer an
    # overlay, so the body part now starts AT the junction station (11 of 57,
    # x = 599 mm) and this tile's 64 V rows span stations 11..56 -- ~38.9 mm
    # per row instead of ~54.5 with row 0 at the START OF THE NECK, not the
    # nose. Every row range below was re-derived for that mapping.
    # THE DARK FRONT GREEN (Fabian, 2026-08-27). Side.png lays it between the
    # blue throat and the light flank; Front.png paints the whole chest in it.
    # So: a belly-adjacent band (columns around U=64 -> texel 16) over the
    # FRONT rows of the body, widest right behind the head, fading out by
    # mid-body with a wobbly hand edge. Painted before the dorsal band and
    # the throat wedge so both stay on top.
    dk = tint(GREEN_DARK, g_green_dark)
    for y in range(0, 35):
        # width of the dark band around the belly line, in texels of U
        w = 24.0 * (1.0 - (y / 33.0) ** 1.6)
        if w < 1.0:
            continue
        wob = 2.0 * np.sin(y * 0.24 + 0.4) + 1.2 * np.sin(y * 0.066 + 1.7)
        for x in range(TILE):
            d = min(abs(x - (16 + wob)), TILE - abs(x - (16 + wob)))
            if d < w:
                t[y, x] = dk[y, x]
    back = 48  # U = 192/256
    # 13, not 4.5 (Fabian, 2026-08-27: "the pink should cover the entire top
    # of the snake, right now it's just a thin line. Top also means part of
    # the sideways dropoff. The part you see if you look at it from the
    # top"). Half-width 13 of 64 texels is ~41% of the circumference centred
    # on the back -- the upper surface INCLUDING the shoulder of the tube,
    # green keeping the flanks and belly. The old 4.5 was itself a narrowing
    # (6 -> 4.5) made to compensate a 26-degree-down showcase camera that
    # showed mostly back; the camera has since flattened to ~15 degrees, so
    # that compensation was for a problem that no longer exists.
    #
    # The neck thinning (rows <= 17) is GONE with it: it existed so the neck
    # hook's back would not merge with the head cap into one pink mass under
    # the old steep camera. A gentle taper at the very first rows keeps the
    # junction row matched to the head tile's crown width.
    for y in range(TILE):
        half = 13.0
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
    # The head part now ends exactly where this tile begins, so the wedge
    # starts AT row 0 -- matched to the head tile's ~10-texel throat width so
    # the two read as one marking across the junction -- and tapers away by
    # row 19 (~1.3 m behind the nose, the owner's authored length).
    # LONGER AND FULLER, 2026-08-28 (Fabian: "The blue should also go down
    # its front body a bit before it goes into the dark green ... We need a
    # bit more of that in the front body" -- and Side.png runs the blue a
    # substantial way back along the tube). The wedge now HOLDS its full
    # width for the first rows and tapers out by row 30 (~1.2 m behind the
    # junction), so the read is blue -> dark green -> light green with the
    # blue owning the chest.
    b = tint(BLUE, g_green)
    for y in range(0, 31):
        yt = max(0.0, (y - 10) / 20.0)
        w = 12.0 * (1.0 - yt) ** 1.3
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
# its heavy black ring, and the red-orange slit pupil through it. The box is
# sized to hold the WHOLE disc including where the slit meets the rim at top
# and bottom -- the old box clipped the right and bottom edges.
EYE_BOX = (1418, 556, 1594, 734)
# THE EYES SIT ON THE SIDES (Fabian, asked three times, 2026-08-26: "Eyes
# clearly need to be on the side"). U columns 0 and 32 ARE the side lines of
# the ring (top is 48, belly is 16), so the eye discs are centred exactly
# there, at mid-head V.
#
# NO SOCKET. 2026-08-27, Fabian: "the eye texture is wrong. The orange goes
# from top to bottom, it's a line and in the middle there's a larger part."
# The orange is the drawing's own PUPIL -- a top-to-bottom slit that swells
# in the middle -- and it is already inside EYE_BOX. The orange ellipse this
# file used to paint UNDER the eye was an invention, and it is what read as
# a socket/ring. Deleted; the drawn eye is painted alone, slightly larger.
EYE_ROW = 12     # first texel row of the eyeball down the head tile.
                 # 12, was 19 (Fabian, 2026-08-27: "eyes need to be more in
                 # front. Not completely, they still should be mostly on the
                 # side"): on the BALL skull the rows forward of the radial
                 # peak (station 4 ~ V row 23) face increasingly FORWARD, so
                 # sliding the eye 7 rows nose-ward wraps its front edge onto
                 # the frontal silhouette while the centre stays on the side
                 # line. Judged on head-on + side zooms, not derived.
EYE_COL_A = 38   # +Z flank, RAISED 4 texels toward the back line (Fabian,
                 # 2026-08-28: "Eyes should be more visible in front. Move
                 # them up a little") -- toward U=48 is up on the ring
EYE_COL_B = 58   # -Z flank, raised the same 4 texels (0 -> 60, wrapping
                 # toward the back line from the other side)
EYE_TEX_U = 17   # texels of U for the yellow ball (angle around the head).
                 # 17, was 15: the sheet's eye is a LARGE disc, a
                 # substantial fraction of the head's height
EYE_TEX_V = 33   # texels of V for the yellow ball (length along the head)
# THE ORANGE SURROND (Fabian, ruled YES; Front.png brackets each eye in
# orange/red). Painted UNDER the lifted eye disc as a feathered ring a few
# texels wider than the ball, so the drawn eye sits inside an orange rim
# that reads head-on the way the front sheet draws it.
EYE_RING_EXTRA = 2     # texels of surround beyond the ball's edge
EYE_RING_RGB = (206, 88, 46)  # judged on the front render, not the scan
# ORIENTATION KNOB. -30 RE-PICKED 2026-08-27 round-skull run (Fabian: "pupils
# should be rotated right"): settled off an 8-angle fan of the PAINTED tile
# viewed through the true screen mapping (U up, nose right), then confirmed
# on a reel head zoom -- at -30 the pupil is the sheet's vertical
# top-to-bottom band with the middle swell. The old note said 12 was
# "verified by head-zoom render"; the ball skull and the forward eye move
# changed the read.
# (original note: the eye texture should be rotate
# a bit more than 90 degrees counter clockwise. That way the orange pupil
# should look about right"). Degrees of extra rotation applied to the crop
# AFTER the transpose, positive = counter-clockwise AS RENDERED on the flank
# (verified by head-zoom render, not derived). The disc and its ink ring are
# rotationally symmetric, so only the pupil band moves.
EYE_ROT_DEG = -30


def eye_patch(u_tex=None, v_tex=None):
    """Sabina drew a better eye than I can model. Take it.

    THE CROP IS TRANSPOSED. On the flank, +U runs VERTICALLY around the body
    and +V runs nose-to-tail -- so the drawing's x axis must land on the tile's
    y axis or the slit pupil comes out horizontal.

    Returns (rgb, alpha) at v_tex rows x u_tex cols (defaults EYE_TEX_*).
    """
    if u_tex is None:
        u_tex = EYE_TEX_U
    if v_tex is None:
        v_tex = EYE_TEX_V
    im = Image.open(CONCEPT / "Side.png").convert("RGB")
    sc = im.size[0] / 2000.0
    x0, y0, x1, y1 = EYE_BOX
    crop = im.crop((int(x0 * sc), int(y0 * sc), int(x1 * sc), int(y1 * sc)))
    crop = crop.transpose(Image.TRANSPOSE)
    # EYE_ROT_DEG: spin the disc so the pupil band lands the way the owner
    # asked (see the knob above). The crop is near-square and the elliptical
    # alpha below clips the corners, so the rotation's fill never shows; the
    # fill colour is the ball yellow just in case a texel of it survives.
    if EYE_ROT_DEG:
        crop = crop.rotate(EYE_ROT_DEG, resample=Image.BICUBIC,
                           fillcolor=(250, 226, 120))
    crop = crop.resize((u_tex, v_tex), Image.BOX)
    a = np.asarray(crop).astype(np.float64)
    # SATURATE TOWARD THE READ (2026-08-27). The raw lift shipped the SCAN's
    # values, and the scan is pale: at 240p under the key light the yellow
    # ball read as beige and the pupil as brown -- the dorsal-pink lesson
    # again (matching the paper is not matching the READ). Classify each
    # texel by what the artist MEANT -- ink ring, orange pupil, yellow ball
    # -- and pull it two thirds of the way to a saturated version of that
    # intent, keeping a third of the original so the hand wobble survives.
    lum = a @ [0.299, 0.587, 0.114]
    # red-minus-GREEN separates the orange pupil from the yellow ball: the
    # scanned yellow is (250,230,150)-ish so red-minus-BLUE flags IT too
    # (first attempt painted the whole disc orange with a yellow rim --
    # exactly inverted). Orange has R >> G; yellow has R ~ G.
    rg = a[..., 0] - a[..., 1]
    ink = lum < 96
    pupil = (~ink) & (rg > 55) & (a[..., 0] > 120)
    ball = (~ink) & (~pupil)
    tgt = np.zeros_like(a)
    tgt[ink] = (26, 22, 26)
    tgt[pupil] = (234, 88, 30)
    tgt[ball] = (252, 224, 74)
    a = a * 0.34 + tgt * 0.66
    yy, xx = np.mgrid[0:v_tex, 0:u_tex]
    cy = (v_tex - 1) / 2.0
    cx = (u_tex - 1) / 2.0
    r = np.hypot((xx - cx) / cx, (yy - cy) / cy)
    alpha = np.clip((1.02 - r) * 6.0, 0.0, 1.0)
    return a, alpha


def paint_eyes(tile, g_orange):
    """The drawing's own eye -- yellow ball, ink rim, top-to-bottom orange
    slit swelling in the middle -- on BOTH side lines. Nothing else: the
    invented orange socket is gone (see the note at EYE_BOX)."""
    rgb, alpha = eye_patch()
    ring = tint(EYE_RING_RGB, g_orange) if g_orange is not None else None
    for col in (EYE_COL_A, EYE_COL_B):
        # the orange surround first, so the drawn eye sits ON it: a feathered
        # ellipse EYE_RING_EXTRA texels wider than the ball in each axis
        if ring is not None:
            ru = EYE_TEX_U / 2.0 + EYE_RING_EXTRA
            rv = EYE_TEX_V / 2.0 + EYE_RING_EXTRA
            cy = EYE_ROW + (EYE_TEX_V - 1) / 2.0
            for ty in range(max(0, int(cy - rv) - 1), min(TILE, int(cy + rv) + 2)):
                for i in range(-int(ru) - 1, int(ru) + 2):
                    tx = (col + i) % TILE
                    r = np.hypot(i / ru, (ty - cy) / rv)
                    w = np.clip((1.04 - r) * 5.0, 0.0, 1.0)
                    if w <= 0.0:
                        continue
                    tile[ty, tx] = tile[ty, tx] * (1.0 - w) + ring[ty, tx] * w
        for j in range(EYE_TEX_V):
            ty = EYE_ROW + j
            if ty < 0 or ty >= TILE:
                continue
            for i in range(EYE_TEX_U):
                tx = (col - EYE_TEX_U // 2 + i) % TILE
                w = alpha[j, i]
                if w <= 0.0:
                    continue
                tile[ty, tx] = tile[ty, tx] * (1.0 - w) + rgb[j, i] * w
    return tile


def head_tile(g_blue, g_green_dark, g_pink, g_orange):
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
    # the neck flanks behind the skull are the FRONT of the animal, so they
    # take the DARK green (Fabian, 2026-08-27) -- the light green begins on
    # the body tile behind them
    t = tint(BLUE, g_blue)
    gr = tint(GREEN_DARK, g_green_dark)
    pk = tint((0, 255, 255) if DEBUG_IDENT else PINK, g_pink)
    # widths per V row: the pink cap is WIDE over the skull and narrows to the
    # body band behind it; the throat pinches slightly behind the jaw. What is
    # left between them on the rear rows is the GREEN neck flank -- Side.png
    # shows green riding high on the neck right behind the skull.
    def pink_half(y):
        # THE PINK RUNS TO THE NOSE TIP (2026-08-28; Side.png -- the shape
        # authority -- draws the dorsal band continuing OVER the head to the
        # very tip, and Fabian: "Top of head should be pink, but from front
        # you only see the blue"). A slim band from the first dome rows,
        # widening over the crown to the body band's half-width by y=30 and
        # holding it to the junction, so head-on the top of the head reads
        # PINK with the blue face beneath it.
        # the band begins BEHIND the visible dome (y=16 ~ station 3): the
        # front read wins -- a band on the dome rows painted a pink stripe
        # down the middle of the face head-on (r5/r6 renders), which
        # Front.png does not draw. From the side the crown pink now starts
        # just behind the nose and widens over the skull.
        if y < 16:
            return 0.0
        if y < 32:
            return 4.0 + (13.0 - 4.0) * (y - 16) / 16.0
        return 13.0
    def throat_half(y):
        return 12.0 if y < 38 else 10.0
    # green rear flanks: fade in behind the skull; wobbly hand edge.
    # PUSHED BACK 2026-08-27 pass 3 (34 -> 40): at 34 the green covered the
    # rear half of the SKULL, and a head-on view showed it as a green rim
    # ringing the face -- Front.png's ball has no green on it at all. At 40
    # the green begins on the neck rows only.
    # ...and again in the head-only run (40 -> 47): the skull is now LEVEL
    # on its own bone, so a frontal camera sees its whole crown -- at 40 the
    # ball's top read as a green cap where Front.png paints blue and pink.
    # 47 leaves the green to the blend-zone neck rows.
    for x in range(TILE):
        start = 47 + 3.0 * np.sin(x * 0.47) + 2.0 * np.sin(x * 0.13 + 0.7)
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
    # the mouth. SHRUNK AGAIN, 2026-08-27 head-only run (Headache.md: the
    # pass-3 "85-95 deg" claim measured 18 of 64 angular texels = ~101 deg
    # of circumference -- a grin wrapping a quarter of the head). Now a
    # SMALL WEIRD SLIT: 9 ink texels of U (~51 deg), 4 rows, a ONE-texel ink
    # boundary, and hand-wobbled per row -- each row's ends shift by one so
    # no two rows share both endpoints and the slit reads drawn, not
    # stamped. Slightly off-centre of the belly line (16) toward +U, the
    # way Side.png's mouth sits. Still on the dome rows (V 1..5): the
    # dome's lower half faces the lifted head's camera.
    mouth = [  # (v row, ink x0..x1 inclusive) -- white is the row inset 1
        (1, 13, 20),
        (2, 12, 21),
        (3, 13, 21),
        (4, 14, 20),
    ]
    for y, x0, x1 in mouth:
        for x in range(x0, x1 + 1):
            t[y, x] = INK
    for y, x0, x1 in mouth[1:3]:  # white only in the middle rows
        for x in range(x0 + 1, x1):
            t[y, x] = WHITE
    # V row 0 is the WHOLE nose cap fan's sample row: flat pigment, no streaks
    t[0, :] = BLUE
    return t


def blade_tile(g_green, g_pink, green_at_lead):
    """A tail blade: BOTH faces carry BOTH colours (Fabian, 2026-08-27: "right
    now you have one side of fin colored pink, the other green. I think both
    sides should actually have both colors. big slice of pink, weaker slice of
    creen"). U wraps the blade's flat section -- upper face is the columns
    around 48, lower face around 16, the thin edges at 0 and 32. Each face is
    a BIG pink slice with a WEAKER green slice laid along one edge, split with
    a wobbly hand edge. The two tiles put the green slice along opposite
    edges so the two blades stay distinguishable."""
    gr = tint(GREEN, g_green)
    pk = tint((255, 128, 0) if DEBUG_IDENT else PINK, g_pink)
    t = np.zeros((TILE, TILE, 3), dtype=np.float64)
    edge = 0 if green_at_lead else 32  # which thin edge the green rides
    for y in range(TILE):
        wob = 1.6 * np.sin(y * 0.19 + 0.5) + 0.9 * np.sin(y * 0.055 + 1.9)
        for x in range(TILE):
            # wrap-aware distance from the green edge line: within ~10 texels
            # of it (on either face) is the weaker green slice, the rest of
            # both faces stays pink -- ~2/3 pink, ~1/3 green per face
            d = min(abs(x - (edge + wob)), TILE - abs(x - (edge + wob)))
            t[y, x] = (gr if d < 10 else pk)[y, x]
    # cap rows flat so the tip fan cannot streak
    t[0, :] = PINK
    t[63, :] = PINK
    return t


def build_tiles():
    g = {k: grain(v[0], v[1], i) for i, (k, v) in enumerate(GRAIN_SOURCES.items())}
    tiles = []
    names = []
    tiles.append(body_tile(g["green"], g["green_dark"], g["pink"], None))
    names.append("body flank, dark-green front band, dorsal band at U=192, throat wedge")
    tiles.append(head_tile(g["blue"], g["green_dark"], g["pink"], g["orange"]))
    names.append("head: blue front/underside, pink crown, dark-green rear flanks, side eyes")
    tiles.append(tint(YELLOW, g["yellow"]))
    names.append("eye (reserved)")
    tiles.append(tint(ORANGE, g["orange"]))
    names.append("eye rim / pupil (reserved)")
    tiles.append(blade_tile(g["green"], g["pink"], True))
    names.append("tail blade, both faces pink with green slice at the U0 edge")
    tiles.append(blade_tile(g["green"], g["pink"], False))
    names.append("tail blade, both faces pink with green slice at the U32 edge")
    return tiles, names



# ======================= THE CONTINUOUS BODY ATLAS (T4/T5/T6) ==============
# One 128x256 RGB565 page for the whole head+body shell: U = circumference
# (column 96 = the back, 32 = the belly, 64 and 0 = the side lines), V =
# nose-to-tail (row 0 = nose tip, row V_JUNCTION = the head/body junction
# station, row 255 = the fork). No repeated 64-px tile, no V restart at the
# junction: one continuous surface, ~11.9 mm of animal per row everywhere.
# The fins stay on their own small 64x64 pages (bilinear + mips BLEED across
# atlas neighbours, so unrelated regions must not share one).
ATLAS_W, ATLAS_H = 128, 256
V_JUNCTION = 50           # station 11 of 57 = x 599 mm of 3050 -> 50 of 255
BACK_COL, BELLY_COL = 96, 32
SIDE_A_COL, SIDE_B_COL = 64, 0

# ---- T5: the multi-scale crayon law ---------------------------------------
# Three honest scan-derived scales, each a named knob (art law: the eye
# chooses the values; these were judged on renders at 240p, not measured):
#   COVERAGE  low frequency: where the crayon was pressed harder over tens
#             of texels -- survives every mip level;
#   STROKES   mid frequency: the directional stroke field, quilted from
#             several real patches so nothing repeats down the animal;
#   TOOTH     high frequency: paper grain, subtle, FADES with mip level
#             (T6) because at distance it can only alias.
COVERAGE_AMP = 0.11
STROKE_AMP = 0.24
TOOTH_AMP = 0.10
QUILT_PATCH = 40          # quilting patch edge, atlas texels
QUILT_OVERLAP = 16        # blended overlap between patches
QUILT_SEED = 20260828     # fixed seed; the generated bytes are committed
# Slight PER-MATERIAL HUE DRIFT (T5): the pigment wanders between two
# hand-picked anchors on a slow deterministic field, so large areas stop
# reading as one flat colour with noise on it. Anchor pairs are owner knobs.
GREEN_DRIFT = (134, 188, 52)       # light green wanders toward yellow-olive
GREEN_DARK_DRIFT = (38, 142, 108)  # dark green wanders toward blue-green
PINK_DRIFT = (238, 120, 168)       # pink gets small warm pressure changes
BLUE_DRIFT = (24, 132, 182)        # blue varies a little in saturation
HUE_DRIFT_AMP = 0.35      # 0 = no drift, 1 = full anchor at field peaks

# larger stroke crops for quilting (display-space boxes on the sheets);
# several per pigment so the quilt has real variety to draw from
STROKE_SOURCES = {
    "green": [("Side", (700, 560, 980, 930)), ("Side", (900, 1050, 1250, 1300)),
              ("Side", (1050, 700, 1250, 950))],
    "green_dark": [("Side", (1050, 1150, 1350, 1330)), ("Side", (760, 480, 1000, 620))],
    "pink": [("Side", (700, 560, 980, 930)), ("Side", (900, 1050, 1250, 1300))],
    "blue": [("Side", (1215, 585, 1360, 700)), ("Side", (1150, 640, 1300, 760))],
}


def _lift_lum(sheet, box):
    """A luminance multiplier field from a sheet crop at native resolution,
    ink/paper rejected the same way grain() does."""
    im = Image.open(CONCEPT / f"{sheet}.png").convert("RGB")
    sc = im.size[0] / 2000.0
    x0, y0, x1, y1 = box
    crop = im.crop((int(x0 * sc), int(y0 * sc), int(x1 * sc), int(y1 * sc)))
    a = np.asarray(crop).astype(np.float64)
    lum = 0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2]
    sat = a.max(2) - a.min(2)
    good = (lum > 70) & (lum < 246) & (sat > 18)
    if good.sum() < 256:
        return np.ones((64, 64))
    med = np.median(lum[good])
    lum = np.where(good, lum, med)
    m = lum.mean()
    return lum / m if m > 1 else np.ones_like(lum)


def _resize(a, w, h):
    return np.asarray(Image.fromarray(
        np.clip(a * 128.0, 0, 255).astype(np.uint8)).resize((w, h), Image.BILINEAR)
    ).astype(np.float64) / 128.0


def quilt_field(mat, w, h):
    """T5 stroke layer: deterministic patch quilting with overlap blending.
    Patches are drawn from several real crops at near-native stroke scale and
    laid with a fixed-seed rng -- no square repeats down the animal."""
    rng = np.random.default_rng(QUILT_SEED + hash(mat) % 1000)
    srcs = [_lift_lum(sh, box) for sh, box in STROKE_SOURCES[mat]]
    out = np.zeros((h, w))
    wgt = np.zeros((h, w))
    step = QUILT_PATCH - QUILT_OVERLAP
    ramp = np.minimum(np.arange(QUILT_PATCH) + 1,
                      np.arange(QUILT_PATCH)[::-1] + 1)
    ramp = np.minimum(ramp / float(QUILT_OVERLAP), 1.0)
    pw = np.outer(ramp, ramp)
    for py in range(0, h, step):
        for px in range(0, w, step):
            src = srcs[int(rng.integers(len(srcs)))]
            sh_, sw_ = src.shape
            # native-scale window, downsampled ~2x so strokes read at texel scale
            win = min(sh_, sw_, QUILT_PATCH * 2)
            oy = int(rng.integers(0, sh_ - win + 1))
            ox = int(rng.integers(0, sw_ - win + 1))
            patch = _resize(src[oy:oy + win, ox:ox + win], QUILT_PATCH, QUILT_PATCH)
            if rng.integers(2):  # the hand turns the paper sometimes
                patch = patch[:, ::-1]
            ph_ = min(QUILT_PATCH, h - py)
            pw_ = min(QUILT_PATCH, w - px)
            out[py:py + ph_, px:px + pw_] += (patch[:ph_, :pw_] - 1.0) * pw[:ph_, :pw_]
            wgt[py:py + ph_, px:px + pw_] += pw[:ph_, :pw_]
    return 1.0 + out / np.maximum(wgt, 1e-6)


def multiscale(mat, w, h, seed_off=0):
    """coverage x strokes (+ tooth separately: the mip builder fades it)."""
    rng = np.random.default_rng(QUILT_SEED + 77 + seed_off)
    # coverage: a real crop reduced far below texel scale then blown up smooth
    cov_src = _lift_lum(*STROKE_SOURCES[mat][0])
    cov = _resize(_resize(cov_src, 8, 16), w, h)
    strokes = quilt_field(mat, w, h)
    # tooth: fine residual of a native crop against its own blur
    t_src = _lift_lum(*STROKE_SOURCES[mat][-1])
    t = _resize(t_src, w * 2, h * 2)
    t_blur = _resize(_resize(t, w // 2, h // 2), w * 2, h * 2)
    tooth = _resize(t - t_blur + 1.0, w, h)
    base = (1.0 + (cov - 1.0) * (COVERAGE_AMP / 0.10)
            ) * (1.0 + (strokes - 1.0) * (STROKE_AMP / 0.18))
    tooth = 1.0 + (tooth - 1.0) * (TOOTH_AMP / 0.03)
    return np.clip(base, 0.62, 1.38), np.clip(tooth, 0.82, 1.18)


def drift_field(w, h, phase):
    """slow deterministic 0..1 field for the hue drift, incommensurate waves"""
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    f = (np.sin(yy * 0.055 + phase) * 0.5 + np.sin(yy * 0.021 + xx * 0.017 + phase * 1.7) * 0.35
         + np.sin(xx * 0.061 + phase * 0.6) * 0.15)
    return (f * 0.5 + 0.5) * HUE_DRIFT_AMP


def tinted(base, drift_anchor, mult, drift, tooth=None):
    """pigment -> drifted colour -> multiplied by the crayon field(s)"""
    h, w = mult.shape
    out = np.zeros((h, w, 3))
    for c in range(3):
        col = base[c] * (1.0 - drift) + drift_anchor[c] * drift
        out[..., c] = col * mult * (tooth if tooth is not None else 1.0)
    return np.clip(out, 0, 255)



# atlas-space constants (the tile-space knobs x2 in U, x50/64 or x205/64 in V)
A_EYE_COL_A = EYE_COL_A * 2          # 76
A_EYE_COL_B = EYE_COL_B * 2          # 116
A_EYE_ROW = 9                        # head-tile row 12 -> atlas V 9
A_EYE_U = 34                         # 17 tile texels of angle -> 34 atlas cols
A_EYE_V = 26                         # 33 head rows -> 26 atlas rows
A_EYE_RING = 4                       # orange surround, atlas texels of U
DITHER_AMP = 0.9                     # T6 ordered dither, 0..1 of one 565 step
_BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6],
                    [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float64) / 16.0


def build_atlas():
    """T4+T5: the one continuous 128x256 body atlas. Returns (rgb, tooth)."""
    mats = {}
    for i, m in enumerate(["green", "green_dark", "pink", "blue"]):
        mats[m] = multiscale(m, ATLAS_W, ATLAS_H, i)
    W, H = ATLAS_W, ATLAS_H
    d_green = drift_field(W, H, 0.3)
    d_dark = drift_field(W, H, 2.1)
    d_pink = drift_field(W, H, 4.0)
    d_blue = drift_field(W, H, 5.6)
    g_rgb = tinted(GREEN, GREEN_DRIFT, mats["green"][0], d_green)
    dk_rgb = tinted(GREEN_DARK, GREEN_DARK_DRIFT, mats["green_dark"][0], d_dark)
    pk_rgb = tinted(PINK, PINK_DRIFT, mats["pink"][0], d_pink)
    bl_rgb = tinted(BLUE, BLUE_DRIFT, mats["blue"][0], d_blue)
    rgb = g_rgb.copy()
    tooth = mats["green"][1].copy()

    def wrapd(x, c):
        return min(abs(x - c), W - abs(x - c))

    # TWO-TONE FLANK (Side.png: the green is darker on the upper flank).
    # A soft dark blend riding just OUTSIDE the dorsal band's edge, body only.
    for y in range(V_JUNCTION, H):
        wob = 3.2 * np.sin(y * 0.055 + 0.9)
        for x in range(W):
            d = wrapd(x, BACK_COL + wob)
            if 26.0 < d < 44.0:
                t = 1.0 - abs(d - 35.0) / 9.0
                w = 0.22 * max(0.0, t)
                rgb[y, x] = rgb[y, x] * (1 - w) + dk_rgb[y, x] * w

    # DARK GREEN CHEST (belly-adjacent, front body, widest behind the head)
    for y in range(V_JUNCTION, 163):
        yt = (y - V_JUNCTION) / 112.0
        w = 48.0 * (1.0 - yt ** 1.6)
        if w < 2.0:
            continue
        wob = 4.0 * np.sin(y * 0.075 + 0.4) + 2.4 * np.sin(y * 0.021 + 1.7)
        for x in range(W):
            if wrapd(x, BELLY_COL + wob) < w:
                rgb[y, x] = dk_rgb[y, x]
                tooth[y, x] = mats["green_dark"][1][y, x]

    # HEAD: blue face + dark-green rear flanks (the junction rows)
    for x in range(W):
        start = int(37 + 2.3 * np.sin(x * 0.24) + 1.6 * np.sin(x * 0.065 + 0.7))
        for y in range(0, V_JUNCTION):
            if y < start:
                rgb[y, x] = bl_rgb[y, x]
                tooth[y, x] = mats["blue"][1][y, x]
            else:
                rgb[y, x] = dk_rgb[y, x]
                tooth[y, x] = mats["green_dark"][1][y, x]

    # BLUE THROAT running on down the chest (holds width, tapers out ~row 146)
    for y in range(V_JUNCTION, 147):
        yt = max(0.0, (y - 82) / 64.0)
        w = 24.0 * (1.0 - yt) ** 1.3
        if w < 1.2:
            continue
        wob = 2.2 * np.sin(y * 0.097)
        for x in range(W):
            if wrapd(x, BELLY_COL + wob) < w:
                rgb[y, x] = bl_rgb[y, x]
                tooth[y, x] = mats["blue"][1][y, x]

    # PINK DORSAL BAND, crown to fork, one continuous top (starts behind the
    # dome so the frontal face is not split -- run 2339 r5/r6 lesson)
    def pink_half_atlas(y):
        if y < 12:
            return 0.0
        if y < 25:
            return 8.0 + (26.0 - 8.0) * (y - 12) / 13.0
        return 26.0
    for y in range(2, H):
        half = pink_half_atlas(y)
        if half <= 0:
            continue
        wob = 3.4 * np.sin(y * 0.058 + 0.8) + 2.0 * np.sin(y * 0.019 + 1.1)
        for x in range(W):
            if wrapd(x, BACK_COL + wob) < half:
                rgb[y, x] = pk_rgb[y, x]
                tooth[y, x] = mats["pink"][1][y, x]

    # EYES on the side lines (raised toward the back), orange surround under
    # the drawn disc
    er, ea = eye_patch(A_EYE_U, A_EYE_V)
    ring_flat = np.array(EYE_RING_RGB, dtype=np.float64)
    for col in (A_EYE_COL_A, A_EYE_COL_B):
        ru = A_EYE_U / 2.0 + A_EYE_RING
        rv = A_EYE_V / 2.0 + A_EYE_RING * 0.78
        cy = A_EYE_ROW + (A_EYE_V - 1) / 2.0
        for ty in range(max(0, int(cy - rv) - 1), min(H, int(cy + rv) + 2)):
            for i in range(-int(ru) - 1, int(ru) + 2):
                tx = (col + i) % W
                r = np.hypot(i / ru, (ty - cy) / rv)
                w = np.clip((1.04 - r) * 5.0, 0.0, 1.0)
                if w > 0:
                    px = ring_flat * mats["green"][0][ty, tx]
                    rgb[ty, tx] = rgb[ty, tx] * (1 - w) + np.clip(px, 0, 255) * w
        for j in range(A_EYE_V):
            ty = A_EYE_ROW + j
            if ty < 0 or ty >= H:
                continue
            for i in range(A_EYE_U):
                tx = (col - A_EYE_U // 2 + i) % W
                w = ea[j, i]
                if w > 0:
                    rgb[ty, tx] = rgb[ty, tx] * (1 - w) + er[j, i] * w
                    tooth[ty, tx] = 1.0

    # the mouth, small weird slit on the dome rows near the belly line
    mouth = [(1, 26, 41), (2, 24, 43), (3, 27, 42)]
    for y, x0, x1 in mouth:
        for x in range(x0, x1 + 1):
            rgb[y, x] = INK
            tooth[y, x] = 1.0
    for x in range(mouth[1][1] + 2, mouth[1][2] - 1):
        rgb[2, x] = WHITE

    # cap sample rows: flat pigment so the fans read solid
    rgb[0, :] = BLUE
    rgb[255, :] = GREEN
    tooth[0, :] = 1.0
    tooth[255, :] = 1.0
    return rgb, tooth


def atlas_mip_words(rgb, tooth):
    """T6: area-filter each level in RGB888 from the full-res source, fade
    the tooth with level, quantise each level to RGB565 with a stable
    texture-space ordered dither (never per frame; never downsample
    quantised values). Level offsets follow level_offset_texels."""
    W, H = ATLAS_W, ATLAS_H
    full = Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), "RGB")
    toothim = Image.fromarray(np.clip(tooth * 128.0, 0, 255).astype(np.uint8))
    words = []
    for l in range(8):
        w, h = max(W >> l, 1), max(H >> l, 1)
        base = np.asarray(full.resize((w, h), Image.BOX)).astype(np.float64)
        tl = np.asarray(toothim.resize((w, h), Image.BOX)).astype(np.float64) / 128.0
        fade = 1.0 / (1 << l)  # tooth halves per level (T6)
        lv = base * (1.0 + (tl - 1.0) * fade)[..., None]
        yy, xx = np.mgrid[0:h, 0:w]
        bay = _BAYER4[yy % 4, xx % 4] - 0.5
        lv = lv + bay[..., None] * np.array([8.0, 4.0, 8.0]) * DITHER_AMP
        lv = np.clip(lv, 0, 255).astype(np.int64)
        for row in lv.reshape(w * h, 3):
            words.append(to565(int(row[0]), int(row[1]), int(row[2])))
    return words

def to565(r, g, b):
    return ((int(r) >> 3) << 11) | ((int(g) >> 2) << 5) | (int(b) >> 3)


def debug_atlas():
    """T7 debug field at ATLAS size: coloured U sectors (col 96 back WHITE,
    32 belly BLACK, 64/0 sides RED/BLUE), V bands + yellow count lines."""
    t = np.zeros((ATLAS_H, ATLAS_W, 3), dtype=np.float64)
    for x in range(ATLAS_W):
        def wd(c):
            return min(abs(x - c), ATLAS_W - abs(x - c))
        if wd(BACK_COL) <= 6:
            base = (255, 255, 255)
        elif wd(BELLY_COL) <= 6:
            base = (30, 30, 30)
        elif wd(SIDE_A_COL) <= 6:
            base = (230, 40, 40)
        elif wd(SIDE_B_COL) <= 6:
            base = (40, 80, 230)
        elif 6 < x < 26:
            base = (60, 160, 160)
        elif 38 < x < 58:
            base = (160, 60, 160)
        elif 70 < x < 90:
            base = (200, 140, 40)
        else:
            base = (90, 200, 60)
        for y in range(ATLAS_H):
            k = 0.55 + 0.45 * ((y % 16) / 15.0)
            t[y, x] = tuple(c * k for c in base)
    for y in range(0, ATLAS_H, 32):
        t[y, :] = (255, 230, 0)
    t[V_JUNCTION, :] = (255, 0, 255)  # the junction row, unmistakable
    return t, np.ones((ATLAS_H, ATLAS_W))


def debug_tile():
    """T7's debug atlas tile: coloured U sectors + banded, ticked V rows, so a
    render PROVES where each texel faces before anything real is painted
    ("avoids the fourth wrong-UV confident paint").

      U (angle round the ring): col 48 = BACK -> WHITE band; col 16 = BELLY ->
      near-BLACK band; col 32 = +Z side -> RED band; col 0 = -Z side -> BLUE
      band; quadrants between are the mixed hues.
      V (along the body): brightness saw every 8 rows, plus a full-width
      YELLOW line at rows 0, 16, 32, 48 so V position is countable on screen.
    """
    t = np.zeros((TILE, TILE, 3), dtype=np.float64)
    for x in range(TILE):
        d_back = min(abs(x - 48), TILE - abs(x - 48))
        d_bel = min(abs(x - 16), TILE - abs(x - 16))
        d_zp = min(abs(x - 32), TILE - abs(x - 32))
        d_zn = min(x, TILE - x)
        if d_back <= 3:
            base = (255, 255, 255)
        elif d_bel <= 3:
            base = (30, 30, 30)
        elif d_zp <= 3:
            base = (230, 40, 40)
        elif d_zn <= 3:
            base = (40, 80, 230)
        else:  # quadrant fills: top-right magenta-ish, etc.
            if 3 < x < 13:
                base = (60, 160, 160)
            elif 19 < x < 29:
                base = (160, 60, 160)
            elif 35 < x < 45:
                base = (200, 140, 40)
            else:
                base = (90, 200, 60)
        for y in range(TILE):
            k = 0.55 + 0.45 * ((y % 8) / 7.0)
            t[y, x] = tuple(c * k for c in base)
    for y in (0, 16, 32, 48):
        t[y, :] = (255, 230, 0)
    return t


def main():
    if "--debug" in sys.argv:
        # T7: the sector/band debug page. Same symbol layout as the real one,
        # written to zixxtrixx_page_debug.h; the reel selects it with
        # -DZIXX_DEBUG_PAGE. Every tile is the same debug field so any part
        # of the animal reports its own UV on screen.
        sys.argv = [a for a in sys.argv if a != "--debug"]
        d = debug_tile()
        tiles = [d.copy() for _ in range(6)]
        names = ["DEBUG sector/band field"] * 6
        emit(tiles, names,
             Path(sys.argv[1]) if len(sys.argv) > 1
             else HERE.parents[1] / "tools" / "reel" / "zixxtrixx_page_debug.h",
             atlas=debug_atlas())
        return
    tiles, names = build_tiles()
    emit(tiles, names,
         Path(sys.argv[1]) if len(sys.argv) > 1
         else HERE.parents[1] / "tools" / "reel" / "zixxtrixx_page.h",
         atlas=build_atlas())


def emit(tiles, names, dst, atlas=None):
    n = len(tiles)
    # one shared 256-entry palette across every tile, quantised together so a
    # creature's materials cannot fight each other for entries
    sheet = np.concatenate([t.astype(np.uint8) for t in tiles], axis=0)
    img = Image.fromarray(sheet, "RGB").quantize(colors=256, method=Image.MEDIANCUT, dither=Image.NONE)
    idx = np.asarray(img).reshape(n, TILE, TILE)
    pal = img.getpalette()[: 256 * 3]
    pal = pal + [0] * (256 * 3 - len(pal))  # a low-colour page (debug) pads out
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
    # ---- DIRECT-COLOUR PAYLOAD (T1/T2, 2026-08-27) ----------------------
    # The same painted tiles, WITHOUT the shared-palette quantisation, as
    # RGB565 mip chains for the TMU's direct path: area-filter each level in
    # RGB888 (BOX = true texel averaging), THEN quantise each level to 565 --
    # never downsample quantised values (T6). Level offsets follow
    # Tmu::level_offset_texels (row-major, level-major). 64x64 -> 7 levels.
    out.append(f"constexpr int kPageTiles = {n};")
    if atlas is not None:
        # ---- THE CONTINUOUS BODY ATLAS (T4/T5/T6) ----------------------
        # 128x256 RGB565, U = circumference (col 96 = back), V = nose-to-
        # tail (row 50 = the head/body junction). Full mip chain, levels
        # 0..7, level offsets per Tmu::level_offset_texels. The multi-scale
        # crayon (coverage/strokes/tooth), the quilting and the hue drift
        # are described at build_atlas/multiscale above; every value is a
        # named knob. Fins stay on the kPageDirect 64x64 pages.
        a_rgb, a_tooth = atlas
        awords = atlas_mip_words(a_rgb, a_tooth)
        out.append(f"constexpr int kPageAtlasW = {ATLAS_W};")
        out.append(f"constexpr int kPageAtlasH = {ATLAS_H};")
        out.append(f"constexpr int kPageAtlasWords = {len(awords)};")
        out.append(f"constexpr uint16_t kPageAtlas[{len(awords)}] = {{")
        for i in range(0, len(awords), 12):
            out.append("    " + " ".join(f"0x{v:04X}," for v in awords[i : i + 12]))
        out.append("};")
        out.append("")
        # a same-named preview png next to the tool, for the authoring loop
        try:
            Image.fromarray(np.clip(a_rgb, 0, 255).astype(np.uint8), "RGB").save(
                str(HERE / "atlas_preview.png"))
        except Exception:
            pass
    total = sum((TILE >> l) * (TILE >> l) for l in range(7))
    out.append(f"// direct RGB565 mip chains: {total} halfwords per tile")
    out.append(f"constexpr uint16_t kPageDirect[{n}][{total}] = {{")
    for t in tiles:
        base = Image.fromarray(t.astype(np.uint8), "RGB")
        words = []
        for l in range(7):
            sz = TILE >> l
            lv = np.asarray(base.resize((sz, sz), Image.BOX)).astype(np.int64)
            for row in lv.reshape(sz * sz, 3):
                words.append(to565(row[0], row[1], row[2]))
        out.append("    {")
        for i in range(0, len(words), 12):
            out.append("        " + " ".join(f"0x{v:04X}," for v in words[i : i + 12]))
        out.append("    },")
    out.append("};")
    out.append("")
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

    dst.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")
    print(f"wrote {dst}")
    print(f"  {n} tiles of {TILE}x{TILE}, {used}/256 palette entries")
    for i, nm in enumerate(names):
        print(f"    tile {i}: {nm}")


if __name__ == "__main__":
    main()
