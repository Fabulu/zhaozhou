// tb_proj_matw_mutant.sv — the REAL core at MATW=32 beside the committed
// MUTANT at MATW=18 (tests/mutants/zhao_project_core_mutant.sv: fits-check
// removed). Driven by tests/geometry/proj_matw_mutant_control.cpp with
// INVERTED polarity: it passes when the differential FAILS on an out-of-range
// coefficient while the mutant's counter stays at zero. Wiring only.

`default_nettype none

module tb_proj_matw_mutant (
    input wire clk,
    input wire rst_n,

    // ---- reference: the real core, MATW = 32 ---------------------------------
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

    // ---- the mutant, MATW = 18 -------------------------------------------------
    input  wire        m_cfg_we_i,
    input  wire        m_cfg_view_i,
    input  wire [ 4:0] m_cfg_addr_i,
    input  wire [31:0] m_cfg_data_i,
    input  wire        m_en_i,
    output wire        m_in_ready_o,
    input  wire        m_in_valid_i,
    input  wire signed [31:0] m_vx_i,
    input  wire signed [31:0] m_vy_i,
    input  wire signed [31:0] m_vz_i,
    input  wire        m_view_i,
    input  wire [15:0] m_payload_i,
    output wire        m_out_valid_o,
    output wire signed [20:0] m_out_x_o,
    output wire signed [20:0] m_out_y_o,
    output wire signed [31:0] m_out_d_o,
    output wire [30:0] m_out_w_o,
    output wire        m_out_behind_o,
    output wire        m_out_view_o,
    output wire [15:0] m_out_payload_o,
    output wire        m_busy_o,
    output wire [31:0] m_mat_refused_o
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

  zhao_project_core_mutant #(
      .PAYLOAD_W(16),
      .ROWS_PER_PASS(3),
      .MATW(18)
  ) u_mut (
      .clk          (clk),
      .rst_n        (rst_n),
      .cfg_we_i     (m_cfg_we_i),
      .cfg_view_i   (m_cfg_view_i),
      .cfg_addr_i   (m_cfg_addr_i),
      .cfg_data_i   (m_cfg_data_i),
      .en_i         (m_en_i),
      .in_ready_o   (m_in_ready_o),
      .in_valid_i   (m_in_valid_i),
      .vx_i         (m_vx_i),
      .vy_i         (m_vy_i),
      .vz_i         (m_vz_i),
      .view_i       (m_view_i),
      .payload_i    (m_payload_i),
      .out_valid_o  (m_out_valid_o),
      .out_x_o      (m_out_x_o),
      .out_y_o      (m_out_y_o),
      .out_d_o      (m_out_d_o),
      .out_w_o      (m_out_w_o),
      .out_behind_o (m_out_behind_o),
      .out_view_o   (m_out_view_o),
      .out_payload_o(m_out_payload_o),
      .busy_o       (m_busy_o),
      .mat_refused_o(m_mat_refused_o)
  );

endmodule : tb_proj_matw_mutant

`default_nettype wire
