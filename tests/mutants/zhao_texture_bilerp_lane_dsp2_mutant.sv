// COMMITTED TEST MUTANTS -- BIL2 packing and vertical-capture controls.
//
// This file declares no module. A private inverse build compiles it before the
// candidate and defines exactly one selector; production never sees it.
`default_nettype none

`ifdef ZHAO_BIL2_MUTANT_COLLAPSE_RESULTB
  `ifdef ZHAO_BIL2_MUTANT_BYPASS_VERTICAL_CAPTURE
    `error "ZHAO_BIL2_MUTANT_SELECTOR_COLLISION"
  `endif
  // Wrong: both horizontal lanes consume the first packed multiplier result.
  `define ZHAO_BIL2_RESULTB(resulta, resultb) resulta
`elsif ZHAO_BIL2_MUTANT_BYPASS_VERTICAL_CAPTURE
  // Wrong: B2 valid/token still describe the held terminal job, but its final
  // arithmetic follows the live B1 base/product. Back-to-back traffic or an
  // output stall therefore exposes a one-job identity swap.
  `define ZHAO_BIL2_FINISH_A(held, live) live
  `define ZHAO_BIL2_FINISH_PV(held, live) live
`endif

`default_nettype wire
