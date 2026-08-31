// raster_attrdiv_svc_directed.cpp — the divide service: does it stay IN ORDER,
// and does adding units actually buy throughput?
//
// ---------------------------------------------------------------------------
// WHAT IS AT RISK
// ---------------------------------------------------------------------------
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
constexpr double kUnitLatency = (ZHAO_SVC_RADIX == 4) ? 20.0 : 36.0;

int64_t div_rhu(__int128 n, int64_t d) {
  const __int128 dd = d;
  return static_cast<int64_t>((n >= 0) ? ((2 * n + dd) / (2 * dd)) : -((-2 * n + dd) / (2 * dd)));
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
  bool ovf;
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
      out->push_back(
          {static_cast<uint16_t>(t.tag_o), static_cast<int32_t>(t.q_o), t.q_overflow_o != 0});
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

    long out_of_order = 0, wrong = 0, ovf = 0;
    for (size_t i = 0; i < got.size(); ++i) {
      if (got[i].tag != static_cast<uint16_t>(i)) {
        if (out_of_order < 3)
          printf("      position %zu carries tag %u\n", i, (unsigned)got[i].tag);
        ++out_of_order;
      }
      // The value must belong to the tag it arrived with, which is what makes
      // "in order" mean something rather than just "the tags count up".
      if (got[i].tag < reqs.size() && got[i].q != reqs[got[i].tag].want) ++wrong;
      if (got[i].ovf) ++ovf;
    }
    zhao::check(got.size() == reqs.size(), "every request is answered exactly once",
                (uint32_t)reqs.size(), (uint32_t)got.size());
    zhao::check(out_of_order == 0, "answers arrive in issue order, tag by tag", 0,
                (uint32_t)out_of_order);
    zhao::check(wrong == 0, "and each answer is its own request's exact quotient", 0,
                (uint32_t)wrong);
    zhao::check(ovf == 0, "no in-range divide is reported as an overflow", 0, (uint32_t)ovf);
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

  return zhao::report_and_exit("raster_attrdiv_svc_directed");
}
