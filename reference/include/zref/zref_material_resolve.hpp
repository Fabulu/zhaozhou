// zref_material_resolve.hpp — MATERIAL.RESOLVE's oracle.
// Authored 2026-09-05 for roadmap gate G2-A.
//
// ---------------------------------------------------------------------------
// WHY NOW, AND NOT BEFORE
// ---------------------------------------------------------------------------
// `design/contracts/MATERIAL.RESOLVE.md` named this function and said:
//
//     Reference: `zref::material::resolve` — PLANNED AND NOT WRITTEN
//     ... Named without paths because neither exists.
//
// It could not sensibly be written earlier, because the record it returns was
// not frozen: "the record layout inside MATERIAL_SET is still not frozen -- the
// draft above is a draft, and the ABI generator owns the emitted constants."
// The freeze landed on 2026-09-05 as `zhao_abi::ZhMaterialRecord` (32 B), so
// this reads the ABI's record and does not restate it.
//
// ---------------------------------------------------------------------------
// THE TWO RULES THAT ARE NOT MINE TO SOFTEN
// ---------------------------------------------------------------------------
// **A miss stalls; it never returns a default.** The contract's reason, in its
// own words: "there is no sensible default material. A guessed material draws
// the wrong surface confidently, which is worse than a stall." So `resolve`
// reports a status and does not invent a record.
//
// **The cache tag includes the residency generation** (owner ruling D-3):
//
//     cache tag = physical line tag + residency generation
//
// Publishing a new table therefore makes every cached record from the old one
// STRUCTURALLY unable to match -- no flush is required for correctness. That is
// modelled here rather than assumed, because "the cache happened to miss" and
// "the cache could not possibly hit" are different guarantees, and only the
// second one survives someone adding a prefetch later.

#ifndef ZREF_MATERIAL_RESOLVE_HPP
#define ZREF_MATERIAL_RESOLVE_HPP

#include <cstdint>
#include <vector>

#include "zhao_abi.h"

namespace zref {
namespace material {

// The verdict. A resolve either produced a record or it did not, and the reason
// is never "here is a guess".
enum class Status : uint8_t {
  kHit = 0,          // served from the cache
  kMiss = 1,         // served from memory (a real fetch happened)
  kRefusedId = 2,    // material_id past the set's count
  kRefusedRecord = 3,// the stored record is malformed (sample_count > 3)
  kNotResident = 4,  // the material_set handle names no resident table
};

struct Request {
  uint32_t material_set = 0;  // handle32 {index:24, generation:8}
  uint32_t material_id = 0;
  uint8_t quality_tier = 0;
};

struct Result {
  Status status = Status::kNotResident;
  zhao_abi::ZhMaterialRecord record{};
  bool has_record = false;  // true only for kHit and kMiss
};

// Counters. Every refusal is counted, per the contract's insistence that a
// material_id past the count is "REFUSED and counted. Not clamped to zero:
// material 0 is a real material and drawing with it hides the bug."
struct Ledger {
  uint32_t hits = 0;
  uint32_t misses = 0;
  uint32_t refused_id = 0;
  uint32_t refused_record = 0;
  uint32_t not_resident = 0;
};

// A resident material table: an immutable, indexed set of records plus the
// 16-bit residency generation it was published under.
struct Table {
  uint32_t index = 0;        // handle index
  uint16_t generation = 0;   // D-3 residency generation (16-bit, never wrapped
                             // silently -- a wrap requires an epoch transition)
  std::vector<zhao_abi::ZhMaterialRecord> records;
};

// The record's own legality, separated so it can be called on stored data
// before anything trusts it. The contract names it:
// `zref::material::record_legal`.
inline bool record_legal(const zhao_abi::ZhMaterialRecord& r) {
  // control bits 0-1 are sample_count; the ruling limit is three, so the field
  // cannot express an illegal value -- but the RECORD can still be malformed in
  // ways the frozen layout permits, and those are what this checks.
  const uint8_t sample_count = static_cast<uint8_t>(r.control & 0x3u);
  const uint8_t recipe = static_cast<uint8_t>((r.control >> 2) & 0x7u);

  // control bits 5-7 are reserved and MUST be zero. A non-zero reserved field
  // is a record from a newer format being read by older logic, and guessing at
  // it is how a forward-compatibility bug becomes a wrong picture.
  if ((r.control & 0xE0u) != 0) return false;

  // Six ratified recipes (TEXTURE.COMBINE). A seventh encoding is refused, not
  // treated as passthrough.
  if (recipe >= 6) return false;

  // flags bits 3-15 reserved 0.
  if ((r.flags & 0xFFF8u) != 0) return false;

  // Reserved words must be zero.
  if (r.rsv0 != 0 || r.rsv1 != 0) return false;

  // Every sample the record CLAIMS must have legal modes; samples beyond the
  // count are not inspected, because they are not read.
  const zhao_abi::ZhMaterialSample* s[3] = {&r.sample0, &r.sample1, &r.sample2};
  for (uint8_t i = 0; i < sample_count; ++i) {
    const uint8_t wrap = static_cast<uint8_t>((s[i]->modes >> 4) & 0x3u);
    if (wrap == 3) return false;  // 3 is reserved by the frozen layout
  }
  return true;
}

// Convenience accessors, so callers read the packing in one place rather than
// each re-deriving the shifts.
inline uint8_t sample_count_of(const zhao_abi::ZhMaterialRecord& r) {
  return static_cast<uint8_t>(r.control & 0x3u);
}
inline uint8_t recipe_of(const zhao_abi::ZhMaterialRecord& r) {
  return static_cast<uint8_t>((r.control >> 2) & 0x7u);
}
inline uint8_t tmu_mode_of(const zhao_abi::ZhMaterialSample& s) {
  return static_cast<uint8_t>(s.modes & 0xFu);
}
inline uint8_t wrap_of(const zhao_abi::ZhMaterialSample& s) {
  return static_cast<uint8_t>((s.modes >> 4) & 0x3u);
}
inline uint8_t mip_policy_of(const zhao_abi::ZhMaterialSample& s) {
  return static_cast<uint8_t>((s.modes >> 6) & 0x3u);
}

// The direct-mapped cache the contract describes, with D-3's tag.
//
// Materials are resolved PER MESHLET, not per fragment, which is what makes a
// small cache sufficient -- the same material serves every triangle of a
// meshlet and usually many meshlets.
template <int kLines = 16>
class Resolver {
 public:
  void reset() {
    for (int i = 0; i < kLines; ++i) valid_[i] = false;
  }

