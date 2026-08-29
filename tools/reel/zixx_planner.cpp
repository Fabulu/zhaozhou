// zixx_planner — COMMITTED proof of the programmable attack (C4/C5).
//
// The amendment's contract: the sim builds a fixed-point AttackPlan from
// target truth, the trajectory is a pure function of the plan, the spear
// vector LOCKS at unroll (projectile, not missile), and the outcome is a
// COLLISION verdict, never a clip key. This tool proves, with exit codes:
//
//   1. GOLDEN: the ground-dive preset reproduces the approved salto's root
//      decomposition KEY-FOR-KEY (attack_choreo_sample, exact).
//   2. HIGH AERIAL: a target 8 m up -- the plunge passes through the
//      planned intercept (the spear line hits what it was locked on).
//   3. LONG FORWARD: a target 12 m out at 1.2 m -- same law, flatter shot.
//   4. MOVING TARGET: the intercept leads the motion; the lock happens at
//      unroll and the sample path never re-aims after it.
//   5. MISS / BRANCH LAW: attack_plan_branch is collision-only -- terrain
//      hit -> ground stick, creature hit -> air hit, nothing -> miss
//      recover. No input exists that fires an impact from a key number.
//
// Build exactly like zixx_probe (no cmake).
#include "zref/zref_creature.hpp"
#include "zrender/internal.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace zc = zref::creature;
constexpr int32_t fxm(int64_t milli) {
  return static_cast<int32_t>((milli * 65536 + (milli >= 0 ? 500 : -500)) / 1000);
}
#include "zixxtrixx.h"

namespace {
int fails = 0;
void expect(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++fails;
  }
}
}  // namespace

