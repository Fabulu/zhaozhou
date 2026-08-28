// field_v3_normalize_directed.cpp — NORMALIZE2 and NORMALIZE3 for four points
// at once, against the ONE semantic layer (zfield::steps::exec_op).
//
// LAWS, and where each is exercised:
//
//   1. THE ARITHMETIC IS THE REFERENCE'S. Every value goes through exec_op --
//      never through a normalisation reimplemented here, which would only
//      prove two reimplementations agree and would very likely agree on the
//      wrong rounding.
//   2. THE ZERO VECTOR IS A DEFINED ANSWER: outputs zero, and an RCP0 event
//      rather than a saturation. Section 3 drives it alone and mixed with
//      non-zero lanes, because a unit that handled it only when every lane
//      was zero would pass the first and fail the second.
//   3. e IS PER POINT. Section 2 drives four vectors whose lengths differ by
//      many binades in one group, so the four normalisation shifts are all
//      different. A unit that computed one exponent for the request would
//      pass every group of similar magnitude.
//   4. n2 IS EXACT. Section 4 drives components near INT32_MAX, where the sum
//      of three squares needs the full 64 bits and any early rounding shows.
//   5. NORMALIZE2 WRITES NO THIRD LANE.
//   6. THE BANK CAN REFUSE, and section 6 proves it did.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_v3_normalize.h"

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
  /** A component whose magnitude spans the whole binade range. */
  int32_t comp() {
    const int shift = (int)below(31);
    const int64_t v = (int64_t)(next64() & ((1ull << (shift + 1)) - 1));
    return (int32_t)((below(2) != 0) ? v : -v);
  }
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

