// zhao_raster_attrdiv_v2_remwidth_mutant.sv -- a COMMITTED, DELIBERATELY BROKEN
// control for `zhao_raster_attrdiv_v2.rem_range_err_o`.
//
// ---------------------------------------------------------------------------
// WHAT IS CHANGED, AND WHY IT HAD TO BE A COMMITTED FILE
// ---------------------------------------------------------------------------
// ONE substantive line: the radix-2 restoring compare demands TWICE the
// divisor before it subtracts.
//
//     production :  (rem_shift_c >= d1_c)
//     here       :  (rem_shift_c >= (d1_c << 1))
//
// `rem_range_err_o` counts completions whose raw restoring residue did not fit
// 47 bits. That state is UNREACHABLE while the compare is correct -- a
// restoring divider leaves rem < den by construction -- so NO LEGAL STIMULUS
// CAN EVER MOVE THE COUNTER. "It can fire" would stay an argument forever, and
// CLAUDE.md is explicit that a detector reading zero is a claim, and the claim
// to check hardest.
//
// With `>= 2*den` the invariant becomes rem < 2A rather than rem < A, so on a
// large area the residue outgrows 47 bits -- precisely the fault the guard
// exists to catch, and precisely the fault that would make v2's `rem_o[46:0]`
// truncation silently wrong.
//
// ---------------------------------------------------------------------------
// THE MUTATION THIS FILE DOES *NOT* USE, AND WHY THAT IS WORTH RECORDING
// ---------------------------------------------------------------------------
// The obvious mutation -- and the one CLAUDE.md cites as the canonical shape,
// `zhao_texture_frag_expand_mutant`'s `>=` becoming `>` -- WAS TRIED FIRST AND
// DOES NOT FIRE. Measured over 360,000 non-saturating operand pairs across
// seven divisors including the widest legal one:
//
//     production  (rem >= den)        final-step fires 0       any-step 0
//     mutant      (rem >  den)        final-step fires 0       any-step 0
//     mutant      (rem >= 2*den)      final-step fires 163,778 any-step 239,999
//     mutant      (low 47 bits only)  final-step fires 110,404 any-step 179,406
//
// `>` lets a residue exactly equal to the divisor survive one step, but the
// very next step's compare reduces it again, so the residue never reaches 2^47
// and the counter never moves. A mutant built on it would have been GREEN
// FOREVER while proving nothing whatever about the guard -- the exact failure
// this file exists to prevent, arrived at by copying a pattern that was correct
// for a different block. The pattern is not the evidence; the firing is.
//
// The production row is this file's real negative control: with the correct
// compare the state is not merely unvisited, it is unreachable.
//
// THE POLARITY OF THE DRIVER IS INVERTED. `raster_attrdiv_v2_remwidth_mutant`
// PASSES WHEN THE COUNTER FIRES. This file is evidence about the INSTRUMENT,
// not about the design; the design's correct behaviour (the counter stays zero)
// is asserted separately and positively by `raster_attrdiv_v2_rem_directed`.
// Neither test asserts the bug.
//
// ---------------------------------------------------------------------------
// WHY THIS IS A SELECTOR AND NOT A COPY
// ---------------------------------------------------------------------------
// This file declares NO module and contains no copy of the divider. It defines
// one macro that the real `zhao_raster_attrdiv_v2.sv` reads through its
// `ifndef` seam, so the mutation rides the CURRENT production body forever and
// cannot go stale. CLAUDE.md records three separate lanes that lost days to
// committed mutant COPIES drifting behind production -- thirteen combiner
// copies and eight AUX-pipe copies that stayed GREEN while measuring a machine
// that no longer existed. `tests/mutants/zhao_raster_attr_v2_mutants.sv` chose
// the selector shape for the same reason and says so in its own header.
//
// ---------------------------------------------------------------------------
// HOW IT IS SELECTED, AND THE TOOLCHAIN TRAP THAT SHAPE AVOIDS
// ---------------------------------------------------------------------------
// The build passes `-DZHAO_ATTR_V2_MUTANT_REM_WIDTH`, an OBJECT-like macro,
// which this file tests with a plain `ifdef` and only then defines the
// FUNCTION-like `ZHAO_ATTR_V2_REM_GE`. Verilator's command-line `-D` CANNOT
// define a function-like macro and says nothing when it fails to -- two
// combiner mutants once passed while measuring unmutated production for exactly
// that reason. Defining the function-like macro HERE, from a plain `ifdef`, is
// the shape that works.
//
// This file must be compiled IMMEDIATELY BEFORE `zhao_raster_attrdiv_v2.sv`, so
// that its `ifndef` sees the name already defined. Note the production file
// `undef`s the macro at its end, so the selection cannot leak into any other
// module in the same build.
//
// THE NEGATIVE CONTROL. `raster_attrdiv_v2_rem_directed` builds this same
// source list WITHOUT the define and requires the counter to stay at zero. That
// pair is what proves the selector actually engaged: a mutant that passes and a
// production build that also passes the inverted test would mean the macro
// never reached the compare.

`ifdef ZHAO_ATTR_V2_MUTANT_REM_WIDTH
// THE MUTATION. The compare demands twice the divisor, so the residue
// invariant becomes rem < 2A and outgrows the 47 bits rem_o publishes.
// Stimulus must use a LARGE area -- the residue can only reach 2^47 when
// 2A does. The driver uses area = 2^47-1.
`define ZHAO_ATTR_V2_REM_GE(rem, den) ((rem) >= ((den) << 1))
`endif
