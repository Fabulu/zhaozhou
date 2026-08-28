#!/usr/bin/env python3
"""The mutant table for zhao_field_v3_noise.sv (FIELD.V3.NOISE).

WHAT THIS BLOCK CLAIMS, AND WHY A SCALAR TEST SUITE WOULD NOT CHECK IT
----------------------------------------------------------------------
The v2 noise unit is scalar and was swept as such. This one runs FOUR POINTS
through the same six hash steps on a four-wide bank, and everything that is
new about it is per-point:

  * each point's own coordinates drive its own bank lane;
  * each point chooses its OWN RXS shift, because the shift is (s>>28)+4 and
    therefore a function of the data;
  * each point's saturation flags are its own.

A vector unit that broadcast point 0, or computed one shift for the whole
request, would pass every value check written for the scalar unit. N01, N05,
N07 and N09 attack exactly that, and the differential's sections 2, 3, 4 and 5
exist to make them reachable.

THE OTHER NEW CLAIM IS REFUSAL. The bank is shared and can say no, so every
issue state holds until granted. The v2 unit does not do this and did not need
to -- zhao_field_seq keeps one instruction in flight, so every request was
granted. N20 and N21 attack the hold.

CRLF: the worktree is checked out with CRLF. Anchors are written with plain
"\\n" and translated to whatever the file actually uses.
"""

import io
import os
import sys

RTL = "fpga/rtl/field/zhao_field_v3_noise.sv"

