// field_ring_directed.cpp — OP_RING, RTL against the interpreter.
//
// The deepest op in the engine: nine products, two reciprocals, two
// smoothsteps and five ledger lanes. Two oracles, as elsewhere:
//
//   * `zfield::interpret` on a real two-instruction program decides the VALUE;
//   * a restatement built on `zref::smoothstep` and `zref::field_rcp` decides
//     the per-lane saturation attribution, because `Status.sat` collapses all
//     five lanes into one bit.
//
// SIX LAWS, each a place an implementation drifts:
//
//   1. THE MIDPOINT IS AN EXACT 64-BIT AVERAGE, not `fx_add` then halve. With
//      r0 = r1 = INT32_MAX the exact midpoint is INT32_MAX; a saturating add
//      would give half of it and put the ring in the wrong place. Section 5
//      sits on that case.
//   2. THE RECIPROCAL IS field_rcp INCLUDING ITS ZERO: r0 == r1 makes both
//      spans zero, and field_rcp(0) is the pinned 0x7FFFFFFF with a sticky
//      rcp0. Section 4 checks the degenerate ring has a DEFINED answer.
//   3. THE CLAMP IS AFTER THE MULTIPLY. Clamping the input to the span first
//      is the natural reading and differs wherever the reciprocal is inexact.
//   4. EACH fx_mul ROUNDS SEPARATELY.
//   5. THE FALLING HALF IS 1 - s1, a FORWARD smoothstep subtracted from one,
//      not a smoothstep with its edges swapped.
//   6. FIVE LEDGER LANES, kept apart.
//
// ONE EQUIVALENT MUTANT, recorded so it does not read as a hole. Moving the
// midpoint's saturation from the `rescale` lane to `add` survives, because the
// midpoint CANNOT saturate: the exact sum of two s32 values lies in
// [-2^32, 2^32 - 2], and halving with round-half-up lands in
// [INT32_MIN, INT32_MAX] for every input. `sat_rescale_o` is therefore always
// low for RING and the assignment is dead. The line stays because the reference
// records the lane there and this block is its differential; section 6 asserts
// the lane is low rather than leaving it unexamined.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "verilated.h"

#include "Vzhao_field_ring.h"

#include "zfield/zfield.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_rcp.hpp"
#include "zref/zref_trig.hpp"

