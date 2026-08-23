// zhao_field_len.sv — the Field IR length ops: OP_LEN2, OP_LEN3, OP_DIST2.
//
// A submodule of the FIELD.SEQ.* family. Reference: the interpreter's `len_of`
// (§3.11) and its OP_DIST2 case (reference/src/zfield/zfield_interpret.cpp).
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
//     LEN2  : sqrt( a0^2 + a1^2 )
//     LEN3  : sqrt( a0^2 + a1^2 + a2^2 )
//     DIST2 : d_i = fx_sub(a_i, b_i)  then  sqrt( d0^2 + d1^2 )
//
// with the sum of squares EXACT in u64 and the root the exact integer floor
// (`zhao_field_isqrt`), then a saturating narrow to s32 recorded in the
// `rescale` lane.
//
// Four things are load-bearing:
//
// 1. **THE SUM OF SQUARES IS UNSIGNED AND EXACT.** Each square is up to
//    (2^31)^2 = 2^62, and three of them reach 3*2^62 — which does NOT fit s64
//    but does fit u64. The reference accumulates into a `uint64_t` for exactly
//    that reason. A signed accumulator overflows on three large lanes and the
//    length comes out negative.
// 2. **THE DIFFERENCE IN DIST2 SATURATES FIRST.** It is `fx_sub`, not a plain
//    subtract: `a - b` is formed in s64 and saturated to s32 BEFORE squaring.
//    Squaring an unsaturated difference would give a different distance for far
//    apart points, and the saturation is recorded in the `add` lane.
// 3. **THE ROOT IS A FLOOR, NEVER ROUNDED.** `res^2 <= n < (res+1)^2`. Rounding
//    to nearest would be a better length and would disagree with the software
//    everywhere the root is inexact, which is almost everywhere.
// 4. **THE NARROW SATURATES INTO `rescale`, NOT `mul`.** The reference keeps
//    four ledger lanes apart; a block recording in the wrong one can still
//    return the right number and be wrong everywhere else.
//
// The result of a length is always non-negative, so no sign is reapplied — and
// the saturating narrow only ever hits the TOP rail.
//
// ---------------------------------------------------------------------------
// NEITHER THE SQUARES NOR THE ROOT ARE THIS BLOCK'S SILICON, AS OF 2026-08-23
// ---------------------------------------------------------------------------
// Three squares stood side by side here and a private `zhao_field_isqrt` sat
// beside them, while an identical root sat inside `zhao_field_normalize` and
// neither could ever be busy at the same time — `zhao_field_seq` retires one
// instruction at a time. Under the DSP ruling of 2026-08-23 the squares walk
// `zhao_field_mul` and the root is the engine's ONE `zhao_field_isqrt`, shared
// with NORMALIZE through a mux in `zhao_field_exec_shared`.
//
// THE THREE SQUARES ARE ISSUED BACK TO BACK, not one per handshake, because
// they are independent of one another. The lane accepts an issue every cycle
// and answers two cycles later, so three squares cost five cycles rather than
// nine, and the root then runs its unchanged 34.
//
// A SQUARE IS `v * v` IN SIGNED ARITHMETIC AND THAT IS EXACTLY `|v|^2`. The old
// code took the magnitude first because it accumulated unsigned; sign-extending
// into a 33x33 signed lane gives the identical product for every input
// including INT32_MIN, whose square is 2^62 either way. What is NOT optional is
// that the ACCUMULATOR stays unsigned: three squares reach 3*2^62, which
// overflows s64 and fits u64, which is law 1 above.
//
// THE ACCUMULATOR IS LOADED BY THE FIRST PRODUCT, NEVER ADDED TO. That is what
// keeps one instruction's leftovers out of the next one's length, and it is the
// defect the alone-versus-interleaved test in field_seq_directed hunts for.
module zhao_field_len (
    input logic clk,
    input logic rst_n,

    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic        [ 1:0] mode_i,   // 0 = LEN2, 1 = LEN3, 2 = DIST2
    input  logic signed [31:0] a0_i,
    input  logic signed [31:0] a1_i,
    input  logic signed [31:0] a2_i,
    input  logic signed [31:0] b0_i,
    input  logic signed [31:0] b1_i,

    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] result_o,
    output logic               sat_add_o,      // the DIST2 differences
    output logic               sat_rescale_o,  // the narrow to s32

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

  // M_LEN2 (2'd0) is the fall-through case and so is never named in a
  // comparison; it is documented here rather than declared, because an unused
  // localparam is noise a linter is right to flag.
  localparam logic [1:0] M_LEN3 = 2'd1;
  localparam logic [1:0] M_DIST2 = 2'd2;

  // ---- fx_sub, for the DIST2 lanes ----------------------------------------
  function automatic logic signed [31:0] fx_sub_sat(input logic signed [31:0] a,
                                                    input logic signed [31:0] b);
    logic signed [32:0] d;
    begin
      d = $signed({a[31], a}) - $signed({b[31], b});
      if (d > 33'sd2147483647) fx_sub_sat = 32'sh7FFF_FFFF;
      else if (d < -33'sd2147483648) fx_sub_sat = 32'sh8000_0000;
      else fx_sub_sat = d[31:0];
    end
  endfunction

  function automatic logic fx_sub_fired(input logic signed [31:0] a,
                                        input logic signed [31:0] b);
    logic signed [32:0] d;
    begin
      d = $signed({a[31], a}) - $signed({b[31], b});
      fx_sub_fired = (d > 33'sd2147483647) || (d < -33'sd2147483648);
    end
  endfunction

  // ---- the lanes actually squared -----------------------------------------
  logic signed [31:0] l0, l1, l2;
  logic               sub_sat;
  always_comb begin
    if (mode_i == M_DIST2) begin
      l0 = fx_sub_sat(a0_i, b0_i);
      l1 = fx_sub_sat(a1_i, b1_i);
      l2 = 32'sd0;
      sub_sat = fx_sub_fired(a0_i, b0_i) || fx_sub_fired(a1_i, b1_i);
    end else begin
      l0 = a0_i;
      l1 = a1_i;
      l2 = (mode_i == M_LEN3) ? a2_i : 32'sd0;
      sub_sat = 1'b0;
    end
  end

  // ---- the walk -----------------------------------------------------------
  localparam logic [2:0] L_IDLE = 3'd0;
  localparam logic [2:0] L_GATH = 3'd1;   // issue and collect three squares
  localparam logic [2:0] L_ROOT = 3'd2;   // hand the sum to the shared root
  localparam logic [2:0] L_WAIT = 3'd3;   // hold r_ready until the root answers
  localparam logic [2:0] L_OUT  = 3'd4;

  logic [2:0] state;

  // The three lanes, HELD from accept: the caller is not required to keep its
  // operands on the ports while the root spends 34 cycles on them.
  logic signed [31:0] h_l0, h_l1, h_l2;
  logic               held_sub_sat;

  // Which square is being issued, and how many have landed. Two counters
  // rather than a state each, because issue and collect OVERLAP: the lane
  // takes an issue every cycle and answers two later, so the third issue and
  // the first product are in flight together.
  logic [1:0] iss_cnt, got_cnt;

  logic signed [31:0] sq_sel;
  always_comb begin
    case (iss_cnt)
      2'd0:    sq_sel = h_l0;
      2'd1:    sq_sel = h_l1;
      default: sq_sel = h_l2;
    endcase
  end

  // `v * v`, sign-extended: exactly `|v|^2` for every s32 including INT32_MIN.
  assign mul_issue_o = (state == L_GATH) && (iss_cnt != 2'd3);
  assign mul_a_o = $signed({sq_sel[31], sq_sel});
  assign mul_b_o = $signed({sq_sel[31], sq_sel});

  logic [63:0] n2;

  assign sqrt_valid_o  = (state == L_ROOT);
  assign sqrt_n_o      = n2;
  assign sqrt_rready_o = (state == L_WAIT);

  assign v_ready_o = (state == L_IDLE) && (!r_valid_o || r_ready_i);

  logic over;
  assign over = (sqrt_r_i > 64'd2147483647);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= L_IDLE;
      h_l0 <= '0;
      h_l1 <= '0;
      h_l2 <= '0;
      iss_cnt <= 2'd0;
      got_cnt <= 2'd0;
      n2 <= 64'd0;
      held_sub_sat <= 1'b0;
      r_valid_o <= 1'b0;
      result_o <= '0;
      sat_add_o <= 1'b0;
      sat_rescale_o <= 1'b0;
    end else begin
      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;

      case (state)
        L_IDLE: begin
          if (v_valid_i && v_ready_o) begin
            // The lanes and the difference-saturation flag are decided at
            // accept, where the operands are.
            h_l0 <= l0;
            h_l1 <= l1;
            h_l2 <= l2;
            held_sub_sat <= sub_sat;
            iss_cnt <= 2'd0;
            got_cnt <= 2'd0;
            state <= L_GATH;
          end
        end

        L_GATH: begin
          if (iss_cnt != 2'd3) iss_cnt <= iss_cnt + 2'd1;
          if (mul_valid_i) begin
            // LOADED by the first product, never added to: an accumulator that
            // carried over would put the previous instruction's squares into
            // this length.
            n2 <= (got_cnt == 2'd0) ? mul_p_i[63:0] : (n2 + mul_p_i[63:0]);
            got_cnt <= got_cnt + 2'd1;
            if (got_cnt == 2'd2) state <= L_ROOT;
          end
        end

        L_ROOT: begin
          if (sqrt_ready_i) state <= L_WAIT;
        end

        L_WAIT: begin
          if (sqrt_rvalid_i) begin
            // A length is never negative, so only the TOP rail is reachable.
            result_o <= over ? 32'sh7FFF_FFFF : $signed(sqrt_r_i[31:0]);
            sat_rescale_o <= over;
            sat_add_o <= held_sub_sat;
            r_valid_o <= 1'b1;
            state <= L_OUT;
          end
        end

        L_OUT: begin
          if (r_valid_o && r_ready_i) state <= L_IDLE;
        end

        default: state <= L_IDLE;
      endcase
    end
  end

endmodule : zhao_field_len
