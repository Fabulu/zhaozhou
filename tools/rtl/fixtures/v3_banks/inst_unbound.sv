// FIXTURE -- DELIBERATELY WRONG. Not built, never used.
//
// A bank primitive instantiated as anonymous scratch: it is a real M10K in the
// fit report and appears in no inventory. Section 21.4's rule is "honest
// counting", and section 21.8 says to stop before a long fit when there is a
// forgotten service store outside the budget.
// Expect V3-UNBOUND.
module fx_inst_unbound (
    input  var logic        clk,
    input  var logic        wr_en,
    input  var logic [5:0]  wr_addr,
    input  var logic [39:0] wr_data,
    input  var logic [5:0]  rd_addr,
    output var logic [39:0] rd_data
);
  zhao_texture_v3bank #(.WIDTH(40), .DEPTH(64)) u_scratch (
      .clk      (clk),
      .wr_en_i  (wr_en),
      .wr_addr_i(wr_addr),
      .wr_data_i(wr_data),
      .rd_addr_i(rd_addr),
      .rd_data_o(rd_data)
  );
endmodule
