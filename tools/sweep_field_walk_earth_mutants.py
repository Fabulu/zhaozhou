#!/usr/bin/env python3
"""The mutant table for zhao_probe_walk_earth.sv (FIELD.WALK.EARTH).

WHY THIS SWEEP EXISTS
---------------------
The walker is the block Field v3 turns on: it deletes v2's 27,225-clock point
transport by GENERATING the lattice instead of receiving it. Everything
downstream -- the executor, the accumulator, the composed-height cache -- is
fed by whatever this block decides the lattice is. A walker that is wrong in
a way its tests do not notice produces terrain that is wrong everywhere, and
every block behind it stays green while it happens.

WHAT MAKES THIS BLOCK'S FAILURES HARD TO SEE
---------------------------------------------
Almost every mutation here produces output that still LOOKS like a lattice
walk: the right number of groups, plausible coordinates, a mask with some
bits set. The stream is only wrong against the reference, which is why the
differential compares element by element against `compose_lattice`'s own two
rules rather than against a hand-written expectation.

Three of the block's claims are the ones worth attacking, because each is a
place where a cheaper wrong thing is available and looks identical in a
casual test:

  * THE BOX IS A HINT, THE PER-VERTEX TEST IS THE LAW. Using the prepared
    box as the coverage answer is faster, simpler, and correct on every
    association whose box is exact. It is wrong exactly when the box is
    conservative -- which is the case the ARM is allowed to produce.
  * THE INTERVAL IS CLOSED (spec/terrain_rules.md 9.1). A border vertex is
    INSIDE. Half-open is the more common convention and one character away.
  * A GROUP NEVER STRADDLES A ROW, which is what lets one z serve four
    lanes. Break the row discipline and three lanes silently take the wrong
    row's z.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/synth/zhao_probe_walk_earth.sv"

# name, old, new -- each applied to the PRISTINE file, one at a time.
MUTANTS = [
    # ---- the coverage law: the box must never decide -----------------------
    # W01 is THE mutant of this block: it lets the HINT grant coverage the
    # LAW would refuse. It is correct on every association whose box is exact
    # and wrong exactly when the box is conservative -- which is the case the
    # ARM is permitted to produce. If it survives, no test drives an inflated
    # box, and the block's central claim is untested.
    #
    # First written as a straight deletion of the coordinate test. That
    # orphaned fp_x1_r and in_z_c and failed the LINTER rather than the
    # tests, and a mutant that cannot build is a discard, not evidence.
    # ORing the box in keeps every operand and keeps the defect.
    ("W01 the prepared box also grants coverage, so a conservative box over-covers",
     "      mask_c[l] = busy_r && (li_c[l] < 6'(LAT_W)) && in_z_c &&\n"
     "                  (lx_c[l] >= fp_x0_r) && (lx_c[l] <= fp_x1_r);",
     "      mask_c[l] = busy_r && (li_c[l] < 6'(LAT_W)) &&\n"
     "                  (((li_c[l] >= box_i0_r) && (li_c[l] <= box_i1_r)) ||\n"
     "                   (in_z_c && (lx_c[l] >= fp_x0_r) && (lx_c[l] <= fp_x1_r)));"),
    ("W02 the x footprint becomes half-open, so the far border falls outside",
     "                  (lx_c[l] >= fp_x0_r) && (lx_c[l] <= fp_x1_r);",
     "                  (lx_c[l] >= fp_x0_r) && (lx_c[l] < fp_x1_r);"),
    ("W03 the z footprint becomes half-open on the near side",
     "  assign in_z_c = (wz[cur_j_r] >= fp_z0_r) && (wz[cur_j_r] <= fp_z1_r);",
     "  assign in_z_c = (wz[cur_j_r] > fp_z0_r) && (wz[cur_j_r] <= fp_z1_r);"),
    # Reshaped: swapping the axis outright orphaned both x registers. The
    # cross-axis copy-paste that actually happens is an EXTRA constraint.
    ("W04 the z far bound is also applied to x",
     "                  (lx_c[l] >= fp_x0_r) && (lx_c[l] <= fp_x1_r);",
     "                  (lx_c[l] >= fp_x0_r) && (lx_c[l] <= fp_x1_r) &&\n"
     "                  (lx_c[l] <= fp_z1_r);"),
    ("W05 a lane past the end of the row is still marked covered",
     "      mask_c[l] = busy_r && (li_c[l] < 6'(LAT_W)) && in_z_c &&",
     "      mask_c[l] = busy_r && in_z_c &&"),

    # ---- the walk arithmetic ----------------------------------------------
    ("W06 the group stride is 3, so vertices are visited twice",
     "          cur_i_r <= cur_i_r + 6'd4;",
     "          cur_i_r <= cur_i_r + 6'd3;"),
    ("W07 a new row restarts at column 0 instead of the box's first column",
     "          cur_i_r <= box_i0_r;\n"
     "          cur_j_r <= cur_j_r + 6'd1;",
     "          cur_i_r <= box_i0_r + 6'd1;\n"
     "          cur_j_r <= cur_j_r + 6'd1;"),
    # W08 is DEGENERATE while the lattice is square and is declared
    # equivalent below. W18 states the same defect as a LITERAL so the index
    # arithmetic is actually scored -- a mutant whose meaning depends on two
    # parameters happening to be equal is not coverage.
    ("W08 the vertex index is strided by the lattice HEIGHT",
     "  assign out_iv_o    = 11'(cur_j_r) * 11'(LAT_W) + 11'(cur_i_r);",
     "  assign out_iv_o    = 11'(cur_j_r) * 11'(LAT_H) + 11'(cur_i_r);"),
    ("W18 the vertex index is strided by 32 rather than the lattice width",
     "  assign out_iv_o    = 11'(cur_j_r) * 11'(LAT_W) + 11'(cur_i_r);",
     "  assign out_iv_o    = 11'(cur_j_r) * 11'd32 + 11'(cur_i_r);"),
    ("W09 the row ends one group early, dropping each row's tail",
     "  assign row_last_c   = (cur_i_r + 6'd4) > box_i1_r;",
     "  assign row_last_c   = (cur_i_r + 6'd4) >= box_i1_r;"),
    ("W10 the association ends one row early",
     "  assign assoc_last_c = row_last_c && (cur_j_r >= box_j1_r);",
     "  assign assoc_last_c = row_last_c && (cur_j_r >= (box_j1_r - 6'd1));"),

    # ---- one z per group, and the tables ----------------------------------
    ("W11 the row's z is indexed by the COLUMN counter",
     "  assign out_z_o     = wz[cur_j_r];",
     "  assign out_z_o     = wz[cur_i_r];"),
    ("W12 the z coverage test reads a different row than the group carries",
     "  assign in_z_c = (wz[cur_j_r] >= fp_z0_r) && (wz[cur_j_r] <= fp_z1_r);",
     "  assign in_z_c = (wz[cur_i_r] >= fp_z0_r) && (wz[cur_i_r] <= fp_z1_r);"),
    ("W13 the table select is inverted, so x and z land in each other's table",
     "      if (lt_sel_i) wz[lt_idx_i] <= lt_val_i;\n"
     "      else wx[lt_idx_i] <= lt_val_i;",
     "      if (lt_sel_i) wx[lt_idx_i] <= lt_val_i;\n"
     "      else wz[lt_idx_i] <= lt_val_i;"),

    # ---- intake and handshake ----------------------------------------------
    # Reshaped: dropping the term orphaned box_empty_c. Inverting it keeps
    # the operand and keeps a real defect -- only EMPTY boxes start a walk.
    ("W14 the emptiness guard is inverted, so real associations never start",
     "        if (as_valid_i && !box_empty_c) begin",
     "        if (as_valid_i && box_empty_c) begin"),
    ("W15 the emptiness test only looks at the column range",
     "  assign box_empty_c = (as_box_i0_i > as_box_i1_i) || (as_box_j0_i > as_box_j1_i);",
     "  assign box_empty_c = (as_box_i0_i > as_box_i1_i);"),
    # Reshaped: dropping out_ready_i orphaned the port.
    ("W16 the walk advances when EITHER side is ready, not both",
     "  assign accept_c = out_valid_o && out_ready_i;",
     "  assign accept_c = out_valid_o || out_ready_i;"),
    ("W17 the covered-vertex counter counts groups rather than lanes",
     "        verts_covered_o  <= verts_covered_o + 32'($countones(mask_c));",
     "        verts_covered_o  <= verts_covered_o + 32'd1;"),
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
# sweep. Nothing is declared until the first run says what actually survives --
# writing a proof before the evidence is how a hole acquires a note.
EQUIVALENT = {
    "W08":
        "DEGENERATE WHILE THE LATTICE IS SQUARE. The Earth profile sets "
        "LAT_W = LAT_H = 33, so `cur_j * LAT_W` and `cur_j * LAT_H` are the "
        "same expression and Verilator emits a BYTE-IDENTICAL model. The "
        "sweep's binary-hash check reported it as a discard rather than a "
        "survivor, which is the guard working: it cannot tell 'semantically "
        "identical' from 'failed to re-elaborate', and refusing to score "
        "either is right. "
        "The defect this mutant was meant to probe -- a wrong row stride in "
        "the vertex index -- IS scored, by W18, which uses a literal 32 and "
        "so does not depend on the two parameters being equal. "
        "RE-SCORE THIS THE MOMENT LAT_W != LAT_H. A non-square lattice makes "
        "it a real mutant again, and the parameters, not this note, are the "
        "thing to watch."
}


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
