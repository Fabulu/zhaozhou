// material_combine_readlate_diff.cpp -- the READ_LATE combiner, behind four
// real owner planes, against the oracle.
//
// reports/TEXTURE-READLATE-COMBINE-20260910.md (roadmap 4.2 / Commit4).
//
// WHAT THIS IS. material_combine_v2_diff.cpp proves the paired-phase
// arithmetic against `zref::material::combine` with the samples handed over on
// pins. Under READ_LATE=1 the samples are no longer on pins: the combiner names
// an owner SLOT per phase and reads them from the planes itself. So this is the
// SAME workload -- V2's generator, corners and recipes, the same phase-count
// law, the same overtaking and back-pressure cases -- driven through
// tests/texture/tb_combine_readlate.sv, where the planes are the island's own
// `zhao_texture_v3bank` and the bench plays the TMU/AUX commit into them.
//
// WHAT IS NEW, and it is the seam's whole content:
//
//   * PUBLICATION-BEFORE-READY. A job is offered only after its planes are
//     written AND readable (two edges after the last accepted beat). The owner
//     enforces this in the island; here the driver does, and asserts it.
//   * RELEASE-AFTER-LAST-READER. A slot is rewritten only after the fragment
//     that owned it has RETIRED at the output. The driver asserts it.
//   * EVERY READ NAMES AN IN-FLIGHT SLOT. `src_rd_valid_o`/`src_rd_slot_o` are
//     watched every cycle: a read of a slot that is not currently owned by an
//     accepted, unretired fragment is a violation. This is the bench-level
//     twin of v3own's `a_src_read_published`.
//   * POISONED PLANES. Planes a fragment must NOT read (s1 when count<2, s2
//     and aux when count<3, the non-chosen one of s2/aux when count==3, all
//     four when count==0) hold a deterministic poison, so picking the wrong
//     plane, or reading a plane instead of BASE, changes the colour.
//
// THE DELIBERATE BREAK. Built with -DREADLATE_MUTANT -DREADLATE_EXPECT_MISMATCH
// this same source drives tests/mutants/zhao_texture_material_combine_v2_
// slotswap_mutant.sv (slot file off by one) and PASSES WHEN THE CHECKERS FIRE:
// oracle mismatches > 0 AND seam violations > 0. Inverse polarity, deliberately
// -- evidence about the instrument. Note what it also shows: v3own's shipped
// counter would see the SAME fault only when slot+1 is not a published owner;
// when it is, the counter is blind and this differential is the only witness.

#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vtb_combine_readlate.h"
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
  bool has_aux = false;   // the third sample lives in the AUX plane
  mat::Sample s[3];
  mat::Sample base;
  uint16_t tag = 0;
  uint8_t slot = 0;       // the owner slot the bench allocates
};

struct Got {
  uint8_t r = 0, g = 0, b = 0, a = 0;
  bool refused = false;
  bool seen = false;
};

struct Seam {
  long reads = 0;       // src_rd_valid cycles observed
  long violations = 0;  // reads naming a slot not in flight
  long reuse_faults = 0;
};

using Dut = Vtb_combine_readlate;

void tick(Dut& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
}

uint64_t lane40(const mat::Sample& s) {
  // result40 = {status8, alpha8, rgb24}; status is not a combiner operand.
  return (static_cast<uint64_t>(s.a) << 24) |
         (static_cast<uint64_t>(s.r) << 16) | (static_cast<uint64_t>(s.g) << 8) | s.b;
}

// A poison that differs from every legal sample the fragment carries and is
// visibly not a colour the oracle could produce from them.
uint64_t poison(const Frag& f, int lane) {
  return (0xE0ull << 32) | (0xA5A5A5A5ull ^ (static_cast<uint64_t>(f.tag) << 4) ^
                            (static_cast<uint64_t>(lane) * 0x01010101ull));
}

// What each plane holds for this fragment.
uint64_t plane_data(const Frag& f, int lane) {
  switch (lane) {
    case 0: return (f.count >= 1) ? lane40(f.s[0]) : poison(f, 0);
    case 1: return (f.count >= 2) ? lane40(f.s[1]) : poison(f, 1);
    case 2: return (f.count >= 3 && !f.has_aux) ? lane40(f.s[2]) : poison(f, 2);
    default: return (f.count >= 3 && f.has_aux) ? lane40(f.s[2]) : poison(f, 3);
  }
}

