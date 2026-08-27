// zhao_probe_v3_exec.sv — Field v3 Phase 4: the vector executor DATAPATH,
// one lane.
//
// WHERE THIS SITS
// ---------------
//   prepared descriptor -> walker -> [THIS] -> patch accumulator -> cache
//
// The walker (zhao_probe_walk_earth) generates the points. The accumulator
// (zhao_probe_patch_acc) reduces the results. This is the middle: it runs a
// context's uops to END and produces that point's four output lanes.
//
// ONE LANE, AND THAT IS THE POINT. The engine is four-wide because a vector
// group is four points, and the four lanes are INDEPENDENT REPLICAS of this
// datapath sharing one instruction stream. Measuring one lane and multiplying
// is honest for storage and near-honest for logic; measuring four lanes is
// the next fit, not this one. Nothing here is written as though four lanes
// were free.
//
// WHY THE OPERANDS ARRIVE THE WAY THEY DO
// ----------------------------------------
// `zhao_field_alu` consumes SEVEN operands -- a, a+1, a+2, b, b+1, b+2, c --
// and `zhao_probe_banked_rf` delivers exactly those seven in one clock by
// banking on `register[1:0]` with three read replicas. That correspondence is
// not a coincidence and it is the argument for the banking: `a, a+1, a+2`
// necessarily land in three DIFFERENT residues, so no bank is ever asked for
// more than three reads.
//
// It is also the argument AGAINST the alternative sizing in
// `reports/Fieldv3.md`, which cuts the file as four lanes x three readers.
// Three readers per lane cannot serve seven operands in a clock, so every
// three-member op would have to be sequenced. See
// `reports/FIELD_V3_EXECUTOR_REGFILE.md` -- both shapes cost twelve memories
// of 8,192 bits, so this is capability at equal price, not a trade.
//
// WHAT THIS INCREMENT DOES NOT DO, STATED SO IT IS NOT MISTAKEN FOR DONE
// -----------------------------------------------------------------------
//   * NO DOT2/DOT3. Those need two and three products, and the brief budgets
//     "four 33-bit lanes at about 12 DSPs" -- one multiplier per lane. So a
//     dot product must be SEQUENCED over two or three clocks, and how much
//     that costs is a measurement, not a guess. `op_unsupported_o` from the
//     ALU is surfaced rather than hidden, so an unsupported op is loud.
//   * NO SCHEDULER. `zhao_probe_ctx_fifo` is the measured ready-context FIFO
//     and belongs in the composition, not in the datapath. This probe uses a
//     plain lowest-ready scan so that what is being measured is the DATAPATH.
//   * NO SERVICES. Curve and distance have their own probes and their own
//     queues.
//
// THE BARREL PROPERTY, which is the whole reason for contexts. One
// instruction is in flight per context (the brief's rule), and the datapath
// is five stages deep. So a single context issues one uop every five clocks,
// and the pipeline is only full when at least five contexts are ready. That
// is not a defect to be bypassed away -- it is why the engine is barrelled,
// and the probe reports occupancy so the effect is measured rather than
// assumed.
//
// Law:
//   reports/Fieldv3.md              Phase 4; prepared vector fabric
//   reports/PIPELINEINGHINTS        the banked register file directive
//   design/contracts/FIELD.SEQ.CORE.md   one semantic engine, profile
//                                   adapters permitted; v3 prepared fabric
//   spec/form/field-ir.md           canonical op semantics
//   reference/include/zfield/zfield_plan.hpp  execute_point, the oracle

