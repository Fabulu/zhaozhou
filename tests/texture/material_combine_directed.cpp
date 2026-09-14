// material_combine_directed.cpp -- authoritative R9 scalar material suite.
//
// This tests reference/include/zref/zref_material.hpp directly against owner
// ruling R9.  Historical V1/V2 behavior belongs only to
// legacy_material_v2_oracle.hpp and is not permitted to outvote these checks.
#include <cstdint>
#include <cstdio>

#include "zref/zref_material.hpp"

namespace mat = zref::material;

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, unsigned long long expected,
           unsigned long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected 0x%llX, got 0x%llX\n", what, expected, got);
  }
}

mat::Sample sample(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                   uint8_t raw_index = 0, uint8_t status = 0) {
  mat::Sample s;
  s.r = r;
  s.g = g;
  s.b = b;
  s.a = a;
  s.raw_index = raw_index;
  s.status = status;
  return s;
}

mat::Out run(uint8_t recipe, uint8_t weight, mat::Sample s0,
             mat::Sample s1, mat::Sample s2, uint8_t count,
             mat::Sample base = {}, mat::Ledger* ledger = nullptr,
             bool aux_required = false, mat::Aux aux = {},
             uint16_t tag = 0xBEEF) {
  const mat::Sample samples[3] = {s0, s1, s2};
  return mat::combine(recipe, weight, samples, count, base, tag, ledger,
                      aux_required, aux);
}

uint32_t rgba(const mat::Out& out) {
  return (static_cast<uint32_t>(out.r) << 24) |
         (static_cast<uint32_t>(out.g) << 16) |
         (static_cast<uint32_t>(out.b) << 8) | out.a;
}

bool loud(const mat::Out& out) {
  return out.r == 255 && out.g == 0 && out.b == 255 && out.a == 255;
}

