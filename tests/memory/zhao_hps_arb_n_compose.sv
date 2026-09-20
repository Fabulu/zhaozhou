// zhao_hps_arb_n_compose.sv -- the N-client arbiter at N=3, wired to the REAL
// bridge. Testbench component for tests/memory/hps_arbiter_n_directed.cpp,
// never synthesis.
//
// Owner ruling R4 (reports/OWNER-RULINGS-20260919-EVENING.md) widened
// MEM.HPS.ARBITER to N clients and asked for the starvation law to be
// PRESERVED AND RE-PROVED. The two-client proof, tests/memory/
// zhao_hps_arb_compose.sv, now exercises the same core at N=2 through
// `zhao_hps_arbiter`; this file puts a THIRD client on it, which is the case
// no two-port test can reach: a middle client that is both outranked and
// outranking.
//
// The bridge is the real `zhao_hps_bridge` for the reason the two-client
// compose gives: the property is a protocol agreement between the arbiter and
// the bridge, and a permissive stub agrees with whatever the arbiter does.
// Ports are flattened per client so the C++ names fields rather than indexing
// packed bit positions it could be silently wrong about.
module zhao_hps_arb_n_compose (
    input logic clk,
    input logic rst_n,

    input  logic [ 2:0]       c_valid_i,
    input  logic [ 2:0]       c_write_i,
    input  logic [ 2:0][ 2:0] c_client_i,
    input  logic [ 2:0][31:0] c_addr_i,
    input  logic [ 2:0][ 6:0] c_len_i,
    output logic [ 2:0]       c_grant_o,
    input  logic [ 2:0]       c_wr_valid_i,
    input  logic [ 2:0][63:0] c_wr_data_i,
    input  logic [ 2:0]       c_wr_last_i,
    output logic [ 2:0]       c_beat_valid_o,
    output logic [ 2:0][63:0] c_beat_data_o,
    output logic [ 2:0]       c_beat_last_o,
    output logic [ 2:0]       c_beat_err_o,

    // ---- the HPS side: the C++ harness ------------------------------------
    output logic        hps_req_valid_o,
    output logic        hps_req_write_o,
    output logic [31:0] hps_req_addr_o,
    output logic [ 6:0] hps_req_len_o,
    input  logic        hps_req_grant_i,
    output logic        hps_wr_valid_o,
    output logic [63:0] hps_wr_data_o,
    output logic        hps_wr_last_o,
    input  logic        hps_rd_valid_i,
    input  logic [63:0] hps_rd_data_i,
    input  logic        hps_rd_last_i,

    input  logic              frame_tick_i,
    output logic [ 2:0][31:0] bursts_o,
    output logic [31:0]       c1_wait_cycles_o,
    output logic [31:0]       c2_wait_cycles_o,
    // R55: the pending slot's silent drop, made loud.
    output logic [31:0]       pend_dropped_o,
    output logic [ 2:0]       pend_dropped_mask_o,
    output logic [31:0]       hps_err_count_o
);

  zhao_pkg::zhao_hps_burst_req_t [2:0] c_req;
  zhao_pkg::zhao_hps_burst_rsp_t [2:0] c_rsp;
  zhao_pkg::zhao_hps_burst_req_t b_req;
  zhao_pkg::zhao_hps_burst_rsp_t b_rsp;
  logic [2:1][31:0] waits;
  logic        ac_wr_ready;
  logic [31:0] ac_wr_early;
  logic b_grant, b_wr_valid, b_wr_last;
  logic [63:0] b_wr_data;

  always_comb begin
    for (int i = 0; i < 3; i++) begin
      c_req[i].valid  = c_valid_i[i];
      c_req[i].write  = c_write_i[i];
      c_req[i].client = zhao_pkg::zhao_client_e'(c_client_i[i]);
      c_req[i].addr   = c_addr_i[i];
      c_req[i].len    = c_len_i[i];
      c_beat_valid_o[i] = c_rsp[i].beat_valid;
      c_beat_data_o[i]  = c_rsp[i].data;
      c_beat_last_o[i]  = c_rsp[i].last;
      c_beat_err_o[i]   = c_rsp[i].err;
    end
  end

  assign c1_wait_cycles_o = waits[1];
  assign c2_wait_cycles_o = waits[2];

  zhao_hps_arbiter_n #(.N(3)) u_arb (
      .clk          (clk),
      .rst_n        (rst_n),
      .req_i        (c_req),
      .req_grant_o  (c_grant_o),
      .wr_valid_i   (c_wr_valid_i),
      .wr_data_i    (c_wr_data_i),
      .wr_last_i    (c_wr_last_i),
      .rsp_o        (c_rsp),
      .b_req_o      (b_req),
      .b_req_grant_i(b_grant),
      .b_wr_valid_o (b_wr_valid),
      .b_wr_data_o  (b_wr_data),
      .b_wr_last_o  (b_wr_last),
      .b_rsp_i      (b_rsp),
      .bursts_o     (bursts_o),
      .wait_cycles_o(waits),
      .pend_dropped_o     (pend_dropped_o),
      .pend_dropped_mask_o(pend_dropped_mask_o)
  );

  logic [6:0][31:0] hps_bytes_unused;
  logic [6:0][31:0] hps_bytes_shadow_unused;

  zhao_hps_bridge u_bridge (
      .clk  (clk),
      .rst_n(rst_n),

      .req      (b_req),
      .req_grant(b_grant),
      .wr_valid (b_wr_valid),
      .wr_data  (b_wr_data),
      .wr_last  (b_wr_last),
      .rsp      (b_rsp),

      .hps_req_valid(hps_req_valid_o),
      .hps_req_write(hps_req_write_o),
      .hps_req_addr (hps_req_addr_o),
      .hps_req_len  (hps_req_len_o),
      .hps_req_grant(hps_req_grant_i),
      .hps_wr_valid (hps_wr_valid_o),
      .hps_wr_data  (hps_wr_data_o),
      .hps_wr_last  (hps_wr_last_o),
      .hps_rd_valid (hps_rd_valid_i),
      .hps_rd_data  (hps_rd_data_i),
      .hps_rd_last  (hps_rd_last_i),

      .frame_tick      (frame_tick_i),
      .hps_bytes       (hps_bytes_unused),
      .hps_bytes_shadow(hps_bytes_shadow_unused),
      .hps_err_count   (hps_err_count_o),
      .wr_ready        (ac_wr_ready),
      .wr_early_beats  (ac_wr_early)
  );

endmodule : zhao_hps_arb_n_compose
