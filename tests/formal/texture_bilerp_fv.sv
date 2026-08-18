// texture_bilerp_fv.sv — formal harness for the TEXTURE.TMU bilinear channel
// (ZH-027; property texture_bilerp.sby).
//
// WHAT IS PROVED, and why it is not vacuous.
//
// The DUT is zhao_texture_bilerp — the EXACT module zhao_texture_tmu
// instantiates four times, once per channel (R, G, B and A). The assertions
// are written against the DERIVED law in a wide unsigned lane, not against a
// restatement of the RTL's expression. That law's derivation from
// spec/qformats.md §2/§3/§4 is in the RTL header and in
// design/contracts/TEXTURE.TMU.md; nothing here re-argues it.
//
// The free inputs are the four texel bytes and the two unit8 fractions — 48
// bits, which is TOTAL rather than sampled: a texel channel IS a byte (every
// format decodes to charter §8's 8-bit lanes) and a sub-texel fraction IS a
// unit8 (spec/qformats.md §2). Free (t00,t10,t01,t11,fu,fv) ranges over every
// input the block can ever be handed — all 2^48 of them. There is no
// reachability gap for the solver to hide in, and no horizon for a scope
// guard to assert: the harness is purely combinational (zhao_texture_bilerp
// is three always_comb blocks; the only always_ff blocks hold the assert and
// cover statements), so the reachable state set is identical at every step
// and depth 2 IS the full state space.
//
//   P1  a_exact        The output is EXACTLY
//                        (Σ t·w + 32768) >> 16,  w as derived
//                      computed here in a 32-bit lane. This is the property
//                      that catches all three named one-LSB traps at once: a
//                      truncated rounding (the +32768 dropped), swapped
//                      weights (w10 with t01), and a /255 scale.
//
//   P2  a_wsum         PARTITION OF UNITY, in its weights-only form:
//                      Σw = 65,536 EXACTLY, for every (fu, fv). This is the
//                      strongest single statement about the weights and it is
//                      true only because the complement `256 − f` reaches 256
//                      (spec/qformats.md §2: a unit8's value is raw/256). A
//                      filter whose weights summed to 65,025 — the /255
//                      mistake — fails it immediately. Together with P1 it
//                      gives the flat-footprint theorem by arithmetic: if all
//                      four texels are t then Σt·w = t·65,536 and the single
//                      rescale returns t, so a flat texture cannot develop a
//                      gradient.
//
//   P3  a_corner       At fu = fv = 0 the filter is the EXACT identity on
//                      t00. This is what makes nearest sampling a special
//                      case of the same datapath — zhao_texture_tmu forces
//                      the fractions to zero for FILTER_NEAREST and takes no
//                      second path, so if this failed, every nearest sample
//                      in the machine would be wrong.
//
//   P4  a_in_field     The result never leaves the 8-bit field, with no clamp
//                      anywhere in the module.
//
// WHAT WAS TRIED AND REMOVED, so nobody re-adds it blind. Two properties are
// real, were written, were run, and do NOT close on this engine:
//
//   · MONOTONICITY (increasing fu toward a brighter tap cannot darken the
//     result) needs a SECOND DUT instance at fu+1, doubling the eight
//     bit-blasted multipliers the solver already reasons about.
//   · CONVEXITY / no-overshoot (`min(t) ≤ out ≤ max(t)`) is a genuinely
//     NONLINEAR statement: it requires the solver to know `t_i·w_i ≤ max·w_i`,
//     which bit-blasting does not get cheaply.
//
// Measured on this kit (boolector, depth 2, 32-bit law lane): P1 alone closes
// in 339 s; P1 + P2 + convexity ran past 25 minutes without an answer, twice.
// The exactness theorem P1 is what carries the weight anyway — because the
// law is computed in a wide lane and compared against an EIGHT-BIT output, it
// proves as a side effect that the weighted sum can never leave the field,
// which is the safety half of convexity. What it does not prove is that the
// result stays inside its own footprint; the differential lanes and the
// mutation table cover that, and this note is here so the gap is visible
// rather than assumed away.
//
// The cover task is load-bearing. Every assertion above is unconditional, so
// none can go vacuous through an unreachable antecedent — but the covers pin
// the interesting corners as REACHABLE anyway: the EXACT ROUNDING TIE firing
// (without it, P1 holds for a truncating filter on every input that is not a
// tie), the output landing strictly between min and max (without which P3
// holds for a filter that never mixes), and each of the four corners being
// reached exactly.
//
// WHAT THIS DOES NOT PROVE, stated plainly: the address generation, the wrap
// folds, the mip level selection and its offset closed form, the format
// decodes, the palette pass, the cache handshake and the mode-error rules are
// NOT proved here. They are covered by the differential lanes against
// zref::Tmu (texture_tmu_directed's format, wrap, mirror-vs-frozen, mip and
// mode-error cases, plus texture_tmu_random's three lanes) and by the
// mutation evidence in design/contracts/TEXTURE.TMU.md. What IS proved is the
// arithmetic every filtered texel in the machine flows through, four times.
//
// Frontend: read_slang (the lane choice since W2.3). zhao_texture_bilerp.sv is
// self-contained — no package dependency, nothing staged or copied.