MUTANTS = [
    # ---- the lattice mix, shared between the two hash lanes -----------------
    # The mix is REPLAYED from s_mix when lane 1 starts. Recomputing it would
    # cost two more bank requests; carrying on from the post-RXS word instead
    # is the cheap-looking mistake, and it produces a lane 1 that is a hash of
    # a hash rather than the same lattice point one salt further.
    #
    # Reshaped after the preflight refused the direct form: `s_reg[l] <=
    # s_reg[l]` removes the only READ of s_mix, orphaning it, and Verilator
    # will not build that. A mutant that cannot build is a discard, not
    # evidence -- the same refusal C16 earned in the curve table.
    #
    # Replaying the NEIGHBOUR's mix keeps the signal live and is a sharper
    # mutant than the one it replaces: it is wrong per point, so it needs a
    # group whose four points differ to be seen at all. Section 3's identical
    # points would pass it, which is exactly why section 2 exists.
    ("N01 lane 1 replays a neighbouring point's lattice mix",
     "              s_reg[l] <= s_mix[l];",
     "              s_reg[l] <= s_mix[(l + 1) % LANES];"),

    # ---- the lane salt ------------------------------------------------------
    ("N02 the lane salt is applied to lane 0 as well, so both lanes are lane 1",
     "          mul_a[l] = s_reg[l] + (lane ? 32'h0000_00E1 : 32'd0);",
     "          mul_a[l] = s_reg[l] + 32'h0000_00E1;"),
    ("N03 the lane salt is off by one",
     "          mul_a[l] = s_reg[l] + (lane ? 32'h0000_00E1 : 32'd0);",
     "          mul_a[l] = s_reg[l] + (lane ? 32'h0000_00E0 : 32'd0);"),

    # ---- the RXS shift, which is the vector-specific claim ------------------
    # Law 2: (s>>28)+4, a function of the DATA, so four points in one request
    # can each want a different shift.
    ("N04 the RXS shift loses its +4 bias",
     "      rxs_sh[l]  = 5'({s_reg[l][31:28]} + 4'd4);",
     "      rxs_sh[l]  = 5'({s_reg[l][31:28]} + 4'd3);"),
    ("N05 every point uses point 0's shift -- the vector-specific defect",
     "      rxs_sh[l]  = 5'({s_reg[l][31:28]} + 4'd4);",
     "      rxs_sh[l]  = 5'({s_reg[0][31:28]} + 4'd4);"),
    ("N06 the shift is taken from the wrong nibble",
     "      rxs_sh[l]  = 5'({s_reg[l][31:28]} + 4'd4);",
     "      rxs_sh[l]  = 5'({s_reg[l][27:24]} + 4'd4);"),

    # ---- the bank interface -------------------------------------------------
    # Law 3: the products are modulo 2^32 and never saturate, while the lane is
    # 33x33 SIGNED. A sign-extended operand is right for half of all inputs and
    # wrong for the other half, which reads as a bad seed rather than a bad
    # multiply. Lane 0 only, so it is also a lane-independence check.
    ("N07 point 0's operand is sign-extended instead of zero-extended",
     "  assign mul_a_0_o = $signed({1'b0, mul_a[0]});",
     "  assign mul_a_0_o = $signed({mul_a[0][31], mul_a[0]});"),
    ("N08 point 0 reads the wrong half of its product",
     "  assign mul_p[0] = mul_p_0_i[31:0];",
     "  assign mul_p[0] = mul_p_0_i[63:32];"),
    ("N09 point 0's x is broadcast to every lane",
     "          mul_a[l] = ix[l];",
     "          mul_a[l] = ix[0];"),

    # ---- the hash tail ------------------------------------------------------
    # N10 SURVIVED and is equivalent -- see EQUIVALENT below for the proof and
    # the exact boundary. N22 is the same claim moved to the other side of that
    # boundary, so the tail is not left untested just because one shift value
    # happens to be unobservable.
    ("N10 the final xor-shift shifts by the wrong amount",
     "      lane1_h[l] = (s_reg[l] >> 22) ^ s_reg[l];",
     "      lane1_h[l] = (s_reg[l] >> 21) ^ s_reg[l];"),
    ("N22 the final xor-shift reaches into the bits the op actually reads",
     "      lane1_h[l] = (s_reg[l] >> 22) ^ s_reg[l];",
     "      lane1_h[l] = (s_reg[l] >> 15) ^ s_reg[l];"),
    ("N11 the output takes the low half of the hash instead of the top",
     "    for (int l = 0; l < LANES; l++) u_val[l] = $signed({16'd0, lane0[l][31:16]});",
     "    for (int l = 0; l < LANES; l++) u_val[l] = $signed({16'd0, lane0[l][15:0]});"),
    ("N12 NOISE2's second output repeats the first",
     "            o1_0_o <= $signed({16'd0, lane1_h[0][31:16]});",
     "            o1_0_o <= $signed({16'd0, lane0[0][31:16]});"),
    ("N13 the LCG addend is subtracted rather than added",
     "            for (int l = 0; l < LANES; l++) s_reg[l] <= mul_p[l] + C_LCG_A;",
     "            for (int l = 0; l < LANES; l++) s_reg[l] <= mul_p[l] - C_LCG_A;"),
    # The seed is folded into the Y term. XOR is associative so the grouping is
    # readability; ADDING it is not, and it is the plausible slip.
    ("N14 the seed is added into the mix rather than xored",
     "              s_mix[l] <= mix_x[l] ^ (mul_p[l] ^ h_seed);\n"
     "              s_reg[l] <= mix_x[l] ^ (mul_p[l] ^ h_seed);",
     "              s_mix[l] <= mix_x[l] ^ (mul_p[l] + h_seed);\n"
     "              s_reg[l] <= mix_x[l] ^ (mul_p[l] + h_seed);"),

    # ---- RIDGE --------------------------------------------------------------
    # RIDGE stops after lane 0. Making it walk lane 1 as well leaves it holding
    # the salted hash, which is a different number entirely.
    ("N15 RIDGE walks the second hash lane before folding",
     "          if (h_ridge) begin",
     "          if (h_ridge && (lane == 1'b1)) begin"),
    ("N16 RIDGE leaves its second output non-zero",
     "            o1_0_o <= 32'sd0;",
     "            o1_0_o <= u_val[0];"),
    ("N17 RIDGE's fold drops the doubling",
     "      ridge_t[l] = sub_sat(add_sat(u_val[l], u_val[l]), FX_ONE);",
     "      ridge_t[l] = sub_sat(u_val[l], FX_ONE);"),
    ("N18 RIDGE's fold omits the absolute value",
     "      ridge_r[l] = sub_sat(FX_ONE, abs_sat(ridge_t[l]));",
     "      ridge_r[l] = sub_sat(FX_ONE, ridge_t[l]);"),
    # N19 SURVIVED and is equivalent: BOTH rails are unreachable, so swapping
    # one constant-false condition for another changes nothing. The proof is
    # below. N23 is the catchable version -- a condition that IS reachable --
    # so "the flags are zero" stays an assertion with teeth rather than a
    # region nothing tests.
    ("N19 the rescale flag watches the wrong rail",
     "              sat_rescale_o[l] <= (ridge_t[l] == 32'sh8000_0000);",
     "              sat_rescale_o[l] <= (ridge_t[l] == 32'sh7FFF_FFFF);"),
    ("N23 RIDGE reports a saturation it did not have",
     "              sat_rescale_o[l] <= (ridge_t[l] == 32'sh8000_0000);",
     "              sat_rescale_o[l] <= (ridge_t[l] < 32'sd0);"),

    # ---- the refusal loop ---------------------------------------------------
    # mul_ready_i is read in four issue states, so dropping ONE of them leaves
    # the port live. That is deliberate: the same defect written across all
    # four would orphan the port and be refused by the preflight, exactly as
    # C16 was in the curve service's table.
    ("N20 the first issue advances on a REFUSED request",
     "        S_MIX_X: if (mul_ready_i) state <= S_MIX_XW;",
     "        S_MIX_X: state <= S_MIX_XW;"),
    ("N21 the LCG issue reads its grant inverted",
     "        S_LCG: if (mul_ready_i) state <= S_LCGW;",
     "        S_LCG: if (!mul_ready_i) state <= S_LCGW;"),
]


