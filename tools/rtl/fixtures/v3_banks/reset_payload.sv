// FIXTURE -- DELIBERATELY WRONG. Not built, not fitted, never instantiated.
//
// OWNER_CONTEXT payload swept by an asynchronous reset. LAW 08: "Wide
// persistent arrays have clock-only payload processes and synchronous read
// outputs. Valid/control state is reset separately." Section 6.5 says the same
// and section 25.5 rejects "resetting every payload to remove unknown values".
// Expect V3-RESETPAYLOAD.
module fx_reset_payload (
    input  var logic        clk,
    input  var logic        rst_n,
    input  var logic        wr_en,
    input  var logic [5:0]  wr_addr,
    input  var logic [63:0] wr_data,
    input  var logic [5:0]  rd_addr,
    output var logic [63:0] ram_q
);
  // V3-BANK: OWNER_CONTEXT
  (* ramstyle = "M10K" *) logic [63:0] owner_context_m [0:63];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int i = 0; i < 64; i++) owner_context_m[i] <= 64'd0;
    end else begin
      if (wr_en) owner_context_m[wr_addr] <= wr_data;
      ram_q <= owner_context_m[rd_addr];
    end
  end
endmodule