namespace {

using zhao::check;

constexpr int32_t kOne = 1 << 16;

struct Res {
  int32_t v = 0;
  bool sat = false;
  // `rcp0` is NOT part of `sat`. `interpret` returns
  // Status{ add||mul||rescale||unit||rcp, rcp0 } -- a divide-by-zero is
  // REPORTED but is not saturation, and it has its own field for exactly that
  // reason. Folding it into the collapsed bit made 55 of 21,404 random checks
  // fail on degenerate rings while every value and every per-lane flag agreed.
  bool rcp0 = false;
};

Res interp(int32_t d, int32_t r0, int32_t r1) {
  zfield::Decoded prog;
  prog.profile = 0;

  zfield::Instr ins{};
  ins.op = zfield::OP_RING;
  ins.dst = 4;
  ins.a = 0;
  ins.b = 1;
  ins.c = 2;
  ins.imm = 0;
  prog.instrs.push_back(ins);

  zfield::Instr end{};
  end.op = zfield::OP_END;
  prog.instrs.push_back(end);

  for (int i = 0; i < 3; ++i) {
    zfield::IoLane l{};
    l.name = "i";
    l.type = 0;
    l.reg = static_cast<uint8_t>(i);
    prog.in_lanes.push_back(l);
  }
  zfield::IoLane o{};
  o.name = "o";
  o.type = 0;
  o.reg = 4;
  prog.out_lanes.push_back(o);

  const int32_t in[3] = {d, r0, r1};
  int32_t out[1] = {0};
  const zfield::Status st = zfield::interpret(prog, in, 3, out, 1);
  return Res{out[0], st.sat, st.rcp0};
}

/** The per-lane attribution, restated on top of the shipped primitives. */
struct Lanes {
  bool add = false, mul = false, rescale = false, rcp = false, rcp0 = false;
  int32_t mid = 0;
  int32_t s0 = 0, s1 = 0;
};

Lanes lanes_of(int32_t d, int32_t r0, int32_t r1) {
  zref::SatLedger L{};
  const int32_t m = zref::rescale_s32(static_cast<int64_t>(r0) + r1, 1, &L);
  const int32_t s0 = zref::smoothstep(zref::fx16{r0}, zref::fx16{m}, zref::fx16{d}, &L).raw;
  const int32_t s1 = zref::smoothstep(zref::fx16{m}, zref::fx16{r1}, zref::fx16{d}, &L).raw;
  (void)zref::fx_mul(zref::fx16{s0}, zref::fx_sub(zref::fx16{kOne}, zref::fx16{s1}, &L), &L);
  Lanes o;
  o.add = L.add != 0;
  o.mul = L.mul != 0;
  o.rescale = L.rescale != 0;
  o.rcp = L.rcp != 0;
  o.rcp0 = L.rcp0 != 0;
  o.mid = m;
  o.s0 = s0;
  o.s1 = s1;
  return o;
}

struct DutRes {
  int32_t v = 0;
  bool add = false, mul = false, rescale = false, rcp = false, rcp0 = false;
  int cycles = 0;
};

DutRes run(Vzhao_field_ring& dut, int32_t d, int32_t r0, int32_t r1) {
  dut.v_valid_i = 1;
  dut.d_i = static_cast<uint32_t>(d);
  dut.r0_i = static_cast<uint32_t>(r0);
  dut.r1_i = static_cast<uint32_t>(r1);
  dut.r_ready_i = 1;
  dut.eval();

  int guard = 0;
  while (!dut.v_ready_o && guard++ < 128) {
    zhao::tick(dut);
    dut.eval();
  }
  zhao::tick(dut);
  dut.v_valid_i = 0;
  dut.eval();

  DutRes r;
  int cycles = 0;
  while (!dut.r_valid_o && cycles < 128) {
    zhao::tick(dut);
    dut.eval();
    ++cycles;
  }
  r.v = static_cast<int32_t>(dut.result_o);
  r.add = dut.sat_add_o != 0;
  r.mul = dut.sat_mul_o != 0;
  r.rescale = dut.sat_rescale_o != 0;
  r.rcp = dut.sat_rcp_o != 0;
  r.rcp0 = dut.rcp0_o != 0;
  r.cycles = cycles;
  zhao::tick(dut);
  dut.eval();
  return r;
}

int g_rcp0_seen = 0, g_rcp_seen = 0, g_clamped_lo = 0, g_clamped_hi = 0;

void diff(Vzhao_field_ring& dut, int32_t d, int32_t r0, int32_t r1, const char* what) {
  const Res want = interp(d, r0, r1);
  const Lanes lane = lanes_of(d, r0, r1);
  const DutRes got = run(dut, d, r0, r1);
  const std::string t(what);

  check(got.v == want.v, (t + ": value").c_str(), static_cast<uint32_t>(want.v),
        static_cast<uint32_t>(got.v));
  check(got.add == lane.add, (t + ": SatLedger::add").c_str(), lane.add ? 1 : 0, got.add ? 1 : 0);
  check(got.mul == lane.mul, (t + ": SatLedger::mul").c_str(), lane.mul ? 1 : 0, got.mul ? 1 : 0);
  check(got.rescale == lane.rescale, (t + ": SatLedger::rescale").c_str(), lane.rescale ? 1 : 0,
        got.rescale ? 1 : 0);
  check(got.rcp == lane.rcp, (t + ": SatLedger::rcp").c_str(), lane.rcp ? 1 : 0, got.rcp ? 1 : 0);
  check(got.rcp0 == lane.rcp0, (t + ": SatLedger::rcp0").c_str(), lane.rcp0 ? 1 : 0,
        got.rcp0 ? 1 : 0);
  // The collapsed bit, WITHOUT rcp0 -- which the interpreter reports separately.
  const bool any = got.add || got.mul || got.rescale || got.rcp;
  check(any == want.sat, (t + ": Status.sat").c_str(), want.sat ? 1 : 0, any ? 1 : 0);
  check(got.rcp0 == want.rcp0, (t + ": Status.rcp0").c_str(), want.rcp0 ? 1 : 0, got.rcp0 ? 1 : 0);

  if (lane.rcp0) ++g_rcp0_seen;
  if (lane.rcp) ++g_rcp_seen;
  if (lane.s0 == 0 || lane.s1 == 0) ++g_clamped_lo;
  if (lane.s0 == kOne || lane.s1 == kOne) ++g_clamped_hi;
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
  int32_t val() {
    switch (below(7)) {
      case 0:
        return 0;
      case 1:
        return kOne;
      case 2:
        return -kOne;
      case 3:
        return INT32_MAX;
      case 4:
        return INT32_MIN;
      case 5:
        return static_cast<int32_t>(next()) >> 12;
      default:
        return static_cast<int32_t>(next());
    }
  }
};

}  // namespace

