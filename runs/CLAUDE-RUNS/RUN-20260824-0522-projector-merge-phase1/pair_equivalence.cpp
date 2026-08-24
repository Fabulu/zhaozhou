// pair_equivalence.cpp -- RUN-20260824-0522, step 2.
//
// DRIVES BOTH SHIPPED PROJECTORS FROM ONE STIMULUS STREAM AND COMPARES EVERY
// PROJECTED VERTEX, BEFORE ANY RTL IS WRITTEN.
//
// The budget audit reports that `zhao_geom_project` and `zhao_terrain_project`
// have "byte-identical arithmetic signatures". That is a statement about the
// SHAPE of the arithmetic -- 11 nonconstant multiplies, widest operand 32 bits,
// 33 DSPs each. It is not a statement about behaviour, and merging two blocks
// on the strength of it would be merging them on a coincidence of census
// counters. This file is the behavioural claim.
//
// WHAT "EQUIVALENT" CAN AND CANNOT MEAN HERE
// -----------------------------------------
// The two blocks do NOT have the same interface, so a cycle-by-cycle port
// comparison is not defined between them:
//
//   * GEOM.PROJECT takes ONE VERTEX per handshake and emits one vertex packet.
//     Contract latency: fixed 36.
//   * TERRAIN.PROJECT takes ONE TRIANGLE per handshake, walks its three
//     corners through one projector on three consecutive clocks, and emits one
//     triangle packet. Contract latency: fixed 38 -- two more stages, because
//     it has a vertex sequencer in front and a reassembly register behind.
//
// So the equivalence under test is the one that actually matters for a merge:
// **for identical (matrix, viewport, view, x, y, z), do the two blocks produce
// the identical projected vertex?** That is compared here on the full packet --
// canvas x, canvas y, the Q16.16 1/w word, and the behind-the-eye flag -- for
// every vertex of every triangle, in order, with no tolerance.
//
// Timing and ordering are NOT compared between the two blocks, because they are
// legitimately different and the merge must PRESERVE that difference rather
// than reconcile it. Timing is held by the per-caller cycle-exact regression
// (caller_regression.cpp), which compares each block against ITS OWN pre-change
// self.
//
// THE THIRD LEG
// -------------
// Both are also compared against `zref::render::project_vertex`, the shipped
// oracle both cite. Two blocks agreeing with each other and both being wrong is
// a real failure mode; the oracle is what rules it out. All three must agree.
//
// FAILURE 1 OF RUN-20260824-0317, INHERITED
// -----------------------------------------
// That run's differential reported 624 mismatches that were entirely its own:
// the stimulus generator was called INSIDE the lambda that drove both models,
// so the RNG advanced twice per cycle and the two DUTs received DIFFERENT
// stimulus. Every "mismatch" was the harness comparing two experiments. Here
// the stimulus is generated ONCE into a `std::vector<Tri>` before either DUT is
// touched, and both DUTs are driven from that same vector. There is no path by
// which the two can be fed differently.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_geom_project.h"
#include "Vzhao_terrain_project.h"

#include "zref/zref_fixp.hpp"
#include "zrender/internal.hpp"  // white-box: project_vertex IS the law

namespace zr = zref::render;

