// terrain_pipe_differential.cpp -- composed terrain projection against the
// retained zhao_terrain_project for both views. The mesh comes from the shipped
// zref tessellator; additive w comes from project_vertex. Memory responses are
// registered by one clock, never answered combinationally.
// ENFORCED-BY: tests/CMakeLists.txt terrain_pipe_differential,
// terrain_pipe_differential_bitmap, terrain_pipe_release_mutant,
// terrain_pipe_rpp3_matw18.

// The matrix width this build elaborated, so the checks that are CLAIMS ABOUT
// THE WIDTH can name it instead of asserting a constant that used to be true.
#ifndef ZHAO_PIPE_MATW
#define ZHAO_PIPE_MATW 32
#endif
#define ZHAO_STR_(x) #x
#define ZHAO_STR(x) ZHAO_STR_(x)

#include <cstdint>
#include <cstdio>
#include <vector>
#include "verilated.h"
#include "Vzhao_terrain_pipe.h"
#include "Vzhao_terrain_project.h"
#include "project_dev.hpp"
#include "layere_fixture.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_tess.hpp"
#include "zrender/internal.hpp"

namespace zt = zref::terrain;
using project_test::Dev;
using project_test::mat_of;
using project_test::oracle;
using project_test::TriIn;
using project_test::TriOut;
using zhao::check;
namespace {
constexpr int32_t kOne = 1 << 16;
constexpr uint32_t kPoison = 0x5BADF00Du;
struct Spec {
  zt::SubpatchJob job;
  bool dual = true;
  uint8_t views = 1;
  uint16_t src = 0;
  bool sparse = false;
};
struct PipePacket {
  TriOut p;
  uint32_t w[3] = {0, 0, 0};
  bool refused = false, missed = false;
};
struct GeoIn {
  int32_t x = 0, y = 0, z = 0;
  uint8_t view = 0;
  uint16_t payload = 0;
};
struct GeoOut {
  int32_t x = 0, y = 0, d = 0;
  uint32_t w = 0;
  bool behind = false;
  uint8_t view = 0;
  uint16_t payload = 0;
};
struct RunResult {
  std::vector<PipePacket> terrain;
  std::vector<GeoOut> geometry;
  int accepted_with_pending_output = 0;
  int opens_with_stalled_output = 0;
  int held_output_errors = 0;
  int cycles = 0;
};

bool same_packet(const PipePacket& a, const PipePacket& b) {
  bool same = a.refused == b.refused && a.missed == b.missed && a.p.behind == b.p.behind &&
              a.p.src_id == b.p.src_id && a.p.view == b.p.view && a.p.mat_a == b.p.mat_a &&
              a.p.mat_b == b.p.mat_b && a.p.weight == b.p.weight;
  for (int c = 0; c < 3; ++c)
    same = same && a.p.x[c] == b.p.x[c] && a.p.y[c] == b.p.y[c] && a.p.d[c] == b.p.d[c] &&
           a.w[c] == b.w[c];
  return same;
}

class PipeDriver {
 public:
  explicit PipeDriver(Vzhao_terrain_pipe& d) : d_(d) {}
  void reset() {
    d_.clk = 0;
    d_.rst_n = 0;
    d_.cfg_we_i = 0;
    d_.cfg_view_i = 0;
    d_.cfg_addr_i = 0;
    d_.cfg_data_i = 0;
    d_.en_i = 1;
    d_.a_valid_i = 0;
    d_.a_vx_i = 0;
    d_.a_vy_i = 0;
    d_.a_vz_i = 0;
    d_.a_view_i = 0;
    d_.a_payload_i = 0;
    d_.job_valid_i = 0;
    d_.job_ox_i = 0;
    d_.job_oz_i = 0;
    d_.job_level_i = 0;
    d_.job_lvl_nz_i = 0;
    d_.job_lvl_pz_i = 0;
    d_.job_lvl_nx_i = 0;
    d_.job_lvl_px_i = 0;
    d_.job_morph_i = 0;
    d_.job_surface_i = 0;
    d_.job_dual_i = 0;
    d_.job_src_id_i = 0;
    d_.job_view_mask_i = 0;
    d_.sparse_fill_i = 0;
    d_.mat_a_i = 0;
    d_.mat_b_i = 0;
    d_.mat_w_i = 0;
    d_.mat_valid_i = 0;
    d_.lat_h_i = kPoison;
    d_.lat_wx_i = kPoison;
    d_.lat_wz_i = kPoison;
    d_.cs_substance_i = 3;
    d_.out_ready_i = 0;
    d_.eval();
    zhao::tick(d_);
    zhao::tick(d_);
    d_.rst_n = 1;
    d_.eval();
    zhao::tick(d_);
    lat_pend_ = false;
    cs_pend_ = false;
    mat_pend_ = false;
  }
  void configure(int view, const zref::mat4fx& m, const zref::render::Viewport& vp) {
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c)
        write_cfg(view, static_cast<uint8_t>(r * 4 + c), static_cast<uint32_t>(m.m[r][c].raw));
    write_cfg(view, 16, (vp.y0 << 16) | (vp.x0 & 0xFFFFu));
    write_cfg(view, 17, (vp.h << 16) | (vp.w & 0xFFFFu));
  }
  RunResult run(const zt::ComposedLattice& lat, const std::vector<Spec>& jobs,
                const std::vector<GeoIn>& geometry, int max_cycles = 200000) {
    RunResult r;
    size_t ji = 0, gi = 0;
    int idle_run = 0, hold = 0;
    bool output_held = false;
    PipePacket held_output;
    uint32_t prior_opened = d_.groups_opened_o;
    // THE OVERLAP IS CONSTRUCTED, NOT WAITED FOR.
    //
    // The check below asserts that a new job is accepted while an older output
    // is still pending -- the cross-job pipelining property. It used to depend
    // on the periodic `cycle%13` stall pattern being low on the exact cycle
    // `job_ready_o` rose, which is a COINCIDENCE and not stimulus. It was
    // already hanging by a thread: each mode reached the state ONCE per run.
    //
    // THE WRONG FIX, tried first and recorded because it is the tempting one:
    // hold `out_ready_i` LOW until `job_ready_o` rises, so an output is
    // guaranteed pending at the handoff. That BACKS PRESSURE UP THE PIPE -- the
    // replay output cannot drain, so the arena cannot be released, so the
    // tessellator cannot push its vertices and never reaches StIdle, and
    // `job_ready_o` is precisely what StIdle drives. The stimulus meant to
    // reach the state is what prevents it.
    //
    // Gating the JOB OFFER is half of it: withhold `job_valid_i` until an
    // output is actually pending. On its own that is still a coincidence hunt,
    // because the offered job may not be taken before the output drains -- and
    // measured on both trees it reached the state on one and not the other, in
    // the OPPOSITE direction to the periodic pattern. Two stimuli that each work
    // on one tree are two coincidences, not a gate.
    //
    // The other half is a SHORT output stall, armed only once the offer is out
    // and an output is pending, to stop that output draining before
    // `job_ready_o` rises. It must be short: the long version jams the pipe as
    // described above. Both are budgeted, so a pipe that genuinely cannot
    // overlap fails the check rather than hanging the driver.
    int offer_budget = 600;
    int arm_budget = 14;
    for (int cycle = 0; cycle < max_cycles; ++cycle) {
      // `out_valid_o` is a register, so its value is stable here, before eval.
      const bool pending = d_.out_valid_o != 0;
      const bool hold_offer = ji != 0 && ji < jobs.size() && !pending && offer_budget > 0;
      if (hold_offer) --offer_budget;
      const bool arm = ji != 0 && ji < jobs.size() && !hold_offer && pending && arm_budget > 0;
      if (arm) --arm_budget;
      serve_memory(lat);
      drive_job((ji < jobs.size() && !hold_offer) ? &jobs[ji] : nullptr);
      drive_geometry(gi < geometry.size() ? &geometry[gi] : nullptr);
      bool out_ready = hold == 0 && !arm && ((cycle % 13) >= 4);
      d_.out_ready_i = out_ready;
      d_.eval();
      const bool job_take = ji < jobs.size() && d_.job_valid_i && d_.job_ready_o;
      if (job_take && ji != 0 && d_.out_valid_o) {
        d_.out_ready_i = 0;
        out_ready = false;
        hold = 6;
        d_.eval();
        ++r.accepted_with_pending_output;
      }
      if (d_.groups_opened_o != prior_opened) {
        if (d_.out_valid_o && !out_ready) ++r.opens_with_stalled_output;
        prior_opened = d_.groups_opened_o;
      }
      if (output_held) {
        if (!d_.out_valid_o || !same_packet(sample_terrain(), held_output)) ++r.held_output_errors;
      }
      if (d_.out_valid_o && !out_ready) {
        if (!output_held) held_output = sample_terrain();
        output_held = true;
      } else
        output_held = false;
      const bool geo_take = gi < geometry.size() && d_.a_valid_i && d_.a_ready_o;
      const bool lreq = d_.lat_req_o != 0, creq = d_.cs_req_o != 0;
      const bool mreq = d_.mat_req_o != 0;
      const uint8_t nvi = d_.lat_vi_o, nvj = d_.lat_vj_o, nci = d_.cs_ci_o, ncj = d_.cs_cj_o;
      const uint8_t nmci = d_.mat_ci_o, nmcj = d_.mat_cj_o;
      const bool nsurf = d_.lat_surface_o != 0;
      if (d_.out_valid_o && out_ready) r.terrain.push_back(sample_terrain());
      if (d_.a_valid_o) {
        GeoOut o;
        o.x = project_test::sx21(d_.a_x_o);
        o.y = project_test::sx21(d_.a_y_o);
        o.d = static_cast<int32_t>(d_.a_d_o);
        o.w = d_.a_w_o;
        o.behind = d_.a_behind_o != 0;
        o.view = d_.a_view_o;
        o.payload = d_.a_payload_o;
        r.geometry.push_back(o);
      }
      zhao::tick(d_);
      if (job_take) ++ji;
      if (geo_take) ++gi;
      lat_pend_ = lreq;
      lat_vi_ = nvi;
      lat_vj_ = nvj;
      lat_surf_ = nsurf;
      cs_pend_ = creq;
      cs_ci_ = nci;
      cs_cj_ = ncj;
      mat_pend_ = mreq;
      mat_ci_ = nmci;
      mat_cj_ = nmcj;
      if (hold > 0) --hold;
      r.cycles = cycle + 1;
      if (ji == jobs.size() && gi == geometry.size() && d_.idle_o) {
        if (++idle_run >= 2) break;
      } else
        idle_run = 0;
    }
    d_.job_valid_i = 0;
    d_.a_valid_i = 0;
    d_.out_ready_i = 0;
    d_.eval();
    return r;
  }

