// geom_bin_pipe_directed.cpp — REAL TILE LISTS DRIVING THE REAL RASTERIZER.
//
// The DUT is fpga/rtl/geometry/zhao_geom_bin_pipe.sv: GEOM.BINNER wired
// straight into zhao_raster_tile_pipe, which is RASTER.EDGEWALK ->
// RASTER.EARLYZ -> RASTER.FRAGMENT -> RASTER.TILESTORE -> RASTER.RESOLVE. A
// setup triangle goes in at one end; the binner's chunked tile list decides
// which tiles get walked; RGB565 framebuffer words come out at the other.
// Nothing in this test hands the rasterizer a tile — the tile list does.
//
// That is what makes it worth having. Every earlier rasterizer lane was fed
// ONE triangle and ONE tile by a C++ driver, so a binning mistake could only
// ever show up as a wrong index in a differential. Here a dropped tile is a
// hole in a picture, and the picture is checked against the software raster.
//
// What is asserted:
//
//   1. tiles       — the set of tiles the chain actually rasterizes is exactly
//                    the set zref::Binner enumerates, in the same order
//   2. coverage    — every tile's `tile_cov_count_o` equals zref::EdgeWalk's
//                    covered-pixel count for that triangle and that tile
//   3. SOUNDNESS   — every tile of the grid with non-zero coverage was
//                    rasterized. This is the end-to-end statement of the
//                    binning law: no covered pixel is lost between the setup
//                    packet and the framebuffer.
//   4. the picture — the resolved RGB565 stream is diffed pixel for pixel
//                    against the same words zref::TileResolve produces from
//                    the same coverage, INCLUDING the known, escalated
//                    resolve.cpp defect (pure black resolves to 0x0020 in 8 of
//                    16 Bayer cells — RASTER.RESOLVE.md "the BLACK rail is not
//                    clean"). The oracle is the law; the defect rides through.
//   5. one tile    — a triangle inside a single tile produces exactly one
//                    rasterized tile
//   6. empty       — a frame with no triangles rasterizes nothing and still
//                    completes its drain
//
// RESTRICTION, recorded rather than hidden: zhao_raster_tile_pipe is one clear
// + one triangle + one resolve per job, so a tile that appears twice in the
// drain is cleared and resolved twice. The scenes below therefore give every
// tile at most one triangle. Lifting that would mean changing a finished
// block's interface; see the composition's own header.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_geom_bin_pipe.h"

#include "zhao_sim.hpp"
#include "zref/zref_edgewalk.hpp"
#include "zref/zref_geom.hpp"
#include "zref/zref_tileresolve.hpp"

using zhao::check;
using zref::Binner;
using zref::Clip;
using zref::Setup;

namespace {

const Clip::Viewport kVp{0, 0, 384, 240};
const int kGridW = 24;
const int kGridH = 15;

// The plain opaque recipe: state 0 is the phase-4 behaviour zhao_raster_tile_
// pipe's header pins — depth test off, depth written, blend REPLACE, no alpha
// test, stencil ALWAYS. The fill word then lands unaltered at every covered
// pixel, so the resolved picture is a pure coverage bitmap.
const uint64_t kFillWord = 0xFFFFFF00'00000000ull;  // white RGB, tag 0, depth 0
const uint64_t kClearWord = 0x00000000'00000000ull;

struct TileResult {
  int32_t tx = 0, ty = 0;
  uint32_t cov = 0;
  bool degenerate = false;
  std::vector<uint16_t> px;  // 256 resolved RGB565 words, tile raster order
};

class PipeDev {
 public:
  PipeDev() { reset(); }
  ~PipeDev() { top_.final(); }
  PipeDev(const PipeDev&) = delete;
  PipeDev& operator=(const PipeDev&) = delete;

  void reset() {
    top_.rst_n = 0;
    park();
    top_.eval();
    for (int i = 0; i < 2; ++i) tick();
    top_.rst_n = 1;
    top_.eval();
    for (int i = 0; i < 1200 && !top_.tri_ready_o; ++i) tick();
  }

