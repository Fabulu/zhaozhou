// measure_tokens_random.cpp — MEASURE.TOKENS randomized differential (phase 8,
// ZH-048).
//
// TWO LANES, both against `zref::measure::TokenGuard`, comparing EVERY
// observable of EVERY cycle: the combinational grant and its pool tag, the
// registered denial and its whole payload, all five pools, all eight
// representation counters and `triangles_culled`.
//
//   Lane A — the workload. A Duo frame the way the console will actually run
//     it: the charter's 45/45/10 split as tokens, meshlet-sized geometry costs,
//     both views busy, mostly low-priority refinement with a minority of
//     essential work, returns of amounts that were really granted, and a
//     budget reload at every frame boundary.
//   Lane B — the domain limit. Budgets and costs near 2^32 so that the credit's
//     wide add, the clamp and the counter rail are in play on most cycles, and
//     the pools spend their time within a few tokens of empty or full.
//
// THE EXACT EQUALITIES ARE CONSTRUCTED, NOT HOPED FOR. Uniform random `cost`
// against a 32-bit pool hits `cost == avail` with probability 2^-32; this
// project has now been bitten five times by exactly that, most recently at
// "exactly 3 mismatches per patch". So on a fraction of cycles this file READS
// THE POOL AND BUILDS the value:
//
//   E1  cost == avail_priv                  (the grant/deny flip point)
//   E2  cost == avail_priv + 1              (one token over it)
//   E3  cost == avail_shared, essential, private short   (the reserve's flip)
//   E4  cost == avail_shared + 1, same shape             (one over the reserve)
//   E5  return cost == budget - pool        (a credit landing EXACTLY on the
//                                            budget, where the clamp begins)
//   E6  return cost == budget - pool + 1    (one token INTO the clamp)
//   E7  a request and a return on the SAME pool in the SAME cycle (law T6)
//   E8  cost == 0 against a pool that is exactly 0
//
// Each is counted, and each lane asserts at the end that it reached every
// construction it claims to make. A lane that never hits its own boundary is
// not evidence, and this file says so out loud rather than passing quietly.

#include "tokens_dev.hpp"

#include <cstdio>
#include <cstring>

namespace {

using zhao::check;
namespace zm = zref::measure;
using tokens_test::cycle;
using tokens_test::Obs;
using tokens_test::Stim;

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed) {}
  uint64_t next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  uint32_t u32() { return static_cast<uint32_t>(next()); }
  uint32_t below(uint32_t n) { return n == 0 ? 0u : static_cast<uint32_t>(next() % n); }
  bool chance(int n) { return (next() % static_cast<uint64_t>(n)) == 0; }
};

struct Stats {
  long cycles = 0;
  long grants = 0, denials = 0, shared_grants = 0;
  long e1 = 0, e2 = 0, e3 = 0, e4 = 0, e5 = 0, e6 = 0, e7 = 0, e8 = 0;
  long clamped = 0;      // a credit that actually hit the budget ceiling
  long cull_sat = 0;     // triangles_culled reached its rail
  long reload_deny = 0;  // a request collided with a budget load
  long mismatches = 0;
};

/** The oracle's private pool for (view, class) — the value E1/E2 are built on. */
uint32_t priv_of(const zm::TokenGuard& g, int view, int cls) {
  return cls ? g.avail_frag(view) : g.avail_geom(view);
}

uint32_t bud_of(const zm::TokenBudgets& b, int view, int cls) {
  return cls ? b.frag[view & 1] : b.geom[view & 1];
}

/**
 * One lane.
 *
 * `limit` selects the domain: false = the workload (meshlet-sized costs against
 * the charter's split), true = the domain limit (everything near 2^32).
 */
