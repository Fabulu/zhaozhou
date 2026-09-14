// zhao_video_slotmgr_v2_mutants.sv
//
// COMMITTED, DELIBERATELY BROKEN Packet-G selector shim. NOT SHIPPED. Compile
// immediately before the exact V2 slot manager and define exactly one selector.
`default_nettype none

`ifdef ZHAO_SLOT_V2_MUTANT_TERM_OMIT_WRITER
  `ifdef ZHAO_SLOT_V2_MUTANT_SELECTED
    `define ZHAO_SLOT_V2_MUTANT_COLLISION
  `else
    `define ZHAO_SLOT_V2_MUTANT_SELECTED
    `define ZHAO_SLOT_V2_TERM_MATCH(writer, slot, generation, lease_writer, lease_slot, lease_generation) \
      (((slot) == (lease_slot)) && ((generation) == (lease_generation)))
  `endif
`endif

`ifdef ZHAO_SLOT_V2_MUTANT_TERM_SLOT_ONLY
  `ifdef ZHAO_SLOT_V2_MUTANT_SELECTED
    `define ZHAO_SLOT_V2_MUTANT_COLLISION
  `else
    `define ZHAO_SLOT_V2_MUTANT_SELECTED
    `define ZHAO_SLOT_V2_TERM_MATCH(writer, slot, generation, lease_writer, lease_slot, lease_generation) \
      ((slot) == (lease_slot))
  `endif
`endif

`ifdef ZHAO_SLOT_V2_MUTANT_FAULT_PUBLISHES
  `ifdef ZHAO_SLOT_V2_MUTANT_SELECTED
    `define ZHAO_SLOT_V2_MUTANT_COLLISION
  `else
    `define ZHAO_SLOT_V2_MUTANT_SELECTED
    `define ZHAO_SLOT_V2_READY_FROM_CLEAN(clean) (term_match_c && term_publish_i)
  `endif
`endif

`ifdef ZHAO_SLOT_V2_MUTANT_SLOT0_BASE
  `ifdef ZHAO_SLOT_V2_MUTANT_SELECTED
    `define ZHAO_SLOT_V2_MUTANT_COLLISION
  `else
    `define ZHAO_SLOT_V2_MUTANT_SELECTED
    `define ZHAO_SLOT_V2_DERIVED_BASE(slot) ZHAO_FB_SLOT0_BASE
  `endif
`endif

`ifdef ZHAO_SLOT_V2_MUTANT_SWAP_OMIT_GENERATION
  `ifdef ZHAO_SLOT_V2_MUTANT_SELECTED
    `define ZHAO_SLOT_V2_MUTANT_COLLISION
  `else
    `define ZHAO_SLOT_V2_MUTANT_SELECTED
    `define ZHAO_SLOT_V2_SWAP_MATCH(writer, slot, generation, mode, base, span, stored_writer, stored_generation, stored_mode, stored_base, stored_span) \
      (((writer) == (stored_writer)) && ((mode) == (stored_mode)) && \
       ((base) == (stored_base)) && ((span) == (stored_span)))
  `endif
`endif

`ifdef ZHAO_SLOT_V2_MUTANT_LIVE_READY_WRITER
  `ifdef ZHAO_SLOT_V2_MUTANT_SELECTED
    `define ZHAO_SLOT_V2_MUTANT_COLLISION
  `else
    `define ZHAO_SLOT_V2_MUTANT_SELECTED
    `define ZHAO_SLOT_V2_READY_WRITER(captured) render_req_valid_i
  `endif
`endif

`ifdef ZHAO_SLOT_V2_MUTANT_SELECTED
  `undef ZHAO_SLOT_V2_MUTANT_SELECTED
`endif
`default_nettype wire
