// zhao_debug_crc.sv — DEBUG.CRC: the displayed-frame CRC-32C lane, in the
// VIDEO clock domain (plan W2.6; moved out of gpu_clk 2026-08-22).
//
// Law (in citation order):
//   design/contracts/DEBUG.CRC.md — the block contract. "`vid_clk` domain for
//       the displayed-stream lane (the CRC follows the serializer)". The
//       wave-2 scope is the DISPLAYED-stream frame CRC that mechanically
//       enforces the 60 Hz law — a repeated frame must CRC identical,
//       spec/video_rules.md 4.
//   spec/capture_format.md 2/2.2 — CRC-32C parameter set (poly 0x82F63B78
//       reflected, init/xorout 0xFFFFFFFF).
//   spec/video_rules.md 3 — RGB565 LITTLE-ENDIAN halfwords: the low byte of
//       a pixel precedes its high byte in the displayed stream.
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK TAKES PIXELS AND NOT BYTES, which is the whole point of the
// 2026-08-22 change.
//
// The displayed stream is produced by VIDEO.SCANOUT's serializer at ONE PIXEL
// PER vid_clk. The previous arrangement ran this CRC on gpu_clk, so the shell
// had to re-time that stream across the clock boundary: it sampled the vid
// pixel register (valid, rgb565, x, y) from gpu logic on a phase toggle and
// unpacked it into two gpu-cycle bytes. That is PER-PIXEL STATE CROSSING A
// CLOCK DOMAIN — sixteen data bits plus position and validity, every active
// pixel, correct only because the simulation freezes vid_clk = gpu_clk/2 with
// coincident posedges. It is the seam that produced the two hold violations
// measured on `vid_clk -> gpu_clk` under the HIGH PERFORMANCE fitter effort,
// and a hold violation is not a speed problem: no clock is slow enough to fix
// data that arrives too early.
//
// With the CRC in vid_clk the crossing disappears rather than being made
// safer. Nothing per-pixel leaves the video domain at all; the shell crosses
// only the FINALISED 32-bit CRC once per frame, on a toggle, with the value
// held stable for the whole frame that follows.
//
// The cost of the move is that a byte-serial CRC cannot keep up: one pixel per
// clock is TWO bytes per clock. So this lane folds two bytes in ONE shallow
// XOR tree via `zhao_crc32c_fold` (about seven levels) instead of two chained
// eight-level `zhao_crc32c_step` calls (sixteen). That is the same generated
// polynomial machine either way — zhao_crc32c_fold derives its columns from
// the CRC-32C definition at elaboration and is held to the SHIPPED
// `zhao_crc32c_step` by tests/differential/crc32c_fold_directed.cpp — so one
// polynomial machine repo-wide (charter 19/29-6, plan A3d) still holds.
// ---------------------------------------------------------------------------
//
// Stream law: one displayed PIXEL arrives per valid cycle (active_width per
// line, border rows included in Duo); in_sof_i marks the first pixel of the
// displayed frame (restarts the CRC, latches expect_bytes_i), in_eof_i the
// last pixel (finalizes). The finalized CRC registers one cycle after the eof
// pixel (contract bound: variable_bounded:4 — this lane uses 1). A mis-sized
// stream (bytes != displayed bytes for the mode) is a raster-side protocol
// violation: size_err_evt_o pulses and the CRC register is NOT published
// (frame_crc_valid_o stays low) — the CRC never "adapts". Because the stream
// is pixel-granular the captured length is always EVEN, so an ODD
// expect_bytes_i can never be satisfied and every such frame is mis-sized;
// that is a statement about the expectation, not about the raster.
//
// The displayed stream cannot stall (free-running raster): the input side
// has no backpressure by law; the output register has a single consumer
// that is always ready in Phase 2.
//
// Conservative SystemVerilog subset only (charter 2).
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_debug_crc).