// Drive a batch and return every retirement keyed by tag (out-of-order
// retirement is the design, per V2's own bench).
std::map<uint16_t, Got> run_batch(const std::vector<Frag>& in, Seam& seam,
                                  uint32_t jobs_by_recipe[8] = nullptr,
                                  bool stall_consumer = false,
                                  uint32_t* phases_issued = nullptr) {
  Dut d;
  d.rst_n = 0;
  d.o_ready_i = 1;
  d.f_valid_i = 0;
  d.pw_en_i = 0;
  for (int i = 0; i < 4; ++i) tick(d);
  d.rst_n = 1;

  std::map<uint16_t, Got> out;
  int busy_tag[64];
  for (int i = 0; i < 64; ++i) busy_tag[i] = -1;
  std::map<uint16_t, uint8_t> slot_of_tag;

  std::size_t next = 0;
  int wstate = 0;   // 0..3 write lanes, 4..5 settle, 6 offer
  int idle = 0;
  const int kMaxCycles = 400000;

  for (int cyc = 0; cyc < kMaxCycles; ++cyc) {
    d.pw_en_i = 0;
    d.f_valid_i = 0;
    if (next < in.size()) {
      const Frag& f = in[next];
      if (wstate <= 3) {
        if (busy_tag[f.slot] != -1) {
          // RELEASE-AFTER-LAST-READER: the driver never rewrites a slot whose
          // fragment is in flight. Reaching here would be a bench bug; count it
          // and wait rather than corrupting the experiment silently.
          ++seam.reuse_faults;
        } else {
          d.pw_en_i = 1;
          d.pw_lane_i = wstate;
          d.pw_slot_i = f.slot;
          d.pw_data_i = plane_data(f, wstate);
          ++wstate;
        }
      } else if (wstate <= 5) {
        // PUBLICATION-BEFORE-READY: the registered enable lands the write one
        // edge after the beat, and the row is readable from then on. Two
        // settle cycles is one more than the minimum; the margin is deliberate.
        ++wstate;
      } else {
        d.f_valid_i = 1;
        d.f_sample_count_i = f.count;
        d.f_recipe_i = f.recipe;
        d.f_weight_i = f.weight;
        d.f_slot_i = f.slot;
        d.f_has_aux_i = f.has_aux ? 1 : 0;
        d.f_base_rgb_i = (f.base.r << 16) | (f.base.g << 8) | f.base.b;
        d.f_base_a_i = f.base.a;
        d.f_tag_i = f.tag;
      }
    }

    d.o_ready_i = stall_consumer ? ((cyc % 3) != 0) : 1;
    d.eval();

    // THE SEAM WATCH: every plane read names a slot owned by an accepted,
    // unretired fragment.
    if (d.src_rd_valid_o) {
      ++seam.reads;
      if (busy_tag[d.src_rd_slot_o] == -1) ++seam.violations;
    }

    const bool accepted = d.f_valid_i && d.f_ready_o;
    if (d.o_valid_o && d.o_ready_i) {
      Got g;
      g.r = (d.o_rgb_o >> 16) & 0xFF;
      g.g = (d.o_rgb_o >> 8) & 0xFF;
      g.b = d.o_rgb_o & 0xFF;
      g.a = d.o_a_o;
      g.refused = d.o_refused_o != 0;
      g.seen = true;
      const uint16_t tag = static_cast<uint16_t>(d.o_tag_o);
      out[tag] = g;
      auto it = slot_of_tag.find(tag);
      if (it != slot_of_tag.end() && busy_tag[it->second] == static_cast<int>(tag))
        busy_tag[it->second] = -1;   // released at the output, not before
    }

    tick(d);
    if (accepted) {
      busy_tag[in[next].slot] = in[next].tag;
      slot_of_tag[in[next].tag] = in[next].slot;
      ++next;
      wstate = 0;
    }

    if (next >= in.size() && !d.o_valid_o) {
      if (++idle > 12) break;
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

// V2's generator, verbatim: corners far more often than uniform.
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

// Slots are allocated round-robin over 64; with at most NCTX=8 jobs in flight
// plus one held output, a slot is never reused under a live fragment -- and
// the driver checks that rather than assuming it.
uint8_t slot_for(uint16_t tag) { return static_cast<uint8_t>(tag % 64); }

int compare_batch(const std::vector<Frag>& batch, const std::map<uint16_t, Got>& got,
                  int* missing_out) {
  int missing = 0, mismatched = 0, first_bad = -1;
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
            "  first mismatch: tag %u recipe %u count %u aux %d w=%u slot %u\n"
            "    s0 %3u %3u %3u %3u   s1 %3u %3u %3u %3u   s2 %3u %3u %3u %3u\n"
            "    want %3u %3u %3u %3u r=%d   got %3u %3u %3u %3u r=%d\n",
            f.tag, f.recipe, f.count, f.has_aux ? 1 : 0, f.weight, f.slot, f.s[0].r,
            f.s[0].g, f.s[0].b, f.s[0].a, f.s[1].r, f.s[1].g, f.s[1].b, f.s[1].a,
            f.s[2].r, f.s[2].g, f.s[2].b, f.s[2].a, want.r, want.g, want.b, want.a,
            want.refused ? 1 : 0, g.r, g.g, g.b, g.a, g.refused ? 1 : 0);
      }
    }
  }
  if (missing_out) *missing_out = missing;
  return mismatched;
}

