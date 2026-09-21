// geom_clipread_elab_guard.cpp -- PROOF THAT THE ELABORATION GUARD FIRES.
//
// `zhao_geom_clipread` refuses a `CLIP_ROWS` outside 1..64 inside an
// `initial begin ... end` (Quartus 17.0 rejects a bare module-scope `if`).
// CLAUDE.md, 2026-09-09: **`--lint-only` does not run `initial` blocks**, so
// the block's clean lint says NOTHING WHATEVER about that guard. "There is a
// guard" is a claim, and this is the only form in which it is evidence.
//
// This executable is built from the SAME `tb_geom_clipread` with
// `-GCLIP_ROWS=65` and does nothing but construct and evaluate the model,
// which is when `$fatal` runs. 65 rather than 0 because `-GCLIP_ROWS=0` makes
// Verilator reject a zero-sized array range and fail the BUILD, which would be
// a broken tree wearing a passing test's clothes.
//
// Its ctest carries **WILL_FAIL TRUE**: it passes when the design REFUSES TO
// ELABORATE, so a green here means the guard fired and a red means it has
// stopped firing. It is a POSITIVE CONTROL for an instrument, not a test of
// the design, and it is separate from `geom_clipread_directed` for exactly
// that reason -- a control folded into the thing it controls cannot be read
// independently.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE OVERRIDES `vl_stop` AND `vl_fatal`
// ---------------------------------------------------------------------------
// MEASURED, not anticipated. The first run of this test through ctest printed
// the %Fatal and then **hung for the full 120-second timeout**, and a TIMEOUT
// is a ctest failure whatever `WILL_FAIL` says -- so the guard fired and the
// test went RED anyway, which is the worst of both readings.
//
// The cause is the one `zhao_sim.hpp` already documents at length: on this
// toolchain every Verilated exe must reach `std::_Exit` deterministically,
// because ordinary termination intermittently deadlocks. `$fatal` never
// reaches `zhao::exit_hard` -- it ends in Verilator's own `abort()` path,
// which is precisely the exit this tree has a standing workaround for.
//
// So `VL_USER_STOP` / `VL_USER_FATAL` replace that tail with the SAME
// workaround the rest of the suite uses. The refusal is still the design's:
// these functions are reached only when the DUT has already refused, and the
// printed line below is the evidence.
//
// The two macros are defined on the TARGET in `tests/CMakeLists.txt` and not
// here, because Verilator's `#ifndef VL_USER_STOP` lives in `verilated.cpp` --
// a define in this file would be invisible to the translation unit that reads
// it, and the override would silently not take. Which is exactly how this
// would have looked fine and hung again.
#include <cstdio>
#include <cstdlib>

#include "Vtb_geom_clipread_elab.h"
#include "verilated.h"

void vl_stop(const char* filename, int linenum, const char* /*hier*/) VL_MT_UNSAFE {
  std::printf("[geom_clipread_elab_guard] THE GUARD FIRED: $stop from %s:%d\n",
              filename ? filename : "?", linenum);
  std::fflush(nullptr);
  std::_Exit(1);
}

void vl_fatal(const char* filename, int linenum, const char* /*hier*/, const char* msg)
    VL_MT_UNSAFE {
  std::printf("[geom_clipread_elab_guard] THE GUARD FIRED: %s:%d %s\n",
              filename ? filename : "?", linenum, msg ? msg : "");
  std::fflush(nullptr);
  std::_Exit(1);
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vtb_geom_clipread_elab;
  top->eval();  // runs the `initial` block; CLIP_ROWS = 65 must $fatal here
  // Reaching this line at all is the failure this test exists to catch.
  std::printf(
      "[geom_clipread_elab_guard] ELABORATED WITH CLIP_ROWS=65 -- the guard did "
      "NOT fire, and every clean lint that cited it was worthless\n");
  std::fflush(nullptr);
  std::_Exit(0);  // a ZERO exit under WILL_FAIL is the red this test wants
}
