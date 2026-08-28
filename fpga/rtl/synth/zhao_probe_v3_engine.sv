// zhao_probe_v3_engine.sv — the first composition: executor + shared
// multiplier bank. Field v3 Phase 4.
//
// WHAT THIS IS, AND WHAT IT IS NOT YET
// -------------------------------------
// The v3 engine's arithmetic is SHARED. `zhao_field_exec_shared` established
// that for v2 after the first Field synthesis measured 79 DSPs against a
// 112-DSP device, with nine of ten op units idle at any instant, each holding
// its own multiplier. v3 keeps that rule and widens only what the sustained
// rate demands.
//
// This composes the two pieces that exist and are measured:
//
//   * `zhao_probe_v3_exec`      — the datapath, swept 31/31, now with its
//                                 private multiplier REMOVED and a claimant
//                                 port in its place;
//   * `zhao_field_v3_mulbank`   — the four-wide bank and its arbiter, which
//                                 routes every product in the engine.
//
// NOT here yet, and named so the gap is not mistaken for scope:
//
//   * the other three lanes. This wires ONE executor to lane 0 of a four-wide
//     bank. The remaining three lanes are tied off, and the bank's own test
//     already proves all four route independently.
//   * the curve and distance services, which are the bank's other two
//     claimants. Their probes exist and are measured; attaching them is the
//     next increment.
//   * the shared sine, isqrt and reciprocal units, which ROT, NORMALIZE and
//     RING borrow. See reports/FIELD_V3_SERVICE_ATTACH.md.
//
// WHY COMPOSE BEFORE THE SERVICES EXIST
// --------------------------------------
// Because the executor's claim on the bank is the part most likely to be
// wrong, and it is wrong in a way its own test cannot see. Alone, the
// executor's multiplier always answered; behind an arbiter it can be REFUSED,
// and an instruction whose product was never started has operands that are
// gone by the next clock. That path does not exist until these two are wired
// together, so it is tested here and nowhere else.
//
// Law:
//   reports/FIELD_V3_SERVICE_ATTACH.md   the five shared resources
//   reports/Fieldv3.md                   Phase 4 composed Earth machine
//   fpga/rtl/field/zhao_field_exec_shared.sv   the measurement that set the rule

