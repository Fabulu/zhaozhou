// field_v3_mulbank_directed.cpp — the differential for the four-wide vector
// multiplier bank and its arbiter (fpga/rtl/field/zhao_field_v3_mulbank.sv).
//
// WHAT THE ORACLE IS HERE, AND WHY IT IS NOT A MODEL
// ---------------------------------------------------
// An arbiter has no shipped reference of its own -- there is no
// `zfield::arbitrate`. But the thing it routes DOES: every reply must equal
// the requester's own a*b, as exact 66-bit signed integer arithmetic. So the
// arithmetic is the oracle and ROUTING UNDER CONTENTION is the property under
// test.
//
// That framing is what makes this testable rather than tautological. A test
// that compared the RTL against a C model of the same arbitration would prove
// only that I wrote the same thing twice. Comparing against a*b proves that
// whoever asked got THEIR product and nobody else's.
//
// THE FOUR CLAIMS
// ---------------
//  1. EVERY REPLY IS THE REQUESTER'S OWN PRODUCT. Each claimant issues
//     operands that are recognisably its own, and every reply is checked
//     against that claimant's outstanding request. A misrouted product is a
//     wrong answer for two claimants at once.
//  2. NOTHING IS LOST. Requests accepted and replies delivered must balance
//     exactly, per claimant, over a long randomized run.
//  3. THE PRIORITY IS THE DECLARED ONE. With every claimant asking at once,
//     the highest-indexed service must win every clock and the lane group
//     must starve -- and `stall_lanes_o` must count exactly those clocks. The
//     starvation is DEMONSTRATED, not assumed away, because the RTL header
//     declares it possible rather than denying it.
//  4. `desync_o` STAYS LOW. It latches if the multiplier lanes' own valids
//     ever disagree with the tag shadow, which would mean a product routed to
//     a claimant that did not ask for it.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_v3_mulbank.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr int kClaimants = 3;
constexpr int kLanes = 4;

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint64_t next64() {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    uint64_t x = s;
    x ^= x >> 33;
    x *= 0xFF51AFD7ED558CCDULL;
    x ^= x >> 33;
    return x;
  }
  uint32_t below(uint32_t n) { return n ? (uint32_t)(next64() % n) : 0; }
  // Bounded to 32 bits signed so the EXACT product fits in int64 and the
  // oracle needs no wide arithmetic of its own. The multiplier's full 33-bit
  // range is zhao_field_mul's own concern and is swept there; what is under
  // test here is ROUTING, and routing does not care how wide the operand is.
  int64_t operand() {
    switch (below(6)) {
      case 0: return 0;
      case 1: return 1;
      case 2: return -1;
      case 3: return INT32_MAX;
      case 4: return INT32_MIN;
      default: return (int32_t)next64();
    }
  }
};

// One outstanding request, remembered so its reply can be checked.
struct Pending {
  int64_t a[kLanes], b[kLanes];
  uint8_t tag;
};

// The 33-bit operands the RTL sees, sign-extended into the C++ side.
int64_t sext33(int64_t v) {
  const uint64_t m = 1ULL << 32;
  const uint64_t u = (uint64_t)v & ((1ULL << 33) - 1);
  return (int64_t)((u ^ m) - m);
}

// A 66-bit product arrives as three 32-bit words. With operands bounded to
// 32 bits signed the true product fits in 64, so the low two words carry it
// and the top word is sign extension -- which is CHECKED rather than ignored,
// because a routing bug that delivered a different lane's product would
// usually show up there first.
template <typename W>
int64_t wide66(const W& w, bool* top_ok) {
  const uint64_t lo = (uint64_t)w[0] | ((uint64_t)w[1] << 32);
  const int64_t v = (int64_t)lo;
  const uint32_t want_top = (v < 0) ? 0x3u : 0x0u;  // bits [65:64]
  *top_ok = ((uint32_t)w[2] & 0x3u) == want_top;
  return v;
}

struct Bank {
  Vzhao_field_v3_mulbank& t;
  std::deque<Pending> outstanding[kClaimants];
  int accepted[kClaimants] = {};
  int delivered[kClaimants] = {};
  int misrouted = 0;
  int wrong_value = 0;

  explicit Bank(Vzhao_field_v3_mulbank& top) : t(top) {}

  void reset() {
    t.rst_n = 0;
    t.req_valid_i = 0;
    t.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
    zhao::tick(t);
  }

  void set_request(int c, const Pending& p) {
    for (int l = 0; l < kLanes; ++l) {
      t.req_a_i[c][l] = (uint64_t)p.a[l] & ((1ULL << 33) - 1);
      t.req_b_i[c][l] = (uint64_t)p.b[l] & ((1ULL << 33) - 1);
    }
    t.req_tag_i[c] = p.tag;
  }

