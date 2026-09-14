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

`default_nettype wire