`default_nettype none

module zhao_probe_v3_engine #(
    parameter int CTX  = 8,
    parameter int REGS = 32,
    parameter int PLAN = 32
) (
    input var logic clk,
    input var logic rst_n,

    // ---- uop store write ---------------------------------------------------
    input var logic                    up_we_i,
    input var logic [$clog2(CTX)-1:0]  up_ctx_i,
    input var logic [$clog2(PLAN)-1:0] up_pc_i,
    input var logic [7:0]              up_op_i,
    input var logic [$clog2(REGS)-1:0] up_dst_i,
    input var logic [$clog2(REGS)-1:0] up_a_i,
    input var logic [$clog2(REGS)-1:0] up_b_i,
    input var logic [$clog2(REGS)-1:0] up_c_i,
    input var logic [31:0]             up_imm_i,

    // ---- register preload --------------------------------------------------
    input var logic                    pre_we_i,
    input var logic [$clog2(CTX)-1:0]  pre_ctx_i,
    input var logic [$clog2(REGS)-1:0] pre_reg_i,
    input var logic signed [31:0]      pre_data_i,

    // ---- run control -------------------------------------------------------
    input var logic                   start_i,
    input var logic [$clog2(CTX)-1:0] start_ctx_i,

    // ---- a rival claimant, so the arbiter can actually be exercised --------
    // The composition's whole point is that the executor can be REFUSED. With
    // no other claimant the bank always grants and the refusal path is dead
    // code. This port lets a test press the bank on the executor's behalf,
    // standing in for the curve and distance services until they are attached.
    input var logic rival_req_i,

    // ---- observation -------------------------------------------------------
    output var logic                    wb_valid_o,
    output var logic [$clog2(CTX)-1:0]  wb_ctx_o,
    output var logic [$clog2(REGS)-1:0] wb_reg_o,
    output var logic signed [31:0]      wb_data_o,
    output var logic                    done_valid_o,
    output var logic [$clog2(CTX)-1:0]  done_ctx_o,
    output var logic [CTX-1:0]          active_o,
    output var logic                    unsupported_o,
    output var logic                    exec_desync_o,
    output var logic                    bank_desync_o,
    output var logic [31:0]             uops_issued_o,
    output var logic [31:0]             idle_clocks_o,
    // The rival's grant and reply, so a test can prove it was actually SERVED
    // rather than merely asking. Tying these off would make the contention
    // unobservable, which would leave the executor's refusal path tested only
    // by its own absence.
    output var logic                    rival_grant_o,
    output var logic                    rival_rsp_o,

    // The saturation ledger the ALU raises, carried out rather than dropped:
    // a block that computes the right value while losing the record of a clamp
    // is wrong where it matters later.
    output var logic                    sat_add_o,
    output var logic                    sat_mul_o,
    output var logic                    sat_rescale_o,

    output var logic [7:0]              bank_tag_o,
    output var logic [31:0]             bank_grants_o,
    output var logic [31:0]             lane_stalls_o
);

  localparam int CLAIMANTS = 2;  // 0 = this executor lane, 1 = the rival

  logic               ex_req_valid, ex_req_ready;
  logic signed [32:0] ex_req_a, ex_req_b;
  logic               ex_rsp_valid;
  logic signed [65:0] ex_rsp_p;

  logic [CLAIMANTS-1:0] bank_req_valid, bank_req_ready, bank_rsp_valid;
  logic signed [32:0]   bank_a[CLAIMANTS][4];
  logic signed [32:0]   bank_b[CLAIMANTS][4];
  logic [7:0]           bank_tag[CLAIMANTS];
  logic signed [65:0]   bank_p[4];

  // The executor drives lane 0 of the four-wide request; the other three lanes
  // carry the other three points of a vector group and are tied off here.
  // They are NOT zero because zero is a plausible product -- they are tied to
  // the executor's own operands so a routing bug that reached them would
  // produce a recognisable value rather than a convincing one.
  always_comb begin
    for (int c = 0; c < CLAIMANTS; c++) begin
      for (int l = 0; l < 4; l++) begin
        bank_a[c][l] = (c == 0) ? ex_req_a : 33'sd3;
        bank_b[c][l] = (c == 0) ? ex_req_b : 33'sd5;
      end
      bank_tag[c] = 8'(c);
    end
    bank_req_valid[0] = ex_req_valid;
    bank_req_valid[1] = rival_req_i;
  end

  assign rival_grant_o = bank_req_ready[1];
  assign rival_rsp_o   = bank_rsp_valid[1];

  assign ex_req_ready = bank_req_ready[0];
  assign ex_rsp_valid = bank_rsp_valid[0];
  assign ex_rsp_p     = bank_p[0];

  zhao_field_v3_mulbank #(
      .CLAIMANTS(CLAIMANTS),
      .PRIO_SERVICES_FIRST(1'b1),
      .TAGW(8)
  ) u_bank (
      .clk         (clk),
      .rst_n       (rst_n),
      .req_valid_i (bank_req_valid),
      .req_ready_o (bank_req_ready),
      .req_a_i     (bank_a),
      .req_b_i     (bank_b),
      .req_tag_i   (bank_tag),
      .rsp_valid_o (bank_rsp_valid),
      .rsp_p_o     (bank_p),
      .rsp_tag_o   (bank_tag_o),
      .grants_o    (bank_grants_o),
      .stall_lanes_o(lane_stalls_o),
      .desync_o    (bank_desync_o)
  );

  zhao_probe_v3_exec #(
      .CTX (CTX),
      .REGS(REGS),
      .PLAN(PLAN)
  ) u_exec (
      .clk   (clk),
      .rst_n (rst_n),
      .up_we_i(up_we_i), .up_ctx_i(up_ctx_i), .up_pc_i(up_pc_i), .up_op_i(up_op_i),
      .up_dst_i(up_dst_i), .up_a_i(up_a_i), .up_b_i(up_b_i), .up_c_i(up_c_i),
      .up_imm_i(up_imm_i),
      .pre_we_i(pre_we_i), .pre_ctx_i(pre_ctx_i), .pre_reg_i(pre_reg_i),
      .pre_data_i(pre_data_i),
      .start_i(start_i), .start_ctx_i(start_ctx_i),
      .mul_req_valid_o(ex_req_valid),
      .mul_req_ready_i(ex_req_ready),
      .mul_req_a_o    (ex_req_a),
      .mul_req_b_o    (ex_req_b),
      .mul_rsp_valid_i(ex_rsp_valid),
      .mul_rsp_p_i    (ex_rsp_p),
      .wb_valid_o(wb_valid_o), .wb_ctx_o(wb_ctx_o), .wb_reg_o(wb_reg_o),
      .wb_data_o(wb_data_o),
      .done_valid_o(done_valid_o), .done_ctx_o(done_ctx_o),
      .active_o(active_o), .unsupported_o(unsupported_o),
      .sat_add_o(sat_add_o), .sat_mul_o(sat_mul_o), .sat_rescale_o(sat_rescale_o),
      .uops_issued_o(uops_issued_o), .idle_clocks_o(idle_clocks_o),
      .desync_o(exec_desync_o)
  );

endmodule

`default_nettype wire
