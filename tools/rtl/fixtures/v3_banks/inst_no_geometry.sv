// FIXTURE -- DELIBERATELY UNPARSEABLE. Not built, never used.
//
// A bank instance with no parameter overrides, in a file that does not contain
// the primitive, so its defaults cannot be read. The gate must EXIT 2 rather
// than assume 40 x 64 and print a reassuring pass. "A guessed width would be a
// measurement that decides a value, which this repository does not do."
module fx_inst_no_geometry (
    input  var logic        clk,
    input  var logic        wr_en,
    input  var logic [5:0]  wr_addr,
    input  var logic [39:0] wr_data,
    input  var logic [5:0]  rd_addr,
    output var logic [39:0] rd_data
);
  // V3-BANK: FINAL_RESULT
  zhao_texture_v3bank u_final_result (
      .clk      (clk),
      .wr_en_i  (wr_en),
      .wr_addr_i(wr_addr),
      .wr_data_i(wr_data),
      .rd_addr_i(rd_addr),
      .rd_data_o(rd_data)
  );
endmodule
