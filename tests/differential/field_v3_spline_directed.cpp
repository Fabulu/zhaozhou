// field_v3_spline_directed.cpp — SPLINE's Catmull-Rom arithmetic for four
// points at once, against the ONE semantic layer.
//
// WHAT THE ORACLE IS, AND WHY IT IS BUILT THE WAY IT IS
// -----------------------------------------------------
// This block is the SECOND HALF of OP_SPLINE: the coefficients, the Horner and
// the half. The first half -- the six-step search, the clamp, the parameter t
// and the four neighbour reads -- belongs to a widened curve service and does
// not exist yet.
//
// So the test builds a real table, runs `zfield::steps::exec_op(OP_SPLINE)` on
// it for the whole answer, and separately derives the SAME t and the SAME four
// neighbours using the reference's own `segment_search` and `clamp_raw` to
// drive the unit. Both sides therefore agree on the lookup by construction,
// and any difference is in the arithmetic -- which is the only thing this block
// is responsible for.
//
// Deriving t with arithmetic written in this file would be inventing a second
// definition of the lookup, and the difference would land on the block under
// test. The reference's helpers are used instead.
//
// LAWS:
//   1. THE COEFFICIENTS SATURATE AT 32 BITS and their small multiples are
//      EXACT. Section 3 drives control points near the rail, where 2*p0 and
//      5*p1 overflow 32 bits and only the clamped result is legal.
//   2. fx_mad IS ONE ROUNDING. Every value check is against exec_op, which is
//      the only thing that can tell one rounding from two.
//   3. THE FINAL HALF IS A RESCALE, not a shift: it rounds half up and
//      saturates. Section 4 drives odd values where the two differ.
//   4. FOUR POINTS, FOUR SEGMENTS. Section 2 puts the four points in four
//      different segments of one table.
//   5. THE BANK CAN REFUSE, and section 5 proves it did.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_v3_spline.h"

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

