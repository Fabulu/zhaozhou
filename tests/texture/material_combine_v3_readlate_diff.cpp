// material_combine_v3_readlate_diff.cpp -- copy vs owner-backed READ_LATE.
//
// Drives one atomic R9 stream through two real V3 engines in
// tb_material_combine_v3_copy_readlate.sv.  The READ_LATE engine reads four
// actual owner planes; the copy engine receives the identical typed values.
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

#include "verilated.h"
#include "../harness/zhao_sim.hpp"
#include "Vtb_material_combine_v3_copy_readlate.h"
#include "zref/zref_material.hpp"

namespace mat = zref::material;

namespace {

using Dut = Vtb_material_combine_v3_copy_readlate;

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
  uint8_t r = 0, g = 0, b = 0, a = 0;
  uint8_t raw_index = 0;
  uint8_t status = 0;
};

struct Frag {
  uint8_t recipe = 0;
  uint8_t count = 0;
  uint8_t weight = 0;
  bool aux_required = false;
  Plane s[3];
  Plane aux;
  Plane base;
  uint16_t tag = 0;
  uint8_t slot = 0;
};

struct Held {
  bool active = false;
  uint64_t result = 0;
  uint16_t tag = 0;
};

struct Stats {
  std::map<uint16_t, uint64_t> copy;
  std::map<uint16_t, uint64_t> late;
  std::set<uint8_t> late_live_slots;
  uint32_t late_reads = 0;
  uint32_t bad_read_slots = 0;
  uint32_t copy_hold_cycles = 0;
  uint32_t late_hold_cycles = 0;
  uint32_t hold_faults = 0;
  uint32_t duplicate_outputs = 0;
};

uint64_t pack_plane(const Plane& p) {
  return (static_cast<uint64_t>(p.status) << 40) |
         (static_cast<uint64_t>(p.raw_index) << 32) |
         (static_cast<uint64_t>(p.a) << 24) |
         (static_cast<uint64_t>(p.r) << 16) |
         (static_cast<uint64_t>(p.g) << 8) | p.b;
}

