// terrain_project_chain.cpp — REAL TERRAIN, PROJECTED BY REAL RTL, ALL THE WAY
// TO RESOLVED PIXELS.
//
// The chain is four Verilated models wired to each other with no adapter of any
// kind, one clock at a time:
//
//   zhao_terrain_project -> zhao_geom_clip -> zhao_geom_setup -> zhao_geom_bin_pipe
//                                                                 |
//                    GEOM.BINNER -> RASTER.EDGEWALK -> EARLYZ -> FRAGMENT ->
//                                   TILESTORE -> RESOLVE
//
// WHY THIS TEST EXISTS. Wiring GEOM.BINNER to the real rasterizer immediately
// exposed a silent 16× tile-index-versus-pixel error that no isolated test
// could see, because each block was self-consistently wrong about what the
// other meant. TERRAIN.PROJECT -> GEOM.CLIP is the same shape of risk and it is
// the seam this increment created: GEOM.CLIP's header says a screen vertex
// arriving at it "is already inside ±2048 px by construction" and names the
// projection as the enforcer. Until this file existed, nothing checked that the
// enforcer and the assumer were the same 21 bits.
//
// The mesh is real terrain: `zref::terrain::tessellate` over a composed 33×33
// lattice with relief — the ratified §4.3 law, which `terrain_tess_normals`
// already proves TERRAIN.TESS reproduces port-for-port. It is used here instead
// of the TESS block itself for one concrete reason, and it is a finding rather
// than a convenience: **TERRAIN.TESS carries ONE `src_id` per JOB, not per
// triangle**, and this test needs a per-triangle id to follow a triangle from
// the world vertex all the way to the framebuffer word that came from it. See
// the report.
//
// What is asserted:
//
//   1. projection   — every screen vertex the RTL chain produced is what
//                     `project_vertex` produces, checked at the CLIP boundary
//                     rather than at PROJECT's own port
//   2. verdicts     — the triangles GEOM.CLIP accepts are exactly the ones
//                     `zref::Clip` accepts from those same screen vertices
//   3. tiles        — the (tile, triangle) pairs the chain rasterizes are
//                     exactly the ones `zref::Binner` enumerates, tile-major
//   4. coverage     — every record's covered-pixel count equals
//                     `zref::EdgeWalk`'s for that triangle and that tile
//   5. SOUNDNESS    — every (tile, triangle) pair with non-zero coverage was
//                     rasterized. No covered terrain pixel is lost between the
//                     world vertex and the framebuffer.
//   6. the picture  — the resolved RGB565 words match `zref::TileResolve`
//   7. the near camera — a camera INSIDE the patch drops the primitives that
//                     touch a behind-the-eye vertex and still draws the rest.
//                     terrain.cpp's own note records that the old whole-patch
//                     abort made a near camera erase the island.
//
// RESTRICTION, recorded rather than hidden: `zhao_raster_tile_pipe` is one
// clear + one triangle + one resolve per job, so a tile referenced by two
// triangles is cleared and resolved twice and emits two records. That is not
// worked around here — it is what makes assertion 4 per (tile, triangle) rather
// than per tile, and `fb_src_id_o` is what makes the pairing observable.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_geom_bin_pipe.h"
#include "Vzhao_geom_clip.h"
#include "Vzhao_geom_setup.h"
#include "Vzhao_terrain_project.h"

#include "project_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_edgewalk.hpp"
#include "zref/zref_geom.hpp"
#include "zref/zref_terrain_tess.hpp"
#include "zref/zref_tileresolve.hpp"
#include "zrender/internal.hpp"

using project_test::mat_of;
using project_test::TriIn;
using zhao::check;
using zref::Binner;
using zref::Clip;
using zref::Setup;

namespace zt = zref::terrain;

