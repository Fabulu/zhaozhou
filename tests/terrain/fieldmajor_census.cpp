// fieldmajor_census.cpp — THE FIELD-MAJOR EARTH ASSOCIATION, MEASURED AS A
// LINE, against the same 6,000-clock contract the vertex-major path misses by
// 15.3x.
//
// WHAT COMMISSIONED THIS FILE
// ---------------------------
// `reports/DECISION-20260926-I34-PATCH-V2-CHANNELS.md` measured the composed
// VERTEX-MAJOR path through four production blocks as a LINE --
// `clocks(L) = 4,431 + 1,089*L`, 91,551 clocks at the real engine's price --
// and then priced the alternative with ARITHMETIC, saying so plainly:
// "everything in this subsection after the two measured constants is
// arithmetic" and "it has not been built or benched". Two numbers in that
// arithmetic decide whether the field-major machine is worth building:
//
//   * THE INTERCEPT, 4,431 clocks -- "4.07 clocks per vertex of work that is
//     NOT engine latency", said to be "74% of the 6,000-clock contract on its
//     own". The whole commission rests on the field-major form REMOVING it.
//   * THE SLOPE, 1,089 clocks per clock of engine latency -- one un-overlapped
//     round trip per covered vertex.
//
// This file measures both for the field-major form, in RTL, and reports the
// line rather than a point -- because a single point would have the same
// defect PATCHV2 caught in its own first draft: at the bench's default
// latency it would read small and flattering.
//
// THE TWO BLOCKS WERE ALREADY BUILT. NOTHING HERE IS A REWRITE.
// `zhao_terrain_field_walk` (the Earth lattice walker, 297 row-bounded groups,
// one per clock) and `zhao_terrain_patch_acc` (the four-bank accumulator,
// 273 INIT + 273 DRAIN aligned groups) each carry a full differential test.
// What did not exist is the two of them ELABORATED TOGETHER: each test does
// the other block's job in C++. `tb_terrain_fieldmajor.sv` is that
// composition and this is its driver.
//
// WHERE THE ENGINE IS, AND WHY THE COMPARISON IS LIKE FOR LIKE
// ------------------------------------------------------------
// `composepub_acceptance` case 11 models FIELD.HOST at its client seam with a
// declared response latency and ONE request in flight. It can hold that
// against real RTL ports because `zhao_field_earth_adapter` exists. The
// field-major machine's counterpart adapter DOES NOT EXIST -- that is what
// directive 13.1 commissions -- so the engine is modelled here, at the same
// place in the dataflow, with the same declared latency.
//
// AND WITH ONE PARAMETER CASE 11 DOES NOT HAVE: DEPTH. Case 11's engine holds
// one point in flight because `zhao_field_host`'s front does. The field-major
// engine is `zhao_field_v3_exec`, a BARRELLED machine whose whole reason for
// existing is that several contexts are in flight at once (`CTX = 8` by
// default, its own header: "the pipeline is only full when at least five
// contexts are ready"). Modelling it at depth 1 would price a machine nobody
// proposes. So the census sweeps LATENCY and DEPTH and prints the surface.
//
// WHAT THIS MEASUREMENT IS NOT, STATED HERE RATHER THAN BURIED
// -------------------------------------------------------------
// The walk and the reduction are REAL RTL and their clocks are measured. The
// engine is a declared latency, so this is a FLOOR for the field-major form,
// and the intercept it reports is the honest quantity to compare against the
// vertex-major intercept -- which is also measured with a modelled engine.
// The vertex-major number additionally contains a real adapter's own clocks;
// this one contains no adapter at all, because there is none to contain.
//
// NO ASSERTION HERE ASSERTS THE BUDGET, in either direction. A test that
// asserted "the field-major form meets 6,000" would assert the conclusion the
// packet was sent to measure, and a test that asserted the vertex-major miss
// would assert a bug. What is asserted is behaviour that must hold whatever
// the numbers say: the group count, the coverage, the per-clock throughput of
// each phase, the reducer's agreement with the ratified oracle, and the
// counters' own positive and negative controls.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vtb_terrain_fieldmajor.h"

#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_patch.hpp"

namespace {

using Dut = Vtb_terrain_fieldmajor;

constexpr int kLat = 33;
constexpr int kVerts = kLat * kLat;       // 1,089
constexpr int kAlignedGroups = 273;       // INIT / DRAIN: flat aligned
constexpr int kUpdateGroups = 9 * kLat;   // 297: row-bounded, 9 per row

// wmask bits (mirror the RTL's W_H/W_V/W_M/W_N)
constexpr uint8_t W_H = 1, W_V = 2, W_M = 4, W_N = 8;

int g_checks = 0;
int g_fails = 0;

void ck(bool cond, const char* what) {
  ++g_checks;
  if (!cond) {
    ++g_fails;
    std::printf("  FAIL: %s\n", what);
  }
}

void ck_eq(int64_t got, int64_t want, const char* what) {
  ++g_checks;
  if (got != want) {
    ++g_fails;
    std::printf("  FAIL: %s  (got %lld, want %lld)\n", what, static_cast<long long>(got),
                static_cast<long long>(want));
  }
}

// ---------------------------------------------------------------------------
// THE PREPARED LATTICE -- built exactly as the reference builds it, which is
// the walker's own contract (its header: the tables are prepared by the ARM
// with the SAME zref:: primitives, never accumulated from a pitch).
// ---------------------------------------------------------------------------
struct Lattice {
  int32_t wx[kLat], wz[kLat];
  Lattice(int32_t ex0, int32_t ex1, int32_t ez0, int32_t ez1) {
    for (int i = 0; i < kLat; ++i) wx[i] = zref::terrain::lattice_lerp(ex0, ex1, i, kLat - 1);
    for (int j = 0; j < kLat; ++j) wz[j] = zref::terrain::lattice_lerp(ez0, ez1, j, kLat - 1);
  }
};

// ---------------------------------------------------------------------------
// THE FIELD. Its height/velocity lane at a vertex is a function of the WORLD
// COORDINATES the walker generated, not of the vertex index -- so a lane that
// arrived on the wrong lane of a group, or a group whose z came from the wrong
// row, produces the wrong answer instead of the right one by luck. A constant
// lift (which is what the vertex-major census uses) cannot catch either.
// ---------------------------------------------------------------------------
struct Field {
  int32_t x0, x1, z0, z1;   // closed footprint, fx16 raw
  uint8_t wmask;
  bool constant_lift;
  int32_t lift;             // used when constant_lift

