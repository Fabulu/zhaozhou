// legacy_material_v2_oracle.hpp -- TEST-ONLY oracle for superseded combiners.
//
// This header intentionally preserves the arithmetic implemented by
// zhao_texture_material_combine_v1/v2 and the unchanged old texture island.
// It is NOT Packet-B product law and must never be included from reference/,
// fpga/, or a Packet-B test.  The sole product authority is owner ruling R9 and
// reference/include/zref/zref_material.hpp after the coordinator's atomic R9
// update.
//
// Deliberately preserved stale behaviours:
//   * count zero bypasses every recipe to admitted base colour;
//   * excess sample counts are accepted (only missing counts are refused);
//   * recipes 1--4 transform alpha with their colour operation;
//   * MASK is a nonzero-alpha binary gate;
//   * MODULATE2X unit-rounds and then doubles;
//   * both terrain recipes use a unit first layer;
//   * the old island may supply AUX in place of sample 2 outside this scalar.
//
// Keeping these rules here makes old-island compatibility tests honest without
// allowing agreement between two stale implementations to outvote R9.
#ifndef ZHAO_TESTS_TEXTURE_LEGACY_MATERIAL_V2_ORACLE_HPP
#define ZHAO_TESTS_TEXTURE_LEGACY_MATERIAL_V2_ORACLE_HPP

#include <cstdint>

#include "zref/zref_fixp.hpp"

