// zhao_raster_rcp24_v4_timing_mutants.sv -- committed V4 positive control.
//
// This is a pre-production shim, not a replacement module.  When the reverse
// selector is enabled it changes only the operand of the production V4 leading-
//zero function, and the source manifest places it immediately before the real
// zhao_raster_rcp24_v4.sv.  The directed test must then print exactly DETECTED
// because the token scoreboard sees the resulting r/k/zero divergence.
//
// Keep the collision guard even while there is only one implemented selector.
// A future selector must not silently combine with this one and turn a targeted
// positive control into an ambiguous mutant.
`default_nettype none

`ifdef ZHAO_RCP_V4_LZC_MUTANT_REVERSE
  `ifdef ZHAO_RCP_V4_LZC_MUTANT_IDENTITY
    `error "ZHAO_RCP_V4_LZC_MUTANT selector collision"
  `endif
  `ifdef ZHAO_RCP_V4_LZC_VALUE
    `error "ZHAO_RCP_V4_LZC_VALUE was already defined before the mutant shim"
  `endif

  // Explicit 24-bit reversal.  Do not replace this with a shift or a helper
  // function: the mutation must be reviewable as one orientation mistake.
  `define ZHAO_RCP_V4_LZC_VALUE(value) \
      {value[0], value[1], value[2], value[3], value[4], value[5], \
       value[6], value[7], value[8], value[9], value[10], value[11], \
       value[12], value[13], value[14], value[15], value[16], value[17], \
       value[18], value[19], value[20], value[21], value[22], value[23]}
`elsif ZHAO_RCP_V4_LZC_MUTANT_IDENTITY
  // The identity selector is a reserved collision sentinel, not a second
  // shipped mutation.  If it is ever commissioned, this file must be updated
  // with an explicit, independently tested selector.
  `error "ZHAO_RCP_V4_LZC_MUTANT_IDENTITY is not an implemented selector"
`endif

`default_nettype wire
