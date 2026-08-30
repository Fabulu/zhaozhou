// zhao_probe_v3_full.sv — the Field v3 engine and its long-op service path,
// wired together as one machine.
//
// ENFORCED-BY: tests/differential/field_v3_full_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS IS A SEPARATE FILE AND NOT A CHANGE TO THE ENGINE
// ---------------------------------------------------------------------------
// `zhao_probe_v3_engine` is closed at 42/42 and `zhao_field_v3_svcpath` at
// 25/25. Both tallies describe the blocks AS THEY ARE. Editing either to make
// them fit would invalidate the tally of the block edited, and this project has
// already spent a day discovering that a tally quoted after its RTL moved is
// worse than no tally at all.
//
// So the composition lives here, it adds no logic of its own beyond the
// connections, and both blocks keep their scores.
//
// ---------------------------------------------------------------------------
// THE THREE SEAMS, AND WHY EACH ONE IS THE INTERESTING PART
// ---------------------------------------------------------------------------
// 1. THE LONG OP GOES OUT. The engine parks a context and offers the
//    instruction; the dispatcher gathers four of them into one request. The
//    engine's `long_ready_i` is the dispatcher's `long_ready_o`, so a group
//    that cannot be gathered back-pressures the executor rather than being
//    dropped.
//
// 2. THE ANSWER COMES BACK AS A WRITE, AND IT COMPETES. The drain and the ALU
//    both want the single register-file write port. `zhao_field_v3_wbarb`
//    inside the service path decides, and its measurement is already in:
//    ALU-first STARVES the drain outright, drain-first costs the ALU exactly
//    eight clocks per four-point group. The engine is claimant 0.
//
// 3. THE REGISTER FILE IS INSIDE THE EXECUTOR. That is why the engine takes
//    `wr_*_i`: whoever wins the port has to be able to reach the file. Without
//    it the drain would have nowhere to put its results, and with a naive
//    loopback the ALU would write even on the clocks it lost.
//
// ---------------------------------------------------------------------------
// THE DEFECT THIS WIRING WOULD HAVE HAD, AND WHY IT DOES NOT
// ---------------------------------------------------------------------------
// `wb_valid_o` used to be an ANNOUNCEMENT -- a bare assign mirroring a write
// the executor had already committed. Wired to an arbiter that refuses by
// design, every refused clock would have been a LOST REGISTER WRITE: eight per
// four-point group, measured, not estimated.
//
// The executor now retires into a four-deep skid instead, because the
// multiplier is fixed-latency and CANNOT be stalled -- freezing an instruction
// while its product arrives on schedule desynchronises the two, which was
// measured wrong twice before the queue existed. That is the piece that makes
// this composition safe, and it is why this file could not be written earlier.
// ---------------------------------------------------------------------------
// A DISAGREEMENT THE COMPOSITION EXPOSES, AND IT DEADLOCKS
// ---------------------------------------------------------------------------
// The executor's `is_long()` routes TEN opcodes to the service path:
//
//     NORMALIZE2 15  NORMALIZE3 16  CURVE 1A  SPLINE 1B  NOISE2 1C
//     DCURVE 1D      RING 21        RIDGE 22  ROT2 28    ROT3 29
//
// The dispatcher's `dst_width_of()` knows EIGHT of them. It does not know
// SPLINE (1B) or RING (21), and its default of width 0 means REFUSE -- which
// is correct there and documented as deliberate, because a wrong width writes
// the wrong number of registers.
//
// Put the two together and a program containing SPLINE or RING PARKS THAT
// CONTEXT FOREVER: the executor hands the instruction over and waits for a
// release that cannot come, because the dispatcher will never accept it.
// Nothing times out. The context simply stops.
//
// NEITHER BLOCK IS WRONG ON ITS OWN, which is why nine sweeps and two
// closed compositions never saw it. The executor is right that SPLINE is a
// long op; the dispatcher is right to refuse a width it does not know. The
// defect is only in the pair, and only when a program uses one of those two.
//
// IT IS NOT FIXED HERE, because the fix depends on a decision that is not an
// agent's to make -- see STATUS.md:
//
//   * if SPLINE stays COLD (the brief's position), `is_long()` should not
//     route it at all: the scalar path in zhao_field_curve.sv implements the
//     whole op, lookup included;
//   * if it goes HOT, the dispatcher needs the case AND a four-point table
//     lookup that does not exist yet.
//
// RING is the same shape with a different answer: the brief costs the PREPARED
// ring (UOP_RING_PREP, 0xF1) as its hot path, and OP_RING (0x21) is the
// varying-radius form that stays cold. So 0x21 probably should not be in
// `is_long()` either, and 0xF1 probably should be in `dst_width_of()`.
//
// Until then the composed test drives only the eight both blocks agree on, and
// says so rather than quietly avoiding the other two.
module zhao_probe_v3_full #(
    parameter int CTX  = 8,
    parameter int OUTSTANDING = 4,
    // Points per context. Everything below simply carries it.
    parameter int LANES = 1,
    parameter int LONGQ = 4,
    parameter int GATHERS = 4,
    parameter int DIST_BANKS = 2,
    parameter int RING_UNITS = 2,
    parameter int REGS = 32,
    parameter int PLAN = 32,
    parameter int TAGW = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- program and data load, straight through to the engine -------------
    input var logic                    up_we_i,
    input var logic [$clog2(CTX)-1:0]  up_ctx_i,
    input var logic [$clog2(PLAN)-1:0] up_pc_i,
    input var logic [7:0]              up_op_i,
    input var logic [$clog2(REGS)-1:0] up_dst_i,
    input var logic [$clog2(REGS)-1:0] up_a_i,
    input var logic [$clog2(REGS)-1:0] up_b_i,
    input var logic [$clog2(REGS)-1:0] up_c_i,
    input var logic [31:0]             up_imm_i,

    input var logic                    pre_we_i,
    input var logic [$clog2(CTX)-1:0]  pre_ctx_i,
    input var logic [$clog2(REGS)-1:0] pre_reg_i,
    input var logic signed [32*LANES-1:0] pre_data_i,

    input var logic                   start_i,
    input var logic [$clog2(CTX)-1:0] start_ctx_i,

    // A rival on the engine's own multiplier bank, so the executor can be
    // REFUSED. Without it every denial path in the executor is dead code --
    // the engine's first sweep scored 4 of 11 for exactly that reason.
    input var logic                   rival_req_i,

    // The writeback policy, still an INPUT because the answer is a
    // measurement. See zhao_field_v3_wbarb.
    input var logic [1:0]             wb_policy_i,

    // ---- what the composition is for ---------------------------------------
    output var logic                    done_valid_o,
    output var logic [$clog2(CTX)-1:0]  done_ctx_o,
    output var logic [CTX-1:0]          active_o,
    output var logic                    unsupported_o,
    // The op ledger. Brought out because a composed engine that saturates
    // silently is exactly the failure the per-op ledgers exist to catch.
    output var logic                    sat_add_o,
    output var logic                    sat_mul_o,
    output var logic                    sat_rescale_o,

    // The single write port, after arbitration. This is the register file's
    // write as it actually happens, and it is exposed so a test can watch the
    // stream rather than reconstruct it.
    output var logic                    wr_en_o,
    output var logic [$clog2(CTX)-1:0]  wr_ctx_o,
    output var logic [$clog2(REGS)-1:0] wr_reg_o,
    output var logic signed [32*LANES-1:0] wr_data_o,

    // ---- evidence, per stage, so a number names its own stage --------------
    output var logic [31:0]             uops_issued_o,
    output var logic [31:0]             idle_clocks_o,
    output var logic [31:0]             hold_clocks_o,
    output var logic [31:0]             blocked_clocks_o,
    output var logic [31:0]             denied_clocks_o,
    output var logic [31:0]             dot_clocks_o,
    output var logic [31:0]             skid_clocks_o,
    output var logic [31:0]             rf_writes_o,
    output var logic [31:0]             groups_o,
    output var logic [31:0]             partial_o,
    output var logic [31:0]             drain_writes_o,
    output var logic [31:0]             wb_served_o  [2],
    output var logic [31:0]             wb_stalled_o [2],

    // ---- every alarm either block owns, brought out unmerged ---------------
    //
    // NOT OR-ED TOGETHER. Five different faults reduced to one bit is a bit
    // that says "something, somewhere", and this project has already learned
    // that a guard which cannot name its own failure gets read as noise.
    output var logic                    exec_desync_o,
    output var logic                    bank_desync_o,
    output var logic                    svc_bank_desync_o,
    output var logic                    tag_mismatch_o,
    output var logic                    wrong_op_o,

    // ---- the curve service's table cache, straight through -----------------
    // The knot tables belong to the PROGRAM, not to the silicon. In the
    // finished machine the command stream fills this; here the differential
    // does. Passing it through rather than parking a table inside the service
    // is the difference between a block that is wired and one that only looks
    // self-contained.
    input  var logic                    tl_we_i,
    input  var logic [1:0]              tl_tbl_i,
    input  var logic [5:0]              tl_idx_i,
    input  var logic signed [31:0]      tl_x_i,
    input  var logic signed [31:0]      tl_y_i,
    input  var logic signed [31:0]      tl_dy_i,
    input  var logic                    tl_commit_i,
    input  var logic [6:0]              tl_n_i,

    // ---- the uniform (scalar) bank, straight through -----------------------
    // The ARM runs the plan's PREP block once per association and writes the
    // answers here; the prepared ring reads them by index.
    input  var logic                    sb_we_i,
    input  var logic [15:0]             sb_waddr_i,
    input  var logic signed [31:0]      sb_wdata_i,
    output var logic                    sb_bad_o,
    output var logic                    imm_bad_o,
    output var logic                    sk_overflow_o,

    // ---- DEBUG: the long-op handover, exactly as the dispatcher sees it ----
    //
    // Not decoration. The composed gate found a CURVE returning the curve of
    // ZERO for a point whose operand was plainly not zero, and no block-level
    // test can see that: the service is handed a number and answers it
    // faithfully. The only way to tell a bad ANSWER from a bad QUESTION is to
    // watch the question being asked.
    output var logic                    dbg_long_valid_o,
    output var logic                    dbg_long_ready_o,
    output var logic [$clog2(CTX)-1:0]  dbg_long_ctx_o,
    output var logic [7:0]              dbg_long_op_o,
    output var logic signed [31:0]      dbg_long_s0_o,
    output var logic                    pre_ready_o,
    output var logic                    dbg_s2_v_o,
    output var logic [$clog2(CTX)-1:0]  dbg_s2_ctx_o,
    output var logic [7:0]              dbg_s2_op_o,
    output var logic signed [31:0]      dbg_use_a0_o,
    output var logic signed [31:0]      dbg_rf_a0_o
);

  // ---- engine -> service path --------------------------------------------
  assign dbg_long_valid_o = long_valid;
  assign dbg_long_ready_o = long_ready;
  assign dbg_long_ctx_o   = long_ctx;
  assign dbg_long_op_o    = long_op;
  // Lane 0. This tap exists for a bench that reads one point at a time, and it
  // is what found the preload defect; a four-lane version would say nothing the
  // per-lane value checks do not already say.
  assign dbg_long_s0_o    = long_s0[31:0];

  logic                    long_valid, long_ready;
  logic [$clog2(CTX)-1:0]  long_ctx;
  logic [7:0]              long_op;
  logic [$clog2(REGS)-1:0] long_dst;
  logic signed [32*LANES-1:0] long_s0, long_s1, long_s2, long_s3, long_s4;
  logic [31:0]             long_imm;
  logic                    long_flush;

  // ---- service path -> engine --------------------------------------------
  logic                    rel_valid;
  logic [$clog2(CTX)-1:0]  rel_ctx;

  // ---- the contested write port ------------------------------------------
  logic                    alu_wb_valid, alu_wb_ready;
  logic [$clog2(CTX)-1:0]  alu_wb_ctx;
  logic [$clog2(REGS)-1:0] alu_wb_reg;
  logic signed [32*LANES-1:0]      alu_wb_data;

  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] svc_bank_grants, svc_bank_stalls;
  logic        svc_rival_grant, svc_rival_rsp;
  logic [31:0] engine_lane_stalls, engine_bank_grants;
  logic        engine_rival_grant, engine_rival_rsp;
  logic [7:0]  engine_bank_tag;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_probe_v3_engine #(
      .CTX(CTX), .REGS(REGS), .PLAN(PLAN), .LANES(LANES), .LONGQ(LONGQ)
  ) u_engine (
      .clk(clk), .rst_n(rst_n),
      .up_we_i(up_we_i), .up_ctx_i(up_ctx_i), .up_pc_i(up_pc_i), .up_op_i(up_op_i),
      .up_dst_i(up_dst_i), .up_a_i(up_a_i), .up_b_i(up_b_i), .up_c_i(up_c_i),
      .up_imm_i(up_imm_i),
      .pre_we_i(pre_we_i), .pre_ctx_i(pre_ctx_i), .pre_reg_i(pre_reg_i),
      .pre_data_i(pre_data_i),
      .start_i(start_i), .start_ctx_i(start_ctx_i),
      .rival_req_i(rival_req_i),
      .pre_ready_o(pre_ready_o),
      .dbg_s2_v_o(dbg_s2_v_o), .dbg_s2_ctx_o(dbg_s2_ctx_o), .dbg_s2_op_o(dbg_s2_op_o),
      .dbg_use_a0_o(dbg_use_a0_o), .dbg_rf_a0_o(dbg_rf_a0_o),

      // SEAM 2: the ALU asks the arbiter, and takes back whatever it grants.
      .wb_valid_o(alu_wb_valid), .wb_ready_i(alu_wb_ready),
      .wb_ctx_o(alu_wb_ctx), .wb_reg_o(alu_wb_reg), .wb_data_o(alu_wb_data),

      // SEAM 3: the granted write, from whoever won it, into the file.
      .wr_en_i(wr_en_o), .wr_ctx_i(wr_ctx_o),
      .wr_reg_i(wr_reg_o), .wr_data_i(wr_data_o),

      .rf_writes_o(rf_writes_o), .sk_overflow_o(sk_overflow_o),
      .done_valid_o(done_valid_o), .done_ctx_o(done_ctx_o),
      .active_o(active_o), .unsupported_o(unsupported_o),
      .exec_desync_o(exec_desync_o), .bank_desync_o(bank_desync_o),
      .uops_issued_o(uops_issued_o), .idle_clocks_o(idle_clocks_o),
      .hold_clocks_o(hold_clocks_o), .blocked_clocks_o(blocked_clocks_o),
      .denied_clocks_o(denied_clocks_o), .dot_clocks_o(dot_clocks_o),
      .skid_clocks_o(skid_clocks_o),
      .rival_grant_o(engine_rival_grant), .rival_rsp_o(engine_rival_rsp),
      .lane_stalls_o(engine_lane_stalls),
      .sat_add_o(sat_add_o), .sat_mul_o(sat_mul_o), .sat_rescale_o(sat_rescale_o),
      .bank_tag_o(engine_bank_tag), .bank_grants_o(engine_bank_grants),

      // SEAM 1: the long op leaves, and the release brings the context back.
      .long_valid_o(long_valid), .long_ready_i(long_ready),
      .long_ctx_o(long_ctx), .long_op_o(long_op), .long_dst_o(long_dst),
      .long_s0_o(long_s0), .long_s1_o(long_s1), .long_s2_o(long_s2),
      .long_s3_o(long_s3), .long_s4_o(long_s4),
      .long_imm_o(long_imm), .long_flush_o(long_flush),
      .rel_valid_i(rel_valid), .rel_ctx_i(rel_ctx)
  );

  zhao_field_v3_svcpath #(
      .CONTEXTS(CTX), .REGS(REGS), .TAGW(TAGW), .OUTSTANDING(OUTSTANDING), .LANES(LANES),
      .GATHERS(GATHERS), .DIST_BANKS(DIST_BANKS), .RING_UNITS(RING_UNITS)
  ) u_svc (
      .clk(clk), .rst_n(rst_n),
      .long_valid_i(long_valid), .long_ready_o(long_ready),
      .long_ctx_i(long_ctx), .long_op_i(long_op), .long_dst_i(long_dst),
      .long_s0_i(long_s0), .long_s1_i(long_s1), .long_s2_i(long_s2),
      .long_s3_i(long_s3), .long_s4_i(long_s4),
      .long_imm_i(long_imm), .flush_i(long_flush),

      .alu_wb_valid_i(alu_wb_valid), .alu_wb_ready_o(alu_wb_ready),
      .alu_wb_ctx_i(alu_wb_ctx), .alu_wb_reg_i(alu_wb_reg),
      .alu_wb_data_i(alu_wb_data),

      // The service path's own bank rival is tied off: the ENGINE's bank is
      // the one under contention here, driven by rival_req_i above. Two
      // independent rivals would make it impossible to say which bank a
      // refusal came from.
      .rival_req_i(1'b0),
      .rival_grant_o(svc_rival_grant), .rival_rsp_o(svc_rival_rsp),

      .wb_policy_i(wb_policy_i),
      .wr_en_o(wr_en_o), .wr_ctx_o(wr_ctx_o), .wr_reg_o(wr_reg_o),
      .wr_data_o(wr_data_o),
      .rel_valid_o(rel_valid), .rel_ctx_o(rel_ctx),

      .groups_o(groups_o), .partial_o(partial_o), .drain_writes_o(drain_writes_o),
      .bank_grants_o(svc_bank_grants), .bank_stall_lanes_o(svc_bank_stalls),
      .wb_served_o(wb_served_o), .wb_stalled_o(wb_stalled_o),
      .bank_desync_o(svc_bank_desync_o), .tag_mismatch_o(tag_mismatch_o),
      .wrong_op_o(wrong_op_o),
      .tl_we_i(tl_we_i), .tl_tbl_i(tl_tbl_i), .tl_idx_i(tl_idx_i),
      .tl_x_i(tl_x_i), .tl_y_i(tl_y_i), .tl_dy_i(tl_dy_i),
      .tl_commit_i(tl_commit_i), .tl_n_i(tl_n_i),
      .sb_we_i(sb_we_i), .sb_waddr_i(sb_waddr_i), .sb_wdata_i(sb_wdata_i),
      .sb_bad_o(sb_bad_o), .imm_bad_o(imm_bad_o)
  );

endmodule : zhao_probe_v3_full
