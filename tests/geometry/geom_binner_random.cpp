// geom_binner_random.cpp — GEOM.BINNER randomized differential test
// (design/contracts/GEOM.BINNER.md "Randomized differential tests").
//
// Two lanes, deterministic from fixed seeds:
//
//   LANE A — tile-list differential. PCG scenes of 1..12 triangles across four
//     populations (canvas-local, tile-sized, thin diagonals, single-tile), each
//     pushed through the zref::Clip / zref::Setup oracles first so the block
//     sees exactly what the real chain emits. The whole drained job stream —
//     every tile, in order, with the right triangle and src_id — must equal the
//     expectation built from zref::Binner. A third of the scenes run with
//     job_ready_i gated by a second PCG stream.
//
//   LANE B — SOUNDNESS against the coverage oracle. For a single PCG triangle,
//     EVERY tile of the grid is walked with zref::EdgeWalk and every tile with
//     non-zero coverage must appear in the drained list. This is the property
//     the binning law exists to keep: the trivial-reject is allowed to be
//     conservative (an empty tile costs an edge walk) and is never allowed to
//     be optimistic (a lost tile is a hole in the picture). It is checked
//     against the FILL LAW, not against the binner's own oracle, so a shared
//     mistake in both would still be caught.
//
// Modes: default = 400 scenes / 600 triangles (CTest fast); --nightly =
// 6,000 / 8,000. Every failing vector is saved (charter §29-17).

#define ZHAO_GEOM_DEV_BINNER
#include "geom_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "zref/zref_edgewalk.hpp"

using zhao::check;
using zhao_geom::BinJob;
using zhao_geom::BinnerDev;
using zhao_geom::BinStatus;
using zhao_geom::BinTri;
using zhao_geom::make_bin_tri;
using zhao_geom::Prng;
using zref::Binner;
using zref::Clip;

namespace {

int failures = 0;
const Clip::Viewport kVp{0, 0, 384, 240};
const int kGridW = 24;
const int kGridH = 15;

std::vector<BinJob> expect(const std::vector<BinTri>& tris) {
  std::vector<std::vector<Binner::Ref>> refs;
  refs.reserve(tris.size());
  for (const BinTri& t : tris) {
    if (!t.token)
      refs.emplace_back();
    else
      refs.push_back(Binner::bin(t.s, t.min_x, t.max_x, t.min_y, t.max_y));
  }
  std::vector<BinJob> out;
  for (int ty = 0; ty < kGridH; ++ty)
    for (int tx = 0; tx < kGridW; ++tx)
      for (size_t i = 0; i < tris.size(); ++i)
        for (const Binner::Ref& r : refs[i])
          if (r.tx == tx && r.ty == ty) {
            BinJob j;
            j.ax = tris[i].ax;
            j.ay = tris[i].ay;
            j.bx = tris[i].bx;
            j.by = tris[i].by;
            j.cx = tris[i].cx;
            j.cy = tris[i].cy;
            j.tx = tx;
            j.ty = ty;
            j.src_id = tris[i].src_id;
            out.push_back(j);
          }
  return out;
}

bool same(const BinJob& a, const BinJob& b) {
  return a.ax == b.ax && a.ay == b.ay && a.bx == b.bx && a.by == b.by && a.cx == b.cx &&
         a.cy == b.cy && a.tx == b.tx && a.ty == b.ty && a.src_id == b.src_id;
}

std::vector<uint8_t> serialize(const std::vector<BinTri>& tris) {
  std::vector<uint8_t> v;
  auto put = [&v](int32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(static_cast<uint32_t>(x) >> (8 * i)));
  };
  for (const BinTri& t : tris) {
    put(t.ax);
    put(t.ay);
    put(t.bx);
    put(t.by);
    put(t.cx);
    put(t.cy);
    put(t.token ? 1 : 0);
  }
  return v;
}

void fail(const char* lane, uint32_t i, const std::vector<BinTri>& tris, const std::string& body) {
  ++failures;
  if (failures <= 8) {
    char name[64];
    std::snprintf(name, sizeof(name), "geom_binner_%s_%u", lane, i);
    std::printf("FAIL %s\n    %s\n", name, body.c_str());
    zhao::save_failing_vector(name, serialize(tris), "zref::Binner", body);
  }
}

