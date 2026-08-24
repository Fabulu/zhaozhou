#!/usr/bin/env python3
"""The mutant table for zhao_terrain_normals.sv, and the apply/restore primitives.

WHY THIS IS A PYTHON MODULE AND NOT A BASH ARRAY
------------------------------------------------
`tools/sweep_geom_lod.sh` carries its mutants in a `MUTS=( "..." )` array of
DOUBLE-QUOTED shell strings. That works there because no mutation in that block
contains a dollar sign. It does NOT work here: `zhao_terrain_normals.sv` forms its
coordinate differences with `$signed(...)`, and inside a double-quoted bash word
`$signed` expands to the empty string. The anchor would then never match, or --
worse for a mutation that only partially mentions it -- would match a DIFFERENT
piece of text than the one written down, and the sweep would score a mutation
nobody authored. That is a sixth way to score a run that never happened, and it
is the reason this table lives where no shell ever reads it.

Every mutation is therefore stored here as an ordinary Python string, and both
the preflight (`sweep_terrain_normals_preflight.py`) and the sweep
(`sweep_terrain_normals.sh`) go through this one module, so they cannot disagree
about what a mutant is.

CRLF: the worktree is checked out with CRLF line endings. Anchors are written
here with plain "\\n" and translated to whatever the file actually uses, which
is the same trick the geom sweep's inline python does.
"""

import io
import os
import sys

RTL = "fpga/rtl/terrain/zhao_terrain_normals.sv"

# name, old, new -- each applied to the PRISTINE file, one at a time.
MUTANTS = [
    # ---- the shared-multiplier walk -------------------------------------
    # M01 is the bug this sweep was written after. `idle_o` was
    # `!s1_valid && !s2_valid`, which was complete before sequencing put state
    # between the handshake and the result. terrain_tess_normals caught it as
    # 127 normals where 128 were due, with every VALUE correct. If this mutant
    # ever survives, the chain test has stopped draining on idle_o.
    ("M01 idle_o forgets the walk, so a drain stops one triangle early",
     "  assign idle_o       = !m_busy && !s1_valid && !s2_valid;",
     "  assign idle_o       = !s1_valid && !s2_valid;"),
    ("M02 the walk emits one term early",
     "        if (mseq == 3'(NSTEP - 1)) begin",
     "        if (mseq == 3'(NSTEP - 2)) begin"),
    ("M03 acc0's second term is added instead of subtracted",
     "          3'd1: acc0 <= acc0 - $signed({{1{m_p[65]}}, m_p});",
     "          3'd1: acc0 <= acc0 + $signed({{1{m_p[65]}}, m_p});"),
    ("M04 acc1's second term is added instead of subtracted",
     "          3'd3: acc1 <= acc1 - $signed({{1{m_p[65]}}, m_p});",
     "          3'd3: acc1 <= acc1 + $signed({{1{m_p[65]}}, m_p});"),
    ("M05 step 2 multiplies the wrong pair, so ny is nx",
     "      3'd2: begin m_a = l1z; m_b = l2x; end",
     "      3'd2: begin m_a = l1y; m_b = l2z; end"),
    ("M06 step 5 swaps its operands with step 4, killing the nz difference",
     "      3'd5: begin m_a = l1y; m_b = l2x; end",
     "      3'd5: begin m_a = l1x; m_b = l2y; end"),
    ("M07 s1_n2 takes the register, one cycle stale, instead of the live value",
     "          s1_n2 <= acc2 - $signed({{1{m_p[65]}}, m_p});",
     "          s1_n2 <= acc2;"),
    ("M08 ready admits a triangle while the multiplier is mid-walk",
     "  assign tri_ready_o = !m_busy && !s1_valid;",
     "  assign tri_ready_o = !s1_valid;"),
    # First written as `l1y <= e1x` in the latch, which left e1y unread and
    # failed -Wall UNUSEDSIGNAL. The preflight caught it before anything was
    # scored -- exactly the guard added after a sweep once reported 21/22 where
    # the truth was 22/23, because a mutant that fails to build re-runs the
    # previous binary and its build failure scores as a catch.
    ("M09 step 0 reads the LIVE edge instead of its latched copy",
     "      3'd0: begin m_a = l1y; m_b = l2z; end",
     "      3'd0: begin m_a = e1y; m_b = l2z; end"),
    ("M10 the degeneracy test reads one lane before its rescale",
     "          s2_degen <= (r_nx == 32'sd0) && (r_ny == 32'sd0) && (r_nz == 32'sd0);",
     "          s2_degen <= (s1_n0 == 67'sd0) && (r_ny == 32'sd0) && (r_nz == 32'sd0);"),
    ("M11 the rescale floors instead of rounding half up",
     "      r = (x + 67'sd32768) >>> 16;",
     "      r = x >>> 16;"),
    ("M12 the walk restarts from step 1, silently dropping the first term",
     "          mseq   <= 3'd0;",
     "          mseq   <= 3'd1;"),
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
    os.utime(path, None)  # NOW, never the future -- see sweep_terrain_normals.sh


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
