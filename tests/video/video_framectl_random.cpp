// video_framectl_random.cpp — VIDEO.FRAMECTL randomized differential (plan
// W2.2; spec/video_rules.md §8; contract VIDEO.FRAMECTL "Randomized
// differential tests"): PCG READY/deadline timelines vs zref::FrameCtl —
// swap/repeat/tick/fence schedule bit-exact, fence-exactly-once under
// adversarial READY flapping.
//
//   fast:    1,000 events
//   nightly: 100,000 events  (--full, dense flapping)
// Run-twice determinism (plan R1): every scenario runs twice.

#include <cstdio>
#include <cstring>

#include "video_harness.hpp"

using namespace zref;
using zhao_video::Rng;
using zhao_video::VideoTb;

static bool g_full = false;
static uint32_t event_spacing() { return g_full ? 40u : 1500u; }

// MERGE FIX: tb.failures now reaches the exit code (see video_mode_random).
static uint64_t scenario(uint64_t seed, uint64_t events, uint64_t* fences,
                         uint64_t* ticks, uint64_t* fault_frames, int* fails) {
  VideoTb tb;
  tb.reset();
  Rng rng(seed);
  std::vector<uint8_t> c(canvas_bytes(2), (uint8_t)(0x60 + seed));
  tb.resp.set_canvas(0, c);
  tb.resp.set_canvas(1, c);

  *fences = *ticks = *fault_frames = 0;
  uint64_t done = 0;
  uint32_t countdown = 0;
  while (done < events) {
    if (countdown == 0) {
      const uint32_t roll = rng.u32() % 100u;
      if (roll < 70u) {
        tb.slot_ready = rng.u32() & 3u;    // adversarial READY flapping
      } else {
        static const uint32_t dl[] = {0, 0, 1000, 4096, 20000, 100000};
        tb.deadline_cycles = dl[rng.u32() % 6u];
      }
      ++done;
      countdown = 1 + (rng.next() % event_spacing());
    }
    tb.step();
    if (tb.top.o_gpu_tick) ++(*fences);   // gpu-domain pulse: 1 gpu cycle
    if (tb.vid_edge() && tb.top.o_frame_tick) {
      ++(*ticks);
      if (tb.top.o_repeated) ++(*fault_frames);
    }
    --countdown;
  }
  *fails += tb.failures;
  return tb.trace.h;
}

int main(int argc, char** argv) {
  g_full = argc > 1 && 0 == std::strcmp(argv[1], "--full");
  const uint64_t events = g_full ? 100000u : 1000u;

  int fail = 0;
  for (uint64_t seed = 1; seed <= 2; ++seed) {
    uint64_t f1, t1, r1, f2, t2, r2;
    int diff = 0;
    const uint64_t h1 = scenario(seed, events, &f1, &t1, &r1, &diff);
    const uint64_t h2 = scenario(seed, events, &f2, &t2, &r2, &diff);
    if (diff != 0) {
      ++fail;
      std::printf("FAIL framectl_random seed %llu: %d differential mismatches\n",
                  (unsigned long long)seed, diff);
    }
    if (h1 != h2) {
      ++fail;
      std::printf("FAIL framectl_random seed %llu: hash mismatch\n",
                  (unsigned long long)seed);
    }
    // fence-exactly-once: one gpu-domain fence pulse per displayed frame
    if (f1 != t1 || f2 != t2) {
      ++fail;
      std::printf("FAIL framectl_random seed %llu: fences %llu != ticks %llu\n",
                  (unsigned long long)seed, (unsigned long long)f1,
                  (unsigned long long)t1);
    }
    std::printf("  seed %llu: %llu frames, %llu repeated (%s)\n",
                (unsigned long long)seed, (unsigned long long)t1,
                (unsigned long long)r1, g_full ? "full" : "fast");
  }
  if (fail == 0) {
    std::printf("video_framectl_random: OK (%s, %llu events x 2 seeds x 2)\n",
                g_full ? "full" : "fast", (unsigned long long)events);
    zhao::exit_hard(0);  // teardown-deadlock workaround (zhao_sim.hpp)
  }
  std::printf("video_framectl_random: %d FAILURES\n", fail);
  zhao::exit_hard(1);  // teardown-deadlock workaround (zhao_sim.hpp)
}
