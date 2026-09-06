// material_combine_v2_diff.cpp -- the paired-phase combiner against the oracle.
//
// The owner's recovery brief, migration map: "tests/texture/
// material_combine_v1_diff.cpp -- Preserve its useful workload; extend/factor
// it to drive V2 against the actual oracle. Do not use the local transcription
// as the final repository oracle."
//
// So this is V1's workload, verbatim, pointed at V2 and at
// `zref::material::combine` -- not at a second transcription of the arithmetic,
// because a codec verified against a copy of itself proves only that two copies
// agree. V1's file stays as it is: the brief also says to freeze V1 as the
// historical comparison rather than erase its known failures from evidence.
//
// WHAT IS NEW HERE IS THE PHASE COUNT. V2's whole claim is that a fragment
// costs a FIXED, KNOWN number of phases under the paired schedule:
//
//   PASSTHRU / ADD_SAT / MASK / count==0 / refused   1 phase
//   MODULATE / MODULATE2X / LERP / DETAIL_MASK       2 phases
//   DETAIL_LIGHT                                     3 phases
//
// V1 issued every microjob ABOUT TWICE and every colour still matched the
// oracle exactly, because recomputing a product is idempotent. A result check
// cannot see that; only a count can. So this file asserts the exact total, and
// a schedule that quietly re-ran a phase would fail it while every pixel stayed
// right.

#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vzhao_texture_material_combine_v2.h"
#include "zref/zref_material.hpp"

namespace mat = zref::material;

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

struct Frag {
  uint8_t recipe = 0;
  uint8_t count = 0;
  uint8_t weight = 0;
  mat::Sample s[3];
  mat::Sample base;
  uint16_t tag = 0;
};

struct Got {
  uint8_t r = 0, g = 0, b = 0, a = 0;
  bool refused = false;
  bool seen = false;
};

using Dut = Vzhao_texture_material_combine_v2;

void tick(Dut& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
}

// Drive a batch through the DUT and return every retirement, keyed by tag.
// Keyed BY TAG on purpose: §15.6 says the combiner may finish fragments out of
// allocation order, so an index-keyed collection would be testing the wrong
// thing and would break as soon as the scheduler behaved correctly.
std::map<uint16_t, Got> run_batch(const std::vector<Frag>& in, uint32_t jobs_by_recipe[8] = nullptr,
                                  bool stall_consumer = false,
                                  uint32_t* phases_issued = nullptr) {
  Dut d;
  d.rst_n = 0;
  d.o_ready_i = 1;
  d.f_valid_i = 0;
  for (int i = 0; i < 4; ++i) tick(d);
  d.rst_n = 1;

  std::map<uint16_t, Got> out;
  std::size_t next = 0;
  int idle = 0;
  // Generous but bounded: depth 2 and up to six jobs per fragment means the
  // worst batch here is well inside this. A cap rather than a while(true) so a
  // scheduler deadlock fails the test instead of hanging CI.
  const int kMaxCycles = 200000;

  for (int cyc = 0; cyc < kMaxCycles; ++cyc) {
    // present the next fragment
    if (next < in.size()) {
      const Frag& f = in[next];
      d.f_valid_i = 1;
      d.f_sample_count_i = f.count;
      d.f_recipe_i = f.recipe;
      d.f_weight_i = f.weight;
      d.f_s0_rgb_i = (f.s[0].r << 16) | (f.s[0].g << 8) | f.s[0].b;
      d.f_s0_a_i = f.s[0].a;
      d.f_s1_rgb_i = (f.s[1].r << 16) | (f.s[1].g << 8) | f.s[1].b;
      d.f_s1_a_i = f.s[1].a;
      d.f_s2_rgb_i = (f.s[2].r << 16) | (f.s[2].g << 8) | f.s[2].b;
      d.f_s2_a_i = f.s[2].a;
      d.f_base_rgb_i = (f.base.r << 16) | (f.base.g << 8) | f.base.b;
      d.f_base_a_i = f.base.a;
      d.f_tag_i = f.tag;
    } else {
      d.f_valid_i = 0;
    }

    // A consumer that is not always ready, when asked for. Back-pressure that
    // is never exercised is back-pressure that has not been tested.
    d.o_ready_i = stall_consumer ? ((cyc % 3) != 0) : 1;
    d.eval();

    const bool accepted = d.f_valid_i && d.f_ready_o;
    if (d.o_valid_o && d.o_ready_i) {
      Got g;
      g.r = (d.o_rgb_o >> 16) & 0xFF;
      g.g = (d.o_rgb_o >> 8) & 0xFF;
      g.b = d.o_rgb_o & 0xFF;
      g.a = d.o_a_o;
      g.refused = d.o_refused_o != 0;
      g.seen = true;
      out[static_cast<uint16_t>(d.o_tag_o)] = g;
    }

    tick(d);
    if (accepted) ++next;

    if (next >= in.size() && !d.o_valid_o) {
      if (++idle > 8) break;  // drained
    } else {
      idle = 0;
    }
  }

  if (jobs_by_recipe)
    for (int i = 0; i < 8; ++i) jobs_by_recipe[i] = d.jobs_by_recipe_o[i];
  if (phases_issued) *phases_issued = d.phases_issued_o;
  return out;
}

