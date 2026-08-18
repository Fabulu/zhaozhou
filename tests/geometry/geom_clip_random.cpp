// geom_clip_random.cpp — GEOM.CLIP randomized differential test
// (design/contracts/GEOM.CLIP.md "Randomized differential tests"; law
// spec/qformats.md §8 + reference/src/zrender/rast.cpp).
//
// Two lanes, both fully deterministic from one base seed (the PCG shape used
// by every other random lane in this tree):
//
//   LANE A — packet differential. PCG triangles across six populations
//     (canvas-local, guard-band-wide, slivers, exactly degenerate, wholly
//     off-screen, and behind-the-eye) against all four shipping viewports and
//     all three cull modes, a third of them with out_ready_i gated by a second
//     PCG stream. The verdict, the winding-normalised vertices, 2A and the
//     four scan-box edges must equal zref::Clip's EXACTLY — and zref::Clip
//     gets its box by calling raster_tri's own scan_bbox.
//
//   LANE B — the property that makes a reject SAFE. A kOffscreen verdict is a
//     promise that the triangle covers no pixel of the viewport, and here it
//     is checked against the coverage oracle rather than against the box:
//     every tile of the viewport is walked with zref::EdgeWalk and the total
//     must be zero. A guard-band off-by-one in either direction breaks this
//     immediately in one direction, and lane A in the other.
//
// Modes: default = 4,000 iterations per lane (CTest fast); --nightly = 60,000.
// Every failing vector is saved (charter §29-17).

#define ZHAO_GEOM_DEV_CLIP
#include "geom_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "zref/zref_edgewalk.hpp"

using zhao::check;
using zhao_geom::ClipDev;
using zhao_geom::Prng;
using zref::Clip;

namespace {

int failures = 0;

const Clip::Viewport kVps[4] = {
    {0, 0, 384, 240},    // Z60
    {0, 0, 320, 240},    // Storm
    {0, 0, 256, 192},    // Duo view 0
    {0, 192, 256, 192},  // Duo view 1 (video_rules.md §3.1, stacked)
};

std::vector<uint8_t> serialize(const Clip::In& t, const Clip::Viewport& vp, int cull) {
  std::vector<uint8_t> v;
  auto put = [&v](int32_t x) {
    for (int i = 0; i < 4; ++i)
      v.push_back(static_cast<uint8_t>(static_cast<uint32_t>(x) >> (8 * i)));
  };
  put(t.ax);
  put(t.ay);
  put(t.bx);
  put(t.by);
  put(t.cx);
  put(t.cy);
  put(t.behind);
  put(vp.x0);
  put(vp.y0);
  put(vp.w);
  put(vp.h);
  put(cull);
  return v;
}

std::string describe(const Clip::Out& want, const Clip::Out& got) {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "verdict oracle %d rtl %d; box oracle [%d..%d]x[%d..%d] rtl [%d..%d]x[%d..%d]; "
                "2A oracle %lld rtl %lld",
                static_cast<int>(want.verdict), static_cast<int>(got.verdict), want.min_x,
                want.max_x, want.min_y, want.max_y, got.min_x, got.max_x, got.min_y, got.max_y,
                static_cast<long long>(want.area2), static_cast<long long>(got.area2));
  return std::string(buf);
}

bool packets_equal(const Clip::Out& a, const Clip::Out& b) {
  if (a.verdict != b.verdict) return false;
  if (a.verdict != Clip::kAccept) return true;
  return a.ax == b.ax && a.ay == b.ay && a.bx == b.bx && a.by == b.by && a.cx == b.cx &&
         a.cy == b.cy && a.area2 == b.area2 && a.min_x == b.min_x && a.max_x == b.max_x &&
         a.min_y == b.min_y && a.max_y == b.max_y;
}

// One PCG triangle from population `pop`.
Clip::In draw_tri(Prng& r, int pop) {
  const int32_t g = Clip::kGuard;
  Clip::In t;
  switch (pop) {
    case 0:  // canvas-local
      t.ax = r.span(0, 383 * 256);
      t.ay = r.span(0, 239 * 256);
      t.bx = r.span(0, 383 * 256);
      t.by = r.span(0, 239 * 256);
      t.cx = r.span(0, 383 * 256);
      t.cy = r.span(0, 239 * 256);
      break;
    case 1:  // guard-band-wide
      t.ax = r.span(-g, g);
      t.ay = r.span(-g, g);
      t.bx = r.span(-g, g);
      t.by = r.span(-g, g);
      t.cx = r.span(-g, g);
      t.cy = r.span(-g, g);
      break;
    case 2: {  // sliver: three nearly collinear points
      const int32_t x = r.span(0, 383 * 256);
      const int32_t y = r.span(0, 239 * 256);
      const int32_t dx = r.span(-4096, 4096);
      const int32_t dy = r.span(-4096, 4096);
      t.ax = x;
      t.ay = y;
      t.bx = x + dx;
      t.by = y + dy;
      t.cx = x + 2 * dx + r.span(-2, 2);
      t.cy = y + 2 * dy + r.span(-2, 2);
      break;
    }
    case 3: {  // exactly degenerate: collinear by construction
      const int32_t x = r.span(0, 383 * 256);
      const int32_t y = r.span(0, 239 * 256);
      const int32_t dx = r.span(-8192, 8192);
      const int32_t dy = r.span(-8192, 8192);
      const int32_t k = r.span(2, 5);
      t.ax = x;
      t.ay = y;
      t.bx = x + dx;
      t.by = y + dy;
      t.cx = x + k * dx;
      t.cy = y + k * dy;
      break;
    }
    case 4: {  // wholly off-screen: a small triangle placed outside
      const int32_t ox = (r.draw() & 1u) ? r.span(-g, -600 * 256) : r.span(600 * 256, g);
      const int32_t oy = (r.draw() & 1u) ? r.span(-g, -600 * 256) : r.span(600 * 256, g);
      t.ax = ox;
      t.ay = oy;
      t.bx = ox + r.span(-2000, 2000);
      t.by = oy + r.span(-2000, 2000);
      t.cx = ox + r.span(-2000, 2000);
      t.cy = oy + r.span(-2000, 2000);
      break;
    }
    default:  // canvas-local, but behind the eye
      t.ax = r.span(0, 383 * 256);
      t.ay = r.span(0, 239 * 256);
      t.bx = r.span(0, 383 * 256);
      t.by = r.span(0, 239 * 256);
      t.cx = r.span(0, 383 * 256);
      t.cy = r.span(0, 239 * 256);
      t.behind = static_cast<uint8_t>(1u + (r.draw() % 7u));
      break;
  }
  return t;
}

