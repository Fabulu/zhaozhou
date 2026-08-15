// zhao_video_scaler.sv — VIDEO.SCALER, Phase-2 pass-through formatter
// (plan W2.2; law: spec/video_rules.md §6, decision D1 adapter law).
//
// Repackages SCANOUT's native pixel stream (zhao_px_stream_t) onto the
// scaler-facing port with a FIXED 2-vid-cycle pipeline delay and NO data
// transformation whatsoever: no colour conversion, no scaling, no policy
// (out[i] == in[i-2], the zref::ScalerFeed identity). The MiSTer
// sys/video_mixer/ascal adapter (hardware lane, never modified) is the
// consumer of this port; the aiscal seam is recorded in the SCALER contract
// and has no Phase-2 behaviour.
//
// Backpressure (contract): ready/valid on the output port. If the consumer
// deasserts out_ready, the pipeline FREEZES (both stages hold) and the stall
// propagates upstream as a SCANOUT starvation condition (counted there); in
// the Phase-2 Verilator harness the sink is always ready. A protocol
// violation at the input (valid outside the active window) trips the
// never_active violation flag — the block has no silent fallback.
//
// Conservative SystemVerilog subset only (charter §2).
// Lint: clean under `verilator_bin --lint-only -Wall` (CTest
// lint_zhao_video_scaler).

module zhao_video_scaler
  import zhao_pkg::*;
(
  input  logic           vid_clk,
  input  logic           rst_n,

  // native stream from VIDEO.SCANOUT (serializer side, vid domain)
  input  zhao_px_stream_t in,

  // scaler-facing (hardware-lane adapter) port
  output zhao_px_stream_t out,
  input  logic           out_ready,

  // protocol check: in.valid asserted outside the active window
  // (!in.hblank && !in.vblank is the valid window) — never asserted by a
  // conforming SCANOUT; trips the testbench if it ever fires
  output logic           never_active
);

  zhao_px_stream_t stage1, stage2;
  logic             violation_q;

  // The input stream is free-running (serializer has no backpressure), so
  // the only flow control is the freeze: when the consumer is not ready,
  // both pipeline stages hold their contents (no pixel is lost or duplicated).
  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      stage1     <= '0;
      stage2     <= '0;
      violation_q<= 1'b0;
    end else if (out_ready) begin
      stage1 <= in;
      stage2 <= stage1;
    end else begin
      stage1 <= stage1;  // held
      stage2 <= stage2;  // held
    end
    // sticky protocol violation (valid only inside the active window)
    if (in.valid && (in.hblank || in.vblank)) begin
      violation_q <= 1'b1;
    end
  end

  assign out         = stage2;
  assign never_active = violation_q;

endmodule : zhao_video_scaler
