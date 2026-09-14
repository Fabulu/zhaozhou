// tb_raster_attrgrad_v2.sv -- private V2 divider/row-gradient harness.
`default_nettype none

module tb_raster_attrgrad_v2 #(
    parameter int unsigned RADIX = 2
) (
    input  logic               clk,
    input  logic               rst_n,

    input  logic               d_valid_i,
    output logic               d_ready_o,
    input  logic signed [95:0] d_num_i,
    input  logic        [46:0] d_area_i,
    output logic               d_rvalid_o,
    input  logic               d_rready_i,
    output logic signed [31:0] d_q_o,
    output logic               d_saturated_o,
    output logic               d_error_o,
    output logic [31:0]        d_divides_o,
    output logic [31:0]        d_saturations_o,
    output logic [31:0]        d_errors_o,
    output logic [31:0]        d_busy_clocks_o,

    input  logic               g_job_valid_i,
    output logic               g_job_ready_o,
    input  logic signed [95:0] g_job_n0_i,
    input  logic signed [71:0] g_job_dndx_i,
    input  logic signed [71:0] g_job_dndy_i,
    input  logic        [46:0] g_job_area2_i,
    input  logic signed [11:0] g_job_min_x_i,
    input  logic signed [11:0] g_job_tile_x_i,
    input  logic signed [11:0] g_job_tile_y_i,

    input  logic               g_cov_valid_i,
    output logic               g_cov_ready_o,
    input  logic        [3:0]  g_cov_row_i,
    input  logic        [15:0] g_cov_mask_i,
    input  logic               g_cov_last_i,

    output logic               g_q_valid_o,
    input  logic               g_q_ready_i,
    output logic signed [31:0] g_q_o,
    output logic        [3:0]  g_q_row_o,
    output logic        [3:0]  g_q_col_o,
    output logic               g_q_last_o,
    output logic               g_q_saturated_o,
    output logic               g_q_error_o,
    output logic               g_idle_o,
    output logic [31:0]        g_pixels_o,
    output logic [31:0]        g_divides_o,
    output logic [31:0]        g_saturations_o,
    output logic [31:0]        g_divide_errors_o
);

  zhao_raster_attrdiv_v2 #(.RADIX(RADIX)) u_div_direct (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (d_valid_i),
      .v_ready_o     (d_ready_o),
      .num_i         (d_num_i),
      .area_i        (d_area_i),
      .r_valid_o     (d_rvalid_o),
      .r_ready_i     (d_rready_i),
      .q_o           (d_q_o),
      .q_saturated_o (d_saturated_o),
      .q_error_o     (d_error_o),
      .divides_o     (d_divides_o),
      .saturations_o (d_saturations_o),
      .errors_o      (d_errors_o),
      .busy_clocks_o (d_busy_clocks_o)
  );

  zhao_raster_attrgrad_v2 #(.RADIX(RADIX)) u_grad (
      .clk             (clk),
      .rst_n           (rst_n),
      .job_valid_i     (g_job_valid_i),
      .job_ready_o     (g_job_ready_o),
      .job_n0_i        (g_job_n0_i),
      .job_dndx_i      (g_job_dndx_i),
      .job_dndy_i      (g_job_dndy_i),
      .job_area2_i     (g_job_area2_i),
      .job_min_x_i     (g_job_min_x_i),
      .job_tile_x_i    (g_job_tile_x_i),
      .job_tile_y_i    (g_job_tile_y_i),
      .cov_valid_i     (g_cov_valid_i),
      .cov_ready_o     (g_cov_ready_o),
      .cov_row_i       (g_cov_row_i),
      .cov_mask_i      (g_cov_mask_i),
      .cov_last_i      (g_cov_last_i),
      .q_valid_o       (g_q_valid_o),
      .q_ready_i       (g_q_ready_i),
      .q_o             (g_q_o),
      .q_row_o         (g_q_row_o),
      .q_col_o         (g_q_col_o),
      .q_last_o        (g_q_last_o),
      .q_saturated_o   (g_q_saturated_o),
      .q_error_o       (g_q_error_o),
      .idle_o          (g_idle_o),
      .pixels_o        (g_pixels_o),
      .divides_o       (g_divides_o),
      .saturations_o   (g_saturations_o),
      .divide_errors_o (g_divide_errors_o)
  );

endmodule : tb_raster_attrgrad_v2

`default_nettype wire
