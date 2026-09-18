// measure_histogram_drain_mutant.cpp -- THE POLARITY OF THIS TEST IS INVERTED.
// It PASSES WHEN `frozen_write_o` FIRES.
//
// It is evidence about the INSTRUMENT, not about the design.
//
// `zhao_measure_histogram`'s `frozen_write_o` counts memory updates aimed at
// the bank the host owns. Every directed lane asserts it reads zero -- and
// CLAUDE.md is explicit that a detector reading zero is a CLAIM, and the claim
// to check hardest. The state is unreachable with legal stimulus while law S3's
// drain is correct, so the only way to show the counter alive is to break the
// drain, which tests/mutants/zhao_measure_histogram_drain_mutant.sv does in one
// line.
//
// BOTH CONTROLS RUN HERE, ON THE SAME STIMULUS, IN ONE EXECUTABLE:
//
//   POSITIVE -- the mutant, whose swap does not wait, must report
//               `frozen_write_o` > 0.
//   NEGATIVE -- the SHIPPED module, given the identical stimulus, must report
//               `frozen_write_o` == 0.
//
// The negative control is the half that is easy to omit and expensive to omit.
// Without it a mutant that fired for some unrelated reason -- a driver bug, a
// reset slip, a counter that increments on everything -- would look exactly
// like a working detector. With it, the lane says the counter distinguishes the
// broken block from the working one, which is the only statement worth making.
//
// THE STIMULUS is the same shape as directed lane 8: accept a beat whose four
// events fall in four DISTINCT bins, so four groups are queued, then pulse
// `snapshot_i` on the very next cycle with three of them still in flight. In
// the shipped block the swap waits for them (law S3). In the mutant it does
// not, so a group issued against the old `active_q` reaches its write stage
// after the bank has moved -- and the counter differences exactly those two
// registers.

#include "Vzhao_measure_histogram.h"
#include "Vzhao_measure_histogram_drain_mutant.h"

#include "histogram_dev.hpp"

#include <cstdio>

namespace {

using zhao::check;
using hist_test::Beat;

/** Four events in four distinct bins: 1 -> 1, 4 -> 4, 100 -> 13, 0xFFFF -> 31. */
Beat four_distinct() {
  Beat b;
  b.mask = 0xF;
  b.err[0] = 1u;
  b.err[1] = 4u;
  b.err[2] = 100u;
  b.err[3] = 0xFFFFu;
  return b;
}

/**
 * Accept a four-group beat and request a snapshot while it is still draining.
 * Returns `frozen_write_o` afterwards.
 */
template <typename Top>
uint32_t run_stimulus(Top& d) {
  hist_test::reset(d);
  hist_test::wait_scrub(d);

  const int refused = hist_test::send_beat(d, four_distinct());
  check(refused == 0, "mutant lane: the beat is accepted on the offered cycle", 0,
        static_cast<uint64_t>(refused));
  // The pulse lands on the cycle after acceptance, with all four lanes queued
  // and nothing yet written.
  hist_test::snapshot(d);
  hist_test::idle(d, 16);
  return static_cast<uint32_t>(d.frozen_write_o);
}

}  // namespace

int main() {
  auto* mutant = new Vzhao_measure_histogram_drain_mutant;
  auto* shipped = new Vzhao_measure_histogram;

  const uint32_t mut_fires = run_stimulus(*mutant);
  const uint32_t ship_fires = run_stimulus(*shipped);

  // POSITIVE CONTROL. Inverted polarity: a zero here means the detector is
  // blind, not that the design is healthy.
  check(mut_fires > 0u,
        "POSITIVE CONTROL: frozen_write_o FIRES on a block whose snapshot does not drain", 1,
        mut_fires);

  // NEGATIVE CONTROL. The same stimulus on the shipped block.
  check(ship_fires == 0u,
        "NEGATIVE CONTROL: the identical stimulus does NOT fire it on the shipped block", 0,
        ship_fires);

  std::printf(
      "measure_histogram_drain_mutant: mutant frozen_write_o=%u (must be > 0),\n"
      "  shipped frozen_write_o=%u (must be 0). Inverted polarity by design.\n",
      mut_fires, ship_fires);

  const int rc = zhao::report_and_exit("measure_histogram_drain_mutant");
  zhao::exit_hard(rc);
}