void step(Vzhao_field_v3_normalize& dut, MulBank& mb) {
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

struct Group {
  int32_t x[kLanes], y[kLanes], z[kLanes];
};

struct Want {
  int32_t r[3][kLanes];
  bool rcp0[kLanes];
  bool sat[kLanes];
};

Want oracle(bool n3, const Group& g) {
  Want w{};
  for (int l = 0; l < kLanes; ++l) {
    const std::vector<zfield::Table> no_tables;
    zref::SatLedger L;
    int32_t dst[3] = {0, 0, 0};
    if (n3) {
      const int32_t src[3] = {g.x[l], g.y[l], g.z[l]};
      zfield::steps::exec_op(zfield::OP_NORMALIZE3, 0, no_tables, src, dst, &L);
      w.r[2][l] = dst[2];
    } else {
      const int32_t src[2] = {g.x[l], g.y[l]};
      zfield::steps::exec_op(zfield::OP_NORMALIZE2, 0, no_tables, src, dst, &L);
      w.r[2][l] = 0;  // law 5
    }
    w.r[0][l] = dst[0];
    w.r[1][l] = dst[1];
    w.rcp0[l] = (L.rcp0 != 0);
    w.sat[l] = (L.rescale != 0);
  }
  return w;
}

void drive(Vzhao_field_v3_normalize& dut, bool n3, const Group& g, uint8_t tag) {
  dut.v_valid_i = 1;
  dut.is_n3_i = n3 ? 1 : 0;
  dut.a0_0_i = (uint32_t)g.x[0];
  dut.a0_1_i = (uint32_t)g.x[1];
  dut.a0_2_i = (uint32_t)g.x[2];
  dut.a0_3_i = (uint32_t)g.x[3];
  dut.a1_0_i = (uint32_t)g.y[0];
  dut.a1_1_i = (uint32_t)g.y[1];
  dut.a1_2_i = (uint32_t)g.y[2];
  dut.a1_3_i = (uint32_t)g.y[3];
  dut.a2_0_i = (uint32_t)g.z[0];
  dut.a2_1_i = (uint32_t)g.z[1];
  dut.a2_2_i = (uint32_t)g.z[2];
  dut.a2_3_i = (uint32_t)g.z[3];
  dut.tag_i = tag;
}

int run_one(Vzhao_field_v3_normalize& dut, MulBank& mb, bool n3, const Group& g, uint8_t tag,
            const std::string& what) {
  const Want w = oracle(n3, g);
  drive(dut, n3, g, tag);
  dut.r_ready_i = 1;
  dut.eval();
  int guard = 0;
  while (!dut.v_ready_o && guard++ < 1024) step(dut, mb);
  step(dut, mb);
  dut.v_valid_i = 0;
  dut.eval();
  int cycles = 0;
  while (!dut.r_valid_o && cycles < 4000) {
    step(dut, mb);
    ++cycles;
  }
  check(cycles < 4000, (what + ": reply arrived").c_str(), 1, cycles < 4000 ? 1 : 0);
  if (cycles >= 4000) return cycles;

  const uint32_t got[3][kLanes] = {{dut.o0_0_o, dut.o0_1_o, dut.o0_2_o, dut.o0_3_o},
                                   {dut.o1_0_o, dut.o1_1_o, dut.o1_2_o, dut.o1_3_o},
                                   {dut.o2_0_o, dut.o2_1_o, dut.o2_2_o, dut.o2_3_o}};
  for (int l = 0; l < kLanes; ++l) {
    for (int m = 0; m < 3; ++m) {
      check(got[m][l] == (uint32_t)w.r[m][l],
            (what + ": lane " + std::to_string(l) + " dst" + std::to_string(m)).c_str(),
            (uint32_t)w.r[m][l], got[m][l]);
    }
    check(((dut.rcp0_o >> l) & 1) == (w.rcp0[l] ? 1u : 0u),
          (what + ": lane " + std::to_string(l) + " RCP0").c_str(), w.rcp0[l] ? 1 : 0,
          (dut.rcp0_o >> l) & 1);
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

  Vzhao_field_v3_normalize dut;
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

  Prng rng(random_n ? 0x4E02u + (uint32_t)random_n : 0x4E01u);

  if (random_n == 0) {
    printf("== section 1: unit-ish vectors, both widths ==\n");
    {
      Group g{};
      const int32_t xs[kLanes] = {1 << 16, 3 << 16, -(2 << 16), 1};
      const int32_t ys[kLanes] = {0, 4 << 16, 2 << 16, 1};
      const int32_t zs[kLanes] = {0, 0, 1 << 16, 1};
      for (int l = 0; l < kLanes; ++l) {
        g.x[l] = xs[l];
        g.y[l] = ys[l];
        g.z[l] = zs[l];
      }
      run_one(dut, mb, false, g, 0x01, "NORMALIZE2 simple");
      run_one(dut, mb, true, g, 0x02, "NORMALIZE3 simple");
    }

    printf("== section 2: four lengths, MANY BINADES apart, in one group ==\n");
    {
      // Law 3: the normalisation exponent is per point. A unit that computed
      // one e for the whole request passes any group of similar magnitude, so
      // the magnitudes here are deliberately spread across the range.
      Group g{};
      const int shifts[kLanes] = {2, 11, 21, 30};
      for (int l = 0; l < kLanes; ++l) {
        g.x[l] = (int32_t)(1 << shifts[l]);
        g.y[l] = (int32_t)((1 << shifts[l]) / 3);
        g.z[l] = (int32_t)((1 << shifts[l]) / 7);
      }
      run_one(dut, mb, true, g, 0x10, "binade spread, N3");
      run_one(dut, mb, false, g, 0x11, "binade spread, N2");
    }

    printf("== section 3: the ZERO vector, alone and mixed ==\n");
    {
      // Law 2, both ways round. A unit that handled zero only when every lane
      // was zero would pass the first of these and fail the second.
      Group all0{};
      run_one(dut, mb, true, all0, 0x20, "all four zero");

      Group mixed{};
      mixed.x[0] = 0;
      mixed.y[0] = 0;
      mixed.z[0] = 0;
      mixed.x[1] = 5 << 16;
      mixed.y[1] = 12 << 16;
      mixed.z[1] = 0;
      mixed.x[2] = 0;
      mixed.y[2] = 0;
      mixed.z[2] = 0;
      mixed.x[3] = -(7 << 16);
      mixed.y[3] = 24 << 16;
      mixed.z[3] = 1 << 16;
      // THE TWO OPS DISAGREE ABOUT THE LEDGER, and that is the reference's
      // doing rather than a choice made here: zfield::steps::normalize2 bumps
      // RCP0 for the zero vector, while zref::normalize3_approx returns zeros
      // and bumps NOTHING. The values are identical, so only the ledger tells
      // them apart -- which makes it exactly the kind of difference that gets
      // implemented once and applied to both.
      //
      // My first version of this section asserted 0x5 on an N3 group and
      // failed, because I had written the assertion before reading
      // normalize3_approx. Both directions are checked now.
      run_one(dut, mb, false, mixed, 0x21, "zero on lanes 0 and 2, NORMALIZE2");
      printf("   MEASURED rcp0_o = %X (N2: the zero lanes report)\n", dut.rcp0_o);
      check((dut.rcp0_o & 0x5u) == 0x5u, "N2 reports RCP0 on exactly the zero lanes", 5,
            dut.rcp0_o & 0x5u);
      check((dut.rcp0_o & 0xAu) == 0u, "and not on the others", 0, dut.rcp0_o & 0xAu);

      run_one(dut, mb, true, mixed, 0x22, "zero on lanes 0 and 2, NORMALIZE3");
      printf("   MEASURED rcp0_o = %X (N3: nothing is reported)\n", dut.rcp0_o);
      check(dut.rcp0_o == 0u, "N3 reports NOTHING for the same zero lanes", 0, (int)dut.rcp0_o);
    }

    printf("== section 4: components near the rail, where n2 needs all 64 bits ==\n");
    {
      // Law 4: three squares of near-INT32_MAX components sum to nearly 3*2^62.
      // Any early rounding of n2 changes the length and every output.
      Group g{};
      const int32_t big[kLanes] = {(int32_t)0x7FFFFFFF, (int32_t)0x80000000, (int32_t)0x7FFFFFFE,
                                   (int32_t)0x40000000};
      for (int l = 0; l < kLanes; ++l) {
        g.x[l] = big[l];
        g.y[l] = big[(l + 1) % kLanes];
        g.z[l] = big[(l + 2) % kLanes];
      }
      run_one(dut, mb, true, g, 0x30, "near the rail, N3");
      run_one(dut, mb, false, g, 0x31, "near the rail, N2");
    }

    printf("== section 4b: the reciprocal's u24 RAIL, and the ONE mantissa\n");
    printf("               that reaches it ==\n");
    {
      // WHY THIS SECTION EXISTS: mutant M18 moved the reciprocal's clamp one
      // binade -- u25 instead of u24 -- and SURVIVED, even though section 1
      // already drives an input that reaches the rail.
      //
      // The reference pins the rail exactly: rcp_u24_norm's ONLY saturating
      // input is m == 2^23, where the pre-clamp value is exactly 2^24 and the
      // clamp gives 0xFFFFFF -- "pinned law, not overflow", in its own words.
      // m is 2^23 precisely when the LENGTH is an exact power of two, and
      // section 1's first lane, (1<<16, 0, 0), is such a vector. So the rail
      // was BEING HIT and the answer was still right.
      //
      // It was right for the wrong reason. The two clamps differ by exactly 1
      // in a u24 reciprocal, so the products differ by the component itself,
      // while the output rescale is by 31 + e = 8 + log2(len). The component
      // is at most the length, so the gap is at most 1/256 of an output LSB:
      // invisible UNLESS it straddles a rounding boundary. Roughly one
      // component in 256 does. Section 1's did not.
      //
      // These four do. Each has an exact power-of-two length (2^17) and a
      // component chosen so that half-up rounding lands on opposite sides:
      //
      //   (131072,      1)   component 1       0 vs 1
      //   (131071,    512)   component 131071  65535 vs 65536
      //   (131071,    513)   component 513     256 vs 257
      //   (131070,    725)   component 725     362 vs 363
      //
      // The second is worth a second look: 131071^2 + 512^2 is 2^34 + 1,
      // one above the square of 2^17, so the floor-root lands on the power of
      // two by a single count. A length that is "nearly" a power of two does
      // NOT reach the rail, which is why these had to be solved for rather
      // than guessed.
      Group g{};
      const int32_t xs[kLanes] = {131072, 131071, 131071, 131070};
      const int32_t ys[kLanes] = {1, 512, 513, 725};
      for (int l = 0; l < kLanes; ++l) {
        g.x[l] = xs[l];
        g.y[l] = ys[l];
        g.z[l] = 0;  // N3 with a zero third component has the SAME length,
                     // so both widths meet the same rail
      }
      run_one(dut, mb, false, g, 0x34, "the u24 rail, N2");
      run_one(dut, mb, true, g, 0x35, "the u24 rail, N3");
    }

    printf("== section 5: NORMALIZE2 writes no third lane ==\n");
    {
      Group g{};
      for (int l = 0; l < kLanes; ++l) {
        g.x[l] = (int32_t)((l + 1) << 16);
        g.y[l] = (int32_t)((l + 2) << 16);
        g.z[l] = (int32_t)0x7FFFFFFF;  // must be IGNORED by N2
      }
      run_one(dut, mb, false, g, 0x40, "N2 ignores the third component");
      check(dut.o2_0_o == 0 && dut.o2_1_o == 0 && dut.o2_2_o == 0 && dut.o2_3_o == 0,
            "and its third output reads zero", 0,
            (uint32_t)(dut.o2_0_o | dut.o2_1_o | dut.o2_2_o | dut.o2_3_o));
    }

    printf("== section 6: the bank refuses, and the answers do not move ==\n");
    {
      Prng r(0xD00Du);
      mb.refusals = 0;
      const int kGroups = 8;
      for (int k = 0; k < kGroups; ++k) {
        Group g{};
        for (int l = 0; l < kLanes; ++l) {
          g.x[l] = r.comp();
          g.y[l] = r.comp();
          g.z[l] = r.comp();
        }
        const bool n3 = (k & 1) != 0;
        const uint8_t tag = (uint8_t)(0x80 + k);
        const Want w = oracle(n3, g);
        drive(dut, n3, g, tag);
        dut.r_ready_i = 1;
        dut.eval();
        int guard = 0;
        while (!dut.v_ready_o && guard++ < 1024) {
          mb.grant = (r.below(2) != 0);
          step(dut, mb);
        }
        mb.grant = true;
        step(dut, mb);
        dut.v_valid_i = 0;
        dut.eval();
        int cycles = 0;
        while (!dut.r_valid_o && cycles < 6000) {
          mb.grant = (r.below(2) != 0);
          step(dut, mb);
          ++cycles;
        }
        mb.grant = true;
        const std::string what = "contended group " + std::to_string(k);
        check(cycles < 6000, (what + ": reply arrived, no hang").c_str(), 1, cycles < 6000 ? 1 : 0);
        if (cycles < 6000) {
          const uint32_t got[3][kLanes] = {{dut.o0_0_o, dut.o0_1_o, dut.o0_2_o, dut.o0_3_o},
                                           {dut.o1_0_o, dut.o1_1_o, dut.o1_2_o, dut.o1_3_o},
                                           {dut.o2_0_o, dut.o2_1_o, dut.o2_2_o, dut.o2_3_o}};
          for (int l = 0; l < kLanes; ++l)
            for (int m = 0; m < 3; ++m)
              check(got[m][l] == (uint32_t)w.r[m][l],
                    (what + ": lane " + std::to_string(l) + " dst" + std::to_string(m)).c_str(),
                    (uint32_t)w.r[m][l], got[m][l]);
        }
        step(dut, mb);
        dut.eval();
      }
      printf("   MEASURED: %d groups under refusal, %d refusals issued\n", kGroups, mb.refusals);
      check(mb.refusals > 0, "the bank ACTUALLY refused -- the test is not vacuous", 1,
            mb.refusals > 0 ? 1 : 0);
    }

    printf("== section 7: the request cost, uncontended ==\n");
    {
      Group g{};
      for (int l = 0; l < kLanes; ++l) {
        g.x[l] = (int32_t)((l + 1) << 16);
        g.y[l] = (int32_t)((l + 2) << 16);
        g.z[l] = (int32_t)((l + 3) << 16);
      }
      const int c2 = run_one(dut, mb, false, g, 0x60, "N2 cost");
      const int c3 = run_one(dut, mb, true, g, 0x61, "N3 cost");
      printf("   MEASURED four-point NORMALIZE2 %d clocks, NORMALIZE3 %d clocks\n", c2, c3);
      // Dominated by FOUR sequential 32-iteration square roots, which is the
      // documented slow path rather than a surprise.
      check(c3 > c2, "N3 costs more than N2 -- it has a third square and a third output", 1,
            c3 > c2 ? 1 : 0);
      check(c3 <= 400, "and a four-point NORMALIZE3 stays under 400 clocks", 400, c3);
    }
  } else {
    printf("== random differential: %d groups against zfield::exec_op ==\n", random_n);
    for (int i = 0; i < random_n; ++i) {
      Group g{};
      for (int l = 0; l < kLanes; ++l) {
        g.x[l] = rng.comp();
        g.y[l] = rng.comp();
        g.z[l] = rng.comp();
      }
      run_one(dut, mb, (rng.below(2) != 0), g, (uint8_t)(i & 0xFF),
              "random group " + std::to_string(i));
    }
  }

  return zhao::report_and_exit("field_v3_normalize_directed");
}
