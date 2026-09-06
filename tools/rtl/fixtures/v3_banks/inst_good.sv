// FIXTURE -- NEGATIVE CONTROL for the INSTANCE path. Not built, never used.
//
// The V3 lane builds each section 6 bank by instantiating one parameterised
// primitive rather than declaring an array per bank. Identity therefore lives
// at the instantiation, and this is what a correct one looks like: the marker
// names the bank, and .WIDTH/.DEPTH match section 6's 64 x 80.
module fx_inst_good (
    input  var logic        clk,
    input  var logic        wr_en,
    input  var logic [5:0]  wr_addr,
    input  var logic [79:0] wr_data,
    input  var logic [5:0]  rd_addr,
    output var logic [79:0] rd_data
);
  // V3-BANK: SAMPLE_DESC_0
  zhao_texture_v3bank #(.WIDTH(80), .DEPTH(64)) u_sample_desc_0 (
      .clk      (clk),
      .wr_en_i  (wr_en),
      .wr_addr_i(wr_addr),
      .wr_data_i(wr_data),
      .rd_addr_i(rd_addr),
      .rd_data_o(rd_data)
  );
endmodule
