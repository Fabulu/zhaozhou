// video_mode_random.cpp — VIDEO.MODE randomized differential (plan W2.2;
// spec/video_rules.md §8; contract VIDEO.MODE "Randomized differential
// tests"): PCG mode-switch timelines at random cycle offsets; the per-cycle
// trace (x, y, syncs, blanks, frame boundaries, mode latch) must equal the
// zref::VideoMode oracle (embedded in the composed zref::Scanout mirror
// that video_harness.hpp drives) exactly.
//
//   fast:    1,000 mode-write events
//   nightly: 100,000 events  (--full)
// Run-twice determinism: every scenario runs twice, trace hashes compared.

#include <cstdio>
#include <cstring>

#include "video_harness.hpp"

using namespace zref;
using zhao_video::Rng;
using zhao_video::VideoTb;

// MERGE FIX: the salvaged version returned only the trace hash and IGNORED
// tb.failures — an RTL/oracle mismatch diverges identically on both runs,
// so the run-twice hash still matched and the differential half of this
// lane was vacuous. The per-cycle mismatch count now reaches the exit code.
static uint64_t scenario(uint64_t seed, uint64_t events, int* fails) {
  VideoTb tb;
  tb.reset();
  Rng rng(seed);
  // keep both slots identical: the scanout side keeps fetching whatever
  // geometry the mode switches produce (the mode test's focus is the
  // raster trace; the full-subsystem driver still compares everything)
  std::vector<uint8_t> c(canvas_bytes(2), (uint8_t)(0x40 + seed));
  tb.resp.set_canvas(0, c);
  tb.resp.set_canvas(1, c);

  uint64_t done = 0;
  uint32_t countdown = 0;
  while (done < events) {
    if (countdown == 0) {
      // one event per expiry: a mode write held a full vid cycle (both
      // step parities) then released — the value includes the rogue 3
      if (tb.mode_we) {
        tb.mode_we = false;
      } else {
        tb.mode_we = true;
        tb.mode_in = rng.u32() & 3u;
        ++done;
      }
      countdown = 1 + (rng.next() % 500u);
    }
    tb.step();
    --countdown;
  }
  *fails += tb.failures;
  return tb.trace.h;
}

int main(int argc, char** argv) {
  const bool full = argc > 1 && 0 == std::strcmp(argv[1], "--full");
  const uint64_t events = full ? 100000u : 1000u;

  int fail = 0;
  const uint64_t seeds = full ? 3 : 2;
  for (uint64_t seed = 1; seed <= seeds; ++seed) {
    int diff = 0;
    const uint64_t h1 = scenario(seed, events, &diff);
    const uint64_t h2 = scenario(seed, events, &diff);
    if (h1 != h2) {
      ++fail;
      std::printf("FAIL mode_random seed %llu: run-twice hash mismatch\n",
                  (unsigned long long)seed);
    }
    if (diff != 0) {
      ++fail;
      std::printf("FAIL mode_random seed %llu: %d differential mismatches\n",
                  (unsigned long long)seed, diff);
    }
  }
  if (fail == 0) {
    std::printf("video_mode_random: OK (%s, %llu events x %llu seeds x 2)\n",
                full ? "full" : "fast", (unsigned long long)events,
                (unsigned long long)seeds);
    zhao::exit_hard(0);  // teardown-deadlock workaround (zhao_sim.hpp)
  }
  std::printf("video_mode_random: %d FAILURES\n", fail);
  zhao::exit_hard(1);  // teardown-deadlock workaround (zhao_sim.hpp)
}
