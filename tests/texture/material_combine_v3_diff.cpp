// material_combine_v3_diff.cpp -- Packet-B R9 combiner differential.
//
// Ordinary build: zhao_texture_material_combine_v3 in READ_LATE=0 mode against
// (1) an independent scalar transcription of owner ruling R9 and (2) the
// coordinator-corrected zref::material::combine for successful arithmetic.
// The source/status/index/AUX laws are checked directly from the typed planes.
//
// Mutant builds: define exactly one MATERIAL_V3_MUTANT_* macro and elaborate
// the correspondingly named top from
// tests/mutants/zhao_texture_material_combine_v3_mutants.sv.  Each such run has
// inverse polarity: it passes only when its focused R9 vector disagrees with the
// deliberately stale output.  These are instrument controls, not design tests.

#include <cstdint>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <vector>

#include "verilated.h"
#include "../harness/zhao_sim.hpp"

#if defined(MATERIAL_V3_MUTANT_ALPHA)
#define MATERIAL_V3_MUTANT_BUILD 1
constexpr int kMutation = 1;
constexpr const char* kMutationName = "stale alpha arithmetic on recipes 1-4";
#elif defined(MATERIAL_V3_MUTANT_MASK)
#define MATERIAL_V3_MUTANT_BUILD 1
constexpr int kMutation = 2;
constexpr const char* kMutationName = "binary MASK";
#elif defined(MATERIAL_V3_MUTANT_MOD2X)
#define MATERIAL_V3_MUTANT_BUILD 1
constexpr int kMutation = 3;
constexpr const char* kMutationName = "rounded-unit-then-double MODULATE2X";
#elif defined(MATERIAL_V3_MUTANT_DETAIL)
#define MATERIAL_V3_MUTANT_BUILD 1
constexpr int kMutation = 4;
constexpr const char* kMutationName = "unit terrain first layer";
#elif defined(MATERIAL_V3_MUTANT_COUNT_ZERO)
#define MATERIAL_V3_MUTANT_BUILD 1
constexpr int kMutation = 5;
constexpr const char* kMutationName = "universal count-zero bypass";
#elif defined(MATERIAL_V3_MUTANT_AUX_SAMPLE2)
#define MATERIAL_V3_MUTANT_BUILD 1
constexpr int kMutation = 6;
constexpr const char* kMutationName = "AUX substituted for sample 2";
#elif defined(MATERIAL_V3_MUTANT_STATUS_OMIT)
#define MATERIAL_V3_MUTANT_BUILD 1
constexpr int kMutation = 7;
constexpr const char* kMutationName = "required sample-1 status omitted";
#elif defined(MATERIAL_V3_MUTANT_INDEX_DROP)
#define MATERIAL_V3_MUTANT_BUILD 1
constexpr int kMutation = 8;
constexpr const char* kMutationName = "dropped sample-0 raw index";
#elif defined(MATERIAL_V3_MUTANT_INDEX_RGB)
#define MATERIAL_V3_MUTANT_BUILD 1
constexpr int kMutation = 9;
constexpr const char* kMutationName = "raw index reconstructed from RGB";
#elif defined(MATERIAL_V3_MUTANT_PHASE_DROP)
#define MATERIAL_V3_MUTANT_BUILD 1
constexpr int kMutation = 10;
constexpr const char* kMutationName = "dropped phase completion accounting";
#elif defined(MATERIAL_V3_MUTANT_PHASE_REISSUE)
#define MATERIAL_V3_MUTANT_BUILD 1
constexpr int kMutation = 11;
constexpr const char* kMutationName = "reissued phase launch accounting";
#elif defined(MATERIAL_V3_MUTANT_SKIP_S)
#define MATERIAL_V3_MUTANT_BUILD 1
constexpr int kMutation = 12;
constexpr const char* kMutationName = "missing source-capture S boundary";
#elif defined(MATERIAL_V3_MUTANT_SKIP_F)
#define MATERIAL_V3_MUTANT_BUILD 1
constexpr int kMutation = 13;
constexpr const char* kMutationName = "missing finish-result F boundary";
#endif

#if defined(MATERIAL_V3_MUTANT_BUILD)
#include "Vmatv3_mut.h"
using Dut = Vmatv3_mut;
#else
#include "Vzhao_texture_material_combine_v3.h"
#include "zref/zref_material.hpp"
using Dut = Vzhao_texture_material_combine_v3;
#endif

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

struct Plane {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;
  uint8_t raw_index = 0;
  uint8_t status = 0;
};

struct Frag {
  uint8_t recipe = 0;
  uint8_t count = 0;
  uint8_t weight = 0;
  bool aux_required = false;
  Plane s[3];
  Plane aux;  // typed AUX: only status may be consumed by the real combiner
  Plane base;
  uint16_t tag = 0;
};

struct Got {
  uint64_t result = 0;
  uint16_t tag = 0;
  bool refused = false;
};

uint64_t pack_plane(const Plane& p) {
  return (static_cast<uint64_t>(p.status) << 40) |
         (static_cast<uint64_t>(p.raw_index) << 32) |
         (static_cast<uint64_t>(p.a) << 24) |
         (static_cast<uint64_t>(p.r) << 16) |
         (static_cast<uint64_t>(p.g) << 8) |
         static_cast<uint64_t>(p.b);
}

uint64_t pack_result(uint8_t status, uint8_t index, const Plane& p) {
  return (static_cast<uint64_t>(status) << 40) |
         (static_cast<uint64_t>(index) << 32) |
         (static_cast<uint64_t>(p.a) << 24) |
         (static_cast<uint64_t>(p.r) << 16) |
         (static_cast<uint64_t>(p.g) << 8) |
         static_cast<uint64_t>(p.b);
}

uint8_t unit_mul8(uint8_t a, uint8_t b) {
  return static_cast<uint8_t>((static_cast<uint32_t>(a) * b + 128u) >> 8);
}

uint8_t modulate2x8(uint8_t a, uint8_t b) {
  const uint32_t value = (static_cast<uint32_t>(a) * b + 64u) >> 7;
  return static_cast<uint8_t>(value > 255u ? 255u : value);
}

int floor_div_256(int value) {
  return value >= 0 ? value / 256 : -((-value + 255) / 256);
}

uint8_t lerp8(uint8_t a, uint8_t b, uint8_t weight) {
  const int delta = floor_div_256((static_cast<int>(b) - a) * weight + 128);
  const int value = static_cast<int>(a) + delta;
  return static_cast<uint8_t>(value < 0 ? 0 : (value > 255 ? 255 : value));
}

uint8_t add_sat8(uint8_t a, uint8_t b) {
  const unsigned value = static_cast<unsigned>(a) + b;
  return static_cast<uint8_t>(value > 255u ? 255u : value);
}

bool legal_count(uint8_t recipe, uint8_t count) {
  if (recipe == 0) return count == 0 || count == 1;
  if (recipe == 6 || recipe == 7) return count == 3;
  return recipe < 8 && count == 2;
}

uint8_t required_status(const Frag& f) {
  uint8_t status = legal_count(f.recipe, f.count) ? 0 : 1;
  if (f.count >= 1) status = static_cast<uint8_t>(status | f.s[0].status);
  if (f.count >= 2) status = static_cast<uint8_t>(status | f.s[1].status);
  if (f.count >= 3) status = static_cast<uint8_t>(status | f.s[2].status);
  if (f.aux_required) status = static_cast<uint8_t>(status | f.aux.status);
  return status;
}

uint64_t r9_expected(const Frag& f) {
  const uint8_t status = required_status(f);
  const uint8_t index = f.count == 0 ? 0 : f.s[0].raw_index;
  if (status != 0) {
    const Plane loud{255, 0, 255, 255, 0, 0};
    return pack_result(status, index, loud);
  }

  const Plane& s0 = f.count == 0 ? f.base : f.s[0];
  const Plane& s1 = f.s[1];
  const Plane& s2 = f.s[2];
  Plane out;

  switch (f.recipe) {
    case 0:
      out = s0;
      break;
    case 1:
      out.r = unit_mul8(s0.r, s1.r);
      out.g = unit_mul8(s0.g, s1.g);
      out.b = unit_mul8(s0.b, s1.b);
      out.a = s0.a;
      break;
    case 2:
      out.r = modulate2x8(s0.r, s1.r);
      out.g = modulate2x8(s0.g, s1.g);
      out.b = modulate2x8(s0.b, s1.b);
      out.a = s0.a;
      break;
    case 3:
      out.r = lerp8(s0.r, s1.r, f.weight);
      out.g = lerp8(s0.g, s1.g, f.weight);
      out.b = lerp8(s0.b, s1.b, f.weight);
      out.a = s0.a;
      break;
    case 4:
      out.r = add_sat8(s0.r, s1.r);
      out.g = add_sat8(s0.g, s1.g);
      out.b = add_sat8(s0.b, s1.b);
      out.a = s0.a;
      break;
    case 5:
      out.r = s0.r;
      out.g = s0.g;
      out.b = s0.b;
      out.a = unit_mul8(s0.a, s1.a);
      break;
    case 6: {
      const uint8_t first_r = modulate2x8(s0.r, s1.r);
      const uint8_t first_g = modulate2x8(s0.g, s1.g);
      const uint8_t first_b = modulate2x8(s0.b, s1.b);
      out.r = unit_mul8(first_r, s2.r);
      out.g = unit_mul8(first_g, s2.g);
      out.b = unit_mul8(first_b, s2.b);
      out.a = s0.a;
      break;
    }
    case 7:
      out.r = modulate2x8(s0.r, s1.r);
      out.g = modulate2x8(s0.g, s1.g);
      out.b = modulate2x8(s0.b, s1.b);
      out.a = unit_mul8(s0.a, s2.a);
      break;
    default:
      break;
  }
  return pack_result(0, index, out);
}