void step(Vzhao_field_v3_spline& dut, MulBank& mb) {
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

/** What the lookup half would hand this block, derived with the REFERENCE's
    own search and clamp so both sides agree on it by construction. */
struct Lookup {
  int32_t t, p0, p1, p2, p3;
};

Lookup lookup_of(const zfield::Table& tab, int32_t a_in) {
  Lookup k{};
  const int n = (int)tab.x.size();
  const int i = zfield::steps::segment_search(tab, a_in);
  const int32_t a = zfield::steps::clamp_raw(a_in, tab.x[0], tab.x[(size_t)n - 1]);
  zref::SatLedger L;
  k.t = zref::fx_clamp(
            zref::fx16{zref::rescale_s32(
                (int64_t)(zref::fx_sub(zref::fx16{a}, zref::fx16{tab.x[(size_t)i]}, &L).raw) *
                    (int64_t)tab.dy[(size_t)i],
                16, &L)},
            zref::fx16{0}, zref::fx16{1 << 16})
            .raw;
  k.p0 = tab.y[(size_t)(i > 0 ? i - 1 : 0)];
  k.p1 = tab.y[(size_t)i];
  k.p2 = tab.y[(size_t)(i + 1 < n ? i + 1 : n - 1)];
  k.p3 = tab.y[(size_t)(i + 2 < n ? i + 2 : n - 1)];
  return k;
}

int32_t oracle_point(const std::vector<zfield::Table>& tabs, uint32_t slot, int32_t a) {
  zref::SatLedger L;
  const int32_t src[1] = {a};
  int32_t dst[1] = {0};
  zfield::steps::exec_op(zfield::OP_SPLINE, slot, tabs, src, dst, &L);
  return dst[0];
}

void drive(Vzhao_field_v3_spline& dut, const Lookup* k, uint8_t tag) {
  dut.v_valid_i = 1;
  dut.t_0_i = (uint32_t)k[0].t;   dut.t_1_i = (uint32_t)k[1].t;
  dut.t_2_i = (uint32_t)k[2].t;   dut.t_3_i = (uint32_t)k[3].t;
  dut.p0_0_i = (uint32_t)k[0].p0; dut.p0_1_i = (uint32_t)k[1].p0;
  dut.p0_2_i = (uint32_t)k[2].p0; dut.p0_3_i = (uint32_t)k[3].p0;
  dut.p1_0_i = (uint32_t)k[0].p1; dut.p1_1_i = (uint32_t)k[1].p1;
  dut.p1_2_i = (uint32_t)k[2].p1; dut.p1_3_i = (uint32_t)k[3].p1;
  dut.p2_0_i = (uint32_t)k[0].p2; dut.p2_1_i = (uint32_t)k[1].p2;
  dut.p2_2_i = (uint32_t)k[2].p2; dut.p2_3_i = (uint32_t)k[3].p2;
  dut.p3_0_i = (uint32_t)k[0].p3; dut.p3_1_i = (uint32_t)k[1].p3;
  dut.p3_2_i = (uint32_t)k[2].p3; dut.p3_3_i = (uint32_t)k[3].p3;
  dut.tag_i = tag;
}

int run_one(Vzhao_field_v3_spline& dut, MulBank& mb, const std::vector<zfield::Table>& tabs,
            uint32_t slot, const int32_t* a, uint8_t tag, const std::string& what) {
  Lookup k[kLanes];
  int32_t want[kLanes];
  for (int l = 0; l < kLanes; ++l) {
    k[l] = lookup_of(tabs[slot], a[l]);
    want[l] = oracle_point(tabs, slot, a[l]);
  }
  drive(dut, k, tag);
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
  if (cycles >= 512) return cycles;
  const uint32_t got[kLanes] = {dut.o0_0_o, dut.o0_1_o, dut.o0_2_o, dut.o0_3_o};
  for (int l = 0; l < kLanes; ++l) {
    check(got[l] == (uint32_t)want[l], (what + ": lane " + std::to_string(l)).c_str(),
          (uint32_t)want[l], got[l]);
  }
  check(dut.tag_o == tag, (what + ": tag").c_str(), tag, dut.tag_o);
  step(dut, mb);
  dut.eval();
  return cycles;
}

/** A table with ascending knots and the given control values. */
zfield::Table make_table(const std::vector<int32_t>& xs, const std::vector<int32_t>& ys) {
  zfield::Table t;
  t.kind = 1;  // spline
  t.x = xs;
  t.y = ys;
  t.dy.resize(xs.size());
  for (size_t i = 0; i + 1 < xs.size(); ++i) {
    // dy is the reciprocal of the segment width, which is what the lookup
    // multiplies by -- taken from the reference's own reciprocal so the table
    // is one a real program could carry.
    zref::SatLedger L;
    t.dy[i] = zref::field_rcp(zref::fx16{(int32_t)(xs[i + 1] - xs[i])}, &L).raw;
  }
  t.dy[xs.size() - 1] = t.dy[xs.size() - 2];
  return t;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int random_n = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--random") && i + 1 < argc) random_n = atoi(argv[i + 1]);
  }

  Vzhao_field_v3_spline dut;
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

  Prng rng(random_n ? 0x5914u + (uint32_t)random_n : 0x5913u);

  // One table used by most sections: five knots, a wave of control points.
  std::vector<zfield::Table> tabs(1);
  tabs[0] = make_table({0, 1 << 16, 2 << 16, 3 << 16, 4 << 16},
                       {0, 2 << 16, -(1 << 16), 3 << 16, 1 << 16});

  if (random_n == 0) {
    printf("== section 1: on the knots and between them ==\n");
    {
      const int32_t a[kLanes] = {0, 1 << 16, 2 << 16, 4 << 16};
      run_one(dut, mb, tabs, 0, a, 0x01, "on the knots");
      const int32_t b[kLanes] = {(1 << 15), (3 << 15), (5 << 15), (7 << 15)};
      run_one(dut, mb, tabs, 0, b, 0x02, "between the knots");
    }

    printf("== section 2: four points in FOUR DIFFERENT segments ==\n");
    {
      // Law 4: the four points land in four segments, so the four neighbour
      // sets and the four coefficient triples all differ. A unit that computed
      // one set for the request would pass any group inside one segment.
      const int32_t a[kLanes] = {(1 << 15), (3 << 15), (5 << 15), (7 << 15)};
      run_one(dut, mb, tabs, 0, a, 0x10, "one point per segment");
      // And outside the table at both ends, where the clamp decides.
      const int32_t b[kLanes] = {(int32_t)0x80000000, -1, (4 << 16) + 1, (int32_t)0x7FFFFFFF};
      run_one(dut, mb, tabs, 0, b, 0x11, "outside both ends");
    }

    printf("== section 3: control points near the rail ==\n");
    {
      // Law 1: 2*p0 and 5*p1 exceed 32 bits here, so the coefficients are
      // formed at 64 and only the RESULT is clamped. A 32-bit intermediate
      // wraps and gives a smooth, wrong curve.
      std::vector<zfield::Table> big(1);
      big[0] = make_table({0, 1 << 16, 2 << 16, 3 << 16, 4 << 16},
                          {(int32_t)0x7FFFFFFF, (int32_t)0x80000000, (int32_t)0x7FFFFFF0,
                           (int32_t)0x80000010, (int32_t)0x7F000000});
      const int32_t a[kLanes] = {(1 << 15), (3 << 15), (5 << 15), (7 << 15)};
      run_one(dut, mb, big, 0, a, 0x20, "coefficients at the rail");
    }

    printf("== section 4: odd values, where a rescale and a shift differ ==\n");
    {
      // Law 3: the final half rounds half up. A >>> 1 truncates toward
      // negative infinity, so the two differ on every odd NEGATIVE value.
      std::vector<zfield::Table> odd(1);
      odd[0] = make_table({0, 1 << 16, 2 << 16, 3 << 16, 4 << 16},
                          {-3, -1, 1, 3, 5});
      const int32_t a[kLanes] = {(1 << 14), (3 << 14), (5 << 14), (7 << 14)};
      run_one(dut, mb, odd, 0, a, 0x30, "odd control points");
      std::vector<zfield::Table> neg(1);
      neg[0] = make_table({0, 1 << 16, 2 << 16, 3 << 16, 4 << 16},
                          {-(5 << 16) - 1, -(3 << 16) - 1, -(1 << 16) - 1, (1 << 16) + 1,
                           (3 << 16) + 1});
      run_one(dut, mb, neg, 0, a, 0x31, "odd and negative");
    }

    printf("== section 5: the bank refuses, and the answers do not move ==\n");
    {
      Prng r(0x5AFEu);
      mb.refusals = 0;
      const int kGroups = 12;
      for (int g = 0; g < kGroups; ++g) {
        int32_t a[kLanes];
        for (int l = 0; l < kLanes; ++l) a[l] = (int32_t)r.below(5u << 16);
        Lookup k[kLanes];
        int32_t want[kLanes];
        for (int l = 0; l < kLanes; ++l) {
          k[l] = lookup_of(tabs[0], a[l]);
          want[l] = oracle_point(tabs, 0, a[l]);
        }
        const uint8_t tag = (uint8_t)(0x80 + g);
        drive(dut, k, tag);
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
        const std::string what = "contended group " + std::to_string(g);
        check(cycles < 1024, (what + ": reply arrived, no hang").c_str(), 1,
              cycles < 1024 ? 1 : 0);
        const uint32_t got[kLanes] = {dut.o0_0_o, dut.o0_1_o, dut.o0_2_o, dut.o0_3_o};
        for (int l = 0; l < kLanes; ++l)
          check(got[l] == (uint32_t)want[l], (what + ": lane " + std::to_string(l)).c_str(),
                (uint32_t)want[l], got[l]);
        step(dut, mb);
        dut.eval();
      }
      printf("   MEASURED: %d groups under refusal, %d refusals issued\n", kGroups, mb.refusals);
      check(mb.refusals > 0, "the bank ACTUALLY refused -- the test is not vacuous", 1,
            mb.refusals > 0 ? 1 : 0);
    }

    printf("== section 6: the request cost, uncontended ==\n");
    {
      const int32_t a[kLanes] = {(1 << 15), (3 << 15), (5 << 15), (7 << 15)};
      const int c = run_one(dut, mb, tabs, 0, a, 0x60, "cost");
      printf("   MEASURED four-point SPLINE arithmetic %d clocks\n", c);
      check(c <= 20, "three products and a finish, at most 20 clocks", 20, c);
    }
  } else {
    printf("== random differential: %d groups against exec_op(OP_SPLINE) ==\n", random_n);
    for (int i = 0; i < random_n; ++i) {
      int32_t a[kLanes];
      for (int l = 0; l < kLanes; ++l) a[l] = (int32_t)rng.next64();
      run_one(dut, mb, tabs, 0, a, (uint8_t)(i & 0xFF), "random group " + std::to_string(i));
    }
  }

  return zhao::report_and_exit("field_v3_spline_directed");
}