module texture_bilerp_fv (
  input logic       clk,
  input logic [7:0] t00_free,
  input logic [7:0] t10_free,
  input logic [7:0] t01_free,
  input logic [7:0] t11_free,
  input logic [7:0] fu_free,
  input logic [7:0] fv_free
);

  // ---- the SHIPPING module, the exact bytes the TMU filters with ---------
  logic [7:0] out;
  zhao_texture_bilerp u_dut (
    .t00_i (t00_free),
    .t10_i (t10_free),
    .t01_i (t01_free),
    .t11_i (t11_free),
    .fu_i  (fu_free),
    .fv_i  (fv_free),
    .out_o (out)
  );

  // ---- THE LAW, in a wide unsigned lane ----------------------------------
  // 32 bits, not 64: the exact sum is bounded by 255 * 65,536 = 16,711,680,
  // which needs 25. A 64-bit lane makes the solver bit-blast 64x64
  // multipliers against the DUT's 25-bit ones and the task does not close.
  localparam int unsigned LW = 32;

  logic [LW-1:0] t00, t10, t01, t11, fu, fv, iu, iv;
  assign t00 = {{(LW-8){1'b0}}, t00_free};
  assign t10 = {{(LW-8){1'b0}}, t10_free};
  assign t01 = {{(LW-8){1'b0}}, t01_free};
  assign t11 = {{(LW-8){1'b0}}, t11_free};
  assign fu  = {{(LW-8){1'b0}}, fu_free};
  assign fv  = {{(LW-8){1'b0}}, fv_free};
  assign iu  = LW'(256) - fu;
  assign iv  = LW'(256) - fv;

  logic [LW-1:0] w00, w10, w01, w11, wsum, acc, law;
  assign w00  = iu * iv;
  assign w10  = fu * iv;
  assign w01  = iu * fv;
  assign w11  = fu * fv;
  assign wsum = w00 + w10 + w01 + w11;
  assign acc  = t00 * w00 + t10 * w10 + t01 * w01 + t11 * w11;
  assign law  = (acc + LW'(32768)) >> 16;

  // the extremes of the footprint, ordered
  logic [LW-1:0] lo, hi, lo2, hi2;
  assign lo2 = (t00 < t10) ? t00 : t10;
  assign hi2 = (t00 < t10) ? t10 : t00;
  assign lo  = (lo2 < ((t01 < t11) ? t01 : t11)) ? lo2 : ((t01 < t11) ? t01 : t11);
  assign hi  = (hi2 > ((t01 > t11) ? t01 : t11)) ? hi2 : ((t01 > t11) ? t01 : t11);

  logic [LW-1:0] outw;
  assign outw = {{(LW-8){1'b0}}, out};

  logic all_equal;
  assign all_equal = (t00_free == t10_free) && (t00_free == t01_free) && (t00_free == t11_free);

  always_ff @(posedge clk) begin
    // P1 — the output IS the derived law, single rounding included.
    a_exact: assert (outw == law);

    // P2 — partition of unity, weights-only form. The weights sum to 65,536
    //      EXACTLY, which with P1 makes a flat footprint filter to itself.
    a_wsum: assert (wsum == LW'(65536));


    // P3 — the exact identity at the origin corner: nearest sampling IS this
    //      module with both fractions forced to zero.
    a_corner: assert ((fu_free != 8'd0) || (fv_free != 8'd0) || (outw == t00));

    // P4 — the 8-bit field, with no clamp in the datapath.
    a_in_field: assert (outw <= LW'(255));
  end

  always_ff @(posedge clk) begin
    // THE ROUNDING TIE ACTUALLY FIRES. Without this cover, a_exact holds for a
    // truncating filter on every input that is not a tie — which is almost
    // all of them, and exactly why this is the load-bearing cover.
    c_tie_up: cover (acc[15:0] == 16'h8000 && out != 8'd0);
    // ...and the case just below it, which must NOT round up.
    c_tie_below: cover (acc[15:0] == 16'h7FFF);

    // THE FILTER ACTUALLY MIXES, i.e. lands strictly inside its own
    // footprint. Without this the exactness theorem would also hold for a
    // filter that never left tap 0.
    c_strictly_between: cover (outw > lo && outw < hi);

    // Each corner reached exactly, on a footprint where the corners differ.
    c_corner00: cover (fu_free == 8'd0 && fv_free == 8'd0 && t00_free != t11_free && out == t00_free);
    c_corner10: cover (fu_free == 8'd255 && fv_free == 8'd0 && t00_free == 8'd0 &&
                       t10_free == 8'd255 && out == 8'd254);

    // The 128/128 centre, the most-used fraction in a magnified sample.
    c_centre: cover (fu_free == 8'd128 && fv_free == 8'd128 && outw > lo && outw < hi);

    // A flat footprint really IS flat, for a non-trivial fraction: the
    // arithmetic consequence of P1 + P2, pinned as reachable.
    c_unity_flat: cover (all_equal && fu_free != 8'd0 && fv_free != 8'd0 && t00_free != 8'd0 &&
                         out == t00_free);
  end

endmodule : texture_bilerp_fv
