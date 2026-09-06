// FIXTURE -- DELIBERATELY WRONG. Not built, not fitted, never instantiated.
//
// SAMPLE_RESULT_1 declared 32x32 where section 6 declares 64x40. A result word
// is RGBA32 plus STATUS8; dropping the status plane is one of the quiet
// reductions section 25.7 rejects, and shrinking the owner window is section
// 21.5's "do not reduce the owner window ... merely to pass the count".
// Expect V3-WIDTH and V3-DEPTH.
module fx_undersized_bank (
    input  var logic        clk,
    input  var logic        wr_en,
    input  var logic [4:0]  wr_addr,
    input  var logic [31:0] wr_data,
    input  var logic [4:0]  rd_addr,
    output var logic [31:0] ram_q
);
  // V3-BANK: SAMPLE_RESULT_1
  (* ramstyle = "M10K" *) logic [31:0] sample_result_1_m [0:31];

  always_ff @(posedge clk) begin
    if (wr_en) sample_result_1_m[wr_addr] <= wr_data;
    ram_q <= sample_result_1_m[rd_addr];
  end
endmodule
