// measure_histogram_sat.cpp -- MEASURE.HISTOGRAM lane 13: saturation at the
// chosen width (phase 8, ZH-049).
//
// WHY THIS IS A SEPARATE EXECUTABLE. The shipped block is CW = 24, derived in
// the module header from the documented interval: one 60 Hz frame at 100 MHz
// cannot contain more than 1,666,666 events, which needs 21 bits, rounded to
// 24. Reaching that ceiling in simulation costs 16.7 million events and hours.
// So this target builds THE SAME RTL FILE with `-GCW=8` -- the saturation logic
// is identical, the ceiling is 255, and 65 four-lane beats reach it.
//
// This is not a smaller block pretending to be the real one: `CW` is the only
// override, and every other law (bucket geometry, banks, epochs, forwarding,
// the scrub) is bit-for-bit the shipped configuration. What the lane can
// therefore claim is exactly "the saturating adder and its counter behave as
// specified at the parameterised width", and nothing about 24 bits beyond the
// fact that the same expression computes both.
//
// What this lane would catch:
//   13a. WRAP INSTEAD OF SATURATE -- spec/counters.md section 4 says counters
//        "MUST saturate, never wrap". At 256 events a wrapping bin reads 0 and
//        a saturating bin reads 255. A wrap is the single worst failure a
//        measurement organ can have, because the number it produces is not
//        merely inaccurate, it is small -- the flattering direction, and the
//        direction nobody audits.
//   13b. A SATURATION THAT IS NOT VISIBLE -- `bin_sat_o` must count each
//        update that hit the ceiling. Without it a host cannot tell a
//        genuinely quiet bin from a clipped one, and the histogram's own sum
//        stops meaning anything silently.
//   13c. THE TOTAL SATURATES TOO, AND SEPARATELY -- `snap_total_o` counts
//        events ACCEPTED while the bins count events STORED, so they diverge
//        exactly once clipping starts. That divergence is deliberate (module
//        header, "WIDTHS, DERIVED") and is asserted here rather than left as
//        prose.
//   13d. SATURATION IS PER BIN -- a clipped bin must not stop or corrupt any
//        other bin.

#include "Vzhao_measure_histogram.h"

#include "histogram_dev.hpp"

#include <cstdio>
#include <vector>

namespace {

using hist_test::Beat;
using hist_test::Counters;
using zhao::check;

// Four magnitudes that all bucket to bin 4 (see the hand-computed table in
// measure_histogram_directed.cpp: 4 and 5 both land in bin 4).
Beat quad_bin4() {
  Beat b;
  b.mask = 0xF;
  b.err[0] = 4u;
  b.err[1] = 5u;
  b.err[2] = 4u;
  b.err[3] = 5u;
  return b;
}

void test_saturation(Vzhao_measure_histogram& d) {
  hist_test::reset(d);
  const int scrub = hist_test::wait_scrub(d);
  check(scrub == hist_test::kScrubCycles,
        "sat: the scrub is unchanged by the width override (it walks addresses, not counts)",
        hist_test::kScrubCycles, static_cast<uint64_t>(scrub));

  // 60 beats x 4 = 240, comfortably under the 255 ceiling.
  for (int i = 0; i < 60; ++i) hist_test::send_beat(d, quad_bin4());
  hist_test::idle(d, 8);
  Counters c = hist_test::counters(d);
  check(c.events == 240u, "sat: 240 events accepted, nothing clipped yet", 240, c.events);
  check(c.bin_sat == 0u, "sat: no saturation has been reported", 0, c.bin_sat);

  // 63 beats = 252. The next beat would make 256.
  for (int i = 0; i < 3; ++i) hist_test::send_beat(d, quad_bin4());
  hist_test::idle(d, 8);
  c = hist_test::counters(d);
  check(c.bin_sat == 0u, "sat: 252 is still under the ceiling", 0, c.bin_sat);

  // The clipping beat: 252 + 4 = 256, which is 0 if the adder wraps.
  hist_test::send_beat(d, quad_bin4());
  hist_test::idle(d, 8);
  c = hist_test::counters(d);
  check(c.bin_sat == 1u, "sat: the clipping update is REPORTED, exactly once", 1, c.bin_sat);

  // And once more: a saturated bin stays saturated and keeps reporting.
  hist_test::send_beat(d, quad_bin4());
  hist_test::idle(d, 8);
  c = hist_test::counters(d);
  check(c.bin_sat == 2u, "sat: a second clipped update is reported too", 2, c.bin_sat);

  // Another bin, untouched by the clipping, must still count normally.
  for (int i = 0; i < 3; ++i) {
    Beat b;
    b.mask = 0x1;
    b.err[0] = 100u;  // bin 13
    hist_test::send_beat(d, b);
  }
  hist_test::idle(d, 8);

  hist_test::snapshot(d);
  const std::vector<uint32_t> bins = hist_test::read_all(d);
  check(bins[4] == 255u, "sat: the clipped bin reads 255 -- SATURATED, not wrapped to 0", 255,
        bins[4]);
  check(bins[13] == 3u, "sat: a neighbouring bin is untouched by the clipping", 3, bins[13]);

  // snap_total_o counts events ACCEPTED (260 + 3 = 263), so at CW = 8 it is
  // clipped too, and to the same ceiling. Bins and total diverging is the
  // documented consequence of clipping, and bin_sat_o is the flag that says so.
  check(static_cast<uint32_t>(d.snap_total_o) == 255u,
        "sat: the interval total saturates as well, and does not wrap", 255, d.snap_total_o);
  check(static_cast<uint32_t>(d.events_o) == 255u, "sat: the event counter saturates too", 255,
        d.events_o);
  check(d.frozen_write_o == 0, "sat: frozen_write_o zero throughout", 0, d.frozen_write_o);
}

}  // namespace

int main() {
  auto* dut = new Vzhao_measure_histogram;  // heap, per the harness contract

  test_saturation(*dut);

  std::printf(
      "measure_histogram_sat: the SHIPPED RTL built with -GCW=8. The saturation\n"
      "  expression is the shipped one; only the ceiling is reachable.\n");

  const int rc = zhao::report_and_exit("measure_histogram_sat");
  zhao::exit_hard(rc);
}
