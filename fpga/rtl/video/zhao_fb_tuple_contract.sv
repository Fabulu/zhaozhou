// zhao_fb_tuple_contract.sv -- the elaboration check for the 84-bit tuple.
//
// Its own file because Verilator's DECLFILENAME is right: a module that does
// not share its file's name is one a reader cannot find. It is a module rather
// than prose so that a wrong width is a COMPILE failure and not a test failure.
`default_nettype none

// THE TILING CHECK, and why it is a module rather than prose. The fields must
// cover the 84 bits exactly: no gap (a bit nobody writes reads as an
// unpredictable value the CDC will faithfully carry) and no overlap (two fields
// silently corrupting each other, which is the worst case because the result
// still looks like data).
//
// It is an elaboration check, so a wrong width is a compile failure rather than
// a test failure. Note `initial begin ... end` and not a bare module-scope
// `if`: Quartus 17.0 rejects the bare form with a syntax error, which CLAUDE.md
// records and which a clean Verilator lint says nothing about.
module zhao_fb_tuple_contract;
  import zhao_fb_tuple_pkg::*;

  // A COVERAGE MASK, not an arithmetic argument. Summing the widths proves
  // nothing on its own -- six fields can sum to 84 while two of them sit on top
  // of each other and leave a hole somewhere else. Painting each field's bits
  // and demanding the result be all ones catches the hole; the widths summing
  // to 84 then leaves no room for an overlap. Both halves are needed and
  // neither is sufficient.
  logic [ZHAO_FB_TUPLE_W-1:0] covered_bits;
  int unsigned total;

  initial begin : p_fb_tuple_tiles_exactly
    covered_bits = '0;
    covered_bits[ZHAO_FB_WRITER_LO +: ZHAO_FB_WRITER_W] = '1;
    covered_bits[ZHAO_FB_SLOT_LO   +: ZHAO_FB_SLOT_W]   = '1;
    covered_bits[ZHAO_FB_GEN_LO    +: ZHAO_FB_GEN_W]    = '1;
    covered_bits[ZHAO_FB_MODE_LO   +: ZHAO_FB_MODE_W]   = '1;
    covered_bits[ZHAO_FB_BASE_LO   +: ZHAO_FB_BASE_W]   = '1;
    covered_bits[ZHAO_FB_SPAN_LO   +: ZHAO_FB_SPAN_W]   = '1;

    total = ZHAO_FB_WRITER_W + ZHAO_FB_SLOT_W + ZHAO_FB_GEN_W +
            ZHAO_FB_MODE_W + ZHAO_FB_BASE_W + ZHAO_FB_SPAN_W;

    if (total != ZHAO_FB_TUPLE_W)
      $fatal(1, "zhao_fb_tuple_pkg: field widths sum to %0d, tuple is %0d",
             total, ZHAO_FB_TUPLE_W);
    if (covered_bits !== {ZHAO_FB_TUPLE_W{1'b1}})
      $fatal(1, "zhao_fb_tuple_pkg: fields leave a hole -- coverage %b", covered_bits);
  end
endmodule : zhao_fb_tuple_contract

`default_nettype wire
