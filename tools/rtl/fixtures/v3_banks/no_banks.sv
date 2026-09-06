// FIXTURE -- a V3 file that declares no transaction-file bank at all.
// Not built, not fitted, never instantiated.
//
// Two roles. Plainly it must not invent findings: small control state is
// explicitly fabric-legal (section 21.4, and section 21.5's 29-bit-per-owner
// scoreboard is budgeted in flops on purpose), so this file is CLEAN by
// default. Under --require-all -- the section 26.2 acceptance form of the gate
// -- every one of the 13 declared banks must come back as V3-MISSING.
module fx_no_banks (
    input  var logic        clk,
    input  var logic        rst_n,
    input  var logic        set_en,
    input  var logic [5:0]  set_slot,
    output var logic [63:0] live_q
);
  // Legal fabric: narrow per-owner control, well under the width floor.
  logic [28:0] owner_ctrl_r [0:63];
  logic [63:0] live_r;

  always_ff @(posedge clk) begin
    if (set_en) owner_ctrl_r[set_slot] <= 29'd1;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) live_r <= 64'd0;
    else if (set_en) live_r[set_slot] <= 1'b1;
  end

  assign live_q = live_r;
endmodule
