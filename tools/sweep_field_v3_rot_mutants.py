#!/usr/bin/env python3
"""The mutant table for zhao_field_v3_rot.sv (FIELD.V3.ROT).

THE DEFECT THIS BLOCK ALREADY HAD, AND WHAT IT LOOKED LIKE
-----------------------------------------------------------
The first build captured COS AND SIN SWAPPED. That is not a subtle error and
it is not a loud one either: swapping them is exactly a rotation by
(90 - theta), so every answer was a VALID rotation of the right point by the
wrong angle. Nothing looked corrupt, no flag fired, no guard tripped.

The differential caught it on the first run and pointed straight at it: lane 0
at 22.5 degrees produced lane 2's 67.5-degree answer, and lane 3 at 90 degrees
produced the 0-degree one. Two complements in the same failure is a signature.

R01 is that defect, kept as a mutant so the check that found it cannot be
weakened later without the sweep noticing.

WHAT ELSE THIS BLOCK CLAIMS
---------------------------
  * the angle is the LOW SIXTEEN BITS, per point;
  * each point gets its OWN cos and sin, from its own angle;
  * each product is ROUNDED SEPARATELY -- the reference's law, and the
    opposite of the house style everywhere else;
  * the pass-through lane is COPIED, not computed;
  * ROT2 writes no third lane;
  * every bank issue holds until granted.

PREFER MUTATING A VALUE OVER DELETING A USE -- see
sweep_field_v3_dispatch_mutants.py for the three preflight refusals that rule
came from.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/field/zhao_field_v3_rot.sv"

MUTANTS = [
    # ---- the trig walk ------------------------------------------------------
    ("R01 cos and sin are captured swapped -- the defect this block HAD",
     "          if (trig_k[0] == 1'b0) h_c[2'((trig_k - 4'd2) >> 1)] <= sin_result;\n"
     "          else                   h_s[2'((trig_k - 4'd2) >> 1)] <= sin_result;",
     "          if (trig_k[0] == 1'b0) h_s[2'((trig_k - 4'd2) >> 1)] <= sin_result;\n"
     "          else                   h_c[2'((trig_k - 4'd2) >> 1)] <= sin_result;"),
    # R02 SURVIVED and is equivalent -- proof in EQUIVALENT below. R24 is the
    # same claim about the table's LATENCY put where it is observable.
    ("R02 the answer is captured one clock early, before the table has it",
     "        if (trig_k >= 4'd2) begin",
     "        if (trig_k >= 4'd1) begin"),
    ("R24 the capture files each answer under the wrong issue index",
     "          if (trig_k[0] == 1'b0) h_c[2'((trig_k - 4'd2) >> 1)] <= sin_result;\n"
     "          else                   h_s[2'((trig_k - 4'd2) >> 1)] <= sin_result;",
     "          if (trig_k[0] == 1'b0) h_c[2'((trig_k - 4'd1) >> 1)] <= sin_result;\n"
     "          else                   h_s[2'((trig_k - 4'd1) >> 1)] <= sin_result;"),
    ("R03 the capture lands in the wrong point's slot",
     "          if (trig_k[0] == 1'b0) h_c[2'((trig_k - 4'd2) >> 1)] <= sin_result;",
     "          if (trig_k[0] == 1'b0) h_c[2'(4'd3 - ((trig_k - 4'd2) >> 1))] <= sin_result;"),
    ("R04 every point looks up point 0's angle",
     "  assign sin_angle_c  = h_ang[trig_k[2:1]];",
     "  assign sin_angle_c  = h_ang[2'd0];"),
    ("R05 the cos/sin alternation is dropped, so every lookup is a sine",
     "  assign sin_is_cos_c = ~trig_k[0];",
     "  assign sin_is_cos_c = 1'b0;"),
    ("R06 the angle takes the whole word, not the low half",
     "            h_ang[0] <= ang_0_i[15:0];",
     "            h_ang[0] <= ang_0_i[31:16];"),

    # ---- the operand mux ----------------------------------------------------
    ("R07 the X axis takes the wrong pair",
     "        sel_p[l] = h_a1[l];\n"
     "        sel_q[l] = h_a2[l];",
     "        sel_p[l] = h_a2[l];\n"
     "        sel_q[l] = h_a1[l];"),
    ("R08 the Y axis takes the Z axis's pair",
     "        sel_p[l] = h_a2[l];\n"
     "        sel_q[l] = h_a0[l];",
     "        sel_p[l] = h_a0[l];\n"
     "        sel_q[l] = h_a1[l];"),
    ("R09 ROT2 is treated as ROT3, so the axis mux applies to it",
     "      if (h_rot3 && (h_axis == 2'd0)) begin",
     "      if ((h_axis == 2'd0)) begin"),
    ("R10 the second product is s*p rather than s*q",
     "        R_SQ: begin\n"
     "          mul_a[l] = 33'(h_s[l]);\n"
     "          mul_b[l] = 33'(sel_q[l]);\n"
     "        end",
     "        R_SQ: begin\n"
     "          mul_a[l] = 33'(h_s[l]);\n"
     "          mul_b[l] = 33'(sel_p[l]);\n"
     "        end"),
    ("R11 the first product uses sin where it should use cos",
     "        R_CP: begin\n"
     "          mul_a[l] = 33'(h_c[l]);",
     "        R_CP: begin\n"
     "          mul_a[l] = 33'(h_s[l]);"),

    # ---- the rounding, which is the law that looks like a defect -----------
    ("R12 the rounding truncates instead of rounding half up",
     "      r = (65'(v) + 65'sd32768) >>> 16;\n"
     "      if (r > 65'sd2147483647) resc16 = 32'sh7FFF_FFFF;",
     "      r = (65'(v)) >>> 16;\n"
     "      if (r > 65'sd2147483647) resc16 = 32'sh7FFF_FFFF;"),
    ("R13 the rescale shifts by the wrong amount",
     "      r = (65'(v) + 65'sd32768) >>> 16;\n"
     "      if (r > 65'sd2147483647) resc16 = 32'sh7FFF_FFFF;",
     "      r = (65'(v) + 65'sd32768) >>> 15;\n"
     "      if (r > 65'sd2147483647) resc16 = 32'sh7FFF_FFFF;"),
    ("R14 the two rotated values are swapped",
     "  function automatic logic signed [31:0] rot_lo(input logic [1:0] l);\n"
     "    rot_lo = sub_sat(pr_cp[l], pr_sq[l]);",
     "  function automatic logic signed [31:0] rot_lo(input logic [1:0] l);\n"
     "    rot_lo = add_sat(pr_sp[l], pr_cq[l]);"),
    ("R15 the difference becomes a sum, fusing what the law keeps apart",
     "    rot_lo = sub_sat(pr_cp[l], pr_sq[l]);",
     "    rot_lo = add_sat(pr_cp[l], pr_sq[l]);"),

    # ---- the destination mapping -------------------------------------------
    ("R16 the X axis writes its pass-through to the wrong register",
     "    if (h_rot3 && (h_axis == 2'd0))      out0 = h_a0[l];   // X: passes through",
     "    if (h_rot3 && (h_axis == 2'd0))      out0 = h_a1[l];   // X: passes through"),
    ("R17 the pass-through lane is a rotated value rather than the input",
     "    else                                 out2 = h_a2[l];   // Z: passes through",
     "    else                                 out2 = rot_hi(l);   // Z: passes through"),
    ("R18 ROT2 writes a third lane, which the op does not have",
     "    if (!h_rot3)                         out2 = 32'sd0;    // law 5: ROT2 has none",
     "    if (!h_rot3)                         out2 = rot_hi(l);    // law 5: ROT2 has none"),
    ("R19 the Y axis swaps which destination gets which rotated value",
     "    else if (h_rot3 && (h_axis == 2'd1)) out0 = rot_hi(l); // Y: q's slot",
     "    else if (h_rot3 && (h_axis == 2'd1)) out0 = rot_lo(l); // Y: q's slot"),

    # ---- the refusal loop ---------------------------------------------------
    # mul_ready_i is read in four issue states, so dropping ONE leaves the port
    # live. Writing the same defect across all four would orphan it and be
    # refused by the preflight.
    ("R20 the first product advances on a REFUSED request",
     "        R_CP: if (mul_ready_i) state <= R_CPW;",
     "        R_CP: state <= R_CPW;"),
    ("R21 the third product reads its grant inverted",
     "        R_SP: if (mul_ready_i) state <= R_SPW;",
     "        R_SP: if (!mul_ready_i) state <= R_SPW;"),

    # ---- the flags ----------------------------------------------------------
    ("R22 a multiply saturation is reported on the wrong lane",
     "              sat_mul_o[l] <= mul_fired[l];",
     "              sat_mul_o[l] <= mul_fired[3 - l];"),
    ("R23 the add flag ignores the second sum",
     "              sat_add_o[l] <= sub_fired(pr_cp[l], pr_sq[l]) ||\n"
     "                              add_fired(pr_sp[l], pr_cq[l]);",
     "              sat_add_o[l] <= sub_fired(pr_cp[l], pr_sq[l]);"),
]


def read_rtl(path=RTL):
    return io.open(path, encoding="utf-8", newline="").read()


def mutate(gold, old, new):
    """Return the mutated text, or raise if the anchor is not unique."""
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
# sweep. NOTHING IS DECLARED PREDICTIVELY.
EQUIVALENT = {
    "R02": (
        "THE EARLY CAPTURE IS OVERWRITTEN BEFORE ANYTHING READS IT, and the "
        "proof is the walk order rather than an argument about timing. With "
        "`>= 4'd1` the loop runs one extra iteration at trig_k == 1, whose "
        "index is 2'((4'd1 - 4'd2) >> 1) -- the subtraction wraps to 4'd15, "
        "the shift gives 7, and the truncation gives 3. So the extra write is "
        "h_s[3], with whatever the table happened to be presenting. "
        "h_s[3] is written AGAIN at trig_k == 9, by the correct capture for "
        "issue 7, which is point 3's sine. Every other index is untouched by "
        "the extra iteration. Modelled over the whole walk for both "
        "thresholds: the final h_c and h_s are IDENTICAL, index for index. "
        "Nothing reads h_c or h_s before R_CP, which cannot be entered until "
        "trig_k reaches 9, so the stale value is never observable either. "
        "R24 IS THE CATCHABLE FORM of the same claim -- it moves the INDEX by "
        "one instead of the threshold, filing each answer under the wrong "
        "issue, which no later write repairs. "
        "RE-SCORE THIS IF THE WALK LENGTH CHANGES, or if anything reads h_c "
        "or h_s before the drain completes: both would break the overwrite "
        "that makes it equivalent."
    ),
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
