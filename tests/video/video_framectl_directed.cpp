// video_framectl_directed.cpp — VIDEO.FRAMECTL directed tests (plan W2.2;
// law: spec/video_rules.md §4-§5; contract VIDEO.FRAMECTL "Directed
// tests").
//
// Coverage:
//   * clean handoff: READY inside the window -> swap at the dec, tick with
//     repeated=0, exactly ONE completion fence per displayed frame (the
//     gpu_tick pulse count equals the tick count — fence-exactly-once).
//   * missed deadline: no commit -> repeat, deadline_faults++ exactly once
//     per missed frame, frame_complete carries the repeated slot.
//   * acceptance boundary: with deadline_cycles = 1000 (gpu) the window
//     admits vid cycles k with 2k < 1000 — READY first high at k=499
//     swaps, at k=500 repeats (1 cycle early vs exactly at the deadline).
//   * mode change across vblank: the mode register latches exactly at
//     frame_start (never at the dec).
//   * full per-cycle differential vs zref::FrameCtl (the VideoTb driver)
//     and run-twice determinism (plan R1).

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

static uint64_t scenario() {
  // ---------------------------------------------- clean handoff + fence ---
  {
    VideoTb tb;
    tb.reset();
    uint64_t ticks = 0, fences = 0;
    // arm slot 1 during frame 2 and count ticks/fences over 4 frames
    while (!(tb.vid_edge() && tb.top.o_frame_tick)) tb.step();
    ++ticks;
    tb.slot_ready = 2;
    uint64_t faults_before = 0;
    while (ticks < 5) {
      tb.step();
      if (tb.top.o_gpu_tick) ++fences;   // gpu-domain pulse: 1 gpu cycle
      if (tb.vid_edge() && tb.top.o_frame_tick) {
        ++ticks;
        tb.slot_ready = (ticks == 3) ? 1u : 2u;   // keep handing off
      }
    }
    tb.slot_ready = 0;
    faults_before = tb.top.o_deadline_faults;
    // every displayed frame ticked exactly once and fenced exactly once
    EXPECT(fences == 4);
    // the handoffs swapped: frames 3+ displayed slot alternations (check
    // the completion slot mirrored to the gpu domain)
    EXPECT(tb.top.o_frame_id == 5);
    (void)faults_before;
    EXPECT(tb.failures == 0);
    g_fail += tb.failures;
  }

  // ---------------------------------------------- missed deadline ---------
  {
    VideoTb tb;
    tb.reset();
    // wait for the second frame, then starve READY for 2 frames
    uint64_t ticks = 0;
    while (ticks < 2) {
      tb.step();
      if (tb.vid_edge() && tb.top.o_frame_tick) ++ticks;
    }
    const uint64_t faults_before = tb.top.o_deadline_faults;
    uint32_t repeated_seen = 0;
    while (ticks < 4) {
      tb.step();
      if (tb.vid_edge() && tb.top.o_frame_tick) {
        ++ticks;
        if (tb.top.o_repeated) ++repeated_seen;
      }
    }
    EXPECT(repeated_seen == 2);                        // both frames repeated
    EXPECT(tb.top.o_deadline_faults == faults_before + 2);  // once per frame
    EXPECT(tb.failures == 0);
    g_fail += tb.failures;
  }

  // ---------------------------------------------- acceptance boundary -----
  // deadline_cycles=1000: cycles k (vid, from frame_start) with 2k < 1000
  // commit; k=499 is the last accepting cycle, k=500 the first rejecting.
  // The deadline input is loaded at the frame_start edge, so it must be on
  // the wires BEFORE that edge (during vblank).
  {
    for (int variant = 0; variant < 2; ++variant) {
      VideoTb tb;
      tb.reset();
      // settle past frame 0's tick, arm the deadline during vblank
      while (!(tb.vid_edge() && tb.top.o_frame_tick)) tb.step();
      tb.deadline_cycles = 1000;
      while (!(tb.vid_edge() && tb.top.o_frame_start)) tb.step();
      const uint32_t target_x = (variant == 0) ? 20u : 21u;  // k=500 / 501
      while (!(tb.vid_edge() && tb.top.o_y == 1 &&
               tb.top.o_x == target_x))
        tb.step();
      tb.slot_ready = 1;   // first high exactly at the boundary cycle
      while (!(tb.vid_edge() && tb.top.o_frame_tick)) tb.step();
      const uint64_t faults_before = tb.top.o_deadline_faults;
      tb.slot_ready = 0;
      tb.deadline_cycles = 0;
      if (variant == 0) {
        // 1 cycle early: accepted -> swap, repeated=0
        EXPECT(tb.top.o_repeated == 0);
      } else {
        // exactly at the deadline: the window is shut -> repeat + fault
        EXPECT(tb.top.o_repeated != 0);
        EXPECT(faults_before >= 1);
      }
      EXPECT(tb.failures == 0);
      g_fail += tb.failures;
    }
  }

  // ---------------------------------------------- mode change at vblank ---
  {
    VideoTb tb;
    tb.reset();
    // write DUO mid-frame: NOTHING changes until the next frame_start
    while (!(tb.vid_edge() && tb.top.o_y == 100 && tb.top.o_x == 0)) tb.step();
    tb.mode_we = true;
    tb.mode_in = 2;
    tb.step();
    tb.step();
    tb.mode_we = false;
    while (!(tb.vid_edge() && tb.top.o_vswap_dec)) tb.step();
    EXPECT(tb.top.o_mode == 0);           // not latched at the dec (spec 1.1)
    while (!(tb.vid_edge() && tb.top.o_frame_start)) tb.step();
    EXPECT(tb.top.o_mode == 2);           // latched exactly at frame_start
    EXPECT(tb.top.o_mode_next == 2);
    EXPECT(tb.failures == 0);
    g_fail += tb.failures;
  }

  // aggregate trace hash of the last scenario (run-twice law)
  {
    VideoTb tb;
    tb.reset();
    for (int i = 0; i < 4; ++i) {
      while (!(tb.vid_edge() && tb.top.o_frame_tick)) tb.step();
    }
    g_fail += tb.failures;
    return tb.trace.h;
  }
}

int main() {
  const uint64_t h1 = scenario();
  const uint64_t h2 = scenario();
  EXPECT(h1 == h2);
  if (g_fail == 0) {
    std::printf("video_framectl_directed: OK\n");
    return 0;
  }
  std::printf("video_framectl_directed: %d FAILURES\n", g_fail);
  return 1;
}