void tick(Dut& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
}

void clear_inputs(Dut& d) {
  d.f_valid_i = 0;
  d.o_ready_i = 1;
  d.f_sample_count_i = 0;
  d.f_recipe_i = 0;
  d.f_weight_i = 0;
  d.f_aux_required_i = 0;
  d.f_base_rgb_i = 0;
  d.f_base_a_i = 0;
  d.f_tag_i = 0;
  d.f_s0_i = 0;
  d.f_s1_i = 0;
  d.f_s2_i = 0;
  d.f_aux_i = 0;
#ifndef MATERIAL_V3_MUTANT_BUILD
  d.f_slot_i = 0;
  d.src_s0_i = 0;
  d.src_s1_i = 0;
  d.src_s2_i = 0;
  d.src_aux_i = 0;
#endif
}

void reset(Dut& d) {
  clear_inputs(d);
  d.rst_n = 0;
  for (int i = 0; i < 4; ++i) tick(d);
  d.rst_n = 1;
  d.eval();
}

void drive_frag(Dut& d, const Frag& f) {
  d.f_valid_i = 1;
  d.f_sample_count_i = f.count;
  d.f_recipe_i = f.recipe;
  d.f_weight_i = f.weight;
  d.f_aux_required_i = f.aux_required ? 1 : 0;
  d.f_base_rgb_i = (static_cast<uint32_t>(f.base.r) << 16) |
                   (static_cast<uint32_t>(f.base.g) << 8) | f.base.b;
  d.f_base_a_i = f.base.a;
  d.f_tag_i = f.tag;
  d.f_s0_i = pack_plane(f.s[0]);
  d.f_s1_i = pack_plane(f.s[1]);
  d.f_s2_i = pack_plane(f.s[2]);
  d.f_aux_i = pack_plane(f.aux);
}

#ifndef MATERIAL_V3_MUTANT_BUILD

namespace mat = zref::material;

struct Run {
  std::map<uint16_t, Got> by_tag;
  int accepted = 0;
  int retired = 0;
  int duplicate_tags = 0;
  int premature_idle = 0;
  uint32_t refused_material = 0;
  uint32_t saturated_add = 0;
  uint32_t saturated_mod2 = 0;
  uint32_t jobs_accepted = 0;
  uint32_t jobs_completed = 0;
  uint32_t phases_issued = 0;
  uint32_t phases_completed = 0;
  uint32_t jobs[8] = {0};
  int recipe_alias_mismatches = 0;
};

uint32_t recipe_alias(const Dut& d, int recipe) {
  switch (recipe) {
    case 0: return d.jobs_recipe_0_o;
    case 1: return d.jobs_recipe_1_o;
    case 2: return d.jobs_recipe_2_o;
    case 3: return d.jobs_recipe_3_o;
    case 4: return d.jobs_recipe_4_o;
    case 5: return d.jobs_recipe_5_o;
    case 6: return d.jobs_recipe_6_o;
    default: return d.jobs_recipe_7_o;
  }
}

Run run_batch(const std::vector<Frag>& batch, bool stall_output = false) {
  Dut d;
  reset(d);
  Run run;
  std::size_t next = 0;

  constexpr int kMaxCycles = 400000;
  for (int cycle = 0; cycle < kMaxCycles; ++cycle) {
    if (next < batch.size()) drive_frag(d, batch[next]);
    else d.f_valid_i = 0;
    d.o_ready_i = stall_output ? ((cycle % 5) != 1 && (cycle % 7) != 3) : 1;
    d.eval();

    const bool accepted = d.f_valid_i && d.f_ready_o;
    const bool retired = d.o_valid_o && d.o_ready_i;
    if (run.accepted > run.retired && d.idle_o) ++run.premature_idle;

    if (retired) {
      Got got;
      got.result = static_cast<uint64_t>(d.o_result_o);
      got.tag = static_cast<uint16_t>(d.o_tag_o);
      got.refused = d.o_refused_o != 0;
      if (!run.by_tag.emplace(got.tag, got).second) ++run.duplicate_tags;
      ++run.retired;
    }

    tick(d);
    if (accepted) {
      ++next;
      ++run.accepted;
    }

    d.eval();
    if (next == batch.size() && run.retired == static_cast<int>(batch.size()) &&
        d.idle_o)
      break;
  }

  run.refused_material = d.refused_material_o;
  run.saturated_add = d.saturated_add_o;
  run.saturated_mod2 = d.saturated_mul2x_o;
  run.jobs_accepted = d.jobs_accepted_o;
  run.jobs_completed = d.jobs_completed_o;
  run.phases_issued = d.phases_issued_o;
  run.phases_completed = d.phases_completed_o;
  for (int i = 0; i < 8; ++i) {
    run.jobs[i] = d.jobs_by_recipe_o[i];
    if (recipe_alias(d, i) != run.jobs[i]) ++run.recipe_alias_mismatches;
  }
  return run;
}

bool has_zero_statuses(const Frag& f) {
  return f.s[0].status == 0 && f.s[1].status == 0 && f.s[2].status == 0 &&
         (!f.aux_required || f.aux.status == 0);
}

uint64_t zref_rgba(const Frag& f) {
  mat::Sample samples[3];
  samples[0].r = f.s[0].r;
  samples[0].g = f.s[0].g;
  samples[0].b = f.s[0].b;
  samples[0].a = f.s[0].a;
  samples[1].r = f.s[1].r;
  samples[1].g = f.s[1].g;
  samples[1].b = f.s[1].b;
  samples[1].a = f.s[1].a;
  samples[2].r = f.s[2].r;
  samples[2].g = f.s[2].g;
  samples[2].b = f.s[2].b;
  samples[2].a = f.s[2].a;
  mat::Sample base;
  base.r = f.base.r;
  base.g = f.base.g;
  base.b = f.base.b;
  base.a = f.base.a;
  const mat::Out out = mat::combine(f.recipe, f.weight, samples, f.count,
                                    base, f.tag, nullptr);
  return (static_cast<uint64_t>(out.a) << 24) |
         (static_cast<uint64_t>(out.r) << 16) |
         (static_cast<uint64_t>(out.g) << 8) |
         static_cast<uint64_t>(out.b);
}

void compare_batch(const char* name, const std::vector<Frag>& batch,
                   const Run& run, bool compare_reference) {
  int missing = 0;
  int mismatched = 0;
  int split_mismatched = 0;
  int reference_mismatched = 0;

  for (const Frag& f : batch) {
    const auto it = run.by_tag.find(f.tag);
    if (it == run.by_tag.end()) {
      ++missing;
      continue;
    }
    const uint64_t want = r9_expected(f);
    if (it->second.result != want) {
      if (mismatched == 0) {
        std::printf("  first %s mismatch tag=%u recipe=%u count=%u want=%012llX got=%012llX\n",
                    name, f.tag, f.recipe, f.count,
                    static_cast<unsigned long long>(want),
                    static_cast<unsigned long long>(it->second.result));
      }
      ++mismatched;
    }
    if (it->second.refused != (((want >> 40) & 1u) != 0)) ++split_mismatched;

    if (compare_reference && legal_count(f.recipe, f.count) &&
        has_zero_statuses(f)) {
      const uint64_t local_rgba = want & 0xFFFFFFFFull;
      if (zref_rgba(f) != local_rgba) ++reference_mismatched;
    }
  }

  check(run.accepted == static_cast<int>(batch.size()),
        "every offered fragment was accepted", batch.size(), run.accepted);
  check(run.retired == static_cast<int>(batch.size()),
        "every accepted fragment retired", batch.size(), run.retired);
  check(run.jobs_accepted == batch.size(),
        "jobs_accepted counts the input handshake exactly", batch.size(),
        run.jobs_accepted);
  check(run.jobs_completed == batch.size(),
        "jobs_completed counts the output handshake exactly", batch.size(),
        run.jobs_completed);
  check(run.jobs_accepted == run.jobs_completed,
        "drain balances accepted and completed jobs", run.jobs_accepted,
        run.jobs_completed);
  check(run.phases_issued == run.phases_completed,
        "drain balances physical phase launches and writebacks",
        run.phases_issued, run.phases_completed);
  check(run.recipe_alias_mismatches == 0,
        "all eight scalar recipe aliases equal jobs_by_recipe_o", 0,
        run.recipe_alias_mismatches);
  check(missing == 0, "no tag was lost", 0, missing);
  check(run.duplicate_tags == 0, "no tag retired twice", 0, run.duplicate_tags);
  check(mismatched == 0, "every typed result matches independent R9", 0,
        mismatched);
  check(split_mismatched == 0, "o_refused is exactly status[0]", 0,
        split_mismatched);
  check(run.premature_idle == 0,
        "idle never asserted while accepted work remained", 0,
        run.premature_idle);
  if (compare_reference)
    check(reference_mismatched == 0,
          "corrected zref::material::combine agrees with R9 arithmetic", 0,
          reference_mismatched);
}

Plane plane(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
            uint8_t index = 0, uint8_t status = 0) {
  Plane p;
  p.r = r;
  p.g = g;
  p.b = b;
  p.a = a;
  p.raw_index = index;
  p.status = status;
  return p;
}

uint8_t legal_count_for(uint8_t recipe) {
  if (recipe == 0) return 1;
  return recipe >= 6 ? 3 : 2;
}

