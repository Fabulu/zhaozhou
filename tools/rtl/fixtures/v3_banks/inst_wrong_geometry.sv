// FIXTURE -- DELIBERATELY WRONG. Not built, never used.
//
// SAMPLE_RESULT_2 instantiated 32 wide and 32 deep where section 6 declares
// 64 x 40. A result word is RGBA32 plus STATUS8; instantiating 32 silently
// deletes the status plane, which section 25.7 rejects, and the number is in a
// parameter override where nobody reads it.
// Expect V3-WIDTH and V3-DEPTH.
module fx_inst_wrong_geometry (
    input  var logic        clk,
    input  var logic        wr_en,
    input  var logic [4:0]  wr_addr,
    input  var logic [31:0] wr_data,
    input  var logic [4:0]  rd_addr,
    output var logic [31:0] rd_data
);
  // V3-BANK: SAMPLE_RESULT_2
  zhao_texture_v3bank #(.WIDTH(32), .DEPTH(32)) u_sample_result_2 (
      .clk      (clk),
      .wr_en_i  (wr_en),
      .wr_addr_i(wr_addr),
      .wr_data_i(wr_data),
      .rd_addr_i(rd_addr),
      .rd_data_o(rd_data)
  );
endmodule
