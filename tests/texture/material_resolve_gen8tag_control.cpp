// material_resolve_gen8tag_control.cpp -- EVIDENCE ABOUT THE INSTRUMENT.
//
// INVERTED POLARITY: this file PASSES WHEN THE COHERENCE CASE FAILS.
//
// ---------------------------------------------------------------------------
// WHY IT EXISTS
// ---------------------------------------------------------------------------
// `material_resolve_rtl_directed.cpp` case 5 asserts owner ruling D-3: after a
// material table is republished at a new residency generation, a resolve of an
// already-cached material_id must MISS and return the NEW record, with no
// flush performed. That case is green.
//
// A green case is a claim, and CLAUDE.md says the claim to check hardest is
// the one that reads as everything-is-fine. D-3 is a STRUCTURAL property --
// the old line is "structurally unable to match" -- not a guarded state with a
// counter, so no legal stimulus can make the shipping resolver fail it. Case 5
// has therefore never been seen to go red, and "the 16-bit tag is load-bearing"
// is an argument rather than evidence.
//
// `tests/mutants/zhao_material_resolve_gen8tag_mutant.sv` is the same block
// with the tag comparison narrowed to the low EIGHT bits of the generation --
// the width a `handle32` carries, and precisely the tag a reader who stopped
// at `{index:24, generation:8}` would have written. This file drives that copy
// through case 5's exact fixture and REQUIRES the stale hit.
//
// So: case 5 going green on production and this file going green on the mutant
// are two halves of one piece of evidence. Either alone is worth much less.
//
// ---------------------------------------------------------------------------
// WHAT IT DELIBERATELY DOES NOT DO
// ---------------------------------------------------------------------------
// It does not re-check the lookup rules, the field map, the refusals or the
// counters. Those are the directed suite's and re-asserting them here would
// make this file fail for reasons that have nothing to do with the tag -- and
// a control that can fail for many reasons is not a control.
//
// It also does not assert "the counter fires", because there is no counter
// here to fire. CLAUDE.md: "Do not write a test that asserts the bug" applies
// to the PRODUCTION suite; this file is not the production suite, it is the
// bug, committed on purpose, with its polarity declared in its name and its
// first line.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "verilated.h"

#include "Vzhao_material_resolve_gen8tag_mutant.h"

#include "zhao_sim.hpp"
#include "zref/zref_material.hpp"
#include "zref/zref_material_resolve.hpp"

namespace mat = zref::material;

namespace {

using Dut = Vzhao_material_resolve_gen8tag_mutant;

constexpr uint8_t kStMiss = static_cast<uint8_t>(mat::Status::kMiss);
constexpr uint8_t kStHit = static_cast<uint8_t>(mat::Status::kHit);

zhao_abi::ZhMaterialRecord make_record(uint8_t sample_count, uint8_t recipe,
                                       uint32_t palette_base) {
  zhao_abi::ZhMaterialRecord r{};
  r.control = static_cast<uint8_t>((sample_count & 0x3u) | ((recipe & 0x7u) << 2));
  r.recipe_weight = 200;
  r.flags = 0x5;
  r.sample0.binding_slot = 0x0011;
  r.sample0.binding_generation = 1;
  r.sample0.modes = 0x02;
  r.sample1.binding_slot = 0x0022;
  r.sample1.binding_generation = 2;
  r.sample1.modes = 0x13;
  r.palette_base = palette_base;
  r.raster_state = 0x0000BEEFu;
  return r;
}

void record_beats(const zhao_abi::ZhMaterialRecord& r, uint64_t beats[4]) {
  uint8_t b[32];
  std::memcpy(b, &r, 32);
  for (int beat = 0; beat < 4; ++beat) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
      v |= static_cast<uint64_t>(b[beat * 8 + i]) << (8 * i);
    beats[beat] = v;
  }
}

void hard_reset(Dut& top) {
  top.rst_n = 0;
  top.dir_we_i = 0;
  top.dir_entry_i = 0;
  top.dir_valid_i = 0;
  top.dir_set_index_i = 0;
  top.dir_generation_i = 0;
  top.dir_base_i = 0;
  top.dir_count_i = 0;
  top.req_valid_i = 0;
  top.req_material_set_i = 0;
  top.req_material_id_i = 0;
  top.req_quality_tier_i = 0;
  top.mem_req_ready_i = 0;
  top.mem_rsp_valid_i = 0;
  top.mem_rsp_data_i = 0;
  top.rsp_ready_i = 0;
  top.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
  zhao::tick(top);
}

void publish_dir(Dut& top, uint16_t generation, uint32_t base, uint32_t count,
                 uint32_t set_index) {
  top.dir_we_i = 1;
  top.dir_entry_i = 0;
  top.dir_valid_i = 1;
  top.dir_set_index_i = set_index;
  top.dir_generation_i = generation;
  top.dir_base_i = base;
  top.dir_count_i = count;
  zhao::tick(top);
  top.dir_we_i = 0;
  top.eval();
}

