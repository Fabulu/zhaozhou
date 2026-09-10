// terrain_wcache_differential.cpp — every replayed triangle bit-identical to
// the retained `zhao_terrain_project`.
//
// THE COMPOSITION UNDER TEST is tests/terrain/tb_terrain_wcache.sv: one
// `zhao_project_service` whose client B is the terrain vertex stream and whose
// client A is saturated with geometry noise, feeding `zhao_terrain_wcache`
// directly off the service's client-B result port; `zhao_terrain_topo` walks
// each sealed group into 128 corner references; the shell answers one
// projected triangle per clock. Alongside it, UNSHARED, sits the legacy
// `zhao_terrain_project` -- the block the shared lane exists to retire -- fed
// the same world triangles under the same configuration. The acceptance
// criterion is the brief's: every field the legacy packet carries must be
// bit-identical, triangle for triangle, in order.
//
// THE FILL PRODUCER IS THE ORACLE TESSELLATOR. The hardware tessellator emits
// world coordinates and has no vertex mode yet (the adoption report names that
// as remaining work), so this harness produces the 81 lattice vertices with
// `zref::terrain::detail::vertex_at` -- the exact function `tessellate` calls
// per corner, and the law `terrain_tess_directed` proves the TESS RTL
// reproduces -- and the oracle triangles with `tessellate` itself. Same
// lattice, same job, same morph; the two paths share a vertex law and differ
// only in WHEN the projection happens.
//
// `w` HAS NO LEGACY ORACLE. `zhao_terrain_project` never exposed clip w, so
// the three `out_*w_o` are checked against `zref::render::project_vertex`'s
// `ProjOut::w` (the shipped function, the same instrument every projector
// suite in this tree uses). x/y/d/behind are checked against BOTH.
//
// The cases the brief requires, each a section below:
//   1. shared edges, underside winding, behind-eye corners, saturation rails,
//      both views, two live groups per view, client-A contention, random
//      stalls on both consumers            -- the 64-group main run
//   2. stale-handle replay                 -- gen-1, reopened, and unsealed
//   3. the dense seal law                  -- short seal refused (SEEN TO FIRE),
//      misordered fill dropped (SEEN TO FIRE); in the VALID_MODE=0 elaboration
//      the sealed-but-incomplete arena MISSES instead (SEEN TO FIRE)
//   4. mid-frame reconfiguration           -- records freeze at fill time; a
//      fill torn by a matrix write is faithful per vertex to the matrix at
//      its accept edge
//   5. POSITIVE CONTROL                    -- one raw LSB skewed in one matrix
//      word of the service alone must make the comparator fire
//
// Compiled UNCHANGED against three elaborations of the same testbench:
// ROWS_PER_PASS=3 (default), ROWS_PER_PASS=1 (one vertex per three clocks --
// the producer honours b_ready_o and nothing else changes), VALID_MODE=0
// (the bitmap primitive under the same shell, section 3's miss control).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <vector>

#include "verilated.h"

#include "Vtb_terrain_wcache.h"

#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_tess.hpp"
#include "zrender/internal.hpp"

using zhao::check;
namespace zt = zref::terrain;

namespace {

constexpr int32_t kOne = 1 << 16;
constexpr int kArenas = 4;
constexpr int kDepth = 81;
constexpr int kTrisPerGroup = 128;
constexpr int kMaxPrinted = 24;  // per family, then count silently

#ifndef ZHAO_TWC_VALID_MODE
#define ZHAO_TWC_VALID_MODE 1
#endif
constexpr bool kDense = (ZHAO_TWC_VALID_MODE == 1);

// ---- deterministic PRNG (xorshift32) ------------------------------------------
struct Rng {
  uint32_t s = 0x9E3779B9u;
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  bool chance(uint32_t pct) { return (next() % 100u) < pct; }
  int32_t range(int32_t lo, int32_t hi) {  // inclusive
    return lo + static_cast<int32_t>(next() % static_cast<uint32_t>(hi - lo + 1));
  }
};

int32_t sx21(uint32_t v) {
  const uint32_t m = v & 0x1FFFFFu;
  return static_cast<int32_t>((m ^ 0x100000u) - 0x100000u);
}

// ---- one projected triangle, whichever path produced it ---------------------------
struct Tri {
  int32_t x[3] = {0, 0, 0};
  int32_t y[3] = {0, 0, 0};
  int32_t d[3] = {0, 0, 0};
  uint32_t w[3] = {0, 0, 0};  // shell and zref only; the legacy packet has no w
  uint8_t behind = 0;
  uint16_t src_id = 0;
  uint8_t view = 0;
  uint8_t mat_a = 0, mat_b = 0, weight = 0;
  bool refused = false, missed = false;
};

// ---- a group: one subpatch x surface x view, projected under one matrix epoch ----
struct Group {
  zt::SubpatchJob job;
  uint8_t view = 0;
  uint16_t src_id = 0;
  uint8_t mat_a = 0, mat_b = 0, weight = 0;
  int arena = -1;
  uint32_t gen = 0;
  std::vector<zt::MeshTri> tris;  // the oracle's 128, in walk order
  zref::mat4fx m;                 // the matrix this group was filled under
  zref::render::Viewport vp;
};

zref::mat4fx mat_of(const int32_t (&m)[16]) {
  zref::mat4fx r{};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) r.m[i][j] = zref::fx16{m[i * 4 + j]};
  return r;
}

Tri zref_tri(const zt::MeshTri& t, const zref::mat4fx& m, const zref::render::Viewport& vp) {
  const int32_t wx[3] = {t.ax, t.bx, t.cx};
  const int32_t wy[3] = {t.ay, t.by, t.cy};
  const int32_t wz[3] = {t.az, t.bz, t.cz};
  Tri o;
  for (int k = 0; k < 3; ++k) {
    const zref::render::ProjOut p = zref::render::project_vertex(
        m, vp, zref::fx16{wx[k]}, zref::fx16{wy[k]}, zref::fx16{wz[k]}, nullptr);
    o.x[k] = p.s.x;
    o.y[k] = p.s.y;
    o.d[k] = p.s.d;
    o.w[k] = static_cast<uint32_t>(p.w) & 0x7FFFFFFFu;
    if (!p.in) o.behind = static_cast<uint8_t>(o.behind | (1u << k));
  }
  return o;
}

// ---- the bench ---------------------------------------------------------------------
class Bench {
 public:
  Vtb_terrain_wcache* tb;
  Rng rng;
  uint64_t cycle = 0;

  // configuration the two clients project through (the service's bank)
  zref::mat4fx svc_m[2];
  zref::render::Viewport svc_vp[2];

