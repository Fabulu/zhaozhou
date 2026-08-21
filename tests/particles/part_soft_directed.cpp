// part_soft_directed.cpp — PART.SOFT against `zref::part::soft_rect`, and that
// reference against the renderer it was extracted from.
//
// TWO LAYERS, same discipline as PART.EXPAND and for the same reason:
// `blit_pattern_block` computes the rectangle and immediately paints it, so the
// reference had to RESTATE the geometry and a restated law is a second
// implementation.
//
//   LAYER 1 — the reference IS the renderer. The same population is drawn twice:
//   once through `draw_population` with the POINTS flag, and once by painting
//   `soft_rect`'s rectangle with the same depth test. Identical surfaces, or the
//   header has drifted from sprites.cpp.
//
//   LAYER 2 — the RTL is the reference.
//
// THE LAW THIS FILE EXISTS FOR: the two edges round differently. The low edge
// ceils, the high edge floors — pixel-centre coverage. Rounding both the same way
// is the obvious tidy-up and makes every sprite a pixel wrong on one side only.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_part_soft.h"

#include "zhao_sim.hpp"
#include "zref/zref_particle_soft.hpp"
#include "zref/zref_render.hpp"
#include "zrender/internal.hpp"

namespace {

using zhao::check;
namespace zp = zref::part;
namespace zr = zref::render;

constexpr int32_t kOne = 1 << 16;

int32_t sx13(uint32_t v) { return static_cast<int32_t>(v << 19) >> 19; }

struct P {
  bool in;
  int32_t x, y, d;
  uint8_t size, r, g, b;
  uint16_t src;
};

struct VP {
  int32_t x0, y0, w, h;
};

class Dut {
 public:
  explicit Dut(Vzhao_part_soft& d) : dut_(d) { reset(); }

  void reset() {
    dut_.rst_n = 0;
    dut_.p_valid_i = 0;
    dut_.s_ready_i = 1;
    dut_.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(dut_);
    dut_.rst_n = 1;
    dut_.eval();
  }

  void set_vp(const VP& v) {
    dut_.vp_x0_i = static_cast<uint32_t>(v.x0);
    dut_.vp_y0_i = static_cast<uint32_t>(v.y0);
    dut_.vp_w_i = static_cast<uint32_t>(v.w);
    dut_.vp_h_i = static_cast<uint32_t>(v.h);
    dut_.eval();
  }

  bool emit(const P& p, zp::SoftRect& got, uint16_t& src, bool& dtest, bool& dwrite) {
    dut_.p_valid_i = 1;
    dut_.p_in_i = p.in ? 1 : 0;
    dut_.p_x_i = static_cast<uint32_t>(p.x);
    dut_.p_y_i = static_cast<uint32_t>(p.y);
    dut_.p_d_i = static_cast<uint32_t>(p.d);
    dut_.p_size_i = p.size;
    dut_.p_r_i = p.r;
    dut_.p_g_i = p.g;
    dut_.p_b_i = p.b;
    dut_.p_src_id_i = p.src;
    dut_.s_ready_i = 1;
    dut_.eval();
    int guard = 0;
    while (!dut_.p_ready_o && guard++ < 64) {
      zhao::tick(dut_);
      dut_.eval();
    }
    zhao::tick(dut_);
    dut_.p_valid_i = 0;
    dut_.eval();

    if (!dut_.s_valid_o) {
      got = zp::SoftRect{};
      return false;
    }
    got.covered = true;
    got.min_x = sx13(dut_.s_min_x_o);
    got.max_x = sx13(dut_.s_max_x_o);
    got.min_y = sx13(dut_.s_min_y_o);
    got.max_y = sx13(dut_.s_max_y_o);
    src = static_cast<uint16_t>(dut_.s_src_id_o);
    dtest = dut_.s_depth_test_o != 0;
    dwrite = dut_.s_depth_write_o != 0;
    zhao::tick(dut_);
    dut_.eval();
    return true;
  }

  uint32_t counted() const { return dut_.soft_particles_o; }

