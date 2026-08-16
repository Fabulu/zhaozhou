// video_scanout_directed.cpp — VIDEO.SCANOUT directed tests (plan W2.2;
// law: spec/video_rules.md §3-§4; contract VIDEO.SCANOUT "Directed tests").
//
// Coverage:
//   * 3 clean single-colour frames: swap at vblank; the displayed stream
//     CRC-32C equals zref::framePixelCrc over the displayed slot's canvas
//     (the Displayed-CRC law, spec §4).
//   * FORCED MISSED DEADLINE: slot READY lands with the deadline window
//     closed -> previous complete frame repeats, deadline_faults++,
//     `repeated` pulses, and the repeated frame's displayed CRC is
//     BIT-IDENTICAL to the prior frame's — the mechanical proof of the
//     60 Hz law (never partially displayed, spec §4).
//   * LINE-UNDERRUN INJECTION: guard service blacked out mid-frame with a
//     patterned canvas -> starvation counter advances, and every displayed
//     line is either a COMPLETE canvas row or a held-pixel line (never a
//     mix — never torn).
//   * Duo canvas map: 48 border rows black, two 256x192 views side by side
//     at x 0/256, y 24..215 (spec §3.1); displayed CRC equals the composed
//     oracle CRC.
//   * Full per-cycle differential vs zref::Scanout (the VideoTb driver).
//
// Fault accounting note (spec §4): EVERY vblank without a committed READY
// slot repeats and counts deadline_faults once — including the un-armed
// frames between directed swaps. The assertions below track exact deltas.

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "zhao_abi.h"
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

// patterned canvas: pixel (x,y) -> RGB565 for the never-torn check
static uint16_t pattern_px(uint32_t x, uint32_t y) {
  return (uint16_t)((x * 7u + y * 13u) ^ (x * y));
}

static std::vector<uint8_t> pattern_canvas(uint32_t mode) {
  const uint32_t w = active_width(mode);
  std::vector<uint8_t> c(canvas_bytes(mode), 0);
  for (uint32_t y = 0; y < 240; ++y) {
    for (uint32_t x = 0; x < w; ++x) {
      const uint16_t p = pattern_px(x, y);
      if (mode != 2) {
        c[(size_t)(y * w + x) * 2 + 0] = (uint8_t)(p & 0xFF);
        c[(size_t)(y * w + x) * 2 + 1] = (uint8_t)(p >> 8);
      } else {
        if (y < 24 || y >= 216) continue;
        const uint32_t sy = y - 24;
        const size_t off = (x < 256)
                               ? (size_t)(sy * 256 + x) * 2
                               : 0x18000u + (size_t)(sy * 256 + (x - 256)) * 2;
        c[off + 0] = (uint8_t)(p & 0xFF);
        c[off + 1] = (uint8_t)(p >> 8);
      }
    }
  }
  return c;
}

// collect exactly one displayed frame (between frame_tick pulses) from the
// post-scaler stream; bytes arrive in raster order
struct FrameCap {
  std::vector<uint8_t> bytes;
  uint32_t mode = 0;
};

// max_steps must cover the reset walk (~124k vid) PLUS at least two frame
// periods (Duo: 318,592 gpu each) — 1.2M gpu steps is two full Duo frames
// of margin after the first tick
static FrameCap capture_frame(VideoTb& tb, uint64_t max_steps = 1200000) {
  FrameCap cap;
  bool started = false;
  uint64_t steps = 0;
  while (steps < max_steps) {
    tb.step();
    ++steps;
    if (!tb.vid_edge()) continue;          // sample once per vid cycle
    if (tb.top.o_frame_tick) {
      if (started) break;
      started = true;
      cap.mode = tb.top.o_mode;   // the mode the NEXT frame displays under
      continue;
    }
    if (started && tb.top.o_px_valid) {
      cap.bytes.push_back((uint8_t)(tb.top.o_px_rgb & 0xFF));
      cap.bytes.push_back((uint8_t)(tb.top.o_px_rgb >> 8));
    }
  }
  return cap;
}

static uint32_t crc_of(const FrameCap& c) {
  return zhao_abi::zhao_crc32c(0, c.bytes.data(), c.bytes.size());
}

// arm `slot`: wait until the raster is INSIDE a frame (safely past any tick
// cycle), hold READY until the next swap decision, then release. Arming
// during the dec cycle itself would latch a commit that the same-edge
// decision discards (the dec branch clears it) — one frame late.
static void arm_and_swap(VideoTb& tb, uint32_t slot) {
  // leave any current tick cycle and wait until the raster is displaying
  while (!(tb.vid_edge() && !tb.top.o_frame_tick && tb.top.o_y < 200)) {
    tb.step();
  }
  tb.slot_ready = (slot == 0) ? 1u : 2u;
  while (!(tb.vid_edge() && tb.top.o_frame_tick)) tb.step();  // the decision
  tb.step();
  tb.step();
  tb.slot_ready = 0;
}