def read_rtl(path=RTL):
    return io.open(path, encoding="utf-8", newline="").read()


def mutate(gold, old, new):
    """Return the mutated text, or raise if the anchor is not unique.

    MIXED LINE ENDINGS ARE REAL AND THEY DEFEAT A SINGLE GUESS, so both forms
    are tried: a multi-line anchor matching under either is accepted, one
    matching under neither raises, and one matching under both is ambiguous
    and raises too.
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
# sweep. NOTHING IS DECLARED PREDICTIVELY -- that rule was broken twice on
# FIELD.V3.EXEC, with two proofs written before any evidence and both then
# contradicted by a run. Every entry below was written AFTER a run showed the
# mutant surviving.
EQUIVALENT = {
    "N07": (
        "THE EXTENSION CANNOT REACH THE BITS THIS UNIT READS, and the mutant "
        "proved my own comment wrong. For any 32-bit a, the zero-extended "
        "33-bit value is A0 = a and the sign-extended one is A1 = a - "
        "2^32*(a>>31), so A1 = A0 or A1 = A0 - 2^32. Their products with the "
        "same B therefore differ by 2^32*B -- an exact multiple of 2^32. The "
        "unit reads mul_p_*_i[31:0] and NOTHING ELSE, and residues mod 2^32 "
        "are unchanged by adding a multiple of 2^32. So the two forms are "
        "bit-identical on every bit this block can observe, for every input. "
        "No stimulus can distinguish them, and section 5 of the differential "
        "-- written to catch exactly this -- was aimed at a divergence that "
        "does not exist. "
        "THE ZERO-EXTENSION IS KEPT ANYWAY, as defence in depth: it states "
        "that these are unsigned hash words, and it is the form that stays "
        "correct if anything ever reads above bit 31. "
        "RE-SCORE THIS THE MOMENT ANY BIT ABOVE 31 OF A PRODUCT IS READ -- by "
        "this unit or by a bank change that makes the lane narrower than 33 "
        "bits, where the operand would be truncated rather than extended and "
        "the two forms would separate."
    ),
    "N10": (
        "THE XOR-SHIFT TAIL CANNOT REACH THE BITS THE OP READS. The hash "
        "finishes as (w >> 22) ^ w, and both NOISE2 and RIDGE take bits "
        "[31:16] of that -- the reference does the same, `>> 16`. A right "
        "shift by S moves bit 31 down to bit 31-S, so the xor term can only "
        "alter bits 31-S and below. For any S >= 16 that is bits 15 and "
        "below, which this op never reads. Shift 22 and shift 21 are "
        "therefore bit-identical on every observable output, for every input. "
        "MEASURED rather than only argued: 200,000 random words, zero "
        "differences in bits [31:16] between shift 22 and 21; and sweeping S "
        "from 10 to 19 over 2,000 words each, S <= 15 differs on most words "
        "while S >= 16 differs on none. The boundary is exactly 16. "
        "N22 IS THE SAME CLAIM AT S = 15, on the observable side of that "
        "boundary, so the tail is still tested -- an equivalence must not "
        "become an excuse to leave a step unchecked. "
        "THE SHIFT STAYS 22 because it is the reference's law and "
        "zref::noise2_hash is used elsewhere -- creature gib velocities, the "
        "star bake -- where the low bits ARE read. The equivalence is a "
        "property of THESE TWO OPS, not of the hash. "
        "RE-SCORE THIS IF ANY OP EVER READS BELOW BIT 16 OF THE HASH."
    ),
    "N19": (
        "RIDGE CANNOT SATURATE, so both rails are unreachable and swapping "
        "one constant-false condition for another is a no-op. u is the TOP 16 "
        "BITS of a hash word, so its whole domain is 0..65535 and nothing "
        "else. EXHAUSTIVELY CHECKED over all 65,536 values: ridge_t = "
        "sub_sat(add_sat(u,u), 1<<16) lands in [-65536, 65534], hits "
        "INT32_MIN zero times and INT32_MAX zero times, and no add or sub in "
        "the fold saturates even once. So sat_rescale_o and sat_add_o are "
        "BOTH identically zero for every input this op can be given, and the "
        "reference agrees -- its SatLedger never bumps either, which is why "
        "the differential passes with both sides reading zero. "
        "THE LOGIC STAYS because it mirrors the reference's fold exactly, and "
        "the reference keeps its saturating forms for the same reason: they "
        "become live the moment the domain widens. It is documented as "
        "unreachable rather than removed, so it does not read as tested "
        "logic. "
        "N23 IS THE CATCHABLE COUNTERPART -- a rescale condition that IS "
        "reachable -- so the differential's 'the flags are zero' assertion is "
        "proven to have teeth. "
        "RE-SCORE THIS IF u EVER BECOMES WIDER THAN 16 BITS: a change to the "
        "`>> 16` in the op's definition, or a fold that scales u before "
        "doubling it, puts the rails back in range."
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
