// geom_quat2mat_mutant_control.cpp — THE POSITIVE CONTROL for the R4
// sequencer's differential checker, driving a committed MUTANT.
//
// geom_quat2mat_directed asserts bit-identity against the oracle, and on the
// sequenced arm that claim leans on the operand-mux SCHEDULE agreeing with the
// product-bank index map — machinery the pre-R4 spatial arm did not have, so
// no earlier run of the suite has ever demonstrated the checker can see a
// schedule fault. tests/mutants/zhao_geom_quat2mat_mutant.sv swaps the wy/wz
// schedule slots; THIS TEST PASSES WHEN THE DIFFERENTIAL FAILS — inverse
// polarity, evidence about the instrument, not about the design.
//
// The identity-quat case is the other half of the demonstration: the mutant
// AGREES with the oracle there (wy == wz == 0), which is why a checker that
// only drove weak vectors would wave the broken schedule through.

#include "Vzhao_geom_quat2mat_mutant.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"

#include <cstdint>
#include <cstdio>

namespace {

using zhao::check;
namespace zc = zref::creature;

/** Drive one quaternion through the MUTANT and count oracle disagreements. */
int mismatches(Vzhao_geom_quat2mat_mutant& dut, int16_t w, int16_t x, int16_t y, int16_t z) {
  zc::quat16 q;
  q.q[0] = w;
  q.q[1] = x;
  q.q[2] = y;
  q.q[3] = z;
  zc::mat3x4fx want{};
  zc::quat16_to_mat3(q, want, nullptr);

  dut.q_valid_i = 1;
  dut.m_ready_i = 1;
  dut.q_w_i = static_cast<uint16_t>(w);
  dut.q_x_i = static_cast<uint16_t>(x);
  dut.q_y_i = static_cast<uint16_t>(y);
  dut.q_z_i = static_cast<uint16_t>(z);
  dut.q_bone_i = 0x11;
  dut.eval();
  zhao::tick(dut);
  dut.q_valid_i = 0;
  dut.eval();
  int waited = 0;
  while (!dut.m_valid_o && waited++ < 64) {
    zhao::tick(dut);
    dut.eval();
  }
  check(dut.m_valid_o == 1, "the mutant still completes its walk", 1, dut.m_valid_o);

  int bad = 0;
  for (int i = 0; i < 12; ++i) {
    if (static_cast<int32_t>(dut.m_o[i]) != want.m[i]) ++bad;
  }
  return bad;
}

}  // namespace

int main(int, char**) {
  Vzhao_geom_quat2mat_mutant dut;
  dut.rst_n = 0;
  dut.q_valid_i = 0;
  dut.m_ready_i = 1;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  const int16_t ONE = 16384;  // S1.0.14

  // The weak vector: wy == wz == 0, so the swapped slots hold equal values
  // and the mutant is INDISTINGUISHABLE from the real block. This is why the
  // directed suite's strength lives in its non-trivial rotations.
  const int weak = mismatches(dut, ONE, 0, 0, 0);
  check(weak == 0, "identity: the schedule break is invisible to a weak vector", 0,
        static_cast<uint64_t>(weak));

  // The schedule-sensitive vector (the directed suite's own "arbitrary
  // rotation"): wy != wz, so the swap must surface. INVERTED POLARITY — this
  // control passes only when the differential checker FAILS the mutant.
  const int strong = mismatches(dut, 9000, -7000, 5000, -3000);
  check(strong > 0, "the differential checker FIRES on the schedule break", 1,
        strong > 0 ? 1 : 0);
  std::printf("[info] schedule-swap mutant: %d of 12 elements diverge on the strong vector\n",
              strong);

  dut.final();
  return zhao::report_and_exit("geom_quat2mat_mutant_control");
}
