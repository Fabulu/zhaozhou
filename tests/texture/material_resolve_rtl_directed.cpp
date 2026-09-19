// material_resolve_rtl_directed.cpp -- does the RTL resolver agree with the
// oracle, field for field and refusal for refusal?
//
// ---------------------------------------------------------------------------
// WHAT IS AT RISK HERE, AND WHAT IS ALREADY PROVEN ELSEWHERE
// ---------------------------------------------------------------------------
// The LOOKUP RULES are not at risk and are not restated in this file.
// `zref::material::Resolver` owns them and
// `tests/texture/material_resolve_directed.cpp` already proves them in 32
// checks. Writing "a material_id at the count is refused" as a C++ expectation
// here would be a second implementation of the ratified rule -- the failure
// CLAUDE.md names for the projector's two cores. So every case below drives
// THE ORACLE AND THE RTL WITH THE SAME STIMULUS AND DIFFERENCES THEM.
//
// What IS at risk is everything the RTL adds on top of the rules:
//
//   * THE FIELD MAP. The record is 32 little-endian bytes and the RTL reads it
//     with sixteen bit offsets. One wrong offset returns a well-formed record
//     with `recipe_weight` where `control` should be -- and every handshake,
//     every counter and the whole status enum still agree. The picture would
//     simply be the wrong surface, which is a bug with no alarm.
//   * THE D-3 TAG. The cache tag carries the 16-bit residency generation. If
//     it carried only the handle's low 8 bits -- which is what a reader who
//     stopped at `handle32 {index:24, generation:8}` would write -- every 256th
//     republication would serve the PREVIOUS table's record. That is
//     unreachable in a short test unless it is aimed at deliberately, so it is.
//   * THE NARROWING. `MaterialSample.binding_slot` is u16; the flat request's
//     `base_binding_selector` is u8. A silent truncation names binding 0 for
//     slot 256.
//   * ATOMICITY. A record is fetched in four 64-bit beats and judged on the
//     last. A legality verdict taken on a partial fill passes or fails for
//     reasons that have nothing to do with the stored bytes.
//
// ---------------------------------------------------------------------------
// THE R1 CONTROL IS AN INSTANCE, NOT A MUTANT
// ---------------------------------------------------------------------------
// At the shipping `RECIPE_COUNT = 8` the recipe field is three bits, so the
// recipe arm of `refused_record_o` is STRUCTURALLY UNREACHABLE with legal
// stimulus. CLAUDE.md's remedy for an unreachable guard is a committed mutant;
// here the guard is a PARAMETER, and CLAUDE.md names that case too -- "break a
// layout with a parameter" is one of the three firings it accepts as stimulus.
// `tb_zhao_material_resolve.sv` therefore instantiates a SECOND resolver at the
// historical ceiling of 6, and the difference between the two response streams
// on a terrain recipe is audit R1's defect, asserted rather than described.
//
// Every counter this file asserts at zero is fired somewhere in it. The map is
// in `kCounterFirings` at the bottom and it is checked, not commented.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vtb_zhao_material_resolve.h"

#include "zhao_sim.hpp"
#include "zref/zref_material.hpp"
#include "zref/zref_material_resolve.hpp"

namespace mat = zref::material;

namespace {

int g_checks = 0;
int g_fails = 0;

void check(bool ok, const char* what, long long want, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_fails;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, want, got);
  }
}

// `zref::material::Status`, mirrored as the RTL's 3-bit encoding. Taken from
// the enum rather than restated as literals, so a renumbering moves both.
uint8_t st(mat::Status s) { return static_cast<uint8_t>(s); }

// ---------------------------------------------------------------------------
// THE RECORD, AS BYTES
//
// `memcpy` of the ABI struct rather than a hand-written packer. The struct's
// layout is pinned by 10 `static_assert`s in `runtime/include/zhao_abi.h`; a
// packer written here would be a SECOND statement of the layout, and the whole
// point of this test is that the RTL's offsets match the ONE that exists.
// ---------------------------------------------------------------------------
struct RecordBytes {
  uint8_t b[32];
};

RecordBytes to_bytes(const zhao_abi::ZhMaterialRecord& r) {
  RecordBytes out{};
  static_assert(sizeof(zhao_abi::ZhMaterialRecord) == 32, "record is not 32 B");
  std::memcpy(out.b, &r, 32);
  return out;
}

uint64_t beat_of(const RecordBytes& rb, int beat) {
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i)
    v |= static_cast<uint64_t>(rb.b[beat * 8 + i]) << (8 * i);
  return v;
}

zhao_abi::ZhMaterialRecord make_record(uint8_t sample_count, uint8_t recipe) {
  zhao_abi::ZhMaterialRecord r{};
  r.control = static_cast<uint8_t>((sample_count & 0x3u) | ((recipe & 0x7u) << 2));
  r.recipe_weight = 200;
  r.flags = 0x5;  // toon + alpha_test; bit 1 (ink) clear on purpose
  r.sample0.binding_slot = 0x0011;
  r.sample0.binding_generation = 1;
  r.sample0.modes = 0x02;  // tmu_mode 2, wrap 0 (repeat), mip 0
  r.sample1.binding_slot = 0x0022;
  r.sample1.binding_generation = 2;
  r.sample1.modes = 0x13;  // tmu_mode 3, wrap 1 (clamp)
  r.sample2.binding_slot = 0x0033;
  r.sample2.binding_generation = 3;
  r.sample2.modes = 0x81;  // tmu_mode 1, wrap 0, mip 2
  r.palette_base = 0xDEAD0000u;
  r.raster_state = 0x0000BEEFu;
  return r;
}

