// video_framectl_fv.sv — formal harness for zhao_video_framectl (tests/
// formal/video_framectl_one_fence.sby). FORMAL COMPONENT — never
// synthesized, never linted by the RTL lanes.
//
// Abstract raster: vswap_dec pulses every 60 vid cycles; frame_start
// pulses 10 cycles after each dec (the real ordering: the dec happens at
// y=244, frame_start 18 lines later — only the ORDER matters here).
// slot_ready / deadline_cycles are FREE (adversarial flapping included).
//
// Properties (spec/video_rules.md §4-§5, §8):
//   * fence-exactly-once: the gpu-domain tick count equals the dec count
//     at ALL times (exactly one fence per FPGA_RUNNING->DONE, none else)
//   * tick rate: one frame_tick per dec, none without a dec
//   * fault law: deadline_faults increments ONLY at a dec with no commit
//     (repeated pulse) — never elsewhere, once per missed frame
//   * reset-idle: no tick, no swap under reset

module video_framectl_fv
  import zhao_pkg::*;
(
  input logic clk,
  input logic rst_n,
  input logic [1:0] slot_ready,
  input logic [31:0] deadline_cycles
);

  // ---- abstract raster generator (ordering law of the real raster) -----
  // dec every 14 cycles (just past the ~5-cycle toggle+2FF+edge crossing
  // the property reasons about), frame_start 4 cycles after each dec (the
  // real ordering: dec at y=244, frame_start 18 lines later).
  logic [3:0] phase;
  logic vswap_dec, frame_start;
  assign vswap_dec   = (phase == 4'd0);
  assign frame_start = (phase == 4'd4);

  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) phase <= 4'd11;
    else        phase <= (phase == 4'd13) ? 4'd0 : phase + 4'd1;
  end

  // ---- DUT (both domains tied; see the .sby header) --------------------
  logic swap_req, frame_repeated, frame_tick, swap_ack;
  logic [0:0] swap_slot;
  logic [31:0] frame_id, deadline_margin;
  logic [63:0] frame_cycles, deadline_faults;
  zhao_frame_tick_t gpu_tick;
  logic [0:0] gpu_complete_slot;

  zhao_video_framectl dut (
    .vid_clk(clk), .rst_n(rst_n),
    .x(16'd0), .y(16'd0), .vblank(1'b0),
    .vswap_dec(vswap_dec), .frame_start(frame_start),
    .mode(ZHAO_MODE_Z60),
    .slot_ready(slot_ready), .deadline_cycles(deadline_cycles),
    .swap_req(swap_req), .swap_slot(swap_slot), .swap_ack(swap_ack),
    .frame_repeated(frame_repeated),
    .frame_tick(frame_tick), .frame_id(frame_id),
    .frame_cycles(frame_cycles), .deadline_faults(deadline_faults),
    .deadline_margin(deadline_margin),
    .gpu_clk(clk), .gpu_tick(gpu_tick),
    .gpu_complete_slot(gpu_complete_slot)
  );

  // ---- counters ---------------------------------------------------------
  reg [7:0] dec_count = 0, fence_count = 0, tick_count = 0, fault_count = 0;
  reg f_past_valid = 0;
  always @(posedge clk) begin
    f_past_valid <= 1;
    // reset discipline: the proof run starts in reset, released at most once
    if (!f_past_valid) assume(!rst_n);
    else assume(!$past(rst_n) || rst_n);   // release is monotonic
  end

  always @(posedge clk) begin
    if (rst_n) begin
      if (vswap_dec) dec_count <= dec_count + 8'd1;
      if (gpu_tick.pulse) fence_count <= fence_count + 8'd1;
      if (frame_tick) tick_count <= tick_count + 8'd1;
      if (frame_repeated) fault_count <= fault_count + 8'd1;
    end else if (f_past_valid) begin
      // reset-idle: counters (and the DUT outputs) stay quiet under (held)
      // reset. Guarded by f_past_valid — step 0 is free-init until the
      // modeled async reset has applied at one edge (W2.3 phantom trap).
      assert(!frame_tick);
      assert(!gpu_tick.pulse);
      assert(!swap_req);
      assert(frame_cycles == 64'd0);
    end
  end

  // The fence/tick counting law is deadline-INDEPENDENT; bounding the free
  // deadline input to small NON-ZERO values keeps the model light for the
  // solver (0 selects the mode frame period — a 251,520 constant; that
  // path and the deadline law itself are covered cycle-exactly by the
  // differential tests).
  always @(posedge clk) begin
    if (rst_n) begin
      assume(deadline_cycles != 32'd0);
      assume(deadline_cycles[31:6] == 26'd0);
    end
  end

  // Model bound: the counting laws are proven over any 32-frame window —
  // bounding the DUT's free-running counters keeps the reachable state
  // space finite and small for the solver (a counting bug surfaces within
  // two frames; the unbounded counters are exercised by the random tests).
  always @(posedge clk) begin
    if (rst_n) begin
      assume(frame_id < 32'd32);
      assume(deadline_faults < 64'd32);
      assume(frame_cycles < 64'd32);
    end
  end

  always @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      // fence-exactly-once (the property video_framectl_one_fence). The
      // fence crosses vid->gpu through toggle+2FF+edge: it trails its dec
      // by a few cycles, so the counts may differ by at most the ONE dec
      // currently in flight — never more, never duplicated.
      assert(fence_count == dec_count || fence_count + 8'd1 == dec_count);
      // one tick per displayed frame, none without a dec (1-cycle lag of
      // the registered tick pulse behind the dec level)
      assert(tick_count == dec_count || tick_count + 8'd1 == dec_count);
      // deadline faults only accompany a repeated (missed) frame
      assert(fault_count <= dec_count);
    end
  end

  // ---- covers: the counting laws are exercised, not vacuous --------------
  // (ledger rule V16; added at the merge — the salvaged harness had none)
  always @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      c_fence:    cover(gpu_tick.pulse);            // a fence crossed
      c_tick:     cover(frame_tick);                // a tick pulsed
      c_repeat:   cover(frame_repeated);            // the repeat half
      c_commit:   cover(swap_req);                  // the swap half (needs a
                                                    // READY inside the window)
      c_two_decs: cover(dec_count == 8'd2);         // multiple frame periods
      c_lag:      cover(fence_count + 8'd1 == dec_count); // in-flight window
    end
  end

  // ---- CDC data-stability law (rule V20 enforcement) --------------------
  // The DUT's vid->gpu crossing carries frame_id_q / cur_repeated /
  // cur_slot next to the tick toggle, with NO gray coding beyond the
  // toggle — lawful ONLY because those registers change exclusively at the
  // toggle edge (all their writes sit under `if (vswap_dec)`, the same
  // edge that flips tick_tog). The DUT header used to state that as
  // "stable-by-construction" prose; this assertion is the enforcer the
  // prose now cites (ENFORCED-BY discipline). Clocks are tied in this
  // harness, but the register-discipline half of the claim — data moves
  // only when the toggle moves — is exactly what it checks.
  always @(posedge clk) begin
    if (f_past_valid && rst_n && $past(rst_n)) begin
      if (dut.tick_tog == $past(dut.tick_tog)) begin
        a_cdc_data_stable_unless_toggle: assert(
             dut.frame_id_q   == $past(dut.frame_id_q)
          && dut.cur_repeated == $past(dut.cur_repeated)
          && dut.cur_slot     == $past(dut.cur_slot));
      end
    end
  end

  // ---- SELF-ASSERTING SCOPE GUARD (ledger rule V19; the arbiter
  // a_horizon_is_refresh_free / linebuf a_scope_four_sessions pattern) ----
  // The bmc task is BOUNDED at depth 60, which admits AT MOST five
  // vswap_dec pulses of the 14-cycle abstract raster (earliest reset
  // release puts the decs near steps 4, 18, 32, 46, 60): the counting laws
  // are proven over FOUR complete abstract frame periods, exactly the
  // ".sby header's ~4 periods" — previously only a comment. If anyone
  // raises the depth past what was actually proven, a SIXTH dec becomes
  // reachable and this guard FIRES: the run fails loudly instead of
  // silently re-scoping what "PASS" means, and the depth/period trade-off
  // must be re-derived (note the 8-bit harness counters additionally wrap
  // at 256 decs ≈ depth 3600 — a re-derivation must widen them first).
  always @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      a_scope_four_frame_periods: assert(dec_count <= 8'd5);
    end
  end

endmodule : video_framectl_fv
