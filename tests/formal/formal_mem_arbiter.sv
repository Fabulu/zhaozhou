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

// (Currently SKIPped under oss-cad-suite yosys — SV-frontend gap vs the
// frozen package style; see mem_formal_lane.cmake.in.)

module formal_mem_arbiter
  import zhao_pkg::*;
(
  input logic clk,
  input logic rst_n
);

  // ---- free environment -----------------------------------------------------
  (* anyseq *) logic       env_valid [0:1];
  (* anyseq *) logic       env_write [0:1];
  (* anyseq *) logic [6:0] env_len   [0:1];
  (* anyseq *) logic [26:0] env_addr [0:1];
  (* anyseq *) logic       env_rearm [0:1];
  (* anyconst *) logic [15:0] env_wdata;

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
    .init_done (), .refresh_stalls (), .bank_conflicts (), .refresh_pulse ()
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

  always_comb begin
    for (int k = 0; k < 2; k++) begin
      if (waiting[k]) begin
        assert (waited[k] < 7'd40)
          else $error("liveness: client %0d unserved for %0d cycles (bound B=40)",
                      k, waited[k]);
      end
    end
  end

endmodule