mat::Out oracle(const Frag& f) {
  return mat::combine(f.recipe, f.weight, f.s, f.count, f.base, f.tag, nullptr);
}

// ---------------------------------------------------------------------------

// A deterministic generator. Values are drawn to hit the corners far more often
// than a uniform draw would -- 0, 1, 127, 128, 254 and 255 are where a rounding
// or a saturation law goes wrong, and a uniform byte finds them 2% of the time.
uint8_t byte_of(uint32_t& st) {
  st = st * 1664525u + 1013904223u;
  const uint32_t pick = (st >> 16) & 0xF;
  static const uint8_t kCorners[8] = {0, 1, 127, 128, 129, 200, 254, 255};
  if (pick < 8) return kCorners[pick];
  return static_cast<uint8_t>((st >> 8) & 0xFF);
}

mat::Sample sample_of(uint32_t& st) {
  mat::Sample s;
  s.r = byte_of(st);
  s.g = byte_of(st);
  s.b = byte_of(st);
  s.a = byte_of(st);
  return s;
}

// THE PHASE COUNT, AGAINST THE SCHEDULE RATHER THAN AGAINST THE RTL.
//
// V2's claim is a FIXED schedule, and a result check cannot see a schedule. V1
// issued every microjob about twice with every colour still exact, because
// recomputing a product is idempotent; the only thing that saw it was a count.
//
// The expected total is restated here from the brief's section 4.2. Reading it
// out of the block would be checking the block against its own opinion.
void test_the_schedule_issues_exactly_its_phases() {
  std::vector<Frag> batch;
  uint32_t st = 0xBEEF01u;
  uint16_t tag = 1;
  for (uint8_t r = 0; r < mat::kRecipeCount; ++r)
    for (int i = 0; i < 40; ++i) {
      Frag f;
      f.recipe = r;
      f.count = 3;
      f.weight = byte_of(st);
      f.s[0] = sample_of(st);
      f.s[1] = sample_of(st);
      f.s[2] = sample_of(st);
      f.base = sample_of(st);
      f.tag = tag++;
      batch.push_back(f);
    }

  uint32_t phases = 0;
  const std::map<uint16_t, Got> got = run_batch(batch, nullptr, false, &phases);

  uint32_t want = 0;
  for (const Frag& f : batch) {
    if (f.recipe == mat::kTerrainDetailLight) want += 3;
    else if (f.recipe == mat::kPassthru || f.recipe == mat::kAddSat ||
             f.recipe == mat::kMask) want += 1;
    else want += 2;   // MODULATE, MODULATE2X, LERP, DETAIL_MASK
  }

  std::printf("  phases issued %u, the schedule owes %u, over %zu fragments\n",
              phases, want, batch.size());
  check(phases == want,
        "the paired schedule issued EXACTLY the phases it owes -- not one "
        "re-run, which is the fault V1 had while every colour stayed exact",
        static_cast<long long>(want), static_cast<long long>(phases));
  check(got.size() == batch.size(), "and every fragment still came back",
        static_cast<long long>(batch.size()), static_cast<long long>(got.size()));
}

