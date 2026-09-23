#!/usr/bin/env python3
"""mkdetailnormal.py -- the DETAIL_NORMAL page (kind 16), emitted.

Owner ruling R243 D-NORMALS-A (2026-09-23): "In v1 -- commission the pyramid."
`spec/cartridge.md` 4g is the freeze; this file emits it.

WHY THIS TOOL EXISTS AT ALL
---------------------------
`zhao_terrain_normalmap` has held a seven-level signed detail pyramid and a
write-only upload port since 2026-09-09 and is instantiated NOWHERE, because
nothing in the tree could put bytes in it. A resident tile with no producer is
the shape the completion ruling's asset clause refuses. This tool is the
producer's offline half.

THE REDUCTION LAW IS NOT DEFINED HERE
-------------------------------------
`reference/include/zref/zref_normal_page.hpp` is the layout AND the pyramid
reduction, and `spec/cartridge.md` 4g states both normatively. This file is a
THIRD statement, which is one more than anybody wants -- so `--check` exists:
it rebuilds the committed golden `tests/golden/detail_normal/
detail_normal_v1.bin` and compares byte for byte, and
`tests/terrain/normal_page_directed.cpp` requires `zref::normal_page::build`
and `zref::normal_page::build_pyramid` to reproduce that SAME file. Packer,
model and the hardware reader are then pinned to one artefact rather than to
each other's good intentions, and a layout edit that misses one of the three
goes red.

That discipline is `tools/pack/mkforgeprogram.py`'s and
`tools/pack/mkcreatureladder.py`'s, deliberately copied rather than
re-invented.

NEVER NORMALISE
---------------
The pyramid is built by averaging SIGNED dx/dz, never re-normalising, and that
is a LAW of the page rather than a preference of this tool -- the hardware has
no way to check it, so the tool is where it is kept. `s*dot(d, L)` is linear in
`d`, so the average of four texels' contributions IS the contribution of their
average. Re-normalising would break that linearity and would give a FLAT region
(dx = dz = 0, the correct answer for "no relief here") an arbitrary unit
direction, so terrain that should go quiet at distance would acquire a constant
tilt instead. Coarsening detail must converge to NOTHING.

Rounding is HALF AWAY FROM ZERO. Negating a tile must negate its whole pyramid
exactly, or a relief and its mirror image would coarsen differently and a
mirrored piece of terrain would shimmer where its twin did not.

INPUT
-----
A raw tile: 8,192 bytes, 64 x 64 texels row-major, each `{s8 dx, s8 dz}` in
that byte order. dx and dz are a PERTURBATION of the surface normal in world
XZ with value raw/128 -- not a direction, which is why they are never
renormalised.

Usage:
    mkdetailnormal.py tile.raw out.bin [--levels N]
    mkdetailnormal.py --check
    mkdetailnormal.py --dump tile.raw        # look at it; that is the job
    mkdetailnormal.py --emit-golden-tile t.raw
"""
import argparse
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
GOLDEN = REPO / "tests" / "golden" / "detail_normal" / "detail_normal_v1.bin"

# ---------------------------------------------------------------------------
# THE LAYOUT (spec/cartridge.md 4g; reference/include/zref/zref_normal_page.hpp)
# ---------------------------------------------------------------------------
MAGIC = 0x4D4E445A  # 'Z','D','N','M' little-endian
VERSION = 1
HEADER_BYTES = 64
WORD_BYTES = 2
TILE_DIM = 64
MAX_LEVELS = 7
# The flat level bases, `zref::terrain::kNormalmapLevelBase`. Restated rather
# than derived: a change on either side must show up as a golden failure, not
# be tracked silently.
LEVEL_BASE = (0, 4096, 5120, 5376, 5440, 5456, 5460)
MAX_WORDS = 5461


def pyramid_addr(level, u6, v6):
    """`zref::terrain::normalmap_pyramid_addr`. u6/v6 are LEVEL-0 coordinates;
    the function shifts them down itself."""
    return LEVEL_BASE[level] + ((v6 >> level) << (6 - level)) + (u6 >> level)


def words_for_levels(levels):
    return MAX_WORDS if levels >= MAX_LEVELS else LEVEL_BASE[levels]


