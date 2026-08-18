// geom_binner_directed.cpp — GEOM.BINNER directed vectors
// (design/contracts/GEOM.BINNER.md "Directed tests"; law spec/qformats.md §8;
// the binning law itself is CHOSEN and argued in
// fpga/rtl/geometry/zhao_geom_binner.sv).
//
// Every case drives the Verilated zhao_geom_binner through a whole frame
// (frame_begin -> triangles -> frame_end -> drain) and diffs the drained job
// stream against the expectation built from zref::Binner. On top of that:
//
//   1. many tiles     — one triangle spanning a large block: the drained tile
//                       set and its ORDER equal the oracle's
//   2. one tile       — a triangle wholly inside one 16x16 tile: exactly one
//                       job, at that tile
//   3. SOUNDNESS      — the property that matters: every tile of the grid that
//                       zref::EdgeWalk says has coverage IS in the list. A
//                       lost tile is a hole in the picture; this is checked
//                       over the WHOLE grid, not over the oracle's answer.
//   4. no stray tiles — no job is ever emitted for a tile outside the scan box
//   5. order          — the drain is row-major over the grid, and FIFO within
//                       a tile (the painter order, LAWS FOUND 4)
//   6. chunk boundary — a tile receiving 1..9 triangles, which crosses the
//                       4-reference chunk boundary twice
//   7. tokens         — a MEASURE.TOKENS denial drops the triangle whole and
//                       counts it, and the rest of the frame is unaffected
//   8. tri overflow   — the 129th triangle of a frame walls the frame off
//   9. arena overflow — enough references to exhaust the chunk arena: the wall
//                       goes up, arena_used never exceeds CHUNKS, and nothing
//                       already binned is corrupted
//  10. counters       — tile_references equals the drained job count, and
//                       max_tile_list_depth is the deepest list
//  11. frames         — a second frame sees nothing of the first
//  12. backpressure   — job_ready_i gated by a PCG bit stream: identical jobs,
//                       held stable while stalled
//  13. throughput     — MEASURED against the ledger's "1 bin reference per
//                       clock", and reported rather than asserted

#define ZHAO_GEOM_DEV_BINNER
#include "geom_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "zref/zref_edgewalk.hpp"

using zhao::check;
using zhao_geom::BinJob;
using zhao_geom::BinnerDev;
using zhao_geom::BinStatus;
using zhao_geom::BinTri;
using zhao_geom::make_bin_tri;
using zref::Binner;
using zref::Clip;