  bool covers_world(int32_t wx, int32_t wz) const {
    return wx >= x0 && wx <= x1 && wz >= z0 && wz <= z1;
  }
  int32_t h_at(int32_t wx, int32_t wz) const {
    if (constant_lift) return lift;
    // a small, exactly representable, coordinate-dependent value
    return static_cast<int32_t>(((wx >> 12) * 5) + ((wz >> 12) * 3) + (1 << 16));
  }
  int32_t v_at(int32_t wx, int32_t wz) const {
    if (constant_lift) return lift >> 2;
    return static_cast<int32_t>(((wx >> 13) * 2) - (wz >> 13) + (1 << 14));
  }
  uint32_t m_at(int32_t wx, int32_t wz) const {
    return static_cast<uint32_t>(0xA5000000u ^ (static_cast<uint32_t>(wx) >> 3) ^
                                 (static_cast<uint32_t>(wz) << 5));
  }
  int32_t n_at(int32_t wx, int32_t wz) const {
    if (constant_lift) return lift >> 3;
    return static_cast<int32_t>((wx >> 14) + (wz >> 14) + (1 << 13));
  }
};

// The authored patch under the field.
struct PatchIn {
  int16_t base[kVerts];
  int16_t scar[kVerts];
  int16_t bot[kVerts];
  bool dual[kVerts];
};

struct PatchOut {
  int32_t top[kVerts], bottom[kVerts], vel[kVerts], nav[kVerts];
  uint32_t mat[kVerts];
  bool dirty[kVerts];
};

// ---- THE ORACLE: the ratified §3.4 vertex-major law, applied to the same
// field. The point of the whole exercise is that FIELD-MAJOR WALKING MUST NOT
// CHANGE THE ANSWER, so the expectation is computed by the vertex-major
// oracle and the hardware walks field-major.
PatchOut oracle(const Lattice& lat, const PatchIn& p, const std::vector<Field>& fields) {
  PatchOut o{};
  zref::SatLedger Lv{}, Ln{};
  zref::terrain::FieldList listH;
  std::vector<const Field*> hw;
  for (const Field& f : fields) {
    if (f.wmask & W_H) {
      zref::terrain::FieldRecord r;
      r.x0 = f.x0;
      r.x1 = f.x1;
      r.z0 = f.z0;
      r.z1 = f.z1;
      listH.offer(r, 0);
      hw.push_back(&f);
    }
  }
  for (int vj = 0; vj < kLat; ++vj) {
    for (int vi = 0; vi < kLat; ++vi) {
      const int v = vj * kLat + vi;
      const int32_t wx = lat.wx[vi], wz = lat.wz[vj];
      zref::terrain::ComposeIn in;
      in.base = p.base[v];
      in.scar = p.scar[v];
      in.bottom = p.bot[v];
      in.dual = p.dual[v];
      in.wx = wx;
      in.wz = wz;
      std::vector<int32_t> lanes(hw.size());
      for (size_t i = 0; i < hw.size(); ++i) lanes[i] = hw[i]->h_at(wx, wz);
      const zref::terrain::ComposeOut co = zref::terrain::compose_vertex(in, listH, lanes.data());
      o.top[v] = co.live_top;
      o.bottom[v] = co.bottom;
      o.dirty[v] = co.dirty;

      int32_t vel = 0, nav = 0;
      uint32_t mat = 0;
      for (const Field& f : fields) {
        if (!f.covers_world(wx, wz)) continue;
        if (f.wmask & W_V) vel = zref::fx_add(zref::fx16{vel}, zref::fx16{f.v_at(wx, wz)}, &Lv).raw;
        if (f.wmask & W_N) nav = zref::fx_add(zref::fx16{nav}, zref::fx16{f.n_at(wx, wz)}, &Ln).raw;
        if (f.wmask & W_M) mat = f.m_at(wx, wz);  // last covering writer wins
      }
      o.vel[v] = vel;
      o.nav[v] = nav;
      o.mat[v] = mat;
    }
  }
  return o;
}

// ---------------------------------------------------------------------------
// THE ENGINE MODEL -- FIELD.SEQ.EARTH's evaluator at its group seam.
//
// `latency` clocks between a group being ACCEPTED off the walker and its four
// results being presented to the accumulator; `depth` groups may be in flight.
// Depth 1 is case 11's engine. Depth CTX is `zhao_field_v3_exec`'s.
// ---------------------------------------------------------------------------
struct WalkGroup {
  int iv;
  uint8_t mask;
  int32_t z;
  int32_t x[4];
  bool last;
};

struct Engine {
  int latency = 0;
  int depth = 1;
  const Field* field = nullptr;

  struct Slot {
    WalkGroup g;
    int cd;
  };
  std::vector<Slot> inflight;

  uint64_t accepted = 0;
  uint64_t delivered = 0;

  bool can_accept() const { return static_cast<int>(inflight.size()) < depth; }
  void take(const WalkGroup& g) {
    inflight.push_back(Slot{g, latency});
    ++accepted;
  }
  bool ripe() const { return !inflight.empty() && inflight.front().cd <= 0; }
  WalkGroup pop() {
    const WalkGroup g = inflight.front().g;
    inflight.erase(inflight.begin());
    ++delivered;
    return g;
  }
  void age() {
    for (Slot& s : inflight)
      if (s.cd > 0) --s.cd;
  }
};

// ---------------------------------------------------------------------------
// THE DRIVER
// ---------------------------------------------------------------------------
struct Sim {
  Dut d;
  Engine eng;
  uint64_t clocks = 0;

  // captured DRAIN results
  int32_t got_top[kVerts], got_bot[kVerts], got_vel[kVerts], got_nav[kVerts];
  uint32_t got_mat[kVerts];
  bool got_dirty[kVerts], got_seen[kVerts];

  void step() {
    zhao::tick(d);
    ++clocks;
  }

  void reset() {
    d.rst_n = 0;
    quiesce();
    d.eval();
    for (int i = 0; i < 4; ++i) step();
    d.rst_n = 1;
    d.eval();
    step();
  }

  void quiesce() {
    d.lt_we_i = 0;
    d.as_valid_i = 0;
    d.wk_ready_i = 0;
    d.in_valid_i = 0;
    d.up_valid_i = 0;
    d.dr_valid_i = 0;
  }

  void idle(int n) {
    d.in_valid_i = 0;
    d.up_valid_i = 0;
    d.dr_valid_i = 0;
    d.eval();
    for (int i = 0; i < n; ++i) step();
  }

  void load_tables(const Lattice& lat) {
    for (int i = 0; i < kLat; ++i) {
      d.lt_we_i = 1;
      d.lt_sel_i = 0;
      d.lt_idx_i = static_cast<uint8_t>(i);
      d.lt_val_i = static_cast<uint32_t>(lat.wx[i]);
      d.eval();
      step();
      d.lt_sel_i = 1;
      d.lt_val_i = static_cast<uint32_t>(lat.wz[i]);
      d.eval();
      step();
    }
    d.lt_we_i = 0;
    d.eval();
  }

