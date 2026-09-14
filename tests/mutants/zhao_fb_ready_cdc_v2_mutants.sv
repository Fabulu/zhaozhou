// zhao_fb_ready_cdc_v2_mutants.sv
//
// COMMITTED, DELIBERATELY BROKEN Packet-G selector shim. NOT SHIPPED. Compile
// immediately before the exact V2 CDC and define exactly one selector.
`default_nettype none

`ifdef ZHAO_FB_CDC_MUTANT_IGNORE_FULL
  `ifdef ZHAO_FB_CDC_MUTANT_SELECTED
    `define ZHAO_FB_CDC_MUTANT_COLLISION
  `else
    `define ZHAO_FB_CDC_MUTANT_SELECTED
    `define ZHAO_FB_CDC_FULL(full) 1'b0
  `endif
`endif

`ifdef ZHAO_FB_CDC_MUTANT_BYPASS_BARRIER
  `ifdef ZHAO_FB_CDC_MUTANT_SELECTED
    `define ZHAO_FB_CDC_MUTANT_COLLISION
  `else
    `define ZHAO_FB_CDC_MUTANT_SELECTED
    `define ZHAO_FB_CDC_BARRIER(done) 1'b1
  `endif
`endif

`ifdef ZHAO_FB_CDC_MUTANT_ZERO_READY_GENERATION
  `ifdef ZHAO_FB_CDC_MUTANT_SELECTED
    `define ZHAO_FB_CDC_MUTANT_COLLISION
  `else
    `define ZHAO_FB_CDC_MUTANT_SELECTED
    `define ZHAO_FB_CDC_READY_TUPLE(tuple) \
      {tuple[83:82], 16'd0, tuple[65:0]}
  `endif
`endif

`ifdef ZHAO_FB_CDC_MUTANT_REPEAT_READ
  `ifdef ZHAO_FB_CDC_MUTANT_SELECTED
    `define ZHAO_FB_CDC_MUTANT_COLLISION
  `else
    `define ZHAO_FB_CDC_MUTANT_SELECTED
    `define ZHAO_FB_CDC_RD_NEXT(next_value) ((next_value) - PTR_W'(1))
  `endif
`endif

`ifdef ZHAO_FB_CDC_MUTANT_SELECTED
  `undef ZHAO_FB_CDC_MUTANT_SELECTED
`endif
`default_nettype wire
