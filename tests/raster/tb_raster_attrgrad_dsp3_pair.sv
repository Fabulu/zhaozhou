// tb_raster_attrgrad_dsp3_pair.sv -- old/ATTR3 cycle differential harness.
`default_nettype none

module tb_raster_attrgrad_dsp3_pair #(
    parameter int unsigned RADIX = 2
) (
    input  logic               clk,
    input  logic               rst_n,
    input  logic               job_valid_i,
    input  logic signed [95:0] job_n0_i,
    input  logic signed [71:0] job_dndx_i,
    input  logic signed [71:0] job_dndy_i,
    input  logic        [46:0] job_area2_i,
    input  logic signed [11:0] job_min_x_i,
    input  logic signed [11:0] job_tile_x_i,
    input  logic signed [11:0] job_tile_y_i,
    input  logic               cov_valid_i,
    input  logic        [3:0]  cov_row_i,
    input  logic        [15:0] cov_mask_i,
    input  logic               cov_last_i,
    input  logic               q_ready_i,

    output logic               old_job_ready_o,
    output logic               new_job_ready_o,
    output logic               old_cov_ready_o,
    output logic               new_cov_ready_o,
    output logic               old_q_valid_o,
    output logic               new_q_valid_o,
    output logic signed [31:0] old_q_o,
    output logic signed [31:0] new_q_o,
    output logic        [3:0]  old_row_o,
    output logic        [3:0]  new_row_o,
    output logic        [3:0]  old_col_o,
    output logic        [3:0]  new_col_o,
    output logic               old_last_o,
    output logic               new_last_o,
    output logic               old_saturated_o,
    output logic               new_saturated_o,
    output logic               old_error_o,
    output logic               new_error_o,
    output logic               old_idle_o,
    output logic               new_idle_o,
    output logic [31:0]        old_pixels_o,
    output logic [31:0]        new_pixels_o,
    output logic [31:0]        old_divides_o,
    output logic [31:0]        new_divides_o,
    output logic [31:0]        old_saturations_o,
    output logic [31:0]        new_saturations_o,
    output logic [31:0]        old_errors_o,
    output logic [31:0]        new_errors_o,
    output logic               new_offset_guard_fault_o
);

  zhao_raster_attrgrad_v2 #(.RADIX(RADIX)) u_old (
      .clk(clk), .rst_n(rst_n),
      .job_valid_i(job_valid_i), .job_ready_o(old_job_ready_o),
      .job_n0_i(job_n0_i), .job_dndx_i(job_dndx_i), .job_dndy_i(job_dndy_i),
      .job_area2_i(job_area2_i), .job_min_x_i(job_min_x_i),
      .job_tile_x_i(job_tile_x_i), .job_tile_y_i(job_tile_y_i),
      .cov_valid_i(cov_valid_i), .cov_ready_o(old_cov_ready_o),
      .cov_row_i(cov_row_i), .cov_mask_i(cov_mask_i), .cov_last_i(cov_last_i),
      .q_valid_o(old_q_valid_o), .q_ready_i(q_ready_i), .q_o(old_q_o),
      .q_row_o(old_row_o), .q_col_o(old_col_o), .q_last_o(old_last_o),
      .q_saturated_o(old_saturated_o), .q_error_o(old_error_o),
      .idle_o(old_idle_o), .pixels_o(old_pixels_o), .divides_o(old_divides_o),
      .saturations_o(old_saturations_o), .divide_errors_o(old_errors_o)
  );

  zhao_raster_attrgrad_dsp3 #(.RADIX(RADIX)) u_new (
      .clk(clk), .rst_n(rst_n),
      .job_valid_i(job_valid_i), .job_ready_o(new_job_ready_o),
      .job_n0_i(job_n0_i), .job_dndx_i(job_dndx_i), .job_dndy_i(job_dndy_i),
      .job_area2_i(job_area2_i), .job_min_x_i(job_min_x_i),
      .job_tile_x_i(job_tile_x_i), .job_tile_y_i(job_tile_y_i),
      .cov_valid_i(cov_valid_i), .cov_ready_o(new_cov_ready_o),
      .cov_row_i(cov_row_i), .cov_mask_i(cov_mask_i), .cov_last_i(cov_last_i),
      .q_valid_o(new_q_valid_o), .q_ready_i(q_ready_i), .q_o(new_q_o),
      .q_row_o(new_row_o), .q_col_o(new_col_o), .q_last_o(new_last_o),
      .q_saturated_o(new_saturated_o), .q_error_o(new_error_o),
      .idle_o(new_idle_o), .pixels_o(new_pixels_o), .divides_o(new_divides_o),
      .saturations_o(new_saturations_o), .divide_errors_o(new_errors_o)
  );

  assign new_offset_guard_fault_o = u_new.verify_offset_guard_fault_q;

endmodule : tb_raster_attrgrad_dsp3_pair

`default_nettype wire