void test_every_recipe_matches_the_oracle() {
  std::vector<Frag> batch;
  uint32_t st = 0xC0FFEEu;
  uint16_t tag = 1;
  // 200 fragments per recipe, all eight recipes, always three samples so the
  // terrain recipes are exercised rather than refused.
  for (uint8_t r = 0; r < mat::kRecipeCount; ++r)
    for (int i = 0; i < 200; ++i) {
      Frag f;
      f.recipe = r;
      f.count = 3;
      f.weight = byte_of(st);
      f.s[0] = sample_of(st);
      f.s[1] = sample_of(st);
      f.s[2] = sample_of(st);
      f.base = sample_of(st);
      f.tag = tag++;
      batch.push_back(f);
    }

  uint32_t jobs[8] = {0};
  const std::map<uint16_t, Got> got = run_batch(batch, jobs);

  int missing = 0, mismatched = 0;
  int first_bad = -1;
  for (const Frag& f : batch) {
    const auto it = got.find(f.tag);
    if (it == got.end()) {
      ++missing;
      continue;
    }
    const mat::Out want = oracle(f);
    const Got& g = it->second;
    if (g.r != want.r || g.g != want.g || g.b != want.b || g.a != want.a ||
        g.refused != want.refused) {
      ++mismatched;
      if (first_bad < 0) {
        first_bad = f.tag;
        std::printf(
            "  first mismatch: tag %u recipe %u w=%u\n"
            "    s0 %3u %3u %3u %3u   s1 %3u %3u %3u %3u   s2 %3u %3u %3u %3u\n"
            "    want %3u %3u %3u %3u   got %3u %3u %3u %3u\n",
            f.tag, f.recipe, f.weight, f.s[0].r, f.s[0].g, f.s[0].b, f.s[0].a, f.s[1].r, f.s[1].g,
            f.s[1].b, f.s[1].a, f.s[2].r, f.s[2].g, f.s[2].b, f.s[2].a, want.r, want.g, want.b,
            want.a, g.r, g.g, g.b, g.a);
      }
    }
  }

  check(missing == 0, "every fragment retired -- none was lost in the scheduler", 0, missing);
  check(mismatched == 0, "every recipe's result matches zref::material::combine exactly", 0,
        mismatched);

  // §15.4's counters. DETAIL_LIGHT must be the block's most expensive recipe,
  // because the entire two-lane capacity argument rests on that being true.
  check(jobs[mat::kPassthru] == 0, "PASSTHRU issues no product jobs", 0, jobs[mat::kPassthru]);
  check(jobs[mat::kAddSat] == 0, "ADD_SAT issues no product jobs", 0, jobs[mat::kAddSat]);
  check(jobs[mat::kMask] == 0, "MASK issues no product jobs", 0, jobs[mat::kMask]);
  check(jobs[mat::kTerrainDetailLight] == 200 * 6,
        "DETAIL_LIGHT issues six product jobs per fragment", 200 * 6,
        jobs[mat::kTerrainDetailLight]);
  check(jobs[mat::kTerrainDetailMask] == 200 * 4, "DETAIL_MASK issues four", 200 * 4,
        jobs[mat::kTerrainDetailMask]);
  check(jobs[mat::kModulate] == 200 * 4, "MODULATE issues four -- 3 RGB + alpha", 200 * 4,
        jobs[mat::kModulate]);
  check(jobs[mat::kModulate2x] == 200 * 4,
        "MODULATE2X issues four -- 3 RGB + alpha", 200 * 4,
        jobs[mat::kModulate2x]);
  check(jobs[mat::kLerp] == 200 * 4, "LERP issues four -- 3 RGB + alpha", 200 * 4,
        jobs[mat::kLerp]);
  check(jobs[mat::kTerrainDetailLight] > jobs[mat::kModulate],
        "and DETAIL_LIGHT really is the most expensive recipe -- the two-lane "
        "capacity argument depends on it",
        1, jobs[mat::kTerrainDetailLight] > jobs[mat::kModulate] ? 1 : 0);
}

