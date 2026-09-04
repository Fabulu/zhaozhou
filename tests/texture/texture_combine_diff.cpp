// texture_combine_diff.cpp — TEXTURE.COMBINE's RTL against its oracle.
// Authored 2026-09-05 (roadmap G1-C).
//
// The scalar law is `zref::material::combine`. This drives the RTL with the
// same inputs and requires bit-exact agreement on RGB, alpha, the refusal flag
// and `frag_tag`.
//
// Why the tag is checked as hard as the colour: FRAGROB retires in ALLOCATION
// order using that tag, so a combiner that reordered or corrupted it would be
// invisible in a triangle count and obvious only in a capture CRC — the exact
// failure mode this repository keeps finding late.
//
// The randomized sweep carries a COVERAGE GUARD. The contract warned that this
// repository "has shipped random tests that never hit their interesting case
// more than once", so the run asserts that every recipe, both refusal classes
// and both saturation classes were actually reached.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_texture_combine.h"

#include "zhao_sim.hpp"
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

namespace mat = zref::material;

// A tiny deterministic PRNG. Deterministic on purpose: a differential that
// cannot be replayed is a differential that cannot be debugged.
struct Rng {
  uint64_t s = 0x9E3779B97F4A7C15ull;
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return static_cast<uint32_t>(s >> 32);
  }
  uint8_t byte() { return static_cast<uint8_t>(next() & 0xFF); }
};

struct Stim {
  uint8_t recipe, weight, count;
  mat::Sample s0, s1, base;
  uint16_t tag;
};

// Push one fragment through and return the retired beat. The block is II=1 with
// one register stage, so a handful of cycles is ample; the loop bound is a
// guard against a hang becoming a silent pass.
bool drive(Vzhao_texture_combine& dut, const Stim& t, uint32_t* rgb, uint8_t* a,
           uint16_t* tag, bool* refused) {
  dut.f_valid_i = 1;
  dut.f_sample_count_i = t.count;
  dut.f_recipe_i = t.recipe;
  dut.f_weight_i = t.weight;
  dut.f_s0_rgb_i = (uint32_t(t.s0.r) << 16) | (uint32_t(t.s0.g) << 8) | t.s0.b;
  dut.f_s0_a_i = t.s0.a;
  dut.f_s1_rgb_i = (uint32_t(t.s1.r) << 16) | (uint32_t(t.s1.g) << 8) | t.s1.b;
  dut.f_s1_a_i = t.s1.a;
  dut.f_base_rgb_i = (uint32_t(t.base.r) << 16) | (uint32_t(t.base.g) << 8) | t.base.b;
  dut.f_base_a_i = t.base.a;
  dut.f_tag_i = t.tag;
  dut.o_ready_i = 1;

  // The handshake order matters and got it wrong once: `f_valid_i` must still
  // be asserted AT the clock edge. Clearing it after eval() but before tick()
  // means the RTL's `if (f_valid_i && f_ready_o)` never fires, and all 4,000
  // fragments silently failed to retire. Deassert AFTER the tick that took it.
  bool sent = false;
  for (int c = 0; c < 16; ++c) {
    dut.eval();
    const bool taking = !sent && dut.f_ready_o;
    if (dut.o_valid_o) {
      *rgb = dut.o_rgb_o;
      *a = dut.o_a_o;
      *tag = dut.o_tag_o;
      *refused = dut.o_refused_o != 0;
      zhao::tick(dut);
      dut.f_valid_i = 0;
      return true;
    }
    zhao::tick(dut);
    if (taking) {
      sent = true;
      dut.f_valid_i = 0;
    }
  }
  dut.f_valid_i = 0;
  return false;
}

