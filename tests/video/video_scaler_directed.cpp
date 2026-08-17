// video_scaler_directed.cpp — VIDEO.SCALER directed tests (plan W2.2; law:
// spec/video_rules.md §6; contract VIDEO.SCALER "Directed tests").
//
// Coverage:
//   * 3 frames per mode: output stream == input stream delayed exactly 2
//     vid cycles (the zref::ScalerFeed identity), sync mapping identical —
//     driven with a synthetic raster-shaped stream (Z60/Storm/Duo active
//     widths, syncs, blanks).
//   * reset flush: output invalid until the pipeline fills.
//   * backpressure hold: out_ready deasserted -> the last pixel is HELD
//     (no loss, no duplication on the output port; the stall propagates
//     upstream per the contract).
//   * protocol check: a conforming stream never trips never_active; a
//     deliberate valid-outside-active violation sticks (no silent
//     fallback).
//   * run-twice determinism (plan R1): the scenario trace hashes equal.

#include <cstdio>

#include "scaler_harness.hpp"

using namespace zref;
using zhao_video::ScalerTb;

static int g_fail = 0;
#define EXPECT(cond)                                              \
  do {                                                            \
    if (!(cond)) {                                                \
      ++g_fail;                                                   \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                             \
  } while (0)

// synthetic raster-shaped stream sample for cycle i within a frame
static PxStream raster_sample(uint32_t mode, uint32_t i) {
  const VidTiming& t = vid_timing(mode);
  const uint32_t frame = t.h_total * t.v_total;
  const uint32_t c = i % frame;
  const uint32_t x = c % t.h_total;
  const uint32_t y = c / t.h_total;
  PxStream p;
  p.hblank = x >= t.h_active;
  p.vblank = y >= t.v_active;
  p.valid = !p.hblank && !p.vblank;
  p.hsync = (x >= t.h_active + t.h_front) && (x < t.h_active + t.h_front + t.h_sync);
  p.vsync = (y >= t.v_active + t.v_front) && (y < t.v_active + t.v_front + t.v_sync);
  p.x = x & 0x3FF;
  p.y = y & 0xFF;
  p.rgb565 = p.valid ? (uint16_t)((x * 7u + y * 13u) ^ (x * y) ^ (mode * 0x1111u)) : (uint16_t)0;
  return p;
}

// the whole directed scenario; returns the trace hash (run twice)
static uint64_t scenario() {
  ScalerTb tb;

  // ---- reset flush: output invalid until the pipeline fills -------------
  tb.reset();
  EXPECT(!tb.top.out_valid);
  EXPECT(!tb.top.never_active);

  // ---- 3 frames per mode: identity with a 2-cycle delay -----------------
  for (uint32_t mode = 0; mode < 3; ++mode) {
    const VidTiming& t = vid_timing(mode);
    const uint64_t frame_cycles = (uint64_t)t.h_total * t.v_total;
    for (uint64_t i = 0; i < 3 * frame_cycles; ++i) {
      tb.step(raster_sample(mode, (uint32_t)i), true);
    }
    EXPECT(tb.failures == 0);
    EXPECT(!tb.top.never_active);
  }
  g_fail += tb.failures;

  // ---- backpressure hold: sink stalls for 40 cycles ---------------------
  // find an active pixel, then freeze the sink; the output must hold its
  // last pixel and the identity law must hold through the stall (the
  // zref::ScalerFeed freeze models it: incoming pixels are dropped, the
  // output port never loses or duplicates a pixel it emitted)
  ScalerTb tb2;
  tb2.reset();
  uint64_t i = 0;
  while (!tb2.top.out_valid) tb2.step(raster_sample(0, (uint32_t)i++), true);
  const uint16_t held = tb2.top.out_rgb;
  for (int k = 0; k < 40; ++k) tb2.step(raster_sample(0, (uint32_t)i++), false);
  // the held pixel is still on the port during the whole stall
  tb2.step(raster_sample(0, (uint32_t)i), false);
  EXPECT(tb2.top.out_valid != 0);
  EXPECT(tb2.top.out_rgb == held);
  // release: the pipeline resumes and the identity law keeps holding
  for (int k = 0; k < 100; ++k) tb2.step(raster_sample(0, (uint32_t)i++), true);
  EXPECT(tb2.failures == 0);
  g_fail += tb2.failures;

  // ---- protocol check: violation sticks ---------------------------------
  ScalerTb tb3;
  tb3.reset();
  PxStream bad;
  bad.valid = 1;
  bad.hblank = 1;  // valid outside the active window: impossible input
  for (int k = 0; k < 4 && !tb3.top.never_active; ++k) tb3.step(bad, true);
  EXPECT(tb3.top.never_active != 0);  // sticky, no silent fallback
  EXPECT(tb3.failures == 0);
  g_fail += tb3.failures;

  return tb.trace.h ^ (tb2.trace.h * 3) ^ (tb3.trace.h * 7);
}

int main() {
  const uint64_t h1 = scenario();
  const uint64_t h2 = scenario();
  EXPECT(h1 == h2);  // run-twice determinism (plan R1)
  if (h1 != h2)
    std::printf("  trace hash %llx != %llx\n", (unsigned long long)h1, (unsigned long long)h2);

  if (g_fail == 0) {
    std::printf("video_scaler_directed: OK\n");
    zhao::exit_hard(0);  // teardown-deadlock workaround (zhao_sim.hpp)
  }
  std::printf("video_scaler_directed: %d FAILURES\n", g_fail);
  zhao::exit_hard(1);  // teardown-deadlock workaround (zhao_sim.hpp)
}
