// geom_mat3x4_mul_mutant_control.cpp — THE POSITIVE CONTROL for the R4
// single-lane walk's differential checker, driving a committed MUTANT.
//
// geom_mat3x4_mul_directed asserts bit-identity against the oracle, and on
// the MUL_LANES=1 arm that claim leans on the issue/commit accumulator
// clearing at every element boundary — machinery the MUL_LANES=3 arm never
// had, so no earlier run of the suite has ever demonstrated the checker can
// see a boundary fault. tests/mutants/zhao_geom_mat3x4_mul_mutant.sv removes
// the clear; THIS TEST PASSES WHEN THE DIFFERENTIAL FAILS — inverse polarity,
// evidence about the instrument, not about the design.
//
// The signature of the fault is asserted too: element 0 is CORRECT (the
// accumulator is still empty after reset), and the corruption begins at
// element 1. A checker that only read the first element would report the
// broken walk clean, which is the lesson worth committing.

#include "Vzhao_geom_mat3x4_mul_mutant.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"

#include <cstdint>
#include <cstdio>

namespace {

using zhao::check;
namespace zc = zref::creature;

const int32_t ONE = 1 << 16;

void setMat(VlUnpacked<IData, 12>& dst, const zc::mat3x4fx& m) {
  for (int i = 0; i < 12; ++i) dst[i] = static_cast<uint32_t>(m.m[i]);
}

}  // namespace

int main(int, char**) {
  Vzhao_geom_mat3x4_mul_mutant dut;
  dut.rst_n = 0;
  dut.in_valid_i = 0;
  dut.out_ready_i = 1;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  // The directed suite's own order-sensitive pair: a 90-degree rotation about
  // Z times a translation. Nothing degenerate, nothing on a rail.
  const zc::mat3x4fx R{{0, -ONE, 0, 0, ONE, 0, 0, 0, 0, 0, ONE, 0}};
  const zc::mat3x4fx T{{ONE, 0, 0, 4 * ONE, 0, ONE, 0, 0, 0, 0, ONE, 0}};
  zc::mat3x4fx want{};
  zc::mat3x4_mul(R, T, want, nullptr);

  dut.in_valid_i = 1;
  setMat(dut.a_m_i, R);
  setMat(dut.b_m_i, T);
  dut.in_tag_i = 0x42;
  dut.eval();
  zhao::tick(dut);
  dut.in_valid_i = 0;
  dut.eval();
  int guard = 0;
  while (!dut.out_valid_o && guard++ < 64) {
    zhao::tick(dut);
    dut.eval();
  }
  check(dut.out_valid_o == 1, "the mutant still completes its walk", 1, dut.out_valid_o);
  check(dut.products_done_o == 1, "and its counter still balances (the fault is invisible there)",
        1, dut.products_done_o);

  int bad = 0;
  for (int i = 0; i < 12; ++i) {
    if (static_cast<int32_t>(dut.out_m_o[i]) != want.m[i]) ++bad;
  }
  // The boundary-fault signature: element 0 right, later elements wrong.
  check(static_cast<int32_t>(dut.out_m_o[0]) == want.m[0],
        "element 0 is exactly right — the fault is a boundary fault, not an arithmetic one",
        static_cast<uint64_t>(static_cast<uint32_t>(want.m[0])),
        static_cast<uint32_t>(dut.out_m_o[0]));
  // INVERTED POLARITY — this control passes only when the differential
  // checker FAILS the mutant.
  check(bad > 0, "the differential checker FIRES on the uncleaned accumulator", 1,
        bad > 0 ? 1 : 0);
  std::printf("[info] boundary mutant: %d of 12 elements diverge\n", bad);

  dut.final();
  return zhao::report_and_exit("geom_mat3x4_mul_mutant_control");
}
