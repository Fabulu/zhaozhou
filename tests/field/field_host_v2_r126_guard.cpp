// field_host_v2_r126_guard.cpp -- R126's elaboration guard, SEEN TO FIRE.
//
// ENFORCES: fpga/rtl/field/zhao_field_host_v2.sv, the `initial` block's
//           OUT_LANES != ZFH_WINDOW_MASK_BITS check.
//
// ---------------------------------------------------------------------------
// WHY THIS IS A SEPARATE BINARY
// ---------------------------------------------------------------------------
// Owner ruling R126: "`ZFH_WINDOW_MASK_BITS` must equal composed `OUT_LANES`,
// and NOTHING CHECKS IT." Three places carry one quantity -- the host's
// parameter default, the console's composition, and the generated schema -- and
// the disagreement is INVISIBLE, because a mask of the wrong width still packs,
// still transmits and still compares.
//
// The ruling names two traps and both of them are the reason this file exists:
//
//   1. Quartus 17.0 rejects a bare module-scope `if`, so the guard has to live
//      inside `initial begin ... end`.
//   2. **`verilator --lint-only` DOES NOT RUN `initial` BLOCKS.** A clean lint
//      is, in the ruling's own words, "no evidence whatever" about it.
//
// So the only way to know the guard works is to ELABORATE AND RUN a
// deliberately wrong parameterisation and watch it die. `tests/CMakeLists.txt`
// builds this target from the SAME host sources with `-GOUT_LANES=5` while the
// generated schema fixes `ZFH_WINDOW_MASK_BITS = 7`.
//
// ---------------------------------------------------------------------------
// THE POLARITY IS INVERTED, AND THE REASON IS CHECKED TOO
// ---------------------------------------------------------------------------
// This program reaching its own last line is the FAILURE case: it means the
// model elaborated at a width the schema forbids. The `$fatal` kills the
// process before that, which is the pass.
//
// `tests/field/run_r126_guard.ps1` is the driver that inverts the verdict, and
// it does NOT merely check for a non-zero exit code. "The process died" is
// satisfied by a segfault, a missing DLL or an unrelated assertion, and a
// control that accepts any death is a control that cannot distinguish the thing
// it was built to prove. The driver requires the fatal text to name BOTH
// quantities, so the guard is shown to have fired FOR ITS OWN REASON.

#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_host_v2_r126.h"

#include "zhao_sim.hpp"

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  std::printf("R126: elaborating zhao_field_host_v2 with OUT_LANES=5 against a\n");
  std::printf("R126: schema that fixes ZFH_WINDOW_MASK_BITS=7. The guard should\n");
  std::printf("R126: $fatal before the line below is reached.\n");
  std::fflush(stdout);

  Vzhao_field_host_v2_r126 dut;

  // Verilator runs `initial` blocks on the first evaluation, so this is where
  // the guard fires. `eval()` rather than a clocked tick: no clock is needed to
  // reach an elaboration check and driving one would only obscure what killed
  // the process.
  dut.eval();

  // Unreachable if the guard is doing its job.
  std::printf("R126-GUARD-DID-NOT-FIRE: the host elaborated at OUT_LANES=5 while\n");
  std::printf("R126-GUARD-DID-NOT-FIRE: the schema fixes 7. The window mask is now\n");
  std::printf("R126-GUARD-DID-NOT-FIRE: two bits narrower than the hardware it\n");
  std::printf("R126-GUARD-DID-NOT-FIRE: describes, and nothing anywhere will say so.\n");
  std::fflush(stdout);
  zhao::exit_hard(0);
}
