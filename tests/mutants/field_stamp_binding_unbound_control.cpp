// field_stamp_binding_unbound_control.cpp -- THE POSITIVE CONTROL for
// `zhao_field_stamp_adapter`'s FH26 elaboration guard.
//
// **THE POLARITY IS INVERTED: this program PASSES WHEN THE GUARD FIRES.**
//
// It elaborates `tb_field_stamp_binding_unbound_mutant`, which instantiates
// the production adapter WITHOUT passing `STAMP_BINDING`. Owner directive
// FH26 -- "legacy is an explicit mode, never the strict default" -- is
// implemented as an elaboration refusal, so that omission must produce a
// `$fatal` at time zero rather than a quiet legacy machine.
//
// WHY THIS FILE EXISTS AT ALL. `verilator --lint-only` does not run `initial`
// blocks, so a clean lint of the mutant proves nothing about the guard --
// measured on this exact wrapper on 2026-09-20, where the only diagnostics
// were unused-signal warnings about the probe's own tie-offs. And the guarded
// state is unreachable by legal stimulus, so without this control "it can
// fire" stays an argument forever.
//
// It is evidence about the INSTRUMENT, not about the design.
//
// ---------------------------------------------------------------------------
// A CONTROL THAT WEDGES IS NOT A PASSING CONTROL -- THREE ATTEMPTS, MEASURED
// ---------------------------------------------------------------------------
// All three of these printed the guard's message correctly. The guard was
// never in doubt after the first run. What took three attempts was making the
// process actually TERMINATE, and every intermediate state looked like
// success in the log while failing as a gate.
//
//   1. DEFAULT RUNTIME. `$fatal` printed, then `Aborting...`, then the
//      process **sat alive at 0.00 CPU** until it was killed -- the
//      ALIVE-AT-ZERO-CPU signature CLAUDE.md records, with no WerFault dialog
//      visible to a non-interactive session.
//
//   2. OVERRODE `vl_fatal` ONLY. Built clean, changed nothing, still wedged.
//      The log said why and had said it all along: *"%Error: ... Verilog
//      $stop"*. An elaboration `$fatal` routes through **`vl_stop`**, not
//      `vl_fatal`. A fix that looks right and builds clean can still be
//      aimed at the wrong function.
//
//   3. OVERRODE BOTH, exiting with `std::exit(0)`. Run directly it exited
//      cleanly. **Under ctest it still reported `Timeout 120.01 sec` -- on a
//      run whose own captured output already said CONTROL PASSED.** That is
//      the most misleading state of the three: a red gate sitting on top of
//      green evidence. `std::exit` runs static destructors and Verilated
//      teardown blocks in them.
//
// The fix is `zhao::exit_hard`, which is `std::_Exit` and skips them. This is
// precisely what the repo's own traps list means by *"Verilated test mains
// call `zhao::exit_hard`, never `return`"* -- a rule that was written down and
// that I had to rediscover from the symptom.
//
// Both exit paths are overridden (`VL_USER_FATAL VL_USER_STOP` in
// tests/CMakeLists.txt) so the control cannot wedge by either route. The
// runtime prints the RTL's own message BEFORE either handler runs, so the
// ctest PASS_REGULAR_EXPRESSION matches the text the guard really produced
// and not a string this file invented.

#include <cstdio>
#include <cstdlib>

#include "verilated.h"

#include "zhao_sim.hpp"

#include "Vtb_field_stamp_binding_unbound_mutant.h"

// BOTH exit paths are overridden, and finding out which one mattered was the
// instructive part. `$fatal` in an `initial` block does NOT land in
// `vl_fatal` on this runtime -- the first attempt overrode only that and the
// process still wedged. The log said exactly where it actually went:
//
//     %Fatal: ...: Assertion failed in ...: STAMP_BINDING is UNBOUND...
//     %Error: ...: Verilog $stop
//     Aborting...
//
// `Verilog $stop` is `vl_stop`'s own text. So the elaboration `$fatal` routes
// through **`vl_stop`**, and overriding `vl_fatal` alone changed nothing --
// a fix that looked right, built clean, and did not work. Both are defined
// here so the control cannot wedge by either route.
//
// The RTL's own message is printed by the runtime BEFORE either of these is
// called, so the ctest regex matches the text the guard really produced and
// not a string this file invented.

void vl_stop(const char* filename, int linenum, const char* hier) {
  std::printf("CONTROL PASSED -- the guard FIRED, exactly as FH26 requires.\n");
  std::printf("  via  : vl_stop\n");
  std::printf("  file : %s:%d\n", filename ? filename : "(none)", linenum);
  std::printf("  scope: %s\n", hier ? hier : "(none)");
  std::fflush(stdout);
  // `zhao::exit_hard` is `std::_Exit`, and the HARD part is load-bearing.
  // `std::exit` was tried first: it printed everything correctly and then the
  // process still never terminated under ctest -- Timeout 120.01 sec on a run
  // whose own log already said CONTROL PASSED. `std::exit` runs static
  // destructors, and Verilated teardown blocks in them. `std::_Exit` skips
  // them. This is exactly why the repo's traps list says "Verilated test mains
  // call zhao::exit_hard, never return".
  zhao::exit_hard(0);
}

void vl_fatal(const char* filename, int linenum, const char* hier, const char* msg) {
  std::printf("CONTROL PASSED -- the guard FIRED, exactly as FH26 requires.\n");
  std::printf("  via  : vl_fatal\n");
  std::printf("  file : %s:%d\n", filename ? filename : "(none)", linenum);
  std::printf("  scope: %s\n", hier ? hier : "(none)");
  std::printf("  fatal: %s\n", msg ? msg : "(none)");
  std::fflush(stdout);
  zhao::exit_hard(0);
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  std::printf("control: elaborating zhao_field_stamp_adapter with STAMP_BINDING omitted\n");
  std::fflush(stdout);

  // The `$fatal` in the adapter's `initial` block fires during construction or
  // at the first evaluation. Both are before anything below runs, and both
  // land in the `vl_fatal` above, which exits 0.
  Vtb_field_stamp_binding_unbound_mutant dut;
  dut.clk = 0;
  dut.rst_n = 0;
  dut.eval();
  dut.clk = 1;
  dut.eval();
  dut.clk = 0;
  dut.eval();

  // Reaching here means the guard DID NOT FIRE, which is the whole failure
  // this control exists to detect: a composer could then omit the binding and
  // silently get the permissive legacy bridge, and every other gate in the
  // tree would stay green while it happened.
  std::printf(
      "CONTROL FAILED: elaboration SUCCEEDED with STAMP_BINDING unbound.\n"
      "  FH26 requires the omission to be refused. The guard in\n"
      "  fpga/rtl/field/zhao_field_stamp_adapter.sv is not firing, so nothing\n"
      "  prevents a production capsule selecting the permissive binding by\n"
      "  omission.\n");
  std::fflush(stdout);
  return 1;
}
