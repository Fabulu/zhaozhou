// tb_texture_bilerp_lane_dsp2.sv -- paired V2/BIL2 cycle differential.
`default_nettype none

module tb_texture_bilerp_lane_dsp2 #(
    parameter int unsigned TOKW = 18
) (
    input  logic            clk,
    input  logic            rst_n,
    input  logic            job_valid_i,
    input  logic [7:0]      t00_i,
    input  logic [7:0]      t10_i,
    input  logic [7:0]      t01_i,
    input  logic [7:0]      t11_i,
    input  logic [7:0]      fu_i,
    input  logic [7:0]      fv_i,
    input  logic [TOKW-1:0] tok_i,
    input  logic [1:0]      chan_i,
    input  logic            out_ready_i,

    output logic            old_job_ready_o,
    output logic            new_job_ready_o,
    output logic            old_out_valid_o,
    output logic            new_out_valid_o,
    output logic [7:0]      old_out_o,
    output logic [7:0]      new_out_o,
    output logic [TOKW-1:0] old_out_tok_o,
    output logic [TOKW-1:0] new_out_tok_o,
    output logic [1:0]      old_out_chan_o,
    output logic [1:0]      new_out_chan_o,
    output logic            old_idle_o,
    output logic            new_idle_o,
    output logic [31:0]     old_jobs_o,
    output logic [31:0]     new_jobs_o,
    output logic [1:0]      old_occupancy_o,
    output logic [1:0]      new_occupancy_o
);

  zhao_texture_bilerp_lane_v2 #(.TOKW(TOKW)) u_old (
      .clk(clk), .rst_n(rst_n),
      .job_valid_i(job_valid_i), .job_ready_o(old_job_ready_o),
      .t00_i(t00_i), .t10_i(t10_i), .t01_i(t01_i), .t11_i(t11_i),
      .fu_i(fu_i), .fv_i(fv_i), .tok_i(tok_i), .chan_i(chan_i),
      .out_valid_o(old_out_valid_o), .out_ready_i(out_ready_i),
      .out_o(old_out_o), .out_tok_o(old_out_tok_o),
      .out_chan_o(old_out_chan_o), .idle_o(old_idle_o),
      .jobs_o(old_jobs_o), .occupancy_o(old_occupancy_o)
  );

  zhao_texture_bilerp_lane_dsp2 #(.TOKW(TOKW)) u_new (
      .clk(clk), .rst_n(rst_n),
      .job_valid_i(job_valid_i), .job_ready_o(new_job_ready_o),
      .t00_i(t00_i), .t10_i(t10_i), .t01_i(t01_i), .t11_i(t11_i),
      .fu_i(fu_i), .fv_i(fv_i), .tok_i(tok_i), .chan_i(chan_i),
      .out_valid_o(new_out_valid_o), .out_ready_i(out_ready_i),
      .out_o(new_out_o), .out_tok_o(new_out_tok_o),
      .out_chan_o(new_out_chan_o), .idle_o(new_idle_o),
      .jobs_o(new_jobs_o), .occupancy_o(new_occupancy_o)
  );

endmodule

`default_nettype wire
