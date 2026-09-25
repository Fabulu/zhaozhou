// material_resolve_directed.cpp — MATERIAL.RESOLVE's oracle.
// Authored 2026-09-05 (roadmap G2-A).
//
// The contract lists these cases by name and they are implemented one for one:
//
//   * every field of the record at its own offset;
//   * a material_id at the set count refused;
//   * sample_count 3 accepted and 4 refused;
//   * a cache hit and miss returning identical records;
//   * and the coherence case -- A RESOLVE AFTER A TABLE REPUBLISH MUST NOT
//     RETURN THE OLD RECORD.
//
// That last one is the reason owner ruling D-3 puts the residency generation in
// the cache tag, and it is the case that would otherwise be found as a wrong
// texture on screen weeks later.

#include <cstdint>
#include <cstdio>

#include "zref/zref_material_resolve.hpp"

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

namespace mat = zref::material;

zhao_abi::ZhMaterialRecord make_record(uint8_t sample_count, uint8_t recipe) {
  zhao_abi::ZhMaterialRecord r{};
  r.control = static_cast<uint8_t>((sample_count & 0x3u) | ((recipe & 0x7u) << 2));
  r.recipe_weight = 200;
  r.flags = 0x5;  // toon + alpha_test
  r.sample0.binding_slot = 11;
  r.sample0.binding_generation = 1;
  r.sample0.modes = 0x02;  // tmu_mode 2, wrap 0, mip 0
  r.sample1.binding_slot = 22;
  r.sample1.binding_generation = 2;
  r.sample1.modes = 0x13;  // tmu_mode 3, wrap 1
  r.sample2.binding_slot = 33;
  r.sample2.binding_generation = 3;
  r.sample2.modes = 0x81;  // tmu_mode 1, wrap 0, mip 2
  r.palette_base = 0xDEAD0000u;
  r.raster_state = 0x0000BEEFu;
  return r;
}

uint32_t handle32(uint32_t index, uint16_t generation) {
  return (index << 8) | (generation & 0xFFu);
}

mat::Table make_table(uint32_t index, uint16_t gen, int n, uint8_t recipe = 1) {
  mat::Table t;
  t.index = index;
  t.generation = gen;
  for (int i = 0; i < n; ++i)
    t.records.push_back(make_record(static_cast<uint8_t>(i % 4 == 0 ? 1 : 2), recipe));
  return t;
}

void test_every_field_survives_at_its_own_offset() {
  mat::Resolver<> R;
  mat::Table t = make_table(5, 1, 4);
  t.records[2] = make_record(3, 4);  // sample_count 3, recipe ADD_SAT
  R.publish(t);

  mat::ResolveLedger L;
  const mat::Result r = R.resolve({handle32(5, 1), 2, 0}, &L);
  check(r.has_record, "a resident id resolves", 1, r.has_record ? 1 : 0);
  check(mat::sample_count_of(r.record) == 3, "sample_count unpacks from control", 3,
        mat::sample_count_of(r.record));
  check(mat::recipe_of(r.record) == 4, "recipe unpacks from control", 4, mat::recipe_of(r.record));
  check(r.record.recipe_weight == 200, "recipe_weight survives", 200, r.record.recipe_weight);
  check(r.record.flags == 0x5, "flags survive", 0x5, r.record.flags);
  check(r.record.sample1.binding_slot == 22, "sample1 binding_slot survives", 22,
        r.record.sample1.binding_slot);
  check(mat::tmu_mode_of(r.record.sample1) == 3, "tmu_mode unpacks from modes", 3,
        mat::tmu_mode_of(r.record.sample1));
  check(mat::wrap_of(r.record.sample1) == 1, "wrap unpacks from modes", 1,
        mat::wrap_of(r.record.sample1));
  check(mat::mip_policy_of(r.record.sample2) == 2, "mip_policy unpacks", 2,
        mat::mip_policy_of(r.record.sample2));
  check(r.record.palette_base == 0xDEAD0000u, "palette_base survives", 0xDEAD0000LL,
        static_cast<long long>(r.record.palette_base));
  check(r.record.raster_state == 0x0000BEEFu, "raster_state survives", 0xBEEF,
        static_cast<long long>(r.record.raster_state));
}

void test_id_past_the_count_is_refused_not_clamped() {
  mat::Resolver<> R;
  R.publish(make_table(1, 1, 4));
  mat::ResolveLedger L;

  const mat::Result at = R.resolve({handle32(1, 1), 4, 0}, &L);  // count is 4
  check(at.status == mat::Status::kRefusedId, "material_id AT the count is refused", 2,
        static_cast<long long>(at.status));
  check(!at.has_record, "and returns no record -- not material 0", 0, at.has_record ? 1 : 0);
  check(L.refused_id == 1, "and is counted", 1, L.refused_id);

  const mat::Result far = R.resolve({handle32(1, 1), 9999, 0}, &L);
  check(far.status == mat::Status::kRefusedId, "so is one far past", 2,
        static_cast<long long>(far.status));
  check(L.refused_id == 2, "counted again", 2, L.refused_id);
}

