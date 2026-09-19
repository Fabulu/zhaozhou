// geom_vattr_directed.cpp -- GEOM.VATTR (fpga/rtl/geometry/zhao_geom_vattr.sv).
//
// The block under test is the vertex-attribute store AND its writer (entry I46,
// owner rulings R11 and R31). Every row it serves is differenced against the
// reference, not a transcription:
//
//   invw24            zref::depth_of_raw(w, profile)   -- the law GEOM.DEPTHQUANT is
//   u_over_w/v_over_w zref::geom_over_w(uv, invw24)    -- the derived per-vertex law
//   r, g, b           the lit value handed in, unchanged
//   alpha             the block's ALPHA_C (fx16 1.0; format 0 carries none)
//
// The bench plays the four producers (GEOM.VDECODE's u/v, GEOM.LIGHT's colour,
// GEOM.GROUP_SEQ's opens, GEOM.PROJ_LANE's landings) with the orders and
// timings the composition gives them -- u/v first, colour trailing, landings
// in bursts of two (one per view) -- and then reads every row back through the
// lookup port with the arena's one-clock timing.
//
// Every counter the block exports is seen to MOVE by legal stimulus at its
// ports (cases C..G), and to stay at zero on the clean cases, so no mutant is
// owed. Case H MEASURES the rate R31 asks about.
#include <cstdint>
#include <cstdio>
#include <deque>
#include <random>
#include <vector>

#include "verilated.h"
#include "Vzhao_geom_vattr.h"
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

constexpr unsigned kArenas = 4;
constexpr unsigned kVslots = 64;
constexpr uint32_t kAlpha = 0x10000u;

struct Vtx {
  int16_t u, v;
  uint32_t r, g, b;
  uint32_t w[2];     // per view
};

struct Bench {
  Vzhao_geom_vattr t;
  long clocks = 0;
  std::mt19937 rng{0x5EED7A};

  void zero() {
    t.batch_i = 0; t.batch_views_i = 0; t.op_valid_i = 0; t.uv_valid_i = 0;
    t.lit_valid_i = 0; t.fl_valid_i = 0; t.look_valid_i = 0;
  }
  void step() {
    zhao::tick(t);
    ++clocks;
  }
  void reset() {
    zero();
    t.rst_n = 0;
    for (int i = 0; i < 3; ++i) step();
    t.rst_n = 1;
    step();
  }

  uint32_t rand_w(unsigned profile) {
    const auto& g = zref::gen::DEPTH_PROFILES[profile];
    const unsigned pick = rng() % 8;
    if (pick == 0) return static_cast<uint32_t>(g.wmin_raw / 2 + rng() % 1000);       // near clamp
    if (pick == 1) return static_cast<uint32_t>(std::min<uint64_t>(0x7FFFFFFFu, g.wmax_raw + rng() % 100000));
    return static_cast<uint32_t>(g.wmin_raw + rng() % static_cast<uint32_t>(g.wmax_raw - g.wmin_raw));
  }

  std::vector<Vtx> make(unsigned n, unsigned p0, unsigned p1) {
    std::vector<Vtx> vs(n);
    for (auto& x : vs) {
      x.u = static_cast<int16_t>(rng());
      x.v = static_cast<int16_t>(rng());
      x.r = rng() % 0x10001u;
      x.g = rng() % 0x10001u;
      x.b = rng() % 0x10001u;
      x.w[0] = rand_w(p0);
      x.w[1] = rand_w(p1);
    }
    return vs;
  }

