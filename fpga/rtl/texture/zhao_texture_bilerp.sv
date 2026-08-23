// zhao_texture_bilerp.sv — one bilinear channel: the four texels of a 2×2
// footprint and the two unit8 sub-texel fractions → the filtered byte.
// Instantiated FILT_LANES× (1, 2 or 4) by zhao_texture_tmu, which
// time-multiplexes the four channels (R, G, B, A) through them.
//
// ---------------------------------------------------------------------------
// THIS ARITHMETIC IS A CHOICE, NOT A CITATION — and here is the derivation
// ---------------------------------------------------------------------------
// NO SPEC IN THIS REPOSITORY DEFINES BILINEAR WEIGHTS OR THEIR ROUNDING.
// Searched and found empty: spec/qformats.md (§2/§3/§4 define `unit8`,
// `unit_mul` and `rescale`, and §8 defines perspective UV, but no filter),
// spec/sky_and_beams.md §2 (says "Bilinear TMU mandatory" and nothing about
// how), spec/stars_and_flares.md §1 (says only where bilinear may NOT go),
// spec/terrain_rules.md §6.2 (the mirrored-repeat fold and the Mosaic pick —
// nearest), and the charter, which lists "bilinear filtered path" (§15) and
// "bilinear rounding" (§20.1, as a thing ZRef must be exact about) without
// stating either. So this module does not pretend to quote a law. It DERIVES
// one from the three that do exist, and the contract records the derivation
// so a future ratification amends one file:
//
//   1. spec/qformats.md §2 — a `unit8` is U 0.0.8 and its VALUE IS raw/256.
//      The sub-texel fractions `fu_i`/`fv_i` are unit8s, so `fu = 128` is
//      exactly half a texel and the complement of `fu` is `256 − fu`, which
//      needs 9 bits and REACHES 256. That is what makes the weights below sum
//      to exactly 65,536 with no correction term anywhere.
//
//   2. spec/qformats.md §3, the single-rounding law — "any multiply-then-add
//      instruction computes the EXACT wide-integer expression and rounds
//      EXACTLY ONCE via `rescale(·,k)` at the end. Double rounding is
//      rejected."
//
//   3. spec/qformats.md §4 — `rescale_u(x, k) = (x + (1 << (k−1))) >> k`,
//      round-half-up. With Q16 weights that is `(Σ + 32768) >> 16`.
//
// so THE LAW IS:
//
//     w00 = (256 − fu)·(256 − fv)      w10 = fu·(256 − fv)
//     w01 = (256 − fu)·fv              w11 = fu·fv          ⎫ Σw = 65,536
//     out = (t00·w00 + t10·w10 + t01·w01 + t11·w11 + 32768) >> 16
//
// The exact same Q16 shape is already shipping in this repository — the
// software raster's alpha blend is `(dst·ia + src·a + 32768) >> 16` with
// `ia = 65536 − a` (reference/src/zrender/rast.cpp) — so this is that frozen
// form widened from two taps to four, not a new numeric idea.
//
// ---------------------------------------------------------------------------
// HOW IT IS COMPUTED: THE FACTORED FORM. 8 products → 3, BIT-IDENTICAL.
// Rearchitected 2026-08-23 (RUN-20260823-1736). See WHY THIS IS NOT THE FORM
// §3 REFUSES, immediately below — that distinction is the whole argument.
// ---------------------------------------------------------------------------
// The law above is a sum of four products of a texel by a weight, and each
// weight is itself a product. Written literally that is EIGHT multiplies a
// channel, and the fit measured what that costs: 7 DSP blocks for this module
// alone and 28 for the TMU — 4 × 7 exactly, because the four instances' weight
// products are identical and were NOT shared (recorded in the contract as a
// prediction that failed). Factor the same integer expression instead:
//
//     A = t00·(256−fu) + t10·fu  =  (t00 << 8) + (t10 − t00)·fu    1 product
//     B = t01·(256−fu) + t11·fu  =  (t01 << 8) + (t11 − t01)·fu    1 product
//     S = A·(256−fv)   + B·fv    =  (A   << 8) + (B   − A  )·fv    1 product
//     out = (S + 32768) >> 16                                      1 rescale
//
// Expanding S gives
//     t00(256−fu)(256−fv) + t10·fu(256−fv) + t01(256−fu)fv + t11·fu·fv
// which is the law TERM FOR TERM. Three products a channel, not eight, and
// there are no weights left to duplicate — `w00` does not exist any more.
//
// WHY THIS IS NOT THE FORM spec/qformats.md §3 REFUSES. The single-rounding
// law rejects "two lerps then a lerp" because the textbook writes each lerp as
// a ROUNDED unit8 blend — three `rescale` calls, three roundings, bits lost at
// every stage. Nothing above is rounded. `A` and `B` are EXACT integers in a
// signed-18 lane (their true range is [0, 65,280]); `S` is the EXACT weighted
// sum in a signed-27 lane; and there is EXACTLY ONE `(S + 32768) >> 16`, at
// the end, on the whole sum. This is algebraic factoring of the wide-integer
// expression §3 demands, not staged rounding of it. The two forms are the same
// integer for every one of the 2^48 inputs this module can be handed, which is
// what tests/formal/texture_bilerp.sby's P1 proves — see below.
//
// THE HARNESS DID NOT HAVE TO MOVE — BUT THE PROOF NO LONGER CLOSES, AND THAT
// SECOND HALF IS THE ONE TO READ. This module's PORTS ARE UNCHANGED, so
// texture_bilerp_fv.sv — which derives the four weights ITSELF from free
// `fu_free`/`fv_free` — needed not one line changed, and its COVER task still
// passes in about a second. Its bmc task, which used to close in 741 s, ran
// 3,300 s on boolector WITHOUT AN ANSWER on 2026-08-23, so P1..P4 are currently
// UNPROVED on this filter.
//
// The reason is the same fact that makes the theorem worth having: the harness
// computes the law as `t00*w00 + ... + t11*w11`, and until the factoring the
// DUT computed THAT SAME EXPRESSION, so `a_exact` was nearly a SYNTACTIC
// identity. It is now a real distributive-law identity across three multiplies
// of three different widths, one feeding another — the hard case for the engine.
//
// WHAT STANDS IN ITS PLACE IS TOTAL, NOT SAMPLED. See the contract's
// "What stands in its place": (1) no lane truncates — proved by enumerating all
// 16 texel CORNERS (each intermediate is monotone in each texel, so corners
// bound the byte domain) against all 65,536 (fu, fv), 0 violations; and (2)
// with no truncation the pre-rounding sum is EXACTLY LINEAR in the texels, so
// the four basis vectors at every (fu, fv) determine the whole map — and they
// equal w00, w10, w01, w11 exactly, 0 mismatches. Together those settle the
// identity for every integer texel quadruple.
//
// This was still the right form to choose over hoisting the weights, which the
// contract had sanctioned: factoring removes 20 of the 32 products where
// hoisting removes 12, and it changes no interface. But "the proof follows the
// weights for free" was too strong, and it is corrected here rather than in a
// paragraph nobody re-reads.
//
// THE WIDTHS ARE THE HONEST ONES, and that is QUARTUS_GOTCHAS.md §5 applied
// rather than quoted. The form this replaced declared its texel products as
// `{17'd0, t00_i} * {8'd0, w00}` — a 25×25 multiply whose real need was 8×17.
// §5 measured that exact class of slack costing zhao_geom_lod ten DSP blocks.
// Here the operands are what the values are:
//     (t10 − t00) ∈ [−255, 255]        signed 9
//     fu, fv      ∈ [0, 255]           signed 9 (a unit8, sign bit clear)
//     (B − A)     ∈ [−65,280, 65,280]  signed 18
// so the three multiplies are 9×9, 9×9 and 18×9 — and no wider.
//
// THE ONE-LSB TRAPS, NAMED SO THE TESTS CAN AIM AT THEM
//   · TRUNCATE instead of round. `Σ >> 16` biases every filtered texel
//     downward by up to one LSB — invisible on a photo, fatal against a
//     bit-identical oracle. The tie `t = (0, 255), fu = 128, fv = 0` gives
//     Σ + 32768 = 8,388,608 exactly, so round gives 128 and truncate gives
//     127. That vector is pinned by name in the directed test.
//   · SWAPPED WEIGHTS. In this form the transpose appears as `t01`/`t10`
//     exchanged between the two U-lerps, which is invisible whenever fu == fv
//     and whenever the footprint is symmetric — most random vectors. The tests
//     use asymmetric fractions.
//   · A /255 SCALE. The scale is 256·256, not 255·255: `(t00 << 8)` and
//     `(A << 8)` are the two factors of 256, and the fractions are unit8s.
//     The same /256-vs-/255 distinction zhao_raster_blend argues at length —
//     this is a WEIGHTING, not a quantizer.
//
// THE ENDPOINTS, stated because they surprise people. `fu = fv = 0` gives
// A = t00<<8, B = t01<<8, S = t00<<16 and out = t00 EXACTLY — the filter is
// the identity at a texel's own sample point, which is what makes bilinear and
// nearest agree there (see the half-texel bias in zhao_texture_tmu) and what
// lets FILTER_NEAREST take THIS datapath rather than a parallel one. The
// identity survives the refactor unchanged; it is formal property P3.
// `fu = 255` is 255/256, NOT 1.0, so t10 is never weighted fully — the
// identical unit8 endpoint zhao_raster_blend documents for a = 255.
//
// Conservative SystemVerilog subset only (charter §2); no dependencies.
// Proved by tests/formal/texture_bilerp.sby.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_texture_tmu).

