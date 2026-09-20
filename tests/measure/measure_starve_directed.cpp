// measure_starve_directed.cpp -- MEASURE.GOVERNOR's starvation verdict:
// `zhao_measure_starve`, the latch `zhao_console_core.sv`'s I18 entry names as
// the reason the governor cannot be composed.
//
// WHAT THIS LANE WOULD CATCH:
//
//   1. THE REASON CLASSIFICATION IS THE ONE MEASURE.TOKENS DEFINES. Reasons 0
//      and 1 are denials against the view's own private pool and count; reason
//      2 is law T9's budget-load collision and does not. Red if the block
//      counts every denial, which is the plausible wrong reading and the one
//      that lets a frame boundary degrade a view that was never short.
//   2. DUO FAIRNESS, STRUCTURALLY (the governor's law G3). A storm of view-1
//      denials must not move `starved0_o` by one bit. This is charter section
//      9's "one player looking into a volcano cannot make the other player's
//      army disappear" applied one block upstream of where the governor
//      applies it, because a verdict that leaked here would leak through a
//      governor that is itself perfectly fair.
//   3. THE VERDICT IS HELD FOR THE WHOLE FRAME. One denial early, hundreds of
//      idle cycles, and the bit is still set at the pulse. A latch that
//      decayed would report the frame quiet because nothing happened lately.
//   4. THE GOVERNOR SEES THE FRAME IT JUST FINISHED, NOT THE ONE BEFORE. The
//      output must read 1 ON the pulse cycle and 0 from the next cycle. This
//      is the case that catches `starved_o <= acc` at the pulse, which is the
//      obvious implementation and is one frame late -- late in the flattering
//      direction, because a view that has stopped being starved stays
//      degraded.
//   5. THE BOUNDARY CONVENTION IS THE DOCUMENTED ONE. A denial arriving in the
//      same cycle as the pulse belongs to the NEW frame, so it must not appear
//      in the verdict being read on that cycle and must appear in the next.
//      Asserted rather than left to be rediscovered by whoever debugs an
//      off-by-one-frame degrade.
//   6. EVERY COUNTER FIRES, AND IS EXACT. Including `reload_ignored_o`, which
//      is the positive control for the deliberate exclusion in case 1 -- a
//      counter asserted zero and never seen to move is a claim, and this file
//      makes it a measurement.
#include "Vzhao_measure_starve.h"

#include <cstdio>

#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr int kLowPriority = 0;  // zhao_measure_tokens REASON_LOW_PRIORITY
constexpr int kExhausted   = 1;  // zhao_measure_tokens REASON_EXHAUSTED
constexpr int kReload      = 2;  // zhao_measure_tokens REASON_RELOAD

void reset_dut(Vzhao_measure_starve& dut) {
  dut.rst_n = 0;
  dut.clk = 0;
  dut.frame_i = 0;
  dut.den_valid_i = 0;
  dut.den_view_i = 0;
  dut.den_reason_i = 0;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);
}

/** One denial cycle. Nothing here is a term in the producer's handshake. */
void deny(Vzhao_measure_starve& dut, int view, int reason) {
  dut.den_valid_i = 1;
  dut.den_view_i = static_cast<uint8_t>(view);
  dut.den_reason_i = static_cast<uint8_t>(reason);
  zhao::tick(dut);
  dut.den_valid_i = 0;
  dut.den_view_i = 0;
  dut.den_reason_i = 0;
  dut.eval();
}

void idle(Vzhao_measure_starve& dut, int cycles) {
  for (int i = 0; i < cycles; ++i) zhao::tick(dut);
}

/**
 * The frame boundary, driven the way MEASURE.GOVERNOR drives it: the verdict
 * is whatever the outputs read ON the pulse cycle, before the clock edge that
 * clears the accumulator. `co_den` optionally presents a denial in the very
 * same cycle (case 5).
 */
struct Verdict {
  bool s0;
  bool s1;
};

Verdict frame_pulse(Vzhao_measure_starve& dut, bool co_den = false, int co_view = 0,
                    int co_reason = kLowPriority) {
  dut.frame_i = 1;
  if (co_den) {
    dut.den_valid_i = 1;
    dut.den_view_i = static_cast<uint8_t>(co_view);
    dut.den_reason_i = static_cast<uint8_t>(co_reason);
  }
  dut.eval();
  // Read BEFORE the edge: this is the value the governor samples, because the
  // governor is clocked by the same pulse and reads the same wires.
  Verdict v{dut.starved0_o != 0, dut.starved1_o != 0};
  zhao::tick(dut);
  dut.frame_i = 0;
  dut.den_valid_i = 0;
  dut.den_view_i = 0;
  dut.den_reason_i = 0;
  dut.eval();
  return v;
}

}  // namespace