namespace {

constexpr int32_t kOne = 1 << 16;
constexpr int kGridW = 16;  // 256 px / 16
constexpr int kGridH = 12;  // 192 px / 16

const zref::render::Viewport kVp{0, 0, 256, 192};
const Clip::Viewport kClipVp{0, 0, 256, 192};

// The plain opaque recipe, same as geom_bin_pipe_directed: state 0, fill word
// at every covered pixel, so the resolved picture is a pure coverage bitmap.
const uint64_t kFillWord = 0xFFFFFF00'00000000ull;
const uint64_t kClearWord = 0x00000000'00000000ull;

/** One rasterized (tile, triangle) record off the framebuffer stream. */
struct TileRecord {
  int32_t tx = 0, ty = 0;
  uint16_t src_id = 0;
  uint32_t cov = 0;
  std::vector<uint16_t> px;
};

int32_t s12(uint32_t raw) {
  const uint32_t v = raw & 0xFFFu;
  return (v & 0x800u) ? static_cast<int32_t>(v | 0xFFFFF000u) : static_cast<int32_t>(v);
}

/** The four models, wired to each other and to nothing else. */
class Chain {
 public:
  Chain() { reset(); }

  void reset() {
    proj_.rst_n = 0;
    clip_.rst_n = 0;
    setup_.rst_n = 0;
    pipe_.rst_n = 0;
    park();
    settle();
    for (int i = 0; i < 3; ++i) tick();
    proj_.rst_n = 1;
    clip_.rst_n = 1;
    setup_.rst_n = 1;
    pipe_.rst_n = 1;
    settle();
    for (int i = 0; i < 1200 && !pipe_.tri_ready_o; ++i) {
      tick();
      settle();
    }
  }