  // Sample replies for the clock about to be taken, then advance.
  void step() {
    const uint32_t rvalid = t.rsp_valid_o;
    const uint8_t rtag = (uint8_t)t.rsp_tag_o;
    int64_t prod[kLanes];
    bool top_ok[kLanes];
    for (int l = 0; l < kLanes; ++l) prod[l] = wide66(t.rsp_p_o[l], &top_ok[l]);

    for (int c = 0; c < kClaimants; ++c) {
      if (!((rvalid >> c) & 1)) continue;
      if (outstanding[c].empty()) {
        ++misrouted;  // a reply for a claimant with nothing outstanding
        continue;
      }
      const Pending p = outstanding[c].front();
      outstanding[c].pop_front();
      ++delivered[c];
      if (p.tag != rtag) ++misrouted;
      for (int l = 0; l < kLanes; ++l) {
        if (prod[l] != sext33(p.a[l]) * sext33(p.b[l])) ++wrong_value;
        if (!top_ok[l]) ++wrong_value;  // the sign extension must agree too
      }
    }
    zhao::tick(t);
  }
};

// ---------------------------------------------------------------------------

// Every claimant asks at once, forever. The declared priority says the
// highest-indexed service wins every clock and the lane group starves.
void test_priority_is_the_declared_one(Vzhao_field_v3_mulbank& top) {
  printf("-- fixed priority: services outrank the lanes, and the lanes starve\n");
  Bank b(top);
  b.reset();
  Prng rng(0xA2B17E);

  for (int c = 0; c < kClaimants; ++c) {
    Pending p{};
    for (int l = 0; l < kLanes; ++l) {
      p.a[l] = rng.operand();
      p.b[l] = rng.operand();
    }
    p.tag = (uint8_t)(0x10 + c);
    b.set_request(c, p);
  }
  top.req_valid_i = (1u << kClaimants) - 1u;  // everyone asks, every clock
  // req_ready_o is COMBINATIONAL from req_valid_i, so it must be settled
  // before the first sample. Without this the first iteration reads the
  // pre-request value and the run scores 39 grants in 40 clocks -- a test
  // artefact that looks exactly like a one-clock arbiter bubble.
  top.eval();

  const int kClocks = 40;
  int granted_to[kClaimants] = {};
  for (int k = 0; k < kClocks; ++k) {
    for (int c = 0; c < kClaimants; ++c) {
      if ((top.req_ready_o >> c) & 1) {
        ++granted_to[c];
        Pending p{};
        for (int l = 0; l < kLanes; ++l) {
          p.a[l] = sext33((int64_t)top.req_a_i[c][l]);
          p.b[l] = sext33((int64_t)top.req_b_i[c][l]);
        }
        p.tag = (uint8_t)top.req_tag_i[c];
        b.outstanding[c].push_back(p);
        ++b.accepted[c];
      }
    }
    b.step();
  }
  top.req_valid_i = 0;

  printf("   MEASURED: grants lane-group=%d svc1=%d svc2=%d over %d clocks\n", granted_to[0],
         granted_to[1], granted_to[2], kClocks);
  check(granted_to[2] == kClocks, "the highest-priority service wins every clock", kClocks,
        granted_to[2]);
  check(granted_to[0] == 0, "the lane group gets nothing while a service asks", 0, granted_to[0]);
  check((int)top.stall_lanes_o == kClocks,
        "and stall_lanes_o counts exactly those starved clocks", kClocks,
        (int)top.stall_lanes_o);
  check(top.desync_o == 0, "the lanes stayed in step with the tag shadow", 0, (int)top.desync_o);
}

// Randomized contention: every reply must be the requester's own product.
void test_every_reply_is_its_own_product(Vzhao_field_v3_mulbank& top, int clocks) {
  printf("-- randomized contention over %d clocks\n", clocks);
  Bank b(top);
  b.reset();
  Prng rng(0x5EEDBA17);
  uint8_t next_tag[kClaimants] = {1, 1, 1};

  for (int k = 0; k < clocks; ++k) {
    uint32_t want = 0;
    Pending fresh[kClaimants];
    for (int c = 0; c < kClaimants; ++c) {
      if (rng.below(100) < 55) {
        want |= (1u << c);
        for (int l = 0; l < kLanes; ++l) {
          fresh[c].a[l] = rng.operand();
          fresh[c].b[l] = rng.operand();
        }
        // A tag that identifies the claimant AND the request, so a misroute
        // between claimants and a stale reply are distinguishable.
        fresh[c].tag = (uint8_t)((c << 6) | (next_tag[c] & 0x3F));
        b.set_request(c, fresh[c]);
      }
    }
    top.req_valid_i = want;
    top.eval();

    for (int c = 0; c < kClaimants; ++c) {
      if (((want >> c) & 1) && ((top.req_ready_o >> c) & 1)) {
        b.outstanding[c].push_back(fresh[c]);
        ++b.accepted[c];
        next_tag[c] = (uint8_t)(next_tag[c] + 1);
      }
    }
    b.step();
  }

  // Drain.
  top.req_valid_i = 0;
  for (int k = 0; k < 8; ++k) b.step();

  int acc = 0, del = 0;
  for (int c = 0; c < kClaimants; ++c) {
    acc += b.accepted[c];
    del += b.delivered[c];
  }
  printf("   MEASURED: %d accepted, %d delivered, %d grants, %d lane stalls\n", acc, del,
         (int)top.grants_o, (int)top.stall_lanes_o);
  check(b.wrong_value == 0, "every reply equals the requester's own a*b", 0, b.wrong_value);
  check(b.misrouted == 0, "no reply reached a claimant that did not ask for it", 0, b.misrouted);
  check(acc == del, "every accepted request produced exactly one reply", acc, del);
  check((int)top.grants_o == acc, "the grant counter matches what was accepted", acc,
        (int)top.grants_o);
  check(top.desync_o == 0, "the lanes stayed in step throughout", 0, (int)top.desync_o);
}

