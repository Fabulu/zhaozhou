// zref_geom_wcache.hpp — the GEOM.WCACHE oracle.
//
// A bounded, direct-indexed vertex ARENA. Not a cache, and the difference is
// the whole design (owner ruling, docs/OWNER_DOCKET.md 2026-08-24).
//
// WHY AN ARENA IS SUFFICIENT, which is the part worth reading:
// the producer already knows each vertex's index. A terrain patch is a 33x33
// lattice and the tessellator holds vi/vj BEFORE it expands them into world
// coordinates; a skinned mesh has a vertex number. Identity is GIVEN, never
// inferred. An associative structure would pay tags, comparators and an eviction
// policy to rediscover something the producer never lost -- and it would answer
// "same place" when the question is "same vertex".
//
// WHAT THIS MODEL IS FOR: the RTL differential replays a command stream against
// it and compares every reply bit-for-bit, including the refusals. The refusal
// semantics are the interesting half; a hit is trivial and a wrong refusal is
// how a vertex silently becomes another vertex.
//
// THE ORIGIN IS NOT DECORATION. Position is carried rebased: one full-width
// origin per arena plus bounded local coordinates per vertex. The projector
// folds M*origin into its per-arena translation and multiplies only the local
// coordinates. Reconstructing origin+local before the multiplied row terms would
// forfeit the entire reason for the representation -- measured 2026-08-24,
// 32x27 costs 3 DSPs, exactly what 32x32 costs, so a wide operand on EITHER side
// loses the cheap band.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "zref/zref_fixp.hpp"

namespace zref {
namespace geom {

// One arena's datum, in fx16 raw words.
struct ArenaOrigin {
  int32_t x = 0;
  int32_t y = 0;
  int32_t z = 0;
};

// Why a lookup did not return a payload. MISS and the REFUSE_* classes are
// deliberately distinct: a miss says "project this vertex", a refusal says "the
// caller asked something illegal". Collapsing them lets a caller bug look like a
// cold arena forever, which is a thing that cannot be noticed from a hit rate.
enum class LookupStatus : uint8_t {
  kHit = 0,
  kMiss,              // in range, sealed, current generation, simply not filled
  kRefuseIndex,       // index >= depth
  kRefuseArena,       // arena >= arenas
  kRefuseUnsealed,    // the producer has not finished writing this arena
  kRefuseGeneration,  // stale: this key names a PREVIOUS use of the arena
};

struct LookupResult {
  LookupStatus status = LookupStatus::kMiss;
  uint64_t payload = 0;  // meaningful only when status == kHit

  constexpr bool hit() const { return status == LookupStatus::kHit; }
  constexpr bool refused() const {
    return status != LookupStatus::kHit && status != LookupStatus::kMiss;
  }
};

// A fill is accepted or dropped; a drop is a producer error and is sticky.
enum class FillStatus : uint8_t { kAccepted = 0, kDropIndex, kDropSealed, kDropArena };

class VertexArena {
 public:
  VertexArena(std::size_t arenas, std::size_t depth)
      : arenas_(arenas),
        depth_(depth),
        payload_(arenas * depth, 0),
        valid_(arenas * depth, 0),
        origin_(arenas),
        generation_(arenas, 0),
        sealed_(arenas, 0) {}

  std::size_t arenas() const { return arenas_; }
  std::size_t depth() const { return depth_; }

  // ---- producer side ------------------------------------------------------

  // Begin a new use of an arena: bump the generation, drop every valid bit,
  // unseal. The generation is what stops a reused arena from serving last
  // frame's vertices -- the failure a plain valid bit cannot catch, because the
  // bit is per-slot and the mistake is per-arena.
  //
  // The payload memory is NOT cleared, deliberately and to match the RTL: a
  // reset that touches the array prevents memory inference (QUARTUS_GOTCHAS 10),
  // so staleness must be answered by metadata, never by contents.
  uint32_t open(std::size_t arena) {
    if (arena >= arenas_) return 0;
    generation_[arena] = static_cast<uint32_t>(generation_[arena] + 1u);
    sealed_[arena] = 0;
    for (std::size_t i = 0; i < depth_; ++i) valid_[arena * depth_ + i] = 0;
    return generation_[arena];
  }

  void set_origin(std::size_t arena, const ArenaOrigin& o) {
    if (arena >= arenas_) return;
    origin_[arena] = o;
  }

  ArenaOrigin origin(std::size_t arena) const {
    if (arena >= arenas_) return ArenaOrigin{};
    return origin_[arena];
  }

  uint32_t generation(std::size_t arena) const { return arena < arenas_ ? generation_[arena] : 0u; }

  bool sealed(std::size_t arena) const { return arena < arenas_ && sealed_[arena] != 0; }

  FillStatus fill(std::size_t arena, std::size_t index, uint64_t payload) {
    if (arena >= arenas_) {
      overflow_ = true;
      return FillStatus::kDropArena;
    }
    if (index >= depth_) {
      overflow_ = true;
      return FillStatus::kDropIndex;
    }
    if (sealed_[arena]) {
      // Writing after sealing means the producer believes it is still filling
      // an arena the consumer is already replaying. Dropped and recorded rather
      // than silently honoured, because honouring it would change a payload a
      // consumer may already have read.
      overflow_ = true;
      return FillStatus::kDropSealed;
    }
    payload_[arena * depth_ + index] = payload;
    valid_[arena * depth_ + index] = 1;
    return FillStatus::kAccepted;
  }

  void seal(std::size_t arena) {
    if (arena < arenas_) sealed_[arena] = 1;
  }

  // ---- consumer side ------------------------------------------------------

  // The order of these checks is part of the contract: a caller that gets BOTH
  // an out-of-range index and a stale generation is told about the index first,
  // so the diagnosis is stable rather than depending on which fault the
  // implementation happens to test first.
  LookupResult lookup(std::size_t arena, uint32_t gen, std::size_t index) {
    LookupResult r;
    if (arena >= arenas_) {
      r.status = LookupStatus::kRefuseArena;
      ++refusals_;
      return r;
    }
    if (index >= depth_) {
      r.status = LookupStatus::kRefuseIndex;
      ++refusals_;
      return r;
    }
    if (!sealed_[arena]) {
      r.status = LookupStatus::kRefuseUnsealed;
      ++refusals_;
      return r;
    }
    if (gen != generation_[arena]) {
      r.status = LookupStatus::kRefuseGeneration;
      ++refusals_;
      return r;
    }
    if (!valid_[arena * depth_ + index]) {
      r.status = LookupStatus::kMiss;
      ++misses_;
      return r;
    }
    r.status = LookupStatus::kHit;
    r.payload = payload_[arena * depth_ + index];
    ++hits_;
    return r;
  }

  // ---- counters -----------------------------------------------------------
  uint32_t hits() const { return hits_; }
  uint32_t misses() const { return misses_; }
  uint32_t refusals() const { return refusals_; }
  bool overflow() const { return overflow_; }

 private:
  std::size_t arenas_;
  std::size_t depth_;
  std::vector<uint64_t> payload_;
  std::vector<uint8_t> valid_;
  std::vector<ArenaOrigin> origin_;
  std::vector<uint32_t> generation_;
  std::vector<uint8_t> sealed_;

  uint32_t hits_ = 0;
  uint32_t misses_ = 0;
  uint32_t refusals_ = 0;
  bool overflow_ = false;  // sticky, like the RTL bit
};

}  // namespace geom
}  // namespace zref
