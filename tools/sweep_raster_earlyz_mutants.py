#!/usr/bin/env python3
"""The mutant table for zhao_raster_earlyz.sv (RASTER.EARLYZ).

WHY THIS SWEEP EXISTS
---------------------
`reports/SWEEP_COVERAGE_AUDIT.md` lists RASTER.EARLYZ among the modules with a
test lane and no mutation sweep. It is the raster tier, and it is the block
whose whole value is REFUSING work before the TMU bandwidth and the tile-store
read have been paid.

WHAT MAKES THIS BLOCK DIFFERENT, AND WHAT THAT MEANS FOR THE MUTATIONS
----------------------------------------------------------------------
Almost every other block is wrong when it computes a wrong value. This one is
wrong when it *rejects something it should not have*, and that failure is
INVISIBLE in a pixel comparison unless the test happens to place geometry that
the exact test would have kept. Its own header states the standard:

    THE CONSERVATISM ARGUMENT -- every rejection this block makes is a
    rejection the exact late test would also have made.

So the mutations split into two kinds, and the second kind is the point:

  * ones that make it reject LESS. Those cost throughput and show up in
    `early_z_rejects`, but they can never put a wrong pixel on screen.
  * ones that make it reject MORE. Those are silent correctness failures --
    geometry vanishes, and only a test that draws something the exact test
    would have kept can see it.

If a "rejects more" mutant survives, the suite is not testing conservatism at
all; it is testing that the counter moves.

THE LAWS, in the header's own citation order
--------------------------------------------
  * qformats 8: `invw24` depth, LARGER IS CLOSER, clear value 0, and the exact
    late test `pass <=> d_new > d_old` -- STRICT, ties fail. This block's
    `frag_depth_i <= floor_r` is that test's contrapositive, and the `=` is
    load-bearing: a tie fails the late test, so rejecting a tie is lawful.
  * the floor NEVER MOVES BACKWARDS -- `max`, not assignment. A floor that can
    fall is a floor that can over-reject on the next fragment.
  * the accumulator qualification is DELIBERATELY NARROW: only an opaque
    fragment that replaces rather than blends, writes depth, cannot be masked
    away by alpha test, and cannot be stencilled away, may raise the floor.
    Every one of those four terms is a separate way to occlude something that
    was never actually drawn.
  * charter 8 pass 6: the coarse bins are the per-tile occupancy that lets
    translucent geometry walk back-to-front without sorting.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/raster/zhao_raster_earlyz.sv"

# name, old, new -- each applied to the PRISTINE file, one at a time.
MUTANTS = [
    # ---- the rejection test itself ---------------------------------------
    # E01 is THE mutant of this block. `>=` rejects everything the pristine
    # block keeps and keeps everything it rejects -- geometry disappears. If
    # this survives, no test draws anything the exact test would have kept.
    ("E01 the rejection test is inverted, so surviving geometry is discarded",
     "  assign reject_c = st_z_test_en && (frag_depth_i <= floor_r);",
     "  assign reject_c = st_z_test_en && (frag_depth_i >= floor_r);"),
    # E02 rejects MORE by one ulp: a fragment exactly one step nearer than the
    # floor still passes the exact late test, so rejecting it is unlawful.
    ("E02 the rejection is one ulp too greedy",
     "  assign reject_c = st_z_test_en && (frag_depth_i <= floor_r);",
     "  assign reject_c = st_z_test_en && (frag_depth_i <= floor_r + 24'd1);"),
    # E03 rejects LESS: ties are kept. Lawful but wasteful -- it should show
    # up as a counter difference and nothing else. Kept precisely to see WHICH
    # lane notices, because a suite that only checks pixels will miss it.
    ("E03 ties are no longer rejected, costing throughput but not pixels",
     "  assign reject_c = st_z_test_en && (frag_depth_i <= floor_r);",
     "  assign reject_c = st_z_test_en && (frag_depth_i < floor_r);"),
    ("E04 rejection happens only when the depth test is DISABLED",
     "  assign reject_c = st_z_test_en && (frag_depth_i <= floor_r);",
     "  assign reject_c = !st_z_test_en && (frag_depth_i <= floor_r);"),

    # ---- the floor, which must never move backwards ----------------------
    ("E05 the floor is assigned rather than maxed, so it can fall",
     "          if (acc_min_next > floor_r) floor_r <= acc_min_next;",
     "          floor_r <= acc_min_next;"),
    ("E06 the floor rises on ties too, one ulp beyond what was proved",
     "          if (acc_min_next > floor_r) floor_r <= acc_min_next;",
     "          if (acc_min_next >= floor_r) floor_r <= acc_min_next + 24'd1;"),
    ("E07 a tile starts one ulp ABOVE its clear depth",
     "        floor_r    <= tile_clear_depth_i;",
     "        floor_r    <= tile_clear_depth_i + 24'd1;"),

    # ---- the accumulator qualification, four separate occlusion laws -----
    # These five began as `1'b1` deletions and every one orphaned its term,
    # failing the LINTER rather than the tests. Inverting the term instead
    # keeps every operand read AND keeps the unsafe direction: each still
    # lets something raise the floor that was never guaranteed to be drawn.
    ("E08 ONLY blended fragments raise the floor, never opaque ones",
     "                  (st_blend == BL_REPLACE) &&      // opaque: replaces, not blends",
     "                  (st_blend != BL_REPLACE) &&      // opaque: replaces, not blends"),
    ("E09 only fragments that do NOT write depth raise the floor",
     "                  !st_z_write_dis &&               // it writes depth",
     "                  st_z_write_dis &&                // it writes depth"),
    ("E10 only alpha-TESTED fragments raise the floor",
     "                  !st_atest_en &&                  // it cannot be masked away",
     "                  st_atest_en &&                   // it cannot be masked away"),
    ("E11 only stencil-conditional fragments raise the floor",
     "                  (st_sten_func == STEN_ALWAYS);   // it cannot be stencilled away",
     "                  (st_sten_func != STEN_ALWAYS);   // it cannot be stencilled away"),
    ("E12 a REJECTED fragment still contributes to the floor",
     "    hiz_qualify = frag_acc && !reject_c &&",
     "    hiz_qualify = frag_acc &&"),

    # ---- forced-far depth -------------------------------------------------
    ("E13 z_force_far swaps its two arms",
     "    hiz_depth   = st_z_force_far ? 24'd0 : frag_depth_i;",
     "    hiz_depth   = st_z_force_far ? frag_depth_i : 24'd0;"),

    # ---- the coarse bins (charter 8, pass 6) ------------------------------
    ("E14 the coarse bin is taken from the wrong depth bits",
     "          out_bin_r     <= frag_depth_i[23:21];",
     "          out_bin_r     <= frag_depth_i[22:20];"),
    ("E15 the bin mask records a different bin than the fragment carries",
     "          bin_mask_r[frag_depth_i[23:21]] <= 1'b1;",
     "          bin_mask_r[frag_depth_i[22:20]] <= 1'b1;"),

    # ---- the handshake ----------------------------------------------------
    ("E16 a fragment is accepted while the output stage is still full",
     "  assign out_free     = !out_v_r || cand_ready_i;",
     "  assign out_free     = 1'b1;"),
]


def read_rtl(path=RTL):
    return io.open(path, encoding="utf-8", newline="").read()


def mutate(gold, old, new):
    """Return the mutated text, or raise if the anchor is not unique."""
    nl = "\r\n" if "\r\n" in gold else "\n"
    o = old.replace("\n", nl)
    n = new.replace("\n", nl)
    count = gold.count(o)
    if count != 1:
        raise ValueError("anchor matches %d times" % count)
    if o == n:
        raise ValueError("mutant identical to base")
    return gold.replace(o, n, 1)


# Machine-readable, so a survivor is either PROVEN equivalent here or fails the
# sweep. Nothing is declared until the first run says what actually survives.
EQUIVALENT = {}


def write_rtl(text, path=RTL):
    io.open(path, "w", encoding="utf-8", newline="").write(text)
    os.utime(path, None)


def main(argv):
    if len(argv) >= 2 and argv[1] == "--count":
        print(len(MUTANTS))
        return 0
    if len(argv) >= 3 and argv[1] == "--name":
        print(MUTANTS[int(argv[2])][0])
        return 0
    if len(argv) >= 3 and argv[1] == "--equiv":
        proof = EQUIVALENT.get(argv[2])
        if proof is None:
            return 1
        print(proof)
        return 0
    if len(argv) >= 3 and argv[1] == "--apply":
        name, old, new = MUTANTS[int(argv[2])]
        try:
            write_rtl(mutate(read_rtl(), old, new))
        except ValueError as exc:
            sys.stderr.write("%s: %s\n" % (name, exc))
            return 9
        return 0
    sys.stderr.write("usage: --count | --name N | --equiv TOK | --apply N\n")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
