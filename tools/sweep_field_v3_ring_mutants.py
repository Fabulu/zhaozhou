#!/usr/bin/env python3
"""The mutant table for zhao_field_v3_ring.sv (FIELD.V3.RING).

WHAT THIS BLOCK CLAIMS
----------------------
UOP_RING_PREP: nine separately-rounded products and NO reciprocal, because the
planner computes both smoothstep reciprocals once in the prep whenever the
radii are uniform. So the claims are about the CHAIN rather than about
arithmetic hardware:

  * nine products, in the reference's order, each ROUNDED SEPARATELY;
  * the clamp is part of t -- immediately after its product and BEFORE the
    square -- and clamps to [0, 1];
  * d is per point; r0, m, rA and rB are shared;
  * the subtractions saturate, and their reporting is separate from the
    products';
  * every issue holds until granted.

A CHAIN IS THE EASIEST THING TO GET SUBTLY WRONG. Nine products feeding each
other means a swapped pair, a dropped clamp or a reused operand all produce a
number that is in range, smooth, and wrong -- which is exactly the shape of
failure that survives a test checking only that something came out.

PREFER MUTATING A VALUE OVER DELETING A USE -- see
sweep_field_v3_dispatch_mutants.py for the rule and the three refusals it came
from.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/field/zhao_field_v3_ring.sv"

MUTANTS = [
    # ---- the first smoothstep ----------------------------------------------
    ("G01 t0 is formed against m rather than r0",
     "          mul_a[l] = 33'(sub_sat(h_d[l], h_r0));\n"
     "          mul_b[l] = 33'(h_rA);",
     "          mul_a[l] = 33'(sub_sat(h_d[l], h_m));\n"
     "          mul_b[l] = 33'(h_rA);"),
    ("G02 the first smoothstep uses the SECOND reciprocal",
     "          mul_a[l] = 33'(sub_sat(h_d[l], h_r0));\n"
     "          mul_b[l] = 33'(h_rA);",
     "          mul_a[l] = 33'(sub_sat(h_d[l], h_r0));\n"
     "          mul_b[l] = 33'(h_rB);"),
    ("G03 the subtraction runs the wrong way round",
     "          mul_a[l] = 33'(sub_sat(h_d[l], h_r0));\n"
     "          mul_b[l] = 33'(h_rA);",
     "          mul_a[l] = 33'(sub_sat(h_r0, h_d[l]));\n"
     "          mul_b[l] = 33'(h_rA);"),
    ("G04 t0 is squared against t1 instead of itself",
     "        G_P2: begin\n"
     "          mul_a[l] = 33'(t0[l]);\n"
     "          mul_b[l] = 33'(t0[l]);\n"
     "        end",
     "        G_P2: begin\n"
     "          mul_a[l] = 33'(t0[l]);\n"
     "          mul_b[l] = 33'(t1[l]);\n"
     "        end"),
    ("G05 the Horner constant is 2 where the law says 3",
     "          mul_b[l] = 33'(sub_sat(FX_THREE, u0[l]));",
     "          mul_b[l] = 33'(sub_sat(FX_TWO, u0[l]));"),
    ("G06 the doubling constant is three",
     "        G_P3: begin\n"
     "          mul_a[l] = 33'(FX_TWO);\n"
     "          mul_b[l] = 33'(t0[l]);\n"
     "        end",
     "        G_P3: begin\n"
     "          mul_a[l] = 33'(FX_THREE);\n"
     "          mul_b[l] = 33'(t0[l]);\n"
     "        end"),

    # ---- the clamp ----------------------------------------------------------
    # Law: t is clamped to [0, 1] straight after its product and BEFORE the
    # square. Each rail on its own, because a test that only goes one way past
    # the ring would not notice the other.
    ("G07 the clamp's lower rail is dropped",
     "      if (v < 32'sd0) clamp01 = 32'sd0;",
     "      if (v < -FX_ONE) clamp01 = -FX_ONE;"),
    ("G08 the clamp's upper rail is twice as high",
     "      else if (v > FX_ONE) clamp01 = FX_ONE;",
     "      else if (v > FX_TWO) clamp01 = FX_TWO;"),
    ("G09 t1 is not clamped at all, only t0",
     "                G_P5 + 5'd1: t1[l]  <= clamp01(resc16(prod[l]));",
     "                G_P5 + 5'd1: t1[l]  <= resc16(prod[l]);"),
    ("G10 the clamp is applied to the SQUARE rather than to t",
     "                G_P2 + 5'd1: t0s[l] <= resc16(prod[l]);",
     "                G_P2 + 5'd1: t0s[l] <= clamp01(resc16(prod[l]));"),

    # ---- the second smoothstep and the finish -------------------------------
    ("G11 the second smoothstep starts from r0 rather than m",
     "          mul_a[l] = 33'(sub_sat(h_d[l], h_m));\n"
     "          mul_b[l] = 33'(h_rB);",
     "          mul_a[l] = 33'(sub_sat(h_d[l], h_r0));\n"
     "          mul_b[l] = 33'(h_rB);"),
    ("G12 the finish uses s1 where the law says ONE MINUS s1",
     "          mul_a[l] = 33'(s0[l]);\n"
     "          mul_b[l] = 33'(sub_sat(FX_ONE, s1[l]));",
     "          mul_a[l] = 33'(s0[l]);\n"
     "          mul_b[l] = 33'(s1[l]);"),
    ("G13 the finish multiplies the two rising halves",
     "          mul_a[l] = 33'(s0[l]);\n"
     "          mul_b[l] = 33'(sub_sat(FX_ONE, s1[l]));",
     "          mul_a[l] = 33'(s1[l]);\n"
     "          mul_b[l] = 33'(sub_sat(FX_ONE, s0[l]));"),
    ("G14 the second Horner uses the FIRST smoothstep's doubled term",
     "          mul_b[l] = 33'(sub_sat(FX_THREE, u1[l]));",
     "          mul_b[l] = 33'(sub_sat(FX_THREE, u0[l]));"),

    # ---- the rounding -------------------------------------------------------
    ("G15 the rescale truncates instead of rounding half up",
     "      r = (65'(v) + 65'sd32768) >>> 16;\n"
     "      if (r > 65'sd2147483647) resc16 = 32'sh7FFF_FFFF;",
     "      r = (65'(v)) >>> 16;\n"
     "      if (r > 65'sd2147483647) resc16 = 32'sh7FFF_FFFF;"),
    ("G16 the rescale shifts by the wrong amount",
     "      r = (65'(v) + 65'sd32768) >>> 16;\n"
     "      if (r > 65'sd2147483647) resc16 = 32'sh7FFF_FFFF;",
     "      r = (65'(v) + 65'sd32768) >>> 17;\n"
     "      if (r > 65'sd2147483647) resc16 = 32'sh7FFF_FFFF;"),

    # ---- per point vs shared ------------------------------------------------
    ("G17 every point uses point 0's distance",
     "            h_d[1] <= d_1_i;",
     "            h_d[1] <= d_0_i;"),
    ("G18 the answer is written from the wrong lane's product",
     "              o0_1_o <= resc16(prod[1]);",
     "              o0_1_o <= resc16(prod[0]);"),

    # ---- the refusal loop and the walk --------------------------------------
    ("G19 an issue advances without a grant",
     "          if (is_issue_c) begin\n"
     "            if (mul_ready_i) state <= state + 5'd1;",
     "          if (is_issue_c) begin\n"
     "            if (1'b1) state <= state + 5'd1;"),
    ("G20 a product is consumed without waiting for it",
     "          end else if (mul_valid_i) begin",
     "          end else if (1'b1) begin"),
    ("G21 the walk skips a step, so one product is never issued",
     "            state <= (state == G_P9 + 5'd1) ? G_OUT : (state + 5'd1);",
     "            state <= (state == G_P9 + 5'd1) ? G_OUT : (state + 5'd2);"),

    # ---- the flags ----------------------------------------------------------
    ("G22 a saturating subtraction is not reported",
     "            G_P9: if (sub_fired(FX_ONE, s1[l]))        fired_add[l] <= 1'b1;",
     "            G_P9: if (1'b0)                            fired_add[l] <= 1'b1;"),
    ("G23 the multiply flag is reported on the mirrored lane",
     "              if (resc16_fired(prod[l])) fired_mul[l] <= 1'b1;",
     "              if (resc16_fired(prod[3 - l])) fired_mul[l] <= 1'b1;"),
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
