// zhao_field_v3_len.sv — LEN2, LEN3 and DIST2, at an initiation interval.
//
// ENFORCED-BY: tests/differential/field_v3_len_directed.cpp:main
//
// ---------------------------------------------------------------------------
// THREE OPCODES, ONE DATAPATH
// ---------------------------------------------------------------------------
//     LEN2   dst = len_of({a0, a1},       2)
//     LEN3   dst = len_of({a0, a1, a2},   3)
//     DIST2  dst = len_of({a0-b0, a1-b1}, 2)
//
// and `len_of` is one function: n2 as an EXACT u64, isqrt_u64, clamp to
// INT32_MAX with a RESCALE bump. DIST2 is LEN2 with a saturating subtract in
// front; LEN3 is LEN2 with a third term.
//
// DIST2 is the reason the executor grew a fifth source port: its shape is
// {1, {2,2,0}, 2, 0} and every long op before it had a single-member b.
//
// ---------------------------------------------------------------------------
// THIS BLOCK IS SHAPED BY A DEADLINE, AND REBUILT TWICE BY MEASUREMENT
// ---------------------------------------------------------------------------
// Earth's stress frame is 850,000 clocks for 128 associations of 273
// four-point groups: 24.3 clocks per group for the WHOLE program, and DIST2
// appears in all three shipped Earth programs.
//
//     one root, walked over four lanes      II 146    74% of the whole frame
//     four roots, one per lane              II  42
//     two banks of four                     this file
//
// The first number is why the other two exist. Nothing here was designed from
// an opinion about what would be fast.
//
// ---------------------------------------------------------------------------
// LATENCY IS NOT THROUGHPUT
// ---------------------------------------------------------------------------
// A group may take 60 clocks end to end provided another group is using the
// machinery meanwhile. The curve service already proves it in this engine:
// latency 32, initiation interval 13.
//
// `zhao_field_isqrt` is 32 fixed iterations, serial, with `n_ready_o` gated on
// idle -- it cannot be pipelined without being rewritten. So the answer is two
// banks of four:
//
//     bank A   [--- 32-clock roots for group N ---]
//     bank B                  [--- 32-clock roots for group N+1 ---]
//     front    [sq N][sq N+1][sq N+2] ...
//
// While one bank roots, the other takes the next group and the front end
// computes squares continuously. The initiation interval becomes the larger of
// "how fast the front produces an n2" and "half the root time", instead of the
// whole latency.
//
// EIGHT ROOTS IS ROUGHLY 2,000 ALMs against ~251 for one. That is a real cost
// and it is the one the deadline demands. `zhao_probe_dist_svc` was probed at
// exactly this topology -- two banks of four, target II <= 20 -- before the
// problem was hit, so the shape was foreseen rather than invented here.
//
// REPLIES DRAIN IN ACCEPT ORDER. A two-entry order queue records which bank
// took which group, so "every reply returns to its issuing requester" is true
// by construction rather than by tag arithmetic -- the same reasoning the
// distance probe gives for the same choice.
//
// ENFORCED-BY: tests/differential/field_v3_len_directed.cpp:main
//
// BY CONSTRUCTION IS A CLAIM, SO IT IS CHECKED. Section 6 streams thirty-two
// groups each carrying DISTINCT operands and compares every reply against its
// own answer. It used to stream one operand pair past all of them, which could
// not have caught a cross-group swap at all -- every value matched because
// every value was the same value. The tag order check did not cover it either:
// the tag rides the order queue and the data rides the banks, so the tags can
// be in perfect order over swapped numbers.
//
// ENFORCED-BY: tests/differential/field_v3_len_directed.cpp:main
//
// What is NOT negotiable is exactness. `len_of` is floor-exact and an
// approximate root would be a different answer, not a faster one.
module zhao_field_v3_len #(
    parameter int LANES = 4,
    // ROOT BANKS. The root is 32 fixed iterations and cannot be pipelined, so
    // throughput here is bought only by having more of them: while one bank
    // roots, the others take the next groups.
    //
    // TWO WAS NOT ENOUGH, and the composed Earth gate is what says so rather
    // than an argument. DIST2's initiation interval is 22 clocks against an
    // admission ceiling of 24.33 for the WHOLE program, so the distance
    // service alone was consuming 90% of the budget and every other op had to
    // fit in the 2.3 clocks left. Both shipped Earth programs landed just above
    // it -- 25 and 29 -- which is what a single binding constraint looks like.
    //
    // Each bank is LANES roots, so this is the expensive parameter in the whole
    // engine. Raise it on evidence and re-fit; do not round it up for comfort.
    parameter int BANKS = 2
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
    output var logic        [ 3:0] sat_rescale_o,
    output var logic        [ 7:0] tag_o,

    // ---- the shared four-wide multiplier bank ------------------------------
    output var logic               mul_issue_o,
    input  var logic               mul_ready_i,
    output var logic signed [32:0] mul_a_0_o, mul_a_1_o, mul_a_2_o, mul_a_3_o,
    output var logic signed [32:0] mul_b_0_o, mul_b_1_o, mul_b_2_o, mul_b_3_o,
    input  var logic               mul_valid_i,
    // The top two bits of each product are not read, and that is a LAW: every
    // product here is a SQUARE, so it is non-negative and at most 2^62.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic signed [65:0] mul_p_0_i, mul_p_1_i, mul_p_2_i, mul_p_3_i
    /* verilator lint_on UNUSEDSIGNAL */
);

  localparam logic [1:0] M_LEN2  = 2'd0;
  localparam logic [1:0] M_LEN3  = 2'd1;
  localparam logic [1:0] M_DIST2 = 2'd2;

  // A wrapped delta would report two maximally distant points as nearly
  // coincident: plausible, and completely wrong.
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

  // ---- the front end: squares, one component across four lanes ------------
  localparam logic [1:0] F_IDLE  = 2'd0;
  localparam logic [1:0] F_ISSUE = 2'd1;
  localparam logic [1:0] F_WAIT  = 2'd2;
  localparam logic [1:0] F_HAND  = 2'd3;

  logic [1:0]         f_state_r;
  logic [1:0]         f_comp_r, f_ncomp_r;
  logic [7:0]         f_tag_r;
  logic signed [31:0] f_v_r [3][LANES];
  logic        [63:0] f_n2_r [LANES];

  // ---- the two root banks --------------------------------------------------
  logic               bk_busy_r [BANKS];
  logic               bk_done_r [BANKS];
  logic        [ 7:0] bk_tag_r  [BANKS];
  logic signed [31:0] bk_res_r  [BANKS][LANES];
  logic        [ 3:0] bk_sat_r  [BANKS];
  logic [LANES-1:0]   bk_started_r [BANKS];
  logic [LANES-1:0]   bk_got_r     [BANKS];
  logic        [63:0] bk_n2_r   [BANKS][LANES];

  logic [LANES-1:0] rt_n_valid [BANKS], rt_n_ready [BANKS], rt_r_valid [BANKS];
  logic [63:0]      rt_r [BANKS][LANES];

  for (genvar bk = 0; bk < BANKS; bk++) begin : gen_bank
    for (genvar g = 0; g < LANES; g++) begin : gen_root
      zhao_field_isqrt u_isqrt (
          .clk(clk), .rst_n(rst_n),
          .n_valid_i(rt_n_valid[bk][g]), .n_ready_o(rt_n_ready[bk][g]),
          .n_i(bk_n2_r[bk][g]),
          .r_valid_o(rt_r_valid[bk][g]), .r_ready_i(1'b1),
          .r_o(rt_r[bk][g])
      );
      assign rt_n_valid[bk][g] = bk_busy_r[bk] && !bk_done_r[bk] && !bk_started_r[bk][g];
    end
  end

  // ---- the order queue: two entries, one bit each -------------------------
  // Replies leave in ACCEPT ORDER, so a bank that finishes early cannot
  // overtake and hand a caller somebody else's length.
  // HEAD AND TAIL POINTERS, NOT A SHIFTING QUEUE. The first version shifted
  // entry 1 down to 0 on a retire while a push wrote entry 1, so a push and a
  // retire on the SAME clock both assigned oq[1] and the push was silently
  // lost. It showed as 31 of 32 streamed groups retiring with the tags out of
  // order -- a lost instruction, not a slow one.
  //
  // Pointers cannot collide: a push writes [tail] and a retire advances head,
  // and the count is decided in ONE place from both. That is the same shape
  // the dispatcher's in-flight queue already uses, and for the same reason.
  // Accept order, by pointer rather than by shifting -- a push and a retire on
  // the same clock must not both write the same entry.
  localparam int BW = (BANKS <= 2) ? 1 : ((BANKS <= 4) ? 2 : 3);
  logic [BW-1:0]  oq_bk_r [BANKS];
  logic [BW-1:0]  oq_head_r, oq_tail_r;
  logic [BW:0]    oq_count_r;

  logic [BW-1:0] free_bank_c;
  logic          have_free_c;
  always_comb begin
    have_free_c = 1'b0;
    free_bank_c = '0;
    // Lowest free bank, scanned downwards so bank 0 wins -- an arbitrary but
    // FIXED choice, because a rotating one would make the order queue's job
    // harder for nothing.
    for (int b = BANKS - 1; b >= 0; b--)
      if (!bk_busy_r[b]) begin
        have_free_c = 1'b1;
        free_bank_c = BW'(b);
      end
  end

  function automatic logic [BW-1:0] next_bank(input logic [BW-1:0] p);
    next_bank = (p == BW'(BANKS - 1)) ? BW'(0) : BW'(p + BW'(1));
  endfunction

  assign mul_issue_o = (f_state_r == F_ISSUE);
  assign mul_a_0_o = 33'(f_v_r[f_comp_r][0]);
  assign mul_a_1_o = 33'(f_v_r[f_comp_r][1]);
  assign mul_a_2_o = 33'(f_v_r[f_comp_r][2]);
  assign mul_a_3_o = 33'(f_v_r[f_comp_r][3]);
  assign mul_b_0_o = 33'(f_v_r[f_comp_r][0]);
  assign mul_b_1_o = 33'(f_v_r[f_comp_r][1]);
  assign mul_b_2_o = 33'(f_v_r[f_comp_r][2]);
  assign mul_b_3_o = 33'(f_v_r[f_comp_r][3]);

  // THE FRONT ACCEPTS WHENEVER IT IS FREE AND THE ORDER QUEUE HAS ROOM. It does
  // NOT wait for the roots, and that gate is the whole difference between II 42
  // and this file -- the same change the curve service made to reach 13.
  assign v_ready_o = (f_state_r == F_IDLE) && (oq_count_r != (BW+1)'(BANKS));

  logic [BW-1:0] head_bk_c;
  assign head_bk_c = oq_bk_r[oq_head_r];
  assign r_valid_o = (oq_count_r != '0) && bk_busy_r[head_bk_c] && bk_done_r[head_bk_c];
  assign o0_0_o    = bk_res_r[head_bk_c][0];
  assign o0_1_o    = bk_res_r[head_bk_c][1];
  assign o0_2_o    = bk_res_r[head_bk_c][2];
  assign o0_3_o    = bk_res_r[head_bk_c][3];
  assign sat_rescale_o = bk_sat_r[head_bk_c];
  assign tag_o     = bk_tag_r[head_bk_c];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      f_state_r  <= F_IDLE;
      f_comp_r   <= 2'd0;
      f_ncomp_r  <= 2'd2;
      f_tag_r    <= 8'd0;
      oq_count_r <= '0;
      oq_head_r  <= '0;
      oq_tail_r  <= '0;
      for (int c = 0; c < 3; c++)
        for (int l = 0; l < LANES; l++) f_v_r[c][l] <= '0;
      for (int l = 0; l < LANES; l++) f_n2_r[l] <= 64'd0;
      for (int b = 0; b < BANKS; b++) begin
        bk_busy_r[b]    <= 1'b0;
        bk_done_r[b]    <= 1'b0;
        bk_tag_r[b]     <= 8'd0;
        bk_sat_r[b]     <= 4'd0;
        bk_started_r[b] <= '0;
        bk_got_r[b]     <= '0;
        oq_bk_r[b]      <= '0;
        for (int l = 0; l < LANES; l++) begin
          bk_res_r[b][l] <= '0;
          bk_n2_r[b][l]  <= 64'd0;
        end
      end
    end else begin
      case (f_state_r)
        F_IDLE: begin
          if (v_valid_i && v_ready_o) begin
            f_tag_r   <= tag_i;
            f_ncomp_r <= (mode_i == M_LEN3) ? 2'd3 : (mode_i == M_LEN2) ? 2'd2 : 2'd2;
            f_comp_r  <= 2'd0;
            for (int l = 0; l < LANES; l++) f_n2_r[l] <= 64'd0;

            if (mode_i == M_DIST2) begin
              f_v_r[0][0] <= sub_sat(a0_0_i, b0_0_i);
              f_v_r[0][1] <= sub_sat(a0_1_i, b0_1_i);
              f_v_r[0][2] <= sub_sat(a0_2_i, b0_2_i);
              f_v_r[0][3] <= sub_sat(a0_3_i, b0_3_i);
              f_v_r[1][0] <= sub_sat(a1_0_i, b1_0_i);
              f_v_r[1][1] <= sub_sat(a1_1_i, b1_1_i);
              f_v_r[1][2] <= sub_sat(a1_2_i, b1_2_i);
              f_v_r[1][3] <= sub_sat(a1_3_i, b1_3_i);
            end else begin
              f_v_r[0][0] <= a0_0_i; f_v_r[0][1] <= a0_1_i;
              f_v_r[0][2] <= a0_2_i; f_v_r[0][3] <= a0_3_i;
              f_v_r[1][0] <= a1_0_i; f_v_r[1][1] <= a1_1_i;
              f_v_r[1][2] <= a1_2_i; f_v_r[1][3] <= a1_3_i;
            end
            f_v_r[2][0] <= a2_0_i; f_v_r[2][1] <= a2_1_i;
            f_v_r[2][2] <= a2_2_i; f_v_r[2][3] <= a2_3_i;
            f_state_r <= F_ISSUE;
          end
        end

        // An instruction may not advance past a refused issue.
        F_ISSUE: if (mul_ready_i) f_state_r <= F_WAIT;

        F_WAIT: begin
          if (mul_valid_i) begin
            f_n2_r[0] <= f_n2_r[0] + mul_p_0_i[63:0];
            f_n2_r[1] <= f_n2_r[1] + mul_p_1_i[63:0];
            f_n2_r[2] <= f_n2_r[2] + mul_p_2_i[63:0];
            f_n2_r[3] <= f_n2_r[3] + mul_p_3_i[63:0];
            if (f_comp_r + 2'd1 >= f_ncomp_r) f_state_r <= F_HAND;
            else begin
              f_comp_r  <= f_comp_r + 2'd1;
              f_state_r <= F_ISSUE;
            end
          end
        end

        // Hand the finished n2 to a free bank. If both are busy the front WAITS
        // here rather than dropping it: back-pressure, not loss.
        F_HAND: begin
          if (have_free_c) begin
            bk_n2_r[free_bank_c][0] <= f_n2_r[0];
            bk_n2_r[free_bank_c][1] <= f_n2_r[1];
            bk_n2_r[free_bank_c][2] <= f_n2_r[2];
            bk_n2_r[free_bank_c][3] <= f_n2_r[3];
            bk_tag_r[free_bank_c]     <= f_tag_r;
            bk_busy_r[free_bank_c]    <= 1'b1;
            bk_done_r[free_bank_c]    <= 1'b0;
            bk_started_r[free_bank_c] <= '0;
            bk_got_r[free_bank_c]     <= '0;
            bk_sat_r[free_bank_c]     <= 4'd0;

            oq_bk_r[oq_tail_r] <= free_bank_c;
            oq_tail_r          <= next_bank(oq_tail_r);
            f_state_r          <= F_IDLE;
          end
        end

        default: f_state_r <= F_IDLE;
      endcase

      // ---- the banks, independently ---------------------------------------
      for (int b = 0; b < BANKS; b++) begin
        if (bk_busy_r[b] && !bk_done_r[b]) begin
          for (int l = 0; l < LANES; l++) begin
            if (rt_n_valid[b][l] && rt_n_ready[b][l]) bk_started_r[b][l] <= 1'b1;
            if (rt_r_valid[b][l] && !bk_got_r[b][l]) begin
              bk_got_r[b][l] <= 1'b1;
              if (rt_r[b][l] > 64'd2147483647) begin
                bk_sat_r[b][l] <= 1'b1;
                bk_res_r[b][l] <= 32'sd2147483647;
              end else begin
                bk_sat_r[b][l] <= 1'b0;
                bk_res_r[b][l] <= rt_r[b][l][31:0];
              end
            end
          end
          // Done only when every lane has LANDED, never while one is in flight.
          if (&bk_got_r[b]) bk_done_r[b] <= 1'b1;
        end
      end

      // ---- retiring the oldest entry ---------------------------------------
      if (r_valid_o && r_ready_i) begin
        bk_busy_r[head_bk_c] <= 1'b0;
        bk_done_r[head_bk_c] <= 1'b0;
        oq_head_r            <= next_bank(oq_head_r);
      end

      // ---- the occupancy, decided in ONE place -----------------------------
      // A push and a retire on the same clock must not each win separately.
      begin
        automatic logic push = (f_state_r == F_HAND) && have_free_c;
        automatic logic pop  = r_valid_o && r_ready_i;
        if (push && !pop)      oq_count_r <= oq_count_r + (BW+1)'(1);
        else if (pop && !push) oq_count_r <= oq_count_r - (BW+1)'(1);
      end
    end
  end

endmodule : zhao_field_v3_len
