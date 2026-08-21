// field_rot_directed.cpp — OP_ROT2 and OP_ROT3, RTL against the interpreter.
//
// FIVE LAWS, each a place an implementation drifts:
//
//   1. EACH PRODUCT IS ROUNDED SEPARATELY. This is the one that looks like a
//      defect and is the law. `fx_sub(fx_mul(c,p), fx_mul(s,q))` rounds TWICE
//      and then saturates the difference. Everywhere else in this design a row
//      of products is summed exactly and rescaled ONCE (A3b) -- because double
//      rounding is normally the bug. Here the reference does the opposite.
//      Section 5 is built to catch a "corrected" fused implementation: it
//      counts the inputs where the two forms actually differ, and asserts that
//      count is large, because a sweep on which they happen to agree proves
//      nothing about which one is implemented.
//   2. THE ANGLE IS THE LOW SIXTEEN BITS of reg[b]; the upper half is ignored,
//      not rejected. Section 3 puts rubbish there.
//   3. ONE ANGLE FEEDS BOTH FUNCTIONS, and cos is sin(a + 0x4000) WRAPPING in
//      sixteen bits. Section 4 sits on the wrap.
//   4. THE PASS-THROUGH LANE IS COPIED, not multiplied by a unit cosine.
//      Section 2 checks it bit for bit at angles where cos is not exactly 1.
//   5. ROT2 IS ROT3'S Z CASE on two lanes.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "verilated.h"

#include "Vzhao_field_rot.h"

#include "zfield/zfield.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_trig.hpp"

namespace {

using zhao::check;

constexpr int32_t kOne = 1 << 16;

struct Res {
  int32_t o[3] = {0, 0, 0};
  bool sat = false;
};

/** One op as a real program, through the shipped interpreter. */
Res interp(bool rot3, uint32_t axis, int32_t ang, int32_t x, int32_t y, int32_t z) {
  zfield::Decoded prog;
  prog.profile = 0;

  zfield::Instr ins{};
  ins.op = rot3 ? zfield::OP_ROT3 : zfield::OP_ROT2;
  ins.dst = 8;
  ins.a = 0;    // the adjacent input triple lives at reg0..reg2
  ins.b = 3;    // the angle
  ins.c = 0;
  ins.imm = axis;
  prog.instrs.push_back(ins);

  zfield::Instr end{};
  end.op = zfield::OP_END;
  prog.instrs.push_back(end);

  for (int i = 0; i < 4; ++i) {
    zfield::IoLane l{};
    l.name = "i";
    l.type = 0;
    l.reg = static_cast<uint8_t>(i);
    prog.in_lanes.push_back(l);
  }
  for (int i = 0; i < 3; ++i) {
    zfield::IoLane l{};
    l.name = "o";
    l.type = 0;
    l.reg = static_cast<uint8_t>(8 + i);
    prog.out_lanes.push_back(l);
  }

  const int32_t in[4] = {x, y, z, ang};
  int32_t out[3] = {0, 0, 0};
  const zfield::Status st = zfield::interpret(prog, in, 4, out, 3);
  Res r;
  r.o[0] = out[0];
  r.o[1] = out[1];
  r.o[2] = out[2];
  r.sat = st.sat;
  return r;
}

struct DutRes {
  int32_t o[3] = {0, 0, 0};
  bool sat_add = false, sat_mul = false;
  int cycles = 0;
};

DutRes run(Vzhao_field_rot& dut, bool rot3, uint32_t axis, int32_t ang, int32_t x, int32_t y,
           int32_t z) {
  dut.v_valid_i = 1;
  dut.is_rot3_i = rot3 ? 1 : 0;
  dut.axis_i = static_cast<uint8_t>(axis & 3u);
  dut.ang_i = static_cast<uint32_t>(ang);
  dut.a0_i = static_cast<uint32_t>(x);
  dut.a1_i = static_cast<uint32_t>(y);
  dut.a2_i = static_cast<uint32_t>(z);
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
  r.o[0] = static_cast<int32_t>(dut.o0_o);
  r.o[1] = static_cast<int32_t>(dut.o1_o);
  r.o[2] = static_cast<int32_t>(dut.o2_o);
  r.sat_add = dut.sat_add_o != 0;
  r.sat_mul = dut.sat_mul_o != 0;
  r.cycles = cycles;
  zhao::tick(dut);
  dut.eval();
  return r;
}

int g_fused_differs = 0;  // law 1 coverage

/**
 * The per-lane saturation attribution.
 *
 * `Status.sat` collapses all five SatLedger lanes into one bit, and the RTL
 * reports `add` and `mul` separately -- so checking only the collapsed bit
 * cannot tell the two apart. A mutation that moves the subtract's saturation
 * from the `add` lane to the `mul` lane survived the first sweep for exactly
 * that reason: the block returned every number correctly and misreported where
 * the range was lost.
 *
 * Built on `zref::fx_*`, so only the ATTRIBUTION is restated here; the rounding
 * law still has one implementation.
 */
struct Lanes {
  bool add = false;
  bool mul = false;
};

Lanes lanes_of(bool rot3, uint32_t axis, int32_t ang, int32_t x, int32_t y, int32_t z) {
  int32_t p = x, q = y;
  if (rot3 && axis == 0) { p = y; q = z; }
  else if (rot3 && axis == 1) { p = z; q = x; }

  zref::SatLedger L{};
  const int32_t c = zref::fx_cos(zref::angle16{static_cast<uint16_t>(ang & 0xFFFF)}).raw;
  const int32_t sn = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ang & 0xFFFF)}).raw;
  const zref::fx16 cp = zref::fx_mul(zref::fx16{c}, zref::fx16{p}, &L);
  const zref::fx16 sq = zref::fx_mul(zref::fx16{sn}, zref::fx16{q}, &L);
  (void)zref::fx_sub(cp, sq, &L);
  const zref::fx16 sp = zref::fx_mul(zref::fx16{sn}, zref::fx16{p}, &L);
  const zref::fx16 cq = zref::fx_mul(zref::fx16{c}, zref::fx16{q}, &L);
  (void)zref::fx_add(sp, cq, &L);

  Lanes o;
  o.add = L.add != 0;
  o.mul = L.mul != 0;
  return o;
}

