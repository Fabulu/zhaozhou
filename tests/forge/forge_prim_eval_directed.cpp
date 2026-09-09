// forge_prim_eval_directed.cpp — the lightning evaluator against its oracle.
//
// ---------------------------------------------------------------------------
// WHAT THIS TEST HOLDS
// ---------------------------------------------------------------------------
// 1. BIT-EXACTNESS: every emitted position equals zref::forge::eval_job,
//    component for component, across directed cases and a randomized sweep of
//    the full legal domain (anchors anywhere in s32, including start == end).
// 2. DETERMINISM UNDER STALLS: the same job under always-ready, periodic,
//    pseudo-random and 90%-stall consumer patterns produces the identical
//    byte stream. The capture-CRC discipline depends on this, and it is the
//    property a per-cycle jitter stream would silently break.
// 3. CAPS REFUSED, NOT CLAMPED: segments 0 and 25, branch_count 3, branch
//    segments 0 and 9, attach past the end — each refused with the counter
//    moving and NOTHING emitted.
// 4. COUNTERS SEEN TO MOVE: jobs, points, vertices, refused_limit,
//    skipped_view and sat_events all fire under legal stimulus here.
//    walk_overrun_o CANNOT fire under legal stimulus while the walk is
//    correct — it is asserted zero here, and its positive control is
//    tests/forge/forge_prim_eval_overrun_control.cpp driving the committed
//    mutant (inverse polarity, per the CLAUDE.md mutant law).
// 5. THE CHECKER SEEN TO FAIL: a deliberately corrupted expectation is fed to
//    the same comparator and must be detected — a comparator that cannot
//    fail is a broken instrument reading reassurance.
// 6. THE SEAM: vertex counts match ring-major vidx(s,k,2) that
//    zhao_forge_prim's ribbon indices reference (via zref::forge).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_forge_prim_eval.h"

#include "zhao_sim.hpp"
#include "zref/zref_forge.hpp"
#include "zref/zref_forge_eval.hpp"

using zref::forge::EvalCounts;
using zref::forge::EvalParams;
using zref::forge::EvalVec3;

