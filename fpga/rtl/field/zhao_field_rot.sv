// zhao_field_rot.sv — the Field IR rotation ops: OP_ROT2 and OP_ROT3.
//
// A submodule of the FIELD.SEQ.* family. Reference: the interpreter's OP_ROT2
// and OP_ROT3 cases (reference/src/zfield/zfield_interpret.cpp), over
// `zref::fx_sin` / `zref::fx_cos` (zref_trig.hpp).
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
//     ang = reg[b] & 0xFFFF          the LOW HALF only
//     c   = fx_cos(ang),  s = fx_sin(ang)
//
//     p' = fx_sub(fx_mul(c, p), fx_mul(s, q))
//     q' = fx_add(fx_mul(s, p), fx_mul(c, q))
//
// with (p, q) chosen by the op, and the third lane CARRIED THROUGH unchanged:
//
//     ROT2         p = a0, q = a1                      (two lanes only)
//     ROT3 imm=0   p = a1, q = a2,  a0 passes through   (X axis)
//     ROT3 imm=1   p = a2, q = a0,  a1 passes through   (Y axis)
//     ROT3 else    p = a0, q = a1,  a2 passes through   (Z axis)
//
// Five things are load-bearing:
//
// 1. **EACH PRODUCT IS ROUNDED SEPARATELY.** This is the one that looks like a
//    defect and is the law. `fx_sub(fx_mul(c,p), fx_mul(s,q))` rounds TWICE —
//    once per product — and then saturates the difference. Everywhere else in
//    this design (A3b: `mat4_vec4`, `fx_mad`, GEOM.SKIN) a row of products is
//    summed exactly and rescaled ONCE, because double rounding is the bug. Here
//    the reference does the opposite, and a "corrected" fused version disagrees
//    by an LSB on a large fraction of inputs.
//
//    The rule is not "single rounding is always right"; it is "match the
//    reference". An implementation improved into the house style is wrong.
//
// 2. **THE ANGLE IS THE LOW SIXTEEN BITS**, `reg[b] & 0xFFFF`. The upper half
//    is ignored rather than being an error, so a caller that leaves rubbish
//    there gets a defined answer and the same one the software gives.
//
// 3. **ONE ANGLE FEEDS BOTH FUNCTIONS.** `c` and `s` come from the same
//    `ang`; `fx_cos(a)` is `fx_sin(a + 0x4000)` with the add WRAPPING in
//    sixteen bits, which is the sine block's own law and not re-derived here.
//
// 4. **THE PASS-THROUGH LANE IS COPIED, NOT COMPUTED.** The axis lane is the
//    input value bit for bit — not `x * cos(0)`, which would be a different
//    number whenever the rounding of a unit multiply is not exact.
//
// 5. **ROT2 IS ROT3'S Z CASE ON TWO LANES.** They share this datapath entirely.
//    ROT2 simply never writes a third lane; `o2_o` reads zero for it, and the
//    sequencer writes only what the op declares.
//
// ---------------------------------------------------------------------------
// ONE MULTIPLIER, ONE SINE TABLE
// ---------------------------------------------------------------------------
// Four 32x32 products and two table lookups, taken one at a time: the sine
// block is combinational, so it is WALKED (cos, then sin) rather than
// instantiated twice, which would put a second 257x17 ROM on the die to save
// one cycle. Same trade as `zhao_field_noise` and `zhao_geom_mat3x4_mul`.
module zhao_field_rot (
    input logic clk,
    input logic rst_n,

    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic               is_rot3_i,  // 0 = ROT2, 1 = ROT3
    input  logic        [ 1:0] axis_i,     // ROT3 only: 0 = X, 1 = Y, else Z
    // Law 2: the angle is `reg[b] & 0xFFFF`. The upper half is IGNORED, not
    // rejected -- a caller that leaves rubbish there gets a defined answer, and
    // the same one the software gives. The waiver is the law, not a silencer.
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic signed [31:0] ang_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  logic signed [31:0] a0_i,
    input  logic signed [31:0] a1_i,
    input  logic signed [31:0] a2_i,

    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] o0_o,
    output logic signed [31:0] o1_o,
    output logic signed [31:0] o2_o,       // zero for ROT2 (law 5)
    output logic               sat_add_o,
    output logic               sat_mul_o,

    // ---- the shared sine table, `zhao_field_sin` --------------------------
    // Law 3 said "one table, walked". As of 2026-08-23 it is not even this
    // block's table: the engine has ONE sine instance and OP_SIN, OP_COS and
    // both of this block's reads take turns on it. The table is combinational,
    // so borrowing it costs nothing and the walk is unchanged.
    output logic        [15:0] sin_angle_o,
    output logic               sin_is_cos_o,
    input  logic signed [31:0] sin_result_i,

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

  // Each product is an ISSUE state and a WAIT state, because the shared lane
  // answers two cycles after it is asked. The four products are independent of
  // one another and COULD be issued back to back; they are not, because law 1
  // rescales each one separately and the sequential form keeps each rescale
  // beside the product it belongs to. Four extra clocks on a per-sample op.
  localparam logic [3:0] R_IDLE = 4'd0;
  localparam logic [3:0] R_COS = 4'd1;
  localparam logic [3:0] R_SIN = 4'd2;
  localparam logic [3:0] R_CP = 4'd3;   // c*p
  localparam logic [3:0] R_CPW = 4'd4;
  localparam logic [3:0] R_SQ = 4'd5;   // s*q  -> p'
  localparam logic [3:0] R_SQW = 4'd6;
  localparam logic [3:0] R_SP = 4'd7;   // s*p
  localparam logic [3:0] R_SPW = 4'd8;
  localparam logic [3:0] R_CQ = 4'd9;   // c*q  -> q'
  localparam logic [3:0] R_CQW = 4'd10;
  localparam logic [3:0] R_OUT = 4'd11;

  logic [3:0] state;

  logic               h_rot3;
  logic        [ 1:0] h_axis;
  logic        [15:0] h_ang;
  logic signed [31:0] h_a0, h_a1, h_a2;
  logic signed [31:0] c_val, s_val;
  logic signed [31:0] t_cp, t_sp;

  // Law 5: ROT2 is the Z case. The lane choice is written once for both ops.
  logic signed [31:0] lane_p, lane_q;
  always_comb begin
    if (h_rot3 && (h_axis == 2'd0)) begin      // X
      lane_p = h_a1;
      lane_q = h_a2;
    end else if (h_rot3 && (h_axis == 2'd1)) begin  // Y
      lane_p = h_a2;
      lane_q = h_a0;
    end else begin                             // Z, and every ROT2
      lane_p = h_a0;
      lane_q = h_a1;
    end
  end

  // ---- the shared sine table, WALKED (law 3) -----------------------------
  logic               trig_is_cos;
  logic signed [31:0] trig_out;
  assign trig_is_cos = (state == R_COS);

  assign sin_angle_o = h_ang;
  assign sin_is_cos_o = trig_is_cos;
  assign trig_out = sin_result_i;

  // ---- the shared multiplier's operands ----------------------------------
  logic signed [31:0] mul_a, mul_b;
  logic signed [63:0] mul_p;
  assign mul_p = $signed(mul_p_i[63:0]);

  always_comb begin
    case (state)
      R_CP: begin
        mul_a = c_val;
        mul_b = lane_p;
      end
      R_SQ: begin
        mul_a = s_val;
        mul_b = lane_q;
      end
      R_SP: begin
        mul_a = s_val;
        mul_b = lane_p;
      end
      default: begin
        mul_a = c_val;
        mul_b = lane_q;
      end
    endcase
  end

  // The request. Both operands are s32 and SIGN-extend into the lane's 33 bits.
  assign mul_issue_o = (state == R_CP) || (state == R_SQ) ||
                       (state == R_SP) || (state == R_CQ);
  assign mul_a_o = $signed({mul_a[31], mul_a});
  assign mul_b_o = $signed({mul_b[31], mul_b});

  // Law 1: ONE rescale PER PRODUCT. Not one per row.
  function automatic logic signed [31:0] resc16(input logic signed [63:0] v);
    logic signed [64:0] r;
    begin
      r = (65'(v) + 65'sd32768) >>> 16;
      if (r > 65'sd2147483647) resc16 = 32'sh7FFF_FFFF;
      else if (r < -65'sd2147483648) resc16 = 32'sh8000_0000;
      else resc16 = r[31:0];
    end
  endfunction

  function automatic logic resc16_fired(input logic signed [63:0] v);
    logic signed [64:0] r;
    begin
      r = (65'(v) + 65'sd32768) >>> 16;
      resc16_fired = (r > 65'sd2147483647) || (r < -65'sd2147483648);
    end
  endfunction

  function automatic logic signed [31:0] add_sat(input logic signed [31:0] a,
                                                 input logic signed [31:0] b);
    logic signed [32:0] t;
    begin
      t = $signed({a[31], a}) + $signed({b[31], b});
      if (t > 33'sd2147483647) add_sat = 32'sh7FFF_FFFF;
      else if (t < -33'sd2147483648) add_sat = 32'sh8000_0000;
      else add_sat = t[31:0];
    end
  endfunction

  function automatic logic signed [31:0] sub_sat(input logic signed [31:0] a,
                                                 input logic signed [31:0] b);
    logic signed [32:0] t;
    begin
      t = $signed({a[31], a}) - $signed({b[31], b});
      if (t > 33'sd2147483647) sub_sat = 32'sh7FFF_FFFF;
      else if (t < -33'sd2147483648) sub_sat = 32'sh8000_0000;
      else sub_sat = t[31:0];
    end
  endfunction

  function automatic logic add_fired(input logic signed [31:0] a, input logic signed [31:0] b);
    logic signed [32:0] t;
    begin
      t = $signed({a[31], a}) + $signed({b[31], b});
      add_fired = (t > 33'sd2147483647) || (t < -33'sd2147483648);
    end
  endfunction

  function automatic logic sub_fired(input logic signed [31:0] a, input logic signed [31:0] b);
    logic signed [32:0] t;
    begin
      t = $signed({a[31], a}) - $signed({b[31], b});
      sub_fired = (t > 33'sd2147483647) || (t < -33'sd2147483648);
    end
  endfunction

  logic signed [31:0] out_p, out_q;

  assign v_ready_o = (state == R_IDLE);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= R_IDLE;
      h_rot3 <= 1'b0;
      h_axis <= 2'd0;
      h_ang <= 16'd0;
      h_a0 <= '0;
      h_a1 <= '0;
      h_a2 <= '0;
      c_val <= '0;
      s_val <= '0;
      t_cp <= '0;
      t_sp <= '0;
      out_p <= '0;
      out_q <= '0;
      o0_o <= '0;
      o1_o <= '0;
      o2_o <= '0;
      r_valid_o <= 1'b0;
      sat_add_o <= 1'b0;
      sat_mul_o <= 1'b0;
    end else begin
      case (state)
        R_IDLE: begin
          if (v_valid_i) begin
            h_rot3 <= is_rot3_i;
            h_axis <= axis_i;
            // Law 2: the LOW HALF only. The rest is ignored, not rejected.
            h_ang  <= ang_i[15:0];
            h_a0 <= a0_i;
            h_a1 <= a1_i;
            h_a2 <= a2_i;
            sat_add_o <= 1'b0;
            sat_mul_o <= 1'b0;
            state <= R_COS;
          end
        end

        // Law 3: one table, walked. Both values come from the SAME angle.
        R_COS: begin
          c_val <= trig_out;
          state <= R_SIN;
        end

        R_SIN: begin
          s_val <= trig_out;
          state <= R_CP;
        end

        R_CP: state <= R_CPW;
        R_CPW: begin
          if (mul_valid_i) begin
            t_cp <= resc16(mul_p);
            sat_mul_o <= sat_mul_o || resc16_fired(mul_p);
            state <= R_SQ;
          end
        end

        R_SQ: state <= R_SQW;
        R_SQW: begin
          if (mul_valid_i) begin
            // p' = fx_sub(c*p, s*q) -- two roundings already done, then ONE
            // saturating subtract. Fusing these is the natural "improvement" and
            // is a different number.
            out_p <= sub_sat(t_cp, resc16(mul_p));
            sat_mul_o <= sat_mul_o || resc16_fired(mul_p);
            sat_add_o <= sat_add_o || sub_fired(t_cp, resc16(mul_p));
            state <= R_SP;
          end
        end

        R_SP: state <= R_SPW;
        R_SPW: begin
          if (mul_valid_i) begin
            t_sp <= resc16(mul_p);
            sat_mul_o <= sat_mul_o || resc16_fired(mul_p);
            state <= R_CQ;
          end
        end

        R_CQ: state <= R_CQW;
        R_CQW: begin
          if (mul_valid_i) begin
            out_q <= add_sat(t_sp, resc16(mul_p));
            sat_mul_o <= sat_mul_o || resc16_fired(mul_p);
            sat_add_o <= sat_add_o || add_fired(t_sp, resc16(mul_p));
            state <= R_OUT;
          end
        end

        R_OUT: begin
          if (!r_valid_o) begin
            // Law 4: the axis lane is COPIED. Not multiplied by a unit cosine.
            if (!h_rot3) begin
              o0_o <= out_p;
              o1_o <= out_q;
              o2_o <= 32'sd0;         // law 5: ROT2 writes two lanes
            end else if (h_axis == 2'd0) begin       // X
              o0_o <= h_a0;
              o1_o <= out_p;
              o2_o <= out_q;
            end else if (h_axis == 2'd1) begin       // Y
              o0_o <= out_q;
              o1_o <= h_a1;
              o2_o <= out_p;
            end else begin                            // Z
              o0_o <= out_p;
              o1_o <= out_q;
              o2_o <= h_a2;
            end
            r_valid_o <= 1'b1;
          end else if (r_ready_i) begin
            r_valid_o <= 1'b0;
            state <= R_IDLE;
          end
        end

        default: state <= R_IDLE;
      endcase
    end
  end

endmodule : zhao_field_rot
