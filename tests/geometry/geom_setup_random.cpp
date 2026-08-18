// geom_setup_random.cpp — GEOM.SETUP randomized differential test
// (design/contracts/GEOM.SETUP.md "Randomized differential tests"; law
// spec/qformats.md §8 + reference/src/zrender/rast.cpp).
//
// Two lanes, deterministic from fixed seeds:
//
//   LANE A — coefficient differential. PCG triangles across four populations
//     (canvas-local, guard-band-wide, slivers, tile-aligned), each pushed
//     through zref::Clip first so the block sees exactly what GEOM.CLIP emits,
//     a third with out_ready_i gated. All nine coefficients, the three
//     top-left bits, 2A and the passthrough must equal zref::Setup EXACTLY.
//     The lane additionally checks the barycentric identity the RTL relies on
//     (kc0 + kc1 + kc2 == 2A) on every iteration — the oracle computes all
//     three constants directly, the RTL derives the third, so agreeing IS the
//     proof that the two-multiplier saving is exact.
//
//   LANE B — THE JOINT. For a PCG tile of each triangle's scan box, coverage
//     is rebuilt from the RTL's coefficients alone (E0 = kx·px + ky·py + kc,
//     then the §8 narrow fill test) and diffed against zref::EdgeWalk. A sign
//     flip on any coefficient, a wrong top-left bit or an off-by-one in kc
//     fails this immediately.
//
// Modes: default = 4,000 / 4,000 (CTest fast); --nightly = 60,000 / 30,000.

#define ZHAO_GEOM_DEV_SETUP
#include "geom_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "zref/zref_edgewalk.hpp"

using zhao::check;
using zhao_geom::Prng;
using zhao_geom::SetupDev;
using zref::Clip;
using zref::Setup;

namespace {

int failures = 0;
const Clip::Viewport kVp{0, 0, 384, 240};

std::vector<uint8_t> serialize(const Clip::Out& c) {
  std::vector<uint8_t> v;
  auto put = [&v](int32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(static_cast<uint32_t>(x) >> (8 * i)));
  };
  put(c.ax);
  put(c.ay);
  put(c.bx);
  put(c.by);
  put(c.cx);
  put(c.cy);
  return v;
}

bool same(const Setup::Out& a, const Setup::Out& b) {
  if (a.area2 != b.area2) return false;
  for (int i = 0; i < 3; ++i)
    if (a.e[i].kx != b.e[i].kx || a.e[i].ky != b.e[i].ky || a.e[i].kc != b.e[i].kc ||
        a.e[i].tl != b.e[i].tl)
      return false;
  return true;
}

std::string describe(const Setup::Out& want, const Setup::Out& got) {
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "2A %lld/%lld  e0 (%d,%d,%lld,%d)/(%d,%d,%lld,%d)  e1 (%d,%d,%lld,%d)/(%d,%d,%lld,%d)"
                "  e2 (%d,%d,%lld,%d)/(%d,%d,%lld,%d)",
                static_cast<long long>(want.area2), static_cast<long long>(got.area2),
                want.e[0].kx, want.e[0].ky, static_cast<long long>(want.e[0].kc), want.e[0].tl,
                got.e[0].kx, got.e[0].ky, static_cast<long long>(got.e[0].kc), got.e[0].tl,
                want.e[1].kx, want.e[1].ky, static_cast<long long>(want.e[1].kc), want.e[1].tl,
                got.e[1].kx, got.e[1].ky, static_cast<long long>(got.e[1].kc), got.e[1].tl,
                want.e[2].kx, want.e[2].ky, static_cast<long long>(want.e[2].kc), want.e[2].tl,
                got.e[2].kx, got.e[2].ky, static_cast<long long>(got.e[2].kc), got.e[2].tl);
  return std::string(buf);
}

// A PCG triangle from population `pop`, already through GEOM.CLIP's law.
Clip::Out draw_clipped(Prng& r, int pop) {
  const int32_t g = Clip::kGuard;
  Clip::In t;
  switch (pop) {
    case 0:
      t.ax = r.span(0, 383 * 256);
      t.ay = r.span(0, 239 * 256);
      t.bx = r.span(0, 383 * 256);
      t.by = r.span(0, 239 * 256);
      t.cx = r.span(0, 383 * 256);
      t.cy = r.span(0, 239 * 256);
      break;
    case 1:
      t.ax = r.span(-g, g);
      t.ay = r.span(-g, g);
      t.bx = r.span(-g, g);
      t.by = r.span(-g, g);
      t.cx = r.span(-g, g);
      t.cy = r.span(-g, g);
      break;
    case 2: {
      const int32_t x = r.span(0, 383 * 256);
      const int32_t y = r.span(0, 239 * 256);
      const int32_t dx = r.span(-4096, 4096);
      const int32_t dy = r.span(-4096, 4096);
      t.ax = x;
      t.ay = y;
      t.bx = x + dx;
      t.by = y + dy;
      t.cx = x + 2 * dx + r.span(-64, 64);
      t.cy = y + 2 * dy + r.span(-64, 64);
      break;
    }
    default: {  // tile-aligned corners, where the top-left bits all fire
      t.ax = r.span(0, 23) * 4096;
      t.ay = r.span(0, 14) * 4096;
      t.bx = r.span(0, 23) * 4096;
      t.by = r.span(0, 14) * 4096;
      t.cx = r.span(0, 23) * 4096;
      t.cy = r.span(0, 14) * 4096;
      break;
    }
  }
  return Clip::clip(t, kVp, Clip::kCullNone);
}