int main(int argc, char** argv) {
  int random_iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && i + 1 < argc) {
      random_iters = std::atoi(argv[++i]);
    }
  }

  Vzhao_field_ring dut;
  dut.rst_n = 0;
  dut.v_valid_i = 0;
  dut.r_ready_i = 1;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  // ---- 1. a ring, swept across its own band -------------------------------
  {
    const int32_t r0 = 2 * kOne, r1 = 8 * kOne;
    for (int i = 0; i <= 24; ++i) {
      const int32_t d = (i * 10 * kOne) / 24;
      char nm[64];
      std::snprintf(nm, sizeof nm, "1.sweep d=%d/24", i);
      diff(dut, d, r0, r1, nm);
    }
    // The shape: zero outside, zero at the edges, and a peak in the middle.
    const Res inner = interp(0, r0, r1);
    const Res outer = interp(12 * kOne, r0, r1);
    const Res mid = interp(5 * kOne, r0, r1);
    check(inner.v == 0, "1.zero well inside the hole", 0, static_cast<uint32_t>(inner.v));
    check(outer.v == 0, "1.zero well outside", 0, static_cast<uint32_t>(outer.v));
    check(mid.v > 0, "1.and non-zero in the band", 1, mid.v > 0 ? 1 : 0);
  }

  // ---- 2. LAW 3: the clamp is on the PRODUCT ------------------------------
  // Below r0 and above r1 the parameter leaves [0, 1] and is clamped. This is
  // where clamping the INPUT to the span instead would differ.
  {
    for (int32_t d : {INT32_MIN, -100 * kOne, -1, 0, kOne, 9 * kOne, 100 * kOne, INT32_MAX}) {
      char nm[64];
      std::snprintf(nm, sizeof nm, "2.outside d=%d", d / kOne);
      diff(dut, d, 2 * kOne, 8 * kOne, nm);
    }
    check(g_clamped_lo > 0 && g_clamped_hi > 0, "2.both clamp rails were actually reached", 1,
          (g_clamped_lo > 0 && g_clamped_hi > 0) ? 1 : 0);
  }

  // ---- 3. LAW 5: reversed radii are lawful, and are still checked ---------
  // A first draft of this section asserted that swapping r0 and r1 gives a
  // DIFFERENT function. It does not, and the reason is worth writing down: the
  // smoothstep cubic satisfies S(1 - t) = 1 - S(t) exactly, so `1 - forward`
  // and `reversed` describe the same curve. RING is therefore symmetric under
  // swapping its radii, and the claim was simply wrong.
  //
  // That also says what law 5 really is: the two forms agree in exact
  // arithmetic and can only differ by ROUNDING. So there is nothing to assert
  // here beyond the differential itself -- which is the point of having one --
  // and the mutation sweep is what proves the rounding is the reference's.
  {
    for (int i = 0; i <= 12; ++i) {
      const int32_t d = (i * 10 * kOne) / 12;
      char nm[64];
      std::snprintf(nm, sizeof nm, "3.reversed radii d=%d/12", i);
      diff(dut, d, 8 * kOne, 2 * kOne, nm);
    }
    // And the sharper fact, measured rather than assumed. The symmetry is
    // EXACT in real arithmetic and BROKEN BY ROUNDING: over a sweep of 41
    // points, some agree and some do not. That is what makes law 5 testable at
    // all -- if the two forms agreed bit for bit everywhere, no mutation
    // between them could ever be caught.
    //
    // The first probe this section used was d = the midpoint, which is one of
    // the points where they DO coincide. A single sample said "symmetric" and
    // meant "I happened to pick a fixed point".
    int asym = 0, same = 0;
    for (int i = 0; i <= 40; ++i) {
      const int32_t d = (i * 12 * kOne) / 40;
      if (interp(d, 2 * kOne, 8 * kOne).v != interp(d, 8 * kOne, 2 * kOne).v)
        ++asym;
      else
        ++same;
    }
    check(asym > 0, "3.rounding BREAKS the exact radius symmetry, and is observable", 1,
          static_cast<uint32_t>(asym));
    check(same > 0, "3.while some points still coincide", 1, static_cast<uint32_t>(same));
    const int32_t m = 5 * kOne;
    check(interp(m, 2 * kOne, 8 * kOne).v == interp(m, 8 * kOne, 2 * kOne).v,
          "3.the midpoint is one of the coinciding points", 1,
          interp(m, 2 * kOne, 8 * kOne).v == interp(m, 8 * kOne, 2 * kOne).v ? 1 : 0);
  }

  // ---- 4. LAW 2: the degenerate ring ------------------------------------
  // r0 == r1 makes both spans zero. field_rcp(0) is pinned and sticky, so the
  // answer is DEFINED and the lane says it happened.
  {
    for (int32_t r : {0, kOne, -kOne, 7 * kOne, INT32_MAX, INT32_MIN}) {
      char nm[64];
      std::snprintf(nm, sizeof nm, "4.degenerate r=%d", r / kOne);
      diff(dut, 3 * kOne, r, r, nm);
    }
    check(g_rcp0_seen > 0, "4.the rcp0 lane was actually exercised", 1,
          static_cast<uint32_t>(g_rcp0_seen));
    // A degenerate ring still answers, rather than hanging or going undefined.
    const DutRes z = run(dut, 3 * kOne, kOne, kOne);
    check(z.rcp0, "4.and the sticky lane is set on it", 1, z.rcp0 ? 1 : 0);
  }

  // ---- 4b. TINY SPANS, where the reciprocal itself overflows -------------
  // `field_rcp` saturates -- bumping the `rcp` lane, distinct from `rcp0` --
  // when the reciprocal exceeds INT32_MAX, which happens for a span of a few
  // raw units. Every ring in sections 1-4 has a span of whole units, so none of
  // them reach it: a mutation pooling the `rcp` lane into `mul` survived the
  // first sweep purely because this case was missing.
  {
    const int32_t tiny[][2] = {{0, 2},     {0, 1},           {5, 7},          {-3, -1},
                               {100, 103}, {kOne, kOne + 2}, {kOne, kOne + 1}};
    for (int i = 0; i < 7; ++i) {
      char nm[64];
      std::snprintf(nm, sizeof nm, "4b.tiny span [%d,%d]", tiny[i][0], tiny[i][1]);
      diff(dut, (tiny[i][0] + tiny[i][1]) / 2, tiny[i][0], tiny[i][1], nm);
      std::snprintf(nm, sizeof nm, "4b.tiny span [%d,%d] off-centre", tiny[i][0], tiny[i][1]);
      diff(dut, tiny[i][1] + 4, tiny[i][0], tiny[i][1], nm);
    }
    check(g_rcp_seen > 0, "4b.the rcp lane was actually exercised", 1,
          static_cast<uint32_t>(g_rcp_seen));
  }

  // ---- 5. LAW 1: the midpoint is an EXACT average ------------------------
  // With both radii at the rail, an `fx_add` would saturate the SUM and halve
  // it -- putting the midpoint at half the radius. The exact 64-bit average is
  // the radius itself.
  {
    const Lanes big = lanes_of(0, INT32_MAX, INT32_MAX);
    check(big.mid == INT32_MAX, "5.midpoint of two rails is the rail",
          static_cast<uint32_t>(INT32_MAX), static_cast<uint32_t>(big.mid));
    const Lanes neg = lanes_of(0, INT32_MIN, INT32_MIN);
    check(neg.mid == INT32_MIN, "5.and of two negative rails is that rail",
          static_cast<uint32_t>(INT32_MIN), static_cast<uint32_t>(neg.mid));
    const Lanes mix = lanes_of(0, INT32_MIN, INT32_MAX);
    check(mix.mid == 0, "5.opposite rails average to zero", 0, static_cast<uint32_t>(mix.mid));
    // What a saturating add would have produced, for contrast.
    zref::SatLedger L{};
    const int32_t wrong = zref::fx_add(zref::fx16{INT32_MAX}, zref::fx16{INT32_MAX}, &L).raw / 2;
    check(wrong != big.mid, "5.a saturating add would give a different midpoint", 1,
          wrong != big.mid ? 1 : 0);

    diff(dut, 0, INT32_MAX, INT32_MAX, "5.rails");
    diff(dut, kOne, INT32_MIN, INT32_MAX, "5.opposite rails");
    diff(dut, INT32_MAX, INT32_MIN, INT32_MAX, "5.opposite rails, far d");
  }

  // ---- 6. the five lanes, and a quiet case -------------------------------
  {
    const DutRes q = run(dut, 5 * kOne, 2 * kOne, 8 * kOne);
    check(!q.add && !q.mul && !q.rescale && !q.rcp && !q.rcp0,
          "6.an ordinary ring reports nothing saturating", 0,
          (q.add ? 16u : 0u) | (q.mul ? 8u : 0u) | (q.rescale ? 4u : 0u) | (q.rcp ? 2u : 0u) |
              (q.rcp0 ? 1u : 0u));
  }

  // ---- 7. interface laws --------------------------------------------------
  {
    const DutRes a = run(dut, kOne, 0, 2 * kOne);
    const DutRes b = run(dut, 3 * kOne, kOne, 9 * kOne);
    check(a.cycles == b.cycles, "7.latency is fixed", static_cast<uint32_t>(a.cycles),
          static_cast<uint32_t>(b.cycles));

    dut.v_valid_i = 1;
    dut.d_i = static_cast<uint32_t>(4 * kOne);
    dut.r0_i = static_cast<uint32_t>(kOne);
    dut.r1_i = static_cast<uint32_t>(9 * kOne);
    dut.r_ready_i = 0;
    dut.eval();
    int g = 0;
    while (!dut.v_ready_o && g++ < 128) {
      zhao::tick(dut);
      dut.eval();
    }
    zhao::tick(dut);
    dut.v_valid_i = 0;
    dut.eval();
    int cyc = 0;
    bool ready_low = true;
    while (!dut.r_valid_o && cyc < 128) {
      if (dut.v_ready_o) ready_low = false;
      zhao::tick(dut);
      dut.eval();
      ++cyc;
    }
    check(ready_low, "7.v_ready is low while the ring is walking", 1, ready_low ? 1 : 0);
    const int32_t held = static_cast<int32_t>(dut.result_o);
    bool stable = true;
    for (int i = 0; i < 10; ++i) {
      zhao::tick(dut);
      dut.eval();
      if (!dut.r_valid_o || static_cast<int32_t>(dut.result_o) != held) stable = false;
    }
    check(stable, "7.the result is held under backpressure", 1, stable ? 1 : 0);
    dut.r_ready_i = 1;
    dut.eval();
    zhao::tick(dut);
    dut.eval();
    check(!dut.r_valid_o, "7.and retires on ready", 0, dut.r_valid_o ? 1 : 0);

    // Back to back, and the second is not the first leaking through.
    const DutRes r1 = run(dut, 3 * kOne, 2 * kOne, 8 * kOne);
    const DutRes r2 = run(dut, 7 * kOne, 2 * kOne, 8 * kOne);
    const Res w1 = interp(3 * kOne, 2 * kOne, 8 * kOne);
    const Res w2 = interp(7 * kOne, 2 * kOne, 8 * kOne);
    check(r1.v == w1.v && r2.v == w2.v, "7.back to back", 1,
          (r1.v == w1.v && r2.v == w2.v) ? 1 : 0);
  }

  // ---- 8. random differential --------------------------------------------
  if (random_iters > 0) {
    Prng rng(0x21A6u);
    for (int i = 0; i < random_iters; ++i) {
      int32_t r0, r1, d;
      if (rng.below(3) == 0) {
        // Rings with a real band, sampled inside and around it.
        r0 = static_cast<int32_t>(rng.below(64)) * kOne;
        r1 = r0 + static_cast<int32_t>(1 + rng.below(64)) * kOne;
        d = r0 + static_cast<int32_t>(rng.below(128)) * kOne - 32 * kOne;
      } else {
        r0 = rng.val();
        r1 = rng.val();
        d = rng.val();
      }
      char nm[64];
      std::snprintf(nm, sizeof nm, "8.random[%d]", i);
      diff(dut, d, r0, r1, nm);
    }
    std::printf("random: %d iterations, %d rcp0, %d lo-clamp, %d hi-clamp\n", random_iters,
                g_rcp0_seen, g_clamped_lo, g_clamped_hi);
  }

  return zhao::report_and_exit("field_ring_directed");
}