void test_exact_r9_arithmetic() {
  const mat::Out pass = run(mat::kPassthru, 0,
                            sample(1, 128, 255, 77, 0xA5), {}, {}, 1);
  check(rgba(pass) == 0x0180FF4Du, "PASSTHRU is bit exact", 0x0180FF4D,
        rgba(pass));
  check(pass.raw_index == 0xA5, "PASSTHRU preserves sample-0 raw index", 0xA5,
        pass.raw_index);

  const mat::Out mod = run(mat::kModulate, 0,
                           sample(200, 128, 1, 201),
                           sample(128, 255, 255, 0), {}, 2);
  check(mod.r == 100 && mod.g == 128 && mod.b == 1,
        "MODULATE uses unit8 independently on RGB", 1,
        mod.r == 100 && mod.g == 128 && mod.b == 1 ? 1 : 0);
  check(mod.a == 201, "MODULATE does not multiply alpha", 201, mod.a);

  mat::Ledger mod2_ledger;
  const mat::Out mod2 = run(mat::kModulate2x, 0,
                            sample(1, 64, 200, 203),
                            sample(64, 64, 200, 0), {}, 2, {},
                            &mod2_ledger);
  check(mod2.r == 1,
        "MODULATE2X rounds once: (1*64+64)>>7 is 1", 1, mod2.r);
  check(mod2.g == 32, "MODULATE2X 64*64 is 32", 32,
        mod2.g);
  check(mod2.b == 255, "MODULATE2X saturation is visible", 255, mod2.b);
  check(mod2.a == 203, "MODULATE2X does not multiply alpha", 203, mod2.a);
  check(mod2_ledger.saturated_mul2x == 1,
        "MODULATE2X saturation counts once per fragment", 1,
        mod2_ledger.saturated_mul2x);

  const mat::Out lerp_down = run(mat::kLerp, 128,
                                 sample(200, 200, 200, 177),
                                 sample(199, 199, 199, 1), {}, 2);
  check(lerp_down.r == 200,
        "negative LERP tie rounds toward positive infinity", 200,
        lerp_down.r);
  check(lerp_down.a == 177, "LERP does not interpolate alpha", 177,
        lerp_down.a);

  mat::Ledger add_ledger;
  const mat::Out add = run(mat::kAddSat, 0,
                           sample(200, 127, 1, 211),
                           sample(100, 128, 2, 99), {}, 2, {},
                           &add_ledger);
  check(add.r == 255 && add.g == 255 && add.b == 3,
        "ADD_SAT saturates RGB only", 1,
        add.r == 255 && add.g == 255 && add.b == 3 ? 1 : 0);
  check(add.a == 211, "ADD_SAT does not add alpha", 211, add.a);
  check(add_ledger.saturated_add == 1,
        "ADD_SAT saturation counts once per fragment", 1,
        add_ledger.saturated_add);

  const mat::Out mask = run(mat::kMask, 0,
                            sample(11, 22, 33, 200),
                            sample(255, 0, 0, 128), {}, 2);
  check(mask.r == 11 && mask.g == 22 && mask.b == 33,
        "MASK RGB is always sample 0", 1,
        mask.r == 11 && mask.g == 22 && mask.b == 33 ? 1 : 0);
  check(mask.a == 100,
        "MASK is continuous unit alpha, not a nonzero gate", 100, mask.a);

  mat::Ledger detail_light_ledger;
  const mat::Out detail_light = run(
      mat::kTerrainDetailLight, 0, sample(1, 1, 1, 173),
      sample(64, 64, 64, 2), sample(255, 255, 255, 3), 3, {},
      &detail_light_ledger);
  check(detail_light.r == 1 && detail_light.g == 1 && detail_light.b == 1,
        "DETAIL_LIGHT uses MODULATE2X first layer then unit sample 2", 1,
        detail_light.r == 1 && detail_light.g == 1 && detail_light.b == 1
            ? 1
            : 0);
  check(detail_light.a == 173, "DETAIL_LIGHT keeps sample-0 alpha", 173,
        detail_light.a);

  const mat::Out detail_mask = run(
      mat::kTerrainDetailMask, 0, sample(1, 1, 1, 240),
      sample(64, 64, 64, 7), sample(0, 0, 0, 128), 3);
  check(detail_mask.r == 1 && detail_mask.g == 1 && detail_mask.b == 1,
        "DETAIL_MASK RGB uses MODULATE2X first layer", 1,
        detail_mask.r == 1 && detail_mask.g == 1 && detail_mask.b == 1
            ? 1
            : 0);
  check(detail_mask.a == 120,
        "DETAIL_MASK alpha is unit(s0.a,s2.a)", 120, detail_mask.a);
}

void test_exact_count_table_and_loud_error() {
  int wrong_legality = 0;
  int malformed_not_loud = 0;
  int malformed_not_counted = 0;

  for (uint8_t recipe = 0; recipe < 8; ++recipe) {
    for (uint8_t count = 0; count < 4; ++count) {
      const bool expected = recipe == 0 ? (count == 0 || count == 1)
                           : recipe >= 6 ? count == 3
                                         : count == 2;
      if (mat::count_legal(recipe, count) != expected) ++wrong_legality;

      mat::Ledger ledger;
      const mat::Out out = run(recipe, 128, sample(10, 20, 30, 40, 0xA5),
                               sample(50, 60, 70, 80),
                               sample(90, 100, 110, 120), count,
                               sample(7, 8, 9, 10), &ledger);
      if (!expected) {
        if (!loud(out) || out.status != 1 || !out.refused)
          ++malformed_not_loud;
        if (ledger.refused_count_mismatch != 1)
          ++malformed_not_counted;
      }
    }
  }

  check(wrong_legality == 0, "all 32 recipe/count cells match R9", 0,
        wrong_legality);
  check(malformed_not_loud == 0,
        "every exact-count mismatch is loud SOURCE_REFUSED", 0,
        malformed_not_loud);
  check(malformed_not_counted == 0,
        "every exact-count mismatch is counted", 0,
        malformed_not_counted);

  mat::Ledger unknown_ledger;
  const mat::Out unknown = run(8, 0, sample(1, 2, 3, 4, 0x66), {}, {}, 1,
                               {}, &unknown_ledger);
  check(unknown_ledger.refused_unknown_recipe == 1,
        "unknown recipe is separately counted", 1,
        unknown_ledger.refused_unknown_recipe);
  check(unknown.status == 1 && unknown.refused && loud(unknown),
        "unknown recipe is a loud terminal refusal", 1,
        unknown.status == 1 && unknown.refused && loud(unknown) ? 1 : 0);

  const mat::Out count0 = run(mat::kPassthru, 0,
                              sample(255, 0, 255, 255, 0xEE, 0x80), {}, {}, 0,
                              sample(7, 8, 9, 10));
  check(rgba(count0) == 0x0708090Au,
        "PASSTHRU count zero returns admitted base", 0x0708090A,
        rgba(count0));
  check(count0.raw_index == 0 && count0.status == 0,
        "PASSTHRU count zero reads no sample status/index", 0,
        static_cast<unsigned>(count0.raw_index) | count0.status);
}

