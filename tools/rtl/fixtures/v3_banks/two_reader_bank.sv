// FIXTURE -- DELIBERATELY WRONG. Not built, not fitted, never instantiated.
//
// MATERIAL read at two independent addresses in the same clock. Section 6.2:
// "ramstyle cannot give an M10K an extra port."
// Expect V3-MULTIREAD.
module fx_two_reader_bank (
    input  var logic        clk,
    input  var logic        wr_en,
    input  var logic [5:0]  wr_addr,
    input  var logic [47:0] wr_data,
    input  var logic [5:0]  addr_a,
    input  var logic [5:0]  addr_b,
    output var logic [47:0] ram_a,
    output var logic [47:0] ram_b
);
  // V3-BANK: MATERIAL
  (* ramstyle = "M10K" *) logic [47:0] material_m [0:63];

  always_ff @(posedge clk) begin
    if (wr_en) material_m[wr_addr] <= wr_data;
    ram_a <= material_m[addr_a];
    ram_b <= material_m[addr_b];
  end
endmodule