// The behaviour the scheduler exists for, and the one an in-order test would
// hide: a cheap fragment behind an expensive one comes out FIRST.
void test_a_cheap_fragment_overtakes_an_expensive_one() {
  std::vector<Frag> batch;
  Frag heavy;
  heavy.recipe = mat::kTerrainDetailLight;
  heavy.count = 3;
  heavy.s[0] = {200, 200, 200, 200};
  heavy.s[1] = {128, 128, 128, 128};
  heavy.s[2] = {255, 255, 255, 255};
  heavy.tag = 0x1000;
  batch.push_back(heavy);

  Frag light;
  light.recipe = mat::kPassthru;
  light.count = 3;
  light.s[0] = {11, 22, 33, 44};
  light.tag = 0x2000;
  batch.push_back(light);

  // Record the ORDER of retirement, which run_batch's map deliberately does
  // not preserve -- so this one watches the port directly.
  Dut d;
  d.rst_n = 0;
  d.o_ready_i = 1;
  d.f_valid_i = 0;
  for (int i = 0; i < 4; ++i) tick(d);
  d.rst_n = 1;

  std::vector<uint16_t> order;
  std::size_t next = 0;
  for (int cyc = 0; cyc < 2000; ++cyc) {
    if (next < batch.size()) {
      const Frag& f = batch[next];
      d.f_valid_i = 1;
      d.f_sample_count_i = f.count;
      d.f_recipe_i = f.recipe;
      d.f_weight_i = f.weight;
      d.f_s0_rgb_i = (f.s[0].r << 16) | (f.s[0].g << 8) | f.s[0].b;
      d.f_s0_a_i = f.s[0].a;
      d.f_s1_rgb_i = (f.s[1].r << 16) | (f.s[1].g << 8) | f.s[1].b;
      d.f_s1_a_i = f.s[1].a;
      d.f_s2_rgb_i = (f.s[2].r << 16) | (f.s[2].g << 8) | f.s[2].b;
      d.f_s2_a_i = f.s[2].a;
      d.f_tag_i = f.tag;
    } else {
      d.f_valid_i = 0;
    }
    d.eval();
    const bool accepted = d.f_valid_i && d.f_ready_o;
    if (d.o_valid_o) order.push_back(static_cast<uint16_t>(d.o_tag_o));
    tick(d);
    if (accepted) ++next;
    if (order.size() >= 2 && next >= batch.size()) break;
  }

  check(order.size() >= 2, "both fragments retired", 2, static_cast<long long>(order.size()));
  if (order.size() >= 2) {
    check(order[0] == 0x2000,
          "the PASSTHRU retires BEFORE the DETAIL_LIGHT it was submitted "
          "behind -- out-of-order retirement is the design, and FRAGROB "
          "restores program order downstream",
          0x2000, order[0]);
  }
}

void test_back_pressure_loses_nothing() {
  std::vector<Frag> batch;
  uint32_t st = 0x5EEDu;
  for (int i = 0; i < 120; ++i) {
    Frag f;
    f.recipe = static_cast<uint8_t>(i % mat::kRecipeCount);
    f.count = 3;
    f.weight = byte_of(st);
    f.s[0] = sample_of(st);
    f.s[1] = sample_of(st);
    f.s[2] = sample_of(st);
    f.base = sample_of(st);
    f.tag = static_cast<uint16_t>(0x300 + i);
    batch.push_back(f);
  }
  const std::map<uint16_t, Got> got = run_batch(batch, nullptr, /*stall_consumer=*/true);

  int missing = 0, mismatched = 0;
  for (const Frag& f : batch) {
    const auto it = got.find(f.tag);
    if (it == got.end()) {
      ++missing;
      continue;
    }
    const mat::Out want = oracle(f);
    if (it->second.r != want.r || it->second.g != want.g || it->second.b != want.b ||
        it->second.a != want.a)
      ++mismatched;
  }
  check(missing == 0, "a stalling consumer loses no fragment", 0, missing);
  check(mismatched == 0, "and corrupts none", 0, mismatched);
}

void test_refusals_retire_rather_than_vanish() {
  // A malformed fragment must come out carrying its tag. Dropping it would
  // leave a hole in the retirement stream FRAGROB orders on, which is far
  // harder to debug than a fragment that says it is malformed.
  std::vector<Frag> batch;
  Frag a;  // three-sample recipe, two samples
  a.recipe = mat::kTerrainDetailLight;
  a.count = 2;
  a.tag = 0x4001;
  batch.push_back(a);
  Frag b;  // two-sample recipe, one sample
  b.recipe = mat::kModulate;
  b.count = 1;
  b.tag = 0x4002;
  batch.push_back(b);
  Frag c;  // untextured: LEGAL, base colour through
  c.recipe = mat::kModulate;
  c.count = 0;
  c.base = {7, 8, 9, 10};
  c.tag = 0x4003;
  batch.push_back(c);

  const std::map<uint16_t, Got> got = run_batch(batch);
  check(got.count(0x4001) == 1 && got.count(0x4002) == 1,
        "both malformed fragments retired with their tags", 2,
        static_cast<long long>(got.count(0x4001) + got.count(0x4002)));
  if (got.count(0x4001)) {
    check(got.at(0x4001).refused, "the three-sample recipe given two is refused", 1,
          got.at(0x4001).refused ? 1 : 0);
  }
  if (got.count(0x4003)) {
    const Got& g = got.at(0x4003);
    check(!g.refused && g.r == 7 && g.g == 8 && g.b == 9 && g.a == 10,
          "and sample_count == 0 is LEGAL: the base colour passes through", 1,
          (!g.refused && g.r == 7) ? 1 : 0);
  }
}

