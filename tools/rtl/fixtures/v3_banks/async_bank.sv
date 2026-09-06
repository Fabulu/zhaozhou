// FIXTURE -- DELIBERATELY WRONG. Not built, not fitted, never instantiated.
//
// FINAL_RESULT read combinationally through a continuous assign. Section 0's
// last paragraph names this exact thing: "another large asynchronous table,
// call it a transaction file, and declare the architecture complete."
// Expect V3-ASYNC.
module fx_async_bank (
    input  var logic        clk,
    input  var logic        wr_en,
    input  var logic [5:0]  wr_addr,
    input  var logic [39:0] wr_data,
    input  var logic [5:0]  rd_addr,
    output var logic [39:0] rd_data
);
  // V3-BANK: FINAL_RESULT
  (* ramstyle = "M10K" *) logic [39:0] final_result_m [0:63];

  always_ff @(posedge clk) begin
    if (wr_en) final_result_m[wr_addr] <= wr_data;
  end

  assign rd_data = final_result_m[rd_addr];
endmodule