namespace {

constexpr int32_t kOne = 1 << 16;

int g_fail = 0;
int g_cmp = 0;

// Sign-extend a 21-bit canvas coordinate. Verilator hands a `logic signed
// [20:0]` port back as a raw 21-bit word; a plain cast reads -524288 as
// 1572864 and every guard-band rail looks like a mismatch.
int32_t sx21(uint32_t v) { return static_cast<int32_t>(v << 11) >> 11; }

template <class T>
void tick(T& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
  d.clk = 0;
  d.eval();
}

// ------------------------------------------------------------- stimulus --
struct Tri {
  int32_t x[3], y[3], z[3];
  uint16_t src;
  uint8_t view;
  uint8_t mat_a, mat_b, weight;
};

// One projected vertex, the common shape of both blocks' per-vertex answer.
struct Vtx {
  int32_t x, y, d;
  uint8_t behind;
};

bool same(const Vtx& a, const Vtx& b) {
  return a.x == b.x && a.y == b.y && a.d == b.d && a.behind == b.behind;
}

// xorshift64*, so the stream is reproducible and does not depend on the host
// standard library.
uint64_t g_s = 0x9E3779B97F4A7C15ull;
uint64_t rnd() {
  g_s ^= g_s >> 12;
  g_s ^= g_s << 25;
  g_s ^= g_s >> 27;
  return g_s * 0x2545F4914F6CDD1Dull;
}
int32_t rnd32() { return static_cast<int32_t>(rnd() >> 32); }

// A world coordinate with a realistic magnitude, but with the tails kept:
// most vertices near the origin in fx16, some at the extremes, so the row-sum
// saturation and the guard-band clamp are both exercised.
int32_t rnd_world() {
  const uint32_t k = static_cast<uint32_t>(rnd() >> 59);  // 0..31
  if (k == 0) return 0x7FFFFFFF;
  if (k == 1) return static_cast<int32_t>(0x80000000u);
  if (k == 2) return rnd32();  // full range: forces row-sum saturation
  if (k < 8) return rnd32() >> 8;
  return (rnd32() >> 14) * (kOne / 256);  // ~ +-8192.0 in fx16
}

// ------------------------------------------------------------ the models --
zref::mat4fx persp(int32_t kx, int32_t ky, int32_t zt, int32_t wz) {
  zref::mat4fx m{};
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) m.m[r][c] = zref::fx16{0};
  m.m[0][0] = zref::fx16{kx};
  m.m[1][1] = zref::fx16{ky};
  m.m[2][2] = zref::fx16{zt};  // inert: row 2 is never read
  m.m[2][3] = zref::fx16{zt};
  m.m[3][2] = zref::fx16{wz};
  m.m[3][3] = zref::fx16{kOne};
  return m;
}

struct Cfg {
  zref::mat4fx m;
  zr::Viewport vp;
};

Vtx oracle_vtx(const Cfg& c, int32_t x, int32_t y, int32_t z) {
  const zr::ProjOut p =
      zr::project_vertex(c.m, c.vp, zref::fx16{x}, zref::fx16{y}, zref::fx16{z}, nullptr);
  Vtx v;
  v.x = p.s.x;
  v.y = p.s.y;
  v.d = p.s.d;
  v.behind = p.in ? 0 : 1;
  return v;
}

// ------------------------------------------------------------ the drivers --
class GeomDut {
 public:
  explicit GeomDut(Vzhao_geom_project& d) : d_(d) {}

  void reset() {
    d_.rst_n = 0;
    d_.cfg_we_i = 0;
    d_.v_valid_i = 0;
    d_.out_ready_i = 0;
    d_.eval();
    for (int i = 0; i < 3; ++i) tick(d_);
    d_.rst_n = 1;
    d_.eval();
  }

  void write_cfg(int view, uint8_t addr, uint32_t data) {
    d_.cfg_we_i = 1;
    d_.cfg_view_i = static_cast<uint8_t>(view);
    d_.cfg_addr_i = addr;
    d_.cfg_data_i = data;
    d_.eval();
    tick(d_);
    d_.cfg_we_i = 0;
    d_.eval();
  }

  void configure(int view, const Cfg& c) {
    for (int r = 0; r < 4; ++r)
      for (int k = 0; k < 4; ++k)
        write_cfg(view, static_cast<uint8_t>(r * 4 + k), static_cast<uint32_t>(c.m.m[r][k].raw));
    write_cfg(view, 16, (static_cast<uint32_t>(c.vp.y0) << 16) | (c.vp.x0 & 0xFFFFu));
    write_cfg(view, 17, (static_cast<uint32_t>(c.vp.h) << 16) | (c.vp.w & 0xFFFFu));
  }

