// raster_attrdiv_svc_directed.cpp — the divide service: does it stay IN ORDER,
// and does adding units actually buy throughput?
//
// ---------------------------------------------------------------------------
// WHAT IS AT RISK
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// THE UNIT BECAME `zhao_raster_attrdiv_v2` ON 2026-09-20 (rulings R100/R104)
// ---------------------------------------------------------------------------
// Three things in this file moved with it, and each was a real assertion about
// the old unit rather than a cosmetic rename:
//
//   * THE LAW. `div_rhu` below restated v1's round-half-AWAY-FROM-ZERO. It now
//     calls `zref::render::div_rhu_s128`, the compiled reference, rather than
//     restating anything -- the divider's own enforcing test restates nothing
//     either, and a restatement is how two files come to agree with each other
//     and with nothing else.
//
//   * THE REFUSAL. Section 1 asserted `ovf == 0`. v1's single `q_overflow_o`
//     became v2's `q_saturated_o` + `q_error_o`, and the service publishes
//     both. In-range divides must raise NEITHER, which is the same claim in
//     the new vocabulary; section 5 is new and drives the saturation path on
//     purpose, because the old file never exercised it at all.
//
//   * THE LATENCY CONSTANT. `kUnitLatency` is what makes the UNITS-scaling
//     check mean something, and v2 walks 98 quotient positions where v1 walked
//     33. 36 -> 101 clocks at radix 2, 20 -> 52 at radix 4. Leaving the old
//     number here would have turned the scaling gate into a tolerance wide
//     enough to accept anything, which is precisely what its own comment warns
//     against.
//
// The single divider is already proved exact, so this file is not about
// arithmetic. Two other things can be wrong, and both are invisible to a casual
// test:
//
//   * ORDER. Fragments reach the tile store in raster order. With N units in
//     flight, a service that returned answers as they finished would silently
//     permute them. A test that issues the SAME value N times cannot see that,
//     so every request here carries a distinct value AND a distinct tag, and
//     the returned tag sequence is checked to be 0,1,2,... with no gaps.
//
//   * THROUGHPUT THAT DOES NOT SCALE. A service that round-robins its issue
//     pointer but accidentally serialises -- one unit's ready gating all of
//     them, or a retire pointer that blocks issue -- is perfectly correct and
//     completely pointless. So the rate is MEASURED, and the whole reason this
//     block exists is that the number moves with UNITS.
//
// The build makes one executable per UNITS so the sweep is real hardware
// elaborated at each point rather than a runtime knob.
//
// ---------------------------------------------------------------------------
// THE FRAME BUDGET THIS IS MEASURED AGAINST
// ---------------------------------------------------------------------------
// 1,666,667 clocks a frame. A terrain-primary component of 276,480 pixels needs
// at least invw24 each before early-Z, and a textured Gouraud triangle needs
// seven attributes per SURVIVING pixel. The rate printed below is what the
// service can actually deliver against that.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_attrdiv_svc.h"

#include "zhao_sim.hpp"
#include "zrender/internal.hpp"

#ifndef ZHAO_SVC_UNITS
#define ZHAO_SVC_UNITS 1
#endif
#ifndef ZHAO_SVC_RADIX
#define ZHAO_SVC_RADIX 2
#endif

namespace {

constexpr int64_t kClocksPerFrame = 1666667;

// One unit's measured latency, which is what UNITS divides into. These are the
// numbers raster_attrdiv_directed prints at each radix -- restated here so the
// scaling check below has something to be right or wrong ABOUT, rather than a
// tolerance wide enough to accept anything.
// V2 walks all 98 dividend positions where v1 walked 33, so these are 2.8x the
// numbers this constant carried until 2026-09-20. See the header.
constexpr double kUnitLatency = (ZHAO_SVC_RADIX == 4) ? 52.0 : 101.0;

// The compiled reference, not a restatement of it. v2 agreed with this function
// on every one of the 640,000 pairs R100 sampled; v1 did not, on 100% of
// negative exact halves with an even divisor.
int64_t div_rhu(__int128 n, int64_t d) {
  return zref::render::div_rhu_s128(n, static_cast<__int128>(d));
}

void put96(Vzhao_raster_attrdiv_svc& t, __int128 v) {
  for (int i = 0; i < 3; ++i) t.num_i[i] = static_cast<uint32_t>((v >> (32 * i)) & 0xFFFFFFFFu);
}

struct Req {
  __int128 num;
  uint64_t area;
  int64_t want;
};

struct Rsp {
  uint16_t tag;
  int64_t q;
  bool sat;
  bool err;
  uint64_t rem;
};

/** Build a spread of divides whose quotients stay inside the stated 32 bits. */
std::vector<Req> make_reqs(int n, uint64_t seed) {
  std::vector<Req> v;
  uint64_t s = seed;
  auto nxt = [&s]() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  };
  for (int i = 0; i < n; ++i) {
    const uint64_t d = (nxt() % 100000ull) + 1ull;
    const __int128 q = static_cast<int64_t>(nxt() % 2000000ull) - 1000000;
    const __int128 num = q * static_cast<__int128>(d) + static_cast<int64_t>(nxt() % d);
    v.push_back({num, d, div_rhu(num, static_cast<int64_t>(d))});
  }
  return v;
}

