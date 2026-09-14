// tb_texture_bilerp_lane_v2_pair.sv — cycle-exact legacy/V2 lockstep wrapper.
`default_nettype none

module tb_texture_bilerp_lane_v2_pair #(
    parameter int unsigned TOKW = 18
) (
    input  var logic             clk,
    input  var logic             rst_n,
    input  var logic             job_valid_i,
    input  var logic [7:0]       t00_i,
    input  var logic [7:0]       t10_i,
    input  var logic [7:0]       t01_i,
    input  var logic [7:0]       t11_i,
    input  var logic [7:0]       fu_i,
    input  var logic [7:0]       fv_i,
    input  var logic [TOKW-1:0]  tok_i,
    input  var logic [1:0]       chan_i,
    input  var logic             out_ready_i,

    output var logic             legacy_job_ready_o,
    output var logic             legacy_out_valid_o,
    output var logic [7:0]       legacy_out_o,
    output var logic [TOKW-1:0]  legacy_out_tok_o,
    output var logic [1:0]       legacy_out_chan_o,
    output var logic [31:0]      legacy_jobs_o,
    output var logic [1:0]       legacy_occupancy_o,

    output var logic             v2_job_ready_o,
    output var logic             v2_out_valid_o,
    output var logic [7:0]       v2_out_o,
    output var logic [TOKW-1:0]  v2_out_tok_o,
    output var logic [1:0]       v2_out_chan_o,
    output var logic             v2_idle_o,
    output var logic [31:0]      v2_jobs_o,
    output var logic [1:0]       v2_occupancy_o
);

  zhao_texture_bilerp_lane #(.TOKW(TOKW)) u_legacy (
      .clk(clk), .rst_n(rst_n),
      .job_valid_i(job_valid_i), .job_ready_o(legacy_job_ready_o),
      .t00_i(t00_i), .t10_i(t10_i), .t01_i(t01_i), .t11_i(t11_i),
      .fu_i(fu_i), .fv_i(fv_i), .tok_i(tok_i), .chan_i(chan_i),
      .out_valid_o(legacy_out_valid_o), .out_ready_i(out_ready_i),
      .out_o(legacy_out_o), .out_tok_o(legacy_out_tok_o),
      .out_chan_o(legacy_out_chan_o),
      .jobs_o(legacy_jobs_o), .occupancy_o(legacy_occupancy_o)
  );

  zhao_texture_bilerp_lane_v2 #(.TOKW(TOKW)) u_v2 (
      .clk(clk), .rst_n(rst_n),
      .job_valid_i(job_valid_i), .job_ready_o(v2_job_ready_o),
      .t00_i(t00_i), .t10_i(t10_i), .t01_i(t01_i), .t11_i(t11_i),
      .fu_i(fu_i), .fv_i(fv_i), .tok_i(tok_i), .chan_i(chan_i),
      .out_valid_o(v2_out_valid_o), .out_ready_i(out_ready_i),
      .out_o(v2_out_o), .out_tok_o(v2_out_tok_o),
      .out_chan_o(v2_out_chan_o),
      .idle_o(v2_idle_o), .jobs_o(v2_jobs_o), .occupancy_o(v2_occupancy_o)
  );

endmodule : tb_texture_bilerp_lane_v2_pair

`default_nettype wire
