// formal_mem_arbiter.sv — formal harness for mem_vram_arbiter_liveness
// (plan W2.5 / plan §4). Composes the REAL zhao_vram_arbiter and the REAL
// zhao_sdram_ctrl (both synthesizable, no memory model needed — the ctrl
// owns only timing).
//
// PROPERTY (the CORRECTED D3 bound, spec/memory_rules.md §2):
//   once a guaranteed client's request is accepted at its port
//   (client_rsp[k].grant pulse), its FIRST SDRAM-edge burst
//   (ctrl_rsp.grant with ctrl_req.client == k) is served within
//     BOUND    = ZHAO_ARB_LIVENESS_BOUND_NOREF = 52 cycles  (RR class), and
//     BOUND_SC = ZHAO_ARB_SCANOUT_BOUND_NOREF  = 34 cycles  (scanout).
//
// Both numbers are the REFRESH-FREE bounds and both are proven TIGHT here:
// the `bmc` task passes at them, and the `bmc_tight_rr` / `bmc_tight_scanout`
// tasks FAIL at bound-1 (expect fail). A bound that only passes is not proven
// tight, so both directions are committed — see the .sby [tasks] block.
//
// The frozen constant this replaces was B = G*MAX_BURST + REFRESH_OVERHEAD =
// 2*14 + 12 = 40, which had never been elaborated and is false. Ratified in
// runs/CLAUDE-RUNS/RUN-20260814-2154-wave2-phase2-console-shell/
// RATIFICATION-arbiter-liveness-bound.md. The derivation is in
// spec/memory_rules.md §2 and zhao_pkg.sv; in one line, a client waits out
// at most N whole burst spans and MAX_BURST_SPAN is 18, not 14:
//   scanout  N = 2 => 2*18 - 2 = 34
//   RR class N = 3 => 3*18 - 2 = 52   (N picks up the bursts-per-request
//                                      factor the old formula omitted)
//
// REFRESH IS OUT OF THIS PROOF'S HORIZON — deliberately, and guarded.
// REFRESH_INTERVAL is 780 sdram cycles; this BMC runs to depth 130, in which
// refresh_cnt cannot reach even CNT_PENDING. So the numbers above are
// refresh-free, and zhao_pkg adds ZHAO_ARB_REFRESH_STEAL = 13 analytically
// for the one AUTO_REFRESH that can land inside a wait window. The assertion
// a_horizon_is_refresh_free below MACHINE-CHECKS that scope claim: raise
// `depth` past the refresh interval and it fires. That is the signal to prove
// the composed bound properly, NOT to delete the assertion.
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
#(
  // Overridden per task from the .sby (`read_slang -G ...`) so the SAME
  // committed harness proves the bound AND disproves bound-1.
  parameter int unsigned BOUND    = ZHAO_ARB_LIVENESS_BOUND_NOREF,  // RR class
  parameter int unsigned BOUND_SC = ZHAO_ARB_SCANOUT_BOUND_NOREF    // scanout
)(
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
  // SEVEN PORTS since the terrain amendment (ruling T3: client 6, with 5 left
  // unspent). The PROPERTY is unchanged: the Phase-2 guaranteed population is
  // still G=2, so clients 2..6 are held at zero exactly as engine0/engine1/
  // debug always were. Widening the array does not widen the environment.
  zhao_arb_req_t [6:0] client_req;
  zhao_arb_rsp_t [6:0] client_rsp;

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
    for (int k = 0; k < 7; k++) begin
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

  // The measurement starts only on an acceptance at or after init_done.
  // WHY (this was a real defect found while bisecting): the arbiter's port
  // handshake does not know about init_done, so a client can be accepted at
  // its port DURING the ~26-cycle power-on init, and the offer the arbiter
  // latches then sits un-taken for the whole init sequence. Measuring from
  // such an acceptance measures the init sequence, not arbitration — it
  // inflated the scanout bound from 34 to 38 until this gate was added. D3 is
  // a STEADY-STATE service bound (the harness header always said so); making
  // the counter agree with that sentence is the fix, not raising the bound.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      waiting <= 2'b0;
      waited[0] <= 7'd0;
      waited[1] <= 7'd0;
    end else begin
      for (int k = 0; k < 2; k++) begin
        if (client_rsp[k].grant && ctrl_init_done) begin
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

  // MEASURED WORST CASES (bisected on this harness, depth 130, boolector):
  //   client 1 (RR class): passes at 52, FAILS at 51  -> tight bound 52
  //   client 0 (scanout) : passes at 34, FAILS at 33  -> tight bound 34
  // The endpoints of each pair are adjacent, so these are the exact bounds,
  // not merely values that happened to pass. The `bmc_tight_*` tasks re-run
  // the failing side as committed evidence.
  always_comb begin
    for (int k = 0; k < 2; k++) begin
      if (checking && ctrl_init_done && waiting[k]) begin
        assert (waited[k] < 7'((k == 0) ? BOUND_SC : BOUND))
          else $error("liveness: client %0d unserved for %0d cycles", k, waited[k]);
      end
    end
  end

  // ---- horizon guard: this proof is refresh-free BY CONSTRUCTION -------------
  // See the header. If someone raises `depth` past REFRESH_INTERVAL, the
  // refresh-free bounds above stop being the whole story and this fires.
  always_comb begin
    if (checking) begin
      a_horizon_is_refresh_free:
        assert (u_ctrl.refresh_cnt < 32'(zhao_sdram_params_pkg::REFRESH_INTERVAL))
          else $error("BMC horizon now reaches a refresh: the bound must be re-proven WITH refresh interference, not merely re-run");
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
      // The bounds are only meaningful if the model can actually get NEAR
      // them; these two covers are what stops a future edit from making the
      // asserts pass by starving the environment instead of the arbiter.
      c_near_rr:   cover (ctrl_init_done && waiting[1] && waited[1] > 7'd40);
      c_near_sc:   cover (ctrl_init_done && waiting[0] && waited[0] > 7'd25);
    end
  end

endmodule
