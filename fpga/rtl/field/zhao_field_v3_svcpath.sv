// zhao_field_v3_svcpath.sv — the whole long-op path, composed: dispatcher,
// service, shared multiplier bank and writeback arbiter, with a rival on the
// bank so refusal is reachable.
//
// ENFORCED-BY: tests/differential/field_v3_svcpath_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS BEFORE THE EXECUTOR IS CHANGED
// ---------------------------------------------------------------------------
// Every defect that has cost real time in this engine was invisible to the
// sweeps of the blocks it lived between:
//
//     the executor's open-loop DOT     no mul_ready to refuse it
//     the curve service's hang         no mul_ready port at all
//     the dispatcher's missing imm     no port to carry it
//
// A sweep cannot mutate a port that does not exist, so scoring each block
// alone -- 25/25, 23 mutants, 17/17, all green -- could never have found any
// of them. Each was found within minutes of trying to wire two of them
// together.
//
// So this composition is built BEFORE the executor gains its long-op path,
// not after. The executor change is the larger and riskier piece; this one
// answers the questions that would otherwise be discovered inside it.
//
// ---------------------------------------------------------------------------
// THE THREE QUESTIONS IT EXISTS TO ANSWER
// ---------------------------------------------------------------------------
// 1. DOES A LONG OP SURVIVE THE WHOLE ROUND TRIP? Gather four contexts, issue
//    one four-point request, compute it on a bank that can refuse, drain the
//    results one register per clock, release each context. Every stage has
//    been proven alone; none has been proven to hand over.
//
// 2. IS THE SERVICE STARVED, OR IS THE ALU? The bank puts services above the
//    lanes and that is a REQUIREMENT there. The writeback arbiter's policy is
//    still open, and this is the block that can measure it: `wb_served_o` and
//    `wb_stalled_o` are per claimant, and the policy is an input.
//
// 3. WHAT DOES A REFUSED SERVICE ACTUALLY COST? The rival claimant makes the
//    bank say no on a schedule the test controls, so the price of contention
//    is measured rather than reasoned about.
//
// ---------------------------------------------------------------------------
// WHAT IS DELIBERATELY NOT HERE
// ---------------------------------------------------------------------------
// * THE EXECUTOR. Its long-op path does not exist yet, and inventing one here
//   would test this file's guess rather than the engine.
// * A SECOND SERVICE. One is enough to prove the round trip; two is what makes
//   the bank's fixed priority a starvation question, and that needs the curve
//   service attached, which is the next step rather than this one.
// * THE REGISTER FILE. The arbiter's output IS the write port, so the file
//   adds storage and no new behaviour. The test checks the port.
module zhao_field_v3_svcpath #(
    parameter int CONTEXTS = 8,
    parameter int REGS     = 32,
    parameter int TAGW     = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- from the executor: one context's long op --------------------------
    input  var logic                          long_valid_i,
    output var logic                          long_ready_o,
    input  var logic [$clog2(CONTEXTS)-1:0]   long_ctx_i,
    input  var logic [7:0]                    long_op_i,
    input  var logic [$clog2(REGS)-1:0]       long_dst_i,
    input  var logic signed [31:0]            long_s0_i,
    input  var logic signed [31:0]            long_s1_i,
    input  var logic signed [31:0]            long_s2_i,
    input  var logic signed [31:0]            long_s3_i,
    input  var logic        [31:0]            long_imm_i,
    input  var logic                          flush_i,

    // ---- the ALU's own writes, claimant 0 of the writeback arbiter ---------
    input  var logic                          alu_wb_valid_i,
    output var logic                          alu_wb_ready_o,
    input  var logic [$clog2(CONTEXTS)-1:0]   alu_wb_ctx_i,
    input  var logic [$clog2(REGS)-1:0]       alu_wb_reg_i,
    input  var logic signed [31:0]            alu_wb_data_i,

    // ---- a rival on the BANK, so the service can actually be refused -------
    // Without this the bank grants every request and the service's whole
    // refusal path is dead code. The engine learned that the expensive way:
    // its first sweep scored 4 of 11 because the test never drove the rival.
    input  var logic                          rival_req_i,
    output var logic                          rival_grant_o,
    // THE RIVAL'S REPLY, so a test can prove it was actually SERVED rather
    // than merely allowed to ask. "The rival contended" and "the rival got
    // nothing for a thousand clocks" are different findings, and a contention
    // test that cannot tell them apart is the vacuous kind this project has
    // already shipped once.
    output var logic                          rival_rsp_o,

    // 0 = ALU first, 1 = the drain first, 2 = round robin. An INPUT because
    // the answer is a measurement; see zhao_field_v3_wbarb.
    input  var logic [1:0]                    wb_policy_i,

    // ---- the register file's single write port -----------------------------
    output var logic                          wr_en_o,
    output var logic [$clog2(CONTEXTS)-1:0]   wr_ctx_o,
    output var logic [$clog2(REGS)-1:0]       wr_reg_o,
    output var logic signed [31:0]            wr_data_o,

    // ---- a context comes back to the ready set -----------------------------
    output var logic                          rel_valid_o,
    output var logic [$clog2(CONTEXTS)-1:0]   rel_ctx_o,

    // ---- evidence, per stage so a number names its own stage ---------------
    output var logic [31:0]                   groups_o,
    output var logic [31:0]                   partial_o,
    output var logic [31:0]                   drain_writes_o,
    output var logic [31:0]                   bank_grants_o,
    output var logic [31:0]                   bank_stall_lanes_o,
    output var logic [31:0]                   wb_served_o  [2],
    output var logic [31:0]                   wb_stalled_o [2],
    output var logic                          bank_desync_o,
    output var logic                          tag_mismatch_o,
    // A request reached the service carrying an op it does not implement.
    // Cannot happen while the dispatcher only routes NOISE2 and RIDGE here --
    // which is exactly why it is worth an output rather than a comment: the
    // day a second service is attached, this is the wire that says the routing
    // went wrong, instead of a wrong answer that looks like an arithmetic bug.
    output var logic                          wrong_op_o
);

  localparam int CTXW = $clog2(CONTEXTS);
  localparam int REGW = $clog2(REGS);

  // The op the one attached service implements. RIDGE and NOISE2 share it.
  localparam logic [7:0] OP_NOISE2 = 8'h1C;
  localparam logic [7:0] OP_RIDGE  = 8'h22;

  // Recognisable, so a routing bug into an unused lane looks wrong rather than
  // convincing -- the same constants zhao_probe_v3_engine ties its spare bank
  // lanes to, and the same argument.
  localparam logic signed [32:0] RIVAL_A = 33'sd3;
  localparam logic signed [32:0] RIVAL_B = 33'sd5;

  // ---- dispatcher <-> service --------------------------------------------
  logic               svc_valid, svc_ready;
  logic        [7:0]  svc_op;
  logic signed [31:0] svc_s0 [4], svc_s1 [4], svc_s2 [4], svc_s3 [4];
  logic        [31:0] svc_imm;
  logic [TAGW-1:0]    svc_tag;

  logic               rsp_valid, rsp_ready;
  logic [TAGW-1:0]    rsp_tag;
  logic signed [31:0] rsp_r0 [4], rsp_r1 [4], rsp_r2 [4];

  // ---- dispatcher's drain, claimant 1 of the writeback arbiter -----------
  logic               drain_valid, drain_ready;
  logic [CTXW-1:0]    drain_ctx;
  logic [REGW-1:0]    drain_reg;
  logic signed [31:0] drain_data;

  zhao_field_v3_dispatch #(
      .CONTEXTS(CONTEXTS), .REGS(REGS), .TAGW(TAGW)
  ) u_dispatch (
      .clk(clk), .rst_n(rst_n),
      .long_valid_i(long_valid_i), .long_ready_o(long_ready_o),
      .long_ctx_i(long_ctx_i), .long_op_i(long_op_i), .long_dst_i(long_dst_i),
      .long_s0_i(long_s0_i), .long_s1_i(long_s1_i), .long_s2_i(long_s2_i),
      .long_s3_i(long_s3_i),
      .long_imm_i(long_imm_i), .flush_i(flush_i),
      .svc_valid_o(svc_valid), .svc_ready_i(svc_ready),
      .svc_op_o(svc_op), .svc_s0_o(svc_s0), .svc_s1_o(svc_s1), .svc_s2_o(svc_s2),
      .svc_s3_o(svc_s3),
      .svc_imm_o(svc_imm), .svc_tag_o(svc_tag),
      .rsp_valid_i(rsp_valid), .rsp_ready_o(rsp_ready), .rsp_tag_i(rsp_tag),
      .rsp_r0_i(rsp_r0), .rsp_r1_i(rsp_r1), .rsp_r2_i(rsp_r2),
      .wb_valid_o(drain_valid), .wb_ready_i(drain_ready),
      .wb_ctx_o(drain_ctx), .wb_reg_o(drain_reg), .wb_data_o(drain_data),
      .rel_valid_o(rel_valid_o), .rel_ctx_o(rel_ctx_o),
      .groups_o(groups_o), .partial_o(partial_o), .writes_o(drain_writes_o),
      .tag_mismatch_o(tag_mismatch_o)
  );

  // `svc_s2` is carried by the dispatcher for ops that need a third operand
  // (ROT3, RING). The noise unit takes two, so the third is unread HERE and
  // not unread in general -- the waiver names that rather than hiding it.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [31:0] svc_s2_unused [4];
  logic signed [31:0] svc_s3_unused [4];
  /* verilator lint_on UNUSEDSIGNAL */
  assign svc_s2_unused = svc_s2;
  assign svc_s3_unused = svc_s3;

  // ---- the service, and the bank it borrows ------------------------------
  logic               nz_mul_issue, nz_mul_ready, nz_mul_valid;
  logic signed [32:0] nz_a [4], nz_b [4];
  logic signed [65:0] bank_p [4];

  logic [3:0] nz_sat_add, nz_sat_rescale;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [3:0] nz_sat_add_unused, nz_sat_rescale_unused;
  /* verilator lint_on UNUSEDSIGNAL */
  assign nz_sat_add_unused = nz_sat_add;
  assign nz_sat_rescale_unused = nz_sat_rescale;

  zhao_field_v3_noise u_noise (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(svc_valid), .v_ready_o(svc_ready),
      .is_ridge_i(svc_op == OP_RIDGE),
      .a0_0_i(svc_s0[0]), .a0_1_i(svc_s0[1]), .a0_2_i(svc_s0[2]), .a0_3_i(svc_s0[3]),
      .a1_0_i(svc_s1[0]), .a1_1_i(svc_s1[1]), .a1_2_i(svc_s1[2]), .a1_3_i(svc_s1[3]),
      .seed_i(svc_imm), .tag_i(svc_tag),
      .r_valid_o(rsp_valid), .r_ready_i(rsp_ready),
      .o0_0_o(rsp_r0[0]), .o0_1_o(rsp_r0[1]), .o0_2_o(rsp_r0[2]), .o0_3_o(rsp_r0[3]),
      .o1_0_o(rsp_r1[0]), .o1_1_o(rsp_r1[1]), .o1_2_o(rsp_r1[2]), .o1_3_o(rsp_r1[3]),
      .sat_add_o(nz_sat_add), .sat_rescale_o(nz_sat_rescale), .tag_o(rsp_tag),
      .mul_issue_o(nz_mul_issue), .mul_ready_i(nz_mul_ready),
      .mul_a_0_o(nz_a[0]), .mul_a_1_o(nz_a[1]), .mul_a_2_o(nz_a[2]), .mul_a_3_o(nz_a[3]),
      .mul_b_0_o(nz_b[0]), .mul_b_1_o(nz_b[1]), .mul_b_2_o(nz_b[2]), .mul_b_3_o(nz_b[3]),
      .mul_valid_i(nz_mul_valid),
      .mul_p_0_i(bank_p[0]), .mul_p_1_i(bank_p[1]),
      .mul_p_2_i(bank_p[2]), .mul_p_3_i(bank_p[3])
  );

  // THE SERVICE IS ASKED ONLY FOR OPS IT IMPLEMENTS, and this says so out
  // loud. It latches, because a single wrong request is the whole finding and
  // a level would be missed by any test that samples.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wrong_op_o <= 1'b0;
    end else if (svc_valid && svc_ready &&
                 (svc_op != OP_NOISE2) && (svc_op != OP_RIDGE)) begin
      wrong_op_o <= 1'b1;
    end
  end

  // NOISE2 and RIDGE write two registers per point; the third is always zero,
  // by the op's own law. Tied here rather than left dangling so the dispatcher
  // drains a defined value if a width-3 op is ever routed to this service by
  // mistake -- it would be wrong, and it would be wrong the same way twice
  // rather than differently each run.
  always_comb begin
    for (int l = 0; l < 4; l++) rsp_r2[l] = 32'sd0;
  end

  // ---- the shared bank: claimant 0 is the rival, 1 is the service ---------
  // The bank's own rule: claimant 0 is the ALU lanes, higher indices are
  // services, and with PRIO_SERVICES_FIRST the highest wins. The rival sits
  // in the lanes' slot so it loses to the service exactly as the lanes would.
  logic [1:0]         bank_req_valid, bank_req_ready, bank_rsp_valid;
  logic signed [32:0] bank_a [2][4], bank_b [2][4];
  logic [TAGW-1:0]    bank_tag [2];
  /* verilator lint_off UNUSEDSIGNAL */
  logic [TAGW-1:0]    bank_rsp_tag;
  /* verilator lint_on UNUSEDSIGNAL */

  always_comb begin
    bank_req_valid[0] = rival_req_i;
    bank_req_valid[1] = nz_mul_issue;
    for (int l = 0; l < 4; l++) begin
      bank_a[0][l] = RIVAL_A;
      bank_b[0][l] = RIVAL_B;
      bank_a[1][l] = nz_a[l];
      bank_b[1][l] = nz_b[l];
    end
    bank_tag[0] = 8'd0;
    bank_tag[1] = 8'd1;
  end

  assign rival_grant_o = bank_req_ready[0];
  assign rival_rsp_o   = bank_rsp_valid[0];
  assign nz_mul_ready  = bank_req_ready[1];
  assign nz_mul_valid  = bank_rsp_valid[1];

  zhao_field_v3_mulbank #(
      .CLAIMANTS(2), .PRIO_SERVICES_FIRST(1'b1), .TAGW(TAGW)
  ) u_bank (
      .clk(clk), .rst_n(rst_n),
      .req_valid_i(bank_req_valid), .req_ready_o(bank_req_ready),
      .req_a_i(bank_a), .req_b_i(bank_b), .req_tag_i(bank_tag),
      .rsp_valid_o(bank_rsp_valid), .rsp_p_o(bank_p), .rsp_tag_o(bank_rsp_tag),
      .grants_o(bank_grants_o), .stall_lanes_o(bank_stall_lanes_o),
      .desync_o(bank_desync_o)
  );

  // ---- the writeback arbiter: ALU is claimant 0, the drain is claimant 1 --
  logic [1:0]         wb_req_valid, wb_req_ready;
  logic [CTXW-1:0]    wb_ctx [2];
  logic [REGW-1:0]    wb_reg [2];
  logic signed [31:0] wb_data [2];

  always_comb begin
    wb_req_valid[0] = alu_wb_valid_i;
    wb_req_valid[1] = drain_valid;
    wb_ctx[0]  = alu_wb_ctx_i;
    wb_reg[0]  = alu_wb_reg_i;
    wb_data[0] = alu_wb_data_i;
    wb_ctx[1]  = drain_ctx;
    wb_reg[1]  = drain_reg;
    wb_data[1] = drain_data;
  end

  assign alu_wb_ready_o = wb_req_ready[0];
  assign drain_ready    = wb_req_ready[1];

  zhao_field_v3_wbarb #(
      .CLAIMANTS(2), .CONTEXTS(CONTEXTS), .REGS(REGS)
  ) u_wbarb (
      .clk(clk), .rst_n(rst_n),
      .policy_i(wb_policy_i),
      .req_valid_i(wb_req_valid), .req_ready_o(wb_req_ready),
      .req_ctx_i(wb_ctx), .req_reg_i(wb_reg), .req_data_i(wb_data),
      .wr_en_o(wr_en_o), .wr_ctx_o(wr_ctx_o), .wr_reg_o(wr_reg_o),
      .wr_data_o(wr_data_o),
      .served_o(wb_served_o), .stalled_o(wb_stalled_o)
  );

endmodule : zhao_field_v3_svcpath
