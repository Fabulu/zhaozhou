#!/usr/bin/env python3
"""The mutant table for zhao_terrain_patch.sv, and the apply/restore primitives.

WHY THIS SWEEP EXISTS
---------------------
TERRAIN.PATCH had RTL, a fully written contract, a resolving oracle
(`zref::terrain::compose_vertex`) and FOUR green test lanes -- and no mutation
sweep at all, while both its siblings (TERRAIN.LOD, TERRAIN.NORMALS) had one.
Green tests with no sweep say the tests pass; they do not say the tests would
notice if the block were wrong. This table is what turns the first claim into
the second.

The block is the Mantle entry point, so what it gets wrong is not arithmetic
noise -- it is the shape of the ground. The mutations below are chosen to hit
the laws the contract actually leads with:

  * the TWO clamps of terrain_rules 3.4, and their `dual` guards -- a transient
    wave punching below the underside would fake a breach;
  * the overflow law of 9.1: append in command order, reject the tail, NEVER
    evict, and never stall the frame;
  * the CLOSED-interval footprint test -- a vertex exactly on a footprint edge
    is INSIDE;
  * the exact `raw << 8` height16 -> fx16 conversion, which is sign-extending;
  * the saturating `fx_add`, which is order-dependent and therefore load-bearing
    for the loop transpose this block is built on.

Structure copied deliberately from `sweep_terrain_normals_mutants.py`: the table
is a Python module rather than a bash array because these anchors contain `$`
(`$signed`), which a double-quoted shell word would silently expand to nothing
-- matching either no text or, far worse, DIFFERENT text than the one written
down, and scoring a mutation nobody authored.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/terrain/zhao_terrain_patch.sv"

# name, old, new -- each applied to the PRISTINE file, one at a time.
MUTANTS = [
    # ---- the two clamps (terrain_rules 3.4) ------------------------------
    ("P01 the underside clamp is dropped from compose_top",
     "  wire signed [31:0] ctop_new = (dual_i && (sum_bs < bot_fx)) ? bot_fx : sum_bs;",
     "  wire signed [31:0] ctop_new = sum_bs;"),
    ("P02 compose_top clamps on a LEGACY page, which has no underside",
     "  wire signed [31:0] ctop_new = (dual_i && (sum_bs < bot_fx)) ? bot_fx : sum_bs;",
     "  wire signed [31:0] ctop_new = (sum_bs < bot_fx) ? bot_fx : sum_bs;"),
    ("P03 the post-field clamp is dropped, so a wave can punch below the underside",
     "  wire signed [31:0] acc_fin = (held_dual && (acc_next < held_bot)) ? held_bot : acc_next;",
     "  wire signed [31:0] acc_fin = acc_next;"),
    ("P04 the post-field clamp is a MAX the wrong way, pinning the top to the bottom",
     "  wire signed [31:0] acc_fin = (held_dual && (acc_next < held_bot)) ? held_bot : acc_next;",
     "  wire signed [31:0] acc_fin = (held_dual && (acc_next > held_bot)) ? held_bot : acc_next;"),

    # ---- the footprint test, CLOSED interval (9.1) -----------------------
    ("P05 the footprint test goes half-open, so a vertex ON the edge falls out",
     "  wire cur_covers = !((held_wx < cur_x0) || (held_wx > cur_x1) ||\n"
     "                      (held_wz < cur_z0) || (held_wz > cur_z1));",
     "  wire cur_covers = !((held_wx < cur_x0) || (held_wx >= cur_x1) ||\n"
     "                      (held_wz < cur_z0) || (held_wz >= cur_z1));"),
    # P06 TOOK TWO GOES, and both failures were the same mistake in different
    # clothes. "Ignores Z entirely" orphaned held_wz; "tests Z against the X
    # bounds" then orphaned cur_z0/cur_z1. Both failed the LINTER rather than
    # the tests, and a mutant that cannot build is not a mutant -- it is a
    # discard, which the sweep's guards would report but which tests nothing.
    # Swapping the two axes is the same class of defect (a vertex checked
    # against the wrong side of the footprint) and reads all four bounds.
    ("P06 the footprint's two axes are swapped",
     "  wire cur_covers = !((held_wx < cur_x0) || (held_wx > cur_x1) ||\n"
     "                      (held_wz < cur_z0) || (held_wz > cur_z1));",
     "  wire cur_covers = !((held_wx < cur_z0) || (held_wx > cur_z1) ||\n"
     "                      (held_wz < cur_x0) || (held_wz > cur_x1));"),

    # ---- the field chain -------------------------------------------------
    ("P07 a lane that MISSES the vertex is added anyway",
     "  wire signed [31:0] acc_next = cur_covers ? fx_add_sat(acc, fld_height_i) : acc;",
     "  wire signed [31:0] acc_next = fx_add_sat(acc, fld_height_i);"),
    ("P08 a lane that covers the vertex is discarded",
     "  wire signed [31:0] acc_next = cur_covers ? fx_add_sat(acc, fld_height_i) : acc;",
     "  wire signed [31:0] acc_next = cur_covers ? acc : fx_add_sat(acc, fld_height_i);"),
    ("P09 the chain finishes one lane early",
     "  wire last_lane = (lane + 5'd1) == n_fields;",
     "  wire last_lane = lane == n_fields;"),

    # ---- height16 -> fx16 is an EXACT, SIGN-EXTENDING raw << 8 -----------
    ("P10 base is zero-extended, so negative ground jumps to the sky",
     "  wire signed [31:0] base_fx = {{8{base_i[15]}}, base_i, 8'b0};",
     "  wire signed [31:0] base_fx = {8'b0, base_i, 8'b0};"),
    # P11 used to drop the scar entirely, which orphaned scar_fx and failed the
    # linter. Half strength is the same class of defect -- the baked layer
    # composed at the wrong weight -- and keeps the signal read.
    ("P11 the scar layer is composed at half strength",
     "  wire signed [31:0] sum_bs = fx_add_sat(base_fx, scar_fx);",
     "  wire signed [31:0] sum_bs = fx_add_sat(base_fx, scar_fx >>> 1);"),
    ("P12 the bottom is scaled wrong, so the underside sits 256x too low",
     "  wire signed [31:0] bot_fx = {{8{bottom_i[15]}}, bottom_i, 8'b0};",
     "  wire signed [31:0] bot_fx = {{16{bottom_i[15]}}, bottom_i};"),

    # ---- the saturating add ---------------------------------------------
    ("P13 fx_add wraps instead of saturating at the positive rail",
     "      if (s > 33'sd2147483647) fx_add_sat = 32'sh7FFF_FFFF;",
     "      if (s > 33'sd2147483647) fx_add_sat = s[31:0];"),
    ("P14 fx_add saturates the negative rail to the POSITIVE one",
     "      else if (s < -33'sd2147483648) fx_add_sat = 32'sh8000_0000;",
     "      else if (s < -33'sd2147483648) fx_add_sat = 32'sh7FFF_FFFF;"),

    # ---- the overflow law (9.1): reject the tail, never evict, never stall
    ("P15 the list accepts a 17th field, overrunning the frozen bound",
     "  wire list_full = (n_fields >= 5'(MaxFields));",
     "  wire list_full = (n_fields > 5'(MaxFields));"),
    ("P16 the intake STALLS when full instead of rejecting",
     "  assign fld_add_ready_o = 1'b1;",
     "  assign fld_add_ready_o = !list_full;"),
    ("P17 a clear and an offer in the same cycle put the record in the stale slot",
     "  wire [3:0] add_slot = list_clear_i ? 4'd0 : n_fields[3:0];",
     "  wire [3:0] add_slot = n_fields[3:0];"),

    # ---- the subpatch mask (chosen law 3) --------------------------------
    ("P18 a vertex on a subpatch border marks only ONE neighbour",
     "      col_lo = (vi == 6'd0) ? 6'd0 : ((vi - 6'd1) >> 3);",
     "      col_lo = vi >> 3;"),
    ("P19 the mask's row span collapses the same way",
     "      row_lo = (vj == 6'd0) ? 6'd0 : ((vj - 6'd1) >> 3);",
     "      row_lo = vj >> 3;"),

    # ---- the handshake ---------------------------------------------------
    ("P20 a vertex is taken with the result register still full",
     "  assign vtx_ready_o = !busy && out_free;",
     "  assign vtx_ready_o = !busy;"),
]


def read_rtl(path=RTL):
    return io.open(path, encoding="utf-8", newline="").read()


def mutate(gold, old, new):
    """Return the mutated text, or raise if the anchor is not unique.

    MIXED LINE ENDINGS ARE REAL AND THEY DEFEAT A SINGLE GUESS. This used to
    pick one ending -- CRLF if the file contained any -- and translate the
    anchor to it. A file edited by a tool that writes LF into an otherwise
    CRLF file then has BOTH, and a multi-line anchor silently matches zero
    times in the region that differs. Two engine mutants failed exactly that
    way on 2026-08-28 while every single-line anchor in the same table worked.

    So both forms are tried. A multi-line anchor that matches under either is
    accepted; one that matches under neither still raises, and one that
    matches under both is still ambiguous and raises too.
    """
    for nl in ("\r\n", "\n"):
        o = old.replace("\n", nl)
        n = new.replace("\n", nl)
        count = gold.count(o)
        if count == 1:
            if o == n:
                raise ValueError("mutant identical to base")
            return gold.replace(o, n, 1)
        if count > 1:
            raise ValueError("anchor matches %d times" % count)
    raise ValueError("anchor matches 0 times (tried CRLF and LF)")


def write_rtl(text, path=RTL):
    io.open(path, "w", encoding="utf-8", newline="").write(text)
    os.utime(path, None)  # NOW, never the future -- see sweep_terrain_patch.sh


def main(argv):
    if len(argv) >= 2 and argv[1] == "--count":
        print(len(MUTANTS))
        return 0
    if len(argv) >= 3 and argv[1] == "--name":
        print(MUTANTS[int(argv[2])][0])
        return 0
    if len(argv) >= 3 and argv[1] == "--apply":
        name, old, new = MUTANTS[int(argv[2])]
        try:
            write_rtl(mutate(read_rtl(), old, new))
        except ValueError as exc:
            sys.stderr.write("%s: %s\n" % (name, exc))
            return 9
        return 0
    sys.stderr.write("usage: --count | --name N | --apply N\n")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