void seam_checks(const Seam& seam, const char* where) {
  std::printf("  [%s] plane reads %ld, seam violations %ld, reuse faults %ld\n", where,
              seam.reads, seam.violations, seam.reuse_faults);
  check(seam.reads > 0, "the phase engine actually read the planes (non-vacuity)", 1,
        seam.reads > 0 ? 1 : 0);
#ifndef READLATE_EXPECT_MISMATCH
  check(seam.violations == 0,
        "every plane read named a slot owned by an accepted, unretired fragment "
        "(publication-before-read, release-after-last-reader)",
        0, seam.violations);
#endif
  check(seam.reuse_faults == 0, "the driver never rewrote a slot under a live fragment", 0,
        seam.reuse_faults);
}

// ---------------------------------------------------------------------------

void test_every_recipe_matches_the_oracle() {
  std::vector<Frag> batch;
  uint32_t st = 0xC0FFEEu;
  uint16_t tag = 1;
  for (uint8_t r = 0; r < mat::kRecipeCount; ++r)
    for (int i = 0; i < 200; ++i) {
      Frag f;
      f.recipe = r;
      f.count = 3;
      f.has_aux = (i & 1) != 0;   // alternate the plane the third sample lives in
      f.weight = byte_of(st);
      f.s[0] = sample_of(st);
      f.s[1] = sample_of(st);
      f.s[2] = sample_of(st);
      f.base = sample_of(st);
      f.tag = tag;
      f.slot = slot_for(tag);
      ++tag;
      batch.push_back(f);
    }

  uint32_t jobs[8] = {0};
  Seam seam;
  const std::map<uint16_t, Got> got = run_batch(batch, seam, jobs);
  int missing = 0;
  const int mismatched = compare_batch(batch, got, &missing);

#ifdef READLATE_EXPECT_MISMATCH
  std::printf("  MUTANT: %d fragments mismatched the oracle, %ld seam violations\n",
              mismatched, seam.violations);
  check(missing == 0, "the mutant still retires every fragment (the fault is silent)", 0,
        missing);
  check(mismatched > 0,
        "THE DIFFERENTIAL FIRES on the slot-swap mutant -- colours from a "
        "neighbouring owner's planes",
        1, mismatched > 0 ? 1 : 0);
  check(seam.violations > 0,
        "THE SEAM WATCH FIRES on the slot-swap mutant -- at least one read named a "
        "slot no accepted fragment owned",
        1, seam.violations > 0 ? 1 : 0);
#else
  check(missing == 0, "every fragment retired -- none was lost in the scheduler", 0,
        missing);
  check(mismatched == 0,
        "every recipe's result matches zref::material::combine exactly, with the "
        "samples read late from the planes and the third sample in either plane",
        0, mismatched);
  check(jobs[mat::kPassthru] == 0, "PASSTHRU issues no product jobs", 0, jobs[mat::kPassthru]);
  check(jobs[mat::kTerrainDetailLight] == 200 * 6,
        "DETAIL_LIGHT issues six product jobs per fragment", 200 * 6,
        jobs[mat::kTerrainDetailLight]);
  check(jobs[mat::kTerrainDetailMask] == 200 * 4, "DETAIL_MASK issues four", 200 * 4,
        jobs[mat::kTerrainDetailMask]);
  check(jobs[mat::kModulate] == 200 * 4, "MODULATE issues four", 200 * 4,
        jobs[mat::kModulate]);
  check(jobs[mat::kLerp] == 200 * 4, "LERP issues four", 200 * 4, jobs[mat::kLerp]);
#endif
  seam_checks(seam, "oracle batch");
}

