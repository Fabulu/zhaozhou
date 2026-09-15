// zhao_attr_dsp3_mutants.sv -- selector-only ATTR3 arithmetic controls.
//
// Declares no module. A private build compiles this before the exact candidate
// sources and selects exactly one deliberate arithmetic defect.
`default_nettype none

`ifdef ZHAO_ATTR_DSP3_MUTANT_SIGNED_MIDDLE
  `ifdef ZHAO_ATTR_DSP3_MUTANT_ACTIVE
    `error "ZHAO_ATTR_DSP3_MUTANT_SELECTOR_COLLISION"
  `endif
  `define ZHAO_ATTR_DSP3_MUTANT_ACTIVE
  `define ZHAO_ATTR_DSP3_MUTANT_DISABLE_ASSERTIONS
  `define ZHAO_ATTR_DSP3_MIDDLE_EXT(value) \
      $signed({{3{value[47]}}, value[47:24]})
`endif

`ifdef ZHAO_ATTR_DSP3_MUTANT_NARROW_HIGH_SHIFT
  `ifdef ZHAO_ATTR_DSP3_MUTANT_ACTIVE
    `error "ZHAO_ATTR_DSP3_MUTANT_SELECTOR_COLLISION"
  `endif
  `define ZHAO_ATTR_DSP3_MUTANT_ACTIVE
  `define ZHAO_ATTR_DSP3_MUTANT_DISABLE_ASSERTIONS
  `define ZHAO_ATTR_DSP3_RECOMBINE(s01, high) \
      ($signed({{23{s01[61]}}, s01}) + \
       $signed({{48{high[36]}}, (high <<< 48)}))
`endif

`ifdef ZHAO_ATTR_DSP3_MUTANT_OMIT_BASE_Y
  `ifdef ZHAO_ATTR_DSP3_MUTANT_ACTIVE
    `error "ZHAO_ATTR_DSP3_MUTANT_SELECTOR_COLLISION"
  `endif
  `define ZHAO_ATTR_DSP3_MUTANT_ACTIVE
  `define ZHAO_ATTR_DSP3_MUTANT_DISABLE_ASSERTIONS
  `define ZHAO_ATTR_DSP3_BASE_Y_RESULT(value) 96'sd0
`endif

`ifdef ZHAO_ATTR_DSP3_MUTANT_DELAY_OFFSET_READY
  `ifdef ZHAO_ATTR_DSP3_MUTANT_ACTIVE
    `error "ZHAO_ATTR_DSP3_MUTANT_SELECTOR_COLLISION"
  `endif
  `define ZHAO_ATTR_DSP3_MUTANT_ACTIVE
  `define ZHAO_ATTR_DSP3_MUTANT_DISABLE_LANE_ASSERTIONS
  `define ZHAO_ATTR_DSP3_OFFSET_READY_VALUE 1'b0
`endif

`ifdef ZHAO_ATTR_DSP3_MUTANT_IDLE_EARLY
  `ifdef ZHAO_ATTR_DSP3_MUTANT_ACTIVE
    `error "ZHAO_ATTR_DSP3_MUTANT_SELECTOR_COLLISION"
  `endif
  `define ZHAO_ATTR_DSP3_MUTANT_ACTIVE
  `define ZHAO_ATTR_DSP3_MUTANT_DISABLE_LANE_ASSERTIONS
  `define ZHAO_ATTR_DSP3_IDLE_EXPR(value) 1'b1
`endif

`undef ZHAO_ATTR_DSP3_MUTANT_ACTIVE
`default_nettype wire