void test_sample_count_three_accepted_four_impossible_and_reserved_bits_refused() {
  mat::Resolver<> R;
  mat::Table t = make_table(2, 1, 3);
  t.records[0] = make_record(3, 0);  // three samples: the ruling limit, legal
  // sample_count is two bits, so 4 CANNOT be expressed -- the frozen layout
  // makes the malformed value unrepresentable, which is better than checking
  // for it. What CAN be malformed is a reserved bit, so that is the case.
  t.records[1] = make_record(2, 1);
  t.records[1].control |= 0x20;  // a reserved control bit set
  t.records[2] = make_record(2, 1);
  // RENAMED 2026-09-25 (FRAGSTATE): this word was `rsv1` and is now
  // `fragment_decl`. All ones still sets reserved bits [7:1] and [31:24], so
  // the case is unchanged in what it proves -- a record from a newer format
  // being read by older logic is refused rather than guessed at.
  t.records[2].fragment_decl = 0xFFFFFFFFu;
  R.publish(t);

  mat::ResolveLedger L;
  check(R.resolve({handle32(2, 1), 0, 0}, &L).has_record,
        "sample_count 3 is accepted -- it is the ruling limit", 1, 1);
  check(R.resolve({handle32(2, 1), 1, 0}, &L).status == mat::Status::kRefusedRecord,
        "a set reserved control bit is REFUSED", 3, 3);
  check(R.resolve({handle32(2, 1), 2, 0}, &L).status == mat::Status::kRefusedRecord,
        "a non-zero reserved word is REFUSED", 3, 3);
  check(L.refused_record == 2, "both malformed records counted", 2, L.refused_record);
}

// THE FRAGMENT PROFILE'S OWN LEGALITY (FRAGSTATE, 2026-09-25).
//
// `MaterialRecord.fragment_state` and `fragment_decl` were `rsv0`/`rsv1` until
// the owner vacation directive of 2026-09-23 section 3 allocated them. This is
// the fire demonstration for the CONTRADICTORY-DECLARATION rule, which is a new
// way to reach an existing counter: `refused_record` was already proven to move,
// but "this counter can fire" says nothing about whether it fires for THIS
// fault, and a rule nobody has watched refuse anything is a claim.
//
// All four cases use LEGAL STIMULUS -- a record is just bytes, so every one of
// them is constructible and no mutant is needed.
void test_fragment_profile_declaration_and_its_contradiction() {
  mat::Resolver<> R;
  mat::Table t = make_table(5, 2, 5);

  // 0: DECLARED, and everything about it is legal.
  t.records[0] = make_record(2, 1);
  t.records[0].fragment_state = 0x0044001Bu;  // sun_additive's word
  t.records[0].fragment_decl = 0x007F5A01u;   // sref 0x7F, tag 0x5A, DECLARED

  // 1: DECLARED, state all zero. THE CASE A `!= 0` TEST WOULD MISS. The opaque
  // profile is bit-identical to declaring nothing, so this record is legal and
  // its declaration is real, and only bit 0 says so.
  t.records[1] = make_record(2, 1);
  t.records[1].fragment_state = 0x00000000u;
  t.records[1].fragment_decl = 0x00000001u;

  // 2: CONTRADICTORY -- a state word filled and the flag forgotten. This is the
  // mistake the shape invites, and it is REFUSED rather than masked: masking
  // would draw the material opaque with no diagnostic anywhere.
  t.records[2] = make_record(2, 1);
  t.records[2].fragment_state = 0x0044001Bu;
  t.records[2].fragment_decl = 0x00000000u;

  // 3: CONTRADICTORY the other way -- a tag and a stencil reference with no
  // declaration. Same rule, a different payload, so the check cannot be passing
  // by only ever looking at the state word.
  t.records[3] = make_record(2, 1);
  t.records[3].fragment_state = 0x00000000u;
  t.records[3].fragment_decl = 0x007F5A00u;

  // 4: a RESERVED bit of fragment_decl set, with the declaration made. Legal
  // declaration, illegal record.
  t.records[4] = make_record(2, 1);
  t.records[4].fragment_decl = 0x00000003u;  // bit 1 is reserved
  R.publish(t);

  mat::ResolveLedger L;
  const auto d0 = R.resolve({handle32(5, 2), 0, 0}, &L);
  check(d0.has_record, "a declared fragment profile is ACCEPTED", 1, 1);
  check(d0.record.fragment_state == 0x0044001Bu,
        "and the state word is carried through unchanged", 1, 1);
  check(d0.record.fragment_decl == 0x007F5A01u,
        "and so are the tag and the stencil reference", 1, 1);

  check(R.resolve({handle32(5, 2), 1, 0}, &L).has_record,
        "a DECLARED all-zero state word is legal -- the opaque profile is a "
        "profile, and only fragment_decl bit 0 distinguishes it from silence",
        1, 1);

  check(R.resolve({handle32(5, 2), 2, 0}, &L).status == mat::Status::kRefusedRecord,
        "a state word with the declaration flag CLEAR is REFUSED, not masked",
        3, 3);
  check(R.resolve({handle32(5, 2), 3, 0}, &L).status == mat::Status::kRefusedRecord,
        "a tag and stencil reference with the flag CLEAR are REFUSED too", 3, 3);
  check(R.resolve({handle32(5, 2), 4, 0}, &L).status == mat::Status::kRefusedRecord,
        "a reserved fragment_decl bit is REFUSED", 3, 3);

  // THE COUNTER IS SEEN TO MOVE, by exactly the number of refusals and no more.
  // Two accepted records sit beside the three refused ones in the same table, so
  // a rule that refused everything would fail the checks above and a rule that
  // refused nothing would fail this one.
  check(L.refused_record == 3, "three contradictory/illegal records counted",
        3, L.refused_record);
}

