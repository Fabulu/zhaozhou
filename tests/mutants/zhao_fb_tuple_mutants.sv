// zhao_fb_tuple_mutants.sv
//
// COMMITTED, DELIBERATELY BROKEN layout shim. NOT SHIPPED. Compile immediately
// before zhao_fb_tuple_pkg.sv and define exactly one selector.
//
// One mutant, and it is chosen for what it survives rather than for what it
// breaks. Swapping the writer and slot bits still tiles the 84 bits exactly, so
// the package's elaboration coverage check passes; and pack and unpack still
// agree with each other, because both read the swapped constants, so any round
// trip passes too. It is visible only to a check written against bit positions
// somewhere else.
`default_nettype none

`ifdef ZHAO_FB_TUPLE_MUTANT_SWAP_WRITER_SLOT
  `define ZHAO_FB_WRITER_LO_SEL 82
  `define ZHAO_FB_SLOT_LO_SEL 83
`endif

`default_nettype wire
