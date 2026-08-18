// geom_clip_directed.cpp — GEOM.CLIP directed vectors
// (design/contracts/GEOM.CLIP.md "Directed tests"; law spec/qformats.md §8 +
// reference/src/zrender/rast.cpp).
//
// Every case runs the Verilated zhao_geom_clip AND zref::Clip over the same
// triangle, viewport and cull mode, and requires the verdict and the whole
// accepted packet — winding-normalised vertices, 2A, and the scissored scan
// box — to be IDENTICAL. zref::Clip reaches its box by CALLING
// zref::render::scan_bbox, the function raster_tri itself calls, so "RTL ==
// oracle" here is "RTL == the software raster's own early return".
//
// On top of the differential each case asserts its own law:
//
//   1. inside        — a triangle wholly inside the viewport is accepted and
//                      its box is the §8 pixel-CENTRE range
//   2. outside       — wholly left/right/above/below, and one a guard band
//                      away: kOffscreen, and NOT degenerate
//   3. near plane    — every one of the 7 non-zero `behind` masks rejects the
//                      WHOLE triangle (the documented Phase-3 clip model), and
//                      wins over a zero area on the same triangle
//   4. degenerate    — coincident / repeated / collinear (including a
//                      subpixel-collinear one): kZeroArea
//   5. windings      — all 6 vertex permutations produce the SAME normalised
//                      triangle, the same |2A| and the same box
//   6. box law       — the +127 / −128 pixel-centre form swept through all 256
//                      subpixel fractions on both axes and both ends: getting
//                      this off by one is the 2026-08-15 seam-crack defect
//   7. scissor       — a triangle straddling each viewport edge in turn, and
//                      the exact one-pixel-in / one-pixel-out boundary
//   8. guard band    — vertices at the ±2048 px extremes; no overflow, and the
//                      box is still exact
//   9. backface      — the CHOSEN cull modes: NONE is rast.cpp (double-sided),
//                      CULL_NEG and CULL_POS reject by the sign of 2A, and the
//                      reject ORDER (near > zero > backface > offscreen) holds
//  10. counters      — submitted / clipped / culled are disjoint and sum
//  11. backpressure  — the same triangles with out_ready_i gated by a PCG bit
//                      stream: identical packets, held stable while stalled

#define ZHAO_GEOM_DEV_CLIP
#include "geom_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

#include "zref/zref_edgewalk.hpp"

using zhao::check;
using zhao_geom::ClipDev;
using zref::Clip;

