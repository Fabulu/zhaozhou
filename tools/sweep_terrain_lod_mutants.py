#!/usr/bin/env python3
"""The mutant table for zhao_terrain_lod.sv, and the apply/restore primitives.

WHY THIS IS A PYTHON MODULE AND NOT A BASH ARRAY
------------------------------------------------
`tools/sweep_geom_lod.sh` carries its mutants in a `MUTS=( "..." )` array of
DOUBLE-QUOTED shell strings. That works there because no mutation in that block
contains a dollar sign. It does NOT work here: `zhao_terrain_lod.sv` forms its
coordinate differences with `$signed(...)`, and inside a double-quoted bash word
`$signed` expands to the empty string. The anchor would then never match, or --
worse for a mutation that only partially mentions it -- would match a DIFFERENT
piece of text than the one written down, and the sweep would score a mutation
nobody authored. That is a sixth way to score a run that never happened, and it
is the reason this table lives where no shell ever reads it.

Every mutation is therefore stored here as an ordinary Python string, and both
the preflight (`sweep_terrain_lod_preflight.py`) and the sweep
(`sweep_terrain_lod.sh`) go through this one module, so they cannot disagree
about what a mutant is.

CRLF: the worktree is checked out with CRLF line endings. Anchors are written
here with plain "\\n" and translated to whatever the file actually uses, which
is the same trick the geom sweep's inline python does.
"""

import io
import os
import sys

RTL = "fpga/rtl/terrain/zhao_terrain_lod.sv"