def s8(v):
    """Interpret a byte as s8."""
    return v - 256 if v >= 128 else v


def avg4_signed(a, b, c, d):
    """Average four signed components, round HALF AWAY FROM ZERO.

    Python's `//` floors, which is NOT what C++'s truncating division does for
    negatives, so this is written with int() on a float-free expression to
    match `zref::normal_page::avg4_signed` exactly. Getting this wrong is a
    one-LSB drift that only shows up on negative relief -- which is half the
    tile, and exactly the half nobody renders first.
    """
    s = a + b + c + d
    if s >= 0:
        r = (s + 2) // 4
    else:
        # C++ truncation toward zero: -(x // y) with positive operands.
        r = -((-(s - 2)) // 4)
    assert -128 <= r <= 127, "avg4_signed left s8: %d from %d" % (r, s)
    return r


def build_pyramid(level0, levels=MAX_LEVELS):
    """level0: list of 4096 (dx, dz) pairs, row-major v*64+u.
    Returns a flat list of (dx, dz) of length words_for_levels(levels)."""
    levels = max(1, min(MAX_LEVELS, levels))
    pyr = [(0, 0)] * words_for_levels(levels)
    for v in range(TILE_DIM):
        for u in range(TILE_DIM):
            src = v * TILE_DIM + u
            if src < len(level0):
                pyr[pyramid_addr(0, u, v)] = level0[src]
    for lvl in range(1, levels):
        side = TILE_DIM >> lvl
        half = 1 << (lvl - 1)
        for v in range(side):
            for u in range(side):
                pu, pv = u << lvl, v << lvl
                t00 = pyr[pyramid_addr(lvl - 1, pu, pv)]
                t10 = pyr[pyramid_addr(lvl - 1, pu + half, pv)]
                t01 = pyr[pyramid_addr(lvl - 1, pu, pv + half)]
                t11 = pyr[pyramid_addr(lvl - 1, pu + half, pv + half)]
                pyr[pyramid_addr(lvl, pu, pv)] = (
                    avg4_signed(t00[0], t10[0], t01[0], t11[0]),
                    avg4_signed(t00[1], t10[1], t01[1], t11[1]),
                )
    return pyr


def pack_word(t):
    """The hardware's upload word: {s8 dz, s8 dx}, dx in the low byte."""
    return ((t[1] & 0xFF) << 8) | (t[0] & 0xFF)


def build_page(pyramid):
    """Header then the flat words, padded to a multiple of 64 -- MEM.UPLOAD's
    length rule, which is a refusal on the command side and a pad here."""
    n_words = min(len(pyramid), MAX_WORDS)
    n = HEADER_BYTES + WORD_BYTES * n_words
    if n % 64:
        n += 64 - (n % 64)
    page = bytearray(n)
    struct.pack_into("<IHH", page, 0, MAGIC, VERSION, n_words)
    for i in range(n_words):
        struct.pack_into("<H", page, HEADER_BYTES + WORD_BYTES * i, pack_word(pyramid[i]))
    return bytes(page)


def read_tile(path):
    raw = Path(path).read_bytes()
    want = TILE_DIM * TILE_DIM * 2
    if len(raw) != want:
        raise SystemExit(
            "mkdetailnormal: %s is %d bytes, need exactly %d (64x64 x {s8 dx, s8 dz})"
            % (path, len(raw), want))
    return [(s8(raw[2 * i]), s8(raw[2 * i + 1])) for i in range(TILE_DIM * TILE_DIM)]


# ---------------------------------------------------------------------------
# THE GOLDEN's OWN CONTENT, here rather than in a data file so that `--check`
# needs nothing but this script and the committed page. The tile is chosen to
# exercise the reduction rather than to look like terrain:
#
#   * a DIAGONAL RIDGE, so dx and dz differ everywhere and a transposed or
#     swapped pair fails loudly instead of averaging to the same answer;
#   * both SIGNS, in unequal proportion, so a rounding that is not symmetric
#     about zero drifts;
#   * a LONE SPIKE at (0, 0) whose decay across the seven levels (127, 32, 8,
#     2, 1, 0, 0) is the "coarsening converges to nothing" property, readable
#     straight out of `--dump`;
#   * the s8 RAILS at +127 and -128, so a reduction that leaves the range
#     trips the assertion rather than wrapping.
# ---------------------------------------------------------------------------
def golden_tile():
    tile = []
    for v in range(TILE_DIM):
        for u in range(TILE_DIM):
            dx = ((u * 7 + v * 3) % 255) - 127        # [-127, 127], diagonal
            dz = 100 - ((u * 5 + v * 11) % 229)       # [-128, 100], different slope
            tile.append((dx, dz))
    tile[0] = (127, -128)                             # the rails, and the spike
    return tile


def dump(pyr, levels):
    """Look at it. Measurement belongs on the comparison side; this is the
    looking side, and it is deliberately the coarse levels that print whole."""
    for lvl in range(levels):
        side = TILE_DIM >> lvl
        n = side * side
        base = LEVEL_BASE[lvl]
        xs = [pyr[base + i][0] for i in range(n)]
        zs = [pyr[base + i][1] for i in range(n)]
        print("L%d  %2dx%-2d  base %4d  dx[min %4d max %4d mean %7.2f]  "
              "dz[min %4d max %4d mean %7.2f]"
              % (lvl, side, side, base, min(xs), max(xs), sum(xs) / n,
                 min(zs), max(zs), sum(zs) / n))
        if side <= 8:
            for v in range(side):
                print("      " + " ".join("%4d/%-4d" % pyr[base + v * side + u]
                                          for u in range(side)))


def main(argv):
    ap = argparse.ArgumentParser(description="emit a DETAIL_NORMAL page (kind 16)")
    ap.add_argument("input", nargs="?", help="64x64 x {s8 dx, s8 dz} raw tile")
    ap.add_argument("output", nargs="?", help="page to write")
    ap.add_argument("--levels", type=int, default=MAX_LEVELS,
                    help="pyramid depth 1..7 (1 = the bare contract tile, no mip tail)")
    ap.add_argument("--check", action="store_true",
                    help="rebuild the committed golden and compare byte for byte")
    ap.add_argument("--dump", action="store_true",
                    help="print every level's extent and the coarse levels whole")
    ap.add_argument("--emit-golden-tile", metavar="PATH",
                    help="write the golden's level-0 tile as a raw file")
    a = ap.parse_args(argv)

    if a.emit_golden_tile:
        raw = bytearray()
        for dx, dz in golden_tile():
            raw += bytes(((dx & 0xFF), (dz & 0xFF)))
        Path(a.emit_golden_tile).write_bytes(bytes(raw))
        print("wrote %d bytes to %s" % (len(raw), a.emit_golden_tile))
        return 0

    if a.check:
        got = build_page(build_pyramid(golden_tile(), MAX_LEVELS))
        if not GOLDEN.exists():
            print("mkdetailnormal --check FAILED: %s is not committed"
                  % GOLDEN.relative_to(REPO).as_posix(), file=sys.stderr)
            return 1
        want = GOLDEN.read_bytes()
        if got != want:
            first = next((i for i in range(min(len(got), len(want))) if got[i] != want[i]),
                         min(len(got), len(want)))
            print("mkdetailnormal --check FAILED: %d bytes built, %d bytes committed, "
                  "first difference at byte %d" % (len(got), len(want), first),
                  file=sys.stderr)
            return 1
        words = struct.unpack_from("<H", want, 6)[0]
        print("mkdetailnormal --check OK: %d bytes, %d words, matches %s"
              % (len(want), words, GOLDEN.relative_to(REPO).as_posix()))
        return 0

    if a.dump:
        tile = read_tile(a.input) if a.input else golden_tile()
        lv = max(1, min(MAX_LEVELS, a.levels))
        dump(build_pyramid(tile, lv), lv)
        return 0

    if not a.input or not a.output:
        ap.error("need INPUT and OUTPUT, or --check, or --dump")
    page = build_page(build_pyramid(read_tile(a.input), a.levels))
    Path(a.output).write_bytes(page)
    print("wrote %d bytes (%d words, %d levels) to %s"
          % (len(page), struct.unpack_from("<H", page, 6)[0], a.levels, a.output))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