uint32_t handle32(uint32_t index, uint16_t generation) {
  return (index << 8) | (generation & 0xFFu);
}

// ---------------------------------------------------------------------------
// The model of the thing on the other side of the BOUNDARY.
//
// This is a test fixture standing in for local SDRAM, and saying so matters:
// the RTL's `mem_*` port has no producer in the console (see the module
// header -- `spec/memory_rules.md` 5f leaves slot->extent undecided). Driving
// it here is STIMULUS, which is what a bench is for, and it is not evidence
// that the console can fetch a material record. It cannot yet.
// ---------------------------------------------------------------------------
struct Sdram {
  uint32_t base = 0;
  std::vector<RecordBytes> records;

  RecordBytes at(uint32_t addr) const {
    const uint32_t off = addr - base;
    const uint32_t idx = off / 32u;
    if (idx < records.size()) return records[idx];
    return RecordBytes{};  // out of range: the id guard should have refused first
  }
};

using Dut = Vtb_zhao_material_resolve;

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
  top.mem_rsp_denied_i = 0;
  top.rsp_ready_i = 0;
  top.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
  zhao::tick(top);
}

void publish_dir(Dut& top, uint8_t entry, bool valid, uint32_t set_index,
                 uint16_t generation, uint32_t base, uint32_t count) {
  top.dir_we_i = 1;
  top.dir_entry_i = entry;
  top.dir_valid_i = valid ? 1 : 0;
  top.dir_set_index_i = set_index;
  top.dir_generation_i = generation;
  top.dir_base_i = base;
  top.dir_count_i = count;
  zhao::tick(top);
  top.dir_we_i = 0;
  top.eval();
}

// What one resolve produced, on both instances.
struct Answer {
  uint8_t a_status = 0xFF;
  bool a_has_record = false;
  RecordBytes a_record{};
  uint8_t b_status = 0xFF;
  uint32_t mem_addr = 0;
  bool fetched = false;
  uint8_t sample_count = 0;
  uint8_t recipe = 0;
  uint8_t weight = 0;
  uint8_t binding = 0;
  bool selector_overflow = false;
  uint32_t palette_base = 0;
  uint32_t raster_state = 0;
  uint8_t flags = 0;
  uint8_t tier = 0;
  bool lockstep_ok = true;
};

// Read the 256-bit `a_rsp_record_o` out of Verilator's word array.
RecordBytes read_record(Dut& top) {
  RecordBytes rb{};
  for (int w = 0; w < 8; ++w) {
    const uint32_t word = top.a_rsp_record_o[w];
    for (int b = 0; b < 4; ++b)
      rb.b[w * 4 + b] = static_cast<uint8_t>((word >> (8 * b)) & 0xFFu);
  }
  return rb;
}

Answer resolve_rtl(Dut& top, const Sdram& mem, uint32_t material_set,
                   uint16_t material_id, uint8_t tier, int rsp_stall = 0,
                   bool deny_fetch = false) {
  Answer out;

  // ---- offer the request --------------------------------------------------
  top.req_valid_i = 1;
  top.req_material_set_i = material_set;
  top.req_material_id_i = material_id;
  top.req_quality_tier_i = tier;
  top.eval();
  int guard = 0;
  while (!top.a_req_ready_o && ++guard < 64) zhao::tick(top);
  // THE LOCKSTEP ASSERTION: the control must be accepting on the same cycle.
  // If it is not, every comparison in this file is between two machines in
  // different states and means nothing.
  if (top.a_req_ready_o != top.b_req_ready_o) out.lockstep_ok = false;
  zhao::tick(top);
  top.req_valid_i = 0;
  top.eval();

  // ---- serve a fetch, if one is asked for ---------------------------------
  guard = 0;
  while (!top.a_rsp_valid_o && ++guard < 256) {
    if (top.a_mem_req_valid_o) {
      out.fetched = true;
      out.mem_addr = top.a_mem_req_addr_o;
      const RecordBytes rb = mem.at(out.mem_addr);
      top.mem_req_ready_i = 1;
      zhao::tick(top);
      top.mem_req_ready_i = 0;
      if (deny_fetch) {
        // R20: MEM.GUARD's refusal -- `violation` the cycle after the accept,
        // and NO beats, ever. The block must answer, not wait.
        top.mem_rsp_denied_i = 1;
        zhao::tick(top);
        top.mem_rsp_denied_i = 0;
        top.eval();
        continue;
      }
      // Four beats, low beat first -- the record's own little-endian order.
      for (int beat = 0; beat < 4; ++beat) {
        top.mem_rsp_valid_i = 1;
        top.mem_rsp_data_i = beat_of(rb, beat);
        zhao::tick(top);
      }
      top.mem_rsp_valid_i = 0;
      top.eval();
    } else {
      zhao::tick(top);
    }
  }

  // ---- hold the response, if asked, then read it --------------------------
  for (int i = 0; i < rsp_stall; ++i) {
    // The record must SURVIVE backpressure. A response that changes while the
    // consumer is not ready is the join fault CLAUDE.md records for the
    // metadata bank, and no counter here would see it.
    zhao::tick(top);
  }

  if (top.a_rsp_valid_o != top.b_rsp_valid_o) out.lockstep_ok = false;
  out.a_status = top.a_rsp_status_o;
  out.a_has_record = top.a_rsp_has_record_o != 0;
  out.a_record = read_record(top);
  out.b_status = top.b_rsp_status_o;
  out.sample_count = top.a_rsp_sample_count_o;
  out.recipe = top.a_rsp_material_recipe_o;
  out.weight = top.a_rsp_recipe_weight_o;
  out.binding = top.a_rsp_base_binding_o;
  out.selector_overflow = top.a_rsp_selector_overflow_o != 0;
  out.palette_base = top.a_rsp_palette_base_o;
  out.raster_state = top.a_rsp_raster_state_o;
  out.flags = top.a_rsp_flags_o;
  out.tier = top.a_rsp_quality_tier_o;

  top.rsp_ready_i = 1;
  zhao::tick(top);
  top.rsp_ready_i = 0;
  top.eval();
  return out;
}

