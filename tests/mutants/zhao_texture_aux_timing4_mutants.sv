// zhao_texture_aux_timing4_mutants.sv -- Timing4 AUX boundary controls.
//
// COMMITTED, DELIBERATELY BROKEN. Compile immediately before the production AUX
// leaf selected by exactly one macro. This file is never a production source.
`default_nettype none

`ifdef ZHAO_AUX_T4_MUTANT_DEGENERATE_DROP
  `ifdef ZHAO_AUX_T4_MUTANT_SELECTED
    `define ZHAO_AUX_T4_MUTANT_COLLISION
  `else
    `define ZHAO_AUX_T4_MUTANT_SELECTED
    // Wrong: the registered A0 degenerate fact is discarded.
    `define ZHAO_AUX_T4_DEGENERATE_CAPTURE(value) 1'b0
  `endif
`endif

`ifdef ZHAO_AUX_T4_MUTANT_INPUT_FAULT_DROP
  `ifdef ZHAO_AUX_T4_MUTANT_SELECTED
    `define ZHAO_AUX_T4_MUTANT_COLLISION
  `else
    `define ZHAO_AUX_T4_MUTANT_SELECTED
    // Wrong: accepted malformed input never reaches the registered fault fact.
    `define ZHAO_AUX_T4_INPUT_FAULT_CAPTURE(value) 1'b0
  `endif
`endif

`ifdef ZHAO_AUX_T4_MUTANT_BORROW_REVERSE
  `ifdef ZHAO_AUX_T4_MUTANT_SELECTED
    `define ZHAO_AUX_T4_MUTANT_COLLISION
  `else
    `define ZHAO_AUX_T4_MUTANT_SELECTED
    // Wrong: subtraction underflow is interpreted as a restoring-step hit.
    `define ZHAO_AUX_T4_DIV_HIT(difference) (difference[REM_W])
  `endif
`endif

`ifdef ZHAO_AUX_T4_MUTANT_COLLISION
  `error "ZHAO_AUX_TIMING4_MUTANT_SELECTOR_COLLISION"
`endif

`ifdef ZHAO_AUX_T4_MUTANT_SELECTED
  `undef ZHAO_AUX_T4_MUTANT_SELECTED
`endif
`default_nettype wire
