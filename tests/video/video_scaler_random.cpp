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

// MERGE FIX: tb.failures now reaches the exit code (see video_mode_random).
static uint64_t scenario(uint64_t seed, uint64_t cycles, int* fails) {
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
    const bool ready = (rng.next() & 3u) != 0;  // 25% stall duty
    tb.step(px, ready);
  }
  *fails += tb.failures;
  return tb.trace.h;
}

int main(int argc, char** argv) {
  const bool full = argc > 1 && 0 == std::strcmp(argv[1], "--full");
  const uint64_t cycles = full ? 100000u : 20000u;

  int fail = 0;
  for (uint64_t seed = 1; seed <= 4; ++seed) {
    int diff = 0;
    const uint64_t h1 = scenario(seed, cycles, &diff);
    const uint64_t h2 = scenario(seed, cycles, &diff);
    if (h1 != h2) {
      ++fail;
      std::printf("FAIL scaler_random seed %llu: run-twice hash mismatch\n",
                  (unsigned long long)seed);
    }
    if (diff != 0) {
      ++fail;
      std::printf("FAIL scaler_random seed %llu: %d differential mismatches\n",
                  (unsigned long long)seed, diff);
    }
  }
  if (fail == 0) {
    std::printf("video_scaler_random: OK (%s, %llu cycles x 4 seeds x 2)\n", full ? "full" : "fast",
                (unsigned long long)cycles);
    zhao::exit_hard(0);  // teardown-deadlock workaround (zhao_sim.hpp)
  }
  std::printf("video_scaler_random: %d FAILURES\n", fail);
  zhao::exit_hard(1);  // teardown-deadlock workaround (zhao_sim.hpp)
}
