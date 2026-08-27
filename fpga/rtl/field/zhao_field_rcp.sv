// zhao_field_rcp.sv — the Field IR reciprocal, OP_RCP, walked over the shared
// multiplier.
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
// WHAT CHANGED, 2026-08-23, AND WHAT DID NOT
// ---------------------------------------------------------------------------
// This block used to be COMBINATIONAL, with the sequencer owning its pipeline,
// and it owned two multipliers of its own — one 32x16 and one 16x49. Under the
// DSP ruling of 2026-08-23 no production op unit keeps a private nonconstant
// multiplier, so both products now walk `zhao_field_mul` and the block became
// ready/valid.
//
// THE ANSWER DID NOT MOVE. Every constant, every shift, the single Newton
// correction, the rounding mode and the four saturation rails are unchanged;
// only WHEN the two products are formed is different. That is the claim the
// differential makes, and it makes it against the same oracle as before.
// ENFORCED-BY: tests/differential/field_rcp_directed.cpp:main
//
// The cost is latency: OP_RCP was a six-clock instruction and is now roughly
// thirteen. Nothing in the engine's contract promised six — the docket's own
// framing is that DSP allocation is justified by sustained frame demand, not
// by preserving one-clock placeholder throughputs — and OP_RCP is a per-sample
// field op, not a per-pixel path.
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
// THE SPLIT PRODUCT, which is the only new arithmetic here
// ---------------------------------------------------------------------------
// `n` is u32 with bit 31 set, so `n` is in [2^31, 2^32). The seed is 16 bits, in
// [0x8020, 0xFF80]. So `p = n * x` is at most about 2^48 — 49 bits — and
// `2^48 - p` stays non-negative because the seed is by construction near 2^47/n.
// That is a property of the TABLE rather than of this arithmetic, and the test
// walks every entry of it; the same claim is restated with its full argument at
// the end of this section.
// ENFORCED-BY: tests/differential/field_rcp_directed.cpp:main
//
// A 49-bit operand does not fit the shared lane's 33 signed bits, so `corr` is
// SPLIT at bit 32 and issued twice:
//
//     corr = corr_hi * 2^32 + corr_lo,   corr_lo = corr[31:0] (32 bits, zero
//                                        extended), corr_hi = corr[48:32]
//     x * corr = (x * corr_lo) + ((x * corr_hi) << 32)
//
// This is EXACT, not an approximation: both partial products are formed at full
// width and recombined before the single rescale by 47, so the rounding happens
// exactly once and in exactly the place the reference puts it. Splitting AFTER
// the rescale, or rescaling each partial, would be a different function.
//
// The non-negativity claim is a property of the TABLE, not of this arithmetic:
// a wrong seed would make `2^48 - p` wrap and the answer would be nonsense. It
// is upheld by the seed table being generated from the reference rather than
// transcribed, and checked entry by entry.
// ENFORCED-BY: tests/differential/field_rcp_directed.cpp:main
module zhao_field_rcp (
    input logic clk,
    input logic rst_n,

    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic signed [31:0] a_i,

    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] result_o,
    output logic               sat_rcp_o,   // SatLedger::rcp
    output logic               rcp0_o,      // SatLedger::rcp0, the sticky lane

    // ---- the shared multiplier, `zhao_field_mul` ---------------------------
    output logic               mul_issue_o,
    output logic signed [32:0] mul_a_o,
    output logic signed [32:0] mul_b_o,
    // The lane is 66 bits wide because DOT3 needs three products summed. An op
    // that consumes ONE 32x32 product reads only the low 64 (or 32) of them,
    // which is a property of this op rather than a hole in the port.
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic signed [65:0] mul_p_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  logic               mul_valid_i
);

  localparam logic [2:0] S_IDLE = 3'd0;
  localparam logic [2:0] S_NORM = 3'd7;   // normalise + seed lookup
  localparam logic [2:0] S_P    = 3'd1;   // waiting on n * seed
  localparam logic [2:0] S_CLO  = 3'd2;   // issue seed * corr_lo
  localparam logic [2:0] S_CHI  = 3'd3;   // issue seed * corr_hi
  localparam logic [2:0] S_WLO  = 3'd4;   // collect the low partial
  localparam logic [2:0] S_WHI  = 3'd5;   // collect the high partial, finish
  localparam logic [2:0] S_OUT  = 3'd6;

  logic [2:0] state;

  // ---- the zero case, pinned -----------------------------------------------
  logic is_zero, neg;
  assign is_zero = (a_i == 32'sd0);
  assign neg = a_i[31];

  // |a| as an unsigned word. INT32_MIN maps to 2^31, which is exactly right
  // unsigned and is the whole reason the magnitude is not carried signed.
  logic [31:0] mag;
  assign mag = neg ? (~$unsigned(a_i) + 32'd1) : $unsigned(a_i);

  // The accepted magnitude, held so the normalise below is not a path from
  // the caller's operand straight into the multiplier.
  logic [31:0] h_mag;

  // ---- normalise: shift left until bit 31 is set --------------------------
  // `e` counts down from 31 by the number of leading zeros, exactly as the
  // reference's while-loop does.
  //
  // IT READS h_mag, NOT mag, AND THAT IS THE WHOLE POINT OF S_NORM.
  //
  // MEASURED 2026-08-27, from the Field fit: with the register file rebuilt
  // and the ALU's multiply pipelined, THIS became the critical path of the
  // whole engine, at 16.41 ns against 10.00 required:
  //
  //     zhao_field_ring|e1 -> ring's subtract -> this magnitude -> the
  //     leading-zero detect -> the barrel shift -> the seed ROM -> the
  //     shared multiplier's input register
  //
  // all in ONE clock, because the S_IDLE case issued the product on the
  // accepting edge itself. Its comment said so and called it a saved clock.
  // It cost the engine its clock rate instead -- and not only v2's: v1 sits
  // at 58.99 MHz and v2 at 58.85 on this same path, in this same module.
  //
  // So the clock is spent. The magnitude is registered on accept and
  // everything after it happens in S_NORM.
  logic [ 4:0] lz;
  logic [31:0] lz_t;
  always_comb begin
    lz = 5'd0;
    lz_t = h_mag;
    if (h_mag != 32'd0) begin
      if (lz_t[31:16] == 16'd0) begin lz = lz + 5'd16; lz_t = lz_t << 16; end
      if (lz_t[31:24] == 8'd0)  begin lz = lz + 5'd8;  lz_t = lz_t << 8;  end
      if (lz_t[31:28] == 4'd0)  begin lz = lz + 5'd4;  lz_t = lz_t << 4;  end
      if (lz_t[31:30] == 2'd0)  begin lz = lz + 5'd2;  lz_t = lz_t << 2;  end
      if (lz_t[31] == 1'b0)     begin lz = lz + 5'd1;  lz_t = lz_t << 1;  end
    end
  end

  logic [31:0] n;
  logic [ 5:0] e;
  assign n = h_mag << lz;
  assign e = 6'd31 - 6'({1'b0, lz});

  // ---- the seed --------------------------------------------------------------
  logic [7:0] idx;
  logic [15:0] seed;
  assign idx = n[30:23];  // (n - 2^31) >> 23, with bit 31 known set

  zhao_field_rcp_rom u_rom (
      .idx_i(idx),
      .seed_o(seed)
  );

  // ---- what the walk holds from accept -------------------------------------
  // The caller is not required to hold `a_i` for the whole walk, so everything
  // derived from it is captured on the accepting edge.
  logic [15:0] h_seed;
  logic [ 5:0] h_e;
  logic        h_neg, h_zero;

  // `n * seed` is at most 2^48, so 49 bits is the whole value and not a
  // truncation: `n` is in [2^31, 2^32) and the seed is 16 bits.
  logic [48:0] p_val;
  logic [65:0] xc;         // seed * (2^48 - p), recombined from two partials

  logic [48:0] corr;
  assign corr = 49'h1_0000_0000_0000 - p_val;   // 2^48 - p

  // ---- the shared-lane requests --------------------------------------------
  // Every operand is UNSIGNED and is zero-extended into the lane's 33 signed
  // bits, so nothing on this path can be reinterpreted as negative.
  always_comb begin
    mul_issue_o = 1'b0;
    mul_a_o     = '0;
    mul_b_o     = '0;
    case (state)
      S_NORM: begin
        // One clock after accept, from the REGISTERED magnitude. This used to
        // be issued on the accepting edge itself; see the normalise above for
        // what that cost.
        mul_issue_o = 1'b1;
        mul_a_o     = $signed({1'b0, n});
        mul_b_o     = $signed({17'd0, seed});
      end
      S_CLO: begin
        mul_issue_o = 1'b1;
        mul_a_o     = $signed({17'd0, h_seed});
        mul_b_o     = $signed({1'b0, corr[31:0]});
      end
      S_CHI: begin
        mul_issue_o = 1'b1;
        mul_a_o     = $signed({17'd0, h_seed});
        mul_b_o     = $signed({16'd0, corr[48:32]});
      end
      default: begin
        mul_issue_o = 1'b0;
        mul_a_o     = '0;
        mul_b_o     = '0;
      end
    endcase
  end

  // ---- the finish, from the recombined product -----------------------------
  logic [63:0] x1, shifted;
  logic [63:0] r_wide;
  logic        over;
  always_comb begin
    // rescale_u(x * corr, 47): unsigned round-half-up.
    x1 = 64'((xc + (66'd1 <<< 46)) >> 47);
    shifted = x1 <<< 16;
    // rescale_u(., e), with e == 0 the identity, exactly as the reference states.
    //
    // Recorded because a mutation removing the guard SURVIVES the whole suite:
    // e == 0 happens only for |a| == 1, and 1/(1/65536) is 2^32, which saturates
    // whatever the shift did. So the identity branch is correct, matches the
    // reference, and its VALUE never escapes -- an equivalent mutant, not a hole
    // in the test. It stays because the reference has it and because a future
    // widening of the result would make it live.
    r_wide = (h_e == 6'd0) ? shifted : ((shifted + (64'd1 << (h_e - 6'd1))) >> h_e);
    over   = (r_wide > 64'd2147483647);
  end

  assign v_ready_o = (state == S_IDLE) && (!r_valid_o || r_ready_i);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state     <= S_IDLE;
      h_seed    <= '0;
      h_e       <= '0;
      h_neg     <= 1'b0;
      h_zero    <= 1'b0;
      p_val     <= '0;
      xc        <= '0;
      r_valid_o <= 1'b0;
      result_o  <= '0;
      sat_rcp_o <= 1'b0;
      rcp0_o    <= 1'b0;
    end else begin
      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;

      case (state)
        S_IDLE: begin
          if (v_valid_i && v_ready_o) begin
            // ONLY THE MAGNITUDE AND THE TWO FLAGS. h_seed and h_e used to be
            // captured here too, from a normalise that ran combinationally off
            // the caller's operand -- which is precisely the path that made
            // this module the engine's speed limit. They are captured in
            // S_NORM now, one clock later, from a registered magnitude.
            h_mag  <= mag;
            h_neg  <= neg;
            h_zero <= is_zero;
            if (is_zero) begin
              // qformats §6.2, pinned. No product is issued and none is needed.
              result_o  <= 32'sh7FFF_FFFF;
              sat_rcp_o <= 1'b0;
              rcp0_o    <= 1'b1;
              r_valid_o <= 1'b1;
              state     <= S_OUT;
            end else begin
              state <= S_NORM;
            end
          end
        end

        S_NORM: begin
          // The normalise and the seed lookup happen this clock, on h_mag, and
          // the product is issued from them combinationally in the block above.
          h_seed <= seed;
          h_e    <= e;
          state  <= S_P;
        end

        S_P: begin
          if (mul_valid_i) begin
            p_val <= mul_p_i[48:0];
            state <= S_CLO;
          end
        end

        // Two issues on consecutive cycles: the partials are independent, so
        // the lane's two-cycle latency is paid once rather than twice.
        S_CLO: state <= S_CHI;
        S_CHI: state <= S_WLO;

        S_WLO: begin
          if (mul_valid_i) begin
            xc    <= 66'(mul_p_i[63:0]);
            state <= S_WHI;
          end
        end

        S_WHI: begin
          if (mul_valid_i) begin
            // The recombination, and the ONLY place the two partials meet.
            xc        <= xc + (66'(mul_p_i[63:0]) <<< 32);
            state     <= S_OUT;
          end
        end

        S_OUT: begin
          if (!r_valid_o && !h_zero) begin
            if (over) begin
              result_o  <= h_neg ? 32'sh8000_0000 : 32'sh7FFF_FFFF;
              sat_rcp_o <= 1'b1;
              rcp0_o    <= 1'b0;
            end else begin
              result_o  <= h_neg ? -$signed(r_wide[31:0]) : $signed(r_wide[31:0]);
              sat_rcp_o <= 1'b0;
              rcp0_o    <= 1'b0;
            end
            r_valid_o <= 1'b1;
          end else if (r_valid_o && r_ready_i) begin
            state <= S_IDLE;
          end
        end

        default: state <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_field_rcp
