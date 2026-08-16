// video_scanout_random.cpp — VIDEO.SCANOUT randomized differential (plan
// W2.2; spec/video_rules.md §8; contract VIDEO.SCANOUT "Randomized
// differential tests"): PCG slot-READY timelines (incl. jittered blit
// completion), deadlines, guard-service blackouts (line-underrun storms)
// and rare mode switches — the displayed pixel stream, fetch order and
// swap/repeat decisions must be bit-exact vs zref::Scanout every cycle.
//
//   fast:    1,000 events
//   nightly: 100,000 events  (--full; dense READY flapping, contract
//              VIDEO.FRAMECTL "adversarial READY flapping" in the same run)
// Run-twice determinism: every scenario runs twice, trace hashes compared.

#include <cstdio>
#include <cstring>

#include "video_harness.hpp"

using namespace zref;
using zhao_video::Rng;
using zhao_video::VideoTb;

// spacing between events: fast runs spread them (more frames per event);
// nightly concentrates them (dense adversarial flapping)
static bool g_full = false;
static uint32_t event_spacing() { return g_full ? 40u : 2000u; }

static uint64_t scenario(uint64_t seed, uint64_t events) {
  VideoTb tb;
  tb.reset();
  Rng rng(seed);
  // patterned canvases (both slots) so any fetch/geometry error shows
  std::vector<uint8_t> c0(canvas_bytes(2));
  std::vector<uint8_t> c1(canvas_bytes(2));
  for (size_t i = 0; i < c0.size(); ++i) {
    c0[i] = (uint8_t)(i * 7u + 0x5Au);
    c1[i] = (uint8_t)(i * 13u + 0xC3u);
  }
  tb.resp.set_canvas(0, c0);
  tb.resp.set_canvas(1, c1);

  uint64_t done = 0;
  uint32_t countdown = 0;
  while (done < events) {
    if (countdown == 0) {
      // one event: READY flap, deadline change, service blackout toggle,
      // or a rare mode switch (mode writes only take effect at frame
      // start — the latch law is part of the compared trace)
      const uint32_t roll = rng.u32() % 100u;
      if (roll < 45u) {
        tb.slot_ready = rng.u32() & 3u;               // READY timeline
      } else if (roll < 60u) {
        static const uint32_t dl[] = {0, 0, 0, 2000, 8000, 40000, 300000};
        tb.deadline_cycles = dl[rng.u32() % 7u];
      } else if (roll < 80u) {
        tb.resp.set_service((rng.u32() & 3u) != 0);   // starvation storms
      } else if (roll < 90u && !tb.mode_we) {
        tb.mode_we = true;                            // rare mode switch
        tb.mode_in = rng.u32() % 3u;
      } else if (tb.mode_we) {
        tb.mode_we = false;
      }
      ++done;
      countdown = 1 + (rng.next() % event_spacing());
    }
    tb.step();
    --countdown;
  }
  return tb.trace.h;
}

int main(int argc, char** argv) {
  g_full = argc > 1 && 0 == std::strcmp(argv[1], "--full");
  const uint64_t events = g_full ? 100000u : 1000u;

  int fail = 0;
  const uint64_t seeds = g_full ? 2 : 2;
  for (uint64_t seed = 1; seed <= seeds; ++seed) {
    const uint64_t h1 = scenario(seed, events);
    const uint64_t h2 = scenario(seed, events);
    if (h1 != h2) {
      ++fail;
      std::printf("FAIL scanout_random seed %llu: run-twice hash mismatch\n",
                  (unsigned long long)seed);
    }
  }
  if (fail == 0) {
    std::printf("video_scanout_random: OK (%s, %llu events x %llu x 2)\n",
                g_full ? "full" : "fast", (unsigned long long)events,
                (unsigned long long)seeds);
    return 0;
  }
  std::printf("video_scanout_random: %d FAILURES\n", fail);
  return 1;
}