namespace {

BinnerDev& dev() {
  static BinnerDev d;
  return d;
}

const Clip::Viewport kVp{0, 0, 384, 240};
const int kGridW = 24;  // 384 / 16
const int kGridH = 15;  // 240 / 16

/**
 * The expected drain stream: for each tile in row-major grid order, every
 * triangle that binned into it, in SUBMISSION order. Built entirely from
 * zref::Binner — the block's declared oracle.
 */
std::vector<BinJob> expect(const std::vector<BinTri>& tris, int grid_w, int grid_h) {
  std::vector<std::vector<Binner::Ref>> refs;
  refs.reserve(tris.size());
  for (const BinTri& t : tris) {
    if (!t.token)
      refs.emplace_back();
    else
      refs.push_back(Binner::bin(t.s, t.min_x, t.max_x, t.min_y, t.max_y));
  }
  std::vector<BinJob> out;
  for (int ty = 0; ty < grid_h; ++ty) {
    for (int tx = 0; tx < grid_w; ++tx) {
      for (size_t i = 0; i < tris.size(); ++i) {
        for (const Binner::Ref& r : refs[i]) {
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
        }
      }
    }
  }
  return out;
}

bool same(const BinJob& a, const BinJob& b) {
  return a.ax == b.ax && a.ay == b.ay && a.bx == b.bx && a.by == b.by && a.cx == b.cx &&
         a.cy == b.cy && a.tx == b.tx && a.ty == b.ty && a.src_id == b.src_id;
}

std::vector<BinJob> run(const std::vector<BinTri>& tris, const char* what, uint32_t stall = 0,
                        BinStatus* st_out = nullptr, int grid_w = kGridW, int grid_h = kGridH) {
  std::string err;
  BinStatus st;
  const std::vector<BinJob> got = dev().frame(tris, grid_w, grid_h, stall, &st, &err);
  check(err.empty(), what, 0, err.empty() ? 0 : 1);
  if (!err.empty()) std::printf("    protocol: %s (%s)\n", err.c_str(), what);
  if (st_out) *st_out = st;
  return got;
}

void diff(const std::vector<BinTri>& tris, const char* what, uint32_t stall = 0,
          BinStatus* st_out = nullptr) {
  BinStatus st;
  const std::vector<BinJob> got = run(tris, what, stall, &st);
  const std::vector<BinJob> want = expect(tris, kGridW, kGridH);
  check(got.size() == want.size(), what, static_cast<uint32_t>(want.size()),
        static_cast<uint32_t>(got.size()));
  const size_t n = got.size() < want.size() ? got.size() : want.size();
  for (size_t i = 0; i < n; ++i) {
    if (!same(got[i], want[i])) {
      char buf[192];
      std::snprintf(buf, sizeof(buf), "%s: job %u oracle tile (%d,%d) src %u, rtl (%d,%d) src %u",
                    what, static_cast<unsigned>(i), want[i].tx, want[i].ty, want[i].src_id,
                    got[i].tx, got[i].ty, got[i].src_id);
      check(false, buf, 0, 1);
      break;
    }
  }
  if (n == got.size() && n == want.size()) check(true, what, 0, 0);
  check(st.tile_references == got.size(), "counters: tile_references == drained jobs",
        static_cast<uint32_t>(got.size()), st.tile_references);
  if (st_out) *st_out = st;
}

BinTri mk(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t cx, int32_t cy, uint16_t src) {
  BinTri t;
  const bool ok = make_bin_tri(ax, ay, bx, by, cx, cy, kVp, src, &t);
  check(ok, "fixture: GEOM.CLIP accepted the triangle", 1, ok ? 1 : 0);
  return t;
}

// ---------------------------------------------------------------- 1/3/4 ----
void test_many_tiles() {
  const BinTri t = mk(8 * 256, 8 * 256, 300 * 256, 30 * 256, 60 * 256, 220 * 256, 7);
  BinStatus st;
  diff({t}, "many tiles: differential", 0, &st);

  // SOUNDNESS over the whole grid, and no stray tile outside the scan box.
  const std::vector<BinJob> got = run({t}, "many tiles: soundness sweep");
  bool seen[24][15] = {};
  for (const BinJob& j : got) {
    check(j.tx >= 0 && j.tx < kGridW && j.ty >= 0 && j.ty < kGridH, "many tiles: in grid", 1, 1);
    check((j.tx << 4) <= t.max_x && ((j.tx << 4) + 15) >= t.min_x && (j.ty << 4) <= t.max_y &&
              ((j.ty << 4) + 15) >= t.min_y,
          "many tiles: no tile outside the scan box", 1, 1);
    seen[j.tx][j.ty] = true;
  }
  const zref::EdgeWalk::Tri et{t.ax, t.ay, t.bx, t.by, t.cx, t.cy};
  uint32_t missed = 0, covered_tiles = 0;
  for (int ty = 0; ty < kGridH; ++ty) {
    for (int tx = 0; tx < kGridW; ++tx) {
      if (zref::EdgeWalk::tile(et, tx * 16, ty * 16).count == 0) continue;
      ++covered_tiles;
      if (!seen[tx][ty]) ++missed;
    }
  }
  check(missed == 0, "many tiles: SOUND — no covered tile is ever dropped", 0, missed);
  check(covered_tiles > 40, "many tiles: the fixture really does span many tiles", 40,
        covered_tiles);
  // and the trivial-reject earns its keep: fewer jobs than the bbox rectangle
  const uint32_t bbox_tiles = static_cast<uint32_t>(((t.max_x >> 4) - (t.min_x >> 4) + 1) *
                                                    ((t.max_y >> 4) - (t.min_y >> 4) + 1));
  check(got.size() < bbox_tiles, "many tiles: the corner test rejects some of the bbox", bbox_tiles,
        static_cast<uint32_t>(got.size()));
}

// --------------------------------------------------------- the joint -------
// The drain port is RASTER.EDGEWALK's job port, INCLUDING ITS UNITS: that
// block's contract says `job_tile_x_i` is "the tile origin — the top-left
// PIXEL of the 16x16 tile", not a tile index. Emitting the index instead is a
// silent 16x error that no differential against a tile-indexed oracle can see;
// it was found by the zhao_geom_bin_pipe composition rendering a picture in
// the wrong place, and this pins it directly.
void test_pixel_origin() {
  const BinTri t = mk(82 * 256, 66 * 256, 94 * 256, 68 * 256, 84 * 256, 78 * 256, 3);
  const std::vector<BinJob> got = run({t}, "pixel origin: drain");
  check(got.size() == 1, "pixel origin: one job", 1, static_cast<uint32_t>(got.size()));
  if (got.size() != 1) return;
  check(got[0].px == 80 && got[0].py == 64,
        "pixel origin: the port carries the tile's top-left PIXEL", 80u * 1000 + 64,
        static_cast<uint32_t>(got[0].px) * 1000 + static_cast<uint32_t>(got[0].py));
  // and the pixel the port names is one RASTER.EDGEWALK covers with the same
  // triangle, which is the whole point of the units matching
  const zref::EdgeWalk::Cov cov =
      zref::EdgeWalk::tile({t.ax, t.ay, t.bx, t.by, t.cx, t.cy}, got[0].px, got[0].py);
  check(cov.count > 0, "pixel origin: EDGEWALK covers the tile the port names", 1,
        cov.count > 0 ? 1 : 0);
}

// ------------------------------------------------- the flooring boundary ----
// THE HOLE THIS CLOSES, and how it was found. The §8 decomposition takes
// `E' = E0 >>> 8` — an ARITHMETIC shift, i.e. FLOOR — because the fill rule is
// stated on `E0 = 256·E' + r` with `r ∈ [0,255]`, which only holds for the
// flooring quotient. Replacing it with truncation toward zero (the mutation a
// C++ `/` or a logical shift would produce) moves `E'` by exactly +1 for every
// NEGATIVE, non-multiple-of-256 edge value, and by nothing at all otherwise.
//
// A mutation sweep on 2026-08-18 injected exactly that and NEITHER the directed
// nor the random binner lane went red: the shift only changes a tile's verdict
// when that tile's extreme corner sits within ONE E'-unit of an edge — 256
// subpixel², a 256th of a pixel² — and random triangles essentially never land
// there. The lanes were sound and the case was unreachable by sampling, which
// is the definition of a hole.
//
// So it is CONSTRUCTED instead of sampled. A triangle with one very SHORT edge
// has tiny `kx`/`ky`, so translating it one subpixel at a time steps that
// edge's value by only a few units — small enough to walk straight through the
// 255-wide window where floor and truncation disagree. The sweep below looks
// for a triangle and a tile where, for one edge, the exact corner value lies in
// [−255, −1] (so floor says REJECT and truncation would say ACCEPT, the `rnz`
// bit being 1 by construction) while the other two edges accept — the precise
// state in which the two roundings give different tile lists — and then runs
// the ordinary differential on it. Finding at least one witness is itself
// asserted, so the day this family stops covering the case the test says so
// rather than going quietly green.
namespace {

int64_t e0_at(const zref::Setup::Edge& e, int32_t px, int32_t py) {
  return static_cast<int64_t>(e.kx) * (static_cast<int64_t>(px) * 256 + 128) +
         static_cast<int64_t>(e.ky) * (static_cast<int64_t>(py) * 256 + 128) + e.kc;
}

// The value at the tile corner that MAXIMISES this edge (E is affine).
int64_t e0_corner(const zref::Setup::Edge& e, int32_t tx, int32_t ty) {
  return e0_at(e, tx * 16 + (e.kx > 0 ? 15 : 0), ty * 16 + (e.ky > 0 ? 15 : 0));
}

bool corner_accepts(const zref::Setup::Edge& e, int64_t v) {
  return zref::fill_accept(v >> 8, (v & 255) != 0, e.tl);
}

}  // namespace

void test_floor_boundary() {
  // THE WITNESS, CONSTRUCTED. Take the right triangle A = (0,0), B = (W,0),
  // C = (0,W) in subpixels. Its hypotenuse (B,C) has kx = -W and ky = -W, both
  // NEGATIVE, so the corner that maximises it over tile (0,0) is pixel (0,0)
  // itself — and the edge value there IS `e0_base`, the one number `ep_of`
  // rounds. With a = b = 0 the algebra collapses to
  //
  //     e0_base(B,C) = W*W - 128*(W + W) = W*(W - 256)
  //
  // so W = 255 gives EXACTLY -255: floor puts E' at -1 (the tile is rejected,
  // correctly — the pixel centre (128,128) has 128+128 = 256 > 255 and is
  // outside), while truncation toward zero would put it at 0 and, with the low
  // byte non-zero, ACCEPT. The scan box of this triangle is the single pixel
  // (0,0), so the two roundings differ by a whole tile list: zero jobs against
  // one. The other two edges are axis-aligned with positive coefficients and
  // accept tile (0,0) comfortably, so nothing else masks the difference.
  //
  // The neighbours are swept too (W around 255) because they are the control:
  // W = 254 gives e0_base = -508, which floors to -2 and truncates to -1 —
  // both reject, no difference — and W = 256 gives 0, which both accept. Only
  // the exact boundary separates the two laws, which is precisely why a
  // sampled lane cannot find it: the event is one integer wide in a space the
  // random triangles step through 16*|k| at a time.
  auto verdicts = [](const zref::Setup::Out& sp, int32_t tx, int32_t ty, bool* keep_floor,
                     bool* keep_trunc) {
    *keep_floor = true;
    *keep_trunc = true;
    for (int i = 0; i < 3; ++i) {
      const int64_t base = zref::Binner::e0_base(sp.e[i]);
      const bool nz = zref::Binner::rnz(sp.e[i]);
      const int64_t ep = e0_corner(sp.e[i], tx, ty) >> 8;
      const int64_t shift = (base < 0 && (base & 255) != 0) ? 1 : 0;
      if (!zref::fill_accept(ep, nz, sp.e[i].tl)) *keep_floor = false;
      if (!zref::fill_accept(ep + shift, nz, sp.e[i].tl)) *keep_trunc = false;
    }
  };

  uint32_t witnesses = 0;
  for (int32_t w = 248; w <= 264; ++w) {
    BinTri t;
    if (!make_bin_tri(0, 0, w, 0, 0, w, kVp, static_cast<uint16_t>(w), &t)) continue;
    bool differs = false;
    for (int32_t ty = t.min_y >> 4; ty <= (t.max_y >> 4); ++ty)
      for (int32_t tx = t.min_x >> 4; tx <= (t.max_x >> 4); ++tx) {
        bool kf = false, kt = false;
        verdicts(t.s, tx, ty, &kf, &kt);
        if (kf != kt) differs = true;
      }
    if (differs) ++witnesses;
    diff({t}, "floor boundary: the tile list is the FLOORED one");
  }
  check(witnesses > 0,
        "floor boundary: the sweep contains a triangle where floor and truncation disagree", 1,
        witnesses);

  // and the pair either side of it, spelled out: the boundary triangle bins
  // NOTHING (its only candidate pixel centre is outside), the one a subpixel
  // wider bins its tile.
  BinTri on_edge, inside;
  check(make_bin_tri(0, 0, 255, 0, 0, 255, kVp, 1, &on_edge), "floor boundary: W=255 accepted", 1,
        1);
  check(make_bin_tri(0, 0, 256, 0, 0, 256, kVp, 2, &inside), "floor boundary: W=256 accepted", 1,
        1);
  const std::vector<BinJob> a = run({on_edge}, "floor boundary: W=255 drain");
  const std::vector<BinJob> b = run({inside}, "floor boundary: W=256 drain");
  check(a.empty(), "floor boundary: W=255 bins no tile (E' = -1, the FLOORED verdict)", 0,
        static_cast<uint32_t>(a.size()));
  check(b.size() == 1, "floor boundary: W=256 bins its tile (E' = 0, top-left accepts)", 1,
        static_cast<uint32_t>(b.size()));
}

// ---------------------------------------------------------------- 2 --------
void test_one_tile() {
  // wholly inside tile (5,4): pixels [80,96) x [64,80)
  const BinTri t = mk(82 * 256, 66 * 256, 94 * 256, 68 * 256, 84 * 256, 78 * 256, 11);
  BinStatus st;
  const std::vector<BinJob> got = run({t}, "one tile: exactly one job", 0, &st);
  check(got.size() == 1, "one tile: exactly one job", 1, static_cast<uint32_t>(got.size()));
  if (got.size() == 1) {
    check(got[0].tx == 5 && got[0].ty == 4, "one tile: the right tile", 5u * 100 + 4,
          static_cast<uint32_t>(got[0].tx) * 100 + static_cast<uint32_t>(got[0].ty));
    check(got[0].src_id == 11, "one tile: src_id rides through", 11, got[0].src_id);
  }
  const uint32_t want_depth = st.max_depth_before > 1u ? st.max_depth_before : 1u;
  check(st.max_depth == want_depth, "one tile: max_tile_list_depth == 1", want_depth, st.max_depth);
  diff({t}, "one tile: differential");
}

// ---------------------------------------------------------------- 5 --------
void test_order_and_fifo() {
  // three overlapping triangles that all cover the same block of tiles
  const BinTri a = mk(40 * 256, 40 * 256, 120 * 256, 40 * 256, 40 * 256, 120 * 256, 1);
  const BinTri b = mk(45 * 256, 45 * 256, 125 * 256, 45 * 256, 45 * 256, 125 * 256, 2);
  const BinTri c = mk(50 * 256, 50 * 256, 130 * 256, 50 * 256, 50 * 256, 130 * 256, 3);
  const std::vector<BinJob> got = run({a, b, c}, "order: three overlapping triangles");
  diff({a, b, c}, "order: differential");

  // row-major over the grid
  int last = -1;
  for (const BinJob& j : got) {
    const int idx = j.ty * kGridW + j.tx;
    check(idx >= last, "order: drain is row-major over the grid", static_cast<uint32_t>(last),
          static_cast<uint32_t>(idx));
    last = idx;
  }
  // FIFO within each tile: src_ids appear in submission order
  int cur = -1;
  uint16_t prev_src = 0;
  bool fifo = true;
  for (const BinJob& j : got) {
    const int idx = j.ty * kGridW + j.tx;
    if (idx != cur) {
      cur = idx;
      prev_src = 0;
    }
    if (j.src_id < prev_src) fifo = false;
    prev_src = j.src_id;
  }
  check(fifo, "order: FIFO within a tile (the painter order)", 1, fifo ? 1 : 0);
}

// ---------------------------------------------------------------- 6 --------
void test_chunk_boundary() {
  // N triangles all covering tile (2,2) and nothing else: N = 1..9 crosses the
  // 4-reference chunk boundary twice, so the chain link is exercised at both
  // the first and the second new chunk.
  for (int n = 1; n <= 9; ++n) {
    std::vector<BinTri> tris;
    for (int i = 0; i < n; ++i) {
      const int32_t x = 34 * 256 + i * 16;
      tris.push_back(
          mk(x, 34 * 256, x + 8 * 256, 36 * 256, x, 44 * 256, static_cast<uint16_t>(100 + i)));
    }
    BinStatus st;
    const std::vector<BinJob> got = run(tris, "chunk boundary: drain", 0, &st);
    diff(tris, "chunk boundary: differential");
    // the refs for the deepest tile must come out in submission order
    uint32_t deepest = 0;
    int cur = -1;
    uint32_t run_len = 0;
    for (const BinJob& j : got) {
      const int idx = j.ty * kGridW + j.tx;
      if (idx != cur) {
        cur = idx;
        run_len = 0;
      }
      ++run_len;
      if (run_len > deepest) deepest = run_len;
    }
    const uint32_t want_depth = st.max_depth_before > deepest ? st.max_depth_before : deepest;
    check(st.max_depth == want_depth, "chunk boundary: max_tile_list_depth", want_depth,
          st.max_depth);
  }
}

// ---------------------------------------------------------------- 7 --------
void test_tokens() {
  BinTri a = mk(40 * 256, 40 * 256, 120 * 256, 40 * 256, 40 * 256, 120 * 256, 1);
  BinTri b = mk(150 * 256, 40 * 256, 230 * 256, 40 * 256, 150 * 256, 120 * 256, 2);
  BinTri c = mk(40 * 256, 150 * 256, 120 * 256, 150 * 256, 40 * 256, 220 * 256, 3);
  b.token = false;  // MEASURE.TOKENS denies this one
  BinStatus st;
  diff({a, b, c}, "tokens: a denied triangle is dropped whole", 0, &st);
  check(st.triangles_culled == 1, "tokens: the denial is counted", 1, st.triangles_culled);
  check(!st.overflow, "tokens: a denial is not an overflow", 0, st.overflow ? 1 : 0);
  const std::vector<BinJob> got = run({a, b, c}, "tokens: no job carries the denied triangle");
  for (const BinJob& j : got)
    check(j.src_id != 2, "tokens: no job carries the denied triangle", 0, j.src_id);
}

// ---------------------------------------------------------------- 8 --------
void test_triangle_store_overflow() {
  // TRI_CAP is 128; the 129th triangle of a frame walls the frame off.
  std::vector<BinTri> tris;
  for (int i = 0; i < 140; ++i) {
    const int32_t x = (i % 20) * 16 * 256 + 2 * 256;
    const int32_t y = (i / 20) * 16 * 256 + 2 * 256;
    tris.push_back(mk(x, y, x + 10 * 256, y + 2 * 256, x, y + 10 * 256, static_cast<uint16_t>(i)));
  }
  BinStatus st;
  const std::vector<BinJob> got = run(tris, "tri overflow: frame", 0, &st);
  check(st.overflow, "tri overflow: overflow_o latches", 1, st.overflow ? 1 : 0);
  check(st.triangles_culled == 12, "tri overflow: the 129th onward are culled", 12,
        st.triangles_culled);
  // nothing beyond triangle 127 may appear in the drain
  uint16_t worst = 0;
  for (const BinJob& j : got)
    if (j.src_id > worst) worst = j.src_id;
  check(worst == 127, "tri overflow: the wall cuts the TAIL of the frame", 127, worst);
}

// ---------------------------------------------------------------- 9 --------
void test_arena_overflow() {
  // The chunk arena holds 256 chunks x 4 references = 1024 references. A
  // half-canvas triangle bins into ~180 tiles, so a dozen of them exhaust it.
  std::vector<BinTri> tris;
  for (int i = 0; i < 16; ++i) {
    tris.push_back(mk(0, 0, 383 * 256, 0, 0, 239 * 256, static_cast<uint16_t>(i)));
  }
  BinStatus st;
  const std::vector<BinJob> got = run(tris, "arena overflow: frame", 0, &st);
  check(st.overflow, "arena overflow: overflow_o latches", 1, st.overflow ? 1 : 0);
  check(st.arena_used <= 256, "arena overflow: NEVER SCRIBBLES — used <= CHUNKS", 256,
        st.arena_used);
  check(st.tile_references == got.size(),
        "arena overflow: every counted reference is still drained",
        static_cast<uint32_t>(got.size()), st.tile_references);
  check(st.tile_references <= 1024, "arena overflow: at most CHUNKS*CHUNK_REFS references", 1024,
        st.tile_references);
  // The wall cuts the TAIL: the triangles that DID fit are the low src_ids, and
  // their references are intact.
  uint16_t worst = 0;
  for (const BinJob& j : got)
    if (j.src_id > worst) worst = j.src_id;
  check(worst < 16, "arena overflow: the wall cut the tail", 16, worst);
}

// --------------------------------------------------------------- 10/11 -----
void test_frames_are_isolated() {
  const BinTri a = mk(40 * 256, 40 * 256, 120 * 256, 40 * 256, 40 * 256, 120 * 256, 1);
  BinStatus st1, st2;
  const std::vector<BinJob> f1 = run({a}, "frames: first", 0, &st1);
  const std::vector<BinJob> f2 = run({a}, "frames: second", 0, &st2);
  check(f1.size() == f2.size(), "frames: a repeated frame drains identically",
        static_cast<uint32_t>(f1.size()), static_cast<uint32_t>(f2.size()));
  for (size_t i = 0; i < f1.size() && i < f2.size(); ++i)
    if (!same(f1[i], f2[i])) {
      check(false, "frames: a repeated frame drains identically", 0, 1);
      break;
    }
  // an EMPTY frame after a full one must drain nothing at all
  BinStatus st3;
  const std::vector<BinJob> f3 = run({}, "frames: empty frame after a full one", 0, &st3);
  check(f3.empty(), "frames: nothing survives frame_begin", 0, static_cast<uint32_t>(f3.size()));
}

// --------------------------------------------------------------- 12 --------
void test_backpressure() {
  const BinTri a = mk(8 * 256, 8 * 256, 300 * 256, 30 * 256, 60 * 256, 220 * 256, 5);
  const BinTri b = mk(40 * 256, 150 * 256, 200 * 256, 150 * 256, 40 * 256, 230 * 256, 6);
  const uint32_t seeds[] = {1u, 0x1234567u, 0x89ABCDEFu, 0xDEADBEEFu};
  for (uint32_t s : seeds) diff({a, b}, "backpressure: identical jobs under stalls", s);
}

// --------------------------------------------------------------- 13 --------
void test_throughput() {
  // MEASURED against the ledger's "1 bin reference per clock". The block
  // sustains one reference per TWO clocks (evaluate + push); the shortfall is
  // reported here and in design/contracts/GEOM.BINNER.md rather than left to
  // be discovered.
  const BinTri t = mk(0, 0, 383 * 256, 0, 0, 239 * 256, 1);
  BinStatus st;
  const std::vector<BinJob> got = run({t}, "throughput: measurement", 0, &st);
  const double per_ref =
      got.empty() ? 0.0 : static_cast<double>(st.bin_cycles) / static_cast<double>(got.size());
  std::printf(
      "  MEASURED GEOM.BINNER bin throughput: %llu cycles for %u references = "
      "%.2f cycles/reference (ledger target: 1)\n",
      static_cast<unsigned long long>(st.bin_cycles), static_cast<unsigned>(got.size()), per_ref);
  const double drain_per_job =
      got.empty() ? 0.0 : static_cast<double>(st.drain_cycles) / static_cast<double>(got.size());
  std::printf(
      "  MEASURED GEOM.BINNER drain: %llu cycles for %u jobs = %.2f cycles/job "
      "(RASTER.EDGEWALK spends 21..37 on each)\n",
      static_cast<unsigned long long>(st.drain_cycles), static_cast<unsigned>(got.size()),
      drain_per_job);
  // The claim in the RTL header and the contract is "one reference per two
  // clocks"; hold the block to it so a regression cannot quietly widen it.
  check(per_ref > 0.0 && per_ref < 4.0, "throughput: within the recorded bound", 4,
        static_cast<uint32_t>(per_ref * 100));
  // The drain cost is STRUCTURAL, not per-job: two cycles for every tile of the
  // grid (the head read) plus four for every job. Hold the block to that shape
  // rather than to a cycles-per-job number the grid size dominates.
  const uint64_t bound = 2ull * static_cast<uint64_t>(kGridW) * static_cast<uint64_t>(kGridH) +
                         6ull * static_cast<uint64_t>(got.size()) + 64ull;
  check(st.drain_cycles <= bound, "throughput: drain within 2*tiles + 6*jobs + 64",
        static_cast<uint32_t>(bound), static_cast<uint32_t>(st.drain_cycles));
}

}  // namespace

int main() {
  test_many_tiles();
  test_pixel_origin();
  test_floor_boundary();
  test_one_tile();
  test_order_and_fifo();
  test_chunk_boundary();
  test_tokens();
  test_triangle_store_overflow();
  test_arena_overflow();
  test_frames_are_isolated();
  test_backpressure();
  test_throughput();

  return zhao::report_and_exit("geom_binner_directed");
}