void test_required_status_index_and_aux_typing() {
  const mat::Out two = run(
      mat::kModulate, 0, sample(10, 20, 30, 40, 0xA5, 0x02),
      sample(50, 60, 70, 80, 0xB6, 0x04),
      sample(90, 100, 110, 120, 0xC7, 0x08), 2, {}, nullptr,
      false, mat::Aux{0x10, 0xAA, 0xBB});
  check(two.status == 0x06,
        "count two ORs only sample 0 and sample 1 status", 0x06,
        two.status);
  check(two.raw_index == 0xA5,
        "sample-0 raw index survives a terminal error", 0xA5,
        two.raw_index);
  check(loud(two), "any nonzero status produces loud RGBA", 1,
        loud(two) ? 1 : 0);
  check(!two.refused,
        "reserved status bits are loud but refused remains status[0]", 0,
        two.refused ? 1 : 0);

  const mat::Out four = run(
      mat::kTerrainDetailLight, 0,
      sample(10, 20, 30, 40, 0xA5, 0x02),
      sample(50, 60, 70, 80, 0xB6, 0x04),
      sample(90, 100, 110, 120, 0xC7, 0x08), 3, {}, nullptr,
      true, mat::Aux{0x10, 0xAA, 0xBB});
  check(four.status == 0x1E,
        "three samples plus required AUX OR exactly four statuses", 0x1E,
        four.status);

  const mat::Out aux_a = run(
      mat::kTerrainDetailLight, 0, sample(100, 120, 140, 201, 0x5A),
      sample(128, 128, 128, 17), sample(255, 200, 64, 99), 3, {},
      nullptr, true, mat::Aux{0, 1, 2});
  const mat::Out aux_b = run(
      mat::kTerrainDetailLight, 0, sample(100, 120, 140, 201, 0x5A),
      sample(128, 128, 128, 17), sample(255, 200, 64, 99), 3, {},
      nullptr, true, mat::Aux{0, 254, 255});
  check(rgba(aux_a) == rgba(aux_b),
        "successful AUX tag/strength are deliberately unconsumed", 1,
        rgba(aux_a) == rgba(aux_b) ? 1 : 0);
  check(aux_a.raw_index == 0x5A,
        "AUX cannot replace sample-0 raw index", 0x5A,
        aux_a.raw_index);

  const mat::Out s2_changed = run(
      mat::kTerrainDetailLight, 0, sample(100, 120, 140, 201, 0x5A),
      sample(128, 128, 128, 17), sample(64, 64, 64, 99), 3, {},
      nullptr, true, mat::Aux{0, 1, 2});
  check(rgba(aux_a) != rgba(s2_changed),
        "true sample 2, not AUX, changes DETAIL_LIGHT", 1,
        rgba(aux_a) != rgba(s2_changed) ? 1 : 0);
}

