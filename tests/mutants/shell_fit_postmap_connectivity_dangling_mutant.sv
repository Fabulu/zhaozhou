// COMMITTED MUTANT: audio_clk is deliberately unused so the genuine post-map
// witness must omit it or report zero mapped fanout. This module is renamed and
// remains outside every production/source closure.
module shell_fit_postmap_connectivity_dangling_mutant (
    input  var logic       gpu_clk,
    input  var logic       vid_clk,
    input  var logic       audio_clk,
    input  var logic       rst_n,
    output var logic [2:0] fit_signature_o,
    output var logic [2:0] fit_epoch_o
);

  logic gpu_q;
  logic vid_q;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) gpu_q <= 1'b0;
    else gpu_q <= ~gpu_q;
  end

  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) vid_q <= 1'b0;
    else vid_q <= ~vid_q;
  end

  always_comb begin
    fit_signature_o = {gpu_q ^ vid_q, gpu_q, vid_q};
    fit_epoch_o = {vid_q, gpu_q, gpu_q ^ vid_q};
  end

endmodule : shell_fit_postmap_connectivity_dangling_mutant
