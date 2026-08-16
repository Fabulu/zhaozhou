// audio_fifo_bounds_harness.sv — formal harness for the AUDIO.FIFO bounds
// (plan W2.4 / plan §4; design/contracts/AUDIO.FIFO.md "Formal properties";
// law spec/audio_rules.md §2).
//
// Properties (all immediate assertions, one per D4 clause):
//   P1 occupancy ∈ [0, DEPTH] — the gpu-domain occupancy view is a
//      CONSERVATIVE OVERESTIMATE of the true occupancy (the synced read
//      pointer only ever lags the real one), so bounding the view bounds
//      the truth: no overflow is possible.
//   P2 no accepted write when full — wr_ready is low exactly at full and
//      nothing is accepted then (the backpressure law; overflow is
//      structurally impossible).
//   P3 the underrun law — an underrun tick re-emits the PREVIOUS pair
//      bit-exactly (stream continuous, never a torn pair: pairs cross as
//      32-bit units), and audio_underruns increments EXACTLY ONCE per
//      continuous event (start tick +1, hold ticks +0, non-event ticks +0).
//
// Both clock inputs are driven from ONE clock: with coincident edges the
// 2-flop gray synchronisers simply shorten, and the pointer/occupancy laws
// are timing-independent (any rational ratio; plan R1). DEPTH is shrunk to
// 8 to keep the SAT problem tractable — the geometry law itself (2048 /
// 512 / 256) is D4-frozen and asserted by the directed/random ctests at
// full size.

module zhao_audio_fifo_bounds (
    input logic        clk,
    input logic        wr_valid,
    input logic [15:0] wr_l,
    input logic [15:0] wr_r,
    input logic        frame_tick
);

  // qualified names only (no `import` — the yosys SV frontend used by the
  // formal lane rejects import statements; see the DUT header note)
  localparam int unsigned DEPTH = 8;

  // reset sequencer: rst_n is LOW in the initial state, HIGH from the first
  // edge on — an unconstrained free rst_n lets the solver skip reset and
  // manufacture x-initialised "underruns" that no real run can reach
  logic rst_n = 1'b0;
  always_ff @(posedge clk) rst_n <= 1'b1;

  // audio_clk = gpu_clk/4 — the SAME sim seam ratio the ctests drive
  // (spec-defined, plan R1). A shared single clock would balance writes
  // 1:1 with pops and the FIFO could never FILL, silently masking the
  // backpressure property; with the /4 divider occupancy genuinely
  // reaches DEPTH and P2 has teeth (verified by mutation: forcing
  // wr_ready=1 FAILS the proof).
  logic [1:0] div_cnt = 2'd0;
  always_ff @(posedge clk) div_cnt <= div_cnt + 2'd1;
  logic clk_audio;
  assign clk_audio = clk & (div_cnt == 2'd3);

  logic        wr_ready, refill_req, pcm_valid, underrun_status;
  logic [3:0]  occupancy;  // $clog2(8)+1 bits
  logic [15:0] pcm_l, pcm_r;
  logic [31:0] underruns;
  zhao_pkg::zhao_counter_snap_t snap;

  zhao_audio_fifo #(
      .DEPTH(DEPTH),
      .WATERMARK(2)
  ) dut (
      .clk_gpu(clk),
      .rst_gpu_n(rst_n),
      .wr_valid_i(wr_valid),
      .wr_l_i(wr_l),
      .wr_r_i(wr_r),
      .wr_ready_o(wr_ready),
      .refill_req_o(refill_req),
      .occupancy_o(occupancy),
      .frame_tick_i(frame_tick),
      .cnt_snap_o(snap),
      .clk_audio(clk_audio),
      .rst_audio_n(rst_n),
      .pcm_valid_o(pcm_valid),
      .pcm_l_o(pcm_l),
      .pcm_r_o(pcm_r),
      .underrun_status_o(underrun_status),
      .audio_underruns_o(underruns)
  );

  // one-cycle-delayed output view (the "previous tick" for the repeat law)
  logic        p_valid;
  logic [15:0] p_l, p_r;
  logic        p_underrun;
  logic [31:0] p_underruns;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      p_valid      <= 1'b0;
      p_l          <= 16'd0;
      p_r          <= 16'd0;
      p_underrun   <= 1'b0;
      p_underruns  <= 32'd0;
    end else begin
      p_valid      <= pcm_valid;
      p_l          <= pcm_l;
      p_r          <= pcm_r;
      p_underrun   <= underrun_status;
      p_underruns  <= underruns;
    end
  end

  // assertions live in a sync-edge block (async-reset-triggered $check
  // cells are unsupported by the formal prep flow)
  always_ff @(posedge clk) begin
    if (rst_n) begin
      // P1: occupancy bounds (conservative view bounds the true occupancy)
      a_occupancy_bounds : assert (occupancy <= DEPTH);

      // P2: no accept when full (backpressure law)
      if (occupancy == DEPTH) begin
        a_no_ready_when_full : assert (!wr_ready);
      end

      // P3: the underrun law
      if (underrun_status) begin
        a_repeat_bitexact : assert (pcm_valid && pcm_l == p_l && pcm_r == p_r);
        if (p_underrun) begin
          a_count_holds_in_event : assert (underruns == p_underruns);
        end else begin
          a_count_once_per_event : assert (underruns == p_underruns + 32'd1);
        end
      end else begin
        a_count_only_on_event : assert (underruns == p_underruns);
      end
    end
  end

  // ---- SELF-ASSERTING SCOPE GUARD (ledger rule V19; the arbiter
  // a_horizon_is_refresh_free / linebuf a_scope_four_sessions pattern) ----
  // MEASURED, not assumed: this .sby runs `multiclock on`, where one clk
  // POSEDGE costs ~2 SMT steps (clk is solver-driven; a rise needs a 0->1
  // step pair). So the depth-32 window admits AT MOST ~16 clk posedges =
  // FOUR audio-domain pop opportunities (div_cnt == 3) — HALF a drain of
  // the shrunk DEPTH-8 geometry, and half of what a naive steps-as-cycles
  // reading of "depth 32" suggests. (Discovered by this guard's own
  // negative probe, 2026-08-16: a <= 8 tick guard would not fire even at
  // depth 48.) The FIFO can still FILL inside the window — the wr_ready=1
  // mutation note above produced a real full-state CEX — but the guard
  // below pins the window at the number that was actually proven. If
  // anyone raises `depth`, a FIFTH pop opportunity becomes reachable and
  // this FIRES: re-derive the DEPTH/ratio shrink and the multiclock step
  // budget together, do not merely re-run.
  logic [4:0] f_aticks = 5'd0;
  always_ff @(posedge clk) begin
    if ((div_cnt == 2'd3) && (f_aticks != 5'h1F)) f_aticks <= f_aticks + 5'd1;
  end
  always_comb begin
    a_scope_half_drain_window : assert (f_aticks <= 5'd4);
  end

endmodule : zhao_audio_fifo_bounds