// One PCG triangle, already through the GEOM.CLIP / GEOM.SETUP oracles.
bool draw_tri(Prng& r, int pop, uint16_t src, BinTri* out) {
  int32_t ax, ay, bx, by, cx, cy;
  switch (pop) {
    case 0:  // canvas-local
      ax = r.span(0, 383 * 256);
      ay = r.span(0, 239 * 256);
      bx = r.span(0, 383 * 256);
      by = r.span(0, 239 * 256);
      cx = r.span(0, 383 * 256);
      cy = r.span(0, 239 * 256);
      break;
    case 1: {  // tile-sized, at a PCG tile
      const int32_t ox = r.span(0, 23) * 4096;
      const int32_t oy = r.span(0, 14) * 4096;
      ax = ox + r.span(0, 4095);
      ay = oy + r.span(0, 4095);
      bx = ox + r.span(0, 4095);
      by = oy + r.span(0, 4095);
      cx = ox + r.span(0, 4095);
      cy = oy + r.span(0, 4095);
      break;
    }
    case 2: {  // a thin diagonal across the canvas — the case a bbox-only
               // binner gets catastrophically wrong
      ax = r.span(0, 40 * 256);
      ay = r.span(0, 40 * 256);
      bx = r.span(340 * 256, 383 * 256);
      by = r.span(200 * 256, 239 * 256);
      cx = ax + r.span(-1024, 1024);
      cy = ay + r.span(256, 3 * 256);
      break;
    }
    default: {  // deliberately spanning a tile boundary
      const int32_t ox = r.span(1, 22) * 4096 - r.span(0, 2048);
      const int32_t oy = r.span(1, 13) * 4096 - r.span(0, 2048);
      ax = ox;
      ay = oy;
      bx = ox + r.span(2048, 8192);
      by = oy + r.span(-1024, 1024);
      cx = ox + r.span(-1024, 1024);
      cy = oy + r.span(2048, 8192);
      break;
    }
  }
  return make_bin_tri(ax, ay, bx, by, cx, cy, kVp, src, out);
}

void lane_a(BinnerDev& dev, uint32_t scenes) {
  Prng r(0xB177E12Au);
  Prng s(0x0B5E12Eu);
  for (uint32_t i = 0; i < scenes; ++i) {
    const uint32_t n = 1u + (r.draw() % 12u);
    std::vector<BinTri> tris;
    for (uint32_t k = 0; k < n; ++k) {
      BinTri t;
      if (!draw_tri(r, static_cast<int>(r.draw() % 4u), static_cast<uint16_t>(k), &t)) continue;
      t.token = (r.draw() % 16u) != 0u;  // a token denial now and then
      tris.push_back(t);
    }
    if (tris.empty()) continue;
    const uint32_t stall = ((i % 3u) == 0u) ? (s.draw() | 1u) : 0u;

    std::string err;
    BinStatus st;
    const std::vector<BinJob> got = dev.frame(tris, kGridW, kGridH, stall, &st, &err);
    const std::vector<BinJob> want = expect(tris);
    if (!err.empty()) {
      fail("a", i, tris, "protocol: " + err);
      continue;
    }
    if (st.overflow) continue;  // overflow scenes are lane-specific (directed)
    if (got.size() != want.size()) {
      char buf[128];
      std::snprintf(buf, sizeof(buf), "job count: oracle %u, rtl %u",
                    static_cast<unsigned>(want.size()), static_cast<unsigned>(got.size()));
      fail("a", i, tris, buf);
      continue;
    }
    for (size_t k = 0; k < got.size(); ++k) {
      if (!same(got[k], want[k])) {
        char buf[192];
        std::snprintf(buf, sizeof(buf),
                      "job %u: oracle tile (%d,%d) src %u, rtl tile (%d,%d) src %u",
                      static_cast<unsigned>(k), want[k].tx, want[k].ty, want[k].src_id, got[k].tx,
                      got[k].ty, got[k].src_id);
        fail("a", i, tris, buf);
        break;
      }
    }
    if (st.tile_references != got.size()) {
      char buf[128];
      std::snprintf(buf, sizeof(buf), "tile_references %u but %u jobs drained", st.tile_references,
                    static_cast<unsigned>(got.size()));
      fail("a", i, tris, buf);
    }
  }
}

void lane_b(BinnerDev& dev, uint32_t iters) {
  Prng r(0x50D0B12Cu);
  for (uint32_t i = 0; i < iters; ++i) {
    BinTri t;
    if (!draw_tri(r, static_cast<int>(r.draw() % 4u), 0, &t)) continue;
    std::string err;
    BinStatus st;
    const std::vector<BinJob> got = dev.frame({t}, kGridW, kGridH, 0, &st, &err);
    if (!err.empty()) {
      fail("b", i, {t}, "protocol: " + err);
      continue;
    }
    bool seen[24][15] = {};
    for (const BinJob& j : got) {
      if (j.tx < 0 || j.tx >= kGridW || j.ty < 0 || j.ty >= kGridH) {
        fail("b", i, {t}, "a job addressed a tile outside the grid");
        break;
      }
      seen[j.tx][j.ty] = true;
    }
    const zref::EdgeWalk::Tri et{t.ax, t.ay, t.bx, t.by, t.cx, t.cy};
    for (int ty = 0; ty < kGridH; ++ty) {
      for (int tx = 0; tx < kGridW; ++tx) {
        if (zref::EdgeWalk::tile(et, tx * 16, ty * 16).count == 0) continue;
        if (!seen[tx][ty]) {
          char buf[128];
          std::snprintf(buf, sizeof(buf), "tile (%d,%d) HAS coverage but was not binned", tx, ty);
          fail("b", i, {t}, buf);
          ty = kGridH;
          break;
        }
      }
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  BinnerDev dev;
  lane_a(dev, nightly ? 6000u : 400u);
  lane_b(dev, nightly ? 8000u : 600u);

  check(failures == 0, "geom_binner_random: differential", 0, static_cast<uint32_t>(failures));
  return zhao::report_and_exit("geom_binner_random");
}