`default_nettype none

module zhao_probe_v3_exec #(
    parameter int CTX  = 8,   // contexts in flight
    parameter int REGS = 32,  // vector registers per context
    parameter int PLAN = 32   // uops per context
) (
    input var logic clk,
    input var logic rst_n,

    // ---- uop store write (host loads a context's program) -----------------
    input var logic                       up_we_i,
    input var logic [$clog2(CTX)-1:0]     up_ctx_i,
    input var logic [$clog2(PLAN)-1:0]    up_pc_i,
    input var logic [7:0]                 up_op_i,
    input var logic [$clog2(REGS)-1:0]    up_dst_i,
    input var logic [$clog2(REGS)-1:0]    up_a_i,
    input var logic [$clog2(REGS)-1:0]    up_b_i,
    input var logic [$clog2(REGS)-1:0]    up_c_i,
    input var logic [31:0]                up_imm_i,

    // ---- register preload (host writes a context's input registers) -------
    input var logic                    pre_we_i,
    input var logic [$clog2(CTX)-1:0]  pre_ctx_i,
    input var logic [$clog2(REGS)-1:0] pre_reg_i,
    input var logic signed [31:0]      pre_data_i,

    // ---- run control -------------------------------------------------------
    input var logic                   start_i,
    input var logic [$clog2(CTX)-1:0] start_ctx_i,

    // ---- observation -------------------------------------------------------
    output var logic                    wb_valid_o,   // a result was written
    output var logic [$clog2(CTX)-1:0]  wb_ctx_o,
    output var logic [$clog2(REGS)-1:0] wb_reg_o,
    output var logic signed [31:0]      wb_data_o,

    output var logic                   done_valid_o,  // a context hit END
    output var logic [$clog2(CTX)-1:0] done_ctx_o,

    output var logic [CTX-1:0] active_o,
    output var logic           unsupported_o,  // an op this increment omits

    // ---- saturation ledger, ORed across the run ---------------------------
    output var logic sat_add_o,
    output var logic sat_mul_o,
    output var logic sat_rescale_o,

    // ---- counters ----------------------------------------------------------
    output var logic [31:0] uops_issued_o,
    output var logic [31:0] idle_clocks_o,  // no context was ready to issue

    // The multiplier's own valid must arrive in the same clock as S3. If it
    // ever does not, the product being fed to the ALU belongs to a DIFFERENT
    // instruction, which is a wrong answer rather than a slow one -- so it is
    // latched and reported instead of being assumed away.
    output var logic desync_o
);

  localparam int CW = $clog2(CTX);
  localparam int RW = $clog2(REGS);
  localparam int PW = $clog2(PLAN);

  // ---- the uop store -----------------------------------------------------
  // One entry per (context, pc). Written by the host, read at issue.
  typedef struct packed {
    logic [7:0]    op;
    logic [RW-1:0] dst;
    logic [RW-1:0] a;
    logic [RW-1:0] b;
    logic [RW-1:0] c;
    logic [31:0]   imm;
  } uop_t;

  uop_t store[0:(CTX*PLAN)-1];

  // ---- per-context state -------------------------------------------------
  logic [PW-1:0] pc_r[0:CTX-1];
  logic [CTX-1:0] active_r;    // started, not yet ENDed
  logic [CTX-1:0] inflight_r;  // has an instruction in the pipe

  assign active_o = active_r;

  // ---- issue: lowest ready context ---------------------------------------
  logic           issue_c;
  logic [CW-1:0]  issue_ctx_c;
  logic [CTX-1:0] ready_c;

  always_comb begin
    ready_c = active_r & ~inflight_r;
    issue_c = |ready_c;
    issue_ctx_c = '0;
    for (int i = CTX - 1; i >= 0; i--) if (ready_c[i]) issue_ctx_c = CW'(i);
  end

  // ---- S1: the fetched uop ------------------------------------------------
  logic          s1_v_r;
  logic [CW-1:0] s1_ctx_r;
  uop_t          s1_uop_r;

  // ---- S2: operands presented (RF read lands this clock) ------------------
  // Like S3, S2 carries only what survives the register-file read: the
  // operand register NUMBERS were spent addressing the file at S1.
  logic          s2_v_r;
  logic [CW-1:0] s2_ctx_r;
  logic [7:0]    s2_op_r;
  logic [RW-1:0] s2_dst_r;
  logic [31:0]   s2_imm_r;

  // ---- S3: product available ---------------------------------------------
  // S3 carries only what is still needed: the operand registers were consumed
  // at S1 by the register-file read and carrying them further would be dead
  // width in every pipeline register.
  logic          s3_v_r;
  logic [CW-1:0] s3_ctx_r;
  logic [7:0]    s3_op_r;
  logic [RW-1:0] s3_dst_r;
  logic [31:0]   s3_imm_r;
  logic signed [31:0] s3_a0_r, s3_a1_r, s3_a2_r;
  logic signed [31:0] s3_b0_r, s3_b1_r, s3_b2_r;
  logic signed [31:0] s3_c_r;

  // ---- the register file, banked on register[1:0] -------------------------
  logic signed [31:0] rf_a0, rf_a1, rf_a2, rf_b0, rf_b1, rf_b2, rf_c;

  logic          rf_we_c;
  logic [CW-1:0] rf_wctx_c;
  logic [RW-1:0] rf_wreg_c;
  logic signed [31:0] rf_wdata_c;

  zhao_probe_banked_rf #(
      .CONTEXTS(CTX),
      .REGS    (REGS),
      .BANKS   (4),
      .COPIES  (3)
  ) u_rf (
      .clk    (clk),
      .wr_en_i(rf_we_c),
      .wr_ctx_i(rf_wctx_c),
      .wr_reg_i(rf_wreg_c),
      .wr_data_i(rf_wdata_c),
      .rd_ctx_i(s1_ctx_r),
      .rd_a_i (s1_uop_r.a),
      .rd_b_i (s1_uop_r.b),
      .rd_c_i (s1_uop_r.c),
      .a0_o(rf_a0), .a1_o(rf_a1), .a2_o(rf_a2),
      .b0_o(rf_b0), .b1_o(rf_b1), .b2_o(rf_b2),
      .c_o (rf_c)
  );

  // ---- one multiplier, registered both sides ------------------------------
  logic signed [65:0] prod_ab;
  logic               prod_valid;

  zhao_field_mul u_mul (
      .clk    (clk),
      .rst_n  (rst_n),
      .issue_i(s2_v_r),
      .a_i    (33'(rf_a0)),
      .b_i    (33'(rf_b0)),
      .p_o    (prod_ab),
      .p_valid_o(prod_valid)
  );

  // ---- the op law, reused verbatim from the v2 engine ---------------------
  logic signed [31:0] alu_result;
  logic               alu_is_end, alu_writes, alu_unsupported;
  logic               alu_sat_add, alu_sat_mul, alu_sat_rescale;

  zhao_field_alu u_alu (
      .op_i  (s3_op_r),
      .imm_i (s3_imm_r),
      .a0_i  (s3_a0_r), .a1_i(s3_a1_r), .a2_i(s3_a2_r),
      .b0_i  (s3_b0_r), .b1_i(s3_b1_r), .b2_i(s3_b2_r),
      .c_i   (s3_c_r),
      .prod_ab_i(prod_ab),
      // DOT2/DOT3 are NOT part of this increment. Feeding them zero would be a
      // wrong ANSWER; the ALU's own op_unsupported_o is the honest signal, and
      // it is surfaced on a port rather than swallowed.
      .dot2_i(66'sd0),
      .dot3_i(66'sd0),
      .result_o(alu_result),
      .is_end_o(alu_is_end),
      .writes_o(alu_writes),
      .op_unsupported_o(alu_unsupported),
      .sat_add_o(alu_sat_add),
      .sat_mul_o(alu_sat_mul),
      .sat_rescale_o(alu_sat_rescale)
  );

  // A DOT op reaching this increment is unsupported even though the ALU could
  // name it, because the products it needs were never computed.
  logic dot_here_c;
  assign dot_here_c = s3_v_r && (s3_op_r == 8'h10 || s3_op_r == 8'h11);

  // ---- writeback ----------------------------------------------------------
  // The host preload wins the port when it is asserted; the machine is not
  // running during preload, so there is no contention to arbitrate.
  always_comb begin
    if (pre_we_i) begin
      rf_we_c    = 1'b1;
      rf_wctx_c  = pre_ctx_i;
      rf_wreg_c  = pre_reg_i;
      rf_wdata_c = pre_data_i;
    end else begin
      rf_we_c    = s3_v_r && alu_writes && !alu_is_end;
      rf_wctx_c  = s3_ctx_r;
      rf_wreg_c  = s3_dst_r;
      rf_wdata_c = alu_result;
    end
  end

  assign wb_valid_o = s3_v_r && alu_writes && !alu_is_end;
  assign wb_ctx_o   = s3_ctx_r;
  assign wb_reg_o   = s3_dst_r;
  assign wb_data_o  = alu_result;

  assign done_valid_o = s3_v_r && alu_is_end;
  assign done_ctx_o   = s3_ctx_r;

  // ---- the pipe -----------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s1_v_r        <= 1'b0;
      s2_v_r        <= 1'b0;
      s3_v_r        <= 1'b0;
      active_r      <= '0;
      inflight_r    <= '0;
      unsupported_o <= 1'b0;
      desync_o      <= 1'b0;
      sat_add_o     <= 1'b0;
      sat_mul_o     <= 1'b0;
      sat_rescale_o <= 1'b0;
      uops_issued_o <= 32'd0;
      idle_clocks_o <= 32'd0;
      for (int i = 0; i < CTX; i++) pc_r[i] <= '0;
    end else begin
      if (up_we_i)
        store[(int'(up_ctx_i) * PLAN) + int'(up_pc_i)] <=
            '{op: up_op_i, dst: up_dst_i, a: up_a_i, b: up_b_i, c: up_c_i, imm: up_imm_i};

      if (start_i) begin
        active_r[start_ctx_i] <= 1'b1;
        pc_r[start_ctx_i]     <= '0;
      end

      // S0 -> S1: issue and fetch
      s1_v_r <= issue_c;
      if (issue_c) begin
        s1_ctx_r              <= issue_ctx_c;
        s1_uop_r              <= store[(int'(issue_ctx_c) * PLAN) + int'(pc_r[issue_ctx_c])];
        inflight_r[issue_ctx_c] <= 1'b1;
        uops_issued_o         <= uops_issued_o + 32'd1;
      end else if (|active_r) begin
        idle_clocks_o <= idle_clocks_o + 32'd1;
      end

      // S1 -> S2: RF read is in flight
      s2_v_r   <= s1_v_r;
      s2_ctx_r <= s1_ctx_r;
      s2_op_r  <= s1_uop_r.op;
      s2_dst_r <= s1_uop_r.dst;
      s2_imm_r <= s1_uop_r.imm;

      // S2 -> S3: capture the operands the RF just produced
      s3_v_r   <= s2_v_r;
      s3_ctx_r <= s2_ctx_r;
      s3_op_r  <= s2_op_r;
      s3_dst_r <= s2_dst_r;
      s3_imm_r <= s2_imm_r;
      s3_a0_r  <= rf_a0;
      s3_a1_r  <= rf_a1;
      s3_a2_r  <= rf_a2;
      s3_b0_r  <= rf_b0;
      s3_b1_r  <= rf_b1;
      s3_b2_r  <= rf_b2;
      s3_c_r   <= rf_c;

      if (s3_v_r != prod_valid) desync_o <= 1'b1;

      // S3: retire
      if (s3_v_r) begin
        inflight_r[s3_ctx_r] <= 1'b0;
        if (alu_is_end) begin
          active_r[s3_ctx_r] <= 1'b0;
        end else begin
          pc_r[s3_ctx_r] <= pc_r[s3_ctx_r] + PW'(1);
        end
        if (alu_unsupported || dot_here_c) unsupported_o <= 1'b1;
        if (alu_sat_add) sat_add_o <= 1'b1;
        if (alu_sat_mul) sat_mul_o <= 1'b1;
        if (alu_sat_rescale) sat_rescale_o <= 1'b1;
      end
    end
  end

endmodule

`default_nettype wire