bool same_bytes(const RecordBytes& x, const RecordBytes& y) {
  return std::memcmp(x.b, y.b, 32) == 0;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut top;

  constexpr uint32_t kSetIndex = 0x00ABCD;
  constexpr uint32_t kBase = 0x06A0'0000u;  // inside RENDER.ASSET_POOL, 5f
  constexpr uint32_t kCount = 6;

  // =========================================================================
  // CASE 1 -- NOT RESIDENT, before anything is published
  // =========================================================================
  {
    hard_reset(top);
    Sdram mem;
    mat::Resolver<16> oracle;
    mat::ResolveLedger L{};
    const uint32_t h = handle32(kSetIndex, 7);

    const mat::Result want = oracle.resolve({h, 0, 0}, &L);
    const Answer got = resolve_rtl(top, mem, h, 0, 0);

    check(got.a_status == st(want.status),
          "case 1: an unpublished set is a residency fault, not a stall",
          st(want.status), got.a_status);
    check(got.a_has_record == want.has_record,
          "case 1: a residency fault returns no record", want.has_record ? 1 : 0,
          got.a_has_record ? 1 : 0);
    check(!got.fetched, "case 1: a residency fault issues no memory read", 0,
          got.fetched ? 1 : 0);
    check(top.a_not_resident_o == L.not_resident,
          "case 1: not_resident counted", L.not_resident, top.a_not_resident_o);
    check(got.lockstep_ok, "case 1: both instances stay in lockstep", 1,
          got.lockstep_ok ? 1 : 0);
  }

  // =========================================================================
  // CASE 2 -- THE WHOLE FIELD MAP, one record, every offset
  //
  // The record's fields are given values that are distinct AND that would be
  // unmistakable if two offsets were exchanged: 0xDEAD0000 cannot be mistaken
  // for 0x0000BEEF the way 1 can be mistaken for 2.
  // =========================================================================
  {
    hard_reset(top);
    const zhao_abi::ZhMaterialRecord rec = make_record(2, mat::kModulate);

    Sdram mem;
    mem.base = kBase;
    for (uint32_t i = 0; i < kCount; ++i) mem.records.push_back(to_bytes(rec));

    mat::Table t;
    t.index = kSetIndex;
    t.generation = 0x0107;  // low byte 0x07 is what the handle carries
    for (uint32_t i = 0; i < kCount; ++i) t.records.push_back(rec);
    mat::Resolver<16> oracle;
    oracle.publish(t);
    mat::ResolveLedger L{};

    publish_dir(top, 0, true, kSetIndex, 0x0107, kBase, kCount);
    const uint32_t h = handle32(kSetIndex, 0x07);

    const mat::Result want = oracle.resolve({h, 3, 2}, &L);
    const Answer got = resolve_rtl(top, mem, h, 3, 2);

    check(got.a_status == st(want.status), "case 2: a first resolve MISSES",
          st(want.status), got.a_status);
    check(got.fetched, "case 2: a miss issues a memory read", 1,
          got.fetched ? 1 : 0);
    check(got.mem_addr == kBase + 3 * 32,
          "case 2: address is base + (material_id << 5)", kBase + 3 * 32,
          got.mem_addr);
    check(same_bytes(got.a_record, to_bytes(want.record)),
          "case 2: the returned record is the stored bytes, all 32", 0,
          same_bytes(got.a_record, to_bytes(want.record)) ? 0 : 1);

    // THE PROJECTION, field by field, each against the ORACLE's accessor and
    // not against a literal -- so a change to the packing moves both.
    check(got.sample_count == mat::sample_count_of(want.record),
          "case 2: sample_count <- control[1:0]",
          mat::sample_count_of(want.record), got.sample_count);
    check(got.recipe == mat::recipe_of(want.record),
          "case 2: material_recipe <- control[4:2]", mat::recipe_of(want.record),
          got.recipe);
    check(got.weight == want.record.recipe_weight,
          "case 2: recipe_weight carried whole", want.record.recipe_weight,
          got.weight);
    check(got.binding == (want.record.sample0.binding_slot & 0xFFu),
          "case 2: base_binding_selector <- sample0.binding_slot",
          want.record.sample0.binding_slot & 0xFFu, got.binding);
    check(!got.selector_overflow,
          "case 2: a slot under 256 is NOT a selector overflow", 0,
          got.selector_overflow ? 1 : 0);
    check(got.palette_base == want.record.palette_base,
          "case 2: palette_base carried whole", want.record.palette_base,
          got.palette_base);
    check(got.raster_state == want.record.raster_state,
          "case 2: raster_state carried whole", want.record.raster_state,
          got.raster_state);
    check(got.flags == (want.record.flags & 0xFFu),
          "case 2: flags low byte carried", want.record.flags & 0xFFu,
          got.flags);
    check(got.tier == 2, "case 2: the quality tier is echoed beside the answer",
          2, got.tier);
    check(top.a_rsp_sample0_modes_o == want.record.sample0.modes,
          "case 2: sample0 modes at its own offset", want.record.sample0.modes,
          top.a_rsp_sample0_modes_o);
    check(top.a_rsp_sample1_modes_o == want.record.sample1.modes,
          "case 2: sample1 modes at its own offset", want.record.sample1.modes,
          top.a_rsp_sample1_modes_o);
    check(top.a_rsp_sample2_modes_o == want.record.sample2.modes,
          "case 2: sample2 modes at its own offset", want.record.sample2.modes,
          top.a_rsp_sample2_modes_o);

    // ---- CASE 3, on the same fixture: the SECOND resolve HITS -------------
    const mat::Result want2 = oracle.resolve({h, 3, 2}, &L);
    const Answer got2 = resolve_rtl(top, mem, h, 3, 2);
    check(got2.a_status == st(want2.status), "case 3: the second resolve HITS",
          st(want2.status), got2.a_status);
    check(!got2.fetched, "case 3: a hit issues NO memory read", 0,
          got2.fetched ? 1 : 0);
    check(same_bytes(got2.a_record, got.a_record),
          "case 3: hit and miss return the SAME record", 0,
          same_bytes(got2.a_record, got.a_record) ? 0 : 1);
    check(top.a_material_hits_o == L.hits, "case 3: hits counted", L.hits,
          top.a_material_hits_o);
    check(top.a_material_misses_o == L.misses, "case 3: misses counted",
          L.misses, top.a_material_misses_o);

    // ---- CASE 4: a material_id at the count is REFUSED, not clamped -------
    const mat::Result want3 = oracle.resolve({h, kCount, 0}, &L);
    const Answer got3 = resolve_rtl(top, mem, h, static_cast<uint16_t>(kCount), 0);
    check(got3.a_status == st(want3.status),
          "case 4: material_id == count is refused", st(want3.status),
          got3.a_status);
    check(!got3.a_has_record,
          "case 4: a refused id returns NO record -- material 0 is a real "
          "material and clamping to it hides the bug",
          0, got3.a_has_record ? 1 : 0);
    check(!got3.fetched, "case 4: a refused id issues no memory read", 0,
          got3.fetched ? 1 : 0);
    check(top.a_refused_id_o == L.refused_id, "case 4: refused_id counted",
          L.refused_id, top.a_refused_id_o);
    check(top.a_material_refused_o == (L.refused_id + L.refused_record + L.not_resident),
          "case 4: material_refused is the sum of the three refusal classes",
          L.refused_id + L.refused_record + L.not_resident,
          top.a_material_refused_o);

    // ---- CASE 5: THE COHERENCE CASE. D-3, and the one that matters. -------
    //
    // Republish the SAME set index at a NEW generation with a DIFFERENT
    // record, and resolve the SAME material_id. The cache line for that id is
    // still valid and its physical tag still matches. Only the generation in
    // the tag makes the old line unmatchable -- so this MUST miss and MUST
    // return the new record, with NO FLUSH PERFORMED. A flush would hide a tag
    // bug rather than prevent one.
    zhao_abi::ZhMaterialRecord rec2 = make_record(3, mat::kTerrainDetailLight);
    rec2.palette_base = 0x1234'5678u;
    rec2.raster_state = 0x0BAD'F00Du;
    rec2.sample2.modes = 0x21;  // wrap 2 (mirror) -- legal, and different

    Sdram mem2;
    mem2.base = kBase + 0x1000;
    for (uint32_t i = 0; i < kCount; ++i) mem2.records.push_back(to_bytes(rec2));

    mat::Table t2;
    t2.index = kSetIndex;
    t2.generation = 0x0207;  // NEW 16-bit generation, SAME low byte 0x07
    for (uint32_t i = 0; i < kCount; ++i) t2.records.push_back(rec2);
    oracle.publish(t2);

    // No invalidate, no flush: only the directory write.
    publish_dir(top, 0, true, kSetIndex, 0x0207, mem2.base, kCount);

    const mat::Result want4 = oracle.resolve({h, 3, 0}, &L);
    const Answer got4 = resolve_rtl(top, mem2, h, 3, 0);
    check(got4.a_status == st(want4.status),
          "case 5 (D-3): a republish at a new generation MISSES -- the old "
          "line is structurally unmatchable",
          st(want4.status), got4.a_status);
    check(same_bytes(got4.a_record, to_bytes(rec2)),
          "case 5 (D-3): and returns the NEW record, no flush performed", 0,
          same_bytes(got4.a_record, to_bytes(rec2)) ? 0 : 1);
    check(got4.palette_base == rec2.palette_base,
          "case 5 (D-3): the projection follows the new record too",
          rec2.palette_base, got4.palette_base);
    // AND THE LOW BYTE IS THE SAME. This is the check that separates a 16-bit
    // tag from an 8-bit one: 0x0107 and 0x0207 are indistinguishable to the
    // handle, so a resolver tagging on the handle's byte would have HIT here
    // and returned the previous table's record.
    check((0x0107u & 0xFFu) == (0x0207u & 0xFFu),
          "case 5 (D-3): the two generations are equal in the 8 bits the "
          "HANDLE carries -- an 8-bit tag would have hit",
          1, ((0x0107u & 0xFFu) == (0x0207u & 0xFFu)) ? 1 : 0);
  }

  // =========================================================================
  // CASE 6 -- MALFORMED RECORDS. The layout makes sample_count == 4
  // unrepresentable (it is two bits), so the refusals exercised are the ones
  // the frozen layout DOES permit, exactly as the contract's directed-test
  // section records.
  // =========================================================================
  {
    hard_reset(top);
    struct Bad {
      const char* what;
      zhao_abi::ZhMaterialRecord rec;
    };
    std::vector<Bad> bads;
    {
      zhao_abi::ZhMaterialRecord r = make_record(2, mat::kModulate);
      r.control |= 0x20;  // a set RESERVED control bit
      bads.push_back({"a set reserved control bit", r});
    }
    {
      zhao_abi::ZhMaterialRecord r = make_record(2, mat::kModulate);
      r.rsv0 = 1;
      bads.push_back({"a non-zero reserved word rsv0", r});
    }
    {
      zhao_abi::ZhMaterialRecord r = make_record(2, mat::kModulate);
      r.rsv1 = 0x8000'0000u;
      bads.push_back({"a non-zero reserved word rsv1", r});
    }
    {
      zhao_abi::ZhMaterialRecord r = make_record(2, mat::kModulate);
      r.flags = 0x0008;  // a reserved FLAG bit
      bads.push_back({"a set reserved flag bit", r});
    }
    {
      zhao_abi::ZhMaterialRecord r = make_record(2, mat::kModulate);
      r.sample1.modes = 0x30;  // wrap == 3, RESERVED, on a CLAIMED sample
      bads.push_back({"wrap 3 on a claimed sample", r});
    }

    uint16_t id = 0;
    for (const Bad& bad : bads) {
      Sdram mem;
      mem.base = kBase;
      mem.records.push_back(to_bytes(bad.rec));

      mat::Table t;
      t.index = kSetIndex + id + 1;
      t.generation = 0x0300;
      t.records.push_back(bad.rec);
      mat::Resolver<16> oracle;
      oracle.publish(t);
      mat::ResolveLedger L{};

      publish_dir(top, 1, true, kSetIndex + id + 1, 0x0300, kBase, 1);
      const uint32_t h = handle32(kSetIndex + id + 1, 0x00);

      const mat::Result want = oracle.resolve({h, 0, 0}, &L);
      const Answer got = resolve_rtl(top, mem, h, 0, 0);
      check(got.a_status == st(want.status), bad.what, st(want.status),
            got.a_status);
      check(!got.a_has_record,
            "case 6: a malformed record returns NO record", 0,
            got.a_has_record ? 1 : 0);

      // AND IT IS NOT CACHED. Resolve again: a block that cached the malformed
      // record would now HIT and hand it over, having refused it once.
      const Answer again = resolve_rtl(top, mem, h, 0, 0);
      check(again.a_status == st(mat::Status::kRefusedRecord),
            "case 6: a malformed record is never cached -- the second resolve "
            "refuses again rather than hitting",
            st(mat::Status::kRefusedRecord), again.a_status);
      check(again.fetched,
            "case 6: and it goes back to memory, proving no line was written",
            1, again.fetched ? 1 : 0);
      ++id;
    }
    check(top.a_refused_record_o == 2 * bads.size(),
          "case 6: every malformed record counted, both times",
          static_cast<long long>(2 * bads.size()), top.a_refused_record_o);
  }

  // =========================================================================
  // CASE 7 -- wrap 3 on an UNCLAIMED sample is LEGAL
  //
  // The oracle inspects only the samples the record CLAIMS, "because they are
  // not read". A resolver that checked all three would refuse a perfectly good
  // one-sample material whose unused sample 2 holds leftover bytes.
  // =========================================================================
  {
    hard_reset(top);
    zhao_abi::ZhMaterialRecord r = make_record(1, mat::kPassthru);
    r.sample2.modes = 0x30;  // wrap 3 on sample 2, which count 1 does not claim

    Sdram mem;
    mem.base = kBase;
    mem.records.push_back(to_bytes(r));
    mat::Table t;
    t.index = 0x000042;
    t.generation = 0x0400;
    t.records.push_back(r);
    mat::Resolver<16> oracle;
    oracle.publish(t);
    mat::ResolveLedger L{};

    publish_dir(top, 2, true, 0x000042, 0x0400, kBase, 1);
    const uint32_t h = handle32(0x000042, 0x00);
    const mat::Result want = oracle.resolve({h, 0, 0}, &L);
    const Answer got = resolve_rtl(top, mem, h, 0, 0);
    check(got.a_status == st(want.status),
          "case 7: a reserved wrap on an UNCLAIMED sample does not refuse the "
          "record",
          st(want.status), got.a_status);
    check(mat::record_legal(r),
          "case 7: and the oracle agrees it is legal", 1,
          mat::record_legal(r) ? 1 : 0);
  }

  // =========================================================================
  // CASE 8 -- THE NARROWING. binding_slot is u16; the request field is u8.
  // =========================================================================
  {
    hard_reset(top);
    zhao_abi::ZhMaterialRecord r = make_record(2, mat::kModulate);
    r.sample0.binding_slot = 0x0140;  // 320: does not fit eight bits

    Sdram mem;
    mem.base = kBase;
    mem.records.push_back(to_bytes(r));
    publish_dir(top, 3, true, 0x000055, 0x0500, kBase, 1);
    const uint32_t h = handle32(0x000055, 0x00);
    const Answer got = resolve_rtl(top, mem, h, 0, 0);

    check(got.a_status == st(mat::Status::kMiss),
          "case 8: an over-wide binding slot is a RESOLVED record, not a "
          "refusal -- the record is well formed",
          st(mat::Status::kMiss), got.a_status);
    check(got.selector_overflow,
          "case 8: rsp_selector_overflow_o fires, so the binding resolver's "
          "req_selector_overflow_i has something true to carry",
          1, got.selector_overflow ? 1 : 0);
    check(got.binding == 0x40,
          "case 8: and the truncated value is reported rather than hidden",
          0x40, got.binding);
    check(top.a_selector_overflow_o == 1, "case 8: selector_overflow counted", 1,
          top.a_selector_overflow_o);
  }

  // =========================================================================
  // CASE 9 -- the recipe/count pairing is OBSERVED and never enforced
  // =========================================================================
  {
    hard_reset(top);
    // kModulate requires two samples; this record claims one. `record_legal`
    // accepts it -- the pairing is the COMBINER's law -- and the combiner will
    // refuse it downstream. The counter is how that becomes visible before
    // somebody debugs a blank triangle.
    zhao_abi::ZhMaterialRecord r = make_record(1, mat::kModulate);
    check(mat::record_legal(r),
          "case 9: the oracle's record_legal accepts a bad recipe/count pair",
          1, mat::record_legal(r) ? 1 : 0);
    check(!mat::count_legal(mat::kModulate, 1),
          "case 9: and count_legal -- the COMBINER's rule -- rejects it", 0,
          mat::count_legal(mat::kModulate, 1) ? 1 : 0);

    Sdram mem;
    mem.base = kBase;
    mem.records.push_back(to_bytes(r));
    publish_dir(top, 0, true, 0x000066, 0x0600, kBase, 1);
    const uint32_t h = handle32(0x000066, 0x00);
    const Answer got = resolve_rtl(top, mem, h, 0, 0);
    check(got.a_status == st(mat::Status::kMiss),
          "case 9: the resolve SUCCEEDS -- two implementations of one rule is "
          "the failure this block refuses",
          st(mat::Status::kMiss), got.a_status);
    check(top.a_recipe_count_mismatch_o == 1,
          "case 9: and recipe_count_mismatch_o fires", 1,
          top.a_recipe_count_mismatch_o);
  }

  // =========================================================================
  // CASE 10 -- AUDIT R1, as a difference between two ceilings
  // =========================================================================
  {
    hard_reset(top);
    for (uint8_t recipe : {mat::kTerrainDetailLight, mat::kTerrainDetailMask}) {
      const uint8_t need = mat::samples_required(recipe);
      zhao_abi::ZhMaterialRecord r = make_record(need, recipe);
      check(mat::record_legal(r),
            "case 10: the terrain recipe is legal to the CURRENT oracle", 1,
            mat::record_legal(r) ? 1 : 0);

      Sdram mem;
      mem.base = kBase;
      mem.records.push_back(to_bytes(r));
      publish_dir(top, 1, true, 0x000077u + recipe, 0x0700, kBase, 1);
      const uint32_t h = handle32(0x000077u + recipe, 0x00);
      const Answer got = resolve_rtl(top, mem, h, 0, 0);

      check(got.a_status == st(mat::Status::kMiss),
            "case 10: the SHIPPING resolver accepts a three-sample terrain "
            "recipe",
            st(mat::Status::kMiss), got.a_status);
      check(got.b_status == st(mat::Status::kRefusedRecord),
            "case 10: the RECIPE_COUNT=6 control REFUSES it -- this is audit "
            "R1's defect, fired on purpose",
            st(mat::Status::kRefusedRecord), got.b_status);
      check(got.lockstep_ok,
            "case 10: and the two instances answered the same cycle", 1,
            got.lockstep_ok ? 1 : 0);
    }
    check(top.b_refused_record_o == 2,
          "case 10: the control's refusal counter moved, twice", 2,
          top.b_refused_record_o);
    check(top.a_refused_record_o == 0,
          "case 10: and the shipping resolver refused nothing", 0,
          top.a_refused_record_o);
  }

  // =========================================================================
  // CASE 11 -- BACKPRESSURE. The response must HOLD.
  //
  // CLAUDE.md's metadata-bank defect is exactly this shape: a stall produced
  // "response A's data, A's token, and B's metadata" with every counter
  // balancing. So the assertion is on the RECORD HOLDING, not on a counter.
  // =========================================================================
  {
    hard_reset(top);
    const zhao_abi::ZhMaterialRecord r = make_record(2, mat::kLerp);
    Sdram mem;
    mem.base = kBase;
    mem.records.push_back(to_bytes(r));
    publish_dir(top, 0, true, 0x000088, 0x0800, kBase, 1);
    const uint32_t h = handle32(0x000088, 0x00);

    const Answer got = resolve_rtl(top, mem, h, 0, 0, /*rsp_stall=*/7);
    check(same_bytes(got.a_record, to_bytes(r)),
          "case 11: the record is unchanged after seven cycles of "
          "backpressure",
          0, same_bytes(got.a_record, to_bytes(r)) ? 0 : 1);
    check(got.a_status == st(mat::Status::kMiss),
          "case 11: and so is the status", st(mat::Status::kMiss),
          got.a_status);

    // And the requester really is stalled: req_ready must be low while a
    // response is pending. "A miss stalls the requester rather than returning
    // a default" is the contract's sentence and this is it.
    top.req_valid_i = 1;
    top.eval();
    check(top.a_req_ready_o == 0 || top.a_rsp_valid_o == 0,
          "case 11: the block never accepts a new request while holding an "
          "unretired response",
          1, 1);
    top.req_valid_i = 0;
    top.eval();
  }

  // =========================================================================
  // CASE 12 -- A DIFFERENTIAL SWEEP against the oracle.
  //
  // Directed cases prove the shapes; this proves there is no sixth shape. The
  // ids deliberately alias the 16 cache lines several times over, so hits,
  // misses and evictions interleave without the driver choosing when.
  // =========================================================================
  {
    hard_reset(top);
    constexpr uint32_t kN = 40;
    Sdram mem;
    mem.base = kBase;
    mat::Table t;
    t.index = 0x000099;
    t.generation = 0x0900;
    for (uint32_t i = 0; i < kN; ++i) {
      // A different, legal record per id, so a line serving the wrong id is a
      // wrong ANSWER and not merely a wrong tag.
      //
      // RECIPES 0..5 ONLY, AND THE REASON IS A FINDING RATHER THAN A DODGE.
      // This sweep REVISITS ids, which is the whole point of it -- hits,
      // misses and evictions have to interleave. The two ceilings judge
      // recipes 6 and 7 differently (case 10), and a legality divergence
      // becomes a CACHE divergence: the shipping resolver caches the terrain
      // record and HITS on the revisit while the control refuses it and goes
      // back to memory. From that moment the two instances are in different
      // states, the driver -- which serves fetches off instance A -- starves
      // B's, and every `b_*` reading after it is meaningless.
      //
      // So the lockstep the wrapper's header claims is real and CONDITIONAL:
      // it holds exactly while both instances return the same verdict. That is
      // now stated in `tb_zhao_material_resolve.sv` too. The terrain recipes
      // keep their own case, on fresh handles, where both instances fetch and
      // the divergence is the SUBJECT rather than a contaminant.
      const uint8_t recipe = static_cast<uint8_t>(i % 6u);
      zhao_abi::ZhMaterialRecord r =
          make_record(mat::samples_required(recipe), recipe);
      r.palette_base = 0xC0DE'0000u + i;
      r.raster_state = 0x0000'1000u + i;
      r.recipe_weight = static_cast<uint8_t>(i);
      mem.records.push_back(to_bytes(r));
      t.records.push_back(r);
    }
    mat::Resolver<16> oracle;
    oracle.publish(t);
    mat::ResolveLedger L{};
    publish_dir(top, 0, true, 0x000099, 0x0900, kBase, kN);
    const uint32_t h = handle32(0x000099, 0x00);

    // A walk that revisits: 0..39, then 0..19 again, then a stride of 17 which
    // is coprime with 16 so it lands on every line in a different order.
    std::vector<uint16_t> order;
    for (uint32_t i = 0; i < kN; ++i) order.push_back(static_cast<uint16_t>(i));
    for (uint32_t i = 0; i < 20; ++i) order.push_back(static_cast<uint16_t>(i));
    for (uint32_t i = 0; i < kN; ++i)
      order.push_back(static_cast<uint16_t>((i * 17) % kN));
    // and a handful past the end, to keep the refusal path in the mix
    for (uint32_t i = 0; i < 5; ++i)
      order.push_back(static_cast<uint16_t>(kN + i));

    int mismatches = 0;
    int status_mismatches = 0;
    for (uint16_t id : order) {
      const mat::Result want = oracle.resolve({h, id, 0}, &L);
      const Answer got = resolve_rtl(top, mem, h, id, 0);
      if (got.a_status != st(want.status)) {
        ++status_mismatches;
        if (status_mismatches <= 3)
          std::printf("  id %u: status want %u got %u\n", id, st(want.status),
                      got.a_status);
      }
      if (want.has_record && !same_bytes(got.a_record, to_bytes(want.record))) {
        ++mismatches;
        if (mismatches <= 3) std::printf("  id %u: record bytes differ\n", id);
      }
      if (!got.lockstep_ok) ++mismatches;
    }
    check(status_mismatches == 0,
          "case 12: every status in the sweep matches the oracle", 0,
          status_mismatches);
    check(mismatches == 0,
          "case 12: every returned record in the sweep matches the oracle", 0,
          mismatches);
    // The counters are the other half of the differential: a machine that
    // returns the right answers while missing twice as often is a machine with
    // a broken cache and a clean picture.
    check(top.a_material_hits_o == L.hits,
          "case 12: the RTL's hit count equals the oracle's -- a cache that "
          "returns right answers and misses twice as often is still broken",
          L.hits, top.a_material_hits_o);
    check(top.a_material_misses_o == L.misses, "case 12: and the miss count",
          L.misses, top.a_material_misses_o);
    check(top.a_refused_id_o == L.refused_id, "case 12: and the refusals",
          L.refused_id, top.a_refused_id_o);
  }

  // =========================================================================
  // THE COUNTER MAP -- asserted, not commented.
  //
  // CLAUDE.md: "every counter asserted zero needs a FIRING control or a stated
  // structural reason". Every counter this module owns is listed here with the
  // case that moves it, and this block re-runs a fixture that moves all of the
  // remaining ones together so none of them is only ever asserted at zero.
  // =========================================================================
  {
    hard_reset(top);
    // one not-resident, one refused id, one refused record, one hit, one miss,
    // one selector overflow, one recipe/count mismatch -- in one fixture.
    zhao_abi::ZhMaterialRecord ok = make_record(2, mat::kModulate);
    ok.sample0.binding_slot = 0x0201;  // 513: overflows the u8 selector
    zhao_abi::ZhMaterialRecord bad = make_record(1, mat::kModulate);  // pair bad
    bad.rsv0 = 7;                                                     // and malformed

    Sdram mem;
    mem.base = kBase;
    mem.records.push_back(to_bytes(ok));
    mem.records.push_back(to_bytes(bad));

    const uint32_t hs = handle32(0x0000AA, 0x00);
    resolve_rtl(top, mem, hs, 0, 0);  // NOT RESIDENT (nothing published yet)
    publish_dir(top, 0, true, 0x0000AA, 0x0A00, kBase, 2);
    resolve_rtl(top, mem, hs, 0, 0);  // MISS + selector overflow
    resolve_rtl(top, mem, hs, 0, 0);  // HIT  + selector overflow again
    resolve_rtl(top, mem, hs, 1, 0);  // REFUSED RECORD
    resolve_rtl(top, mem, hs, 9, 0);  // REFUSED ID

    struct Firing {
      const char* name;
      uint32_t value;
    };
    const Firing firings[] = {
        {"material_hits_o", top.a_material_hits_o},
        {"material_misses_o", top.a_material_misses_o},
        {"material_refused_o", top.a_material_refused_o},
        {"refused_id_o", top.a_refused_id_o},
        {"refused_record_o", top.a_refused_record_o},
        {"not_resident_o", top.a_not_resident_o},
        {"selector_overflow_o", top.a_selector_overflow_o},
    };
    for (const Firing& f : firings)
      check(f.value > 0,
            "counter map: this counter was SEEN TO FIRE, not asserted at zero",
            1, f.value);
    // `recipe_count_mismatch_o` fires in case 9 above; here the only pair-bad
    // record is also malformed, so it is refused before the pairing is read --
    // which is the correct order and is stated rather than left as a surprise.
    std::printf("counter map: hits=%u misses=%u refused=%u id=%u rec=%u "
                "nores=%u selov=%u\n",
                top.a_material_hits_o, top.a_material_misses_o,
                top.a_material_refused_o, top.a_refused_id_o,
                top.a_refused_record_o, top.a_not_resident_o,
                top.a_selector_overflow_o);
  }

  // =========================================================================
  // CASE R20 -- A DENIED FETCH is a defined fault, counted, never a hang, and
  // never cached. Differenced against `zref::material::Resolver::resolve(...,
  // fetch_denied = true)`, then the SAME request is served normally: it must
  // MISS (a real fetch), because the denial cached nothing.
  // =========================================================================
  {
    hard_reset(top);
    const zhao_abi::ZhMaterialRecord rec = make_record(2, mat::kModulate);
    Sdram mem;
    mem.base = kBase;
    for (uint32_t i = 0; i < kCount; ++i) mem.records.push_back(to_bytes(rec));
    mat::Table tb;
    tb.index = kSetIndex;
    tb.generation = 0x0107;
    for (uint32_t i = 0; i < kCount; ++i) tb.records.push_back(rec);
    mat::Resolver<16> oracle;
    oracle.publish(tb);
    mat::ResolveLedger L{};
    publish_dir(top, 0, true, kSetIndex, 0x0107, kBase, kCount);
    const uint32_t h = handle32(kSetIndex, 0x07);

    const mat::Result want = oracle.resolve({h, 2, 0}, &L, /*fetch_denied=*/true);
    const Answer got = resolve_rtl(top, mem, h, 2, 0, 0, /*deny_fetch=*/true);
    check(got.fetched, "case R20: the miss path asked memory", 1, got.fetched ? 1 : 0);
    check(got.a_status == st(want.status), "case R20: a denied fetch resolves to kFetchDenied",
          st(want.status), got.a_status);
    check(!got.a_has_record && !want.has_record, "case R20: and returns NO record", 0,
          got.a_has_record ? 1 : 0);
    check(top.a_fetch_denied_o == L.fetch_denied, "case R20: fetch_denied_o counts it",
          L.fetch_denied, top.a_fetch_denied_o);
    check(top.a_material_refused_o == 1, "case R20: and it is a refusal in the catalog total", 1,
          top.a_material_refused_o);

    const mat::Result want2 = oracle.resolve({h, 2, 0}, &L);
    const Answer got2 = resolve_rtl(top, mem, h, 2, 0);
    check(got2.a_status == st(want2.status) && want2.status == mat::Status::kMiss,
          "case R20: the retry MISSES -- the denial cached nothing", st(want2.status),
          got2.a_status);
    check(got2.a_has_record, "case R20: and the retry returns the record", 1,
          got2.a_has_record ? 1 : 0);
  }

  top.final();
  std::printf("material_resolve_rtl_directed: %d checks, %d failures\n",
              g_checks, g_fails);
  // `zhao::exit_hard` and not `return`: a Verilated main that returns hangs
  // under ctest for the full timeout with ~0 CPU (the libwinpthread deadlock
  // documented in tests/harness/zhao_sim.hpp).
  zhao::exit_hard(g_fails == 0 ? 0 : 1);
}
