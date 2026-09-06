// FIXTURE -- DELIBERATELY WRONG. Not built, never used.
//
// The multi-bank marker exists for a GENERATE loop that really does create
// several physical stores. Putting it on a single array declaration claims
// three banks where there is one plane, which is the accounting error the
// marker was added to avoid -- three inventory rows, one M10K.
// Expect exit 2.
module fx_array_multi_marker (
    input  var logic        clk,
    input  var logic        wr_en,
    input  var logic [5:0]  wr_addr,
    input  var logic [39:0] wr_data,
    input  var logic [5:0]  rd_addr,
    output var logic [39:0] ram_q
);
  // V3-BANK: SAMPLE_RESULT_0, SAMPLE_RESULT_1, SAMPLE_RESULT_2
  (* ramstyle = "M10K" *) logic [39:0] sres_m [0:63];

  always_ff @(posedge clk) begin
    if (wr_en) sres_m[wr_addr] <= wr_data;
    ram_q <= sres_m[rd_addr];
  end
endmodule