  // Push every triangle's three corners as three separate vertices, in order.
  std::vector<Vtx> run(const std::vector<Tri>& tris, uint32_t stall_mask) {
    std::vector<Vtx> out;
    const size_t want = tris.size() * 3;
    out.reserve(want);
    size_t pushed = 0;
    uint32_t mask = stall_mask;
    const long limit = static_cast<long>(want) * 128 + 16384;

    for (long cyc = 0; cyc < limit && out.size() < want; ++cyc) {
      const bool ready = (stall_mask == 0) || ((mask & 1u) == 0);
      mask = (mask >> 1) | (mask << 31);
      d_.out_ready_i = ready ? 1 : 0;

      if (pushed < want) {
        const Tri& t = tris[pushed / 3];
        const int k = static_cast<int>(pushed % 3);
        d_.v_valid_i = 1;
        d_.vx_i = static_cast<uint32_t>(t.x[k]);
        d_.vy_i = static_cast<uint32_t>(t.y[k]);
        d_.vz_i = static_cast<uint32_t>(t.z[k]);
        d_.view_i = t.view;
        d_.src_id_i = t.src;
      } else {
        d_.v_valid_i = 0;
      }
      d_.eval();

      if (d_.out_valid_o && ready) {
        Vtx v;
        v.x = sx21(d_.out_x_o);
        v.y = sx21(d_.out_y_o);
        v.d = static_cast<int32_t>(d_.out_d_o);
        v.behind = static_cast<uint8_t>(d_.out_behind_o);
        out.push_back(v);
      }
      if (d_.v_valid_i && d_.v_ready_o) ++pushed;
      tick(d_);
    }
    return out;
  }

 private:
  Vzhao_geom_project& d_;
};

class TerrDut {
 public:
  explicit TerrDut(Vzhao_terrain_project& d) : d_(d) {}

  void reset() {
    d_.rst_n = 0;
    d_.cfg_we_i = 0;
    d_.tri_valid_i = 0;
    d_.out_ready_i = 0;
    d_.eval();
    for (int i = 0; i < 3; ++i) tick(d_);
    d_.rst_n = 1;
    d_.eval();
  }

  void write_cfg(int view, uint8_t addr, uint32_t data) {
    d_.cfg_we_i = 1;
    d_.cfg_view_i = static_cast<uint8_t>(view);
    d_.cfg_addr_i = addr;
    d_.cfg_data_i = data;
    d_.eval();
    tick(d_);
    d_.cfg_we_i = 0;
    d_.eval();
  }

  void configure(int view, const Cfg& c) {
    for (int r = 0; r < 4; ++r)
      for (int k = 0; k < 4; ++k)
        write_cfg(view, static_cast<uint8_t>(r * 4 + k), static_cast<uint32_t>(c.m.m[r][k].raw));
    write_cfg(view, 16, (static_cast<uint32_t>(c.vp.y0) << 16) | (c.vp.x0 & 0xFFFFu));
    write_cfg(view, 17, (static_cast<uint32_t>(c.vp.h) << 16) | (c.vp.w & 0xFFFFu));
  }

  // Emits three Vtx per triangle, unpacked in corner order A, B, C, so the
  // result vector lines up index-for-index with GeomDut::run's.
  std::vector<Vtx> run(const std::vector<Tri>& tris, uint32_t stall_mask) {
    std::vector<Vtx> out;
    out.reserve(tris.size() * 3);
    size_t pushed = 0;
    uint32_t mask = stall_mask;
    const long limit = static_cast<long>(tris.size()) * 384 + 16384;

    for (long cyc = 0; cyc < limit && out.size() < tris.size() * 3; ++cyc) {
      const bool ready = (stall_mask == 0) || ((mask & 1u) == 0);
      mask = (mask >> 1) | (mask << 31);
      d_.out_ready_i = ready ? 1 : 0;

      if (pushed < tris.size()) {
        const Tri& t = tris[pushed];
        d_.tri_valid_i = 1;
        d_.ax_i = static_cast<uint32_t>(t.x[0]);
        d_.ay_i = static_cast<uint32_t>(t.y[0]);
        d_.az_i = static_cast<uint32_t>(t.z[0]);
        d_.bx_i = static_cast<uint32_t>(t.x[1]);
        d_.by_i = static_cast<uint32_t>(t.y[1]);
        d_.bz_i = static_cast<uint32_t>(t.z[1]);
        d_.cx_i = static_cast<uint32_t>(t.x[2]);
        d_.cy_i = static_cast<uint32_t>(t.y[2]);
        d_.cz_i = static_cast<uint32_t>(t.z[2]);
        d_.src_id_i = t.src;
        d_.view_i = t.view;
        d_.mat_a_i = t.mat_a;
        d_.mat_b_i = t.mat_b;
        d_.weight_i = t.weight;
      } else {
        d_.tri_valid_i = 0;
      }
      d_.eval();

      if (d_.out_valid_o && ready) {
        const uint8_t bh = static_cast<uint8_t>(d_.out_behind_o);
        Vtx a{sx21(d_.out_ax_o), sx21(d_.out_ay_o), static_cast<int32_t>(d_.out_ad_o),
              static_cast<uint8_t>(bh & 1u)};
        Vtx b{sx21(d_.out_bx_o), sx21(d_.out_by_o), static_cast<int32_t>(d_.out_bd_o),
              static_cast<uint8_t>((bh >> 1) & 1u)};
        Vtx c{sx21(d_.out_cx_o), sx21(d_.out_cy_o), static_cast<int32_t>(d_.out_cd_o),
              static_cast<uint8_t>((bh >> 2) & 1u)};
        out.push_back(a);
        out.push_back(b);
        out.push_back(c);
      }
      if (d_.tri_valid_i && d_.tri_ready_o) ++pushed;
      tick(d_);
    }
    return out;
  }