struct Answer {
  uint8_t status = 0xFF;
  uint32_t palette_base = 0;
  bool fetched = false;
};

Answer resolve(Dut& top, const zhao_abi::ZhMaterialRecord& stored,
               uint32_t handle, uint16_t id) {
  Answer out;
  uint64_t beats[4];
  record_beats(stored, beats);

  top.req_valid_i = 1;
  top.req_material_set_i = handle;
  top.req_material_id_i = id;
  top.req_quality_tier_i = 0;
  top.eval();
  int guard = 0;
  while (!top.req_ready_o && ++guard < 64) zhao::tick(top);
  zhao::tick(top);
  top.req_valid_i = 0;
  top.eval();

  guard = 0;
  while (!top.rsp_valid_o && ++guard < 256) {
    if (top.mem_req_valid_o) {
      out.fetched = true;
      top.mem_req_ready_i = 1;
      zhao::tick(top);
      top.mem_req_ready_i = 0;
      for (int beat = 0; beat < 4; ++beat) {
        top.mem_rsp_valid_i = 1;
        top.mem_rsp_data_i = beats[beat];
        zhao::tick(top);
      }
      top.mem_rsp_valid_i = 0;
      top.eval();
    } else {
      zhao::tick(top);
    }
  }

  out.status = top.rsp_status_o;
  out.palette_base = top.rsp_palette_base_o;
  top.rsp_ready_i = 1;
  zhao::tick(top);
  top.rsp_ready_i = 0;
  top.eval();
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut top;
  hard_reset(top);

  constexpr uint32_t kSetIndex = 0x00ABCD;
  constexpr uint32_t kBase = 0x06A0'0000u;
  constexpr uint32_t kCount = 6;

  // The two publications. They differ in the HIGH byte of the generation and
  // agree in the low byte -- so they are indistinguishable to a handle32 and
  // must be distinguishable to a D-3 tag.
  constexpr uint16_t kGenA = 0x0107;
  constexpr uint16_t kGenB = 0x0207;
  static_assert((kGenA & 0xFFu) == (kGenB & 0xFFu),
                "the fixture is pointless unless the two generations agree in "
                "the eight bits a handle carries");

  const zhao_abi::ZhMaterialRecord rec_a = make_record(2, mat::kModulate, 0xAAAA'0001u);
  const zhao_abi::ZhMaterialRecord rec_b = make_record(2, mat::kModulate, 0xBBBB'0002u);
  const uint32_t handle = (kSetIndex << 8) | (kGenA & 0xFFu);

  // ---- publication A, and a resolve that populates the line ---------------
  publish_dir(top, kGenA, kBase, kCount, kSetIndex);
  const Answer first = resolve(top, rec_a, handle, 3);

  int problems = 0;
  if (first.status != kStMiss) {
    std::printf("CONTROL BROKEN: the first resolve should MISS, got status %u\n",
                first.status);
    ++problems;
  }
  if (first.palette_base != rec_a.palette_base) {
    std::printf("CONTROL BROKEN: the first resolve returned 0x%08X, want 0x%08X\n",
                first.palette_base, rec_a.palette_base);
    ++problems;
  }

  // ---- publication B at a NEW generation, no flush ------------------------
  publish_dir(top, kGenB, kBase, kCount, kSetIndex);
  const Answer second = resolve(top, rec_b, handle, 3);

  // ---- THE INVERSION ------------------------------------------------------
  // Production MISSES here and returns rec_b. The mutant's eight-bit tag still
  // matches, so it HITS and hands back publication A's record. THAT is what
  // this file requires.
  const bool stale_hit =
      (second.status == kStHit) && (second.palette_base == rec_a.palette_base);

  if (!stale_hit) {
    std::printf(
        "FAIL (inverted polarity): the gen8 mutant did NOT return a stale "
        "record.\n"
        "  status %u (wanted %u = kHit), palette_base 0x%08X (wanted 0x%08X, "
        "publication A's)\n"
        "  This control is supposed to FAIL D-3's coherence case. If it does "
        "not,\n"
        "  the mutation no longer reaches the tag -- the copy has drifted from "
        "the\n"
        "  production block, or the tag moved. Regenerate the mutant (see its "
        "header)\n"
        "  before trusting the directed suite's case 5, which this file is the "
        "only\n"
        "  evidence for.\n",
        second.status, kStHit, second.palette_base, rec_a.palette_base);
    ++problems;
  } else {
    std::printf(
        "material_resolve_gen8tag_control: the 8-bit-tag mutant returned "
        "publication A's record (0x%08X) for a resolve after publication B -- "
        "exactly the stale hit owner ruling D-3 forbids. The directed suite's "
        "case 5 is therefore a test that has been seen to fail.\n",
        second.palette_base);
  }

  // `zhao::exit_hard`, not `return`: a Verilated main that returns hangs under
  // ctest for the full timeout with ~0 CPU (tests/harness/zhao_sim.hpp).
  zhao::exit_hard(problems == 0 ? 0 : 1);
}