  void configure(const zref::mat4fx& m, const zref::render::Viewport& vp) {
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        write_cfg(static_cast<uint8_t>(r * 4 + c), static_cast<uint32_t>(m.m[r][c].raw));
      }
    }
    write_cfg(16, (vp.y0 << 16) | (vp.x0 & 0xFFFFu));
    write_cfg(17, (vp.h << 16) | (vp.w & 0xFFFFu));
  }

  /**
   * One whole frame: push every world triangle into TERRAIN.PROJECT and let the
   * chain carry it to the framebuffer. Returns the rasterized records in the
   * order the drain produced them.
   */
  std::vector<TileRecord> frame(const std::vector<TriIn>& world, std::string* err) {
    std::vector<TileRecord> out;

    pipe_.frame_begin_i = 1;
    settle();
    tick();
    pipe_.frame_begin_i = 0;
    settle();

    size_t pushed = 0;
    int quiet = 0;
    for (int guard = 0; guard < 400000; ++guard) {
      if (pushed < world.size()) {
        present(world[pushed]);
      } else {
        proj_.tri_valid_i = 0;
      }
      settle();
      const bool take = (pushed < world.size()) && proj_.tri_ready_o;
      tick();
      if (take) ++pushed;
      if (pushed == world.size()) {
        // The chain's own latency is bounded (37 + 3 + 3 + the binner's write),
        // so a long quiet run after the last accepted packet means every
        // triangle has reached the tile lists.
        if (proj_.idle_o && !clip_.out_valid_o && !setup_.out_valid_o) {
          if (++quiet > 512) break;
        } else {
          quiet = 0;
        }
      }
    }
    proj_.tri_valid_i = 0;
    settle();
    if (pushed != world.size()) {
      *err = "the chain never accepted every triangle";
      return out;
    }

    pipe_.frame_end_i = 1;
    settle();
    tick();
    pipe_.frame_end_i = 0;
    settle();

    TileRecord cur;
    cur.px.assign(256, 0);
    bool drained = false;
    int idle = 0;
    for (int guard = 0; guard < 8000000; ++guard) {
      settle();
      if (pipe_.fb_valid_o) {
        const uint32_t addr = pipe_.fb_addr_o;
        if (addr == 0) {
          cur = TileRecord();
          cur.px.assign(256, 0);
          cur.tx = s12(pipe_.fb_x_o) >> 4;
          cur.ty = s12(pipe_.fb_y_o) >> 4;
          cur.src_id = static_cast<uint16_t>(pipe_.fb_src_id_o);
        }
        cur.px[addr] = static_cast<uint16_t>(pipe_.fb_rgb565_o);
        if (pipe_.fb_last_o) {
          cur.cov = pipe_.tile_cov_count_o;
          out.push_back(cur);
        }
        idle = 0;
      }
      if (pipe_.drain_done_o) drained = true;
      tick();
      if (drained) {
        ++idle;
        if (idle > 1024) break;
      }
    }
    if (!drained) *err = "drain never completed";
    return out;
  }

  uint32_t clip_submitted() const { return clip_.triangles_submitted_o; }
  uint32_t clip_clipped() const { return clip_.triangles_clipped_o; }
  uint32_t clip_culled() const { return clip_.triangles_culled_o; }

 private:
  void write_cfg(uint8_t addr, uint32_t data) {
    proj_.cfg_we_i = 1;
    proj_.cfg_view_i = 0;
    proj_.cfg_addr_i = addr;
    proj_.cfg_data_i = data;
    settle();
    tick();
    proj_.cfg_we_i = 0;
    settle();
  }

  void present(const TriIn& t) {
    proj_.tri_valid_i = 1;
    proj_.ax_i = t.ax;
    proj_.ay_i = t.ay;
    proj_.az_i = t.az;
    proj_.bx_i = t.bx;
    proj_.by_i = t.by;
    proj_.bz_i = t.bz;
    proj_.cx_i = t.cx;
    proj_.cy_i = t.cy;
    proj_.cz_i = t.cz;
    proj_.src_id_i = t.src_id;
    proj_.view_i = 0;
    proj_.mat_a_i = t.mat_a;
    proj_.mat_b_i = t.mat_b;
    proj_.weight_i = t.weight;
  }

  void park() {
    proj_.cfg_we_i = 0;
    proj_.tri_valid_i = 0;
    proj_.view_i = 0;
    clip_.vp_x0_i = kClipVp.x0;
    clip_.vp_y0_i = kClipVp.y0;
    clip_.vp_w_i = kClipVp.w;
    clip_.vp_h_i = kClipVp.h;
    clip_.cull_mode_i = 0;  // NONE — GEOM.CLIP's default and reset value
    pipe_.frame_begin_i = 0;
    pipe_.frame_end_i = 0;
    pipe_.tok_grant_i = 1;
    pipe_.fb_ready_i = 1;
    pipe_.grid_w_i = kGridW;
    pipe_.grid_h_i = kGridH;
    pipe_.job_fill_word_i = kFillWord;
    pipe_.job_clear_word_i = kClearWord;
    pipe_.job_state_i = 0;
    pipe_.job_src_a_i = 0xFF;
    pipe_.job_texel_rgb_i = 0xFFFFFF;
    pipe_.job_texel_a_i = 0xFF;
    pipe_.job_texel_idx_i = 0xFF;
  }

  /**
   * THE WIRING. Ready flows backwards, then valid and data flow forwards, and
   * nothing in between reshapes a field: PROJECT's `out_*` ARE GEOM.CLIP's
   * `tri_*`, GEOM.CLIP's `out_*` ARE GEOM.SETUP's `tri_*`, GEOM.SETUP's
   * `out_*` ARE the bin pipe's `tri_*`. If that stops being true this function
   * stops compiling, which is the point of writing it this way.
   *
   * Two passes, because a ready signal in this chain depends only on the
   * downstream ready and on state — never on the upstream valid — so the
   * fixpoint is reached in one and the second only proves it.
   */
  void settle() {
    for (int it = 0; it < 2; ++it) {
      pipe_.eval();
      setup_.out_ready_i = pipe_.tri_ready_o;
      setup_.eval();
      clip_.out_ready_i = setup_.tri_ready_o;
      clip_.eval();
      proj_.out_ready_i = clip_.tri_ready_o;
      proj_.eval();

      clip_.tri_valid_i = proj_.out_valid_o;
      clip_.tri_ax_i = proj_.out_ax_o;
      clip_.tri_ay_i = proj_.out_ay_o;
      clip_.tri_bx_i = proj_.out_bx_o;
      clip_.tri_by_i = proj_.out_by_o;
      clip_.tri_cx_i = proj_.out_cx_o;
      clip_.tri_cy_i = proj_.out_cy_o;
      clip_.tri_behind_i = proj_.out_behind_o;
      clip_.tri_src_id_i = proj_.out_src_id_o;
      clip_.eval();

      setup_.tri_valid_i = clip_.out_valid_o;
      setup_.tri_ax_i = clip_.out_ax_o;
      setup_.tri_ay_i = clip_.out_ay_o;
      setup_.tri_bx_i = clip_.out_bx_o;
      setup_.tri_by_i = clip_.out_by_o;
      setup_.tri_cx_i = clip_.out_cx_o;
      setup_.tri_cy_i = clip_.out_cy_o;
      setup_.tri_area2_i = clip_.out_area2_o;
      setup_.tri_min_x_i = clip_.out_min_x_o;
      setup_.tri_max_x_i = clip_.out_max_x_o;
      setup_.tri_min_y_i = clip_.out_min_y_o;
      setup_.tri_max_y_i = clip_.out_max_y_o;
      setup_.tri_src_id_i = clip_.out_src_id_o;
      setup_.eval();

      pipe_.tri_valid_i = setup_.out_valid_o;
      pipe_.tri_kx0_i = setup_.out_kx0_o;
      pipe_.tri_ky0_i = setup_.out_ky0_o;
      pipe_.tri_kc0_i = setup_.out_kc0_o;
      pipe_.tri_kx1_i = setup_.out_kx1_o;
      pipe_.tri_ky1_i = setup_.out_ky1_o;
      pipe_.tri_kc1_i = setup_.out_kc1_o;
      pipe_.tri_kx2_i = setup_.out_kx2_o;
      pipe_.tri_ky2_i = setup_.out_ky2_o;
      pipe_.tri_kc2_i = setup_.out_kc2_o;
      pipe_.tri_tl_i = setup_.out_tl_o;
      pipe_.tri_ax_i = setup_.out_ax_o;
      pipe_.tri_ay_i = setup_.out_ay_o;
      pipe_.tri_bx_i = setup_.out_bx_o;
      pipe_.tri_by_i = setup_.out_by_o;
      pipe_.tri_cx_i = setup_.out_cx_o;
      pipe_.tri_cy_i = setup_.out_cy_o;
      pipe_.tri_min_x_i = setup_.out_min_x_o;
      pipe_.tri_max_x_i = setup_.out_max_x_o;
      pipe_.tri_min_y_i = setup_.out_min_y_o;
      pipe_.tri_max_y_i = setup_.out_max_y_o;
      pipe_.tri_src_id_i = setup_.out_src_id_o;
      pipe_.eval();
    }
  }

  void tick() {
    proj_.clk = 0;
    clip_.clk = 0;
    setup_.clk = 0;
    pipe_.clk = 0;
    proj_.eval();
    clip_.eval();
    setup_.eval();
    pipe_.eval();
    proj_.clk = 1;
    clip_.clk = 1;
    setup_.clk = 1;
    pipe_.clk = 1;
    proj_.eval();
    clip_.eval();
    setup_.eval();
    pipe_.eval();
    proj_.clk = 0;
    clip_.clk = 0;
    setup_.clk = 0;
    pipe_.clk = 0;
    proj_.eval();
    clip_.eval();
    setup_.eval();
    pipe_.eval();
  }

  Vzhao_terrain_project proj_;
  Vzhao_geom_clip clip_;
  Vzhao_geom_setup setup_;
  Vzhao_geom_bin_pipe pipe_;
};

