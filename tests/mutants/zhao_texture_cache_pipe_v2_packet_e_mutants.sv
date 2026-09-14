// zhao_texture_cache_pipe_v2_packet_e_mutants.sv
//
// COMMITTED, DELIBERATELY BROKEN Packet-E cache selectors. NOT SHIPPED.
// This file declares no module and is absent from production source lists. A
// private inverse build compiles it immediately before the exact production
// cache and defines exactly one selector. No RTL is copied here.
`default_nettype none

// Selector cardinality is part of the instrument. A build that asks for both
// defects is ambiguous and must be rejected by the exact cache source.
`ifdef ZHAO_PACKET_E_MUTANT_DENIAL_REPLAYS
  `ifdef ZHAO_PACKET_E_MUTANT_PREPAID_DOUBLE_RESV
    `define ZHAO_PACKET_E_MUTANT_SELECTOR_COLLISION
  `endif
`endif

// Denial head-skip failure: after emitting the refused head, issue rewinds to
// that denied slot rather than advancing to the younger request. The inverse
// driver requires exactly a second fill offer for the denied address, with the
// refusal response and every counter otherwise exact.
`ifdef ZHAO_PACKET_E_MUTANT_DENIAL_REPLAYS
  `define ZHAO_PACKET_E_REFUSAL_NEXT_IP(rp, one) (rp)
`endif

// Blocking-credit double reservation: successful fill still transfers its
// reservation into a prepaid replay, but that replay also increments rs_resv.
// The private inverse run disables immediate assertions only so it can observe
// the shipped structural symptom: all payload/counters drain, yet idle remains
// low solely because one response reservation leaked.
`ifdef ZHAO_PACKET_E_MUTANT_PREPAID_DOUBLE_RESV
  `define ZHAO_PACKET_E_ISSUE_OWNS_FRESH_RESV(prepaid) 1'b1
`endif

`default_nettype wire
