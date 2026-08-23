// zhao_field_normalize.sv — the Field IR normalise ops: OP_NORMALIZE2 and
// OP_NORMALIZE3.
//
// A submodule of the FIELD.SEQ.* family. Reference: the interpreter's
// `normalize2` (§3.12) and `zref::normalize3_approx` (qformats §7.4), which are
// the same shape and are implemented here as one datapath over two or three
// lanes.
//
// The seed table lives in `zhao_field_rcp24_rom.sv`, GENERATED from
// `zref_tables.hpp`. It is the SECOND reciprocal table in this engine and it is
// NOT the one `zhao_field_rcp` uses — `FIELD_RCP_T0` seeds a 32-bit reciprocal
// with one correction step, `RCP24_T0` seeds a 24-bit one with two. Feeding
// either function the other's table would be invisible until some normalised
// vector came out slightly short.
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
//     n2  = sum of squares, EXACT and unsigned
//     n2 == 0                    -> all lanes zero (see the asymmetry below)
//     len = isqrt_u64(n2)                       exact floor
//     normalise len into [2^23, 2^24), counting e
//     r   = rcp_u24_norm(m)                     TWO correction steps
//     out = rescale_s32(lane * r, 31 + e)       ONE rounding per lane
//
// Five things are load-bearing:
//
// 1. **ONE ROUNDING PER LANE.** `lane * r` is exact in s64 and rescaled once by
//    `31 + e`. The shift is a function of the vector's magnitude, not a
//    constant, which is what keeps the result accurate across the whole range —
//    and it is why `e` has to survive from the normalisation loop to the final
//    rescale rather than being folded away early.
// 2. **TWO CORRECTION STEPS, NOT ONE.** `rcp_u24_norm` iterates twice.
//    `field_rcp` next door iterates ONCE. The two functions are different and
//    the count is not a tuning knob.
// 3. **THE ZERO CASE IS ASYMMETRIC BETWEEN THE TWO OPS**, and this is the one
//    that looks like a bug and is not. `normalize2` bumps the `rcp0` ledger lane
//    on a zero vector; `normalize3_approx` returns zeros and bumps NOTHING. The
//    reference really does differ, so the RTL differs too, and the test pins
//    both. Making them consistent would be tidier and would disagree with every
//    capture the software has produced.
// 4. **THE SUM OF SQUARES IS UNSIGNED.** Three squares reach 3*2^62, which
//    overflows s64 and fits u64. The reference accumulates `normalize3`'s in a
//    128-bit unsigned type and hands it to a u64 root; the value always fits.
// 5. **THE ROOT IS A FLOOR.** Shared with `zhao_field_len`, same block, same
//    exactness argument.
//
// ---------------------------------------------------------------------------
// TEN PRODUCTS AND A SHARED ROOT, AS OF 2026-08-23
// ---------------------------------------------------------------------------
// This was the most expensive block in the engine: three squares, four
// reciprocal-correction products and three output-lane products, ten
// multipliers standing side by side, plus a second copy of the exact integer
// root that `zhao_field_len` also owned. Under the DSP ruling of 2026-08-23 all
// ten products walk `zhao_field_mul` and the root is the engine's ONE
// `zhao_field_isqrt`.
//
// THE SCHEDULE, and where the parallelism actually is:
//
//   squares    three issues back to back      independent -> 5 clocks
//   root       the unchanged restoring walk                  34 clocks
//   reciprocal four issues, each dependent on the last    -> 4 x 3 clocks
//   lanes      three issues back to back      independent -> 5 clocks
//
// The four reciprocal steps are a genuine chain -- the second correction reads
// what the first produced -- so each pays the lane's full two-cycle latency.
// The squares and the output lanes are not, so they do not.
//
// SIX OF THE TEN OPERANDS ARE UNSIGNED AND ONE IS NEARLY 33 BITS. `m` is u24,
// the seed is u31, and the corrected `x` reaches 2^31 -- 32 bits, which
// zero-extends into the lane's 33 signed bits and stays positive. That bound is
// not an observation about the table: for ANY seed, x*(2 - m*x/2^54) is
// maximised at x = 2^54/m, and m >= 2^23 puts the maximum at 2^31. The lane is
// 33 bits wide so that this fits without a split.
//
// THE ACCUMULATOR IS LOADED BY THE FIRST SQUARE, NEVER ADDED TO, for the same
// reason it is in `zhao_field_len`: a sum left over from the previous
// instruction would be invisible in every test that runs one op at a time.
module zhao_field_normalize (
    input logic clk,
    input logic rst_n,

    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic               is3_i,      // 0 = NORMALIZE2, 1 = NORMALIZE3
    input  logic signed [31:0] a0_i,
    input  logic signed [31:0] a1_i,
    input  logic signed [31:0] a2_i,

    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] o0_o,
    output logic signed [31:0] o1_o,
    output logic signed [31:0] o2_o,
    output logic               rcp0_o,        // set only by NORMALIZE2, see law 3
    output logic               sat_rescale_o,

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
    input  logic               mul_valid_i,

    // ---- the shared integer square root, `zhao_field_isqrt` ----------------
    output logic        sqrt_valid_o,
    input  logic        sqrt_ready_i,
    output logic [63:0] sqrt_n_o,
    input  logic        sqrt_rvalid_i,
    output logic        sqrt_rready_o,
    input  logic [63:0] sqrt_r_i
);

  localparam logic [3:0] N_IDLE = 4'd0;
  localparam logic [3:0] N_GATH = 4'd1;    // three squares, issued and collected
  localparam logic [3:0] N_ROOT = 4'd2;    // hand the sum to the shared root
  localparam logic [3:0] N_WAIT = 4'd3;    // hold r_ready until the root answers
  localparam logic [3:0] N_R0 = 4'd4;      // p0 = m * x0
  localparam logic [3:0] N_R0W = 4'd5;
  localparam logic [3:0] N_R1 = 4'd6;      // x1 = rescale_u(x0 * (2^31 - w0), 30)
  localparam logic [3:0] N_R1W = 4'd7;
  localparam logic [3:0] N_R2 = 4'd8;      // p1 = m * x1
  localparam logic [3:0] N_R2W = 4'd9;
  localparam logic [3:0] N_R3 = 4'd10;     // x2 = rescale_u(x1 * (2^31 - w1), 30)
  localparam logic [3:0] N_R3W = 4'd11;
  localparam logic [3:0] N_LANE = 4'd12;   // three output products
  localparam logic [3:0] N_OUT = 4'd13;

  logic [3:0] state;

  // ---- what the walk holds from accept -------------------------------------
  logic signed [31:0] h_a0, h_a1, h_a2;
  logic               h_is3;

  logic [63:0] n2;
  logic        is_zero;
  assign is_zero = (n2 == 64'd0);

  // ---- the three squares ----------------------------------------------------
  // `v * v` sign-extended is exactly `|v|^2`, INT32_MIN included. The sum stays
  // UNSIGNED because three squares reach 3*2^62 (law 4).
  logic [1:0] iss_cnt, got_cnt;

  logic signed [31:0] sq_sel;
  always_comb begin
    case (iss_cnt)
      2'd0:    sq_sel = h_a0;
      2'd1:    sq_sel = h_a1;
      default: sq_sel = h_a2;
    endcase
  end

  // ---- the root's answer, held ---------------------------------------------
  logic [63:0] h_rt;

  // ---- normalise the length into [2^23, 2^24), counting e -----------------
  //
  // THIS WAS THE WHOLE DESIGN'S CRITICAL PATH. Measured 2026-08-23, the first
  // timing analysis ever run on this block: the three worst setup paths were all
  // `h_rt -> o1_o`, at SEVENTY-EIGHT LEVELS OF LOGIC in one cycle, and the Field
  // engine closed at 8.59 MHz against a 10 ns clock.
  //
  // The cause was here, and the comment that used to sit on these lines already
  // said so without anyone acting on it: "the loops become a leading-zero count
  // either way". They were written as the reference writes them --
  //
  //     while (m < (1<<23)) { m <<= 1; --e; }
  //     while (m >= (1<<24)) { m >>= 1; ++e; }
  //
  // -- and a `for (int k = 0; k < 64; ...)` in RTL is not a loop, it is 64 COPIES.
  // Two of them unrolled into 128 dependent compare-and-shift stages on a 64-bit
  // value, feeding both the seed ROM index and the per-lane rescale shift.
  //
  // ---------------------------------------------------------------------------
  // THE CLOSED FORM, AND WHY IT IS THE SAME NUMBER
  // ---------------------------------------------------------------------------
  // `n2 == 0` is handled before this is reached (the oracle returns the zero
  // vector and so does the walk), so `len` is at least 1 and has a most
  // significant set bit. Call its index `n`.
  //
  //   * the FIRST loop runs only when n < 23, and stops the first time the value
  //     reaches 2^23 -- which is after exactly 23 - n doublings, leaving the MSB
  //     at bit 23;
  //   * the SECOND loop runs only when n >= 24, and stops the first time the
  //     value drops below 2^24 -- after exactly n - 23 halvings, again leaving
  //     the MSB at bit 23;
  //   * when n == 23 neither runs.
  //
  // So in every case the result has its MSB at bit 23, and `e` is `n - 23`:
  // negative below, positive above, zero at. And repeated TRUNCATING halvings
  // compose exactly -- floor(floor(x/2)/2) == floor(x/4) -- so the second loop
  // is one truncating right shift, not an approximation of one.
  //
  //     e     = n - 23
  //     m     = (n >= 23) ? (len >> (n - 23)) : (len << (23 - n))
  //
  // With `lz` the leading-zero count of the 64-bit value, n = 63 - lz, so
  // e = 40 - lz and the two shifts are a barrel shift by at most 40. That is a
  // handful of levels instead of 128, and it is bit-identical rather than close:
  // there is no rounding anywhere in this function to get wrong.
  //
  // The leading-zero count is the SAME SIX-STAGE BINARY SEARCH `zhao_field_rcp`
  // uses forty lines away, which is the other half of why this was a defect
  // rather than a trade-off -- the correct shape was already in the file.
  // ENFORCED-BY: tests/differential/field_normalize_directed.cpp:main
  logic [ 5:0] lz;
  logic [63:0] lz_t;
  always_comb begin
    lz = 6'd0;
    lz_t = h_rt;
    if (h_rt != 64'd0) begin
      if (lz_t[63:32] == 32'd0) begin lz = lz + 6'd32; lz_t = lz_t << 32; end
      if (lz_t[63:48] == 16'd0) begin lz = lz + 6'd16; lz_t = lz_t << 16; end
      if (lz_t[63:56] == 8'd0)  begin lz = lz + 6'd8;  lz_t = lz_t << 8;  end
      if (lz_t[63:60] == 4'd0)  begin lz = lz + 6'd4;  lz_t = lz_t << 4;  end
      if (lz_t[63:62] == 2'd0)  begin lz = lz + 6'd2;  lz_t = lz_t << 2;  end
      if (lz_t[63] == 1'b0)     begin lz = lz + 6'd1;  lz_t = lz_t << 1;  end
    end
  end

  logic signed [7:0] e_val;
  logic [23:0] m_val;
  logic signed [7:0] d_exp;
  logic [ 5:0] rsh, lsh;
  always_comb begin
    // e = 40 - lz, which is n - 23 written in terms of leading zeros.
    d_exp = 8'sd40 - $signed({2'd0, lz});
    // Exactly one of these is ever nonzero, so the pair below is a single
    // shift in whichever direction the exponent asks for.
    rsh = (d_exp > 8'sd0) ? 6'(d_exp) : 6'd0;
    lsh = (d_exp < 8'sd0) ? 6'(-d_exp) : 6'd0;
  end

  assign m_val = (h_rt == 64'd0) ? 24'd0 : 24'((h_rt >> rsh) << lsh);
  assign e_val = (h_rt == 64'd0) ? 8'sd0 : d_exp;

  /* verilator lint_off UNUSEDSIGNAL */
  logic [63:0] lz_t_unused;
  /* verilator lint_on UNUSEDSIGNAL */
  assign lz_t_unused = lz_t;

  // ---- rcp_u24_norm: seed, TWO correction steps, then rescale by 7 --------
  logic [ 7:0] idx;
  logic [30:0] seed;
  assign idx = m_val[22:15];  // (m - 2^23) >> 15, with bit 23 known set

  zhao_field_rcp24_rom u_rom (
      .idx_i(idx),
      .seed_o(seed)
  );

  function automatic logic [63:0] resc_u(input logic [63:0] v, input int unsigned k);
    resc_u = (k == 0) ? v : ((v + (64'd1 << (k - 1))) >> k);
  endfunction

  // The correction chain, one register per step so that each product is read
  // exactly once and by the state that asked for it.
  logic [63:0] w0, w1;
  logic [31:0] x1_q;
  logic [23:0] r24;

  // `2^31 - w` is at most 2^31 and so needs 32 unsigned bits; the lane sees it
  // zero-extended and therefore positive.
  logic [31:0] corr0, corr1;
  assign corr0 = 32'((64'd2 << 30) - w0);
  assign corr1 = 32'((64'd2 << 30) - w1);

  // ---- one rescale per lane, by 31 + e ------------------------------------
  logic [7:0] shift_amt;
  assign shift_amt = 8'(8'sd31 + e_val);

  function automatic logic signed [31:0] resc_s(input logic signed [63:0] v,
                                                input logic [7:0] k);
    logic signed [64:0] r;
    begin
      r = (k == 8'd0) ? 65'(v) : ((65'(v) + (65'sd1 <<< (k - 8'd1))) >>> k);
      if (r > 65'sd2147483647) resc_s = 32'sh7FFF_FFFF;
      else if (r < -65'sd2147483648) resc_s = 32'sh8000_0000;
      else resc_s = r[31:0];
    end
  endfunction

  function automatic logic resc_s_fired(input logic signed [63:0] v, input logic [7:0] k);
    logic signed [64:0] r;
    begin
      r = (k == 8'd0) ? 65'(v) : ((65'(v) + (65'sd1 <<< (k - 8'd1))) >>> k);
      resc_s_fired = (r > 65'sd2147483647) || (r < -65'sd2147483648);
    end
  endfunction

  // ---- the output lanes, issued back to back -------------------------------
  logic [1:0] lane_iss, lane_got;

  logic signed [31:0] lane_sel;
  always_comb begin
    case (lane_iss)
      2'd0:    lane_sel = h_a0;
      2'd1:    lane_sel = h_a1;
      default: lane_sel = h_a2;
    endcase
  end

  // ---- the shared lane's request -------------------------------------------
  always_comb begin
    mul_issue_o = 1'b0;
    mul_a_o = '0;
    mul_b_o = '0;
    case (state)
      N_GATH: begin
        mul_issue_o = (iss_cnt != 2'd3);
        mul_a_o = $signed({sq_sel[31], sq_sel});
        mul_b_o = $signed({sq_sel[31], sq_sel});
      end
      N_R0: begin
        mul_issue_o = 1'b1;
        mul_a_o = $signed({9'd0, m_val});
        mul_b_o = $signed({2'd0, seed});
      end
      N_R1: begin
        mul_issue_o = 1'b1;
        mul_a_o = $signed({2'd0, seed});
        mul_b_o = $signed({1'b0, corr0});
      end
      N_R2: begin
        mul_issue_o = 1'b1;
        mul_a_o = $signed({9'd0, m_val});
        mul_b_o = $signed({1'b0, x1_q});
      end
      N_R3: begin
        mul_issue_o = 1'b1;
        mul_a_o = $signed({1'b0, x1_q});
        mul_b_o = $signed({1'b0, corr1});
      end
      N_LANE: begin
        mul_issue_o = (lane_iss != 2'd3);
        mul_a_o = $signed({lane_sel[31], lane_sel});
        mul_b_o = $signed({9'd0, r24});
      end
      default: begin
        mul_issue_o = 1'b0;
        mul_a_o = '0;
        mul_b_o = '0;
      end
    endcase
  end

  assign sqrt_valid_o  = (state == N_ROOT);
  assign sqrt_n_o      = n2;
  assign sqrt_rready_o = (state == N_WAIT);

  assign v_ready_o = (state == N_IDLE) && (!r_valid_o || r_ready_i);

  // The product the lane is answering with, as a signed s64: every value on
  // this path fits, and the widths are argued in the header.
  logic signed [63:0] p_signed;
  logic        [63:0] p_unsigned;
  assign p_signed   = $signed(mul_p_i[63:0]);
  assign p_unsigned = mul_p_i[63:0];

  logic [31:0] r24_next;
  always_comb begin
    logic [63:0] x2;
    x2 = resc_u(p_unsigned, 30);
    r24_next = 32'(resc_u(x2, 7));
    if (r24_next > 32'h00FF_FFFF) r24_next = 32'h00FF_FFFF;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= N_IDLE;
      h_a0 <= '0; h_a1 <= '0; h_a2 <= '0;
      h_is3 <= 1'b0;
      n2 <= 64'd0;
      h_rt <= 64'd0;
      iss_cnt <= 2'd0;
      got_cnt <= 2'd0;
      lane_iss <= 2'd0;
      lane_got <= 2'd0;
      w0 <= 64'd0;
      w1 <= 64'd0;
      x1_q <= 32'd0;
      r24 <= 24'd0;
      r_valid_o <= 1'b0;
      o0_o <= '0; o1_o <= '0; o2_o <= '0;
      rcp0_o <= 1'b0;
      sat_rescale_o <= 1'b0;
    end else begin
      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;

      case (state)
        N_IDLE: begin
          if (v_valid_i && v_ready_o) begin
            h_a0 <= a0_i;
            h_a1 <= a1_i;
            // NORMALIZE2's third lane is zero, so its square is zero and the
            // sum is the two-lane one. The width, not the value, is what keeps
            // the third OUTPUT lane from being written; that is the
            // sequencer's job.
            h_a2 <= is3_i ? a2_i : 32'sd0;
            h_is3 <= is3_i;
            iss_cnt <= 2'd0;
            got_cnt <= 2'd0;
            state <= N_GATH;
          end
        end

        N_GATH: begin
          if (iss_cnt != 2'd3) iss_cnt <= iss_cnt + 2'd1;
          if (mul_valid_i) begin
            n2 <= (got_cnt == 2'd0) ? p_unsigned : (n2 + p_unsigned);
            got_cnt <= got_cnt + 2'd1;
            if (got_cnt == 2'd2) state <= N_ROOT;
          end
        end

        N_ROOT: begin
          if (sqrt_ready_i) state <= N_WAIT;
        end

        N_WAIT: begin
          if (sqrt_rvalid_i) begin
            h_rt <= sqrt_r_i;
            if (is_zero) begin
              o0_o <= '0; o1_o <= '0; o2_o <= '0;
              // Law 3: NORMALIZE2 records rcp0 on a zero vector, NORMALIZE3
              // does not. The reference really is asymmetric here.
              rcp0_o <= !h_is3;
              sat_rescale_o <= 1'b0;
              r_valid_o <= 1'b1;
              state <= N_OUT;
            end else begin
              state <= N_R0;
            end
          end
        end

        // The two correction steps of `rcp_u24_norm`, four products, each one
        // dependent on the one before it.
        N_R0: state <= N_R0W;
        N_R0W: if (mul_valid_i) begin
          w0 <= p_unsigned >> 24;
          state <= N_R1;
        end

        N_R1: state <= N_R1W;
        N_R1W: if (mul_valid_i) begin
          x1_q <= 32'(resc_u(p_unsigned, 30));
          state <= N_R2;
        end

        N_R2: state <= N_R2W;
        N_R2W: if (mul_valid_i) begin
          w1 <= p_unsigned >> 24;
          state <= N_R3;
        end

        N_R3: state <= N_R3W;
        N_R3W: if (mul_valid_i) begin
          r24 <= r24_next[23:0];
          lane_iss <= 2'd0;
          lane_got <= 2'd0;
          rcp0_o <= 1'b0;
          sat_rescale_o <= 1'b0;
          state <= N_LANE;
        end

        // Three independent products, issued on consecutive cycles, each
        // rescaled once by 31 + e as it lands (law 1).
        N_LANE: begin
          if (lane_iss != 2'd3) lane_iss <= lane_iss + 2'd1;
          if (mul_valid_i) begin
            lane_got <= lane_got + 2'd1;
            case (lane_got)
              2'd0: o0_o <= resc_s(p_signed, shift_amt);
              2'd1: o1_o <= resc_s(p_signed, shift_amt);
              default: o2_o <= h_is3 ? resc_s(p_signed, shift_amt) : 32'sd0;
            endcase
            if (!(lane_got == 2'd2) || h_is3) begin
              sat_rescale_o <= sat_rescale_o || resc_s_fired(p_signed, shift_amt);
            end
            if (lane_got == 2'd2) begin
              r_valid_o <= 1'b1;
              state <= N_OUT;
            end
          end
        end

        N_OUT: begin
          if (r_valid_o && r_ready_i) state <= N_IDLE;
        end

        default: state <= N_IDLE;
      endcase
    end
  end

endmodule : zhao_field_normalize
