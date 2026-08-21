// zhao_field_rcp.sv — the Field IR reciprocal, OP_RCP.
//
// A submodule of the FIELD.SEQ.* family. Reference: `zref::field_rcp`
// (reference/include/zref/zref_rcp.hpp §6.2), which is what
// `zfield::interpret` calls for OP_RCP.
//
// The seed table lives in `zhao_field_rcp_rom.sv`, GENERATED from
// `zref_tables.hpp` rather than transcribed, and checked entry by entry in the
// test. A wrong seed does not fail loudly; it just makes some reciprocals
// slightly wrong.
//
// ---------------------------------------------------------------------------
// THE LAW, step for step
// ---------------------------------------------------------------------------
//     a == 0        -> rcp0 (STICKY in the ledger) and the PINNED 0x7FFFFFFF
//     neg = a < 0,  n = |a|
//     normalise n left until bit 31 is set, counting e down from 31
//     idx = (n - 2^31) >> 23                       (the 8 bits below the 1)
//     x   = FIELD_RCP_T0[idx]
//     p   = n * x
//     x   = rescale_u(x * (2^48 - p), 47)          ONE pinned correction
//     r   = rescale_u(x << 16, e)
//     r > INT32_MAX -> rcp saturation, +-INT32_MAX
//     result = neg ? -r : r
//
// `rescale_u(v, k)` is UNSIGNED round-half-up: `(v + 2^(k-1)) >> k`, and `k == 0`
// is the identity. Not the signed `rescale_s32` used elsewhere in the tree —
// every value on this path is a magnitude, the sign is reapplied at the very
// end, and using the signed form here would be a different function.
//
// Four things are load-bearing:
//
// 1. **`1/0` IS 0x7FFFFFFF AND IS NOT AN ERROR.** qformats §6.2 pins it. The
//    ledger's `rcp0` lane is STICKY so a capture records that it happened, but
//    the program runs on with a defined value. A block that trapped or returned
//    zero would change what a division by zero does to the game.
// 2. **ONE CORRECTION STEP, NOT TWO.** The Newton iteration is applied exactly
//    once. Two steps would be more accurate and would disagree with every
//    reciprocal the reference has ever produced.
// 3. **THE SIGN IS REAPPLIED LAST**, to the magnitude. `-INT32_MIN` does not fit
//    s32, which is why the magnitude is carried as unsigned throughout: taking
//    the absolute value early in signed arithmetic loses exactly that input.
// 4. **THE SATURATION LANE IS `rcp`, NOT `mul` OR `rescale`.** The reference
//    keeps four lanes apart and a block that recorded in the wrong one could
//    still return the right number.
//
// ---------------------------------------------------------------------------
// WIDTHS, and the one that is nearly tight
// ---------------------------------------------------------------------------
// `n` is u32 with bit 31 set, so `n` is in [2^31, 2^32). The seed is 16 bits, in
// [0x8020, 0xFF80]. So `p = n * x` is at most about 2^48 — 49 bits — and
// `2^48 - p` stays non-negative because the seed is by construction near 2^47/n.
// `x * (2^48 - p)` reaches about 2^63: it needs a full 64 bits and no more, and
// that is the tightest width in the block. `x << 16` is 33 bits, and the final
// shift by `e` (0..31) brings it into range.
//
// The non-negativity claim is a property of the TABLE, not of this arithmetic:
// a wrong seed would make `2^48 - p` wrap and the answer would be nonsense. It
// is upheld by the seed table being generated from the reference rather than
// transcribed, and checked entry by entry.
// ENFORCED-BY: tests/differential/field_rcp_directed.cpp:main
module zhao_field_rcp (
    // Combinational: the sequencer owns the pipeline.
    input  logic signed [31:0] a_i,

    output logic signed [31:0] result_o,
    output logic               sat_rcp_o,   // SatLedger::rcp
    output logic               rcp0_o       // SatLedger::rcp0, the sticky lane
);

  // ---- the zero case, pinned -----------------------------------------------
  logic is_zero, neg;
  assign is_zero = (a_i == 32'sd0);
  assign neg = a_i[31];

  // |a| as an unsigned word. INT32_MIN maps to 2^31, which is exactly right
  // unsigned and is the whole reason the magnitude is not carried signed.
  logic [31:0] mag;
  assign mag = neg ? (~$unsigned(a_i) + 32'd1) : $unsigned(a_i);

  // ---- normalise: shift left until bit 31 is set --------------------------
  // `e` counts down from 31 by the number of leading zeros, exactly as the
  // reference's while-loop does.
  logic [ 4:0] lz;
  logic [31:0] lz_t;
  always_comb begin
    lz = 5'd0;
    lz_t = mag;
    if (mag != 32'd0) begin
      if (lz_t[31:16] == 16'd0) begin lz = lz + 5'd16; lz_t = lz_t << 16; end
      if (lz_t[31:24] == 8'd0)  begin lz = lz + 5'd8;  lz_t = lz_t << 8;  end
      if (lz_t[31:28] == 4'd0)  begin lz = lz + 5'd4;  lz_t = lz_t << 4;  end
      if (lz_t[31:30] == 2'd0)  begin lz = lz + 5'd2;  lz_t = lz_t << 2;  end
      if (lz_t[31] == 1'b0)     begin lz = lz + 5'd1;  lz_t = lz_t << 1;  end
    end
  end

  logic [31:0] n;
  logic [ 5:0] e;
  assign n = mag << lz;
  assign e = 6'd31 - 6'({1'b0, lz});

  // ---- the seed --------------------------------------------------------------
  logic [7:0] idx;
  logic [15:0] seed;
  assign idx = n[30:23];  // (n - 2^31) >> 23, with bit 31 known set

  zhao_field_rcp_rom u_rom (
      .idx_i(idx),
      .seed_o(seed)
  );

  // ---- one pinned Newton correction ---------------------------------------
  logic [63:0] p, corr, x1, shifted;
  logic [63:0] r_wide;
  always_comb begin
    p = 64'(n) * 64'({48'd0, seed});
    corr = 64'h0001_0000_0000_0000 - p;              // 2^48 - p
    // rescale_u(x * corr, 47): unsigned round-half-up.
    x1 = ((64'({48'd0, seed}) * corr) + (64'd1 <<< 46)) >> 47;
    shifted = x1 <<< 16;
    // rescale_u(., e), with e == 0 the identity, exactly as the reference states.
    //
    // Recorded because a mutation removing the guard SURVIVES the whole suite:
    // e == 0 happens only for |a| == 1, and 1/(1/65536) is 2^32, which saturates
    // whatever the shift did. So the identity branch is correct, matches the
    // reference, and its VALUE never escapes -- an equivalent mutant, not a hole
    // in the test. It stays because the reference has it and because a future
    // widening of the result would make it live.
    r_wide = (e == 6'd0) ? shifted : ((shifted + (64'd1 << (e - 6'd1))) >> e);
  end

  logic over;
  assign over = (r_wide > 64'd2147483647);

  always_comb begin
    if (is_zero) begin
      result_o = 32'sh7FFF_FFFF;  // qformats §6.2, pinned
      sat_rcp_o = 1'b0;
      rcp0_o = 1'b1;
    end else if (over) begin
      result_o = neg ? 32'sh8000_0000 : 32'sh7FFF_FFFF;
      sat_rcp_o = 1'b1;
      rcp0_o = 1'b0;
    end else begin
      result_o = neg ? -$signed(r_wide[31:0]) : $signed(r_wide[31:0]);
      sat_rcp_o = 1'b0;
      rcp0_o = 1'b0;
    end
  end

endmodule : zhao_field_rcp