// THE LANE-STALL COUNTER MUST COUNT LOSSES, NOT REQUESTS.
//
// M08 -- making stall_lanes_o count every clock the lanes ASKED rather than
// only those they LOST -- survived the first sweep. The priority test has the
// lanes asking and losing on every single clock, so asking and losing are the
// same number there and the mutant is indistinguishable. The randomized lane
// printed the counter but never asserted on it.
//
// The distinguishing case is the lanes asking and WINNING. Alone, claimant 0
// takes the bank every clock, so a counter that counts requests reads the full
// clock count while a counter that counts losses reads ZERO.
void test_lane_stalls_count_losses_not_requests(Vzhao_field_v3_mulbank& top) {
  printf("-- the lane-stall counter counts losses, not requests\n");
  Bank b(top);
  b.reset();
  Prng rng(0x105E5);
  const int kClocks = 24;
  int granted = 0;
  for (int k = 0; k < kClocks; ++k) {
    Pending p{};
    for (int l = 0; l < kLanes; ++l) {
      p.a[l] = rng.operand();
      p.b[l] = rng.operand();
    }
    p.tag = (uint8_t)(k & 0x3F);
    b.set_request(0, p);          // ONLY the lane group asks
    top.req_valid_i = 0x1;
    top.eval();
    if (top.req_ready_o & 1) {
      b.outstanding[0].push_back(p);
      ++b.accepted[0];
      ++granted;
    }
    b.step();
  }
  top.req_valid_i = 0;
  for (int k = 0; k < 8; ++k) b.step();

  printf("   MEASURED: %d grants, %d lane stalls over %d clocks\n", granted,
         (int)top.stall_lanes_o, kClocks);
  check(granted == kClocks, "the lanes win every clock when nobody contends", kClocks, granted);
  check((int)top.stall_lanes_o == 0,
        "and stall_lanes_o stays ZERO -- it counts losses, not requests", 0,
        (int)top.stall_lanes_o);
  check(b.wrong_value == 0, "every product is still the requester's own", 0, b.wrong_value);
}

// A single claimant must sustain one request per clock -- the bank is
// pipelined, and if it did not the services would each cost 3x.
void test_sustains_one_per_clock(Vzhao_field_v3_mulbank& top) {
  printf("-- a lone claimant sustains one four-wide request per clock\n");
  Bank b(top);
  b.reset();
  Prng rng(0xF00D11);
  const int kClocks = 32;
  int granted = 0;
  for (int k = 0; k < kClocks; ++k) {
    Pending p{};
    for (int l = 0; l < kLanes; ++l) {
      p.a[l] = rng.operand();
      p.b[l] = rng.operand();
    }
    p.tag = (uint8_t)(k & 0x3F);
    b.set_request(1, p);
    top.req_valid_i = 0x2;
    top.eval();
    if ((top.req_ready_o >> 1) & 1) {
      b.outstanding[1].push_back(p);
      ++b.accepted[1];
      ++granted;
    }
    b.step();
  }
  top.req_valid_i = 0;
  for (int k = 0; k < 8; ++k) b.step();

  printf("   MEASURED: %d grants in %d clocks\n", granted, kClocks);
  check(granted == kClocks, "THE GATE: one request accepted every clock", kClocks, granted);
  check(b.wrong_value == 0, "and every product is correct", 0, b.wrong_value);
  check(b.accepted[1] == b.delivered[1], "and none were lost", b.accepted[1], b.delivered[1]);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--random" && i + 1 < argc) iters = std::atoi(argv[++i]);
  }

  Vzhao_field_v3_mulbank top;

  if (iters > 0) {
    test_every_reply_is_its_own_product(top, iters);
  } else {
    test_priority_is_the_declared_one(top);
    test_lane_stalls_count_losses_not_requests(top);
    test_every_reply_is_its_own_product(top, 300);
    test_sustains_one_per_clock(top);
  }
  return zhao::report_and_exit("FIELD.V3.MULBANK");
}