/**
 * The software path, from the same world triangles: `project_vertex` per vertex,
 * then `zref::Clip`, then `zref::Setup`, then `zref::Binner`.
 */
struct SoftTri {
  bool accepted = false;
  Clip::Out c;
  Setup::Out s;
  int32_t sx[3] = {0, 0, 0};
  int32_t sy[3] = {0, 0, 0};
  uint8_t behind = 0;
};

SoftTri soften(const TriIn& t, const zref::mat4fx& m, const zref::render::Viewport& vp) {
  const int32_t wx[3] = {t.ax, t.bx, t.cx};
  const int32_t wy[3] = {t.ay, t.by, t.cy};
  const int32_t wz[3] = {t.az, t.bz, t.cz};
  SoftTri o;
  for (int k = 0; k < 3; ++k) {
    const zref::render::ProjOut p = zref::render::project_vertex(
        m, vp, zref::fx16{wx[k]}, zref::fx16{wy[k]}, zref::fx16{wz[k]}, nullptr);
    o.sx[k] = p.s.x;
    o.sy[k] = p.s.y;
    if (!p.in) o.behind = static_cast<uint8_t>(o.behind | (1u << k));
  }
  Clip::In in;
  in.ax = o.sx[0];
  in.ay = o.sy[0];
  in.bx = o.sx[1];
  in.by = o.sy[1];
  in.cx = o.sx[2];
  in.cy = o.sy[2];
  in.behind = o.behind;
  o.c = Clip::clip(in, kClipVp, Clip::kCullNone);
  o.accepted = (o.c.verdict == Clip::kAccept);
  if (o.accepted) o.s = Setup::setup(o.c.ax, o.c.ay, o.c.bx, o.c.by, o.c.cx, o.c.cy, o.c.area2);
  return o;
}

