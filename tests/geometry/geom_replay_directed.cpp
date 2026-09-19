// geom_replay_directed.cpp -- GEOM.REPLAY (fpga/rtl/geometry/zhao_geom_replay.sv).
//
// The arena and the vertex-attribute store are MODELLED here with the arena's
// own contract (look_ready always 1, the reply one clock later, refusal before
// presence). Since owner ruling R31 (2026-09-19) depth is NOT computed here: the
// store (GEOM.VATTR) answers each vertex's invw24 beside its attributes, so the
// check is that every corner carries ITS OWN store word, depth included. The
// depth law itself is differenced against `zref::depth_of_raw` in
// geom_vattr_directed.cpp and geom_depthquant_stream_directed.cpp.
//
// What each case asserts is in its own banner. Every counter the block exports
// is seen to MOVE by legal stimulus at its ports, and seen to stay at zero on
// the clean cases, so no mutant is owed.
#include <array>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <map>
#include <random>
#include <vector>
#include "verilated.h"
#include "Vzhao_geom_replay.h"
#include "zhao_sim.hpp"
#include "zref/zref_depth.hpp"

namespace {

int g_checks = 0, g_fail = 0;
void ck(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

constexpr unsigned kDepth = 1089;   // the console's GEOM_DEPTH

struct Vtx {
  int32_t x, y;        // s21
  uint32_t w;          // 31 bits: in the arena payload, NOT read by replay
  bool behind;
  uint32_t invw;       // the store's invw24 for this vertex in this view
  uint32_t attr[6];
  bool written;        // a MISS if false
};

struct Arena {
  uint8_t gen = 0;
  bool sealed = false;
  std::vector<Vtx> v;
};

struct Emitted {
  int32_t ax, ay, bx, by, cx, cy;
  unsigned behind;
  uint32_t invw[3];
  uint32_t attr[3][6];
  unsigned view;
  unsigned src, material;
  uint32_t raster;
};

int32_t s21(uint32_t raw) { return static_cast<int32_t>(raw << 11) >> 11; }

struct Bench {
  Vzhao_geom_replay t;
  std::map<unsigned, Arena> arenas;
  std::mt19937 rng{0xC0FFEE};
  // one-clock reply pipeline
  bool pend = false;
  unsigned pend_arena = 0, pend_gen = 0, pend_index = 0;
  int att_delay_next = 0;     // >0: the store answers one clock LATE once
  bool att_late_pend = false;
  uint32_t att_late_data[6] = {};
  uint32_t att_late_invw = 0;
  int stall_pct = 0;
  std::vector<Emitted> out;
  std::vector<unsigned> rels;
  std::vector<unsigned> looked_index;
  int af_releases = 0;
  long clocks = 0;

  void zero_inputs() {
    t.mt_valid_i = 0; t.grp_valid_i = 0; t.t_valid_i = 0; t.m_done_i = 0;
    t.o_ready_i = 1; t.look_ready_i = 1;
    t.rep_valid_i = 0; t.att_rep_valid_i = 0; t.grp_poison_i = 0;
  }

  void reset() {
    zero_inputs();
    t.rst_n = 0;
    for (int i = 0; i < 3; ++i) zhao::tick(t);
    t.rst_n = 1;
    zhao::tick(t);
  }

  static void put_payload(Vzhao_geom_replay& t, const Vtx& v) {
    // {behind[105], w[104:74], d[73:42], y[41:21], x[20:0]} in 32-bit words
    uint64_t lo = (uint64_t(uint32_t(v.x) & 0x1FFFFF)) |
                  (uint64_t(uint32_t(v.y) & 0x1FFFFF) << 21) |
                  (uint64_t(0xDEADBEEFu & 0x3FFFFF) << 42);  // d: garbage, unread
    uint64_t hi = (uint64_t(0xDEADBEEFu) >> 22) |              // d[31:22] at bit 64
                  (uint64_t(v.w & 0x7FFFFFFF) << 10) |
                  (uint64_t(v.behind ? 1 : 0) << 41);
    t.rep_payload_i[0] = uint32_t(lo);
    t.rep_payload_i[1] = uint32_t(lo >> 32);
    t.rep_payload_i[2] = uint32_t(hi);
    t.rep_payload_i[3] = uint32_t(hi >> 32);
  }

  // One clock. Drives the reply to LAST clock's lookup, samples outputs.
  void step() {
    t.rep_valid_i = 0;
    t.att_rep_valid_i = 0;
    if (pend) {
      bool refuse = false, hit = false;
      Vtx vx{};
      auto it = arenas.find(pend_arena);
      if (it == arenas.end() || pend_index >= kDepth || !it->second.sealed ||
          it->second.gen != pend_gen) {
        refuse = true;
      } else if (pend_index < it->second.v.size() && it->second.v[pend_index].written) {
        hit = true;
        vx = it->second.v[pend_index];
      }
      t.rep_valid_i = 1;
      t.rep_hit_i = hit;
      t.rep_refuse_i = refuse;
      put_payload(t, vx);
      if (att_delay_next > 0) {
        --att_delay_next;
        att_late_pend = true;
        att_late_invw = vx.invw;
        for (int k = 0; k < 6; ++k) att_late_data[k] = vx.attr[k];
      } else {
        t.att_rep_valid_i = 1;
        t.att_invw_i = vx.invw;
        for (int k = 0; k < 6; ++k) t.att_rep_data_i[k] = vx.attr[k];
      }
    } else if (att_late_pend) {
      att_late_pend = false;
      t.att_rep_valid_i = 1;
      t.att_invw_i = att_late_invw;
      for (int k = 0; k < 6; ++k) t.att_rep_data_i[k] = att_late_data[k];
    }
    t.o_ready_i = (int(rng() % 100) >= stall_pct) ? 1 : 0;
    t.eval();

    pend = t.look_valid_o && t.look_ready_i;
    if (pend) {
      pend_arena = t.look_arena_o;
      pend_gen = t.look_gen_o;
      pend_index = t.look_index_o;
      looked_index.push_back(pend_index);
    }
    if (t.o_valid_o && t.o_ready_i) {
      Emitted e{};
      e.ax = s21(t.o_ax_o); e.ay = s21(t.o_ay_o);
      e.bx = s21(t.o_bx_o); e.by = s21(t.o_by_o);
      e.cx = s21(t.o_cx_o); e.cy = s21(t.o_cy_o);
      e.behind = t.o_behind_o;
      e.invw[0] = t.o_invw_a_o; e.invw[1] = t.o_invw_b_o; e.invw[2] = t.o_invw_c_o;
      for (int k = 0; k < 6; ++k) {
        e.attr[0][k] = t.o_attr_a_o[k];
        e.attr[1][k] = t.o_attr_b_o[k];
        e.attr[2][k] = t.o_attr_c_o[k];
      }
      e.view = t.o_view_o;
      e.src = t.o_src_id_o;
      e.material = t.o_material_o;
      e.raster = t.o_raster_o;
      out.push_back(e);
    }
    if (t.rel_valid_o) rels.push_back(t.rel_arena_o);
    if (t.af_release_o) ++af_releases;
    zhao::tick(t);
    ++clocks;
  }

  // Wait for a ready, holding `set` driven; returns false on timeout.
  template <typename F, typename R>
  bool hold_until(F set, R ready, int budget = 4000) {
    for (int i = 0; i < budget; ++i) {
      set(true);
      t.eval();
      const bool take = ready();
      step();
      if (take) {
        set(false);
        return true;
      }
    }
    set(false);
    return false;
  }

  Arena& make_arena(unsigned a, unsigned n, unsigned gen) {
    Arena& ar = arenas[a];
    ar.gen = static_cast<uint8_t>(gen);
    ar.sealed = true;
    ar.v.assign(n, Vtx{});
    for (unsigned i = 0; i < n; ++i) {
      Vtx& v = ar.v[i];
      v.x = static_cast<int32_t>(rng() % 0x200000) - 0x100000;
      v.y = static_cast<int32_t>(rng() % 0x200000) - 0x100000;
      // w spans below wmin, the working range and above wmax.
      const unsigned pick = rng() % 8;
      v.w = pick == 0 ? (rng() % 70000u)
          : pick == 1 ? (0x7FFFFFFFu - (rng() % 1000u))
          : (65536u + rng() % 200000000u);
      v.behind = (rng() % 5) == 0;
      v.invw = rng() & 0xFFFFFFu;
      for (int k = 0; k < 6; ++k) v.attr[k] = rng();
      v.written = true;
    }
    return ar;
  }

  // The arena's lifetime is GEOM.GROUP_SEQ's and GEOM.VATTR's business now;
  // these remain as one idle clock each so every case keeps its timing.
  void land(unsigned, unsigned) { step(); }
  void open(unsigned) { step(); }

  bool token(unsigned mask, unsigned vcount) {
    return hold_until(
        [&](bool on) { t.mt_valid_i = on; t.mt_view_mask_i = mask; t.mt_vertex_count_i = vcount; },
        [&]() { return t.mt_ready_o != 0; });
  }
  bool handle(unsigned arena, unsigned gen, unsigned view, unsigned poison = 0) {
    return hold_until(
        [&](bool on) {
          t.grp_valid_i = on; t.grp_arena_i = arena; t.grp_gen_i = gen; t.grp_view_i = view;
          t.grp_poison_i = on ? poison : 0;
        },
        [&]() { return t.grp_ready_o != 0; });
  }
  bool tri(unsigned a, unsigned b, unsigned c, unsigned src, unsigned mat) {
    return hold_until(
        [&](bool on) {
          t.t_valid_i = on; t.t_v0_i = a; t.t_v1_i = b; t.t_v2_i = c;
          t.t_src_id_i = src; t.t_material_i = mat; t.t_raster_i = 0xA5000000u | mat;
        },
        [&]() { return t.t_ready_o != 0; });
  }
  void done() {
    t.m_done_i = 1;
    step();
    t.m_done_i = 0;
  }
  bool drain_release(int budget = 4000) {
    const int before = af_releases;
    for (int i = 0; i < budget && af_releases == before; ++i) step();
    return af_releases == before + 1;
  }
};

bool corner_ok(const Emitted& e, int k, const Vtx& v, unsigned /*profile: the store's business*/) {
  const int32_t xs[3] = {e.ax, e.bx, e.cx}, ys[3] = {e.ay, e.by, e.cy};
  if (xs[k] != v.x || ys[k] != v.y) return false;
  if (((e.behind >> k) & 1u) != (v.behind ? 1u : 0u)) return false;
  if (e.invw[k] != v.invw) return false;   // the STORE's word, for THIS vertex
  for (int j = 0; j < 6; ++j)
    if (e.attr[k][j] != v.attr[j]) return false;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Bench b;
  b.reset();

  // ---- A: one view, twenty triangles, stalled consumer ----------------------
  // Every corner's x/y/behind are the arena's and its invw24 + attributes the
  // store's, at the looked-up index.
  {
    b.stall_pct = 40;
    Arena& ar = b.make_arena(1, 40, 7);
    b.open(1);
    b.land(1, 1);
    b.land(1, 1);
    ck(b.token(0b01, 40), "A: the meshlet token is taken");
    ck(b.handle(1, 7, 0), "A: the handle is taken");
    std::vector<std::array<unsigned, 3>> tris;
    for (unsigned n = 0; n < 20; ++n) {
      std::array<unsigned, 3> tr{(n * 3) % 40, (n * 3 + 1) % 40, (n * 7 + 5) % 40};
      tris.push_back(tr);
      ck(b.tri(tr[0], tr[1], tr[2], 0x0A1, 0x0033), "A: a triangle is taken");
    }
    b.done();
    ck(b.drain_release(), "A: the meshlet is released exactly once, after its last triangle");
    int bad = 0;
    if (b.out.size() != 20) ++bad;
    for (size_t n = 0; n < b.out.size() && n < 20; ++n) {
      for (int k = 0; k < 3; ++k)
        if (!corner_ok(b.out[n], k, ar.v[tris[n][k]], 1)) ++bad;
      if (b.out[n].view != 0 || b.out[n].src != 0x0A1 || b.out[n].material != 0x0033 ||
          b.out[n].raster != (0xA5000000u | 0x33u))
        ++bad;
    }
    ck(bad == 0, "A: every corner, depth, attribute and pass-through field matches, in order");
    ck(b.rels.size() == 1 && b.rels[0] == 1, "A: arena 1 is released, once");
    ck(b.t.refused_o == 0 && b.t.missed_o == 0 && b.t.att_skew_o == 0 &&
       b.t.view_bad_o == 0,
       "A: no fault counter moved on a clean meshlet");
    std::printf("  A: 20 triangles in %ld clocks at 40%% consumer stall\n", b.clocks);
  }

  // ---- B: two views, one walk: each triangle twice, each view its OWN arena -
  {
    b.stall_pct = 0;
    b.out.clear(); b.rels.clear();
    Arena& a0 = b.make_arena(2, 12, 3);
    Arena& a1 = b.make_arena(3, 12, 9);
    b.open(2); b.open(3);
    b.land(2, 0);
    b.land(3, 2);
    const long c0 = b.clocks;
    ck(b.token(0b11, 12), "B: token");
    ck(b.handle(2, 3, 0) && b.handle(3, 9, 1), "B: both handles");
    std::vector<std::array<unsigned, 3>> tris;
    for (unsigned n = 0; n < 8; ++n) {
      std::array<unsigned, 3> tr{n % 12, (n + 4) % 12, (n + 9) % 12};
      tris.push_back(tr);
      ck(b.tri(tr[0], tr[1], tr[2], 0x0B2, 0x0044), "B: triangle");
    }
    b.done();
    ck(b.drain_release(), "B: released once");
    int bad = 0;
    if (b.out.size() != 16) ++bad;
    for (size_t n = 0; n < 8 && 2 * n + 1 < b.out.size(); ++n) {
      const Emitted& e0 = b.out[2 * n];
      const Emitted& e1 = b.out[2 * n + 1];
      if (e0.view != 0 || e1.view != 1) ++bad;
      for (int k = 0; k < 3; ++k) {
        if (!corner_ok(e0, k, a0.v[tris[n][k]], 0)) ++bad;
        if (!corner_ok(e1, k, a1.v[tris[n][k]], 2)) ++bad;
      }
    }
    ck(bad == 0, "B: view 0 reads arena 2, view 1 reads arena 3 -- the second eye never "
                 "sees the first eye's vertices, nor their depth");
    ck(b.rels.size() == 2 && b.rels[0] == 2 && b.rels[1] == 3, "B: both arenas released");
    const long per = (b.clocks - c0) / 16;
    std::printf("  B: 16 view-triangles in %ld clocks (%ld per view-triangle, unstalled)\n",
                b.clocks - c0, per);
  }

  // ---- C: refused and missed corners DROP the triangle and are counted ------
  {
    b.out.clear(); b.rels.clear();
    Arena& ar = b.make_arena(1, 10, 4);
    ar.v[6].written = false;          // a MISS
    b.open(1);
    b.land(1, 0);
    ck(b.token(0b01, 10) && b.handle(1, 4, 0), "C: token and handle");
    ck(b.tri(0, 1, 2, 1, 1), "C: clean");
    ck(b.tri(0, 6, 2, 1, 1), "C: missed corner");
    ck(b.tri(0, 1, 900, 1, 1), "C: index past the vertices written -> miss");
    ck(b.tri(0, 1, 2000, 1, 1), "C: index past DEPTH -> refused");
    ck(b.tri(0, 1, 0x1005, 1, 1), "C: vertex id wider than the index -> refused, not aliased");
    ck(b.tri(3, 4, 5, 1, 1), "C: clean again");
    b.done();
    ck(b.drain_release(), "C: released");
    ck(b.out.size() == 2, "C: only the two drawable triangles are emitted");
    ck(b.t.missed_o == 2, "C: missed_o counts both missed triangles");
    ck(b.t.refused_o == 2, "C: refused_o counts both refused triangles");
    bool forced = false;
    for (unsigned ix : b.looked_index) if (ix == 0xFFF) forced = true;
    ck(forced, "C: id 0x1005 was looked up as 0xFFF, which the arena REFUSES, rather than truncated to 5");
  }

  // ---- D: a stale generation is refused ------------------------------------
  {
    b.out.clear(); b.rels.clear();
    b.make_arena(2, 8, 5);
    b.open(2);
    b.land(2, 0);
    const uint32_t before = b.t.refused_o;
    ck(b.token(0b01, 8) && b.handle(2, 4 /* stale */, 0), "D: token and a STALE handle");
    ck(b.tri(0, 1, 2, 1, 1), "D: triangle");
    b.done();
    ck(b.drain_release(), "D: released");
    ck(b.out.empty() && b.t.refused_o == before + 1, "D: a stale handle draws nothing and is refused");
  }

  // ---- E: a meshlet with NO vertices expects no handle and cannot wedge -----
  {
    b.out.clear(); b.rels.clear();
    const uint32_t before = b.t.refused_o;
    ck(b.token(0b01, 0), "E: a zero-vertex token");
    ck(b.tri(0, 1, 2, 1, 1), "E: its triangle is still taken, so the walk drains");
    b.done();
    ck(b.drain_release(), "E: released with no arena held");
    ck(b.rels.empty() && b.out.empty() && b.t.refused_o == before + 1,
       "E: nothing released to GROUP_SEQ, nothing drawn, the triangle counted refused");
  }

  // ---- F: the walk ends BEFORE the handles arrive (an empty meshlet) --------
  {
    b.out.clear(); b.rels.clear();
    b.make_arena(1, 4, 6);
    b.open(1);
    b.land(1, 0);
    ck(b.token(0b01, 4), "F: token");
    b.done();                                   // ASSEMBLE ended first
    for (int i = 0; i < 5; ++i) b.step();
    ck(b.af_releases >= 0 && b.t.grp_ready_o, "F: still waiting for its handle");
    ck(b.handle(1, 6, 0), "F: the handle arrives late");
    ck(b.drain_release(), "F: released: the early end was not lost");
    ck(b.rels.size() == 1, "F: its arena is released");
  }

  // ---- G: a handle for a view the meshlet is not visible in -----------------
  {
    b.rels.clear();
    b.make_arena(2, 4, 8);
    b.open(2);
    b.land(2, 0);
    ck(b.token(0b01, 4) && b.handle(2, 8, 1), "G: mask 01, handle for view 1");
    b.done();
    ck(b.drain_release(), "G: released");
    ck(b.t.view_bad_o == 1, "G: view_bad_o counts it");
  }

  // ---- I: the attribute store answering LATE is counted ---------------------
  {
    b.out.clear(); b.rels.clear();
    b.make_arena(1, 8, 10);
    b.open(1);
    b.land(1, 0);
    ck(b.token(0b01, 8) && b.handle(1, 10, 0), "I: token and handle");
    b.att_delay_next = 1;
    ck(b.tri(0, 1, 2, 1, 1), "I: triangle");
    b.done();
    ck(b.drain_release(), "I: released");
    ck(b.t.att_skew_o >= 1, "I: att_skew_o moves when the store and the arena disagree on timing");
  }

  // (H and J -- the per-arena profile and DEPTHQUANT's refusal -- moved to
  // geom_vattr_directed.cpp with the depth law, owner ruling R31.)

  // ---- K: R31 -- a POISONED handle drops the whole meshlet, and releases it --
  // GEOM.GROUP_SEQ marks a batch that lost a record upstream. One poisoned view
  // poisons the meshlet (both views came from the same record stream). Every
  // triangle is TAKEN (so ASSEMBLE's walk drains) and dropped WITHOUT a lookup
  // -- a lookup would HIT, with a corner from the wrong vertex -- and both
  // arenas still go back. Before the fix no poisoned handle could exist: the
  // batch never sealed and this block waited on S_HAND for ever.
  {
    b.out.clear(); b.rels.clear();
    b.make_arena(1, 6, 12);
    b.make_arena(3, 6, 13);
    b.open(1); b.open(3);
    b.land(1, 0); b.land(3, 0);
    const uint32_t p0 = b.t.poisoned_o, r0 = b.t.refused_o, m0 = b.t.missed_o;
    const size_t looks0 = b.looked_index.size();
    ck(b.token(0b11, 6), "K: token");
    ck(b.handle(1, 12, 0, 1) && b.handle(3, 13, 1, 0), "K: view 0 poisoned, view 1 clean");
    ck(b.tri(0, 1, 2, 1, 1) && b.tri(3, 4, 5, 1, 1) && b.tri(0, 2, 4, 1, 1), "K: three triangles taken");
    b.done();
    ck(b.drain_release(), "K: R31: the poisoned meshlet is RELEASED -- the frame completes");
    ck(b.out.empty(), "K: nothing drawn from a poisoned meshlet, in EITHER view");
    ck(b.t.poisoned_o - p0 == 3, "K: poisoned_o counts every dropped descriptor");
    ck(b.t.refused_o == r0 && b.t.missed_o == m0, "K: ... and not as refused or missed");
    ck(b.looked_index.size() == looks0, "K: no lookup was issued for a poisoned meshlet");
    ck(b.rels.size() == 2, "K: both arenas are released back to GEOM.GROUP_SEQ");
  }

  // ---- L: the NEXT meshlet is clean again (the poison is per meshlet) -------
  {
    b.out.clear(); b.rels.clear();
    b.make_arena(2, 4, 14);
    b.open(2);
    b.land(2, 0);
    const uint32_t p0 = b.t.poisoned_o;
    ck(b.token(0b01, 4) && b.handle(2, 14, 0, 0), "L: token and a clean handle");
    ck(b.tri(0, 1, 2, 1, 1), "L: triangle");
    b.done();
    ck(b.drain_release(), "L: released");
    ck(b.out.size() == 1 && b.t.poisoned_o == p0, "L: a clean meshlet after a poisoned one draws");
  }

  ck(b.t.meshlets_o == 10, "the meshlet count is every token taken");
  std::printf("geom_replay_directed: %d checks, %d failed (meshlets=%u groups=%u tri_in=%u "
              "tri_out=%u refused=%u missed=%u skew=%u view_bad=%u poisoned=%u)\n",
              g_checks, g_fail, b.t.meshlets_o, b.t.groups_o, b.t.triangles_in_o,
              b.t.triangles_out_o, b.t.refused_o, b.t.missed_o, b.t.att_skew_o,
              b.t.view_bad_o, b.t.poisoned_o);
  zhao::exit_hard(g_fail ? 1 : 0);
}