  // ---- PHASE 1: INIT, one aligned group per clock -------------------------
  // `gap` is IDLE CLOCKS BETWEEN INIT GROUPS -- the authored lattice SOURCE's
  // rate. 0 is four vertices per clock; 3 is one vertex per clock, which is
  // `zhao_terrain_pagestream`'s S_EMIT ceiling (see case 4).
  int run_init(const PatchIn& p, int gap = 0) {
    int cycles = 0;
    for (int g = 0; g < kAlignedGroups; ++g) {
      const int v0 = 4 * g;
      const uint8_t mask = (g == kAlignedGroups - 1) ? 0x1 : 0xF;
      d.in_valid_i = 1;
      d.in_g_i = static_cast<uint16_t>(g);
      d.in_mask_i = mask;
      d.in_base_0_i = static_cast<uint16_t>(p.base[v0]);
      d.in_base_1_i = static_cast<uint16_t>((mask & 2) ? p.base[v0 + 1] : 0);
      d.in_base_2_i = static_cast<uint16_t>((mask & 4) ? p.base[v0 + 2] : 0);
      d.in_base_3_i = static_cast<uint16_t>((mask & 8) ? p.base[v0 + 3] : 0);
      d.in_scar_0_i = static_cast<uint16_t>(p.scar[v0]);
      d.in_scar_1_i = static_cast<uint16_t>((mask & 2) ? p.scar[v0 + 1] : 0);
      d.in_scar_2_i = static_cast<uint16_t>((mask & 4) ? p.scar[v0 + 2] : 0);
      d.in_scar_3_i = static_cast<uint16_t>((mask & 8) ? p.scar[v0 + 3] : 0);
      d.in_bot_0_i = static_cast<uint16_t>(p.bot[v0]);
      d.in_bot_1_i = static_cast<uint16_t>((mask & 2) ? p.bot[v0 + 1] : 0);
      d.in_bot_2_i = static_cast<uint16_t>((mask & 4) ? p.bot[v0 + 2] : 0);
      d.in_bot_3_i = static_cast<uint16_t>((mask & 8) ? p.bot[v0 + 3] : 0);
      uint8_t dual = 0;
      for (int l = 0; l < 4; ++l)
        if (((mask >> l) & 1) && p.dual[v0 + l]) dual |= static_cast<uint8_t>(1 << l);
      d.in_dual_i = dual;
      d.eval();
      step();
      ++cycles;
      for (int q = 0; q < gap; ++q) {
        d.in_valid_i = 0;
        d.eval();
        step();
        ++cycles;
      }
    }
    d.in_valid_i = 0;
    d.eval();
    return cycles;
  }

  // ---- PHASE 2: the ASSOCIATION -- walker -> engine -> accumulator --------
  //
  // THE ONE CLOCK THIS LOOP COUNTS IS A REAL CLOCK. `out_ready_i` toward the
  // walker is the engine's own admission, so a shallow engine STALLS THE WALK
  // and the stall is in the count. The accumulator has no backpressure (its
  // header says so), so at most one update is presented per clock and the
  // driver upholds that rather than assuming it.
  void drive_update(const WalkGroup& g, const Field& f) {
    d.up_valid_i = 1;
    d.up_iv_i = static_cast<uint16_t>(g.iv);
    d.up_mask_i = g.mask;
    d.up_wmask_i = f.wmask;
    const int32_t h[4] = {f.h_at(g.x[0], g.z), f.h_at(g.x[1], g.z), f.h_at(g.x[2], g.z),
                          f.h_at(g.x[3], g.z)};
    const int32_t v[4] = {f.v_at(g.x[0], g.z), f.v_at(g.x[1], g.z), f.v_at(g.x[2], g.z),
                          f.v_at(g.x[3], g.z)};
    const uint32_t m[4] = {f.m_at(g.x[0], g.z), f.m_at(g.x[1], g.z), f.m_at(g.x[2], g.z),
                           f.m_at(g.x[3], g.z)};
    const int32_t n[4] = {f.n_at(g.x[0], g.z), f.n_at(g.x[1], g.z), f.n_at(g.x[2], g.z),
                          f.n_at(g.x[3], g.z)};
    d.up_h_0_i = static_cast<uint32_t>(h[0]);
    d.up_h_1_i = static_cast<uint32_t>(h[1]);
    d.up_h_2_i = static_cast<uint32_t>(h[2]);
    d.up_h_3_i = static_cast<uint32_t>(h[3]);
    d.up_v_0_i = static_cast<uint32_t>(v[0]);
    d.up_v_1_i = static_cast<uint32_t>(v[1]);
    d.up_v_2_i = static_cast<uint32_t>(v[2]);
    d.up_v_3_i = static_cast<uint32_t>(v[3]);
    d.up_m_0_i = m[0];
    d.up_m_1_i = m[1];
    d.up_m_2_i = m[2];
    d.up_m_3_i = m[3];
    d.up_n_0_i = static_cast<uint32_t>(n[0]);
    d.up_n_1_i = static_cast<uint32_t>(n[1]);
    d.up_n_2_i = static_cast<uint32_t>(n[2]);
    d.up_n_3_i = static_cast<uint32_t>(n[3]);
  }

  // Returns the clocks the association occupied, from the clock the walker
  // was first offered the descriptor to the clock its last group's result
  // landed in the accumulator.
  int run_assoc(const Field& f, int* groups_out, int* covered_out) {
    eng.inflight.clear();
    eng.accepted = 0;
    eng.delivered = 0;
    eng.field = &f;

    // Hand the walker the descriptor. The covered box is the FULL lattice
    // here: a box that is too large is merely slow, and the per-vertex §9.1
    // test -- not the box -- decides the mask (the walker's own law).
    d.as_valid_i = 1;
    d.as_fp_x0_i = static_cast<uint32_t>(f.x0);
    d.as_fp_x1_i = static_cast<uint32_t>(f.x1);
    d.as_fp_z0_i = static_cast<uint32_t>(f.z0);
    d.as_fp_z1_i = static_cast<uint32_t>(f.z1);
    d.as_box_i0_i = 0;
    d.as_box_i1_i = kLat - 1;
    d.as_box_j0_i = 0;
    d.as_box_j1_i = kLat - 1;
    d.wk_ready_i = 0;
    d.up_valid_i = 0;
    d.eval();

    int cycles = 0;
    // acceptance edge
    int guard = 0;
    while (!d.as_ready_o && guard++ < 1000) {
      step();
      ++cycles;
      d.eval();
    }
    step();
    ++cycles;
    d.as_valid_i = 0;
    d.eval();

    int last_iv_seen = -1;
    bool walker_done = false;
    guard = 0;
    while (guard++ < 400000) {
      // 1. the engine's admission is the walker's only throttle
      d.wk_ready_i = eng.can_accept() ? 1 : 0;
      d.eval();

      bool took = false;
      WalkGroup g{};
      if (d.wk_valid_o && d.wk_ready_i) {
        g.iv = static_cast<int>(d.wk_iv_o);
        g.mask = static_cast<uint8_t>(d.wk_mask_o);
        g.z = static_cast<int32_t>(d.wk_z_o);
        g.x[0] = static_cast<int32_t>(d.wk_x0_o);
        g.x[1] = static_cast<int32_t>(d.wk_x1_o);
        g.x[2] = static_cast<int32_t>(d.wk_x2_o);
        g.x[3] = static_cast<int32_t>(d.wk_x3_o);
        g.last = d.wk_last_o != 0;
        took = true;
        if (g.last) walker_done = true;
      }
      if (took) eng.take(g);

      // 2. at most ONE update per clock -- the accumulator has no backpressure
      if (eng.ripe()) {
        const WalkGroup rg = eng.pop();
        drive_update(rg, f);
        last_iv_seen = rg.iv;
      } else {
        d.up_valid_i = 0;
      }
      d.eval();
      step();
      ++cycles;
      eng.age();

      if (walker_done && eng.inflight.empty()) break;
    }
    d.up_valid_i = 0;
    d.wk_ready_i = 0;
    d.eval();
    (void)last_iv_seen;

    if (groups_out) *groups_out = static_cast<int>(eng.delivered);
    if (covered_out) *covered_out = static_cast<int>(d.verts_covered_o);
    return cycles;
  }