Stats run_lane(Vzhao_measure_tokens& dut, Rng& rng, long cycles, bool limit) {
  Stats st;
  zm::TokenGuard ref;
  tokens_test::reset_dut(dut);
  ref.reset();

  zm::TokenBudgets bud;
  // The charter's 45/45/10, as tokens; or the domain limit.
  if (limit) {
    bud.geom[0] = 0xFFFFFFF0u;
    bud.geom[1] = 0xFFFFFFF0u;
    bud.frag[0] = 0xFFFFFFFFu;
    bud.frag[1] = 0xFFFFFF00u;
    bud.shared = 0xFFFFFFFFu;
  } else {
    bud.geom[0] = 45000;
    bud.geom[1] = 45000;
    bud.frag[0] = 45000;
    bud.frag[1] = 45000;
    bud.shared = 10000;
  }

  // Outstanding grants, so returns hand back amounts that were really taken.
  struct Held {
    int view, cls;
    bool shared;
    uint32_t cost;
  };
  Held held[64];
  int n_held = 0;

  zm::TokenAnswer prev;
  zm::TokenRequest prev_req;
  bool prev_valid = false;
  bool loaded = false;

  for (long t = 0; t < cycles; ++t) {
    Stim s;
    s.budgets = bud;

    // ---- frame boundary: reload the budgets -------------------------------
    const bool frame = (!loaded) || rng.chance(limit ? 400 : 2000);
    if (frame) {
      s.budget_valid = true;
      loaded = true;
      n_held = 0;
      // Lane B moves the budgets around so the clamp ceiling is not constant.
      if (limit) {
        bud.geom[0] = 0xFFFFFFFFu - rng.below(64);
        bud.geom[1] = 0xFFFFFFFFu - rng.below(64);
        bud.frag[0] = 0xFFFFFFFFu - rng.below(64);
        bud.frag[1] = 0xFFFFFFFFu - rng.below(64);
        bud.shared = 0xFFFFFFFFu - rng.below(64);
        s.budgets = bud;
      }
    }

    // ---- the request ------------------------------------------------------
    const bool want_req = !rng.chance(8);
    if (want_req) {
      s.req.valid = true;
      s.req.view = static_cast<int>(rng.below(2));
      s.req.cls = static_cast<int>(rng.below(2));
      s.req.rep = static_cast<int>(rng.below(8));
      s.req.essential = rng.chance(4);
      s.req.src_id = static_cast<uint16_t>(rng.u32());

      const uint32_t avail = priv_of(ref, s.req.view, s.req.cls);
      const uint32_t shar = ref.avail_shared();
      const int pick = static_cast<int>(rng.below(16));
      if (pick == 0) {  // E1: exactly the private pool
        s.req.cost = avail;
        ++st.e1;
      } else if (pick == 1 && avail != 0xFFFFFFFFu) {  // E2: one token over it
        s.req.cost = avail + 1;
        ++st.e2;
      } else if (pick == 2) {  // E3: exactly the reserve, private short
        s.req.essential = true;
        s.req.cost = shar;
        if (shar > avail) ++st.e3;
      } else if (pick == 3 && shar != 0xFFFFFFFFu) {  // E4: one over the reserve
        s.req.essential = true;
        s.req.cost = shar + 1;
        if (shar >= avail) ++st.e4;
      } else if (pick == 4 && avail == 0) {  // E8: zero against zero
        s.req.cost = 0;
        ++st.e8;
      } else if (limit) {
        // the domain limit: costs within a whisker of the pool, from both sides
        const uint32_t d = rng.below(8);
        s.req.cost = rng.chance(2) ? (avail >= d ? avail - d : 0u)
                                   : (avail <= 0xFFFFFFFFu - d ? avail + d : 0xFFFFFFFFu);
      } else {
        s.req.cost = 1 + rng.below(256);  // a meshlet's worth of triangles
      }
      if (s.req.cost == 0 && avail == 0) ++st.e8;
    }

    // ---- the return -------------------------------------------------------
    const int rpick = static_cast<int>(rng.below(8));
    if (rpick < 3 && n_held > 0) {
      const int k = static_cast<int>(rng.below(static_cast<uint32_t>(n_held)));
      s.ret.valid = true;
      s.ret.view = held[k].view;
      s.ret.cls = held[k].cls;
      s.ret.shared = held[k].shared;
      s.ret.cost = held[k].cost;
      held[k] = held[--n_held];
    } else if (rpick == 3) {
      // E5/E6: a credit built to land EXACTLY on the budget, or one past it.
      const int v = static_cast<int>(rng.below(2));
      const int cl = static_cast<int>(rng.below(2));
      const bool sh = rng.chance(3);
      const uint32_t pool = sh ? ref.avail_shared() : priv_of(ref, v, cl);
      const uint32_t cap = sh ? bud.shared : bud_of(bud, v, cl);
      if (cap >= pool) {
        const uint32_t room = cap - pool;
        s.ret.valid = true;
        s.ret.view = v;
        s.ret.cls = cl;
        s.ret.shared = sh;
        if (rng.chance(2)) {
          s.ret.cost = room;  // E5: exactly onto the ceiling
          ++st.e5;
        } else if (room != 0xFFFFFFFFu) {
          s.ret.cost = room + 1;  // E6: one token INTO the clamp
          ++st.e6;
          ++st.clamped;
        }
      }
    }

    if (s.req.valid && s.ret.valid && s.req.view == s.ret.view && s.req.cls == s.ret.cls &&
        !s.ret.shared) {
      ++st.e7;  // a debit and a credit on the same pool, same cycle (law T6)
    }
    if (s.req.valid && s.budget_valid) ++st.reload_deny;

    // ---- lockstep ---------------------------------------------------------
    const zm::TokenAnswer a = ref.step(s.req, s.ret, s.budget_valid, s.budgets);
    const Obs o = cycle(dut, s);
    ++st.cycles;

    long before = zhao::check_failures();
    check(o.grant == a.grant, "random: grant", a.grant ? 1 : 0, o.grant ? 1 : 0);
    check(o.shared == a.shared, "random: shared tag", a.shared ? 1 : 0, o.shared ? 1 : 0);

    const bool want_den = prev_valid && prev.deny;
    check(o.den_valid == want_den, "random: den_valid", want_den ? 1 : 0, o.den_valid ? 1 : 0);
    if (want_den && o.den_valid) {
      check(o.den_reason == static_cast<uint8_t>(prev.reason), "random: den_reason", prev.reason,
            o.den_reason);
      check(o.den_view == static_cast<uint8_t>(prev_req.view), "random: den_view", prev_req.view,
            o.den_view);
      check(o.den_class == static_cast<uint8_t>(prev_req.cls), "random: den_class", prev_req.cls,
            o.den_class);
      check(o.den_rep == static_cast<uint8_t>(prev_req.rep), "random: den_rep", prev_req.rep,
            o.den_rep);
      check(o.den_src_id == prev_req.src_id, "random: den_src_id", prev_req.src_id, o.den_src_id);
      check(o.den_cost == prev_req.cost, "random: den_cost", prev_req.cost, o.den_cost);
    }

    check(dut.avail_geom0_o == ref.avail_geom(0), "random: pool g0", ref.avail_geom(0),
          dut.avail_geom0_o);
    check(dut.avail_geom1_o == ref.avail_geom(1), "random: pool g1", ref.avail_geom(1),
          dut.avail_geom1_o);
    check(dut.avail_frag0_o == ref.avail_frag(0), "random: pool f0", ref.avail_frag(0),
          dut.avail_frag0_o);
    check(dut.avail_frag1_o == ref.avail_frag(1), "random: pool f1", ref.avail_frag(1),
          dut.avail_frag1_o);
    check(dut.avail_shared_o == ref.avail_shared(), "random: pool shared", ref.avail_shared(),
          dut.avail_shared_o);
    check(dut.triangles_culled_o == ref.triangles_culled(), "random: triangles_culled",
          ref.triangles_culled(), dut.triangles_culled_o);
    const uint32_t rc[8] = {dut.tok_rep_count0_o, dut.tok_rep_count1_o, dut.tok_rep_count2_o,
                            dut.tok_rep_count3_o, dut.tok_rep_count4_o, dut.tok_rep_count5_o,
                            dut.tok_rep_count6_o, dut.tok_rep_count7_o};
    for (int k = 0; k < 8; ++k) {
      check(rc[k] == ref.rep_count(k), "random: rep counter", ref.rep_count(k), rc[k]);
    }

    // THE GUARANTEE (law T2), on every cycle of the lane rather than once: a
    // pool never exceeds its budget, so no view's spendable allowance can be
    // raised above what the frame promised it.
    check(dut.avail_geom0_o <= bud.geom[0], "random: g0 within budget", bud.geom[0],
          dut.avail_geom0_o);
    check(dut.avail_geom1_o <= bud.geom[1], "random: g1 within budget", bud.geom[1],
          dut.avail_geom1_o);
    check(dut.avail_frag0_o <= bud.frag[0], "random: f0 within budget", bud.frag[0],
          dut.avail_frag0_o);
    check(dut.avail_frag1_o <= bud.frag[1], "random: f1 within budget", bud.frag[1],
          dut.avail_frag1_o);
    check(dut.avail_shared_o <= bud.shared, "random: shared within budget", bud.shared,
          dut.avail_shared_o);

    if (zhao::check_failures() != before) ++st.mismatches;

    if (a.grant) {
      ++st.grants;
      if (a.shared) ++st.shared_grants;
      if (n_held < 64) {
        held[n_held].view = s.req.view;
        held[n_held].cls = s.req.cls;
        held[n_held].shared = a.shared;
        held[n_held].cost = s.req.cost;
        ++n_held;
      }
    }
    if (a.deny) ++st.denials;
    if (dut.triangles_culled_o == 0xFFFFFFFFu) ++st.cull_sat;

    prev = a;
    prev_req = s.req;
    prev_valid = s.req.valid;
  }
  return st;
}