void test_r9_arithmetic_and_stale_separators() {
  std::vector<Frag> batch;
  uint16_t tag = 1;

  // Focused vectors for every superseded arithmetic.  Their expected values
  // are also checked explicitly so a mistaken scalar helper cannot merely agree
  // with a mistaken RTL implementation.
  Frag alpha;
  alpha.recipe = 1;
  alpha.count = 2;
  alpha.s[0] = plane(200, 129, 1, 201, 0x91);
  alpha.s[1] = plane(128, 255, 64, 0);
  alpha.tag = tag++;
  batch.push_back(alpha);

  Frag mod2;
  mod2.recipe = 2;
  mod2.count = 2;
  mod2.s[0] = plane(1, 127, 255, 203, 0x92);
  mod2.s[1] = plane(64, 128, 255, 17);
  mod2.tag = tag++;
  batch.push_back(mod2);

  Frag lerp_down;
  lerp_down.recipe = 3;
  lerp_down.count = 2;
  lerp_down.weight = 128;
  lerp_down.s[0] = plane(200, 200, 200, 177, 0x93);
  lerp_down.s[1] = plane(199, 199, 199, 1);
  lerp_down.tag = tag++;
  batch.push_back(lerp_down);

  Frag add;
  add.recipe = 4;
  add.count = 2;
  add.s[0] = plane(200, 127, 1, 211, 0x94);
  add.s[1] = plane(100, 128, 2, 99);
  add.tag = tag++;
  batch.push_back(add);

  Frag mask;
  mask.recipe = 5;
  mask.count = 2;
  mask.s[0] = plane(11, 22, 33, 200, 0x95);
  mask.s[1] = plane(255, 0, 0, 128);
  mask.tag = tag++;
  batch.push_back(mask);

  Frag detail_light;
  detail_light.recipe = 6;
  detail_light.count = 3;
  detail_light.s[0] = plane(1, 1, 1, 173, 0x96);
  detail_light.s[1] = plane(64, 64, 64, 2);
  detail_light.s[2] = plane(255, 255, 255, 3);
  detail_light.tag = tag++;
  batch.push_back(detail_light);

  Frag detail_mask;
  detail_mask.recipe = 7;
  detail_mask.count = 3;
  detail_mask.s[0] = plane(1, 1, 1, 240, 0x97);
  detail_mask.s[1] = plane(64, 64, 64, 7);
  detail_mask.s[2] = plane(0, 0, 0, 128);
  detail_mask.tag = tag++;
  batch.push_back(detail_mask);

  const Run focused = run_batch(batch, true);
  compare_batch("focused R9", batch, focused, true);
  check((focused.by_tag.at(alpha.tag).result >> 24 & 0xFFu) == 201,
        "MODULATE keeps sample-0 alpha", 201,
        focused.by_tag.at(alpha.tag).result >> 24 & 0xFFu);
  check((focused.by_tag.at(mod2.tag).result >> 16 & 0xFFu) == 1,
        "MODULATE2X single rounding distinguishes 1*64", 1,
        focused.by_tag.at(mod2.tag).result >> 16 & 0xFFu);
  check((focused.by_tag.at(lerp_down.tag).result >> 16 & 0xFFu) == 200,
        "negative LERP tie rounds toward positive infinity", 200,
        focused.by_tag.at(lerp_down.tag).result >> 16 & 0xFFu);
  check((focused.by_tag.at(add.tag).result >> 24 & 0xFFu) == 211,
        "ADD_SAT does not add alpha", 211,
        focused.by_tag.at(add.tag).result >> 24 & 0xFFu);
  check((focused.by_tag.at(mask.tag).result >> 24 & 0xFFu) == 100,
        "MASK alpha is continuous unit multiplication", 100,
        focused.by_tag.at(mask.tag).result >> 24 & 0xFFu);
  check((focused.by_tag.at(detail_light.tag).result >> 16 & 0xFFu) == 1,
        "DETAIL_LIGHT first layer is MODULATE2X", 1,
        focused.by_tag.at(detail_light.tag).result >> 16 & 0xFFu);
  check((focused.by_tag.at(detail_mask.tag).result >> 16 & 0xFFu) == 1,
        "DETAIL_MASK first layer is MODULATE2X", 1,
        focused.by_tag.at(detail_mask.tag).result >> 16 & 0xFFu);

  // Corner-heavy differential.  Every one of 0,1,127,128,254,255 appears in
  // every operand/channel position and as LERP weight.
  static const uint8_t corners[6] = {0, 1, 127, 128, 254, 255};
  batch.clear();
  for (uint8_t recipe = 0; recipe < 8; ++recipe) {
    for (int i = 0; i < 96; ++i) {
      Frag f;
      f.recipe = recipe;
      f.count = legal_count_for(recipe);
      if (recipe == 0 && (i & 1) == 0) f.count = 0;
      f.weight = corners[(i + 4) % 6];
      f.s[0] = plane(corners[(i + 0) % 6], corners[(i + 1) % 6],
                     corners[(i + 2) % 6], corners[(i + 3) % 6],
                     static_cast<uint8_t>(0x40 + recipe));
      f.s[1] = plane(corners[(i + 3) % 6], corners[(i + 4) % 6],
                     corners[(i + 5) % 6], corners[(i + 0) % 6], 0xE1);
      f.s[2] = plane(corners[(i + 5) % 6], corners[(i + 2) % 6],
                     corners[(i + 1) % 6], corners[(i + 4) % 6], 0xE2);
      f.aux = plane(0xAA, 0xBB, 0xCC, 0xDD, 0xE3, 0);
      f.base = plane(corners[(i + 1) % 6], corners[(i + 2) % 6],
                     corners[(i + 3) % 6], corners[(i + 4) % 6]);
      f.tag = tag++;
      batch.push_back(f);
    }
  }
  const Run corners_run = run_batch(batch, true);
  compare_batch("corner sweep", batch, corners_run, true);
}

void test_exact_counts_loud_errors_and_required_status() {
  std::vector<Frag> counts;
  uint16_t tag = 0x1000;
  int malformed = 0;
  for (uint8_t recipe = 0; recipe < 8; ++recipe) {
    for (uint8_t count = 0; count < 4; ++count) {
      Frag f;
      f.recipe = recipe;
      f.count = count;
      f.s[0] = plane(20, 30, 40, 50, static_cast<uint8_t>(0x80 + recipe));
      f.s[1] = plane(60, 70, 80, 90, 0xD1);
      f.s[2] = plane(100, 110, 120, 130, 0xD2);
      f.base = plane(7, 8, 9, 10);
      f.tag = tag++;
      if (!legal_count(recipe, count)) ++malformed;
      counts.push_back(f);
    }
  }
  const Run count_run = run_batch(counts, true);
  compare_batch("exact-count table", counts, count_run, false);
  check(count_run.refused_material == static_cast<uint32_t>(malformed),
        "every and only exact-count mismatch is counted", malformed,
        count_run.refused_material);

  int non_loud = 0;
  for (const Frag& f : counts) {
    if (legal_count(f.recipe, f.count)) continue;
    const uint64_t result = count_run.by_tag.at(f.tag).result;
    if ((result & 0xFFFFFFFFull) != 0xFFFF00FFull ||
        ((result >> 40) & 1u) == 0)
      ++non_loud;
  }
  check(non_loud == 0,
        "every malformed material retires loud magenta with SOURCE_REFUSED", 0,
        non_loud);

  std::vector<Frag> status;
  tag = 0x2000;

  Frag count0;
  count0.recipe = 0;
  count0.count = 0;
  count0.s[0].status = 0x02;
  count0.s[1].status = 0x04;
  count0.s[2].status = 0x08;
  count0.aux.status = 0x10;
  count0.base = plane(9, 8, 7, 6);
  count0.tag = tag++;
  status.push_back(count0);

  Frag two;
  two.recipe = 1;
  two.count = 2;
  two.s[0] = plane(10, 20, 30, 40, 0xA5, 0x02);
  two.s[1] = plane(50, 60, 70, 80, 0xB6, 0x04);
  two.s[2] = plane(90, 100, 110, 120, 0xC7, 0x08);
  two.aux = plane(1, 2, 3, 4, 0xD8, 0x10);
  two.tag = tag++;
  status.push_back(two);

  Frag three_aux = two;
  three_aux.recipe = 6;
  three_aux.count = 3;
  three_aux.aux_required = true;
  three_aux.tag = tag++;
  status.push_back(three_aux);

  Frag count0_aux = count0;
  count0_aux.aux_required = true;
  count0_aux.aux.status = 0x20;
  count0_aux.tag = tag++;
  status.push_back(count0_aux);

  Frag reserved;
  reserved.recipe = 0;
  reserved.count = 1;
  reserved.s[0] = plane(1, 2, 3, 4, 0x77, 0x02);
  reserved.tag = tag++;
  status.push_back(reserved);

  const Run status_run = run_batch(status, true);
  compare_batch("required status", status, status_run, false);
  check((status_run.by_tag.at(count0.tag).result >> 40) == 0,
        "count zero ignores every unrequested TMU/AUX status", 0,
        status_run.by_tag.at(count0.tag).result >> 40);
  check((status_run.by_tag.at(two.tag).result >> 40) == 0x06,
        "count two ORs only sample 0 and sample 1 status", 0x06,
        status_run.by_tag.at(two.tag).result >> 40);
  check((status_run.by_tag.at(three_aux.tag).result >> 40) == 0x1E,
        "count three plus AUX ORs exactly four required statuses", 0x1E,
        status_run.by_tag.at(three_aux.tag).result >> 40);
  check((status_run.by_tag.at(two.tag).result >> 32 & 0xFFu) == 0xA5,
        "sample-0 raw index survives even a refused result", 0xA5,
        status_run.by_tag.at(two.tag).result >> 32 & 0xFFu);
  check((status_run.by_tag.at(count0.tag).result >> 32 & 0xFFu) == 0,
        "count-zero raw index is canonical zero", 0,
        status_run.by_tag.at(count0.tag).result >> 32 & 0xFFu);
  check(!status_run.by_tag.at(reserved.tag).refused,
        "reserved nonzero status is loud but o_refused remains status[0]", 0,
        status_run.by_tag.at(reserved.tag).refused ? 1 : 0);
}

