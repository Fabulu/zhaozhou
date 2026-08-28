// field_v3_rot_directed.cpp — ROT2 and ROT3 for four points at once, against
// the ONE semantic layer (zfield::steps::exec_op).
//
// LAWS, and where each is exercised:
//
//   1. THE ARITHMETIC IS THE REFERENCE'S, including the part that looks like a
//      defect: EACH PRODUCT IS ROUNDED SEPARATELY. Every value check runs
//      through exec_op, never through a rotation reimplemented here -- which
//      would only prove that two reimplementations agree, and would very
//      likely agree on the fused single-rounding form that is WRONG.
//   2. THE ANGLE IS THE LOW SIXTEEN BITS. Section 3 drives angles whose upper
//      half is deliberately rubbish and expects the defined answer, not an
//      error.
//   3. EVERY POINT HAS ITS OWN ANGLE. Section 2 gives four points four
//      different angles; section 4 gives them the same one. A unit that
//      broadcast point 0's angle passes the second and fails the first, and
//      the pair is what makes the failure specific.
//   4. THE PASS-THROUGH LANE IS COPIED, NOT COMPUTED. Section 5 checks the
//      axis lane is bit-identical to the input, which `x * cos(0)` is not
//      whenever a unit multiply does not round exactly.
//   5. ROT2 WRITES NO THIRD LANE. It reads zero, by the op's own law.
//   6. THE BANK CAN REFUSE. Section 6 refuses on a pseudo-random schedule and
//      asserts refusals ACTUALLY HAPPENED.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_v3_rot.h"

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
    switch (below(6)) {
      case 0:
        return 0;
      case 1:
        return (int32_t)0x00010000;
      case 2:
        return -(int32_t)0x00010000;
      case 3:
        return (int32_t)0x7FFFFFFF;
      case 4:
        return (int32_t)0x80000000;
      default:
        return (int32_t)next64();
    }
  }
  int32_t angle() {
    // Law 2: the upper half is IGNORED. Half the angles carry rubbish there,
    // so a unit that used the whole word diverges on those and only those.
    const uint32_t low = below(0x10000);
    const uint32_t high = (below(2) != 0) ? (uint32_t)(next64() & 0xFFFF0000u) : 0u;
    return (int32_t)(high | low);
  }
};

// ---- the four-wide bank model (engine property) ---------------------------
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