void report(const char* name, const Stats& s) {
  std::printf(
      "  lane %s: %ld cycles, %ld grants (%ld from the reserve), %ld denials, %ld mismatching "
      "cycles\n",
      name, s.cycles, s.grants, s.shared_grants, s.denials, s.mismatches);
  std::printf(
      "    constructed: E1 cost==avail %ld | E2 avail+1 %ld | E3 cost==shared %ld | E4 shared+1 "
      "%ld\n",
      s.e1, s.e2, s.e3, s.e4);
  std::printf(
      "                 E5 credit onto the ceiling %ld | E6 one into the clamp %ld | E7 same-pool "
      "debit+credit %ld | E8 zero-on-zero %ld\n",
      s.e5, s.e6, s.e7, s.e8);
  std::printf("    reached: %ld clamped credits, %ld reload collisions, %ld cull-rail cycles\n",
              s.clamped, s.reload_deny, s.cull_sat);
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;
  }
  const long n = nightly ? 400000L : 60000L;

  Vzhao_measure_tokens dut;
  Rng rng_a(0x4D45415355524501ULL);
  Rng rng_b(0x4D45415355524502ULL);

  const Stats a = run_lane(dut, rng_a, n, /*limit=*/false);
  const Stats b = run_lane(dut, rng_b, n, /*limit=*/true);

  report("A (Duo workload) ", a);
  report("B (domain limit) ", b);

  // ---- each lane must have REACHED what it exists for ----------------------
  check(a.e1 > 0, "lane A CONSTRUCTED cost == avail", 1, a.e1 > 0 ? 1 : 0);
  check(a.e2 > 0, "lane A CONSTRUCTED cost == avail + 1", 1, a.e2 > 0 ? 1 : 0);
  check(a.e3 > 0, "lane A CONSTRUCTED cost == the reserve", 1, a.e3 > 0 ? 1 : 0);
  check(a.e4 > 0, "lane A CONSTRUCTED one token over the reserve", 1, a.e4 > 0 ? 1 : 0);
  check(a.e5 > 0, "lane A CONSTRUCTED a credit landing exactly on the budget", 1, a.e5 > 0 ? 1 : 0);
  check(a.e6 > 0, "lane A CONSTRUCTED a credit one token into the clamp", 1, a.e6 > 0 ? 1 : 0);
  check(a.e7 > 0, "lane A hit a same-cycle debit and credit on one pool", 1, a.e7 > 0 ? 1 : 0);
  check(a.e8 > 0, "lane A hit a zero-cost request against an empty pool", 1, a.e8 > 0 ? 1 : 0);
  check(a.grants > 0, "lane A actually granted work", 1, a.grants > 0 ? 1 : 0);
  check(a.denials > 0, "lane A actually refused work", 1, a.denials > 0 ? 1 : 0);
  check(a.shared_grants > 0, "lane A actually reached the emergency pool", 1,
        a.shared_grants > 0 ? 1 : 0);
  check(a.reload_deny > 0, "lane A hit a request colliding with a budget load", 1,
        a.reload_deny > 0 ? 1 : 0);

  check(b.e1 > 0, "lane B CONSTRUCTED cost == avail at the domain limit", 1, b.e1 > 0 ? 1 : 0);
  check(b.e2 > 0, "lane B CONSTRUCTED cost == avail + 1 at the domain limit", 1, b.e2 > 0 ? 1 : 0);
  check(b.e5 > 0, "lane B CONSTRUCTED a credit landing exactly on a near-2^32 budget", 1,
        b.e5 > 0 ? 1 : 0);
  check(b.e6 > 0, "lane B CONSTRUCTED a credit one token into the clamp at the limit", 1,
        b.e6 > 0 ? 1 : 0);
  check(b.clamped > 0, "lane B actually clamped a credit", 1, b.clamped > 0 ? 1 : 0);
  check(b.cull_sat > 0, "lane B drove triangles_culled to its rail", 1, b.cull_sat > 0 ? 1 : 0);
  check(b.grants > 0, "lane B granted work at the domain limit", 1, b.grants > 0 ? 1 : 0);
  check(b.denials > 0, "lane B refused work at the domain limit", 1, b.denials > 0 ? 1 : 0);

  const int rc = zhao::report_and_exit("measure_tokens_random");
  zhao::exit_hard(rc);
}