  // client A bookkeeping
  struct AVert {
    int32_t x, y, z;
    uint8_t view;
    uint16_t pay;
    zref::mat4fx m;
    zref::render::Viewport vp;
  };
  std::deque<AVert> a_inflight;
  uint16_t a_seq = 1;
  bool a_enable = false;
  uint32_t a_stall_pct = 50;
  int a_checked = 0;

  // the stream of shell and legacy outputs, in order
  std::deque<Tri> shell_out, legacy_out;
  uint32_t stall_pct = 30;
  bool legacy_feed = true;

  // comparison families
  int mism_legacy = 0, mism_zref = 0, printed_legacy = 0, printed_zref = 0;
  int compared = 0;

  // fills: presented-and-accepted on client B vs landed in the shell. CUMULATIVE,
  // because with a 36-cycle core most of a group's fills land WHILE its later
  // vertices are still being presented -- a per-call "wait for n more" count
  // starts too late and was this harness's first bug.
  uint64_t fills_sent = 0, fills_landed = 0;

  explicit Bench(Vtb_terrain_wcache* t) : tb(t) {}

  void settle() {
    tb->clk = 0;
    tb->eval();
  }
  void edge() {
    tb->clk = 1;
    tb->eval();
    ++cycle;
  }

  void quiet_inputs() {
    tb->cfg_we_i = 0;
    tb->ocfg_we_i = 0;
    tb->en_i = 1;
    tb->a_valid_i = 0;
    tb->b_valid_i = 0;
    tb->open_i = 0;
    tb->seal_i = 0;
    tb->job_valid_i = 0;
    tb->tri_valid_i = 0;
    tb->out_ready_i = 1;
    tb->o_ready_i = 1;
  }

  void reset() {
    quiet_inputs();
    tb->rst_n = 0;
    for (int i = 0; i < 4; ++i) {
      settle();
      edge();
    }
    tb->rst_n = 1;
    settle();
    edge();
  }

  // One configuration write to the service (svc) and/or the oracle (orc).
  void write_cfg(bool svc, bool orc, int view, uint8_t addr, uint32_t data) {
    tb->cfg_we_i = svc ? 1 : 0;
    tb->cfg_view_i = view;
    tb->cfg_addr_i = addr;
    tb->cfg_data_i = data;
    tb->ocfg_we_i = orc ? 1 : 0;
    tb->ocfg_view_i = view;
    tb->ocfg_addr_i = addr;
    tb->ocfg_data_i = data;
    step();  // a plain cycle: the concurrent actors keep running
    tb->cfg_we_i = 0;
    tb->ocfg_we_i = 0;
  }

