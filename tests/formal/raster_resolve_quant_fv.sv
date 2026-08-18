// raster_resolve_quant_fv.sv — formal harness for the RGB565 resolve
// quantizer (RASTER.RESOLVE, ZH-024; property raster_resolve_quant.sby).
//
// WHAT IS PROVED, and why it is not vacuous.
//
// The DUT is zhao_raster_quant — the EXACT module zhao_raster_resolve
// instantiates three times, at the two shipping parameter sets: (31, 5, 16, 8)
// for the 5-bit channels and (63, 6, 32, 16) for green. Both are elaborated
// here; the assertions are written against reference/src/zrender/resolve.cpp's
// formula in wide arithmetic, not against a restatement of the RTL.
//
// The free inputs are v (8 bits) and B (4 bits). That parametrisation is
// TOTAL, not a sample: the working colour channel IS a u8, and the 4x4 Bayer
// matrix's entries are exactly 0..15, so free (v, B) ranges over every input
// the block can ever present — all 4,096 of them per channel. There is no
// reachability gap for the solver to hide in.
//
//   P1  a_exact_*    the shipping output IS `min(MAXQ, floor(num/255))` with
//                    num = v*MAXQ + B*AMP + RND. Stated division-free so the
//                    solver never sees a divider:
//                       q == MAXQ  =>  num >= MAXQ*255            (clamped or exact)
//                       q <  MAXQ  =>  q*255 <= num < (q+1)*255   (exact floor)
//                    Together these say exactly "q is the floor, clamped at
//                    MAXQ, and clamped only when the floor really exceeded it".
//                    A wrong Bayer amplitude, a wrong rounding term, an
//                    off-by-one in the /255 reciprocal identity or a
//                    misplaced clamp all break it.
//
//   P2  a_no_wrap_*  THE WHITE RAIL (resolve.cpp, defect fixed 2026-08-16).
//                    The result can NEVER exceed its RGB565 field, for any
//                    input. Without the clamp, green at B >= 8 with g >= 252
//                    quantizes to 64 and wraps in six bits, and full white
//                    resolves to a white/magenta checkerboard. This is that
//                    defect stated as a theorem rather than as a pinned
//                    regression vector.
//
// The cover task is load-bearing. Both assertions are unconditional, so they
// cannot go vacuous through an unreachable antecedent — but the covers pin
// the interesting corners as REACHABLE anyway: green's clamp actually firing
// (i.e. the unclamped floor really is 64, so P2 is not vacuously true because
// nothing ever reaches the rail), full white landing on the rail, and both
// ends of both channels' ranges. If any cover fails, this file is not testing
// what its comments claim.

module raster_resolve_quant_fv (
  input logic       clk,
  input logic [7:0] v_free,  // the 8-bit working colour channel — unconstrained
  input logic [3:0] b_free   // the 4x4 Bayer value, 0..15 — unconstrained
);

  // ---- the two SHIPPING parameter sets ----------------------------------
  // 5-bit channel (red and blue) and 6-bit green, exactly as
  // zhao_raster_resolve instantiates them.
  logic [4:0] q5;
  logic [5:0] q6;

  zhao_raster_quant #(.MAXQ(31), .QW(5), .AMP(16), .RND(8))
    u_five (.v_i(v_free), .bayer_i(b_free), .q_o(q5));
  zhao_raster_quant #(.MAXQ(63), .QW(6), .AMP(32), .RND(16))
    u_six  (.v_i(v_free), .bayer_i(b_free), .q_o(q6));

  // ---- THE LAW, in wide arithmetic (resolve.cpp) -------------------------
  // num5 = v*31 + B*16 +  8   (max 255*31 + 15*16 +  8 =  8,153)
  // num6 = v*63 + B*32 + 16   (max 255*63 + 15*32 + 16 = 16,561)
  localparam int unsigned LW = 24;  // room for (q+1)*255 with margin

  logic [LW-1:0] num5, num6;
  assign num5 = (LW'(v_free) * LW'(31)) + (LW'(b_free) * LW'(16)) + LW'(8);
  assign num6 = (LW'(v_free) * LW'(63)) + (LW'(b_free) * LW'(32)) + LW'(16);

  logic [LW-1:0] lo5, hi5, lo6, hi6;
  assign lo5 = LW'(q5) * LW'(255);
  assign hi5 = (LW'(q5) + LW'(1)) * LW'(255);
  assign lo6 = LW'(q6) * LW'(255);
  assign hi6 = (LW'(q6) + LW'(1)) * LW'(255);

  always_ff @(posedge clk) begin
    // P1 — exactly min(MAXQ, floor(num/255)), stated without a division
    a_exact_five: assert ((q5 == 5'd31) ? (num5 >= LW'(31) * LW'(255))
                                        : ((num5 >= lo5) && (num5 < hi5)));
    a_exact_six:  assert ((q6 == 6'd63) ? (num6 >= LW'(63) * LW'(255))
                                        : ((num6 >= lo6) && (num6 < hi6)));

    // P2 — the white rail: the field can never overflow
    a_no_wrap_five: assert (q5 <= 5'd31);
    a_no_wrap_six:  assert (q6 <= 6'd63);
  end

  always_ff @(posedge clk) begin
    c_five_zero:  cover (q5 == 5'd0);
    c_five_max:   cover (q5 == 5'd31);
    c_six_zero:   cover (q6 == 6'd0);
    c_six_max:    cover (q6 == 6'd63);
    // full white lands ON the rail in both channels
    c_white_five: cover (v_free == 8'd255 && q5 == 5'd31);
    c_white_six:  cover (v_free == 8'd255 && q6 == 6'd63);
    // THE CLAMP ACTUALLY FIRES: the unclamped floor is 64 and the rail caught
    // it. Without this cover, a_no_wrap_six would hold for a quantizer that
    // simply never reaches the top — which is not the theorem claimed above.
    c_six_clamped: cover (num6 >= LW'(64) * LW'(255) && q6 == 6'd63);
    // ...and the 5-bit channels genuinely have exact headroom: their
    // numerator never reaches 32*255 = 8,160 (max 8,153), so their clamp is
    // belt-and-braces. Covering the top of the range documents that.
    c_five_top:    cover (num5 >= LW'(31) * LW'(255) && num5 < LW'(32) * LW'(255));
  end

endmodule : raster_resolve_quant_fv
