#!/usr/bin/env python3
"""The mutant table for zhao_field_v3_spline.sv (FIELD.V3.SPLINE).

THE DEFECT THIS BLOCK ALREADY HAD
---------------------------------
The coefficients were formed in the same state that ISSUES t*C3, and a
non-blocking assignment lands a clock later -- so every multiply went out
against the PREVIOUS group's C3.

Every value was wrong and every value was still a plausible spline, because a
stale coefficient is a real coefficient FOR A DIFFERENT SEGMENT. Nothing was
out of range, nothing saturated, no flag fired. S01 is that defect, kept so the
check that found it cannot be weakened without the sweep noticing.

WHAT ELSE THIS BLOCK CLAIMS
---------------------------
  * the three coefficients, with their small multiples EXACT at 64 bits and
    only the result clamped;
  * fx_mad is ONE rounding -- the opposite of ROT, deliberately;
  * the Horner runs C3 then C1, in that order, and the final product is t*u;
  * the finish is a RESCALE by one onto p1, not a shift;
  * every issue holds until granted.

PREFER MUTATING A VALUE OVER DELETING A USE, and an anchor must SPAN every
place a mutation has to change -- see sweep_field_v3_dispatch_mutants.py and
sweep_field_v3_ring_mutants.py for the six refusals those two rules came from.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/field/zhao_field_v3_spline.sv"

MUTANTS = [
    # ---- the timing defect this block had ----------------------------------
    ("S01 the coefficients are formed on the clock they are USED",
     "            state <= P_H1;\n"
     "          end\n"
     "        end\n"
     "\n"
     "        P_H1: if (mul_ready_i) state <= P_H1W;",
     "            state <= P_H1W;\n"
     "          end\n"
     "        end\n"
     "\n"
     "        P_H1: if (mul_ready_i) state <= P_H1W;"),

    # ---- the coefficients ---------------------------------------------------
    ("S02 C1 is p0 - p2 rather than p2 - p0",
     "              c1[l] <= sat32(64'(h_p2[l]) - 64'(h_p0[l]));",
     "              c1[l] <= sat32(64'(h_p0[l]) - 64'(h_p2[l]));"),
    ("S03 C2's middle term has the wrong weight",
     "              c2[l] <= sat32(64'sd2 * 64'(h_p0[l]) - 64'sd5 * 64'(h_p1[l]) +",
     "              c2[l] <= sat32(64'sd2 * 64'(h_p0[l]) - 64'sd4 * 64'(h_p1[l]) +"),
    ("S04 C2's last term has the wrong weight",
     "                             64'sd4 * 64'(h_p2[l]) - 64'(h_p3[l]));",
     "                             64'sd5 * 64'(h_p2[l]) - 64'(h_p3[l]));"),
    ("S05 C3's sign is inverted",
     "              c3[l] <= sat32(-64'(h_p0[l]) + 64'sd3 * 64'(h_p1[l]) -",
     "              c3[l] <= sat32(64'(h_p0[l]) - 64'sd3 * 64'(h_p1[l]) +"),
    ("S06 C3's last term takes the wrong control point",
     "                             64'sd3 * 64'(h_p2[l]) + 64'(h_p3[l]));",
     "                             64'sd3 * 64'(h_p2[l]) + 64'(h_p2[l]));"),
    # The small multiples are EXACT at 64 bits and only the RESULT is clamped.
    # Clamping the OPERANDS instead is the plausible-looking version and gives
    # a smooth, wrong curve on large control points.
    ("S07 the coefficient is clamped before its terms are summed",
     "              c2[l] <= sat32(64'sd2 * 64'(h_p0[l]) - 64'sd5 * 64'(h_p1[l]) +\n"
     "                             64'sd4 * 64'(h_p2[l]) - 64'(h_p3[l]));",
     "              c2[l] <= sat32(64'(sat32(64'sd2 * 64'(h_p0[l]))) -\n"
     "                             64'(sat32(64'sd5 * 64'(h_p1[l]))) +\n"
     "                             64'sd4 * 64'(h_p2[l]) - 64'(h_p3[l]));"),

    # ---- the Horner ---------------------------------------------------------
    ("S08 the Horner's first step folds in C1 rather than C2",
     "            u[l] <= mad_fin(prod[l], c2[l]);",
     "            u[l] <= mad_fin(prod[l], c1[l]);"),
    ("S09 the Horner's second step folds in C2 rather than C1",
     "            u[l] <= mad_fin(prod[l], c1[l]);",
     "            u[l] <= mad_fin(prod[l], c2[l]);"),
    # Reshaped: pointing P_H1 at c2 ORPHANS c3, which is used in exactly one
    # place. Pointing the SECOND Horner step at c3 keeps both live and is the
    # same claim -- the Horner multiplies by the wrong thing.
    ("S10 the second Horner step multiplies by C3 rather than by u",
     "        P_H2:    mul_b[l] = 33'(u[l]);",
     "        P_H2:    mul_b[l] = 33'(c3[l]);"),
    # Reshaped: h_t is read in exactly one place, so replacing it orphans it.
    # BROADCASTING lane 0's parameter keeps it live and is the sharper defect
    # anyway -- it needs a group whose four points sit in different segments to
    # be visible at all, which is what section 2 provides.
    ("S11 every point uses point 0's segment parameter",
     "      mul_a[l] = 33'(h_t[l]);",
     "      mul_a[l] = 33'(h_t[0]);"),
    # fx_mad is ONE rounding: a*b + (c << 16) formed at full width and rescaled
    # once. Rescaling the product FIRST and adding after is two roundings, and
    # it is exactly the arrangement ROT uses -- so this mutant is the "same way
    # round for every op" mistake.
    ("S12 the mad rounds twice, ROT-style, instead of once",
     "      s = 67'(p) + (67'(c) <<< 16);\n"
     "      r = (s + 67'sd32768) >>> 16;\n"
     "      if (r > 67'sd2147483647) mad_fin = 32'sh7FFF_FFFF;",
     "      s = ((67'(p) + 67'sd32768) >>> 16) + 67'(c);\n"
     "      r = s;\n"
     "      if (r > 67'sd2147483647) mad_fin = 32'sh7FFF_FFFF;"),

    # ---- the finish ---------------------------------------------------------
    ("S13 the half is a SHIFT, which neither rounds nor saturates",
     "      r = (34'(v) + 34'sd1) >>> 1;",
     "      r = (34'(v)) >>> 1;"),
    ("S14 the finish adds p0 rather than p1",
     "          o0_0_o <= add_sat(h_p1[0], resc1(resc16(prod[0])));",
     "          o0_0_o <= add_sat(h_p0[0], resc1(resc16(prod[0])));"),
    ("S15 the half is applied twice",
     "          o0_1_o <= add_sat(h_p1[1], resc1(resc16(prod[1])));",
     "          o0_1_o <= add_sat(h_p1[1], resc1(resc1(resc16(prod[1]))));"),
    ("S16 the last product is t*t rather than t*u",
     "        default: mul_b[l] = 33'(u[l]);",
     "        default: mul_b[l] = 33'(h_t[l]);"),

    # ---- per point ----------------------------------------------------------
    ("S17 two points' parameters are exchanged",
     "            h_t[0] <= t_0_i;  h_t[1] <= t_1_i;",
     "            h_t[0] <= t_1_i;  h_t[1] <= t_0_i;"),
    ("S18 a lane's answer is written from another lane's product",
     "          o0_2_o <= add_sat(h_p1[2], resc1(resc16(prod[2])));",
     "          o0_2_o <= add_sat(h_p1[2], resc1(resc16(prod[3])));"),

    # ---- the refusal loop ---------------------------------------------------
    ("S19 the first issue advances on a REFUSED request",
     "        P_H1: if (mul_ready_i) state <= P_H1W;",
     "        P_H1: state <= P_H1W;"),
    ("S20 the last issue reads its grant inverted",
     "        P_V: if (mul_ready_i) state <= P_VW;",
     "        P_V: if (!mul_ready_i) state <= P_VW;"),

    # ---- the flags ----------------------------------------------------------
    ("S21 a coefficient saturation is reported on the wrong lane",
     "                fired_resc[l] <= 1'b1;",
     "                fired_resc[3 - l] <= 1'b1;"),
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
