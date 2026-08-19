// measure_tokens_directed.cpp — MEASURE.TOKENS directed tests (phase 8,
// ZH-048).
//
// Every case here CONSTRUCTS the value it is about. Uniform random input does
// not produce `cost == avail`, `cost == avail + 1`, `cost == shared` or a
// counter at its rail, and those four equalities are where a token guard's
// entire law lives — five increments of this project have now been bitten by
// hoping for an exact boundary instead of building it.
//
// What each lane would catch (the "could have been red" statement):
//   1. exact fit / one over — `cost == avail` grants and empties the pool;
//      `cost == avail + 1` denies. Red on: `<` where the law says `<=`, an
//      off-by-one in the comparator, a pool that goes negative.
//   2. zero cost — granted against an EMPTY pool, because 0 <= 0. Red on: a
//      guard that special-cases zero, or one that treats "empty" as "closed".
//   3. the emergency pool (law T1) — an essential request whose private pool
//      is short takes the shared pool at exactly `cost == shared`, and is
//      refused at `shared == cost - 1` with reason EXHAUSTED. Red on: a
//      shared-first draw, a missing fallback, a wrong reason code.
//   4. low priority never reaches the emergency pool (law T1) — the charter's
//      Version 1 sentence, executed: refinement is what stops when the budget
//      is nearly exhausted, and the shared pool is untouched by its refusal.
//      Red on: an admission rule that ignores `essential`.
//   5. THE VOLCANO (law T2) — view 1 drains its own geometry pool AND the
//      whole emergency pool, and view 0 then spends its entire guaranteed
//      budget anyway. This is charter §9's "one player looking directly into a
//      volcano cannot make the other player's army disappear", run as a test
//      rather than quoted. Red on: any shared accounting between the views.
//   6. the return path (laws T4, T5) — a return names the pool its grant drew;
//      a return past the budget CLAMPS and does not wrap. Red on: a
//      return-to-private-first rule (which would move tokens permanently out
//      of the emergency pool), or an unclamped credit (which would raise a
//      view's spendable budget above its guarantee).
//   7. same-cycle return does not help the request (law T6) — the request is
//      decided against the pools as they stand, and the same request succeeds
//      one cycle later. Red on: a forwarded credit.
//   8. latency (the ledger's `fixed:1`) — the denial presents EXACTLY one
//      cycle after the refused request, and the grant is combinational in the
//      SAME cycle (GEOM.BINNER's law E). Red on: a registered grant, a
//      two-cycle denial, a denial that sticks.
//   9. the reload cycle (law T9) — a request colliding with a budget load is
//      refused with reason RELOAD, not dropped. Red on: silence.
//  10. counters — all eight `lod_representation_counts` lanes move
//      independently on grants; `triangles_culled` adds the COST of a denied
//      geometry request, adds nothing for a denied fragment request, and
//      SATURATES at the rail instead of wrapping. Red on: counting one per
//      denial, counting fragments as triangles, a wrapping counter.
//  11. sustained rate — 1 grant decision per clock over a long burst, which is
//      the ledger's `target_throughput`. Red on: any internal stall.
//
// Every lane also runs the oracle in lockstep, so "the RTL is right" and "the
// RTL agrees with zref::measure::TokenGuard" are the same statement here.

#include "tokens_dev.hpp"

#include <cstdio>

namespace {

using zhao::check;
namespace zm = zref::measure;
using tokens_test::cycle;
using tokens_test::Obs;
using tokens_test::pools;
using tokens_test::Stim;

// ---------------------------------------------------------------- cosim --
// One cycle through BOTH, comparing everything observable. `den_*` compares
// the denial the DUT is presenting now, which belongs to the previous cycle —
// so the oracle's answers are kept one deep.
struct Cosim {
  Vzhao_measure_tokens dut;
  zm::TokenGuard ref;
  zm::TokenAnswer prev;  // the oracle's answer for the previous cycle
  zm::TokenRequest prev_req;
  bool prev_valid = false;
  int failures_at_start = 0;

  void reset() {
    tokens_test::reset_dut(dut);
    ref.reset();
    prev = zm::TokenAnswer();
    prev_req = zm::TokenRequest();
    prev_valid = false;
  }