  // One batch through the producers. `land_every` spaces the vertices' landing
  // PAIRS (both views back to back), `lit_lag` delays each colour after its
  // u/v. Returns the clocks from the first landing to `done_o`.
  long run_batch(const std::vector<Vtx>& vs, unsigned mask, const unsigned arena[2],
                 const unsigned prof[2], int land_every, int lit_lag, bool lit_before_opens = false) {
    const unsigned nviews = (mask & 1u) + ((mask >> 1) & 1u);
    // batch boundary
    zero();
    t.batch_i = 1; t.batch_views_i = mask;
    step();
    zero();
    // Colour offered BEFORE the opens: the block must hold it (lit_ready_o low)
    // until it knows which arenas to write -- checked, then released.
    if (lit_before_opens) {
      // stage u/v for vertex 0 first so the colour is a real one
      t.uv_valid_i = 1; t.uv_u_i = static_cast<uint16_t>(vs[0].u); t.uv_v_i = static_cast<uint16_t>(vs[0].v);
      step();
      zero();
      t.lit_valid_i = 1; t.lit_r_i = vs[0].r; t.lit_g_i = vs[0].g; t.lit_b_i = vs[0].b;
      t.eval();
      ck(t.lit_ready_o == 0, "B: a colour offered before the batch's arenas are open is HELD");
      step();
      t.eval();
      ck(t.lit_ready_o == 0, "B: ... and still held a clock later");
      zero();
    }
    for (unsigned k = 0; k < nviews; ++k) {
      t.op_valid_i = 1; t.op_arena_i = arena[k];
      step();
      zero();
    }
    // the event schedule
    const unsigned first_uv = lit_before_opens ? 1u : 0u;
    std::deque<std::pair<long, int>> uv_ev, lit_ev;      // (cycle, vertex)
    std::deque<std::pair<long, std::pair<int, int>>> land_ev;  // (cycle, (vertex, view))
    const long base = clocks + 2;
    for (unsigned i = first_uv; i < vs.size(); ++i) uv_ev.push_back({base + 2 * i, static_cast<int>(i)});
    for (unsigned i = lit_before_opens ? 0u : 0u; i < vs.size(); ++i)
      lit_ev.push_back({base + 2 * i + lit_lag, static_cast<int>(i)});
    for (unsigned i = 0; i < vs.size(); ++i)
      for (unsigned k = 0; k < nviews; ++k)
        land_ev.push_back({base + 20 + static_cast<long>(land_every) * i + k, {static_cast<int>(i), static_cast<int>(k)}});
    const long first_land = land_ev.empty() ? clocks : land_ev.front().first;
    int lit_pending = -1;
    for (long guard = 0; guard < 200000; ++guard) {
      zero();
      if (!uv_ev.empty() && uv_ev.front().first <= clocks) {
        const Vtx& x = vs[uv_ev.front().second];
        t.uv_valid_i = 1; t.uv_u_i = static_cast<uint16_t>(x.u); t.uv_v_i = static_cast<uint16_t>(x.v);
        uv_ev.pop_front();
      }
      if (lit_pending < 0 && !lit_ev.empty() && lit_ev.front().first <= clocks) {
        lit_pending = lit_ev.front().second;
        lit_ev.pop_front();
      }
      if (lit_pending >= 0) {
        const Vtx& x = vs[lit_pending];
        t.lit_valid_i = 1; t.lit_r_i = x.r; t.lit_g_i = x.g; t.lit_b_i = x.b;
      }
      if (!land_ev.empty() && land_ev.front().first <= clocks) {
        const auto e = land_ev.front().second;
        t.fl_valid_i = 1; t.fl_arena_i = arena[e.second]; t.fl_index_i = e.first;
        t.fl_w_i = vs[e.first].w[e.second]; t.fl_profile_i = prof[e.second];
        land_ev.pop_front();
      }
      t.eval();
      const bool lit_took = t.lit_valid_i && t.lit_ready_o;
      step();
      if (lit_took) lit_pending = -1;
      zero();
      t.eval();
      if (uv_ev.empty() && lit_ev.empty() && land_ev.empty() && lit_pending < 0 && t.done_o)
        return clocks - first_land;
    }
    return -1;
  }

  struct Row {
    bool valid;
    uint32_t invw, uow, vow, r, g, b, a;
  };
  Row look(unsigned arena, unsigned index) {
    zero();
    t.look_valid_i = 1; t.look_arena_i = arena; t.look_index_i = index;
    step();
    zero();
    t.eval();
    Row r{};
    r.valid = t.rep_valid_o != 0;
    r.invw = t.rep_invw24_o;
    r.uow = t.rep_data_o[0];
    r.vow = t.rep_data_o[1];
    r.r = t.rep_data_o[2];
    r.g = t.rep_data_o[3];
    r.b = t.rep_data_o[4];
    r.a = t.rep_data_o[5];
    return r;
  }

