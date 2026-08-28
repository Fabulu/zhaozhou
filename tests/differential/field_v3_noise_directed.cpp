// field_v3_noise_directed.cpp — NOISE2 and RIDGE for four points at once, on
// the shared four-wide multiplier bank, against the ONE semantic layer
// (zfield::exec_op, which zfield::interpret itself calls).
//
// LAWS, and where each is exercised:
//
//   1. THE HASH IS THE REFERENCE'S, BIT FOR BIT. Every value check is
//      against zfield::exec_op with OP_NOISE2 or OP_RIDGE -- never against a
//      hash reimplemented in this file, which would only prove the two
//      reimplementations agree.
//   2. THE FOUR POINTS ARE INDEPENDENT. Section 2 drives four DIFFERENT
//      coordinates in one group and checks all four, so a unit that
//      broadcasts point 0 across the lanes fails. Section 3 drives four
//      IDENTICAL ones, so a unit that indexes the wrong lane still passes
//      there and fails in section 2 -- the pair is the point.
//   3. THE SHIFT AMOUNT IS PER POINT. Law 2 of the RTL: the RXS shift is
//      (s>>28)+4, a function of the DATA, so four points in one request can
//      each want a different shift. Section 4 hunts coordinates whose shifts
//      differ within one group and asserts it found some -- a vector unit
//      that computed one shift for the whole request would pass every other
//      section.
//   4. THE LATTICE MIX IS SHARED BETWEEN THE HASH LANES and replayed from a
//      register. NOISE2's two outputs must both be right, which is what
//      catches a replay that reuses the post-salt word instead.
//   5. PRODUCTS ARE MODULO 2^32 AND NEVER SATURATE, while the bank lane is
//      33x33 SIGNED. Section 5 drives coordinates whose lattice terms have
//      the top bit set.
//
//      IT DOES NOT TEST THE EXTENSION, though it was written believing it
//      did. Mutant N07 sign-extends an operand and survives, provably: the
//      two 33-bit forms differ by 2^32, so their products differ by a
//      multiple of 2^32 and their low 32 bits -- the only bits read -- are
//      identical. What the section DOES exercise is the shift-and-xor tail
//      on words with bit 31 set, which is worth keeping and is not what the
//      old comment claimed.
//   6. RIDGE'S FOLD AND ITS FLAGS are the reference's fx_add/fx_sub/abs_sat,
//      and RIDGE writes dst1 = 0.
//   7. THE BANK CAN REFUSE. Section 6 refuses on a pseudo-random schedule
//      and asserts refusals ACTUALLY HAPPENED. This is the section that
//      exists because the executor's DOT sequencer kept full marks for a week
//      behind a test that never refused it.
//
// The multiplier bank is the ENGINE's, not this unit's: the test models the
// four-wide lane (registered, two-cycle, one issue per clock) exactly as the
// engine prices it, and this unit's cost excludes it.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_v3_noise.h"

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
  int32_t coord() {
    // Coordinates that matter: small, huge, negative, and exactly on a
    // lattice boundary. The floor is an ARITHMETIC shift, so the sign of the
    // input is the interesting axis.
    switch (below(6)) {
      case 0: return 0;
      case 1: return (int32_t)0x00010000;   // exactly 1.0
      case 2: return -(int32_t)0x00010000;  // exactly -1.0
      case 3: return (int32_t)0x7FFFFFFF;
      case 4: return (int32_t)0x80000000;
      default: return (int32_t)next64();
    }
  }
};

// ---- the four-wide multiplier bank model (engine property) -----------------
// One four-lane issue, products back with the lane's registered two-cycle
// latency. 33x33 products fit s65; the unit reads the low 32 only, so int64
// holds everything that is read exactly.
struct MulBank {
  bool busy = false;
  int cnt = 0;
  int64_t p[kLanes] = {0, 0, 0, 0};

