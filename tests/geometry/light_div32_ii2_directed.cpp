// light_div32_ii2_directed.cpp -- qualification of the folded II=2 quotient.
//
// WHAT THE ORACLE IS, AND WHY IT IS NOT A SECOND COPY OF THE RTL
// ---------------------------------------------------------------------------
// The arithmetic under test is the divide inside the ratified light law. The
// law's compiled form is `zref::render::shade_from_world_normal_unclamped`,
// which is `div_rhu_s128(ndot, isqrt_u64(nmag2))`. So the differential tier
// (section 7) drives NORMALS AND LIGHTS through the compiled law and through
// this block, deriving the block's operands the way the streaming shell will
// -- and never restates the rounding rule in C++.
//
// The edge tiers (sections 2..6) cannot be expressed as normals: there is no
// normal/light pair whose numerator is exactly 2^63, and the domain edges are
// the whole point of qualifying a divider. Those tiers use `__int128`
// division from the host compiler, which is an INDEPENDENT implementation of
// floor division, not this file's reading of the RTL. Where the two tiers
// overlap they must agree, and section 7 is where they do.
//
// THE NEGATIVE CONTROL THAT MATTERS
// ---------------------------------------------------------------------------
// The one cheap-looking mistake this architecture invites is seeding the
// remainder at zero and dividing only the low word -- which is right for every
// numerator below 2^32 and wrong above it. Section 6 constructs numerators
// whose high word is nonzero and whose true quotient is representable, and
// asserts the RTL does NOT return the low-word answer. That is a control on
// the instrument, stated as a property of the correct machine (the block
// returns the true quotient) rather than as an assertion about a bug.
//
// COUNTERS / OBSERVABILITY
// ---------------------------------------------------------------------------
// This primitive has no counters -- it has two status BITS, `saturated_o` and
// `degenerate_o`, and both are seen to move as deltas in their own sections
// (4, 5, 2). Neither is asserted zero anywhere.
//
// POSITIVE CONTROL: --break-oracle corrupts ONE expected quotient by +1 and
// the suite must then FAIL.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_light_div32_ii2.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_shade.hpp"
#include "zref/zref_trig.hpp"

using zhao::check;