  // Publishing a table does NOT flush. That is the point of D-3: the new
  // generation makes old lines structurally unmatchable, and a flush would
  // hide a tag bug rather than prevent one.
  void publish(const Table& t) { tables_.push_back(t); }

  const Table* find(uint32_t handle32) const {
    const uint32_t idx = handle32 >> 8;
    const uint8_t gen8 = static_cast<uint8_t>(handle32 & 0xFFu);
    for (std::size_t i = tables_.size(); i-- > 0;) {
      if (tables_[i].index == idx &&
          static_cast<uint8_t>(tables_[i].generation & 0xFFu) == gen8)
        return &tables_[i];
    }
    return nullptr;
  }

  Result resolve(const Request& q, Ledger* L = nullptr) {
    Result out;
    const Table* t = find(q.material_set);
    if (t == nullptr) {
      out.status = Status::kNotResident;
      if (L) L->not_resident++;
      return out;  // a residency fault, not a stall-forever: the frame is not
                   // published and the previous one repeats.
    }

    if (q.material_id >= t->records.size()) {
      out.status = Status::kRefusedId;
      if (L) L->refused_id++;
      return out;  // NOT clamped to zero: material 0 is a real material.
    }

    const int line = static_cast<int>(q.material_id % kLines);
    // THE D-3 TAG: physical line tag AND residency generation. Both must match.
    if (valid_[line] && tag_id_[line] == q.material_id &&
        tag_index_[line] == t->index && tag_gen_[line] == t->generation) {
      out.status = Status::kHit;
      out.record = line_[line];
      out.has_record = true;
      if (L) L->hits++;
      return out;
    }

    const zhao_abi::ZhMaterialRecord& r = t->records[q.material_id];
    if (!record_legal(r)) {
      out.status = Status::kRefusedRecord;
      if (L) L->refused_record++;
      return out;  // a malformed stored record is never cached
    }

    line_[line] = r;
    tag_id_[line] = q.material_id;
    tag_index_[line] = t->index;
    tag_gen_[line] = t->generation;
    valid_[line] = true;

    out.status = Status::kMiss;
    out.record = r;
    out.has_record = true;
    if (L) L->misses++;
    return out;
  }

 private:
  std::vector<Table> tables_;
  zhao_abi::ZhMaterialRecord line_[kLines]{};
  uint32_t tag_id_[kLines]{};
  uint32_t tag_index_[kLines]{};
  uint16_t tag_gen_[kLines]{};
  bool valid_[kLines]{};
};

}  // namespace material
}  // namespace zref

#endif  // ZREF_MATERIAL_RESOLVE_HPP
