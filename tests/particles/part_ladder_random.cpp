// part_ladder_random.cpp — a randomized walk through the ladder, against the
// law rather than against the block.
//
// ---------------------------------------------------------------------------
// WHAT RANDOM BUYS OVER DIRECTED HERE
// ---------------------------------------------------------------------------
// The directed lane tests each rule once, on a clean input. What it cannot
// test is the SEQUENCE: a particle whose projected size wanders across
// thresholds for hundreds of frames while a governor changes its mind
// underneath, carrying `prev_rung` and `hold_count` forward the whole way.
//
// Hysteresis is a state machine, and a state machine with one input pattern is
// a state machine that has been looked at, not tested.
//
// The model is `zref::part::ladder_want` and `ladder_step`, written from the
// contract. The coverage guards at the end are the point of the file as much
// as the comparison is: a walk that never crosses a threshold, never gets
// held, and never has the governor bite would agree with the model perfectly
// and prove nothing.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_part_ladder.h"

#include "zhao_sim.hpp"
#include "zref/zref_particle.hpp"

namespace {

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_part_ladder top;

  top.v_valid_i = 0;
  top.r_ready_i = 1;
  top.rst_n = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  uint32_t s = 0x1ADDE12u;
  int bad_rung = 0, bad_hold = 0, bad_changed = 0;
  int steps = 0, crossings = 0, holds = 0, gov_bites = 0, protections = 0;

  // Several particles, each walked for many frames, each carrying its own
  // hold state forward exactly as the real record would.
  for (int particle = 0; particle < 60; ++particle) {
    const bool narrow = (rnd(&s) & 1u) != 0u;
    const bool prot = (rnd(&s) & 7u) == 0u;
    uint8_t prev = static_cast<uint8_t>(rnd(&s) % 6u);
    uint8_t hold = 0;
    bool first = true;

    // start somewhere on the scale and drift
    double size = 0.2 + static_cast<double>(rnd(&s) % 2400u) / 100.0;

    for (int f = 0; f < 120; ++f) {
      // a random walk that spends real time near thresholds
      const double step = (static_cast<double>(rnd(&s) % 200u) - 100.0) / 100.0;
      size += step;
      if (size < 0.05) size = 0.05;
      if (size > 30.0) size = 30.0;

      const uint16_t usize = static_cast<uint16_t>(size * 256.0);
      const uint16_t trail = static_cast<uint16_t>((rnd(&s) % 4000u));
      const uint8_t gov = static_cast<uint8_t>(rnd(&s) % 6u);

      const uint8_t want =
          zref::part::ladder_want(usize, trail, narrow, prot, gov);
      const zref::part::LadderOut w =
          zref::part::ladder_step(want, prev, hold, first);

      top.v_valid_i = 1;
      top.p_size_i = usize;
      top.p_trail_i = trail;
      top.p_narrow_i = narrow ? 1 : 0;
      top.p_protected_i = prot ? 1 : 0;
      top.p_gov_floor_i = gov;
      top.p_prev_rung_i = prev;
      top.p_hold_i = hold;
      top.p_first_i = first ? 1 : 0;
      top.eval();
      zhao::tick(top);
      top.v_valid_i = 0;
      top.eval();

      if (top.r_rung_o != w.rung) ++bad_rung;
      if (top.r_hold_o != w.hold) ++bad_hold;
      if ((top.r_changed_o != 0) != w.changed) ++bad_changed;

      // coverage
      if (!first && want != prev) ++crossings;
      if (!first && want != prev && !w.changed) ++holds;
      if (gov > zref::part::ladder_raw(usize, trail, narrow)) ++gov_bites;
      if (prot && zref::part::ladder_raw(usize, trail, narrow) ==
                      zref::part::kCulled)
        ++protections;

      prev = w.rung;
      hold = w.hold;
      first = false;
      ++steps;
    }
  }

  zhao::check(bad_rung == 0,
              "every rung matches zref::part::ladder_want + ladder_step across "
              "60 particles x 120 frames",
              0, bad_rung);
  zhao::check(bad_hold == 0, "and the hold count carried forward matches", 0,
              bad_hold);
  zhao::check(bad_changed == 0, "and the `changed` flag matches", 0, bad_changed);

  // The walk has to have REACHED the cases it exists for. A random walk that
  // never crossed a threshold would agree with the model perfectly and say
  // nothing about hysteresis at all.
  zhao::check(crossings > 100,
              "the walk really did cross thresholds, hundreds of times",
              1, crossings > 100 ? 1 : 0);
  zhao::check(holds > 50,
              "and the hysteresis really did suppress changes -- without this "
              "the agreement above is about a state machine that never left "
              "its first state",
              1, holds > 50 ? 1 : 0);
  zhao::check(gov_bites > 50, "and the governor really did coarsen decisions", 1,
              gov_bites > 50 ? 1 : 0);
  zhao::check(protections > 0,
              "and a protected particle really did get rescued from the cull",
              1, protections > 0 ? 1 : 0);

  std::printf("  %d steps: %d threshold crossings, %d held, %d governor bites, "
              "%d protections\n",
              steps, crossings, holds, gov_bites, protections);

  return zhao::report_and_exit("part_ladder_random");
}
