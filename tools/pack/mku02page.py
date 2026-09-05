#!/usr/bin/env python3
"""mku02page — the creature-02 (Unnamed02) texture page generator.

Forked from mkcreaturepage.py's STRUCTURE, not its Zixxtrixx pigment recipe:
one direct-colour RGB565 atlas (body / loop / hinge rows selected per part by
v0..v1) plus ONE SEPARATE 64x64 eye page (bilinear + mips bleed across atlas
neighbours; the lens must not share rows with pink). No CLUT8 tier — the
creature ships full-colour and falls back to flat part materials when the
generated header is absent (see unnamed02.h's __has_include guard).

Determinism: zlib.crc32 seeds (never Python hash()), no clock. Verify two
regens `cmp`-identical — the required_checks in CREATURE.json.

Pigments are AUTHORED, chosen by eye at 384x240 in scene; nothing here is
sampled from the concept scans (a scanner flattens and lightens — the art
law). The crayon grain amplitude deliberately exceeds the light rig's own
range: the +-16% grain of an early Zixxtrixx page measured fine and was
invisible.

Usage:  python tools/pack/mku02page.py [out.h]
"""

import sys
import zlib
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent

ATLAS_W, ATLAS_H = 256, 256
EYE_TILE = 64

# ---- the authored pigments (owner knobs; see unnamed02_art.h for geometry) --
# PASS 2 (Direction 2 §1d + the form recon's bias finding): the shipped
# pink ran ~20 counts BLUER than the drawings' crimson-rose and the old
# blue-heavy rig pushed it further toward lilac. The blue bias comes out —
# crimson, not magenta. Chosen by LOOKING at native 240p on dark ground
# under the u02 moving rig (the sheet numbers are a comparison, never the
# shipped value).
BODY_PINK = np.array([228.0, 70.0, 124.0])   # the crayon pink, saturated on
BODY_PINK_DEEP = np.array([190.0, 42.0, 96.0])   # purpose (the pale-scan trap)
LOOP_PINK = np.array([212.0, 60.0, 116.0])
HINGE_PINK = np.array([236.0, 96.0, 138.0])
EYE_PURPLE = np.array([104.0, 42.0, 168.0])
EYE_PURPLE_DEEP = np.array([76.0, 26.0, 128.0])
EYE_RIM_WHITE = np.array([246.0, 242.0, 250.0])
# The pupil-star cyan — matches kStarR/G/B in unnamed02_art.h (the flat
# fallback). The star NEEDS a page: untextured parts render black under
# celmain (creature/09-ENGINE-GOTCHAS.md §7 — the toon ramp normalises the
# pre-lit colour lanes to the light-gain scale and the >>16 floors them).
# PASS 2: deepened toward the drawn teal — the shipped cyan ran lighter and
# greener than the sheets' star blue.
STAR_CYAN = np.array([46.0, 184.0, 210.0])
STAR_CYAN_DEEP = np.array([30.0, 150.0, 176.0])
# the lens's own ink (the sheet outlines every almond in heavy black — the
# cel ink pass draws only the creature silhouette, so the lens/pink contour
# is painted at the junction band of the page)
LENS_INK = np.array([24.0, 14.0, 40.0])
GRAIN_AMP = 0.26        # value amplitude — must beat the light rig's range
STROKE_AMP = 0.10       # directional crayon-stroke modulation

# atlas V rows per part family (parts select them via v0/v1)
BODY_V0, BODY_V1 = 8, 120
LOOP_V0, LOOP_V1 = 136, 200
HINGE_V0, HINGE_V1 = 208, 248


def seed_of(tag: str) -> int:
    return zlib.crc32(tag.encode("ascii")) & 0xFFFFFFFF


def value_noise(h, w, seed, octaves=((8, 1.0), (24, 0.55), (72, 0.30))):
    """multi-scale smooth value noise in [-1, 1], deterministic."""
    rng = np.random.RandomState(seed)
    acc = np.zeros((h, w))
    total = 0.0
    for cells, amp in octaves:
        g = rng.uniform(-1.0, 1.0, (cells + 1, cells + 1))
        img = Image.fromarray(((g + 1) * 127.5).astype(np.uint8), "L")
        img = img.resize((w, h), Image.BILINEAR)
        acc += amp * (np.asarray(img).astype(np.float64) / 127.5 - 1.0)
        total += amp
    return acc / total


