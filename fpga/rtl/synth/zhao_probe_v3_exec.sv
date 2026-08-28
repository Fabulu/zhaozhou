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

    // ---- the shared multiplier bank (this lane's claimant port) -----------
    // One lane of a four-wide bank. The other three belong to the other three
    // points of a vector group, which are replicas of this datapath.
    output var logic               mul_req_valid_o,
    input  var logic               mul_req_ready_i,
    output var logic signed [32:0] mul_req_a_o,
    output var logic signed [32:0] mul_req_b_o,
    input  var logic               mul_rsp_valid_i,
    input  var logic signed [65:0] mul_rsp_p_i,

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

  // ---- DOT sequencing: one multiplier, two or three products -------------
  // A DOT2 needs two products and a DOT3 three, against a budget of ONE
  // multiplier per lane (reports/Fieldv3.md: four 33-bit lanes at ~12 DSPs).
  // So they are sequenced, and the schedule is in
  // reports/FIELD_V3_DOT_SEQUENCING.md.
  //
  // THE ONE RULE THAT MAKES THIS SIMPLE: a DOT anywhere in the pipe freezes
  // ISSUE. Nothing can enter behind it, so no two instructions ever want the
  // multiplier in the same clock and there is no arbiter. Instructions AHEAD
  // of the DOT are unaffected -- each issued its own a0*b0 at its own S2, and
  // back-to-back issues are exactly what this multiplier supports.
  function automatic logic is_dot(input logic [7:0] op);
    is_dot = (op == 8'h10) || (op == 8'h11);  // OP_DOT2, OP_DOT3
  endfunction

  // The bank refused a request made this clock.
  logic mul_denied_c;
  assign mul_denied_c = mul_req_valid_o && !mul_req_ready_i;

  logic dot_inflight_c;
  assign dot_inflight_c = (s1_v_r && is_dot(s1_uop_r.op)) || (s2_v_r && is_dot(s2_op_r)) ||
                          (s3_v_r && is_dot(s3_op_r))     || (s4_v_r && is_dot(s4_op_r));

  always_comb begin
    ready_c = active_r & ~inflight_r;
    // A REFUSED REQUEST STALLS ISSUE. The register file's operands are live
    // for exactly one clock, so an instruction whose product the bank
    // declined to start cannot carry on -- its operands are gone next clock.
    issue_c = |ready_c && !dot_inflight_c && !hold_c && !mul_denied_c;
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

  // ---- S4: the product lands ----------------------------------------------
  // MEASURED, not assumed: zhao_field_mul is TWO clocks deep -- issue_i
  // registers the operands, and the product appears the clock after that. The
  // first version of this datapath put the ALU at S3, one clock early, and fed
  // it the PREVIOUS instruction's product. desync_o caught it on the first
  // run, which is the whole reason that signal is a port rather than a
  // comment. The operands are carried a second clock to meet the product.
  logic          s4_v_r;
  logic [CW-1:0] s4_ctx_r;
  logic [7:0]    s4_op_r;
  logic [RW-1:0] s4_dst_r;
  logic [31:0]   s4_imm_r;
  logic signed [31:0] s4_a0_r, s4_a1_r, s4_a2_r;
  logic signed [31:0] s4_b0_r, s4_b1_r, s4_b2_r;
  logic signed [31:0] s4_c_r;

  // ---- the register file, banked on register[1:0] -------------------------
  logic signed [31:0] rf_a0, rf_a1, rf_a2, rf_b0, rf_b1, rf_b2, rf_c;

  logic          rf_we_c;
  logic [CW-1:0] rf_wctx_c;
  logic [RW-1:0] rf_wreg_c;
  logic signed [31:0] rf_wdata_c;

  // THE FUNCTIONAL register file, not the fit probe. `zhao_probe_banked_rf`
  // measures the storage shape and says in its own header that it implements
  // no Field semantics: it addresses every bank with the SAME row, which
  // cannot read a group that crosses a multiple of four. This block used it
  // until 2026-08-28 and the differential passed 440 programs, because scalar
  // ops never read a+1 or a+2. The first DOT2 disagreed with the interpreter
  // on exactly the group starts that are 2 or 3 modulo 4.
  zhao_field_v3_rf #(
      .CONTEXTS(CTX),
      .REGS    (REGS)
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
  // Its operand ports are MULTIPLEXED by which stage currently needs a
  // product. Ordinary ops issue a0*b0 at S2. A DOT additionally issues
  // a1*b1 at S3 and, for DOT3, a2*b2 at S4 -- taking each pair from the
  // stage where it is still live, because the register file's outputs are
  // valid for exactly one clock.
  logic signed [65:0] prod_ab;
  logic               prod_valid;

  logic               mul_issue_c;
  logic signed [32:0] mul_a_c, mul_b_c;

  // How many products this instruction still owes, counted down at S4.
  logic [1:0] dot_cnt_r;
  logic signed [65:0] dot_acc_r;

  logic dot2_at_s4_c, dot3_at_s4_c, hold_c;
  assign dot2_at_s4_c = s4_v_r && (s4_op_r == 8'h10);
  assign dot3_at_s4_c = s4_v_r && (s4_op_r == 8'h11);
  // DOT2 owes one more product after its first, DOT3 owes two. The hold ends
  // when the last one has been accumulated.
  assign hold_c = (dot2_at_s4_c && dot_cnt_r < 2'd1) || (dot3_at_s4_c && dot_cnt_r < 2'd2);

  always_comb begin
    mul_issue_c = s2_v_r;
    mul_a_c     = 33'(rf_a0);
    mul_b_c     = 33'(rf_b0);
    if (s3_v_r && is_dot(s3_op_r)) begin
      mul_issue_c = 1'b1;
      mul_a_c     = 33'(s3_a1_r);
      mul_b_c     = 33'(s3_b1_r);
    end else if (dot3_at_s4_c && dot_cnt_r == 2'd0) begin
      mul_issue_c = 1'b1;
      mul_a_c     = 33'(s4_a2_r);
      mul_b_c     = 33'(s4_b2_r);
    end
  end

  // THE MULTIPLIER IS NOT OURS. It lives in zhao_field_v3_mulbank, shared
  // with the curve and distance services, because zhao_field_exec_shared
  // measured what happens when every op unit owns one: 79 DSPs of a 112-DSP
  // device, with nine units idle at any instant.
  //
  // This block was written with a private multiplier as a STATED
  // simplification. Removing it is what lets the composition exist at all.
  assign mul_req_valid_o = mul_issue_c;
  assign mul_req_a_o     = mul_a_c;
  assign mul_req_b_o     = mul_b_c;
  assign prod_ab         = mul_rsp_p_i;
  assign prod_valid      = mul_rsp_valid_i;

  // The sum is formed at the FULL 66-bit product width and rescaled ONCE by
  // the ALU. Rescaling each product and adding is a different answer, and
  // zfield's dot2/dot3 are the single-rounding form.
  logic signed [65:0] dot_sum_c;
  assign dot_sum_c = dot_acc_r + prod_ab;

  // ---- the op law, reused verbatim from the v2 engine ---------------------
  logic signed [31:0] alu_result;
  logic               alu_is_end, alu_writes, alu_unsupported;
  logic               alu_sat_add, alu_sat_mul, alu_sat_rescale;

  zhao_field_alu u_alu (
      .op_i  (s4_op_r),
      .imm_i (s4_imm_r),
      .a0_i  (s4_a0_r), .a1_i(s4_a1_r), .a2_i(s4_a2_r),
      .b0_i  (s4_b0_r), .b1_i(s4_b1_r), .b2_i(s4_b2_r),
      .c_i   (s4_c_r),
      .prod_ab_i(prod_ab),
      // The accumulated sum, at full product width. The ALU rescales it once.
      // Only one of these is read per op, and both carry the same accumulator
      // because only one DOT is ever in flight -- the issue freeze guarantees
      // it.
      .dot2_i(dot_sum_c),
      .dot3_i(dot_sum_c),
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
  //
  // AND IT MUST NOT WRITE. The ALU KNOWS OP_DOT2/OP_DOT3 -- they are real arms
  // of its decode, not the `default` refusal -- so it leaves writes_o HIGH and
  // produces a result computed from the zero I hand it on dot2_i/dot3_i. Left
  // to itself the block would therefore flag the op as unsupported AND write
  // the garbage anyway, which is the worst of both: a wrong value in a live
  // register, under a flag that says it was refused.
  //
  // Found 2026-08-28 by the test written to close mutant X11, which had
  // survived precisely because nothing checked that a refused op leaves the
  // register file alone. The header of this file already CLAIMED the write was
  // refused; the claim was wrong until this line existed.
  // DOT USED TO BE REFUSED HERE and is now IMPLEMENTED, so this term is gone
  // from the write enable. What remains refused is an opcode the ALU itself
  // does not know, via its own `default` arm clearing writes_o -- a different
  // gate, and the one mutant X11 attacks.
  logic dot_here_c;
  assign dot_here_c = 1'b0;

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
      rf_we_c    = s4_v_r && alu_writes && !alu_is_end && !dot_here_c;
      rf_wctx_c  = s4_ctx_r;
      rf_wreg_c  = s4_dst_r;
      rf_wdata_c = alu_result;
    end
  end

  assign wb_valid_o = s4_v_r && alu_writes && !alu_is_end && !dot_here_c;
  assign wb_ctx_o   = s4_ctx_r;
  assign wb_reg_o   = s4_dst_r;
  assign wb_data_o  = alu_result;

  assign done_valid_o = s4_v_r && alu_is_end;
  assign done_ctx_o   = s4_ctx_r;

  // ---- the pipe -----------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s1_v_r        <= 1'b0;
      s2_v_r        <= 1'b0;
      s3_v_r        <= 1'b0;
      s4_v_r        <= 1'b0;
      active_r      <= '0;
      inflight_r    <= '0;
      unsupported_o <= 1'b0;
      desync_o      <= 1'b0;
      dot_acc_r     <= '0;
      dot_cnt_r     <= 2'd0;
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

      // THE DOT HOLD, and it lives OUTSIDE the freeze below on purpose: this
      // is the counter that ENDS the hold, so gating it with `!hold_c` would
      // deadlock the pipe -- which is exactly what the first version did, and
      // the barrel test caught it as every context failing to finish.
      if (hold_c) begin
        dot_acc_r <= dot_sum_c;
        dot_cnt_r <= dot_cnt_r + 2'd1;
      end else if (s4_v_r) begin
        dot_acc_r <= '0;
        dot_cnt_r <= 2'd0;
      end

      // Every stage advance below is gated on `!hold_c`: a held DOT must not
      // be overwritten, and nothing behind it may move past it.
      if (!hold_c) begin
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

      if (s4_v_r != prod_valid) desync_o <= 1'b1;


      // S3 -> S4: carry a second clock so the operands meet their product
      s4_v_r   <= s3_v_r;
      s4_ctx_r <= s3_ctx_r;
      s4_op_r  <= s3_op_r;
      s4_dst_r <= s3_dst_r;
      s4_imm_r <= s3_imm_r;
      s4_a0_r  <= s3_a0_r;
      s4_a1_r  <= s3_a1_r;
      s4_a2_r  <= s3_a2_r;
      s4_b0_r  <= s3_b0_r;
      s4_b1_r  <= s3_b1_r;
      s4_b2_r  <= s3_b2_r;
      s4_c_r   <= s3_c_r;

      // S4: retire
      if (s4_v_r) begin
        inflight_r[s4_ctx_r] <= 1'b0;
        if (alu_is_end) begin
          active_r[s4_ctx_r] <= 1'b0;
        end else begin
          pc_r[s4_ctx_r] <= pc_r[s4_ctx_r] + PW'(1);
        end
        if (alu_unsupported || dot_here_c) unsupported_o <= 1'b1;
        if (alu_sat_add) sat_add_o <= 1'b1;
        if (alu_sat_mul) sat_mul_o <= 1'b1;
        if (alu_sat_rescale) sat_rescale_o <= 1'b1;
      end
      end  // !hold_c
    end
  end

endmodule

`default_nettype wire
