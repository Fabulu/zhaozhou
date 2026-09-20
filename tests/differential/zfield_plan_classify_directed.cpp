// zfield_plan_classify_directed.cpp -- THE ADMISSION DECISION AT ITS BOUNDARY.
//
// `zfield::plan()`'s `perf_class` is not a diagnostic. It is the gate that
// decides whether a field program is admitted to the realtime path, and owner
// ruling R156 is about exactly that:
//
//   > `reference/src/zfield/zfield_plan.cpp` carries `kGroups`, and it feeds
//   > `hot = bind <= 6000`. With the correct 297, a program taking up to
//   > 6,527 clocks per association is classified HOT while being over its
//   > deadline. ... the stale value makes the classifier optimistic --
//   > flattering direction, again. A program that should be refused gets
//   > admitted.
//
// WHY THIS FILE EXISTS AT ALL. The 273 -> 297 correction changes NOTHING
// observable about the three shipped Earth programs: every one of them binds
// on DIST (1 request * 20 clocks = 20 per group), so its occupancy moves
// 20*273 = 5,460 -> 20*297 = 5,940 and it reads HOT either way. Measured, both
// ways, with `tools/field/measure_earth_ops.cpp` -- byte-identical output.
//
// That is a correct result and it is ALSO the exact shape of the stale-binary
// trap: "the measurement did not move after a change that must have moved it".
// A correction whose only evidence is an unchanged number is indistinguishable
// from a correction that never compiled. So the constant owes a case where it
// DOES change the verdict, and this file is that case.
//
// THE ARITHMETIC, because the boundary is narrow and worth writing down.
// `finish_demand()` computes
//
//     bind = max(vec_issue, vmul_slots, curve_req*14, dist_req*20) * kGroups
//
// so `bind` is always (an integer per-group factor M) * kGroups, and
//
//     HOT at 273  <=>  M * 273 <= 6000  <=>  M <= 21
//     HOT at 297  <=>  M * 297 <= 6000  <=>  M <= 20
//
// M = 21 is therefore the ENTIRE misclassification band, and it is reachable:
// its real cost is 21 * 297 = 6,237 clocks against a 6,000-clock deadline --
// 237 over, and admitted under the stale constant. (R156's "6,527" is the
// continuous bound 6000 * 297/273 = 6527.47; because M is an integer count of
// uops, services or bank slots, 6,237 is the largest cost actually reachable.
// Both numbers are true; this is the one a program can exhibit.)
//
// The shipped programs sit at M = 20 -- ONE STEP below the band. That is the
// real finding here: they are not comfortably inside the deadline, they are
// one vector uop away from being misclassified by the old constant.
//
// WHAT IS ASSERTED, and what is deliberately NOT. This test asserts the
// CORRECT behaviour (M=20 admits, M=21 refuses) and never asserts the defect
// -- a test that fires only while the bug exists passes for the wrong reason
// forever after the repair. The evidence that the old constant DID admit M=21
// is kept separately, in the findings, by running the same program through a
// binary built from the pre-correction source.
//
// EACH VERDICT IS DISCRIMINATED. `hot` is a conjunction of three terms:
//     cold_ops == 0  &&  bind <= 6000  &&  n_vreg <= 32
// so a `kCold` that came from the register file or from a cold op would pass a
// naive test for entirely the wrong reason. Every case below pins the other
// two terms explicitly, so the only thing left moving is the deadline.
//
// Build (no build tree, no RTL) -- this file is NOT yet registered in
// tests/CMakeLists.txt, which is another packet's file this pass:
//
//   g++ -std=c++20 -O1 -Wall -Wextra -I reference/include -I runtime/include
//       -o zfield_plan_classify_directed.exe
//       tests/differential/zfield_plan_classify_directed.cpp
//       reference/src/zfield/zfield_plan.cpp
//
//   ./zfield_plan_classify_directed.exe
//
// (one line; the continuations are written without a trailing backslash on
// purpose -- a `\` at the end of a `//` line is a line splice and -Wcomment
// rejects it, which is a lint error in a file that must build -Wall clean.)

#include <cstdint>
#include <cstdio>
#include <vector>

#include "zfield/zfield.hpp"
#include "zfield/zfield_plan.hpp"

