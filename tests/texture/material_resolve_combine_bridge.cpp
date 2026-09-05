// material_resolve_combine_bridge.cpp -- the test that crosses the boundary.
//
// AUDIT R1 and R2 (reports/AUDIT.md). Both headers were internally consistent
// and both suites were green, and the machine was still broken between them:
//
//   * zref_material.hpp implemented kRecipeCount = 8, including the two
//     three-sample terrain recipes, and material_combine_v1_diff drove all
//     eight against it.
//   * zref_material_resolve.hpp::record_legal refused `recipe >= 6`, so a
//     legal record naming either terrain recipe was rejected BEFORE it could
//     reach the combiner that exists to execute it.
//   * And no translation unit could include both headers, because each
//     defined a different `zref::material::Ledger`, so nothing could have
//     noticed even in principle.
//
// The audit's instruction was explicit: "Add tests crossing these boundaries
// rather than just enlarging separate suites." Enlarging either suite would
// not have found this. Only a file that includes BOTH headers and runs a
// record through resolve and then into combine can.
//
// THIS FILE EXISTING AT ALL IS PART OF THE FIX. It is the compile-time proof
// for R2: if the two Ledger types are ever merged back under one name, this
// stops building.

#include <cstdint>
#include <cstdio>

#include "zref/zref_material.hpp"
#include "zref/zref_material_resolve.hpp"

namespace {

int g_checks = 0;
bool g_failed = false;

void check(bool ok, const char* what, long want, long got) {
  ++g_checks;
  if (!ok) {
    g_failed = true;
    std::printf("FAIL: %s: expected %ld, got %ld\n", what, want, got);
  }
}

// A record that is legal in every respect EXCEPT the recipe under test:
// reserved fields zero, sample count three, legal wrap modes.
zhao_abi::ZhMaterialRecord record_with(uint8_t recipe, uint8_t sample_count) {
  zhao_abi::ZhMaterialRecord r{};
  r.control = static_cast<uint8_t>((sample_count & 0x3u) | ((recipe & 0x7u) << 2));
  return r;
}

}  // namespace

int main() {
  namespace mat = zref::material;

  // ---- R1: every ratified recipe survives the route in ---------------------
  // Driven from kRecipeCount rather than a literal 8, so that adding a recipe
  // to the enum extends this test instead of silently outrunning it -- which
  // is exactly how the ceiling drifted to begin with.
  for (uint8_t recipe = 0; recipe < static_cast<uint8_t>(mat::kRecipeCount); ++recipe) {
    const uint8_t samples = mat::samples_required(recipe);
    const zhao_abi::ZhMaterialRecord r = record_with(recipe, samples);
    const bool legal = mat::record_legal(r);
    if (!legal) std::printf("  recipe %u REFUSED by record_legal\n", recipe);
    check(legal, "every recipe the combiner implements is accepted by record_legal", 1,
          legal ? 1 : 0);
  }

  // ---- and the two that were actually refused, named ----------------------
  // Stated separately because these are the two the old `recipe >= 6` ceiling
  // rejected, and a loop passing tells you less than these passing by name.
  check(mat::record_legal(record_with(mat::kTerrainDetailLight, 3)),
        "kTerrainDetailLight (6) is accepted -- it was refused before", 1,
        mat::record_legal(record_with(mat::kTerrainDetailLight, 3)) ? 1 : 0);
  check(mat::record_legal(record_with(mat::kTerrainDetailMask, 3)),
        "kTerrainDetailMask (7) is accepted -- it was refused before", 1,
        mat::record_legal(record_with(mat::kTerrainDetailMask, 3)) ? 1 : 0);

  // ---- the ceiling is still a ceiling --------------------------------------
  // The fix must not become "accept anything". The recipe field is three bits,
  // so 8..15 are expressible and must still be refused. If kRecipeCount ever
  // reaches 8 legitimately this check retires itself rather than lying.
  if (static_cast<int>(mat::kRecipeCount) < 8) {
    const zhao_abi::ZhMaterialRecord bad = record_with(7, 3);
    check(!mat::record_legal(bad), "an encoding beyond kRecipeCount is still refused", 0,
          mat::record_legal(bad) ? 1 : 0);
  }

  // ---- R2: both ledgers coexist, and count different things ---------------
  // The compile is the assertion. These lines exist so the types are actually
  // instantiated rather than merely declared, and so their distinctness is
  // visible to a reader.
  mat::Ledger combine_ledger{};
  mat::ResolveLedger resolve_ledger{};
  combine_ledger.saturated_add = 1;
  resolve_ledger.not_resident = 1;
  check(combine_ledger.saturated_add == 1 && resolve_ledger.not_resident == 1,
        "the COMBINE and RESOLVE ledgers are distinct types in one translation "
        "unit -- this file compiling at all is the R2 fix",
        1, 1);

  // ---- the record actually reaches the combiner ---------------------------
  // record_legal accepting is necessary and not sufficient: the point of the
  // repair is that a terrain-recipe record can travel the whole way. Combine
  // it and require a definite, non-refused answer.
  {
    mat::Sample s[3];
    s[0].r = 200;
    s[0].g = 150;
    s[0].b = 100;
    s[0].a = 255;
    s[1].r = 128;
    s[1].g = 128;
    s[1].b = 128;
    s[1].a = 255;
    s[2].r = 64;
    s[2].g = 64;
    s[2].b = 64;
    s[2].a = 255;
    mat::Sample base{};
    base.r = 32;
    base.g = 32;
    base.b = 32;
    base.a = 255;
    mat::Ledger led{};
    const mat::Out out = mat::combine(mat::kTerrainDetailLight, /*weight=*/128, s, /*count=*/3,
                                      base, /*frag_tag=*/0x1234, &led);
    check(!out.refused,
          "and a kTerrainDetailLight record COMBINES rather than being refused "
          "-- accepted at the door is not the same as executed",
          0, out.refused ? 1 : 0);
    check(led.refused_unknown_recipe == 0,
          "with the combiner's own unknown-recipe counter still zero", 0,
          static_cast<long>(led.refused_unknown_recipe));
  }

  std::printf("[material_resolve_combine_bridge] %d checks %s\n", g_checks,
              g_failed ? "FAILED" : "passed");
  return g_failed ? 1 : 0;
}
