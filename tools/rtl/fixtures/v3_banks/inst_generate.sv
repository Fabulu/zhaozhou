// FIXTURE -- NEGATIVE CONTROL for the multi-bank marker. Not built, never used.
//
// SAMPLE_RESULT_0/1/2 are three separate 64 x 40 stores with three separate
// writers (section 6: "TMU commit bank 0/1/2"), and the natural RTL for that is
// ONE generate loop over three genvar values -- which is how
// zhao_texture_v3own writes it. One source line, three physical banks.
//
// A marker that could only name one bank would make that unexpressible, so the
// marker takes a list. Expect exit 0 and all three banks accounted for.
module fx_inst_generate (
    input  var logic        clk,
    input  var logic [2:0]  we,
    input  var logic [5:0]  wr_addr,
    input  var logic [39:0] wr_data,
    input  var logic [5:0]  rd_addr
);
  logic [39:0] rd [3];

  generate
    genvar gs;
    for (gs = 0; gs < 3; gs++) begin : g_sres
      // V3-BANK: SAMPLE_RESULT_0, SAMPLE_RESULT_1, SAMPLE_RESULT_2
      zhao_texture_v3bank #(.WIDTH(40), .DEPTH(64)) u_sres (
          .clk      (clk),
          .wr_en_i  (we[gs]),
          .wr_addr_i(wr_addr),
          .wr_data_i(wr_data),
          .rd_addr_i(rd_addr),
          .rd_data_o(rd[gs])
      );
    end
  endgenerate
endmodule
