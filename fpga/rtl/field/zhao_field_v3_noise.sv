// zhao_field_v3_noise.sv — NOISE2 and RIDGE for four points at once, on the
// shared four-wide multiplier bank.
//
// ENFORCED-BY: tests/differential/field_v3_noise_directed.cpp:main
//
// WHAT CHANGED FROM zhao_field_noise, AND WHY IT IS NOT A REWRITE
// ---------------------------------------------------------------
// The v2 unit is SCALAR: it walks one point through six products, each an
// issue state and a wait state, because the shared lane answers two clocks
// after it is asked. The v3 executor is a FOUR-POINT machine and the bank is
// FOUR WIDE, so the port is not "instantiate the v2 unit four times":
//
//     ONE four-wide bank request = the SAME hash step for all four points.
//
// A four-point NOISE2 is therefore SIX bank requests, not twenty-four
// products that have to be scheduled somehow. RIDGE is four. The state
// machine below is the v2 one, step for step, in the same order and with the
// same names; only the datapath widened. That is the whole design, and it is
// a fit rather than a compromise.
//
// THE ISSUE STATES HOLD UNTIL GRANTED, which the v2 unit does not do and did
// not need to. zhao_field_exec_shared says why in its own header: there is no
// arbiter because zhao_field_seq keeps exactly one instruction in flight, so
// one requester exists and every request is granted. The v3 bank retires that
// premise on purpose -- executor lanes and services claiming at once is the
// reason it exists, after the first Field synthesis measured 79 DSPs of 112
// with nine of ten units idle at any instant.
//
// So every issue state here waits on mul_ready_i. The operands are held in
// registers that do not move for the duration, exactly as the curve service's
// finish stage does, and a refusal costs one clock. The executor's DOT
// sequencer took six attempts to reach this shape; this unit starts there.
//
// THE ARITHMETIC IS THE REFERENCE'S, UNCHANGED (zref::noise2_hash, qformats
// 7.5, PCG RXS-M-XS constants frozen verbatim):
//
//     s = (x*Cx) ^ ((y*Cy) ^ seed)      the lattice mix, SHARED by both lanes
//     s = s + lane*0xE1                 the lane salt
//     s = s*747796405 + 2891336453      the LCG advance
//     w = ((s >> ((s>>28)+4)) ^ s) * 277803737
//     h = (w >> 22) ^ w
//
//     NOISE2: dst0 = h(lane 0) >> 16,  dst1 = h(lane 1) >> 16
//     RIDGE : u = h(lane 0) >> 16
//             t = fx_sub(fx_add(u, u), 1<<16)
//             dst0 = fx_sub(1<<16, abs_sat(t)),  dst1 = 0
//
// EVERY PRODUCT IS MODULO 2^32 AND NEVER SATURATES (law 3). The bank lane is
// 33x33 SIGNED, and the two agree on the only bits read: the low 32 bits of a
// signed product are bit-identical to the low 32 of the unsigned product of
// the same bit patterns, because sign extension only affects bits at or above
// the operand width. The operands are ZERO-extended into 33 bits -- a u32
// handed to a signed port unextended reads as negative for half of all inputs
// -- and only mul_p_*_i[31:0] is consumed.
//
// This is the one place where the shared bank's signedness could silently
// disagree with an op's semantics, and the disagreement would look like a bad
// seed rather than a bad multiply: right for small coordinates, wrong for
// large ones. Hence it is stated here rather than inherited.
module zhao_field_v3_noise (
    input logic clk,
    input logic rst_n,

    // ---- request: one four-point group -------------------------------------
    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic               is_ridge_i,   // 0 = NOISE2, 1 = RIDGE
    input  logic signed [31:0] a0_0_i,       // x per point
    input  logic signed [31:0] a0_1_i,
    input  logic signed [31:0] a0_2_i,
    input  logic signed [31:0] a0_3_i,
    input  logic signed [31:0] a1_0_i,       // y per point
    input  logic signed [31:0] a1_1_i,
    input  logic signed [31:0] a1_2_i,
    input  logic signed [31:0] a1_3_i,
    input  logic        [31:0] seed_i,       // the instruction's imm
    input  logic        [ 7:0] tag_i,

    // ---- reply -------------------------------------------------------------
    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] o0_0_o,
    output logic signed [31:0] o0_1_o,
    output logic signed [31:0] o0_2_o,
    output logic signed [31:0] o0_3_o,
    output logic signed [31:0] o1_0_o,
    output logic signed [31:0] o1_1_o,
    output logic signed [31:0] o1_2_o,
    output logic signed [31:0] o1_3_o,
    output logic        [ 3:0] sat_add_o,
    output logic        [ 3:0] sat_rescale_o,
    output logic        [ 7:0] tag_o,

    // ---- the shared four-wide bank (engine property, not this unit's) ------
    output logic               mul_issue_o,
    input  logic               mul_ready_i,
    output logic signed [32:0] mul_a_0_o,
    output logic signed [32:0] mul_a_1_o,
    output logic signed [32:0] mul_a_2_o,
    output logic signed [32:0] mul_a_3_o,
    output logic signed [32:0] mul_b_0_o,
    output logic signed [32:0] mul_b_1_o,
    output logic signed [32:0] mul_b_2_o,
    output logic signed [32:0] mul_b_3_o,
    input  logic               mul_valid_i,
    // The bank lane is 66 bits because DOT3 sums three products. A hash
    // consumes the low 32 of a 32x32, which is a property of the op and not a
    // hole in the port -- the same note zhao_field_noise carries.
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic signed [65:0] mul_p_0_i,
    input  logic signed [65:0] mul_p_1_i,
    input  logic signed [65:0] mul_p_2_i,
    input  logic signed [65:0] mul_p_3_i
    /* verilator lint_on UNUSEDSIGNAL */
);

  localparam int LANES = 4;

  localparam logic [31:0] C_X = 32'h9E37_79B1;
  localparam logic [31:0] C_Y = 32'h85EB_CA77;
  localparam logic [31:0] C_LCG_M = 32'd747796405;
  localparam logic [31:0] C_LCG_A = 32'd2891336453;
  localparam logic [31:0] C_XSM = 32'd277803737;
  localparam logic signed [31:0] FX_ONE = 32'sh0001_0000;

  // The v2 state names, kept verbatim so the operand mux below still reads as
  // the reference's step list.
  localparam logic [3:0] S_IDLE = 4'd0;
  localparam logic [3:0] S_MIX_X = 4'd1;
  localparam logic [3:0] S_MIX_XW = 4'd2;
  localparam logic [3:0] S_MIX_Y = 4'd3;
  localparam logic [3:0] S_MIX_YW = 4'd4;
  localparam logic [3:0] S_LCG = 4'd5;
  localparam logic [3:0] S_LCGW = 4'd6;
  localparam logic [3:0] S_RXS = 4'd7;
  localparam logic [3:0] S_RXSW = 4'd8;
  localparam logic [3:0] S_LANE = 4'd9;
  localparam logic [3:0] S_OUT = 4'd10;

  logic [3:0] state;

  logic signed [31:0] h_a0[LANES];
  logic signed [31:0] h_a1[LANES];
  logic        [31:0] h_seed;
  logic               h_ridge;
  logic        [ 7:0] h_tag;
  logic               lane;              // which hash lane is being walked

  logic        [31:0] mix_x[LANES];      // x * C_X
  logic        [31:0] s_mix[LANES];      // the finished lattice mix, SHARED
  logic        [31:0] s_reg[LANES];      // the running hash word
  logic        [31:0] lane0[LANES];      // lane 0's finished hash

  // Law 1: ARITHMETIC shift, then reinterpreted as unsigned.
  logic [31:0] ix[LANES], iy[LANES];
  always_comb begin
    for (int l = 0; l < LANES; l++) begin
      ix[l] = $unsigned(h_a0[l] >>> 16);
      iy[l] = $unsigned(h_a1[l] >>> 16);
    end
  end

  // Law 2: the shift amount is a function of the DATA, 4..19, PER POINT.
  // Four points in one request each choose their own shift, which is the one
  // place a vector unit differs from four scalar ones in more than width.
  logic [ 4:0] rxs_sh[LANES];
  logic [31:0] rxs_in[LANES];
  logic [31:0] lane1_h[LANES];
  always_comb begin
    for (int l = 0; l < LANES; l++) begin
      rxs_sh[l]  = 5'({s_reg[l][31:28]} + 4'd4);
      rxs_in[l]  = (s_reg[l] >> rxs_sh[l]) ^ s_reg[l];
      lane1_h[l] = (s_reg[l] >> 22) ^ s_reg[l];
    end
  end

  // ---- the bank's operands, law 3: modulo 2^32, never saturating ----------
  logic [31:0] mul_a[LANES], mul_b[LANES];
  logic [31:0] mul_p[LANES];
  assign mul_p[0] = mul_p_0_i[31:0];
  assign mul_p[1] = mul_p_1_i[31:0];
  assign mul_p[2] = mul_p_2_i[31:0];
  assign mul_p[3] = mul_p_3_i[31:0];

  always_comb begin
    for (int l = 0; l < LANES; l++) begin
      mul_a[l] = 32'd0;
      mul_b[l] = 32'd0;
      case (state)
        S_MIX_X: begin
          mul_a[l] = ix[l];
          mul_b[l] = C_X;
        end
        S_MIX_Y: begin
          mul_a[l] = iy[l];
          mul_b[l] = C_Y;
        end
        S_LCG: begin
          // The lane salt is added BEFORE the advance, not mixed into the
          // lattice: lane 1 is the same lattice point taken one salt further.
          mul_a[l] = s_reg[l] + (lane ? 32'h0000_00E1 : 32'd0);
          mul_b[l] = C_LCG_M;
        end
        default: begin
          mul_a[l] = rxs_in[l];
          mul_b[l] = C_XSM;
        end
      endcase
    end
  end

  assign mul_issue_o = (state == S_MIX_X) || (state == S_MIX_Y) ||
                       (state == S_LCG)   || (state == S_RXS);
  assign mul_a_0_o = $signed({1'b0, mul_a[0]});
  assign mul_a_1_o = $signed({1'b0, mul_a[1]});
  assign mul_a_2_o = $signed({1'b0, mul_a[2]});
  assign mul_a_3_o = $signed({1'b0, mul_a[3]});
  assign mul_b_0_o = $signed({1'b0, mul_b[0]});
  assign mul_b_1_o = $signed({1'b0, mul_b[1]});
  assign mul_b_2_o = $signed({1'b0, mul_b[2]});
  assign mul_b_3_o = $signed({1'b0, mul_b[3]});

  // ---- RIDGE's fold, on each point's finished lane-0 hash -----------------
  // Law 5: the TOP half. u is [0, 1) and never negative.
  logic signed [31:0] u_val[LANES];
  always_comb begin
    for (int l = 0; l < LANES; l++) u_val[l] = $signed({16'd0, lane0[l][31:16]});
  end

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

  // Law 6: abs with the INT32_MIN rail, matching abs_sat.
  function automatic logic signed [31:0] abs_sat(input logic signed [31:0] a);
    begin
      if (a == 32'sh8000_0000) abs_sat = 32'sh7FFF_FFFF;
      else abs_sat = (a < 0) ? -a : a;
    end
  endfunction

  logic signed [31:0] ridge_t[LANES], ridge_r[LANES];
  always_comb begin
    for (int l = 0; l < LANES; l++) begin
      ridge_t[l] = sub_sat(add_sat(u_val[l], u_val[l]), FX_ONE);
      ridge_r[l] = sub_sat(FX_ONE, abs_sat(ridge_t[l]));
    end
  end

  assign v_ready_o = (state == S_IDLE);
  assign tag_o     = h_tag;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= S_IDLE;
      h_seed <= '0;
      h_ridge <= 1'b0;
      h_tag <= '0;
      lane <= 1'b0;
      for (int l = 0; l < LANES; l++) begin
        h_a0[l]  <= '0;
        h_a1[l]  <= '0;
        mix_x[l] <= 32'd0;
        s_mix[l] <= 32'd0;
        s_reg[l] <= 32'd0;
        lane0[l] <= 32'd0;
      end
      r_valid_o <= 1'b0;
      o0_0_o <= '0;
      o0_1_o <= '0;
      o0_2_o <= '0;
      o0_3_o <= '0;
      o1_0_o <= '0;
      o1_1_o <= '0;
      o1_2_o <= '0;
      o1_3_o <= '0;
      sat_add_o <= 4'b0;
      sat_rescale_o <= 4'b0;
    end else begin
      case (state)
        S_IDLE: begin
          if (v_valid_i) begin
            h_a0[0] <= a0_0_i;
            h_a0[1] <= a0_1_i;
            h_a0[2] <= a0_2_i;
            h_a0[3] <= a0_3_i;
            h_a1[0] <= a1_0_i;
            h_a1[1] <= a1_1_i;
            h_a1[2] <= a1_2_i;
            h_a1[3] <= a1_3_i;
            h_seed <= seed_i;
            h_ridge <= is_ridge_i;
            h_tag <= tag_i;
            lane <= 1'b0;
            sat_add_o <= 4'b0;
            sat_rescale_o <= 4'b0;
            state <= S_MIX_X;
          end
        end

        // EVERY ISSUE STATE HOLDS UNTIL GRANTED. The request stays asserted
        // across a refusal -- mul_issue_o is a function of the state -- and
        // the operands are registers that do not move, so a retry costs one
        // clock and nothing else. Advancing here is what leaves the wait
        // state below waiting for a product nobody started.
        S_MIX_X: if (mul_ready_i) state <= S_MIX_XW;
        S_MIX_XW: begin
          if (mul_valid_i) begin
            for (int l = 0; l < LANES; l++) mix_x[l] <= mul_p[l];
            state <= S_MIX_Y;
          end
        end

        S_MIX_Y: if (mul_ready_i) state <= S_MIX_YW;
        S_MIX_YW: begin
          if (mul_valid_i) begin
            // s = (x*Cx) ^ ((y*Cy) ^ seed). The seed is folded into the Y
            // term; xor is associative, so the grouping is readability rather
            // than arithmetic.
            for (int l = 0; l < LANES; l++) begin
              s_mix[l] <= mix_x[l] ^ (mul_p[l] ^ h_seed);
              s_reg[l] <= mix_x[l] ^ (mul_p[l] ^ h_seed);
            end
            state <= S_LCG;
          end
        end

        S_LCG: if (mul_ready_i) state <= S_LCGW;
        S_LCGW: begin
          if (mul_valid_i) begin
            for (int l = 0; l < LANES; l++) s_reg[l] <= mul_p[l] + C_LCG_A;
            state <= S_RXS;
          end
        end

        S_RXS: if (mul_ready_i) state <= S_RXSW;
        S_RXSW: begin
          if (mul_valid_i) begin
            for (int l = 0; l < LANES; l++) s_reg[l] <= mul_p[l];  // w
            state <= S_LANE;
          end
        end

        S_LANE: begin
          if (h_ridge) begin
            for (int l = 0; l < LANES; l++) lane0[l] <= lane1_h[l];
            state <= S_OUT;
          end else if (lane == 1'b0) begin
            for (int l = 0; l < LANES; l++) begin
              lane0[l] <= lane1_h[l];
              // The lattice mix is SHARED between the lanes: only the salt
              // and everything after it is walked again. REPLAYED from a
              // register, not recomputed -- recomputing reads as harmless and
              // costs two more bank requests, which is the cost this shape
              // exists to avoid.
              s_reg[l] <= s_mix[l];
            end
            lane  <= 1'b1;
            state <= S_LCG;
          end else begin
            o0_0_o <= u_val[0];
            o0_1_o <= u_val[1];
            o0_2_o <= u_val[2];
            o0_3_o <= u_val[3];
            o1_0_o <= $signed({16'd0, lane1_h[0][31:16]});
            o1_1_o <= $signed({16'd0, lane1_h[1][31:16]});
            o1_2_o <= $signed({16'd0, lane1_h[2][31:16]});
            o1_3_o <= $signed({16'd0, lane1_h[3][31:16]});
            r_valid_o <= 1'b1;
            state <= S_OUT;
          end
        end

        S_OUT: begin
          if (!r_valid_o) begin
            // RIDGE lands here with lane0 holding each point's finished hash.
            o0_0_o <= ridge_r[0];
            o0_1_o <= ridge_r[1];
            o0_2_o <= ridge_r[2];
            o0_3_o <= ridge_r[3];
            o1_0_o <= 32'sd0;
            o1_1_o <= 32'sd0;
            o1_2_o <= 32'sd0;
            o1_3_o <= 32'sd0;
            for (int l = 0; l < LANES; l++) begin
              sat_add_o[l] <= add_fired(u_val[l], u_val[l]) ||
                              sub_fired(add_sat(u_val[l], u_val[l]), FX_ONE) ||
                              sub_fired(FX_ONE, abs_sat(ridge_t[l]));
              sat_rescale_o[l] <= (ridge_t[l] == 32'sh8000_0000);
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

endmodule : zhao_field_v3_noise