int main() {
  Vzhao_measure_starve dut;

  // ---- case 1: reset is quiet -------------------------------------------
  reset_dut(dut);
  check(dut.starved0_o == 0, "reset: view 0 not starved", 0, dut.starved0_o);
  check(dut.starved1_o == 0, "reset: view 1 not starved", 0, dut.starved1_o);
  check(dut.denials_seen_o == 0, "reset: denials_seen", 0, dut.denials_seen_o);
  check(dut.frames_o == 0, "reset: frames", 0, dut.frames_o);

  // ---- case 2: a LOW_PRIORITY denial on view 0 sets view 0 only ----------
  deny(dut, 0, kLowPriority);
  check(dut.starved0_o == 1, "low-priority denial sets view 0", 1, dut.starved0_o);
  check(dut.starved1_o == 0, "low-priority denial leaves view 1", 0, dut.starved1_o);
  {
    Verdict v = frame_pulse(dut);
    check(v.s0, "view 0 verdict at the pulse", 1, v.s0 ? 1 : 0);
    check(!v.s1, "view 1 verdict at the pulse", 0, v.s1 ? 1 : 0);
  }
  // Case 4: cleared from the cycle after the pulse.
  check(dut.starved0_o == 0, "cleared after the pulse", 0, dut.starved0_o);

  // ---- case 3: EXHAUSTED counts too, on view 1 ---------------------------
  deny(dut, 1, kExhausted);
  check(dut.starved1_o == 1, "exhausted denial sets view 1", 1, dut.starved1_o);
  check(dut.starved0_o == 0, "exhausted denial leaves view 0", 0, dut.starved0_o);
  {
    Verdict v = frame_pulse(dut);
    check(v.s1, "view 1 verdict at the pulse", 1, v.s1 ? 1 : 0);
    check(!v.s0, "view 0 stays clear", 0, v.s0 ? 1 : 0);
  }

  // ---- case 1 proper: RELOAD does NOT count, and is counted separately ----
  const uint32_t reload_before = dut.reload_ignored_o;
  deny(dut, 0, kReload);
  check(dut.starved0_o == 0, "RELOAD denial does not starve view 0", 0, dut.starved0_o);
  check(dut.reload_ignored_o == reload_before + 1, "reload_ignored_o fired",
        reload_before + 1, dut.reload_ignored_o);
  {
    Verdict v = frame_pulse(dut);
    check(!v.s0 && !v.s1, "a RELOAD-only frame is not a starved frame", 0,
          (v.s0 || v.s1) ? 1 : 0);
  }

  // ---- case 2 proper: the volcano. View 1 storms; view 0 must not move ----
  {
    const uint32_t s0_frames_before = dut.starved_frames0_o;
    for (int f = 0; f < 20; ++f) {
      for (int d = 0; d < 8; ++d) deny(dut, 1, (d & 1) ? kExhausted : kLowPriority);
      Verdict v = frame_pulse(dut);
      check(v.s1, "volcano: view 1 starved every frame", 1, v.s1 ? 1 : 0);
      check(!v.s0, "volcano: view 0 never starved", 0, v.s0 ? 1 : 0);
    }
    check(dut.starved_frames0_o == s0_frames_before, "volcano: view 0 frame count frozen",
          s0_frames_before, dut.starved_frames0_o);
    check(dut.starved_frames1_o >= 20, "volcano: view 1 frame count moved", 20,
          dut.starved_frames1_o);
  }

  // ---- case 3 proper: the verdict is HELD across a long quiet frame ------
  deny(dut, 0, kLowPriority);
  idle(dut, 500);
  check(dut.starved0_o == 1, "held across 500 idle cycles", 1, dut.starved0_o);
  {
    Verdict v = frame_pulse(dut);
    check(v.s0, "held verdict survives to the pulse", 1, v.s0 ? 1 : 0);
  }

  // ---- case 5: a denial in the pulse cycle belongs to the NEW frame ------
  {
    // The accumulator is empty; present a view-0 denial in the same cycle as
    // the pulse. The verdict being read must be clean, and the NEXT frame must
    // carry it.
    Verdict v = frame_pulse(dut, /*co_den=*/true, /*co_view=*/0, kLowPriority);
    check(!v.s0, "pulse-cycle denial is not in the ending frame", 0, v.s0 ? 1 : 0);
    check(dut.starved0_o == 1, "pulse-cycle denial seeds the new frame", 1, dut.starved0_o);
    Verdict v2 = frame_pulse(dut);
    check(v2.s0, "the new frame carries it", 1, v2.s0 ? 1 : 0);
  }

  // ---- case 6: the counters are exact over a known script ----------------
  {
    reset_dut(dut);
    // 3 low-priority on view 0, 2 exhausted on view 1, 4 reloads, 1 frame.
    for (int i = 0; i < 3; ++i) deny(dut, 0, kLowPriority);
    for (int i = 0; i < 2; ++i) deny(dut, 1, kExhausted);
    for (int i = 0; i < 4; ++i) deny(dut, 0, kReload);
    Verdict v = frame_pulse(dut);
    check(v.s0 && v.s1, "both views starved in the scripted frame", 1,
          (v.s0 && v.s1) ? 1 : 0);
    check(dut.denials_seen_o == 9, "denials_seen_o exact", 9, dut.denials_seen_o);
    check(dut.starving_denials_o == 5, "starving_denials_o exact", 5, dut.starving_denials_o);
    check(dut.reload_ignored_o == 4, "reload_ignored_o exact", 4, dut.reload_ignored_o);
    check(dut.frames_o == 1, "frames_o exact", 1, dut.frames_o);
    check(dut.starved_frames0_o == 1, "starved_frames0_o exact", 1, dut.starved_frames0_o);
    check(dut.starved_frames1_o == 1, "starved_frames1_o exact", 1, dut.starved_frames1_o);
  }

  zhao::exit_hard(zhao::report_and_exit("measure_starve_directed"));
}