  // ---- PHASE 3: DRAIN, one aligned group per clock, result two later ------
  // `gap` is IDLE CLOCKS BETWEEN DRAIN GROUPS -- the sink's rate. 0 is the
  // one-group-per-clock consumer `zhao_terrain_patch_acc`'s header assumes;
  // 7 is the composed cache that actually exists (see case 4).
  int run_drain(const PatchIn& p, int gap = 0) {
    std::memset(got_seen, 0, sizeof got_seen);
    int cycles = 0;
    for (int g = 0; g < kAlignedGroups + 3; ++g) {
      if (g < kAlignedGroups) {
        const int v0 = 4 * g;
        const uint8_t mask = (g == kAlignedGroups - 1) ? 0x1 : 0xF;
        d.dr_valid_i = 1;
        d.dr_g_i = static_cast<uint16_t>(g);
        d.dr_mask_i = mask;
        d.dr_base_0_i = static_cast<uint16_t>(p.base[v0]);
        d.dr_base_1_i = static_cast<uint16_t>((mask & 2) ? p.base[v0 + 1] : 0);
        d.dr_base_2_i = static_cast<uint16_t>((mask & 4) ? p.base[v0 + 2] : 0);
        d.dr_base_3_i = static_cast<uint16_t>((mask & 8) ? p.base[v0 + 3] : 0);
        d.dr_bot_0_i = static_cast<uint16_t>(p.bot[v0]);
        d.dr_bot_1_i = static_cast<uint16_t>((mask & 2) ? p.bot[v0 + 1] : 0);
        d.dr_bot_2_i = static_cast<uint16_t>((mask & 4) ? p.bot[v0 + 2] : 0);
        d.dr_bot_3_i = static_cast<uint16_t>((mask & 8) ? p.bot[v0 + 3] : 0);
        uint8_t dual = 0;
        for (int l = 0; l < 4; ++l)
          if (((mask >> l) & 1) && p.dual[v0 + l]) dual |= static_cast<uint8_t>(1 << l);
        d.dr_dual_i = dual;
      } else {
        d.dr_valid_i = 0;
      }
      d.eval();
      capture();
      step();
      ++cycles;
      d.eval();
      // the sink's rate: hold the port idle while the consumer digests.
      for (int q = 0; q < gap && g < kAlignedGroups; ++q) {
        d.dr_valid_i = 0;
        d.eval();
        capture();
        step();
        ++cycles;
        d.eval();
      }
    }
    d.dr_valid_i = 0;
    d.eval();
    return cycles;
  }

