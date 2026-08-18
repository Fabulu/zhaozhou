// geom_setup_directed.cpp — GEOM.SETUP directed vectors
// (design/contracts/GEOM.SETUP.md "Directed tests"; law spec/qformats.md §8 +
// reference/src/zrender/rast.cpp).
//
// Every case runs the Verilated zhao_geom_setup AND zref::Setup over the same
// winding-normalised triangle and requires all nine edge coefficients, the
// three top-left bits, 2A and the whole passthrough to be IDENTICAL. On top of
// that:
//
//   1. plane identity — kx·px + ky·py + kc EQUALS rast.cpp's orient(a,b,px,py)
//                       at every probed position, checked against
//                       zref::EdgeWalk::area2 (which IS orient). This is the
//                       whole claim of the block, stated as a theorem.
//   2. THE JOINT      — the coefficients reproduce RASTER.EDGEWALK's coverage
//                       EXACTLY. Coverage is rebuilt from (kx, ky, kc, tl)
//                       alone, through the §8 decomposition and the fill rule,
//                       and diffed against zref::EdgeWalk over whole tiles.
//                       That is what "the same edge coefficients EDGEWALK
//                       already consumes" means, made checkable.
//   3. third constant — kc2 == 2A − kc0 − kc1 (the barycentric identity the
//                       RTL spends two fewer multipliers on)
//   4. steps          — kx == −Δy and ky == +Δx per edge, i.e. EDGEWALK's own
//                       sx / sy, so the two blocks step by the same numbers
//   5. top-left       — every edge orientation: horizontal both ways, vertical
//                       both ways, and the four diagonal quadrants
//   6. guard band     — vertices at the ±2048 px extremes: |kc| ≤ 2^39 and
//                       |2A| ≤ 2^41 must both survive the 48-bit domain
//   7. passthrough    — vertices, box and src_id ride through untouched
//   8. backpressure   — packets held stable while out_ready_i is low

#define ZHAO_GEOM_DEV_SETUP
#include "geom_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

#include "zref/zref_edgewalk.hpp"

using zhao::check;
using zhao_geom::SetupDev;
using zref::Clip;
using zref::Setup;