/**
 * Drive `reqs` through the service, draining as it goes.
 * `ready_pattern` is a bit pattern cycled over r_ready_i; ~0 means always ready.
 * Returns the clocks the whole batch took, or -1 on timeout.
 */
int64_t drive(Vzhao_raster_attrdiv_svc& t, const std::vector<Req>& reqs, std::vector<Rsp>* out,
              uint32_t ready_pattern) {
  size_t next = 0;
  int64_t clocks = 0;
  int pat = 0;
  const int64_t limit = static_cast<int64_t>(reqs.size()) * 200 + 5000;
  while (out->size() < reqs.size()) {
    const bool offering = next < reqs.size();
    if (offering) {
      put96(t, reqs[next].num);
      t.area_i = reqs[next].area;
      t.tag_i = static_cast<uint16_t>(next);
    }
    t.v_valid_i = offering ? 1 : 0;
    t.r_ready_i = ((ready_pattern >> (pat & 31)) & 1u) ? 1 : 0;
    t.eval();
    const bool took = offering && t.v_ready_o;
    if (t.r_valid_o && t.r_ready_i)
      out->push_back({static_cast<uint16_t>(t.tag_o), static_cast<int32_t>(t.q_o),
                      t.q_saturated_o != 0, t.q_error_o != 0,
                      static_cast<uint64_t>(t.rem_o)});
    zhao::tick(t);
    ++clocks;
    ++pat;
    if (took) ++next;
    if (clocks > limit) return -1;
  }
  t.v_valid_i = 0;
  t.eval();
  return clocks;
}

