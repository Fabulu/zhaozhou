// video_mode_directed.cpp — VIDEO.MODE directed tests (plan W2.2; law:
// spec/video_rules.md §1-§2; contract VIDEO.MODE "Directed tests").
//
// Coverage:
//   * 3-frame cycle-for-cycle walk per mode (Z60, Storm, Duo) vs the
//     zref::VideoMode timing trace — x, y, syncs, blanks, frame_start,
//     frame_end, vswap_dec every vid cycle (the full-subsystem driver
//     compares the whole trace; this test additionally walks per-mode).
//   * frame period spot checks: exactly 251,520 / 217,984 / 318,592 gpu
//     cycles between successive frame_start pulses (spec §2 table).
//   * mode switch exactly at the next frame_start boundary: a mid-frame
//     mode_we changes NOTHING until frame_start; the timing constants then
//     change atomically (new h_total from the first line of the new frame).
//   * reset-idle: raster lands pre-active at (h_active+h_front+h_sync,
//     v_total - v_back) under reset value VIDEO_Z60.
//   * rogue mode value 3: holds the previous mode (contract "Overflow and
//     malformed-input behaviour").
//   * run-twice determinism (plan R1): the scenario trace hashes equal.

#include <cstdio>

#include "video_harness.hpp"

using namespace zref;
using zhao_video::VideoTb;

static int g_fail = 0;
#define EXPECT(cond)                                                        \
  do {                                                                      \
    if (!(cond)) {                                                          \
      ++g_fail;                                                             \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
    }                                                                       \
  } while (0)

// walk `frames` frame periods from the current position, measuring gpu
// cycles between frame_start pulses and checking exactly one per period
// (sampled at vid edges only: the pulse spans one vid cycle = 2 gpu steps)
static void walk_frames(VideoTb& tb, int frames, uint32_t expected_period) {
  uint64_t last_start = 0;
  bool started = false;
  int seen = 0;
  bool prev = false;
  while (seen < frames && tb.gpu_steps() < 400000ull * (uint64_t)(frames + 2)) {
    tb.mode_we = false;
    tb.step();
    const bool fs = tb.top.o_frame_start != 0;
    if (fs && !prev && (tb.gpu_steps() & 1ull)) {   // rising, at the vid edge
      if (started) {
        const uint64_t period = tb.gpu_steps() - last_start;
        EXPECT(period == expected_period);
        if (period != expected_period) {
          std::printf("  frame period %llu != %u\n",
                      (unsigned long long)period, expected_period);
        }
      }
      last_start = tb.gpu_steps();
      started = true;
      ++seen;
    }
    prev = fs;
  }
  EXPECT(seen == frames);
}