void test_hit_and_miss_return_identical_records() {
  mat::Resolver<> R;
  R.publish(make_table(7, 3, 8));
  mat::ResolveLedger L;

  const mat::Result first = R.resolve({handle32(7, 3), 5, 0}, &L);
  const mat::Result second = R.resolve({handle32(7, 3), 5, 0}, &L);
  check(first.status == mat::Status::kMiss, "the first resolve MISSES", 1,
        static_cast<long long>(first.status));
  check(second.status == mat::Status::kHit, "the second HITS", 0,
        static_cast<long long>(second.status));

  const bool same = first.record.control == second.record.control &&
                    first.record.recipe_weight == second.record.recipe_weight &&
                    first.record.flags == second.record.flags &&
                    first.record.palette_base == second.record.palette_base &&
                    first.record.raster_state == second.record.raster_state &&
                    first.record.sample0.binding_slot == second.record.sample0.binding_slot &&
                    first.record.sample2.modes == second.record.sample2.modes;
  check(same, "and both return the IDENTICAL record", 1, same ? 1 : 0);
  check(L.hits == 1 && L.misses == 1, "one hit, one miss", 2, L.hits + L.misses);
}

// ---------------------------------------------------------------------------
// The coherence case D-3 exists for.
// ---------------------------------------------------------------------------
void test_a_resolve_after_republish_must_not_return_the_old_record() {
  mat::Resolver<> R;

  // Publish generation 1, with recipe 1 everywhere, and warm the cache.
  R.publish(make_table(9, 1, 4, /*recipe=*/1));
  mat::ResolveLedger L;
  const mat::Result before = R.resolve({handle32(9, 1), 2, 0}, &L);
  check(mat::recipe_of(before.record) == 1, "generation 1 gives recipe 1", 1,
        mat::recipe_of(before.record));
  check(R.resolve({handle32(9, 1), 2, 0}, &L).status == mat::Status::kHit, "and it is cached", 0,
        0);

  // Republish the SAME index at generation 2 with different contents. No flush
  // is performed, deliberately: D-3 says the tag makes old lines structurally
  // unmatchable, and a flush here would hide a tag bug instead of preventing it.
  R.publish(make_table(9, 2, 4, /*recipe=*/4));

  const mat::Result after = R.resolve({handle32(9, 2), 2, 0}, &L);
  check(after.status == mat::Status::kMiss,
        "a resolve after republish MISSES -- the old line cannot match", 1,
        static_cast<long long>(after.status));
  check(mat::recipe_of(after.record) == 4, "and returns the NEW record, not the cached old one", 4,
        mat::recipe_of(after.record));
}

void test_a_non_resident_set_is_a_residency_fault() {
  mat::Resolver<> R;
  R.publish(make_table(3, 1, 2));
  mat::ResolveLedger L;

  const mat::Result r = R.resolve({handle32(99, 1), 0, 0}, &L);
  check(r.status == mat::Status::kNotResident, "an unknown set is not resident", 4,
        static_cast<long long>(r.status));
  check(!r.has_record, "and produces no record", 0, r.has_record ? 1 : 0);
  check(L.not_resident == 1, "counted", 1, L.not_resident);

  // Right index, WRONG generation: also not resident. This is the stale-handle
  // case, and it must not silently resolve against the newer table.
  const mat::Result stale = R.resolve({handle32(3, 7), 0, 0}, &L);
  check(stale.status == mat::Status::kNotResident, "a stale generation is not resident either", 4,
        static_cast<long long>(stale.status));
}

}  // namespace

int main() {
  test_every_field_survives_at_its_own_offset();
  test_id_past_the_count_is_refused_not_clamped();
  test_sample_count_three_accepted_four_impossible_and_reserved_bits_refused();
  test_fragment_profile_declaration_and_its_contradiction();
  test_hit_and_miss_return_identical_records();
  test_a_resolve_after_republish_must_not_return_the_old_record();
  test_a_non_resident_set_is_a_residency_fault();

  if (g_failed) {
    std::printf("[material_resolve_directed] %d/%d checks FAILED\n", g_failed, g_checks);
    return 1;
  }
  std::printf("[material_resolve_directed] %d checks passed\n", g_checks);
  return 0;
}