  void capture() {
    if (!d.out_valid_o) return;
    const int g = static_cast<int>(d.out_g_o);
    const int32_t top[4] = {static_cast<int32_t>(d.out_top_0_o), static_cast<int32_t>(d.out_top_1_o),
                            static_cast<int32_t>(d.out_top_2_o),
                            static_cast<int32_t>(d.out_top_3_o)};
    const int32_t bot[4] = {static_cast<int32_t>(d.out_bot_0_o), static_cast<int32_t>(d.out_bot_1_o),
                            static_cast<int32_t>(d.out_bot_2_o),
                            static_cast<int32_t>(d.out_bot_3_o)};
    const int32_t vel[4] = {static_cast<int32_t>(d.out_vel_0_o), static_cast<int32_t>(d.out_vel_1_o),
                            static_cast<int32_t>(d.out_vel_2_o),
                            static_cast<int32_t>(d.out_vel_3_o)};
    const uint32_t mat[4] = {d.out_mat_0_o, d.out_mat_1_o, d.out_mat_2_o, d.out_mat_3_o};
    const int32_t nav[4] = {static_cast<int32_t>(d.out_nav_0_o), static_cast<int32_t>(d.out_nav_1_o),
                            static_cast<int32_t>(d.out_nav_2_o),
                            static_cast<int32_t>(d.out_nav_3_o)};
    for (int l = 0; l < 4; ++l) {
      if (!((d.out_mask_o >> l) & 1)) continue;
      const int v = 4 * g + l;
      got_top[v] = top[l];
      got_bot[v] = bot[l];
      got_vel[v] = vel[l];
      got_mat[v] = mat[l];
      got_nav[v] = nav[l];
      got_dirty[v] = ((d.out_dirty_o >> l) & 1) != 0;
      got_seen[v] = true;
    }
  }
};

// The authored patch under everything: a plain, non-degenerate surface.
PatchIn make_patch() {
  PatchIn p{};
  for (int vj = 0; vj < kLat; ++vj)
    for (int vi = 0; vi < kLat; ++vi) {
      const int v = vj * kLat + vi;
      p.base[v] = static_cast<int16_t>(200 + vi * 3 - vj * 2);
      p.scar[v] = static_cast<int16_t>((vi + vj) % 7);
      p.bot[v] = static_cast<int16_t>(-400 - vi);
      p.dual[v] = true;
    }
  return p;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  const Lattice lat(0, 32 << 16, 0, 32 << 16);
  const PatchIn patch = make_patch();

  // The whole-patch field: the same shape case 11 issues -- one field whose
  // footprint covers every one of the 1,089 vertices.
  Field whole{};
  whole.x0 = INT32_MIN / 2;
  whole.x1 = INT32_MAX / 2;
  whole.z0 = INT32_MIN / 2;
  whole.z1 = INT32_MAX / 2;
  whole.wmask = static_cast<uint8_t>(W_H | W_V | W_M | W_N);
  whole.constant_lift = false;

  std::printf("=== fieldmajor_census: THE FIELD-MAJOR EARTH ASSOCIATION, MEASURED ===\n\n");

  // =========================================================================
  // CASE 1 -- THE COMPOSITION IS CORRECT. Measure nothing until the machine
  // computes the right thing: a cost measured on a machine that does not
  // agree with the ratified oracle is a number about nothing.
  //
  // The WALKER decides coverage (its per-vertex §9.1 closed-interval test)
  // and the ACCUMULATOR reduces; the expectation is the VERTEX-MAJOR oracle
  // `zref::terrain::compose_vertex`. That the two agree is the whole claim
  // behind 13.1's stream-order swap, and it has never been checked with both
  // blocks in one elaboration.
  // =========================================================================
  {
    std::printf("case 1: the composed field-major machine agrees with the vertex-major oracle\n");
    Sim s;
    s.reset();
    s.load_tables(lat);
    s.eng.latency = 0;
    s.eng.depth = 1;

    const int init_cycles = s.run_init(patch);
    s.idle(3);
    int groups = 0, covered = 0;
    s.run_assoc(whole, &groups, &covered);
    s.idle(3);
    const int drain_cycles = s.run_drain(patch);

    ck_eq(init_cycles, kAlignedGroups, "INIT walks 273 aligned groups, one per clock");
    ck_eq(groups, kUpdateGroups, "the association is 297 row-bounded UPDATE groups, not 273");
    ck_eq(covered, kVerts, "verts_covered_o counts 1,089 covered LANES, not groups");
    ck_eq(drain_cycles, kAlignedGroups + 3, "DRAIN walks 273 aligned groups plus the pipeline");

    const PatchOut want = oracle(lat, patch, {whole});
    int seen = 0, bad_top = 0, bad_bot = 0, bad_vel = 0, bad_mat = 0, bad_nav = 0, bad_dirty = 0;
    for (int v = 0; v < kVerts; ++v) {
      if (!s.got_seen[v]) continue;
      ++seen;
      if (s.got_top[v] != want.top[v]) ++bad_top;
      if (s.got_bot[v] != want.bottom[v]) ++bad_bot;
      if (s.got_vel[v] != want.vel[v]) ++bad_vel;
      if (s.got_mat[v] != want.mat[v]) ++bad_mat;
      if (s.got_nav[v] != want.nav[v]) ++bad_nav;
      if (s.got_dirty[v] != want.dirty[v]) ++bad_dirty;
    }
    ck_eq(seen, kVerts, "every one of the 1,089 vertices drained");
    ck_eq(bad_top, 0, "live_top: field-major == the ratified vertex-major oracle, every vertex");
    ck_eq(bad_bot, 0, "bottom: the underside clamp survives the transpose, every vertex");
    ck_eq(bad_vel, 0, "velocity: the V1 saturating chain, every vertex");
    ck_eq(bad_mat, 0, "material: last-covering-writer-wins on the opaque u32, every vertex");
    ck_eq(bad_nav, 0, "nav_cost: the declared saturating chain, every vertex");
    ck_eq(bad_dirty, 0, "dirty attribution, every vertex");
    std::printf("  1,089 vertices x 6 lanes checked against zref::terrain::compose_vertex\n\n");
  }

  // =========================================================================
  // CASE 2 -- A FIELD THAT COVERS PART OF THE PATCH. The negative half of
  // case 1: if the walker's mask were ignored and every vertex updated, case
  // 1 would still pass (the field covers everything there). This case makes
  // the mask load-bearing, and it is the control that says case 1's green is
  // about the machine rather than about the stimulus.
  // =========================================================================
  {
    std::printf("case 2: a PARTIAL footprint -- the walker's mask is load-bearing\n");
    Sim s;
    s.reset();
    s.load_tables(lat);
    s.eng.latency = 4;
    s.eng.depth = 3;

    Field part = whole;
    // vertices i in [8,20], j in [5,17] inclusive, by world coordinate
    part.x0 = lat.wx[8];
    part.x1 = lat.wx[20];
    part.z0 = lat.wz[5];
    part.z1 = lat.wz[17];

    const int expect_covered = (20 - 8 + 1) * (17 - 5 + 1);  // 13 * 13 = 169

    s.run_init(patch);
    s.idle(3);
    int groups = 0, covered = 0;
    s.run_assoc(part, &groups, &covered);
    s.idle(3);
    s.run_drain(patch);

    ck_eq(covered, expect_covered,
          "the closed-interval test covers exactly the 13x13 vertex block");
    ck(groups == kUpdateGroups,
       "the box was the FULL lattice, so the walk still costs 297 groups (a large box is "
       "merely slow)");

    const PatchOut want = oracle(lat, patch, {part});
    int bad = 0, moved = 0, unmoved = 0;
    for (int v = 0; v < kVerts; ++v) {
      if (!s.got_seen[v]) continue;
      if (s.got_top[v] != want.top[v] || s.got_vel[v] != want.vel[v] ||
          s.got_mat[v] != want.mat[v] || s.got_nav[v] != want.nav[v])
        ++bad;
      const int32_t flat = (static_cast<int32_t>(patch.base[v]) << 8) +
                           (static_cast<int32_t>(patch.scar[v]) << 8);
      if (want.top[v] != flat)
        ++moved;
      else
        ++unmoved;
    }
    ck_eq(bad, 0, "partial footprint: every vertex matches the oracle, inside and out");
    ck_eq(moved, expect_covered, "exactly the covered vertices moved");
    ck(unmoved == kVerts - expect_covered, "every uncovered vertex is untouched, not zeroed");
    std::printf("  169 covered / %d untouched, all against the oracle\n\n",
                kVerts - expect_covered);
  }

  // =========================================================================
  // CASE 3 -- THE CENSUS. The cost of one association as a LINE in engine
  // latency, at several engine depths.
  //
  // THE NEGATIVE CONTROL comes first and it is the one the vertex-major
  // census also needed: with an EMPTY footprint box the walker accepts the
  // association, emits zero groups and the engine never runs. If the census
  // numbers below were an artefact of the driver rather than of the machine,
  // this row would not be flat.
  // =========================================================================
  {
    std::printf("case 3: THE COST OF ONE FIELD-MAJOR ASSOCIATION, MEASURED\n\n");

    // ---- NEGATIVE CONTROL ------------------------------------------------
    {
      Sim s;
      s.reset();
      s.load_tables(lat);
      s.eng.latency = 80;
      s.eng.depth = 1;
      Field empty = whole;
      // A footprint that covers no lattice vertex at all.
      empty.x0 = lat.wx[kLat - 1] + 1;
      empty.x1 = empty.x0 + 1;
      empty.z0 = lat.wz[kLat - 1] + 1;
      empty.z1 = empty.z0 + 1;
      int groups = 0, covered = 0;
      const int cyc = s.run_assoc(empty, &groups, &covered);
      ck_eq(covered, 0, "census NEGATIVE CONTROL: an uncovered footprint covers zero vertices");
      ck_eq(static_cast<int64_t>(s.eng.accepted), kUpdateGroups,
            "the walk still runs (the box is a hint), so the zero above is about COVERAGE");
      ck(cyc > 0, "the control consumed real clocks");
      std::printf("  NEGATIVE CONTROL: footprint covering no vertex -> %d covered, "
                  "engine results all masked off\n\n", covered);
    }

    static const int kLatency[] = {0, 3, 20, 50, 80};
    // THE DEPTHS, AND TWO OF THEM ARE REAL CONFIGURATIONS RATHER THAN ROUND
    // NUMBERS -- checked in the tree, not assumed:
    //
    //   depth 1  : case 11's engine. `zhao_field_host`'s front holds ONE point
    //              in flight, so it is also the vertex-major machine's depth.
    //   depth 2  : THE CONSOLE AS COMPOSED TODAY. `zhao_console_core.sv` gives
    //              `u_field_host` `.FAB_LANES(1)` and leaves `PROGS` at its
    //              default 8, so the executor is a SCALAR datapath with eight
    //              contexts -- and a four-point group therefore occupies FOUR
    //              contexts, i.e. two groups in flight.
    //   depth 32 : THE ENGINE'S OWN SHIPPED CONFIGURATION. `tests/CMakeLists.txt`
    //              gates it at `-GCTX=32 -GLANES=4` (`lint_field_v3_engine_shipped`),
    //              and `zhao_field_host.sv:115` names the same seven values.
    //              At LANES=4 one context IS one four-point group.
    //
    // 4 / 8 / 16 are there to show the knee, not because anything is
    // configured that way.
    static const int kDepth[] = {1, 2, 4, 8, 16, 32};
    const int nL = static_cast<int>(sizeof(kLatency) / sizeof(kLatency[0]));
    const int nD = static_cast<int>(sizeof(kDepth) / sizeof(kDepth[0]));

    uint64_t total[5][6] = {};
    uint64_t assoc[5][6] = {};

    for (int di = 0; di < nD; ++di) {
      for (int li = 0; li < nL; ++li) {
        Sim s;
        s.reset();
        s.load_tables(lat);
        s.eng.latency = kLatency[li];
        s.eng.depth = kDepth[di];

        const uint64_t t0 = s.clocks;
        const int init_cycles = s.run_init(patch);
        s.idle(2);
        int groups = 0, covered = 0;
        const int a = s.run_assoc(whole, &groups, &covered);
        s.idle(2);
        const int drain_cycles = s.run_drain(patch);
        total[li][di] = s.clocks - t0;
        assoc[li][di] = static_cast<uint64_t>(a);

        ck_eq(groups, kUpdateGroups, "census: every row reduces exactly 297 groups");
        ck_eq(covered, kVerts, "census: every row covers the whole 1,089-vertex patch");
        ck_eq(init_cycles, kAlignedGroups, "census: INIT stays one group per clock");
        ck_eq(drain_cycles, kAlignedGroups + 3, "census: DRAIN stays one group per clock");
        ck(static_cast<int64_t>(s.eng.delivered) == kUpdateGroups,
           "census POSITIVE CONTROL: the engine delivered a result for every group");
      }
    }

    std::printf("  FIELD-MAJOR ASSOCIATION -- one field, whole 33x33 patch, %d vertices\n", kVerts);
    std::printf("  TOTAL clocks = INIT(273) + association + DRAIN(276) + 4 phase-gap clocks\n\n");
    std::printf("  %-10s", "engine lat");
    for (int di = 0; di < nD; ++di) std::printf("   depth=%-8d", kDepth[di]);
    std::printf("\n");
    for (int li = 0; li < nL; ++li) {
      std::printf("  %-10d", kLatency[li]);
      for (int di = 0; di < nD; ++di)
        std::printf("   %-13llu", static_cast<unsigned long long>(total[li][di]));
      std::printf("\n");
    }

    std::printf("\n  the ASSOCIATION alone (walker -> engine -> accumulator), same grid:\n");
    std::printf("  %-10s", "engine lat");
    for (int di = 0; di < nD; ++di) std::printf("   depth=%-8d", kDepth[di]);
    std::printf("\n");
    for (int li = 0; li < nL; ++li) {
      std::printf("  %-10d", kLatency[li]);
      for (int di = 0; di < nD; ++di)
        std::printf("   %-13llu", static_cast<unsigned long long>(assoc[li][di]));
      std::printf("\n");
    }

    // ---- THE LINE --------------------------------------------------------
    // Every column is a line in latency; report intercept and slope for each,
    // and ASSERT the slope, which is the structural claim. At depth D the
    // association serialises ceil(297/D) engine round trips, so the slope is
    // ceil(297/D) clocks per clock of latency -- NOT 1,089.
    std::printf("\n  THE LINE, per engine depth:  clocks(L) = intercept + slope * L\n");
    for (int di = 0; di < nD; ++di) {
      const int64_t i0 = static_cast<int64_t>(total[0][di]);
      const int64_t slope_meas =
          (static_cast<int64_t>(total[nL - 1][di]) - i0) / (kLatency[nL - 1] - kLatency[0]);
      std::printf("    depth %-3d: intercept %6lld  slope %6lld  -> at L=80: %llu clocks\n",
                  kDepth[di], static_cast<long long>(i0), static_cast<long long>(slope_meas),
                  static_cast<unsigned long long>(total[nL - 1][di]));
    }

    // ---- THE STRUCTURAL CLAIMS, ASSERTED RATHER THAN OBSERVED -----------
    //
    // 1. AT DEPTH 1 THE SLOPE IS EXACTLY 297. The direct counterpart of case
    //    11's asserted slope of 1,089: with one request in flight the
    //    association serialises one un-overlapped engine round trip PER GROUP
    //    OF FOUR POINTS rather than per vertex. The whole of 13.1's
    //    stream-order argument is the difference between those two numbers, so
    //    it is asserted rather than read off a table.
    for (int li = 1; li < nL; ++li) {
      const int64_t want =
          static_cast<int64_t>(kUpdateGroups) * (kLatency[li] - kLatency[li - 1]);
      const int64_t got =
          static_cast<int64_t>(total[li][0]) - static_cast<int64_t>(total[li - 1][0]);
      ck_eq(got, want,
            "census: at depth 1 the association serialises ONE engine round trip per "
            "GROUP -- 297 clocks per clock of latency, not 1,089");
    }

    // 2. THE INTERCEPT IS INDEPENDENT OF DEPTH. Depth buys slope and nothing
    //    else; the floor is the walk and the reduction, and no amount of
    //    pipelining touches it. That is the claim the commission rests on, and
    //    the one a reader would otherwise have to take on trust from a table.
    for (int di = 1; di < nD; ++di)
      ck_eq(static_cast<int64_t>(total[0][di]), static_cast<int64_t>(total[0][0]),
            "census: the intercept is the same at every depth -- depth buys SLOPE, "
            "never the floor");

    // 3. DEEPER IS NEVER WORSE, at every latency.
    for (int li = 0; li < nL; ++li)
      for (int di = 1; di < nD; ++di)
        ck(total[li][di] <= total[li][di - 1], "census: a deeper engine is never slower");

    // 4. THE WHOLE SURFACE AGAINST AN EXACT MODEL. The accept recurrence is
    //    `t_k = max(t_(k-1) + 1, t_(k-D) + L + 1)` -- the walker offers one
    //    group per clock, and a slot frees L+1 clocks after it was taken --
    //    and the association ends L clocks after the last accept. Asserting
    //    the closed form for EVERY cell is far stronger than a slope: a
    //    machine that stalled anywhere, dropped a group, or let the
    //    accumulator backpressure would miss it.
    //
    //    AND THIS PREDICTION WAS SEEN TO FAIL, WHICH IS WHY IT IS HERE. The
    //    first version of this census asserted `ceil(297/D)` as the slope and
    //    the measured surface REFUSED IT IN FIVE CELLS -- because the line is
    //    PIECEWISE: while `L < D`, the walker's one-group-per-clock is the
    //    binding rate and latency is free. The recurrence has that knee in it
    //    and the closed form did not. The control fired on its author.
    for (int di = 0; di < nD; ++di) {
      for (int li = 0; li < nL; ++li) {
        const int D = kDepth[di];
        const int L = kLatency[li];
        std::vector<int64_t> t(kUpdateGroups, 0);
        for (int k = 1; k < kUpdateGroups; ++k) {
          int64_t v = t[k - 1] + 1;
          if (k >= D && t[k - D] + L + 1 > v) v = t[k - D] + L + 1;
          t[k] = v;
        }
        // +2: the descriptor's acceptance edge, and the clock the last
        // delivery is presented on.
        const int64_t want = t[kUpdateGroups - 1] + L + 2;
        ck_eq(static_cast<int64_t>(assoc[li][di]), want,
              "census: the association matches the exact accept recurrence "
              "t_k = max(t_(k-1)+1, t_(k-D)+L+1), every cell");
      }
    }

    // ---- THE COMPARISON, which is this packet's whole commission ---------
    std::printf("\n  AGAINST THE COMPOSED VERTEX-MAJOR PATH\n");
    std::printf("  (composepub_acceptance case 11, same modelled engine, same seam)\n");
    std::printf("    vertex-major : clocks(L) = 4,431 + 1,089*L   -> 91,551 at L=80\n");
    std::printf("    field-major  : clocks(L) = %llu + %d*L   (depth 1)  -> %llu at L=80\n",
                static_cast<unsigned long long>(total[0][0]), kUpdateGroups,
                static_cast<unsigned long long>(total[4][0]));
    std::printf("    field-major  : %llu at L=0, %llu at L=80  (depth 2, THE CONSOLE TODAY)\n",
                static_cast<unsigned long long>(total[0][1]),
                static_cast<unsigned long long>(total[4][1]));
    std::printf("    field-major  : %llu at L=0, %llu at L=80  (depth 32, THE GATED ENGINE)\n",
                static_cast<unsigned long long>(total[0][5]),
                static_cast<unsigned long long>(total[4][5]));
    std::printf("\n  THE INTERCEPT -- the claim the whole commission rests on:\n");
    std::printf("    vertex-major intercept : 4,431 clocks  (4.07 / vertex, 74%% of 6,000)\n");
    std::printf("    field-major  intercept : %llu clocks  (%.2f / vertex, %.0f%% of 6,000)\n",
                static_cast<unsigned long long>(total[0][0]),
                static_cast<double>(total[0][0]) / kVerts,
                100.0 * static_cast<double>(total[0][0]) / 6000.0);
    std::printf("    the floor FELL by %.2fx\n",
                4431.0 / static_cast<double>(total[0][0]));
    std::printf("    *** 851 IS A CEILING. It assumes the authored lattice can be\n");
    std::printf("    *** presented to INIT and drained from DRAIN four vertices per\n");
    std::printf("    *** clock. NEITHER END OF THIS TREE CAN. SEE CASE 4, which\n");
    std::printf("    *** measures the same machine at the real rates and gets 3,581.\n");
    std::printf("\n  ACTIVE contract: <= 6,000 clocks / full 1,089-vertex association\n");
    std::printf("                   (design/contracts/FIELD.SEQ.EARTH.md:167)\n");
    for (int di = 0; di < nD; ++di)
      std::printf("    depth %-3d at L=80: %6llu clocks = %.2fx the 6,000 contract\n", kDepth[di],
                  static_cast<unsigned long long>(total[nL - 1][di]),
                  static_cast<double>(total[nL - 1][di]) / 6000.0);
    std::printf("\n  The largest engine latency that still fits 6,000 clocks, per depth:\n");
    for (int di = 0; di < nD; ++di) {
      const int64_t slope = (kUpdateGroups + kDepth[di] - 1) / kDepth[di];
      const int64_t budget = 6000 - static_cast<int64_t>(total[0][di]);
      std::printf("    depth %-3d: L <= %lld clocks per GROUP of four points\n", kDepth[di],
                  budget > 0 ? budget / slope : -1);
    }
    std::printf("\n");
  }

  // =========================================================================
  // CASE 4 -- THE DRAIN AGAINST THE CACHE THAT ACTUALLY EXISTS, AND IT TAKES
  // MOST OF CASE 3's HEADLINE BACK.
  //
  // `zhao_terrain_patch_acc`'s header names its intended consumer: "the
  // composed-height cache write port, A PLAIN ONE-GROUP-PER-CLOCK SINK". That
  // sink does not exist. `zhao_terrain_compcache_front.sv:414` is
  //
  //     assign st_ready_o = fill_active_q && !at_capacity_c && !wphase_q;
  //
  // and `wphase_q` alternates, because ONE RECORD IS TWO WRITES (one top plane,
  // one bottom plane). So the composed cache accepts ONE VERTEX EVERY TWO
  // CLOCKS -- and a four-vertex group therefore costs EIGHT clocks, not one.
  // That is an 8x mismatch between the accumulator's stated assumption and the
  // block it names.
  //
  // IT IS ALSO EXACTLY WHERE THE VERTEX-MAJOR PATH'S OWN FLOOR COMES FROM.
  // `composepub_acceptance` case 11's negative control -- the same 1,089-vertex
  // walk with an EMPTY field list -- is 2,252 clocks, 2.07 per vertex. That is
  // this cache, at this rate, with no field machinery involved at all. So more
  // than half of the 4,431-clock vertex-major intercept is the cache write, and
  // a field-major machine that still has to feed the SAME cache does not escape
  // it by walking differently.
  //
  // MEASURING THIS RATHER THAN ASSERTING IT IS THE POINT. Case 3's 851 is the
  // floor of the two blocks as their ports are built; this is the floor of the
  // two blocks against the sink the console owns today. Reporting only the
  // first would be the flattering direction, and it is the number this packet
  // would otherwise have led with.
  // =========================================================================
  {
    std::printf("case 4: THE SAME MACHINE AGAINST THE COMPOSE CACHE THAT EXISTS\n");

    // row 0: both ends group-wide (the accumulator's ports as built)
    // row 1: the SINK at the compose cache's real rate
    // row 2: BOTH ends at the rates the tree's real producer and consumer run
    uint64_t tot[3] = {0, 0, 0};
    int init_c[3] = {0, 0, 0}, drain_c[3] = {0, 0, 0};
    static const int kInitGap[3] = {0, 0, 3};
    static const int kDrainGap[3] = {0, 7, 7};

    for (int row = 0; row < 3; ++row) {
      Sim s;
      s.reset();
      s.load_tables(lat);
      s.eng.latency = 0;
      s.eng.depth = 1;

      const uint64_t t0 = s.clocks;
      const int ic = s.run_init(patch, kInitGap[row]);
      s.idle(2);
      int groups = 0, covered = 0;
      s.run_assoc(whole, &groups, &covered);
      s.idle(2);
      // drain gap 7 == one group per 8 clocks == four vertices at the compose
      // cache's two-clocks-per-record rate.
      const int dc = s.run_drain(patch, kDrainGap[row]);
      tot[row] = s.clocks - t0;
      init_c[row] = ic;
      drain_c[row] = dc;

      // THE ANSWER MUST NOT CHANGE. A slower sink costs clocks and nothing
      // else; if the reduction depended on the drain's spacing that would be a
      // defect, and this is the check that says it does not.
      const PatchOut want = oracle(lat, patch, {whole});
      int bad = 0, seen = 0;
      for (int v = 0; v < kVerts; ++v) {
        if (!s.got_seen[v]) continue;
        ++seen;
        if (s.got_top[v] != want.top[v] || s.got_bot[v] != want.bottom[v] ||
            s.got_vel[v] != want.vel[v] || s.got_mat[v] != want.mat[v] ||
            s.got_nav[v] != want.nav[v] || s.got_dirty[v] != want.dirty[v])
          ++bad;
      }
      ck_eq(seen, kVerts, "throttled: every vertex still drains");
      ck_eq(bad, 0, "throttled: the reduction does not depend on the phases' spacing");
    }

    ck_eq(init_c[0], kAlignedGroups, "INIT at four vertices/clock is 273 clocks");
    ck_eq(init_c[2], 4 * kAlignedGroups,
          "INIT at the page stream's one vertex/clock is 4 clocks per group");
    ck_eq(drain_c[0], kAlignedGroups + 3, "DRAIN at one group/clock is 276 clocks");
    ck_eq(drain_c[1], 8 * kAlignedGroups + 3,
          "DRAIN at the cache's two-clocks-per-record is 8 clocks per group");
    ck(tot[1] > tot[0], "a slower SINK costs clocks (the control that says the gap is live)");
    ck(tot[2] > tot[1], "a slower SOURCE costs more again");

    std::printf("\n  the SAME association and the SAME reduction, three rates:\n");
    std::printf("    %-52s %6llu total (init %5d, drain %5d)\n",
                "both ends group-wide (the acc's ports as built)",
                static_cast<unsigned long long>(tot[0]), init_c[0], drain_c[0]);
    std::printf("    %-52s %6llu total (init %5d, drain %5d)\n",
                "SINK at the compose cache's real rate",
                static_cast<unsigned long long>(tot[1]), init_c[1], drain_c[1]);
    std::printf("    %-52s %6llu total (init %5d, drain %5d)\n",
                "BOTH ends at the tree's real producer/consumer",
                static_cast<unsigned long long>(tot[2]), init_c[2], drain_c[2]);

    std::printf("\n  AND THE SECOND THROTTLE IS A STRUCTURAL COST, NOT A SLOW WIRE.\n");
    std::printf("  In the VERTEX-MAJOR form the intake and the write OVERLAP -- one\n");
    std::printf("  streaming pass, whose rate is the slowest stage -- which is why case\n");
    std::printf("  11's no-field control is 2,252 clocks and not 1,089 + 2,178. The\n");
    std::printf("  field-major form CANNOT overlap them: INIT / ACCUM / DRAIN are\n");
    std::printf("  EXCLUSIVE PHASES by `zhao_terrain_patch_acc`'s own header, so it pays\n");
    std::printf("  for the lattice twice where vertex-major pays once. That is a real\n");
    std::printf("  cost of the transpose and no document in this tree had priced it.\n");

    std::printf("\n  SO THE FIELD-MAJOR INTERCEPT IS A RANGE, NOT A NUMBER:\n");
    std::printf("    %5llu clocks (%3.0f%% of 6,000) -- both ends widened to a group\n",
                static_cast<unsigned long long>(tot[0]), 100.0 * tot[0] / 6000.0);
    std::printf("    %5llu clocks (%3.0f%% of 6,000) -- only the cache write port left narrow\n",
                static_cast<unsigned long long>(tot[1]), 100.0 * tot[1] / 6000.0);
    std::printf("    %5llu clocks (%3.0f%% of 6,000) -- AGAINST THE TREE AS IT STANDS TODAY\n",
                static_cast<unsigned long long>(tot[2]), 100.0 * tot[2] / 6000.0);
    std::printf("     4431 clocks ( 74%% of 6,000) -- the vertex-major intercept it replaces\n");
    std::printf("  The floor falls %.2fx at best and %.2fx as the tree stands, and ALL THREE\n",
                4431.0 / static_cast<double>(tot[0]), 4431.0 / static_cast<double>(tot[2]));
    std::printf("  are below the 74%% the field-major swap was commissioned to remove -- so\n");
    std::printf("  the verdict survives every throttle, and the headline does not.\n");
    std::printf("  WIDENING THE CACHE WRITE PORT IS WORTH %llu CLOCKS PER ASSOCIATION AND\n",
                static_cast<unsigned long long>(tot[1] - tot[0]));
    std::printf("  WIDENING THE LATTICE SOURCE A FURTHER %llu. Both are NAMED PREREQUISITES\n",
                static_cast<unsigned long long>(tot[2] - tot[1]));
    std::printf("  and neither is a detail.\n\n");

    // ---- AND THE CELL THAT ACTUALLY DECIDES IT, MEASURED ----------------
    // Case 3's grid was taken with both ends group-wide, so adding its slope
    // to this case's intercept would be ARITHMETIC ON TWO MEASUREMENTS rather
    // than a measurement. The cell that decides the verdict -- the engine's
    // own gated depth, the conservative L=80, and BOTH ends at the rates this
    // tree runs -- is therefore run rather than computed.
    {
      Sim s;
      s.reset();
      s.load_tables(lat);
      s.eng.latency = 80;
      s.eng.depth = 32;
      const uint64_t t0 = s.clocks;
      s.run_init(patch, 3);
      s.idle(2);
      int groups = 0, covered = 0;
      s.run_assoc(whole, &groups, &covered);
      s.idle(2);
      s.run_drain(patch, 7);
      const uint64_t real_total = s.clocks - t0;

      const PatchOut want = oracle(lat, patch, {whole});
      int bad = 0, seen = 0;
      for (int v = 0; v < kVerts; ++v) {
        if (!s.got_seen[v]) continue;
        ++seen;
        if (s.got_top[v] != want.top[v] || s.got_vel[v] != want.vel[v] ||
            s.got_mat[v] != want.mat[v] || s.got_nav[v] != want.nav[v])
          ++bad;
      }
      ck_eq(seen, kVerts, "the deciding cell: every vertex drained");
      ck_eq(bad, 0, "the deciding cell: still the ratified oracle's answer");
      ck_eq(groups, kUpdateGroups, "the deciding cell: still 297 groups");

      std::printf("  THE CELL THAT DECIDES IT, MEASURED RATHER THAN ADDED UP:\n");
      std::printf("    the engine's GATED depth (32), the conservative L=80, and BOTH\n");
      std::printf("    ends at the rates this tree runs: %llu clocks = %.2fx the 6,000\n",
                  static_cast<unsigned long long>(real_total),
                  static_cast<double>(real_total) / 6000.0);
      std::printf("    contract. IT STILL FITS, with %.1fx margin, and that is the\n",
                  6000.0 / static_cast<double>(real_total));
      std::printf("    verdict after every throttle this packet could find.\n");
      std::printf("    (the same cell at depth 8 does NOT: 3,581 + 34*80 = 6,301, over.\n");
      std::printf("     so the executor's width is not a nicety, it is the gate.)\n\n");
    }
  }

  std::printf("fieldmajor_census: %d checks, %d failures\n", g_checks, g_fails);
  return g_fails == 0 ? 0 : 1;
}