  Obs step(const Stim& s, const char* where) {
    const zm::TokenAnswer a = ref.step(s.req, s.ret, s.budget_valid, s.budgets);
    const Obs o = cycle(dut, s);

    check(o.grant == a.grant, where, a.grant ? 1 : 0, o.grant ? 1 : 0);
    check(o.shared == a.shared, where, a.shared ? 1 : 0, o.shared ? 1 : 0);

    const bool want_den = prev_valid && prev.deny;
    check(o.den_valid == want_den, where, want_den ? 1 : 0, o.den_valid ? 1 : 0);
    if (want_den && o.den_valid) {
      check(o.den_reason == static_cast<uint8_t>(prev.reason), where, prev.reason, o.den_reason);
      check(o.den_view == static_cast<uint8_t>(prev_req.view), where, prev_req.view, o.den_view);
      check(o.den_class == static_cast<uint8_t>(prev_req.cls), where, prev_req.cls, o.den_class);
      check(o.den_rep == static_cast<uint8_t>(prev_req.rep), where, prev_req.rep, o.den_rep);
      check(o.den_src_id == prev_req.src_id, where, prev_req.src_id, o.den_src_id);
      check(o.den_cost == prev_req.cost, where, prev_req.cost, o.den_cost);
    }

    const tokens_test::Pools pd = pools(dut);
    const tokens_test::Pools pr = pools(ref);
    check(pd == pr, where, pr.g0, pd.g0);
    check(pd.g1 == pr.g1, where, pr.g1, pd.g1);
    check(pd.f0 == pr.f0, where, pr.f0, pd.f0);
    check(pd.f1 == pr.f1, where, pr.f1, pd.f1);
    check(pd.sh == pr.sh, where, pr.sh, pd.sh);

    check(dut.triangles_culled_o == ref.triangles_culled(), where, ref.triangles_culled(),
          dut.triangles_culled_o);
    const uint32_t rc[8] = {dut.tok_rep_count0_o, dut.tok_rep_count1_o, dut.tok_rep_count2_o,
                            dut.tok_rep_count3_o, dut.tok_rep_count4_o, dut.tok_rep_count5_o,
                            dut.tok_rep_count6_o, dut.tok_rep_count7_o};
    for (int k = 0; k < 8; ++k) check(rc[k] == ref.rep_count(k), where, ref.rep_count(k), rc[k]);

    // The pools NEVER exceed the loaded budget — law T5's safety half, ridden
    // on every cycle of every directed case rather than checked once.
    if (s.budget_valid) {
      // the load itself sets them; nothing to check against a stale budget
    } else {
      check(pd.g0 <= budget_.geom[0], where, budget_.geom[0], pd.g0);
      check(pd.g1 <= budget_.geom[1], where, budget_.geom[1], pd.g1);
      check(pd.f0 <= budget_.frag[0], where, budget_.frag[0], pd.f0);
      check(pd.f1 <= budget_.frag[1], where, budget_.frag[1], pd.f1);
      check(pd.sh <= budget_.shared, where, budget_.shared, pd.sh);
    }

    prev = a;
    prev_req = s.req;
    prev_valid = s.req.valid;
    return o;
  }

  /** Load budgets on a clean cycle and remember them for the T5 invariant. */
  void load(const zm::TokenBudgets& b, const char* where) {
    Stim s;
    s.budget_valid = true;
    s.budgets = b;
    budget_ = b;
    step(s, where);
  }

  Obs idle(const char* where) {
    Stim s;
    s.budgets = budget_;
    return step(s, where);
  }

