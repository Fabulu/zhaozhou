// tb_bilerp_pair.sv — the shipped combinational bilerp and the pipelined lane.
//
// The lane claims to be bit-identical BY CONSTRUCTION: it splits the same
// expressions at a register between two exact intermediates. Construction
// arguments are how wrong arithmetic gets shipped, so the shipped block is
// instantiated here and every result is compared.
`default_nettype none

module tb_bilerp_pair #(
    parameter int unsigned TOKW = 16
) (
    input var logic clk,
    input var logic rst_n,

    // shared stimulus
    input  var logic [7:0] t00_i,
    input  var logic [7:0] t10_i,
    input  var logic [7:0] t01_i,
    input  var logic [7:0] t11_i,
    input  var logic [7:0] fu_i,
    input  var logic [7:0] fv_i,

    // the combinational reference, answering the same cycle
    output var logic [7:0] ref_o,

    // the lane
    input  var logic            lane_valid_i,
    output var logic            lane_ready_o,
    input  var logic [TOKW-1:0] lane_tok_i,
    input  var logic [1:0]      lane_chan_i,
    output var logic            lane_valid_o,
    input  var logic            lane_ready_i,
    output var logic [7:0]      lane_out_o,
    output var logic [TOKW-1:0] lane_tok_o,
    output var logic [1:0]      lane_chan_o,
    output var logic [31:0]     lane_jobs_o,
    output var logic [1:0]      lane_occ_o
);

  zhao_texture_bilerp u_ref (
      .t00_i(t00_i),
      .t10_i(t10_i),
      .t01_i(t01_i),
      .t11_i(t11_i),
      .fu_i (fu_i),
      .fv_i (fv_i),
      .out_o(ref_o)
  );

  zhao_texture_bilerp_lane #(
      .TOKW(TOKW)
  ) u_lane (
      .clk        (clk),
      .rst_n      (rst_n),
      .job_valid_i(lane_valid_i),
      .job_ready_o(lane_ready_o),
      .t00_i      (t00_i),
      .t10_i      (t10_i),
      .t01_i      (t01_i),
      .t11_i      (t11_i),
      .fu_i       (fu_i),
      .fv_i       (fv_i),
      .tok_i      (lane_tok_i),
      .chan_i     (lane_chan_i),
      .out_valid_o(lane_valid_o),
      .out_ready_i(lane_ready_i),
      .out_o      (lane_out_o),
      .out_tok_o  (lane_tok_o),
      .out_chan_o (lane_chan_o),
      .jobs_o     (lane_jobs_o),
      .occupancy_o(lane_occ_o)
  );

endmodule : tb_bilerp_pair

`default_nettype wire
