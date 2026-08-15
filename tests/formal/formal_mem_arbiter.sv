// formal_mem_arbiter.sv — formal harness for mem_vram_arbiter_liveness
// (plan W2.5 / plan §4). Composes the REAL zhao_vram_arbiter and the REAL
// zhao_sdram_ctrl (both synthesizable, no memory model needed — the ctrl
// owns only timing).
//
// PROPERTY (the frozen D3 bound, spec/memory_rules.md §2):
//   once a guaranteed client's request is accepted at its port
//   (client_rsp[k].grant pulse), its FIRST SDRAM-edge burst
//   (ctrl_rsp.grant with ctrl_req.client == k) is served within
//   B = ZHAO_ARB_LIVENESS_BOUND = 40 cycles.
//
// ASSUMPTIONS (environment law, all documented):
//   * Phase-2 guaranteed population (memory_rules §2: G=2): clients
//     engine0/engine1/debug never raise valid (their ports are the reserved
//     reservation; the frozen bound derivation is for G=2).
//   * One outstanding request per client: the environment holds valid until
//     the port grant, drops it, and may re-arm later (anyseq) — it never
//     overlaps a second request onto an unserved one.
//   * Request lengths are legal (1..64 bytes).
//   * The bound is a STEADY-STATE service bound and is checked only once the
//     SDRAM controller reports init_done. During power-on initialisation the
//     controller serves nobody by design, so an ungated check simply measures
//     the init sequence and fails — which is what it did the first time this
//     harness was ever actually elaborated.

// RESET AND ENVIRONMENT DISCIPLINE (see formal_mem_guard.sv for the full
// reasoning; the same two defects were present here):
//   * `rst_n` must NOT be a free input. With it free the solver simply starts
//     the trace out of reset with arbitrary register contents — `waiting` set
//     and `waited` already past the bound — and the property fails at step 0
//     for reasons that say nothing about the arbiter (the free-init trap).
//     It is generated below from an initialised counter instead.
//   * the free environment must be PORTS, not `(* anyseq *)` locals: that
//     attribute does not survive this frontend and elaborates to constants,
//     which silently empties the model.
module formal_mem_arbiter
  import zhao_pkg::*;
