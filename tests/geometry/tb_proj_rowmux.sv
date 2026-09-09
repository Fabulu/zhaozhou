// tb_proj_rowmux.sv — two zhao_project_core instances, one parameter apart.
//
// r_* is the reference: ROWS_PER_PASS=3, the historical spatial shape whose
// default configuration the existing geom/terrain differential suites already
// pin to the shipped oracle. d_* is the device under test: ROWS_PER_PASS=1,
// the row-sequenced shape (dsp.md lever 1).
//
// The two instances have SEPARATE configuration buses and SEPARATE enables so
// the driver (tests/geometry/proj_rowmux_directed.cpp) can:
//   * run each under its own stall pattern and compare output STREAMS,
//   * land a configuration write mid-sequence on the DUT only (the tear case
//     the capture-at-accept law must survive),
//   * skew one DUT matrix word as the checker's positive control.
//
// Nothing in here computes anything: this file is wiring, so that the
// comparison logic lives in C++ where a mismatch prints operands.

`default_nettype none

module tb_proj_rowmux (
    input wire clk,
    input wire rst_n,

    // ---- reference: ROWS_PER_PASS = 3 --------------------------------------
    input  wire        r_cfg_we_i,
    input  wire        r_cfg_view_i,
    input  wire [ 4:0] r_cfg_addr_i,
    input  wire [31:0] r_cfg_data_i,
    input  wire        r_en_i,
    output wire        r_in_ready_o,
    input  wire        r_in_valid_i,
    input  wire signed [31:0] r_vx_i,
    input  wire signed [31:0] r_vy_i,
    input  wire signed [31:0] r_vz_i,
    input  wire        r_view_i,
    input  wire [15:0] r_payload_i,
    output wire        r_out_valid_o,
    output wire signed [20:0] r_out_x_o,
    output wire signed [20:0] r_out_y_o,
    output wire signed [31:0] r_out_d_o,
    output wire [30:0] r_out_w_o,
    output wire        r_out_behind_o,
    output wire        r_out_view_o,
    output wire [15:0] r_out_payload_o,
    output wire        r_busy_o,

    // ---- device under test: ROWS_PER_PASS = 1 ------------------------------
    input  wire        d_cfg_we_i,
    input  wire        d_cfg_view_i,
    input  wire [ 4:0] d_cfg_addr_i,
    input  wire [31:0] d_cfg_data_i,
    input  wire        d_en_i,
    output wire        d_in_ready_o,
    input  wire        d_in_valid_i,
    input  wire signed [31:0] d_vx_i,
    input  wire signed [31:0] d_vy_i,
    input  wire signed [31:0] d_vz_i,
    input  wire        d_view_i,
    input  wire [15:0] d_payload_i,
    output wire        d_out_valid_o,
    output wire signed [20:0] d_out_x_o,
    output wire signed [20:0] d_out_y_o,
    output wire signed [31:0] d_out_d_o,
    output wire [30:0] d_out_w_o,
    output wire        d_out_behind_o,
    output wire        d_out_view_o,
    output wire [15:0] d_out_payload_o,
    output wire        d_busy_o
);

  zhao_project_core #(
      .PAYLOAD_W(16),
      .ROWS_PER_PASS(3)
  ) u_ref (
      .clk          (clk),
      .rst_n        (rst_n),
      .cfg_we_i     (r_cfg_we_i),
      .cfg_view_i   (r_cfg_view_i),
      .cfg_addr_i   (r_cfg_addr_i),
      .cfg_data_i   (r_cfg_data_i),
      .en_i         (r_en_i),
      .in_ready_o   (r_in_ready_o),
      .in_valid_i   (r_in_valid_i),
      .vx_i         (r_vx_i),
      .vy_i         (r_vy_i),
      .vz_i         (r_vz_i),
      .view_i       (r_view_i),
      .payload_i    (r_payload_i),
      .out_valid_o  (r_out_valid_o),
      .out_x_o      (r_out_x_o),
      .out_y_o      (r_out_y_o),
      .out_d_o      (r_out_d_o),
      .out_w_o      (r_out_w_o),
      .out_behind_o (r_out_behind_o),
      .out_view_o   (r_out_view_o),
      .out_payload_o(r_out_payload_o),
      .busy_o       (r_busy_o)
  );

  zhao_project_core #(
      .PAYLOAD_W(16),
      .ROWS_PER_PASS(1)
  ) u_dut (
      .clk          (clk),
      .rst_n        (rst_n),
      .cfg_we_i     (d_cfg_we_i),
      .cfg_view_i   (d_cfg_view_i),
      .cfg_addr_i   (d_cfg_addr_i),
      .cfg_data_i   (d_cfg_data_i),
      .en_i         (d_en_i),
      .in_ready_o   (d_in_ready_o),
      .in_valid_i   (d_in_valid_i),
      .vx_i         (d_vx_i),
      .vy_i         (d_vy_i),
      .vz_i         (d_vz_i),
      .view_i       (d_view_i),
      .payload_i    (d_payload_i),
      .out_valid_o  (d_out_valid_o),
      .out_x_o      (d_out_x_o),
      .out_y_o      (d_out_y_o),
      .out_d_o      (d_out_d_o),
      .out_w_o      (d_out_w_o),
      .out_behind_o (d_out_behind_o),
      .out_view_o   (d_out_view_o),
      .out_payload_o(d_out_payload_o),
      .busy_o       (d_busy_o)
  );

endmodule : tb_proj_rowmux

`default_nettype wire
