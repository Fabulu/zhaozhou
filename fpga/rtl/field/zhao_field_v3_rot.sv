// zhao_field_v3_rot.sv — OP_ROT2 and OP_ROT3 for four points at once, on the
// shared four-wide multiplier bank and a PRIVATE sine table.
//
// ENFORCED-BY: tests/differential/field_v3_rot_directed.cpp:main
//
// ---------------------------------------------------------------------------
// THE LAW, unchanged from zhao_field_rot and from the reference
// ---------------------------------------------------------------------------
//     ang = angle & 0xFFFF          the LOW HALF only; the upper half is
//                                   IGNORED, not rejected
//     c   = fx_cos(ang),  s = fx_sin(ang)
//
//     p' = fx_sub(fx_mul(c, p), fx_mul(s, q))
//     q' = fx_add(fx_mul(s, p), fx_mul(c, q))
//
//     ROT2         p = a0, q = a1                      (two lanes only)
//     ROT3 imm=0   p = a1, q = a2,  a0 passes through   (X axis)
//     ROT3 imm=1   p = a2, q = a0,  a1 passes through   (Y axis)
//     ROT3 else    p = a0, q = a1,  a2 passes through   (Z axis)
//
// EACH PRODUCT IS ROUNDED SEPARATELY. This is the one that looks like a defect
// and is the law: `fx_sub(fx_mul(c,p), fx_mul(s,q))` rounds TWICE, once per
// product, and then saturates the difference. Everywhere else in this design a
// row of products is summed exactly and rescaled ONCE, because double rounding
// is the bug. Here the reference does the opposite, and a "corrected" fused
// version disagrees by an LSB on a large fraction of inputs. The rule is not
// "single rounding is always right"; it is "match the reference".
//
// THE PASS-THROUGH LANE IS COPIED, NOT COMPUTED. The axis lane is the input
// value bit for bit -- not `x * cos(0)`, which is a different number whenever
// the rounding of a unit multiply is not exact.
//
// ---------------------------------------------------------------------------
// THE SINE TABLE IS PRIVATE, AND THAT IS THE OPPOSITE OF THE MULTIPLIER
// ---------------------------------------------------------------------------
// The v2 unit BORROWS the engine's one sine instance. This one owns a copy,
// and the reasoning is in reports/FIELD_V3_REMAINING_OPS.md:
//
// The sharing this architecture came from is about SCARCITY. The first Field
// synthesis measured 79 DSPs against a device with 112, and the answer was to
// share the multipliers. The device has 553 M10K, and the quarter-wave table
// is 257 entries of 17 bits -- about one of them.
//
// A private table costs one M10K and removes an arbitration path, a refusal
// path and a starvation question. Every one of those has been the expensive
// class of defect in this engine. Sharing a plentiful resource buys nothing
// and adds exactly the failure that has cost the most time.
//
// If a later count shows M10K pressure -- the terrain and texture caches are
// the plausible source -- this is the first thing to revisit, and the
// arbitration it would need is already designed twice over.
//
// ---------------------------------------------------------------------------
// THE SHAPE: EIGHT LOOKUPS, THEN FOUR BANK REQUESTS
// ---------------------------------------------------------------------------
// `zhao_field_sin` is LATENCY 2, INITIATION INTERVAL 1 -- a request every
// clock. Four points need cos and sin each, so eight lookups stream out in
// eight clocks and the last answer lands two clocks later.
//
// Then the four products are FOUR FOUR-WIDE BANK REQUESTS, not sixteen
// multiplies, because c and s are per point and so are p and q:
//
//     request 1   c[l] * p[l]      all four points
//     request 2   s[l] * q[l]
//     request 3   s[l] * p[l]
//     request 4   c[l] * q[l]
//
// EVERY ISSUE HOLDS UNTIL GRANTED. The bank is shared and can refuse; the
// operands sit in registers that do not move for the duration, so a refusal
// costs one clock. The executor's DOT sequencer took six attempts to reach
// this shape -- every unit written since starts there.
module zhao_field_v3_rot (
    input var logic clk,
    input var logic rst_n,

    // ---- request: one four-point group -------------------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic               is_rot3_i,   // 0 = ROT2, 1 = ROT3
    input  var logic        [ 1:0] axis_i,      // ROT3 only: 0 = X, 1 = Y, else Z
    // The angle per point. Law: the LOW SIXTEEN BITS are the angle and the
    // upper half is ignored rather than being an error, so a caller that
    // leaves rubbish there gets a defined answer and the same one the software
    // gives. The waiver states the law; it does not silence a bug.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic signed [31:0] ang_0_i, ang_1_i, ang_2_i, ang_3_i,
    /* verilator lint_on UNUSEDSIGNAL */
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
    output var logic        [ 3:0] sat_add_o,
    output var logic        [ 3:0] sat_mul_o,
    output var logic        [ 7:0] tag_o,

    // ---- the shared four-wide bank (engine property) -----------------------
    output var logic               mul_issue_o,
    input  var logic               mul_ready_i,
    output var logic signed [32:0] mul_a_0_o, mul_a_1_o, mul_a_2_o, mul_a_3_o,
    output var logic signed [32:0] mul_b_0_o, mul_b_1_o, mul_b_2_o, mul_b_3_o,
    input  var logic               mul_valid_i,
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic signed [65:0] mul_p_0_i, mul_p_1_i, mul_p_2_i, mul_p_3_i
    /* verilator lint_on UNUSEDSIGNAL */
);

  localparam int LANES = 4;

  localparam logic [3:0] R_IDLE  = 4'd0;
  localparam logic [3:0] R_TRIG  = 4'd1;   // stream eight lookups
  localparam logic [3:0] R_TDRAIN = 4'd2;  // the last two answers land
  localparam logic [3:0] R_CP    = 4'd3;   // c*p
  localparam logic [3:0] R_CPW   = 4'd4;
  localparam logic [3:0] R_SQ    = 4'd5;   // s*q
  localparam logic [3:0] R_SQW   = 4'd6;
  localparam logic [3:0] R_SP    = 4'd7;   // s*p
  localparam logic [3:0] R_SPW   = 4'd8;
  localparam logic [3:0] R_CQ    = 4'd9;   // c*q
  localparam logic [3:0] R_CQW   = 4'd10;
  localparam logic [3:0] R_OUT   = 4'd11;

  logic [3:0] state;

  logic               h_rot3;
  logic [1:0]         h_axis;
  logic [7:0]         h_tag;
  logic [15:0]        h_ang [LANES];
  logic signed [31:0] h_a0 [LANES], h_a1 [LANES], h_a2 [LANES];
  logic signed [31:0] h_c  [LANES], h_s  [LANES];

  // Products, rescaled the moment they arrive -- one rounding each, which is
  // the law.
  logic signed [31:0] pr_cp [LANES], pr_sq [LANES], pr_sp [LANES], pr_cq [LANES];
  logic [3:0]         mul_fired;

  // ---- the private sine table --------------------------------------------
  // Eight lookups stream out at one per clock; `trig_k` is the issue index and
  // the answer for index k is captured at k+2.
  logic [3:0]         trig_k;
  logic [15:0]        sin_angle_c;
  logic               sin_is_cos_c;
  logic signed [31:0] sin_result;

  // Issue order is (cos, sin) per point, so index k belongs to point k>>1 and
  // is a cosine when k is even.
  assign sin_angle_c  = h_ang[trig_k[2:1]];
  assign sin_is_cos_c = ~trig_k[0];

  zhao_field_sin u_sin (
      .clk(clk),
      .angle_i(sin_angle_c),
      .is_cos_i(sin_is_cos_c),
      .result_o(sin_result)
  );

  // ---- (p, q) selection, and the lane that passes through -----------------
  // ROT2 is ROT3's Z case on two lanes: they share this datapath entirely.
  logic signed [31:0] sel_p [LANES], sel_q [LANES];
  always_comb begin
    for (int l = 0; l < LANES; l++) begin
      if (h_rot3 && (h_axis == 2'd0)) begin
        sel_p[l] = h_a1[l];
        sel_q[l] = h_a2[l];
      end else if (h_rot3 && (h_axis == 2'd1)) begin
        sel_p[l] = h_a2[l];
        sel_q[l] = h_a0[l];
      end else begin
        sel_p[l] = h_a0[l];
        sel_q[l] = h_a1[l];
      end
    end
  end

  // ---- the bank's operands ------------------------------------------------
  logic signed [32:0] mul_a [LANES], mul_b [LANES];
  logic signed [63:0] prod  [LANES];
  assign prod[0] = mul_p_0_i[63:0];
  assign prod[1] = mul_p_1_i[63:0];
  assign prod[2] = mul_p_2_i[63:0];
  assign prod[3] = mul_p_3_i[63:0];

  always_comb begin
    for (int l = 0; l < LANES; l++) begin
      mul_a[l] = 33'sd0;
      mul_b[l] = 33'sd0;
      case (state)
        R_CP: begin
          mul_a[l] = 33'(h_c[l]);
          mul_b[l] = 33'(sel_p[l]);
        end
        R_SQ: begin
          mul_a[l] = 33'(h_s[l]);
          mul_b[l] = 33'(sel_q[l]);
        end
        R_SP: begin
          mul_a[l] = 33'(h_s[l]);
          mul_b[l] = 33'(sel_p[l]);
        end
        default: begin
          mul_a[l] = 33'(h_c[l]);
          mul_b[l] = 33'(sel_q[l]);
        end
      endcase
    end
  end

  assign mul_issue_o = (state == R_CP) || (state == R_SQ) ||
                       (state == R_SP) || (state == R_CQ);
  assign mul_a_0_o = mul_a[0];
  assign mul_a_1_o = mul_a[1];
  assign mul_a_2_o = mul_a[2];
  assign mul_a_3_o = mul_a[3];
  assign mul_b_0_o = mul_b[0];
  assign mul_b_1_o = mul_b[1];
  assign mul_b_2_o = mul_b[2];
  assign mul_b_3_o = mul_b[3];

  // ---- the reference's arithmetic, function for function ------------------
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

  // The rotation produces TWO values whatever the axis: the one that lands in
  // p's slot and the one that lands in q's. Which destination register each
  // reaches is the axis's only job, and the third slot is the input COPIED.
  function automatic logic signed [31:0] rot_lo(input logic [1:0] l);
    rot_lo = sub_sat(pr_cp[l], pr_sq[l]);
  endfunction

  function automatic logic signed [31:0] rot_hi(input logic [1:0] l);
    rot_hi = add_sat(pr_sp[l], pr_cq[l]);
  endfunction

  //   ROT3 X : p=a1 q=a2  ->  dst1=lo dst2=hi dst0=a0
  //   ROT3 Y : p=a2 q=a0  ->  dst2=lo dst0=hi dst1=a1
  //   Z/ROT2 : p=a0 q=a1  ->  dst0=lo dst1=hi dst2=a2  (ROT2: dst2 is ZERO)
  function automatic logic signed [31:0] out0(input logic [1:0] l);
    if (h_rot3 && (h_axis == 2'd0))      out0 = h_a0[l];   // X: passes through
    else if (h_rot3 && (h_axis == 2'd1)) out0 = rot_hi(l); // Y: q's slot
    else                                 out0 = rot_lo(l);
  endfunction

  function automatic logic signed [31:0] out1(input logic [1:0] l);
    if (h_rot3 && (h_axis == 2'd0))      out1 = rot_lo(l);
    else if (h_rot3 && (h_axis == 2'd1)) out1 = h_a1[l];   // Y: passes through
    else                                 out1 = rot_hi(l);
  endfunction

  function automatic logic signed [31:0] out2(input logic [1:0] l);
    if (!h_rot3)                         out2 = 32'sd0;    // law 5: ROT2 has none
    else if (h_axis == 2'd0)             out2 = rot_hi(l);
    else if (h_axis == 2'd1)             out2 = rot_lo(l);
    else                                 out2 = h_a2[l];   // Z: passes through
  endfunction

  assign v_ready_o = (state == R_IDLE);
  assign tag_o     = h_tag;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= R_IDLE;
      h_rot3 <= 1'b0;
      h_axis <= 2'd0;
      h_tag <= 8'd0;
      trig_k <= 4'd0;
      mul_fired <= 4'b0;
      r_valid_o <= 1'b0;
      sat_add_o <= 4'b0;
      sat_mul_o <= 4'b0;
      for (int l = 0; l < LANES; l++) begin
        h_ang[l] <= 16'd0;
        h_a0[l]  <= '0;
        h_a1[l]  <= '0;
        h_a2[l]  <= '0;
        h_c[l]   <= '0;
        h_s[l]   <= '0;
        pr_cp[l] <= '0;
        pr_sq[l] <= '0;
        pr_sp[l] <= '0;
        pr_cq[l] <= '0;
      end
      o0_0_o <= '0; o0_1_o <= '0; o0_2_o <= '0; o0_3_o <= '0;
      o1_0_o <= '0; o1_1_o <= '0; o1_2_o <= '0; o1_3_o <= '0;
      o2_0_o <= '0; o2_1_o <= '0; o2_2_o <= '0; o2_3_o <= '0;
    end else begin
      // The sine answer for issue index k lands at k+2, so whenever an answer
      // is in flight it belongs to `trig_k - 2`.
      if ((state == R_TRIG) || (state == R_TDRAIN)) begin
        if (trig_k >= 4'd2) begin
          // The index is a LANE NUMBER, so it is two bits. Writing it as the
          // four-bit counter arithmetic and letting it truncate is how an
          // out-of-range write becomes a silent wrap.
          // THE PARITY IS THE ISSUE INDEX'S, NOT THE COUNTER'S, and getting
          // that backwards is not a subtle error: swapping c and s is exactly
          // a rotation by (90 - theta), so every answer is a VALID rotation of
          // the right point by the wrong angle. Nothing looks corrupt.
          //
          // The differential caught it immediately and pointed straight at it:
          // lane 0 at 22.5 degrees produced lane 2's 67.5-degree answer, and
          // lane 3 at 90 degrees produced the 0-degree one. Two complements in
          // the same failure is a signature, not a coincidence.
          //
          // Issue k belongs to point k>>1 and is a COSINE when k is even. The
          // answer arriving while the counter reads `trig_k` is issue
          // `trig_k - 2`, whose parity is the SAME as trig_k's -- so an even
          // counter means a cosine.
          if (trig_k[0] == 1'b0) h_c[2'((trig_k - 4'd2) >> 1)] <= sin_result;
          else                   h_s[2'((trig_k - 4'd2) >> 1)] <= sin_result;
        end
      end

      case (state)
        R_IDLE: begin
          if (v_valid_i) begin
            h_rot3 <= is_rot3_i;
            h_axis <= axis_i;
            h_tag  <= tag_i;
            h_ang[0] <= ang_0_i[15:0];
            h_ang[1] <= ang_1_i[15:0];
            h_ang[2] <= ang_2_i[15:0];
            h_ang[3] <= ang_3_i[15:0];
            h_a0[0] <= a0_0_i; h_a0[1] <= a0_1_i; h_a0[2] <= a0_2_i; h_a0[3] <= a0_3_i;
            h_a1[0] <= a1_0_i; h_a1[1] <= a1_1_i; h_a1[2] <= a1_2_i; h_a1[3] <= a1_3_i;
            h_a2[0] <= a2_0_i; h_a2[1] <= a2_1_i; h_a2[2] <= a2_2_i; h_a2[3] <= a2_3_i;
            trig_k <= 4'd0;
            mul_fired <= 4'b0;
            sat_add_o <= 4'b0;
            sat_mul_o <= 4'b0;
            state <= R_TRIG;
          end
        end

        // Eight lookups at one per clock. The table is II 1, so this is the
        // whole cost of the trig -- the alternative, one instance per point,
        // is four ROMs to save six clocks.
        R_TRIG: begin
          trig_k <= trig_k + 4'd1;
          if (trig_k == 4'd7) state <= R_TDRAIN;
        end

        R_TDRAIN: begin
          trig_k <= trig_k + 4'd1;
          if (trig_k == 4'd9) state <= R_CP;
        end

        // EVERY ISSUE HOLDS UNTIL GRANTED. The operands are registers that do
        // not move, so a refusal costs one clock and nothing else.
        R_CP: if (mul_ready_i) state <= R_CPW;
        R_CPW: begin
          if (mul_valid_i) begin
            for (int l = 0; l < LANES; l++) begin
              pr_cp[l] <= resc16(prod[l]);
              if (resc16_fired(prod[l])) mul_fired[l] <= 1'b1;
            end
            state <= R_SQ;
          end
        end

        R_SQ: if (mul_ready_i) state <= R_SQW;
        R_SQW: begin
          if (mul_valid_i) begin
            for (int l = 0; l < LANES; l++) begin
              pr_sq[l] <= resc16(prod[l]);
              if (resc16_fired(prod[l])) mul_fired[l] <= 1'b1;
            end
            state <= R_SP;
          end
        end

        R_SP: if (mul_ready_i) state <= R_SPW;
        R_SPW: begin
          if (mul_valid_i) begin
            for (int l = 0; l < LANES; l++) begin
              pr_sp[l] <= resc16(prod[l]);
              if (resc16_fired(prod[l])) mul_fired[l] <= 1'b1;
            end
            state <= R_CQ;
          end
        end

        R_CQ: if (mul_ready_i) state <= R_CQW;
        R_CQW: begin
          if (mul_valid_i) begin
            for (int l = 0; l < LANES; l++) begin
              pr_cq[l] <= resc16(prod[l]);
              if (resc16_fired(prod[l])) mul_fired[l] <= 1'b1;
            end
            state <= R_OUT;
          end
        end

        // Everything is settled here: all four products are rescaled and
        // parked, so the outputs are formed once rather than threaded through
        // the wait states.
        R_OUT: begin
          if (!r_valid_o) begin
            for (int l = 0; l < LANES; l++) begin
              sat_mul_o[l] <= mul_fired[l];
              sat_add_o[l] <= sub_fired(pr_cp[l], pr_sq[l]) ||
                              add_fired(pr_sp[l], pr_cq[l]);
            end
            o0_0_o <= out0(2'd0); o0_1_o <= out0(2'd1);
            o0_2_o <= out0(2'd2); o0_3_o <= out0(2'd3);
            o1_0_o <= out1(2'd0); o1_1_o <= out1(2'd1);
            o1_2_o <= out1(2'd2); o1_3_o <= out1(2'd3);
            o2_0_o <= out2(2'd0); o2_1_o <= out2(2'd1);
            o2_2_o <= out2(2'd2); o2_3_o <= out2(2'd3);
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

endmodule : zhao_field_v3_rot
