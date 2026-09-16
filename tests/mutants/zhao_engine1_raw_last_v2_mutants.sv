// zhao_engine1_raw_last_v2_mutants.sv -- committed inverse controls.
//
// This selector shim is deliberately compiled immediately before the exact
// production leaf.  It is never part of a production source closure.  Define
// exactly one selector; the production leaf turns a collision into an
// elaboration failure rather than allowing an inverse test to become ambiguous.
//
// The four mutations remain intentionally narrow, but their controls now run
// against the independent controller-retirement framer:
//   EARLY_LAST  : publish LAST on raw beat E-1; the fixed publication checker
//                 must fire on that exact beat.
//   MISSING_LAST: suppress LAST on the validated FINAL_HELD tuple; its fixed
//                 publication checker must fire before the tuple can retire.
//   LATE_LAST   : move only the action terminal comparison to E+1; the fixed
//                 exact-raw/controller detector must fire at physical E.
//   STATE_INVALID: force only the detector's independent state view to 2'b11.
//
// The target/macro names stay stable so the existing four executable CMake
// targets remain useful; the directed driver records the semantic migration.
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
    // Move only the action-side terminal view.  The fixed exact raw comparison
    // and independent controller-retirement comparison remain at E, so their
    // disagreement fires the intended detector at physical terminal retirement.
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
