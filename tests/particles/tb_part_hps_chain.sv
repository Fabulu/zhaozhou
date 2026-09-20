// tb_part_hps_chain.sv -- the particle generation store end to end: the REAL
// `zhao_part_hps` streamer, the REAL `zhao_part_state`, and the REAL
// `zhao_hps_arbiter_n` in front of the bridge, with a rival client at the
// higher priority so the streamer is made to wait.
//
// The C++ driver plays the three things outside this chain: PART.UPDATE and
// PART.SPAWN (the verdicts and the children) and the HPS itself (DDR memory
// behind `zhao_hps_bridge`'s protocol, per plan D10). What it checks is that a
// record put into DDR by the HPS comes back out of DDR in the next generation,
// in the contract's order, bit for bit.
`default_nettype none

module tb_part_hps_chain #(
    parameter int unsigned CAPACITY  = 16,
    parameter int unsigned SPECIES_N = 4,
    parameter int unsigned CHILD_D   = 8,
    parameter int unsigned CNT_W     = $clog2(CAPACITY) + 1
) (
    input  var logic             clk,
    input  var logic             rst_n,

    input  var logic [31:0]      cfg_base0_i,
    input  var logic [31:0]      cfg_base1_i,
    input  var logic             seed_valid_i,
    output var logic             seed_ready_o,
    input  var logic             seed_buf_i,
    input  var logic [CNT_W-1:0] seed_count_i,
    input  var logic             tick_i,

    // PART.UPDATE's side of PART.STATE
    output var logic             prt_valid_o,
    input  var logic             prt_ready_i,
    output var logic [127:0]     prt_record_o,
    input  var logic             vrd_valid_i,
    output var logic             vrd_ready_o,
    input  var logic             vrd_survive_i,
    input  var logic [127:0]     vrd_record_i,
    // PART.SPAWN's side
    input  var logic             chl_valid_i,
    output var logic             chl_ready_o,
    input  var logic [127:0]     chl_record_i,
    input  var logic             chl_busy_i,

    // A REAL BRIDGE REFUSAL (R54). Nothing here fakes a response. While this
    // is held, the address on its way INTO `zhao_hps_bridge` is misaligned by
    // one qword -- which is one of the two conditions the bridge itself calls
    // `malformed` (`zhao_hps_bridge.sv:106`). The bridge then runs its OWN
    // refusal path: `err` and `last` high, NO grant, nothing issued, and
    // `hps_err_count` incremented. That is the shape the streamer used to be
    // blind to, and it is produced by the block that produces it in silicon.
    // It applies to whichever client owns the bridge, so the rival is left off
    // in the cases that use it.
    input  var logic             err_inject_i,

    // the rival bridge client (index 0, higher priority): reads only
    input  var logic             rv_valid_i,
    input  var logic [31:0]      rv_addr_i,
    input  var logic [6:0]       rv_len_i,
    output var logic             rv_grant_o,
    output var logic             rv_beat_o,

    // the HPS side of the REAL `zhao_hps_bridge`: the C++ harness is the HPS
    output var logic             hps_req_valid_o,
    output var logic             hps_req_write_o,
    output var logic [31:0]      hps_req_addr_o,
    output var logic [6:0]       hps_req_len_o,
    input  var logic             hps_req_grant_i,
    output var logic             hps_wr_valid_o,
    output var logic [63:0]      hps_wr_data_o,
    output var logic             hps_wr_last_o,
    input  var logic             hps_rd_valid_i,
    input  var logic [63:0]      hps_rd_data_i,
    input  var logic             hps_rd_last_i,
    output var logic [31:0]      br_err_count_o,
    output var logic [31:0]      br_wr_early_o,
    // the bridge's client tag as it was requested, sampled on its accept
    output var logic [2:0]       br_req_client_o,
    output var logic             br_req_valid_o,

    // evidence
    output var logic             ps_tick_done_o,
    output var logic             hps_busy_o,
    output var logic             cur_buf_o,
    output var logic [CNT_W-1:0] cur_count_o,
    output var logic [31:0]      ticks_o,
    output var logic [31:0]      ticks_dropped_o,
    output var logic [31:0]      ticks_unseeded_o,
    output var logic [31:0]      seeds_o,
    output var logic [31:0]      seeds_refused_o,
    output var logic [31:0]      rd_bursts_o,
    output var logic [31:0]      wr_bursts_o,
    output var logic [31:0]      records_read_o,
    output var logic [31:0]      records_written_o,
    output var logic [31:0]      bridge_errs_o,
    output var logic [31:0]      ticks_faulted_o,
    output var logic [31:0]      records_discarded_o,
    output var logic             tick_abort_o,
    output var logic [31:0]      survivors_o,
    output var logic [31:0]      children_written_o,
    output var logic [31:0]      arb_c1_wait_o,
    // R55: the streamer is a HOLDER -- it re-presents the same request until
    // grant or refusal -- so it can never make the arbiter drop a second,
    // different one. This is that claim as a reading.
    output var logic [31:0]      arb_pend_dropped_o
);

  import zhao_pkg::*;

  logic             ps_tick_start, ps_rd_empty;
  logic             rd_valid, rd_ready, rd_last;
  logic [127:0]     rd_record;
  logic             wr_valid, wr_ready;
  logic [127:0]     wr_record;

  zhao_hps_burst_req_t          p_req;
  logic                         p_grant;
  zhao_hps_burst_rsp_t          p_rsp;
  logic                         p_wr_valid, p_wr_last;
  logic [63:0]                  p_wr_data;
  logic        b_grant, b_wr_valid, b_wr_last, b_wr_ready;
  logic [63:0] b_wr_data;

  zhao_part_hps #(
      .CAPACITY(CAPACITY),
      .CNT_W   (CNT_W)
  ) u_hps (
      .clk(clk), .rst_n(rst_n),
      .cfg_base0_i(cfg_base0_i), .cfg_base1_i(cfg_base1_i),
      .seed_valid_i(seed_valid_i), .seed_ready_o(seed_ready_o),
      .seed_buf_i(seed_buf_i), .seed_count_i(seed_count_i),
      .tick_i(tick_i),
      .ps_tick_start_o(ps_tick_start), .ps_rd_empty_o(ps_rd_empty),
      .ps_tick_abort_o(tick_abort_o),
      .ps_tick_done_i(ps_tick_done_o),
      .rd_valid_o(rd_valid), .rd_ready_i(rd_ready),
      .rd_record_o(rd_record), .rd_last_o(rd_last),
      .wr_valid_i(wr_valid), .wr_ready_o(wr_ready), .wr_record_i(wr_record),
      .hps_req_o(p_req), .hps_grant_i(p_grant), .hps_rsp_i(p_rsp),
      .hps_wr_valid_o(p_wr_valid), .hps_wr_data_o(p_wr_data),
      .hps_wr_last_o(p_wr_last), .hps_wr_ready_i(b_wr_ready),
      .busy_o(hps_busy_o), .cur_buf_o(cur_buf_o), .cur_count_o(cur_count_o),
      .ticks_o(ticks_o), .ticks_dropped_o(ticks_dropped_o),
      .ticks_unseeded_o(ticks_unseeded_o),
      .seeds_o(seeds_o), .seeds_refused_o(seeds_refused_o),
      .rd_bursts_o(rd_bursts_o), .wr_bursts_o(wr_bursts_o),
      .records_read_o(records_read_o), .records_written_o(records_written_o),
      .bridge_errs_o(bridge_errs_o), .ticks_faulted_o(ticks_faulted_o),
      .records_discarded_o(records_discarded_o)
  );

  /* verilator lint_off UNUSEDSIGNAL */
  logic        ps_busy, ps_cap_full;
  logic [31:0] ps_dropped, ps_stall, ps_refused;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_part_state #(
      .CAPACITY (CAPACITY),
      .CHILD_D  (CHILD_D),
      .SPECIES_N(SPECIES_N)
  ) u_ps (
      .clk(clk), .rst_n(rst_n),
      .tick_start_i(ps_tick_start), .tick_busy_o(ps_busy), .tick_done_o(ps_tick_done_o),
      .rd_valid_i(rd_valid), .rd_ready_o(rd_ready), .rd_record_i(rd_record),
      .rd_last_i(rd_last), .rd_empty_i(ps_rd_empty),
      .tick_abort_i(tick_abort_o),
      .prt_valid_o(prt_valid_o), .prt_ready_i(prt_ready_i), .prt_record_o(prt_record_o),
      .vrd_valid_i(vrd_valid_i), .vrd_ready_o(vrd_ready_o),
      .vrd_survive_i(vrd_survive_i), .vrd_record_i(vrd_record_i),
      .chl_valid_i(chl_valid_i), .chl_ready_o(chl_ready_o),
      .chl_record_i(chl_record_i), .chl_busy_i(chl_busy_i),
      .wr_valid_o(wr_valid), .wr_ready_i(wr_ready), .wr_record_o(wr_record),
      .capacity_full_o(ps_cap_full),
      .survivors_o(survivors_o), .children_written_o(children_written_o),
      .children_dropped_capacity_o(ps_dropped),
      .staging_stall_cycles_o(ps_stall), .species_refused_o(ps_refused)
  );

  // ---- the arbiter: rival at 0, the particle store at 1 ------------------
  zhao_hps_burst_req_t [1:0] a_req;
  logic                [1:0] a_grant;
  zhao_hps_burst_rsp_t [1:0] a_rsp;
  zhao_hps_burst_req_t       b_req_raw;
  zhao_hps_burst_req_t       b_req;
  zhao_hps_burst_rsp_t       b_rsp;

  // R54: the one line that makes a REAL refusal happen. See `err_inject_i`.
  always_comb begin
    b_req = b_req_raw;
    if (err_inject_i) b_req.addr = {b_req_raw.addr[31:6], 6'h08};
  end
  /* verilator lint_off UNUSEDSIGNAL */
  logic [1:0][31:0]          a_bursts;
  logic [1:0]                arb_pend_dropped_mask_unused;
  /* verilator lint_on UNUSEDSIGNAL */

  always_comb begin
    a_req[0]        = '0;
    a_req[0].valid  = rv_valid_i;
    a_req[0].write  = 1'b0;
    a_req[0].client = ZHAO_CLIENT_DEBUG;
    a_req[0].addr   = rv_addr_i;
    a_req[0].len    = rv_len_i;
    a_req[1]        = p_req;
  end
  assign rv_grant_o = a_grant[0];
  assign rv_beat_o  = a_rsp[0].beat_valid;
  assign p_grant    = a_grant[1];
  assign p_rsp      = a_rsp[1];

  /* verilator lint_off UNUSEDSIGNAL */
  logic [6:0][31:0] br_bytes, br_bytes_shadow;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_hps_arbiter_n #(.N(2)) u_arb (
      .clk(clk), .rst_n(rst_n),
      .req_i(a_req), .req_grant_o(a_grant),
      .wr_valid_i({p_wr_valid, 1'b0}),
      .wr_data_i ({p_wr_data, 64'd0}),
      .wr_last_i ({p_wr_last, 1'b0}),
      .rsp_o(a_rsp),
      .b_req_o(b_req_raw), .b_req_grant_i(b_grant),
      .b_wr_valid_o(b_wr_valid), .b_wr_data_o(b_wr_data), .b_wr_last_o(b_wr_last),
      .b_rsp_i(b_rsp),
      .bursts_o(a_bursts), .wait_cycles_o(arb_c1_wait_o),
      .pend_dropped_o(arb_pend_dropped_o),
      .pend_dropped_mask_o(arb_pend_dropped_mask_unused)
  );

  assign br_req_client_o = b_req.client;
  assign br_req_valid_o  = b_req.valid;

  zhao_hps_bridge u_bridge (
      .clk(clk), .rst_n(rst_n),
      .req(b_req), .req_grant(b_grant),
      .wr_valid(b_wr_valid), .wr_data(b_wr_data), .wr_last(b_wr_last),
      .rsp(b_rsp),
      .hps_req_valid(hps_req_valid_o), .hps_req_write(hps_req_write_o),
      .hps_req_addr(hps_req_addr_o), .hps_req_len(hps_req_len_o),
      .hps_req_grant(hps_req_grant_i),
      .hps_wr_valid(hps_wr_valid_o), .hps_wr_data(hps_wr_data_o), .hps_wr_last(hps_wr_last_o),
      .hps_rd_valid(hps_rd_valid_i), .hps_rd_data(hps_rd_data_i), .hps_rd_last(hps_rd_last_i),
      .frame_tick(1'b0),
      .hps_bytes(br_bytes), .hps_bytes_shadow(br_bytes_shadow),
      .hps_err_count(br_err_count_o),
      .wr_ready(b_wr_ready), .wr_early_beats(br_wr_early_o)
  );

endmodule : tb_part_hps_chain

`default_nettype wire