  // THE BANK CAN REFUSE. `grant` drives mul_ready_i; `refusals` counts how
  // often it actually said no, so a contention test can prove it contended
  // rather than passing vacuously.
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

/** One cycle: serve the bank, then clock the DUT. */
void step(Vzhao_field_v3_noise& dut, MulBank& mb) {
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
    // Refused: nothing is started, so nothing may ever arrive. The unit must
    // hold the request and ask again.
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

// ---- the oracle ------------------------------------------------------------
struct Want {
  int32_t r0[kLanes];
  int32_t r1[kLanes];
  bool sat_add[kLanes];
  bool sat_rescale[kLanes];
};

/** One point through zfield::exec_op — the shipped interpreter, not a model. */
void oracle_point(bool ridge, int32_t x, int32_t y, uint32_t seed, int32_t* r0, int32_t* r1,
                  bool* sat_add, bool* sat_rescale) {
  const int32_t src[3] = {x, y, 0};
  int32_t dst[3] = {0, 0, 0};
  const std::vector<zfield::Table> no_tables;
  zref::SatLedger L;
  zfield::steps::exec_op(ridge ? zfield::OP_RIDGE : zfield::OP_NOISE2, seed, no_tables, src, dst,
                         &L);
  *r0 = dst[0];
  *r1 = ridge ? 0 : dst[1];
  // The ledger COUNTS clamps; this unit reports one bit per point per lane, so
  // the comparison is "did any clamp land in that lane" -- which is what the
  // RTL's sat_add_o/sat_rescale_o mean. Reset per point, so the counts cannot
  // accumulate across the four.
  *sat_add = (L.add != 0);
  *sat_rescale = (L.rescale != 0);
}

Want oracle(bool ridge, const int32_t* x, const int32_t* y, uint32_t seed) {
  Want w{};
  for (int l = 0; l < kLanes; ++l)
    oracle_point(ridge, x[l], y[l], seed, &w.r0[l], &w.r1[l], &w.sat_add[l], &w.sat_rescale[l]);
  return w;
}

// ---- driving ---------------------------------------------------------------
void drive(Vzhao_field_v3_noise& dut, bool ridge, const int32_t* x, const int32_t* y,
           uint32_t seed, uint8_t tag) {
  dut.v_valid_i = 1;
  dut.is_ridge_i = ridge ? 1 : 0;
  dut.a0_0_i = (uint32_t)x[0];
  dut.a0_1_i = (uint32_t)x[1];
  dut.a0_2_i = (uint32_t)x[2];
  dut.a0_3_i = (uint32_t)x[3];
  dut.a1_0_i = (uint32_t)y[0];
  dut.a1_1_i = (uint32_t)y[1];
  dut.a1_2_i = (uint32_t)y[2];
  dut.a1_3_i = (uint32_t)y[3];
  dut.seed_i = seed;
  dut.tag_i = tag;
}

void check_rsp(Vzhao_field_v3_noise& dut, const Want& w, uint8_t tag, const std::string& what) {
  const uint32_t got0[kLanes] = {dut.o0_0_o, dut.o0_1_o, dut.o0_2_o, dut.o0_3_o};
  const uint32_t got1[kLanes] = {dut.o1_0_o, dut.o1_1_o, dut.o1_2_o, dut.o1_3_o};
  for (int l = 0; l < kLanes; ++l) {
    check(got0[l] == (uint32_t)w.r0[l], (what + ": lane " + std::to_string(l) + " dst0").c_str(),
          (uint32_t)w.r0[l], got0[l]);
    check(got1[l] == (uint32_t)w.r1[l], (what + ": lane " + std::to_string(l) + " dst1").c_str(),
          (uint32_t)w.r1[l], got1[l]);
    check(((dut.sat_add_o >> l) & 1) == (w.sat_add[l] ? 1u : 0u),
          (what + ": lane " + std::to_string(l) + " add flag").c_str(), w.sat_add[l] ? 1 : 0,
          (dut.sat_add_o >> l) & 1);
    check(((dut.sat_rescale_o >> l) & 1) == (w.sat_rescale[l] ? 1u : 0u),
          (what + ": lane " + std::to_string(l) + " rescale flag").c_str(),
          w.sat_rescale[l] ? 1 : 0, (dut.sat_rescale_o >> l) & 1);
  }
  check(dut.tag_o == tag, (what + ": tag").c_str(), tag, dut.tag_o);
}

/** One group through an idle unit; returns the reply latency in clocks. */
int run_one(Vzhao_field_v3_noise& dut, MulBank& mb, bool ridge, const int32_t* x,
            const int32_t* y, uint32_t seed, uint8_t tag, const std::string& what) {
  const Want w = oracle(ridge, x, y, seed);
  drive(dut, ridge, x, y, seed, tag);
  dut.r_ready_i = 1;
  dut.eval();
  int guard = 0;
  while (!dut.v_ready_o && guard++ < 256) step(dut, mb);
  step(dut, mb);  // accepted
  dut.v_valid_i = 0;
  dut.eval();
  int cycles = 0;
  while (!dut.r_valid_o && cycles < 512) {
    step(dut, mb);
    ++cycles;
  }
  check(cycles < 512, (what + ": reply arrived").c_str(), 1, cycles < 512 ? 1 : 0);
  check_rsp(dut, w, tag, what);
  step(dut, mb);  // reply taken
  dut.eval();
  return cycles;
}

/** The RXS shift the reference would use for one point, for law 3. */
unsigned rxs_shift_of(int32_t x, int32_t y, uint32_t seed) {
  const uint32_t ix = (uint32_t)(x >> 16);
  const uint32_t iy = (uint32_t)(y >> 16);
  uint32_t s = (ix * 0x9E3779B1u) ^ ((iy * 0x85EBCA77u) ^ seed);
  s = s * 747796405u + 2891336453u;
  return (unsigned)((s >> 28) + 4);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int random_n = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--random") && i + 1 < argc) random_n = atoi(argv[i + 1]);
  }

  Vzhao_field_v3_noise dut;
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

  Prng rng(random_n ? 0x1D0FFu + (uint32_t)random_n : 0x1D0FFu);

  if (random_n == 0) {
    printf("== section 1: directed points, NOISE2 and RIDGE ==\n");
    {
      // Zero, one, minus one and the two rails, in one group each.
      const int32_t xs[kLanes] = {0, (int32_t)0x00010000, -(int32_t)0x00010000,
                                  (int32_t)0x7FFFFFFF};
      const int32_t ys[kLanes] = {(int32_t)0x80000000, 0, (int32_t)0x00020000,
                                  -(int32_t)0x00030000};
      run_one(dut, mb, false, xs, ys, 0u, 0x01, "NOISE2 rails, seed 0");
      run_one(dut, mb, true, xs, ys, 0u, 0x02, "RIDGE rails, seed 0");
      run_one(dut, mb, false, xs, ys, 0xB00B1E5u, 0x03, "NOISE2 rails, seeded");
      run_one(dut, mb, true, xs, ys, 0xB00B1E5u, 0x04, "RIDGE rails, seeded");
    }

    printf("== section 2: four DIFFERENT points in one group ==\n");
    {
      // Law 2: a unit that broadcasts point 0 across the lanes fails here.
      for (int g = 0; g < 8; ++g) {
        int32_t x[kLanes], y[kLanes];
        for (int l = 0; l < kLanes; ++l) {
          x[l] = rng.coord();
          y[l] = rng.coord();
        }
        run_one(dut, mb, (g & 1) != 0, x, y, 0x5EEDu + (uint32_t)g, (uint8_t)(0x10 + g),
                "distinct group " + std::to_string(g));
      }
    }

    printf("== section 3: four IDENTICAL points in one group ==\n");
    {
      // The other half of law 2: this passes even with the lanes crossed, and
      // says so. It is here to make section 2's failure specific.
      for (int g = 0; g < 4; ++g) {
        const int32_t c = rng.coord(), d = rng.coord();
        const int32_t x[kLanes] = {c, c, c, c};
        const int32_t y[kLanes] = {d, d, d, d};
        run_one(dut, mb, (g & 1) != 0, x, y, 0xA11Ceu, (uint8_t)(0x20 + g),
                "identical group " + std::to_string(g));
      }
    }

    printf("== section 4: the RXS shift differs WITHIN one group ==\n");
    {
      // Law 3: the shift is (s>>28)+4, a function of the data, so it is a
      // PER-POINT quantity. A unit that computed one shift for the whole
      // four-wide request would pass every other section in this file.
      int groups = 0, spread_seen = 0;
      Prng sr(0x5417u);
      for (int attempt = 0; attempt < 400 && groups < 8; ++attempt) {
        int32_t x[kLanes], y[kLanes];
        unsigned sh[kLanes];
        for (int l = 0; l < kLanes; ++l) {
          x[l] = (int32_t)sr.next64();
          y[l] = (int32_t)sr.next64();
          sh[l] = rxs_shift_of(x[l], y[l], 0x3141u);
        }
        unsigned lo = sh[0], hi = sh[0];
        for (int l = 1; l < kLanes; ++l) {
          if (sh[l] < lo) lo = sh[l];
          if (sh[l] > hi) hi = sh[l];
        }
        if (hi == lo) continue;  // no spread: not the case under test
        if (hi - lo >= 4) ++spread_seen;
        run_one(dut, mb, false, x, y, 0x3141u, (uint8_t)(0x30 + groups),
                "shift spread " + std::to_string(lo) + ".." + std::to_string(hi));
        ++groups;
      }
      printf("   MEASURED: %d groups with a per-point shift spread, %d of them wide\n", groups,
             spread_seen);
      check(groups == 8, "eight groups with differing shifts were found", 8, groups);
      check(spread_seen > 0, "at least one group spans four or more shift values", 1,
            spread_seen > 0 ? 1 : 0);
    }

    printf("== section 5: lattice terms with the top bit set ==\n");
    {
      // Law 5, corrected by mutant N07: a sign-extended operand does NOT
      // diverge here, or anywhere, while only the low 32 bits are read. This
      // section exercises the shift-and-xor tail on words with bit 31 set,
      // which is a real path and not the one it was named for.
      // THE BUDGET IS A 1-IN-256 DRAW, and 400 attempts found two groups.
      // Requiring all EIGHT lattice terms of a group to have bit 31 set is
      // deliberate -- it puts every lane on the divergent side at once -- but
      // that is (1/2)^8 per attempt, so the search has to be sized for it.
      // The attempts are pure arithmetic; only the eight accepted groups cost
      // DUT time.
      int groups = 0;
      Prng sr(0x80B17u);
      for (int attempt = 0; attempt < 40000 && groups < 8; ++attempt) {
        int32_t x[kLanes], y[kLanes];
        bool all_top = true;
        for (int l = 0; l < kLanes; ++l) {
          x[l] = (int32_t)sr.next64();
          y[l] = (int32_t)sr.next64();
          const uint32_t ix = (uint32_t)(x[l] >> 16);
          const uint32_t iy = (uint32_t)(y[l] >> 16);
          if (((ix * 0x9E3779B1u) & 0x80000000u) == 0) all_top = false;
          if (((iy * 0x85EBCA77u) & 0x80000000u) == 0) all_top = false;
        }
        if (!all_top) continue;
        run_one(dut, mb, (groups & 1) != 0, x, y, 0xFFFFFFFFu, (uint8_t)(0x40 + groups),
                "top-bit lattice " + std::to_string(groups));
        ++groups;
      }
      printf("   MEASURED: %d groups where every lattice term has bit 31 set\n", groups);
      check(groups == 8, "eight all-top-bit groups were found", 8, groups);
    }

    printf("== section 6: the bank refuses, and the answers do not move ==\n");
    {
      // THE UNIT COULD NOT BE ATTACHED WITHOUT THIS. The bank is shared and
      // can say no. Advancing out of an issue state on a refusal leaves the
      // wait state below it waiting for a product nobody started.
      Prng crng(0xC0FFEEu);
      mb.refusals = 0;
      const int kGroups = 24;
      int ran = 0;
      for (int g = 0; g < kGroups; ++g) {
        int32_t x[kLanes], y[kLanes];
        for (int l = 0; l < kLanes; ++l) {
          x[l] = crng.coord();
          y[l] = crng.coord();
        }
        const bool ridge = (crng.below(3) == 0);
        const uint32_t seed = (uint32_t)crng.next64();
        const uint8_t tag = (uint8_t)(0x80 + g);
        const Want w = oracle(ridge, x, y, seed);

        drive(dut, ridge, x, y, seed, tag);
        dut.r_ready_i = 1;
        dut.eval();
        int guard = 0;
        while (!dut.v_ready_o && guard++ < 512) {
          mb.grant = (crng.below(2) != 0);
          step(dut, mb);
        }
        mb.grant = true;
        step(dut, mb);
        dut.v_valid_i = 0;
        dut.eval();

        int cycles = 0;
        while (!dut.r_valid_o && cycles < 1024) {
          mb.grant = (crng.below(2) != 0);
          step(dut, mb);
          ++cycles;
        }
        mb.grant = true;
        const std::string what = "contended group " + std::to_string(g);
        check(cycles < 1024, (what + ": reply arrived, no hang").c_str(), 1,
              cycles < 1024 ? 1 : 0);
        check_rsp(dut, w, tag, what);
        step(dut, mb);
        dut.eval();
        ++ran;
      }
      printf("   MEASURED: %d groups under refusal, %d refusals issued\n", ran, mb.refusals);
      check(mb.refusals > 0, "the bank ACTUALLY refused -- the test is not vacuous", 1,
            mb.refusals > 0 ? 1 : 0);
    }

    printf("== section 7: the request cost, uncontended ==\n");
    {
      const int32_t x[kLanes] = {1, 2, 3, 4};
      const int32_t y[kLanes] = {5, 6, 7, 8};
      const int n2 = run_one(dut, mb, false, x, y, 0x1234u, 0x60, "NOISE2 cost");
      const int rg = run_one(dut, mb, true, x, y, 0x1234u, 0x61, "RIDGE cost");
      printf("   MEASURED four-point NOISE2 %d clocks, RIDGE %d clocks\n", n2, rg);
      // Six bank requests for NOISE2, four for RIDGE, each two clocks deep,
      // plus the walk between them. The bound is generous on purpose: what is
      // being asserted is the SHAPE (RIDGE strictly cheaper, both far under a
      // scalar unit run four times), not a schedule nobody has pinned yet.
      check(rg < n2, "RIDGE is cheaper than NOISE2 -- it stops after four products", 1,
            rg < n2 ? 1 : 0);
      check(n2 <= 32, "a four-point NOISE2 costs at most 32 clocks", 32, n2);
    }
  } else {
    printf("== random differential: %d groups against zfield::exec_op ==\n", random_n);
    for (int i = 0; i < random_n; ++i) {
      int32_t x[kLanes], y[kLanes];
      for (int l = 0; l < kLanes; ++l) {
        x[l] = rng.coord();
        y[l] = rng.coord();
      }
      const bool ridge = (rng.below(3) == 0);
      run_one(dut, mb, ridge, x, y, (uint32_t)rng.next64(), (uint8_t)(i & 0xFF),
              "random group " + std::to_string(i));
    }
  }

  return zhao::report_and_exit("field_v3_noise_directed");
}