namespace {

uint64_t g_cycles = 0;
void tk(Vzhao_light_div32_ii2& d) {
  zhao::tick(d);
  ++g_cycles;
}

void reset_dut(Vzhao_light_div32_ii2& d) {
  d.rst_n = 0;
  d.v_valid_i = 0;
  d.r_ready_i = 0;
  d.num_i = 0;
  d.den_i = 0;
  d.neg_i = 0;
  d.tag_i = 0;
  d.eval();
  for (int i = 0; i < 3; ++i) tk(d);
  d.rst_n = 1;
  d.eval();
  tk(d);
}

// ---------------------------------------------------------------------------
// The independent reference for the edge tiers: floor division on __int128,
// then the law's INT32 rail. `h` is already the biased numerator.
// ---------------------------------------------------------------------------
struct Expect {
  int32_t result;
  bool saturated;
  bool degenerate;
};

Expect expect_of(__int128 h, uint32_t den) {
  Expect e{0, false, false};
  if (den == 0) {
    e.degenerate = true;
    return e;
  }
  const __int128 d = static_cast<__int128>(den);
  __int128 q = h / d;
  const __int128 r = h % d;
  if (r != 0 && r < 0) q -= 1;  // floor, not truncate
  if (q > INT32_MAX) {
    e.result = INT32_MAX;
    e.saturated = true;
  } else if (q < INT32_MIN) {
    e.result = INT32_MIN;
    e.saturated = true;
  } else {
    e.result = static_cast<int32_t>(q);
  }
  return e;
}

struct Req {
  uint64_t num;  // |h|
  uint32_t den;
  bool neg;  // h < 0
  uint16_t tag;
  Expect exp;
  const char* what;
};

Req req_of(__int128 h, uint32_t den, uint16_t tag, const char* what) {
  Req q;
  const __int128 a = (h < 0) ? -h : h;
  q.num = static_cast<uint64_t>(a);
  q.den = den;
  q.neg = (h < 0);
  q.tag = tag;
  q.exp = expect_of(h, den);
  q.what = what;
  return q;
}

struct Got {
  int32_t result;
  bool saturated;
  bool degenerate;
  uint16_t tag;
};

// ---------------------------------------------------------------------------
// Stream a batch through the block with a caller-chosen stall pattern, and
// return every response in order. Accept and retire cycles are recorded
// SEPARATELY, because a block that overlaps nothing has latency == II by
// accident and a quoted latency then silently becomes an optimistic II the
// first time anything does overlap.
// ---------------------------------------------------------------------------
struct StreamStats {
  uint64_t first_accept = 0;
  uint64_t last_accept = 0;
  uint64_t first_retire = 0;
  uint64_t last_retire = 0;
  uint64_t accepted = 0;
  uint64_t retired = 0;
  uint64_t first_latency = 0;
};

// stall_mod == 0 means the consumer never stalls.
std::vector<Got> stream(Vzhao_light_div32_ii2& d, const std::vector<Req>& rs, int stall_mod,
                        int bubble_mod, StreamStats* st) {
  std::vector<Got> out;
  out.reserve(rs.size());
  size_t sent = 0;
  uint64_t guard = 0;
  StreamStats s;
  while (out.size() < rs.size()) {
    // Consumer policy for THIS cycle.
    const bool consumer_ready =
        (stall_mod == 0) || ((g_cycles % static_cast<uint64_t>(stall_mod)) != 0);
    // Producer policy: an input bubble every bubble_mod cycles.
    const bool want_offer =
        sent < rs.size() &&
        (bubble_mod == 0 || ((g_cycles % static_cast<uint64_t>(bubble_mod)) != 0));

    d.r_ready_i = consumer_ready ? 1 : 0;
    if (want_offer) {
      d.v_valid_i = 1;
      d.num_i = rs[sent].num;
      d.den_i = rs[sent].den;
      d.neg_i = rs[sent].neg ? 1 : 0;
      d.tag_i = rs[sent].tag;
    } else {
      d.v_valid_i = 0;
    }
    d.eval();

    const bool accept_now = want_offer && d.v_ready_o;
    const bool retire_now = d.r_valid_o && consumer_ready;
    Got g{};
    if (retire_now) {
      g.result = static_cast<int32_t>(d.result_o);
      g.saturated = d.saturated_o != 0;
      g.degenerate = d.degenerate_o != 0;
      g.tag = static_cast<uint16_t>(d.tag_o);
    }

    const uint64_t now = g_cycles;
    tk(d);

    if (accept_now) {
      if (s.accepted == 0) s.first_accept = now;
      s.last_accept = now;
      ++s.accepted;
      ++sent;
    }
    if (retire_now) {
      if (s.retired == 0) {
        s.first_retire = now;
        s.first_latency = now - s.first_accept;
      }
      s.last_retire = now;
      ++s.retired;
      out.push_back(g);
    }
    if (++guard > 4000000) {
      check(false, "divider stream never completed", 1, 0);
      zhao::exit_hard(zhao::report_and_exit("light_div32_ii2_directed"));
    }
  }
  d.v_valid_i = 0;
  d.r_ready_i = 0;
  d.eval();
  if (st) *st = s;
  return out;
}

void check_batch(Vzhao_light_div32_ii2& d, const std::vector<Req>& rs, int stall_mod,
                 int bubble_mod, int inject_index, int32_t inject) {
  StreamStats st;
  const std::vector<Got> got = stream(d, rs, stall_mod, bubble_mod, &st);
  check(got.size() == rs.size(), "every request produced exactly one response", rs.size(),
        got.size());
  for (size_t i = 0; i < got.size() && i < rs.size(); ++i) {
    const int32_t want = rs[i].exp.result + ((static_cast<int>(i) == inject_index) ? inject : 0);
    check(got[i].result == want, rs[i].what, static_cast<uint32_t>(want),
          static_cast<uint32_t>(got[i].result));
    check(got[i].saturated == rs[i].exp.saturated, "saturated_o matches the law's INT32 rail",
          rs[i].exp.saturated ? 1 : 0, got[i].saturated ? 1 : 0);
    check(got[i].degenerate == rs[i].exp.degenerate, "degenerate_o matches den==0",
          rs[i].exp.degenerate ? 1 : 0, got[i].degenerate ? 1 : 0);
    check(got[i].tag == rs[i].tag, "the tag came back with its own result", rs[i].tag, got[i].tag);
  }
}

uint64_t g_rng = 0xD1B54A32D192ED03ULL;
uint64_t rnd() {
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 7;
  g_rng ^= g_rng << 17;
  return g_rng;
}

int32_t rnd_normal_lane() {
  switch (rnd() & 7) {
    case 0:
      return static_cast<int32_t>(rnd() & 7) - 3;
    case 1:
      return static_cast<int32_t>(rnd() % 131073) - 65536;
    case 2:
      return (rnd() & 1) ? INT32_MAX - static_cast<int32_t>(rnd() & 3)
                         : INT32_MIN + static_cast<int32_t>(rnd() & 3);
    case 3:
      return static_cast<int32_t>(rnd()) >> 16;
    default:
      return static_cast<int32_t>(rnd());
  }
}

}  // namespace