void test_aux_is_status_only_and_sample2_is_real() {
  std::vector<Frag> batch;
  uint16_t tag = 0x2800;

  Frag a;
  a.recipe = 6;
  a.count = 3;
  a.aux_required = true;
  a.s[0] = plane(100, 120, 140, 201, 0x5A);
  a.s[1] = plane(128, 128, 128, 17);
  a.s[2] = plane(255, 200, 64, 99);
  a.aux = plane(0, 0, 0, 33, 0x11, 0);
  a.tag = tag++;
  batch.push_back(a);

  Frag b = a;
  b.aux = plane(255, 254, 253, 244, 0xEE, 0);  // poison tag/strength/payload
  b.tag = tag++;
  batch.push_back(b);

  Frag c = a;
  c.s[2] = plane(64, 64, 64, 88);
  c.tag = tag++;
  batch.push_back(c);

  Frag d = a;
  d.recipe = 7;
  d.s[2].a = 128;
  d.aux.a = 1;
  d.tag = tag++;
  batch.push_back(d);

  const Run run = run_batch(batch, true);
  compare_batch("AUX/sample-2 separation", batch, run, true);
  check((run.by_tag.at(a.tag).result & 0xFFFFFFFFull) ==
            (run.by_tag.at(b.tag).result & 0xFFFFFFFFull),
        "changing successful AUX tag/strength/data cannot change RGBA", 1,
        (run.by_tag.at(a.tag).result & 0xFFFFFFFFull) ==
                (run.by_tag.at(b.tag).result & 0xFFFFFFFFull)
            ? 1
            : 0);
  check((run.by_tag.at(a.tag).result & 0xFFFFFFull) !=
            (run.by_tag.at(c.tag).result & 0xFFFFFFull),
        "changing true sample 2 changes DETAIL_LIGHT RGB", 1,
        (run.by_tag.at(a.tag).result & 0xFFFFFFull) !=
                (run.by_tag.at(c.tag).result & 0xFFFFFFull)
            ? 1
            : 0);
  check((run.by_tag.at(d.tag).result >> 24 & 0xFFu) == 101,
        "DETAIL_MASK alpha consumes sample 2: (201*128+128)>>8", 101,
        run.by_tag.at(d.tag).result >> 24 & 0xFFu);
}

void test_rtl_add_lerp_boundary_cross_products() {
  static const uint8_t boundary[7] = {0, 1, 127, 128, 129, 254, 255};
  std::vector<Frag> batch;
  uint16_t tag = 0x2A00;
  uint32_t expected_add_saturation = 0;

  for (uint8_t a : boundary) {
    for (uint8_t b : boundary) {
      Frag add;
      add.recipe = 4;
      add.count = 2;
      add.s[0] = plane(a, 0, 0, 0xD1, 0xA5);
      add.s[1] = plane(b, 0, 0, 0xF2);
      add.tag = tag++;
      if (static_cast<unsigned>(a) + b > 255u) ++expected_add_saturation;
      batch.push_back(add);

      for (uint8_t weight : boundary) {
        Frag lerp;
        lerp.recipe = 3;
        lerp.count = 2;
        lerp.weight = weight;
        lerp.s[0] = plane(a, 255 - a, a, 0xC3, 0xB6);
        lerp.s[1] = plane(b, 255 - b, b, 0x24);
        lerp.tag = tag++;
        batch.push_back(lerp);
      }
    }
  }

  // Explicit signed half ties in both directions, beyond the Cartesian set.
  Frag positive_tie;
  positive_tie.recipe = 3;
  positive_tie.count = 2;
  positive_tie.weight = 128;
  positive_tie.s[0] = plane(199, 0, 0, 0xA1, 0xC7);
  positive_tie.s[1] = plane(200, 0, 0, 0x02);
  positive_tie.tag = tag++;
  batch.push_back(positive_tie);

  Frag negative_tie = positive_tie;
  negative_tie.s[0].r = 200;
  negative_tie.s[1].r = 199;
  negative_tie.tag = tag++;
  batch.push_back(negative_tie);

  const Run run = run_batch(batch, true);
  compare_batch("ADD/LERP boundary matrix", batch, run, true);
  check(run.saturated_add == expected_add_saturation,
        "RTL ADD saturation count matches every boundary pair",
        expected_add_saturation, run.saturated_add);
  check((run.by_tag.at(positive_tie.tag).result >> 16 & 0xFFu) == 200,
        "positive signed LERP tie rounds upward", 200,
        run.by_tag.at(positive_tie.tag).result >> 16 & 0xFFu);
  check((run.by_tag.at(negative_tie.tag).result >> 16 & 0xFFu) == 200,
        "negative signed LERP tie rounds toward positive infinity", 200,
        run.by_tag.at(negative_tie.tag).result >> 16 & 0xFFu);
}

void test_exact_saturation_accounting_under_stalls() {
  std::vector<Frag> batch;
  uint16_t tag = 0x3200;

  auto push = [&](uint8_t recipe, uint8_t count, Plane s0, Plane s1,
                  Plane s2 = {}) {
    Frag f;
    f.recipe = recipe;
    f.count = count;
    f.s[0] = s0;
    f.s[1] = s1;
    f.s[2] = s2;
    f.tag = tag++;
    batch.push_back(f);
  };

  push(4, 2, plane(200, 1, 2, 255), plane(100, 2, 3, 255));  // ADD saturates
  push(4, 2, plane(127, 1, 2, 255), plane(128, 2, 3, 255));  // exact 255
  push(4, 2, plane(1, 2, 3, 255), plane(2, 3, 4, 255));      // alpha ignored
  push(2, 2, plane(200, 200, 200, 17), plane(200, 200, 200, 99));
  push(2, 2, plane(128, 128, 128, 17), plane(255, 255, 255, 99));  // exact 255
  push(6, 3, plane(255, 255, 255, 17), plane(255, 255, 255, 99),
       plane(128, 128, 128, 3));
  push(7, 3, plane(255, 255, 255, 240), plane(255, 255, 255, 99),
       plane(0, 0, 0, 128));

  Frag source_error;
  source_error.recipe = 2;
  source_error.count = 2;
  source_error.s[0] = plane(255, 255, 255, 17, 0, 1);
  source_error.s[1] = plane(255, 255, 255, 99);
  source_error.tag = tag++;
  batch.push_back(source_error);

  Frag malformed;
  malformed.recipe = 2;
  malformed.count = 3;
  malformed.s[0] = plane(255, 255, 255, 17);
  malformed.s[1] = plane(255, 255, 255, 99);
  malformed.s[2] = plane(255, 255, 255, 33);
  malformed.tag = tag++;
  batch.push_back(malformed);

  // Force context reuse after saturating jobs; stale scratch flags must not
  // turn later exact/non-saturating MODULATE2X fragments into extra events.
  for (int i = 0; i < 16; ++i)
    push(2, 2, plane(1, 2, 3, 4), plane(64, 64, 64, 5));

  const Run run = run_batch(batch, true);
  compare_batch("saturation accounting", batch, run, true);
  check(run.saturated_add == 1,
        "ADD_SAT counts only the one RGB-overflow fragment", 1,
        run.saturated_add);
  check(run.saturated_mod2 == 3,
        "MODULATE2X saturation counts recipe 2 and both detail first layers once",
        3, run.saturated_mod2);
}