static uint64_t scenario() {
  uint64_t hsum = 0;
  // ------------------------------------------------- clean frames + CRC ---
  {
    VideoTb tb;
    tb.reset();
    const uint32_t w = active_width(0);
    std::vector<uint8_t> c0(canvas_bytes(0), 0x30);   // slot 0: dark
    std::vector<uint8_t> c1(canvas_bytes(0), 0xC0);   // slot 1: light
    tb.resp.set_canvas(0, c0);
    tb.resp.set_canvas(1, c1);

    // frame 0 (no READY ever): slot 0 free-runs from reset, dec0 repeats
    FrameCap f0 = capture_frame(tb);
    std::printf("f0: %zu bytes (want %u) starve=%llu faults=%llu\n",
                f0.bytes.size(), 2u * w * 240u,
                (unsigned long long)tb.top.o_starvation,
                (unsigned long long)tb.top.o_deadline_faults);
    EXPECT(f0.bytes.size() == 2u * w * 240u);
    EXPECT(crc_of(f0) == frame_pixel_crc(0, c0));

    // clean handoff: arm slot 1 -> swap at vblank -> slot 1 displays
    arm_and_swap(tb, 1);
    FrameCap f1 = capture_frame(tb);
    std::printf("f1: %zu bytes first=%02x%02x faults=%llu starve=%llu\n",
                f1.bytes.size(), f1.bytes[0], f1.bytes[1],
                (unsigned long long)tb.top.o_deadline_faults,
                (unsigned long long)tb.top.o_starvation);
    EXPECT(f1.bytes.size() == 2u * w * 240u);
    EXPECT(crc_of(f1) == frame_pixel_crc(0, c1));

    // and back to slot 0
    arm_and_swap(tb, 0);
    FrameCap f2 = capture_frame(tb);
    EXPECT(f2.bytes.size() == 2u * w * 240u);
    EXPECT(crc_of(f2) == frame_pixel_crc(0, c0));
    // repeats so far: dec0/dec1 (before the first arm), dec3 (f1's close),
    // dec5 (f2's close) — every un-armed vblank repeats (spec §4)
    EXPECT(tb.top.o_deadline_faults == 4);
    EXPECT(tb.top.o_starvation == 0);

    // ---------------------------------------- forced missed deadline ----
    // close the window early (4096 gpu = 2048 vid after frame_start);
    // the READY lands past the shut window -> repeat + fault (spec §4)
    tb.deadline_cycles = 4096;
    while (!tb.top.o_frame_start) tb.step();
    for (int i = 0; i < 5000; ++i) tb.step();   // > 2048 vid cycles
    const uint64_t faults_before = tb.top.o_deadline_faults;
    tb.slot_ready = 2;                          // slot 1, LATE
    while (!tb.top.o_frame_tick) tb.step();     // the repeat decision
    tb.step();
    tb.step();
    tb.slot_ready = 0;
    tb.deadline_cycles = 0;
    EXPECT(tb.top.o_deadline_faults == faults_before + 1);

    // the 60 Hz law, mechanically: the repeated frame's displayed CRC is
    // bit-identical to the previous display of the SAME slot (slot 0)
    FrameCap fr = capture_frame(tb);
    EXPECT(fr.bytes.size() == 2u * w * 240u);
    EXPECT(crc_of(fr) == crc_of(f2));
    EXPECT(crc_of(fr) == frame_pixel_crc(0, c0));

    // recovery: an on-time READY swaps again
    arm_and_swap(tb, 1);
    FrameCap f3 = capture_frame(tb);
    EXPECT(crc_of(f3) == frame_pixel_crc(0, c1));
    EXPECT(tb.top.o_starvation == 0);

    EXPECT(tb.failures == 0);
    g_fail += tb.failures;
    hsum = hsum * 1315423911ull + tb.trace.h;
  }

  // ------------------------------------------------- line-underrun law ---
  {
    VideoTb tb;
    tb.reset();
    const uint32_t w = active_width(0);
    tb.resp.set_canvas(0, pattern_canvas(0));
    tb.resp.set_canvas(1, pattern_canvas(0));
    capture_frame(tb);   // settle into clean display

    // black the guard service out for ~40 lines, capture every line,
    // then restore and confirm exact display again
    tb.resp.set_service(false);
    std::vector<std::vector<uint16_t>> lines;
    std::vector<uint16_t> cur;
    bool in_line = false;
    uint64_t starve_at_blackout = tb.top.o_starvation;
    for (uint64_t i = 0; i < 70ull * 480u * 2u; ++i) {
      tb.step();
      if (!tb.vid_edge()) continue;        // sample once per vid cycle
      if (tb.top.o_px_valid) {
        if (!in_line) { in_line = true; cur.clear(); }
        cur.push_back((uint16_t)tb.top.o_px_rgb);
      } else if (in_line) {
        in_line = false;
        if (cur.size() == w) lines.push_back(cur);
      }
    }
    tb.resp.set_service(true);

    // starvation advanced during the blackout (visible, spec §4)
    EXPECT(tb.top.o_starvation > starve_at_blackout);
    if (lines.size() < 40)
      std::printf("  captured %zu full lines in the blackout window\n",
                  lines.size());
    EXPECT(lines.size() >= 40);

    // never torn: every displayed line is a COMPLETE canvas row (any row —
    // starvation may lag lines) or a held-pixel (constant) line
    uint32_t torn = 0;
    for (const auto& ln : lines) {
      bool full_row = false;
      for (uint32_t y = 0; y < 240 && !full_row; ++y) {
        uint32_t x = 0;
        while (x < w && ln[x] == pattern_px(x, y)) ++x;
        if (x == w) full_row = true;
      }
      bool held = true;
      for (uint32_t x = 1; x < w; ++x) {
        if (ln[x] != ln[0]) held = false;
      }
      if (!full_row && !held) ++torn;
    }
    EXPECT(torn == 0);
    if (torn) std::printf("  torn lines: %u\n", torn);

    // after restoration, let the starved frame flush out, then confirm the
    // displayed CRC is exact again
    while (!(tb.vid_edge() && tb.top.o_frame_tick)) tb.step();  // a full tick
    FrameCap fr2 = capture_frame(tb);
    EXPECT(fr2.bytes.size() == 2u * w * 240u);
    EXPECT(crc_of(fr2) == frame_pixel_crc(0, pattern_canvas(0)));
    EXPECT(tb.failures == 0);
    g_fail += tb.failures;
    hsum = hsum * 2654435761ull + tb.trace.h;
  }

  // ------------------------------------------------- Duo canvas map -----
  {
    VideoTb tb;
    tb.reset();
    tb.resp.set_canvas(0, pattern_canvas(2));
    tb.resp.set_canvas(1, pattern_canvas(2));
    tb.mode_we = true;
    tb.mode_in = 2;
    tb.step();
    tb.step();
    tb.mode_we = false;
    capture_frame(tb);   // the latch frame (Z60 timing -> Duo at frame_start)
    FrameCap fd = capture_frame(tb);
    const uint32_t w = active_width(2);
    EXPECT(fd.bytes.size() == 2u * w * 240u);
    if (fd.bytes.size() == 2u * w * 240u) {
      EXPECT(crc_of(fd) == frame_pixel_crc(2, pattern_canvas(2)));

      // border rows black (spec §3.1); views land at their offsets
      uint32_t border_bad = 0, view_bad = 0;
      for (uint32_t y = 0; y < 240; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
          const size_t i = (size_t)(y * w + x) * 2;
          const uint16_t got =
              (uint16_t)(fd.bytes[i] | ((uint16_t)fd.bytes[i + 1] << 8));
          if (y < 24 || y >= 216) {
            if (got != 0) ++border_bad;
          } else if (got != pattern_px(x, y)) {
            ++view_bad;
          }
        }
      }
      EXPECT(border_bad == 0);
      EXPECT(view_bad == 0);
      if (border_bad) std::printf("  border mismatch: %u\n", border_bad);
      if (view_bad) {
        std::printf("  view mismatch: %u\n", view_bad);
        uint32_t shown = 0;
        for (uint32_t y = 24; y < 216 && shown < 6; ++y) {
          for (uint32_t x = 0; x < 512 && shown < 6; ++x) {
            const size_t i = (size_t)(y * w + x) * 2;
            const uint16_t got =
                (uint16_t)(fd.bytes[i] | ((uint16_t)fd.bytes[i + 1] << 8));
            if (got != pattern_px(x, y)) {
              std::printf("  first: y=%u x=%u got=%04x want=%04x\n", y, x,
                          got, pattern_px(x, y));
              ++shown;
            }
          }
        }
      }
    }

    EXPECT(tb.failures == 0);
    g_fail += tb.failures;
    hsum = hsum * 40503ull + tb.trace.h;
  }
  return hsum;
}

int main() {
  const uint64_t h1 = scenario();
  const uint64_t h2 = scenario();
  EXPECT(h1 == h2);   // run-twice determinism (plan R1)

  if (g_fail == 0) {
    std::printf("video_scanout_directed: OK\n");
    return 0;
  }
  std::printf("video_scanout_directed: %d FAILURES\n", g_fail);
  return 1;
}
