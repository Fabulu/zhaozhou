// formal_mem_refresh.sv — formal harness for mem_sdram_refresh_bound
// (plan W2.5, BANKED evidence: MEM.SDRAM stays SPECIFIED, blocked_on:
// hardware; this property is part of the banked set).
//
// PROPERTIES (the ctrl law table, zhao_sdram_ctrl.sv):
//   R1 bounded deferral: after init, no more than INTERVAL + URGENT +
//      one burst span + the refresh execution (i.e. <= 870) cycles elapse
//      between consecutive AUTO_REFRESH commands, under ARBITRARY client
//      pressure and an adversarial hold_refresh (anyseq).
//   R2 the refresh counter register never exceeds the CNT_HARD law (the
//      hard-preempt threshold plus one burst span) — expressed at the port
//      as R1 since the counter is internal.
//
// The ctrl here runs with a fully adversarial environment: req/hold_refresh
// are anyseq, so this proves refresh cannot be starved by clients OR by the
// aging override, only bounded-deferred (the D3 liveness interplay).

// (Currently SKIPped under oss-cad-suite yosys — SV-frontend gap vs the
// frozen package style; see mem_formal_lane.cmake.in.)

// RESET AND ENVIRONMENT DISCIPLINE (see formal_mem_guard.sv for the full
// reasoning): `rst_n` is generated from an initialised counter rather than
// being a free input (a free rst_n lets the solver start out of reset with
// `since_ref` already past the bound — the free-init trap), and the free
// environment is carried on PORTS because `(* anyseq *)` locals do not
// survive this frontend and elaborate to constants.
module formal_mem_refresh
  import zhao_pkg::*;
  import zhao_sdram_params_pkg::*;
(
  input logic        clk,
  input logic        env_valid,
  input logic        env_write,
  input logic [26:0] env_addr,
  input logic [6:0]  env_len,
  input logic        env_hold,
  input logic [15:0] env_wdata_i
);

  // ---- reset discipline -----------------------------------------------------
  logic [3:0] cyc = 4'd0;
  always_ff @(posedge clk) begin
    if (cyc != 4'hF) cyc <= cyc + 4'd1;
  end
  wire rst_n    = (cyc >= 4'd2);
  wire checking = (cyc >= 4'd3);

  // write data is a don't-care constant for this property
  logic [15:0] env_wdata;
  always_ff @(posedge clk) begin
    if (cyc == 4'd0) env_wdata <= env_wdata_i;
  end

  zhao_arb_req_t req;
  assign req.valid = env_valid;
  assign req.write = env_write;
  assign req.client = ZHAO_CLIENT_BLIT_DMA;
  assign req.addr = env_addr;
  assign req.len = env_len;

  zhao_arb_rsp_t rsp;
  logic wr_beat, rdata_valid, init_done, refresh_pulse;
  logic [15:0] rdata;

  zhao_sdram_ctrl u_ctrl (
    .clk, .rst_n,
    .req, .rsp, .hold_refresh (env_hold),
    .wdata (env_wdata), .wr_beat, .rdata, .rdata_valid,
    .phy_cs_n (), .phy_ras_n (), .phy_cas_n (), .phy_we_n (),
    .phy_a (), .phy_ba (), .phy_dq_o (), .phy_dq_oe (), .phy_dqm (),
    .phy_dq_i (16'h0),
    .init_done, .refresh_stalls (), .bank_conflicts (), .refresh_pulse
  );

  // cycles since the last AUTO_REFRESH (post-init only)
  logic [10:0] since_ref;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) since_ref <= 11'd0;
    else if (refresh_pulse) since_ref <= 11'd0;
    else if (init_done) since_ref <= since_ref + 11'd1;
  end

  // worst case: hard threshold (840) + one in-flight burst span (19) + the
  // REF command offset (3) + margin — 780+40+2*19+4 = 862
  localparam logic [10:0] BOUND =
    11'(REFRESH_INTERVAL + REFRESH_URGENT + 2 * MAX_BURST_SPAN + 4);

  always_comb begin
    if (checking && init_done) begin
      assert (since_ref <= BOUND)
        else $error("refresh bound: %0d cycles since last AUTO_REFRESH (bound %0d)",
                    since_ref, BOUND);
    end
  end

  // Non-vacuity: the bound is an implication on init_done. If initialisation
  // is never reached within the BMC depth the assertion proves nothing, and
  // a refresh must actually be observed for the counter to mean anything.
  always_ff @(posedge clk) begin
    if (checking) begin
      c_init_done: cover (init_done);
      c_refresh:   cover (init_done && refresh_pulse);
      c_aged:      cover (init_done && since_ref > 11'd100);
    end
  end

endmodule
