// tb_zhao_hps_bridge.sv — TESTBENCH WRAPPER (W2.5): zhao_hps_bridge with the
// harness C++ acting as the HPS on the generic burst port (plan D10).
// Flat ports for Verilator. Testbench tree — never linted/synthesized.

module tb_zhao_hps_bridge
  import zhao_pkg::*;
(
  input logic clk,
  input logic rst_n,

  // ---- client burst port ---------------------------------------------------
  input logic        req_valid,
  input logic        req_write,
  input logic [2:0]  req_client,
  input logic [31:0] req_addr,
  input logic [6:0]  req_len,
  output logic       req_grant,
  input logic        wr_valid,
  input logic [63:0] wr_data,
  input logic        wr_last,
  output logic       rsp_beat_valid,
  output logic [63:0] rsp_data,
  output logic       rsp_last,
  output logic       rsp_err,

  // ---- HPS side (the C++ harness answers here; sim latency profile:
  //      16 cycles request->first beat, 1 beat/cycle after) -----------------
  output logic        hps_req_valid,
  output logic        hps_req_write,
  output logic [31:0] hps_req_addr,
  output logic [6:0]  hps_req_len,
  input logic         hps_req_grant,
  output logic        hps_wr_valid,
  output logic [63:0] hps_wr_data,
  output logic        hps_wr_last,
  input logic         hps_rd_valid,
  input logic [63:0]  hps_rd_data,
  input logic         hps_rd_last,

  // ---- counters / status ----------------------------------------------------
  input  logic        frame_tick,
  output logic [31:0] hps_bytes_0, hps_bytes_1, hps_bytes_2,
                      hps_bytes_3, hps_bytes_4,
  output logic [31:0] hps_err_count
);

  zhao_hps_burst_req_t  creq;
  zhao_hps_burst_rsp_t  crsp;

  assign creq.valid  = req_valid;
  assign creq.write  = req_write;
  assign creq.client = zhao_client_e'(req_client);
  assign creq.addr   = req_addr;
  assign creq.len    = req_len;
  assign rsp_beat_valid = crsp.beat_valid;
  assign rsp_data       = crsp.data;
  assign rsp_last       = crsp.last;
  assign rsp_err        = crsp.err;

  logic [4:0][31:0] hb_flat;
  assign hps_bytes_0 = hb_flat[0];
  assign hps_bytes_1 = hb_flat[1];
  assign hps_bytes_2 = hb_flat[2];
  assign hps_bytes_3 = hb_flat[3];
  assign hps_bytes_4 = hb_flat[4];

  zhao_hps_bridge u_bridge (
    .clk, .rst_n,
    .req (creq), .req_grant,
    .wr_valid, .wr_data, .wr_last,
    .rsp (crsp),
    .hps_req_valid, .hps_req_write, .hps_req_addr, .hps_req_len,
    .hps_req_grant,
    .hps_wr_valid, .hps_wr_data, .hps_wr_last,
    .hps_rd_valid, .hps_rd_data, .hps_rd_last,
    .frame_tick,
    .hps_bytes (hb_flat), .hps_bytes_shadow (), .hps_err_count
  );

endmodule