void test_cadence_tables_and_corner_primitives() {
  static constexpr uint8_t expected_phases[8] = {1, 2, 2, 2, 1, 1, 3, 2};
  static constexpr uint8_t expected_jobs[8] = {0, 3, 3, 3, 0, 1, 6, 4};
  int wrong_phases = 0;
  int wrong_jobs = 0;
  for (uint8_t recipe = 0; recipe < 8; ++recipe) {
    if (mat::phases_required(recipe) != expected_phases[recipe]) ++wrong_phases;
    if (mat::product_jobs(recipe) != expected_jobs[recipe]) ++wrong_jobs;
  }
  check(wrong_phases == 0, "phase table is 1/2/2/2/1/1/3/2", 0,
        wrong_phases);
  check(wrong_jobs == 0, "product-job table is 0/3/3/3/0/1/6/4", 0,
        wrong_jobs);

  static constexpr uint8_t corners[6] = {0, 1, 127, 128, 254, 255};
  int wrong_unit = 0;
  int wrong_mod2 = 0;
  for (uint8_t a : corners) {
    for (uint8_t b : corners) {
      const uint8_t unit_expected = static_cast<uint8_t>(
          (static_cast<uint32_t>(a) * b + 128u) >> 8);
      const uint32_t mod2_wide =
          (static_cast<uint32_t>(a) * b + 64u) >> 7;
      const uint8_t mod2_expected = static_cast<uint8_t>(
          mod2_wide > 255u ? 255u : mod2_wide);
      const mat::Out unit = run(mat::kModulate, 0, sample(a, a, a, 99),
                                sample(b, b, b, 1), {}, 2);
      const mat::Out mod2 = run(mat::kModulate2x, 0, sample(a, a, a, 99),
                                sample(b, b, b, 1), {}, 2);
      if (unit.r != unit_expected) ++wrong_unit;
      if (mod2.r != mod2_expected) ++wrong_mod2;
    }
  }
  check(wrong_unit == 0,
        "unit multiply matches formula at all required corner pairs", 0,
        wrong_unit);
  check(wrong_mod2 == 0,
        "single-round MODULATE2X matches formula at all corner pairs", 0,
        wrong_mod2);
}

void test_exhaustive_add_and_signed_lerp() {
  int add_wrong = 0;
  int add_sat_wrong = 0;
  int add_alpha_wrong = 0;
  uint32_t add_saturations = 0;
  for (int a = 0; a < 256; ++a) {
    for (int b = 0; b < 256; ++b) {
      mat::Ledger ledger;
      const mat::Out out = run(
          mat::kAddSat, 0,
          sample(static_cast<uint8_t>(a), 0, 0, 0xE1),
          sample(static_cast<uint8_t>(b), 0, 0, 0xF2), {}, 2, {}, &ledger);
      const int wide = a + b;
      const uint8_t expected = static_cast<uint8_t>(wide > 255 ? 255 : wide);
      const uint32_t expected_sat = wide > 255 ? 1u : 0u;
      if (out.r != expected) ++add_wrong;
      if (ledger.saturated_add != expected_sat) ++add_sat_wrong;
      if (out.a != 0xE1) ++add_alpha_wrong;
      add_saturations += ledger.saturated_add;
    }
  }
  check(add_wrong == 0, "ADD_SAT is exact across all 65,536 byte pairs", 0,
        add_wrong);
  check(add_sat_wrong == 0,
        "ADD_SAT reports exactly the pairs strictly above 255", 0,
        add_sat_wrong);
  check(add_saturations == 32640,
        "ADD_SAT exhaustive sweep has the exact saturation population", 32640,
        add_saturations);
  check(add_alpha_wrong == 0,
        "ADD_SAT keeps sample-0 alpha across every boundary pair", 0,
        add_alpha_wrong);

  int lerp_wrong = 0;
  int lerp_alpha_wrong = 0;
  int positive_ties = 0;
  int negative_ties = 0;
  for (int a = 0; a < 256; ++a) {
    for (int b = 0; b < 256; ++b) {
      for (int weight = 0; weight < 256; ++weight) {
        const int product = (b - a) * weight;
        const int numerator = product + 128;
        const int scaled = numerator >= 0
            ? numerator / 256
            : -((-numerator + 255) / 256);
        const int wide = a + scaled;
        const uint8_t expected = static_cast<uint8_t>(
            wide < 0 ? 0 : (wide > 255 ? 255 : wide));
        const mat::Out out = run(
            mat::kLerp, static_cast<uint8_t>(weight),
            sample(static_cast<uint8_t>(a), 0, 0, 0xD3),
            sample(static_cast<uint8_t>(b), 0, 0, 0x24), {}, 2);
        if (out.r != expected) ++lerp_wrong;
        if (out.a != 0xD3) ++lerp_alpha_wrong;
        int residue = product % 256;
        if (residue < 0) residue += 256;
        if (residue == 128) {
          if (product >= 0) ++positive_ties;
          else ++negative_ties;
        }
      }
    }
  }
  check(lerp_wrong == 0,
        "signed LERP is exact for every a/b/weight byte cross-product", 0,
        lerp_wrong);
  check(lerp_alpha_wrong == 0,
        "signed LERP keeps sample-0 alpha in all 16,777,216 cases", 0,
        lerp_alpha_wrong);
  check(positive_ties > 0 && negative_ties > 0,
        "exhaustive LERP reaches ties on both sides of zero", 1,
        positive_ties > 0 && negative_ties > 0 ? 1 : 0);
}