int main(int argc, char** argv) {
  bool break_oracle = false;
  for (int i = 1; i < argc; ++i)
    if (!std::strcmp(argv[i], "--break-oracle")) break_oracle = true;

  Verilated::commandArgs(argc, argv);
  Vzhao_light_div32_ii2* dutp = new Vzhao_light_div32_ii2;  // heap, harness rule
  Vzhao_light_div32_ii2& dut = *dutp;
  reset_dut(dut);

  // ---- 1: RESET STATE ------------------------------------------------------
  {
    check(dut.r_valid_o == 0, "no result offered out of reset", 0, dut.r_valid_o);
    dut.r_ready_i = 0;
    dut.eval();
    // `v_ready_o` is high on the fold's phase-0 clocks only -- that IS the
    // II=2 acceptance rule, so "ready every cycle" would be the bug, not the
    // pass. What must hold is that it rises within one phase either way.
    int rose = 0;
    for (int i = 0; i < 2; ++i) {
      if (dut.v_ready_o) {
        rose = 1;
        break;
      }
      tk(dut);
      dut.eval();
    }
    check(rose == 1, "the input port becomes ready within one phase of reset", 1,
          static_cast<uint32_t>(rose));
  }

  // ---- 2: den == 0 IS DEGENERATE, NOT A DIVIDE ----------------------------
  // "No divide by zero is permitted to masquerade as a valid lit result."
  // The block must answer zero AND raise degenerate_o -- a zero with the flag
  // low is indistinguishable from a legitimately unlit face.
  {
    std::vector<Req> rs;
    rs.push_back(req_of(0, 0, 0x0201, "den==0 answers zero"));
    rs.push_back(req_of(static_cast<__int128>(1) << 62, 0, 0x0202,
                        "den==0 answers zero for a large numerator too"));
    rs.push_back(req_of(-(static_cast<__int128>(1) << 62), 0, 0x0203,
                        "den==0 answers zero for a negative numerator too"));
    check(rs[0].exp.degenerate, "the reference agrees den==0 is degenerate", 1, 1);
    check_batch(dut, rs, 0, 0, -1, 0);
  }

  // ---- 3: THE SMALL EXACT CASES A READER CAN CHECK WITH A PENCIL ----------
  // den==1 (the quotient is the numerator), equality inside the subtraction
  // (num == den, num == 2*den), and both signs of every one.
  {
    std::vector<Req> rs;
    rs.push_back(req_of(0, 1, 0x0301, "0 / 1 == 0"));
    rs.push_back(req_of(7, 1, 0x0302, "7 / 1 == 7"));
    rs.push_back(req_of(-7, 1, 0x0303, "-7 / 1 == -7"));
    rs.push_back(req_of(5, 5, 0x0304, "5 / 5 == 1 (equality in the subtraction)"));
    rs.push_back(req_of(-5, 5, 0x0305, "-5 / 5 == -1, exactly, no floor adjust"));
    rs.push_back(req_of(10, 5, 0x0306, "10 / 5 == 2"));
    rs.push_back(req_of(9, 5, 0x0307, "9 / 5 == 1"));
    rs.push_back(req_of(-9, 5, 0x0308, "-9 / 5 == -2 (FLOOR, not truncate)"));
    rs.push_back(req_of(-1, 2, 0x0309, "-1 / 2 == -1 (floor of -0.5)"));
    rs.push_back(req_of(1, 2, 0x030A, "1 / 2 == 0"));
    rs.push_back(req_of(0xFFFFFFFFULL, 0xFFFFFFFFu, 0x030B, "UINT32_MAX / UINT32_MAX == 1"));
    rs.push_back(req_of(0xFFFFFFFEULL, 0xFFFFFFFFu, 0x030C, "one below the divisor gives 0"));
    check_batch(dut, rs, 0, 0, -1, 0);
    // The floor cases are the ones that separate this from a truncating
    // divide, so they are pinned by hand as well as by the reference.
    check(expect_of(-9, 5).result == -2, "reference: -9/5 floors to -2", static_cast<uint32_t>(-2),
          static_cast<uint32_t>(expect_of(-9, 5).result));
    check(expect_of(-5, 5).result == -1, "reference: -5/5 is exactly -1", static_cast<uint32_t>(-1),
          static_cast<uint32_t>(expect_of(-5, 5).result));
  }

  // ---- 4: THE SIGNED RAILS, BOTH SIDES, ON AND OFF BY ONE -----------------
  // INT32_MIN is exactly representable and must NOT report saturated;
  // INT32_MIN - 1 must. Likewise INT32_MAX and INT32_MAX + 1. A rail test
  // that only visits the far side cannot tell a correct clamp from one that
  // is one out.
  {
    std::vector<Req> rs;
    const __int128 kMax = INT32_MAX;
    const __int128 kMin = INT32_MIN;
    rs.push_back(req_of(kMax, 1, 0x0401, "INT32_MAX / 1 is exact, NOT saturated"));
    rs.push_back(req_of(kMax + 1, 1, 0x0402, "INT32_MAX + 1 saturates high"));
    rs.push_back(req_of(kMin, 1, 0x0403, "INT32_MIN / 1 is exact, NOT saturated"));
    rs.push_back(req_of(kMin - 1, 1, 0x0404, "INT32_MIN - 1 saturates low"));
    // A negative case whose floor adjustment is what pushes it over the rail:
    // -(2^31 * 5 + 1) / 5 == -(2^31) - 1 -> saturates, and only because of
    // the nonzero remainder.
    rs.push_back(req_of(-((static_cast<__int128>(1) << 31) * 5 + 1), 5, 0x0405,
                        "the floor adjustment itself pushes past INT32_MIN"));
    rs.push_back(req_of(-((static_cast<__int128>(1) << 31) * 5), 5, 0x0406,
                        "the same quotient with zero remainder lands exactly on INT32_MIN"));
    check(rs[4].exp.saturated, "reference: the floor-adjusted case really does saturate", 1,
          rs[4].exp.saturated ? 1 : 0);
    check(!rs[5].exp.saturated, "reference: the exact case really does not", 0,
          rs[5].exp.saturated ? 1 : 0);
    check_batch(dut, rs, 0, 0, -1, 0);
  }

  // ---- 5: THE 'big' BYPASS -- QUOTIENT >= 2^32 -----------------------------
  // When num[63:32] >= den the true quotient needs more than 32 bits and the
  // answer is on the rail whatever the low word does. The recurrence runs on
  // zeros; the verdict rides the same ordered pipeline as the payload.
  {
    std::vector<Req> rs;
    rs.push_back(
        req_of(static_cast<__int128>(UINT64_MAX), 1, 0x0501, "UINT64_MAX / 1 saturates high"));
    rs.push_back(
        req_of(-static_cast<__int128>(UINT64_MAX), 1, 0x0502, "-UINT64_MAX / 1 saturates low"));
    // den exactly on the boundary: num = 2^63, den = 2^31 -> q = 2^32, the
    // first quotient the signed word cannot hold.
    rs.push_back(req_of(static_cast<__int128>(1) << 63, 1u << 31, 0x0503,
                        "q == 2^32 exactly is the first unrepresentable quotient"));
    // One less divisor step: num = 2^63 - 2^31, den = 2^31 -> q = 2^32 - 1,
    // which still exceeds INT32_MAX and must saturate WITHOUT the big path.
    rs.push_back(req_of((static_cast<__int128>(1) << 63) - (static_cast<__int128>(1) << 31),
                        1u << 31, 0x0504,
                        "q == 2^32 - 1 saturates through the ordinary recurrence"));
    check_batch(dut, rs, 0, 0, -1, 0);
  }

  // ---- 6: THE NEGATIVE CONTROL ON THE SEEDED REMAINDER --------------------
  // The architecture skips 32 of the 64 recurrence steps. That is exact only
  // because `rem` is seeded with num[63:32]. A build that seeded zero would
  // return floor(num_lo / den) and would be RIGHT for every numerator below
  // 2^32 -- so the whole edge set above would pass. These cases have a
  // nonzero high word AND a representable quotient, which is the only region
  // where the two implementations differ AND the answer is observable.
  {
    std::vector<Req> rs;
    int checked_distinct = 0;
    for (int i = 0; i < 64; ++i) {
      const uint64_t hi = 1u + static_cast<uint32_t>(rnd() % 0x7FFFu);
      const uint64_t lo = rnd();
      const uint64_t num = (hi << 32) | (lo & 0xFFFFFFFFull);
      // Choose den so the quotient lands inside INT32: den > num / 2^31.
      uint64_t den = (num >> 31) + 1u + (rnd() % 4096u);
      if (den > 0xFFFFFFFFull) den = 0xFFFFFFFFull;
      if (den == 0) den = 1;
      const uint32_t d32 = static_cast<uint32_t>(den);
      const bool neg = (rnd() & 1) != 0;
      const __int128 h = neg ? -static_cast<__int128>(num) : static_cast<__int128>(num);
      // The low-word-only answer this control exists to exclude.
      const uint64_t wrong = (num & 0xFFFFFFFFull) / d32;
      if (static_cast<uint64_t>(expect_of(h, d32).result < 0 ? -expect_of(h, d32).result
                                                             : expect_of(h, d32).result) != wrong)
        ++checked_distinct;
      rs.push_back(req_of(h, d32, static_cast<uint16_t>(0x0600 + i),
                          "high-word seed: the true quotient, not the low-word one"));
    }
    check(checked_distinct >= 60,
          "coverage: the control cases really do separate the two implementations", 60,
          static_cast<uint32_t>(checked_distinct));
    check_batch(dut, rs, 0, 0, -1, 0);
  }

  // ---- 7: THE DIFFERENTIAL TIER, AGAINST THE COMPILED LAW -----------------
  // Normals and lights in, and the block's operands derived exactly as
  // `zhao_light_stream` derives them: dot in the full signed domain, then
  // h = dot + (mag >> 1), then sign/magnitude. The expectation comes from
  // `zref::render::shade_from_world_normal_unclamped` -- the shipped law,
  // compiled, not restated here.
  int cov_neg = 0, cov_sat = 0, cov_degen = 0, cov_mid = 0;
  {
    std::vector<Req> rs;
    for (int i = 0; i < 700; ++i) {
      const uint64_t shape = rnd() % 5;
      int32_t nx, ny, nz, lx, ly, lz;
      if (shape == 0) {
        // The law's INT32 rail is reachable only where the normal is TINY and
        // the light is huge -- a unit-ish normal under a rail sun. Sampling
        // uniformly never lands here (7 hits in 700 before this arm existed),
        // so the saturation coverage number would have read like coverage and
        // not been one.
        nx = static_cast<int32_t>(rnd() % 5) - 2;
        ny = static_cast<int32_t>(rnd() % 5) - 2;
        nz = static_cast<int32_t>(rnd() % 5) - 2;
        lx = (rnd() & 1) ? INT32_MAX : INT32_MIN + 1;
        ly = (rnd() & 1) ? INT32_MAX : INT32_MIN + 1;
        lz = (rnd() & 1) ? INT32_MAX : INT32_MIN + 1;
      } else {
        nx = rnd_normal_lane();
        ny = rnd_normal_lane();
        nz = rnd_normal_lane();
        if ((rnd() % 37) == 0) {
          nx = 0;
          ny = 0;
          nz = 0;
        }
        switch (rnd() & 3) {
          case 0:
            lx = zref::terrain::kShadeLightX;
            ly = zref::terrain::kShadeLightY;
            lz = zref::terrain::kShadeLightZ;
            break;
          case 1:
            lx = static_cast<int32_t>(rnd());
            ly = static_cast<int32_t>(rnd());
            lz = static_cast<int32_t>(rnd());
            break;
          default:
            lx = static_cast<int32_t>(rnd() % 131073) - 65536;
            ly = static_cast<int32_t>(rnd() % 131073) - 65536;
            lz = static_cast<int32_t>(rnd() % 131073) - 65536;
            break;
        }
      }
      const int32_t law =
          zref::render::shade_from_world_normal_unclamped(nx, ny, nz, lx, ly, lz, nullptr);

      const uint64_t nmag2 = static_cast<uint64_t>(nx) * static_cast<uint64_t>(nx) +
                             static_cast<uint64_t>(ny) * static_cast<uint64_t>(ny) +
                             static_cast<uint64_t>(nz) * static_cast<uint64_t>(nz);
      const uint64_t mag = zref::isqrt_u64(nmag2);
      // The magnitude must fit the 32-bit divisor port. |n| <= sqrt(3)*2^31
      // exceeds UINT32_MAX for rail normals, so those are OUT OF DOMAIN for
      // this primitive and are the streaming shell's problem, not a licence
      // to truncate. Skip them here and record the skip.
      if (mag > 0xFFFFFFFFull) continue;
      const __int128 dot = static_cast<__int128>(nx) * lx + static_cast<__int128>(ny) * ly +
                           static_cast<__int128>(nz) * lz;
      const __int128 h = (mag == 0) ? 0 : dot + static_cast<__int128>(mag / 2);
      Req q = req_of(h, static_cast<uint32_t>(mag), static_cast<uint16_t>(rnd() & 0xFFFF),
                     "DIFFERENTIAL: the block's quotient IS the compiled law's");
      // The law's own answer, restated through the block's interface.
      const int32_t want = (mag == 0) ? 0 : q.exp.result;
      check(want == law, "the block's operand derivation reproduces the compiled law exactly",
            static_cast<uint32_t>(law), static_cast<uint32_t>(want));
      q.exp.result = want;
      if (mag == 0) {
        q.exp.degenerate = true;
        q.exp.saturated = false;
        ++cov_degen;
      }
      if (want < 0) ++cov_neg;
      if (q.exp.saturated) ++cov_sat;
      if (!q.exp.saturated && want > 0 && want < 0x10000) ++cov_mid;
      rs.push_back(q);
    }
    const int inject_at = break_oracle ? 5 : -1;
    check_batch(dut, rs, 0, 0, inject_at, 1);
    check(cov_neg > 50, "coverage: negative quotients", 50, static_cast<uint32_t>(cov_neg));
    check(cov_sat > 20, "coverage: saturating quotients", 20, static_cast<uint32_t>(cov_sat));
    check(cov_degen > 5, "coverage: degenerate (mag == 0) requests", 5,
          static_cast<uint32_t>(cov_degen));
    check(cov_mid > 50, "coverage: ordinary in-range quotients", 50,
          static_cast<uint32_t>(cov_mid));
  }

  // ---- 8: BUBBLES AND CONSUMER STALLS DO NOT MOVE AN ANSWER ---------------
  // The same batch, four traffic patterns. If the fold's phase alignment or
  // the global enable were wrong, the values would move with the pattern.
  {
    std::vector<Req> rs;
    for (int i = 0; i < 200; ++i) {
      const uint64_t num = rnd();
      uint32_t den = static_cast<uint32_t>(rnd());
      if (den == 0) den = 1;
      const bool neg = (rnd() & 1) != 0;
      const __int128 h = neg ? -static_cast<__int128>(num) : static_cast<__int128>(num);
      rs.push_back(req_of(h, den, static_cast<uint16_t>(0x0800 + i),
                          "value is independent of the traffic pattern"));
    }
    check_batch(dut, rs, 0, 0, -1, 0);  // no bubbles, no stalls
    check_batch(dut, rs, 3, 0, -1, 0);  // consumer stalls 1 cycle in 3
    check_batch(dut, rs, 0, 5, -1, 0);  // producer bubbles 1 cycle in 5
    check_batch(dut, rs, 7, 3, -1, 0);  // both
  }

  // ---- 9: RESET WITH EVERY STAGE OCCUPIED ---------------------------------
  // Fill the pipeline, assert reset mid-flight, then prove the block is not
  // holding a stale result and that the NEXT batch is correct. Abandoned
  // requests are abandoned; they must not reappear behind later answers.
  {
    for (int i = 0; i < 20; ++i) {
      dut.v_valid_i = 1;
      dut.num_i = 0xDEADBEEFCAFEBABEull;
      dut.den_i = 0x1234567u;
      dut.neg_i = 1;
      dut.tag_i = static_cast<uint16_t>(0x0900 + i);
      dut.r_ready_i = 0;
      dut.eval();
      tk(dut);
    }
    dut.v_valid_i = 0;
    dut.eval();
    reset_dut(dut);
    check(dut.r_valid_o == 0, "reset mid-flight leaves no result offered", 0, dut.r_valid_o);
    std::vector<Req> rs;
    rs.push_back(req_of(100, 7, 0x09FF, "the batch after a mid-flight reset is correct"));
    rs.push_back(req_of(-100, 7, 0x09FE, "and its negative floors correctly"));
    check_batch(dut, rs, 0, 0, -1, 0);
    check(rs[0].exp.result == 14, "pencil: 100/7 == 14", 14,
          static_cast<uint32_t>(rs[0].exp.result));
    check(rs[1].exp.result == -15, "pencil: -100/7 floors to -15", static_cast<uint32_t>(-15),
          static_cast<uint32_t>(rs[1].exp.result));
  }

  // ---- 10: THE SERVICE INTERVAL, MEASURED SEPARATELY FROM THE LATENCY -----
  // >= 4096 requests, no bubbles, no stalls. Accept-to-accept and
  // retire-to-retire are computed from their own timestamps; the first
  // latency is reported but never used as the rate.
  {
    std::vector<Req> rs;
    for (int i = 0; i < 4096; ++i) {
      const uint64_t num = rnd() >> 1;
      uint32_t den = static_cast<uint32_t>(rnd() | 1u);
      const __int128 h = static_cast<__int128>(num);
      rs.push_back(req_of(h, den, static_cast<uint16_t>(i & 0xFFFF), "II stream value"));
    }
    StreamStats st;
    const std::vector<Got> got = stream(dut, rs, 0, 0, &st);
    check(got.size() == rs.size(), "the II stream retired every request", rs.size(), got.size());
    for (size_t i = 0; i < got.size(); ++i)
      check(got[i].result == rs[i].exp.result, "II stream value is still exact",
            static_cast<uint32_t>(rs[i].exp.result), static_cast<uint32_t>(got[i].result));

    const double ii_accept =
        static_cast<double>(st.last_accept - st.first_accept) / static_cast<double>(rs.size() - 1);
    const double ii_retire =
        static_cast<double>(st.last_retire - st.first_retire) / static_cast<double>(rs.size() - 1);
    std::printf(
        "[div32] accept-to-accept II = %.4f clk | retire-to-retire II = %.4f clk | "
        "first accept->consume latency = %llu clk | %llu requests\n",
        ii_accept, ii_retire, static_cast<unsigned long long>(st.first_latency),
        static_cast<unsigned long long>(rs.size()));
    check(ii_accept <= 2.0001 && ii_accept >= 1.9999, "sustained ACCEPT interval is exactly 2", 2,
          static_cast<uint64_t>(ii_accept * 10000.0));
    check(ii_retire <= 2.0001 && ii_retire >= 1.9999, "sustained RETIRE interval is exactly 2", 2,
          static_cast<uint64_t>(ii_retire * 10000.0));
    check(st.first_latency >= 32 && st.first_latency <= 34,
          "latency is ~33 clocks and is NOT the service interval", 33,
          static_cast<uint64_t>(st.first_latency));
  }

  dut.final();
  zhao::exit_hard(zhao::report_and_exit("light_div32_ii2_directed"));
}