// count in {0,1,2,3}: the canonicalisation now happens at D on the planes,
// with poison in every plane the fragment must not consult.
void test_canonicalisation_reads_only_the_planes_it_may() {
  std::vector<Frag> batch;
  uint32_t st = 0x5EED5EEDu;
  uint16_t tag = 0x800;
  for (uint8_t r = 0; r < mat::kRecipeCount; ++r)
    for (uint8_t c = 0; c <= 3; ++c)
      for (int i = 0; i < 12; ++i) {
        Frag f;
        f.recipe = r;
        f.count = c;
        f.has_aux = (i % 3) == 0;
        f.weight = byte_of(st);
        f.s[0] = sample_of(st);
        f.s[1] = sample_of(st);
        f.s[2] = sample_of(st);
        f.base = sample_of(st);
        f.tag = tag;
        f.slot = slot_for(tag);
        ++tag;
        batch.push_back(f);
      }
  Seam seam;
  const std::map<uint16_t, Got> got = run_batch(batch, seam);
  int missing = 0;
  const int mismatched = compare_batch(batch, got, &missing);
  check(missing == 0, "every mixed-count fragment retired (refusals included)", 0, missing);
  check(mismatched == 0,
        "count 0..3 all match the oracle with POISON in every plane the fragment may "
        "not read -- BASE through for count 0, s0 standing in for missing samples, "
        "the non-chosen of s2/aux never consulted",
        0, mismatched);
  int refused = 0;
  for (const Frag& f : batch)
    if (got.count(f.tag) && got.at(f.tag).refused) ++refused;
  check(refused > 0, "and some fragments were refused (non-vacuity of the refusal path)", 1,
        refused > 0 ? 1 : 0);
  seam_checks(seam, "mixed-count batch");
}

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
      f.tag = tag;
      f.slot = slot_for(tag);
      ++tag;
      batch.push_back(f);
    }
  uint32_t phases = 0;
  Seam seam;
  const std::map<uint16_t, Got> got = run_batch(batch, seam, nullptr, false, &phases);
  uint32_t want = 0;
  for (const Frag& f : batch) {
    if (f.recipe == mat::kTerrainDetailLight) want += 3;
    else if (f.recipe == mat::kPassthru || f.recipe == mat::kAddSat || f.recipe == mat::kMask)
      want += 1;
    else want += 2;
  }
  std::printf("  phases issued %u, the schedule owes %u, plane reads %ld\n", phases, want,
              seam.reads);
  check(phases == want, "the paired schedule issued EXACTLY the phases it owes", want, phases);
  check(seam.reads == static_cast<long>(want),
        "and read the planes EXACTLY once per phase -- one address per ticket, no "
        "re-read, no prefetch",
        want, seam.reads);
  check(got.size() == batch.size(), "and every fragment still came back",
        static_cast<long long>(batch.size()), static_cast<long long>(got.size()));
  seam_checks(seam, "phase batch");
}

void test_back_pressure_loses_nothing() {
  std::vector<Frag> batch;
  uint32_t st = 0x5EEDu;
  for (int i = 0; i < 120; ++i) {
    Frag f;
    f.recipe = static_cast<uint8_t>(i % mat::kRecipeCount);
    f.count = 3;
    f.has_aux = (i & 2) != 0;
    f.weight = byte_of(st);
    f.s[0] = sample_of(st);
    f.s[1] = sample_of(st);
    f.s[2] = sample_of(st);
    f.base = sample_of(st);
    f.tag = static_cast<uint16_t>(0x300 + i);
    f.slot = slot_for(f.tag);
    batch.push_back(f);
  }
  Seam seam;
  const std::map<uint16_t, Got> got = run_batch(batch, seam, nullptr, /*stall_consumer=*/true);
  int missing = 0;
  const int mismatched = compare_batch(batch, got, &missing);
  check(missing == 0, "a stalling consumer loses no fragment", 0, missing);
  check(mismatched == 0, "and corrupts none", 0, mismatched);
  seam_checks(seam, "back-pressure batch");
}