def crayon(base, deep, h, w, tag, stroke_axis=0):
    """the crayon field: base pigment, multi-scale value grain, directional
    stroke bands, and a scatter of paper-tooth flecks where the wax skipped."""
    g = value_noise(h, w, seed_of(tag))
    field = np.zeros((h, w, 3))
    t = (g * 0.5 + 0.5)[..., None]
    field = base * (1 - 0.35 * t) + deep * (0.35 * t)
    field *= (1.0 + GRAIN_AMP * g)[..., None]
    # stroke bands along one axis (the hand pulling the crayon)
    ax = np.arange(w if stroke_axis else h)
    stroke = 0.5 * np.sin(ax * 0.61 + 0.9) + 0.5 * np.sin(ax * 0.23 + 4.0)
    stroke = stroke[None, :, None] if stroke_axis else stroke[:, None, None]
    field *= 1.0 + STROKE_AMP * stroke
    # paper tooth: sparse light flecks
    rng = np.random.RandomState(seed_of(tag + "-tooth"))
    tooth = rng.uniform(0, 1, (h, w)) > 0.988
    field[tooth] = field[tooth] * 0.55 + np.array([250.0, 246.0, 240.0]) * 0.45
    return np.clip(field, 0, 255)


def build_atlas():
    a = np.zeros((ATLAS_H, ATLAS_W, 3))
    # ground everything in body pink so bleed between families stays pink
    a[:] = crayon(BODY_PINK, BODY_PINK_DEEP, ATLAS_H, ATLAS_W, "u02-ground")
    a[BODY_V0:BODY_V1 + 1] = crayon(BODY_PINK, BODY_PINK_DEEP,
                                    BODY_V1 + 1 - BODY_V0, ATLAS_W, "u02-body")
    a[LOOP_V0:LOOP_V1 + 1] = crayon(LOOP_PINK, BODY_PINK_DEEP,
                                    LOOP_V1 + 1 - LOOP_V0, ATLAS_W, "u02-loop",
                                    stroke_axis=1)
    a[HINGE_V0:HINGE_V1 + 1] = crayon(HINGE_PINK, BODY_PINK,
                                      HINGE_V1 + 1 - HINGE_V0, ATLAS_W, "u02-hinge")
    return a


def build_eye():
    """The lens page. U wraps around the lens ring (outward face centred on
    u=0 by the ring builder's angle law); V runs tip to tip.

    PASS 2 REPAINT (Direction 2 §2 + the traced area split): the sheet gives
    one lens 65-71% PURPLE, a THIN white ring (4-7%) tracing the star, and a
    big cyan star (the star stays geometry riding the pupil bone). The old
    page painted a fat white slab over 40% of the circumference — which is
    why the shipped eye's most common colour was a hueless mid-grey. Purple
    is the dominant field now; the white survives only as a ring ballooning
    just past the star's tips, as drawn; the junction band and the tips
    carry the lens's own ink so the almond separates from the pink at any
    size (the sheet outlines every lens in heavy black)."""
    t = crayon(EYE_PURPLE, EYE_PURPLE_DEEP, EYE_TILE, EYE_TILE, "u02-eye")
    v = np.arange(EYE_TILE)[:, None] / (EYE_TILE - 1)   # 0..1 tip to tip
    u = np.arange(EYE_TILE)[None, :] / EYE_TILE         # around the ring
    # angular distance from the outward station (u = 0/1 wrap)
    du = np.minimum(u, 1.0 - u) * 2.0                   # 0 at front, 1 at back
    # the thin white ring: an oval annulus around the outward centre, sized
    # to balloon just past the star geometry's tips (star arm reaches
    # |v-0.5| ~ 0.20 at the shipped kPupilStarArmMm)
    rv = np.abs(v - 0.5) / 0.26
    ru = du / 0.50
    rr = np.sqrt(rv * rv + ru * ru + 1e-9)
    ring = (rr > 0.74) & (rr < 1.02)
    white = crayon(EYE_RIM_WHITE, EYE_RIM_WHITE * 0.9,
                   EYE_TILE, EYE_TILE, "u02-rim")
    t[ring] = white[ring]
    # the lens ink: the junction band where the almond meets the body, and
    # the sharp tips — a painted contour, present at any render size
    ink = (du > 0.86) | (v < 0.055) | (v > 0.945)
    t[ink] = LENS_INK
    return np.clip(t, 0, 255)


