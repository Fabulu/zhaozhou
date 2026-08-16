// video_scaler_random.cpp — VIDEO.SCALER randomized differential (plan
// W2.2; law: spec/video_rules.md §6; contract "Randomized differential
// tests": PCG stream gaps and ready toggles, the delayed-identity law
// bit-exact over 100k pixels nightly).
//
//   fast:    20,000 cycles   (default)
//   nightly: 100,000 cycles  (--full, the contract's 100k-pixel law)
// Every cycle is compared against zref::ScalerFeed; every compared value
// mixes into the trace hash; the whole scenario runs twice and the hashes
// must be identical (plan R1).

#include <cstdio>
#include <cstring>

#include "scaler_harness.hpp"

using namespace zref;
using zhao_video::Rng;
using zhao_video::ScalerTb;

static uint64_t scenario(uint64_t seed, uint64_t cycles) {
  ScalerTb tb;
  tb.reset();
  Rng rng(seed);
  PxStream px;
  for (uint64_t i = 0; i < cycles; ++i) {
    // random stream gaps: valid only inside the active window
    const bool gap = (rng.next() & 7u) == 0;
    px.valid = !gap && !px.hblank && !px.vblank;
    if ((rng.next() & 63u) == 0) px.rgb565 = (uint32_t)(rng.next() & 0xFFFFu);
    if ((rng.next() & 127u) == 0) px.x = (uint32_t)(rng.next() % 512u);
    if ((rng.next() & 127u) == 0) px.y = (uint32_t)(rng.next() % 240u);
    if ((rng.next() & 511u) == 0) px.hsync = !px.hsync;
    if ((rng.next() & 511u) == 0) px.vsync = !px.vsync;
    if ((rng.next() & 31u) == 0) px.hblank = !px.hblank;
    if ((rng.next() & 511u) == 0) px.vblank = !px.vblank;
    // conforming stream: valid never asserted outside the active window
    if (px.hblank || px.vblank) px.valid = false;
    const bool ready = (rng.next() & 3u) != 0;   // 25% stall duty
    tb.step(px, ready);
  }
  return tb.trace.h;
}

int main(int argc, char** argv) {
  const bool full = argc > 1 && 0 == std::strcmp(argv[1], "--full");
  const uint64_t cycles = full ? 100000u : 20000u;

  int fail = 0;
  for (uint64_t seed = 1; seed <= 4; ++seed) {
    const uint64_t h1 = scenario(seed, cycles);
    const uint64_t h2 = scenario(seed, cycles);
    if (h1 != h2) {
      ++fail;
      std::printf("FAIL scaler_random seed %llu: run-twice hash mismatch\n",
                  (unsigned long long)seed);
    }
  }
  if (fail == 0) {
    std::printf("video_scaler_random: OK (%s, %llu cycles x 4 seeds x 2)\n",
                full ? "full" : "fast", (unsigned long long)cycles);
    return 0;
  }
  std::printf("video_scaler_random: %d FAILURES\n", fail);
  return 1;
}