namespace {

ClipDev& dev() {
  static ClipDev d;
  return d;
}

const Clip::Viewport kVp{0, 0, 384, 240};  // the Z60 canvas (video_rules.md §1)

// One differential run. Returns the RTL packet; the oracle's is the law.
Clip::Out diff(const Clip::In& t, const Clip::Viewport& vp, Clip::CullMode cull, const char* what,
               uint32_t stall = 0) {
  std::string err;
  const Clip::Out got = dev().run(t, vp, cull, 0xA5A5, stall, &err);
  const Clip::Out want = Clip::clip(t, vp, cull);
  check(err.empty(), what, 0, err.empty() ? 0 : 1);
  if (!err.empty()) std::printf("    protocol: %s (%s)\n", err.c_str(), what);
  check(got.verdict == want.verdict, what, want.verdict, got.verdict);
  if (want.verdict == Clip::kAccept) {
    check(got.ax == want.ax && got.ay == want.ay && got.bx == want.bx && got.by == want.by &&
              got.cx == want.cx && got.cy == want.cy,
          what, 0, 1);
    check(got.area2 == want.area2, what, static_cast<uint64_t>(want.area2),
          static_cast<uint64_t>(got.area2));
    check(got.min_x == want.min_x, what, static_cast<uint32_t>(want.min_x),
          static_cast<uint32_t>(got.min_x));
    check(got.max_x == want.max_x, what, static_cast<uint32_t>(want.max_x),
          static_cast<uint32_t>(got.max_x));
    check(got.min_y == want.min_y, what, static_cast<uint32_t>(want.min_y),
          static_cast<uint32_t>(got.min_y));
    check(got.max_y == want.max_y, what, static_cast<uint32_t>(want.max_y),
          static_cast<uint32_t>(got.max_y));
  }
  return got;
}

Clip::In tri(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t cx, int32_t cy,
             uint8_t behind = 0) {
  Clip::In t;
  t.ax = ax;
  t.ay = ay;
  t.bx = bx;
  t.by = by;
  t.cx = cx;
  t.cy = cy;
  t.behind = behind;
  return t;
}

// ---------------------------------------------------------------- 1 --------
void test_inside() {
  // a plain triangle in the middle of the canvas
  const Clip::Out o = diff(tri(100 * 256, 100 * 256, 140 * 256, 100 * 256, 120 * 256, 140 * 256),
                           kVp, Clip::kCullNone, "inside: accepted");
  check(o.verdict == Clip::kAccept, "inside: accepted", Clip::kAccept, o.verdict);
  // §8: pixel centres are at 256p + 128, so a vertex exactly on a pixel origin
  // puts the first candidate column at that pixel.
  check(o.min_x == 100 && o.max_x == 139, "inside: x box", 100u * 1000 + 139,
        static_cast<uint32_t>(o.min_x) * 1000 + static_cast<uint32_t>(o.max_x));
  check(o.min_y == 100 && o.max_y == 139, "inside: y box", 100u * 1000 + 139,
        static_cast<uint32_t>(o.min_y) * 1000 + static_cast<uint32_t>(o.max_y));

  // one that fills the whole canvas
  diff(tri(0, 0, Clip::kGuard, 0, 0, Clip::kGuard), kVp, Clip::kCullNone,
       "inside: fills the canvas");
}

// ---------------------------------------------------------------- 2 --------
void test_outside() {
  const struct {
    int32_t dx, dy;
    const char* name;
  } off[] = {{-500 * 256, 0, "left"},
             {500 * 256, 0, "right"},
             {0, -500 * 256, "above"},
             {0, 500 * 256, "below"},
             {-2000 * 256, -2000 * 256, "guard band away"}};
  for (const auto& o : off) {
    const Clip::Out r = diff(tri(o.dx + 10 * 256, o.dy + 10 * 256, o.dx + 30 * 256, o.dy + 10 * 256,
                                 o.dx + 20 * 256, o.dy + 30 * 256),
                             kVp, Clip::kCullNone, "outside: offscreen");
    check(r.verdict == Clip::kOffscreen, o.name, Clip::kOffscreen, r.verdict);
  }
}

// ---------------------------------------------------------------- 3 --------
void test_near_plane() {
  const Clip::In good = tri(100 * 256, 100 * 256, 140 * 256, 100 * 256, 120 * 256, 140 * 256);
  for (uint8_t m = 1; m < 8; ++m) {
    Clip::In t = good;
    t.behind = m;
    const Clip::Out r = diff(t, kVp, Clip::kCullNone, "near plane: whole primitive");
    check(r.verdict == Clip::kNearPlane, "near plane: whole primitive", Clip::kNearPlane,
          r.verdict);
  }
  // the near plane WINS over a zero area: behind-the-eye screen coordinates
  // are meaningless, so classifying on them would be classifying on garbage
  Clip::In degen = tri(50 * 256, 50 * 256, 50 * 256, 50 * 256, 50 * 256, 50 * 256);
  degen.behind = 4;
  const Clip::Out r = diff(degen, kVp, Clip::kCullNone, "near plane beats zero area");
  check(r.verdict == Clip::kNearPlane, "near plane beats zero area", Clip::kNearPlane, r.verdict);
}

// ---------------------------------------------------------------- 4 --------
void test_degenerate() {
  const Clip::In cases[] = {
      tri(50 * 256, 50 * 256, 50 * 256, 50 * 256, 50 * 256, 50 * 256),  // coincident
      tri(50 * 256, 50 * 256, 90 * 256, 70 * 256, 50 * 256, 50 * 256),  // repeated A/C
      tri(50 * 256, 50 * 256, 60 * 256, 60 * 256, 80 * 256, 80 * 256),  // collinear
      tri(50 * 256, 50 * 256, 50 * 256 + 1, 50 * 256 + 1, 50 * 256 + 2, 50 * 256 + 2),
      tri(0, 0, 100 * 256, 0, 200 * 256, 0),  // horizontal
  };
  for (const Clip::In& t : cases) {
    const Clip::Out r = diff(t, kVp, Clip::kCullNone, "degenerate: zero area");
    check(r.verdict == Clip::kZeroArea, "degenerate: zero area", Clip::kZeroArea, r.verdict);
    check(zref::EdgeWalk::area2({t.ax, t.ay, t.bx, t.by, t.cx, t.cy}) == 0,
          "degenerate: oracle agrees the area is zero", 0, 1);
  }
}

// ---------------------------------------------------------------- 5 --------
void test_windings() {
  const int32_t v[3][2] = {{60 * 256 + 37, 70 * 256 + 200},
                           {130 * 256 + 91, 80 * 256 + 5},
                           {95 * 256 + 133, 150 * 256 + 64}};
  const int perm[6][3] = {{0, 1, 2}, {1, 2, 0}, {2, 0, 1}, {0, 2, 1}, {2, 1, 0}, {1, 0, 2}};
  Clip::Out first;
  bool have = false;
  for (const auto& p : perm) {
    const Clip::Out r =
        diff(tri(v[p[0]][0], v[p[0]][1], v[p[1]][0], v[p[1]][1], v[p[2]][0], v[p[2]][1]), kVp,
             Clip::kCullNone, "windings: differential");
    check(r.verdict == Clip::kAccept, "windings: accepted", Clip::kAccept, r.verdict);
    if (!have) {
      first = r;
      have = true;
    } else {
      // 2A and the box are permutation invariant; the normalised triangle is
      // the same cycle, so its area and box must match exactly
      check(r.area2 == first.area2, "windings: |2A| invariant", static_cast<uint64_t>(first.area2),
            static_cast<uint64_t>(r.area2));
      check(r.min_x == first.min_x && r.max_x == first.max_x && r.min_y == first.min_y &&
                r.max_y == first.max_y,
            "windings: box invariant", 0, 1);
    }
  }
}

// ---------------------------------------------------------------- 6 --------
void test_box_law() {
  // §8: the first candidate column is (v_min + 127) >> 8 and the last is
  // (v_max − 128) >> 8. Sweeping the subpixel fraction walks the boundary of
  // both, so an off-by-one in either constant shows up somewhere in 0..255.
  for (int32_t frac = 0; frac < 256; ++frac) {
    const int32_t x0 = 40 * 256 + frac;
    const int32_t x1 = 60 * 256 + frac;
    diff(tri(x0, 40 * 256 + frac, x1, 40 * 256 + frac, x0, 60 * 256 + frac), kVp, Clip::kCullNone,
         "box law: subpixel sweep");
  }
  // and a triangle exactly one subpixel wide/tall at every fraction — the case
  // where min and max land on the same pixel or on none at all
  for (int32_t frac = 0; frac < 256; ++frac) {
    const int32_t x = 50 * 256 + frac;
    diff(tri(x, 50 * 256, x + 1, 50 * 256, x, 50 * 256 + 1), kVp, Clip::kCullNone,
         "box law: one-subpixel needle");
  }
}

// ---------------------------------------------------------------- 7 --------
void test_scissor() {
  const Clip::Viewport vps[] = {
      {0, 0, 384, 240}, {0, 0, 320, 240}, {0, 0, 256, 192}, {0, 192, 256, 192}};
  for (const Clip::Viewport& vp : vps) {
    const int32_t x0 = static_cast<int32_t>(vp.x0) * 256;
    const int32_t y0 = static_cast<int32_t>(vp.y0) * 256;
    const int32_t x1 = static_cast<int32_t>(vp.x0 + vp.w) * 256;
    const int32_t y1 = static_cast<int32_t>(vp.y0 + vp.h) * 256;
    // straddling each edge in turn
    diff(tri(x0 - 20 * 256, y0 + 10 * 256, x0 + 10 * 256, y0 + 10 * 256, x0, y0 + 40 * 256), vp,
         Clip::kCullNone, "scissor: straddle left");
    diff(tri(x1 - 10 * 256, y0 + 10 * 256, x1 + 20 * 256, y0 + 10 * 256, x1, y0 + 40 * 256), vp,
         Clip::kCullNone, "scissor: straddle right");
    diff(tri(x0 + 10 * 256, y0 - 20 * 256, x0 + 40 * 256, y0 - 20 * 256, x0 + 20 * 256,
             y0 + 10 * 256),
         vp, Clip::kCullNone, "scissor: straddle top");
    diff(tri(x0 + 10 * 256, y1 - 10 * 256, x0 + 40 * 256, y1 - 10 * 256, x0 + 20 * 256,
             y1 + 20 * 256),
         vp, Clip::kCullNone, "scissor: straddle bottom");
    // exactly one pixel inside each edge, and exactly one outside
    for (int d = -2; d <= 2; ++d) {
      diff(tri(x0 + d * 256, y0 + 10 * 256, x0 + d * 256 + 200, y0 + 10 * 256, x0 + d * 256,
               y0 + 10 * 256 + 200),
           vp, Clip::kCullNone, "scissor: left boundary");
      diff(tri(x1 + d * 256, y0 + 10 * 256, x1 + d * 256 + 200, y0 + 10 * 256, x1 + d * 256,
               y0 + 10 * 256 + 200),
           vp, Clip::kCullNone, "scissor: right boundary");
      diff(tri(x0 + 10 * 256, y1 + d * 256, x0 + 10 * 256 + 200, y1 + d * 256, x0 + 10 * 256,
               y1 + d * 256 + 200),
           vp, Clip::kCullNone, "scissor: bottom boundary");
    }
  }
}

// ---------------------------------------------------------------- 8 --------
void test_guard_band() {
  const int32_t g = Clip::kGuard;  // 2048 px in S 12.8
  // vertices at the guard-band extremes, in every combination of corners
  const int32_t xs[] = {-g, -g + 1, 0, g - 1, g};
  for (int32_t x : xs) {
    for (int32_t y : xs) {
      diff(tri(x, y, 100 * 256, 100 * 256, 140 * 256, 120 * 256), kVp, Clip::kCullNone,
           "guard band: extreme vertex");
    }
  }
  // the widest possible triangle: 2A is |kx|·|ky| at its largest, and it must
  // not overflow the 48-bit setup domain (§8 Giesen bound 2^43−2 at p = 21)
  const Clip::Out r = diff(tri(-g, -g, g, -g, -g, g), kVp, Clip::kCullNone, "guard band: widest");
  check(r.verdict == Clip::kAccept, "guard band: widest accepted", Clip::kAccept, r.verdict);
  const int64_t want2a = 4ll * static_cast<int64_t>(g) * static_cast<int64_t>(g);
  check(r.area2 == want2a, "guard band: 2A exact", static_cast<uint64_t>(want2a),
        static_cast<uint64_t>(r.area2));
}

// ---------------------------------------------------------------- 9 --------
void test_backface() {
  // CCW in a y-down space gives 2A < 0; CW gives 2A > 0.
  const Clip::In pos = tri(50 * 256, 50 * 256, 90 * 256, 50 * 256, 70 * 256, 90 * 256);
  const Clip::In neg = tri(50 * 256, 50 * 256, 70 * 256, 90 * 256, 90 * 256, 50 * 256);
  check(zref::EdgeWalk::area2({pos.ax, pos.ay, pos.bx, pos.by, pos.cx, pos.cy}) > 0,
        "backface: fixture winding", 1, 1);

  // NONE — rast.cpp exactly: both are accepted, and both normalise to 2A > 0
  const Clip::Out a = diff(pos, kVp, Clip::kCullNone, "backface: NONE accepts CW");
  const Clip::Out b = diff(neg, kVp, Clip::kCullNone, "backface: NONE accepts CCW");
  check(a.verdict == Clip::kAccept && b.verdict == Clip::kAccept, "backface: NONE double-sided", 0,
        0);
  check(a.area2 > 0 && b.area2 > 0, "backface: normalised to 2A > 0", 1, 1);
  check(a.area2 == b.area2, "backface: same |2A| either way", static_cast<uint64_t>(a.area2),
        static_cast<uint64_t>(b.area2));

  // CULL_NEG rejects 2A < 0, CULL_POS rejects 2A > 0
  check(diff(neg, kVp, Clip::kCullNegative, "backface: NEG rejects CCW").verdict == Clip::kBackface,
        "backface: NEG rejects CCW", Clip::kBackface, 0);
  check(diff(pos, kVp, Clip::kCullNegative, "backface: NEG keeps CW").verdict == Clip::kAccept,
        "backface: NEG keeps CW", Clip::kAccept, 0);
  check(diff(pos, kVp, Clip::kCullPositive, "backface: POS rejects CW").verdict == Clip::kBackface,
        "backface: POS rejects CW", Clip::kBackface, 0);
  check(diff(neg, kVp, Clip::kCullPositive, "backface: POS keeps CCW").verdict == Clip::kAccept,
        "backface: POS keeps CCW", Clip::kAccept, 0);

  // ORDER: zero area beats backface (a degenerate triangle has no side), and
  // backface beats offscreen (a backface is culled wherever it is).
  const Clip::In flat = tri(50 * 256, 50 * 256, 60 * 256, 60 * 256, 80 * 256, 80 * 256);
  check(diff(flat, kVp, Clip::kCullNegative, "order: zero area beats backface").verdict ==
            Clip::kZeroArea,
        "order: zero area beats backface", Clip::kZeroArea, 0);
  Clip::In far_neg = neg;
  far_neg.ax -= 1500 * 256;
  far_neg.bx -= 1500 * 256;
  far_neg.cx -= 1500 * 256;
  check(diff(far_neg, kVp, Clip::kCullNegative, "order: backface beats offscreen").verdict ==
            Clip::kBackface,
        "order: backface beats offscreen", Clip::kBackface, 0);
}

// --------------------------------------------------------------- 10 --------
void test_counters() {
  // The counters are cumulative over the whole suite; the law asserted here is
  // the INVARIANT, which holds at every point: submitted = clipped + culled +
  // accepted, and the two reject counters are disjoint by construction.
  const uint32_t s0 = dev().submitted();
  const uint32_t c0 = dev().clipped();
  const uint32_t k0 = dev().culled();

  diff(tri(100 * 256, 100 * 256, 140 * 256, 100 * 256, 120 * 256, 140 * 256), kVp, Clip::kCullNone,
       "counters: one accept");
  check(dev().submitted() == s0 + 1, "counters: submitted counts every input", s0 + 1,
        dev().submitted());
  check(dev().clipped() == c0 && dev().culled() == k0, "counters: an accept moves neither reject",
        0, 0);

  Clip::In behind = tri(100 * 256, 100 * 256, 140 * 256, 100 * 256, 120 * 256, 140 * 256);
  behind.behind = 1;
  diff(behind, kVp, Clip::kCullNone, "counters: near plane is CLIPPED");
  check(dev().clipped() == c0 + 1 && dev().culled() == k0, "counters: near plane -> clipped",
        c0 + 1, dev().clipped());

  diff(tri(-1000 * 256, 0, -900 * 256, 0, -950 * 256, 100 * 256), kVp, Clip::kCullNone,
       "counters: offscreen is CLIPPED");
  check(dev().clipped() == c0 + 2 && dev().culled() == k0, "counters: offscreen -> clipped", c0 + 2,
        dev().clipped());

  diff(tri(50 * 256, 50 * 256, 60 * 256, 60 * 256, 80 * 256, 80 * 256), kVp, Clip::kCullNone,
       "counters: zero area is CULLED");
  check(dev().culled() == k0 + 1 && dev().clipped() == c0 + 2, "counters: zero area -> culled",
        k0 + 1, dev().culled());

  check(dev().submitted() == s0 + 4, "counters: submitted totals", s0 + 4, dev().submitted());
}

// --------------------------------------------------------------- 11 --------
void test_backpressure() {
  const uint32_t seeds[] = {1u, 0x1234567u, 0x89ABCDEFu, 0xDEADBEEFu};
  for (uint32_t s : seeds) {
    diff(tri(60 * 256 + 37, 70 * 256 + 200, 130 * 256 + 91, 80 * 256 + 5, 95 * 256 + 133,
             150 * 256 + 64),
         kVp, Clip::kCullNone, "backpressure: accepted packet held stable", s);
    diff(tri(-1000 * 256, 0, -900 * 256, 0, -950 * 256, 100 * 256), kVp, Clip::kCullNone,
         "backpressure: a reject never stalls", s);
  }
}

}  // namespace

int main() {
  test_inside();
  test_outside();
  test_near_plane();
  test_degenerate();
  test_windings();
  test_box_law();
  test_scissor();
  test_guard_band();
  test_backface();
  test_counters();
  test_backpressure();

  return zhao::report_and_exit("geom_clip_directed");
}