 private:
  Vzhao_part_soft& dut_;
};

void diff(Dut& dut, const VP& v, const P& p, const char* what) {
  const zp::SoftRect want =
      zp::soft_rect(p.in, p.x, p.y, p.size, v.x0, v.y0, v.w, v.h);
  dut.set_vp(v);
  zp::SoftRect got{};
  uint16_t src = 0;
  bool dtest = false, dwrite = true;
  const bool emitted = dut.emit(p, got, src, dtest, dwrite);

  const std::string t(what);
  check(emitted == want.covered, (t + ": covered at all").c_str(), want.covered ? 1 : 0,
        emitted ? 1 : 0);
  if (!want.covered || !emitted) return;

  check(got.min_x == want.min_x, (t + ": min_x").c_str(), static_cast<uint32_t>(want.min_x),
        static_cast<uint32_t>(got.min_x));
  check(got.max_x == want.max_x, (t + ": max_x").c_str(), static_cast<uint32_t>(want.max_x),
        static_cast<uint32_t>(got.max_x));
  check(got.min_y == want.min_y, (t + ": min_y").c_str(), static_cast<uint32_t>(want.min_y),
        static_cast<uint32_t>(got.min_y));
  check(got.max_y == want.max_y, (t + ": max_y").c_str(), static_cast<uint32_t>(want.max_y),
        static_cast<uint32_t>(got.max_y));
  check(src == p.src, (t + ": src_id rides its own sprite").c_str(), p.src, src);
  check(dtest && !dwrite, (t + ": depth is TESTED and never WRITTEN").c_str(), 1,
        (dtest && !dwrite) ? 1 : 0);
}

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t v0 = s;
    s = v0 * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((v0 >> 22) ^ v0) >> 29);
    const uint32_t v = (static_cast<uint32_t>(v0 >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

// ---------------------------------------------------------------------------
// LAYER 1: the reference IS the renderer
// ---------------------------------------------------------------------------
void test_reference_is_draw_population() {
  const uint32_t W = 128, H = 96;
  zr::Viewport vp;
  vp.x0 = 0; vp.y0 = 0; vp.w = W; vp.h = H;

  zref::mat4fx m{};
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) m.m[i][j] = zref::fx16{i == j ? kOne : 0};
  }

  zr::Population pop;
  Prng rng(0x50F7u);
  for (int k = 0; k < 60; ++k) {
    zr::Particle p;
    // Deliberately straddling every edge, so the ceiling/floor asymmetry and the
    // scissor are both exercised on the same particles in both paths.
    p.x = static_cast<int32_t>(rng.next() % (3u * kOne)) - (3 * kOne) / 2;
    p.y = static_cast<int32_t>(rng.next() % (3u * kOne)) - (3 * kOne) / 2;
    p.z = 0;
    p.size = static_cast<uint8_t>(1 + rng.below(120));
    p.r = static_cast<uint8_t>(rng.next() | 1u);
    p.g = static_cast<uint8_t>(rng.next() | 1u);
    p.b = static_cast<uint8_t>(rng.next() | 1u);
    pop.parts.push_back(p);
  }

  const zref::sky::SkyColor bg{};

  // Path A: the renderer's own point-sprite path.
  zr::WorkSurface sa;
  sa.reset(W, H, bg);
  zr::draw_population(sa, vp, m, pop, 0x0001, nullptr);  // points only

  // Path B: soft_rect's rectangle, painted with the same depth test.
  zr::WorkSurface sb;
  sb.reset(W, H, bg);
  for (const zr::Particle& p : pop.parts) {
    const zr::ProjOut c = zr::project_vertex(m, vp, zref::fx16{p.x}, zref::fx16{p.y},
                                             zref::fx16{p.z}, nullptr);
    const zp::SoftRect r = zp::soft_rect(c.in, c.s.x, c.s.y, p.size, static_cast<int32_t>(vp.x0),
                                         static_cast<int32_t>(vp.y0),
                                         static_cast<int32_t>(vp.w),
                                         static_cast<int32_t>(vp.h));
    if (!r.covered) continue;
    for (int32_t py = r.min_y; py <= r.max_y; ++py) {
      for (int32_t px = r.min_x; px <= r.max_x; ++px) {
        const size_t idx = static_cast<size_t>(py) * sb.w + static_cast<size_t>(px);
        if (c.s.d > sb.depth[idx]) {  // depth TEST only, never written
          sb.rgb[idx * 3 + 0] = p.r;
          sb.rgb[idx * 3 + 1] = p.g;
          sb.rgb[idx * 3 + 2] = p.b;
        }
      }
    }
  }

  uint64_t painted = 0;
  for (size_t i = 0; i < sa.rgb.size(); ++i) {
    if (sa.rgb[i] != 0) ++painted;
  }
  check(painted > 0, "layer 1 fixture: draw_population actually painted something", 1,
        painted > 0 ? 1 : 0);

  uint64_t rgb_diff = 0, depth_diff = 0;
  for (size_t i = 0; i < sa.rgb.size(); ++i) {
    if (sa.rgb[i] != sb.rgb[i]) ++rgb_diff;
  }
  for (size_t i = 0; i < sa.depth.size(); ++i) {
    if (sa.depth[i] != sb.depth[i]) ++depth_diff;
  }
  check(rgb_diff == 0, "LAYER 1: soft_rect reproduces draw_population, pixel for pixel", 0,
        rgb_diff);
  check(depth_diff == 0, "LAYER 1: and writes no depth, exactly as the renderer does not", 0,
        depth_diff);
}

}  // namespace