# name, old, new -- each applied to the PRISTINE file, one at a time.
MUTANTS = [
    # ---- the ladder ------------------------------------------------------
    ("M01 ladder compares < instead of <=",
     "ladder_ok = (lhs <= rhs);",
     "ladder_ok = (lhs < rhs);"),
    # M02 was first written as `scale[15:8]`, which leaves scale[7:0] unread
    # and fails -Wall UNUSEDSIGNAL. The preflight caught it before any scoring.
    ("M02 the deviation is doubled",
     "lhs       = {9'b0, dev} * {24'b0, scale};",
     "lhs       = {8'b0, dev, 1'b0} * {24'b0, scale};"),
    ("M03 the right-hand side is off by one",
     "rhs       = {17'b0, dstv} * {33'b0, h};",
     "rhs       = {17'b0, dstv} * {33'b0, h} + 49'd1;"),
    ("M04 finest legal level wins, not coarsest",
     "      if (ladder_ok(dev1, scale, dstv, h)) ladder = 2'd1;\n"
     "      if (ladder_ok(dev2, scale, dstv, h)) ladder = 2'd2;\n"
     "      if (ladder_ok(dev3, scale, dstv, h)) ladder = 2'd3;",
     "      if (ladder_ok(dev3, scale, dstv, h)) ladder = 2'd3;\n"
     "      if (ladder_ok(dev2, scale, dstv, h)) ladder = 2'd2;\n"
     "      if (ladder_ok(dev1, scale, dstv, h)) ladder = 2'd1;"),
    ("M05 the top rung maps to level 2",
     "      if (ladder_ok(dev3, scale, dstv, h)) ladder = 2'd3;",
     "      if (ladder_ok(dev3, scale, dstv, h)) ladder = 2'd2;"),
    ("M06 the strict ladder is not strict",
     "    s0 = ladder(d_dev1, d_dev2, d_dev3, cam0_scale_i, sq_res0[31:0], 16'd256);",
     "    s0 = ladder(d_dev1, d_dev2, d_dev3, cam0_scale_i, sq_res0[31:0], hyst_eff);"),

    # ---- the square root -------------------------------------------------
    ("M07 the root runs one step short",
     "  localparam int unsigned SqrtSteps = 32;",
     "  localparam int unsigned SqrtSteps = 31;"),
    ("M08 the root's bit steps by one, not two",
     "      sq_bit  <= sq_bit >> 2;",
     "      sq_bit  <= sq_bit >> 1;"),
    ("M09 the root's residual forgets its shift",
     "    sqrt_next_res = (num >= (res + bitv)) ? ((res >> 1) + bitv) : (res >> 1);",
     "    sqrt_next_res = (num >= (res + bitv)) ? ((res >> 1) + bitv) : res;"),
    ("M10 the root subtracts on > instead of >=",
     "    sqrt_next_num = (num >= (res + bitv)) ? (num - (res + bitv)) : num;",
     "    sqrt_next_num = (num > (res + bitv)) ? (num - (res + bitv)) : num;"),

    # ---- the squared distance -------------------------------------------
    # M11 was first written as a plain wrap (`dsq_sat = s[63:0]`), which leaves
    # s[65:64] unread and fails -Wall. One short of the rail is the same defect
    # -- the saturation value is wrong -- and it lints.
    ("M11 the squared distance saturates one short of the rail",
     "    dsq_sat = (s[65:64] != 2'b00) ? 64'hFFFF_FFFF_FFFF_FFFF : s[63:0];",
     "    dsq_sat = (s[65:64] != 2'b00) ? 64'hFFFF_FFFF_FFFF_FFFE : s[63:0];"),
    # M12 was first written as "drop the y term", which leaves cam0_y_i unused
    # and fails -Wall. Transposing the two axes is the same class of defect --
    # and is the distance-side twin of M27's neighbour transposition.
    ("M12 camera 0's x and y axes are transposed",
     "    dsq0 = dsq_sat(sq66(diff33(sp_cx_i, cam0_x_i)) + sq66(diff33(sp_cy_i, cam0_y_i)) +",
     "    dsq0 = dsq_sat(sq66(diff33(sp_cx_i, cam0_y_i)) + sq66(diff33(sp_cy_i, cam0_x_i)) +"),
    ("M13 the coordinate difference is formed unsigned",
     "    diff33 = $signed({a[31], a}) - $signed({b[31], b});",
     "    diff33 = $signed({1'b0, a}) - $signed({1'b0, b});"),

    # ---- the two cameras -------------------------------------------------
    ("M14 the cameras take the coarser strict decision",
     "      t_strict  = (s0 < s1) ? s0 : s1;",
     "      t_strict  = (s0 > s1) ? s0 : s1;"),
    ("M15 the cameras take the coarser relaxed decision",
     "      t_relaxed = (r0 < r1) ? r0 : r1;",
     "      t_relaxed = (r0 > r1) ? r0 : r1;"),
    ("M16 with no camera enabled the level goes coarsest",
     "      t_strict  = d_level;\n      t_relaxed = d_level;",
     "      t_strict  = 2'd3;\n      t_relaxed = 2'd3;"),

    # ---- the band --------------------------------------------------------
    ("M17 the hysteresis floor is removed",
     "  wire [15:0] hyst_eff = (hyst_i < 16'd256) ? 16'd256 : hyst_i;",
     "  wire [15:0] hyst_eff = hyst_i;"),
    ("M18 the band targets the far edge, not the near one",
     "    if (d_level < t_strict) want = t_strict;\n"
     "    else if (d_level > t_relaxed) want = t_relaxed;",
     "    if (d_level < t_strict) want = t_relaxed;\n"
     "    else if (d_level > t_relaxed) want = t_strict;"),

    # ---- the minimum hold ------------------------------------------------
    ("M19 the hold refuses a change at exactly min_hold",
     "  wire        hold_ok = (d_hold >= min_hold_i);",
     "  wire        hold_ok = (d_hold > min_hold_i);"),
    ("M20 a refused change does not age the subpatch",
     "    else n_hold = (d_hold == 8'hFF) ? 8'hFF : (d_hold + 8'd1);",
     "    else n_hold = d_hold;"),
    ("M21 the hold is not cleared on a change",
     "    if (n_changed) n_hold = 8'd0;",
     "    if (n_changed) n_hold = d_hold;"),
    ("M22 the hold does not saturate",
     "    else n_hold = (d_hold == 8'hFF) ? 8'hFF : (d_hold + 8'd1);",
     "    else n_hold = d_hold + 8'd1;"),

    # ---- the geomorph walk ----------------------------------------------
    ("M23 refine adopts the finer level with the factor at zero",
     "        n_morph   = (step == 17'd0) ? 17'd0 : MorphOne;",
     "        n_morph   = 17'd0;"),
    ("M24 the coarsen commit is off by one",
     "      if ((step == 17'd0) || (morph_up >= {1'b0, MorphOne})) begin",
     "      if ((step == 17'd0) || (morph_up > {1'b0, MorphOne})) begin"),
    ("M25 morph_step 0 does not snap a coarsening",
     "      if ((step == 17'd0) || (morph_up >= {1'b0, MorphOne})) begin",
     "      if (morph_up >= {1'b0, MorphOne}) begin"),
    ("M26 the incoming morph factor is not clamped",
     "  wire [16:0] morph_in = (d_morph > MorphOne) ? MorphOne : d_morph;",
     "  wire [16:0] morph_in = d_morph;"),

    # ---- the neighbour lookup -------------------------------------------
    ("M27 the -x neighbour lookup transposes the grid index",
     "    nb_nx = (e_i != 2'd0) ? lvl[{e_j, e_i - 2'd1}] : edge_lane(edge_nx_i, e_j);",
     "    nb_nx = (e_i != 2'd0) ? lvl[{e_i - 2'd1, e_j}] : edge_lane(edge_nx_i, e_j);"),
    ("M28 the -z neighbour reads the +z cell",
     "    nb_nz = (e_j != 2'd0) ? lvl[{e_j - 2'd1, e_i}] : edge_lane(edge_nz_i, e_i);",
     "    nb_nz = (e_j != 2'd0) ? lvl[{e_j + 2'd1, e_i}] : edge_lane(edge_nz_i, e_i);"),
    ("M29 the -z border lane is indexed along the wrong axis",
     "    nb_nz = (e_j != 2'd0) ? lvl[{e_j - 2'd1, e_i}] : edge_lane(edge_nz_i, e_i);",
     "    nb_nz = (e_j != 2'd0) ? lvl[{e_j - 2'd1, e_i}] : edge_lane(edge_nz_i, e_j);"),

    # ---- the packet, the counters and the handshake ----------------------
    ("M30 the level-2 triangle estimate is wrong",
     "      2'd2: tris_of = 32'd8;",
     "      2'd2: tris_of = 32'd4;"),
    ("M31 the counters advance on the underside, not the top",
     "            if (!emit_surf) begin",
     "            if (emit_surf) begin"),
    ("M32 a packet is dropped under backpressure",
     "      if (out_valid_o && out_ready_i) out_valid_o <= 1'b0;",
     "      if (out_valid_o) out_valid_o <= 1'b0;"),
    ("M33 ready is asserted outside the fill phase",
     "  assign sp_ready_o = (state == StFill);",
     "  assign sp_ready_o = (state != StEmit);"),
    ("M34 ox and oz are swapped",
     "            out_ox_o      <= {1'b0, e_i, 3'b000};",
     "            out_ox_o      <= {1'b0, e_j, 3'b000};"),
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


def write_rtl(text, path=RTL):
    io.open(path, "w", encoding="utf-8", newline="").write(text)
    os.utime(path, None)  # NOW, never the future -- see sweep_terrain_lod.sh


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
