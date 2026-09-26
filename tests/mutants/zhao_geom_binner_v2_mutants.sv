// zhao_geom_binner_v2_mutants.sv -- COMMITTED, DELIBERATELY BROKEN control.
//
// This file declares no module and is absent from production source lists. A
// private inverse build compiles it immediately before the exact V2 RTL. The
// selector changes only the metadata RAM read address; old/V2 structure remains
// the real implementation and must stay cycle/byte identical.
`default_nettype none

// Swap each adjacent triangle-store record (0<->1, 2<->3, ...). The dedicated
// accepted-A / denied-X / accepted-B witness stores A at 0 and B at 1, so the
// inverse lane must observe exactly A/B metadata swapped while source IDs,
// vertices, tile order, first/last, handshakes, and counters remain identical.
`ifdef ZHAO_GEOM_BINNER_V2_MUTANT_META_ADDR_SWAP
  `define ZHAO_GEOM_BINNER_V2_META_RA(addr) \
      ((addr) ^ {{(TRI_W-1){1'b0}}, 1'b1})
`endif

// ---------------------------------------------------------------------------
// CNT_W WRAP -- the state BEFORE GIANTREFS derived the per-tile count width.
// ---------------------------------------------------------------------------
// WHY THIS IS A COMMITTED MUTANT AND NOT A TEST.
//
// The derivation `CNT_W = $clog2(CHUNKS*CHUNK_REFS + 1)` makes a per-tile
// reference count that WRAPS structurally impossible, and a state that cannot
// be reached is a state no legal stimulus can demonstrate. "The count cannot
// wrap" therefore stays an ARGUMENT forever unless the derivation is broken on
// purpose and the wrap is watched to happen -- CLAUDE.md, "A guard you cannot
// reach with legal stimulus needs a COMMITTED MUTANT".
//
// WHAT IS CHANGED: one substantive line. `CNT_W` goes back to the literal 11
// that shipped until 2026-09-26, hand-sized to the DEFAULT arena
// (CHUNKS=256 x CHUNK_REFS=4 = 1,024 references, which 11 bits holds) and tied
// to nothing. Every other bit of the block is the production RTL, compiled
// from the production file.
//
// WHAT IT PROVES, and note that it is NOT a fault the overflow wall can see.
// At a parameterisation whose per-tile count can exceed 2,047 the count wraps
// to zero; `cur_count == 0` then makes the next push re-seed the tile's list
// HEAD, orphaning every reference already chained there. The drain reads the
// wrapped count and emits a fraction of the references that were pushed, and
// `overflow_o` stays LOW throughout, because the chunk arena never ran out and
// the triangle store never filled. That is a silent corruption wearing the
// clothes of a healthy frame -- the exact shape CLAUDE.md's broken-instrument
// law says nobody audits.
//
// DRIVER POLARITY IS INVERTED: `geom_binner_v2_cntw_wrap.cpp` built with
// EXPECT_GEOM_BINNER_V2_CNTW_WRAP=1 PASSES when the wrap FIRES, and the same
// source built without it PASSES only when every pushed reference survives.
// The second form is the assertion about the SHIPPED design and it asserts the
// CORRECT behaviour, never the bug.
`ifdef ZHAO_GEOM_BINNER_V2_MUTANT_CNT_W_HARDCODED
  `define ZHAO_GEOM_BINNER_V2_CNT_W 11
  // The production elaboration guard `CNT_W >= $clog2(REF_CAP+1)` catches this
  // mutation and $fatals at time 0, before the drain -- the thing being
  // measured -- can run. Disabled HERE ONLY, per CLAUDE.md: "when a mutant
  // trips a SIMULATION assertion before the synthesizable counter can be read,
  // disable the assertion in the mutant only, with the reason beside it."
  // That it fires at all is independent corroboration of the derivation.
  `define ZHAO_GEOM_BINNER_V2_NO_CNTW_GUARD
`endif

`default_nettype wire
