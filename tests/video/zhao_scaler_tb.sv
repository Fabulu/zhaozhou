// zhao_scaler_tb.sv — Verilator wrapper for the VIDEO.SCALER directed and
// random tests: flattened struct ports for clean C++ access (the same
// convention as zhao_video_tb.sv; testbench component, not synthesized).

module zhao_scaler_tb
  import zhao_pkg::*;
(
  input  logic        vid_clk,
  input  logic        rst_n,
  input  logic        in_valid,
  input  logic [15:0] in_rgb,
  input  logic [9:0]  in_x,
  input  logic [7:0]  in_y,
  input  logic        in_hsync,
  input  logic        in_vsync,
  input  logic        in_hblank,
  input  logic        in_vblank,
  input  logic        out_ready,
  output logic        out_valid,
  output logic [15:0] out_rgb,
  output logic [9:0]  out_x,
  output logic [7:0]  out_y,
  output logic        out_hsync,
  output logic        out_vsync,
  output logic        out_hblank,
  output logic        out_vblank,
  output logic        never_active
);

  zhao_px_stream_t in_s, out_s;
  assign in_s.valid  = in_valid;
  assign in_s.rgb565 = in_rgb;
  assign in_s.x      = in_x;
  assign in_s.y      = in_y;
  assign in_s.hsync  = in_hsync;
  assign in_s.vsync  = in_vsync;
  assign in_s.hblank = in_hblank;
  assign in_s.vblank = in_vblank;

  zhao_video_scaler u_scaler (
    .vid_clk      (vid_clk),
    .rst_n        (rst_n),
    .in           (in_s),
    .out          (out_s),
    .out_ready    (out_ready),
    .never_active (never_active)
  );

  assign out_valid  = out_s.valid;
  assign out_rgb    = out_s.rgb565;
  assign out_x      = out_s.x;
  assign out_y      = out_s.y;
  assign out_hsync  = out_s.hsync;
  assign out_vsync  = out_s.vsync;
  assign out_hblank = out_s.hblank;
  assign out_vblank = out_s.vblank;

endmodule : zhao_scaler_tb