(
  input logic clk,
  // free per-client request environment (2 guaranteed Phase-2 clients)
  input logic [1:0]  env_valid,
  input logic [1:0]  env_write,
  input logic [1:0]  env_rearm,
  input logic [6:0]  env_len0,
  input logic [6:0]  env_len1,
  input logic [26:0] env_addr0,
  input logic [26:0] env_addr1,
  input logic [15:0] env_wdata_i
);

  // ---- reset discipline -----------------------------------------------------
  logic [3:0] cyc = 4'd0;
  always_ff @(posedge clk) begin
    if (cyc != 4'hF) cyc <= cyc + 4'd1;
  end
  wire rst_n    = (cyc >= 4'd2);   // low for cycles 0,1 — registers reset
  wire checking = (cyc >= 4'd3);   // assert only once reset has been released

  // ---- free environment -----------------------------------------------------
  logic [6:0]  env_len  [0:1];
  logic [26:0] env_addr [0:1];
  always_comb begin
    env_len[0]  = env_len0;
    env_len[1]  = env_len1;
    env_addr[0] = env_addr0;
    env_addr[1] = env_addr1;
  end

  // write data is a don't-care constant for this property
  logic [15:0] env_wdata;
  always_ff @(posedge clk) begin
    if (cyc == 4'd0) env_wdata <= env_wdata_i;
  end

  // legality assumptions
  always_comb begin
    for (int k = 0; k < 2; k++) begin
      assume(env_len[k] >= 7'd1 && env_len[k] <= 7'd64);
    end
  end

  // ---- client request generation (one outstanding per client) ----------------
  logic [1:0] holding;          // request offered, not yet accepted at port
  zhao_arb_req_t [4:0] client_req;
  zhao_arb_rsp_t [4:0] client_rsp;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      holding <= 2'b0;
    end else begin
      for (int k = 0; k < 2; k++) begin
        if (!holding[k] && env_valid[k] && env_rearm[k]) holding[k] <= 1'b1;
        else if (holding[k] && client_rsp[k].grant) holding[k] <= 1'b0;
      end
    end
  end

  always_comb begin
    for (int k = 0; k < 5; k++) begin
      client_req[k] = '0;
      if (k < 2) begin
        client_req[k].valid = holding[k];
        client_req[k].write = env_write[k];
        client_req[k].client = zhao_client_e'(3'(k));
        client_req[k].addr = env_addr[k];
        client_req[k].len = env_len[k];
      end
    end
  end

  // ---- the DUT pair -----------------------------------------------------------
  logic hold_refresh;
  logic ctrl_init_done;
  zhao_arb_req_t ctrl_req;
  zhao_arb_rsp_t ctrl_rsp;
  logic unused_wr_beat, unused_rdata_valid;
  logic [15:0] unused_rdata;

  zhao_vram_arbiter u_arb (
    .clk, .rst_n,
    .client_req, .client_rsp,
    .ctrl_req, .hold_refresh, .ctrl_rsp,
    .frame_tick (1'b0),
    .vram_bytes (), .vram_bytes_shadow (), .scanout_preempted ()
  );

  zhao_sdram_ctrl u_ctrl (
    .clk, .rst_n,
    .req (ctrl_req), .rsp (ctrl_rsp), .hold_refresh,
    .wdata (env_wdata), .wr_beat (unused_wr_beat),
    .rdata (unused_rdata), .rdata_valid (unused_rdata_valid),
    .phy_cs_n (), .phy_ras_n (), .phy_cas_n (), .phy_we_n (),
    .phy_a (), .phy_ba (), .phy_dq_o (), .phy_dq_oe (), .phy_dqm (),
    .phy_dq_i (16'h0),
    .init_done (ctrl_init_done), .refresh_stalls (), .bank_conflicts (),
    .refresh_pulse ()
  );

  // ---- the property: B-bound first-burst service ------------------------------
  logic [1:0] waiting;      // accepted at port, first burst not yet served
  logic [6:0] waited [0:1];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      waiting <= 2'b0;
      waited[0] <= 7'd0;
      waited[1] <= 7'd0;
    end else begin
      for (int k = 0; k < 2; k++) begin
        if (client_rsp[k].grant) begin
          waiting[k] <= 1'b1;
          waited[k] <= 7'd0;
        end else if (ctrl_rsp.grant && (ctrl_req.client == zhao_client_e'(3'(k)))) begin
          waiting[k] <= 1'b0;   // served (first burst of the request)
          waited[k] <= 7'd0;
        end else if (waiting[k]) begin
          waited[k] <= waited[k] + 7'd1;
        end
      end
    end
  end

  // !! THIS PROPERTY CURRENTLY FAILS — AND THE FAILURE IS REAL. !!
  //
  // The first time this harness was ever actually elaborated (it had been
  // SKIPped, and its environment was empty besides), the frozen D3 bound
  // B = 40 did not hold. Measured by re-running this proof with the bound
  // raised: it FAILS at 40, and PASSES at 60, 90 and 120 — so the true
  // worst-case first-burst latency is somewhere in 41..60 cycles.
  //
  // The likely reason the derivation is short: B = G*MAX_BURST +
  // REFRESH_OVERHEAD = 2*14 + 12 = 40 budgets each client's turn at ONE
  // maximum burst, but a 64-byte request is 32 words = four 8-word bursts, so
  // one client's turn can cost about 4*14 and the other client waits behind
  // all of it.
  //
  // The assertion below deliberately still states the FROZEN bound, so this
  // proof fails loudly instead of quietly agreeing with itself. Deciding
  // whether the spec constant is wrong (re-derive B) or the arbiter must
  // interleave at burst granularity is a ratification call for the
  // orchestrator, not something to paper over by editing a frozen number.
  // Note this lane is labelled formal/nightly, not fast.
  always_comb begin
    for (int k = 0; k < 2; k++) begin
      if (checking && ctrl_init_done && waiting[k]) begin
        assert (waited[k] < 7'd40)
          else $error("liveness: client %0d unserved for %0d cycles (bound B=40)",
                      k, waited[k]);
      end
    end
  end

  // Non-vacuity: the bound means nothing unless a client actually gets
  // accepted at the port and then actually gets served. Both must be
  // reachable, or the assertion above is an empty implication.
  always_ff @(posedge clk) begin
    if (checking) begin
      c_init:      cover (ctrl_init_done);
      c_accepted:  cover (ctrl_init_done && waiting[0]);
      c_accepted1: cover (ctrl_init_done && waiting[1]);
      c_served:    cover (ctrl_init_done && ctrl_rsp.grant);
      c_waited:    cover (ctrl_init_done && waiting[0] && waited[0] > 7'd8);
      c_both:      cover (ctrl_init_done && waiting[0] && waiting[1]);
    end
  end

endmodule