void test_a_cheap_fragment_overtakes_an_expensive_one() {
  Frag heavy;
  heavy.recipe = mat::kTerrainDetailLight;
  heavy.count = 3;
  heavy.s[0] = {200, 200, 200, 200};
  heavy.s[1] = {128, 128, 128, 128};
  heavy.s[2] = {255, 255, 255, 255};
  heavy.tag = 0x1000;
  heavy.slot = 5;
  Frag light;
  light.recipe = mat::kPassthru;
  light.count = 3;
  light.s[0] = {11, 22, 33, 44};
  light.tag = 0x2000;
  light.slot = 9;

  Dut d;
  d.rst_n = 0;
  d.o_ready_i = 1;
  d.f_valid_i = 0;
  d.pw_en_i = 0;
  for (int i = 0; i < 4; ++i) tick(d);
  d.rst_n = 1;

  // Write both owners' planes up front (both slots free), then offer the heavy
  // one and the light one back to back.
  const Frag* both[2] = {&heavy, &light};
  for (const Frag* f : both)
    for (int lane = 0; lane < 4; ++lane) {
      d.pw_en_i = 1;
      d.pw_lane_i = lane;
      d.pw_slot_i = f->slot;
      d.pw_data_i = plane_data(*f, lane);
      tick(d);
    }
  d.pw_en_i = 0;
  tick(d);
  tick(d);

  std::vector<uint16_t> order;
  std::size_t next = 0;
  for (int cyc = 0; cyc < 2000; ++cyc) {
    if (next < 2) {
      const Frag& f = *both[next];
      d.f_valid_i = 1;
      d.f_sample_count_i = f.count;
      d.f_recipe_i = f.recipe;
      d.f_weight_i = f.weight;
      d.f_slot_i = f.slot;
      d.f_has_aux_i = 0;
      d.f_base_rgb_i = 0;
      d.f_base_a_i = 0;
      d.f_tag_i = f.tag;
    } else {
      d.f_valid_i = 0;
    }
    d.eval();
    const bool accepted = d.f_valid_i && d.f_ready_o;
    if (d.o_valid_o) order.push_back(static_cast<uint16_t>(d.o_tag_o));
    tick(d);
    if (accepted) ++next;
    if (order.size() >= 2 && next >= 2) break;
  }
  check(order.size() >= 2, "both fragments retired", 2, static_cast<long long>(order.size()));
  if (order.size() >= 2)
    check(order[0] == 0x2000,
          "the PASSTHRU retires BEFORE the DETAIL_LIGHT it was submitted behind -- "
          "read-late did not serialise the scheduler",
          0x2000, order[0]);
}

