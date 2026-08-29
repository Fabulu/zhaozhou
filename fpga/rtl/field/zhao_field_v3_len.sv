// zhao_field_v3_len.sv — LEN2, LEN3 and DIST2.
//
// ENFORCED-BY: tests/differential/field_v3_len_directed.cpp:main
//
// ---------------------------------------------------------------------------
// THREE OPCODES, ONE DATAPATH
// ---------------------------------------------------------------------------
// The oracle makes the sharing obvious rather than clever:
//
//     LEN2   dst = len_of({a0, a1},      2)
//     LEN3   dst = len_of({a0, a1, a2},  3)
//     DIST2  dst = len_of({a0-b0, a1-b1}, 2)
//
// and `len_of` is one function:
//
//     n2  = sum of v[i]*v[i] as an EXACT u64
//     len = isqrt_u64(n2)
//     if len > INT32_MAX -> INT32_MAX and bump the rescale ledger
//
// So DIST2 is LEN2 with a saturating subtract in front, and LEN3 is LEN2 with
// a third term. Building them as one block is not an economy measure; it is
// what the reference already says they are.
//
// DIST2 is the reason the executor grew a fifth source port. Its shape is
// {1, {2,2,0}, 2, 0} -- two members in operand a and TWO in b -- and every
// long op before it had a single-member b.
//
// ---------------------------------------------------------------------------
// EXACT, NOT APPROXIMATE, AND THAT IS THE EXPENSIVE PART
// ---------------------------------------------------------------------------
// `n2` is a u64 and must stay one: two s32 squares can reach 2^62, so summing
// them in 32 or even 33 bits would wrap on perfectly ordinary coordinates.
// The squares come from the shared four-wide bank, one component at a time
// across all four lanes, and are accumulated at full width here.
//
// The root is `zhao_field_isqrt`, the engine's own floor-exact restoring unit
// -- 32 fixed iterations, the same one NORMALIZE uses, which is why that op
// costs 182 clocks. ONE unit is walked across the four lanes rather than four
// instantiated: this engine is short of area, and the walk is the same trade
// the rotation and trig services already make.
//
// That makes this the slowest service on the path by a wide margin, and it is
// slow for a reason that is not negotiable: `len_of` is exact, and an
// approximate root would be a different answer, not a faster one.
module zhao_field_v3_len #(
    parameter int LANES = 4
) (
    input var logic clk,
    input var logic rst_n,

    // ---- request: one four-point group -------------------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    // 0 = LEN2, 1 = LEN3, 2 = DIST2
    input  var logic        [ 1:0] mode_i,
    input  var logic signed [31:0] a0_0_i, a0_1_i, a0_2_i, a0_3_i,
    input  var logic signed [31:0] a1_0_i, a1_1_i, a1_2_i, a1_3_i,
    input  var logic signed [31:0] a2_0_i, a2_1_i, a2_2_i, a2_3_i,
    input  var logic signed [31:0] b0_0_i, b0_1_i, b0_2_i, b0_3_i,
    input  var logic signed [31:0] b1_0_i, b1_1_i, b1_2_i, b1_3_i,
    input  var logic        [ 7:0] tag_i,

    // ---- reply -------------------------------------------------------------
    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] o0_0_o, o0_1_o, o0_2_o, o0_3_o,
    // Per lane: the length exceeded INT32_MAX and was clamped. `len_of` bumps
    // the RESCALE lane for this, not the add lane.
    output var logic        [ 3:0] sat_rescale_o,
    output var logic        [ 7:0] tag_o,

    // ---- the shared four-wide multiplier bank ------------------------------
    output var logic               mul_issue_o,
    input  var logic               mul_ready_i,
    output var logic signed [32:0] mul_a_0_o, mul_a_1_o, mul_a_2_o, mul_a_3_o,
    output var logic signed [32:0] mul_b_0_o, mul_b_1_o, mul_b_2_o, mul_b_3_o,
    input  var logic               mul_valid_i,
    // THE TOP TWO BITS OF EACH PRODUCT ARE NOT READ, AND THAT IS A LAW RATHER
    // THAN AN OVERSIGHT. The bank returns 66 bits because a general 33x33
    // product needs them. Every product this service asks for is a SQUARE, so
    // it is non-negative and at most (2^31)^2 = 2^62 -- it cannot reach bit 64.
    // The waiver states that; it does not hide a truncation.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic signed [65:0] mul_p_0_i, mul_p_1_i, mul_p_2_i, mul_p_3_i
    /* verilator lint_on UNUSEDSIGNAL */
);

  localparam logic [1:0] M_LEN2  = 2'd0;
  localparam logic [1:0] M_LEN3  = 2'd1;
  localparam logic [1:0] M_DIST2 = 2'd2;

  localparam logic [2:0] L_IDLE  = 3'd0;
  localparam logic [2:0] L_ISSUE = 3'd1;
  localparam logic [2:0] L_WAIT  = 3'd2;
  localparam logic [2:0] L_ROOT  = 3'd3;
  localparam logic [2:0] L_HOLD  = 3'd4;

  logic [2:0]         state_r;
  // No `mode_r`. The mode decides two things -- how many components and
  // whether to subtract -- and BOTH are resolved at capture time, into
  // `ncomp_r` and into `v_r` itself. Storing the mode as well would be a
  // second copy of a fact already recorded, which is the shape of every
  // seam defect this engine has produced.
  logic [7:0]         tag_r;
  logic [1:0]         comp_r;      // which component is being squared, 0..2
  logic [1:0]         ncomp_r;     // 2 or 3
  logic signed [31:0] v_r [3][LANES];
  logic        [63:0] n2_r [LANES];

  // ---- the saturating subtract DIST2 needs --------------------------------
  // `fx_sub` saturates; a wrapped delta would give a plausible short distance
  // for two far-apart points, which is the worst kind of wrong.
  function automatic logic signed [31:0] sub_sat(input logic signed [31:0] a,
                                                 input logic signed [31:0] b);
    logic signed [32:0] w;
    begin
      w = 33'(a) - 33'(b);
      if (w > 33'sd2147483647)       sub_sat = 32'sd2147483647;
      else if (w < -33'sd2147483648) sub_sat = -32'sd2147483648;
      else                           sub_sat = w[31:0];
    end
  endfunction

  // ---- the root, walked ----------------------------------------------------
  logic        rt_n_valid, rt_n_ready, rt_r_valid;
  logic [63:0] rt_n, rt_r;
  logic [1:0]  root_lane_r;
  logic        root_done_r;

  zhao_field_isqrt u_isqrt (
      .clk(clk), .rst_n(rst_n),
      .n_valid_i(rt_n_valid), .n_ready_o(rt_n_ready),
      .n_i(rt_n),
      .r_valid_o(rt_r_valid), .r_ready_i(1'b1),
      .r_o(rt_r)
  );

  assign rt_n       = n2_r[root_lane_r];
  assign rt_n_valid = (state_r == L_ROOT) && !root_done_r;

  // ---- the bank request: one component across all four lanes ---------------
  assign mul_issue_o = (state_r == L_ISSUE);
  assign mul_a_0_o = 33'(v_r[comp_r][0]);
  assign mul_a_1_o = 33'(v_r[comp_r][1]);
  assign mul_a_2_o = 33'(v_r[comp_r][2]);
  assign mul_a_3_o = 33'(v_r[comp_r][3]);
  assign mul_b_0_o = 33'(v_r[comp_r][0]);
  assign mul_b_1_o = 33'(v_r[comp_r][1]);
  assign mul_b_2_o = 33'(v_r[comp_r][2]);
  assign mul_b_3_o = 33'(v_r[comp_r][3]);

  assign v_ready_o = (state_r == L_IDLE);
  assign r_valid_o = (state_r == L_HOLD);
  assign tag_o     = tag_r;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_r       <= L_IDLE;
      tag_r         <= 8'd0;
      comp_r        <= 2'd0;
      ncomp_r       <= 2'd2;
      root_lane_r   <= 2'd0;
      root_done_r   <= 1'b0;
      sat_rescale_o <= 4'd0;
      o0_0_o <= '0; o0_1_o <= '0; o0_2_o <= '0; o0_3_o <= '0;
      for (int c = 0; c < 3; c++)
        for (int l = 0; l < LANES; l++) v_r[c][l] <= '0;
      for (int l = 0; l < LANES; l++) n2_r[l] <= 64'd0;
    end else begin
      case (state_r)
        L_IDLE: begin
          if (v_valid_i) begin
            tag_r   <= tag_i;
            // Spelled out for all three rather than defaulted, so adding a
            // fourth mode has to name its own width instead of inheriting one.
            ncomp_r <= (mode_i == M_LEN3)  ? 2'd3
                     : (mode_i == M_LEN2)  ? 2'd2
                     : /* M_DIST2 */         2'd2;
            comp_r  <= 2'd0;
            for (int l = 0; l < LANES; l++) n2_r[l] <= 64'd0;

            // DIST2 subtracts; the other two take the components as they are.
            if (mode_i == M_DIST2) begin
              v_r[0][0] <= sub_sat(a0_0_i, b0_0_i);
              v_r[0][1] <= sub_sat(a0_1_i, b0_1_i);
              v_r[0][2] <= sub_sat(a0_2_i, b0_2_i);
              v_r[0][3] <= sub_sat(a0_3_i, b0_3_i);
              v_r[1][0] <= sub_sat(a1_0_i, b1_0_i);
              v_r[1][1] <= sub_sat(a1_1_i, b1_1_i);
              v_r[1][2] <= sub_sat(a1_2_i, b1_2_i);
              v_r[1][3] <= sub_sat(a1_3_i, b1_3_i);
            end else begin
              v_r[0][0] <= a0_0_i; v_r[0][1] <= a0_1_i;
              v_r[0][2] <= a0_2_i; v_r[0][3] <= a0_3_i;
              v_r[1][0] <= a1_0_i; v_r[1][1] <= a1_1_i;
              v_r[1][2] <= a1_2_i; v_r[1][3] <= a1_3_i;
            end
            v_r[2][0] <= a2_0_i; v_r[2][1] <= a2_1_i;
            v_r[2][2] <= a2_2_i; v_r[2][3] <= a2_3_i;

            state_r <= L_ISSUE;
          end
        end

        // AN INSTRUCTION MAY NOT ADVANCE PAST A REFUSED ISSUE. The bank is
        // shared and can say no; advancing anyway would wait for a product
        // nobody started, which is the open-loop defect this engine has fixed
        // twice already.
        L_ISSUE: begin
          if (mul_ready_i) state_r <= L_WAIT;
        end

        L_WAIT: begin
          if (mul_valid_i) begin
            // The square of an s32 needs 64 bits and the sum of three needs
            // 64 too; taking the low 64 of the 66-bit product is exact here
            // because a square is non-negative and fits.
            n2_r[0] <= n2_r[0] + mul_p_0_i[63:0];
            n2_r[1] <= n2_r[1] + mul_p_1_i[63:0];
            n2_r[2] <= n2_r[2] + mul_p_2_i[63:0];
            n2_r[3] <= n2_r[3] + mul_p_3_i[63:0];

            if (comp_r + 2'd1 >= ncomp_r) begin
              comp_r      <= 2'd0;
              root_lane_r <= 2'd0;
              root_done_r <= 1'b0;
              state_r     <= L_ROOT;
            end else begin
              comp_r  <= comp_r + 2'd1;
              state_r <= L_ISSUE;
            end
          end
        end

        // One root unit, four lanes, in order. `root_done_r` drops the request
        // for the clock the answer is taken so a single n2 is not offered
        // twice -- the same guard the curve and ring services carry.
        L_ROOT: begin
          if (rt_n_valid && rt_n_ready) root_done_r <= 1'b1;
          if (rt_r_valid) begin
            // len_of's saturation: above INT32_MAX the answer clamps and the
            // RESCALE lane is bumped, not the add lane.
            if (rt_r > 64'd2147483647) begin
              sat_rescale_o[root_lane_r] <= 1'b1;
              case (root_lane_r)
                2'd0: o0_0_o <= 32'sd2147483647;
                2'd1: o0_1_o <= 32'sd2147483647;
                2'd2: o0_2_o <= 32'sd2147483647;
                default: o0_3_o <= 32'sd2147483647;
              endcase
            end else begin
              sat_rescale_o[root_lane_r] <= 1'b0;
              case (root_lane_r)
                2'd0: o0_0_o <= rt_r[31:0];
                2'd1: o0_1_o <= rt_r[31:0];
                2'd2: o0_2_o <= rt_r[31:0];
                default: o0_3_o <= rt_r[31:0];
              endcase
            end

            if (root_lane_r == 2'd3) begin
              state_r <= L_HOLD;
            end else begin
              root_lane_r <= root_lane_r + 2'd1;
              root_done_r <= 1'b0;
            end
          end
        end

        L_HOLD: begin
          if (r_ready_i) state_r <= L_IDLE;
        end

        default: state_r <= L_IDLE;
      endcase
    end
  end

endmodule : zhao_field_v3_len
