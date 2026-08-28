// field_v3_ring_directed.cpp — UOP_RING_PREP for four points at once, against
// zfield::steps::ring_prepared, which is the step the planner actually emits.
//
// WHY THE ORACLE IS ring_prepared AND NOT exec_op(OP_RING)
// --------------------------------------------------------
// `zfield_plan.cpp` lowers RING three ways, and only one of them reaches a
// vector unit:
//
//   nothing varying          the whole op is computed ONCE in the prep
//   d varying, radii uniform UOP_RING_PREP  <- this block
//   radii varying            the full OP_RING, which needs a per-point
//                            reciprocal and is NOT implemented in hardware
//
// So `ring_prepared` is the shipped semantics for what this unit does, and
// checking against `exec_op(OP_RING)` would be checking against a different
// lowering. The two agree by construction -- the planner's comment says the
// nine products are unchanged -- but the differential should compare against
// what the plan emits, not against what the op is called.
//
// The prep values (m, rA, rB) come from the reference's own steps too:
// ring_mid and field_rcp. Computing them in this file with a divide would be
// inventing a second definition of the thing under test.
//
// LAWS:
//   1. NINE PRODUCTS, EACH ROUNDED SEPARATELY. A fused chain is a different
//      number; only the reference can say which.
//   2. THE CLAMP IS PART OF t, immediately after its product and before the
//      square. Section 3 drives d far outside [r0, r1] in both directions so
//      both rails are exercised.
//   3. d IS PER POINT; r0, m, rA and rB are SHARED. Section 2 gives four
//      points four different distances against one prepared ring.
//   4. THE BANK CAN REFUSE, and section 5 proves it did.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_v3_ring.h"

#include "zfield/zfield_steps.hpp"
#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr int kLanes = 4;

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint64_t next64() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  }
  uint32_t below(uint32_t n) { return n ? (uint32_t)(next64() % n) : 0; }
};

// ---- the four-wide bank model --------------------------------------------
struct MulBank {
  bool busy = false;
  int cnt = 0;
  int64_t p[kLanes] = {0, 0, 0, 0};
  bool grant = true;
  int refusals = 0;
};

int64_t sx33(uint64_t v) { return ((int64_t)(v << 31)) >> 31; }

template <typename W>
void set66(W& w, int64_t p) {
  w[0] = (uint32_t)((uint64_t)p & 0xFFFFFFFFull);
  w[1] = (uint32_t)(((uint64_t)p >> 32) & 0xFFFFFFFFull);
  w[2] = (p < 0) ? 0x3u : 0x0u;
}

void step(Vzhao_field_v3_ring& dut, MulBank& mb) {
  dut.mul_ready_i = mb.grant ? 1 : 0;
  if (mb.busy && mb.cnt == 0) {
    set66(dut.mul_p_0_i, mb.p[0]);
    set66(dut.mul_p_1_i, mb.p[1]);
    set66(dut.mul_p_2_i, mb.p[2]);
    set66(dut.mul_p_3_i, mb.p[3]);
    dut.mul_valid_i = 1;
    mb.busy = false;
  } else {
    dut.mul_valid_i = 0;
  }
  dut.eval();
  if (dut.mul_issue_o && !mb.grant) {
    ++mb.refusals;
  } else if (dut.mul_issue_o) {
    mb.p[0] = sx33(dut.mul_a_0_o) * sx33(dut.mul_b_0_o);
    mb.p[1] = sx33(dut.mul_a_1_o) * sx33(dut.mul_b_1_o);
    mb.p[2] = sx33(dut.mul_a_2_o) * sx33(dut.mul_b_2_o);
    mb.p[3] = sx33(dut.mul_a_3_o) * sx33(dut.mul_b_3_o);
    mb.busy = true;
    mb.cnt = 1;
  } else if (mb.busy && mb.cnt > 0) {
    --mb.cnt;
  }
  zhao::tick(dut);
}

/** A prepared ring: the values the planner's PREP path computes once. */
struct Prep {
  int32_t r0, r1, m, rA, rB;
};

Prep prepare(int32_t r0, int32_t r1) {
  Prep p{};
  p.r0 = r0;
  p.r1 = r1;
  zref::SatLedger L;
  // ring_mid and field_rcp are the reference's own, for the same reason the
  // value check is: a divide written here would be a second definition.
  p.m = zfield::steps::ring_mid(r0, r1, &L);
  p.rA = zref::field_rcp(zref::fx16{(int32_t)(p.m - r0)}, &L).raw;
  p.rB = zref::field_rcp(zref::fx16{(int32_t)(r1 - p.m)}, &L).raw;
  return p;
}