namespace {

struct OutVertex {
  int32_t x, y, z;
  int poly;
  int last;
};

// test-local deterministic RNG (not the DUT's law — just stimulus)
uint32_t g_rng = 0xC0FFEE01u;
uint32_t rnd() {
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 17;
  g_rng ^= g_rng << 5;
  return g_rng;
}

void apply(Vzhao_forge_prim_eval& top, const EvalParams& p) {
  top.j_start_x_i = p.start.x;
  top.j_start_y_i = p.start.y;
  top.j_start_z_i = p.start.z;
  top.j_end_x_i = p.end.x;
  top.j_end_y_i = p.end.y;
  top.j_end_z_i = p.end.z;
  top.j_perp1_x_i = p.perp1.x;
  top.j_perp1_y_i = p.perp1.y;
  top.j_perp1_z_i = p.perp1.z;
  top.j_perp2_x_i = p.perp2.x;
  top.j_perp2_y_i = p.perp2.y;
  top.j_perp2_z_i = p.perp2.z;
  top.j_waxis_x_i = p.waxis.x;
  top.j_waxis_y_i = p.waxis.y;
  top.j_waxis_z_i = p.waxis.z;
  top.j_half_width_i = p.half_width;
  top.j_branch_half_width_i = p.branch_half_width;
  top.j_amp_i = p.amp;
  top.j_branch_amp_i = p.branch_amp;
  top.j_seed_i = p.seed;
  top.j_tick_phase_i = p.tick_phase;
  top.j_segments_i = p.segments & 0x7F;
  top.j_branch_count_i = p.branch_count & 3;
  top.j_br0_attach_i = p.br[0].attach & 0x7F;
  top.j_br0_segments_i = p.br[0].segments & 0xF;
  top.j_br0_end_x_i = p.br[0].end.x;
  top.j_br0_end_y_i = p.br[0].end.y;
  top.j_br0_end_z_i = p.br[0].end.z;
  top.j_br1_attach_i = p.br[1].attach & 0x7F;
  top.j_br1_segments_i = p.br[1].segments & 0xF;
  top.j_br1_end_x_i = p.br[1].end.x;
  top.j_br1_end_y_i = p.br[1].end.y;
  top.j_br1_end_z_i = p.br[1].end.z;
  top.j_view_mask_i = p.view_mask & 3;
}

void hard_reset(Vzhao_forge_prim_eval& top) {
  top.j_valid_i = 0;
  top.v_ready_i = 0;
  top.view_sel_i = 1;
  top.rst_n = 0;
  top.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
  zhao::tick(top);
}

// Offer the job, wait for the accept/refuse/skip handshake.
void submit(Vzhao_forge_prim_eval& top, const EvalParams& p, int view_sel) {
  apply(top, p);
  top.view_sel_i = view_sel & 3;
  top.j_valid_i = 1;
  for (int w = 0; w < 1000; ++w) {
    top.eval();
    if (top.j_ready_o) {
      zhao::tick(top);
      top.j_valid_i = 0;
      return;
    }
    zhao::tick(top);
  }
  zhao::check(false, "job handshake hung", 1, 0);
  top.j_valid_i = 0;
}

// Stall patterns. 0: always ready. 1: every 3rd cycle. 2: pseudo-random 50%.
// 3: ready only 1 cycle in 10 (heavy backpressure).
bool ready_now(int pattern, int cycle, uint32_t& lrng) {
  switch (pattern) {
    case 0: return true;
    case 1: return (cycle % 3) == 0;
    case 2:
      lrng ^= lrng << 13;
      lrng ^= lrng >> 17;
      lrng ^= lrng << 5;
      return (lrng & 1) != 0;
    default: return (cycle % 10) == 0;
  }
}

// Collect the whole vertex stream of an already-submitted job. g_last_cycles
// reports how long the walk took — the directed rate measurement.
int g_last_cycles = 0;
std::vector<OutVertex> collect(Vzhao_forge_prim_eval& top, int pattern, int max_cycles = 400000) {
  std::vector<OutVertex> out;
  uint32_t lrng = 0xB0B0CAFEu;
  bool saw_last = false;
  for (int c = 0; c < max_cycles && !saw_last; ++c) {
    top.v_ready_i = ready_now(pattern, c, lrng) ? 1 : 0;
    top.eval();
    if (top.v_valid_o && top.v_ready_i) {
      OutVertex v;
      v.x = (int32_t)top.v_x_o;
      v.y = (int32_t)top.v_y_o;
      v.z = (int32_t)top.v_z_o;
      v.poly = (int)top.v_poly_o;
      v.last = (int)top.v_last_o;
      out.push_back(v);
      if (v.last) saw_last = true;
    }
    zhao::tick(top);
    g_last_cycles = c + 1;
  }
  top.v_ready_i = 0;
  zhao::check(saw_last, "stream terminated with v_last", 1, saw_last ? 1 : 0);
  return out;
}

// The comparator every case funnels through — and the one the negative
// control corrupts. Returns the number of mismatching components.
int compare_stream(const char* what, const std::vector<OutVertex>& got,
                   const std::vector<EvalVec3>& want, bool record) {
  int bad = 0;
  if (got.size() != want.size()) bad++;
  const size_t n = got.size() < want.size() ? got.size() : want.size();
  for (size_t i = 0; i < n; ++i) {
    if (got[i].x != want[i].x) bad++;
    if (got[i].y != want[i].y) bad++;
    if (got[i].z != want[i].z) bad++;
  }
  if (record) {
    zhao::check(got.size() == want.size(), what, want.size(), got.size());
    zhao::check(bad == 0, what, 0, (uint64_t)bad);
  }
  return bad;
}

EvalParams base_params() {
  EvalParams p{};
  p.start = {-3 << 16, 20 << 16, 5 << 16};
  p.end = {40 << 16, 2 << 16, -7 << 16};
  p.perp1 = {0, 46341, 46341};        // ~unit, tilted
  p.perp2 = {65536, 0, 0};
  p.waxis = {0, 65536, 0};
  p.half_width = 6554;                // 0.1
  p.branch_half_width = 3277;         // 0.05
  p.amp = 2 << 16;                    // 2.0 world units of kink
  p.branch_amp = 1 << 16;
  p.seed = 0xDEADBEEFu;
  p.tick_phase = 137;
  p.segments = 24;
  p.branch_count = 2;
  p.br[0] = {6, 8, {10 << 16, -30 << 16, 9 << 16}};
  p.br[1] = {17, 8, {55 << 16, 25 << 16, 0}};
  p.view_mask = 3;
  return p;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_forge_prim_eval top;
  hard_reset(top);

  // Expected cumulative counters, tracked against the oracle as we go.
  uint64_t x_jobs = 0, x_points = 0, x_vertices = 0, x_refused = 0, x_skipped = 0, x_sat = 0;

  auto run_ok_job = [&](const char* what, const EvalParams& p, int pattern)
      -> std::vector<OutVertex> {
    std::vector<EvalVec3> want;
    EvalCounts wc;
    zref::forge::eval_job(p, want, wc);
    submit(top, p, 1);
    x_jobs++;
    auto got = collect(top, pattern);
    compare_stream(what, got, want, true);
    x_points += wc.points;
    x_vertices += wc.vertices;
    x_sat += wc.sat_events;
    return got;
  };

  auto check_counters = [&](const char* where) {
    zhao::check(top.jobs_o == (uint32_t)x_jobs, where, x_jobs, top.jobs_o);
    zhao::check(top.points_o == (uint32_t)x_points, where, x_points, top.points_o);
    zhao::check(top.vertices_o == (uint32_t)x_vertices, where, x_vertices, top.vertices_o);
    zhao::check(top.refused_limit_o == (uint32_t)x_refused, where, x_refused,
                top.refused_limit_o);
    zhao::check(top.skipped_view_o == (uint32_t)x_skipped, where, x_skipped,
                top.skipped_view_o);
    zhao::check(top.sat_events_o == (uint32_t)x_sat, where, x_sat, top.sat_events_o);
    zhao::check(top.walk_overrun_o == 0, where, 0, top.walk_overrun_o);
  };

  // ---- 1. the owner's worst legal bolt, against the oracle -----------------
  {
    EvalParams p = base_params();
    auto got = run_ok_job("T1 worst bolt vs oracle", p, 0);
    check_counters("T1 counters");

    // The MEASURED rate of the worst legal bolt with an always-ready
    // consumer, against computeClocksPerFrame = 1,666,666. Also a throughput
    // regression tripwire: 12,000 clocks is ~2x the intended budget.
    std::printf("[forge_prim_eval_directed] worst legal bolt: %d clocks "
                "(%.3f%% of a 1,666,666-clock frame)\n",
                g_last_cycles, 100.0 * g_last_cycles / 1666666.0);
    zhao::check(g_last_cycles < 12000, "T1 worst-bolt clock budget", 12000, g_last_cycles);

    // The seam: 86 vertices = 2*(24+1) + 2*(8+1) + 2*(8+1); polyline ids in
    // emission order; v_last exactly once, at the end.
    zhao::check((int)got.size() == zref::forge::eval_vertex_count(p), "T1 vertex count",
                zref::forge::eval_vertex_count(p), got.size());
    int lasts = 0;
    for (auto& v : got) lasts += v.last;
    zhao::check(lasts == 1 && got.back().last == 1, "T1 v_last once at end", 1, lasts);
    for (size_t i = 0; i < got.size(); ++i) {
      int want_poly = i < 50 ? 0 : (i < 68 ? 1 : 2);
      zhao::check(got[i].poly == want_poly, "T1 v_poly", want_poly, got[i].poly);
    }
    // Ribbon topology for segments=24, sides=1 references vertex indices
    // 0..2*24+1 — exactly the main polyline's emission (ring-major, ring=2).
    int max_idx = 0;
    const int tris = zref::forge::prim_triangles(zref::forge::kRibbon, 24, 1);
    for (int t = 0; t < tris; ++t) {
      auto tr = zref::forge::prim_triangle(zref::forge::kRibbon, 24, 1, t);
      if (tr.i0 > max_idx) max_idx = tr.i0;
      if (tr.i1 > max_idx) max_idx = tr.i1;
      if (tr.i2 > max_idx) max_idx = tr.i2;
    }
    zhao::check(max_idx == 2 * (24 + 1) - 1, "T1 prim seam: topology spans emission", 49,
                max_idx);
  }

  // ---- 2. determinism across stall patterns and reruns ---------------------
  {
    EvalParams p = base_params();
    std::vector<EvalVec3> want;
    EvalCounts wc;
    zref::forge::eval_job(p, want, wc);
    for (int pat = 0; pat < 4; ++pat) {
      submit(top, p, 1);
      x_jobs++;
      auto got = collect(top, pat);
      compare_stream("T2 stall-pattern determinism", got, want, true);
      x_points += wc.points;
      x_vertices += wc.vertices;
      x_sat += wc.sat_events;
    }
    check_counters("T2 counters");
  }

  // ---- 3. tick animates; anchors hold -------------------------------------
  {
    EvalParams p = base_params();
    std::vector<EvalVec3> want_a;
    EvalCounts wc;
    zref::forge::eval_job(p, want_a, wc);
    p.tick_phase = 138;
    auto got_b = run_ok_job("T3 next tick vs oracle", p, 0);
    int diff = 0;
    for (size_t i = 0; i < want_a.size(); ++i)
      diff += (got_b[i].x != want_a[i].x) + (got_b[i].y != want_a[i].y) +
              (got_b[i].z != want_a[i].z);
    zhao::check(diff > 0, "T3 tick_phase animates the bolt", 1, diff > 0 ? 1 : 0);
    // main-polyline endpoint vertices are anchor-exact on BOTH ticks
    for (int i : {0, 1, 48, 49}) {
      zhao::check(got_b[i].x == want_a[i].x && got_b[i].y == want_a[i].y &&
                      got_b[i].z == want_a[i].z,
                  "T3 anchors immune to tick", 1, 0 + 1);
    }
    check_counters("T3 counters");
  }

  // ---- 4. degenerate anchors: start == end is LEGAL ------------------------
  {
    EvalParams p = base_params();
    p.end = p.start;
    run_ok_job("T4 degenerate start==end", p, 2);
    check_counters("T4 counters");
  }

  // ---- 5. saturation domain: extremes, counted, bit-exact ------------------
  {
    EvalParams p = base_params();
    p.start = {INT32_MAX - 5, INT32_MIN + 5, INT32_MAX};
    p.end = {INT32_MIN, INT32_MAX, INT32_MIN + 77};
    p.perp1 = {INT32_MAX, INT32_MIN, INT32_MAX};
    p.perp2 = {INT32_MIN, INT32_MAX, INT32_MIN};
    p.amp = INT32_MAX;
    p.half_width = INT32_MAX;
    p.waxis = {INT32_MAX, INT32_MIN, INT32_MAX};
    uint64_t sat_before = x_sat;
    run_ok_job("T5 saturating extremes vs oracle", p, 0);
    zhao::check(x_sat > sat_before, "T5 sat_events fired", 1, x_sat > sat_before ? 1 : 0);
    check_counters("T5 counters");
  }

  // ---- 6. caps REFUSED, nothing emitted ------------------------------------
  {
    EvalParams bad[6];
    for (auto& b : bad) b = base_params();
    bad[0].segments = 0;
    bad[1].segments = 25;   // MAX_MAIN_SEGMENTS + 1 — the boundary
    bad[2].branch_count = 3;
    bad[3].br[0].segments = 9;  // MAX_BRANCH_SEGMENTS + 1
    bad[4].br[1].segments = 0;
    bad[5].br[0].attach = 25;   // past the last main point
    for (int i = 0; i < 6; ++i) {
      zhao::check(zref::forge::eval_verdict(bad[i], 1) == zref::forge::kEvalRefusedLimit,
                  "T6 oracle agrees this is illegal", zref::forge::kEvalRefusedLimit,
                  zref::forge::eval_verdict(bad[i], 1));
      submit(top, bad[i], 1);
      x_jobs++;
      x_refused++;
      top.v_ready_i = 1;
      for (int c = 0; c < 50; ++c) {
        top.eval();
        zhao::check(top.v_valid_o == 0, "T6 refused job emits nothing", 0, top.v_valid_o);
        zhao::tick(top);
      }
      top.v_ready_i = 0;
    }
    // the boundary from the LEGAL side: 24 accepted (T1 already), branch seg 8
    // accepted (T1), attach == segments accepted:
    EvalParams edge = base_params();
    edge.br[0].attach = 24;
    run_ok_job("T6 attach at the far anchor is legal", edge, 0);
    check_counters("T6 counters");
  }

  // ---- 7. a job outside the view is SKIPPED, not refused -------------------
  {
    EvalParams p = base_params();
    p.view_mask = 2;  // view B only
    submit(top, p, 1);  // we are view A
    x_jobs++;
    x_skipped++;
    top.v_ready_i = 1;
    for (int c = 0; c < 50; ++c) {
      top.eval();
      zhao::check(top.v_valid_o == 0, "T7 skipped job emits nothing", 0, top.v_valid_o);
      zhao::tick(top);
    }
    top.v_ready_i = 0;
    check_counters("T7 counters");
  }

  // ---- 8. seed corner cases (seed^2 degeneracy) ----------------------------
  for (uint32_t s : {0u, 1u, 0xFFFFFFFFu}) {
    EvalParams p = base_params();
    p.seed = s;
    run_ok_job("T8 seed corner", p, 0);
  }
  check_counters("T8 counters");

  // ---- 9. randomized full-domain differential ------------------------------
  {
    int refused_drawn = 0;
    for (int n = 0; n < 150; ++n) {
      EvalParams p{};
      auto r32 = [&]() { return (int32_t)rnd(); };
      p.start = {r32(), r32(), r32()};
      p.end = (n % 7 == 3) ? p.start : EvalVec3{r32(), r32(), r32()};
      p.perp1 = {r32(), r32(), r32()};
      p.perp2 = {r32(), r32(), r32()};
      p.waxis = {r32(), r32(), r32()};
      p.half_width = r32();
      p.branch_half_width = r32();
      p.amp = r32();
      p.branch_amp = r32();
      p.seed = rnd();
      p.tick_phase = (uint16_t)rnd();
      p.segments = 1 + (int)(rnd() % 24);
      p.branch_count = (int)(rnd() % 3);
      for (int b = 0; b < 2; ++b) {
        p.br[b].attach = (int)(rnd() % (uint32_t)(p.segments + 1));
        p.br[b].segments = 1 + (int)(rnd() % 8);
        p.br[b].end = {r32(), r32(), r32()};
      }
      p.view_mask = 1;
      if (n % 10 == 9) {  // a deliberate illegal fraction
        p.segments = 25 + (int)(rnd() % 40);
        refused_drawn++;
        submit(top, p, 1);
        x_jobs++;
        x_refused++;
        continue;
      }
      run_ok_job("T9 random differential", p, (int)(rnd() % 4));
    }
    zhao::check(refused_drawn > 0, "T9 illegal fraction drawn", 1, refused_drawn ? 1 : 0);
    check_counters("T9 counters");
  }

  // ---- 10. the checker SEEN TO FAIL ----------------------------------------
  {
    EvalParams p = base_params();
    std::vector<EvalVec3> want;
    EvalCounts wc;
    zref::forge::eval_job(p, want, wc);
    submit(top, p, 1);
    x_jobs++;
    x_points += wc.points;
    x_vertices += wc.vertices;
    x_sat += wc.sat_events;
    auto got = collect(top, 0);
    std::vector<EvalVec3> corrupted = want;
    corrupted[13].y ^= 1;  // one bit, one component
    int bad = compare_stream("T10 negative control (not recorded)", got, corrupted,
                             /*record=*/false);
    zhao::check(bad == 1, "T10 comparator detects a planted 1-bit lie", 1, bad);
    compare_stream("T10 stream itself is right", got, want, true);
    check_counters("T10 counters");
  }

  return zhao::report_and_exit("forge_prim_eval_directed");
}