void lane_a(ClipDev& dev, uint32_t iters) {
  Prng r(0x5EED1C11u);
  Prng s(0xC0FFEE11u);
  for (uint32_t i = 0; i < iters; ++i) {
    const int pop = static_cast<int>(r.draw() % 6u);
    const Clip::In t = draw_tri(r, pop);
    const Clip::Viewport& vp = kVps[r.draw() % 4u];
    const Clip::CullMode cull = static_cast<Clip::CullMode>(r.draw() % 3u);
    const uint32_t stall = ((i % 3u) == 0u) ? (s.draw() | 1u) : 0u;

    std::string err;
    const Clip::Out got = dev.run(t, vp, cull, static_cast<uint16_t>(i), stall, &err);
    const Clip::Out want = Clip::clip(t, vp, cull);
    if (!err.empty() || !packets_equal(got, want)) {
      ++failures;
      if (failures <= 8) {
        char name[64];
        std::snprintf(name, sizeof(name), "geom_clip_a_%u", i);
        const std::string body =
            describe(want, got) + (err.empty() ? std::string() : ("\n  protocol: " + err));
        std::printf("FAIL %s\n    %s\n", name, body.c_str());
        zhao::save_failing_vector(name, serialize(t, vp, cull), "zref::Clip", body);
      }
    }
  }
}

// A kOffscreen verdict must never delete coverage: walk every tile of the
// viewport with the coverage oracle and require zero covered pixels.
void lane_b(ClipDev& dev, uint32_t iters) {
  Prng r(0x1DEA5EEDu);
  for (uint32_t i = 0; i < iters; ++i) {
    // deliberately biased toward the viewport edges, where an off-by-one lives
    const Clip::Viewport& vp = kVps[r.draw() % 4u];
    const int32_t x0 = static_cast<int32_t>(vp.x0) * 256;
    const int32_t y0 = static_cast<int32_t>(vp.y0) * 256;
    const int32_t x1 = static_cast<int32_t>(vp.x0 + vp.w) * 256;
    const int32_t y1 = static_cast<int32_t>(vp.y0 + vp.h) * 256;
    Clip::In t;
    t.ax = r.span(x0 - 1024, x0 + 1024);
    t.ay = r.span(y0 - 1024, y1 + 1024);
    t.bx = r.span(x1 - 1024, x1 + 1024);
    t.by = r.span(y0 - 1024, y1 + 1024);
    t.cx = r.span(x0 - 1024, x1 + 1024);
    t.cy = r.span(y1 - 1024, y1 + 1024);
    if ((r.draw() & 1u) != 0u) {  // half of them shoved fully outside
      const int32_t d = static_cast<int32_t>(vp.w) * 256 + 4096;
      t.ax += d;
      t.bx += d;
      t.cx += d;
    }

    std::string err;
    const Clip::Out got = dev.run(t, vp, Clip::kCullNone, static_cast<uint16_t>(i), 0, &err);
    const Clip::Out want = Clip::clip(t, vp, Clip::kCullNone);
    bool bad = !err.empty() || !packets_equal(got, want);

    if (!bad && got.verdict == Clip::kOffscreen) {
      uint32_t total = 0;
      const zref::EdgeWalk::Tri et{t.ax, t.ay, t.bx, t.by, t.cx, t.cy};
      for (uint32_t ty = vp.y0; ty < vp.y0 + vp.h && total == 0; ty += 16)
        for (uint32_t tx = vp.x0; tx < vp.x0 + vp.w && total == 0; tx += 16)
          total +=
              zref::EdgeWalk::tile(et, static_cast<int32_t>(tx), static_cast<int32_t>(ty)).count;
      if (total != 0) {
        bad = true;
        err = "kOffscreen deleted real coverage";
      }
    }

    if (bad) {
      ++failures;
      if (failures <= 8) {
        char name[64];
        std::snprintf(name, sizeof(name), "geom_clip_b_%u", i);
        const std::string body =
            describe(want, got) + (err.empty() ? std::string() : ("\n  note: " + err));
        std::printf("FAIL %s\n    %s\n", name, body.c_str());
        zhao::save_failing_vector(name, serialize(t, vp, 0), "zref::Clip", body);
      }
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;
  const uint32_t iters_a = nightly ? 60000u : 4000u;
  const uint32_t iters_b = nightly ? 20000u : 1000u;

  ClipDev dev;
  lane_a(dev, iters_a);
  lane_b(dev, iters_b);

  check(failures == 0, "geom_clip_random: differential", 0, static_cast<uint32_t>(failures));
  return zhao::report_and_exit("geom_clip_random");
}
