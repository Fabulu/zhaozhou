// material_combine_directed.cpp — the directed suite TEXTURE.COMBINE.md asks
// for, authored 2026-09-05 (roadmap G1-C).
//
// The contract lists exactly these cases and they are implemented one for one:
//
//   * each of the six recipes at the unit8 corners (0, 1, 128, 255) --
//     INCLUDING that modulate by 255 is NOT identity;
//   * sample_count == 0 returning the untextured colour unchanged;
//   * a recipe demanding more samples than supplied, refused and counted;
//   * a seventh recipe encoding refused;
//   * frag_tag preserved through every path, because retirement order depends
//     on it;
//   * saturation reported for ADD_SAT and MODULATE2X.
//
// There is no RTL yet. This pins the SCALAR law so that when the RTL arrives it
// has something to be differentiated against -- which is the whole reason the
// contract wanted the oracle first.

#include <cstdint>
#include <cstdio>

#include "zref/zref_material.hpp"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, uint32_t expected, uint32_t got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected 0x%X, got 0x%X\n", what, expected, got);
  }
}

using zref::material::Ledger;
using zref::material::Out;
using zref::material::Sample;
namespace mat = zref::material;

Out run(uint8_t recipe, uint8_t weight, Sample a, Sample b, uint8_t count,
        Ledger* L = nullptr, uint16_t tag = 0xBEEF) {
  const Sample s[3] = {a, b, Sample{}};
  const Sample base{9, 9, 9, 9};
  return mat::combine(recipe, weight, s, count, base, tag, L);
}

// ---------------------------------------------------------------------------
// The unit8 law itself, which everything below depends on being understood.
// ---------------------------------------------------------------------------
// Modulate by 255 is not identity IN GENERAL -- but it is at exactly one
// value, and both halves of that are pinned here.
void test_modulate_by_255_is_not_identity() {
  const Out o = run(mat::kModulate, 0, Sample{200, 128, 1, 255},
                    Sample{255, 255, 255, 255}, 2);
  // unit_mul(200,255) = (200*255 + 128) >> 8 = (51000+128)>>8 = 199
  check(o.r == 199, "modulate 200 by 255 darkens to 199, NOT 200", 199, o.r);
  // THE EXACT LAW, worked out rather than guessed at:
  //   unit_mul(a,255) = floor((255a + 128)/256), and that equals a exactly
  //   when 255a + 128 >= 256a, i.e. when a <= 128.
  // So modulate by 255 is IDENTITY for every a <= 128 and subtracts exactly 1
  // for every a > 128. "Modulate by 255 always darkens" is the obvious summary
  // and it is false for more than half the input range.
  //
  // Two drafts of this comment were wrong before this one -- first "128 darkens
  // to 127", then "the one fixed point". There are 129 of them. Recorded
  // because a test comment that misstates the law it pins is worse than no
  // comment: the assertion still passes, so nothing ever contradicts it.
  check(o.g == 128, "modulate 128 by 255 is EXACTLY 128 (identity holds to 128)",
        128, o.g);
  check(o.b == 1, "modulate 1 by 255 stays 1", 1, o.b);
  check(o.a == 254, "modulate 255 by 255 is 254 -- 255 is not 1.0", 254, o.a);
}

void test_modulate_by_255_boundary_is_at_128() {
  // The identity/darken boundary is a property of the frozen unit8 law, so it
  // is pinned directly rather than left implied by one sampled value.
  const Sample white{255, 255, 255, 255};
  int wrong = 0;
  for (int a = 0; a <= 255; ++a) {
    const Sample s0{static_cast<uint8_t>(a), 0, 0, 0};
    const uint8_t got = run(mat::kModulate, 0, s0, white, 2).r;
    const uint8_t want = static_cast<uint8_t>(a <= 128 ? a : a - 1);
    if (got != want) ++wrong;
  }
  check(wrong == 0,
        "modulate by 255 is identity for a<=128 and a-1 above, across all 256",
        0, wrong);
}

void test_modulate_by_zero_and_corners() {
  const Out z = run(mat::kModulate, 0, Sample{255, 255, 255, 255},
                    Sample{0, 0, 0, 0}, 2);
  check(z.r == 0 && z.g == 0 && z.b == 0 && z.a == 0,
        "modulate by 0 is 0 on every channel", 0, z.r | z.g | z.b | z.a);

  // unit_mul(128,128) = (16384+128)>>8 = 64
  const Out h = run(mat::kModulate, 0, Sample{128, 128, 128, 128},
                    Sample{128, 128, 128, 128}, 2);
  check(h.r == 64, "modulate 128 by 128 is 64", 64, h.r);

  // unit_mul(1,1) = (1+128)>>8 = 0 -- rounding, not a bug
  const Out t = run(mat::kModulate, 0, Sample{1, 1, 1, 1}, Sample{1, 1, 1, 1}, 2);
  check(t.r == 0, "modulate 1 by 1 rounds to 0", 0, t.r);
}