 private:
  Vzhao_terrain_project& d_;
};

// --------------------------------------------------------------- compare --
void report(const char* phase, size_t i, const char* field, int64_t g, int64_t t, int64_t o) {
  if (g_fail < 24) {
    std::printf("  MISMATCH %s vertex %zu %s: geom=%lld terrain=%lld oracle=%lld\n", phase, i,
                field, static_cast<long long>(g), static_cast<long long>(t),
                static_cast<long long>(o));
  }
  ++g_fail;
}

void compare(const char* phase, const std::vector<Tri>& tris, const std::vector<Vtx>& gv,
             const std::vector<Vtx>& tv, const Cfg cfg[2]) {
  if (gv.size() != tris.size() * 3 || tv.size() != tris.size() * 3) {
    std::printf("  *** %s: SHORT RUN geom=%zu terrain=%zu expected=%zu (a block stalled out)\n",
                phase, gv.size(), tv.size(), tris.size() * 3);
    ++g_fail;
    return;
  }
  for (size_t i = 0; i < gv.size(); ++i) {
    const Tri& t = tris[i / 3];
    const int k = static_cast<int>(i % 3);
    const Vtx o = oracle_vtx(cfg[t.view], t.x[k], t.y[k], t.z[k]);
    ++g_cmp;
    if (!same(gv[i], tv[i]) || !same(gv[i], o)) {
      if (gv[i].x != tv[i].x || gv[i].x != o.x) report(phase, i, "x", gv[i].x, tv[i].x, o.x);
      if (gv[i].y != tv[i].y || gv[i].y != o.y) report(phase, i, "y", gv[i].y, tv[i].y, o.y);
      if (gv[i].d != tv[i].d || gv[i].d != o.d) report(phase, i, "d", gv[i].d, tv[i].d, o.d);
      if (gv[i].behind != tv[i].behind || gv[i].behind != o.behind)
        report(phase, i, "behind", gv[i].behind, tv[i].behind, o.behind);
    }
  }
}

// Run an EXPLICIT triangle list, for the cases a random stream cannot be
// trusted to contain.
void phase_explicit(const char* name, GeomDut& g, TerrDut& t, const Cfg cfg[2],
                    const std::vector<Tri>& tris, uint32_t stall_mask) {
  g.reset();
  t.reset();
  for (int v = 0; v < 2; ++v) {
    g.configure(v, cfg[v]);
    t.configure(v, cfg[v]);
  }
  const std::vector<Vtx> gv = g.run(tris, stall_mask);
  const std::vector<Vtx> tv = t.run(tris, stall_mask);
  const int before = g_fail;
  compare(name, tris, gv, tv, cfg);
  std::printf("  %-34s %6zu vertices   %s\n", name, tris.size() * 3,
              g_fail == before ? "identical" : "*** DIFFER ***");
}