  void configure(bool svc, bool orc, int view, const zref::mat4fx& m,
                 const zref::render::Viewport& vp) {
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c)
        write_cfg(svc, orc, view, static_cast<uint8_t>(r * 4 + c),
                  static_cast<uint32_t>(m.m[r][c].raw));
    write_cfg(svc, orc, view, 16, (vp.y0 << 16) | (vp.x0 & 0xFFFu));
    write_cfg(svc, orc, view, 17, (vp.h << 16) | (vp.w & 0xFFFu));
    if (svc) {
      svc_m[view] = m;
      svc_vp[view] = vp;
    }
  }

  // Write only the nine PRODUCT words + translation of a matrix (no viewport):
  // the core samples the matrix at accept but the viewport ~34 cycles later,
  // so a torn-fill experiment must move only the matrix.
  void write_matrix_only(bool svc, bool orc, int view, const zref::mat4fx& m) {
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c)
        write_cfg(svc, orc, view, static_cast<uint8_t>(r * 4 + c),
                  static_cast<uint32_t>(m.m[r][c].raw));
    if (svc) svc_m[view] = m;
  }

  // ---- the legacy feed: world triangles waiting to enter the oracle ------------
  struct LegIn {
    zt::MeshTri t;
    uint16_t src;
    uint8_t view, mat_a, mat_b, weight;
  };
  std::deque<LegIn> legacy_in;

  // ---- ONE CYCLE of every concurrent actor ----------------------------------------------
  // The producer/consumer FSMs set b_*/open/seal/job before calling this; this
  // function owns client A, the legacy feed, the two ready lines, and the
  // collection of every output. Inputs are sampled at the edge like hardware.
  void step() {
    // client A: geometry noise, random valid, honours a_ready_o
    if (a_enable && !rng.chance(a_stall_pct)) {
      tb->a_valid_i = 1;
      tb->a_vx_i = static_cast<uint32_t>(rng.range(-8 * kOne, 8 * kOne));
      tb->a_vy_i = static_cast<uint32_t>(rng.range(-8 * kOne, 8 * kOne));
      tb->a_vz_i = static_cast<uint32_t>(rng.range(-2 * kOne, 16 * kOne));
      tb->a_view_i = rng.chance(50) ? 1 : 0;
      tb->a_payload_i = a_seq;
    } else {
      tb->a_valid_i = 0;
    }
    // the legacy oracle's input
    if (legacy_feed && !legacy_in.empty()) {
      const LegIn& li = legacy_in.front();
      tb->tri_valid_i = 1;
      tb->ax_i = static_cast<uint32_t>(li.t.ax);
      tb->ay_i = static_cast<uint32_t>(li.t.ay);
      tb->az_i = static_cast<uint32_t>(li.t.az);
      tb->bx_i = static_cast<uint32_t>(li.t.bx);
      tb->by_i = static_cast<uint32_t>(li.t.by);
      tb->bz_i = static_cast<uint32_t>(li.t.bz);
      tb->cx_i = static_cast<uint32_t>(li.t.cx);
      tb->cy_i = static_cast<uint32_t>(li.t.cy);
      tb->cz_i = static_cast<uint32_t>(li.t.cz);
      tb->src_id_i = li.src;
      tb->view_i = li.view;
      tb->mat_a_i = li.mat_a;
      tb->mat_b_i = li.mat_b;
      tb->weight_i = li.weight;
    } else {
      tb->tri_valid_i = 0;
    }
    // random consumer stalls on both output ports
    tb->out_ready_i = rng.chance(stall_pct) ? 0 : 1;
    tb->o_ready_i = rng.chance(stall_pct) ? 0 : 1;

    settle();

    // ---- sample: handshakes and outputs, before the edge ----
    const bool a_take = tb->a_valid_i && tb->a_ready_o;
    const bool leg_take = tb->tri_valid_i && tb->tri_ready_o;
    b_take_ = tb->b_valid_i && tb->b_ready_o;
    fill_seen_ = tb->fill_seen_o != 0;
    if (fill_seen_) ++fills_landed;
    if (b_take_) ++fills_sent;
    job_take_ = tb->job_valid_i && tb->job_ready_o;
    open_gen_sampled_ = tb->open_gen_o;

    if (tb->a_valid_o) {
      // A's own result: in order, its payload, and the shipped law
      check(!a_inflight.empty(), "client A result with nothing in flight", 0, 1);
      if (!a_inflight.empty()) {
        const AVert v = a_inflight.front();
        a_inflight.pop_front();
        const zref::render::ProjOut p = zref::render::project_vertex(
            v.m, v.vp, zref::fx16{v.x}, zref::fx16{v.y}, zref::fx16{v.z}, nullptr);
        const bool ok = tb->a_payload_o == v.pay && sx21(tb->a_x_o) == p.s.x &&
                        sx21(tb->a_y_o) == p.s.y &&
                        static_cast<int32_t>(tb->a_d_o) == p.s.d &&
                        (tb->a_behind_o != 0) == !p.in && tb->a_view_o == v.view &&
                        tb->a_w_o == (static_cast<uint32_t>(p.w) & 0x7FFFFFFFu);
        if (!ok && a_bad_printed_ < kMaxPrinted) {
          ++a_bad_printed_;
          std::printf("  client A mismatch: pay %u want %u  x %d/%d y %d/%d d %d/%d\n",
                      tb->a_payload_o, v.pay, sx21(tb->a_x_o), p.s.x, sx21(tb->a_y_o),
                      p.s.y, static_cast<int32_t>(tb->a_d_o), p.s.d);
        }
        if (!ok) ++a_bad_;
        ++a_checked;
      }
    }
    if (tb->out_valid_o && tb->out_ready_i) {
      Tri t;
      t.x[0] = sx21(tb->out_ax_o);
      t.y[0] = sx21(tb->out_ay_o);
      t.x[1] = sx21(tb->out_bx_o);
      t.y[1] = sx21(tb->out_by_o);
      t.x[2] = sx21(tb->out_cx_o);
      t.y[2] = sx21(tb->out_cy_o);
      t.d[0] = static_cast<int32_t>(tb->out_ad_o);
      t.d[1] = static_cast<int32_t>(tb->out_bd_o);
      t.d[2] = static_cast<int32_t>(tb->out_cd_o);
      t.w[0] = tb->out_aw_o;
      t.w[1] = tb->out_bw_o;
      t.w[2] = tb->out_cw_o;
      t.behind = tb->out_behind_o;
      t.src_id = tb->out_src_id_o;
      t.view = tb->out_view_o;
      t.mat_a = tb->out_mat_a_o;
      t.mat_b = tb->out_mat_b_o;
      t.weight = tb->out_weight_o;
      t.refused = tb->out_refused_o != 0;
      t.missed = tb->out_missed_o != 0;
      shell_out.push_back(t);
    }
    if (tb->o_valid_o && tb->o_ready_i) {
      Tri t;
      t.x[0] = sx21(tb->o_ax_o);
      t.y[0] = sx21(tb->o_ay_o);
      t.x[1] = sx21(tb->o_bx_o);
      t.y[1] = sx21(tb->o_by_o);
      t.x[2] = sx21(tb->o_cx_o);
      t.y[2] = sx21(tb->o_cy_o);
      t.d[0] = static_cast<int32_t>(tb->o_ad_o);
      t.d[1] = static_cast<int32_t>(tb->o_bd_o);
      t.d[2] = static_cast<int32_t>(tb->o_cd_o);
      t.behind = tb->o_behind_o;
      t.src_id = tb->o_src_id_o;
      t.view = tb->o_view_o;
      t.mat_a = tb->o_mat_a_o;
      t.mat_b = tb->o_mat_b_o;
      t.weight = tb->o_weight_o;
      legacy_out.push_back(t);
    }

    edge();

    if (a_take) {
      AVert v;
      v.x = static_cast<int32_t>(tb->a_vx_i);
      v.y = static_cast<int32_t>(tb->a_vy_i);
      v.z = static_cast<int32_t>(tb->a_vz_i);
      v.view = tb->a_view_i;
      v.pay = a_seq;
      v.m = svc_m[v.view];
      v.vp = svc_vp[v.view];
      a_inflight.push_back(v);
      ++a_seq;
      if (a_seq == 0) a_seq = 1;
    }
    if (leg_take) legacy_in.pop_front();
  }

  bool b_take() const { return b_take_; }
  bool fill_seen() const { return fill_seen_; }
  bool job_take() const { return job_take_; }
  uint32_t open_gen_sampled() const { return open_gen_sampled_; }
  int a_bad() const { return a_bad_; }

  // ---- blocking helpers built on step() (each is one or more full cycles) --------
  uint32_t open(int arena) {
    tb->open_i = 1;
    tb->open_arena_i = arena;
    step();
    tb->open_i = 0;
    return open_gen_sampled();
  }
  void seal(int arena) {
    tb->seal_i = 1;
    tb->seal_arena_i = arena;
    step();
    tb->seal_i = 0;
  }
  // Present one vertex on client B until accepted. Returns cycles waited.
  int fill_vertex(int arena, int index, const zt::detail::TessVert& v, uint8_t view) {
    tb->b_valid_i = 1;
    tb->b_vx_i = static_cast<uint32_t>(v.x);
    tb->b_vy_i = static_cast<uint32_t>(v.y);
    tb->b_vz_i = static_cast<uint32_t>(v.z);
    tb->b_view_i = view;
    tb->b_arena_i = arena;
    tb->b_index_i = index;
    int waited = 0;
    do {
      step();
      ++waited;
      if (waited > 5000) {
        check(false, "client B never accepted a vertex", 1, 0);
        break;
      }
    } while (!b_take());
    tb->b_valid_i = 0;
    return waited;
  }
  // Wait until every vertex client B has accepted has landed in the shell.
  void wait_fills() {
    int guard = 0;
    while (fills_landed < fills_sent) {
      step();
      if (++guard > 20000) {
        check(false, "fills never landed", fills_sent, fills_landed);
        break;
      }
    }
  }
  void issue_job(const Group& g, uint32_t gen) {
    tb->job_valid_i = 1;
    tb->job_arena_i = g.arena;
    tb->job_gen_i = gen & 0xFF;
    tb->job_surface_i = g.job.surface == zt::Surface::kUnderside ? 1 : 0;
    tb->job_src_id_i = g.src_id;
    tb->job_view_i = g.view;
    tb->job_mat_a_i = g.mat_a;
    tb->job_mat_b_i = g.mat_b;
    tb->job_weight_i = g.weight;
    int guard = 0;
    do {
      step();
      if (++guard > 5000) {
        check(false, "walker never took the job", 1, 0);
        break;
      }
    } while (!job_take());
    tb->job_valid_i = 0;
  }
  void idle(int n) {
    for (int i = 0; i < n; ++i) step();
  }
  // Run until the shell has produced `n` triangles total (or time out).
  void drain_shell_to(size_t n) {
    int guard = 0;
    while (shell_out.size() < n) {
      step();
      if (++guard > 40000) {
        check(false, "shell never produced the expected triangles", n, shell_out.size());
        break;
      }
    }
  }
  void drain_legacy_to(size_t n) {
    int guard = 0;
    while (legacy_out.size() < n) {
      step();
      if (++guard > 40000) {
        check(false, "legacy never produced the expected triangles", n, legacy_out.size());
        break;
      }
    }
  }

  // ---- the comparison: shell vs legacy (bit-identical) and shell vs zref (w) ------
  void compare_group(const Group& g, size_t base, bool against_legacy) {
    for (int i = 0; i < kTrisPerGroup; ++i) {
      const Tri& s = shell_out[base + i];
      const Tri z = zref_tri(g.tris[i], g.m, g.vp);
      ++compared;
      bool ok = !s.refused && !s.missed && s.src_id == g.src_id && s.view == g.view &&
                s.mat_a == g.mat_a && s.mat_b == g.mat_b && s.weight == g.weight &&
                s.behind == z.behind;
      for (int k = 0; k < 3; ++k)
        ok = ok && s.x[k] == z.x[k] && s.y[k] == z.y[k] && s.d[k] == z.d[k] && s.w[k] == z.w[k];
      if (!ok) {
        ++mism_zref;
        if (printed_zref < kMaxPrinted) {
          ++printed_zref;
          std::printf("  shell vs zref: group src %u tri %d  A x %d/%d y %d/%d d %d/%d w %u/%u  "
                      "behind %u/%u refused %d missed %d\n",
                      g.src_id, i, s.x[0], z.x[0], s.y[0], z.y[0], s.d[0], z.d[0], s.w[0],
                      z.w[0], s.behind, z.behind, s.refused ? 1 : 0, s.missed ? 1 : 0);
        }
      }
      if (against_legacy) {
        const Tri& l = legacy_out[base + i];
        bool same = s.behind == l.behind && s.src_id == l.src_id && s.view == l.view &&
                    s.mat_a == l.mat_a && s.mat_b == l.mat_b && s.weight == l.weight;
        for (int k = 0; k < 3; ++k)
          same = same && s.x[k] == l.x[k] && s.y[k] == l.y[k] && s.d[k] == l.d[k];
        if (!same) {
          ++mism_legacy;
          if (printed_legacy < kMaxPrinted) {
            ++printed_legacy;
            std::printf("  shell vs LEGACY: group src %u tri %d  A x %d/%d y %d/%d d %d/%d  "
                        "behind %u/%u src %u/%u\n",
                        g.src_id, i, s.x[0], l.x[0], s.y[0], l.y[0], s.d[0], l.d[0], s.behind,
                        l.behind, s.src_id, l.src_id);
          }
        }
      }
    }
  }

 private:
  bool b_take_ = false, fill_seen_ = false, job_take_ = false;
  uint32_t open_gen_sampled_ = 0;
  int a_bad_ = 0, a_bad_printed_ = 0;
};