bool edge_covers(const Setup::Edge& e, int32_t px, int32_t py) {
  const int64_t sx = static_cast<int64_t>(px) * 256 + 128;
  const int64_t sy = static_cast<int64_t>(py) * 256 + 128;
  const int64_t e0 = static_cast<int64_t>(e.kx) * sx + static_cast<int64_t>(e.ky) * sy + e.kc;
  return zref::fill_accept(e0 >> 8, (e0 & 255) != 0, e.tl);
}

void fail(const char* lane, uint32_t i, const Clip::Out& c, const std::string& body) {
  ++failures;
  if (failures <= 8) {
    char name[64];
    std::snprintf(name, sizeof(name), "geom_setup_%s_%u", lane, i);
    std::printf("FAIL %s\n    %s\n", name, body.c_str());
    zhao::save_failing_vector(name, serialize(c), "zref::Setup", body);
  }
}

void lane_a(SetupDev& dev, uint32_t iters) {
  Prng r(0x5E7079A1u);
  Prng s(0x5741115u);
  for (uint32_t i = 0; i < iters; ++i) {
    const Clip::Out c = draw_clipped(r, static_cast<int>(r.draw() % 4u));
    if (c.verdict != Clip::kAccept) continue;
    const uint32_t stall = ((i % 3u) == 0u) ? (s.draw() | 1u) : 0u;
    std::string err;
    const Setup::Out got = dev.run(c, static_cast<uint16_t>(i), stall, &err);
    const Setup::Out want = Setup::setup(c.ax, c.ay, c.bx, c.by, c.cx, c.cy, c.area2);
    if (!err.empty() || !same(got, want)) {
      fail("a", i, c, describe(want, got) + (err.empty() ? std::string() : ("\n  protocol: " + err)));
      continue;
    }
    if (got.e[0].kc + got.e[1].kc + got.e[2].kc != got.area2)
      fail("a", i, c, "kc0 + kc1 + kc2 != 2A (the derived third constant is wrong)");
    if (got.e[0].kx + got.e[1].kx + got.e[2].kx != 0 ||
        got.e[0].ky + got.e[1].ky + got.e[2].ky != 0)
      fail("a", i, c, "the edge steps do not sum to zero");
  }
}

void lane_b(SetupDev& dev, uint32_t iters) {
  Prng r(0x105EED7u);
  for (uint32_t i = 0; i < iters; ++i) {
    const Clip::Out c = draw_clipped(r, static_cast<int>(r.draw() % 4u));
    if (c.verdict != Clip::kAccept) continue;
    std::string err;
    const Setup::Out got = dev.run(c, static_cast<uint16_t>(i), 0, &err);
    if (!err.empty()) {
      fail("b", i, c, "protocol: " + err);
      continue;
    }
    // one PCG tile of the scan box
    const int32_t tx0 = c.min_x >> 4, tx1 = c.max_x >> 4;
    const int32_t ty0 = c.min_y >> 4, ty1 = c.max_y >> 4;
    const int32_t tx = (tx0 + static_cast<int32_t>(r.draw() % static_cast<uint32_t>(tx1 - tx0 + 1)))
                       << 4;
    const int32_t ty = (ty0 + static_cast<int32_t>(r.draw() % static_cast<uint32_t>(ty1 - ty0 + 1)))
                       << 4;
    const zref::EdgeWalk::Cov want = zref::EdgeWalk::tile({c.ax, c.ay, c.bx, c.by, c.cx, c.cy}, tx,
                                                          ty);
    for (int row = 0; row < 16; ++row) {
      uint16_t mask = 0;
      for (int col = 0; col < 16; ++col) {
        if (edge_covers(got.e[0], tx + col, ty + row) &&
            edge_covers(got.e[1], tx + col, ty + row) && edge_covers(got.e[2], tx + col, ty + row))
          mask = static_cast<uint16_t>(mask | (1u << col));
      }
      if (mask != want.row[row]) {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "joint: tile (%d,%d) row %d — EDGEWALK %04X, from coefficients %04X", tx, ty,
                      row, want.row[row], mask);
        fail("b", i, c, buf);
        break;
      }
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  SetupDev dev;
  lane_a(dev, nightly ? 60000u : 4000u);
  lane_b(dev, nightly ? 30000u : 4000u);

  check(failures == 0, "geom_setup_random: differential", 0, static_cast<uint32_t>(failures));
  return zhao::report_and_exit("geom_setup_random");
}