/** A real composed lattice with relief, and one subpatch tessellated from it. */
std::vector<TriIn> terrain_mesh(int level, uint16_t first_src) {
  zt::ComposedLattice lat;
  lat.w = lat.h = 33;
  lat.dual = false;
  lat.wx.resize(33);
  lat.wz.resize(33);
  for (int i = 0; i < 33; ++i) {
    lat.wx[static_cast<size_t>(i)] = (i * 2) << 16;  // 2 m pitch
    lat.wz[static_cast<size_t>(i)] = (i * 2) << 16;
  }
  lat.top.assign(33 * 33, 0);
  uint32_t s = 0x51D0'1234u;
  const auto next = [&s]() {
    s = s * 1103515245u + 12345u;
    return s >> 16;
  };
  for (size_t k = 0; k < lat.top.size(); ++k) {
    // Relief around 10 m with a few metres of variation, on the height16 grid
    // plus sub-step fx16 detail (terrain_rules §3.4: live ground is not on the
    // authored 1/256 m step).
    const int32_t base = 2560 + static_cast<int32_t>(next() % 1024) - 512;
    lat.top[k] = (base << 8) + static_cast<int32_t>(next() % 511) - 255;
  }

  zt::SubpatchJob job;
  job.ox = 0;
  job.oz = 0;
  job.level = level;
  const zt::TessResult r = zt::tessellate(lat, job, nullptr);

  std::vector<TriIn> out;
  out.reserve(r.tris.size());
  uint16_t id = first_src;
  for (const zt::MeshTri& t : r.tris) {
    TriIn w;
    w.ax = t.ax;
    w.ay = t.ay;
    w.az = t.az;
    w.bx = t.bx;
    w.by = t.by;
    w.bz = t.bz;
    w.cx = t.cx;
    w.cy = t.cy;
    w.cz = t.cz;
    w.src_id = id++;
    out.push_back(w);
  }
  return out;
}

