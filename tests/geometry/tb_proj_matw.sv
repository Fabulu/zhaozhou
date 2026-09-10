// tb_proj_matw.sv — three zhao_project_core instances, one parameter apart:
// the matrix operand width, owner ruling R1 (2026-09-09).
//
//   r_*  MATW=32, ROWS_PER_PASS=3  the reference: the historical full-width
//                                  operand, pinned to the shipped oracle by
//                                  geom_project_directed and the terrain
//                                  suites (the latter sweeping matrix words
//                                  over the FULL s32 domain).
//   d_*  MATW=18, ROWS_PER_PASS=3  the narrowed operand, spatial rows.
//   s_*  MATW=18, ROWS_PER_PASS=1  the narrowed operand, sequenced rows --
//                                  because ROWS_PER_PASS landed the day before
//                                  and the two levers must keep composing.
//
// Three SEPARATE configuration buses and three SEPARATE enables, so the
// driver (tests/geometry/proj_matw_directed.cpp) can:
//   * load the same matrix into all three and compare output STREAMS under
//     independent stall patterns -- the acceptance criterion: for every
//     coefficient inside the ruled +-1.99998 range the narrowing is INVISIBLE;
//   * write an out-of-range word to the two narrowed instances only, and
//     require the refusal counter to move by exactly one while the reference
//     -- never written -- still agrees with them afterwards (the register kept
//     its old value: refusal, not clamp);
//   * skew one narrowed instance's word as the checker's positive control.
//
// Nothing in here computes anything: this file is wiring, so that the
// comparison logic lives in C++ where a mismatch prints operands.

`default_nettype none

module tb_proj_matw (
    input wire clk,
    input wire rst_n,

    // ---- reference: MATW = 32, ROWS_PER_PASS = 3 -----------------------------
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
    output wire [31:0] r_mat_refused_o,

    // ---- narrowed, spatial: MATW = 18, ROWS_PER_PASS = 3 ----------------------
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
    output wire        d_busy_o,
    output wire [31:0] d_mat_refused_o,

    // ---- narrowed, sequenced: MATW = 18, ROWS_PER_PASS = 1 --------------------
    input  wire        s_cfg_we_i,
    input  wire        s_cfg_view_i,
    input  wire [ 4:0] s_cfg_addr_i,
    input  wire [31:0] s_cfg_data_i,
    input  wire        s_en_i,
    output wire        s_in_ready_o,
    input  wire        s_in_valid_i,
    input  wire signed [31:0] s_vx_i,
    input  wire signed [31:0] s_vy_i,
    input  wire signed [31:0] s_vz_i,
    input  wire        s_view_i,
    input  wire [15:0] s_payload_i,
    output wire        s_out_valid_o,
    output wire signed [20:0] s_out_x_o,
    output wire signed [20:0] s_out_y_o,
    output wire signed [31:0] s_out_d_o,
    output wire [30:0] s_out_w_o,
    output wire        s_out_behind_o,
    output wire        s_out_view_o,
    output wire [15:0] s_out_payload_o,
    output wire        s_busy_o,
    output wire [31:0] s_mat_refused_o
);

  zhao_project_core #(
      .PAYLOAD_W(16),
      .ROWS_PER_PASS(3),
      .MATW(32)
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
      .busy_o       (r_busy_o),
      .mat_refused_o(r_mat_refused_o)
  );

  zhao_project_core #(
      .PAYLOAD_W(16),
      .ROWS_PER_PASS(3),
      .MATW(18)
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
      .busy_o       (d_busy_o),
      .mat_refused_o(d_mat_refused_o)
  );

  zhao_project_core #(
      .PAYLOAD_W(16),
      .ROWS_PER_PASS(1),
      .MATW(18)
  ) u_seq (
      .clk          (clk),
      .rst_n        (rst_n),
      .cfg_we_i     (s_cfg_we_i),
      .cfg_view_i   (s_cfg_view_i),
      .cfg_addr_i   (s_cfg_addr_i),
      .cfg_data_i   (s_cfg_data_i),
      .en_i         (s_en_i),
      .in_ready_o   (s_in_ready_o),
      .in_valid_i   (s_in_valid_i),
      .vx_i         (s_vx_i),
      .vy_i         (s_vy_i),
      .vz_i         (s_vz_i),
      .view_i       (s_view_i),
      .payload_i    (s_payload_i),
      .out_valid_o  (s_out_valid_o),
      .out_x_o      (s_out_x_o),
      .out_y_o      (s_out_y_o),
      .out_d_o      (s_out_d_o),
      .out_w_o      (s_out_w_o),
      .out_behind_o (s_out_behind_o),
      .out_view_o   (s_out_view_o),
      .out_payload_o(s_out_payload_o),
      .busy_o       (s_busy_o),
      .mat_refused_o(s_mat_refused_o)
  );

endmodule : tb_proj_matw

`default_nettype wire