void test_phase_and_product_cadence() {
  static const uint8_t phases[8] = {1, 2, 2, 2, 1, 1, 3, 2};
  static const uint8_t jobs[8] = {0, 3, 3, 3, 0, 1, 6, 4};
  std::vector<Frag> batch;
  uint16_t tag = 0x3000;
  uint32_t expected_phases = 0;
  for (uint8_t recipe = 0; recipe < 8; ++recipe) {
    Frag f;
    f.recipe = recipe;
    f.count = legal_count_for(recipe);
    f.s[0] = plane(20, 30, 40, 50, static_cast<uint8_t>(0x70 + recipe));
    f.s[1] = plane(60, 70, 80, 90);
    f.s[2] = plane(100, 110, 120, 130);
    f.tag = tag++;
    batch.push_back(f);
    expected_phases += phases[recipe];
  }
  const Run legal = run_batch(batch, false);
  compare_batch("cadence legal", batch, legal, true);
  check(legal.phases_issued == expected_phases,
        "legal recipes issue exactly the frozen 1/2/3 phase schedule",
        expected_phases, legal.phases_issued);
  check(legal.phases_completed == expected_phases,
        "legal recipes complete exactly the frozen phase demand",
        expected_phases, legal.phases_completed);
  for (int recipe = 0; recipe < 8; ++recipe) {
    char what[96];
    std::snprintf(what, sizeof what,
                  "recipe %d issues exactly its meaningful product jobs", recipe);
    check(legal.jobs[recipe] == jobs[recipe], what, jobs[recipe],
          legal.jobs[recipe]);
  }

  batch.clear();
  for (uint8_t recipe = 0; recipe < 8; ++recipe) {
    Frag f;
    f.recipe = recipe;
    f.count = recipe == 0 ? 2 : 0;  // illegal for every row
    f.tag = tag++;
    batch.push_back(f);
  }
  const Run malformed = run_batch(batch, false);
  compare_batch("cadence malformed", batch, malformed, false);
  check(malformed.phases_issued == 8,
        "every malformed material issues one refusal phase", 8,
        malformed.phases_issued);
  check(malformed.phases_completed == 8,
        "every malformed material completes one refusal phase", 8,
        malformed.phases_completed);
  uint32_t malformed_jobs = 0;
  for (uint32_t value : malformed.jobs) malformed_jobs += value;
  check(malformed_jobs == 0,
        "malformed materials launch no product jobs", 0, malformed_jobs);

  batch.clear();
  for (uint8_t recipe = 0; recipe < 8; ++recipe) {
    Frag f;
    f.recipe = recipe;
    f.count = legal_count_for(recipe);
    f.s[0].status = 1;
    f.tag = tag++;
    batch.push_back(f);
  }
  const Run source_error = run_batch(batch, false);
  compare_batch("cadence source error", batch, source_error, false);
  check(source_error.phases_issued == 8,
        "every terminal source error issues one loud phase", 8,
        source_error.phases_issued);
  check(source_error.phases_completed == 8,
        "every terminal source error completes one loud phase", 8,
        source_error.phases_completed);
  uint32_t source_error_jobs = 0;
  for (uint32_t value : source_error.jobs) source_error_jobs += value;
  check(source_error_jobs == 0,
        "terminal source errors launch no product jobs", 0,
        source_error_jobs);
}

void test_one_phase_retirement_is_one_per_clock() {
  Dut d;
  reset(d);
  d.o_ready_i = 0;

  int accepted = 0;
  for (int cycle = 0; cycle < 400 && accepted < 8; ++cycle) {
    Frag f;
    f.recipe = 0;
    f.count = 1;
    f.s[0] = plane(static_cast<uint8_t>(0x20 + accepted), 0x44, 0x66,
                   0x88, static_cast<uint8_t>(0xA0 + accepted));
    f.tag = static_cast<uint16_t>(0x3400 + accepted);
    drive_frag(d, f);
    d.eval();
    const bool take = d.f_ready_o != 0;
    tick(d);
    if (take) ++accepted;
  }
  d.f_valid_i = 0;
  for (int cycle = 0; cycle < 80; ++cycle) tick(d);

  check(accepted == 8, "elastic gate filled all eight contexts", 8, accepted);
  check(d.jobs_accepted_o == 8 && d.jobs_completed_o == 0,
        "closed sink holds eight completed jobs without retiring one", 8,
        d.jobs_accepted_o - d.jobs_completed_o);
  check(d.phases_issued_o == 8 && d.phases_completed_o == 8,
        "the prepared PASSTHRU run contains exactly eight physical phases", 8,
        d.phases_completed_o);

  d.o_ready_i = 1;
  int handshakes = 0;
  int bubbles = 0;
  int wrong = 0;
  for (int cycle = 0; cycle < 8; ++cycle) {
    d.eval();
    if (!d.o_valid_o) {
      ++bubbles;
    } else {
      const int index = static_cast<int>(d.o_tag_o) - 0x3400;
      if (index < 0 || index >= 8 ||
          d.o_rgb_o != static_cast<uint32_t>(0x204466 + (index << 16)) ||
          d.o_a_o != 0x88 || d.o_raw_index_o != 0xA0 + index ||
          d.o_status_o != 0)
        ++wrong;
      ++handshakes;
    }
    tick(d);
  }

  check(bubbles == 0,
        "full one-phase queue retires on eight consecutive clocks", 0, bubbles);
  check(handshakes == 8, "all eight consecutive cycles handshake", 8,
        handshakes);
  check(wrong == 0, "elastic reload preserves every held payload and tag", 0,
        wrong);
  check(d.jobs_completed_o == 8,
        "consecutive output handshakes complete each job once", 8,
        d.jobs_completed_o);
  check(d.phases_issued_o == 8 && d.phases_completed_o == 8,
        "elastic output reload neither reissues nor recompletes a phase", 8,
        d.phases_completed_o);
}

void test_timing4_eight_context_pipeline() {
  Dut d;
  reset(d);
  d.o_ready_i = 1;

  int accepted = 0;
  int retired = 0;
  int input_bubbles = 0;
  int output_bubbles = 0;
  int first_output_cycle = -1;
  int last_output_cycle = -1;
  int wrong = 0;
  int accept_cycle[8] = {0};
  int retire_cycle[8] = {0};

  for (int cycle = 0; cycle < 80 && retired < 8; ++cycle) {
    if (accepted < 8) {
      Frag f;
      f.recipe = 0;
      f.count = 1;
      f.s[0] = plane(static_cast<uint8_t>(0x30 + accepted), 0x51, 0x72,
                     0x93, static_cast<uint8_t>(0xB0 + accepted));
      f.tag = static_cast<uint16_t>(0x4400 + accepted);
      drive_frag(d, f);
    } else {
      d.f_valid_i = 0;
    }
    d.o_ready_i = 1;
    d.eval();

    if (accepted < 8) {
      if (d.f_ready_o) {
        accept_cycle[accepted] = cycle;
        ++accepted;
      } else {
        ++input_bubbles;
      }
    }

    if (d.o_valid_o) {
      const int index = static_cast<int>(d.o_tag_o) - 0x4400;
      if (first_output_cycle < 0) first_output_cycle = cycle;
      if (last_output_cycle >= 0 && cycle != last_output_cycle + 1)
        output_bubbles += cycle - last_output_cycle - 1;
      last_output_cycle = cycle;
      if (index < 0 || index >= 8) {
        ++wrong;
      } else {
        retire_cycle[index] = cycle;
        if (d.o_rgb_o != static_cast<uint32_t>(0x305172 + (index << 16)) ||
            d.o_a_o != 0x93 || d.o_raw_index_o != 0xB0 + index ||
            d.o_status_o != 0)
          ++wrong;
      }
      ++retired;
    }
    tick(d);
  }
  d.f_valid_i = 0;

  int latency_errors = 0;
  for (int i = 0; i < 8; ++i)
    if (retire_cycle[i] - accept_cycle[i] != 11) ++latency_errors;

  check(accepted == 8 && input_bubbles == 0,
        "NCTX=8 accepts eight back-to-back phases without a bubble", 8,
        accepted - input_bubbles);
  check(retired == 8 && output_bubbles == 0,
        "Timing4 pipeline retires eight results on consecutive clocks", 8,
        retired - output_bubbles);
  check(first_output_cycle == 11 && latency_errors == 0,
        "S/F pipeline has exact eleven-cycle accept-to-visible latency", 11,
        first_output_cycle);
  check(wrong == 0,
        "S/F pipeline preserves every ordered context, phase, and result", 0,
        wrong);
  check(d.jobs_accepted_o == 8 && d.jobs_completed_o == 8 &&
            d.phases_issued_o == 8 && d.phases_completed_o == 8,
        "S/F pipeline counters close exactly after eight-context traffic", 8,
        d.jobs_completed_o);

  tick(d);
  d.eval();
  check(d.idle_o != 0,
        "S/F pipeline returns to structural idle after complete drain", 1,
        d.idle_o ? 1 : 0);
}

// --------------------------------------------------------------------------
// M3: THE S+F THROUGHPUT CALENDAR, MEASURED RATHER THAN ASSUMED.
//
// The two tests above cover ONE-PHASE traffic: eight contexts accepted without a
// bubble, eight results retired on consecutive clocks, eleven-cycle latency.
// That is exactly the case which cannot see the trap the owner brief names in
// section 8.1 -- a one-phase job never rides the continuation queue at all.
//
// A multi-phase job does. It launches at Q, walks R/D/S/O/M/F/WB, writes scratch,
// is re-queued as a continuation, and only then can its NEXT phase launch. Adding
// the S and F registers lengthens that loop, and with only eight contexts a
// recurrence longer than eight clocks caps sustained phase throughput at 8/R
// phases per clock no matter how well each individual stage pipelines.
//
// So measure R directly, and measure the sustained rates it predicts. The
// numbers are printed, not merely asserted, because section 21.5 asks for the
// calendar itself rather than a pass mark.
// --------------------------------------------------------------------------
struct PhaseTrace {
  std::vector<int> issue_cycles;
  std::vector<int> complete_cycles;
  std::vector<int> output_cycles;
  int first_accept = -1;
  int last_accept = -1;
  int accepted = 0;
  int retired = 0;
  int max_inflight = 0;
  int drain_cycle = -1;
};

