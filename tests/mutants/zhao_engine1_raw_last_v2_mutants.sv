// zhao_engine1_raw_last_v2_mutants.sv -- committed inverse controls.
//
// This selector shim is deliberately compiled immediately before the exact
// production leaf.  It is never part of a production source closure.  Define
// exactly one selector; the production leaf turns a collision into an
// elaboration failure rather than allowing an inverse test to become ambiguous.
//
// The four mutations are intentionally narrow and have distinct diagnostics:
//   EARLY_LAST  : LAST is one raw beat early, with the state machine unchanged.
//   MISSING_LAST: no physical LAST is published, although retirement is normal.
//   LATE_LAST   : the terminal edge is one raw beat late so the extra word
//                 carries the independently wrong LAST.
//   STATE_INVALID: force only the detector's independent state view to 2'b11.
`default_nettype none

`ifdef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_EARLY_LAST
  `ifdef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTED
    `define ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTED
    `define ZHAO_ENGINE1_RAW_LAST_V2_LAST_EXPR(raw_valid, terminal, next_count, expected_count) \
      ((raw_valid) && ((next_count) == ((expected_count) - 6'd1)))
  `endif
`endif

`ifdef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_MISSING_LAST
  `ifdef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTED
    `define ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTED
    `define ZHAO_ENGINE1_RAW_LAST_V2_LAST_EXPR(raw_valid, terminal, next_count, expected_count) \
      1'b0
  `endif
`endif

`ifdef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_LATE_LAST
  `ifdef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTED
    `define ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTED
    // The production terminal edge is also the state transition.  Move both
    // together so this control produces exactly one late LAST, not a protocol
    // fault before that LAST can be observed.
    `define ZHAO_ENGINE1_RAW_LAST_V2_TERMINAL_EXPR(next_count, expected_count) \
      ((next_count) == ((expected_count) + 6'd1))
  `endif
`endif

`ifdef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_STATE_INVALID
  `ifdef ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTED
    `define ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTED
    // Independent detector view only: production state remains legal, while the
    // fail-stop detector sees the otherwise-unreachable 2'b11 encoding.
    `define ZHAO_ENGINE1_RAW_LAST_V2_STATE_EXPR(state_value) 2'b11
  `endif
`endif

`default_nettype wire