 private:
  Vzhao_terrain_pipe& d_;
  bool lat_pend_ = false, lat_surf_ = false, cs_pend_ = false, mat_pend_ = false;
  uint8_t lat_vi_ = 0, lat_vj_ = 0, cs_ci_ = 0, cs_cj_ = 0, mat_ci_ = 0, mat_cj_ = 0;
  PipePacket sample_terrain() const {
    PipePacket o;
    o.p.x[0] = project_test::sx21(d_.out_ax_o);
    o.p.y[0] = project_test::sx21(d_.out_ay_o);
    o.p.x[1] = project_test::sx21(d_.out_bx_o);
    o.p.y[1] = project_test::sx21(d_.out_by_o);
    o.p.x[2] = project_test::sx21(d_.out_cx_o);
    o.p.y[2] = project_test::sx21(d_.out_cy_o);
    o.p.d[0] = static_cast<int32_t>(d_.out_ad_o);
    o.p.d[1] = static_cast<int32_t>(d_.out_bd_o);
    o.p.d[2] = static_cast<int32_t>(d_.out_cd_o);
    o.w[0] = d_.out_aw_o;
    o.w[1] = d_.out_bw_o;
    o.w[2] = d_.out_cw_o;
    o.p.behind = d_.out_behind_o;
    o.p.src_id = d_.out_src_id_o;
    o.p.view = d_.out_view_o;
    o.p.mat_a = d_.out_mat_a_o;
    o.p.mat_b = d_.out_mat_b_o;
    o.p.weight = d_.out_weight_o;
    o.refused = d_.out_refused_o != 0;
    o.missed = d_.out_missed_o != 0;
    return o;
  }
  void write_cfg(int view, uint8_t addr, uint32_t data) {
    d_.cfg_we_i = 1;
    d_.cfg_view_i = view;
    d_.cfg_addr_i = addr;
    d_.cfg_data_i = data;
    d_.eval();
    zhao::tick(d_);
    d_.cfg_we_i = 0;
    d_.eval();
  }
  void serve_memory(const zt::ComposedLattice& lat) {
    if (lat_pend_ && lat_vi_ < lat.w && lat_vj_ < lat.h) {
      const size_t k = static_cast<size_t>(lat_vj_) * static_cast<size_t>(lat.w) + lat_vi_;
      d_.lat_h_i = lat_surf_ ? lat.bottom[k] : lat.top[k];
      d_.lat_wx_i = lat.wx[lat_vi_];
      d_.lat_wz_i = lat.wz[lat_vj_];
    } else {
      d_.lat_h_i = kPoison;
      d_.lat_wx_i = kPoison;
      d_.lat_wz_i = kPoison;
    }
    d_.cs_substance_i =
        (cs_pend_ && cs_ci_ < lat.w - 1 && cs_cj_ < lat.h - 1) ? lat.substance(cs_ci_, cs_cj_) : 3;
    // RULING R13's layer-E plane, answered one cycle after the request exactly
    // as the compose cache does. Poison on the data while `mat_valid_i` is
    // low, so a block that passed the wires through instead of emitting its
    // declared {0,0,0} is caught rather than looking correct.
    if (mat_pend_) {
      d_.mat_a_i = tess_test::mat_a_at(mat_ci_, mat_cj_);
      d_.mat_b_i = tess_test::mat_b_at(mat_ci_, mat_cj_);
      d_.mat_w_i = tess_test::weight_at(mat_ci_, mat_cj_);
      d_.mat_valid_i = 1;
    } else {
      d_.mat_a_i = 0xA5;
      d_.mat_b_i = 0xA5;
      d_.mat_w_i = 0xA5;
      d_.mat_valid_i = 0;
    }
  }
  void drive_job(const Spec* s) {
    d_.job_valid_i = s != nullptr;
    if (!s) return;
    d_.job_ox_i = s->job.ox;
    d_.job_oz_i = s->job.oz;
    d_.job_level_i = s->job.level;
    d_.job_lvl_nz_i = s->job.nlevel[zt::kSideNegZ];
    d_.job_lvl_pz_i = s->job.nlevel[zt::kSidePosZ];
    d_.job_lvl_nx_i = s->job.nlevel[zt::kSideNegX];
    d_.job_lvl_px_i = s->job.nlevel[zt::kSidePosX];
    d_.job_morph_i = static_cast<uint32_t>(s->job.morph);
    d_.job_surface_i = s->job.surface == zt::Surface::kUnderside;
    d_.job_dual_i = s->dual;
    d_.job_src_id_i = s->src;
    d_.job_view_mask_i = s->views;
    d_.sparse_fill_i = s->sparse;
  }
  void drive_geometry(const GeoIn* g) {
    d_.a_valid_i = g != nullptr;
    if (!g) return;
    d_.a_vx_i = g->x;
    d_.a_vy_i = g->y;
    d_.a_vz_i = g->z;
    d_.a_view_i = g->view;
    d_.a_payload_i = g->payload;
  }
};