module zhao_texture_bilerp (
  input  logic [7:0] t00_i,  // texel (i,   j)
  input  logic [7:0] t10_i,  // texel (i+1, j)
  input  logic [7:0] t01_i,  // texel (i,   j+1)
  input  logic [7:0] t11_i,  // texel (i+1, j+1)
  input  logic [7:0] fu_i,   // unit8 sub-texel fraction in u (value = fu/256)
  input  logic [7:0] fv_i,   // unit8 sub-texel fraction in v
  output logic [7:0] out_o
);

  // ---- the two fractions as non-negative signed 9s -----------------------
  // A unit8 is 0..255, so the sign bit is always clear. Nine bits, not eight,
  // because the multiplicand beside it is a signed difference.
  logic signed [8:0] fu_s, fv_s;
  always_comb begin
    fu_s = $signed({1'b0, fu_i});
    fv_s = $signed({1'b0, fv_i});
  end

  // ---- the U lerps: A and B, EXACT, no rounding --------------------------
  // du0 = t10 − t00 and du1 = t11 − t01 are in [−255, 255]: signed 9.
  // The products are in [−65,025, 65,025] and the sums A, B in [0, 65,280].
  logic signed [8:0]  du0, du1;
  logic signed [17:0] pu0, pu1;   // 9×9
  logic signed [17:0] a_s, b_s;
  always_comb begin
    du0 = $signed({1'b0, t10_i}) - $signed({1'b0, t00_i});
    du1 = $signed({1'b0, t11_i}) - $signed({1'b0, t01_i});
    pu0 = du0 * fu_s;
    pu1 = du1 * fu_s;
    a_s = $signed({2'b00, t00_i, 8'd0}) + pu0;
    b_s = $signed({2'b00, t01_i, 8'd0}) + pu1;
  end

  // ---- the V lerp: ONE exact wide sum, ONE rescale -----------------------
  // dv = B − A is in [−65,280, 65,280]: signed 18, which is exactly the width
  // a Cyclone V variable-precision block's 18×19 mode wants.
  // pv = dv·fv is in ±16,646,400; S = (A<<8) + pv is the EXACT Σ t·w and lies
  // in [0, 16,711,680], so S + 32768 ≤ 16,744,448 and the shifted result
  // cannot exceed 255 — no clamp anywhere, because the weights are a partition
  // of unity and the inputs are bytes (formal P2 and P4).
  logic signed [17:0] dv;
  logic signed [26:0] pv, a_ext, s_w;
  always_comb begin
    dv    = b_s - a_s;
    pv    = 27'(dv * fv_s);
    a_ext = 27'(a_s);
    s_w   = (a_ext <<< 8) + pv;
    out_o = 8'((s_w + 27'sd32768) >>> 16);
  end

endmodule : zhao_texture_bilerp
