#!/usr/bin/env python3
"""The mutant table for zhao_field_v3_normalize.sv (FIELD.V3.NORMALIZE).

THE FOUR DEFECTS THIS BLOCK ALREADY HAD, ALL KEPT AS MUTANTS
-------------------------------------------------------------
This unit was wrong four ways before the differential got through it, and each
one is here so the check that found it cannot be weakened later without the
sweep noticing:

    M01  the final rescale-by-7 of rcp_u24_norm was missing -> a clean factor
         of 128 on every output, with every value still smooth and ordered
    M02  the Newton iterate was 31 bits where the reference uses 32 -- it
         REACHES 2^31 for any length that is an exact power of two, and wrapped
         to zero. Lanes 1 and 2 were right, lanes 0 and 3 read zero.
    M03  the isqrt request was gated on the `zero` REGISTER, one clock late, so
         the isqrt accepted n2 == 0 and its unconsumed answer hung every later
         lane
    M04  the isqrt's `ready` was ignored, so the same n2 could be re-accepted
         on the clock its answer was taken

WHAT ELSE THIS BLOCK CLAIMS
---------------------------
  * n2 is EXACT, summed at full width, nothing rescaled until the end;
  * the zero vector is a DEFINED answer, and the two ops disagree about the
    LEDGER -- normalize2 bumps RCP0, normalize3_approx bumps nothing;
  * e is PER POINT;
  * ONE rescale per component, by 31 + e.

PREFER MUTATING A VALUE OVER DELETING A USE, and an anchor must SPAN every
place a mutation has to change.

FOUR OF THESE MUTANTS WERE REFUSED BY THE PREFLIGHT ON THEIR FIRST RUN, which
takes the running total of orphan refusals across this family to TWELVE. M04
dropped isq_ready, M14 dropped newt_step, M18 dropped bit 32 of an internal by
truncating instead of clamping, and M21 dropped expo. Each is now a value
change, each is annotated with what it replaced and why, and two of them came
out SHARPER for it -- M14 now produces a wrong reciprocal instead of a hang,
and M18 now asks whether the clamp is at the right VALUE rather than merely
present.

The refusals cost nothing: the preflight runs before anything is scored, so
the sweep stopped at exit 8 with zero mutants scored and the tree clean. That
is the whole reason the preflight exists -- an unbuildable mutant scored as
"caught" is the most flattering possible way to be wrong.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/field/zhao_field_v3_normalize.sv"

MUTANTS = [
    # ---- the four defects it had -------------------------------------------
    ("M01 the reciprocal's final rescale-by-7 is dropped",
     "      r = (33'(x) + 33'd64) >> 7;",
     "      r = (33'(x) + 33'd64) >> 6;"),
    ("M02 the Newton iterate loses its top bit",
     "      resc_u30 = 32'(((u + (66'd1 << 29)) >> 30) & 66'hFFFF_FFFF);",
     "      resc_u30 = 32'(((u + (66'd1 << 29)) >> 30) & 66'h7FFF_FFFF);"),
    ("M03 the isqrt request is gated on the ZERO REGISTER, one clock late",
     "  assign isq_valid  = (state == S_ROOT) && (n2[root_lane] != 64'd0) && !root_busy;",
     "  assign isq_valid  = (state == S_ROOT) && !zero[root_lane] && !root_busy;"),
    # Reshaped: dropping isq_ready ORPHANS it -- it is read in exactly one
    # place. Requiring only ONE of the two is the same claim (the grant is not
    # what gates acceptance) and keeps the signal live.
    ("M04 the isqrt's grant is not REQUIRED, only welcomed",
     "          end else if (isq_valid && isq_ready) begin",
     "          end else if (isq_valid || isq_ready) begin"),

    # ---- the sum of squares -------------------------------------------------
    ("M05 the second square OVERWRITES the first rather than summing",
     "          for (int l = 0; l < LANES; l++) n2[l] <= n2[l] + 64'(prod[l]);\n"
     "          state <= h_n3 ? S_SQ2 : S_ROOT;",
     "          for (int l = 0; l < LANES; l++) n2[l] <= 64'(prod[l]);\n"
     "          state <= h_n3 ? S_SQ2 : S_ROOT;"),
    ("M06 NORMALIZE2 sums the third square too",
     "          state <= h_n3 ? S_SQ2 : S_ROOT;",
     "          state <= S_SQ2;"),
    ("M07 the first square is taken from the second component",
     "        S_SQ0: begin\n"
     "          mul_a[l] = 33'(h_a[0][l]);\n"
     "          mul_b[l] = 33'(h_a[0][l]);\n"
     "        end",
     "        S_SQ0: begin\n"
     "          mul_a[l] = 33'(h_a[1][l]);\n"
     "          mul_b[l] = 33'(h_a[1][l]);\n"
     "        end"),
    # n2 is EXACT. Rescaling it is the plausible-looking economy and changes
    # the length, and therefore every output.
    ("M08 the sum of squares is rounded rather than kept exact",
     "          for (int l = 0; l < LANES; l++) n2[l] <= 64'(prod[l]);\n"
     "          state <= S_SQ1;",
     "          for (int l = 0; l < LANES; l++) n2[l] <= 64'(prod[l]) >> 1;\n"
     "          state <= S_SQ1;"),

    # ---- the normalisation --------------------------------------------------
    ("M09 the mantissa is shifted the wrong way",
     "            mant[root_lane] <= 24'((top_bit(isq_r) >= 6'd23)\n"
     "                                   ? (isq_r >> (top_bit(isq_r) - 6'd23))\n"
     "                                   : (isq_r << (6'd23 - top_bit(isq_r))));",
     "            mant[root_lane] <= 24'((top_bit(isq_r) >= 6'd23)\n"
     "                                   ? (isq_r << (top_bit(isq_r) - 6'd23))\n"
     "                                   : (isq_r >> (6'd23 - top_bit(isq_r))));"),
    ("M10 the exponent is off by one binade",
     "            expo[root_lane] <= 8'sd0 + 8'(top_bit(isq_r)) - 8'sd23;",
     "            expo[root_lane] <= 8'sd0 + 8'(top_bit(isq_r)) - 8'sd24;"),
    ("M11 the exponent's sign is inverted",
     "            expo[root_lane] <= 8'sd0 + 8'(top_bit(isq_r)) - 8'sd23;",
     "            expo[root_lane] <= 8'sd23 - 8'(top_bit(isq_r));"),
    ("M12 top_bit reports the LOWEST set bit",
     "      for (int b = 0; b < 64; b++) if (v[b]) p = 6'(b);",
     "      for (int b = 63; b >= 0; b--) if (v[b]) p = 6'(b);"),
    ("M13 the seed index takes the wrong slice of the mantissa",
     "    assign seed_idx[g] = mant[g][22:15];",
     "    assign seed_idx[g] = mant[g][23:16];"),

    # ---- the Newton steps ---------------------------------------------------
    # Reshaped, and the reshape is SHARPER than what it replaces. `if (1'b0)`
    # orphans newt_step, and it also makes the machine take the SECOND branch
    # first -- which is a different defect from the one the name claims.
    # Finishing in the first branch is literally "only one Newton step": the
    # flag stays live, the second branch becomes unreachable, and the result is
    # a WRONG RECIPROCAL rather than a hang. A wrong number is a better mutant
    # than a timeout, because a timeout is caught by any guard at all.
    ("M14 only ONE Newton step is taken",
     "          if (newt_step == 1'b0) begin\n"
     "            for (int l = 0; l < LANES; l++) rx[l] <= resc_u30(prod[l]);\n"
     "            newt_step <= 1'b1;\n"
     "            state <= S_NEWT_A;",
     "          if (newt_step == 1'b0) begin\n"
     "            for (int l = 0; l < LANES; l++) rx[l] <= rcp_finish(resc_u30(prod[l]));\n"
     "            newt_step <= 1'b1;\n"
     "            state <= S_OUT0;"),
    ("M15 the correction term is 2^30 rather than 2^31",
     "          mul_b[l] = $signed({1'b0, 32'h8000_0000 - newt_w[l]});",
     "          mul_b[l] = $signed({1'b0, 32'h4000_0000 - newt_w[l]});"),
    ("M16 the Newton product is shifted by the wrong amount",
     "      newt_w[l] = 32'((65'(prod[l]) >> 24) & 65'h7FFF_FFFF);",
     "      newt_w[l] = 32'((65'(prod[l]) >> 23) & 65'h7FFF_FFFF);"),
    ("M17 the first Newton multiply squares the mantissa",
     "          mul_a[l] = $signed({9'd0, mant[l]});\n"
     "          mul_b[l] = $signed({1'b0, rx[l]});",
     "          mul_a[l] = $signed({9'd0, mant[l]});\n"
     "          mul_b[l] = $signed({9'd0, mant[l]});"),
    # Reshaped: removing the clamp leaves `32'(r)`, which TRUNCATES, so bit 32
    # of r goes unread and Verilator refuses the file. Moving the rail one
    # binade keeps the 33-bit comparison -- and therefore bit 32 -- alive, and
    # asks the sharper question anyway: not "is there a clamp" but "is it at
    # the right value".
    # SURVIVED its first run, and the reason is the "same answer, wrong reason"
    # pattern for the third time tonight: section 1 ALREADY drove an input that
    # reaches the rail -- the rail is hit whenever the length is an exact power
    # of two -- but the two clamps differ by 1 in a u24 reciprocal, which is at
    # most 1/256 of an output LSB, so it shows only where it straddles a
    # rounding boundary. Section 4b solves for four components that do.
    ("M18 the reciprocal is clamped to u25 rather than u24",
     "      rcp_finish = (r > 33'h0_00FF_FFFF) ? 32'h00FF_FFFF : 32'(r);",
     "      rcp_finish = (r > 33'h0_01FF_FFFF) ? 32'h01FF_FFFF : 32'(r);"),

    # ---- the zero vector and its ledger ------------------------------------
    ("M19 the zero vector is reported by BOTH ops, not just NORMALIZE2",
     "            rcp0_o[root_lane] <= !h_n3;",
     "            rcp0_o[root_lane] <= 1'b1;"),
    ("M20 the zero vector's outputs are not forced to zero",
     "          o0_0_o <= zero[0] ? 32'sd0 : resc_var(prod[0], out_k[0]);",
     "          o0_0_o <= resc_var(prod[0], out_k[0]);"),

    # ---- the output scaling -------------------------------------------------
    # Reshaped, and NOT to the obvious replacement. Tying out_k to 7'd31
    # orphans expo. The obvious repair -- invert its sign -- is worse than
    # useless: expo is READ IN THIS ONE PLACE, so negating it here and negating
    # it at its source (M11) are THE SAME MUTANT, and the table would carry a
    # duplicate wearing two names. The shift CONSTANT is the one claim in this
    # line not already made elsewhere.
    #
    # "The exponent is ignored entirely" stays covered: out_k would be 31 for
    # every lane, which differs from 31 + e for exactly the inputs where M11
    # differs, so whatever check catches M11 catches that too.
    ("M21 the output shift is 30 rather than 31",
     "    for (int l = 0; l < LANES; l++) out_k[l] = 7'(31 + expo[l]);",
     "    for (int l = 0; l < LANES; l++) out_k[l] = 7'(30 + expo[l]);"),
    ("M22 every lane uses lane 0's exponent",
     "    for (int l = 0; l < LANES; l++) out_k[l] = 7'(31 + expo[l]);",
     "    for (int l = 0; l < LANES; l++) out_k[l] = 7'(31 + expo[0]);"),
    ("M23 the output rescale truncates instead of rounding half up",
     "      r = (67'(v) + (67'sd1 <<< (k - 7'd1))) >>> k;\n"
     "      if (r > 67'sd2147483647) resc_var = 32'sh7FFF_FFFF;",
     "      r = (67'(v)) >>> k;\n"
     "      if (r > 67'sd2147483647) resc_var = 32'sh7FFF_FFFF;"),
    ("M24 NORMALIZE2 writes a third lane",
     "            if (!h_n3) begin\n"
     "              o2_0_o <= 32'sd0;",
     "            if (1'b0) begin\n"
     "              o2_0_o <= 32'sd0;"),

    # ---- the refusal loop ---------------------------------------------------
    ("M25 the first square advances on a REFUSED request",
     "        S_SQ0: if (mul_ready_i) state <= S_SQ0W;",
     "        S_SQ0: state <= S_SQ0W;"),
    ("M26 the first output issue reads its grant inverted",
     "        S_OUT0: if (mul_ready_i) state <= S_OUT0W;",
     "        S_OUT0: if (!mul_ready_i) state <= S_OUT0W;"),
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
    "M20": (
        "THE GUARD IS REDUNDANT FOR THE VALUE, and exhaustively so rather than "
        "on the tested inputs. n2 is `logic [63:0]`, UNSIGNED, and three "
        "squares of 32-bit components sum to at most 3*2^62, which is below "
        "2^64 -- so the sum cannot wrap and n2 == 0 holds IF AND ONLY IF every "
        "component is zero. The zero branch then sets rx and expo to 0 "
        "explicitly, so the output multiply forms 0 * 0 = 0, and out_k is "
        "31 + 0 = 31. resc_var(0, k) = (0 + 2^(k-1)) >> k = 0 for every k >= 1, "
        "and k here ranges over 8..39. The forced zero and the computed value "
        "are therefore THE SAME NUMBER on every input that can reach this "
        "line, not merely on the ones the suite drives. "
        "THE GUARD STAYS. It is not dead code being tolerated: it states the "
        "op's law at the point the law applies, and M19 -- the LEDGER half of "
        "the same law, which is NOT redundant, because normalize2 bumps RCP0 "
        "and normalize3_approx bumps nothing -- is caught. "
        "RE-SCORE THIS IF n2 EVER NARROWS, IF IT BECOMES SIGNED, OR IF THE "
        "ZERO BRANCH STOPS ZEROING rx: each of the three breaks a different "
        "step of the argument above."
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