// ---- the terrain -----------------------------------------------------------------------------
zt::ComposedLattice make_lattice(Rng& rng) {
  zt::ComposedLattice lat;
  lat.w = 33;
  lat.h = 33;
  lat.dual = true;
  lat.wx.resize(33);
  lat.wz.resize(33);
  lat.top.resize(33 * 33);
  lat.bottom.resize(33 * 33);
  // Pitch 0.5 world units. z runs from -1.0 so lattice rows 0, 1 and 2 sit
  // behind or ON the near plane under the fixture's w = z (row 2 is w == 0
  // exactly -- the boundary, which is REJECTED by `<=`). x is centred.
  for (int i = 0; i < 33; ++i) {
    lat.wx[static_cast<size_t>(i)] = -8 * kOne + i * (kOne / 2);
    lat.wz[static_cast<size_t>(i)] = -kOne + i * (kOne / 2);
  }
  for (size_t k = 0; k < lat.top.size(); ++k) {
    lat.top[k] = rng.range(-3 * kOne, 3 * kOne);
    lat.bottom[k] = lat.top[k] - rng.range(kOne / 4, 2 * kOne);
  }
  return lat;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* top = new Vtb_terrain_wcache;
  Bench b(top);
  b.reset();

  Rng lat_rng{0x1234567u};
  const zt::ComposedLattice lat = make_lattice(lat_rng);

  // Two views, two cameras. View 0: w = z. View 1: w = z + 0.75, with a shear
  // in x and a different viewport -- a different behind set, different rails.
  const int32_t m0[16] = {kOne, 0, 0, 0, 0, kOne, 0, 0, 0, 0, kOne, 0, 0, 0, kOne, 0};
  const int32_t m1[16] = {kOne, 0, kOne / 4, kOne / 2, 0, -kOne, 0, 0, 0, 0, kOne, 0, 0, 0,
                          kOne, 3 * kOne / 4};
  const zref::mat4fx M0 = mat_of(m0), M1 = mat_of(m1);
  const zref::render::Viewport VP0{0, 0, 256, 192};
  const zref::render::Viewport VP1{64, 32, 128, 96};
  b.configure(true, true, 0, M0, VP0);
  b.configure(true, true, 1, M1, VP1);

  // =========================================================================
  // 1. THE MAIN RUN: 4x4 subpatches x {top, underside} x {view 0, view 1}
  //    two groups in flight per view, client A saturating, both consumers
  //    stalling at random.
  // =========================================================================
  std::vector<Group> groups;
  for (int view = 0; view < 2; ++view)
    for (int surf = 0; surf < 2; ++surf)
      for (int sz = 0; sz < 4; ++sz)
        for (int sx = 0; sx < 4; ++sx) {
          Group g;
          g.job.ox = sx * 8;
          g.job.oz = sz * 8;
          g.job.level = 0;
          for (int k = 0; k < 4; ++k) g.job.nlevel[k] = 0;
          // level-0 geomorph on odd columns: interior vertices move toward
          // the level-1 coarse height, boundary vertices never do
          g.job.morph = (sx & 1) ? 0x8000 : 0;
          g.job.surface = surf ? zt::Surface::kUnderside : zt::Surface::kTop;
          g.view = static_cast<uint8_t>(view);
          g.src_id = static_cast<uint16_t>(0x1000 + groups.size());
          g.mat_a = static_cast<uint8_t>(0x10 + groups.size());
          g.mat_b = static_cast<uint8_t>(0x80 ^ groups.size());
          g.weight = static_cast<uint8_t>(3 * groups.size());
          g.m = view ? M1 : M0;
          g.vp = view ? VP1 : VP0;
          const zt::TessResult r = zt::tessellate(lat, g.job, nullptr);
          check(r.tris.size() == kTrisPerGroup, "oracle emits 128 triangles for a level-0 subpatch",
                kTrisPerGroup, r.tris.size());
          g.tris = r.tris;
          groups.push_back(g);
        }

  b.a_enable = true;

  // The scheduler: arenas 0,1 serve view 0 and 2,3 serve view 1 (the 2 x 2 of
  // the proposed ARENAS=4). Fill the next group of a view into the free
  // arena while the previous one replays.
  struct ArenaState {
    int sealed_group = -1;
  };
  ArenaState ar[kArenas];
  size_t next_group[2] = {0, 32};        // group index cursors per view
  const size_t end_group[2] = {32, 64};
  std::deque<int> replay_queue;          // groups whose job has been issued, in order
  size_t shell_base = 0;                 // triangles compared so far
  int groups_done = 0;

  // A simple round: for each view, if an arena is free, fill+seal the next
  // group into it (this is blocking per vertex but client A and the legacy
  // feed keep running inside step()); then issue jobs for sealed groups; then
  // compare whatever has fully landed.
  int guard = 0;
  while (groups_done < 64 && guard++ < 400) {
    for (int view = 0; view < 2; ++view) {
      for (int a = 2 * view; a < 2 * view + 2; ++a) {
        if (ar[a].sealed_group < 0 && next_group[view] < end_group[view]) {
          Group& g = groups[next_group[view]++];
          g.arena = a;
          // The walk of this arena's PREVIOUS group may still be reading it:
          // wait for the walker's hold to drop, and only then open. Its
          // neighbour arena is meanwhile being walked or filled -- this is
          // the 2-per-view overlap ARENAS=4 exists for.
          int hg = 0;
          while (top->hold_o && top->hold_arena_o == static_cast<uint32_t>(a) && hg++ < 4000) b.step();
          check(hg < 4000, "walker released the arena", 1, hg < 4000 ? 1 : 0);
          g.gen = b.open(a);
          for (int k = 0; k < kDepth; ++k) {
            const int vi = g.job.ox + k % 9, vj = g.job.oz + k / 9;
            const zt::detail::TessVert v = zt::detail::vertex_at(lat, g.job, vi, vj, g.job.morph, nullptr);
            b.fill_vertex(a, k, v, g.view);
            if (b.rng.chance(10)) b.idle(b.rng.range(1, 3));
          }
          b.wait_fills();
          b.seal(a);
          ar[a].sealed_group = static_cast<int>(&g - &groups[0]);
        }
      }
    }
    // issue jobs for sealed groups (the walker takes one at a time)
    for (int a = 0; a < kArenas; ++a) {
      if (ar[a].sealed_group >= 0) {
        const Group& g = groups[static_cast<size_t>(ar[a].sealed_group)];
        b.issue_job(g, g.gen);
        // the oracle gets the same 128 world triangles, same riders
        for (const zt::MeshTri& t : g.tris)
          b.legacy_in.push_back({t, g.src_id, g.view, g.mat_a, g.mat_b, g.weight});
        replay_queue.push_back(ar[a].sealed_group);
        // NOT waited for here: the walk proceeds while the scheduler moves on
        // to fill the other arena of this view (see the hold check at open).
        ar[a].sealed_group = -1;
      }
    }
    // compare every fully landed group, in order
    while (!replay_queue.empty()) {
      const int gi = replay_queue.front();
      b.drain_shell_to(shell_base + kTrisPerGroup);
      b.drain_legacy_to(shell_base + kTrisPerGroup);
      b.compare_group(groups[static_cast<size_t>(gi)], shell_base, /*against_legacy=*/true);
      shell_base += kTrisPerGroup;
      replay_queue.pop_front();
      ++groups_done;
    }
  }
  check(groups_done == 64, "all 64 groups replayed and compared", 64, groups_done);
  check(b.mism_legacy == 0, "MAIN RUN: every replayed triangle bit-identical to zhao_terrain_project",
        0, b.mism_legacy);
  check(b.mism_zref == 0, "MAIN RUN: every replayed triangle (incl. w) equals project_vertex", 0,
        b.mism_zref);
  std::printf("main run: %d triangles compared against the legacy projector and project_vertex\n",
              b.compared);

  // behind-eye and rail coverage really happened (a fixture that never
  // reaches the branch proves nothing about it)
  {
    int behind_corners = 0, rail_corners = 0;
    for (size_t i = 0; i < shell_base; ++i) {
      const Tri& t = b.shell_out[i];
      for (int k = 0; k < 3; ++k) {
        if (t.behind & (1u << k)) ++behind_corners;
        if (t.x[k] == 524288 || t.x[k] == -524288 || t.y[k] == 524288 || t.y[k] == -524288)
          ++rail_corners;
      }
    }
    check(behind_corners > 0, "the fixture reached behind-the-eye corners", 1, behind_corners);
    check(rail_corners > 0, "the fixture reached the guard-band rails", 1, rail_corners);
    std::printf("coverage: %d behind-eye corners, %d rail-clamped corners of %zu\n", behind_corners,
                rail_corners, shell_base * 3);
  }

  // counters after the main run
  top->eval();
  check(top->replay_triangles_o == 64u * kTrisPerGroup, "replay_triangles_o counts every landed triangle",
        64u * kTrisPerGroup, top->replay_triangles_o);
  check(top->replay_refused_o == 0, "no refusals in the main run", 0, top->replay_refused_o);
  check(top->replay_missed_o == 0, "no misses in the main run", 0, top->replay_missed_o);
  check(top->corner_hits_o == 64u * kTrisPerGroup * 3, "corner_hits_o = 3 per triangle",
        64u * kTrisPerGroup * 3, top->corner_hits_o);
  check(top->corner_refusals_o == 0, "corner_refusals_o zero", 0, top->corner_refusals_o);
  check(top->corner_misses_o == 0, "corner_misses_o zero", 0, top->corner_misses_o);
  check(top->arena_overflow_o == 0, "overflow clear after the main run", 0, top->arena_overflow_o);
  check(top->arena_seal_short_o == 0, "seal_short clear after the main run", 0, top->arena_seal_short_o);
  check(top->jobs_done_o == 64, "walker completed 64 jobs", 64, top->jobs_done_o);
  check(top->b_grants_o == 64u * kDepth, "client B was granted exactly 64 x 81 vertices", 64u * kDepth,
        top->b_grants_o);
  check(top->contended_o > 0, "SEEN TO FIRE: contended_o under dual load", 1, top->contended_o);
  check(top->a_grants_o > 0, "client A got grants", 1, top->a_grants_o);
  check(top->mat_refused_o == 0, "mat_refused_o structurally zero at MATW=32", 0, top->mat_refused_o);
  std::printf("service: a_grants %u b_grants %u contended %u; client A results checked %d\n",
              top->a_grants_o, top->b_grants_o, top->contended_o, b.a_checked);
  b.a_enable = false;
  // let client A drain, then verify it never misrouted
  b.idle(80);
  check(b.a_inflight.empty(), "every client-A vertex came back", 0, b.a_inflight.size());
  check(b.a_bad() == 0, "client A: in order, payload intact, equal to project_vertex", 0, b.a_bad());

  // =========================================================================
  // 2. STALE HANDLES: gen-1 on a sealed arena, an old gen after reopen, and an
  //    open-but-unsealed arena. Every triangle refused, payload zero.
  // =========================================================================
  {
    const size_t before = b.shell_out.size();
    // The last group filled into arena 3 is still sealed in hardware.
    int gi = -1;
    for (int i = 63; i >= 0; --i)
      if (groups[static_cast<size_t>(i)].arena == 3) {
        gi = i;
        break;
      }
    const Group& g = groups[static_cast<size_t>(gi)];
    b.legacy_feed = false;
    b.issue_job(g, g.gen - 1);
    b.drain_shell_to(before + kTrisPerGroup);
    int refused = 0, zero = 0;
    for (size_t i = before; i < before + kTrisPerGroup; ++i) {
      const Tri& t = b.shell_out[i];
      if (t.refused) ++refused;
      bool z = t.behind == 0 && !t.missed;
      for (int k = 0; k < 3; ++k) z = z && t.x[k] == 0 && t.y[k] == 0 && t.d[k] == 0 && t.w[k] == 0;
      if (z) ++zero;
    }
    check(refused == kTrisPerGroup, "stale gen-1: every triangle REFUSED", kTrisPerGroup, refused);
    check(zero == kTrisPerGroup, "stale gen-1: every refused triangle carries ZEROS", kTrisPerGroup, zero);
    top->eval();
    check(top->replay_refused_o == static_cast<uint32_t>(kTrisPerGroup),
          "SEEN TO FIRE: replay_refused_o", kTrisPerGroup, top->replay_refused_o);
    check(top->corner_refusals_o == static_cast<uint32_t>(3 * kTrisPerGroup),
          "SEEN TO FIRE: corner_refusals_o = 3 per refused triangle", 3 * kTrisPerGroup,
          top->corner_refusals_o);

    // reopen arena 3: the CURRENT gen is now stale and the arena is unsealed
    const uint32_t newgen = b.open(3);
    const size_t before2 = b.shell_out.size();
    b.issue_job(g, g.gen);  // the old handle after a reopen
    b.drain_shell_to(before2 + kTrisPerGroup);
    int refused2 = 0;
    for (size_t i = before2; i < before2 + kTrisPerGroup; ++i)
      if (b.shell_out[i].refused) ++refused2;
    check(refused2 == kTrisPerGroup, "old handle after reopen: every triangle REFUSED", kTrisPerGroup,
          refused2);
    const size_t before3 = b.shell_out.size();
    Group gu = g;
    b.issue_job(gu, newgen);  // right gen, but the arena is not sealed
    b.drain_shell_to(before3 + kTrisPerGroup);
    int refused3 = 0;
    for (size_t i = before3; i < before3 + kTrisPerGroup; ++i)
      if (b.shell_out[i].refused) ++refused3;
    check(refused3 == kTrisPerGroup, "unsealed arena: every triangle REFUSED", kTrisPerGroup, refused3);
    check(top->replay_missed_o == 0, "refusals are refusals, never misses", 0, top->replay_missed_o);
  }

  // =========================================================================
  // 3. THE DENSE SEAL LAW (or, at VALID_MODE=0, the miss path)
  // =========================================================================
  {
    const Group& g = groups[5];  // view 0, top, a morphing column
    Group gl = g;
    gl.arena = 0;
    gl.gen = b.open(0);
    for (int k = 0; k < kDepth - 1; ++k) {
      const int vi = g.job.ox + k % 9, vj = g.job.oz + k / 9;
      b.fill_vertex(0, k, zt::detail::vertex_at(lat, g.job, vi, vj, g.job.morph, nullptr), g.view);
    }
    b.wait_fills();
    b.seal(0);  // 80 of 81
    top->eval();
    if (kDense) {
      check(top->arena_seal_short_o == 1, "SEEN TO FIRE: dense short seal refused, sticky", 1,
            top->arena_seal_short_o);
      const size_t before = b.shell_out.size();
      b.issue_job(gl, gl.gen);
      b.drain_shell_to(before + kTrisPerGroup);
      int refused = 0;
      for (size_t i = before; i < before + kTrisPerGroup; ++i)
        if (b.shell_out[i].refused) ++refused;
      check(refused == kTrisPerGroup, "dense: a short-sealed arena is UNSEALED and refuses every triangle",
            kTrisPerGroup, refused);
      // complete it, seal, replay: identical to the oracle
      const int vi = g.job.ox + 8, vj = g.job.oz + 8;
      b.fill_vertex(0, 80, zt::detail::vertex_at(lat, g.job, vi, vj, g.job.morph, nullptr), g.view);
      b.wait_fills();
      b.seal(0);
      const size_t before2 = b.shell_out.size();
      b.issue_job(gl, gl.gen);
      b.drain_shell_to(before2 + kTrisPerGroup);
      const int mz = b.mism_zref;
      b.compare_group(gl, before2, /*against_legacy=*/false);
      check(b.mism_zref == mz, "dense: completed after a refused seal, replay equals project_vertex", mz,
            b.mism_zref);

      // a MISORDERED fill is dropped, sticky, and advances nothing
      Group gm = groups[9];
      gm.arena = 1;
      gm.gen = b.open(1);
      {
        const int vi5 = gm.job.ox + 5, vj5 = gm.job.oz;
        b.fill_vertex(1, 5, zt::detail::vertex_at(lat, gm.job, vi5, vj5, gm.job.morph, nullptr), gm.view);
        b.wait_fills();
      }
      top->eval();
      check(top->arena_overflow_o == 1, "SEEN TO FIRE: misordered fill dropped, overflow sticky", 1,
            top->arena_overflow_o);
      for (int k = 0; k < kDepth; ++k) {
        const int vi = gm.job.ox + k % 9, vj = gm.job.oz + k / 9;
        b.fill_vertex(1, k, zt::detail::vertex_at(lat, gm.job, vi, vj, gm.job.morph, nullptr), gm.view);
      }
      b.wait_fills();
      b.seal(1);
      const size_t before3 = b.shell_out.size();
      b.issue_job(gm, gm.gen);
      b.drain_shell_to(before3 + kTrisPerGroup);
      const int mz2 = b.mism_zref;
      b.compare_group(gm, before3, /*against_legacy=*/false);
      check(b.mism_zref == mz2, "dense: the early misorder left no trace, replay equals project_vertex",
            mz2, b.mism_zref);
    } else {
      // BITMAP: the short seal is ACCEPTED, and the two triangles of cell (7,7)
      // -- the only ones referencing index 80 -- MISS. The miss path is SEEN.
      check(top->arena_seal_short_o == 0, "bitmap: seal_short is structurally zero", 0,
            top->arena_seal_short_o);
      const size_t before = b.shell_out.size();
      b.issue_job(gl, gl.gen);
      b.drain_shell_to(before + kTrisPerGroup);
      int missed = 0, refused = 0, missed_last_two = 0;
      for (size_t i = before; i < before + kTrisPerGroup; ++i) {
        if (b.shell_out[i].missed) ++missed;
        if (b.shell_out[i].refused) ++refused;
      }
      if (b.shell_out[before + 126].missed) ++missed_last_two;
      if (b.shell_out[before + 127].missed) ++missed_last_two;
      check(missed == 2, "SEEN TO FIRE (bitmap): exactly the two triangles touching row 80 MISS", 2, missed);
      check(missed_last_two == 2, "bitmap: the misses are cell (7,7)'s two triangles", 2, missed_last_two);
      check(refused == 0, "bitmap: a miss is not a refusal", 0, refused);
      top->eval();
      check(top->replay_missed_o == 2, "SEEN TO FIRE (bitmap): replay_missed_o", 2, top->replay_missed_o);
      check(top->corner_misses_o == 2, "SEEN TO FIRE (bitmap): corner_misses_o (one corner each)", 2,
            top->corner_misses_o);
      // the 126 complete triangles are still exact
      int exact = 0;
      for (int i = 0; i < 126; ++i) {
        const Tri& s = b.shell_out[before + static_cast<size_t>(i)];
        const Tri z = zref_tri(gl.tris[static_cast<size_t>(i)], gl.m, gl.vp);
        bool ok = s.behind == z.behind;
        for (int k = 0; k < 3; ++k)
          ok = ok && s.x[k] == z.x[k] && s.y[k] == z.y[k] && s.d[k] == z.d[k] && s.w[k] == z.w[k];
        if (ok) ++exact;
      }
      check(exact == 126, "bitmap: the 126 complete triangles equal project_vertex", 126, exact);
      // the missed corner carries zeros
      const Tri& t126 = b.shell_out[before + 126];
      check(t126.x[1] == 0 && t126.y[1] == 0 && t126.d[1] == 0 && t126.w[1] == 0,
            "bitmap: a missed corner is ZEROED (tri 126, corner B = i11 = 80)", 1, 1);
    }
  }

  // =========================================================================
  // 4. MID-FRAME RECONFIGURATION
  // =========================================================================
  {
    // (a) records freeze at fill time. Fill G under M0 (view 0), seal, then
    //     move the SERVICE's view-0 matrix to M1's shape while the oracle
    //     stays at M0: the replay must equal the fill-time law.
    Group g = groups[2];  // view 0, top
    g.arena = 2;
    g.gen = b.open(2);
    for (int k = 0; k < kDepth; ++k) {
      const int vi = g.job.ox + k % 9, vj = g.job.oz + k / 9;
      b.fill_vertex(2, k, zt::detail::vertex_at(lat, g.job, vi, vj, g.job.morph, nullptr), g.view);
    }
    b.wait_fills();
    b.seal(2);
    const int32_t m2[16] = {2 * kOne, 0, 0, kOne / 8, 0, 2 * kOne, 0, -kOne / 8, 0, 0, kOne, 0, 0, 0,
                            kOne, kOne / 2};
    const zref::mat4fx M2 = mat_of(m2);
    b.write_matrix_only(/*svc=*/true, /*orc=*/false, 0, M2);
    b.idle(40);
    const size_t before = b.shell_out.size();
    const size_t lbase = b.legacy_out.size();  // the legacy stream has its own base
    b.legacy_feed = true;
    b.issue_job(g, g.gen);
    for (const zt::MeshTri& t : g.tris) b.legacy_in.push_back({t, g.src_id, g.view, g.mat_a, g.mat_b, g.weight});
    b.drain_shell_to(before + kTrisPerGroup);
    b.drain_legacy_to(lbase + kTrisPerGroup);
    {
      int same = 0;
      for (int i = 0; i < kTrisPerGroup; ++i) {
        const Tri& s = b.shell_out[before + static_cast<size_t>(i)];
        const Tri& l = b.legacy_out[lbase + static_cast<size_t>(i)];
        bool ok = s.behind == l.behind && !s.refused;
        for (int k = 0; k < 3; ++k) ok = ok && s.x[k] == l.x[k] && s.y[k] == l.y[k] && s.d[k] == l.d[k];
        if (ok) ++same;
      }
      check(same == kTrisPerGroup,
            "reconfig (a): a group sealed under M0 replays the M0 law after the service moved to M2",
            kTrisPerGroup, same);
    }
    // and the same group re-filled under M2 equals the oracle at M2
    b.write_matrix_only(/*svc=*/false, /*orc=*/true, 0, M2);
    g.m = M2;
    g.gen = b.open(2);
    for (int k = 0; k < kDepth; ++k) {
      const int vi = g.job.ox + k % 9, vj = g.job.oz + k / 9;
      b.fill_vertex(2, k, zt::detail::vertex_at(lat, g.job, vi, vj, g.job.morph, nullptr), g.view);
    }
    b.wait_fills();
    b.seal(2);
    const size_t before2 = b.shell_out.size();
    b.issue_job(g, g.gen);
    for (const zt::MeshTri& t : g.tris) b.legacy_in.push_back({t, g.src_id, g.view, g.mat_a, g.mat_b, g.weight});
    b.drain_shell_to(before2 + kTrisPerGroup);
    b.drain_legacy_to(lbase + 2 * kTrisPerGroup);
    {
      int same = 0, mz = b.mism_zref;
      for (int i = 0; i < kTrisPerGroup; ++i) {
        const Tri& s = b.shell_out[before2 + static_cast<size_t>(i)];
        const Tri& l = b.legacy_out[lbase + kTrisPerGroup + static_cast<size_t>(i)];
        bool ok = s.behind == l.behind && !s.refused;
        for (int k = 0; k < 3; ++k) ok = ok && s.x[k] == l.x[k] && s.y[k] == l.y[k] && s.d[k] == l.d[k];
        if (ok) ++same;
      }
      check(same == kTrisPerGroup, "reconfig (a): re-filled under M2, replay equals the legacy at M2",
            kTrisPerGroup, same);
      b.compare_group(g, before2, /*against_legacy=*/false);
      check(b.mism_zref == mz, "reconfig (a): re-filled under M2, replay equals project_vertex at M2", mz,
            b.mism_zref);
    }
    b.legacy_feed = false;

    // (b) a fill TORN by a matrix write: vertices 0..40 accepted under M2,
    //     41..80 under M0. Each replayed corner equals project_vertex under
    //     the matrix in force at ITS accept edge. The arena is faithful to
    //     the core; the epoch discipline is the producer's.
    Group gt = groups[6];  // view 0, top, the morphing column
    gt.arena = 3;
    gt.gen = b.open(3);
    zt::detail::TessVert verts[kDepth];
    for (int k = 0; k < kDepth; ++k) {
      const int vi = gt.job.ox + k % 9, vj = gt.job.oz + k / 9;
      verts[k] = zt::detail::vertex_at(lat, gt.job, vi, vj, gt.job.morph, nullptr);
      b.fill_vertex(3, k, verts[k], gt.view);
      if (k == 40) b.write_matrix_only(/*svc=*/true, /*orc=*/false, 0, M0);
    }
    b.wait_fills();
    b.seal(3);
    const size_t before3 = b.shell_out.size();
    b.issue_job(gt, gt.gen);
    b.drain_shell_to(before3 + kTrisPerGroup);
    // expected per corner: walk the topology the way the walker does
    int corners_ok = 0, corners_total = 0, epoch_changed = 0;
    for (int i = 0; i < kTrisPerGroup; ++i) {
      const int cell = i / 2, t = i & 1, a = cell % 8, bb = cell / 8;
      const int i00 = bb * 9 + a, i10 = i00 + 1, i01 = i00 + 9, i11 = i00 + 10;
      const int idx[3] = {i00, t ? i01 : i11, t ? i11 : i10};  // top winding
      const Tri& s = b.shell_out[before3 + static_cast<size_t>(i)];
      for (int k = 0; k < 3; ++k) {
        const zref::mat4fx& m = (idx[k] <= 40) ? M2 : M0;
        const zref::render::ProjOut p = zref::render::project_vertex(
            m, VP0, zref::fx16{verts[idx[k]].x}, zref::fx16{verts[idx[k]].y},
            zref::fx16{verts[idx[k]].z}, nullptr);
        const zref::render::ProjOut q = zref::render::project_vertex(
            M0, VP0, zref::fx16{verts[idx[k]].x}, zref::fx16{verts[idx[k]].y},
            zref::fx16{verts[idx[k]].z}, nullptr);
        if (p.s.x != q.s.x || p.s.y != q.s.y || p.s.d != q.s.d) ++epoch_changed;
        ++corners_total;
        const bool ok = s.x[k] == p.s.x && s.y[k] == p.s.y && s.d[k] == p.s.d &&
                        ((s.behind >> k) & 1u) == (p.in ? 0u : 1u) &&
                        s.w[k] == (static_cast<uint32_t>(p.w) & 0x7FFFFFFFu);
        if (ok) ++corners_ok;
      }
    }
    check(corners_ok == corners_total,
          "reconfig (b): a torn fill is faithful per corner to the matrix at its accept edge",
          corners_total, corners_ok);
    check(epoch_changed > 0, "reconfig (b): the tear actually changed some corners", 1, epoch_changed);
  }

  // =========================================================================
  // 5. POSITIVE CONTROL: skew ONE raw LSB of one matrix word in the service
  //    only. The comparator must fire on the view-1 group that follows.
  // =========================================================================
  {
    b.configure(true, true, 0, M0, VP0);  // restore
    zref::mat4fx skew = M1;
    skew.m[0][0] = zref::fx16{M1.m[0][0].raw + 1};
    b.write_matrix_only(/*svc=*/true, /*orc=*/false, 1, skew);
    Group g = groups[40];  // view 1, top
    g.arena = 0;
    g.gen = b.open(0);
    for (int k = 0; k < kDepth; ++k) {
      const int vi = g.job.ox + k % 9, vj = g.job.oz + k / 9;
      b.fill_vertex(0, k, zt::detail::vertex_at(lat, g.job, vi, vj, g.job.morph, nullptr), g.view);
    }
    b.wait_fills();
    b.seal(0);
    const size_t before = b.shell_out.size();
    b.issue_job(g, g.gen);
    b.drain_shell_to(before + kTrisPerGroup);
    const int mz = b.mism_zref, printed = b.printed_zref;
    b.printed_zref = kMaxPrinted;  // this family is EXPECTED to fire; do not print it
    b.compare_group(g, before, /*against_legacy=*/false);
    const int fired = b.mism_zref - mz;
    b.mism_zref = mz;  // the control's mismatches are not defects
    b.printed_zref = printed;
    check(fired > 0, "POSITIVE CONTROL: one raw LSB in one service matrix word makes the comparator fire",
          1, fired);
    std::printf("positive control: %d of %d triangles differed under a one-LSB matrix skew\n", fired,
                kTrisPerGroup);
    b.write_matrix_only(true, false, 1, M1);  // restore
  }

  // =========================================================================
  // 6. throughput, measured: one sealed group replays in ~128 clocks with the
  //    consumer always ready, i.e. one triangle per clock.
  // =========================================================================
  {
    b.stall_pct = 0;
    Group g = groups[3];
    g.arena = 1;
    g.gen = b.open(1);
    for (int k = 0; k < kDepth; ++k) {
      const int vi = g.job.ox + k % 9, vj = g.job.oz + k / 9;
      b.fill_vertex(1, k, zt::detail::vertex_at(lat, g.job, vi, vj, g.job.morph, nullptr), g.view);
    }
    b.wait_fills();
    b.seal(1);
    const size_t before = b.shell_out.size();
    const uint64_t c0 = b.cycle;
    b.issue_job(g, g.gen);
    b.drain_shell_to(before + kTrisPerGroup);
    const uint64_t took = b.cycle - c0;
    check(took <= kTrisPerGroup + 4, "MEASURED: 128 triangles replay in <= 132 clocks (one per clock)",
          kTrisPerGroup + 4, took);
    std::printf("throughput: 128 triangles in %llu clocks with the consumer always ready\n",
                static_cast<unsigned long long>(took));
    const int mz = b.mism_zref;
    b.compare_group(g, before, false);
    check(b.mism_zref == mz, "throughput run: still exact", mz, b.mism_zref);
    b.idle(4);
    top->eval();
    check(top->shell_idle_o == 1, "shell idle after the drain", 1, top->shell_idle_o);
  }

  delete top;
  return zhao::report_and_exit(kDense ? "terrain_wcache_differential"
                                      : "terrain_wcache_differential[bitmap]");
}