def build_star():
    """The pupil-star page: near-flat cyan in the house crayon treatment.
    The star's COLOUR authority stays here + kStarR/G/B; the page exists so
    the star takes the textured light path (gotcha §7 — an untextured part
    renders black under celmain). Grain is kept gentle: the star is a few
    pixels at 240p and samples deep mips, so heavy grain would only mud the
    565 quantisation."""
    t = crayon(STAR_CYAN, STAR_CYAN_DEEP, EYE_TILE, EYE_TILE, "u02-star")
    # pull halfway back toward the flat pigment: half-strength grain
    return np.clip(0.5 * t + 0.5 * STAR_CYAN, 0, 255)


def to565(rgb):
    r = (rgb[..., 0].astype(np.int64) * 31 + 127) // 255
    g = (rgb[..., 1].astype(np.int64) * 63 + 127) // 255
    b = (rgb[..., 2].astype(np.int64) * 31 + 127) // 255
    return (r << 11) | (g << 5) | b


def mip_words(rgb, levels):
    base = Image.fromarray(rgb.astype(np.uint8), "RGB")
    w, h = base.size
    words = []
    for l in range(levels):
        lw, lh = max(1, w >> l), max(1, h >> l)
        lv = np.asarray(base.resize((lw, lh), Image.BOX)).astype(np.float64)
        words.extend(to565(lv).reshape(-1).tolist())
    return words


def emit(dst: Path):
    atlas = build_atlas()
    eye = build_eye()
    star = build_star()
    awords = mip_words(atlas, 8)   # 256 -> 2 (max_level 7)
    ewords = mip_words(eye, 7)     # 64 -> 1
    swords = mip_words(star, 7)    # 64 -> 1

    out = []
    out.append("// GENERATED by tools/pack/mku02page.py — do not edit, do not track.")
    out.append("// Creature 02 direct-colour page: one atlas (body/loop/hinge rows)")
    out.append("// + one separate eye page. Regenerate: python tools/pack/mku02page.py")
    out.append("#ifndef ZHAO_REEL_UNNAMED02_PAGE_H")
    out.append("#define ZHAO_REEL_UNNAMED02_PAGE_H")
    out.append("namespace u02 {")
    out.append(f"constexpr int kU02AtlasW = {ATLAS_W};")
    out.append(f"constexpr int kU02AtlasH = {ATLAS_H};")
    out.append(f"constexpr int kU02AtlasWords = {len(awords)};")
    out.append(f"constexpr uint16_t kU02Atlas[{len(awords)}] = {{")
    for i in range(0, len(awords), 12):
        out.append("    " + " ".join(f"0x{v:04X}," for v in awords[i:i + 12]))
    out.append("};")
    out.append(f"constexpr int kU02EyeWords = {len(ewords)};")
    out.append(f"constexpr uint16_t kU02Eye[{len(ewords)}] = {{")
    for i in range(0, len(ewords), 12):
        out.append("    " + " ".join(f"0x{v:04X}," for v in ewords[i:i + 12]))
    out.append("};")
    out.append(f"constexpr int kU02StarWords = {len(swords)};")
    out.append(f"constexpr uint16_t kU02Star[{len(swords)}] = {{")
    for i in range(0, len(swords), 12):
        out.append("    " + " ".join(f"0x{v:04X}," for v in swords[i:i + 12]))
    out.append("};")
    out.append("}  // namespace u02")
    out.append("#endif  // ZHAO_REEL_UNNAMED02_PAGE_H")
    dst.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")
    print(f"wrote {dst}: atlas {ATLAS_W}x{ATLAS_H} ({len(awords)} words) + "
          f"eye {EYE_TILE}x{EYE_TILE} ({len(ewords)} words) + "
          f"star {EYE_TILE}x{EYE_TILE} ({len(swords)} words)")


if __name__ == "__main__":
    emit(Path(sys.argv[1]) if len(sys.argv) > 1
         else HERE.parents[1] / "tools" / "reel" / "unnamed02_page.h")