// The lattice column (or row) the per-axis MINIMUM of a triangle's three world
// corners sits on, clamped to the cell grid. `w` is monotone with a constant
// step, so this is an exact inverse of the placement and not a search: it maps
// the oracle's world triangle back to the cell TERRAIN.TESS read its material
// at. Clamped to 0..31 because vertex 32 is a lattice line with no cell, which
// only a degenerate triangle could minimise onto.
int cell_index(const std::vector<int32_t>& w, int32_t a, int32_t b, int32_t c) {
  int32_t m = a;
  if (b < m) m = b;
  if (c < m) m = c;
  const int32_t step = w[1] - w[0];
  int idx = static_cast<int>((static_cast<int64_t>(m) - w[0]) / step);
  if (idx < 0) idx = 0;
  if (idx > static_cast<int>(w.size()) - 2) idx = static_cast<int>(w.size()) - 2;
  return idx;
}

zt::ComposedLattice make_lattice() {
  zt::ComposedLattice lat;
  lat.w = lat.h = 33;
  lat.dual = true;
  lat.wx.resize(33);
  lat.wz.resize(33);
  lat.top.resize(33 * 33);
  lat.bottom.resize(33 * 33);
  lat.cell_state.assign(32 * 32, zt::kSolid);
  for (int i = 0; i < 33; ++i) {
    lat.wx[static_cast<size_t>(i)] = -4 * kOne + i * (kOne / 4);
    lat.wz[static_cast<size_t>(i)] = -1 * kOne + i * (kOne / 4);
  }
  uint32_t r = 0xC001D00Du;
  for (int j = 0; j < 33; ++j)
    for (int i = 0; i < 33; ++i) {
      r = r * 1664525u + 1013904223u;
      const int32_t fine = static_cast<int32_t>((r >> 16) & 0x3FFu) - 512;
      const size_t k = static_cast<size_t>(j) * 33u + static_cast<size_t>(i);
      lat.top[k] = ((i * 31 - j * 17) << 8) + fine;
      lat.bottom[k] = lat.top[k] - (kOne / 2) - static_cast<int32_t>(r & 0x1FFu);
    }
  lat.cell_state[3u * 32u + 3u] = zt::kVoidAuthored;
  return lat;
}
zt::SubpatchJob job(int ox, int oz, int level, int32_t morph,
                    zt::Surface surface = zt::Surface::kTop) {
  zt::SubpatchJob j;
  j.ox = ox;
  j.oz = oz;
  j.level = level;
  for (int s = 0; s < 4; ++s) j.nlevel[s] = level;
  j.morph = morph;
  j.surface = surface;
  return j;
}
std::vector<Spec> make_specs(bool sparse) {
  std::vector<Spec> v;
  auto add = [&](zt::SubpatchJob j, uint8_t views, uint16_t src, bool dual = true) {
    Spec s;
    s.job = j;
    s.dual = dual;
    s.views = views;
    s.src = src;
    s.sparse = sparse;
    v.push_back(s);
  };
  add(job(0, 0, 0, 0x4000), 3, 0x5101);  // level-0 void skip, both views
  auto stitched = job(8, 0, 0, 0x8000);
  stitched.nlevel[zt::kSidePosX] = 2;
  add(stitched, 1, 0x5102);               // stitched fan, view 0
  add(job(16, 8, 1, 0xFFFF), 2, 0x5103);  // coarse, view 1
  auto underside = job(8, 16, 1, 1, zt::Surface::kUnderside);
  underside.nlevel[zt::kSideNegZ] = 3;
  underside.nlevel[zt::kSidePosX] = 2;
  add(underside, 3, 0x5104);                // underside winding + stitch, both views
  add(job(16, 16, 2, 0x10001), 3, 0x5105);  // morph clamp, both views
  add(job(24, 24, 3, 0x2000, zt::Surface::kUnderside), 1, 0x5106);
  auto reject = job(0, 0, 0, 0x6000);
  reject.nlevel[zt::kSideNegZ] = 1;
  add(reject, 3, 0x51E0);  // stitched + local void => rejected
  add(job(8, 8, 0, 0, zt::Surface::kUnderside), 2, 0x51E1, false);  // legacy empty
  add(job(8, 8, 1, 0x3000), 0, 0x51E2);                             // explicit no-view
  return v;
}
std::vector<GeoIn> make_geometry() {
  std::vector<GeoIn> v;
  for (int i = 0; i < 200; ++i) {
    GeoIn g;
    g.x = ((i % 17) - 8) * (kOne / 4) + (i & 7);
    g.y = ((i % 11) - 5) * (kOne / 8) - (i & 3);
    g.z = (i % 19 == 0) ? -kOne : ((i % 13) + 1) * (kOne / 4);
    g.view = static_cast<uint8_t>(i & 1);
    g.payload = static_cast<uint16_t>(0xA000u + i);
    v.push_back(g);
  }
  return v;
}
struct Expected {
  std::vector<TriIn> inputs;
  uint32_t groups = 0, fills = 0, drops = 0, refs = 0, vertices = 0, tess_refs = 0;
  uint32_t rejected = 0, empty = 0, no_view = 0, clamped = 0;
};
Expected make_expected(const zt::ComposedLattice& dual_lat, const std::vector<Spec>& specs) {
  Expected e;
  for (const Spec& s : specs) {
    if (s.views == 0) {
      ++e.no_view;
      continue;
    }
    const uint32_t nv = ((s.views & 1u) ? 1u : 0u) + ((s.views & 2u) ? 1u : 0u);
    e.groups += nv;
    zt::ComposedLattice lat = dual_lat;
    lat.dual = s.dual;
    const zt::TessResult tr = zt::tessellate(lat, s.job, nullptr);
    if (tr.verdict != zt::TessVerdict::kOk) {
      ++e.rejected;
      continue;
    }
    if (tr.tris.empty() && !s.dual && s.job.surface == zt::Surface::kUnderside) {
      ++e.empty;
      continue;
    }
    e.vertices += 81;
    e.tess_refs += static_cast<uint32_t>(tr.tris.size());
    const uint32_t side = static_cast<uint32_t>(8 / (1 << s.job.level) + 1), useful = side * side;
    e.fills += (s.sparse ? useful : 81u) * nv;
    if (s.sparse) e.drops += 81u - useful;
    e.refs += static_cast<uint32_t>(tr.tris.size()) * nv;
    if (s.job.morph > 65536) e.clamped += 2;
    for (const zt::MeshTri& t : tr.tris)
      for (int view = 0; view < 2; ++view) {
        if (((s.views >> view) & 1u) == 0) continue;
        TriIn in;
        in.ax = t.ax;
        in.ay = t.ay;
        in.az = t.az;
        in.bx = t.bx;
        in.by = t.by;
        in.bz = t.bz;
        in.cx = t.cx;
        in.cy = t.cy;
        in.cz = t.cz;
        in.src_id = s.src;
        in.view = static_cast<uint8_t>(view);
        // RULING R13: THE MATERIAL IS THE TRIANGLE'S, NOT THE JOB'S. It was
        // `s.mat_a` -- one value carried on the job port for a whole 8x8-cell
        // subpatch -- and the job port no longer has it. The expectation is
        // now the played plane at the TRIANGLE's own cell.
        //
        // THE CELL IS DERIVED FROM THE ORACLE'S TRIANGLE, NOT FROM THE RTL'S
        // ENUMERATOR, which is what keeps this a differential. `lat.wx` is
        // monotone with a constant step, so the per-axis minimum world corner
        // maps back to the per-axis minimum LATTICE corner exactly, and the
        // geomorph moves y alone and cannot disturb it.
        const int ci = cell_index(lat.wx, t.ax, t.bx, t.cx);
        const int cj = cell_index(lat.wz, t.az, t.bz, t.cz);
        in.mat_a = tess_test::mat_a_at(ci, cj);
        in.mat_b = tess_test::mat_b_at(ci, cj);
        in.weight = tess_test::weight_at(ci, cj);
        e.inputs.push_back(in);
      }
  }
  return e;
}
int diff_legacy(const std::vector<TriOut>& got, const std::vector<TriIn>& in,
                const zref::mat4fx (&m)[2], const zref::render::Viewport (&vp)[2]) {
  if (got.size() != in.size()) return 100000 + static_cast<int>(got.size());
  int bad = 0;
  for (size_t i = 0; i < got.size(); ++i) {
    const TriOut w = oracle(in[i], m[in[i].view], vp[in[i].view]);
    const TriOut& g = got[i];
    bool one = false;
    for (int c = 0; c < 3; ++c)
      one = one || g.x[c] != w.x[c] || g.y[c] != w.y[c] || g.d[c] != w.d[c];
    one = one || g.behind != w.behind || g.src_id != w.src_id || g.view != w.view ||
          g.mat_a != w.mat_a || g.mat_b != w.mat_b || g.weight != w.weight;
    if (one) ++bad;
  }
  return bad;
}
int diff_pipe(const std::vector<PipePacket>& got, const std::vector<TriOut>& legacy,
              const std::vector<TriIn>& in, const zref::mat4fx (&m)[2],
              const zref::render::Viewport (&vp)[2]) {
  if (got.size() != in.size() || legacy.size() != in.size())
    return 100000 + static_cast<int>(got.size());
  int bad = 0;
  for (size_t i = 0; i < got.size(); ++i) {
    const PipePacket& g = got[i];
    const TriOut& l = legacy[i];
    bool one = false;
    for (int c = 0; c < 3; ++c)
      one = one || g.p.x[c] != l.x[c] || g.p.y[c] != l.y[c] || g.p.d[c] != l.d[c];
    one = one || g.p.behind != l.behind || g.p.src_id != l.src_id || g.p.view != l.view ||
          g.p.mat_a != l.mat_a || g.p.mat_b != l.mat_b || g.p.weight != l.weight || g.refused ||
          g.missed;
    const int32_t x[3] = {in[i].ax, in[i].bx, in[i].cx};
    const int32_t y[3] = {in[i].ay, in[i].by, in[i].cy};
    const int32_t z[3] = {in[i].az, in[i].bz, in[i].cz};
    for (int c = 0; c < 3; ++c) {
      const zref::render::ProjOut p =
          zref::render::project_vertex(m[in[i].view], vp[in[i].view], zref::fx16{x[c]},
                                       zref::fx16{y[c]}, zref::fx16{z[c]}, nullptr);
      one = one || g.w[c] != static_cast<uint32_t>(p.w);
    }
    if (one) ++bad;
  }
  return bad;
}
int diff_geometry(const std::vector<GeoOut>& got, const std::vector<GeoIn>& in,
                  const zref::mat4fx (&m)[2], const zref::render::Viewport (&vp)[2]) {
  if (got.size() != in.size()) return 100000 + static_cast<int>(got.size());
  int bad = 0;
  for (size_t i = 0; i < got.size(); ++i) {
    const zref::render::ProjOut p =
        zref::render::project_vertex(m[in[i].view], vp[in[i].view], zref::fx16{in[i].x},
                                     zref::fx16{in[i].y}, zref::fx16{in[i].z}, nullptr);
    const bool one = got[i].x != p.s.x || got[i].y != p.s.y || got[i].d != p.s.d ||
                     got[i].w != static_cast<uint32_t>(p.w) || got[i].behind == p.in ||
                     got[i].view != in[i].view || got[i].payload != in[i].payload;
    if (one) ++bad;
  }
  return bad;
}
void configure_legacy(Dev& d, const zref::mat4fx (&m)[2], const zref::render::Viewport (&vp)[2]) {
  d.configure(0, m[0], vp[0]);
  d.configure(1, m[1], vp[1]);
}
}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_pipe pipe;
  PipeDriver pd(pipe);
  pd.reset();
  const int32_t m0raw[16] = {kOne, 0, 0, 0, 0, kOne, 0, 0, 0, 0, kOne, 0, 0, 0, kOne, 0};
  const int32_t m1raw[16] = {-3 * kOne / 4, 0, 0, 0, 0,    kOne / 2, 0, 0, 0, 0,
                             kOne,          0, 0, 0, kOne, 0};
  const zref::mat4fx matrices[2] = {mat_of(m0raw), mat_of(m1raw)};
  const zref::render::Viewport viewports[2] = {{0, 0, 256, 192}, {17, 9, 191, 137}};
  pd.configure(0, matrices[0], viewports[0]);
  pd.configure(1, matrices[1], viewports[1]);
