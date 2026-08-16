// zhao_debug_crc.sv — DEBUG.CRC: the displayed-frame CRC-32C lane
// (plan W2.6).
//
// Law (in citation order):
//   design/contracts/DEBUG.CRC.md — the block contract (wave-2 scope: the
//       DISPLAYED-stream frame CRC that mechanically enforces the 60 Hz
//       law — a repeated frame must CRC identical, spec/video_rules.md 4).
//   spec/capture_format.md 2/2.2 — CRC-32C parameter set (poly 0x82F63B78
//       reflected, init/xorout 0xFFFFFFFF) and the per-byte SV step. The
//       step function is the GENERATED zhao_crc32c_step — never a hand
//       copy (one polynomial machine-wide, plan A3d).
//
// Stream law: bytes arrive one per valid cycle (2 x active_width per line,
// border rows included in Duo); in_sof_i marks the first byte of the
// displayed frame (restarts the CRC, latches expect_bytes_i), in_eof_i the
// last byte (finalizes). The finalized CRC registers one cycle after the
// eof byte (contract bound: variable_bounded:4 — this lane uses 1). A
// mis-sized stream (bytes != canvas bytes for the mode) is a raster-side
// protocol violation: size_err_evt_o pulses and the CRC register is NOT
// published (frame_crc_valid_o stays low) — the CRC never "adapts".
//
// The displayed stream cannot stall (free-running raster): the input side
// has no backpressure by law; the output register has a single consumer
// that is always ready in Phase 2.
//
// Conservative SystemVerilog subset only (charter 2).
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_debug_crc).

module zhao_debug_crc (
  input  logic clk,
  input  logic rst_n,

  // the displayed byte stream from VIDEO.SCANOUT's serializer
  input  logic        in_valid_i,
  input  logic [7:0]  in_byte_i,
  input  logic        in_sof_i,          // with the FIRST byte of the frame
  input  logic        in_eof_i,          // with the LAST byte of the frame
  input  logic [31:0] expect_bytes_i,    // canvas bytes for the mode (at sof)

  // the finalized frame CRC (contract output)
  output logic [31:0] frame_crc_o,
  output logic        frame_crc_valid_o, // one-cycle pulse after the eof byte
  output logic [31:0] bytes_captured_o,
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
  logic        err_v = 1'b0;
  /* verilator lint_on PROCASSINIT */

  // ------------------------------------------------------ sequential ------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      crc_r <= 32'hFFFF_FFFF;
      running <= 1'b0;
      n_bytes <= 32'd0;
      expect_n <= 32'd0;
      fin_v <= 1'b0;
      fin_crc <= 32'd0;
      err_v <= 1'b0;
    end else begin
      fin_v <= 1'b0;
      err_v <= 1'b0;
      if (in_valid_i) begin
        if (in_sof_i) begin
          // frame start: seed + first byte, latch the expected size
          crc_r <= zhao_abi_pkg::zhao_crc32c_step(32'hFFFF_FFFF, in_byte_i);
          running <= 1'b1;
          n_bytes <= 32'd1;
          expect_n <= expect_bytes_i;
        end else if (running) begin
          crc_r <= zhao_abi_pkg::zhao_crc32c_step(crc_r, in_byte_i);
          n_bytes <= n_bytes + 32'd1;
          if (in_eof_i) begin
            // finalize: xorout + the mis-sized-stream gate
            if (n_bytes + 32'd1 == expect_n) begin
              fin_crc <= ~(zhao_abi_pkg::zhao_crc32c_step(crc_r, in_byte_i));
              fin_v <= 1'b1;
            end else begin
              err_v <= 1'b1;  // protocol violation: CRC not published
            end
            running <= 1'b0;
            n_bytes <= 32'd0;
          end
        end else begin
          // byte outside any frame (no sof since eof/idle): raster violation
          err_v <= 1'b1;
        end
      end
    end
  end

  assign frame_crc_o = fin_crc;
  assign frame_crc_valid_o = fin_v;
  assign bytes_captured_o = n_bytes;
  assign size_err_evt_o = err_v;

endmodule : zhao_debug_crc
