#!/usr/bin/env python3
"""The mutant table for zhao_field_v3_mulbank.sv (FIELD.V3.MULBANK).

WHY THIS SWEEP EXISTS
---------------------
This block routes every product in the engine. Its differential shows that
18,202 requests over 20,000 clocks each came back with their own a*b -- strong
evidence it works, and no evidence at all that the tests would NOTICE if it
stopped. That second question is this file.

WHAT ITS FAILURES LOOK LIKE
---------------------------
Quiet. A misrouted product is still A product: the right shape, the right
width, plausibly signed. It is wrong only against the claimant who asked, and
it is wrong for TWO claimants at once -- one gets a value it never requested
and another loses the value it did. Nothing about the pipeline stops, no
counter looks odd, and every downstream block computes confidently on it.

The three claims worth attacking:

  * THE REPLY GOES TO WHOEVER ASKED. The tag shadow is two stages because the
    multiplier is two clocks deep. Off by one in either direction hands every
    product to the previous or the next requester.
  * ONE CLAIMANT IS ACCEPTED PER CLOCK. If two are told `ready` at once, both
    believe they own the bank and one of them silently loses its request.
  * THE PRIORITY IS THE DECLARED ONE, and the starvation it causes is
    COUNTED. A stall counter that undercounts turns a measured cost into an
    invisible one.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/field/zhao_field_v3_mulbank.sv"

# name, old, new -- each applied to the PRISTINE file, one at a time.
MUTANTS = [
    # ---- the reply must reach whoever asked -------------------------------
    # Reshaped: routing by who_s1 alone orphaned who_s2. Requiring BOTH
    # stages to name the same claimant keeps every operand and is a real
    # defect -- a reply is dropped whenever consecutive grants go to
    # different claimants, which is the normal case under contention.
    ("M01 a reply is delivered only if both shadow stages agree",
     "      rsp_valid_o[i] = v_s2 && (who_s2 == CW'(i));",
     "      rsp_valid_o[i] = v_s2 && (who_s2 == CW'(i)) && (who_s1 == CW'(i));"),
    # Reshaped: bypassing stage 1 orphaned who_s1 and tag_s1. Making the
    # stage-2 valid too EAGER keeps them and announces a reply a clock before
    # the product exists.
    ("M02 the reply valid fires a clock before the product lands",
     "      v_s2   <= v_s1;",
     "      v_s2   <= v_s1 | grant_v_c;"),
    # Reshaped: tag_s1 alone orphaned tag_s2.
    ("M03 the reply carries the stage-1 tag whenever one is in flight",
     "  assign rsp_tag_o = tag_s2;",
     "  assign rsp_tag_o = v_s1 ? tag_s1 : tag_s2;"),
    # Reshaped: dropping the comparison orphaned who_s2. Telling the LANE
    # GROUP as well as the winner keeps it and is the same defect in the place
    # it would actually happen.
    ("M04 the lane group is told every reply is also theirs",
     "      rsp_valid_o[i] = v_s2 && (who_s2 == CW'(i));",
     "      rsp_valid_o[i] = v_s2 && ((who_s2 == CW'(i)) || (i == 0));"),

    # ---- one claimant per clock -------------------------------------------
    ("M05 every asking claimant is told it won",
     "      req_ready_o[i] = grant_v_c && (grant_c == CW'(i));",
     "      req_ready_o[i] = req_valid_i[i];"),
    ("M06 a grant is issued even when nobody asked",
     "    grant_v_c = |req_valid_i;",
     "    grant_v_c = 1'b1;"),

    # ---- the priority, and the cost it is declared to have ----------------
    ("M07 the priority is reversed, so the lanes outrank the services",
     "      for (int i = 0; i < CLAIMANTS; i++) if (req_valid_i[i]) grant_c = CW'(i);",
     "      for (int i = CLAIMANTS - 1; i >= 0; i--) if (req_valid_i[i]) grant_c = CW'(i);"),
    ("M08 the lane-stall counter also counts the clocks the lanes WON",
     "      if (req_valid_i[0] && !(grant_v_c && grant_c == CW'(0)))\n"
     "        stall_lanes_o <= stall_lanes_o + 32'd1;",
     "      if (req_valid_i[0])\n"
     "        stall_lanes_o <= stall_lanes_o + 32'd1;"),
    ("M09 the grant counter counts every clock, asking or not",
     "      if (grant_v_c) grants_o <= grants_o + 32'd1;",
     "      grants_o <= grants_o + 32'd1;"),

    # ---- the operands actually multiplied ---------------------------------
    ("M10 the bank always multiplies claimant 0's operands",
     "          .a_i      (req_a_i[grant_c][l]),\n"
     "          .b_i      (req_b_i[grant_c][l]),",
     "          .a_i      (req_a_i[0][l]),\n"
     "          .b_i      (req_b_i[0][l]),"),
    ("M11 lane l multiplies lane l's a against lane 0's b",
     "          .b_i      (req_b_i[grant_c][l]),",
     "          .b_i      (req_b_i[grant_c][0]),"),
    ("M12 the multiplier is issued every clock, grant or not",
     "          .issue_i  (grant_v_c),",
     "          .issue_i  (1'b1),"),

    # ---- the desync guard itself ------------------------------------------
    # A guard that cannot fire is decoration. It is mutated like anything else.
    ("M13 the desync guard never latches",
     "      if (v_s2 ? (p_valid_lane != 4'hF) : (p_valid_lane != 4'h0)) desync_o <= 1'b1;",
     "      if (v_s2 ? (p_valid_lane != 4'hF) : (p_valid_lane == 4'h0)) desync_o <= 1'b1;"),
    # Reshaped: narrowing to lane 0 orphaned the other three bits -- which
    # is inherent, since the mutant WEAKENS the check. Inverting the sense
    # keeps all four bits and weakens it just as badly: it now fires only when
    # EVERY lane disagrees, so one lane falling out of step is invisible.
    ("M14 the desync guard fires only if all four lanes disagree at once",
     "      if (v_s2 ? (p_valid_lane != 4'hF) : (p_valid_lane != 4'h0)) desync_o <= 1'b1;",
     "      if (v_s2 ? (p_valid_lane == 4'h0) : (p_valid_lane == 4'hF)) desync_o <= 1'b1;"),
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


# Machine-readable, so a survivor is either PROVEN equivalent here or fails the
# sweep. Nothing is declared until the first run says what actually survives.
#
# AND NOTHING IS DECLARED PREDICTIVELY. That rule was broken twice on
# FIELD.V3.EXEC -- two proofs written before any evidence, both then
# contradicted by a run. An equivalence claim is worth its evidence, and the
# evidence is a SURVIVING mutant, never an argument.
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
        entry = MUTANTS[int(argv[2])]
        name = entry[0]
        path = entry[1] if len(entry) == 4 else RTL
        old, new = entry[-2], entry[-1]
        try:
            write_rtl(mutate(read_rtl(path), old, new), path)
        except ValueError as exc:
            sys.stderr.write("%s: %s\n" % (name, exc))
            return 9
        return 0
    sys.stderr.write("usage: --count | --name N | --equiv TOK | --apply N\n")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
