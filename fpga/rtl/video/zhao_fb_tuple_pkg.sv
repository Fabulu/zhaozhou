// zhao_fb_tuple_pkg.sv -- the 84-bit READY/swap tuple layout, defined once.
//
// WHY THIS FILE EXISTS. `zhao_fb_ready_cdc_v2` carries "one frozen 84-bit
// tuple" and is deliberately layout-agnostic: it moves 84 bits across a clock
// boundary and never looks inside. `zhao_video_slotmgr_v2` presents the same
// information as six separate ports, whose widths sum to exactly 84:
//
//     writer 1 + slot 1 + generation 16 + mode 2 + base 32 + span 32 = 84
//
// So the correspondence is certain and THE FIELD ORDER WAS DEFINED NOWHERE.
// Both the pack and the unpack belong to the sibling shell, both were
// unwritten, and `fb_ready_cdc_v2_directed.cpp` drives arbitrary values and
// checks only that what goes in comes out -- so nothing in the tree would have
// noticed the two sides disagreeing.
//
// WHAT A DISAGREEMENT LOOKS LIKE, which is the reason this is a package and not
// two bit ranges written eighty lines apart. A rotated layout does not produce
// an obvious fault. It produces a swap echo carrying a PLAUSIBLE slot and a
// PLAUSIBLE generation that simply are not the frame's; the manager refuses it
// as stale and is right to; the screen holds the previous frame; and every
// lifecycle counter in the machine balances, because no counter looks at the
// field that moved. That is this repository's own recurring shape.
//
// THE ORDER BELOW IS A DEFINITION, NOT A DISCOVERY. Nothing constrained it, so
// it is simply declared: fields ascend from bit 0 in the order
// `zhao_video_slotmgr_v2`'s port list declares them, which is the only ordering
// a reader can check against something. Its value is not that it is clever; it
// is that there is exactly one of it.
`default_nettype none

// The two lowest fields are macro-selectable so a committed mutant can SWAP
// them. That particular fault is the interesting one: it tiles the 84 bits
// perfectly, so the structural check below passes, and it round-trips
// perfectly, because pack and unpack read the same constants. Only a check
// against independently written bit positions can see it -- which is why the
// directed test carries literals rather than a round trip.
`ifndef ZHAO_FB_WRITER_LO_SEL
`define ZHAO_FB_WRITER_LO_SEL 0
`endif
`ifndef ZHAO_FB_SLOT_LO_SEL
`define ZHAO_FB_SLOT_LO_SEL 1
`endif

package zhao_fb_tuple_pkg;

  localparam int unsigned ZHAO_FB_TUPLE_W = 84;

  localparam int unsigned ZHAO_FB_WRITER_W = 1;
  localparam int unsigned ZHAO_FB_SLOT_W   = 1;
  localparam int unsigned ZHAO_FB_GEN_W    = 16;
  localparam int unsigned ZHAO_FB_MODE_W   = 2;
  localparam int unsigned ZHAO_FB_BASE_W   = 32;
  localparam int unsigned ZHAO_FB_SPAN_W   = 32;

  localparam int unsigned ZHAO_FB_WRITER_LO = `ZHAO_FB_WRITER_LO_SEL;
  localparam int unsigned ZHAO_FB_SLOT_LO   = `ZHAO_FB_SLOT_LO_SEL;
  localparam int unsigned ZHAO_FB_GEN_LO    = 2;
  localparam int unsigned ZHAO_FB_MODE_LO   = ZHAO_FB_GEN_LO    + ZHAO_FB_GEN_W;
  localparam int unsigned ZHAO_FB_BASE_LO   = ZHAO_FB_MODE_LO   + ZHAO_FB_MODE_W;
  localparam int unsigned ZHAO_FB_SPAN_LO   = ZHAO_FB_BASE_LO   + ZHAO_FB_BASE_W;

  function automatic logic [ZHAO_FB_TUPLE_W-1:0] zhao_fb_tuple_pack(
      input logic        writer,
      input logic        slot,
      input logic [15:0] generation,
      input logic [1:0]  mode,
      input logic [31:0] base,
      input logic [31:0] span);
    logic [ZHAO_FB_TUPLE_W-1:0] t;
    t = '0;
    t[ZHAO_FB_WRITER_LO +: ZHAO_FB_WRITER_W] = writer;
    t[ZHAO_FB_SLOT_LO   +: ZHAO_FB_SLOT_W]   = slot;
    t[ZHAO_FB_GEN_LO    +: ZHAO_FB_GEN_W]    = generation;
    t[ZHAO_FB_MODE_LO   +: ZHAO_FB_MODE_W]   = mode;
    t[ZHAO_FB_BASE_LO   +: ZHAO_FB_BASE_W]   = base;
    t[ZHAO_FB_SPAN_LO   +: ZHAO_FB_SPAN_W]   = span;
    return t;
  endfunction

  // Each accessor reads ONE slice of the tuple, so every one of them leaves
  // the other 68-to-83 bits unread. That is the definition of an accessor,
  // not an oversight, and it is the whole set or none.
  /* verilator lint_off UNUSEDSIGNAL */
  function automatic logic zhao_fb_tuple_writer(
      input logic [ZHAO_FB_TUPLE_W-1:0] t);
    return t[ZHAO_FB_WRITER_LO +: ZHAO_FB_WRITER_W];
  endfunction

  function automatic logic zhao_fb_tuple_slot(
      input logic [ZHAO_FB_TUPLE_W-1:0] t);
    return t[ZHAO_FB_SLOT_LO +: ZHAO_FB_SLOT_W];
  endfunction

  function automatic logic [15:0] zhao_fb_tuple_generation(
      input logic [ZHAO_FB_TUPLE_W-1:0] t);
    return t[ZHAO_FB_GEN_LO +: ZHAO_FB_GEN_W];
  endfunction

  function automatic logic [1:0] zhao_fb_tuple_mode(
      input logic [ZHAO_FB_TUPLE_W-1:0] t);
    return t[ZHAO_FB_MODE_LO +: ZHAO_FB_MODE_W];
  endfunction

  function automatic logic [31:0] zhao_fb_tuple_base(
      input logic [ZHAO_FB_TUPLE_W-1:0] t);
    return t[ZHAO_FB_BASE_LO +: ZHAO_FB_BASE_W];
  endfunction

  function automatic logic [31:0] zhao_fb_tuple_span(
      input logic [ZHAO_FB_TUPLE_W-1:0] t);
    return t[ZHAO_FB_SPAN_LO +: ZHAO_FB_SPAN_W];
  endfunction

  /* verilator lint_on UNUSEDSIGNAL */

endpackage : zhao_fb_tuple_pkg

`default_nettype wire