void step(Vzhao_field_v3_rot& dut, MulBank& mb) {
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

// ---- the oracle -----------------------------------------------------------
struct Want {
  int32_t r[3][kLanes];
  bool sat_add[kLanes];
  bool sat_mul[kLanes];
};

/** One point through the shipped interpreter, never a rotation written here. */
void oracle_point(bool rot3, uint32_t axis, int32_t x, int32_t y, int32_t z, int32_t ang,
                  int32_t* out, bool* sat_add, bool* sat_mul) {
  const std::vector<zfield::Table> no_tables;
  zref::SatLedger L;
  int32_t dst[3] = {0, 0, 0};
  if (rot3) {
    const int32_t src[4] = {x, y, z, ang};
    zfield::steps::exec_op(zfield::OP_ROT3, axis, no_tables, src, dst, &L);
    out[0] = dst[0];
    out[1] = dst[1];
    out[2] = dst[2];
  } else {
    const int32_t src[3] = {x, y, ang};
    zfield::steps::exec_op(zfield::OP_ROT2, 0, no_tables, src, dst, &L);
    out[0] = dst[0];
    out[1] = dst[1];
    out[2] = 0;  // law 5: ROT2 writes no third lane
  }
  *sat_add = (L.add != 0);
  *sat_mul = (L.mul != 0);
}

struct Group {
  int32_t x[kLanes], y[kLanes], z[kLanes], ang[kLanes];
};

Want oracle(bool rot3, uint32_t axis, const Group& g) {
  Want w{};
  for (int l = 0; l < kLanes; ++l) {
    int32_t out[3];
    oracle_point(rot3, axis, g.x[l], g.y[l], g.z[l], g.ang[l], out, &w.sat_add[l], &w.sat_mul[l]);
    for (int m = 0; m < 3; ++m) w.r[m][l] = out[m];
  }
  return w;
}

void drive(Vzhao_field_v3_rot& dut, bool rot3, uint32_t axis, const Group& g, uint8_t tag) {
  dut.v_valid_i = 1;
  dut.is_rot3_i = rot3 ? 1 : 0;
  dut.axis_i = (uint8_t)axis;
  dut.ang_0_i = (uint32_t)g.ang[0];
  dut.ang_1_i = (uint32_t)g.ang[1];
  dut.ang_2_i = (uint32_t)g.ang[2];
  dut.ang_3_i = (uint32_t)g.ang[3];
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

void check_rsp(Vzhao_field_v3_rot& dut, const Want& w, uint8_t tag, const std::string& what) {
  const uint32_t got[3][kLanes] = {{dut.o0_0_o, dut.o0_1_o, dut.o0_2_o, dut.o0_3_o},
                                   {dut.o1_0_o, dut.o1_1_o, dut.o1_2_o, dut.o1_3_o},
                                   {dut.o2_0_o, dut.o2_1_o, dut.o2_2_o, dut.o2_3_o}};
  for (int l = 0; l < kLanes; ++l) {
    for (int m = 0; m < 3; ++m) {
      check(got[m][l] == (uint32_t)w.r[m][l],
            (what + ": lane " + std::to_string(l) + " dst" + std::to_string(m)).c_str(),
            (uint32_t)w.r[m][l], got[m][l]);
    }
    check(((dut.sat_add_o >> l) & 1) == (w.sat_add[l] ? 1u : 0u),
          (what + ": lane " + std::to_string(l) + " add flag").c_str(), w.sat_add[l] ? 1 : 0,
          (dut.sat_add_o >> l) & 1);
    check(((dut.sat_mul_o >> l) & 1) == (w.sat_mul[l] ? 1u : 0u),
          (what + ": lane " + std::to_string(l) + " mul flag").c_str(), w.sat_mul[l] ? 1 : 0,
          (dut.sat_mul_o >> l) & 1);
  }
  check(dut.tag_o == tag, (what + ": tag").c_str(), tag, dut.tag_o);
}

int run_one(Vzhao_field_v3_rot& dut, MulBank& mb, bool rot3, uint32_t axis, const Group& g,
            uint8_t tag, const std::string& what) {
  const Want w = oracle(rot3, axis, g);
  drive(dut, rot3, axis, g, tag);
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
  check_rsp(dut, w, tag, what);
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

  Vzhao_field_v3_rot dut;
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

  Prng rng(random_n ? 0x8012u + (uint32_t)random_n : 0x8010u);

  if (random_n == 0) {
    printf("== section 1: the cardinal angles, ROT2 and every ROT3 axis ==\n");
    {
      // 0, a quarter, a half and three quarters of a turn: the places where
      // cos and sin are exactly 0 or +/-1, and where a fused rounding would
      // agree with a separate one -- so these prove the plumbing, not the law.
      Group g{};
      const int32_t quarters[kLanes] = {0x0000, 0x4000, 0x8000, 0xC000};
      for (int l = 0; l < kLanes; ++l) {
        g.x[l] = 0x00010000;
        g.y[l] = 0x00020000;
        g.z[l] = 0x00030000;
        g.ang[l] = quarters[l];
      }
      run_one(dut, mb, false, 0, g, 0x01, "ROT2 cardinal");
      run_one(dut, mb, true, 0, g, 0x02, "ROT3 X cardinal");
      run_one(dut, mb, true, 1, g, 0x03, "ROT3 Y cardinal");
      run_one(dut, mb, true, 2, g, 0x04, "ROT3 Z cardinal");
    }

    printf("== section 2: four points, four DIFFERENT angles ==\n");
    {
      // Law 3: a unit that broadcast point 0's angle fails here and passes
      // section 4, which is what makes the failure specific.
      Prng r(0xA9E5u);
      for (int k = 0; k < 8; ++k) {
        Group g{};
        for (int l = 0; l < kLanes; ++l) {
          g.x[l] = r.coord();
          g.y[l] = r.coord();
          g.z[l] = r.coord();
          g.ang[l] = r.angle();
        }
        const bool rot3 = (k & 1) != 0;
        run_one(dut, mb, rot3, (uint32_t)(k % 3), g, (uint8_t)(0x10 + k),
                "distinct angles " + std::to_string(k));
      }
    }

    printf("== section 3: the angle's UPPER half is ignored, not rejected ==\n");
    {
      // Law 2. The same low half with four different upper halves must give
      // four identical answers -- and the oracle is asked the same question,
      // so this is a differential rather than a self-consistency check.
      Group g{};
      for (int l = 0; l < kLanes; ++l) {
        g.x[l] = 0x00012345;
        g.y[l] = -0x00023456;
        g.z[l] = 0x00034567;
      }
      g.ang[0] = (int32_t)0x00001234;
      g.ang[1] = (int32_t)0xFFFF1234;
      g.ang[2] = (int32_t)0x7FFF1234;
      g.ang[3] = (int32_t)0x80001234;
      run_one(dut, mb, true, 2, g, 0x20, "rubbish in the upper half");

      // And the four lanes must agree with each other, which the oracle
      // comparison alone would not say out loud.
      check(dut.o0_0_o == dut.o0_1_o && dut.o0_1_o == dut.o0_2_o && dut.o0_2_o == dut.o0_3_o,
            "all four lanes agree -- only the low sixteen bits mattered", 1,
            (dut.o0_0_o == dut.o0_1_o && dut.o0_1_o == dut.o0_2_o && dut.o0_2_o == dut.o0_3_o) ? 1
                                                                                               : 0);
    }

    printf("== section 4: four points, the SAME angle ==\n");
    {
      Prng r(0x5A3Eu);
      for (int k = 0; k < 4; ++k) {
        Group g{};
        const int32_t a = r.angle();
        for (int l = 0; l < kLanes; ++l) {
          g.x[l] = r.coord();
          g.y[l] = r.coord();
          g.z[l] = r.coord();
          g.ang[l] = a;
        }
        run_one(dut, mb, (k & 1) != 0, 2, g, (uint8_t)(0x30 + k),
                "shared angle " + std::to_string(k));
      }
    }

    printf("== section 5: the pass-through lane is COPIED, bit for bit ==\n");
    {
      // Law 4. An axis lane computed as x*cos(0) differs whenever that unit
      // multiply does not round exactly, so the check is EQUALITY WITH THE
      // INPUT rather than closeness.
      Prng r(0xC0B1u);
      for (uint32_t axis = 0; axis < 3; ++axis) {
        Group g{};
        for (int l = 0; l < kLanes; ++l) {
          g.x[l] = r.coord();
          g.y[l] = r.coord();
          g.z[l] = r.coord();
          g.ang[l] = r.angle();
        }
        run_one(dut, mb, true, axis, g, (uint8_t)(0x40 + axis),
                "axis " + std::to_string(axis) + " pass-through");
        const uint32_t got[3][kLanes] = {{dut.o0_0_o, dut.o0_1_o, dut.o0_2_o, dut.o0_3_o},
                                         {dut.o1_0_o, dut.o1_1_o, dut.o1_2_o, dut.o1_3_o},
                                         {dut.o2_0_o, dut.o2_1_o, dut.o2_2_o, dut.o2_3_o}};
        const int32_t* src = (axis == 0) ? g.x : (axis == 1) ? g.y : g.z;
        for (int l = 0; l < kLanes; ++l) {
          check(got[axis][l] == (uint32_t)src[l],
                ("axis " + std::to_string(axis) + " lane " + std::to_string(l) +
                 " is the INPUT, bit for bit")
                    .c_str(),
                (uint32_t)src[l], got[axis][l]);
        }
      }
    }

    printf("== section 6: the bank refuses, and the answers do not move ==\n");
    {
      Prng r(0xBEEFu);
      mb.refusals = 0;
      const int kGroups = 16;
      for (int k = 0; k < kGroups; ++k) {
        Group g{};
        for (int l = 0; l < kLanes; ++l) {
          g.x[l] = r.coord();
          g.y[l] = r.coord();
          g.z[l] = r.coord();
          g.ang[l] = r.angle();
        }
        const bool rot3 = (r.below(2) != 0);
        const uint32_t axis = r.below(3);
        const uint8_t tag = (uint8_t)(0x80 + k);
        const Want w = oracle(rot3, axis, g);

        drive(dut, rot3, axis, g, tag);
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
        check(cycles < 1024, (what + ": reply arrived, no hang").c_str(), 1, cycles < 1024 ? 1 : 0);
        check_rsp(dut, w, tag, what);
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
        g.x[l] = 1 << 16;
        g.y[l] = 2 << 16;
        g.z[l] = 3 << 16;
        g.ang[l] = 0x1000 * (l + 1);
      }
      const int c2 = run_one(dut, mb, false, 0, g, 0x60, "ROT2 cost");
      const int c3 = run_one(dut, mb, true, 2, g, 0x61, "ROT3 cost");
      printf("   MEASURED four-point ROT2 %d clocks, ROT3 %d clocks\n", c2, c3);
      // Eight lookups at one per clock, two to drain, then four bank requests
      // at three clocks each. The bound is generous: what is asserted is the
      // SHAPE, not a schedule nobody has pinned yet.
      check(c2 <= 40, "a four-point ROT2 costs at most 40 clocks", 40, c2);
      check(c3 == c2, "ROT3 costs the same -- the axis is a mux, not more work", (uint32_t)c2,
            (uint32_t)c3);
    }
    printf("== section 8: saturation on SOME lanes, so a mirrored flag shows ==\n");
    {
      // R22 -- reporting sat_mul_o on the MIRRORED lane -- survived the first
      // sweep. Every earlier section either saturates on no lane or on all
      // four, and a mirror is invisible when the vector is symmetric.
      //
      // The reachable corner: cos(0x8000) is exactly -1.0, so c*p with
      // p = INT32_MIN gives +2^31, one past the rail. Give that to ONE lane
      // and leave the others small, and the fired vector is 1000 -- whose
      // mirror is 0001.
      Group g{};
      g.ang[0] = 0x8000;  // cos = -1.0
      g.x[0] = (int32_t)0x80000000;
      g.y[0] = 0;
      for (int l = 1; l < kLanes; ++l) {
        g.ang[l] = 0x1000 * l;
        g.x[l] = 1 << 16;
        g.y[l] = 2 << 16;
      }
      for (int l = 0; l < kLanes; ++l) g.z[l] = 0;
      run_one(dut, mb, false, 0, g, 0x70, "saturation on lane 0 only");
      printf("   MEASURED sat_mul_o = %X (lane 0 alone)\n", dut.sat_mul_o);
      check((dut.sat_mul_o & 1u) == 1u, "lane 0 reports its multiply saturation", 1,
            dut.sat_mul_o & 1u);
      check((dut.sat_mul_o & 0x8u) == 0u, "and lane 3 -- its mirror -- does NOT", 0,
            (dut.sat_mul_o >> 3) & 1u);

      // And the other way round, so a mirror cannot pass by luck of position.
      Group h{};
      h.ang[3] = 0x8000;
      h.x[3] = (int32_t)0x80000000;
      h.y[3] = 0;
      for (int l = 0; l < 3; ++l) {
        h.ang[l] = 0x1000 * (l + 1);
        h.x[l] = 1 << 16;
        h.y[l] = 2 << 16;
      }
      for (int l = 0; l < kLanes; ++l) h.z[l] = 0;
      run_one(dut, mb, false, 0, h, 0x71, "saturation on lane 3 only");
      printf("   MEASURED sat_mul_o = %X (lane 3 alone)\n", dut.sat_mul_o);
      check((dut.sat_mul_o & 0x8u) == 0x8u, "lane 3 reports its own", 1, (dut.sat_mul_o >> 3) & 1u);
      check((dut.sat_mul_o & 1u) == 0u, "and lane 0 does not", 0, dut.sat_mul_o & 1u);
    }
  } else {
    printf("== random differential: %d groups against zfield::exec_op ==\n", random_n);
    for (int i = 0; i < random_n; ++i) {
      Group g{};
      for (int l = 0; l < kLanes; ++l) {
        g.x[l] = rng.coord();
        g.y[l] = rng.coord();
        g.z[l] = rng.coord();
        g.ang[l] = rng.angle();
      }
      const bool rot3 = (rng.below(2) != 0);
      run_one(dut, mb, rot3, rng.below(3), g, (uint8_t)(i & 0xFF),
              "random group " + std::to_string(i));
    }
  }

  return zhao::report_and_exit("field_v3_rot_directed");
}
