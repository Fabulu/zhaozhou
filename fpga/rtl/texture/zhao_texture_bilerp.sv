// zhao_texture_bilerp.sv — one bilinear channel: the four texels of a 2×2
// footprint and the two unit8 sub-texel fractions → the filtered byte.
// Instantiated 4× (red, green, blue, alpha) by zhao_texture_tmu.
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
//      rejected." A bilinear filter is a multiply-then-add of four products,
//      so the two-lerps-then-a-lerp formulation every textbook writes (three
//      roundings) is REFUSED here by that law. One product sum, one rescale.
//
//   3. spec/qformats.md §4 — `rescale_u(x, k) = (x + (1 << (k−1))) >> k`,
//      round-half-up. With Q16 weights that is `(Σ + 32768) >> 16`.
//
// so:
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
// THE ONE-LSB TRAPS, NAMED SO THE TESTS CAN AIM AT THEM
//   · TRUNCATE instead of round. `Σ >> 16` biases every filtered texel
//     downward by up to one LSB — invisible on a photo, fatal against a
//     bit-identical oracle. The tie `t = (0, 255), fu = 128, fv = 0` gives
//     Σ + 32768 = 8,388,608 exactly, so round gives 128 and truncate gives
//     127. That vector is pinned by name in the directed test.
//   · SWAPPED WEIGHTS. `w10` paired with `t01` is a transpose that is
//     invisible whenever fu == fv and whenever the footprint is symmetric —
//     which is most random vectors. The tests use asymmetric fractions.
//   · A /255 SCALE. Weights are unit8 PRODUCTS, so the scale is 256·256, not
//     255·255. The same /256-vs-/255 distinction zhao_raster_blend argues at
//     length: this is a WEIGHTING, not a quantizer.
//
// THE ENDPOINTS, stated because they surprise people. `fu = fv = 0` gives
// w00 = 65,536 and the result is EXACTLY t00 — the filter is the identity at
// a texel's own sample point, which is what makes bilinear and nearest agree
// there (see the half-texel bias in zhao_texture_tmu). `fu = 255` is 255/256,
// NOT 1.0, so t10 is never weighted fully — the identical unit8 endpoint
// zhao_raster_blend documents for a = 255.
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

  // 256 − f reaches 256, so the complement is 9 bits. This is the whole
  // reason Σw is exactly 65,536 rather than 65,025 + a fudge.
  logic [8:0] iu, iv, fu, fv;
  always_comb begin
    fu = {1'b0, fu_i};
    fv = {1'b0, fv_i};
    iu = 9'd256 - fu;
    iv = 9'd256 - fv;
  end

  // Weights: each ≤ 65,536, so 17 bits. Σ = (iu+fu)·(iv+fv) = 256·256 exactly.
  logic [16:0] w00, w10, w01, w11;
  always_comb begin
    w00 = iu * iv;
    w10 = fu * iv;
    w01 = iu * fv;
    w11 = fu * fv;
  end

  // One exact wide sum, ONE rescale (spec/qformats.md §3/§4). Σ t·w ≤ 255 ·
  // 65,536 = 16,711,680, so 25 bits carry the sum and its rounding term with
  // room to spare; the shifted result cannot exceed 255 and needs no clamp —
  // the weights are a partition of unity and the inputs are bytes.
  logic [24:0] p00, p10, p01, p11, acc;
  always_comb begin
    p00 = {17'd0, t00_i} * {8'd0, w00};
    p10 = {17'd0, t10_i} * {8'd0, w10};
    p01 = {17'd0, t01_i} * {8'd0, w01};
    p11 = {17'd0, t11_i} * {8'd0, w11};
    acc = p00 + p10 + p01 + p11;
    out_o = 8'((acc + 25'd32768) >> 16);
  end

endmodule : zhao_texture_bilerp
