// zixx_striketip — committed 60 Hz attack trajectory/weapon validator.
//
// This is comparison-side evidence, never an art generator.  It consumes the
// exact plans, shared phase descriptors and runtime midpoint decoder used by
// the reel.  Every showcased variant must reach one event, preserve its whole
// turn count, hold the embedded spear bit-constant, extract completely along
// the committed aim, delay recoil until clear, and recover to the rest pose.
#include "zref/zref_creature.hpp"
#include "zrender/internal.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

namespace zc = zref::creature;
constexpr int32_t fxm(int64_t milli) {
  return static_cast<int32_t>((milli * 65536 + (milli >= 0 ? 500 : -500)) /
                              1000);
}
#include "zixxtrixx.h"

namespace {

int32_t to_mm(int64_t fx) { return static_cast<int32_t>(fx * 1000 >> 16); }

const zc::Clip* find_slot(const zc::CreatureType& type, uint16_t slot) {
  for (const zc::Clip& c : type.bank.clips)
    if (c.slot_id == slot) return &c;
  return nullptr;
}

struct Point3 {
  int32_t x = 0;
  int32_t y = 0;
  int32_t z = 0;
};

struct WeaponSample {
  Point3 nose;
  Point3 tip;
};

WeaponSample weapon_sample(const zc::CreatureType& type, const zc::Clip& clip,
                           int tick) {
  const uint16_t key = static_cast<uint16_t>(tick / 2);
  const uint8_t sub = static_cast<uint8_t>(tick & 1);
  std::array<zc::mat3x4fx, zc::kMaxBones> pose;
  zc::decode_pose(type, clip, key, pose, nullptr, sub);
  WeaponSample out;
  {
    const zixx::Bind bd = zixx::head_station_bind(0);
    const zc::SkinVertex v{-fxm(zixx::station_x(0)), fxm(zixx::kBodyY), 0,
                           bd.b0, bd.b1, bd.w0, 0, 0};
    int32_t x = 0, y = 0, z = 0;
    zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
    out.nose = {to_mm(x), to_mm(y), to_mm(z)};
  }
  {
    // One physical blade endpoint.  The actual victim-mesh intersection gate
    // in zhao_reel uses the full base-to-tip segment and both rendered meshes.
    const zc::SkinVertex v{-fxm(zixx::station_x(56) + zixx::kBladeLen),
                           fxm(zixx::kBodyY), 0, zixx::kBBladeL2,
                           zixx::kBBladeL2, 64, 0, 0};
    int32_t x = 0, y = 0, z = 0;
    zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
    out.tip = {to_mm(x), to_mm(y), to_mm(z)};
  }
  return out;
}

bool point_equal(const Point3& a, const Point3& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

int64_t dist2(const Point3& a, const Point3& b) {
  const int64_t dx = a.x - b.x;
  const int64_t dy = a.y - b.y;
  const int64_t dz = a.z - b.z;
  return dx * dx + dy * dy + dz * dz;
}

bool key_pose_equal(const zc::Clip& c, int a, int b, int bones,
                    bool root_too) {
  if (a < 0 || b < 0 || a >= c.frame_count || b >= c.frame_count) return false;
  if (root_too &&
      std::memcmp(&c.root[static_cast<size_t>(a) * 3],
                  &c.root[static_cast<size_t>(b) * 3],
                  3 * sizeof(c.root[0])) != 0)
    return false;
  return std::memcmp(&c.quats[static_cast<size_t>(a) * bones],
                     &c.quats[static_cast<size_t>(b) * bones],
                     static_cast<size_t>(bones) * sizeof(c.quats[0])) == 0;
}

struct Validation {
  int failures = 0;
  void require(bool ok, uint16_t slot, const char* what) {
    if (!ok) {
      ++failures;
      std::printf("  ** FAIL slot %u: %s\n", slot, what);
    }
  }
};

void validate_variant(const zc::CreatureType& type, uint16_t slot,
                      const char* name, Validation& v) {
  const zc::Clip* cp = find_slot(type, slot);
  v.require(cp != nullptr, slot, "clip missing");
  if (!cp) return;
  const zc::Clip& clip = *cp;
  const bool target_hit = zixx::zixx_variant_air_hit(slot);
  const zc::AttackPlan raw_plan = zixx::zixx_variant_plan(slot);
  const zc::AttackPlan plan = zixx::zixx_orient_variant_spin(raw_plan);
  const zixx::AttackVariantPhases phase =
      zixx::zixx_attack_variant_phases(plan, target_hit);

  v.require(clip.frame_count == phase.frame_count, slot,
            "duration differs from shared phase table");
  v.require(clip.frame_count > 1 &&
                2 * (static_cast<int>(clip.frame_count) - 1) + 1 <= 65535,
            slot, "presentation tick range exceeds fixed clip limits");
  int attack_events = 0;
  int event_key = -1;
  for (const zc::ClipEvent& ev : clip.events) {
    if (ev.event == zc::kEvAttack) {
      ++attack_events;
      event_key = ev.frame;
    }
  }
  v.require(attack_events == 1 && event_key == phase.impact, slot,
            "attack event missing, duplicated, or phase-drifted");

  // Root choreography and unwrapped rotation are checked before quaternion
  // quantisation.  Whole turns finish at coil_end; only the spear alignment
  // fraction is added monotonically during unroll.
  int32_t apex = INT32_MIN;
  int32_t previous_theta = 0;
  int32_t max_theta_step = 0;
  bool theta_monotonic = true;
  int32_t max_root_step = 0;
  zixx::ChoreoSample previous = zixx::zixx_plan_sample(plan, 0);
  for (int k = 0; k <= phase.impact; ++k) {
    const zixx::ChoreoSample s = zixx::zixx_plan_sample(plan, k);
    apex = std::max(apex, s.y_mm);
    if (k > 0) {
      if (s.theta < previous_theta) theta_monotonic = false;
      max_theta_step = std::max(max_theta_step, s.theta - previous_theta);
      const int64_t dx = s.x_mm - previous.x_mm;
      const int64_t dy = s.y_mm - previous.y_mm;
      max_root_step = std::max(
          max_root_step,
          static_cast<int32_t>(zref::isqrt_u64(
              static_cast<uint64_t>(dx * dx + dy * dy))));
    }
    previous_theta = s.theta;
    previous = s;
  }
  const zixx::ChoreoSample coil_end =
      zixx::zixx_plan_sample(plan, phase.coil_end);
  const zixx::ChoreoSample impact =
      zixx::zixx_plan_sample(plan, phase.impact);
  const int requested_turns = raw_plan.spin_mturns / 1000;
  v.require(apex == raw_plan.apex_mm, slot,
            "authored root trajectory does not reach its exact apex");
  v.require(theta_monotonic &&
                coil_end.theta == requested_turns * 65536 &&
                (coil_end.theta & 0xFFFF) == 0 &&
                impact.theta == static_cast<int32_t>(
                    static_cast<int64_t>(plan.spin_mturns) * 65536 / 1000),
            slot, "turn count, wrap, or final aim alignment drifted");
  v.require(max_theta_step <= 16384, slot,
            "unwrapped rotation advances by more than a quarter turn per key");
  v.require(max_root_step <= 6000, slot,
            "root trajectory contains an implausible one-key discontinuity");

  const int impact_tick = 2 * phase.impact;
  const WeaponSample entered = weapon_sample(type, clip, impact_tick);
  const Point3 intercept{raw_plan.intercept_x_mm,
                         raw_plan.intercept_y_mm, 0};
  const int32_t gap = static_cast<int32_t>(zref::isqrt_u64(
      static_cast<uint64_t>(dist2(entered.tip, intercept))));
  const int32_t reach = static_cast<int32_t>(zref::isqrt_u64(
      static_cast<uint64_t>(dist2(entered.tip, entered.nose))));
  v.require(gap >= 390 && gap <= 455, slot,
            "weapon endpoint left the authored intercept convention");
  v.require(reach >= 3800 && reach <= 4050, slot,
            "nose-to-blade reach left the complete-profile convention");

  // Stable insertion: every presented hold sample is exactly the same weapon
  // pose, and every authored key is bit-identical through extract_begin (whose
  // first extraction parameter is deliberately zero).
  bool stable_hold = true;
  int first_hold_mismatch = -1;
  WeaponSample first_hold_sample{};
  for (int tick = impact_tick; tick <= 2 * phase.extract_begin; ++tick) {
    const WeaponSample s = weapon_sample(type, clip, tick);
    if (!point_equal(s.nose, entered.nose) || !point_equal(s.tip, entered.tip)) {
      stable_hold = false;
      if (first_hold_mismatch < 0) {
        first_hold_mismatch = tick;
        first_hold_sample = s;
      }
    }
  }
  bool stable_root_keys = true;
  bool stable_quat_keys = true;
  int first_key_mismatch = -1;
  for (int k = phase.impact; k <= phase.extract_begin; ++k) {
    const bool root_equal =
        std::memcmp(&clip.root[static_cast<size_t>(phase.impact) * 3],
                    &clip.root[static_cast<size_t>(k) * 3],
                    3 * sizeof(clip.root[0])) == 0;
    const bool quats_equal =
        std::memcmp(&clip.quats[static_cast<size_t>(phase.impact) *
                                    type.bank.bone_count],
                    &clip.quats[static_cast<size_t>(k) *
                                    type.bank.bone_count],
                    static_cast<size_t>(type.bank.bone_count) *
                        sizeof(clip.quats[0])) == 0;
    stable_root_keys = stable_root_keys && root_equal;
    stable_quat_keys = stable_quat_keys && quats_equal;
    if ((!root_equal || !quats_equal) && first_key_mismatch < 0)
      first_key_mismatch = k;
  }
  if (!stable_hold || !stable_root_keys || !stable_quat_keys) {
    std::printf(
        "  hold diagnostic slot %u: first presentation mismatch %d%s "
        "nose delta (%d,%d,%d) tip delta (%d,%d,%d); first key mismatch "
        "%d (root=%s quats=%s)\n",
        slot,
        first_hold_mismatch >= 0 ? first_hold_mismatch / 2 : -1,
        first_hold_mismatch >= 0 && (first_hold_mismatch & 1) ? ".5" : "",
        first_hold_sample.nose.x - entered.nose.x,
        first_hold_sample.nose.y - entered.nose.y,
        first_hold_sample.nose.z - entered.nose.z,
        first_hold_sample.tip.x - entered.tip.x,
        first_hold_sample.tip.y - entered.tip.y,
        first_hold_sample.tip.z - entered.tip.z, first_key_mismatch,
        stable_root_keys ? "constant" : "changed",
        stable_quat_keys ? "constant" : "changed");
  }
  v.require(stable_hold && stable_root_keys && stable_quat_keys, slot,
            "target/ground hold is not bit-constant at 60 Hz");

  // Extraction begins only after the hold.  The constant spear pose means its
  // endpoint is an exact witness for root travel along -aim.
  const int64_t aim_dx = raw_plan.intercept_x_mm - raw_plan.apex_fwd_mm;
  const int64_t aim_dy = raw_plan.intercept_y_mm - raw_plan.apex_mm;
  int32_t aim_len = static_cast<int32_t>(zref::isqrt_u64(
      static_cast<uint64_t>(aim_dx * aim_dx + aim_dy * aim_dy)));
  if (aim_len < 1) aim_len = 1;
  int32_t prior_projection = 0;
  bool monotonic_extract = true;
  const int extraction_last_tick = 2 * (phase.recoil_begin - 1);
  Point3 extracted_tip = entered.tip;
  for (int tick = 2 * phase.extract_begin;
       tick <= extraction_last_tick; ++tick) {
    const WeaponSample s = weapon_sample(type, clip, tick);
    const int64_t dx = s.tip.x - entered.tip.x;
    const int64_t dy = s.tip.y - entered.tip.y;
    const int32_t projection = static_cast<int32_t>(
        -(dx * aim_dx + dy * aim_dy) / aim_len);
    if (projection < prior_projection) monotonic_extract = false;
    prior_projection = projection;
    extracted_tip = s.tip;
  }
  const int32_t extracted_distance = prior_projection;
  v.require(monotonic_extract &&
                extracted_distance >= zixx::kAtkExtractionMm - 3 &&
                extracted_distance <= zixx::kAtkExtractionMm + 3,
            slot, "extraction is not monotonic and at its authored distance along aim");
  // The root cannot recoil before the extraction's terminal key.  Recoil key
  // zero shares that root exactly; body recoil starts only on/after this key.
  const size_t a = static_cast<size_t>(phase.recoil_begin - 1) * 3;
  const size_t b = static_cast<size_t>(phase.recoil_begin) * 3;
  v.require(std::memcmp(&clip.root[a], &clip.root[b],
                        3 * sizeof(clip.root[0])) == 0,
            slot, "recoil root begins before extraction is complete");

  // The body must finish in the canonical rest rig, on terrain, with no
  // quaternion residue from the many wraps.  World X may remain beside the
  // target by design.
  zixx::Rig canonical_rest;
  zixx::rest_rig(canonical_rest);
  const int last = phase.last_key;
  const bool rest_quats =
      std::memcmp(&clip.quats[static_cast<size_t>(last) *
                              type.bank.bone_count],
                  canonical_rest.q,
                  static_cast<size_t>(type.bank.bone_count) *
                      sizeof(clip.quats[0])) == 0;
  v.require(rest_quats && to_mm(clip.root[last * 3 + 1]) == 0 &&
                clip.root[last * 3 + 2] == 0,
            slot, "final recovery does not restore rest pose on terrain");

  std::printf(
      "%s: %d keys/%d ticks; impact %d; root apex %d mm; %d whole turns "
      "+ aim fraction; max root step %d mm; tip (%d,%d), gap %d mm; "
      "extract %d mm; final tip (%d,%d)\n",
      name, clip.frame_count, 2 * (static_cast<int>(clip.frame_count) - 1) + 1,
      phase.impact, apex, requested_turns, max_root_step, entered.tip.x,
      entered.tip.y, gap, extracted_distance, extracted_tip.x,
      extracted_tip.y);
}

}  // namespace

int main() {
  const zc::CreatureType& type = zixx::type();
  Validation validation;
  validate_variant(type, zixx::kSlotAtkDummy, "salto-dummy",
                   validation);
  validate_variant(type, zixx::kSlotAtkFly, "salto-fly", validation);
  validate_variant(type, zixx::kSlotAtkSix, "salto-six", validation);
  validate_variant(type, zixx::kSlotAtkNine, "salto-nine", validation);

  const zc::AttackPlan six = zixx::zixx_variant_plan(zixx::kSlotAtkSix);
  const zc::AttackPlan nine = zixx::zixx_variant_plan(zixx::kSlotAtkNine);
  validation.require(six.apex_mm == 12000 && nine.apex_mm == 24000 &&
                         nine.apex_mm == 2 * six.apex_mm,
                     zixx::kSlotAtkNine,
                     "matched six/nine apex is not exact 1:2 root height");
  validation.require(six.spin_mturns == 6000 && nine.spin_mturns == 9000,
                     zixx::kSlotAtkNine,
                     "matched variants are not programmed for six/nine turns");

  if (validation.failures == 0) {
    std::printf("STRIKETIP: PASS — duration, apex, turns/wrap, continuity, "
                "hold, extraction, delayed recoil and recovery\n");
    return 0;
  }
  std::printf("STRIKETIP: FAIL — %d assertion(s)\n",
              validation.failures);
  return 1;
}
