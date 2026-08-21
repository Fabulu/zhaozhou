// part_expand_directed.cpp — PART.EXPAND against `zref::part::expand_polygon`,
// and that reference against the renderer it was extracted from.
//
// TWO LAYERS, because this reference is the risky kind. The forty
// `zref::fieldir::*` phantoms could be forwarded to a real callable;
// `zref::ParticleExpand` could not. The law lives INLINE inside
// `zref::render::draw_population`'s `tris` branch, which computes the fan and
// immediately rasterises it, so the reference had to RESTATE it.
//
// A restated law is a second implementation, and a second implementation that
// only its author compares against the first is exactly the failure the
// phantom-reference rules exist to catch. So:
//
//   LAYER 1 — the reference IS the renderer. Render a population twice: once
//   through `draw_population` with the triangle flag, and once by feeding
//   `expand_polygon`'s vertices to the same `raster_tri` with the same mode.
//   Require the two surfaces to be identical, pixel for pixel and depth word
//   for depth word. If zref_particle.hpp ever drifts from sprites.cpp, this
//   fails -- and it fails whichever of the two moved.
//
//   LAYER 2 — the RTL is the reference. Ordinary differential on the vertices.
//
// Layer 1 is what makes layer 2 worth anything.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_part_expand.h"

#include "zhao_sim.hpp"
#include "zref/zref_particle.hpp"
#include "zref/zref_render.hpp"
#include "zrender/internal.hpp"

namespace {

using zhao::check;
namespace zp = zref::part;
namespace zr = zref::render;

constexpr int32_t kOne = 1 << 16;

int32_t sx21(uint32_t v) { return static_cast<int32_t>(v << 11) >> 11; }
int32_t sx22(uint32_t v) { return static_cast<int32_t>(v << 10) >> 10; }

struct P {
  bool in;
  int32_t x, y, d;
  uint8_t size, r, g, b;
  uint16_t src;
};

class Dut {
 public:
  explicit Dut(Vzhao_part_expand& d) : dut_(d) { reset(); }

  void reset() {
    dut_.rst_n = 0;
    dut_.p_valid_i = 0;
    dut_.t_ready_i = 1;
    dut_.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(dut_);
    dut_.rst_n = 1;
    dut_.eval();
  }

  /** Offer one particle. Returns true if a triangle came out. */
  bool expand(const P& p, zp::PolyExpand& got, uint16_t& src, bool& dtest, bool& dwrite) {
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
    dut_.t_ready_i = 1;
    dut_.eval();
    int guard = 0;
    while (!dut_.p_ready_o && guard++ < 64) {
      zhao::tick(dut_);
      dut_.eval();
    }
    zhao::tick(dut_);
    dut_.p_valid_i = 0;
    dut_.eval();

    if (!dut_.t_valid_o) {
      got = zp::PolyExpand{};
      return false;
    }
    got.emitted = true;
    got.a = zp::ExpandedVertex{sx22(dut_.t_ax_o), sx22(dut_.t_ay_o),
                               static_cast<int32_t>(dut_.t_d_o)};
    got.b = zp::ExpandedVertex{sx22(dut_.t_bx_o), sx22(dut_.t_by_o),
                               static_cast<int32_t>(dut_.t_d_o)};
    got.c = zp::ExpandedVertex{sx22(dut_.t_cx_o), sx22(dut_.t_cy_o),
                               static_cast<int32_t>(dut_.t_d_o)};
    got.r = static_cast<uint8_t>(dut_.t_r_o);
    got.g = static_cast<uint8_t>(dut_.t_g_o);
    got.b_ = static_cast<uint8_t>(dut_.t_b_o);
    src = static_cast<uint16_t>(dut_.t_src_id_o);
    dtest = dut_.t_depth_test_o != 0;
    dwrite = dut_.t_depth_write_o != 0;
    zhao::tick(dut_);
    dut_.eval();
    return true;
  }

  uint32_t counted() const { return dut_.polygon_particles_o; }

