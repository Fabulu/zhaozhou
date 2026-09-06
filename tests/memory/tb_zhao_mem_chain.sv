// tb_zhao_mem_chain.sv — TESTBENCH WRAPPER (W2.5): zhao_vram_arbiter +
// zhao_sdram_ctrl + the behavioural model (sim/models/zhao_sdram_model.sv,
// testbench-only). Flat scalar ports for the Verilator C++ harness.
//
// Used by: sdram_directed, vram_arbiter_directed, mem_random (fast/nightly),
// mem_bandwidth_budget. Never linted/synthesized (tests/memory/ is a
// testbench tree; the model carries the TESTBENCH-ONLY banner).

module tb_zhao_mem_chain
  import zhao_pkg::*;
(
  input logic clk,
  input logic rst_n,

  // ---- five arbiter client ports (zhao_client_e order; len = BYTES) -------
  input logic [4:0]  c_valid,
  input logic [4:0]  c_write,
  input logic [4:0][26:0] c_addr,
  input logic [4:0][6:0]  c_len,
  output logic [4:0] c_grant,
  output logic [4:0][7:0] c_credits,

  // ---- write data path into the ctrl --------------------------------------
  input  logic [15:0] wdata,

  // ---- ctrl observability --------------------------------------------------
  output logic        init_done,
  output logic        ctrl_grant,      // SDRAM-edge acceptance (cycle G)
  output logic [26:0] ctrl_addr,       // the accepted burst's byte address
  output logic [2:0]  ctrl_client,     // issuing client (zhao_client_e)
  output logic [3:0]  ctrl_words,      // 1..8 (0 encodes 8 on the wire)
  output logic        ctrl_write,
  output logic        ctrl_req_valid,
  output logic        wr_beat,
  output logic        rdata_valid,
  output logic [15:0] rdata,
  output logic [31:0] refresh_stalls,
  output logic [31:0] bank_conflicts,
  output logic        refresh_pulse,
  output logic [3:0]  dbg_ctrl_state,
  output logic [3:0]  dbg_ctrl_beat,
  output logic        dbg_ctrl_cur_write,
  output logic [3:0]  dbg_ctrl_cur_words,
  output logic        dbg_sel_valid,
  output logic [2:0]  dbg_sel,
  output logic [4:0]  dbg_eligible,
  output logic [4:0][5:0] dbg_age,

  // ---- arbiter counters ----------------------------------------------------
  input  logic        frame_tick,
  output logic [31:0] vram_bytes_0, vram_bytes_1, vram_bytes_2,
                      vram_bytes_3, vram_bytes_4,
  output logic [31:0] scanout_preempted,

  // ---- model peek + error flags -------------------------------------------
  input  logic        peek_en,
  input  logic [25:0] peek_waddr,
  output logic [15:0] peek_data,
  output logic        model_error,
  output logic [5:0]  model_err_kind   // {mrs,protocol,refresh,trc,trp,trcd}
);

  zhao_arb_req_t [4:0] client_req;
  zhao_arb_rsp_t [4:0] client_rsp;
  zhao_arb_req_t       ctrl_req;
  zhao_arb_rsp_t       ctrl_rsp;
  logic                hold_refresh;

  genvar gi;
  generate
    for (gi = 0; gi < 5; gi++) begin : g_map
      assign client_req[gi].valid  = c_valid[gi];
      assign client_req[gi].write  = c_write[gi];
      assign client_req[gi].client = zhao_client_e'(3'(gi));
      assign client_req[gi].addr   = c_addr[gi];
      assign client_req[gi].len    = c_len[gi];
      assign c_grant[gi]   = client_rsp[gi].grant;
      assign c_credits[gi] = client_rsp[gi].credits;
    end
  endgenerate

  logic [4:0][31:0] vb_flat;
  assign vram_bytes_0 = vb_flat[0];
  assign vram_bytes_1 = vb_flat[1];
  assign vram_bytes_2 = vb_flat[2];
  assign vram_bytes_3 = vb_flat[3];
  assign vram_bytes_4 = vb_flat[4];

  zhao_vram_arbiter u_arb (
    .clk, .rst_n,
    .client_req, .client_rsp,
    .ctrl_req, .hold_refresh, .ctrl_rsp,
    .frame_tick,
    .vram_bytes (vb_flat),
    .vram_bytes_shadow (),
    .scanout_preempted
  );

  zhao_sdram_ctrl u_ctrl (
    .clk, .rst_n,
    .req           (ctrl_req),
    .rsp           (ctrl_rsp),
    .hold_refresh,
    .wdata,
    .wr_beat,
    .rdata, .rdata_valid,
    .phy_cs_n   (phy_cs_n),
    .phy_ras_n  (phy_ras_n),
    .phy_cas_n  (phy_cas_n),
    .phy_we_n   (phy_we_n),
    .phy_a      (phy_a),
    .phy_ba     (phy_ba),
    .phy_dq_o   (phy_dq_o),
    .phy_dq_oe  (phy_dq_oe),
    .phy_dqm    (phy_dqm),
    .phy_dq_i   (phy_dq_i),
    .init_done, .refresh_stalls, .bank_conflicts, .refresh_pulse
  );

  // ctrl observability taps
  assign ctrl_grant  = ctrl_rsp.grant;
  assign ctrl_addr   = ctrl_req.addr;
  assign ctrl_client = ctrl_req.client;
  assign ctrl_words  = ctrl_req.len[3:0];
  assign ctrl_write  = ctrl_req.write;
  assign ctrl_req_valid = ctrl_req.valid;
  assign dbg_ctrl_state     = u_ctrl.state;
  assign dbg_ctrl_beat      = u_ctrl.beat;
  assign dbg_ctrl_cur_write = u_ctrl.cur_write;
  assign dbg_ctrl_cur_words = u_ctrl.cur_words;
  assign dbg_sel_valid      = u_arb.sel_valid;
  assign dbg_sel            = u_arb.sel;
  assign dbg_eligible       = u_arb.eligible;
  assign dbg_age[0] = u_arb.age[0];
  assign dbg_age[1] = u_arb.age[1];
  assign dbg_age[2] = u_arb.age[2];
  assign dbg_age[3] = u_arb.age[3];
  assign dbg_age[4] = u_arb.age[4];

  logic        phy_cs_n, phy_ras_n, phy_cas_n, phy_we_n, phy_dq_oe;
  logic [12:0] phy_a;
  logic [1:0]  phy_ba, phy_dqm;
  logic [15:0] phy_dq_o, phy_dq_i;

  zhao_sdram_model u_model (
    .clk,
    .phy_cs_n, .phy_ras_n, .phy_cas_n, .phy_we_n,
    .phy_a, .phy_ba,
    .phy_dq_o, .phy_dq_oe, .phy_dqm, .phy_dq_i,
    .peek_en, .peek_waddr, .peek_data,
    // The poke backdoor exists for the shell bench, which has to PLACE an
    // asset pool in memory before the geometry fetcher reads it. Nothing
    // here needs it, so it is tied off explicitly rather than left to a
    // PINMISSING that would read as an oversight.
    .poke_en(1'b0), .poke_waddr(26'd0), .poke_data(16'd0),
    .err_trcd (model_err_kind[0]), .err_trp (model_err_kind[1]),
    .err_trc (model_err_kind[2]),
    .err_refresh_interval (model_err_kind[3]), .err_protocol (model_err_kind[4]),
    .err_mrs (model_err_kind[5]),
    .model_error
  );

endmodule