void reset(Vzhao_texture_combine& dut) {
  dut.rst_n = 0;
  dut.f_valid_i = 0;
  dut.o_ready_i = 1;
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  zhao::tick(dut);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_combine dut;
  reset(dut);

  Rng rng;
  mat::Ledger oracle_ledger;

  int seen_recipe[8] = {0};
  int seen_refuse_unknown = 0, seen_refuse_missing = 0;
  int seen_untextured = 0;
  int mismatch_rgb = 0, mismatch_a = 0, mismatch_tag = 0, mismatch_refused = 0;
  int no_retire = 0;

  constexpr int kN = 4000;
  for (int i = 0; i < kN; ++i) {
    Stim t;
    // Recipes 0..7 so the two refused encodings (6, 7) are reached often.
    t.recipe = static_cast<uint8_t>(rng.next() % 8u);
    t.weight = rng.byte();
    t.count = static_cast<uint8_t>(rng.next() % 4u);  // 0..3
    // Bias toward the unit8 corners, which is where the rounding law bites and
    // where uniform bytes almost never land.
    auto corner = [&](void) -> uint8_t {
      const uint32_t k = rng.next() % 8u;
      if (k == 0) return 0;
      if (k == 1) return 1;
      if (k == 2) return 128;
      if (k == 3) return 255;
      return rng.byte();
    };
    t.s0 = {corner(), corner(), corner(), corner()};
    t.s1 = {corner(), corner(), corner(), corner()};
    t.base = {corner(), corner(), corner(), corner()};
    t.tag = static_cast<uint16_t>(rng.next() & 0xFFFF);

    const mat::Sample s[3] = {t.s0, t.s1, mat::Sample{}};
    const mat::Out want =
        mat::combine(t.recipe, t.weight, s, t.count, t.base, t.tag, &oracle_ledger);

    uint32_t rgb = 0;
    uint8_t a = 0;
    uint16_t tag = 0;
    bool refused = false;
    if (!drive(dut, t, &rgb, &a, &tag, &refused)) {
      ++no_retire;
      continue;
    }

    const uint32_t want_rgb =
        (uint32_t(want.r) << 16) | (uint32_t(want.g) << 8) | want.b;
    if (rgb != want_rgb) ++mismatch_rgb;
    if (a != want.a) ++mismatch_a;
    if (tag != want.frag_tag) ++mismatch_tag;
    if (refused != want.refused) ++mismatch_refused;

    if (t.count == 0)
      ++seen_untextured;
    else if (t.recipe >= mat::kRecipeCount)
      ++seen_refuse_unknown;
    else if (t.count < mat::samples_required(t.recipe))
      ++seen_refuse_missing;
    else
      ++seen_recipe[t.recipe];
  }

  check(no_retire == 0, "every fragment retired", 0, no_retire);
  check(mismatch_rgb == 0, "RGB matches the oracle on every fragment", 0, mismatch_rgb);
  check(mismatch_a == 0, "alpha matches the oracle on every fragment", 0, mismatch_a);
  check(mismatch_tag == 0, "frag_tag rides through untouched", 0, mismatch_tag);
  check(mismatch_refused == 0, "the refusal flag agrees with the oracle", 0,
        mismatch_refused);

  // ---- coverage guard ------------------------------------------------------
  int unreached = 0;
  for (int r = 0; r < mat::kRecipeCount; ++r)
    if (seen_recipe[r] == 0) ++unreached;
  check(unreached == 0, "every ratified recipe was exercised", 0, unreached);
  check(seen_refuse_unknown > 0, "the unknown-recipe refusal was reached", 1,
        seen_refuse_unknown > 0 ? 1 : 0);
  check(seen_refuse_missing > 0, "the missing-sample refusal was reached", 1,
        seen_refuse_missing > 0 ? 1 : 0);
  check(seen_untextured > 0, "the untextured (count==0) path was reached", 1,
        seen_untextured > 0 ? 1 : 0);
  check(oracle_ledger.saturated_add > 0, "ADD_SAT saturation was reached", 1,
        oracle_ledger.saturated_add > 0 ? 1 : 0);
  check(oracle_ledger.saturated_mul2x > 0, "MODULATE2X saturation was reached", 1,
        oracle_ledger.saturated_mul2x > 0 ? 1 : 0);

  // ---- the RTL's own counters agree with the oracle's -----------------------
  dut.eval();
  check(dut.refused_recipe_o == oracle_ledger.refused_unknown_recipe,
        "RTL unknown-recipe count matches the oracle",
        oracle_ledger.refused_unknown_recipe, dut.refused_recipe_o);
  check(dut.refused_missing_o == oracle_ledger.refused_missing_sample,
        "RTL missing-sample count matches the oracle",
        oracle_ledger.refused_missing_sample, dut.refused_missing_o);
  check(dut.saturated_add_o == oracle_ledger.saturated_add,
        "RTL ADD_SAT saturation count matches the oracle",
        oracle_ledger.saturated_add, dut.saturated_add_o);
  check(dut.saturated_mul2x_o == oracle_ledger.saturated_mul2x,
        "RTL MODULATE2X saturation count matches the oracle",
        oracle_ledger.saturated_mul2x, dut.saturated_mul2x_o);

  std::printf(
      "[texture_combine_diff] %d fragments; recipes %d/%d/%d/%d/%d/%d, "
      "untextured %d, refused %d+%d\n",
      kN, seen_recipe[0], seen_recipe[1], seen_recipe[2], seen_recipe[3],
      seen_recipe[4], seen_recipe[5], seen_untextured, seen_refuse_unknown,
      seen_refuse_missing);

  if (g_failed) {
    std::printf("[texture_combine_diff] %d/%d checks FAILED\n", g_failed, g_checks);
    return 1;
  }
  std::printf("[texture_combine_diff] %d checks passed\n", g_checks);
  return 0;
}