  /** One whole frame; returns the tiles the chain actually rasterized. */
  std::vector<TileResult> frame(const std::vector<Setup::Out>& setups,
                                const std::vector<Clip::Out>& clips, std::string* err) {
    std::vector<TileResult> out;
    park();
    top_.frame_begin_i = 1;
    top_.eval();
    tick();
    top_.frame_begin_i = 0;
    top_.eval();
    for (int i = 0; i < 1200 && !top_.tri_ready_o; ++i) tick();

    for (size_t k = 0; k < setups.size(); ++k) {
      const Setup::Out& s = setups[k];
      const Clip::Out& c = clips[k];
      top_.tri_valid_i = 1;
      top_.tri_kx0_i = m23(s.e[0].kx);
      top_.tri_ky0_i = m23(s.e[0].ky);
      top_.tri_kc0_i = m48(s.e[0].kc);
      top_.tri_kx1_i = m23(s.e[1].kx);
      top_.tri_ky1_i = m23(s.e[1].ky);
      top_.tri_kc1_i = m48(s.e[1].kc);
      top_.tri_kx2_i = m23(s.e[2].kx);
      top_.tri_ky2_i = m23(s.e[2].ky);
      top_.tri_kc2_i = m48(s.e[2].kc);
      top_.tri_tl_i = static_cast<uint32_t>((s.e[0].tl ? 1 : 0) | (s.e[1].tl ? 2 : 0) |
                                            (s.e[2].tl ? 4 : 0));
      top_.tri_ax_i = m21(c.ax);
      top_.tri_ay_i = m21(c.ay);
      top_.tri_bx_i = m21(c.bx);
      top_.tri_by_i = m21(c.by);
      top_.tri_cx_i = m21(c.cx);
      top_.tri_cy_i = m21(c.cy);
      top_.tri_min_x_i = m12(c.min_x);
      top_.tri_max_x_i = m12(c.max_x);
      top_.tri_min_y_i = m12(c.min_y);
      top_.tri_max_y_i = m12(c.max_y);
      top_.tri_src_id_i = static_cast<uint32_t>(k);
      top_.eval();
      int guard = 0;
      while (!top_.tri_ready_o) {
        if (++guard > 4096) {
          *err = "tri_ready_o never asserted";
          top_.tri_valid_i = 0;
          return out;
        }
        tick();
        top_.eval();
      }
      tick();
      top_.tri_valid_i = 0;
      top_.eval();
    }
    for (int i = 0; i < 8192 && !top_.tri_ready_o; ++i) tick();

    // ---- frame_end, then drain the whole grid through the rasterizer -----
    top_.frame_end_i = 1;
    top_.eval();
    tick();
    top_.frame_end_i = 0;
    top_.eval();

    // The framebuffer stream is one tile at a time in raster order, closed by
    // `fb_last_o` (the 256th pixel), and `tile_done_o` follows once all 256
    // have been accepted (RASTER.RESOLVE / zhao_raster_tile_pipe). Keying the
    // tile boundary off `fb_addr_o == 0` / `fb_last_o` rather than off the
    // done pulse is what keeps a tile from being split across two records.
    TileResult cur;
    cur.px.assign(256, 0);
    size_t pending = static_cast<size_t>(-1);
    bool drained = false;
    int idle = 0;
    for (int guard = 0; guard < 4000000; ++guard) {
      top_.eval();
      if (top_.fb_valid_o) {
        const uint32_t addr = top_.fb_addr_o;
        if (addr == 0) {
          cur = TileResult();
          cur.px.assign(256, 0);
          cur.tx = s12(top_.fb_x_o) >> 4;
          cur.ty = s12(top_.fb_y_o) >> 4;
        }
        cur.px[addr] = static_cast<uint16_t>(top_.fb_rgb565_o);
        if (top_.fb_last_o) {
          out.push_back(cur);
          pending = out.size() - 1;
        }
        idle = 0;
      }
      if (top_.tile_done_o && pending != static_cast<size_t>(-1)) {
        out[pending].cov = top_.tile_cov_count_o;
        out[pending].degenerate = top_.tile_degenerate_o != 0;
        pending = static_cast<size_t>(-1);
      }
      if (top_.drain_done_o) drained = true;
      tick();
      if (drained) {
        ++idle;
        if (idle > 1024) break;
      }
    }
    if (!drained) *err = "drain never completed";
    return out;
  }

 private:
  static int32_t s12(uint32_t raw) {
    const uint32_t v = raw & 0xFFFu;
    return (v & 0x800u) ? static_cast<int32_t>(v | 0xFFFFF000u) : static_cast<int32_t>(v);
  }
  static uint32_t m21(int32_t v) { return static_cast<uint32_t>(v) & 0x1FFFFFu; }
  static uint32_t m23(int32_t v) { return static_cast<uint32_t>(v) & 0x7FFFFFu; }
  static uint32_t m12(int32_t v) { return static_cast<uint32_t>(v) & 0xFFFu; }
  static uint64_t m48(int64_t v) { return static_cast<uint64_t>(v) & 0xFFFFFFFFFFFFull; }

