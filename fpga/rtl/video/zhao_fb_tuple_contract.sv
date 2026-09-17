// zhao_fb_tuple_contract.sv -- the elaboration check for the 84-bit tuple.
//
// Its own file because Verilator's DECLFILENAME is right: a module that does
// not share its file's name is one a reader cannot find. It is a module rather
// than prose so that a wrong width is a COMPILE failure and not a test failure.
//
// DO NOT PUT THIS IN THE PER-BLOCK MAP LANE. It was tried on 2026-09-17 and
// came back `failed:analysis` with
//
//   Error (12061): Can't synthesize current design -- Top partition does not
//                  contain any logic
//
// which is correct and is not a defect: a per-block map characterizes area, DSP
// and RAM inference, and a module with no circuit in it has none of those. That
// is a measurement which does not exist, not one that came out badly, and the
// row was removed from reports/synthesis/zhao_block_map.json because left in
// place it reads as "this block is broken".
//
// The question a map WOULD have answered -- does Quartus 17.0.2's parser accept
// this, given that a clean Verilator lint settles one tool's opinion and nothing
// else -- was answered instead by mapping it inside a wrapper that does contain
// logic: Analysis & Synthesis successful, 0 errors. So the `initial begin`/
// `$fatal` form, `int unsigned` inside it, and the `+:` part-selects are all
// accepted.
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
