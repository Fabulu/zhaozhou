// FIXTURE -- DELIBERATELY WRONG. Not built, not fitted, never instantiated.
//
// THE D19m SHAPE, EXACTLY. Sixteen bits wide and 4,096 rows deep: the single
// largest payload plane in the old island, most of `zhao_texture_tmu_pipe`'s
// 72,824 registers, and INVISIBLE to a width>=32 rule. The first version of
// this gate's fabric check had only that rule and sailed straight past it.
//
// It is here so the miss cannot come back, and because the array this gate's
// own header cites as its reason for existing must be one the gate can see.
// Expect V3-FABRIC.
module fx_narrow_huge_payload (
    input  var logic         clk,
    input  var logic         wr_en,
    input  var logic [3:0]   slot,
    input  var logic [7:0]   idx,
    input  var logic [15:0]  wr_data,
    output var logic [15:0]  rd_q
);
  logic [15:0] pal_dat_r [16][256];

  always_ff @(posedge clk) begin
    if (wr_en) pal_dat_r[slot][idx] <= wr_data;
    rd_q <= pal_dat_r[slot][idx];
  end
endmodule