  void park() {
    top_.tri_valid_i = 0;
    top_.frame_begin_i = 0;
    top_.frame_end_i = 0;
    top_.tok_grant_i = 1;
    top_.fb_ready_i = 1;
    top_.grid_w_i = kGridW;
    top_.grid_h_i = kGridH;
    top_.job_fill_word_i = kFillWord;
    top_.job_clear_word_i = kClearWord;
    top_.job_state_i = 0;
    top_.job_src_a_i = 0xFF;
    top_.job_texel_rgb_i = 0xFFFFFF;
    top_.job_texel_a_i = 0xFF;
    top_.job_texel_idx_i = 0xFF;
  }
  void tick() {
    top_.clk = 0;
    top_.eval();
    top_.clk = 1;
    top_.eval();
    top_.clk = 0;
    top_.eval();
  }

  Vzhao_geom_bin_pipe top_;
};

PipeDev& dev() {
  static PipeDev d;
  return d;
}

bool build(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t cx, int32_t cy, Clip::Out* c,
           Setup::Out* s) {
  Clip::In in;
  in.ax = ax;
  in.ay = ay;
  in.bx = bx;
  in.by = by;
  in.cx = cx;
  in.cy = cy;
  *c = Clip::clip(in, kVp, Clip::kCullNone);
  if (c->verdict != Clip::kAccept) return false;
  *s = Setup::setup(c->ax, c->ay, c->bx, c->by, c->cx, c->cy, c->area2);
  return true;
}

// ---------------------------------------------------------------- 1..4 -----
void test_one_triangle_many_tiles() {
  Clip::Out c;
  Setup::Out s;
  check(build(8 * 256, 8 * 256, 300 * 256, 30 * 256, 60 * 256, 220 * 256, &c, &s),
        "fixture: accepted by GEOM.CLIP", 1, 1);

  std::string err;
  const std::vector<TileResult> got = dev().frame({s}, {c}, &err);
  check(err.empty(), "pipe: protocol", 0, err.empty() ? 0 : 1);
  if (!err.empty()) std::printf("    %s\n", err.c_str());

  // 1. the rasterized tile set IS the binner's tile list, in the same order
  const std::vector<Binner::Ref> want = Binner::bin(s, c.min_x, c.max_x, c.min_y, c.max_y);
  check(got.size() == want.size(), "pipe: one tile rasterized per bin reference",
        static_cast<uint32_t>(want.size()), static_cast<uint32_t>(got.size()));
  const size_t n = got.size() < want.size() ? got.size() : want.size();
  bool order_ok = true;
  for (size_t i = 0; i < n; ++i)
    if (got[i].tx != want[i].tx || got[i].ty != want[i].ty) {
      char buf[128];
      std::snprintf(buf, sizeof(buf), "pipe: tile %u — binner (%d,%d), rasterized (%d,%d)",
                    static_cast<unsigned>(i), want[i].tx, want[i].ty, got[i].tx, got[i].ty);
      check(false, buf, 0, 1);
      order_ok = false;
      break;
    }
  if (order_ok) check(true, "pipe: the rasterized tiles are the binner's, in order", 0, 0);

  // 2 + 4. coverage and the resolved picture, tile by tile
  const zref::EdgeWalk::Tri et{c.ax, c.ay, c.bx, c.by, c.cx, c.cy};
  uint32_t total = 0;
  bool cov_ok = true, px_ok = true;
  for (const TileResult& t : got) {
    const zref::EdgeWalk::Cov cov = zref::EdgeWalk::tile(et, t.tx * 16, t.ty * 16);
    total += cov.count;
    if (t.cov != cov.count) {
      char buf[128];
      std::snprintf(buf, sizeof(buf), "pipe: tile (%d,%d) coverage — oracle %u, chain %u", t.tx,
                    t.ty, cov.count, t.cov);
      check(false, buf, cov.count, t.cov);
      cov_ok = false;
      break;
    }
    // The resolved picture. The expected TILE STORE contents are the fill word
    // at every covered pixel and the clear word everywhere else (uncovered
    // pixels never become fragments, so their PRESENT bit stays 0 and they
    // resolve as the clear word — zhao_raster_tile_pipe's law 2). The expected
    // PIXELS are then whatever zref::TileResolve makes of that, called rather
    // than re-derived: the known, escalated resolve.cpp defect — pure black
    // resolving to 0x0020 in 8 of 16 Bayer cells (RASTER.RESOLVE.md, "the
    // BLACK rail is not clean") — rides through unchanged, because the oracle
    // is the law and this test pins the actual behaviour, not the desired one.
    uint64_t words[zref::TileResolve::kPixels];
    for (int row = 0; row < 16; ++row)
      for (int col = 0; col < 16; ++col)
        words[static_cast<size_t>(row) * 16 + col] =
            (((cov.row[row] >> col) & 1u) != 0u) ? kFillWord : kClearWord;
    const zref::TileResolve::Out want_px = zref::TileResolve::tile(words, t.tx * 16, t.ty * 16);
    for (int i = 0; i < zref::TileResolve::kPixels; ++i) {
      if (want_px.rgb565[i] != t.px[static_cast<size_t>(i)]) {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "pipe: tile (%d,%d) pixel %d — oracle %04X, resolved %04X", t.tx, t.ty, i,
                      want_px.rgb565[i], t.px[static_cast<size_t>(i)]);
        check(false, buf, want_px.rgb565[i], t.px[static_cast<size_t>(i)]);
        px_ok = false;
        break;
      }
    }
    if (!px_ok) break;
  }
  if (cov_ok) check(true, "pipe: every tile's coverage matches the software raster", 0, 0);
  if (px_ok) check(true, "pipe: the resolved picture matches, dither defect and all", 0, 0);