void reset(Vzhao_raster_attrdiv_svc& t) {
  t.rst_n = 0;
  t.v_valid_i = 0;
  t.r_ready_i = 0;
  t.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_raster_attrdiv_svc top;
  const int units = ZHAO_SVC_UNITS;
  printf("== the attribute divide service, UNITS = %d, RADIX = %d ==\n", units, ZHAO_SVC_RADIX);

  // ------------------------------------------------------------------ 1 ---
  printf("== section 1: distinct values come back in ISSUE ORDER ==\n");
  int64_t saturated_clocks = 0;
  {
    reset(top);
    const std::vector<Req> reqs = make_reqs(240, 0xBEEF01ull);
    std::vector<Rsp> got;
    saturated_clocks = drive(top, reqs, &got, ~0u);
    zhao::check(saturated_clocks > 0, "the batch completes", 1, saturated_clocks > 0 ? 1 : 0);

    long out_of_order = 0, wrong = 0, refused = 0, bad_pair = 0;
    for (size_t i = 0; i < got.size(); ++i) {
      if (got[i].tag != static_cast<uint16_t>(i)) {
        if (out_of_order < 3)
          printf("      position %zu carries tag %u\n", i, (unsigned)got[i].tag);
        ++out_of_order;
      }
      // The value must belong to the tag it arrived with, which is what makes
      // "in order" mean something rather than just "the tags count up".
      if (got[i].tag < reqs.size() && got[i].q != reqs[got[i].tag].want) ++wrong;
      if (got[i].sat || got[i].err) ++refused;
      // THE REMAINDER MUST SURVIVE THE SERVICE, not merely appear on its port.
      // The whole reason this port exists is that the stepping path seeds from
      // it, so it is checked against the same request's own dividend -- routing
      // the wrong unit's remainder alongside the right unit's quotient is
      // exactly the metadata-swap shape CLAUDE.md records, and a test that only
      // read `q_o` could not see it.
      if (got[i].tag < reqs.size()) {
        const Req& rq = reqs[got[i].tag];
        const __int128 A = static_cast<__int128>(rq.area);
        const __int128 M = rq.num + static_cast<__int128>(rq.area / 2);
        if (static_cast<__int128>(got[i].q) * A + static_cast<__int128>(got[i].rem) != M ||
            got[i].rem >= rq.area)
          ++bad_pair;
      }
    }
    zhao::check(got.size() == reqs.size(), "every request is answered exactly once",
                (uint32_t)reqs.size(), (uint32_t)got.size());
    zhao::check(out_of_order == 0, "answers arrive in issue order, tag by tag", 0,
                (uint32_t)out_of_order);
    zhao::check(wrong == 0, "and each answer is its own request's exact quotient", 0,
                (uint32_t)wrong);
    zhao::check(refused == 0, "no in-range divide saturates or errors", 0, (uint32_t)refused);
    zhao::check(bad_pair == 0,
                "each answer's rem_o is ITS OWN request's: q*area + rem == num + area/2", 0,
                (uint32_t)bad_pair);
    zhao::check(top.rem_range_seen_o == 0, "no unit's residue outgrew the 47 bits rem_o carries",
                0, (uint32_t)top.rem_range_seen_o);
    zhao::check(top.accepted_o == top.retired_o && top.retired_o == reqs.size(),
                "accepted and retired agree with the batch size", (uint32_t)reqs.size(),
                (uint32_t)top.retired_o);
  }

  // ------------------------------------------------------------------ 2 ---
  printf("== section 2: the rate, and whether UNITS buys anything ==\n");
  {
    // 240 divides at a saturating producer. The ramp is 36 clocks of it, so the
    // rate below understates the steady state slightly -- deliberately, because
    // a measurement that flatters itself is worse than none.
    const double per_divide = static_cast<double>(saturated_clocks) / 240.0;
    const int64_t per_frame = static_cast<int64_t>(kClocksPerFrame / per_divide);
    printf("   MEASURED: 240 divides in %lld clocks = %.2f clocks a divide\n",
           (long long)saturated_clocks, per_divide);
    printf("   THROUGHPUT: %lld attribute-pixels a frame at UNITS = %d, RADIX = %d\n",
           (long long)per_frame, units, ZHAO_SVC_RADIX);
    printf("   AGAINST: 276480 terrain-primary pixels need invw24 each before early-Z\n");
    printf("   VERDICT: %s for depth alone\n", per_frame >= 276480 ? "SUFFICIENT" : "SHORT");

    // The claim under test is that units are actually parallel. One unit
    // measures kUnitLatency; N units must beat that over N by a margin that no
    // bug could fake. 15% of headroom absorbs the ramp.
    const double ideal = kUnitLatency / units;
    zhao::check(per_divide <= ideal * 1.15 + 1.0,
                "the measured rate tracks UNITS, so the units really are parallel",
                (uint32_t)(ideal * 100), (uint32_t)(per_divide * 100));
  }

  // ------------------------------------------------------------------ 3 ---
  printf("== section 3: a stalling consumer loses nothing and permutes nothing ==\n");
  {
    reset(top);
    const std::vector<Req> reqs = make_reqs(160, 0xC0DE77ull);
    std::vector<Rsp> got;
    // r_ready_i on an irregular pattern: long stalls, single-clock windows, and
    // a run of back-to-back accepts, so no unit gets a steady rhythm.
    const int64_t c = drive(top, reqs, &got, 0x8C1A5303u);
    zhao::check(c > 0, "the stalled batch still completes", 1, c > 0 ? 1 : 0);

    long bad = 0;
    for (size_t i = 0; i < got.size(); ++i)
      if (got[i].tag != static_cast<uint16_t>(i) || got[i].q != reqs[i].want) ++bad;
    zhao::check(got.size() == reqs.size() && bad == 0,
                "every answer survives backpressure, in order and exact", 0, (uint32_t)bad);
  }

  // ------------------------------------------------------------------ 4 ---
  printf("== section 4: the service REPORTS its own refusals ==\n");
  {
    // The Field lane's lesson: the wall is whichever resource refuses, and a
    // service that cannot report a refusal cannot be sized. A saturating
    // producer against a finite number of 36-clock units MUST be refused, so a
    // zero here would mean the counter is decoration.
    reset(top);
    const std::vector<Req> reqs = make_reqs(120, 0x5EED11ull);
    std::vector<Rsp> got;
    drive(top, reqs, &got, ~0u);
    printf("   MEASURED: %u stall clocks over %u accepted\n", (unsigned)top.stall_clocks_o,
           (unsigned)top.accepted_o);
    zhao::check(top.stall_clocks_o > 0,
                "a saturating producer is refused, and the refusals are counted", 1,
                top.stall_clocks_o > 0 ? 1 : 0);
    zhao::check(top.accepted_o == 120 && top.retired_o == 120,
                "and nothing is dropped while being refused", 120, (uint32_t)top.retired_o);
  }

  // ------------------------------------------------------------------ 5 ---
  printf("== section 5: refusals and saturations keep their PLACE IN THE QUEUE ==\n");
  {
    // NEW WITH THE V2 SWAP, and the old file had no equivalent: it asserted
    // that overflow never happens and never drove it. v1 REFUSED where v2
    // SATURATES, so this is the mapping decision being exercised on both sides
    // rather than reasoned about.
    //
    // The service-level risk is not the arithmetic -- the unit's own test owns
    // that -- it is that an exceptional answer takes a DIFFERENT PATH through
    // the unit (D_SAT jumps straight to D_DONE, skipping all 98 D_RUN clocks),
    // so it completes far sooner than its neighbours. A pool that retired by
    // completion rather than by issue order would let it overtake, and every
    // downstream law here assumes raster order.
    reset(top);
    std::vector<Req> reqs = make_reqs(24, 0xFA11EDull);
    const uint64_t A = 12346;
    // A saturating divide at position 5, another at 13, a zero-area refusal at
    // 9, each surrounded by ordinary work that must not move.
    const __int128 hi = (static_cast<__int128>(INT32_MAX) + 5) * static_cast<__int128>(A);
    const __int128 lo = (static_cast<__int128>(INT32_MIN) - 5) * static_cast<__int128>(A);
    reqs[5] = {hi, A, INT32_MAX};
    reqs[13] = {lo, A, INT32_MIN};
    reqs[9] = {777, 0, 0};  // zero area: a terminal error, q is meaningless

    std::vector<Rsp> got;
    const int64_t c = drive(top, reqs, &got, ~0u);
    zhao::check(c > 0 && got.size() == reqs.size(), "the mixed batch completes in full",
                (uint32_t)reqs.size(), (uint32_t)got.size());

    long order_bad = 0, value_bad = 0;
    for (size_t i = 0; i < got.size(); ++i) {
      if (got[i].tag != static_cast<uint16_t>(i)) ++order_bad;
      if (i == 9) continue;  // the error's q carries nothing to compare
      if (got[i].q != reqs[i].want) ++value_bad;
    }
    zhao::check(order_bad == 0,
                "an exceptional answer completes early and still retires IN ISSUE ORDER", 0,
                (uint32_t)order_bad);
    zhao::check(value_bad == 0, "and every ordinary neighbour is untouched", 0,
                (uint32_t)value_bad);

    // The mapping itself: saturation is a VALUE, the zero area is a REFUSAL.
    zhao::check(got[5].sat && !got[5].err && got[5].q == INT32_MAX && got[5].rem == 0,
                "positive saturation: q_saturated_o only, clamped q, rem_o zero", 1,
                (uint32_t)(got[5].sat && !got[5].err && got[5].q == INT32_MAX && got[5].rem == 0));
    zhao::check(got[13].sat && !got[13].err && got[13].q == INT32_MIN && got[13].rem == 0,
                "negative saturation: q_saturated_o only, clamped q, rem_o zero", 1,
                (uint32_t)(got[13].sat && !got[13].err && got[13].q == INT32_MIN &&
                           got[13].rem == 0));
    zhao::check(got[9].err && !got[9].sat,
                "a zero area raises q_error_o and NOT q_saturated_o: they are two events", 1,
                (uint32_t)(got[9].err && !got[9].sat));
    printf("   MEASURED: 2 saturations and 1 terminal error retired in order among 21 ordinary\n");
  }

  return zhao::report_and_exit("raster_attrdiv_svc_directed");
}