/** Would a FUSED (single-rounding) implementation give a different answer? */
bool fused_would_differ(int32_t ang, int32_t p, int32_t q) {
  zref::SatLedger L{};
  const int32_t c = zref::fx_cos(zref::angle16{static_cast<uint16_t>(ang & 0xFFFF)}).raw;
  const int32_t s = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ang & 0xFFFF)}).raw;
  // The law: round each product, then subtract.
  const int32_t split =
      zref::fx_sub(zref::fx_mul(zref::fx16{c}, zref::fx16{p}, &L),
                   zref::fx_mul(zref::fx16{s}, zref::fx16{q}, &L), &L).raw;
  // The tempting "improvement": sum exactly, round once.
  const int64_t exact = static_cast<int64_t>(c) * p - static_cast<int64_t>(s) * q;
  const int32_t fused = zref::rescale_s32(exact, 16, &L);
  return split != fused;
}

void diff(Vzhao_field_rot& dut, bool rot3, uint32_t axis, int32_t ang, int32_t x, int32_t y,
          int32_t z, const char* what) {
  const Res want = interp(rot3, axis, ang, x, y, z);
  const DutRes got = run(dut, rot3, axis, ang, x, y, z);
  const std::string t(what);

  check(got.o[0] == want.o[0], (t + ": lane 0").c_str(), static_cast<uint32_t>(want.o[0]),
        static_cast<uint32_t>(got.o[0]));
  check(got.o[1] == want.o[1], (t + ": lane 1").c_str(), static_cast<uint32_t>(want.o[1]),
        static_cast<uint32_t>(got.o[1]));
  if (rot3) {
    check(got.o[2] == want.o[2], (t + ": lane 2").c_str(), static_cast<uint32_t>(want.o[2]),
          static_cast<uint32_t>(got.o[2]));
  } else {
    check(got.o[2] == 0, (t + ": ROT2 writes two lanes").c_str(), 0,
          static_cast<uint32_t>(got.o[2]));
  }
  check((got.sat_add || got.sat_mul) == want.sat, (t + ": Status.sat").c_str(),
        want.sat ? 1 : 0, (got.sat_add || got.sat_mul) ? 1 : 0);

  // ...and the lanes SEPARATELY. The collapsed bit above cannot tell an `add`
  // saturation from a `mul` one, and the reference keeps them apart on purpose.
  const Lanes lane = lanes_of(rot3, axis, ang, x, y, z);
  check(got.sat_add == lane.add, (t + ": SatLedger::add").c_str(), lane.add ? 1 : 0,
        got.sat_add ? 1 : 0);
  check(got.sat_mul == lane.mul, (t + ": SatLedger::mul").c_str(), lane.mul ? 1 : 0,
        got.sat_mul ? 1 : 0);

  // Law 1 coverage, on whichever pair actually rotates.
  int32_t p = x, q = y;
  if (rot3 && axis == 0) { p = y; q = z; }
  else if (rot3 && axis == 1) { p = z; q = x; }
  if (fused_would_differ(ang, p, q)) ++g_fused_differs;
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
    switch (below(6)) {
      case 0: return 0;
      case 1: return kOne;
      case 2: return -kOne;
      case 3: return INT32_MAX;
      case 4: return INT32_MIN;
      default: return static_cast<int32_t>(next());
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

  Vzhao_field_rot dut;
  dut.rst_n = 0;
  dut.v_valid_i = 0;
  dut.r_ready_i = 1;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  // ---- 1. ROT2 around the circle ------------------------------------------
  {
    for (int k = 0; k < 16; ++k) {
      const int32_t ang = k * 0x1000;
      char nm[64];
      std::snprintf(nm, sizeof nm, "1.rot2 ang=%04X", ang);
      diff(dut, false, 0, ang, 3 * kOne, -2 * kOne, 0, nm);
    }
    // The cardinal angles, where the answer is checkable by inspection.
    const Res r0 = interp(false, 0, 0x0000, kOne, 0, 0);
    check(r0.o[0] == kOne && r0.o[1] == 0, "1.angle 0 is the identity", kOne,
          static_cast<uint32_t>(r0.o[0]));
    const Res r90 = interp(false, 0, 0x4000, kOne, 0, 0);
    check(r90.o[1] == kOne, "1.a quarter turn sends x to y", kOne,
          static_cast<uint32_t>(r90.o[1]));
  }

  // ---- 2. ROT3, all three axes, and LAW 4 --------------------------------
  {
    for (uint32_t axis = 0; axis < 3; ++axis) {
      for (int k = 0; k < 8; ++k) {
        const int32_t ang = k * 0x2000 + 0x0777;
        char nm[80];
        std::snprintf(nm, sizeof nm, "2.rot3 axis=%u ang=%04X", axis, ang);
        diff(dut, true, axis, ang, 5 * kOne, -3 * kOne, 7 * kOne, nm);
      }
    }
    // LAW 4: the axis lane is COPIED. At an angle where cos is not exactly 1,
    // a pass-through implemented as `v * cos` would differ.
    const int32_t ang = 0x1234;
    const int32_t xv = 12345678, yv = -87654321, zv = 4242424;
    const Res rx = interp(true, 0, ang, xv, yv, zv);
    check(rx.o[0] == xv, "2.X axis: lane 0 is copied bit for bit",
          static_cast<uint32_t>(xv), static_cast<uint32_t>(rx.o[0]));
    const Res ry = interp(true, 1, ang, xv, yv, zv);
    check(ry.o[1] == yv, "2.Y axis: lane 1 is copied bit for bit",
          static_cast<uint32_t>(yv), static_cast<uint32_t>(ry.o[1]));
    const Res rz = interp(true, 2, ang, xv, yv, zv);
    check(rz.o[2] == zv, "2.Z axis: lane 2 is copied bit for bit",
          static_cast<uint32_t>(zv), static_cast<uint32_t>(rz.o[2]));
    // ...and prove the copy is not trivially right: cos(ang) != 1 here, so a
    // multiply-by-cosine really would change the value.
    const int32_t c = zref::fx_cos(zref::angle16{static_cast<uint16_t>(ang)}).raw;
    check(c != kOne, "2.and cos is not 1 at that angle, so the copy is load-bearing", 1,
          c != kOne ? 1 : 0);
  }

  // ---- 3. LAW 2: the upper half of the angle register is IGNORED ---------
  {
    for (int k = 0; k < 8; ++k) {
      const int32_t low = k * 0x2001;
      const int32_t dirty = static_cast<int32_t>((0xDEADu << 16) | (low & 0xFFFF));
      const Res clean = interp(false, 0, low & 0xFFFF, 7 * kOne, 11 * kOne, 0);
      const Res messy = interp(false, 0, dirty, 7 * kOne, 11 * kOne, 0);
      char nm[80];
      std::snprintf(nm, sizeof nm, "3.rubbish in the top half %d", k);
      check(clean.o[0] == messy.o[0] && clean.o[1] == messy.o[1], nm,
            static_cast<uint32_t>(clean.o[0]), static_cast<uint32_t>(messy.o[0]));
      std::snprintf(nm, sizeof nm, "3.dut agrees %d", k);
      diff(dut, false, 0, dirty, 7 * kOne, 11 * kOne, 0, nm);
    }
    // A negative angle register: the low half is still what counts.
    diff(dut, false, 0, -1, kOne, kOne, 0, "3.angle register -1");
  }

  // ---- 4. LAW 3: the cos wrap at the top of the circle -------------------
  {
    for (int32_t ang : {0x3FFF, 0x4000, 0x4001, 0xBFFF, 0xC000, 0xC001, 0xFFFF}) {
      char nm[64];
      std::snprintf(nm, sizeof nm, "4.wrap ang=%04X", ang);
      diff(dut, false, 0, ang, 6 * kOne, -5 * kOne, 0, nm);
      diff(dut, true, 1, ang, 6 * kOne, -5 * kOne, 2 * kOne, nm);
    }
  }

  // ---- 5. LAW 1: the two-rounding law, and proof the sweep can see it -----
  // A fused single-rounding implementation is the natural "improvement" and is
  // WRONG here. It only differs on inputs where the two roundings actually
  // disagree, so a sweep that never hits one cannot tell them apart.
  {
    const int before = g_fused_differs;
    Prng rng(0xC10Bu);
    // Roughly a quarter of random inputs distinguish the two forms, so 512
    // draws give ~120 discriminating cases. The threshold below is stated
    // against the DISCRIMINATING count, not the sweep size: only those inputs
    // can catch a fused implementation, and a sweep of any size on which the
    // two agree everywhere would prove nothing.
    for (int i = 0; i < 512; ++i) {
      const int32_t ang = static_cast<int32_t>(rng.next() & 0xFFFF);
      const int32_t p = static_cast<int32_t>(rng.next()) >> 4;
      const int32_t q = static_cast<int32_t>(rng.next()) >> 4;
      char nm[64];
      std::snprintf(nm, sizeof nm, "5.split-rounding %d", i);
      diff(dut, false, 0, ang, p, q, 0, nm);
    }
    check(g_fused_differs - before > 64,
          "5.the sweep reached many inputs where a FUSED form would differ", 64,
          static_cast<uint32_t>(g_fused_differs - before));
  }

  // ---- 6. saturation, on both lanes --------------------------------------
  {
    diff(dut, false, 0, 0x2000, INT32_MAX, INT32_MAX, 0, "6.both rails");
    diff(dut, false, 0, 0x6000, INT32_MIN, INT32_MAX, 0, "6.opposite rails");
    diff(dut, true, 0, 0xA000, INT32_MIN, INT32_MIN, INT32_MIN, "6.rot3 rails");
    const DutRes r = run(dut, false, 0, 0x2000, INT32_MAX, INT32_MAX, 0);
    check(r.sat_add || r.sat_mul, "6.and something was reported saturating", 1,
          (r.sat_add || r.sat_mul) ? 1 : 0);
    // A quiet case, so "always report saturation" is not a passing answer.
    const DutRes q = run(dut, false, 0, 0x1000, kOne, kOne, 0);
    check(!q.sat_add && !q.sat_mul, "6.a quiet case reports nothing", 0,
          (q.sat_add ? 2u : 0u) | (q.sat_mul ? 1u : 0u));
  }

  // ---- 6b. the two saturation lanes, driven APART -------------------------
  // `mul` fires when a product overflows the rescale; `add` fires when the
  // difference or sum of two in-range products does. Section 6 drives both at
  // once, which cannot distinguish them -- a block that reported everything in
  // one lane would pass it.
  {
    // A large coordinate against a near-unit cosine: the PRODUCTS saturate.
    const DutRes m = run(dut, false, 0, 0x0000, INT32_MAX, 0, 0);
    const Lanes ml = lanes_of(false, 0, 0x0000, INT32_MAX, 0, 0);
    check(m.sat_mul == ml.mul, "6b.mul lane agrees on a saturating product",
          ml.mul ? 1 : 0, m.sat_mul ? 1 : 0);
    check(m.sat_add == ml.add, "6b.and the add lane agrees too", ml.add ? 1 : 0,
          m.sat_add ? 1 : 0);

    // Two in-range products whose DIFFERENCE leaves the range: `add` alone.
    int found = 0;
    Prng rng(0x5A7Du);
    for (int i = 0; i < 2000 && found < 3; ++i) {
      const int32_t ang = static_cast<int32_t>(rng.next() & 0xFFFF);
      const int32_t px = static_cast<int32_t>(rng.next());
      const int32_t qy = static_cast<int32_t>(rng.next());
      const Lanes l = lanes_of(false, 0, ang, px, qy, 0);
      if (l.add && !l.mul) {
        const DutRes d = run(dut, false, 0, ang, px, qy, 0);
        check(d.sat_add && !d.sat_mul,
              "6b.an add-only saturation is reported in the ADD lane alone", 1,
              (d.sat_add && !d.sat_mul) ? 1 : 0);
        ++found;
      }
    }
    check(found > 0, "6b.and such a case was actually found", 1,
          static_cast<uint32_t>(found));
  }

  // ---- 7. interface laws --------------------------------------------------
  {
    const DutRes a = run(dut, false, 0, 0x1111, kOne, kOne, 0);
    const DutRes b = run(dut, true, 1, 0x2222, kOne, kOne, kOne);
    check(a.cycles == b.cycles, "7.latency is fixed and the same for both ops",
          static_cast<uint32_t>(a.cycles), static_cast<uint32_t>(b.cycles));

    dut.v_valid_i = 1;
    dut.is_rot3_i = 0;
    dut.axis_i = 0;
    dut.ang_i = 0x3333;
    dut.a0_i = static_cast<uint32_t>(2 * kOne);
    dut.a1_i = static_cast<uint32_t>(3 * kOne);
    dut.a2_i = 0;
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
    check(ready_low, "7.v_ready is low while a rotation is walking", 1, ready_low ? 1 : 0);
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

  // ---- 8. random differential --------------------------------------------
  if (random_iters > 0) {
    Prng rng(0x8A17u);
    for (int i = 0; i < random_iters; ++i) {
      const bool rot3 = (rng.below(2) != 0);
      const uint32_t axis = rng.below(3);
      const int32_t ang = static_cast<int32_t>(rng.next());
      char nm[64];
      std::snprintf(nm, sizeof nm, "8.random[%d]", i);
      diff(dut, rot3, axis, ang, rng.val(), rng.val(), rng.val(), nm);
    }
    std::printf("random: %d iterations, %d inputs where a fused form would differ\n",
                random_iters, g_fused_differs);
  }

  return zhao::report_and_exit("field_rot_directed");
}
