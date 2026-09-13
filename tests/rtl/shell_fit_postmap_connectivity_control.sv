// Genuine Quartus post-map positive baseline for the shell boundary witness.
// This test-only top keeps the production six-port/ten-bit interface and gives
// every input mapped fanout and every output mapped fanin.
module shell_fit_postmap_connectivity_control (
    input  var logic       gpu_clk,
    input  var logic       vid_clk,
    input  var logic       audio_clk,
    input  var logic       rst_n,
    output var logic [2:0] fit_signature_o,
    output var logic [2:0] fit_epoch_o
);

  logic gpu_q;
  logic vid_q;
  logic audio_q;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) gpu_q <= 1'b0;
    else gpu_q <= ~gpu_q;
  end

  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) vid_q <= 1'b0;
    else vid_q <= ~vid_q;
  end

  always_ff @(posedge audio_clk or negedge rst_n) begin
    if (!rst_n) audio_q <= 1'b0;
    else audio_q <= ~audio_q;
  end

  always_comb begin
    fit_signature_o = {gpu_q ^ vid_q, audio_q ^ gpu_q, vid_q ^ audio_q};
    fit_epoch_o = {gpu_q, vid_q, audio_q};
  end

endmodule : shell_fit_postmap_connectivity_control