struct Want {
  int32_t r[kLanes];
};

Want oracle(const Prep& p, const int32_t* d) {
  Want w{};
  for (int l = 0; l < kLanes; ++l) {
    zref::SatLedger L;
    w.r[l] = zfield::steps::ring_prepared(d[l], p.r0, p.m, p.rA, p.rB, &L);
  }
  return w;
}

void drive(Vzhao_field_v3_ring& dut, const Prep& p, const int32_t* d, uint8_t tag) {
  dut.v_valid_i = 1;
  dut.d_0_i = (uint32_t)d[0];
  dut.d_1_i = (uint32_t)d[1];
  dut.d_2_i = (uint32_t)d[2];
  dut.d_3_i = (uint32_t)d[3];
  dut.r0_i = (uint32_t)p.r0;
  dut.m_i = (uint32_t)p.m;
  dut.rA_i = (uint32_t)p.rA;
  dut.rB_i = (uint32_t)p.rB;
  dut.tag_i = tag;
}

int run_one(Vzhao_field_v3_ring& dut, MulBank& mb, const Prep& p, const int32_t* d, uint8_t tag,
            const std::string& what) {
  const Want w = oracle(p, d);
  drive(dut, p, d, tag);
  dut.r_ready_i = 1;
  dut.eval();
  int guard = 0;
  while (!dut.v_ready_o && guard++ < 256) step(dut, mb);
  step(dut, mb);
  dut.v_valid_i = 0;
  dut.eval();
  int cycles = 0;
  while (!dut.r_valid_o && cycles < 512) {
    step(dut, mb);
    ++cycles;
  }
  check(cycles < 512, (what + ": reply arrived").c_str(), 1, cycles < 512 ? 1 : 0);
  const uint32_t got[kLanes] = {dut.o0_0_o, dut.o0_1_o, dut.o0_2_o, dut.o0_3_o};
  for (int l = 0; l < kLanes; ++l) {
    check(got[l] == (uint32_t)w.r[l], (what + ": lane " + std::to_string(l)).c_str(),
          (uint32_t)w.r[l], got[l]);
  }
  check(dut.tag_o == tag, (what + ": tag").c_str(), tag, dut.tag_o);
  step(dut, mb);
  dut.eval();
  return cycles;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int random_n = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--random") && i + 1 < argc) random_n = atoi(argv[i + 1]);
  }

  Vzhao_field_v3_ring dut;
  MulBank mb;
  dut.rst_n = 0;
  dut.v_valid_i = 0;
  dut.r_ready_i = 0;
  dut.mul_valid_i = 0;
  dut.mul_ready_i = 1;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);

  Prng rng(random_n ? 0x21C6u + (uint32_t)random_n : 0x21C6u);

  if (random_n == 0) {
    printf("== section 1: a ring, sampled across its whole span ==\n");
    {
      const Prep p = prepare(1 << 16, 5 << 16);  // r0 = 1.0, r1 = 5.0
      const int32_t d[kLanes] = {1 << 16, 2 << 16, 3 << 16, 5 << 16};
      run_one(dut, mb, p, d, 0x01, "span, on the knots");
      const int32_t d2[kLanes] = {(3 << 16) / 2, (5 << 16) / 2, (7 << 16) / 2, (9 << 16) / 2};
      run_one(dut, mb, p, d2, 0x02, "span, between them");
    }

    printf("== section 2: four points, four DIFFERENT distances, one ring ==\n");
    {
      // Law 3: d is per point and the prepared values are shared. A unit that
      // broadcast d[0] would pass a group where all four are equal.
      Prng r(0x9114u);
      const Prep p = prepare(2 << 16, 9 << 16);
      for (int k = 0; k < 8; ++k) {
        int32_t d[kLanes];
        for (int l = 0; l < kLanes; ++l) d[l] = (int32_t)(r.below(12u << 16));
        run_one(dut, mb, p, d, (uint8_t)(0x10 + k), "distinct d " + std::to_string(k));
      }
    }

    printf("== section 3: BOTH clamp rails, on purpose ==\n");
    {
      // Law 2: t is clamped to [0, 1] straight after its product. Inside the
      // ring t0 saturates high while t1 is still climbing, outside it is the
      // other way, and far outside both rails hold.
      const Prep p = prepare(4 << 16, 6 << 16);
      const int32_t d[kLanes] = {(int32_t)0x80000000, 0, 5 << 16, (int32_t)0x7FFFFFFF};
      run_one(dut, mb, p, d, 0x20, "both rails");
      const int32_t d2[kLanes] = {-(100 << 16), 100 << 16, 4 << 16, 6 << 16};
      run_one(dut, mb, p, d2, 0x21, "far outside and exactly on");
    }

    printf("== section 4: a DEGENERATE ring, where the reciprocal is extreme ==\n");
    {
      // r1 - m and m - r0 are one LSB, so both prepared reciprocals are huge
      // and the products saturate. The reference decides what that means; the
      // point here is that the unit agrees with it rather than avoiding it.
      const Prep p = prepare(1 << 16, (1 << 16) + 2);
      const int32_t d[kLanes] = {0, 1 << 16, (1 << 16) + 1, (1 << 16) + 2};
      run_one(dut, mb, p, d, 0x30, "one-LSB ring");
    }

    printf("== section 5: the bank refuses, and the answers do not move ==\n");
    {
      Prng r(0xC0FFu);
      mb.refusals = 0;
      const Prep p = prepare(3 << 16, 11 << 16);
      const int kGroups = 12;
      for (int k = 0; k < kGroups; ++k) {
        int32_t d[kLanes];
        for (int l = 0; l < kLanes; ++l) d[l] = (int32_t)(r.below(14u << 16));
        const Want w = oracle(p, d);
        const uint8_t tag = (uint8_t)(0x80 + k);
        drive(dut, p, d, tag);
        dut.r_ready_i = 1;
        dut.eval();
        int guard = 0;
        while (!dut.v_ready_o && guard++ < 512) {
          mb.grant = (r.below(2) != 0);
          step(dut, mb);
        }
        mb.grant = true;
        step(dut, mb);
        dut.v_valid_i = 0;
        dut.eval();
        int cycles = 0;
        while (!dut.r_valid_o && cycles < 1024) {
          mb.grant = (r.below(2) != 0);
          step(dut, mb);
          ++cycles;
        }
        mb.grant = true;
        const std::string what = "contended group " + std::to_string(k);
        check(cycles < 1024, (what + ": reply arrived, no hang").c_str(), 1,
              cycles < 1024 ? 1 : 0);
        const uint32_t got[kLanes] = {dut.o0_0_o, dut.o0_1_o, dut.o0_2_o, dut.o0_3_o};
        for (int l = 0; l < kLanes; ++l) {
          check(got[l] == (uint32_t)w.r[l], (what + ": lane " + std::to_string(l)).c_str(),
                (uint32_t)w.r[l], got[l]);
        }
        step(dut, mb);
        dut.eval();
      }
      printf("   MEASURED: %d groups under refusal, %d refusals issued\n", kGroups, mb.refusals);
      check(mb.refusals > 0, "the bank ACTUALLY refused -- the test is not vacuous", 1,
            mb.refusals > 0 ? 1 : 0);
    }

    printf("== section 6: the request cost, uncontended ==\n");
    {
      const Prep p = prepare(1 << 16, 4 << 16);
      const int32_t d[kLanes] = {1 << 16, 2 << 16, 3 << 16, 4 << 16};
      const int c = run_one(dut, mb, p, d, 0x60, "cost");
      printf("   MEASURED four-point RING_PREP %d clocks\n", c);
      // Nine products at three clocks each plus the handover. Generous on
      // purpose: the SHAPE is what is asserted, not a schedule nobody pinned.
      check(c <= 48, "a four-point prepared RING costs at most 48 clocks", 48, c);
    }
  } else {
    printf("== random differential: %d groups against ring_prepared ==\n", random_n);
    for (int i = 0; i < random_n; ++i) {
      const int32_t r0 = (int32_t)rng.below(8u << 16);
      const int32_t r1 = r0 + 1 + (int32_t)rng.below(8u << 16);
      const Prep p = prepare(r0, r1);
      int32_t d[kLanes];
      for (int l = 0; l < kLanes; ++l) d[l] = (int32_t)rng.next64();
      run_one(dut, mb, p, d, (uint8_t)(i & 0xFF), "random group " + std::to_string(i));
    }
  }

  return zhao::report_and_exit("field_v3_ring_directed");
}
