// zhao_texture_v3own_timing4_event_mutants.sv -- event-identity controls.
//
// Compile this shim immediately before the production owner. Each implemented
// selector corrupts only the generation bit captured at one Timing4 event
// boundary. The production assertion compares against an independently clocked
// source identity, so the selected mutant must fire before any stale slot can
// alter the scoreboard.
`default_nettype none

`ifdef ZHAO_V3OWN_T4_MUTANT_ADMISSION_STALE
  `ifdef ZHAO_V3OWN_T4_MUTANT_RESERVATION_STALE
    `error "ZHAO_V3OWN_T4_EVENT_MUTANT_SELECTOR_COLLISION"
  `endif
  `ifdef ZHAO_V3OWN_ADMISSION_EVENT_OWNER
    `error "ZHAO_V3OWN_ADMISSION_EVENT_OWNER already defined"
  `endif
  `define ZHAO_V3OWN_ADMISSION_EVENT_OWNER(owner) ((owner) ^ 14'h001)
`elsif ZHAO_V3OWN_T4_MUTANT_RESERVATION_STALE
  `ifdef ZHAO_V3OWN_RESERVATION_EVENT_OWNER
    `error "ZHAO_V3OWN_RESERVATION_EVENT_OWNER already defined"
  `endif
  `define ZHAO_V3OWN_RESERVATION_EVENT_OWNER(owner) ((owner) ^ 14'h001)
`endif

`default_nettype wire
