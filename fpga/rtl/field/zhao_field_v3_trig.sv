// zhao_field_v3_trig.sv — OP_SIN and OP_COS, streamed.
//
// ENFORCED-BY: tests/differential/field_v3_trig_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT THIS IS FOR
// ---------------------------------------------------------------------------
// `tools/field/measure_earth_ops.cpp` planned the three shipped Earth programs
// and found three canonical opcodes they use that the hardware did not serve:
// DIST2, SIN and COS. This is two of them, and the arithmetic already existed
// as `zhao_field_sin`, which the rotation service has trusted since it closed.
//
// So this block is a SEQUENCER and nothing else. Its whole job is to keep four
// points per group moving through one lookup unit.
//
//     OP_SIN   dst[0] = fx_sin(angle16{(uint16_t)src[0]})
//     OP_COS   dst[0] = fx_cos(angle16{(uint16_t)src[0]})
//
// THE LOW SIXTEEN BITS ARE THE ANGLE and the upper half is ignored rather than
// being an error -- the same law the rotation service states for its own angle
// port, so a caller that leaves rubbish there gets a defined answer, and the
// same one the software gives.
//
// ---------------------------------------------------------------------------
// STREAMED, BECAUSE LATENCY IS NOT THROUGHPUT
// ---------------------------------------------------------------------------
// This block first shipped as one group at a time: IDLE, walk four lanes, HOLD,
// IDLE. It measured 22 clocks end to end and its initiation interval was the
// same 22, because `v_ready` was gated on being idle.
//
// That is the pattern an audit found in six of the seven services on this path,
// and it is why Earth missed its 850,000-clock frame budget by 9x. The curve
// service is the exception and the worked example: latency 32, II 13.
//
// `zhao_field_sin` is a TWO-STAGE PIPELINE. It accepts a new angle every clock
// and answers two clocks later. The old walk used one lookup every clock for
// four clocks and then sat idle for eighteen. Nothing about the arithmetic
// required that.
//
// So lookups now stream continuously across group boundaries. Two group slots
// are in flight; addresses go out one per clock; a two-deep shadow pipeline
// carries {slot, lane} alongside each lookup so the answer two clocks later
// lands in the right place. A group retires when all four of its lanes have
// LANDED -- tracked by `got_r`, never by a cycle count, because "declared
// finished while the last capture was still in flight" is a bug this engine
// has now paid for three times.
//
// The initiation interval becomes four clocks -- one per lane -- instead of
// twenty-two.
module zhao_field_v3_trig #(
    parameter int LANES = 4
) (
    input var logic clk,
    input var logic rst_n,

    // ---- request: one four-point group -------------------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic               is_cos_i,   // 0 = OP_SIN, 1 = OP_COS
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic signed [31:0] a0_0_i, a0_1_i, a0_2_i, a0_3_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic        [ 7:0] tag_i,

    // ---- reply -------------------------------------------------------------
    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] o0_0_o, o0_1_o, o0_2_o, o0_3_o,
    output var logic        [ 7:0] tag_o
);

  // ---- two group slots -----------------------------------------------------
  logic               sl_busy_r [2];
  logic        [15:0] sl_ang_r  [2][LANES];
  logic               sl_cos_r  [2];
  logic        [ 7:0] sl_tag_r  [2];
  logic signed [31:0] sl_res_r  [2][LANES];
  logic [LANES-1:0]   sl_got_r  [2];

  // Accept order, by pointer rather than by shifting -- a push and a retire on
  // the same clock must not both write the same entry, which is a bug this
  // engine has already paid for once in the distance service.
  logic       oq_slot_r [2];
  logic       oq_head_r, oq_tail_r;
  logic [1:0] oq_count_r;

  logic free_slot_c, have_free_c;
  always_comb begin
    have_free_c = 1'b0;
    free_slot_c = 1'b0;
    if (!sl_busy_r[0]) begin
      have_free_c = 1'b1;
      free_slot_c = 1'b0;
    end else if (!sl_busy_r[1]) begin
      have_free_c = 1'b1;
      free_slot_c = 1'b1;
    end
  end

  // A lane is addressed exactly once. `sl_addr_done_r` is the honest counter:
  // 4 means every lane of that slot has gone out.
  logic [2:0] sl_addr_r [2];
  logic       fire_c;
  logic       fire_slot_c;
  logic [1:0] fire_lane_c;
  always_comb begin
    fire_c      = 1'b0;
    fire_slot_c = 1'b0;
    fire_lane_c = 2'd0;
    for (int k = 0; k < 2; k++) begin
      automatic logic sl = oq_slot_r[(oq_head_r + 1'(k)) & 1'b1];
      if (!fire_c && ((2'(k)) < oq_count_r) && sl_busy_r[sl] && (sl_addr_r[sl] < 3'd4)) begin
        fire_c      = 1'b1;
        fire_slot_c = sl;
        fire_lane_c = sl_addr_r[sl][1:0];
      end
    end
  end

  logic signed [31:0] sin_result;
  zhao_field_sin u_sin (
      .clk(clk),
      .angle_i(sl_ang_r[fire_slot_c][fire_lane_c]),
      .is_cos_i(sl_cos_r[fire_slot_c]),
      .result_o(sin_result)
  );

  // ---- the shadow pipeline: two deep, matching the lookup's latency --------
  logic       sh_v_r    [2];
  logic       sh_slot_r [2];
  logic [1:0] sh_lane_r [2];

  assign v_ready_o = have_free_c && (oq_count_r != 2'd2);

  logic head_sl_c;
  assign head_sl_c = oq_slot_r[oq_head_r];
  assign r_valid_o = (oq_count_r != 2'd0) && sl_busy_r[head_sl_c] &&
                     (sl_got_r[head_sl_c] == 4'hF);
  assign o0_0_o = sl_res_r[head_sl_c][0];
  assign o0_1_o = sl_res_r[head_sl_c][1];
  assign o0_2_o = sl_res_r[head_sl_c][2];
  assign o0_3_o = sl_res_r[head_sl_c][3];
  assign tag_o  = sl_tag_r[head_sl_c];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      oq_head_r  <= 1'b0;
      oq_tail_r  <= 1'b0;
      oq_count_r <= 2'd0;
      for (int b = 0; b < 2; b++) begin
        sl_busy_r[b]   <= 1'b0;
        sl_cos_r[b]    <= 1'b0;
        sl_tag_r[b]    <= 8'd0;
        sl_got_r[b]    <= '0;
        sl_addr_r[b]   <= 3'd0;
        oq_slot_r[b]   <= 1'b0;
        sh_v_r[b]      <= 1'b0;
        sh_slot_r[b]   <= 1'b0;
        sh_lane_r[b]   <= 2'd0;
        for (int l = 0; l < LANES; l++) begin
          sl_ang_r[b][l] <= 16'd0;
          sl_res_r[b][l] <= '0;
        end
      end
    end else begin
      // ---- accept ---------------------------------------------------------
      if (v_valid_i && v_ready_o) begin
        sl_ang_r[free_slot_c][0] <= a0_0_i[15:0];
        sl_ang_r[free_slot_c][1] <= a0_1_i[15:0];
        sl_ang_r[free_slot_c][2] <= a0_2_i[15:0];
        sl_ang_r[free_slot_c][3] <= a0_3_i[15:0];
        sl_cos_r[free_slot_c]    <= is_cos_i;
        sl_tag_r[free_slot_c]    <= tag_i;
        sl_busy_r[free_slot_c]   <= 1'b1;
        sl_got_r[free_slot_c]    <= '0;
        sl_addr_r[free_slot_c]   <= 3'd0;
        oq_slot_r[oq_tail_r]     <= free_slot_c;
        oq_tail_r                <= ~oq_tail_r;
      end

      // ---- issue one lookup per clock -------------------------------------
      sh_v_r[1]    <= sh_v_r[0];
      sh_slot_r[1] <= sh_slot_r[0];
      sh_lane_r[1] <= sh_lane_r[0];
      sh_v_r[0]    <= fire_c;
      sh_slot_r[0] <= fire_slot_c;
      sh_lane_r[0] <= fire_lane_c;
      if (fire_c) sl_addr_r[fire_slot_c] <= sl_addr_r[fire_slot_c] + 3'd1;

      // ---- capture, two clocks behind -------------------------------------
      // The answer on the port now belongs to the lookup that left two clocks
      // ago, and the shadow pipeline says which slot and lane that was. No
      // cycle counting, so there is no off-by-one to get wrong.
      if (sh_v_r[1]) begin
        sl_res_r[sh_slot_r[1]][sh_lane_r[1]] <= sin_result;
        sl_got_r[sh_slot_r[1]][sh_lane_r[1]] <= 1'b1;
      end

      // ---- retire the oldest ----------------------------------------------
      if (r_valid_o && r_ready_i) begin
        sl_busy_r[head_sl_c] <= 1'b0;
        oq_head_r            <= ~oq_head_r;
      end

      // ---- occupancy, decided in ONE place --------------------------------
      begin
        automatic logic push = v_valid_i && v_ready_o;
        automatic logic pop  = r_valid_o && r_ready_i;
        if (push && !pop)      oq_count_r <= oq_count_r + 2'd1;
        else if (pop && !push) oq_count_r <= oq_count_r - 2'd1;
      end
    end
  end

endmodule : zhao_field_v3_trig
