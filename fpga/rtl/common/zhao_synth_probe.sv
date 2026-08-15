// zhao_synth_probe.sv — standing synthesis probe for the generated ABI
// package (charter 21 step 11 applied to the ABI artifacts themselves).
//
// Purpose: prove fpga/rtl/generated/zhao_abi_pkg.sv is QUARTUS-synthesizable
// (the verification-only open-array helpers are QUARTUS_SYNTHESIS-guarded by
// abi-gen) and report the fabric cost of a real hardware use of it: a
// streaming CRC-32C byte engine plus a packed frame-header unpack + validate
// datapath — the seed of the real CMD.DMA CRC gate (W2.6 owns the full one).
//
// NOT a console block: never instantiated by Zhaozhou.sv; synthesis reports
// only (tools/report consumes them when the synthesis lane runs).

module zhao_synth_probe (
    input  logic        clk,
    input  logic        rst_n,
    input  logic        byte_valid,
    input  logic [7:0]  byte_in,
    input  logic        frame_start,   // restart the CRC accumulation
    output logic [31:0] crc_running,
    output logic [31:0] hdr_magic,
    output logic [15:0] hdr_abi_version
);
  import zhao_abi_pkg::*;

  logic [31:0] crc;
  logic [7:0]  window [0:ZHAO_FRAME_HEADER_BYTES-32-1]; // header bytes 0..3

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      crc           <= 32'hFFFFFFFF;
      crc_running   <= 32'hFFFFFFFF;
      for (int i = 0; i < 4; i++) window[i] <= 8'h00;
    end else begin
      if (frame_start) begin
        crc <= 32'hFFFFFFFF;
      end else if (byte_valid) begin
        crc <= zhao_crc32c_step(crc, byte_in);
      end
      crc_running <= frame_start ? 32'hFFFFFFFF : (byte_valid ? zhao_crc32c_step(crc, byte_in) : crc);
      // keep the first four bytes for the magic/version exposure
      if (byte_valid) begin
        window[0] <= window[1];
        window[1] <= window[2];
        window[2] <= window[3];
        window[3] <= byte_in;
      end
    end
  end

  assign hdr_magic       = {window[3], window[2], window[1], window[0]};
  assign hdr_abi_version = '0; // bytes 4-5 exposed when the probe grows; width law kept
endmodule