  // 3. SOUNDNESS end to end: no covered tile is missing from the picture
  bool seen[24][15] = {};
  for (const TileResult& t : got)
    if (t.tx >= 0 && t.tx < kGridW && t.ty >= 0 && t.ty < kGridH) seen[t.tx][t.ty] = true;
  uint32_t missed = 0, oracle_total = 0;
  for (int ty = 0; ty < kGridH; ++ty)
    for (int tx = 0; tx < kGridW; ++tx) {
      const uint32_t cnt = zref::EdgeWalk::tile(et, tx * 16, ty * 16).count;
      oracle_total += cnt;
      if (cnt != 0 && !seen[tx][ty]) ++missed;
    }
  check(missed == 0, "pipe: SOUND — every covered tile reached the rasterizer", 0, missed);
  check(total == oracle_total, "pipe: the whole triangle was rasterized, pixel for pixel",
        oracle_total, total);
  check(oracle_total > 8000, "pipe: the fixture really is a big triangle", 8000, oracle_total);
}

// ---------------------------------------------------------------- 5/6 ------
void test_one_tile_and_empty() {
  Clip::Out c;
  Setup::Out s;
  check(build(82 * 256, 66 * 256, 94 * 256, 68 * 256, 84 * 256, 78 * 256, &c, &s),
        "fixture: single-tile triangle accepted", 1, 1);
  std::string err;
  const std::vector<TileResult> got = dev().frame({s}, {c}, &err);
  check(err.empty(), "pipe: protocol (one tile)", 0, err.empty() ? 0 : 1);
  check(got.size() == 1, "pipe: a single-tile triangle rasterizes one tile", 1,
        static_cast<uint32_t>(got.size()));
  if (got.size() == 1) {
    check(got[0].tx == 5 && got[0].ty == 4, "pipe: and it is the right tile", 5u * 100 + 4,
          static_cast<uint32_t>(got[0].tx) * 100 + static_cast<uint32_t>(got[0].ty));
    const zref::EdgeWalk::Cov cov = zref::EdgeWalk::tile({c.ax, c.ay, c.bx, c.by, c.cx, c.cy},
                                                         got[0].tx * 16, got[0].ty * 16);
    check(got[0].cov == cov.count, "pipe: single-tile coverage", cov.count, got[0].cov);
  }

  std::string err2;
  const std::vector<TileResult> none = dev().frame({}, {}, &err2);
  check(err2.empty(), "pipe: protocol (empty frame)", 0, err2.empty() ? 0 : 1);
  check(none.empty(), "pipe: an empty frame rasterizes nothing", 0,
        static_cast<uint32_t>(none.size()));
}

}  // namespace

int main() {
  test_one_triangle_many_tiles();
  test_one_tile_and_empty();
  return zhao::report_and_exit("geom_bin_pipe_directed");
}