void test_output_stall_keeps_every_context_reserved() {
  Dut d;
  d.rst_n = 0;
  d.o_ready_i = 0;
  d.f_valid_i = 0;
  for (int i = 0; i < 4; ++i) tick(d);
  d.rst_n = 1;

  int accepted = 0;
  d.f_valid_i = 1;
  d.f_sample_count_i = 1;
  d.f_recipe_i = mat::kPassthru;
  d.f_weight_i = 0;
  d.f_s0_a_i = 0xA5;
  d.f_s1_rgb_i = 0;
  d.f_s1_a_i = 0;
  d.f_s2_rgb_i = 0;
  d.f_s2_a_i = 0;
  d.f_base_rgb_i = 0;
  d.f_base_a_i = 0;

  for (int cyc = 0; cyc < 256; ++cyc) {
    d.f_tag_i = static_cast<uint16_t>(0x5000 + accepted);
    d.f_s0_rgb_i = static_cast<uint32_t>(accepted + 1) << 16;
    d.eval();
    const bool admit = d.f_ready_o != 0;
    tick(d);
    if (admit) ++accepted;
  }

  check(accepted == 8,
        "a stopped output reserves all eight contexts -- loading the held "
        "register does not create a ninth credit",
        8, accepted);
  check(!d.f_ready_o, "input remains blocked while none of those outputs is accepted", 0,
        d.f_ready_o ? 1 : 0);
  check(d.o_valid_o, "one completed result is held at the stopped output", 1,
        d.o_valid_o ? 1 : 0);

  const uint32_t held_rgb = d.o_rgb_o;
  const uint8_t held_a = d.o_a_o;
  const uint16_t held_tag = static_cast<uint16_t>(d.o_tag_o);
  const bool held_refused = d.o_refused_o != 0;
  bool stable = true;
  for (int cyc = 0; cyc < 64; ++cyc) {
    // Poison every live input while the accepted transaction is stalled.
    d.f_tag_i = static_cast<uint16_t>(0x7000 + cyc);
    d.f_s0_rgb_i = 0x00FFFFFFu ^ static_cast<uint32_t>(cyc);
    d.f_s0_a_i = static_cast<uint8_t>(cyc);
    d.eval();
    stable = stable && d.o_valid_o && d.o_rgb_o == held_rgb && d.o_a_o == held_a &&
             static_cast<uint16_t>(d.o_tag_o) == held_tag &&
             (d.o_refused_o != 0) == held_refused && !d.f_ready_o;
    tick(d);
  }
  check(stable, "valid && !ready holds RGBA, tag, status, and input backpressure stable", 1,
        stable ? 1 : 0);

  // No same-cycle full-to-free bypass: the outgoing acceptance returns its
  // context on this edge, and only the following cycle may accept item nine.
  d.o_ready_i = 1;
  d.eval();
  check(!d.f_ready_o, "the acceptance edge does not bypass a still-full FREE queue", 0,
        d.f_ready_o ? 1 : 0);
  tick(d);
  check(d.f_ready_o, "one accepted output returns exactly one context credit", 1,
        d.f_ready_o ? 1 : 0);
  d.eval();
  const bool ninth_admit = d.f_ready_o != 0;
  tick(d);
  if (ninth_admit) ++accepted;
  check(accepted == 9, "the ninth input is admitted only after that credit returns", 9, accepted);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  test_every_recipe_matches_the_oracle();
  test_the_schedule_issues_exactly_its_phases();
  test_a_cheap_fragment_overtakes_an_expensive_one();
  test_back_pressure_loses_nothing();
  test_refusals_retire_rather_than_vanish();
  test_output_stall_keeps_every_context_reserved();

  if (g_failed) {
    std::printf("[material_combine_v2_diff] %d/%d checks FAILED\n", g_failed, g_checks);
    return 1;
  }
  std::printf("[material_combine_v2_diff] %d checks passed\n", g_checks);
  return 0;
}