#ifdef ZHAO_EXPECT_RELEASE_MUTANT
  check(pipe.release_unsafe_o == 0, "mutant detector starts at zero", 0, pipe.release_unsafe_o);
  const zt::ComposedLattice lat = make_lattice();
  Spec s;
  s.job = job(8, 8, 0, 0x4000);
  s.dual = true;
  s.views = 1;
  s.src = 0x6BAD;
  const RunResult r = pd.run(lat, {s}, {});
  check(r.cycles < 200000, "mutant job drains", 1, r.cycles < 200000 ? 1 : 0);
  check(pipe.release_unsafe_o > 0,
        "MUTANT POSITIVE CONTROL: detector catches release after one accepted triple but before "
        "ModeRef drains",
        1, pipe.release_unsafe_o > 0 ? 1 : 0);
  return zhao::report_and_exit("terrain_pipe_release_mutant");
#else
#ifdef ZHAO_PIPE_VALID_MODE
  constexpr bool kSparse = ZHAO_PIPE_VALID_MODE == 0;
#else
  constexpr bool kSparse = false;
#endif
  const zt::ComposedLattice lat = make_lattice();
  const std::vector<Spec> specs = make_specs(kSparse);
  const std::vector<GeoIn> geometry = make_geometry();
  const Expected want = make_expected(lat, specs);
  const RunResult got = pd.run(lat, specs, geometry);
  Vzhao_terrain_project legacy_dut;
  Dev legacy(legacy_dut);
  legacy.reset();
  configure_legacy(legacy, matrices, viewports);
  const std::vector<TriOut> old = legacy.run(want.inputs, 0x49249249u);
  check(got.cycles < 200000, "composed stream drains", 1, got.cycles < 200000 ? 1 : 0);
  const int old_bad = diff_legacy(old, want.inputs, matrices, viewports);
  check(old_bad == 0, "retained zhao_terrain_project remains bit-exact to project_vertex", 0,
        old_bad);
  const int packet_bad = diff_pipe(got.terrain, old, want.inputs, matrices, viewports);
  check(packet_bad == 0,
        "composed pipe equals retained projection for both views; riders/order and w match", 0,
        packet_bad);
  const int geo_bad = diff_geometry(got.geometry, geometry, matrices, viewports);
  check(geo_bad == 0, "client A remains bit-exact while contending with terrain", 0, geo_bad);
  check(pipe.jobs_accepted_o == specs.size(), "jobs_accepted_o exact", specs.size(),
        pipe.jobs_accepted_o);
  check(pipe.jobs_no_view_o == want.no_view, "jobs_no_view_o seen to fire exactly", want.no_view,
        pipe.jobs_no_view_o);
  check(pipe.jobs_rejected_o == want.rejected, "jobs_rejected_o seen to fire exactly",
        want.rejected, pipe.jobs_rejected_o);
  check(pipe.jobs_empty_o == want.empty, "jobs_empty_o seen to fire exactly", want.empty,
        pipe.jobs_empty_o);
  check(pipe.groups_opened_o == want.groups, "groups_opened_o exact", want.groups,
        pipe.groups_opened_o);
  check(pipe.groups_released_o == want.groups, "groups_released_o exact", want.groups,
        pipe.groups_released_o);
  check(pipe.fills_forwarded_o == want.fills, "fills_forwarded_o exact", want.fills,
        pipe.fills_forwarded_o);
  check(pipe.fills_dropped_o == want.drops, "fills_dropped_o exact", want.drops,
        pipe.fills_dropped_o);
  check(pipe.refs_forwarded_o == want.refs, "refs_forwarded_o exact", want.refs,
        pipe.refs_forwarded_o);
  check(pipe.release_unsafe_o == 0, "release_unsafe_o silent in legal composition", 0,
        pipe.release_unsafe_o);
  check(pipe.tess_vertices_o == want.vertices, "tess vertex transfers exact", want.vertices,
        pipe.tess_vertices_o);
  check(pipe.tess_refs_o == want.tess_refs, "tess reference transfers exact", want.tess_refs,
        pipe.tess_refs_o);
  check(pipe.tess_rejected_o == want.rejected, "tess reject counter exact", want.rejected,
        pipe.tess_rejected_o);
  check(pipe.tess_lod_clamped_o == want.clamped, "tess clamp counter seen to fire", want.clamped,
        pipe.tess_lod_clamped_o);
  check(pipe.tess_mode_invalid_o == 0, "sequencer presents only legal tess modes", 0,
        pipe.tess_mode_invalid_o);
  check(pipe.replay_triangles_o == want.refs, "every forwarded reference lands", want.refs,
        pipe.replay_triangles_o);
  check(pipe.replay_refused_o == 0 && pipe.replay_missed_o == 0, "legal replay has no refusal/miss",
        0, pipe.replay_refused_o + pipe.replay_missed_o);
  check(pipe.corner_hits_o == 3u * want.refs, "all three corners hit", 3u * want.refs,
        pipe.corner_hits_o);
  check(pipe.corner_refusals_o == 0 && pipe.corner_misses_o == 0,
        "legal replay has no corner fault", 0, pipe.corner_refusals_o + pipe.corner_misses_o);
  check(pipe.arena_overflow_o == 0 && pipe.arena_seal_short_o == 0,
        "legal arena use has no fill fault", 0, pipe.arena_overflow_o + pipe.arena_seal_short_o);
  check(pipe.a_grants_o == geometry.size(), "all geometry vertices granted", geometry.size(),
        pipe.a_grants_o);
  check(pipe.b_grants_o == want.fills, "all terrain fills granted", want.fills, pipe.b_grants_o);
  check(pipe.contended_o > 0, "shared-projector contention counter seen to fire", 1,
        pipe.contended_o > 0 ? 1 : 0);
  // The refusal count is a claim about the MATRIX WIDTH, so it has to name the
  // width it was compiled for. It read "MATW=32" unconditionally, which was
  // true of every build that existed -- and would have gone on reading MATW=32
  // in the MATW=18 build the G8B target actually characterises.
  //
  // The expectation itself is the same at both widths and deliberately so: the
  // narrowed core must refuse an out-of-range product rather than clamp it, so
  // a refusal here is not a tolerated rounding difference, it is this stimulus
  // going outside the range G8B is specified for. If MATW=18 ever refuses on
  // legal terrain, that is a finding about the G8B target and not a test to
  // relax.
  check(pipe.mat_refused_o == 0, "MATW=" ZHAO_STR(ZHAO_PIPE_MATW) " refuses no projection", 0,
        pipe.mat_refused_o);
  check(pipe.held_o == 0 && pipe.idle_o, "all arenas released and composition idle", 0,
        pipe.held_o);
  check(got.held_output_errors == 0, "stalled output packet remains completely stable", 0,
        got.held_output_errors);
  check(got.accepted_with_pending_output > 0 && got.opens_with_stalled_output > 0,
        "next job accepts and reopens arenas while an older copied output remains stalled", 1,
        got.accepted_with_pending_output > 0 && got.opens_with_stalled_output > 0 ? 1 : 0);
  check(!old.empty(), "positive-control stream is nonempty", 1, old.empty() ? 0 : 1);
  if (!old.empty()) {
    std::vector<TriOut> broken = old;
    broken[broken.size() / 2].x[1] ^= 1;
    const int broken_bad = diff_pipe(got.terrain, broken, want.inputs, matrices, viewports);
    check(broken_bad == 1, "POSITIVE CONTROL: comparator catches one flipped coordinate", 1,
          broken_bad);
  }
  // The two overlap counters are PRINTED, not merely asserted. They are
  // coverage, not results: the check above fails when this stimulus stops
  // REACHING the overlap, which looks identical to the machine losing it. A
  // bare "expected 0x1, got 0x0" cannot tell those apart, and the first costs
  // an investigation of the wrong component. Printing both says which.
  std::printf(
      "terrain_pipe_differential: %zu packets, %zu geometry vertices, %d cycles; mode=%s"
      " (overlap coverage: accepted_with_pending_output=%d, opens_with_stalled_output=%d)\n",
      got.terrain.size(), got.geometry.size(), got.cycles, kSparse ? "bitmap+sparse" : "dense",
      got.accepted_with_pending_output, got.opens_with_stalled_output);
  if (!kSparse) {
    // THE DELIBERATELY ILLEGAL COMBINATION -- RETAINED, WITH ITS EXPECTATION
    // TURNED THE RIGHT WAY UP. Owner ruling 2026-09-22 item 5:
    //
    //   "Enabling sparse fill with a dense seal is not a permissible
    //    configuration ... refuse an unsafe combination and hold the chosen
    //    setting stable for a whole job ... Demonstrate identical rendered
    //    output and no unfilled reference ... retain the deliberately
    //    illegal-combination test."
    //
    // This case used to assert the DEFECT: the pairing was let through and
    // caught downstream as a short seal, a refused replay and corner
    // refusals, and the checks read "SEEN TO FIRE". That is a test that
    // passes only while the hole exists. zhao_terrain_group_seq now carries
    // VALID_MODE and REFUSES the pairing at the job accept, so the assertions
    // below are the correct behaviour instead: the request is refused and
    // counted, the shell never sees a short arena, and the job renders
    // BYTE-IDENTICALLY to the same job configured legally.
    //
    // The three downstream counters keep their positive controls at their own
    // blocks -- arena_seal_short_o in vertex_arena_dense_seal_control and
    // vertex_arena_dense_directed, replay_refused_o and corner_refusals_o in
    // terrain_wcache_differential and tb_terrain_wcache -- so none is left
    // asserted-zero and unfireable by this change, and no committed mutant is
    // owed for it.
    Spec fault;
    fault.job = job(8, 8, 2, 0x2000);
    fault.views = 3;
    fault.dual = true;
    fault.src = 0x5EAD;

    // (a) the same job configured LEGALLY (sparse off) -- the reference render.
    pd.reset();
    pd.configure(0, matrices[0], viewports[0]);
    pd.configure(1, matrices[1], viewports[1]);
    Spec legal = fault;
    legal.sparse = false;
    const RunResult lr = pd.run(lat, {legal}, {});
    check(lr.cycles < 200000, "legal reference job drains", 1, lr.cycles < 200000 ? 1 : 0);
    check(pipe.sparse_refused_o == 0,
          "CONTROL: a job that did not ask for sparse fill is not counted as refused", 0,
          pipe.sparse_refused_o);
    const std::vector<PipePacket> legal_out = lr.terrain;

    // (b) the same job with the ILLEGAL request.
    pd.reset();
    pd.configure(0, matrices[0], viewports[0]);
    pd.configure(1, matrices[1], viewports[1]);
    fault.sparse = true;
    const RunResult fr = pd.run(lat, {fault}, {});
    check(fr.cycles < 200000, "dense sparse-fill request drains", 1,
          fr.cycles < 200000 ? 1 : 0);
    check(pipe.sparse_refused_o == 1,
          "SEEN TO FIRE: sparse fill refused against a dense-seal shell", 1,
          pipe.sparse_refused_o);
    check(pipe.fills_dropped_o == 0,
          "the refused job filled every vertex -- no filler was skipped", 0,
          pipe.fills_dropped_o);
    check(pipe.arena_seal_short_o == 0,
          "the shell never sees a short arena, because the pairing never reaches it", 0,
          pipe.arena_seal_short_o);
    check(pipe.replay_refused_o == 0 && pipe.corner_refusals_o == 0,
          "no replay or corner refusal follows a refused sparse request", 0,
          pipe.replay_refused_o + pipe.corner_refusals_o);
    check(pipe.release_unsafe_o == 0, "the refused job still releases after reference acceptance",
          0, pipe.release_unsafe_o);

    // (c) IDENTICAL RENDERED OUTPUT -- "reduced redundant work, not reduced
    // visual capability", and the half no counter above can show.
    bool same = legal_out.size() == fr.terrain.size() && !legal_out.empty();
    for (size_t k = 0; same && k < legal_out.size(); ++k) {
      const PipePacket& a = legal_out[k];
      const PipePacket& b = fr.terrain[k];
      same = a.refused == b.refused && a.missed == b.missed && a.p.behind == b.p.behind &&
             a.p.src_id == b.p.src_id && a.p.view == b.p.view && a.p.mat_a == b.p.mat_a &&
             a.p.mat_b == b.p.mat_b && a.p.weight == b.p.weight;
      for (int v = 0; v < 3 && same; ++v) {
        same = same && a.p.x[v] == b.p.x[v] && a.p.y[v] == b.p.y[v] && a.p.d[v] == b.p.d[v] &&
               a.w[v] == b.w[v];
      }
    }
    check(same,
          "IDENTICAL RENDER: the refused job emits exactly what the legal one does", 1,
          same ? 1 : 0);
    std::printf("  item 5: %zu packets legal, %zu refused, sparse_refused_o=%u\n",
                legal_out.size(), fr.terrain.size(),
                static_cast<unsigned>(pipe.sparse_refused_o));
  }
  return zhao::report_and_exit(kSparse ? "terrain_pipe_differential_bitmap"
                                       : "terrain_pipe_differential");
#endif
}
