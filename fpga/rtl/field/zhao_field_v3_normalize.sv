// zhao_field_v3_normalize.sv — NORMALIZE2 and NORMALIZE3 for four points at
// once: an owned isqrt walked four times, an owned rcp24 seed ROM, and ten
// four-wide requests on the shared multiplier bank.
//
// ENFORCED-BY: tests/differential/field_v3_normalize_directed.cpp:main
//
// ---------------------------------------------------------------------------
// THE LAW: zref::normalize3_approx / zfield::steps::normalize2, step for step
// ---------------------------------------------------------------------------
//     n2  = SUM of x*x (exact u64; two lanes for N2, three for N3)
//     n2 == 0  ->  every output ZERO and an RCP0 event. Not an error.
//     len = isqrt_u64(n2)                     exact floor square root
//     m, e: shift len into [2^23, 2^24), e counting the shifts
//     r   = rcp_u24_norm(m)                   seed ROM + TWO Newton steps
//     out = rescale_s32(x * r, 31 + e)        one rescale per component
//
// FOUR THINGS ARE LOAD-BEARING:
//
// 1. **n2 IS EXACT, NOT ROUNDED.** The squares are summed at full width --
//    three s32 squares reach 3*2^62, which fits u64 -- and nothing is rescaled
//    until the very end. A rounded n2 gives a different length and a different
//    answer everywhere.
//
// 2. **THE ZERO VECTOR IS A DEFINED ANSWER**, not a division by zero: outputs
//    zero and the ledger records RCP0. A unit that skipped the test would
//    normalise len == 0 and shift forever.
//
// 3. **e IS PER POINT.** Four points have four lengths, four normalisation
//    shifts and four output scalings. The bank is four wide and the products
//    are shared, but the SHIFT is not -- which is the one place a vector unit
//    differs from four scalar ones by more than width.
//
// 4. **ONE RESCALE PER COMPONENT, at the end.** Not one per Newton step and
//    not one on the length. The reference's error bound (<= 2 LSB per
//    component, measured 0.51) is a property of that arrangement.
//
// ---------------------------------------------------------------------------
// THE isqrt IS OWNED AND WALKED, WHICH IS A DELIBERATE SLOW PATH
// ---------------------------------------------------------------------------
// `zhao_field_isqrt` is a scalar block running a fixed THIRTY-TWO iterations,
// and this unit walks four points through one instance rather than
// instantiating four. That is ~128 clocks of the ~150 this op costs.
//
// It is the right first shape for two reasons and a third that matters more:
//
//   * NORMALIZE is COLD in the op shape table (svc class 4), so the engine
//     already expects it to be slow.
//   * Four instances is four copies of a 64-bit restoring recurrence -- real
//     ALM against a device whose Field engine has already been redesigned once
//     for area.
//   * A four-wide isqrt would be a SHARED RESOURCE with its own arbitration,
//     refusal and starvation questions, and that class of thing has cost more
//     time in this engine than every arithmetic bug put together.
//
// If a program mix ever makes NORMALIZE hot, widening the walk is a contained
// change and the measurement to justify it is the composed engine's occupancy.
// `zhao_probe_dist_svc` already computes four-point lengths and is a candidate
// to borrow from -- but its reply SATURATES into s32, and normalisation needs
// the exact u32 length, so borrowing it is not the trivial swap it looks like.
module zhao_field_v3_normalize (
    input var logic clk,
    input var logic rst_n,

    // ---- request: one four-point group -------------------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic               is_n3_i,     // 0 = NORMALIZE2, 1 = NORMALIZE3
    input  var logic signed [31:0] a0_0_i, a0_1_i, a0_2_i, a0_3_i,
    input  var logic signed [31:0] a1_0_i, a1_1_i, a1_2_i, a1_3_i,
    input  var logic signed [31:0] a2_0_i, a2_1_i, a2_2_i, a2_3_i,
    input  var logic        [ 7:0] tag_i,

    // ---- reply -------------------------------------------------------------
    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] o0_0_o, o0_1_o, o0_2_o, o0_3_o,
    output var logic signed [31:0] o1_0_o, o1_1_o, o1_2_o, o1_3_o,
    output var logic signed [31:0] o2_0_o, o2_1_o, o2_2_o, o2_3_o,
    output var logic        [ 3:0] sat_rescale_o,
    output var logic        [ 3:0] rcp0_o,       // the zero vector, per point
    output var logic        [ 7:0] tag_o,

    // ---- the shared four-wide bank (engine property) -----------------------
    output var logic               mul_issue_o,
    input  var logic               mul_ready_i,
    output var logic signed [32:0] mul_a_0_o, mul_a_1_o, mul_a_2_o, mul_a_3_o,
    output var logic signed [32:0] mul_b_0_o, mul_b_1_o, mul_b_2_o, mul_b_3_o,
    input  var logic               mul_valid_i,
    input  var logic signed [65:0] mul_p_0_i, mul_p_1_i, mul_p_2_i, mul_p_3_i
);

  localparam int LANES = 4;

  localparam logic [4:0] S_IDLE   = 5'd0;
  localparam logic [4:0] S_SQ0    = 5'd1;   // x*x
  localparam logic [4:0] S_SQ0W   = 5'd2;
  localparam logic [4:0] S_SQ1    = 5'd3;   // y*y
  localparam logic [4:0] S_SQ1W   = 5'd4;
  localparam logic [4:0] S_SQ2    = 5'd5;   // z*z (N3 only)
  localparam logic [4:0] S_SQ2W   = 5'd6;
  localparam logic [4:0] S_ROOT   = 5'd7;   // walk four lanes through the isqrt
  localparam logic [4:0] S_NEWT_A = 5'd8;   // p = m * x
  localparam logic [4:0] S_NEWT_AW = 5'd9;
  localparam logic [4:0] S_NEWT_B = 5'd10;  // x = rescale30(x * (2^31 - w))
  localparam logic [4:0] S_NEWT_BW = 5'd11;
  localparam logic [4:0] S_OUT0   = 5'd12;  // x * r
  localparam logic [4:0] S_OUT0W  = 5'd13;
  localparam logic [4:0] S_OUT1   = 5'd14;
  localparam logic [4:0] S_OUT1W  = 5'd15;
  localparam logic [4:0] S_OUT2   = 5'd16;
  localparam logic [4:0] S_OUT2W  = 5'd17;
  localparam logic [4:0] S_SEED   = 5'd19;  // latch the ROM seed, once
  localparam logic [4:0] S_DONE   = 5'd18;

  logic [4:0] state;
  logic       h_n3;
  logic [7:0] h_tag;
  logic signed [31:0] h_a [3][LANES];

  logic [63:0] n2   [LANES];
  // No `len` register: the mantissa and exponent below are everything the
  // length contributes, and a copy nothing reads is dead state that reads as
  // if somebody uses it.
  logic [23:0] mant [LANES];         // len normalised into [2^23, 2^24)
  logic signed [7:0] expo [LANES];   // e, signed and per point (law 3)
  // THIRTY-TWO BITS, NOT THIRTY-ONE. The reference's iterate is a uint32_t
  // and it REACHES 2^31: for m == 2^23 exactly -- any length that is an
  // exact power of two -- the true reciprocal is 2^24 and the pre-shift
  // iterate is 2^31, which does not fit in 31 bits and wraps to ZERO.
  //
  // That is precisely how the first run failed: lanes 1 and 2 were right and
  // lanes 0 and 3 read zero, and the only thing those two shared was a
  // power-of-two length. A width bug that only bites on exact powers of two
  // is the kind that survives random testing for a long time.
  logic [31:0] rx   [LANES];         // the Newton iterate
  logic        zero [LANES];

  logic [1:0]  root_lane;            // which point the isqrt is chewing on
  // ISSUED, NOT MERELY ASKING. Without this the request stays asserted while
  // the root is in flight, and on the clock the answer is taken the isqrt can
  // become ready again and accept THE SAME n2 -- because root_lane has not
  // advanced yet. The next lane would then be handed the previous lane's
  // length: a plausible number, for the wrong point.
  logic        root_busy;
  logic        newt_step;            // 0 = first Newton pass, 1 = second

  // ---- the owned isqrt ----------------------------------------------------
  logic        isq_valid, isq_ready, isq_rvalid, isq_rready;
  logic [63:0] isq_n, isq_r;

  zhao_field_isqrt u_isqrt (
      .clk(clk), .rst_n(rst_n),
      .n_valid_i(isq_valid), .n_ready_o(isq_ready), .n_i(isq_n),
      .r_valid_o(isq_rvalid), .r_ready_i(isq_rready), .r_o(isq_r)
  );

  // GATED ON n2 ITSELF, NOT ON THE `zero` REGISTER. The register is set on the
  // clock the zero lane is recognised, which is one clock too late: the isqrt
  // would already have accepted n2 == 0, and nobody would ever consume its
  // answer, so `n_ready_o` stays low and EVERY LATER LANE HANGS.
  //
  // Measured exactly that way: a group with zeros on lanes 0 and 2 never
  // replied, while a group of four zeros passed -- because with every lane
  // zero, nothing needed the isqrt afterwards.
  assign isq_valid  = (state == S_ROOT) && (n2[root_lane] != 64'd0) && !root_busy;
  assign isq_n      = n2[root_lane];
  assign isq_rready = (state == S_ROOT) && root_busy;

  // ---- the owned rcp24 seed ROM, one lookup per lane ----------------------
  // Combinational, so all four seeds are available at once; the ROM is 256
  // entries and the same scarcity argument as ROT's sine table applies.
  logic [7:0]  seed_idx [LANES];
  logic [30:0] seed_val [LANES];
  for (genvar g = 0; g < LANES; g++) begin : g_seed
    assign seed_idx[g] = mant[g][22:15];
    zhao_field_rcp24_rom u_rom (.idx_i(seed_idx[g]), .seed_o(seed_val[g]));
  end

  // ---- the bank's operands ------------------------------------------------
  logic signed [32:0] mul_a [LANES], mul_b [LANES];
  logic signed [65:0] prod  [LANES];
  assign prod[0] = mul_p_0_i;
  assign prod[1] = mul_p_1_i;
  assign prod[2] = mul_p_2_i;
  assign prod[3] = mul_p_3_i;

  // 2^31 - (p >> 24), the Newton correction term.
  logic [31:0] newt_w [LANES];
  always_comb begin
    for (int l = 0; l < LANES; l++) begin
      newt_w[l] = 32'((65'(prod[l]) >> 24) & 65'h7FFF_FFFF);
    end
  end

  always_comb begin
    for (int l = 0; l < LANES; l++) begin
      mul_a[l] = 33'sd0;
      mul_b[l] = 33'sd0;
      case (state)
        S_SQ0: begin
          mul_a[l] = 33'(h_a[0][l]);
          mul_b[l] = 33'(h_a[0][l]);
        end
        S_SQ1: begin
          mul_a[l] = 33'(h_a[1][l]);
          mul_b[l] = 33'(h_a[1][l]);
        end
        S_SQ2: begin
          mul_a[l] = 33'(h_a[2][l]);
          mul_b[l] = 33'(h_a[2][l]);
        end
        // p = M * x, with M the 24-bit mantissa and x the iterate. Both are
        // UNSIGNED and both fit, so they zero-extend into the signed lane.
        S_NEWT_A: begin
          mul_a[l] = $signed({9'd0, mant[l]});
          mul_b[l] = $signed({1'b0, rx[l]});
        end
        // x = x * (2^31 - w)
        S_NEWT_B: begin
          mul_a[l] = $signed({1'b0, rx[l]});
          mul_b[l] = $signed({1'b0, 32'h8000_0000 - newt_w[l]});
        end
        S_OUT0: begin
          mul_a[l] = 33'(h_a[0][l]);
          mul_b[l] = $signed({1'b0, rx[l]});
        end
        S_OUT1: begin
          mul_a[l] = 33'(h_a[1][l]);
          mul_b[l] = $signed({1'b0, rx[l]});
        end
        default: begin
          mul_a[l] = 33'(h_a[2][l]);
          mul_b[l] = $signed({1'b0, rx[l]});
        end
      endcase
    end
  end

  assign mul_issue_o = (state == S_SQ0) || (state == S_SQ1) || (state == S_SQ2) ||
                       (state == S_NEWT_A) || (state == S_NEWT_B) ||
                       (state == S_OUT0) || (state == S_OUT1) || (state == S_OUT2);
  assign mul_a_0_o = mul_a[0];
  assign mul_a_1_o = mul_a[1];
  assign mul_a_2_o = mul_a[2];
  assign mul_a_3_o = mul_a[3];
  assign mul_b_0_o = mul_b[0];
  assign mul_b_1_o = mul_b[1];
  assign mul_b_2_o = mul_b[2];
  assign mul_b_3_o = mul_b[3];

  // ---- the reference's rescales -------------------------------------------
  // rescale by a VARIABLE amount, round half up, saturating into s32. `k` is
  // 31 + e and e is per point, so this cannot be a constant shift.
  function automatic logic signed [31:0] resc_var(input logic signed [65:0] v,
                                                  input logic [6:0] k);
    logic signed [66:0] r;
    begin
      r = (67'(v) + (67'sd1 <<< (k - 7'd1))) >>> k;
      if (r > 67'sd2147483647) resc_var = 32'sh7FFF_FFFF;
      else if (r < -67'sd2147483648) resc_var = 32'sh8000_0000;
      else resc_var = r[31:0];
    end
  endfunction

  function automatic logic resc_var_fired(input logic signed [65:0] v,
                                          input logic [6:0] k);
    logic signed [66:0] r;
    begin
      r = (67'(v) + (67'sd1 <<< (k - 7'd1))) >>> k;
      resc_var_fired = (r > 67'sd2147483647) || (r < -67'sd2147483648);
    end
  endfunction

  // The last line of rcp_u24_norm: r = rescale_u64_nosat(x, 7), then clamped
  // to a u24. The clamp is PINNED LAW, not an overflow -- the reference says
  // so: the only input that reaches it is m == 2^23 exactly, whose true
  // reciprocal is 2^24, and 0xFFFFFF is one LSB below. Nothing is recorded.
  function automatic logic [31:0] rcp_finish(input logic [31:0] x);
    logic [32:0] r;
    begin
      r = (33'(x) + 33'd64) >> 7;
      rcp_finish = (r > 33'h0_00FF_FFFF) ? 32'h00FF_FFFF : 32'(r);
    end
  endfunction

  // The unsigned Newton rescale, which never saturates by construction: the
  // iterate is bounded above by 2^31 and the result is masked to 32 bits, so
  // the mask never discards a set bit. That reasoning has already been wrong
  // here once -- the iterate REACHES 2^31 whenever the length is an exact
  // power of two, and a 31-bit register wrapped it to zero, leaving two lanes
  // right and two reading zero. The defect is kept as mutant M02 and caught,
  // so the bound below is checked rather than asserted.
  //
  // ENFORCED-BY: tests/differential/field_v3_normalize_directed.cpp:main
  function automatic logic [31:0] resc_u30(input logic signed [65:0] v);
    logic [65:0] u;
    begin
      u = 66'(v);
      resc_u30 = 32'(((u + (66'd1 << 29)) >> 30) & 66'hFFFF_FFFF);
    end
  endfunction

  // Shift `len` into [2^23, 2^24) and report how far. A priority encode: the
  // position of the highest set bit decides both.
  function automatic logic [5:0] top_bit(input logic [63:0] v);
    logic [5:0] p;
    begin
      p = 6'd0;
      for (int b = 0; b < 64; b++) if (v[b]) p = 6'(b);
      top_bit = p;
    end
  endfunction

  logic [6:0] out_k [LANES];
  always_comb begin
    for (int l = 0; l < LANES; l++) out_k[l] = 7'(31 + expo[l]);
  end

  assign v_ready_o = (state == S_IDLE);
  assign tag_o     = h_tag;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= S_IDLE;
      h_n3 <= 1'b0;
      h_tag <= 8'd0;
      root_lane <= 2'd0;
      root_busy <= 1'b0;
      newt_step <= 1'b0;
      r_valid_o <= 1'b0;
      sat_rescale_o <= 4'b0;
      rcp0_o <= 4'b0;
      for (int l = 0; l < LANES; l++) begin
        n2[l] <= 64'd0;
        mant[l] <= 24'd0;
        expo[l] <= 8'sd0;
        rx[l] <= 32'd0;
        zero[l] <= 1'b0;
        for (int c = 0; c < 3; c++) h_a[c][l] <= '0;
      end
      o0_0_o <= '0; o0_1_o <= '0; o0_2_o <= '0; o0_3_o <= '0;
      o1_0_o <= '0; o1_1_o <= '0; o1_2_o <= '0; o1_3_o <= '0;
      o2_0_o <= '0; o2_1_o <= '0; o2_2_o <= '0; o2_3_o <= '0;
    end else begin
      case (state)
        S_IDLE: begin
          if (v_valid_i) begin
            h_n3 <= is_n3_i;
            h_tag <= tag_i;
            h_a[0][0] <= a0_0_i; h_a[0][1] <= a0_1_i;
            h_a[0][2] <= a0_2_i; h_a[0][3] <= a0_3_i;
            h_a[1][0] <= a1_0_i; h_a[1][1] <= a1_1_i;
            h_a[1][2] <= a1_2_i; h_a[1][3] <= a1_3_i;
            h_a[2][0] <= a2_0_i; h_a[2][1] <= a2_1_i;
            h_a[2][2] <= a2_2_i; h_a[2][3] <= a2_3_i;
            for (int l = 0; l < LANES; l++) begin
              n2[l] <= 64'd0;
              zero[l] <= 1'b0;
            end
            root_lane <= 2'd0;
            root_busy <= 1'b0;
            newt_step <= 1'b0;
            sat_rescale_o <= 4'b0;
            rcp0_o <= 4'b0;
            state <= S_SQ0;
          end
        end

        // Law 1: the squares are summed EXACTLY, at full width, and nothing is
        // rescaled until the very end.
        S_SQ0: if (mul_ready_i) state <= S_SQ0W;
        S_SQ0W: if (mul_valid_i) begin
          for (int l = 0; l < LANES; l++) n2[l] <= 64'(prod[l]);
          state <= S_SQ1;
        end

        S_SQ1: if (mul_ready_i) state <= S_SQ1W;
        S_SQ1W: if (mul_valid_i) begin
          for (int l = 0; l < LANES; l++) n2[l] <= n2[l] + 64'(prod[l]);
          state <= h_n3 ? S_SQ2 : S_ROOT;
        end

        S_SQ2: if (mul_ready_i) state <= S_SQ2W;
        S_SQ2W: if (mul_valid_i) begin
          for (int l = 0; l < LANES; l++) n2[l] <= n2[l] + 64'(prod[l]);
          state <= S_ROOT;
        end

        // Law 2: the zero vector is a DEFINED answer. It is detected here
        // rather than at the isqrt, because a zero length would make the
        // normalisation below shift forever.
        S_ROOT: begin
          if (n2[root_lane] == 64'd0) begin
            zero[root_lane] <= 1'b1;
            // THE TWO OPS DISAGREE ABOUT THE LEDGER, and the reference is the
            // one that decides. `zfield::steps::normalize2` bumps RCP0 for the
            // zero vector; `zref::normalize3_approx` returns zeros and bumps
            // NOTHING. Both produce the same values, so only the ledger tells
            // them apart -- which is why this is a wire and not an assumption.
            rcp0_o[root_lane] <= !h_n3;
            mant[root_lane] <= 24'h800000;   // a defined seed index; unused
            expo[root_lane] <= 8'sd0;
            rx[root_lane] <= 32'd0;
            if (root_lane == 2'd3) begin
              root_lane <= 2'd0;
              state <= S_SEED;
            end else begin
              root_lane <= root_lane + 2'd1;
            end
          end else if (isq_valid && isq_ready) begin
            // Taken. Stop asking until the answer comes back.
            root_busy <= 1'b1;
          end else if (isq_rvalid && root_busy) begin
            root_busy <= 1'b0;
            // Law 3: e is PER POINT. top_bit(len) - 23 is the shift that puts
            // the mantissa in [2^23, 2^24), and it is a different number for
            // every lane.
            mant[root_lane] <= 24'((top_bit(isq_r) >= 6'd23)
                                   ? (isq_r >> (top_bit(isq_r) - 6'd23))
                                   : (isq_r << (6'd23 - top_bit(isq_r))));
            expo[root_lane] <= 8'sd0 + 8'(top_bit(isq_r)) - 8'sd23;
            if (root_lane == 2'd3) begin
              root_lane <= 2'd0;
              state <= S_SEED;
            end else begin
              root_lane <= root_lane + 2'd1;
            end
          end
        end

        // ONE CLOCK, AND IT NEEDS TO BE ITS OWN STATE. The seed ROM is
        // combinational on `mant`, and the LAST lane's mantissa is written on
        // the very clock S_ROOT finishes -- so a seed taken during that
        // transition would be one lane stale, and stale by an amount that
        // still produces a plausible reciprocal. Everything is settled here.
        S_SEED: begin
          // The ROM's seed is 31 bits; the iterate is 32 because it GROWS to
          // 2^31. Widening at the load rather than widening the ROM keeps the
          // generated table byte-for-byte what the reference generated.
          for (int l = 0; l < LANES; l++) rx[l] <= {1'b0, seed_val[l]};
          newt_step <= 1'b0;
          state <= S_NEWT_A;
        end

        // The seed, then two Newton steps. The ROM is combinational so the
        // seed is taken on the way into the first multiply.
        S_NEWT_A: if (mul_ready_i) state <= S_NEWT_AW;
        S_NEWT_AW: if (mul_valid_i) state <= S_NEWT_B;

        S_NEWT_B: if (mul_ready_i) state <= S_NEWT_BW;
        S_NEWT_BW: if (mul_valid_i) begin
          if (newt_step == 1'b0) begin
            for (int l = 0; l < LANES; l++) rx[l] <= resc_u30(prod[l]);
            newt_step <= 1'b1;
            state <= S_NEWT_A;
          end else begin
            // THE ITERATE IS NOT THE RECIPROCAL. rcp_u24_norm finishes with
            // `r = rescale_u64_nosat(x, 7)`, clamped to 0xFFFFFF -- a final
            // shift of SEVEN that turns the Q30-ish iterate into the Q24
            // reciprocal the scaling below expects.
            //
            // Leaving it out is a clean factor of 128 on every output, which
            // is exactly what the first run measured: 0x4CCCCD where the
            // reference says 0x999A. A wrong scale is the easiest kind of bug
            // to see and the easiest to leave in, because every value is still
            // smooth and ordered -- only the magnitude is wrong.
            for (int l = 0; l < LANES; l++) rx[l] <= rcp_finish(resc_u30(prod[l]));
            state <= S_OUT0;
          end
        end

        // Law 4: ONE rescale per component, by 31 + e, at the end.
        S_OUT0: if (mul_ready_i) state <= S_OUT0W;
        S_OUT0W: if (mul_valid_i) begin
          o0_0_o <= zero[0] ? 32'sd0 : resc_var(prod[0], out_k[0]);
          o0_1_o <= zero[1] ? 32'sd0 : resc_var(prod[1], out_k[1]);
          o0_2_o <= zero[2] ? 32'sd0 : resc_var(prod[2], out_k[2]);
          o0_3_o <= zero[3] ? 32'sd0 : resc_var(prod[3], out_k[3]);
          for (int l = 0; l < LANES; l++)
            if (!zero[l] && resc_var_fired(prod[l], out_k[l])) sat_rescale_o[l] <= 1'b1;
          state <= S_OUT1;
        end

        S_OUT1: if (mul_ready_i) state <= S_OUT1W;
        S_OUT1W: if (mul_valid_i) begin
          o1_0_o <= zero[0] ? 32'sd0 : resc_var(prod[0], out_k[0]);
          o1_1_o <= zero[1] ? 32'sd0 : resc_var(prod[1], out_k[1]);
          o1_2_o <= zero[2] ? 32'sd0 : resc_var(prod[2], out_k[2]);
          o1_3_o <= zero[3] ? 32'sd0 : resc_var(prod[3], out_k[3]);
          for (int l = 0; l < LANES; l++)
            if (!zero[l] && resc_var_fired(prod[l], out_k[l])) sat_rescale_o[l] <= 1'b1;
          state <= h_n3 ? S_OUT2 : S_DONE;
        end

        S_OUT2: if (mul_ready_i) state <= S_OUT2W;
        S_OUT2W: if (mul_valid_i) begin
          o2_0_o <= zero[0] ? 32'sd0 : resc_var(prod[0], out_k[0]);
          o2_1_o <= zero[1] ? 32'sd0 : resc_var(prod[1], out_k[1]);
          o2_2_o <= zero[2] ? 32'sd0 : resc_var(prod[2], out_k[2]);
          o2_3_o <= zero[3] ? 32'sd0 : resc_var(prod[3], out_k[3]);
          for (int l = 0; l < LANES; l++)
            if (!zero[l] && resc_var_fired(prod[l], out_k[l])) sat_rescale_o[l] <= 1'b1;
          state <= S_DONE;
        end

        S_DONE: begin
          if (!r_valid_o) begin
            // NORMALIZE2 writes no third lane, by the op's own width.
            if (!h_n3) begin
              o2_0_o <= 32'sd0;
              o2_1_o <= 32'sd0;
              o2_2_o <= 32'sd0;
              o2_3_o <= 32'sd0;
            end
            r_valid_o <= 1'b1;
          end else if (r_ready_i) begin
            r_valid_o <= 1'b0;
            state <= S_IDLE;
          end
        end

        default: state <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_field_v3_normalize
