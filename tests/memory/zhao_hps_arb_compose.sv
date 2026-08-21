// zhao_hps_arb_compose.sv — the arbiter wired to the REAL bridge.
//
// Testbench component for tests/memory/hps_arbiter_directed.cpp, never
// synthesis. It answers the review's Case H directly
// (reports/DEBUG.FRAMEBLIT_Integration_Corrections.md §12): hold the bridge
// busy with one client's burst, present the other's request, and prove no
// request is dropped, no request is duplicated, and no response beat reaches
// the wrong client.
//
// The bridge here is the REAL `zhao_hps_bridge`, not a model of it. That
// matters more than usual: the property being tested is a protocol agreement
// between the arbiter and the bridge, and a fake bridge is exactly the thing
// that agrees with whatever the arbiter does. In particular the real bridge
// answers a request arriving while it is BUSY with `err` rather than stalling,
// and answers a MALFORMED request with `err` and no grant at all -- neither of
// which a permissive stub would ever do.
//
// The C++ harness is the HPS.
module zhao_hps_arb_compose (
    input logic clk,
    input logic rst_n,

    // ---- client 0 (CMD.DMA's place) ----------------------------------------
    input  logic        c0_valid_i,
    input  logic        c0_write_i,
    input  logic [ 2:0] c0_client_i,
    input  logic [31:0] c0_addr_i,
    input  logic [ 6:0] c0_len_i,
    output logic        c0_grant_o,
    input  logic        c0_wr_valid_i,
    input  logic [63:0] c0_wr_data_i,
    input  logic        c0_wr_last_i,
    output logic        c0_beat_valid_o,
    output logic [63:0] c0_beat_data_o,
    output logic        c0_beat_last_o,
    output logic        c0_beat_err_o,

    // ---- client 1 (DEBUG.FRAMEBLIT's place) --------------------------------
    input  logic        c1_valid_i,
    input  logic        c1_write_i,
    input  logic [ 2:0] c1_client_i,
    input  logic [31:0] c1_addr_i,
    input  logic [ 6:0] c1_len_i,
    output logic        c1_grant_o,
    input  logic        c1_wr_valid_i,
    input  logic [63:0] c1_wr_data_i,
    input  logic        c1_wr_last_i,
    output logic        c1_beat_valid_o,
    output logic [63:0] c1_beat_data_o,
    output logic        c1_beat_last_o,
    output logic        c1_beat_err_o,

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

    input  logic        frame_tick_i,
    output logic [31:0] c0_bursts_o,
    output logic [31:0] c1_bursts_o,
    output logic [31:0] c1_wait_cycles_o,
    output logic [31:0] hps_err_count_o
);

  zhao_pkg::zhao_hps_burst_req_t c0_req, c1_req, b_req;
  zhao_pkg::zhao_hps_burst_rsp_t c0_rsp, c1_rsp, b_rsp;
  logic b_grant, b_wr_valid, b_wr_last;
  logic [63:0] b_wr_data;

  always_comb begin
    c0_req.valid  = c0_valid_i;
    c0_req.write  = c0_write_i;
    c0_req.client = zhao_pkg::zhao_client_e'(c0_client_i);
    c0_req.addr   = c0_addr_i;
    c0_req.len    = c0_len_i;

    c1_req.valid  = c1_valid_i;
    c1_req.write  = c1_write_i;
    c1_req.client = zhao_pkg::zhao_client_e'(c1_client_i);
    c1_req.addr   = c1_addr_i;
    c1_req.len    = c1_len_i;
  end

  assign c0_beat_valid_o = c0_rsp.beat_valid;
  assign c0_beat_data_o  = c0_rsp.data;
  assign c0_beat_last_o  = c0_rsp.last;
  assign c0_beat_err_o   = c0_rsp.err;
  assign c1_beat_valid_o = c1_rsp.beat_valid;
  assign c1_beat_data_o  = c1_rsp.data;
  assign c1_beat_last_o  = c1_rsp.last;
  assign c1_beat_err_o   = c1_rsp.err;

  zhao_hps_arbiter u_arb (
      .clk           (clk),
      .rst_n         (rst_n),
      .c0_req_i      (c0_req),
      .c0_req_grant_o(c0_grant_o),
      .c0_wr_valid_i (c0_wr_valid_i),
      .c0_wr_data_i  (c0_wr_data_i),
      .c0_wr_last_i  (c0_wr_last_i),
      .c0_rsp_o      (c0_rsp),
      .c1_req_i      (c1_req),
      .c1_req_grant_o(c1_grant_o),
      .c1_wr_valid_i (c1_wr_valid_i),
      .c1_wr_data_i  (c1_wr_data_i),
      .c1_wr_last_i  (c1_wr_last_i),
      .c1_rsp_o      (c1_rsp),
      .b_req_o       (b_req),
      .b_req_grant_i (b_grant),
      .b_wr_valid_o  (b_wr_valid),
      .b_wr_data_o   (b_wr_data),
      .b_wr_last_o   (b_wr_last),
      .b_rsp_i       (b_rsp)
      ,
      .c0_bursts_o     (c0_bursts_o),
      .c1_bursts_o     (c1_bursts_o),
      .c1_wait_cycles_o(c1_wait_cycles_o)
  );

  logic [4:0][31:0] hps_bytes_unused;
  logic [4:0][31:0] hps_bytes_shadow_unused;

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
      .hps_err_count   (hps_err_count_o)
  );

endmodule : zhao_hps_arb_compose
