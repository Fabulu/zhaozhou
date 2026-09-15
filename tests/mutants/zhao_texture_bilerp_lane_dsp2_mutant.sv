// COMMITTED TEST MUTANT -- collapse BIL2's second horizontal product route.
//
// This file declares no module. A private inverse build compiles it before the
// candidate and defines exactly one selected expression; production never sees it.
`default_nettype none

`ifdef ZHAO_BIL2_MUTANT_COLLAPSE_RESULTB
  `ifdef ZHAO_BIL2_MUTANT_RESERVED_SECOND
    `error "ZHAO_BIL2_MUTANT_SELECTOR_COLLISION"
  `endif
  `define ZHAO_BIL2_RESULTB(resulta, resultb) resulta
`elsif ZHAO_BIL2_MUTANT_RESERVED_SECOND
  // Reserved solely to prove that a future second selector cannot combine
  // silently with the committed collapse-resultb control.
  `error "ZHAO_BIL2_MUTANT_RESERVED_SECOND is not an implemented selector"
`endif

`default_nettype wire