static uint64_t scenario() {
  uint64_t hsum = 0;
  // ------------------------------------------------ per-mode 3-frame walk --
  for (uint32_t mode = 0; mode < 3; ++mode) {
    VideoTb tb;
    tb.reset();
    // canvas so scanout fetches real data (single-colour frames)
    std::vector<uint8_t> canvas(canvas_bytes(mode));
    const uint8_t colour = (uint8_t)(0x11 * (mode + 1));
    for (auto& b : canvas) b = colour;
    tb.resp.set_canvas(0, canvas);
    tb.resp.set_canvas(1, canvas);

    // switch to `mode` right after reset (latches at the first frame_start)
    tb.mode_we = true;
    tb.mode_in = mode;
    tb.step();
    tb.mode_we = false;

    walk_frames(tb, 3, vid_timing(mode).frame_gpu_cycles);
    EXPECT(tb.top.o_mode == mode);

    // timing-table spot checks (spec §2): exactly one frame_start per
    // frame period was asserted above; spot-check raster bounds too
    EXPECT(tb.top.o_x < vid_timing(mode).h_total);
    EXPECT(tb.top.o_y < vid_timing(mode).v_total);
    EXPECT(tb.failures == 0);
    if (tb.failures) {
      std::printf("mode %u: %d trace mismatches\n", mode, tb.failures);
      g_fail += tb.failures;
    }
  }

  // --------------------------------------------------- mode switch law ----
  {
    VideoTb tb;
    tb.reset();
    std::vector<uint8_t> c0(canvas_bytes(0), 0x5A);
    std::vector<uint8_t> c2(canvas_bytes(2), 0xA5);
    tb.resp.set_canvas(0, c0);
    tb.resp.set_canvas(1, c2);

    // run to mid-frame of a Z60 frame (y=100), then write STORM mid-frame
    // (hold the write 2 gpu steps = 1 full vid cycle so the level is
    // sampled at a vid edge regardless of the step parity)
    while (!(tb.top.o_y == 100 && tb.top.o_x == 0)) tb.step();
    tb.mode_we = true;
    tb.mode_in = 1;
    tb.step();
    tb.step();
    tb.mode_we = false;
    const uint32_t mode_at_write = tb.top.o_mode;
    EXPECT(mode_at_write == 0);          // no mid-frame effect (spec §1.1)

    // the frame COMPLETES under Z60 (h_total stays 480 until frame_start)
    uint32_t max_x = 0;
    while (!tb.top.o_frame_start) {
      tb.step();
      if (tb.top.o_x > max_x) max_x = tb.top.o_x;
    }
    EXPECT(max_x == 479);                // old mode completed the frame
    EXPECT(tb.top.o_mode_next == 1);
    // mode_out latches exactly AT frame_start (the cycle the raster left
    // vblank): by now it must be STORM
    EXPECT(tb.top.o_mode == 1);

    // the new mode's timing is atomic from the first line: max x = 415
    tb.step();   // leave the frame_start pulse before re-walking
    tb.step();
    max_x = 0;
    while (!tb.top.o_frame_start) {
      tb.step();
      if (tb.top.o_x > max_x) max_x = tb.top.o_x;
    }
    EXPECT(max_x == 415);                // Storm h_total-1
    if (max_x != 415) std::printf("  max_x=%u (expected 415)\n", max_x);
    EXPECT(tb.failures == 0);
    g_fail += tb.failures;
  }

  // -------------------------------------------------------- reset-idle ----
  {
    VideoTb tb;
    tb.reset(2);
    // after reset the raster is pre-active at the start of both back
    // porches under VIDEO_Z60 (contract VIDEO.MODE)
    EXPECT(tb.top.o_x == 384 + 8 + 48);
    EXPECT(tb.top.o_y == 262 - 14);
    EXPECT(tb.top.o_mode == 0);
    EXPECT(tb.top.o_vblank != 0);
    EXPECT(tb.failures == 0);
    g_fail += tb.failures;
  }

  // --------------------------------------------------- rogue mode value ---
  {
    VideoTb tb;
    tb.reset();
    while (!tb.top.o_frame_start) tb.step();   // frame 0 boundary
    tb.mode_we = true;
    tb.mode_in = 3;                            // not a declared value
    tb.step();
    tb.step();
    tb.mode_we = false;
    EXPECT(tb.top.o_mode_next == 0);           // holds (last valid wins)
    tb.step();
    while (!tb.top.o_frame_start) tb.step();
    EXPECT(tb.top.o_mode == 0);                // still Z60 after the latch
    EXPECT(tb.failures == 0);
    g_fail += tb.failures;
    hsum = hsum * 37 + tb.trace.h;
  }
  return hsum;
}

int main() {
  const uint64_t h1 = scenario();
  const uint64_t h2 = scenario();
  EXPECT(h1 == h2);   // run-twice determinism (plan R1)
  if (h1 != h2)
    std::printf("  trace hash %llu != %llu\n",
                (unsigned long long)h1, (unsigned long long)h2);

  if (g_fail == 0) {
    std::printf("video_mode_directed: OK\n");
    zhao::exit_hard(0);  // teardown-deadlock workaround (zhao_sim.hpp)
  }
  std::printf("video_mode_directed: %d FAILURES\n", g_fail);
  zhao::exit_hard(1);  // teardown-deadlock workaround (zhao_sim.hpp)
}
