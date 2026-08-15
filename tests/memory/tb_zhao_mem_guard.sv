// tb_zhao_mem_guard.sv — TESTBENCH WRAPPER (W2.5): zhao_mem_guard feeding
// the full arbiter + ctrl + model chain, so "nothing was written" is proven
// against the real memory (shadow compare through the model peek port).
// Flat ports for the Verilator C++ harness. Testbench tree — never linted.

module tb_zhao_mem_guard
  import zhao_pkg::*;
(
  input logic clk,
  input logic rst_n,

  // ---- the guard's (muxed) client port ------------------------------------
  input logic        g_valid,
  input logic        g_write,
  input logic [2:0]  g_client,
  input logic [26:0] g_addr,
  input logic [6:0]  g_len,
  input logic [63:0] g_be,
  output logic       g_ready,
  output logic       g_ok,
  output logic       g_violation,

  // ---- region map (CMD.SCHEDULER grants) ----------------------------------
  input logic        map_valid,
  input logic        blit_slot,
  input logic [31:0] blit_span,

  // ---- guard events --------------------------------------------------------
  output logic        guard_violation,
  output logic [31:0] guard_violations,
  output logic [2:0]  viol_client,
  output logic        viol_write,
  output logic [26:0] viol_addr,
  output logic [6:0]  viol_len,

  // ---- write data path + ctrl observability (same taps as the chain TB) ---
  input  logic [15:0] wdata,
  output logic        init_done,
  output logic        ctrl_grant,
  output logic [26:0] ctrl_addr,
  output logic [3:0]  ctrl_words,
  output logic        ctrl_write,
  output logic        wr_beat,
  output logic        rdata_valid,
  output logic [15:0] rdata,

  // ---- model peek (shadow compare) ----------------------------------------
  input  logic        peek_en,
  input  logic [25:0] peek_waddr,
  output logic [15:0] peek_data,
  output logic        model_error
);

  zhao_guard_req_t  g_req;
  zhao_guard_rsp_t  g_rsp;
  zhao_arb_req_t    arb_req;
  zhao_arb_rsp_t    arb_rsp;
  zhao_guard_req_t  viol_req;

  assign g_req.valid  = g_valid;
  assign g_req.write  = g_write;
  assign g_req.client = zhao_client_e'(g_client);
  assign g_req.addr   = g_addr;
  assign g_req.len    = g_len;
  assign g_req.be     = g_be;
  assign g_ready      = g_rsp.ready;
  assign g_ok         = g_rsp.ok;
  assign g_violation  = g_rsp.violation;

  assign viol_client = viol_req.client;
  assign viol_write  = viol_req.write;
  assign viol_addr   = viol_req.addr;
  assign viol_len    = viol_req.len;

  zhao_mem_guard u_guard (
    .clk, .rst_n,
    .req (g_req), .rsp (g_rsp),
    .map_valid, .blit_slot, .blit_span,
    .arb_req, .arb_rsp,
    .guard_violation, .guard_violations,
    .guard_violation_req (viol_req)
  );

  // arbiter: only the guard port is wired (clients 1..4 idle)
  zhao_arb_req_t [4:0] client_req;
  zhao_arb_rsp_t [4:0] client_rsp;
  assign client_req[1] = arb_req;
  assign arb_rsp      = client_rsp[1];   // the guard sees the arbiter's rsp
  assign client_req[0] = '0;
  assign client_req[2] = '0;
  assign client_req[3] = '0;
  assign client_req[4] = '0;

  logic hold_refresh;
  zhao_arb_req_t ctrl_req;
  zhao_arb_rsp_t ctrl_rsp;

  zhao_vram_arbiter u_arb (
    .clk, .rst_n,
    .client_req, .client_rsp,
    .ctrl_req, .hold_refresh, .ctrl_rsp,
    .frame_tick (1'b0),
    .vram_bytes (), .vram_bytes_shadow (), .scanout_preempted ()
  );

  logic        phy_cs_n, phy_ras_n, phy_cas_n, phy_we_n, phy_dq_oe;
  logic [12:0] phy_a;
  logic [1:0]  phy_ba, phy_dqm;
  logic [15:0] phy_dq_o, phy_dq_i;

  zhao_sdram_ctrl u_ctrl (
    .clk, .rst_n,
    .req (ctrl_req), .rsp (ctrl_rsp), .hold_refresh,
    .wdata, .wr_beat, .rdata, .rdata_valid,
    .phy_cs_n, .phy_ras_n, .phy_cas_n, .phy_we_n,
    .phy_a, .phy_ba, .phy_dq_o, .phy_dq_oe, .phy_dqm, .phy_dq_i,
    .init_done, .refresh_stalls (), .bank_conflicts (), .refresh_pulse ()
  );

  assign ctrl_grant = ctrl_rsp.grant;
  assign ctrl_addr  = ctrl_req.addr;
  assign ctrl_words = ctrl_req.len[3:0];
  assign ctrl_write = ctrl_req.write;

  zhao_sdram_model u_model (
    .clk,
    .phy_cs_n, .phy_ras_n, .phy_cas_n, .phy_we_n,
    .phy_a, .phy_ba, .phy_dq_o, .phy_dq_oe, .phy_dqm, .phy_dq_i,
    .peek_en, .peek_waddr, .peek_data,
    .err_trcd (), .err_trp (), .err_trc (),
    .err_refresh_interval (), .err_protocol (), .err_mrs (),
    .model_error
  );

endmodule