uint64_t pack_out(const mat::Out& out) {
  return (static_cast<uint64_t>(out.status) << 40) |
         (static_cast<uint64_t>(out.raw_index) << 32) |
         (static_cast<uint64_t>(out.a) << 24) |
         (static_cast<uint64_t>(out.r) << 16) |
         (static_cast<uint64_t>(out.g) << 8) | out.b;
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

void tick(Dut& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
}

void clear_inputs(Dut& d) {
  d.f_valid_i = 0;
  d.f_sample_count_i = 0;
  d.f_recipe_i = 0;
  d.f_weight_i = 0;
  d.f_aux_required_i = 0;
  d.f_base_rgb_i = 0;
  d.f_base_a_i = 0;
  d.f_tag_i = 0;
  d.f_slot_i = 0;
  d.f_s0_i = 0;
  d.f_s1_i = 0;
  d.f_s2_i = 0;
  d.f_aux_i = 0;
  d.pw_en_i = 0;
  d.pw_lane_i = 0;
  d.pw_slot_i = 0;
  d.pw_data_i = 0;
  d.copy_ready_i = 0;
  d.late_ready_i = 0;
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
  d.f_slot_i = f.slot;
  d.f_s0_i = pack_plane(f.s[0]);
  d.f_s1_i = pack_plane(f.s[1]);
  d.f_s2_i = pack_plane(f.s[2]);
  d.f_aux_i = pack_plane(f.aux);
}

void write_owner_planes(Dut& d, const Frag& f) {
  const uint64_t values[4] = {
      pack_plane(f.s[0]), pack_plane(f.s[1]),
      pack_plane(f.s[2]), pack_plane(f.aux)};
  d.f_valid_i = 0;
  d.copy_ready_i = 0;
  d.late_ready_i = 0;
  for (int lane = 0; lane < 4; ++lane) {
    d.pw_en_i = 1;
    d.pw_lane_i = lane;
    d.pw_slot_i = f.slot;
    d.pw_data_i = values[lane];
    tick(d);
  }
  d.pw_en_i = 0;
  tick(d);  // lands lane 3's registered write
  tick(d);  // publication-before-read margin
}

mat::Out oracle(const Frag& f, mat::Ledger* ledger) {
  mat::Sample samples[3];
  for (int i = 0; i < 3; ++i) {
    samples[i].r = f.s[i].r;
    samples[i].g = f.s[i].g;
    samples[i].b = f.s[i].b;
    samples[i].a = f.s[i].a;
    samples[i].raw_index = f.s[i].raw_index;
    samples[i].status = f.s[i].status;
  }
  mat::Sample base;
  base.r = f.base.r;
  base.g = f.base.g;
  base.b = f.base.b;
  base.a = f.base.a;
  mat::Aux aux;
  aux.status = f.aux.status;
  aux.tag = f.aux.raw_index;
  aux.strength = f.aux.a;
  return mat::combine(f.recipe, f.weight, samples, f.count, base, f.tag,
                      ledger, f.aux_required, aux);
}

uint32_t phase_demand(const Frag& f) {
  const mat::Out out = oracle(f, nullptr);
  return out.status != 0 ? 1u : mat::phases_required(f.recipe);
}

uint32_t source_read_demand(const Frag& f) {
  if (f.count == 0 && !f.aux_required) return 0;
  return phase_demand(f);
}

void observe_hold(bool valid, bool ready, uint64_t result, uint16_t tag,
                  Held& held, uint32_t& held_cycles, Stats& stats) {
  if (held.active) {
    if (!valid || result != held.result || tag != held.tag) ++stats.hold_faults;
  }
  if (valid && !ready) {
    if (!held.active) {
      held.result = result;
      held.tag = tag;
    }
    held.active = true;
    ++held_cycles;
  } else {
    held.active = false;
  }
}

void run_chunk(Dut& d, std::vector<Frag> chunk, Stats& stats, int& clock_seed) {
  for (std::size_t i = 0; i < chunk.size(); ++i) {
    chunk[i].slot = static_cast<uint8_t>(i);
    write_owner_planes(d, chunk[i]);
  }

  std::size_t next = 0;
  std::size_t copy_retired = 0;
  std::size_t late_retired = 0;
  Held copy_held, late_held;

  for (int local_cycle = 0; local_cycle < 200000; ++local_cycle, ++clock_seed) {
    if (next < chunk.size()) drive_frag(d, chunk[next]);
    else d.f_valid_i = 0;

    d.copy_ready_i = ((clock_seed % 5) != 1) && ((clock_seed % 11) != 7);
    d.late_ready_i = ((clock_seed % 7) != 2) && ((clock_seed % 13) != 5);
    d.eval();

    observe_hold(d.copy_valid_o != 0, d.copy_ready_i != 0,
                 static_cast<uint64_t>(d.copy_result_o),
                 static_cast<uint16_t>(d.copy_tag_o), copy_held,
                 stats.copy_hold_cycles, stats);
    observe_hold(d.late_valid_o != 0, d.late_ready_i != 0,
                 static_cast<uint64_t>(d.late_result_o),
                 static_cast<uint16_t>(d.late_tag_o), late_held,
                 stats.late_hold_cycles, stats);

    const bool accepted = d.f_valid_i && d.f_ready_o;
    const bool copy_fire = d.copy_valid_o && d.copy_ready_i;
    const bool late_fire = d.late_valid_o && d.late_ready_i;

    if (d.late_src_rd_valid_o) {
      ++stats.late_reads;
      if (stats.late_live_slots.count(
              static_cast<uint8_t>(d.late_src_rd_slot_o)) == 0)
        ++stats.bad_read_slots;
    }

    if (copy_fire) {
      if (!stats.copy.emplace(static_cast<uint16_t>(d.copy_tag_o),
                              static_cast<uint64_t>(d.copy_result_o)).second)
        ++stats.duplicate_outputs;
      ++copy_retired;
    }
    if (late_fire) {
      if (!stats.late.emplace(static_cast<uint16_t>(d.late_tag_o),
                              static_cast<uint64_t>(d.late_result_o)).second)
        ++stats.duplicate_outputs;
      const uint16_t tag = static_cast<uint16_t>(d.late_tag_o);
      for (const Frag& f : chunk)
        if (f.tag == tag) stats.late_live_slots.erase(f.slot);
      ++late_retired;
    }

    tick(d);
    if (accepted) {
      stats.late_live_slots.insert(chunk[next].slot);
      ++next;
    }

    d.eval();
    if (next == chunk.size() && copy_retired == chunk.size() &&
        late_retired == chunk.size() && d.copy_idle_o && d.late_idle_o)
      break;
  }

  check(next == chunk.size(), "paired input accepted the whole chunk", chunk.size(), next);
  check(copy_retired == chunk.size(), "copy mode retired the whole chunk",
        chunk.size(), copy_retired);
  check(late_retired == chunk.size(), "READ_LATE retired the whole chunk",
        chunk.size(), late_retired);
  check(stats.late_live_slots.empty(),
        "READ_LATE released every source slot only after retirement", 0,
        stats.late_live_slots.size());
}

std::vector<Frag> build_vectors() {
  std::vector<Frag> out;
  uint16_t tag = 1;
  static const uint8_t corners[6] = {0, 1, 127, 128, 254, 255};

  for (uint8_t recipe = 0; recipe < 8; ++recipe) {
    for (int i = 0; i < 48; ++i) {
      Frag f;
      f.recipe = recipe;
      f.count = recipe == 0 ? static_cast<uint8_t>((i & 1) != 0)
                            : static_cast<uint8_t>(recipe >= 6 ? 3 : 2);
      f.weight = corners[(i + 4) % 6];
      f.aux_required = (i % 7) == 0;
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
      out.push_back(f);
    }
  }

  // Every exact-count cell, including all count-zero malformed cases.
  for (uint8_t recipe = 0; recipe < 8; ++recipe) {
    for (uint8_t count = 0; count < 4; ++count) {
      Frag f;
      f.recipe = recipe;
      f.count = count;
      f.weight = 128;
      f.s[0] = plane(20, 30, 40, 50, 0xA5);
      f.s[1] = plane(60, 70, 80, 90, 0xB6);
      f.s[2] = plane(100, 110, 120, 130, 0xC7);
      f.base = plane(7, 8, 9, 10);
      f.tag = tag++;
      out.push_back(f);
    }
  }

  // Required-only status reduction with every unrequested plane poisoned.
  for (uint8_t count = 0; count < 4; ++count) {
    Frag f;
    f.recipe = count <= 1 ? 0 : (count == 2 ? 1 : 6);
    f.count = count;
    f.aux_required = (count & 1) != 0;
    f.s[0] = plane(10, 20, 30, 40, 0xA5, 0x02);
    f.s[1] = plane(50, 60, 70, 80, 0xB6, 0x04);
    f.s[2] = plane(90, 100, 110, 120, 0xC7, 0x08);
    f.aux = plane(1, 2, 3, 4, 0xD8, 0x10);
    f.base = plane(7, 8, 9, 10);
    f.tag = tag++;
    out.push_back(f);
  }

  // Exact saturation cases for ADD, MOD2X and both detail first layers.
  Frag f;
  f.recipe = 4; f.count = 2;
  f.s[0] = plane(200, 1, 2, 255); f.s[1] = plane(100, 2, 3, 255);
  f.tag = tag++; out.push_back(f);
  f = {}; f.recipe = 2; f.count = 2;
  f.s[0] = plane(200, 200, 200, 17); f.s[1] = plane(200, 200, 200, 99);
  f.tag = tag++; out.push_back(f);
  f = {}; f.recipe = 6; f.count = 3;
  f.s[0] = plane(255, 255, 255, 17); f.s[1] = plane(255, 255, 255, 99);
  f.s[2] = plane(128, 128, 128, 3); f.tag = tag++; out.push_back(f);
  f = {}; f.recipe = 7; f.count = 3;
  f.s[0] = plane(255, 255, 255, 240); f.s[1] = plane(255, 255, 255, 99);
  f.s[2] = plane(0, 0, 0, 128); f.tag = tag++; out.push_back(f);

  return out;
}

void test_copy_and_readlate_are_identical() {
  Dut* const model = new Dut;
  Dut& d = *model;
  reset(d);
  check(d.copy_idle_o && d.late_idle_o,
        "both implementations reset structurally idle", 1,
        d.copy_idle_o && d.late_idle_o ? 1 : 0);

  std::vector<Frag> vectors = build_vectors();
  Stats stats;
  int clock_seed = 0;

  // Isolate the no-source law before any other request can obscure its count.
  Frag zero;
  zero.recipe = 0;
  zero.count = 0;
  zero.base = plane(7, 8, 9, 10);
  zero.s[0] = plane(255, 0, 255, 255, 0xEE, 0xFF);
  zero.s[1].status = 0x80;
  zero.s[2].status = 0x40;
  zero.aux.status = 0x20;
  zero.tag = 0x3F00;
  run_chunk(d, {zero}, stats, clock_seed);
  check(stats.late_reads == 0,
        "READ_LATE count-zero/no-AUX makes zero source requests", 0,
        stats.late_reads);

  for (std::size_t begin = 0; begin < vectors.size(); begin += 8) {
    const std::size_t end =
        begin + 8 < vectors.size() ? begin + 8 : vectors.size();
    run_chunk(d, std::vector<Frag>(vectors.begin() + begin,
                                  vectors.begin() + end),
              stats, clock_seed);
  }

  mat::Ledger expected_ledger;
  std::map<uint16_t, uint64_t> expected;
  expected.emplace(zero.tag, pack_out(oracle(zero, &expected_ledger)));
  uint32_t expected_phases = phase_demand(zero);
  uint32_t expected_reads = source_read_demand(zero);
  uint32_t expected_jobs[8] = {0};
  if (oracle(zero, nullptr).status == 0)
    expected_jobs[zero.recipe] += mat::product_jobs(zero.recipe);

  for (const Frag& f : vectors) {
    const mat::Out want = oracle(f, &expected_ledger);
    expected.emplace(f.tag, pack_out(want));
    expected_phases += phase_demand(f);
    expected_reads += source_read_demand(f);
    if (want.status == 0)
      expected_jobs[f.recipe] += mat::product_jobs(f.recipe);
  }

  int copy_wrong = 0;
  int late_wrong = 0;
  int pair_wrong = 0;
  for (const auto& item : expected) {
    const auto ci = stats.copy.find(item.first);
    const auto li = stats.late.find(item.first);
    if (ci == stats.copy.end() || ci->second != item.second) ++copy_wrong;
    if (li == stats.late.end() || li->second != item.second) ++late_wrong;
    if (ci == stats.copy.end() || li == stats.late.end() ||
        ci->second != li->second)
      ++pair_wrong;
  }

  const uint32_t count = static_cast<uint32_t>(expected.size());
  check(stats.copy.size() == expected.size(), "copy mode emitted every tag",
        expected.size(), stats.copy.size());
  check(stats.late.size() == expected.size(), "READ_LATE emitted every tag",
        expected.size(), stats.late.size());
  check(copy_wrong == 0, "copy result48/tag matches R9 for every vector", 0,
        copy_wrong);
  check(late_wrong == 0, "owner-backed READ_LATE result48/tag matches R9", 0,
        late_wrong);
  check(pair_wrong == 0, "copy and READ_LATE are bit-identical by tag", 0,
        pair_wrong);
  check(stats.duplicate_outputs == 0, "neither implementation emits a tag twice",
        0, stats.duplicate_outputs);
  check(stats.hold_faults == 0,
        "both result48/tag outputs hold exactly under independent stalls", 0,
        stats.hold_faults);
  check(stats.copy_hold_cycles > 0 && stats.late_hold_cycles > 0,
        "both output hold checkers were exercised", 1,
        stats.copy_hold_cycles > 0 && stats.late_hold_cycles > 0 ? 1 : 0);
  check(stats.bad_read_slots == 0,
        "every READ_LATE source request names an accepted live slot", 0,
        stats.bad_read_slots);
  check(stats.late_reads == expected_reads,
        "READ_LATE makes exactly one source request per source-bearing phase",
        expected_reads, stats.late_reads);

  check(d.copy_jobs_accepted_o == count && d.copy_jobs_completed_o == count,
        "copy CJ/CD equal the paired admission count", count,
        d.copy_jobs_completed_o);
  check(d.late_jobs_accepted_o == count && d.late_jobs_completed_o == count,
        "READ_LATE CJ/CD equal the paired admission count", count,
        d.late_jobs_completed_o);
  check(d.copy_phases_issued_o == expected_phases &&
            d.copy_phases_completed_o == expected_phases,
        "copy PI/PC equal exact phase demand", expected_phases,
        d.copy_phases_completed_o);
  check(d.late_phases_issued_o == expected_phases &&
            d.late_phases_completed_o == expected_phases,
        "READ_LATE PI/PC equal exact phase demand", expected_phases,
        d.late_phases_completed_o);
  check(d.copy_saturated_add_o == expected_ledger.saturated_add &&
            d.late_saturated_add_o == expected_ledger.saturated_add,
        "both modes report exact ADD saturation count",
        expected_ledger.saturated_add, d.late_saturated_add_o);
  check(d.copy_saturated_mul2x_o == expected_ledger.saturated_mul2x &&
            d.late_saturated_mul2x_o == expected_ledger.saturated_mul2x,
        "both modes report exact MODULATE2X/detail saturation count",
        expected_ledger.saturated_mul2x, d.late_saturated_mul2x_o);

  int wrong_jobs = 0;
  for (int recipe = 0; recipe < 8; ++recipe) {
    if (d.copy_jobs_by_recipe_o[recipe] != expected_jobs[recipe] ||
        d.late_jobs_by_recipe_o[recipe] != expected_jobs[recipe])
      ++wrong_jobs;
  }
  check(wrong_jobs == 0,
        "jobs_by_recipe remains meaningful product jobs in both modes", 0,
        wrong_jobs);
  check(d.copy_idle_o && d.late_idle_o,
        "both implementations return to structural idle", 1,
        d.copy_idle_o && d.late_idle_o ? 1 : 0);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  test_copy_and_readlate_are_identical();

  if (g_failed) {
    std::printf("[material_combine_v3_readlate_diff] %d/%d checks FAILED\n",
                g_failed, g_checks);
    zhao::exit_hard(1);
  }
  std::printf("[material_combine_v3_readlate_diff] %d checks passed\n", g_checks);
  zhao::exit_hard(0);
}