// stall_period 0 means the sink is always ready; otherwise the sink refuses on
// every stall_period-th cycle, which is what exercises DONE backlog and the
// completion/refill coincidence the brief asks for.
PhaseTrace trace_batch(const std::vector<Frag>& batch, int stall_period) {
  Dut d;
  reset(d);
  PhaseTrace trace;
  std::size_t next = 0;
  uint32_t prev_issued = 0;
  uint32_t prev_completed = 0;

  for (int cycle = 0; cycle < 20000; ++cycle) {
    if (next < batch.size()) drive_frag(d, batch[next]);
    else d.f_valid_i = 0;
    d.o_ready_i = (stall_period == 0 || (cycle % stall_period) != 0) ? 1 : 0;
    d.eval();

    const bool accepted = d.f_valid_i && d.f_ready_o;
    const bool retired = d.o_valid_o && d.o_ready_i;
    if (retired) trace.output_cycles.push_back(cycle);

    tick(d);
    if (accepted) {
      if (trace.first_accept < 0) trace.first_accept = cycle;
      trace.last_accept = cycle;
      ++next;
      ++trace.accepted;
    }
    if (retired) ++trace.retired;
    const int inflight = trace.accepted - trace.retired;
    if (inflight > trace.max_inflight) trace.max_inflight = inflight;

    d.eval();
    for (uint32_t i = prev_issued; i < d.phases_issued_o; ++i)
      trace.issue_cycles.push_back(cycle);
    for (uint32_t i = prev_completed; i < d.phases_completed_o; ++i)
      trace.complete_cycles.push_back(cycle);
    prev_issued = d.phases_issued_o;
    prev_completed = d.phases_completed_o;

    if (next == batch.size() &&
        trace.retired == static_cast<int>(batch.size()) && d.idle_o) {
      trace.drain_cycle = cycle;
      break;
    }
  }
  return trace;
}

Frag multi_phase_frag(uint8_t recipe, uint16_t tag, uint8_t seed) {
  Frag f;
  f.recipe = recipe;
  // DETAIL_LIGHT and DETAIL_MASK need three real samples; MODULATE needs two.
  f.count = (recipe == 6 || recipe == 7) ? 3 : 2;
  f.weight = 0x40;
  f.s[0] = plane(static_cast<uint8_t>(0x20 + seed), 0x40, 0x60, 0x80,
                 static_cast<uint8_t>(seed));
  f.s[1] = plane(0x30, 0x50, 0x70, 0x90, 0);
  f.s[2] = plane(0x38, 0x58, 0x78, 0x98, 0);
  f.base = plane(0x11, 0x22, 0x33, 0x44, 0);
  f.tag = tag;
  return f;
}

int steady_recurrence(const std::vector<int>& cycles) {
  // The gap between a context's consecutive phase launches, taken from a lone
  // job so no other context can fill the pipeline in between.
  if (cycles.size() < 2) return -1;
  const int gap = cycles[1] - cycles[0];
  for (std::size_t i = 2; i < cycles.size(); ++i)
    if (cycles[i] - cycles[i - 1] != gap) return -2;
  return gap;
}

void test_timing4_multiphase_recurrence_and_rate() {
  std::printf("\n[M3] S+F throughput calendar, measured on this RTL\n");

  // ---- 1. Continuation recurrence, measured on a lone multi-phase job ----
  int recurrence[3] = {0, 0, 0};
  const uint8_t lone_recipes[3] = {1, 6, 7};  // MODULATE, DETAIL_LIGHT, DMASK
  const char* lone_names[3] = {"MODULATE(2ph)", "DETAIL_LIGHT(3ph)",
                               "DETAIL_MASK(2ph)"};
  for (int i = 0; i < 3; ++i) {
    std::vector<Frag> lone{multi_phase_frag(lone_recipes[i], 0x7100, 3)};
    const PhaseTrace t = trace_batch(lone, 0);
    recurrence[i] = steady_recurrence(t.issue_cycles);
    std::printf("[M3]   %-18s phases=%d recurrence=%d accept->first_out=%d "
                "accept->drain=%d\n",
                lone_names[i], static_cast<int>(t.issue_cycles.size()),
                recurrence[i],
                t.output_cycles.empty() ? -1
                                        : t.output_cycles.front() - t.first_accept,
                t.drain_cycle - t.first_accept);
  }

  // ---- 2. Saturated multi-phase stream: the rate the recurrence predicts ----
  std::vector<Frag> saturated;
  for (int i = 0; i < 48; ++i)
    saturated.push_back(multi_phase_frag(6, static_cast<uint16_t>(0x7200 + i),
                                         static_cast<uint8_t>(i)));
  const PhaseTrace sat = trace_batch(saturated, 0);
  const int sat_span = sat.issue_cycles.empty()
                           ? 0
                           : sat.issue_cycles.back() - sat.issue_cycles.front() + 1;
  const double sat_rate =
      sat_span > 0 ? static_cast<double>(sat.issue_cycles.size()) / sat_span : 0.0;
  std::printf("[M3]   saturated DETAIL_LIGHT x48: phases=%d span=%d "
              "phases/clk=%.3f max_inflight=%d drain=%d\n",
              static_cast<int>(sat.issue_cycles.size()), sat_span, sat_rate,
              sat.max_inflight, sat.drain_cycle);

  // ---- 3. Alternating recipes and mixed continued/new work ----
  std::vector<Frag> mixed;
  for (int i = 0; i < 48; ++i) {
    const uint8_t recipe = (i % 3 == 0) ? 0 : ((i % 3 == 1) ? 1 : 6);
    Frag f = multi_phase_frag(recipe, static_cast<uint16_t>(0x7300 + i),
                              static_cast<uint8_t>(i));
    if (recipe == 0) f.count = 1;  // one-phase PASSTHRU beside the multi-phase
    mixed.push_back(f);
  }
  const PhaseTrace mix = trace_batch(mixed, 0);
  const int mix_span = mix.issue_cycles.empty()
                           ? 0
                           : mix.issue_cycles.back() - mix.issue_cycles.front() + 1;
  std::printf("[M3]   mixed 1/2/3-phase x48: jobs=%d phases=%d span=%d "
              "phases/clk=%.3f drain=%d\n",
              mix.retired, static_cast<int>(mix.issue_cycles.size()), mix_span,
              mix_span > 0 ? static_cast<double>(mix.issue_cycles.size()) / mix_span
                           : 0.0,
              mix.drain_cycle);

  // ---- 4. DONE backlog and output stalls over the same sequence ----
  const PhaseTrace stalled = trace_batch(saturated, 4);
  std::printf("[M3]   saturated DETAIL_LIGHT x48, sink stalls 1-in-4: "
              "jobs=%d phases=%d max_inflight=%d drain=%d\n",
              stalled.retired, static_cast<int>(stalled.issue_cycles.size()),
              stalled.max_inflight, stalled.drain_cycle);

  // ---- 5. Fast error / count-zero jobs ----
  std::vector<Frag> fast;
  for (int i = 0; i < 16; ++i) {
    Frag f;
    f.recipe = 0;
    f.count = 0;  // count-zero issues no source request at all
    f.base = plane(0x10, 0x20, 0x30, 0x40, 0);
    f.tag = static_cast<uint16_t>(0x7400 + i);
    fast.push_back(f);
  }
  const PhaseTrace quick = trace_batch(fast, 0);
  std::printf("[M3]   count-zero x16: jobs=%d phases=%d drain=%d\n",
              quick.retired, static_cast<int>(quick.issue_cycles.size()),
              quick.drain_cycle);

  // ---- Invariants the measurement must hold, whatever the calendar is ----
  // Every job retires exactly once, and every launched phase is written back.
  check(sat.retired == 48 && mix.retired == 48 && stalled.retired == 48 &&
            quick.retired == 16,
        "every multi-phase, mixed, stalled and count-zero job retires exactly once",
        1, (sat.retired == 48 && mix.retired == 48 && stalled.retired == 48 &&
            quick.retired == 16) ? 1 : 0);
  check(sat.issue_cycles.size() == sat.complete_cycles.size() &&
            mix.issue_cycles.size() == mix.complete_cycles.size() &&
            stalled.issue_cycles.size() == stalled.complete_cycles.size(),
        "issued and completed phase counts close on every measured stream", 1,
        (sat.issue_cycles.size() == sat.complete_cycles.size() &&
         mix.issue_cycles.size() == mix.complete_cycles.size() &&
         stalled.issue_cycles.size() == stalled.complete_cycles.size()) ? 1 : 0);
  check(sat.max_inflight <= 8 && stalled.max_inflight <= 8,
        "context occupancy never exceeds NCTX under saturation or stalls", 1,
        (sat.max_inflight <= 8 && stalled.max_inflight <= 8) ? 1 : 0);
  check(sat.issue_cycles.size() == 144,
        "48 DETAIL_LIGHT jobs launch exactly three physical phases each", 144,
        static_cast<int>(sat.issue_cycles.size()));

  // The recurrence is the number that decides whether a bypass is owed: with
  // NCTX contexts, sustained phase throughput cannot exceed NCTX/recurrence.
  check(recurrence[0] > 0 && recurrence[1] > 0,
        "continuation recurrence is a single stable gap for a lone job", 1,
        (recurrence[0] > 0 && recurrence[1] > 0) ? 1 : 0);
  check(recurrence[1] <= 8,
        "eight contexts can cover the measured continuation recurrence", 1,
        recurrence[1] <= 8 ? 1 : 0);

  // THIS FLOOR IS THE MEASURED TRUTH, NOT THE TARGET, AND THE GAP IS REAL.
  //
  // Eight contexts covering a seven-clock recurrence ought to sustain 1.000
  // phases per clock. Measured here: 0.867. The shortfall is NOT the phase loop
  // -- the WB->Q forwarding above shortened that and barely moved this number,
  // because its empty-queue guard almost never fires while eight contexts are
  // cycling. The limit is the per-context RECYCLE time: a context is released
  // only on its output handshake, so DONE occupancy plus the completion read and
  // response slots sit between its last writeback and its reuse. That is the
  // path section 8.4 forwarding targets, and it is still owed.
  //
  // The floor is set just under the measurement so a real regression is caught
  // while the honest gap stays visible instead of being asserted away.
  check(sat_rate > 0.85,
        "saturated multi-phase traffic holds its measured phase rate", 1,
        sat_rate > 0.85 ? 1 : 0);
  std::printf("[M3]   shortfall vs one phase/clk: %.3f phases/clk "
              "(recycle tail, not the phase loop)\n", 1.0 - sat_rate);
}

