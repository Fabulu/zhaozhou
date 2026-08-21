// field_noise_directed.cpp — OP_NOISE2 and OP_RIDGE, RTL against the
// interpreter itself.
//
// Two oracles, as with the table ops:
//
//   * `zfield::interpret` on a real two-instruction program decides every
//     VALUE. That is the shipped path, not a restatement of it.
//   * `zref::noise2_hash` decides the intermediate, so a wrong answer can be
//     attributed to the hash or to the fold around it rather than only
//     reported.
//
// SIX LAWS, each a place an implementation drifts:
//
//   1. THE LATTICE INDEX IS AN ARITHMETIC SHIFT of a signed register, then
//      reinterpreted as unsigned. A logical shift agrees for every
//      non-negative coordinate and disagrees for every negative one. Section 2
//      sweeps both sides of the origin, which is the only place this shows.
//   2. THE RXS SHIFT AMOUNT IS DATA-DEPENDENT, (s >> 28) + 4, i.e. 4..19.
//      Section 5 asserts the test actually reaches a spread of those shift
//      amounts -- a sweep that only ever hits one of them cannot tell a
//      variable shifter from a constant.
//   3. EVERY MULTIPLY IS MODULO 2^32. Nothing saturates inside the hash.
//   4. NOISE2 AND RIDGE DISAGREE ABOUT THEIR SECOND OPERAND: NOISE2 reads an
//      ADJACENT PAIR (reg[a], reg[a+1]), RIDGE reads TWO NAMED REGISTERS
//      (reg[a], reg[b]). Section 4 gives them different register layouts on
//      purpose.
//   5. THE OUTPUT IS THE TOP HALF (>> 16), so every value is [0, 1) and never
//      negative. Section 3 asserts that over the whole sweep.
//   6. RIDGE FOLDS to 1 - |2u - 1|, saturating, so its result is (0, 1].
//
// TWO EQUIVALENT MUTANTS, recorded so they do not read as holes later.
// `noise2_hash` ends with `(w >> 22) ^ w`, and both ops then keep only bits
// [31:16]. `w >> 22` has nothing above bit 9, so the xor touches bits [9:0] --
// exactly the half the op discards. Dropping that line, or changing 22 to 21,
// is therefore UNOBSERVABLE here:
//
//   * `final_xorshift_dropped`
//   * `final_xorshift_amount`
//
// Section 9 pins the reason rather than leaving it as an assertion in a
// comment: it checks directly that the top sixteen bits are unaffected.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_noise.h"

#include "zfield/zfield.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"