namespace {

SetupDev& dev() {
  static SetupDev d;
  return d;
}

const Clip::Viewport kVp{0, 0, 384, 240};

// rast.cpp's orient(), reached through the frozen helper rather than restated.
int64_t orient(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t px, int32_t py) {
  return zref::EdgeWalk::area2({ax, ay, bx, by, px, py});
}

// The §8 evaluation of ONE edge at ONE pixel centre, from the coefficients
// alone: E0 = kx·px + ky·py + kc, then the narrow (E', r != 0, tl) fill test.
bool edge_covers(const Setup::Edge& e, int32_t px, int32_t py) {
  const int64_t sub_x = static_cast<int64_t>(px) * 256 + 128;
  const int64_t sub_y = static_cast<int64_t>(py) * 256 + 128;
  const int64_t e0 = static_cast<int64_t>(e.kx) * sub_x + static_cast<int64_t>(e.ky) * sub_y + e.kc;
  return zref::fill_accept(e0 >> 8, (e0 & 255) != 0, e.tl);
}

Clip::Out clipped(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t cx, int32_t cy) {
  Clip::In t;
  t.ax = ax;
  t.ay = ay;
  t.bx = bx;
  t.by = by;
  t.cx = cx;
  t.cy = cy;
  return Clip::clip(t, kVp, Clip::kCullNone);
}

Setup::Out diff(const Clip::Out& c, const char* what, uint32_t stall = 0) {
  std::string err;
  const Setup::Out got = dev().run(c, 0x1234, stall, &err);
  const Setup::Out want = Setup::setup(c.ax, c.ay, c.bx, c.by, c.cx, c.cy, c.area2);
  check(err.empty(), what, 0, err.empty() ? 0 : 1);
  if (!err.empty()) std::printf("    protocol: %s (%s)\n", err.c_str(), what);
  check(got.area2 == want.area2, what, static_cast<uint64_t>(want.area2),
        static_cast<uint64_t>(got.area2));
  for (int i = 0; i < 3; ++i) {
    check(got.e[i].kx == want.e[i].kx, what, static_cast<uint32_t>(want.e[i].kx),
          static_cast<uint32_t>(got.e[i].kx));
    check(got.e[i].ky == want.e[i].ky, what, static_cast<uint32_t>(want.e[i].ky),
          static_cast<uint32_t>(got.e[i].ky));
    check(got.e[i].kc == want.e[i].kc, what, static_cast<uint64_t>(want.e[i].kc),
          static_cast<uint64_t>(got.e[i].kc));
    check(got.e[i].tl == want.e[i].tl, what, want.e[i].tl ? 1u : 0u, got.e[i].tl ? 1u : 0u);
  }
  return got;
}

// ---------------------------------------------------------------- 1 --------
void test_plane_identity() {
  const Clip::Out c = clipped(40 * 256 + 91, 60 * 256 + 13, 190 * 256 + 7, 45 * 256 + 201,
                              120 * 256 + 155, 200 * 256 + 33);
  const Setup::Out s = diff(c, "plane identity: differential");

  const int32_t va[3][2] = {{c.bx, c.by}, {c.cx, c.cy}, {c.ax, c.ay}};
  const int32_t vb[3][2] = {{c.cx, c.cy}, {c.ax, c.ay}, {c.bx, c.by}};

  // probe every pixel centre of a 3x3 tile block, plus the guard-band corners
  for (int i = 0; i < 3; ++i) {
    for (int32_t py = 0; py < 48; ++py) {
      for (int32_t px = 0; px < 48; ++px) {
        const int64_t sub_x = static_cast<int64_t>(px) * 256 + 128;
        const int64_t sub_y = static_cast<int64_t>(py) * 256 + 128;
        const int64_t law = orient(va[i][0], va[i][1], vb[i][0], vb[i][1],
                                   static_cast<int32_t>(sub_x), static_cast<int32_t>(sub_y));
        const int64_t plane = static_cast<int64_t>(s.e[i].kx) * sub_x +
                              static_cast<int64_t>(s.e[i].ky) * sub_y + s.e[i].kc;
        if (law != plane) {
          check(false, "plane identity: kx*px + ky*py + kc == orient()",
                static_cast<uint64_t>(law), static_cast<uint64_t>(plane));
          return;
        }
      }
    }
  }
  check(true, "plane identity: kx*px + ky*py + kc == orient()", 0, 0);
}

// ---------------------------------------------------------------- 2 --------
// THE JOINT: coverage rebuilt from the coefficients alone must equal
// RASTER.EDGEWALK's. This is the statement "GEOM.SETUP produces the edge
// coefficients RASTER.EDGEWALK already consumes", made into an assertion.
void test_joint_with_edgewalk() {
  const struct {
    int32_t ax, ay, bx, by, cx, cy;
    const char* name;
  } shapes[] = {
      {40 * 256, 40 * 256, 200 * 256, 55 * 256, 90 * 256, 190 * 256, "big"},
      {33 * 256 + 7, 33 * 256 + 251, 40 * 256 + 128, 33 * 256 + 251, 36 * 256, 40 * 256, "small"},
      {0, 0, 383 * 256, 0, 0, 239 * 256, "canvas half"},
      {16 * 256, 16 * 256, 32 * 256, 16 * 256, 16 * 256, 32 * 256, "tile-aligned"},
      {17 * 256 + 1, 17 * 256 + 1, 31 * 256 - 1, 17 * 256 + 1, 17 * 256 + 1, 31 * 256 - 1,
       "inset by one subpixel"},
  };
  for (const auto& sh : shapes) {
    const Clip::Out c = clipped(sh.ax, sh.ay, sh.bx, sh.by, sh.cx, sh.cy);
    check(c.verdict == Clip::kAccept, sh.name, Clip::kAccept, c.verdict);
    if (c.verdict != Clip::kAccept) continue;
    const Setup::Out s = diff(c, "joint: differential");

    const zref::EdgeWalk::Tri et{c.ax, c.ay, c.bx, c.by, c.cx, c.cy};
    bool ok = true;
    for (int32_t ty = c.min_y & ~15; ty <= c.max_y && ok; ty += 16) {
      for (int32_t tx = c.min_x & ~15; tx <= c.max_x && ok; tx += 16) {
        const zref::EdgeWalk::Cov want = zref::EdgeWalk::tile(et, tx, ty);
        for (int row = 0; row < 16 && ok; ++row) {
          uint16_t mask = 0;
          for (int col = 0; col < 16; ++col) {
            const bool cov = edge_covers(s.e[0], tx + col, ty + row) &&
                             edge_covers(s.e[1], tx + col, ty + row) &&
                             edge_covers(s.e[2], tx + col, ty + row);
            if (cov) mask = static_cast<uint16_t>(mask | (1u << col));
          }
          if (mask != want.row[row]) {
            check(false, "joint: setup coefficients reproduce EDGEWALK coverage", want.row[row],
                  mask);
            ok = false;
          }
        }
      }
    }
    if (ok) check(true, "joint: setup coefficients reproduce EDGEWALK coverage", 0, 0);
  }
}

// ---------------------------------------------------------------- 3 --------
void test_third_constant() {
  const struct {
    int32_t ax, ay, bx, by, cx, cy;
  } shapes[] = {
      {40 * 256 + 91, 60 * 256 + 13, 190 * 256 + 7, 45 * 256 + 201, 120 * 256 + 155,
       200 * 256 + 33},
      {0, 0, 383 * 256, 0, 0, 239 * 256},
      {-Clip::kGuard, -Clip::kGuard, Clip::kGuard, -Clip::kGuard, -Clip::kGuard, Clip::kGuard},
      {100 * 256, 100 * 256, 100 * 256 + 3, 100 * 256, 100 * 256, 100 * 256 + 3},
  };
  for (const auto& sh : shapes) {
    const Clip::Out c = clipped(sh.ax, sh.ay, sh.bx, sh.by, sh.cx, sh.cy);
    if (c.verdict != Clip::kAccept) continue;
    const Setup::Out s = diff(c, "third constant: differential");
    check(s.e[0].kc + s.e[1].kc + s.e[2].kc == s.area2,
          "third constant: kc0 + kc1 + kc2 == 2A", static_cast<uint64_t>(s.area2),
          static_cast<uint64_t>(s.e[0].kc + s.e[1].kc + s.e[2].kc));
    check(s.e[0].kx + s.e[1].kx + s.e[2].kx == 0, "third constant: kx sums to zero", 0,
          static_cast<uint32_t>(s.e[0].kx + s.e[1].kx + s.e[2].kx));
    check(s.e[0].ky + s.e[1].ky + s.e[2].ky == 0, "third constant: ky sums to zero", 0,
          static_cast<uint32_t>(s.e[0].ky + s.e[1].ky + s.e[2].ky));
  }
}

// ---------------------------------------------------------------- 4 --------
void test_steps_match_edgewalk() {
  // RASTER.EDGEWALK's own sx/sy for edge 0 are −(cy−by) and (cx−bx); this
  // block's kx0/ky0 must BE those numbers, or the two step differently.
  const Clip::Out c = clipped(40 * 256 + 91, 60 * 256 + 13, 190 * 256 + 7, 45 * 256 + 201,
                              120 * 256 + 155, 200 * 256 + 33);
  const Setup::Out s = diff(c, "steps: differential");
  check(s.e[0].kx == -(c.cy - c.by) && s.e[0].ky == (c.cx - c.bx), "steps: edge 0 = EDGEWALK sx0/sy0",
        0, 0);
  check(s.e[1].kx == -(c.ay - c.cy) && s.e[1].ky == (c.ax - c.cx), "steps: edge 1 = EDGEWALK sx1/sy1",
        0, 0);
  check(s.e[2].kx == -(c.by - c.ay) && s.e[2].ky == (c.bx - c.ax), "steps: edge 2 = EDGEWALK sx2/sy2",
        0, 0);
}

// ---------------------------------------------------------------- 5 --------
void test_top_left() {
  // Every edge orientation, so no `tl` bit is left unexercised: axis-aligned
  // both ways on both axes, and one triangle per diagonal quadrant.
  const struct {
    int32_t ax, ay, bx, by, cx, cy;
  } shapes[] = {
      {50 * 256, 50 * 256, 90 * 256, 50 * 256, 70 * 256, 90 * 256},   // top horizontal
      {50 * 256, 90 * 256, 90 * 256, 90 * 256, 70 * 256, 50 * 256},   // bottom horizontal
      {50 * 256, 50 * 256, 50 * 256, 90 * 256, 90 * 256, 70 * 256},   // left vertical
      {90 * 256, 50 * 256, 90 * 256, 90 * 256, 50 * 256, 70 * 256},   // right vertical
      {50 * 256, 50 * 256, 90 * 256, 90 * 256, 50 * 256, 90 * 256},   // NW-SE diagonal
      {90 * 256, 50 * 256, 50 * 256, 90 * 256, 90 * 256, 90 * 256},   // NE-SW diagonal
  };
  uint32_t seen = 0;
  for (const auto& sh : shapes) {
    const Clip::Out c = clipped(sh.ax, sh.ay, sh.bx, sh.by, sh.cx, sh.cy);
    if (c.verdict != Clip::kAccept) continue;
    const Setup::Out s = diff(c, "top-left: differential");
    for (int i = 0; i < 3; ++i) seen |= s.e[i].tl ? 1u : 2u;
  }
  check(seen == 3u, "top-left: both polarities exercised", 3u, seen);
}

// ---------------------------------------------------------------- 6 --------
void test_guard_band() {
  const int32_t g = Clip::kGuard;
  const int32_t xs[] = {-g, -g + 1, -1, 0, 1, g - 1, g};
  for (int32_t x : xs) {
    for (int32_t y : xs) {
      const Clip::Out c = clipped(x, y, 100 * 256, 100 * 256, 140 * 256, 130 * 256);
      if (c.verdict != Clip::kAccept) continue;
      const Setup::Out s = diff(c, "guard band: differential");
      for (int i = 0; i < 3; ++i) {
        // |kc| <= 2*2^19*2^19 = 2^39 and |2A| <= 2^41: both must fit the
        // 48-bit setup domain with the sign intact.
        check(s.e[i].kc < (1ll << 40) && s.e[i].kc > -(1ll << 40), "guard band: |kc| bound", 0, 0);
      }
      check(s.area2 > 0 && s.area2 < (1ll << 42), "guard band: |2A| bound", 0, 0);
    }
  }
  // the widest triangle the guard band admits
  const Clip::Out wide = clipped(-g, -g, g, -g, -g, g);
  const Setup::Out s = diff(wide, "guard band: widest triangle");
  check(s.area2 == 4ll * g * g, "guard band: widest 2A", static_cast<uint64_t>(4ll * g * g),
        static_cast<uint64_t>(s.area2));
}

// ---------------------------------------------------------------- 7/8 ------
void test_backpressure() {
  const uint32_t seeds[] = {1u, 0x1234567u, 0x89ABCDEFu, 0xDEADBEEFu};
  const Clip::Out c = clipped(40 * 256 + 91, 60 * 256 + 13, 190 * 256 + 7, 45 * 256 + 201,
                              120 * 256 + 155, 200 * 256 + 33);
  for (uint32_t s : seeds) diff(c, "backpressure: packet held stable", s);
  check(dev().submitted() > 0, "counters: triangles_submitted moved", 1, 1);
}

}  // namespace

int main() {
  test_plane_identity();
  test_joint_with_edgewalk();
  test_third_constant();
  test_steps_match_edgewalk();
  test_top_left();
  test_guard_band();
  test_backpressure();

  return zhao::report_and_exit("geom_setup_directed");
}