void test_passthru_is_exact() {
  const Out o = run(mat::kPassthru, 0, Sample{1, 128, 255, 77},
                    Sample{9, 9, 9, 9}, 1);
  check(o.r == 1 && o.g == 128 && o.b == 255 && o.a == 77,
        "passthru returns sample 0 bit-exact", 0x0180FF4D,
        (o.r << 24) | (o.g << 16) | (o.b << 8) | o.a);
}

void test_add_sat_saturates_and_reports() {
  Ledger L;
  const Out o = run(mat::kAddSat, 0, Sample{200, 1, 128, 0},
                    Sample{100, 1, 127, 0}, 2, &L);
  check(o.r == 255, "200 + 100 saturates to 255", 255, o.r);
  check(o.g == 2, "1 + 1 is 2", 2, o.g);
  check(o.b == 255, "128 + 127 is exactly 255, the last unsaturated value", 255, o.b);
  check(L.saturated_add == 1, "saturation counted ONCE per fragment", 1,
        L.saturated_add);

  Ledger L2;
  run(mat::kAddSat, 0, Sample{1, 1, 1, 1}, Sample{2, 2, 2, 2}, 2, &L2);
  check(L2.saturated_add == 0, "a non-saturating add is not counted", 0,
        L2.saturated_add);
}

void test_modulate2x_saturates_and_reports() {
  Ledger L;
  // unit_mul(200,200) = (40000+128)>>8 = 156; *2 = 312 -> saturates
  const Out o = run(mat::kModulate2x, 0, Sample{200, 64, 0, 0},
                    Sample{200, 64, 0, 0}, 2, &L);
  check(o.r == 255, "modulate2x 200x200 saturates", 255, o.r);
  // unit_mul(64,64) = (4096+128)>>8 = 16; *2 = 32
  check(o.g == 32, "modulate2x 64x64 is 32", 32, o.g);
  check(L.saturated_mul2x == 1, "modulate2x saturation counted once", 1,
        L.saturated_mul2x);
}

void test_lerp_endpoints_and_midpoint() {
  const Out a = run(mat::kLerp, 0, Sample{10, 10, 10, 10},
                    Sample{250, 250, 250, 250}, 2);
  check(a.r == 10, "lerp at weight 0 is exactly sample 0", 10, a.r);

  // w=255: d=240, (240*255+128)>>8 = (61200+128)>>8 = 239 -> 10+239 = 249
  const Out b = run(mat::kLerp, 255, Sample{10, 10, 10, 10},
                    Sample{250, 250, 250, 250}, 2);
  check(b.r == 249, "lerp at weight 255 is ALMOST sample 1 -- 255 is not 1.0",
        249, b.r);

  // w=128: d=240, (240*128+128)>>8 = (30720+128)>>8 = 120 -> 130
  const Out m = run(mat::kLerp, 128, Sample{10, 10, 10, 10},
                    Sample{250, 250, 250, 250}, 2);
  check(m.r == 130, "lerp at weight 128 is the midpoint 130", 130, m.r);
}

void test_lerp_rounds_symmetrically_when_darkening() {
  // Darkening: s0 > s1, so d is negative. The magnitude is rounded and the sign
  // reapplied, so a darkening lerp and its mirror move by the same amount.
  const Out down = run(mat::kLerp, 128, Sample{250, 0, 0, 0}, Sample{10, 0, 0, 0}, 2);
  const Out up = run(mat::kLerp, 128, Sample{10, 0, 0, 0}, Sample{250, 0, 0, 0}, 2);
  const int moved_down = 250 - down.r;
  const int moved_up = up.r - 10;
  check(moved_down == moved_up,
        "lerp moves the same distance darkening as brightening", moved_up,
        moved_down);
}

void test_mask_uses_alpha_not_rgb() {
  const Out pass = run(mat::kMask, 0, Sample{11, 22, 33, 44},
                       Sample{0, 0, 0, 1}, 2);
  check(pass.r == 11 && pass.a == 44,
        "mask passes sample 0 when the mask alpha is non-zero", 11, pass.r);

  const Out block = run(mat::kMask, 0, Sample{11, 22, 33, 44},
                        Sample{255, 255, 255, 0}, 2);
  check(block.r == 0 && block.a == 0,
        "a bright but ZERO-ALPHA mask blocks -- the test is alpha, not RGB", 0,
        block.r | block.a);
}