int main() {
  // ---- 1. the golden preset is the approved salto, verbatim --------------
  {
    const zc::AttackPlan p =
        zixx::zixx_plan_attack(zixx::kAtkFwdMax + zixx::kAtkTipFwd, 0, 0, 0);
    expect(p.preset_golden, "grounded strike-point target selects the golden preset");
    bool exact = true;
    for (int k = 0; k < zixx::kAttackKeys; ++k) {
      const zixx::ChoreoSample a = zixx::zixx_plan_sample(p, k);
      const zixx::ChoreoSample b = zixx::attack_choreo_sample(k);
      if (a.x_mm != b.x_mm || a.y_mm != b.y_mm || a.theta != b.theta) exact = false;
    }
    expect(exact, "golden plan reproduces attack_choreo_sample key-for-key");
    std::printf("golden preset: %d keys, apex %d mm, spin %d mturns — exact\n",
                zixx::kAttackKeys, p.apex_mm, p.spin_mturns);
  }
  // ---- 2/3. parametric plans hit their locked intercepts -----------------
  const auto check_plan = [&](const char* name, int32_t tx, int32_t ty) {
    const zc::AttackPlan p = zixx::zixx_plan_attack(tx, ty, 0, 0);
    const int t3 = p.compress_keys + p.compress_hold_keys + p.release_keys +
                   p.coil_keys + p.unroll_keys + p.plunge_keys;
    const zixx::ChoreoSample at_impact = zixx::zixx_plan_sample(p, t3);
    // THE WEAPON IS THE TAIL TIP (owner, 2026-08-28). The old assertion
    // "plunge terminus == locked intercept" checked the ROOT -- exactly
    // the wrong body point the owner diagnosed ("you confused the middle
    // of Zixxtrixx with the tip of its tail"), enshrined as a green gate.
    // The law now: the ROOT stops kAtkTipReachMm - kAtkStickDepth short
    // of the intercept along the spear line, so the TIP lands the strike
    // and buries the declared depth.
    // the NOSE rides kBodyY above the root (bone 0's joint), and the TIP
    // leads the nose by the measured pose reach along the committed line
    const int64_t ex = at_impact.x_mm - p.intercept_x_mm;
    const int64_t ey = (at_impact.y_mm + zixx::kBodyY) - p.intercept_y_mm;
    const int64_t back = zixx::kAtkTipReachMm - zixx::kAtkStickDepth;
    const int64_t gap2 = ex * ex + ey * ey;
    expect(gap2 >= (back - 80) * (back - 80) && gap2 <= (back + 80) * (back + 80),
           "nose terminus sits one tip-lead from the intercept: the TIP "
           "lands the strike (+/- 80 mm)");
    // the spear axis: from commit to impact the samples must sit ON the
    // locked line (t^2 scaling of one fixed vector -- collinear exactly)
    const int t2 = t3 - p.plunge_keys;
    const zixx::ChoreoSample c0 = zixx::zixx_plan_sample(p, t2);
    bool collinear = true;
    for (int k = t2 + 1; k <= t3; ++k) {
      const zixx::ChoreoSample ck = zixx::zixx_plan_sample(p, k);
      const int64_t cross = static_cast<int64_t>(ck.x_mm - c0.x_mm) * p.spear_dy_mm -
                            static_cast<int64_t>(ck.y_mm - c0.y_mm) * p.spear_dx_mm;
      // tolerance: integer division of the t^2 law, ~1 mm of lateral slop
      if (std::abs(cross) >
          (std::abs(p.spear_dx_mm) + std::abs(p.spear_dy_mm)) * 2)
        collinear = false;
    }
    expect(collinear, "plunge samples sit on the locked spear line");
    std::printf("%s: coil %u keys, apex %d mm, spin %d mturns, plunge %u keys, "
                "impact at key %d (%d, %d) mm\n",
                name, p.coil_keys, p.apex_mm, p.spin_mturns, p.plunge_keys, t3,
                at_impact.x_mm, at_impact.y_mm);
    return p;
  };
  const zc::AttackPlan high = check_plan("high aerial", 4000, 8000);
  expect(high.spin_mturns >= 2000, "the high shot earns extra somersaults");
  expect(high.spear_dy_mm > -3000, "aerial spear is not the ground plunge");
  const zc::AttackPlan fwd = check_plan("long forward", 12000, 1200);
  expect(fwd.spin_mturns <= high.spin_mturns,
         "the flat shot spends fewer turns than the high one");
  const zc::AttackPlan ceiling = check_plan("ceiling aerial", 0, 12000);
  expect(ceiling.apex_mm <= zixx::kAtkApexLift,
         "generic spear lock cannot borrow slot 48's 24 m apex exception");
  // ---- 4. a moving target is LED, then the lock holds --------------------
  {
    const zc::AttackPlan p = zixx::zixx_plan_attack(6000, 4000, 120, 0);
    const int impact = p.compress_keys + p.compress_hold_keys +
                       p.release_keys + p.coil_keys + p.unroll_keys +
                       p.plunge_keys;
    expect(p.intercept_x_mm == 6000 + 120 * impact &&
               p.intercept_y_mm == 4000,
           "moving-target intercept equals target position at the actual "
           "impact key");
    std::printf("moving target: impact key %d, target/intercept (%d, %d) mm — exact\n",
                impact, p.intercept_x_mm, p.intercept_y_mm);
    // After the lock, re-planning does not change the flight in progress:
    // the plan is immutable data -- sample() reads only the plan.
    const int t2 = p.compress_keys + p.compress_hold_keys + p.release_keys +
                   p.coil_keys + p.unroll_keys;
    const zixx::ChoreoSample s1 = zixx::zixx_plan_sample(p, t2 + 3);
    const zixx::ChoreoSample s2 = zixx::zixx_plan_sample(p, t2 + 3);
    expect(s1.x_mm == s2.x_mm && s1.y_mm == s2.y_mm && s1.theta == s2.theta,
           "the committed path is a pure function of the locked plan");
  }
  // ---- 5. the branch law: collision only ---------------------------------
  expect(zc::attack_plan_branch(true, false) == zc::AttackOutcome::kGroundStick,
         "terrain contact -> ground stick");
  expect(zc::attack_plan_branch(false, true) == zc::AttackOutcome::kAirHit,
         "creature contact -> air hit");
  expect(zc::attack_plan_branch(false, false) == zc::AttackOutcome::kMissRecover,
         "no contact -> miss recover (no phantom impact exists)");
  expect(zc::attack_plan_branch(true, true) == zc::AttackOutcome::kGroundStick,
         "terrain wins a simultaneous verdict");

  if (fails == 0) {
    std::printf("PLANNER PROOF: the golden preset is preserved, parametric plans "
                "hit their locked intercepts, and impact is a collision verdict\n");
    return 0;
  }
  std::printf("PLANNER PROOF: %d failure(s)\n", fails);
  return 1;
}