module zhao_debug_crc (
  input  logic clk,                    // vid_clk (contract: video domain)
  input  logic rst_n,

  // the displayed pixel stream from VIDEO.SCANOUT (post-scaler, vid domain)
  input  logic        in_valid_i,
  input  logic [15:0] in_px_i,         // RGB565; [7:0] is the FIRST byte (LE)
  input  logic        in_sof_i,        // with the FIRST pixel of the frame
  input  logic        in_eof_i,        // with the LAST pixel of the frame
  input  logic [31:0] expect_bytes_i,  // displayed bytes for the mode (at sof)

  // the finalized frame CRC (contract output)
  output logic [31:0] frame_crc_o,
  output logic        frame_crc_valid_o, // one-cycle pulse after the eof pixel
  output logic [31:0] bytes_captured_o,  // the length the LAST event reported
  output logic        size_err_evt_o     // mis-sized stream (asserted in sim)
);

  // ------------------------------------------------------------ state -----
  /* verilator lint_off PROCASSINIT */
  logic [31:0] crc_r = 32'hFFFF_FFFF;   // init-seeded register (no xorout)
  logic        running = 1'b0;
  logic [31:0] n_bytes = 32'd0;
  logic [31:0] expect_n = 32'd0;
  logic        fin_v = 1'b0;
  logic [31:0] fin_crc = 32'd0;
  logic [31:0] fin_bytes = 32'd0;
  logic        err_v = 1'b0;
  /* verilator lint_on PROCASSINIT */

  // ------------------------------------------------------- combinational --
  // ONE fold instance, seeded at sof and chained otherwise. `n_i` is tied to
  // two, so synthesis keeps only the two-byte matrix; the other eight
  // constant-fold away.
  //
  // d_i's LOW byte is folded FIRST (fold header), and in_px_i[7:0] is the low
  // byte of the RGB565 halfword, which video_rules.md 3 puts first on the
  // wire. So {48'd0, in_px_i} is exactly the two displayed bytes in stream
  // order — no swap, and a swap here would be caught by the byte-stream
  // differential in tests/debug/debug_crc_directed.cpp.
  logic [31:0] fold_c, fold_o;
  assign fold_c = in_sof_i ? 32'hFFFF_FFFF : crc_r;

  zhao_crc32c_fold u_fold (
    .c_i (fold_c),
    .d_i ({48'd0, in_px_i}),
    .n_i (4'd2),
    .c_o (fold_o)
  );

  // The length and expectation AFTER this pixel. A sof restarts both, even
  // mid-frame (the raster re-framing wins over a frame already open — the
  // same precedence the byte-serial lane had).
  logic [31:0] n_next, exp_now;
  assign n_next  = in_sof_i ? 32'd2 : (n_bytes + 32'd2);
  assign exp_now = in_sof_i ? expect_bytes_i : expect_n;

  // ------------------------------------------------------ sequential ------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      crc_r <= 32'hFFFF_FFFF;
      running <= 1'b0;
      n_bytes <= 32'd0;
      expect_n <= 32'd0;
      fin_v <= 1'b0;
      fin_crc <= 32'd0;
      fin_bytes <= 32'd0;
      err_v <= 1'b0;
    end else begin
      fin_v <= 1'b0;
      err_v <= 1'b0;
      if (in_valid_i) begin
        if (in_sof_i || running) begin
          crc_r <= fold_o;
          running <= 1'b1;
          n_bytes <= n_next;
          expect_n <= exp_now;
          if (in_eof_i) begin
            // finalize: xorout + the mis-sized-stream gate. A single-pixel
            // frame (sof && eof on the same pixel) takes this path unchanged,
            // publishing iff the expectation is exactly two bytes.
            fin_bytes <= n_next;
            if (n_next == exp_now) begin
              fin_crc <= ~fold_o;
              fin_v <= 1'b1;
            end else begin
              err_v <= 1'b1;  // protocol violation: CRC not published
            end
            running <= 1'b0;
            n_bytes <= 32'd0;
          end
        end else begin
          // pixel outside any frame (no sof since eof/idle): raster violation
          err_v <= 1'b1;
          fin_bytes <= 32'd0;
        end
      end
    end
  end

  assign frame_crc_o = fin_crc;
  assign frame_crc_valid_o = fin_v;
  assign bytes_captured_o = fin_bytes;
  assign size_err_evt_o = err_v;

endmodule : zhao_debug_crc