// ---------------------------------------------------------------------------
// Refusals. The contract's reason: quietly accepting a bad input is how a
// content bug becomes a shipped picture nobody questions.
// ---------------------------------------------------------------------------
void test_zero_samples_returns_base_unchanged() {
  Ledger L;
  const Out o = run(mat::kModulate, 0, Sample{1, 2, 3, 4}, Sample{5, 6, 7, 8}, 0, &L);
  check(o.r == 9 && o.g == 9 && o.b == 9 && o.a == 9,
        "sample_count 0 returns the untextured colour unchanged", 9, o.r);
  check(!o.refused, "an untextured surface is LEGAL, not a refusal", 0,
        o.refused ? 1 : 0);
  check(L.refused_missing_sample == 0,
        "and it is not counted as a missing sample", 0, L.refused_missing_sample);
}

void test_recipe_demanding_more_samples_than_supplied_is_refused() {
  Ledger L;
  const Out o = run(mat::kModulate, 0, Sample{200, 200, 200, 200},
                    Sample{200, 200, 200, 200}, 1, &L);
  check(o.refused, "MODULATE with one sample is REFUSED", 1, o.refused ? 1 : 0);
  check(L.refused_missing_sample == 1, "and counted", 1, L.refused_missing_sample);
  check(o.r == 0 && o.a == 0,
        "a refused fragment does not silently degrade to passthrough", 0,
        o.r | o.a);
}

void test_seventh_encoding_is_refused() {
  Ledger L;
  const Out o = run(6, 0, Sample{1, 1, 1, 1}, Sample{1, 1, 1, 1}, 2, &L);
  check(o.refused, "recipe 6 is refused -- the set is closed", 1,
        o.refused ? 1 : 0);
  check(L.refused_unknown_recipe == 1, "and counted", 1,
        L.refused_unknown_recipe);

  Ledger L2;
  run(255, 0, Sample{1, 1, 1, 1}, Sample{1, 1, 1, 1}, 2, &L2);
  check(L2.refused_unknown_recipe == 1, "so is 255", 1, L2.refused_unknown_recipe);
}

void test_frag_tag_survives_every_path() {
  const uint16_t kTag = 0x5A5A;
  int bad = 0;
  // every recipe, plus the two refusal paths, plus the untextured path
  for (uint8_t r = 0; r < mat::kRecipeCount; ++r) {
    if (run(r, 128, Sample{1, 1, 1, 1}, Sample{2, 2, 2, 2}, 2, nullptr, kTag)
            .frag_tag != kTag) ++bad;
  }
  if (run(6, 0, Sample{}, Sample{}, 2, nullptr, kTag).frag_tag != kTag) ++bad;
  if (run(mat::kModulate, 0, Sample{}, Sample{}, 1, nullptr, kTag).frag_tag != kTag) ++bad;
  if (run(mat::kPassthru, 0, Sample{}, Sample{}, 0, nullptr, kTag).frag_tag != kTag) ++bad;
  check(bad == 0, "frag_tag rides through all nine paths untouched", 0, bad);
}

// A coverage guard, because the contract warns that this repository "has
// shipped random tests that never hit their interesting case more than once".
void test_every_recipe_and_refusal_was_reached() {
  Ledger L;
  bool seen[mat::kRecipeCount] = {false};
  for (uint8_t r = 0; r < mat::kRecipeCount; ++r) {
    const Out o = run(r, 100, Sample{130, 60, 20, 200}, Sample{90, 200, 5, 128}, 2);
    seen[r] = !o.refused;
  }
  int unseen = 0;
  for (bool b : seen)
    if (!b) ++unseen;
  check(unseen == 0, "all six ratified recipes produced a result", 0, unseen);

  run(6, 0, Sample{}, Sample{}, 2, &L);
  run(mat::kModulate, 0, Sample{}, Sample{}, 1, &L);
  check(L.refused_unknown_recipe == 1 && L.refused_missing_sample == 1,
        "both refusal classes were reached", 2,
        L.refused_unknown_recipe + L.refused_missing_sample);
}

}  // namespace

int main() {
  test_modulate_by_255_is_not_identity();
  test_modulate_by_255_boundary_is_at_128();
  test_modulate_by_zero_and_corners();
  test_passthru_is_exact();
  test_add_sat_saturates_and_reports();
  test_modulate2x_saturates_and_reports();
  test_lerp_endpoints_and_midpoint();
  test_lerp_rounds_symmetrically_when_darkening();
  test_mask_uses_alpha_not_rgb();
  test_zero_samples_returns_base_unchanged();
  test_recipe_demanding_more_samples_than_supplied_is_refused();
  test_seventh_encoding_is_refused();
  test_frag_tag_survives_every_path();
  test_every_recipe_and_refusal_was_reached();

  if (g_failed) {
    std::printf("[material_combine_directed] %d/%d checks FAILED\n", g_failed,
                g_checks);
    return 1;
  }
  std::printf("[material_combine_directed] %d checks passed\n", g_checks);
  return 0;
}