int main(int argc, char** argv) {
  Vzhao_part_soft raw;
  Dut dut(raw);

  const VP vp0{0, 0, 256, 192};
  const VP vp1{0, 192, 256, 192};   // the Duo second view
  const VP vpo{37, 11, 100, 80};    // an odd origin, so the scissor is not at 0

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    Prng rng(0x50F1u);
    for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      VP v;
      v.x0 = static_cast<int32_t>(rng.below(200));
      v.y0 = static_cast<int32_t>(rng.below(200));
      v.w = 1 + static_cast<int32_t>(rng.below(512));
      v.h = 1 + static_cast<int32_t>(rng.below(512));
      P p;
      p.in = rng.below(8) != 0;
      // Centred on the viewport and spread by a little MORE than its own size,
      // so most sprites land inside or straddling an edge and only a minority
      // are fully scissored.
      //
      // The first version spread by a fixed +/-512 px regardless of viewport
      // size, and only about 7% of iterations covered anything at all -- the
      // lane was mostly re-testing "empty" over and over. Sampling relative to
      // the viewport is what makes the random checks land on the law.
      const int32_t cx_px = v.x0 + v.w / 2;
      const int32_t cy_px = v.y0 + v.h / 2;
      const int32_t spread_x = (v.w * 3) / 4 + 8;
      const int32_t spread_y = (v.h * 3) / 4 + 8;
      p.x = (cx_px << 8) +
            (static_cast<int32_t>(rng.next() % static_cast<uint32_t>(spread_x * 512 + 1)) -
             spread_x * 256);
      p.y = (cy_px << 8) +
            (static_cast<int32_t>(rng.next() % static_cast<uint32_t>(spread_y * 512 + 1)) -
             spread_y * 256);
      p.d = static_cast<int32_t>(rng.next());
      // Biased small: a sprite the size of the viewport tests the clamp and
      // nothing else, while a few-pixel sprite is where the ceil/floor
      // asymmetry actually decides the answer. The full byte still appears.
      p.size = (rng.below(4) == 0) ? static_cast<uint8_t>(rng.next())
                                   : static_cast<uint8_t>(1 + rng.below(64));
      p.r = static_cast<uint8_t>(rng.next());
      p.g = static_cast<uint8_t>(rng.next());
      p.b = static_cast<uint8_t>(rng.next());
      p.src = static_cast<uint16_t>(rng.next());
      char tag[96];
      std::snprintf(tag, sizeof tag, "random[%u] size=%u vp %dx%d", it, p.size, v.w, v.h);
      diff(dut, v, p, tag);
    }
    raw.final();
    return zhao::report_and_exit("part_soft_random");
  }

  // ---- LAYER 1 ------------------------------------------------------------
  test_reference_is_draw_population();

  // ---- LAYER 2, 1. a sprite well inside the viewport ----------------------
  diff(dut, vp0, P{true, 100 * 256, 80 * 256, kOne, 64, 200, 100, 50, 0x11},
       "a four-pixel sprite at (100, 80)");

  // ---- 2. THE TWO EDGES ROUND DIFFERENTLY ---------------------------------
  // The low edge ceils, the high edge floors. Sweeping the sprite across a
  // pixel one subpixel at a time is what exposes it: a symmetric rounding gives
  // the same width every step, the real law gives a width that steps.
  {
    for (int32_t off = 0; off < 16; ++off) {
      char tag[96];
      std::snprintf(tag, sizeof tag, "subpixel sweep +%d: ceil low, floor high", off);
      diff(dut, vp0, P{true, 50 * 256 + off * 16, 50 * 256 + off * 16, kOne, 32, 1, 2, 3,
                       static_cast<uint16_t>(0x200 + off)},
           tag);
    }
    // Asserted directly too: the reference and the RTL could agree on a
    // convention and both be wrong about which way each edge goes.
    const zp::SoftRect r = zp::soft_rect(true, 10 * 256 + 1, 10 * 256 + 1, 16, 0, 0, 256, 192);
    check(r.min_x == 10, "a low edge one subpixel past a boundary still ceils to that pixel", 10,
          static_cast<uint32_t>(r.min_x));
  }

  // ---- 3. A ZERO EXTENT DRAWS NOTHING -------------------------------------
  // blit_pattern_block returns before any clamping on w_sub <= 0, so size 0 is
  // no pixels at all rather than a one-pixel dot.
  {
    const uint32_t before = dut.counted();
    diff(dut, vp0, P{true, 100 * 256, 80 * 256, kOne, 0, 1, 2, 3, 0x31}, "size 0 covers nothing");
    check(dut.counted() == before, "and is not counted as a soft particle", before,
          dut.counted());
    diff(dut, vp0, P{true, 100 * 256, 80 * 256, kOne, 1, 1, 2, 3, 0x32},
         "size 1 is a sixteenth of a pixel");
  }

  // ---- 4. SCISSORED TO NOTHING IS NOT AN EMPTY SPAN -----------------------
  // Far outside on each side. The reference's loops do not run; this block must
  // raise no beat rather than emit a rectangle covering zero pixels.
  {
    const uint32_t before = dut.counted();
    diff(dut, vp0, P{true, -5000 * 256, 80 * 256, kOne, 32, 1, 2, 3, 0x41}, "far left");
    diff(dut, vp0, P{true, 5000 * 256, 80 * 256, kOne, 32, 1, 2, 3, 0x42}, "far right");
    diff(dut, vp0, P{true, 100 * 256, -5000 * 256, kOne, 32, 1, 2, 3, 0x43}, "far above");
    diff(dut, vp0, P{true, 100 * 256, 5000 * 256, kOne, 32, 1, 2, 3, 0x44}, "far below");
    check(dut.counted() == before, "a fully scissored sprite is not counted", before,
          dut.counted());
  }

  // ---- 5. straddling every edge -------------------------------------------
  // The clamp on one side and the real geometry on the other, which is where a
  // min/max swap or a wrong viewport bound shows up.
  {
    diff(dut, vp0, P{true, 0, 80 * 256, kOne, 96, 1, 2, 3, 0x51}, "straddling the left edge");
    diff(dut, vp0, P{true, 255 * 256, 80 * 256, kOne, 96, 1, 2, 3, 0x52}, "the right edge");
    diff(dut, vp0, P{true, 100 * 256, 0, kOne, 96, 1, 2, 3, 0x53}, "the top edge");
    diff(dut, vp0, P{true, 100 * 256, 191 * 256, kOne, 96, 1, 2, 3, 0x54}, "the bottom edge");
    diff(dut, vp0, P{true, 0, 0, kOne, 255, 1, 2, 3, 0x55}, "the top-left corner, largest size");
  }

  // ---- 6. THE VIEWPORT ORIGIN IS NOT ASSUMED TO BE ZERO -------------------
  // The Duo second view sits at y0 = 192, and sprites.cpp carries a fixed defect
  // note about exactly this: bounds that were viewport-RELATIVE while the
  // coordinates were canvas, so view 1's markers were silently invisible.
  {
    diff(dut, vp1, P{true, 100 * 256, 200 * 256, kOne, 64, 1, 2, 3, 0x61},
         "inside the Duo second view");
    diff(dut, vp1, P{true, 100 * 256, 100 * 256, kOne, 64, 1, 2, 3, 0x62},
         "in view 0's rows, so outside view 1");
    diff(dut, vpo, P{true, 40 * 256, 15 * 256, kOne, 48, 1, 2, 3, 0x63},
         "inside a viewport with an odd origin");
    diff(dut, vpo, P{true, 36 * 256, 15 * 256, kOne, 16, 1, 2, 3, 0x64},
         "one pixel left of an odd origin");

    // The HIGH bounds of an offset viewport, which the cases above never reach.
    // A mutation that computed the right edge as (w - 1) instead of
    // (x0 + w - 1) passed every directed case here and was caught only by the
    // random lane -- because every sprite above sits left of x = 99 and the two
    // bounds only differ beyond it.
    diff(dut, vpo, P{true, 135 * 256, 50 * 256, kOne, 32, 1, 2, 3, 0x65},
         "on the RIGHT edge of an odd-origin viewport");
    diff(dut, vpo, P{true, 137 * 256, 50 * 256, kOne, 16, 1, 2, 3, 0x66},
         "one pixel past its right edge");
    diff(dut, vpo, P{true, 60 * 256, 90 * 256, kOne, 32, 1, 2, 3, 0x67},
         "on the BOTTOM edge of an odd-origin viewport");
    diff(dut, vpo, P{true, 60 * 256, 92 * 256, kOne, 16, 1, 2, 3, 0x68},
         "one pixel past its bottom edge");
  }

  // ---- 7. behind the eye produces nothing ---------------------------------
  {
    const uint32_t before = dut.counted();
    diff(dut, vp0, P{false, 100 * 256, 80 * 256, kOne, 64, 1, 2, 3, 0x71}, "behind the eye");
    check(dut.counted() == before, "a behind-the-eye sprite is not counted", before,
          dut.counted());
    diff(dut, vp0, P{true, 100 * 256, 80 * 256, kOne, 64, 1, 2, 3, 0x72},
         "and the next one still works");
    check(dut.counted() == before + 1, "the one in front is counted", before + 1, dut.counted());
  }

  raw.final();
  return zhao::report_and_exit("part_soft_directed");
}
