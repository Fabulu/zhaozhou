// FIXTURE -- DELIBERATELY WRONG. Not built, not fitted, never instantiated.
//
// RCP_RESULT read with an adder between the array output and the first flop.
// QUARTUS_GOTCHAS section 14 / spec section 22.1: this is the shape that turns
// a declared M10K into flip-flops. It is the actual pathology being fixed, so a
// tool that cannot see it is not doing its job.
// Expect V3-COMBLOGIC.
module fx_logic_before_register (
    input  var logic        clk,
    input  var logic        wr_en,
    input  var logic [5:0]  wr_addr,
    input  var logic [31:0] wr_data,
    input  var logic [5:0]  rd_addr,
    input  var logic [31:0] bias,
    output var logic [31:0] ram_q
);
  // V3-BANK: RCP_RESULT
  (* ramstyle = "M10K" *) logic [31:0] rcp_result_m [0:63];

  always_ff @(posedge clk) begin
    if (wr_en) rcp_result_m[wr_addr] <= wr_data;
    ram_q <= rcp_result_m[rd_addr] + bias;
  end
endmodule
