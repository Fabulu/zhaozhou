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
    //
    // ENFORCED-BY: tests/differential/field_v3_svcpath_directed.cpp:main
    //
    // Every group asserts wrong_op_o == 0, and mutant V23 -- which makes the
    // detector fire on the ops that ARE implemented -- is caught, so the wire
    // is known to be live rather than merely tied low.
    output var logic                          wrong_op_o,

    // ---- the curve service's table cache -----------------------------------
    //
    // TABLES COME FROM OUTSIDE AND THAT IS NOT A PLACEHOLDER. CURVE, DCURVE
    // and SPLINE read a knot table the program supplies; the service path does
    // not synthesise one and must not pretend to. In the finished machine the
    // command stream fills this port. Here the differential drives it, which
    // is the same wire either way -- what would be wrong is inventing a table
    // inside the service so the block looks self-contained.
    input  var logic                          tl_we_i,
    input  var logic [1:0]                    tl_tbl_i,
    input  var logic [5:0]                    tl_idx_i,
    input  var logic signed [31:0]            tl_x_i,
    input  var logic signed [31:0]            tl_y_i,
    input  var logic signed [31:0]            tl_dy_i,
    input  var logic                          tl_commit_i,
    input  var logic [6:0]                    tl_n_i,

    // ---- the uniform (scalar) bank's load port -----------------------------
    //
    // The ARM runs the plan's PREP block once per association and writes the
    // answers here. Same reasoning as the table port above: the values belong
    // to the PROGRAM, and a service that synthesised its own uniforms would
    // look self-contained while hiding the missing plumbing.
    //
    // The address is SIXTEEN bits against a six-bit bank on purpose -- the
    // planner's slot numbers are uint16_t, so an overflow has to be
    // representable at this boundary or it wraps silently in the wiring.
    input  var logic                          sb_we_i,
    input  var logic [15:0]                   sb_waddr_i,
    input  var logic signed [31:0]            sb_wdata_i,
    output var logic                          sb_bad_o,

    // Latches if a prepared-ring instruction sets the immediate's reserved
    // bits -- a program written against a later encoding than this silicon.
    output var logic                          imm_bad_o
);

  localparam int CTXW = $clog2(CONTEXTS);
  localparam int REGW = $clog2(REGS);

  // The ops the attached services implement. RIDGE and NOISE2 share the noise
  // unit; CURVE, DCURVE and SPLINE share the curve service.
  localparam logic [7:0] OP_NOISE2 = 8'h1C;
  localparam logic [7:0] OP_RIDGE  = 8'h22;

  // THE SECOND SERVICE, added 2026-08-29 when SPLINE went hot. Until now this
  // path had exactly one service and `wrong_op_o` was the wire that said so.
  //
  // Two services is not a bigger version of one. It is the first arrangement
  // in which a service can be STARVED -- one claimant cannot starve anybody --
  // and measuring that is the reason the owner paid for the hot path.
  localparam logic [7:0] OP_CURVE  = 8'h1A;
  localparam logic [7:0] OP_SPLINE = 8'h1B;
  localparam logic [7:0] OP_DCURVE = 8'h1D;

  // THE OTHER FOUR THE TABLE ALREADY OFFERED, wired 2026-08-29.
  //
  // zhao_field_ops_pkg gave NORMALIZE2/3 and ROT2/3 a destination width, which
  // is what makes the executor OFFER them -- and nothing here answered them.
  // They were routed to the NOISE UNIT by the else-branch of the old
  // predicate, which computed noise and wrote it back. `wrong_op_o` was raised,
  // but a raised flag beside a WRONG VALUE IN A REGISTER is a worse outcome
  // than a refusal: the answer is individually plausible and completely wrong.
  //
  // Both units already exist and are closed (NORMALIZE 26/26, ROT 24), and
  // both take PER-POINT sources only, so neither needs the uniform path that
  // UOP_RING_PREP is still waiting on.
  localparam logic [7:0] OP_NORMALIZE2 = 8'h15;
  localparam logic [7:0] OP_NORMALIZE3 = 8'h16;
  localparam logic [7:0] OP_ROT2       = 8'h28;
  localparam logic [7:0] OP_ROT3       = 8'h29;

  // UOP_RING_PREP IS NOT A CANONICAL OPCODE. It is a plan-internal micro-op,
  // deliberately outside the canonical space (>= 0xF0; canonical v1 tops out
  // at 0x29), emitted by the lowerer when a RING's radii are uniform. It never
  // appears in a .zprog -- it appears in the uop stream the executor runs,
  // which is why the hardware has to know it and the file format does not.
  localparam logic [7:0] UOP_RING_PREP = 8'hF1;

  // A DEBUGGING CONVENIENCE, AND NOT MORE THAN THAT. These are the same
  // constants zhao_probe_v3_engine ties its spare bank lanes to, and this
  // comment used to repeat that block's argument: "recognisable, so a routing
  // bug into an unused lane looks wrong rather than convincing".
  //
  // Mutant V25 disproved the argument HERE by surviving. The bank returns
  // products on a SHARED rsp_p bus with a per-claimant valid, the noise unit
  // samples it only under bank_rsp_valid[1], and no other reader exists --
  // rival_rsp_o carries the valid, not the value. So nothing can read the
  // rival's product at all, and its operands cannot change any output.
  //
  // The routing bug the argument worried about IS caught, by V02, and for a
  // different reason: the service's own ANSWER comes out wrong, which it would
  // for any rival operands whatever, including zero. Recognisable values are
  // worth having when reading a waveform by eye. They are not evidence.
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

  logic               nz_rsp_valid, nz_rsp_ready;
  logic [TAGW-1:0]    nz_rsp_tag;
  logic signed [31:0] nz_r0 [4], nz_r1 [4];

  logic               nm_rsp_valid, nm_rsp_ready;
  logic        [ 7:0] nm_rsp_tag;
  logic signed [31:0] nm_r0 [4], nm_r1 [4], nm_r2 [4];
  logic               nm_mul_issue, nm_mul_ready, nm_mul_valid;
  logic signed [32:0] nm_a [4], nm_b [4];

  logic               rt_rsp_valid, rt_rsp_ready;
  logic        [ 7:0] rt_rsp_tag;
  logic signed [31:0] rt_r0 [4], rt_r1 [4], rt_r2 [4];
  logic               rt_mul_issue, rt_mul_ready, rt_mul_valid;
  logic signed [32:0] rt_a [4], rt_b [4];

  /* verilator lint_off UNUSEDSIGNAL */
  logic [3:0] nm_sat_resc_unused, nm_rcp0_unused;
  logic [3:0] rt_sat_add_unused, rt_sat_mul_unused;
  /* verilator lint_on UNUSEDSIGNAL */

  logic               rg_rsp_valid, rg_rsp_ready;
  logic        [ 7:0] rg_rsp_tag;
  logic signed [31:0] rg_r0 [4];
  logic               rg_mul_issue, rg_mul_ready, rg_mul_valid;
  logic signed [32:0] rg_a [4], rg_b [4];
  logic        [ 5:0] sb_raddr;
  logic signed [31:0] sb_rdata;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [3:0] rg_sat_add_unused, rg_sat_mul_unused;
  /* verilator lint_on UNUSEDSIGNAL */

  logic               cv_rsp_valid, cv_rsp_ready;
  logic        [ 7:0] cv_rsp_tag;
  logic signed [31:0] cv_r0 [4];
  logic               cv_mul_issue, cv_mul_ready, cv_mul_valid;
  logic signed [32:0] cv_a [4], cv_b [4];
  /* verilator lint_off UNUSEDSIGNAL */
  logic [ 3:0] cv_sat_add_unused, cv_sat_mul_unused;
  logic [23:0] cv_seg_unused;
  /* verilator lint_on UNUSEDSIGNAL */

  logic [3:0] nz_sat_add, nz_sat_rescale;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [3:0] nz_sat_add_unused, nz_sat_rescale_unused;
  /* verilator lint_on UNUSEDSIGNAL */
  assign nz_sat_add_unused = nz_sat_add;
  assign nz_sat_rescale_unused = nz_sat_rescale;

  // WHICH SERVICE OWNS THIS OP. Decoded once, read by the request routing,
  // the response mux and the wrong-op detector, so the three cannot disagree
  // about it -- which is the seam defect this engine has produced four times.
  logic is_curve_c, is_norm_c, is_rot_c, is_noise_c, is_ring_c;
  assign is_curve_c = (svc_op == OP_CURVE) || (svc_op == OP_DCURVE) ||
                      (svc_op == OP_SPLINE);
  assign is_norm_c  = (svc_op == OP_NORMALIZE2) || (svc_op == OP_NORMALIZE3);
  assign is_rot_c   = (svc_op == OP_ROT2) || (svc_op == OP_ROT3);
  assign is_noise_c = (svc_op == OP_NOISE2) || (svc_op == OP_RIDGE);
  assign is_ring_c  = (svc_op == UOP_RING_PREP);

  // EVERY SERVICE IS SELECTED BY ITS OWN PREDICATE, and the noise unit no
  // longer sits in an else-branch. That branch is exactly how four ops the
  // table offered ended up being answered with noise: "not curve" is not the
  // same claim as "is noise", and only one of them stays true when an op is
  // added.
  logic nz_v_ready, cv_req_ready, nm_v_ready, rt_v_ready, rg_req_ready;
  assign svc_ready = is_curve_c ? cv_req_ready
                   : is_norm_c  ? nm_v_ready
                   : is_rot_c   ? rt_v_ready
                   : is_ring_c  ? rg_req_ready
                   : is_noise_c ? nz_v_ready : 1'b0;

  zhao_field_v3_noise u_noise (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(svc_valid && is_noise_c), .v_ready_o(nz_v_ready),
      .is_ridge_i(svc_op == OP_RIDGE),
      .a0_0_i(svc_s0[0]), .a0_1_i(svc_s0[1]), .a0_2_i(svc_s0[2]), .a0_3_i(svc_s0[3]),
      .a1_0_i(svc_s1[0]), .a1_1_i(svc_s1[1]), .a1_2_i(svc_s1[2]), .a1_3_i(svc_s1[3]),
      .seed_i(svc_imm), .tag_i(svc_tag),
      .r_valid_o(nz_rsp_valid), .r_ready_i(nz_rsp_ready),
      .o0_0_o(nz_r0[0]), .o0_1_o(nz_r0[1]), .o0_2_o(nz_r0[2]), .o0_3_o(nz_r0[3]),
      .o1_0_o(nz_r1[0]), .o1_1_o(nz_r1[1]), .o1_2_o(nz_r1[2]), .o1_3_o(nz_r1[3]),
      .sat_add_o(nz_sat_add), .sat_rescale_o(nz_sat_rescale), .tag_o(nz_rsp_tag),
      .mul_issue_o(nz_mul_issue), .mul_ready_i(nz_mul_ready),
      .mul_a_0_o(nz_a[0]), .mul_a_1_o(nz_a[1]), .mul_a_2_o(nz_a[2]), .mul_a_3_o(nz_a[3]),
      .mul_b_0_o(nz_b[0]), .mul_b_1_o(nz_b[1]), .mul_b_2_o(nz_b[2]), .mul_b_3_o(nz_b[3]),
      .mul_valid_i(nz_mul_valid),
      .mul_p_0_i(bank_p[0]), .mul_p_1_i(bank_p[1]),
      .mul_p_2_i(bank_p[2]), .mul_p_3_i(bank_p[3])
  );

  // ---- the curve service: CURVE, DCURVE and SPLINE -----------------------
  //
  // The mode is derived from the op HERE and nowhere else, so the encoding
  // lives beside the opcodes it comes from.
  logic [1:0] cv_mode_c;
  assign cv_mode_c = (svc_op == OP_DCURVE) ? 2'd1
                   : (svc_op == OP_SPLINE) ? 2'd2 : 2'd0;

  zhao_probe_curve_svc u_curve (
      .clk(clk), .rst_n(rst_n),
      .tl_we_i(tl_we_i), .tl_tbl_i(tl_tbl_i), .tl_idx_i(tl_idx_i),
      .tl_x_i(tl_x_i), .tl_y_i(tl_y_i), .tl_dy_i(tl_dy_i),
      .tl_commit_i(tl_commit_i), .tl_n_i(tl_n_i),
      .req_valid_i(svc_valid && is_curve_c), .req_ready_o(cv_req_ready),
      .req_mode_i(cv_mode_c), .req_tbl_i(svc_imm[1:0]),
      .req_a_0_i(svc_s0[0]), .req_a_1_i(svc_s0[1]),
      .req_a_2_i(svc_s0[2]), .req_a_3_i(svc_s0[3]),
      .req_tag_i(svc_tag),
      .mul_issue_o(cv_mul_issue), .mul_ready_i(cv_mul_ready),
      .mul_a_0_o(cv_a[0]), .mul_a_1_o(cv_a[1]),
      .mul_a_2_o(cv_a[2]), .mul_a_3_o(cv_a[3]),
      .mul_b_0_o(cv_b[0]), .mul_b_1_o(cv_b[1]),
      .mul_b_2_o(cv_b[2]), .mul_b_3_o(cv_b[3]),
      .mul_valid_i(cv_mul_valid),
      .mul_p_0_i(bank_p[0]), .mul_p_1_i(bank_p[1]),
      .mul_p_2_i(bank_p[2]), .mul_p_3_i(bank_p[3]),
      .rsp_valid_o(cv_rsp_valid), .rsp_ready_i(cv_rsp_ready),
      .rsp_r_0_o(cv_r0[0]), .rsp_r_1_o(cv_r0[1]),
      .rsp_r_2_o(cv_r0[2]), .rsp_r_3_o(cv_r0[3]),
      .rsp_sat_add_o(cv_sat_add_unused), .rsp_sat_mul_o(cv_sat_mul_unused),
      .rsp_seg_o(cv_seg_unused), .rsp_tag_o(cv_rsp_tag)
  );

  zhao_field_v3_normalize u_norm (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(svc_valid && is_norm_c), .v_ready_o(nm_v_ready),
      .is_n3_i(svc_op == OP_NORMALIZE3),
      .a0_0_i(svc_s0[0]), .a0_1_i(svc_s0[1]), .a0_2_i(svc_s0[2]), .a0_3_i(svc_s0[3]),
      .a1_0_i(svc_s1[0]), .a1_1_i(svc_s1[1]), .a1_2_i(svc_s1[2]), .a1_3_i(svc_s1[3]),
      .a2_0_i(svc_s2[0]), .a2_1_i(svc_s2[1]), .a2_2_i(svc_s2[2]), .a2_3_i(svc_s2[3]),
      .tag_i(svc_tag),
      .r_valid_o(nm_rsp_valid), .r_ready_i(nm_rsp_ready),
      .o0_0_o(nm_r0[0]), .o0_1_o(nm_r0[1]), .o0_2_o(nm_r0[2]), .o0_3_o(nm_r0[3]),
      .o1_0_o(nm_r1[0]), .o1_1_o(nm_r1[1]), .o1_2_o(nm_r1[2]), .o1_3_o(nm_r1[3]),
      .o2_0_o(nm_r2[0]), .o2_1_o(nm_r2[1]), .o2_2_o(nm_r2[2]), .o2_3_o(nm_r2[3]),
      .sat_rescale_o(nm_sat_resc_unused), .rcp0_o(nm_rcp0_unused), .tag_o(nm_rsp_tag),
      .mul_issue_o(nm_mul_issue), .mul_ready_i(nm_mul_ready),
      .mul_a_0_o(nm_a[0]), .mul_a_1_o(nm_a[1]), .mul_a_2_o(nm_a[2]), .mul_a_3_o(nm_a[3]),
      .mul_b_0_o(nm_b[0]), .mul_b_1_o(nm_b[1]), .mul_b_2_o(nm_b[2]), .mul_b_3_o(nm_b[3]),
      .mul_valid_i(nm_mul_valid),
      .mul_p_0_i(bank_p[0]), .mul_p_1_i(bank_p[1]),
      .mul_p_2_i(bank_p[2]), .mul_p_3_i(bank_p[3])
  );

  // THE ANGLE IS ALWAYS s3, FOR BOTH ROT2 AND ROT3, and getting this wrong
  // once is worth the comment.
  //
  // The four source ports are filled BY OPERAND GROUP, not by flattening:
  // zhao_probe_v3_exec drives long_s0/s1/s2 from operand a's three members and
  // long_s3 from operand b's single member, whatever a's width. ROT2's angle
  // is operand b, so it arrives on s3 even though a uses only s0 and s1 and
  // leaves s2 idle.
  //
  // I first wired it to s2 for ROT2, reasoning from the ORACLE, where src[] is
  // the flattened list and ROT2's angle really is src[2] while ROT3's is
  // src[3]. Both statements are true and they are about different things: the
  // same register, a different index, in two different representations. ROT3
  // passed and ROT2 came back wrong, which is what the invariant section is
  // for -- one op right and its near twin wrong is the shape of an indexing
  // error, not an arithmetic one.

  zhao_field_v3_rot u_rot (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(svc_valid && is_rot_c), .v_ready_o(rt_v_ready),
      .is_rot3_i(svc_op == OP_ROT3), .axis_i(svc_imm[1:0]),
      .ang_0_i(svc_s3[0]), .ang_1_i(svc_s3[1]), .ang_2_i(svc_s3[2]), .ang_3_i(svc_s3[3]),
      .a0_0_i(svc_s0[0]), .a0_1_i(svc_s0[1]), .a0_2_i(svc_s0[2]), .a0_3_i(svc_s0[3]),
      .a1_0_i(svc_s1[0]), .a1_1_i(svc_s1[1]), .a1_2_i(svc_s1[2]), .a1_3_i(svc_s1[3]),
      .a2_0_i(svc_s2[0]), .a2_1_i(svc_s2[1]), .a2_2_i(svc_s2[2]), .a2_3_i(svc_s2[3]),
      .tag_i(svc_tag),
      .r_valid_o(rt_rsp_valid), .r_ready_i(rt_rsp_ready),
      .o0_0_o(rt_r0[0]), .o0_1_o(rt_r0[1]), .o0_2_o(rt_r0[2]), .o0_3_o(rt_r0[3]),
      .o1_0_o(rt_r1[0]), .o1_1_o(rt_r1[1]), .o1_2_o(rt_r1[2]), .o1_3_o(rt_r1[3]),
      .o2_0_o(rt_r2[0]), .o2_1_o(rt_r2[1]), .o2_2_o(rt_r2[2]), .o2_3_o(rt_r2[3]),
      .sat_add_o(rt_sat_add_unused), .sat_mul_o(rt_sat_mul_unused), .tag_o(rt_rsp_tag),
      .mul_issue_o(rt_mul_issue), .mul_ready_i(rt_mul_ready),
      .mul_a_0_o(rt_a[0]), .mul_a_1_o(rt_a[1]), .mul_a_2_o(rt_a[2]), .mul_a_3_o(rt_a[3]),
      .mul_b_0_o(rt_b[0]), .mul_b_1_o(rt_b[1]), .mul_b_2_o(rt_b[2]), .mul_b_3_o(rt_b[3]),
      .mul_valid_i(rt_mul_valid),
      .mul_p_0_i(bank_p[0]), .mul_p_1_i(bank_p[1]),
      .mul_p_2_i(bank_p[2]), .mul_p_3_i(bank_p[3])
  );

  zhao_field_v3_sbank u_sbank (
      .clk(clk), .rst_n(rst_n),
      .we_i(sb_we_i), .waddr_i(sb_waddr_i), .wdata_i(sb_wdata_i), .we_bad_o(sb_bad_o),
      .raddr_i(sb_raddr), .rdata_o(sb_rdata)
  );

  // ONE READER, SO NO ARBITRATION. The prepared ring is the only consumer of
  // uniforms today, so its read port connects straight through. The day a
  // second service needs one, this is where the contention appears -- and it
  // will be visible as a port that two blocks drive, not as a silent wrong
  // answer.
  zhao_field_v3_ring_svc u_ring (
      .clk(clk), .rst_n(rst_n),
      .req_valid_i(svc_valid && is_ring_c), .req_ready_o(rg_req_ready),
      .req_d_0_i(svc_s0[0]), .req_d_1_i(svc_s0[1]),
      .req_d_2_i(svc_s0[2]), .req_d_3_i(svc_s0[3]),
      .req_imm_i(svc_imm), .req_tag_i(svc_tag),
      .sb_raddr_o(sb_raddr), .sb_rdata_i(sb_rdata),
      .mul_issue_o(rg_mul_issue), .mul_ready_i(rg_mul_ready),
      .mul_a_0_o(rg_a[0]), .mul_a_1_o(rg_a[1]), .mul_a_2_o(rg_a[2]), .mul_a_3_o(rg_a[3]),
      .mul_b_0_o(rg_b[0]), .mul_b_1_o(rg_b[1]), .mul_b_2_o(rg_b[2]), .mul_b_3_o(rg_b[3]),
      .mul_valid_i(rg_mul_valid),
      .mul_p_0_i(bank_p[0]), .mul_p_1_i(bank_p[1]),
      .mul_p_2_i(bank_p[2]), .mul_p_3_i(bank_p[3]),
      .rsp_valid_o(rg_rsp_valid), .rsp_ready_i(rg_rsp_ready),
      .rsp_r_0_o(rg_r0[0]), .rsp_r_1_o(rg_r0[1]),
      .rsp_r_2_o(rg_r0[2]), .rsp_r_3_o(rg_r0[3]),
      .rsp_sat_add_o(rg_sat_add_unused), .rsp_sat_mul_o(rg_sat_mul_unused),
      .rsp_tag_o(rg_rsp_tag),
      .imm_bad_o(imm_bad_o)
  );

  // ---- one response port, FIVE services ----------------------------------
  //
  // BOTH CAN BE HOLDING AN ANSWER AT ONCE and the dispatcher takes one per
  // cycle, so this is an arbitration and not a mux. The curve service wins
  // when both are ready: its answer is parked in the finish registers of a
  // pipelined barrel, so making it wait stalls a group BEHIND it, whereas the
  // noise unit's answer is its own last stage and holding costs one group.
  //
  // NEITHER CAN BE DROPPED. The loser keeps r_ready low and keeps its answer;
  // it is not overwritten, because a lost response is a wrong VALUE reaching a
  // register rather than a slower machine.
  assign cv_rsp_ready = rsp_ready;
  assign nm_rsp_ready = rsp_ready && !cv_rsp_valid;
  assign rt_rsp_ready = rsp_ready && !cv_rsp_valid && !nm_rsp_valid;
  assign rg_rsp_ready = rsp_ready && !cv_rsp_valid && !nm_rsp_valid && !rt_rsp_valid;
  assign nz_rsp_ready = rsp_ready && !cv_rsp_valid && !nm_rsp_valid && !rt_rsp_valid &&
                        !rg_rsp_valid;

  assign rsp_valid = cv_rsp_valid || nm_rsp_valid || rt_rsp_valid || rg_rsp_valid ||
                     nz_rsp_valid;
  assign rsp_tag   = cv_rsp_valid ? cv_rsp_tag
                   : nm_rsp_valid ? nm_rsp_tag
                   : rt_rsp_valid ? rt_rsp_tag
                   : rg_rsp_valid ? rg_rsp_tag : nz_rsp_tag;

  always_comb begin
    for (int l = 0; l < 4; l++) begin
      rsp_r0[l] = cv_rsp_valid ? cv_r0[l]
                : nm_rsp_valid ? nm_r0[l]
                : rt_rsp_valid ? rt_r0[l]
                : rg_rsp_valid ? rg_r0[l] : nz_r0[l];
      // CURVE, DCURVE and SPLINE write ONE register per point, so their
      // second and third are zero by the op's own law. NOISE2 and RIDGE write
      // two. NORMALIZE3 and ROT3 write THREE, which is the first time this
      // path has ever driven rsp_r2 with anything but zero.
      // The prepared ring writes ONE register per point, like the curve ops.
      rsp_r1[l] = cv_rsp_valid ? 32'sd0
                : nm_rsp_valid ? nm_r1[l]
                : rt_rsp_valid ? rt_r1[l]
                : rg_rsp_valid ? 32'sd0 : nz_r1[l];
      rsp_r2[l] = nm_rsp_valid ? nm_r2[l]
                : rt_rsp_valid ? rt_r2[l] : 32'sd0;
    end
  end

  // THE SERVICES ARE ASKED ONLY FOR OPS THEY IMPLEMENT, and this says so out
  // loud. It latches, because a single wrong request is the whole finding and
  // a level would be missed by any test that samples.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wrong_op_o <= 1'b0;
    end else if (svc_valid && svc_ready &&
                 !is_noise_c && !is_curve_c && !is_norm_c && !is_rot_c &&
                 !is_ring_c) begin
      wrong_op_o <= 1'b1;
    end
  end

  // ---- the shared bank: 0 is the rival, 1 the noise unit, 2 the curve -----
  // The bank's own rule: claimant 0 is the ALU lanes, higher indices are
  // services, and with PRIO_SERVICES_FIRST the highest wins. The rival sits
  // in the lanes' slot so it loses to the services exactly as the lanes would.
  //
  // SO THE CURVE SERVICE OUTRANKS THE NOISE UNIT, and that ordering is a
  // CHOICE rather than a consequence of where it was added. It is also the
  // arrangement the starvation question is about: with a fixed priority and a
  // claimant that can ask every cycle, the loser's worst case is not obviously
  // bounded. This wires it; the measurement is a separate step and its number
  // is not predicted here.
  logic [5:0]         bank_req_valid, bank_req_ready, bank_rsp_valid;
  logic signed [32:0] bank_a [6][4], bank_b [6][4];
  logic [TAGW-1:0]    bank_tag [6];
  /* verilator lint_off UNUSEDSIGNAL */
  logic [TAGW-1:0]    bank_rsp_tag;
  /* verilator lint_on UNUSEDSIGNAL */

  always_comb begin
    bank_req_valid[0] = rival_req_i;
    bank_req_valid[1] = nz_mul_issue;
    bank_req_valid[2] = cv_mul_issue;
    bank_req_valid[3] = nm_mul_issue;
    bank_req_valid[4] = rt_mul_issue;
    bank_req_valid[5] = rg_mul_issue;
    for (int l = 0; l < 4; l++) begin
      bank_a[0][l] = RIVAL_A;
      bank_b[0][l] = RIVAL_B;
      bank_a[1][l] = nz_a[l];
      bank_b[1][l] = nz_b[l];
      bank_a[2][l] = cv_a[l];
      bank_b[2][l] = cv_b[l];
      bank_a[3][l] = nm_a[l];
      bank_b[3][l] = nm_b[l];
      bank_a[4][l] = rt_a[l];
      bank_b[4][l] = rt_b[l];
      bank_a[5][l] = rg_a[l];
      bank_b[5][l] = rg_b[l];
    end
    bank_tag[0] = 8'd0;
    bank_tag[1] = 8'd1;
    bank_tag[2] = 8'd2;
    bank_tag[3] = 8'd3;
    bank_tag[4] = 8'd4;
    bank_tag[5] = 8'd5;
  end

  assign rival_grant_o = bank_req_ready[0];
  assign rival_rsp_o   = bank_rsp_valid[0];
  assign nz_mul_ready  = bank_req_ready[1];
  assign nz_mul_valid  = bank_rsp_valid[1];
  assign cv_mul_ready  = bank_req_ready[2];
  assign cv_mul_valid  = bank_rsp_valid[2];
  assign nm_mul_ready  = bank_req_ready[3];
  assign nm_mul_valid  = bank_rsp_valid[3];
  assign rt_mul_ready  = bank_req_ready[4];
  assign rt_mul_valid  = bank_rsp_valid[4];
  assign rg_mul_ready  = bank_req_ready[5];
  assign rg_mul_valid  = bank_rsp_valid[5];

  zhao_field_v3_mulbank #(
      .CLAIMANTS(6), .PRIO_SERVICES_FIRST(1'b1), .TAGW(TAGW)
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
