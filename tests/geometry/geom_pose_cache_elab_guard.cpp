// geom_pose_cache_elab_guard.cpp -- PROOF THAT THE `TYPE_W` GUARD FIRES.
//
// The owner ruling of 2026-09-21 (kind-8 / kind-9 ownership), section 4a,
// widens `zhao_geom_pose_cache`'s form key to 24 bits and permits a `TYPE_W`
// parameter so a legacy bench can still be configured at 16. It permits that
// for BENCHES. It does not permit production to be narrowed, and the ruling is
// explicit: "production must use 24 bits ... Do not invent a compact mapping
// merely to retain the old width."
//
// So the module refuses a `TYPE_W` outside 16..24 inside an
// `initial begin ... end` (Quartus 17.0 rejects a bare module-scope `if`), and
// CLAUDE.md, 2026-09-09: **`--lint-only` does not run `initial` blocks**. The
// block's clean lint therefore says NOTHING about that guard. "There is a
// guard" is a claim; this executable is the only form in which it is evidence.
//
// Built from the SAME module with `-GTYPE_W=8`, doing nothing but construct
// and evaluate the model -- which is when `$fatal` runs. Its ctest carries
// **WILL_FAIL TRUE**: green means the design refused to elaborate, red means
// the guard has stopped firing and a narrowed key could ship.
//
// `VL_USER_STOP` / `VL_USER_FATAL` are defined on the TARGET, for the reason
// `geom_clipread_elab_guard.cpp` records at length: `$fatal` never reaches
// `zhao::exit_hard`, and on this toolchain that hangs until the ctest timeout
// -- which is a RED whatever WILL_FAIL says, so the guard would fire and the
// test would fail anyway.
#include <cstdio>
#include <cstdlib>

#include "Vzhao_geom_pose_cache_elab.h"
#include "verilated.h"

void vl_stop(const char* filename, int linenum, const char* /*hier*/) VL_MT_UNSAFE {
  std::printf("[geom_pose_cache_elab_guard] THE GUARD FIRED: $stop from %s:%d\n",
              filename ? filename : "?", linenum);
  std::fflush(nullptr);
  std::_Exit(1);
}

void vl_fatal(const char* filename, int linenum, const char* /*hier*/, const char* msg)
    VL_MT_UNSAFE {
  std::printf("[geom_pose_cache_elab_guard] THE GUARD FIRED: %s:%d %s\n",
              filename ? filename : "?", linenum, msg ? msg : "");
  std::fflush(nullptr);
  std::_Exit(1);
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vzhao_geom_pose_cache_elab;
  top->eval();  // runs the `initial` block; TYPE_W = 8 must $fatal here
  std::printf(
      "[geom_pose_cache_elab_guard] ELABORATED WITH TYPE_W=8 -- the guard did NOT "
      "fire, and a pose cache keyed on eight bits of a twenty-four-bit form index "
      "could ship with every counter reading correct\n");
  std::fflush(nullptr);
  std::_Exit(0);  // a ZERO exit under WILL_FAIL is the red this test wants
}