void test_output_stall_keeps_every_context_reserved_and_planes_immutable() {
  Dut d;
  d.rst_n = 0;
  d.o_ready_i = 0;
  d.f_valid_i = 0;
  d.pw_en_i = 0;
  for (int i = 0; i < 4; ++i) tick(d);
  d.rst_n = 1;

  // Nine PASSTHRU fragments on nine slots, planes written first.
  std::vector<Frag> fr;
  for (int i = 0; i < 9; ++i) {
    Frag f;
    f.recipe = mat::kPassthru;
    f.count = 1;
    f.s[0] = {static_cast<uint8_t>(i + 1), 0x22, 0x33, 0xA5};
    f.tag = static_cast<uint16_t>(0x5000 + i);
    f.slot = static_cast<uint8_t>(10 + i);
    fr.push_back(f);
  }
  for (const Frag& f : fr)
    for (int lane = 0; lane < 4; ++lane) {
      d.pw_en_i = 1;
      d.pw_lane_i = lane;
      d.pw_slot_i = f.slot;
      d.pw_data_i = plane_data(f, lane);
      tick(d);
    }
  d.pw_en_i = 0;
  tick(d);
  tick(d);

  int accepted = 0;
  for (int cyc = 0; cyc < 256 && accepted < 9; ++cyc) {
    const Frag& f = fr[accepted];
    d.f_valid_i = 1;
    d.f_sample_count_i = f.count;
    d.f_recipe_i = f.recipe;
    d.f_weight_i = 0;
    d.f_slot_i = f.slot;
    d.f_has_aux_i = 0;
    d.f_base_rgb_i = 0;
    d.f_base_a_i = 0;
    d.f_tag_i = f.tag;
    d.eval();
    const bool admit = d.f_ready_o != 0;
    tick(d);
    if (admit) ++accepted;
  }
  check(accepted == 8,
        "a stopped output reserves all eight contexts -- loading the held register "
        "does not create a ninth credit",
        8, accepted);
  d.f_valid_i = 0;
  for (int i = 0; i < 40; ++i) tick(d);
  check(d.o_valid_o, "one completed result is held at the stopped output", 1,
        d.o_valid_o ? 1 : 0);

  const uint32_t held_rgb = d.o_rgb_o;
  const uint8_t held_a = d.o_a_o;
  const uint16_t held_tag = static_cast<uint16_t>(d.o_tag_o);
  bool stable = true;
  for (int cyc = 0; cyc < 64; ++cyc) {
    // Poison every live input, AND rewrite planes of slots no live fragment
    // owns (legal traffic: other owners committing). The eight held contexts'
    // planes are not touched -- that would be the release law's violation.
    d.f_valid_i = 1;
    d.f_tag_i = static_cast<uint16_t>(0x7000 + cyc);
    d.f_slot_i = static_cast<uint8_t>(10 + (cyc % 9));
    d.f_base_rgb_i = 0x00FFFFFFu ^ static_cast<uint32_t>(cyc);
    d.pw_en_i = 1;
    d.pw_lane_i = cyc & 3;
    d.pw_slot_i = static_cast<uint8_t>(40 + (cyc % 8));
    d.pw_data_i = 0xFFFFFFFFFFull ^ cyc;
    d.eval();
    stable = stable && d.o_valid_o && d.o_rgb_o == held_rgb && d.o_a_o == held_a &&
             static_cast<uint16_t>(d.o_tag_o) == held_tag && !d.f_ready_o;
    tick(d);
  }
  d.f_valid_i = 0;
  d.pw_en_i = 0;
  check(stable, "valid && !ready holds RGBA, tag and input backpressure stable while every "
                "pin is poisoned and foreign slots are rewritten",
        1, stable ? 1 : 0);

  // Release the output: each of the eight comes out with ITS OWN s0 -- the plane
  // it was read from, not the poison, not a neighbour.
  d.o_ready_i = 1;
  std::map<uint16_t, Got> got;
  for (int cyc = 0; cyc < 400 && got.size() < 8; ++cyc) {
    d.eval();
    if (d.o_valid_o) {
      Got g;
      g.r = (d.o_rgb_o >> 16) & 0xFF;
      g.g = (d.o_rgb_o >> 8) & 0xFF;
      g.b = d.o_rgb_o & 0xFF;
      g.a = d.o_a_o;
      g.seen = true;
      got[static_cast<uint16_t>(d.o_tag_o)] = g;
    }
    tick(d);
  }
  int wrong = 0;
  for (int i = 0; i < 8; ++i) {
    const auto it = got.find(fr[i].tag);
    if (it == got.end() || it->second.r != fr[i].s[0].r || it->second.g != 0x22 ||
        it->second.b != 0x33 || it->second.a != 0xA5)
      ++wrong;
  }
  check(wrong == 0, "all eight held fragments retired with their OWN plane's s0", 0, wrong);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

#ifdef READLATE_EXPECT_MISMATCH
  std::printf("[material_combine_readlate] MUTANT CONTROL -- passes when the checkers FIRE\n");
  test_every_recipe_matches_the_oracle();
#else
  test_every_recipe_matches_the_oracle();
  test_canonicalisation_reads_only_the_planes_it_may();
  test_the_schedule_issues_exactly_its_phases();
  test_back_pressure_loses_nothing();
  test_a_cheap_fragment_overtakes_an_expensive_one();
  test_output_stall_keeps_every_context_reserved_and_planes_immutable();
#endif

  if (g_failed) {
    std::printf("[material_combine_readlate_diff] %d/%d checks FAILED\n", g_failed, g_checks);
    return 1;
  }
  std::printf("[material_combine_readlate_diff] %d checks passed\n", g_checks);
  return 0;
}