  zm::TokenBudgets budget_;
};

zm::TokenRequest mkreq(int view, int cls, bool essential, uint32_t cost, int rep = 0,
                       uint16_t src = 0) {
  zm::TokenRequest r;
  r.valid = true;
  r.view = view;
  r.cls = cls;
  r.essential = essential;
  r.cost = cost;
  r.rep = rep;
  r.src_id = src;
  return r;
}

zm::TokenBudgets mkbud(uint32_t g0, uint32_t g1, uint32_t f0, uint32_t f1, uint32_t sh) {
  zm::TokenBudgets b;
  b.geom[0] = g0;
  b.geom[1] = g1;
  b.frag[0] = f0;
  b.frag[1] = f1;
  b.shared = sh;
  return b;
}

// ------------------------------------------------------------------ 1, 2 --
// The exact-fit boundary, CONSTRUCTED. `cost == avail` must grant and leave
// the pool at exactly zero; `cost == avail + 1` must deny. Then zero cost
// against the emptied pool, which is the other exact equality (0 <= 0).
void test_exact_fit(Cosim& c) {
  c.reset();
  c.load(mkbud(1000, 1000, 1000, 1000, 0), "exact/load");

  // spend 400, leaving exactly 600
  Stim s;
  s.budgets = c.budget_;
  s.req = mkreq(0, 0, false, 400, 1, 0x11);
  Obs o = c.step(s, "exact/spend400");
  check(o.grant, "exact: 400 of 1000 granted", 1, o.grant ? 1 : 0);
  check(!o.shared, "exact: 400 came from the private pool", 0, o.shared ? 1 : 0);

  // one MORE than what is left: 601 of 600 — denied
  s.req = mkreq(0, 0, false, 601, 1, 0x12);
  o = c.step(s, "exact/601");
  check(!o.grant, "exact: avail+1 is DENIED", 0, o.grant ? 1 : 0);
  check(c.dut.avail_geom0_o == 600u, "exact: a denial spends nothing", 600, c.dut.avail_geom0_o);

  // exactly what is left: 600 of 600 — granted, pool to zero
  s.req = mkreq(0, 0, false, 600, 1, 0x13);
  o = c.step(s, "exact/600");
  check(o.grant, "exact: cost == avail is GRANTED", 1, o.grant ? 1 : 0);
  check(c.dut.avail_geom0_o == 0u, "exact: the pool lands on exactly zero", 0, c.dut.avail_geom0_o);

  // zero cost against an EMPTY pool — 0 <= 0, so granted, and spends nothing
  s.req = mkreq(0, 0, false, 0, 2, 0x14);
  o = c.step(s, "exact/zero");
  check(o.grant, "exact: a zero-cost request is granted against an empty pool", 1, o.grant ? 1 : 0);
  check(!o.shared, "exact: a zero-cost request does not touch the emergency pool", 0,
        o.shared ? 1 : 0);

  // and one token more than empty is refused
  s.req = mkreq(0, 0, false, 1, 2, 0x15);
  o = c.step(s, "exact/one-over-empty");
  check(!o.grant, "exact: one token against an empty pool is refused", 0, o.grant ? 1 : 0);
}

// --------------------------------------------------------------------- 3 --
// The emergency pool, at its own exact boundary. The private pool is short by
// construction; the shared pool is set to exactly the cost, then to one less.
void test_emergency_pool(Cosim& c) {
  c.reset();
  c.load(mkbud(10, 10, 10, 10, 50), "emerg/load");

  Stim s;
  s.budgets = c.budget_;

  // essential, private short (10 < 50), shared holds exactly 50
  s.req = mkreq(0, 0, true, 50, 3, 0x21);
  Obs o = c.step(s, "emerg/exact");
  check(o.grant, "emergency: cost == shared is granted", 1, o.grant ? 1 : 0);
  check(o.shared, "emergency: and it is tagged as a SHARED draw", 1, o.shared ? 1 : 0);
  check(c.dut.avail_shared_o == 0u, "emergency: the emergency pool lands on zero", 0,
        c.dut.avail_shared_o);
  check(c.dut.avail_geom0_o == 10u, "emergency: the private pool was NOT touched", 10,
        c.dut.avail_geom0_o);

  // refill and take one more than the pool holds
  c.load(mkbud(10, 10, 10, 10, 49), "emerg/reload");
  s.budgets = c.budget_;
  s.req = mkreq(0, 0, true, 50, 3, 0x22);
  o = c.step(s, "emerg/one-over");
  check(!o.grant, "emergency: cost == shared+1 is refused", 0, o.grant ? 1 : 0);
  Obs d = c.idle("emerg/reason");
  check(d.den_valid, "emergency: the refusal is reported", 1, d.den_valid ? 1 : 0);
  check(d.den_reason == zm::kDenyExhausted, "emergency: reason is EXHAUSTED", zm::kDenyExhausted,
        d.den_reason);

  // the private pool is tried FIRST even for essential work: a cost that fits
  // privately must NOT touch the emergency pool.
  s.req = mkreq(0, 0, true, 10, 3, 0x23);
  o = c.step(s, "emerg/private-first");
  check(o.grant && !o.shared, "emergency: private is tried first, always", 1,
        (o.grant && !o.shared) ? 1 : 0);
  check(c.dut.avail_shared_o == 49u, "emergency: the reserve is intact", 49, c.dut.avail_shared_o);
}

// --------------------------------------------------------------------- 4 --
// Charter §9 Version 1: "a global token guard rejects only low-priority
// refinement when the budget is nearly exhausted." Refinement is confined to
// its own pool, so a huge emergency pool does not save it.
void test_low_priority_is_confined(Cosim& c) {
  c.reset();
  c.load(mkbud(5, 5, 5, 5, 1000000), "lowprio/load");

  Stim s;
  s.budgets = c.budget_;
  s.req = mkreq(1, 0, /*essential=*/false, 6, 4, 0x31);
  Obs o = c.step(s, "lowprio/deny");
  check(!o.grant, "low priority: refinement is refused when its own pool is short", 0,
        o.grant ? 1 : 0);
  check(c.dut.avail_shared_o == 1000000u, "low priority: the emergency pool is untouched", 1000000,
        c.dut.avail_shared_o);
  Obs d = c.idle("lowprio/reason");
  check(d.den_reason == zm::kDenyLowPriority, "low priority: reason is LOW_PRIORITY",
        zm::kDenyLowPriority, d.den_reason);

  // the SAME request, marked essential, is admitted from the emergency pool.
  s.req = mkreq(1, 0, /*essential=*/true, 6, 4, 0x32);
  o = c.step(s, "lowprio/essential");
  check(o.grant && o.shared, "low priority: the same cost, essential, takes the reserve", 1,
        (o.grant && o.shared) ? 1 : 0);
}

// --------------------------------------------------------------------- 5 --
// THE VOLCANO. Charter §9: "One player looking directly into a volcano cannot
// make the other player's army disappear." View 1 empties its own geometry
// pool and the entire emergency pool; view 0 then spends its whole guaranteed
// budget, one token at a time, and every single request is granted.
void test_duo_guarantee(Cosim& c) {
  c.reset();
  const uint32_t kGuaranteed = 450;  // the charter's 45%, as tokens
  c.load(mkbud(kGuaranteed, kGuaranteed, 100, 100, 100), "volcano/load");

  Stim s;
  s.budgets = c.budget_;

  // view 1 stares into the volcano: it drains its private pool ...
  s.req = mkreq(1, 0, true, kGuaranteed, 0, 0x41);
  Obs o = c.step(s, "volcano/drain-private");
  check(o.grant && !o.shared, "volcano: view 1 spends its own budget", 1,
        (o.grant && !o.shared) ? 1 : 0);
  // ... and then the whole emergency pool ...
  s.req = mkreq(1, 0, true, 100, 0, 0x42);
  o = c.step(s, "volcano/drain-shared");
  check(o.grant && o.shared, "volcano: view 1 then takes the whole reserve", 1,
        (o.grant && o.shared) ? 1 : 0);
  // ... and is then refused everything.
  s.req = mkreq(1, 0, true, 1, 0, 0x43);
  o = c.step(s, "volcano/view1-now-broke");
  check(!o.grant, "volcano: view 1 has nothing left", 0, o.grant ? 1 : 0);

  check(c.dut.avail_geom0_o == kGuaranteed,
        "volcano: view 0's guaranteed pool is untouched by all of that", kGuaranteed,
        c.dut.avail_geom0_o);

  // view 0 now spends every last token of its guarantee.
  int granted = 0;
  for (uint32_t k = 0; k < kGuaranteed; ++k) {
    s.req = mkreq(0, 0, false, 1, 1, static_cast<uint16_t>(0x100 + (k & 0xFF)));
    o = c.step(s, "volcano/view0-spends");
    if (o.grant) ++granted;
  }
  check(granted == static_cast<int>(kGuaranteed),
        "volcano: view 0 got EVERY token of its guarantee anyway", kGuaranteed, granted);
  check(c.dut.avail_geom0_o == 0u, "volcano: and spent exactly all of it", 0, c.dut.avail_geom0_o);
}

// --------------------------------------------------------------------- 6 --
// The return path. T4: the return names the pool. T5: it cannot inflate a pool
// past its budget — checked AT the budget and one past it, constructed.
void test_returns(Cosim& c) {
  c.reset();
  c.load(mkbud(100, 100, 100, 100, 100), "ret/load");

  Stim s;
  s.budgets = c.budget_;

  // draw 60 from view-0 geometry
  s.req = mkreq(0, 0, false, 60, 5, 0x51);
  c.step(s, "ret/draw");
  check(c.dut.avail_geom0_o == 40u, "return: pool at 40 after a 60 draw", 40, c.dut.avail_geom0_o);

  // return EXACTLY what was drawn
  s.req = zm::TokenRequest();
  s.ret.valid = true;
  s.ret.view = 0;
  s.ret.cls = 0;
  s.ret.shared = false;
  s.ret.cost = 60;
  c.step(s, "ret/exact");
  check(c.dut.avail_geom0_o == 100u, "return: the pool is back at exactly its budget", 100,
        c.dut.avail_geom0_o);

  // return ONE MORE than the budget can hold — the clamp boundary
  s.ret.cost = 1;
  c.step(s, "ret/one-over");
  check(c.dut.avail_geom0_o == 100u, "return: a credit past the budget CLAMPS, not wraps", 100,
        c.dut.avail_geom0_o);

  // and a colossal credit still clamps rather than wrapping
  s.ret.cost = 0xFFFFFFFFu;
  c.step(s, "ret/huge");
  check(c.dut.avail_geom0_o == 100u, "return: 0xFFFFFFFF still clamps at the budget", 100,
        c.dut.avail_geom0_o);

  // T4: a SHARED draw returns to the shared pool, and the private one is not
  // touched by either half of the round trip.
  s.ret = zm::TokenReturn();
  s.req = mkreq(1, 1, true, 100, 5, 0x52);  // fits frag1 privately
  c.step(s, "ret/shared-setup-a");
  s.req = mkreq(1, 1, true, 100, 5, 0x53);  // now private is empty -> shared
  Obs o = c.step(s, "ret/shared-draw");
  check(o.grant && o.shared, "return: the second draw came from the reserve", 1,
        (o.grant && o.shared) ? 1 : 0);
  check(c.dut.avail_shared_o == 0u, "return: the reserve is spent", 0, c.dut.avail_shared_o);

  s.req = zm::TokenRequest();
  s.ret.valid = true;
  s.ret.view = 1;
  s.ret.cls = 1;
  s.ret.shared = true;  // the echo the grant asked for
  s.ret.cost = 100;
  c.step(s, "ret/shared-back");
  check(c.dut.avail_shared_o == 100u, "return: the reserve is restored, not the private pool", 100,
        c.dut.avail_shared_o);
  check(c.dut.avail_frag1_o == 0u, "return: and the private pool stayed where it was", 0,
        c.dut.avail_frag1_o);
}

// --------------------------------------------------------------------- 7 --
// Law T6: a return landing in the same cycle as a request does not help it.
// Constructed so the request fits ONLY if the credit were forwarded.
void test_same_cycle_return(Cosim& c) {
  c.reset();
  c.load(mkbud(20, 20, 20, 20, 0), "samecycle/load");

  Stim s;
  s.budgets = c.budget_;
  s.req = mkreq(0, 0, false, 15, 6, 0x61);
  c.step(s, "samecycle/spend");
  check(c.dut.avail_geom0_o == 5u, "same-cycle: pool at 5", 5, c.dut.avail_geom0_o);

  // 10 requested against 5, with a credit of 10 arriving the SAME cycle
  s.req = mkreq(0, 0, false, 10, 6, 0x62);
  s.ret.valid = true;
  s.ret.view = 0;
  s.ret.cls = 0;
  s.ret.shared = false;
  s.ret.cost = 10;
  Obs o = c.step(s, "samecycle/collide");
  check(!o.grant, "same-cycle: the credit does NOT help the request in its own cycle", 0,
        o.grant ? 1 : 0);
  check(c.dut.avail_geom0_o == 15u, "same-cycle: but the credit did land", 15, c.dut.avail_geom0_o);

  // one cycle later the identical request succeeds
  s.ret = zm::TokenReturn();
  o = c.step(s, "samecycle/next");
  check(o.grant, "same-cycle: the identical request succeeds one cycle later", 1, o.grant ? 1 : 0);

  // and the composition of a debit and a credit on the SAME pool in the SAME
  // cycle is exact: 5 - 3 + 4 = 6.
  c.load(mkbud(20, 20, 20, 20, 0), "samecycle/reload");
  s.budgets = c.budget_;
  s.req = mkreq(0, 0, false, 15, 6, 0x63);
  s.ret = zm::TokenReturn();
  c.step(s, "samecycle/to5");
  s.req = mkreq(0, 0, false, 3, 6, 0x64);
  s.ret.valid = true;
  s.ret.view = 0;
  s.ret.cls = 0;
  s.ret.shared = false;
  s.ret.cost = 4;
  o = c.step(s, "samecycle/both");
  check(o.grant, "same-cycle: the 3-token draw is granted against the standing 5", 1,
        o.grant ? 1 : 0);
  check(c.dut.avail_geom0_o == 6u, "same-cycle: 5 - 3 + 4 == 6, exactly", 6, c.dut.avail_geom0_o);
}

// --------------------------------------------------------------------- 8 --
// The ledger's `latency: fixed:1`, MEASURED. The grant is combinational in the
// request's own cycle (GEOM.BINNER law E); the denial presents exactly one
// cycle later and for exactly one cycle.
void test_latency(Cosim& c) {
  c.reset();
  c.load(mkbud(0, 0, 0, 0, 0), "latency/load");

  Stim s;
  s.budgets = c.budget_;
  s.req = mkreq(0, 0, false, 7, 5, 0xBEEF);
  Obs o = c.step(s, "latency/req");
  check(!o.grant, "latency: the request is refused", 0, o.grant ? 1 : 0);
  check(!o.den_valid, "latency: nothing is presented in the request's OWN cycle", 0,
        o.den_valid ? 1 : 0);

  o = c.idle("latency/+1");
  check(o.den_valid, "latency: the denial presents EXACTLY one cycle later", 1,
        o.den_valid ? 1 : 0);
  check(o.den_src_id == 0xBEEF, "latency: carrying its own src_id", 0xBEEF, o.den_src_id);
  check(o.den_cost == 7u, "latency: and its own cost", 7, o.den_cost);
  check(o.den_rep == 5, "latency: and its own ladder rung", 5, o.den_rep);

  o = c.idle("latency/+2");
  check(!o.den_valid, "latency: and it does not stick", 0, o.den_valid ? 1 : 0);
}

// --------------------------------------------------------------------- 9 --
// Law T9: a request colliding with a budget load is REFUSED WITH A REASON.
void test_reload_collision(Cosim& c) {
  c.reset();
  c.load(mkbud(1000, 1000, 1000, 1000, 1000), "reload/pre");

  Stim s;
  s.budget_valid = true;
  s.budgets = mkbud(7, 8, 9, 10, 11);
  c.budget_ = s.budgets;
  s.req = mkreq(0, 0, true, 1, 7, 0x71);  // would trivially fit the OLD pools
  Obs o = c.step(s, "reload/collide");
  check(!o.grant, "reload: no grant is made against state that is being replaced", 0,
        o.grant ? 1 : 0);
  check(c.dut.avail_geom0_o == 7u, "reload: the new budgets landed", 7, c.dut.avail_geom0_o);
  check(c.dut.avail_shared_o == 11u, "reload: including the reserve", 11, c.dut.avail_shared_o);

  o = c.idle("reload/reason");
  check(o.den_valid, "reload: and the refusal is NOT silent", 1, o.den_valid ? 1 : 0);
  check(o.den_reason == zm::kDenyReload, "reload: reason is RELOAD", zm::kDenyReload, o.den_reason);
}

// -------------------------------------------------------------------- 10 --
// Counters. All eight representation lanes move independently; triangles_culled
// adds the COST of a denied geometry request and nothing for a fragment; and it
// SATURATES at the rail, constructed rather than waited for.
void test_counters(Cosim& c) {
  c.reset();
  c.load(mkbud(1000, 1000, 1000, 1000, 0), "cnt/load");

  Stim s;
  s.budgets = c.budget_;
  // one grant on each of the eight rungs, k+1 times, so a swapped lane shows
  for (int rep = 0; rep < 8; ++rep) {
    for (int k = 0; k <= rep; ++k) {
      s.req = mkreq(0, 0, false, 1, rep, static_cast<uint16_t>(0x80 + rep));
      c.step(s, "cnt/grant");
    }
  }
  const uint32_t got[8] = {c.dut.tok_rep_count0_o, c.dut.tok_rep_count1_o, c.dut.tok_rep_count2_o,
                           c.dut.tok_rep_count3_o, c.dut.tok_rep_count4_o, c.dut.tok_rep_count5_o,
                           c.dut.tok_rep_count6_o, c.dut.tok_rep_count7_o};
  for (int rep = 0; rep < 8; ++rep) {
    check(got[rep] == static_cast<uint32_t>(rep + 1), "counters: each ladder rung counts its own",
          rep + 1, got[rep]);
  }

  // a denied GEOMETRY request adds its COST
  c.reset();
  c.load(mkbud(0, 0, 0, 0, 0), "cnt/load2");
  s.budgets = c.budget_;
  s.req = mkreq(0, 0, false, 500, 0, 0x91);
  c.step(s, "cnt/deny-geom");
  check(c.dut.triangles_culled_o == 500u, "counters: a denied geometry request culls its COST", 500,
        c.dut.triangles_culled_o);

  // a denied FRAGMENT request adds NOTHING — fragments are not triangles
  s.req = mkreq(0, 1, false, 500, 0, 0x92);
  c.step(s, "cnt/deny-frag");
  check(c.dut.triangles_culled_o == 500u, "counters: a denied fragment culls no triangles", 500,
        c.dut.triangles_culled_o);

  // THE RAIL, constructed: two denials of 0xFFFFFFFF must saturate, not wrap.
  s.req = mkreq(0, 0, false, 0xFFFFFFFFu, 0, 0x93);
  c.step(s, "cnt/rail-a");
  check(c.dut.triangles_culled_o == 0xFFFFFFFFu, "counters: the first huge denial reaches the rail",
        0xFFFFFFFFu, c.dut.triangles_culled_o);
  c.step(s, "cnt/rail-b");
  check(c.dut.triangles_culled_o == 0xFFFFFFFFu, "counters: and it SATURATES rather than wrapping",
        0xFFFFFFFFu, c.dut.triangles_culled_o);
}

// -------------------------------------------------------------------- 11 --
// The ledger's `target_throughput: 1 grant decision per clock`, MEASURED over
// a long back-to-back burst with no idle cycle anywhere.
void test_sustained_rate(Cosim& c) {
  c.reset();
  const int kN = 4096;
  c.load(mkbud(kN, kN, kN, kN, 0), "rate/load");

  Stim s;
  s.budgets = c.budget_;
  int decisions = 0;
  for (int k = 0; k < kN; ++k) {
    s.req = mkreq(k & 1, (k >> 1) & 1, false, 1, k & 7, static_cast<uint16_t>(k));
    const Obs o = c.step(s, "rate/burst");
    if (o.grant) ++decisions;
  }
  check(decisions == kN, "rate: one grant decision every clock, for 4096 clocks with no bubble", kN,
        decisions);
  std::printf("  rate: %d grants in %d clocks = %.3f clocks/decision\n", decisions, kN,
              static_cast<double>(kN) / decisions);
}

}  // namespace

int main() {
  Cosim c;
  test_exact_fit(c);
  test_emergency_pool(c);
  test_low_priority_is_confined(c);
  test_duo_guarantee(c);
  test_returns(c);
  test_same_cycle_return(c);
  test_latency(c);
  test_reload_collision(c);
  test_counters(c);
  test_sustained_rate(c);

  const int rc = zhao::report_and_exit("measure_tokens_directed");
  zhao::exit_hard(rc);
}