void test_held_output_and_structural_idle() {
  Dut d;
  reset(d);
  check(d.idle_o != 0, "reset state is structurally idle", 1,
        d.idle_o ? 1 : 0);
  check(d.jobs_accepted_o == 0 && d.jobs_completed_o == 0 &&
            d.phases_issued_o == 0 && d.phases_completed_o == 0,
        "all accepted/completed job and phase counters reset to zero", 0,
        d.jobs_accepted_o | d.jobs_completed_o |
            d.phases_issued_o | d.phases_completed_o);

  d.o_ready_i = 0;
  int accepted = 0;
  for (int cycle = 0; cycle < 400 && accepted < 8; ++cycle) {
    Frag f;
    f.recipe = 0;
    f.count = 1;
    f.s[0] = plane(static_cast<uint8_t>(accepted + 1), 0x22, 0x33,
                   0xA5, static_cast<uint8_t>(0x80 + accepted));
    f.tag = static_cast<uint16_t>(0x3800 + accepted);
    drive_frag(d, f);
    d.eval();
    const bool take = d.f_ready_o != 0;
    tick(d);
    if (take) ++accepted;
  }
  d.f_valid_i = 0;
  for (int i = 0; i < 80; ++i) tick(d);

  check(accepted == 8,
        "stalled output reserves all eight contexts and no ninth", 8,
        accepted);
  check(d.o_valid_o != 0, "one complete output is held", 1,
        d.o_valid_o ? 1 : 0);
  check(d.f_ready_o == 0, "all contexts remain reserved under output stall", 0,
        d.f_ready_o ? 1 : 0);
  check(d.idle_o == 0, "held output keeps structural idle low", 0,
        d.idle_o ? 1 : 0);
  check(d.jobs_accepted_o == 8 && d.jobs_completed_o == 0,
        "eight jobs are accepted but none completes while output ready is low",
        8, d.jobs_accepted_o - d.jobs_completed_o);
  check(d.phases_issued_o == 8 && d.phases_completed_o == 8,
        "all PASSTHRU phases write back even while retirement is held", 8,
        d.phases_completed_o);

  const uint32_t held_jobs_accepted = d.jobs_accepted_o;
  const uint32_t held_jobs_completed = d.jobs_completed_o;
  const uint32_t held_phases_issued = d.phases_issued_o;
  const uint32_t held_phases_completed = d.phases_completed_o;
  const uint64_t held_result = static_cast<uint64_t>(d.o_result_o);
  const uint16_t held_tag = static_cast<uint16_t>(d.o_tag_o);
  const bool held_refused = d.o_refused_o != 0;
  bool stable = true;
  bool aliases_exact = true;
  for (int cycle = 0; cycle < 64; ++cycle) {
    Frag poison;
    poison.recipe = static_cast<uint8_t>(cycle & 7);
    poison.count = static_cast<uint8_t>(cycle & 3);
    poison.weight = static_cast<uint8_t>(cycle * 17);
    poison.aux_required = true;
    poison.s[0] = plane(255, 0, 255, static_cast<uint8_t>(cycle), 0xEE, 0xFF);
    poison.s[1] = plane(1, 2, 3, 4, 0xDD, 0x80);
    poison.s[2] = plane(5, 6, 7, 8, 0xCC, 0x40);
    poison.aux = plane(9, 10, 11, 12, 0xBB, 0x20);
    poison.tag = static_cast<uint16_t>(0x3C00 + cycle);
    drive_frag(d, poison);
    d.o_ready_i = 0;
    d.eval();
    const uint64_t rebuilt =
        (static_cast<uint64_t>(d.o_status_o) << 40) |
        (static_cast<uint64_t>(d.o_raw_index_o) << 32) |
        (static_cast<uint64_t>(d.o_a_o) << 24) |
        static_cast<uint64_t>(d.o_rgb_o);
    stable = stable && d.o_valid_o &&
             static_cast<uint64_t>(d.o_result_o) == held_result &&
             static_cast<uint16_t>(d.o_tag_o) == held_tag &&
             (d.o_refused_o != 0) == held_refused && !d.f_ready_o && !d.idle_o &&
             d.jobs_accepted_o == held_jobs_accepted &&
             d.jobs_completed_o == held_jobs_completed &&
             d.phases_issued_o == held_phases_issued &&
             d.phases_completed_o == held_phases_completed;
    aliases_exact = aliases_exact && rebuilt == held_result &&
                    (d.o_refused_o != 0) == ((d.o_status_o & 1u) != 0);
    tick(d);
  }
  check(stable,
        "valid&&!ready holds payload, credit, and all completion counters", 1,
        stable ? 1 : 0);
  check(aliases_exact,
        "split outputs remain exact aliases of the held 48-bit result", 1,
        aliases_exact ? 1 : 0);

  d.f_valid_i = 0;
  d.o_ready_i = 1;
  int retired = 0;
  bool premature_idle = false;
  for (int cycle = 0; cycle < 2000; ++cycle) {
    d.eval();
    if (d.o_valid_o) ++retired;
    if (retired < 8 && d.idle_o) premature_idle = true;
    tick(d);
    d.eval();
    if (retired == 8 && d.idle_o) break;
  }
  check(retired == 8, "all held contexts retire after ready returns", 8,
        retired);
  check(d.jobs_accepted_o == 8 && d.jobs_completed_o == 8,
        "each held job completes exactly once on its output handshake", 8,
        d.jobs_completed_o);
  check(d.phases_issued_o == 8 && d.phases_completed_o == 8,
        "held retirement does not duplicate phase issue or completion", 8,
        d.phases_completed_o);
  check(!premature_idle, "idle stays low until the final context releases", 0,
        premature_idle ? 1 : 0);
  check(d.idle_o != 0, "complete drain returns to structural idle", 1,
        d.idle_o ? 1 : 0);
}

#else  // MATERIAL_V3_MUTANT_BUILD

Frag focused_mutant_frag() {
  Frag f;
  f.tag = 1;
  f.base = Plane{7, 8, 9, 10, 0, 0};
  switch (kMutation) {
    case 1:
      f.recipe = 1;
      f.count = 2;
      f.s[0] = Plane{20, 30, 40, 200, 0xA5, 0};
      f.s[1] = Plane{50, 60, 70, 128, 0, 0};
      break;
    case 2:
      f.recipe = 5;
      f.count = 2;
      f.s[0] = Plane{11, 22, 33, 200, 0xA5, 0};
      f.s[1] = Plane{0, 0, 0, 128, 0, 0};
      break;
    case 3:
      f.recipe = 2;
      f.count = 2;
      f.s[0] = Plane{1, 1, 1, 200, 0xA5, 0};
      f.s[1] = Plane{64, 64, 64, 7, 0, 0};
      break;
    case 4:
      f.recipe = 7;
      f.count = 3;
      f.s[0] = Plane{1, 1, 1, 240, 0xA5, 0};
      f.s[1] = Plane{64, 64, 64, 7, 0, 0};
      f.s[2] = Plane{255, 255, 255, 128, 0, 0};
      break;
    case 5:
      f.recipe = 1;
      f.count = 0;
      break;
    case 6:
      f.recipe = 6;
      f.count = 3;
      f.aux_required = true;
      f.s[0] = Plane{100, 100, 100, 200, 0xA5, 0};
      f.s[1] = Plane{128, 128, 128, 7, 0, 0};
      f.s[2] = Plane{255, 255, 255, 99, 0, 0};
      f.aux = Plane{0, 0, 0, 33, 0, 0};
      break;
    case 7:
      f.recipe = 1;
      f.count = 2;
      f.s[0] = Plane{11, 22, 33, 44, 0xA5, 0};
      f.s[1] = Plane{50, 60, 70, 80, 0, 0x04};
      break;
    case 8:
      f.recipe = 0;
      f.count = 1;
      f.s[0] = Plane{11, 22, 33, 44, 0xA5, 0};
      break;
    case 9:
      f.recipe = 0;
      f.count = 1;
      f.s[0] = Plane{11, 22, 3, 44, 0xA5, 0};
      break;
    case 10:
      f.recipe = 6;
      f.count = 3;
      f.s[0] = Plane{100, 120, 140, 44, 0xA5, 0};
      f.s[1] = Plane{128, 128, 128, 80, 0, 0};
      f.s[2] = Plane{255, 200, 64, 90, 0, 0};
      break;
    default:
      f.recipe = 0;
      f.count = 1;
      f.s[0] = Plane{11, 22, 3, 44, 0xA5, 0};
      break;
  }
  return f;
}