namespace {

int failures = 0;
int checks = 0;

#define CHECK(cond, msg)         \
  do {                           \
    ++checks;                    \
    if (!(cond)) {               \
      printf("FAIL: %s\n", msg); \
      ++failures;                \
    } else {                     \
      printf("ok: %s\n", msg);   \
    }                            \
  } while (0)

// The association deadline `finish_demand()` compares against. Kept here as a
// named constant so this test states the law it is testing rather than
// re-deriving it from the number it is checking.
constexpr uint32_t kDeadlineClocks = 6000;
constexpr uint32_t kExpectedGroups = 297;

// A program with exactly `n` VARYING vector uops and nothing else: two varying
// inputs (Earth's x and z) and `n` ADDs over them. OP_ADD is a lane-private
// VALU op, so it contributes to `vec_issue` and to NOTHING else -- no
// multiplier bank slot, no curve request, no distance request, no cold op.
// That makes `vec_issue` the binding term and `n` the per-group factor M.
zfield::Decoded issue_program(int n) {
  zfield::Decoded p;
  p.profile = 0;
  p.in_lanes.push_back(zfield::IoLane{"x", 0, 0, 0, 0});
  p.in_lanes.push_back(zfield::IoLane{"z", 0, 1, 0, 0});
  for (int i = 0; i < n; ++i) {
    zfield::Instr in{};
    in.op = zfield::OP_ADD;
    in.dst = (uint8_t)(2 + i);  // never aliases an input or an earlier dst
    in.a = 0;                   // x, varying
    in.b = 1;                   // z, varying
    in.c = 0;
    in.imm = 0;
    p.instrs.push_back(in);
  }
  // One output reading the last result, so the whole chain is live and
  // `compact_vregs()` cannot fold any of it away.
  p.out_lanes.push_back(zfield::IoLane{"h", 0, (uint8_t)(2 + n - 1), 0, 0});
  return p;
}

// Assert the shape of a plan so that a later verdict is attributable to the
// deadline alone. Without this, `kCold` from `n_vreg > 32` would read as a
// pass and the test would be blind to the thing it exists to watch.
void pin_shape(const zfield::Fplan& fp, uint32_t want_issue, const char* tag) {
  char msg[192];
  snprintf(msg, sizeof msg, "%s: binding term is vec_issue = %u (the factor M)", tag,
           fp.demand.vec_issue);
  CHECK(fp.demand.vec_issue == want_issue, msg);

  snprintf(msg, sizeof msg, "%s: no other service competes (vmul %u curve %u dist %u)", tag,
           fp.demand.vmul_slots, fp.demand.curve_req, fp.demand.dist_req);
  CHECK(fp.demand.vmul_slots == 0 && fp.demand.curve_req == 0 && fp.demand.dist_req == 0, msg);

  snprintf(msg, sizeof msg, "%s: cold_ops == 0, so the cold term cannot be the verdict", tag);
  CHECK(fp.demand.cold_ops == 0, msg);

  snprintf(msg, sizeof msg, "%s: n_vreg = %u <= 32, so the register term cannot be the verdict",
           tag, fp.demand.vreg_hwm);
  CHECK(fp.demand.vreg_hwm <= 32, msg);
}

}  // namespace

int main() {
  printf("== kGroups is the row-bounded count, not the flat one ==\n");
  {
    const zfield::Fplan fp = zfield::plan(issue_program(1), 0b11);
    char msg[192];
    snprintf(msg, sizeof msg,
             "vec_groups = %u (33 rows * ceil(33/4) = 297, NOT ceil(1089/4) = 273)",
             fp.demand.vec_groups);
    CHECK(fp.demand.vec_groups == kExpectedGroups, msg);
  }

  printf("== the admission boundary, both polarities ==\n");
  // M = 20: 20 * 297 = 5,940 <= 6,000. Admitted, and it is the shipped
  // programs' own factor -- they all bind on dist_req 1 * 20.
  {
    const zfield::Fplan fp = zfield::plan(issue_program(20), 0b11);
    pin_shape(fp, 20, "M=20");
    const uint32_t bind = 20u * kExpectedGroups;
    char msg[192];
    snprintf(msg, sizeof msg, "M=20: %u clocks <= %u deadline, so the plan is ADMITTED (hot)",
             bind, kDeadlineClocks);
    CHECK(bind <= kDeadlineClocks && fp.perf_class == zfield::PlanClass::kHot, msg);
  }

  // M = 21: 21 * 297 = 6,237 > 6,000. Refused. This is the whole
  // misclassification band -- under the stale 273 it computed 5,733 and was
  // admitted 237 clocks over its deadline.
  {
    const zfield::Fplan fp = zfield::plan(issue_program(21), 0b11);
    pin_shape(fp, 21, "M=21");
    const uint32_t bind = 21u * kExpectedGroups;
    char msg[192];
    snprintf(msg, sizeof msg, "M=21: %u clocks > %u deadline, so the plan is REFUSED (cold)", bind,
             kDeadlineClocks);
    CHECK(bind > kDeadlineClocks && fp.perf_class == zfield::PlanClass::kCold, msg);

    snprintf(msg, sizeof msg,
             "M=21: the stale 273 would have computed %u and ADMITTED it (the defect R156 names)",
             21u * 273u);
    CHECK(21u * 273u <= kDeadlineClocks, msg);
  }

  printf("== the band is exactly one step wide ==\n");
  {
    // Nothing below 20 may refuse and nothing above 21 may admit, or the
    // boundary has moved and every number in this file's header is stale.
    const zfield::Fplan lo = zfield::plan(issue_program(19), 0b11);
    CHECK(lo.perf_class == zfield::PlanClass::kHot, "M=19 still admits (5,643 clocks)");
    const zfield::Fplan hi = zfield::plan(issue_program(22), 0b11);
    CHECK(hi.perf_class == zfield::PlanClass::kCold, "M=22 still refuses (6,534 clocks)");
  }

  printf("[zfield_plan_classify_directed] %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
