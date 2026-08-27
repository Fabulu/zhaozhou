// zhao_probe_ctx_fifo.sv — Field v3 decisive probe 1 (reports/Fieldv3.md
// Phase 3): the ready-context FIFO scheduler with REGISTERED plan fetch.
//
// WHAT IT KILLS: v2's measured scheduler critical path was a combinational
// ready-scoreboard scan -> round-robin select -> indexed state read -> state
// feedback, one of the three ~16-18 ns paths that form the pipeline-depth
// plateau. This probe replaces the scan with explicit ownership:
//
//     a context becomes ready -> its ID is ENQUEUED;
//     issue POPS one ID;
//     short-op completion RE-ENQUEUES it;
//     service completion RE-ENQUEUES it;
//     END releases it.
//
// A context ID is always in exactly one place: FREE, the ready queue, the
// issue pipeline, waiting on the service, or retiring. That invariant is a
// Fieldv3 formal property; here it is carried as a per-context one-hot state
// the directed test audits every cycle through inflight_o.
//
// THE PIPELINE IS INTENTIONALLY STAGED (the brief's five steps, three of
// which live here — the register file belongs to the RF probe):
//   S0  dequeue and REGISTER the context ID;
//   S1  REGISTERED plan fetch (a true synchronous RAM read, M10K-style:
//       address on one edge, word on the next — no combinational plan mux);
//   S2  dispatch: short op completes and re-enqueues; long op enters the
//       service; the LAST op releases the context.
// The modeled service is a countdown bank — the probe measures the
// SCHEDULER's timing and ordering, not any arithmetic.
//
// TARGETS: issue sustains 1 instruction/clock with >= 2 ready contexts
// (measured by the directed test), and the fitted cone is timing-clean at
// the gpu constraint — the select-then-index-then-feedback loop is gone BY
// CONSTRUCTION, and the fit either confirms that or kills the topology.
// ENFORCED-BY: tests/differential/field_ctx_fifo_directed.cpp:main
module zhao_probe_ctx_fifo #(
    parameter int CTX = 8,
    parameter int PLAN_WORDS = 256  // CTX x 32 plan slots
) (
    input logic clk,
    input logic rst_n,

    // ---- host: load plan words, then start contexts ------------------------
    // plan word: {is_long[1], latency[4:0]} at {ctx, pc}
    input  logic                          plan_we_i,
    input  logic [$clog2(PLAN_WORDS)-1:0] plan_waddr_i,
    input  logic [5:0]                    plan_wdata_i,

    input  logic                   start_i,
    input  logic [$clog2(CTX)-1:0] start_ctx_i,
    input  logic [4:0]             start_len_i,  // instructions before END, 1..31
    output logic                   start_ready_o,

    // ---- observability -----------------------------------------------------
    output logic                   issue_valid_o,  // an op dispatched (S2)
    output logic [$clog2(CTX)-1:0] issue_ctx_o,
    output logic [4:0]             issue_pc_o,
    output logic                   issue_long_o,
    output logic                   done_valid_o,  // a context released
    output logic [$clog2(CTX)-1:0] done_ctx_o,
    output logic [CTX-1:0]         busy_o,     // context is not FREE
    output logic [CTX-1:0]         inflight_o  // context is in S0..S2 or service
);

  localparam int CW = $clog2(CTX);

  // ---- per-context architectural state ------------------------------------
  logic [CTX-1:0]      c_busy;      // allocated (started, not yet released)
  logic [CTX-1:0]      c_inflight;  // owned by pipeline or service
  logic [CTX-1:0][4:0] c_pc;
  logic [CTX-1:0][4:0] c_len;

  assign busy_o     = c_busy;
  assign inflight_o = c_inflight;

  // ---- ready FIFO (CTX deep — every context at most once) -----------------
  logic [CTX-1:0][CW-1:0] rq;
  logic [CW:0]            rq_wr, rq_rd;  // one extra bit for full/empty
  logic rq_empty;
  assign rq_empty = (rq_wr == rq_rd);

  // THREE enqueue sources exist (short-op requeue, service completion, host
  // start), and the first draft gave them ONE write port with the short path
  // preferred. The directed test caught the consequence before any fit did:
  // eight circulating all-short contexts requeue EVERY cycle, so service
  // completions and host starts starved forever and the machine wedged.
  // The FIFO therefore has TWO write ports: the S1 requeue owns port 1;
  // port 2 serves the service completion first and a host start otherwise.
  // The service can no longer starve (its port is always available), and a
  // start waits only for service-idle cycles.
  logic          enq_valid;   // port 1: S1 short-op requeue
  logic [CW-1:0] enq_ctx;
  logic          enq2_valid;  // port 2: service completion, else host start
  logic [CW-1:0] enq2_ctx;
  logic [CW:0]   rq_count;

  // ---- S0: dequeue and register the context ID ----------------------------
  logic          s0_valid;
  logic [CW-1:0] s0_ctx;

  // ---- S1: registered plan fetch ------------------------------------------
  // True synchronous RAM: plan_q answers one edge after plan_addr.
  logic [5:0] plan_mem[PLAN_WORDS];
  logic [5:0] plan_q;
  logic       s1_valid;
  logic [CW-1:0] s1_ctx;
  logic [4:0]    s1_pc;
  logic          s1_last;

  logic [$clog2(PLAN_WORDS)-1:0] plan_addr;
  assign plan_addr = {s0_ctx, c_pc[s0_ctx]};

  always_ff @(posedge clk) begin
    if (plan_we_i) plan_mem[plan_waddr_i] <= plan_wdata_i;
    plan_q <= plan_mem[plan_addr];
  end

  // ---- modeled long-op service: per-context countdown ---------------------
  logic [CTX-1:0]      svc_busy;
  logic [CTX-1:0][4:0] svc_cnt;

  // exactly one service completion per cycle (single re-enqueue port):
  // priority-pick the lowest finished context.
  logic          svc_done_valid;
  logic [CW-1:0] svc_done_ctx;
  always_comb begin
    svc_done_valid = 1'b0;
    svc_done_ctx   = '0;
    for (int c = CTX - 1; c >= 0; c--) begin
      if (svc_busy[c] && svc_cnt[c] == 5'd0) begin
        svc_done_valid = 1'b1;
        svc_done_ctx   = CW'(c);
      end
    end
  end

  // ---- S2: dispatch --------------------------------------------------------
  logic is_long;
  assign is_long = plan_q[5];

  assign issue_valid_o = s1_valid;
  assign issue_ctx_o   = s1_ctx;
  assign issue_pc_o    = s1_pc;
  assign issue_long_o  = s1_valid && is_long && !s1_last;

  // a short op (or any last op) re-enqueues / releases THIS cycle
  logic short_requeue;
  assign short_requeue = s1_valid && !s1_last && !is_long;

  assign done_valid_o = s1_valid && s1_last;
  assign done_ctx_o   = s1_ctx;

  assign enq_valid = short_requeue;
  assign enq_ctx   = s1_ctx;

  logic start_fire;
  assign enq2_valid = svc_done_valid || start_fire;
  assign enq2_ctx   = svc_done_valid ? svc_done_ctx : start_ctx_i;

  logic svc_take_done;
  assign svc_take_done = svc_done_valid;

  assign rq_count = rq_wr - rq_rd;

  // a host start needs port 2 idle of service work, room for both possible
  // writes this cycle, and a FREE context
  assign start_ready_o = !svc_done_valid && (rq_count <= (CW + 1)'(CTX - 2)) &&
                         !c_busy[start_ctx_i];
  assign start_fire = start_i && start_ready_o;

  // S2 never stalls: short ops retire, long ops enter the service (which
  // always has room because a context has at most ONE op in flight), and the
  // LAST op releases the context outright — the probe treats the final plan
  // word as END regardless of its class, since END is its own op in the real
  // machine and a scheduler probe has nothing to execute.

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      c_busy     <= '0;
      c_inflight <= '0;
      c_pc       <= '0;
      c_len      <= '0;
      rq_wr      <= '0;
      rq_rd      <= '0;
      rq         <= '0;
      s0_valid   <= 1'b0;
      s0_ctx     <= '0;
      s1_valid   <= 1'b0;
      s1_ctx     <= '0;
      s1_pc      <= '0;
      s1_last    <= 1'b0;
      svc_busy   <= '0;
      svc_cnt    <= '0;
    end else begin
      // ---- service countdowns
      for (int c = 0; c < CTX; c++) begin
        if (svc_busy[c] && svc_cnt[c] != 5'd0) svc_cnt[c] <= svc_cnt[c] - 5'd1;
      end
      if (svc_take_done) svc_busy[svc_done_ctx] <= 1'b0;

      // ---- enqueue: two write ports
      unique case ({enq_valid, enq2_valid})
        2'b11: begin
          rq[rq_wr[CW-1:0]] <= enq_ctx;
          rq[(rq_wr[CW-1:0] + 1'b1) & CW'(CTX - 1)] <= enq2_ctx;
          rq_wr <= rq_wr + (CW + 1)'(2);
        end
        2'b10: begin
          rq[rq_wr[CW-1:0]] <= enq_ctx;
          rq_wr <= rq_wr + 1'b1;
        end
        2'b01: begin
          rq[rq_wr[CW-1:0]] <= enq2_ctx;
          rq_wr <= rq_wr + 1'b1;
        end
        default: ;
      endcase
      if (start_fire) begin
        c_busy[start_ctx_i]     <= 1'b1;
        c_inflight[start_ctx_i] <= 1'b0;
        c_pc[start_ctx_i]       <= 5'd0;
        c_len[start_ctx_i]      <= start_len_i;
      end

      // ---- S0 dequeue
      if (!rq_empty) begin
        s0_valid <= 1'b1;
        s0_ctx   <= rq[rq_rd[CW-1:0]];
        rq_rd    <= rq_rd + 1'b1;
        c_inflight[rq[rq_rd[CW-1:0]]] <= 1'b1;
      end else begin
        s0_valid <= 1'b0;
      end

      // ---- S1 plan fetch result registers travel with the fetched word
      s1_valid <= s0_valid;
      s1_ctx   <= s0_ctx;
      s1_pc    <= c_pc[s0_ctx];
      s1_last  <= s0_valid && (c_pc[s0_ctx] == c_len[s0_ctx] - 5'd1);
      if (s0_valid) c_pc[s0_ctx] <= c_pc[s0_ctx] + 5'd1;

      // ---- S2 dispatch
      if (s1_valid) begin
        if (s1_last) begin
          c_busy[s1_ctx]     <= 1'b0;  // END releases
          c_inflight[s1_ctx] <= 1'b0;
        end else if (is_long) begin
          svc_busy[s1_ctx] <= 1'b1;
          svc_cnt[s1_ctx]  <= plan_q[4:0];
          // stays inflight (waiting on the service)
        end else begin
          c_inflight[s1_ctx] <= 1'b0;  // back to the ready queue
        end
      end
      if (svc_take_done) c_inflight[svc_done_ctx] <= 1'b0;
      if (enq_valid) c_inflight[enq_ctx] <= 1'b0;
      if (enq2_valid) c_inflight[enq2_ctx] <= 1'b0;
    end
  end

endmodule : zhao_probe_ctx_fifo
