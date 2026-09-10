// tb_proj_service_rowmux.sv — zhao_project_service at ROWS_PER_PASS=1.
//
// The service predates its own test (a gap this file narrows, not closes);
// what is NEW and untested is three lines: the arbiter's grants are gated on
// the core's `in_ready_o`, so at ROWS_PER_PASS=1 the aggregate rate is one
// vertex per three cycles and the round-robin flip happens only on a grant
// the core can actually take. The driver
// (tests/geometry/proj_service_rowmux_smoke.cpp) checks routing, order,
// fairness and the contended counter under that gating. Byte-level projection
// correctness is NOT re-proven here — proj_rowmux_directed pins the core.

`default_nettype none

module tb_proj_service_rowmux (
    input wire clk,
    input wire rst_n,

    input  wire        cfg_we_i,
    input  wire        cfg_view_i,
    input  wire [ 4:0] cfg_addr_i,
    input  wire [31:0] cfg_data_i,
    input  wire        en_i,

    input  wire        a_valid_i,
    output wire        a_ready_o,
    input  wire signed [31:0] a_vx_i,
    input  wire signed [31:0] a_vy_i,
    input  wire signed [31:0] a_vz_i,
    input  wire        a_view_i,
    input  wire [15:0] a_payload_i,
    output wire        a_valid_o,
    output wire signed [20:0] a_x_o,
    output wire signed [20:0] a_y_o,
    output wire signed [31:0] a_d_o,
    output wire [30:0] a_w_o,
    output wire        a_behind_o,
    output wire        a_view_o,
    output wire [15:0] a_payload_o,

    input  wire        b_valid_i,
    output wire        b_ready_o,
    input  wire signed [31:0] b_vx_i,
    input  wire signed [31:0] b_vy_i,
    input  wire signed [31:0] b_vz_i,
    input  wire        b_view_i,
    input  wire [41:0] b_payload_i,
    output wire        b_valid_o,
    output wire signed [20:0] b_x_o,
    output wire signed [20:0] b_y_o,
    output wire signed [31:0] b_d_o,
    output wire [30:0] b_w_o,
    output wire        b_behind_o,
    output wire        b_view_o,
    output wire [41:0] b_payload_o,

    output wire        busy_o,
    output wire [31:0] a_grants_o,
    output wire [31:0] b_grants_o,
    output wire [31:0] contended_o,
    output wire [31:0] mat_refused_o
);

  zhao_project_service #(
      .PAYLOAD_A_W(16),
      .PAYLOAD_B_W(42),
      .ROWS_PER_PASS(1)
  ) u_svc (
      .clk        (clk),
      .rst_n      (rst_n),
      .cfg_we_i   (cfg_we_i),
      .cfg_view_i (cfg_view_i),
      .cfg_addr_i (cfg_addr_i),
      .cfg_data_i (cfg_data_i),
      .en_i       (en_i),
      .a_valid_i  (a_valid_i),
      .a_ready_o  (a_ready_o),
      .a_vx_i     (a_vx_i),
      .a_vy_i     (a_vy_i),
      .a_vz_i     (a_vz_i),
      .a_view_i   (a_view_i),
      .a_payload_i(a_payload_i),
      .a_valid_o  (a_valid_o),
      .a_x_o      (a_x_o),
      .a_y_o      (a_y_o),
      .a_d_o      (a_d_o),
      .a_w_o      (a_w_o),
      .a_behind_o (a_behind_o),
      .a_view_o   (a_view_o),
      .a_payload_o(a_payload_o),
      .b_valid_i  (b_valid_i),
      .b_ready_o  (b_ready_o),
      .b_vx_i     (b_vx_i),
      .b_vy_i     (b_vy_i),
      .b_vz_i     (b_vz_i),
      .b_view_i   (b_view_i),
      .b_payload_i(b_payload_i),
      .b_valid_o  (b_valid_o),
      .b_x_o      (b_x_o),
      .b_y_o      (b_y_o),
      .b_d_o      (b_d_o),
      .b_w_o      (b_w_o),
      .b_behind_o (b_behind_o),
      .b_view_o   (b_view_o),
      .b_payload_o(b_payload_o),
      .busy_o     (busy_o),
      .a_grants_o (a_grants_o),
      .b_grants_o (b_grants_o),
      .contended_o(contended_o),
      .mat_refused_o(mat_refused_o)
  );

endmodule : tb_proj_service_rowmux

`default_nettype wire
