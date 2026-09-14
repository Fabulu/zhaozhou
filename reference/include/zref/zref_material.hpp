// zref_material.hpp -- Packet-B TEXTURE.COMBINE scalar authority.
//
// MATERIAL_RECIPE_VERSION = 1.  The arithmetic and source-reduction law are
// frozen by reports/MATERIAL_ARCHITECTURE.md owner ruling R9 and
// reports/SHELL-TEXTURE-V3-COMPOSITION-ARCHITECTURE-20260913.md ss3.5.
// These are Zhaozhou-native recipes, not a Sacrifice-exact claim.
//
// The old V1/V2 combiner/island arithmetic is intentionally quarantined in
// tests/texture/legacy_material_v2_oracle.hpp.  Agreement with that test-only
// oracle cannot override this file.
#ifndef ZREF_MATERIAL_HPP
#define ZREF_MATERIAL_HPP

#include <cstdint>

#include "zref_fixp.hpp"

namespace zref {
namespace material {

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

// Typed TMU plane: {status8,raw_index8,alpha8,RGB24} in hardware.  Field order
// keeps historical four-field aggregate initialisers source-compatible.
struct Sample {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;
  uint8_t raw_index = 0;
  uint8_t status = 0;
};

// Typed AUX plane.  Packet B validates tag/strength but the material combiner
// consumes only status; retaining all three fields lets tests prove the other
// bytes are deliberately unconsumed rather than accidentally unavailable.
struct Aux {
  uint8_t status = 0;
  uint8_t tag = 0;
  uint8_t strength = 0;
};

struct Ledger {
  uint32_t refused_unknown_recipe = 0;
  uint32_t refused_count_mismatch = 0;
  uint32_t saturated_add = 0;
  uint32_t saturated_mul2x = 0;
};

struct Out {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;
  uint8_t raw_index = 0;
  uint8_t status = 0;
  uint16_t frag_tag = 0;
  bool refused = false;  // exact compatibility view: status bit 0
};

// Nominal nonzero count used by MATERIAL.RESOLVE fixtures.  Acceptance is
// governed by count_legal(), because PASSTHRU also admits count zero and excess
// counts are malformed rather than ignored.
constexpr uint8_t samples_required(uint8_t recipe) {
  if (recipe == kPassthru) return 1;
  if (recipe == kTerrainDetailLight || recipe == kTerrainDetailMask) return 3;
  return 2;
}

constexpr bool count_legal(uint8_t recipe, uint8_t count) {
  if (recipe >= kRecipeCount || count > 3) return false;
  if (recipe == kPassthru) return count == 0 || count == 1;
  if (recipe == kTerrainDetailLight || recipe == kTerrainDetailMask)
    return count == 3;
  return count == 2;
}

// Paired physical phase cadence, recipe IDs 0..7 = 1/2/2/2/1/1/3/2.
// A malformed material or terminal source error is collapsed to one loud phase
// by hardware; this helper states the legal-recipe schedule.
constexpr uint8_t phases_required(uint8_t recipe) {
  switch (recipe) {
    case kModulate:
    case kModulate2x:
    case kLerp:
    case kTerrainDetailMask:
      return 2;
    case kTerrainDetailLight:
      return 3;
    default:
      return 1;
  }
}

// Meaningful product jobs, recipe IDs 0..7 = 0/3/3/3/0/1/6/4.
constexpr uint8_t product_jobs(uint8_t recipe) {
  switch (recipe) {
    case kModulate:
    case kModulate2x:
    case kLerp:
      return 3;
    case kMask:
      return 1;
    case kTerrainDetailLight:
      return 6;
    case kTerrainDetailMask:
      return 4;
    default:
      return 0;
  }
}

namespace detail {

constexpr uint8_t add_sat8(uint8_t a, uint8_t b, bool* saturated) {
  const uint32_t sum = static_cast<uint32_t>(a) + b;
  if (sum > 255u) {
    *saturated = true;
    return 255;
  }
  return static_cast<uint8_t>(sum);
}

// R9 single-round MODULATE2X: never unit-round and then double.
constexpr uint8_t modulate2x8(uint8_t a, uint8_t b, bool* saturated) {
  const uint32_t value =
      (static_cast<uint32_t>(a) * static_cast<uint32_t>(b) + 64u) >> 7;
  if (value > 255u) {
    *saturated = true;
    return 255;
  }
  return static_cast<uint8_t>(value);
}

// Portable arithmetic-right-shift-by-eight for the bounded signed numerator.
// C++ integer division truncates toward zero, so the negative arm explicitly
// implements floor division, matching SystemVerilog >>>.
constexpr int32_t floor_div_256(int32_t value) {
  return value >= 0 ? value / 256 : -((-value + 255) / 256);
}

// R9: sat_u8(a + (((b-a)*weight + 128) >>> 8)); ties toward +infinity.
constexpr uint8_t lerp8(uint8_t a, uint8_t b, uint8_t weight) {
  const int32_t difference = static_cast<int32_t>(b) - a;
  const int32_t scaled = floor_div_256(
      difference * static_cast<int32_t>(weight) + 128);
  const int32_t value = static_cast<int32_t>(a) + scaled;
  return static_cast<uint8_t>(value < 0 ? 0 : (value > 255 ? 255 : value));
}

constexpr uint8_t required_status(const Sample* samples, uint8_t count,
                                  bool aux_required, Aux aux) {
  uint8_t status = 0;
  if (count >= 1) status = static_cast<uint8_t>(status | samples[0].status);
  if (count >= 2) status = static_cast<uint8_t>(status | samples[1].status);
  if (count >= 3) status = static_cast<uint8_t>(status | samples[2].status);
  if (aux_required) status = static_cast<uint8_t>(status | aux.status);
  return status;
}

}  // namespace detail

/**
 * Packet-B scalar material law.
 *
 * Existing seven-argument callers remain valid.  Packet-B callers append the
 * independently required AUX flag and typed AUX terminal plane.
 */
inline Out combine(uint8_t recipe, uint8_t weight, const Sample* samples,
                   uint8_t count, Sample base, uint16_t frag_tag,
                   Ledger* ledger = nullptr, bool aux_required = false,
                   Aux aux = {}) {
  Out out;
  out.frag_tag = frag_tag;
  out.raw_index = count == 0 ? 0 : samples[0].raw_index;

  const bool unknown_recipe = recipe >= kRecipeCount;
  const bool malformed = !count_legal(recipe, count);
  if (unknown_recipe) {
    if (ledger) ++ledger->refused_unknown_recipe;
  } else if (malformed) {
    if (ledger) ++ledger->refused_count_mismatch;
  }

  out.status = detail::required_status(samples, count, aux_required, aux);
  if (malformed) out.status = static_cast<uint8_t>(out.status | 0x01u);
  out.refused = (out.status & 0x01u) != 0;

  // Any status bit is terminal and visible.  Preserve status, sample-0 index,
  // tag and the refused compatibility view while replacing partial arithmetic
  // with the one loud error value.
  if (out.status != 0) {
    out.r = 255;
    out.g = 0;
    out.b = 255;
    out.a = 255;
    return out;
  }

  // Only legal PASSTHRU count zero reaches here.  No sample field is read.
  if (count == 0) {
    out.r = base.r;
    out.g = base.g;
    out.b = base.b;
    out.a = base.a;
    return out;
  }

  const Sample& s0 = samples[0];

  switch (recipe) {
    case kPassthru:
      out.r = s0.r;
      out.g = s0.g;
      out.b = s0.b;
      out.a = s0.a;
      break;

    case kModulate:
      out.r = unit_mul(unit8{s0.r}, unit8{samples[1].r});
      out.g = unit_mul(unit8{s0.g}, unit8{samples[1].g});
      out.b = unit_mul(unit8{s0.b}, unit8{samples[1].b});
      out.a = s0.a;
      break;

    case kModulate2x: {
      bool saturated = false;
      out.r = detail::modulate2x8(s0.r, samples[1].r, &saturated);
      out.g = detail::modulate2x8(s0.g, samples[1].g, &saturated);
      out.b = detail::modulate2x8(s0.b, samples[1].b, &saturated);
      out.a = s0.a;
      if (saturated && ledger) ++ledger->saturated_mul2x;
      break;
    }

    case kLerp:
      out.r = detail::lerp8(s0.r, samples[1].r, weight);
      out.g = detail::lerp8(s0.g, samples[1].g, weight);
      out.b = detail::lerp8(s0.b, samples[1].b, weight);
      out.a = s0.a;
      break;

    case kAddSat: {
      bool saturated = false;
      out.r = detail::add_sat8(s0.r, samples[1].r, &saturated);
      out.g = detail::add_sat8(s0.g, samples[1].g, &saturated);
      out.b = detail::add_sat8(s0.b, samples[1].b, &saturated);
      out.a = s0.a;
      if (saturated && ledger) ++ledger->saturated_add;
      break;
    }

    case kMask:
      out.r = s0.r;
      out.g = s0.g;
      out.b = s0.b;
      out.a = unit_mul(unit8{s0.a}, unit8{samples[1].a});
      break;

    case kTerrainDetailLight: {
      bool saturated = false;
      const uint8_t first_r =
          detail::modulate2x8(s0.r, samples[1].r, &saturated);
      const uint8_t first_g =
          detail::modulate2x8(s0.g, samples[1].g, &saturated);
      const uint8_t first_b =
          detail::modulate2x8(s0.b, samples[1].b, &saturated);
      out.r = unit_mul(unit8{first_r}, unit8{samples[2].r});
      out.g = unit_mul(unit8{first_g}, unit8{samples[2].g});
      out.b = unit_mul(unit8{first_b}, unit8{samples[2].b});
      out.a = s0.a;
      if (saturated && ledger) ++ledger->saturated_mul2x;
      break;
    }

    case kTerrainDetailMask: {
      bool saturated = false;
      out.r = detail::modulate2x8(s0.r, samples[1].r, &saturated);
      out.g = detail::modulate2x8(s0.g, samples[1].g, &saturated);
      out.b = detail::modulate2x8(s0.b, samples[1].b, &saturated);
      out.a = unit_mul(unit8{s0.a}, unit8{samples[2].a});
      if (saturated && ledger) ++ledger->saturated_mul2x;
      break;
    }

    default:
      // Range/count validation above made every unknown recipe loud already.
      break;
  }

  return out;
}

}  // namespace material
}  // namespace zref

#endif  // ZREF_MATERIAL_HPP