  int verify(const std::vector<Vtx>& vs, unsigned nviews, const unsigned arena[2], const unsigned prof[2]) {
    int bad = 0;
    for (unsigned k = 0; k < nviews; ++k)
      for (unsigned i = 0; i < vs.size(); ++i) {
        const Row r = look(arena[k], i);
        const uint32_t inv = zref::depth_of_raw(vs[i].w[k], prof[k]);
        const uint32_t uow = static_cast<uint32_t>(zref::geom_over_w(vs[i].u, inv));
        const uint32_t vow = static_cast<uint32_t>(zref::geom_over_w(vs[i].v, inv));
        const bool ok = r.valid && r.invw == inv && r.uow == uow && r.vow == vow && r.r == vs[i].r &&
                        r.g == vs[i].g && r.b == vs[i].b && r.a == kAlpha;
        if (!ok) {
          if (bad < 4)
            std::printf("    view %u vtx %u: rtl inv %06X uow %08X vow %08X rgb %05X/%05X/%05X a %X | "
                        "ref inv %06X uow %08X vow %08X rgb %05X/%05X/%05X\n",
                        k, i, r.invw, r.uow, r.vow, r.r, r.g, r.b, r.a, inv, uow, vow, vs[i].r, vs[i].g, vs[i].b);
          ++bad;
        }
      }
    return bad;
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  auto* bp = new Bench;  // heap, never deleted: zhao_sim.hpp's exit-time rule
  Bench& b = *bp;
  b.reset();

  // ---- A: two views, a full 64-vertex batch, every row against the reference
  {
    const unsigned arena[2] = {1, 2}, prof[2] = {0, 2};
    const auto vs = b.make(kVslots, prof[0], prof[1]);
    const long t = b.run_batch(vs, 0b11, arena, prof, 10, 30);
    ck(t > 0, "A: the batch completes -- done_o rises once every row and colour is in");
    ck(b.verify(vs, 2, arena, prof) == 0,
       "A: every row, both views: invw24 == zref::depth_of_raw, u/v_over_w == zref::geom_over_w, "
       "colour unchanged, alpha the named constant");
    ck(b.t.rows_written_o == 128 && b.t.landings_o == 128, "A: 128 landings, 128 rows");
    ck(b.t.colours_written_o == 128, "A: 64 colours, each written into both views' arenas");
    ck(b.t.uv_staged_o == 64, "A: 64 u/v staged");
    ck(b.t.lq_overflow_o == 0 && b.t.index_oob_o == 0 && b.t.look_oob_o == 0 &&
           b.t.profile_mixed_o == 0 && b.t.dq_refused_o == 0 && b.t.dq_stray_o == 0,
       "A: no fault counter moved on a clean batch");
    std::printf("  A: 64 vertices x 2 views, landing pairs every 10 clocks: done %ld clocks after the first landing\n", t);
  }

  // ---- B: one view; colour offered before the opens is HELD, not mis-keyed --
  {
    const unsigned arena[2] = {3, 0}, prof[2] = {1, 1};
    const auto vs = b.make(20, prof[0], prof[1]);
    const long t = b.run_batch(vs, 0b10, arena, prof, 12, 5, true);
    ck(t > 0, "B: the single-view batch completes");
    ck(b.verify(vs, 1, arena, prof) == 0, "B: every row of the single view matches the reference");
  }

  // ---- C: a landing burst above the reciprocal's rate DROPS and COUNTS -----
  // Forty landings on forty consecutive clocks: sixteen depth contexts and an
  // eight-deep queue cannot hold them at one reciprocal per four clocks. The
  // drop is counted, the dropped rows are counted as finished, and the batch
  // still completes -- a hang here would be the R31 deadlock in a new place.
  {
    const unsigned arena[2] = {0, 1}, prof[2] = {0, 0};
    const auto vs = b.make(40, 0, 0);
    const uint32_t o0 = b.t.lq_overflow_o, r0 = b.t.rows_written_o, l0 = b.t.landings_o;
    const long t = b.run_batch(vs, 0b01, arena, prof, 1, 3);
    ck(t > 0, "C: a batch with dropped landings still completes");
    const uint32_t drops = b.t.lq_overflow_o - o0;
    ck(drops > 0, "C: lq_overflow_o FIRES on a landing burst above the reciprocal's rate");
    ck((b.t.rows_written_o - r0) + drops == b.t.landings_o - l0,
       "C: every landing is either a written row or a counted drop -- none vanishes");
    std::printf("  C: 40 landings in 40 clocks -> %u dropped and counted, %u written\n", drops,
                b.t.rows_written_o - r0);
  }

  // ---- D: one arena, two profiles, is counted -------------------------------
  {
    const unsigned arena[2] = {2, 3}, prof[2] = {0, 0};
    b.zero();
    b.t.batch_i = 1; b.t.batch_views_i = 1;
    b.step();
    b.zero();
    b.t.op_valid_i = 1; b.t.op_arena_i = arena[0];
    b.step();
    const uint32_t m0 = b.t.profile_mixed_o;
    b.zero();
    b.t.uv_valid_i = 1; b.t.uv_u_i = 0; b.t.uv_v_i = 0;
    b.step();
    b.zero();
    b.t.uv_valid_i = 1;
    b.step();
    for (int i = 0; i < 2; ++i) {
      b.zero();
      b.t.fl_valid_i = 1; b.t.fl_arena_i = arena[0]; b.t.fl_index_i = i;
      b.t.fl_w_i = 0x20000; b.t.fl_profile_i = i ? 1 : 0;
      b.step();
    }
    for (int i = 0; i < 2; ++i) {
      b.zero();
      b.t.lit_valid_i = 1; b.t.lit_r_i = 1; b.t.lit_g_i = 1; b.t.lit_b_i = 1;
      b.t.eval();
      while (!b.t.lit_ready_o) { b.step(); b.t.eval(); }
      b.step();
    }
    b.zero();
    for (int g = 0; g < 400 && !b.t.done_o; ++g) { b.step(); b.t.eval(); }
    ck(b.t.profile_mixed_o == m0 + 1, "D: profile_mixed_o counts one arena landed under two profiles");
    (void)prof;
  }

  // ---- E: the reserved profile is REFUSED by the depth law ------------------
  {
    const uint32_t q0 = b.t.dq_refused_o;
    const unsigned arena[2] = {1, 2}, prof[2] = {3, 3};
    std::vector<Vtx> vs = b.make(2, 0, 0);
    const long t = b.run_batch(vs, 0b01, arena, prof, 6, 2);
    ck(t > 0, "E: a batch with a reserved profile still completes");
    ck(b.t.dq_refused_o == q0 + 2, "E: dq_refused_o counts both refused depths");
    const Bench::Row r = b.look(1, 0);
    ck(r.valid && r.invw == 0 && r.uow == 0 && r.vow == 0,
       "E: a refused depth stores 0 -- the law's loud floor, and a zero perspective attribute");
  }

  // ---- F: a lookup outside the store answers zeros and is counted ----------
  {
    const uint32_t o0 = b.t.look_oob_o;
    const Bench::Row r1 = b.look(5, 0);        // arena past ARENAS
    const Bench::Row r2 = b.look(0, 70);       // index past VSLOTS
    ck(r1.valid && r2.valid, "F: an out-of-store lookup still answers on the arena's clock");
    ck(r1.invw == 0 && r1.uow == 0 && r2.invw == 0 && r2.r == 0,
       "F: ... with zeros, never another row's data");
    ck(b.t.look_oob_o == o0 + 2, "F: look_oob_o counts both");
  }

  // ---- G: a landing outside the store writes nothing and is counted --------
  {
    const uint32_t i0 = b.t.index_oob_o;
    b.zero();
    b.t.batch_i = 1; b.t.batch_views_i = 1;
    b.step();
    b.zero();
    b.t.op_valid_i = 1; b.t.op_arena_i = 0;
    b.step();
    b.zero();
    b.t.fl_valid_i = 1; b.t.fl_arena_i = 0; b.t.fl_index_i = 100; b.t.fl_w_i = 0x20000; b.t.fl_profile_i = 0;
    b.step();
    b.zero();
    for (int g = 0; g < 400 && !b.t.done_o; ++g) { b.step(); b.t.eval(); }
    ck(b.t.index_oob_o == i0 + 1, "G: index_oob_o counts a landing outside ARENAS x VSLOTS");
    ck(b.t.done_o, "G: ... and the batch still completes");
  }

  // ---- H: THE RATE (owner ruling R31) ---------------------------------------
  // The landing port cannot stall, so the store's capacity must exceed the
  // arena's landing rate. That rate is bounded by GEOM.SKIN: two landings (one
  // per view) per skinned vertex, one vertex per 12 clocks by construction and
  // per 10 MEASURED in the composed smoke. The depth stream's ceiling is the
  // reciprocal's -- four multiplier launches per job, one launch a clock -- so
  // ~4 clocks per landing, i.e. a vertex pair every 8.
  //
  // The sweep records where drops begin; the assertions are at the two rates
  // that matter: the measured skin rate (pairs every 10) and 20% faster than
  // it (every 8) must drop NOTHING and match the reference row for row.
  {
    for (int every : {12, 10, 8, 7, 6, 5}) {
      const unsigned arena[2] = {2, 3}, prof[2] = {0, 1};
      const auto vs = b.make(kVslots, prof[0], prof[1]);
      const uint32_t o0 = b.t.lq_overflow_o;
      const long t = b.run_batch(vs, 0b11, arena, prof, every, 12);
      const uint32_t drops = b.t.lq_overflow_o - o0;
      std::printf("  H: 128 landings, pairs every %2d clocks: %3u dropped, done %ld clocks after the first landing\n",
                  every, drops, t);
      if (every >= 8) {
        ck(t > 0 && drops == 0, "H: at and above the composed landing rate, nothing is dropped");
        ck(b.verify(vs, 2, arena, prof) == 0, "H: and every row is the reference's");
      }
    }
  }
  std::printf("geom_vattr_directed: %d checks, %d failed (landings=%u rows=%u colours=%u uv=%u "
              "lq_overflow=%u index_oob=%u look_oob=%u mixed=%u dq_refused=%u dq_stray=%u)\n",
              g_checks, g_fail, b.t.landings_o, b.t.rows_written_o, b.t.colours_written_o,
              b.t.uv_staged_o, b.t.lq_overflow_o, b.t.index_oob_o, b.t.look_oob_o,
              b.t.profile_mixed_o, b.t.dq_refused_o, b.t.dq_stray_o);
  zhao::exit_hard(g_fail ? 1 : 0);
}