/**
 * The camera. `eye_z` is where the eye sits on the world Z axis, so pushing it
 * forward walks it into the patch and the near plane starts rejecting corners.
 * The Y row is oblique so the heightfield is seen from above and its triangles
 * have area, not so that they are hand-tuned to look nice.
 */
zref::mat4fx camera(int32_t f, int32_t eye_z) {
  // clip.x = f·(x − 8 m), clip.y = f·y + (f/2)·z − f·12 m, clip.w = z − eye_z.
  // The Y row mixes z so the heightfield is seen from above and its triangles
  // have area; a level camera would make every cell a sliver.
  const int32_t m[16] = {f, 0, 0, f * -8, 0, f, f / 2, f * -12, 0, 0, kOne, 0, 0, 0, kOne, -eye_z};
  return mat_of(m);
}

/** Run one scene end to end and assert every one of the seven properties. */
void run_scene(Chain& ch, const zref::mat4fx& m, const std::vector<TriIn>& world, const char* what,
               bool expect_behind, size_t min_refs, uint32_t min_px) {
  ch.configure(m, kVp);

  std::vector<SoftTri> soft;
  soft.reserve(world.size());
  uint32_t behind_tris = 0;
  uint32_t accepted = 0;
  for (const TriIn& t : world) {
    soft.push_back(soften(t, m, kVp));
    if (soft.back().behind != 0) ++behind_tris;
    if (soft.back().accepted) ++accepted;
  }

  std::string err;
  const uint32_t sub0 = ch.clip_submitted();
  const std::vector<TileRecord> got = ch.frame(world, &err);
  check(err.empty(), what, 0, err.empty() ? 0u : 1u);
  if (!err.empty()) {
    std::printf("    %s: %s\n", what, err.c_str());
    return;
  }

  // ---- 2. the verdicts: GEOM.CLIP saw and judged exactly what it should ----
  check(ch.clip_submitted() - sub0 == world.size(), "GEOM.CLIP was offered every triangle",
        world.size(), ch.clip_submitted() - sub0);

  // ---- 3. ONE RECORD PER TILE, tile-major --------------------------------
  //
  // This used to expect one record per (tile, triangle) reference, and that was
  // right until ruling 2 of reports/RENDERER_ARCHITECTURE.md. The tile pipe now
  // clears the bank on a tile's FIRST reference and resolves on its LAST, so a
  // tile several triangles share is rendered ONCE with all of them in it --
  // which is the whole point: under the old shape the second triangle's clear
  // erased the first.
  //
  // So a tile produces one record, carrying the LAST referencing triangle's
  // source id (the resolve fires on that job) and the coverage of the TILE,
  // summed over its triangles rather than taken from whichever ran last.
  //
  // THIS TEST WAS MISSED WHEN THAT RULING LANDED. render_pipe_directed and
  // render_fb_directed were updated in the same commit; this one was not, and
  // nothing noticed because the scoped gates run during that work matched
  // `raster_|render_|geom_` and this file is `terrain_`.
  struct Ref {
    int32_t tx, ty;
    uint16_t src;
  };
  std::vector<std::vector<Binner::Ref>> refs(soft.size());
  for (size_t i = 0; i < soft.size(); ++i) {
    if (!soft[i].accepted) continue;
    refs[i] =
        Binner::bin(soft[i].s, soft[i].c.min_x, soft[i].c.max_x, soft[i].c.min_y, soft[i].c.max_y);
  }
  // Per tile, the triangles that reference it, in the order the binner drains
  // them -- tile list head to tail, which is submission order.
  std::vector<Ref> want;
  std::vector<std::vector<size_t>> tile_tris;
  long pair_refs = 0;
  for (int ty = 0; ty < kGridH; ++ty) {
    for (int tx = 0; tx < kGridW; ++tx) {
      std::vector<size_t> mine;
      for (size_t i = 0; i < soft.size(); ++i)
        for (const Binner::Ref& r : refs[i])
          if (r.tx == tx && r.ty == ty) mine.push_back(i);
      if (mine.empty()) continue;
      pair_refs += static_cast<long>(mine.size());
      want.push_back(Ref{tx, ty, world[mine.back()].src_id});
      tile_tris.push_back(mine);
    }
  }
  check(got.size() == want.size(), "one rasterized record per TILE, not per triangle",
        want.size(), got.size());
  // The distinction only means something if some tile really is shared.
  check(pair_refs > static_cast<long>(want.size()),
        "and some tile really is referenced by more than one triangle", 1,
        (pair_refs > static_cast<long>(want.size())) ? 1 : 0);
  // `min_refs` was calibrated when a record WAS a (tile, triangle) pair, so it
  // is checked against the pair count -- which the change above did not alter --
  // rather than against the smaller per-tile record count. Comparing the new
  // number to the old threshold would have quietly weakened the floor.
  check(pair_refs >= static_cast<long>(min_refs), "the scene really does reference many tiles",
        min_refs, static_cast<uint32_t>(pair_refs));

  bool tiles_ok = true;
  const size_t n = got.size() < want.size() ? got.size() : want.size();
  for (size_t i = 0; i < n; ++i) {
    if (got[i].tx != want[i].tx || got[i].ty != want[i].ty || got[i].src_id != want[i].src) {
      char buf[192];
      std::snprintf(buf, sizeof(buf), "%s: record %u — binner (%d,%d)#%u, rasterized (%d,%d)#%u",
                    what, static_cast<unsigned>(i), want[i].tx, want[i].ty, want[i].src, got[i].tx,
                    got[i].ty, got[i].src_id);
      check(false, buf, 0, 1);
      tiles_ok = false;
      break;
    }
  }
  if (tiles_ok)
    check(true, "the rasterized (tile, triangle) pairs are the binner's, in order", 0, 0);

  // ---- 1 + 4 + 6. coverage and the picture, record by record -------------
  bool cov_ok = true;
  bool px_ok = true;
  uint32_t total = 0;
  for (size_t rec = 0; rec < got.size() && rec < tile_tris.size(); ++rec) {
    const TileRecord& t = got[rec];
    // The tile's coverage is the SUM over its triangles -- the same way the
    // pipe accumulates it -- and its PICTURE is the union of their masks, since
    // every covered pixel is written the same fill colour.
    uint32_t summed = 0;
    zref::EdgeWalk::Cov cov{};
    for (size_t i : tile_tris[rec]) {
      const Clip::Out& ci = soft[i].c;
      const zref::EdgeWalk::Tri eti{ci.ax, ci.ay, ci.bx, ci.by, ci.cx, ci.cy};
      const zref::EdgeWalk::Cov one = zref::EdgeWalk::tile(eti, t.tx * 16, t.ty * 16);
      summed += one.count;
      for (int row = 0; row < 16; ++row) cov.row[row] |= one.row[row];
    }
    cov.count = summed;
    total += summed;
    if (t.cov != cov.count) {
      char buf[192];
      std::snprintf(buf, sizeof(buf), "%s: tile (%d,%d)#%u coverage — oracle %u, chain %u", what,
                    t.tx, t.ty, t.src_id, cov.count, t.cov);
      check(false, buf, cov.count, t.cov);
      cov_ok = false;
      break;
    }
    uint64_t words[zref::TileResolve::kPixels];
    for (int row = 0; row < 16; ++row) {
      for (int col = 0; col < 16; ++col) {
        words[static_cast<size_t>(row) * 16 + col] =
            (((cov.row[row] >> col) & 1u) != 0u) ? kFillWord : kClearWord;
      }
    }
    const zref::TileResolve::Out wpx = zref::TileResolve::tile(words, t.tx * 16, t.ty * 16);
    for (int p = 0; p < zref::TileResolve::kPixels; ++p) {
      if (wpx.rgb565[p] != t.px[static_cast<size_t>(p)]) {
        char buf[192];
        std::snprintf(buf, sizeof(buf), "%s: tile (%d,%d)#%u pixel %d — oracle %04X, got %04X",
                      what, t.tx, t.ty, t.src_id, p, wpx.rgb565[p], t.px[static_cast<size_t>(p)]);
        check(false, buf, wpx.rgb565[p], t.px[static_cast<size_t>(p)]);
        px_ok = false;
        break;
      }
    }
    if (!px_ok) break;
  }
  if (cov_ok) check(true, "every record's coverage matches the software raster", 0, 0);
  if (px_ok) check(true, "the resolved terrain picture matches, dither defect and all", 0, 0);

  // ---- 5. SOUNDNESS: no covered (tile, triangle) pair is missing ----------
  uint32_t missed = 0;
  uint32_t oracle_total = 0;
  for (size_t i = 0; i < soft.size(); ++i) {
    if (!soft[i].accepted) continue;
    const Clip::Out& c = soft[i].c;
    const zref::EdgeWalk::Tri et{c.ax, c.ay, c.bx, c.by, c.cx, c.cy};
    for (int ty = 0; ty < kGridH; ++ty) {
      for (int tx = 0; tx < kGridW; ++tx) {
        const uint32_t cnt = zref::EdgeWalk::tile(et, tx * 16, ty * 16).count;
        if (cnt == 0) continue;
        oracle_total += cnt;
        // A covered (tile, triangle) pair must reach the rasterizer, but it
        // now arrives INSIDE its tile's single record rather than as a record
        // of its own -- so the tile is what has to be present.
        bool seen = false;
        for (const TileRecord& t : got) {
          if (t.tx == tx && t.ty == ty) {
            seen = true;
            break;
          }
        }
        if (!seen) ++missed;
      }
    }
  }
  check(missed == 0, "SOUND — every covered terrain (tile, triangle) reached the rasterizer", 0,
        missed);
  check(total == oracle_total, "the whole mesh was rasterized, pixel for pixel", oracle_total,
        total);
  check(oracle_total >= min_px, "the scene really does cover a lot of pixels", min_px,
        oracle_total);
  check(accepted > 0, "some terrain survived the clipper", 1, accepted);

  // ---- 7. the near camera ------------------------------------------------
  if (expect_behind) {
    check(behind_tris > 0, "the near camera really does put corners behind the eye", 1,
          behind_tris);
    check(accepted > 0 && accepted < soft.size(),
          "a near camera drops the primitives it must and STILL DRAWS THE REST", 1, accepted);
  } else {
    check(behind_tris == 0, "the ordinary camera has nothing behind the eye", 0, behind_tris);
  }

  std::printf(
      "[terrain_project_chain] %s: %u triangles, %u accepted, %u behind, %u records, "
      "%u covered pixels\n",
      what, static_cast<unsigned>(world.size()), accepted, behind_tris,
      static_cast<unsigned>(got.size()), oracle_total);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Chain ch;

  // Scene 1: a whole subpatch at stride 2 (level 1), seen from a camera that
  // clears the whole patch. 32 triangles of real §4.3 terrain.
  {
    const std::vector<TriIn> mesh = terrain_mesh(1, 0x1000);
    check(!mesh.empty(), "the tessellator produced a mesh", 1, mesh.size());
    run_scene(ch, camera(2 * kOne, -40 * kOne), mesh, "ordinary camera", false, 64, 2000);
  }

  // Scene 2: the SAME patch with the eye pushed into it, so part of the mesh is
  // behind the near plane. terrain.cpp: "the old whole-PATCH abort made a near
  // camera erase the island; now a cell whose corner vertices include one behind
  // the eye is dropped and the rest draws."
  {
    const std::vector<TriIn> mesh = terrain_mesh(1, 0x2000);
    run_scene(ch, camera(kOne / 2, 2 * kOne), mesh, "near camera", true, 8, 200);
  }

  return zhao::report_and_exit("terrain_project_chain");
}
