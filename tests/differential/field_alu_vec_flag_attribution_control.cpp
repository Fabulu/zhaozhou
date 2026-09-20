// field_alu_vec_flag_attribution_control.cpp — the positive control for the
// FH11 per-lane saturation attribution.
//
// ---------------------------------------------------------------------------
// ONE DRIVER, TWO BUILDS, OPPOSITE OUTCOMES
// ---------------------------------------------------------------------------
// This file is compiled twice from the same source, selected by the object-like
// macro `ZHAO_ALUVEC_MUTANT`:
//
//   field_alu_vec_flag_attribution        production RTL      must PASS
//   field_alu_vec_flag_broadcast_control  the committed mutant must FAIL
//                                         (registered WILL_FAIL TRUE)
//
// The expectations below are written ONCE and asserted in both builds, so the
// two polarities cannot drift apart the way two hand-written drivers would.
//
// ---------------------------------------------------------------------------
// WHY THE SELECTOR CANNOT SILENTLY FAIL TO ENGAGE
// ---------------------------------------------------------------------------
// CLAUDE.md records a real incident where a mutant build measured unmutated
// production because a command-line `-D` could not override a FUNCTION-LIKE
// Verilog `define`, and said nothing when it failed to. That trap cannot reach
// this file, and the reason is structural rather than careful:
//
//  * the selector is a plain OBJECT-LIKE C++ macro, which `-D` does reach;
//  * it chooses a DIFFERENT HEADER AND A DIFFERENT CLASS NAME, so a build in
//    which it failed to apply would compile against the production class and
//    link against the mutant's Verilated objects -- or vice versa -- and FAIL
//    TO LINK rather than quietly measuring the wrong thing;
//  * and the mutant build is registered WILL_FAIL. If the selector somehow did
//    not engage, the driver would run against production, PASS, and ctest would
//    report the control as FAILED because it expected a failure.
//
// So every way this control can be wrong is LOUD. That is the property the
// silent-`-D` incident lacked, and it is why the negative control here is the
// build outcome itself rather than a separate diff of stdout.
//
// ---------------------------------------------------------------------------
// WHAT THESE CHECKS DISCRIMINATE
// ---------------------------------------------------------------------------
// The broadcast defect is INVISIBLE to every aggregate assertion: `sat_add_o`
// is bit-identical under production and under the mutant, because the OR of a
// broadcast OR is the same OR. Only a check that NAMES A LANE can separate
// them, and that is the entire content of this file.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#ifdef ZHAO_ALUVEC_MUTANT
#include "Vzhao_field_alu_vec_flag_broadcast_mutant.h"
using AluVec = Vzhao_field_alu_vec_flag_broadcast_mutant;
static const char* kWhich = "MUTANT (flag broadcast)";
#else
#include "Vzhao_field_alu_vec.h"
using AluVec = Vzhao_field_alu_vec;
static const char* kWhich = "production";
#endif

#include "zfield/zfield.hpp"
#include "zhao_sim.hpp"

namespace {

constexpr int kLanes = 4;

void put(uint32_t* w, int lane, int32_t v) { w[lane] = (uint32_t)v; }

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  AluVec top;

  printf("== field_alu_vec flag attribution, against %s ==\n", kWhich);

  // Exactly one lane is driven into an ADD overflow; the other three add 1+1.
  // The per-lane flag must name THAT LANE AND NO OTHER.
  //
  // Every lane takes its turn, so RTL that hard-wired a plausible constant is
  // caught too -- a control that only ever saturated lane 2 would be satisfied
  // by `sat_add_lane_o = 4'b0100`.
  for (int sat_lane = 0; sat_lane < kLanes; ++sat_lane) {
    top.lane_live_i = 0xF;
    for (int l = 0; l < kLanes; ++l) {
      put(top.a0_i.data(), l, l == sat_lane ? INT32_MAX : 1);
      put(top.b0_i.data(), l, l == sat_lane ? INT32_MAX : 1);
      put(top.c_i.data(), l, 0);
      put(top.a1_i.data(), l, 0);
      put(top.a2_i.data(), l, 0);
      put(top.b1_i.data(), l, 0);
      put(top.b2_i.data(), l, 0);
    }
    top.op_i = zfield::OP_ADD;
    top.imm_i = 0;
    top.eval();

    const uint32_t want = 1u << sat_lane;
    char what[128];
    snprintf(what, sizeof what,
             "lane %d saturates ALONE: sat_add_lane_o names only it (want 0x%X)", sat_lane,
             want);
    zhao::check((uint32_t)top.sat_add_lane_o == want, what, want,
                (uint32_t)top.sat_add_lane_o);

    // The aggregate is asserted too -- and is deliberately expected to be the
    // SAME in both builds. It is here to show what the group flag cannot see:
    // this check passes against the mutant, and the one above does not.
    snprintf(what, sizeof what, "group aggregate rises for lane %d (both builds agree)",
             sat_lane);
    zhao::check(top.sat_add_o == 1, what, 1, (uint32_t)top.sat_add_o);
  }

  // A dead lane saturating must reach nothing at all. Under the mutant the
  // broadcast is re-masked by `lane_live_i`, so this specific check passes
  // there as well -- recorded so nobody reads the control's failure as being
  // about masking when it is about attribution.
  {
    for (int l = 0; l < kLanes; ++l) {
      put(top.a0_i.data(), l, l == 2 ? INT32_MAX : 1);
      put(top.b0_i.data(), l, l == 2 ? INT32_MAX : 1);
      put(top.c_i.data(), l, 0);
    }
    top.op_i = zfield::OP_ADD;
    top.imm_i = 0;
    top.lane_live_i = 0xB;  // lane 2 is padding
    top.eval();
    zhao::check((uint32_t)top.sat_add_lane_o == 0u,
                "a DEAD lane's overflow reaches no lane flag", 0u,
                (uint32_t)top.sat_add_lane_o);
    zhao::check(top.sat_add_o == 0, "and no group flag", 0, (uint32_t)top.sat_add_o);
  }

  return zhao::report_and_exit("field_alu_vec_flag_attribution_control");
}