 private:
  Vzhao_part_expand& dut_;
};

void diff(Dut& dut, const P& p, const char* what) {
  const zp::PolyExpand want =
      zp::expand_polygon(p.in, p.x, p.y, p.d, p.size, p.r, p.g, p.b);
  zp::PolyExpand got{};
  uint16_t src = 0;
  bool dtest = false, dwrite = true;
  const bool emitted = dut.expand(p, got, src, dtest, dwrite);

  const std::string t(what);
  check(emitted == want.emitted, (t + ": emitted at all").c_str(), want.emitted ? 1 : 0,
        emitted ? 1 : 0);
  if (!want.emitted || !emitted) return;

  check(got.a.x == want.a.x, (t + ": a.x").c_str(), static_cast<uint32_t>(want.a.x),
        static_cast<uint32_t>(got.a.x));
  check(got.a.y == want.a.y, (t + ": a.y").c_str(), static_cast<uint32_t>(want.a.y),
        static_cast<uint32_t>(got.a.y));
  check(got.b.x == want.b.x, (t + ": b.x").c_str(), static_cast<uint32_t>(want.b.x),
        static_cast<uint32_t>(got.b.x));
  check(got.b.y == want.b.y, (t + ": b.y").c_str(), static_cast<uint32_t>(want.b.y),
        static_cast<uint32_t>(got.b.y));
  check(got.c.x == want.c.x, (t + ": c.x").c_str(), static_cast<uint32_t>(want.c.x),
        static_cast<uint32_t>(got.c.x));
  check(got.c.y == want.c.y, (t + ": c.y").c_str(), static_cast<uint32_t>(want.c.y),
        static_cast<uint32_t>(got.c.y));
  check(got.a.d == want.a.d && got.b.d == want.b.d && got.c.d == want.c.d,
        (t + ": all three vertices carry the particle's depth").c_str(), 1,
        (got.a.d == want.a.d && got.b.d == want.b.d && got.c.d == want.c.d) ? 1 : 0);
  check(got.r == want.r && got.g == want.g && got.b_ == want.b_, (t + ": colour").c_str(), 1,
        (got.r == want.r && got.g == want.g && got.b_ == want.b_) ? 1 : 0);
  check(src == p.src, (t + ": src_id rides its own particle").c_str(), p.src, src);
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

  // An identity view-projection: clip == the vertex, w == 1.0, so a particle's
  // world position maps predictably and every particle is in front of the eye.
  zref::mat4fx m{};
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) m.m[i][j] = zref::fx16{i == j ? kOne : 0};
  }

  zr::Population pop;
  Prng rng(0x9A11u);
  for (int k = 0; k < 40; ++k) {
    zr::Particle p;
    // Spread across the viewport, including partly off the edges so the
    // rasteriser's scissor is exercised on both paths identically.
    p.x = static_cast<int32_t>(rng.next() % (3 * kOne)) - static_cast<int32_t>(1.5 * kOne);
    p.y = static_cast<int32_t>(rng.next() % (3 * kOne)) - static_cast<int32_t>(1.5 * kOne);
    p.z = 0;
    p.size = static_cast<uint8_t>(1 + rng.below(200));
    p.r = static_cast<uint8_t>(rng.next());
    p.g = static_cast<uint8_t>(rng.next());
    p.b = static_cast<uint8_t>(rng.next());
    pop.parts.push_back(p);
  }

  const zref::sky::SkyColor bg{};

  // Path A: the renderer's own.
  zr::WorkSurface sa;
  sa.reset(W, H, bg);
  zr::draw_population(sa, vp, m, pop, 0x0002, nullptr);  // tris only

  // Path B: expand_polygon's vertices through the SAME raster_tri.
  zr::WorkSurface sb;
  sb.reset(W, H, bg);
  for (const zr::Particle& p : pop.parts) {
    const zr::ProjOut c = zr::project_vertex(m, vp, zref::fx16{p.x}, zref::fx16{p.y},
                                             zref::fx16{p.z}, nullptr);
    const zp::PolyExpand e =
        zp::expand_polygon(c.in, c.s.x, c.s.y, c.s.d, p.size, p.r, p.g, p.b);
    if (!e.emitted) continue;
    const zr::ScreenV a{e.a.x, e.a.y, e.a.d, 0};
    const zr::ScreenV b{e.b.x, e.b.y, e.b.d, 0};
    const zr::ScreenV cc{e.c.x, e.c.y, e.c.d, 0};
    zr::TriMode tm;
    tm.depth_write = zp::PolyExpand::kDepthWrite;
    tm.depth_test = zp::PolyExpand::kDepthTest;
    zr::raster_tri(sb, vp, a, b, cc, e.r, e.g, e.b_, tm);
  }

  // The fixture only means something if the particles actually drew.
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
  check(rgb_diff == 0, "LAYER 1: expand_polygon reproduces draw_population, pixel for pixel", 0,
        rgb_diff);
  check(depth_diff == 0, "LAYER 1: and depth word for depth word", 0, depth_diff);
}

}  // namespace