void test_mutant_control_fires() {
  Dut d;
  reset(d);
  const Frag f = focused_mutant_frag();
  bool sent = false;
  bool seen = false;
  uint64_t got = 0;
  int accepted_cycle = -1;
  int seen_cycle = -1;

  for (int cycle = 0; cycle < 2000 && !seen; ++cycle) {
    if (!sent) drive_frag(d, f);
    else d.f_valid_i = 0;
    d.o_ready_i = 1;
    d.eval();
    const bool accepted = !sent && d.f_valid_i && d.f_ready_o;
    if (d.o_valid_o) {
      got = static_cast<uint64_t>(d.o_result_o);
      seen_cycle = cycle;
      seen = true;
    }
    tick(d);
    if (accepted) {
      accepted_cycle = cycle;
      sent = true;
    }
  }

  const uint64_t want = r9_expected(f);
  std::printf("  mutant %s: want=%012llX got=%012llX\n", kMutationName,
              static_cast<unsigned long long>(want),
              static_cast<unsigned long long>(got));
  check(sent, "mutant control accepted its focused fragment", 1, sent ? 1 : 0);
  check(seen, "mutant control retired its focused fragment", 1, seen ? 1 : 0);
  if (kMutation <= 9) {
    check(got != want,
          "inverse-polarity R9 checker FIRES on the named stale alternative", 1,
          got != want ? 1 : 0);

    if (kMutation == 1) {
      std::vector<Frag> alpha_cases;
      Frag mod2_alpha;
      mod2_alpha.recipe = 2;
      mod2_alpha.count = 2;
      mod2_alpha.s[0] = Plane{20, 30, 40, 200, 0xA6, 0};
      mod2_alpha.s[1] = Plane{50, 60, 70, 64, 0, 0};
      mod2_alpha.tag = 2;
      alpha_cases.push_back(mod2_alpha);

      Frag lerp_alpha = mod2_alpha;
      lerp_alpha.recipe = 3;
      lerp_alpha.weight = 128;
      lerp_alpha.s[1].a = 0;
      lerp_alpha.tag = 3;
      alpha_cases.push_back(lerp_alpha);

      Frag add_alpha = mod2_alpha;
      add_alpha.recipe = 4;
      add_alpha.s[1].a = 100;
      add_alpha.tag = 4;
      alpha_cases.push_back(add_alpha);

      int extra_mismatches = 0;
      int extra_missing = 0;
      for (const Frag& alpha_case : alpha_cases) {
        bool extra_sent = false;
        bool extra_seen = false;
        uint64_t extra_got = 0;
        for (int cycle = 0; cycle < 2000 && !extra_seen; ++cycle) {
          if (!extra_sent) drive_frag(d, alpha_case);
          else d.f_valid_i = 0;
          d.o_ready_i = 1;
          d.eval();
          const bool extra_accept =
              !extra_sent && d.f_valid_i && d.f_ready_o;
          if (d.o_valid_o) {
            extra_got = static_cast<uint64_t>(d.o_result_o);
            extra_seen = true;
          }
          tick(d);
          if (extra_accept) extra_sent = true;
        }
        if (!extra_sent || !extra_seen) ++extra_missing;
        if (extra_seen && extra_got != r9_expected(alpha_case))
          ++extra_mismatches;
      }
      check(extra_missing == 0,
            "alpha mutant retires the MOD2X/LERP/ADD_SAT controls", 0,
            extra_missing);
      check(extra_mismatches == 3,
            "recipes 2, 3, and 4 each independently expose stale alpha arithmetic",
            3, extra_mismatches);
    }
  } else if (kMutation <= 11) {
    check(d.jobs_accepted_o == 1 && d.jobs_completed_o == 1,
          "phase mutant still retires exactly one accepted job", 1,
          d.jobs_completed_o);
    if (kMutation == 10) {
      check(d.phases_issued_o == 3,
            "phase-drop control launches all three DETAIL_LIGHT phases", 3,
            d.phases_issued_o);
      check(d.phases_completed_o == 2,
            "phase-drop control suppresses one actual scratch writeback", 2,
            d.phases_completed_o);
      check(d.phases_issued_o != d.phases_completed_o,
            "PI/PC balance checker FIRES on actual writeback loss", 1,
            d.phases_issued_o != d.phases_completed_o ? 1 : 0);
    } else {
      check(got == want,
            "phase reissue leaves the idempotent PASSTHRU result unchanged", want,
            got);
      check(d.phases_issued_o == 2 && d.phases_completed_o == 2,
            "phase reissue launches and writes back two actual phases", 2,
            d.phases_completed_o);
      check(d.phases_issued_o != 1,
            "exact phase-demand checker FIRES despite balanced PI/PC", 1,
            d.phases_issued_o != 1 ? 1 : 0);
    }
  } else {
    check(got == want,
          "pipeline-boundary mutant preserves arithmetic while changing latency",
          want, got);
    check(seen_cycle - accepted_cycle == 10,
          "missing S/F boundary is detected one cycle before the contract", 10,
          seen_cycle - accepted_cycle);
    check(d.jobs_accepted_o == 1 && d.jobs_completed_o == 1 &&
              d.phases_issued_o == 1 && d.phases_completed_o == 1,
          "pipeline-boundary mutant keeps functional counters deceptively balanced",
          1, d.jobs_completed_o);
  }
}

void test_pipeline_boundary_mutant_fires() {
  Dut d;
  reset(d);

  std::vector<Frag> jobs;
  std::map<uint16_t, uint64_t> expected;
  for (int i = 0; i < 8; ++i) {
    Frag f;
    f.recipe = 5;
    f.count = 2;
    f.weight = static_cast<uint8_t>(17 + i * 23);
    f.s[0] = Plane{static_cast<uint8_t>(11 + i * 7),
                   static_cast<uint8_t>(31 + i * 5),
                   static_cast<uint8_t>(53 + i * 3),
                   static_cast<uint8_t>(91 + i),
                   static_cast<uint8_t>(0xC0 + i), 0};
    f.s[1] = Plane{static_cast<uint8_t>(211 - i * 9),
                   static_cast<uint8_t>(173 - i * 7),
                   static_cast<uint8_t>(137 - i * 5),
                   static_cast<uint8_t>(41 + i), 0, 0};
    f.tag = static_cast<uint16_t>(0x5200 + i);
    expected[f.tag] = r9_expected(f);
    jobs.push_back(f);
  }

  int accepted = 0;
  int retired = 0;
  int mismatches = 0;
  int duplicate_or_unknown = 0;
  int latency_errors = 0;
  int accept_cycle[8] = {0};
  int retire_cycle[8] = {0};
  bool seen[8] = {false};
  for (int cycle = 0; cycle < 4000 && retired < 8; ++cycle) {
    if (accepted < 8) drive_frag(d, jobs[accepted]);
    else d.f_valid_i = 0;
    d.o_ready_i = 1;
    d.eval();
    const bool take = accepted < 8 && d.f_valid_i && d.f_ready_o;
    if (take) accept_cycle[accepted] = cycle;
    if (d.o_valid_o) {
      const uint16_t tag = static_cast<uint16_t>(d.o_tag_o);
      const int index = static_cast<int>(tag) - 0x5200;
      if (index < 0 || index >= 8 || seen[index]) {
        ++duplicate_or_unknown;
      } else {
        seen[index] = true;
        retire_cycle[index] = cycle;
        if (static_cast<uint64_t>(d.o_result_o) != expected[tag]) ++mismatches;
      }
      ++retired;
    }
    tick(d);
    if (take) ++accepted;
  }

  for (int i = 0; i < 8; ++i)
    if (retire_cycle[i] - accept_cycle[i] != 10) ++latency_errors;

  check(accepted == 8 && retired == 8,
        "pipeline mutant accepts and retires the complete eight-context stream",
        8, retired);
  check(duplicate_or_unknown == 0 && mismatches == 0,
        "pipeline mutant remains arithmetically plausible and identity-clean",
        0, duplicate_or_unknown + mismatches);
  check(latency_errors == 0,
        "inverse-polarity checker FIRES on the one-cycle-short S/F pipeline", 0,
        latency_errors);
  check(d.jobs_accepted_o == 8 && d.jobs_completed_o == 8,
        "pipeline mutant can balance job counters while violating latency",
        8, d.jobs_completed_o);
}

#endif  // MATERIAL_V3_MUTANT_BUILD

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
#ifdef MATERIAL_V3_MUTANT_BUILD
  std::printf("[material_combine_v3_diff] MUTANT CONTROL: %s\n", kMutationName);
  if (kMutation >= 12) test_pipeline_boundary_mutant_fires();
  else test_mutant_control_fires();
#else
  // Every result lookup below is by_tag.at(), which THROWS when a fragment the
  // run accepted never retired. Left uncaught that is a bare std::out_of_range
  // and the reader learns nothing; a positive control registered against it
  // would accept a build break or a missing DLL as evidence of detection.
  // Convert it into the named diagnostic it actually is. Inert on a healthy
  // build -- nothing throws -- and load-bearing for
  // material_combine_v3_cont_bypass_double_issue_control, whose double-issued
  // phase is invisible in every colour and visible only in the accounting.
  try {
    test_r9_arithmetic_and_stale_separators();
    test_exact_counts_loud_errors_and_required_status();
    test_aux_is_status_only_and_sample2_is_real();
    test_rtl_add_lerp_boundary_cross_products();
    test_exact_saturation_accounting_under_stalls();
    test_phase_and_product_cadence();
    test_one_phase_retirement_is_one_per_clock();
    test_timing4_eight_context_pipeline();
    test_timing4_multiphase_recurrence_and_rate();
    test_held_output_and_structural_idle();
  } catch (const std::out_of_range&) {
    ++g_failed;
    std::printf(
        "FAIL: retirement bookkeeping lost a tag: the machine did not retire "
        "what it accepted\n");
  }
#endif

  if (g_failed) {
    std::printf("[material_combine_v3_diff] %d/%d checks FAILED\n", g_failed,
                g_checks);
    zhao::exit_hard(1);
  }
  std::printf("[material_combine_v3_diff] %d checks passed\n", g_checks);
  zhao::exit_hard(0);
}
