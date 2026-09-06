// FIXTURE -- DELIBERATELY WRONG. Not built, not fitted, never instantiated.
//
// SAMPLE_METADATA declared as 64 x 4 rows instead of one flat 256-row plane.
// Section 6.1 says the 256x40 shape matches a native simple-dual-port M10K
// geometry and the sample_index-3 rows are intentionally unused. D19m is the
// concrete cost of getting this wrong: zhao_texture_tmu_pipe's two-axis arrays
// synthesised to 72,824 registers against 256 block-memory bits.
// Expect V3-MULTIDIM.
module fx_multidim_bank (
    input  var logic        clk,
    input  var logic        wr_en,
    input  var logic [5:0]  wr_slot,
    input  var logic [1:0]  wr_idx,
    input  var logic [39:0] wr_data,
    input  var logic [5:0]  rd_slot,
    input  var logic [1:0]  rd_idx,
    output var logic [39:0] ram_q
);
  // V3-BANK: SAMPLE_METADATA
  (* ramstyle = "M10K" *) logic [39:0] sample_metadata_m [0:63][0:3];

  always_ff @(posedge clk) begin
    if (wr_en) sample_metadata_m[wr_slot][wr_idx] <= wr_data;
    ram_q <= sample_metadata_m[rd_slot][rd_idx];
  end
endmodule
