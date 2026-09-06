// FIXTURE -- DELIBERATELY WRONG. Not built, not fitted, never instantiated.
//
// A correct AUX_GEOMETRY bank, plus a 64x80 shadow copy of the same payload
// living in fabric so a second consumer can read it whenever it likes. That is
// precisely section 6.6's "A replicated 64-entry payload table because three
// modules want asynchronous access ... must not happen accidentally", and
// section 21.8's "supposedly banked 64x80 payload as thousands of registers".
// Expect V3-FABRIC on shadow_desc_r.
module fx_fabric_payload (
    input  var logic        clk,
    input  var logic        wr_en,
    input  var logic [5:0]  wr_addr,
    input  var logic [79:0] wr_data,
    input  var logic [5:0]  rd_addr,
    input  var logic [5:0]  peek_addr,
    output var logic [79:0] ram_q,
    output var logic [79:0] peek_q
);
  // V3-BANK: AUX_GEOMETRY
  (* ramstyle = "M10K" *) logic [79:0] aux_geometry_m [0:63];

  logic [79:0] shadow_desc_r [0:63];

  always_ff @(posedge clk) begin
    if (wr_en) aux_geometry_m[wr_addr] <= wr_data;
    ram_q <= aux_geometry_m[rd_addr];
  end

  always_ff @(posedge clk) begin
    if (wr_en) shadow_desc_r[wr_addr] <= wr_data;
    peek_q <= shadow_desc_r[peek_addr];
  end
endmodule