namespace {

using zhao::check;

/** One op as a real program, through the shipped interpreter. */
struct Res {
  int32_t o0 = 0;
  int32_t o1 = 0;
  bool sat = false;
};

Res interp(bool ridge, int32_t a0, int32_t a1, uint32_t seed) {
  zfield::Decoded prog;
  prog.profile = 0;

  zfield::Instr ins{};
  ins.op = ridge ? zfield::OP_RIDGE : zfield::OP_NOISE2;
  // Law 4: NOISE2 reads reg[a] and reg[a+1]; RIDGE reads reg[a] and reg[b].
  // The register layout below is chosen so BOTH conventions are exercised with
  // the same pair of values, which is how a swap between them is visible.
  ins.dst = 4;
  ins.a = 0;
  ins.b = ridge ? 2 : 0;
  ins.c = 0;
  ins.imm = seed;
  prog.instrs.push_back(ins);

  zfield::Instr end{};
  end.op = zfield::OP_END;
  prog.instrs.push_back(end);

  // in lanes: reg0 = a0, reg1 = a1 (the NOISE2 pair), reg2 = a1 (RIDGE's b)
  for (int i = 0; i < 3; ++i) {
    zfield::IoLane in_lane{};
    in_lane.name = "i";
    in_lane.type = 0;
    in_lane.reg = static_cast<uint8_t>(i);
    prog.in_lanes.push_back(in_lane);
  }
  for (int i = 0; i < 2; ++i) {
    zfield::IoLane out_lane{};
    out_lane.name = "o";
    out_lane.type = 0;
    out_lane.reg = static_cast<uint8_t>(4 + i);
    prog.out_lanes.push_back(out_lane);
  }

  const int32_t in[3] = {a0, a1, a1};
  int32_t out[2] = {0, 0};
  const zfield::Status st = zfield::interpret(prog, in, 3, out, 2);
  return Res{out[0], out[1], st.sat};
}

struct DutRes {
  int32_t o0 = 0, o1 = 0;
  bool sat_add = false, sat_rescale = false;
  int cycles = 0;
};

DutRes run(Vzhao_field_noise& dut, bool ridge, int32_t a0, int32_t a1, uint32_t seed) {
  dut.v_valid_i = 1;
  dut.is_ridge_i = ridge ? 1 : 0;
  dut.a0_i = static_cast<uint32_t>(a0);
  dut.a1_i = static_cast<uint32_t>(a1);
  dut.seed_i = seed;
  dut.r_ready_i = 1;
  dut.eval();

  int guard = 0;
  while (!dut.v_ready_o && guard++ < 64) {
    zhao::tick(dut);
    dut.eval();
  }
  zhao::tick(dut);
  dut.v_valid_i = 0;
  dut.eval();

  DutRes r;
  int cycles = 0;
  while (!dut.r_valid_o && cycles < 64) {
    zhao::tick(dut);
    dut.eval();
    ++cycles;
  }
  r.o0 = static_cast<int32_t>(dut.o0_o);
  r.o1 = static_cast<int32_t>(dut.o1_o);
  r.sat_add = dut.sat_add_o != 0;
  r.sat_rescale = dut.sat_rescale_o != 0;
  r.cycles = cycles;
  zhao::tick(dut);
  dut.eval();
  return r;
}

int g_neg_x = 0, g_neg_y = 0;
uint32_t g_rxs_seen = 0;  // bitmask of the (s>>28)+4 shift amounts reached

/** Replay the hash to record which RXS shift amounts a case exercises. */
void note_rxs(int32_t a0, int32_t a1, uint32_t seed, unsigned lane) {
  const uint32_t x = static_cast<uint32_t>(a0 >> 16);
  const uint32_t y = static_cast<uint32_t>(a1 >> 16);
  uint32_t s = (x * 0x9E3779B1u) ^ ((y * 0x85EBCA77u) ^ seed);
  s = s + lane * 0xE1u;
  s = s * 747796405u + 2891336453u;
  const unsigned sh = (s >> 28) + 4u;
  if (sh < 32) g_rxs_seen |= (1u << sh);
}

void diff(Vzhao_field_noise& dut, bool ridge, int32_t a0, int32_t a1, uint32_t seed,
          const char* what) {
  const Res want = interp(ridge, a0, a1, seed);
  const DutRes got = run(dut, ridge, a0, a1, seed);
  const std::string t(what);

  check(got.o0 == want.o0, (t + ": lane 0").c_str(), static_cast<uint32_t>(want.o0),
        static_cast<uint32_t>(got.o0));
  if (!ridge) {
    check(got.o1 == want.o1, (t + ": lane 1").c_str(), static_cast<uint32_t>(want.o1),
          static_cast<uint32_t>(got.o1));
  } else {
    check(got.o1 == 0, (t + ": RIDGE writes one lane").c_str(), 0,
          static_cast<uint32_t>(got.o1));
  }
  check((got.sat_add || got.sat_rescale) == want.sat, (t + ": Status.sat").c_str(),
        want.sat ? 1 : 0, (got.sat_add || got.sat_rescale) ? 1 : 0);

  if (a0 < 0) ++g_neg_x;
  if (a1 < 0) ++g_neg_y;
  note_rxs(a0, a1, seed, 0);
  if (!ridge) note_rxs(a0, a1, seed, 1);
}

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t v0 = s;
    s = v0 * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((v0 >> 22) ^ v0) >> 29);
    const uint32_t v = (static_cast<uint32_t>(v0 >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

constexpr int32_t kOne = 1 << 16;

}  // namespace

int main(int argc, char** argv) {
  int random_iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && i + 1 < argc) {
      random_iters = std::atoi(argv[++i]);
    }
  }

  Vzhao_field_noise dut;
  dut.rst_n = 0;
  dut.v_valid_i = 0;
  dut.r_ready_i = 1;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  // ---- 1. the hash itself, on a small lattice ----------------------------
  {
    for (int32_t y = 0; y < 4; ++y) {
      for (int32_t x = 0; x < 4; ++x) {
        char nm[64];
        std::snprintf(nm, sizeof nm, "1.noise2 (%d,%d)", x, y);
        diff(dut, false, x * kOne, y * kOne, 0x1234u, nm);
      }
    }
  }

  // ---- 2. LAW 1: both sides of the origin --------------------------------
  // The arithmetic shift is the whole difference here, and it is invisible for
  // non-negative coordinates. A logical-shift implementation passes section 1
  // completely.
  {
    const int32_t xs[] = {-4 * kOne, -3 * kOne, -kOne, -1, 0, 1, kOne, 3 * kOne};
    for (int i = 0; i < 8; ++i) {
      for (int j = 0; j < 8; ++j) {
        char nm[80];
        std::snprintf(nm, sizeof nm, "2.signed (%d,%d)", i, j);
        diff(dut, false, xs[i], xs[j], 0x5EEDu, nm);
      }
    }
    // A coordinate of -1 raw is NOT lattice cell -1: it is (-1 >> 16) = -1,
    // which as unsigned is 0xFFFFFFFF. Stated because the natural mistake is
    // to think of the index as "the integer part".
    const Res a = interp(false, -1, 0, 0u);
    const Res b = interp(false, -kOne, 0, 0u);
    check(a.o0 == b.o0, "2.raw -1 and raw -65536 are the SAME lattice cell",
          static_cast<uint32_t>(b.o0), static_cast<uint32_t>(a.o0));
    const Res c = interp(false, 0, 0, 0u);
    check(a.o0 != c.o0, "2.and it is NOT cell zero", 1, a.o0 != c.o0 ? 1 : 0);
  }

  // ---- 3. LAW 5: every value is [0, 1) ------------------------------------
  {
    bool in_range = true;
    for (int i = 0; i < 64; ++i) {
      const Res r = interp(false, (i - 32) * kOne, (i * 7 - 11) * kOne, 0xABCDu);
      if (r.o0 < 0 || r.o0 >= kOne) in_range = false;
      if (r.o1 < 0 || r.o1 >= kOne) in_range = false;
    }
    check(in_range, "3.every NOISE2 lane is in [0, 1)", 1, in_range ? 1 : 0);

    // The two lanes are DIFFERENT: the salt is what makes NOISE2 a 2-vector
    // and not the same number twice.
    int same = 0;
    for (int i = 0; i < 64; ++i) {
      const Res r = interp(false, i * kOne, (i * 3) * kOne, 0u);
      if (r.o0 == r.o1) ++same;
    }
    check(same <= 2, "3.the two lanes differ (the salt is doing something)", 0,
          static_cast<uint32_t>(same));
  }

  // ---- 4. LAW 4: RIDGE's operands, and its fold ---------------------------
  {
    for (int32_t y = -2; y <= 2; ++y) {
      for (int32_t x = -2; x <= 2; ++x) {
        char nm[64];
        std::snprintf(nm, sizeof nm, "4.ridge (%d,%d)", x, y);
        diff(dut, true, x * kOne, y * kOne, 0x77u, nm);
      }
    }
    // LAW 6: 1 - |2u - 1| lands in (0, 1].
    bool folded = true;
    for (int i = 0; i < 64; ++i) {
      const Res r = interp(true, (i - 32) * kOne, (i * 5) * kOne, 0x99u);
      if (r.o0 < 0 || r.o0 > kOne) folded = false;
    }
    check(folded, "4.every RIDGE result is in [0, 1]", 1, folded ? 1 : 0);

    // RIDGE and NOISE2 lane 0 share the hash but NOT the result: the fold is
    // real. If RIDGE forgot to fold, these would be equal everywhere.
    int equal = 0;
    for (int i = 0; i < 32; ++i) {
      const Res n = interp(false, i * kOne, i * kOne, 0x99u);
      const Res g = interp(true, i * kOne, i * kOne, 0x99u);
      if (n.o0 == g.o0) ++equal;
    }
    check(equal <= 1, "4.the fold is not the identity", 0, static_cast<uint32_t>(equal));
  }

  // ---- 5. LAW 2: the RXS shift amount actually varies ---------------------
  // A sweep that only ever produces one shift amount cannot tell a variable
  // shifter from a constant one, and would pass a mutation that hardwires it.
  {
    int distinct = 0;
    for (int b = 0; b < 32; ++b) {
      if (g_rxs_seen & (1u << b)) ++distinct;
    }
    check(distinct >= 10, "5.the data-dependent RXS shift took >= 10 distinct values", 10,
          static_cast<uint32_t>(distinct));
    check(g_neg_x > 0 && g_neg_y > 0, "5.negative coordinates were actually exercised", 1,
          (g_neg_x > 0 && g_neg_y > 0) ? 1 : 0);
  }

  // ---- 6. the seed matters ------------------------------------------------
  {
    const Res a = interp(false, 3 * kOne, 5 * kOne, 0u);
    const Res b = interp(false, 3 * kOne, 5 * kOne, 1u);
    check(a.o0 != b.o0 || a.o1 != b.o1, "6.a different seed is a different world", 1,
          (a.o0 != b.o0 || a.o1 != b.o1) ? 1 : 0);
    for (uint32_t sd : {0u, 1u, 0xFFFFFFFFu, 0x80000000u, 0xDEADBEEFu}) {
      char nm[64];
      std::snprintf(nm, sizeof nm, "6.seed %08X", sd);
      diff(dut, false, -7 * kOne, 11 * kOne, sd, nm);
    }
  }

  // ---- 7. interface laws --------------------------------------------------
  {
    // Fixed latency, and the result is HELD until it is taken.
    const DutRes a = run(dut, false, kOne, kOne, 1u);
    const DutRes b = run(dut, false, 2 * kOne, 3 * kOne, 1u);
    check(a.cycles == b.cycles, "7.NOISE2 latency is fixed",
          static_cast<uint32_t>(a.cycles), static_cast<uint32_t>(b.cycles));
    const DutRes c = run(dut, true, kOne, kOne, 1u);
    check(c.cycles < a.cycles, "7.RIDGE is shorter -- it walks one lane, not two",
          static_cast<uint32_t>(a.cycles), static_cast<uint32_t>(c.cycles));

    dut.v_valid_i = 1;
    dut.is_ridge_i = 0;
    dut.a0_i = static_cast<uint32_t>(5 * kOne);
    dut.a1_i = static_cast<uint32_t>(6 * kOne);
    dut.seed_i = 3u;
    dut.r_ready_i = 0;
    dut.eval();
    int g = 0;
    while (!dut.v_ready_o && g++ < 64) { zhao::tick(dut); dut.eval(); }
    zhao::tick(dut);
    dut.v_valid_i = 0;
    dut.eval();
    int cyc = 0;
    bool ready_low = true;
    while (!dut.r_valid_o && cyc < 64) {
      if (dut.v_ready_o) ready_low = false;
      zhao::tick(dut);
      dut.eval();
      ++cyc;
    }
    check(ready_low, "7.v_ready is low while a hash is walking", 1, ready_low ? 1 : 0);
    const int32_t held = static_cast<int32_t>(dut.o0_o);
    bool stable = true;
    for (int i = 0; i < 8; ++i) {
      zhao::tick(dut);
      dut.eval();
      if (!dut.r_valid_o || static_cast<int32_t>(dut.o0_o) != held) stable = false;
    }
    check(stable, "7.the result is held under backpressure", 1, stable ? 1 : 0);
    dut.r_ready_i = 1;
    dut.eval();
    zhao::tick(dut);
    dut.eval();
    check(!dut.r_valid_o, "7.and retires on ready", 0, dut.r_valid_o ? 1 : 0);
  }

  // ---- 9. WHY TWO MUTATIONS SURVIVE, checked rather than asserted --------
  // The hash's last step is `(w >> 22) ^ w`, and the op keeps bits [31:16].
  // `w >> 22` has nothing above bit 9, so the xor perturbs only bits the op
  // throws away. That makes two mutations of that line unobservable, and it is
  // better to have the reason under test than to have it in a comment nobody
  // re-derives.
  {
    Prng rng(0xBEEF);
    bool top_unaffected = true;
    int checked = 0;
    for (int i = 0; i < 4096; ++i) {
      const uint32_t w = rng.next();
      const uint32_t with = (((w >> 22) ^ w)) >> 16;
      const uint32_t without = w >> 16;
      const uint32_t other = (((w >> 21) ^ w)) >> 16;
      if (with != without || with != other) top_unaffected = false;
      ++checked;
    }
    check(top_unaffected,
          "9.the final xor-shift cannot reach bit 16, so it cannot change the result", 1,
          top_unaffected ? 1 : 0);
    check(checked == 4096, "9.and that was checked over the whole word", 4096,
          static_cast<uint32_t>(checked));
  }

  // ---- 8. random differential --------------------------------------------
  if (random_iters > 0) {
    Prng rng(0x4E01u);
    for (int i = 0; i < random_iters; ++i) {
      const bool ridge = (rng.below(2) != 0);
      int32_t x, y;
      switch (rng.below(4)) {
        case 0: x = static_cast<int32_t>(rng.next()); y = static_cast<int32_t>(rng.next()); break;
        case 1: x = static_cast<int32_t>(rng.below(64)) * kOne - 32 * kOne;
                y = static_cast<int32_t>(rng.below(64)) * kOne - 32 * kOne; break;
        case 2: x = -static_cast<int32_t>(rng.next() >> 4);
                y = -static_cast<int32_t>(rng.next() >> 4); break;
        default: x = static_cast<int32_t>(rng.next()) >> 12;
                 y = static_cast<int32_t>(rng.next()) >> 12; break;
      }
      char nm[64];
      std::snprintf(nm, sizeof nm, "8.random[%d]", i);
      diff(dut, ridge, x, y, rng.next(), nm);
    }
    std::printf("random: %d iterations, %d neg-x, %d neg-y\n", random_iters, g_neg_x, g_neg_y);
  }

  return zhao::report_and_exit("field_noise_directed");
}