namespace zref {
namespace legacy_material_v2 {

enum Recipe : uint8_t {
  kPassthru = 0,
  kModulate = 1,
  kModulate2x = 2,
  kLerp = 3,
  kAddSat = 4,
  kMask = 5,
  kTerrainDetailLight = 6,
  kTerrainDetailMask = 7,
  kRecipeCount = 8
};

struct Sample {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;
};

struct Ledger {
  uint32_t refused_unknown_recipe = 0;
  uint32_t refused_missing_sample = 0;
  uint32_t saturated_add = 0;
  uint32_t saturated_mul2x = 0;
};

struct Out {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;
  uint16_t frag_tag = 0;
  bool refused = false;
};

constexpr uint8_t samples_required(uint8_t recipe) {
  if (recipe == kPassthru) return 1;
  if (recipe == kTerrainDetailLight || recipe == kTerrainDetailMask) return 3;
  return 2;
}

constexpr uint8_t product_jobs(uint8_t recipe) {
  switch (recipe) {
    case kPassthru:
    case kAddSat:
    case kMask:
      return 0;
    case kModulate:
    case kModulate2x:
    case kLerp:
    case kTerrainDetailMask:
      return 4;
    case kTerrainDetailLight:
      return 6;
    default:
      return 0;
  }
}

namespace detail {

constexpr uint8_t add_sat(uint8_t a, uint8_t b, bool* saturated) {
  const uint32_t sum = static_cast<uint32_t>(a) + b;
  if (sum > 255u) {
    *saturated = true;
    return 255;
  }
  return static_cast<uint8_t>(sum);
}

// Superseded V2 law: unit round first, then double and saturate.
constexpr uint8_t rounded_unit_then_double(uint8_t a, uint8_t b, bool* saturated) {
  const uint32_t doubled =
      static_cast<uint32_t>(zref::unit_mul(unit8{a}, unit8{b})) * 2u;
  if (doubled > 255u) {
    *saturated = true;
    return 255;
  }
  return static_cast<uint8_t>(doubled);
}

// Superseded V2 darkening rule: round a positive magnitude, then reapply sign.
constexpr uint8_t magnitude_lerp(uint8_t a, uint8_t b, uint8_t weight) {
  const int32_t difference = static_cast<int32_t>(b) - static_cast<int32_t>(a);
  const uint32_t magnitude =
      static_cast<uint32_t>(difference < 0 ? -difference : difference);
  const uint32_t scaled =
      (magnitude * static_cast<uint32_t>(weight) + 128u) >> 8;
  const int32_t result = static_cast<int32_t>(a) +
      (difference < 0 ? -static_cast<int32_t>(scaled)
                      : static_cast<int32_t>(scaled));
  return static_cast<uint8_t>(result < 0 ? 0 : (result > 255 ? 255 : result));
}

}  // namespace detail

inline Out combine(uint8_t recipe, uint8_t weight, const Sample* samples,
                   uint8_t count, Sample base, uint16_t frag_tag,
                   Ledger* ledger = nullptr) {
  Out out;
  out.frag_tag = frag_tag;

  // Preserved intentionally: V2 bypassed before validating the recipe.
  if (count == 0) {
    out.r = base.r;
    out.g = base.g;
    out.b = base.b;
    out.a = base.a;
    return out;
  }

  if (recipe >= kRecipeCount) {
    if (ledger) ++ledger->refused_unknown_recipe;
    out.refused = true;
    return out;
  }

  // Preserved intentionally: V2 accepted excess counts.
  if (count < samples_required(recipe)) {
    if (ledger) ++ledger->refused_missing_sample;
    out.refused = true;
    return out;
  }

  const Sample& s0 = samples[0];
  const Sample& s1 = count > 1 ? samples[1] : samples[0];
  const Sample& s2 = count > 2 ? samples[2] : samples[0];

  switch (recipe) {
    case kPassthru:
      out.r = s0.r;
      out.g = s0.g;
      out.b = s0.b;
      out.a = s0.a;
      break;

    case kModulate:
      out.r = zref::unit_mul(unit8{s0.r}, unit8{s1.r});
      out.g = zref::unit_mul(unit8{s0.g}, unit8{s1.g});
      out.b = zref::unit_mul(unit8{s0.b}, unit8{s1.b});
      out.a = zref::unit_mul(unit8{s0.a}, unit8{s1.a});
      break;

    case kModulate2x: {
      bool saturated = false;
      out.r = detail::rounded_unit_then_double(s0.r, s1.r, &saturated);
      out.g = detail::rounded_unit_then_double(s0.g, s1.g, &saturated);
      out.b = detail::rounded_unit_then_double(s0.b, s1.b, &saturated);
      out.a = detail::rounded_unit_then_double(s0.a, s1.a, &saturated);
      if (saturated && ledger) ++ledger->saturated_mul2x;
      break;
    }

    case kLerp:
      out.r = detail::magnitude_lerp(s0.r, s1.r, weight);
      out.g = detail::magnitude_lerp(s0.g, s1.g, weight);
      out.b = detail::magnitude_lerp(s0.b, s1.b, weight);
      out.a = detail::magnitude_lerp(s0.a, s1.a, weight);
      break;

    case kAddSat: {
      bool saturated = false;
      out.r = detail::add_sat(s0.r, s1.r, &saturated);
      out.g = detail::add_sat(s0.g, s1.g, &saturated);
      out.b = detail::add_sat(s0.b, s1.b, &saturated);
      out.a = detail::add_sat(s0.a, s1.a, &saturated);
      if (saturated && ledger) ++ledger->saturated_add;
      break;
    }

    case kMask:
      if (s1.a != 0) {
        out.r = s0.r;
        out.g = s0.g;
        out.b = s0.b;
        out.a = s0.a;
      }
      break;

    case kTerrainDetailLight: {
      const uint8_t first_r = zref::unit_mul(unit8{s0.r}, unit8{s1.r});
      const uint8_t first_g = zref::unit_mul(unit8{s0.g}, unit8{s1.g});
      const uint8_t first_b = zref::unit_mul(unit8{s0.b}, unit8{s1.b});
      out.r = zref::unit_mul(unit8{first_r}, unit8{s2.r});
      out.g = zref::unit_mul(unit8{first_g}, unit8{s2.g});
      out.b = zref::unit_mul(unit8{first_b}, unit8{s2.b});
      out.a = s0.a;
      break;
    }

    case kTerrainDetailMask:
      out.r = zref::unit_mul(unit8{s0.r}, unit8{s1.r});
      out.g = zref::unit_mul(unit8{s0.g}, unit8{s1.g});
      out.b = zref::unit_mul(unit8{s0.b}, unit8{s1.b});
      out.a = zref::unit_mul(unit8{s0.a}, unit8{s2.a});
      break;

    default:
      if (ledger) ++ledger->refused_unknown_recipe;
      out.refused = true;
      break;
  }

  return out;
}

}  // namespace legacy_material_v2
}  // namespace zref

#endif  // ZHAO_TESTS_TEXTURE_LEGACY_MATERIAL_V2_ORACLE_HPP