void test_one_element_passthru_caller() {
  mat::Sample only = sample(9, 8, 7, 6, 0xA5, 0);
  const mat::Out out = mat::combine(mat::kPassthru, 0, &only, 1, {},
                                    0x1234, nullptr);
  check(rgba(out) == 0x09080706u,
        "legal count-1 PASSTHRU accepts a literal one-Sample array", 0x09080706,
        rgba(out));
  check(out.raw_index == 0xA5 && out.frag_tag == 0x1234,
        "one-element PASSTHRU preserves index and tag without touching s1/s2", 1,
        out.raw_index == 0xA5 && out.frag_tag == 0x1234 ? 1 : 0);
}

void test_tag_survives_every_path() {
  constexpr uint16_t kTag = 0x5A5A;
  int wrong = 0;
  for (uint8_t recipe = 0; recipe < 8; ++recipe) {
    const uint8_t count = recipe == 0 ? 1 : (recipe >= 6 ? 3 : 2);
    const mat::Out out = run(recipe, 128, sample(1, 2, 3, 4),
                             sample(5, 6, 7, 8), sample(9, 10, 11, 12),
                             count, {}, nullptr, false, {}, kTag);
    if (out.frag_tag != kTag) ++wrong;
  }
  const mat::Out malformed = run(mat::kModulate, 0, {}, {}, {}, 0, {},
                                 nullptr, false, {}, kTag);
  const mat::Out source_error = run(mat::kPassthru, 0,
                                    sample(1, 2, 3, 4, 0, 1), {}, {}, 1,
                                    {}, nullptr, false, {}, kTag);
  if (malformed.frag_tag != kTag) ++wrong;
  if (source_error.frag_tag != kTag) ++wrong;
  check(wrong == 0, "tag survives all recipes and terminal errors", 0, wrong);
}

}  // namespace

int main() {
  test_exact_r9_arithmetic();
  test_exact_count_table_and_loud_error();
  test_required_status_index_and_aux_typing();
  test_cadence_tables_and_corner_primitives();
  test_exhaustive_add_and_signed_lerp();
  test_one_element_passthru_caller();
  test_tag_survives_every_path();

  if (g_failed) {
    std::printf("[material_combine_directed] %d/%d checks FAILED\n", g_failed,
                g_checks);
    return 1;
  }
  std::printf("[material_combine_directed] %d checks passed\n", g_checks);
  return 0;
}