int main(int argc, char** argv) {
  Vzhao_part_expand raw;
  Dut dut(raw);

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    Prng rng(0x5A17u);
    for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      P p;
      p.in = rng.below(8) != 0;  // mostly in front, a steady trickle behind
      // The whole guard band, both rails, so the 22-bit output width is
      // exercised rather than assumed.
      p.x = (static_cast<int32_t>(rng.next() % 1048577)) - 524288;
      p.y = (static_cast<int32_t>(rng.next() % 1048577)) - 524288;
      p.d = static_cast<int32_t>(rng.next());
      p.size = static_cast<uint8_t>(rng.next());
      p.r = static_cast<uint8_t>(rng.next());
      p.g = static_cast<uint8_t>(rng.next());
      p.b = static_cast<uint8_t>(rng.next());
      p.src = static_cast<uint16_t>(rng.next());
      char tag[80];
      std::snprintf(tag, sizeof tag, "random[%u] size=%u%s", it, p.size, p.in ? "" : " behind");
      diff(dut, p, tag);
    }
    raw.final();
    return zhao::report_and_exit("part_expand_random");
  }

  // ---- LAYER 1 ------------------------------------------------------------
  test_reference_is_draw_population();

  // ---- LAYER 2, 1. the plain expansion ------------------------------------
  diff(dut, P{true, 100 * 256, 80 * 256, kOne, 16, 200, 100, 50, 0x11},
       "a one-pixel particle at (100, 80)");

  // ---- 2. THE SIZE SCALE: U 0.4.4, so `size << 4` -------------------------
  // A size byte of 16 is ONE pixel, not sixteen. An implementation that shifted
  // by 8 would make every particle sixteen times too large and would still pass
  // any test that only checked one size.
  {
    diff(dut, P{true, 0, 0, kOne, 16, 1, 2, 3, 0x21}, "size 16 == one pixel");
    diff(dut, P{true, 0, 0, kOne, 1, 1, 2, 3, 0x22}, "size 1 == one sixteenth of a pixel");
    diff(dut, P{true, 0, 0, kOne, 255, 1, 2, 3, 0x23}, "size 255, the largest a byte holds");
    diff(dut, P{true, 0, 0, kOne, 0, 1, 2, 3, 0x24}, "size 0 collapses to a point");

    // Asserted directly, not only differentially: the reference and the RTL
    // could agree on a shift and both be wrong about what U 0.4.4 means.
    const zp::PolyExpand e = zp::expand_polygon(true, 0, 0, kOne, 16, 0, 0, 0);
    check(e.a.y == -256, "size 16 drops the apex exactly one pixel (256 subpixels)",
          static_cast<uint32_t>(-256), static_cast<uint32_t>(e.a.y));
  }

  // ---- 3. THE FAN SHAPE, which must not be 'corrected' --------------------
  // Half-width 3/4 of a side, drop half a side. Both asymmetric about the
  // centre, and both integer-divided.
  {
    const zp::PolyExpand e = zp::expand_polygon(true, 0, 0, kOne, 64, 0, 0, 0);
    const int32_t side = 64 << 4;  // 1024 subpixels = 4 px
    check(e.a.x == 0, "the apex sits on the particle's own x", 0,
          static_cast<uint32_t>(e.a.x));
    check(e.a.y == -side, "the apex is a full side above", static_cast<uint32_t>(-side),
          static_cast<uint32_t>(e.a.y));
    check(e.b.y == side / 2 && e.c.y == side / 2, "the base is half a side below", 1,
          (e.b.y == side / 2 && e.c.y == side / 2) ? 1 : 0);
    check(e.c.x - e.b.x == 2 * ((side * 3) / 4), "the base is 3/2 of a side wide", 1,
          (e.c.x - e.b.x == 2 * ((side * 3) / 4)) ? 1 : 0);
    check(e.a.y != e.b.y, "the fan is NOT equilateral and must not be made so", 1,
          (e.a.y != e.b.y) ? 1 : 0);
    diff(dut, P{true, 0, 0, kOne, 64, 9, 8, 7, 0x31}, "the fan shape at size 64");
  }

  // ---- 4. a size sweep, and a note about what it CANNOT show --------------
  // This section originally claimed to catch a rounding error in (side*3)/4 and
  // side/2. It cannot, and neither can any test: `side_sub` is `size << 4`, so
  // it is always a multiple of 16 and both divisions are EXACT for every size
  // byte. A mutation that rounds instead of truncating survives the whole suite
  // -- an equivalent mutant, not a gap.
  //
  // The sweep stays because it still pins the vertex arithmetic across the full
  // range of sizes against the oracle; only the claim about rounding was wrong.
  {
    for (uint8_t sz : {1, 3, 5, 7, 9, 11, 13, 15, 17, 31, 33, 63, 65, 127, 129, 253}) {
      char tag[80];
      std::snprintf(tag, sizeof tag, "size %u across the range", sz);
      diff(dut, P{true, 7 * 256, 5 * 256, kOne, sz, 1, 1, 1, static_cast<uint16_t>(sz)}, tag);
    }
  }

  // ---- 5. A PARTICLE BEHIND THE EYE PRODUCES NOTHING ----------------------
  // Not a degenerate triangle -- that would be work for the setup stage and a
  // silent zero-area primitive in every capture.
  {
    const uint32_t before = dut.counted();
    diff(dut, P{false, 100 * 256, 80 * 256, kOne, 32, 1, 2, 3, 0x51}, "behind the eye");
    diff(dut, P{false, 0, 0, 0, 255, 1, 2, 3, 0x52}, "behind the eye, largest size");
    check(dut.counted() == before, "a behind-the-eye particle is not counted as expanded",
          before, dut.counted());
    diff(dut, P{true, 0, 0, kOne, 32, 1, 2, 3, 0x53}, "and the next one still works");
    check(dut.counted() == before + 1, "the one in front is counted", before + 1, dut.counted());
  }

  // ---- 6. the guard-band edges are NOT re-clamped -------------------------
  // A big particle at the rail puts a vertex outside ±2048 px, and that is
  // correct: the software does the same and lets the scan box scissor it.
  // Clamping here would deform the triangle instead of cropping it.
  {
    const int32_t rail = 524288;  // 2048 px in S 12.8
    diff(dut, P{true, rail, rail, kOne, 255, 4, 5, 6, 0x61}, "a huge particle on the +rail");
    diff(dut, P{true, -rail, -rail, kOne, 255, 4, 5, 6, 0x62}, "and on the -rail");
    const zp::PolyExpand e = zp::expand_polygon(true, rail, rail, kOne, 255, 0, 0, 0);
    check(e.c.x > rail, "a vertex is allowed outside the guard band", 1, e.c.x > rail ? 1 : 0);
  }

  // ---- 7. back-to-back throughput and the counter -------------------------
  {
    const uint32_t before = dut.counted();
    for (int k = 0; k < 25; ++k) {
      diff(dut, P{true, k * 256, -k * 128, kOne + k, static_cast<uint8_t>(3 * k + 1),
                  static_cast<uint8_t>(k), static_cast<uint8_t>(2 * k),
                  static_cast<uint8_t>(3 * k), static_cast<uint16_t>(0x700 + k)},
           "back to back");
    }
    check(dut.counted() == before + 25, "every accepted particle is counted once", before + 25,
          dut.counted());
  }

  raw.final();
  return zhao::report_and_exit("part_expand_directed");
}