// A phase: configure both views on both DUTs, build a triangle stream, run it
// through both, compare every vertex against the other block and the oracle.
void phase(const char* name, GeomDut& g, TerrDut& t, const Cfg cfg[2], size_t ntri,
           uint32_t stall_mask, bool reset_first) {
  if (reset_first) {
    g.reset();
    t.reset();
  }
  for (int v = 0; v < 2; ++v) {
    g.configure(v, cfg[v]);
    t.configure(v, cfg[v]);
  }

  std::vector<Tri> tris(ntri);
  for (size_t i = 0; i < ntri; ++i) {
    Tri& x = tris[i];
    for (int k = 0; k < 3; ++k) {
      x.x[k] = rnd_world();
      x.y[k] = rnd_world();
      x.z[k] = rnd_world();
    }
    x.src = static_cast<uint16_t>(rnd() >> 48);
    x.view = static_cast<uint8_t>((rnd() >> 40) & 1u);
    x.mat_a = static_cast<uint8_t>(rnd() >> 56);
    x.mat_b = static_cast<uint8_t>(rnd() >> 56);
    x.weight = static_cast<uint8_t>(rnd() >> 56);
  }

  const std::vector<Vtx> gv = g.run(tris, stall_mask);
  const std::vector<Vtx> tv = t.run(tris, stall_mask);
  const int before = g_fail;
  compare(name, tris, gv, tv, cfg);
  std::printf("  %-34s %6zu vertices   %s\n", name, tris.size() * 3,
              g_fail == before ? "identical" : "*** DIFFER ***");
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  Vzhao_geom_project gdut;
  Vzhao_terrain_project tdut;
  GeomDut g(gdut);
  TerrDut t(tdut);

  std::printf("pair_equivalence: zhao_geom_project vs zhao_terrain_project vs project_vertex\n");

  // A normal camera pair: two views with genuinely different matrices and
  // genuinely different viewports, because "the dual view is two register
  // sets" is law 5 on one side and law B on the other, and a merge that
  // crossed the two views would still pass a single-view test.
  Cfg duo[2];
  duo[0].m = persp(kOne * 2, kOne * 2, kOne / 2, kOne);
  duo[0].vp = zr::Viewport{0, 0, 256, 192};
  duo[1].m = persp(-kOne * 3, kOne / 3, kOne, -kOne / 2);
  duo[1].vp = zr::Viewport{256, 0, 256, 192};

  // Deliberately asymmetric: x0/y0 nonzero on both, odd widths, so the
  // (x0 + w/2) centre term and the w*2^15 half-extent differ per view and a
  // swapped viewport cannot pass by symmetry.
  Cfg odd[2];
  odd[0].m = persp(kOne + 7, kOne - 11, kOne, kOne / 4);
  odd[0].vp = zr::Viewport{17, 33, 255, 191};
  odd[1].m = persp(kOne * 5, kOne * 7, kOne, kOne * 2);
  odd[1].vp = zr::Viewport{1000, 700, 4095, 4095};

  // A matrix whose w row is dominated by the constant term, so clip.w is
  // frequently <= 0 and the near-plane branch is taken on a large fraction of
  // vertices rather than on a handful.
  Cfg behind[2];
  behind[0].m = persp(kOne, kOne, kOne, kOne * 64);
  behind[0].m.m[3][3] = zref::fx16{-kOne * 1024};
  behind[0].vp = zr::Viewport{0, 0, 320, 240};
  behind[1].m = persp(kOne, kOne, kOne, -kOne * 64);
  behind[1].m.m[3][3] = zref::fx16{-kOne * 4096};
  behind[1].vp = zr::Viewport{4, 4, 64, 64};

  // Huge half-extents and huge matrices: drives to_screen_xy onto BOTH rails
  // of the +-2048 px guard band, which is the clamp both headers call the law.
  Cfg rails[2];
  rails[0].m = persp(kOne * 1000, kOne * 1000, kOne, kOne / 65536);
  rails[0].vp = zr::Viewport{0, 0, 4095, 4095};
  rails[1].m = persp(-kOne * 1000, -kOne * 1000, kOne, kOne / 65536);
  rails[1].vp = zr::Viewport{2048, 2048, 4095, 4095};

  // THE NEAR-PLANE BOUNDARY, HIT EXACTLY.
  //
  // Added because control P6 -- "the near plane becomes strict, w == 0 moves to
  // the accept side" -- was NOT CAUGHT by the eleven random phases. The law is
  // `clip.w <= 0`, and `w == 0` exactly belongs on the REJECT side; a random
  // world coordinate lands on the single word that makes clip.w zero with
  // probability far too small to rely on. Arguing that the two blocks both
  // spell `<=` is reading, not evidence.
  //
  // So: a matrix whose w row is the IDENTITY ON X, m[3][0] = 1.0 and every
  // other w-row word zero. Then row_cw = 1.0 * x exactly and
  // rescale(.,16) of it is the raw word x itself, so clip.w == x -- and a
  // vertex with x == 0 puts the near plane precisely on its boundary. View 1
  // uses -1.0 so the same sweep straddles the boundary from the other side.
  Cfg nearp[2];
  for (int v = 0; v < 2; ++v) {
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c) nearp[v].m.m[r][c] = zref::fx16{0};
    nearp[v].m.m[0][0] = zref::fx16{kOne};
    nearp[v].m.m[1][1] = zref::fx16{kOne};
    nearp[v].m.m[3][0] = zref::fx16{v ? -kOne : kOne};  // clip.w = +x or -x
  }
  nearp[0].vp = zr::Viewport{0, 0, 256, 192};
  nearp[1].vp = zr::Viewport{256, 0, 256, 192};

  std::vector<Tri> boundary;
  for (int a = -3; a <= 3; ++a)
    for (int b = -3; b <= 3; ++b)
      for (int c = -3; c <= 3; ++c)
        for (int view = 0; view < 2; ++view) {
          Tri x{};
          x.x[0] = a;
          x.x[1] = b;
          x.x[2] = c;
          x.y[0] = 1;
          x.y[1] = -1;
          x.y[2] = 0;
          x.z[0] = 5;
          x.z[1] = -5;
          x.z[2] = 0;
          x.src = static_cast<uint16_t>(((a + 3) * 49 + (b + 3) * 7 + (c + 3)) & 0xFFFF);
          x.view = static_cast<uint8_t>(view);
          boundary.push_back(x);
        }
  phase_explicit("near-plane boundary w == 0", g, t, nearp, boundary, 0);
  phase_explicit("near-plane boundary, stalling", g, t, nearp, boundary, 0x6DB6DB6Du);

  phase("duo, no stall", g, t, duo, 400, 0, true);
  phase("duo, stalling consumer", g, t, duo, 400, 0xB6DB6DB6u, true);
  phase("odd viewports, no stall", g, t, odd, 400, 0, true);
  phase("odd viewports, stalling", g, t, odd, 400, 0x55555555u, true);
  phase("behind the eye, no stall", g, t, behind, 400, 0, true);
  phase("behind the eye, stalling", g, t, behind, 400, 0xF0F0F0F0u, true);
  phase("guard-band rails", g, t, rails, 400, 0, true);
  phase("guard-band rails, stalling", g, t, rails, 400, 0x80000001u, true);

  // Reconfiguration WITHOUT an intervening reset: the second phase overwrites
  // both views' matrices and viewports while the blocks are quiescent but not
  // reset. A merge that latched configuration anywhere other than the register
  // file -- or that kept a stale matrix -- shows up here and nowhere else.
  phase("reconfigured, no reset", g, t, odd, 300, 0, false);
  phase("reconfigured again, stalling", g, t, rails, 300, 0x3333CCCCu, false);
  phase("reconfigured back to duo", g, t, duo, 300, 0, false);

  std::printf("----\n%d vertices compared on three-way agreement, %d mismatches\n", g_cmp, g_fail);
  if (g_fail == 0) {
    std::printf("RESULT: geom_project, terrain_project and project_vertex AGREE EVERYWHERE.\n");
  } else {
    std::printf("RESULT: THE TWO BLOCKS ARE NOT EQUIVALENT. Do not merge.\n");
  }
  std::fflush(nullptr);
  std::_Exit(g_fail == 0 ? 0 : 1);
}
