// terrain_lodhist_widthguard_control.cpp -- THE POSITIVE CONTROL for the
// width guard on entry I18's event mapping. ITS POLARITY IS INVERTED: this
// program is registered `WILL_FAIL`, so the ctest PASSES when the model dies.
//
// WHAT IT GUARDS. `zhao_console_core.sv` maps three 24-bit LOD deviations onto
// MEASURE.HISTOGRAM's four EW-bit lanes, and `spec/measure_rules.md` section 3
// says that is an ADAPTATION only because zero-extension moves no value into a
// different bucket. At EW < 24 it stops being zero-extension and becomes a
// TRUNCATION -- which reads LOW, the flattering direction, in an organ that is
// metric-agnostic by design and therefore cannot notice. So there is an
// elaboration `$fatal`, and this file is the evidence that it speaks.
//
// WHY IT EXISTS AS A COMMITTED CONTROL AND NOT AS AN ARGUMENT, and this is the
// part worth reading. The guard's FIRST version could never have fired, and the
// fault was one line above it:
//
//     wire [LANES*EW-1:0] ev_err_c = { ..., {(EW-24){1'b0}}, w_dev3_o, ... };
//
// At EW = 16, `EW-24` underflows to 4,294,967,288 and elaboration dies INSIDE
// THE CONCATENATION -- `verilator --lint-only -GEW=16` returned
// "%Error: Internal Error: ../V3Number.h:242: `num` member accessed when data
// type is UNINITIALIZED". The `$fatal` written to explain that exact mistake
// was unreachable BECAUSE of that exact mistake. CLAUDE.md's law -- a detector
// that has not been seen to fire is a claim, not an instrument -- caught its
// own author here. The widening is a size cast now, and this control is what
// stops the next version of that accident being silent.
//
// AND A SECOND TOOL FACT, which is why a lint could not have been the control:
// `--lint-only` DOES NOT RUN `initial` BLOCKS. After the repair, linting at
// -GEW=16 returns RC 0 and says nothing whatever. The model has to be BUILT and
// EVALUATED, which is what this does.
//
// AND A THIRD, WHICH IS WHY THE SIGABRT HANDLER IS HERE. The guard fires and
// Verilator then calls `std::abort()`, which on this toolchain HANGS at ~0 CPU
// instead of exiting -- the same libwinpthread deadlock `tests/harness/
// zhao_sim.hpp` documents for a main that `return`s instead of calling
// `zhao::exit_hard`. Under ctest that is a TIMEOUT, and a timeout is NOT
// inverted by WILL_FAIL, so a control that fires perfectly would have been
// reported as a failure. Measured: the guard printed its message and the
// process then sat until it was killed. The handler below turns the abort into
// a hard, immediate non-zero exit, which is what WILL_FAIL reads.
//
// A SIGABRT HANDLER WAS TRIED FIRST AND DID NOT WORK: the deadlock is reached
// BEFORE `abort()` raises anything -- Verilator prints "Aborting..." and then
// hangs inside its own flush/exit machinery. So the escape is a WATCHDOG, and
// it keeps the control's polarity honest in BOTH directions:
//
//   * the guard FIRES  -> elaboration wedges -> the watchdog exits 2 -> the
//     WILL_FAIL ctest PASSES;
//   * the guard is DEAD -> `eval()` returns in milliseconds, `main` prints the
//     failure line and exits 0 long before the watchdog -> the WILL_FAIL ctest
//     FAILS, which is what a dead guard should do.
//
// There is no race worth worrying about between those two: the model is six
// modules and evaluates in under a millisecond against a 3-second window.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "verilated.h"

#include "Vtb_terrain_lodhist.h"

namespace {
// THE WATCHDOG DOES NO I/O, AND THAT IS THE WHOLE POINT OF ITS SECOND VERSION.
// The first one flushed and printed a friendly line before exiting, and it
// DEADLOCKED: run under ctest alongside other builds it hit the 300-second
// timeout, which is the one outcome WILL_FAIL cannot invert. The wedge is
// inside Verilator's flush/exit machinery, so it is holding the stdio lock --
// and the escape hatch was asking for that same lock before it could escape.
//
// That is this repository's own law in miniature: the recovery path must not
// depend on the thing that failed. `_Exit` takes no locks, flushes nothing and
// runs no handler, so it cannot be blocked by whatever wedged. The message the
// old version printed is the one thing that had to go.
//
// The direct run had exited cleanly at RC 1, which is why the first version
// looked fine: the abort only wedges under load. A control that works when the
// machine is idle and times out when it is busy is worse than no control.
void watchdog() {
  std::this_thread::sleep_for(std::chrono::seconds(2));
  std::_Exit(2);  // non-zero: the ctest is WILL_FAIL, so this is the PASS
}
}  // namespace

int main(int argc, char** argv) {
  std::thread(watchdog).detach();
  Verilated::commandArgs(argc, argv);
  std::printf("widthguard control: elaborating tb_terrain_lodhist at EW=16.\n");
  std::printf("  EXPECTED: the elaboration $fatal stops this process.\n");
  std::printf("  If you are reading the line after this one, the guard is DEAD.\n");
  std::fflush(stdout);

  Vtb_terrain_lodhist dut;
  dut.rst_n = 0;
  dut.eval();  // `initial` blocks run here

  std::printf("widthguard control: FAILED -- the model elaborated at EW=16 and"
              " the 24-bit deviations are being TRUNCATED silently.\n");
  std::fflush(stdout);
  // Deliberately a clean exit: registered WILL_FAIL, a zero status is the
  // ctest FAILING, which is what this line means. `_Exit` rather than `return`
  // for zhao_sim.hpp's reason -- a Verilated main that returns hangs.
  std::fflush(nullptr);
  std::_Exit(0);
}
