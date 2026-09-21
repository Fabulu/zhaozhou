// geom_clipread_elab_guard.cpp -- PROOF THAT THE ELABORATION GUARD FIRES.
//
// `zhao_geom_clipread` refuses a `CLIP_ROWS` outside 1..64 inside an
// `initial begin ... end` (Quartus 17.0 rejects a bare module-scope `if`).
// CLAUDE.md, 2026-09-09: **`--lint-only` does not run `initial` blocks**, so
// the block's clean lint says NOTHING WHATEVER about that guard. "There is a
// guard" is a claim, and this is the only form in which it is evidence.
//
// This executable is built from the SAME `tb_geom_clipread` with
// `-GCLIP_ROWS=65` and does nothing but construct and evaluate the model, which
// is when `$fatal` runs. 65 rather than 0 because `-GCLIP_ROWS=0` makes
// Verilator reject a zero-sized array range and fail the BUILD, which would be
// a broken tree wearing a passing test's clothes. Its ctest carries **WILL_FAIL TRUE**: it passes when
// the design REFUSES TO ELABORATE, so a green here means the guard fired and a
// red means it has stopped firing.
//
// It is a POSITIVE CONTROL for an instrument, not a test of the design, and it
// is separate from `geom_clipread_directed` for exactly that reason: a control
// folded into the thing it controls cannot be read independently.
#include <cstdio>

#include "Vtb_geom_clipread_elab.h"
#include "verilated.h"

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vtb_geom_clipread_elab;
  top->eval();  // runs the `initial` block; CLIP_ROWS = 65 must $fatal here
  // Reaching this line at all is the failure this test exists to catch.
  std::printf(
      "[geom_clipread_elab_guard] ELABORATED WITH CLIP_ROWS=65 -- the guard did "
      "NOT fire, and every clean lint that cited it was worthless\n");
  top->final();
  return 0;
}
