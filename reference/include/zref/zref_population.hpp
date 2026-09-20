// zref_population.hpp -- the BRIDGE from SetPopulation 0x0303 to the particle
// engine's population descriptor bank (`zhao_part_pop`), owner ruling R41
// (reports/OWNER-RULINGS-20260919-EVENING.md, 2026-09-19).
//
// R41: "Ratify `SetPopulation` (origin, plane, active_count), lowered by
// CMD.EXEC. active_count also seeds PART.STATE's first generation, replacing
// I1's provisional HPS seed."
//
// This header IS that bridge: the one place a SetPopulation record becomes the
// values the particle engine holds. `zhao_part_pop`
// (fpga/rtl/particles/zhao_part_pop.sv) computes exactly this, and
// tests/particles/part_pop_directed.cpp differences every field of it.
//
// WHAT IS CARRIED UNCHANGED, AND WHY THERE IS NO ARITHMETIC HERE. The command
// was ratified in the engine's OWN formats rather than in a convenient one, so
// this bridge is a VALIDATOR, not a converter:
//
//   * origin x/y/z travel on the 1/256-m grid spec/qformats.md 10 states, which
//     is the grid `zhao_part_terrain_tap` already works in
//     (`world = origin + (local <<< 8)`). Converting here would mean the wire
//     format and the silicon disagreed and one of them rounded.
//   * the plane normal travels as PART.COLLIDE's own signed Q1.10, and its
//     constant as PART.COLLIDE's own Q10. `n . p = c` is the surface.
//
// A CONVERSION THAT CANNOT BE EXACT IS A REFUSAL, NEVER A TRUNCATION. Two
// fields are wider on the wire than in the engine, and both are refused rather
// than narrowed, because the narrowing is invisible and wrong in a way that
// looks like physics:
//
//   * a plane normal component outside signed 12 bits. Truncating 0x0800 to 12
//     bits gives -2048: a +Y plane silently becomes a -Y plane, and every
//     particle falls through the floor it was supposed to rest on.
//   * an `active_count` above the store's CAPACITY. Truncating it seeds a
//     generation whose length is a wrapped number, so the store reads records
//     that were never written.
//
// A REFUSED RECORD IS REFUSED WHOLE. Origin, plane and seed all come from one
// record and describe one population; applying the two fields that fit and
// dropping the one that did not would leave the engine holding a descriptor
// no author ever wrote.
#pragma once

#include <cstdint>

namespace zref {
namespace population {

/** PART.COLLIDE's normal format (`NRM_W` = 12, `NRM_Q` = 10). */
inline constexpr int kNormalBits = 12;
inline constexpr int32_t kNormalMin = -(1 << (kNormalBits - 1));   // -2048
inline constexpr int32_t kNormalMax = (1 << (kNormalBits - 1)) - 1;  // 2047

/** SetPopulation 0x0303's payload, as the wire carries it. */
struct Record {
  uint32_t population = 0;
  int32_t origin_x = 0;      // 1/256-m grid
  int32_t origin_y = 0;
  int32_t origin_z = 0;
  uint32_t active_count = 0;
  int32_t plane_c = 0;       // Q10
  int16_t plane_nx = 0;      // Q1.10
  int16_t plane_ny = 0;
  int16_t plane_nz = 0;
  uint16_t flags = 0;        // b0 seed_first_generation, b1 plane_enable
};

inline constexpr uint16_t kFlagSeed = 0x0001u;
inline constexpr uint16_t kFlagPlaneEnable = 0x0002u;
/** b2..b15 are reserved zero; a record setting one is refused. */
inline constexpr uint16_t kFlagsKnown = kFlagSeed | kFlagPlaneEnable;

/** Why a record was refused. Each is a counter on `zhao_part_pop`. */
enum class Refusal : uint8_t {
  kNone = 0,
  kNormalRange = 1,   // a plane component outside signed 12 bits
  kCountRange = 2,    // active_count above the store's capacity
  kReservedFlag = 3,  // a flag bit this ratification does not define
};

/** What the engine holds after a record. `seed_pending` is one shot: the bank
 *  owes the store a seed, and clears it when the store takes it. */
struct Bank {
  uint32_t population = 0;
  int32_t origin[3] = {0, 0, 0};
  bool plane_en = false;
  int16_t plane_n[3] = {0, 0, 0};
  int32_t plane_c = 0;
  uint32_t active_count = 0;
  bool seed_pending = false;
  uint32_t seed_count = 0;
  // evidence
  uint32_t taken = 0;
  uint32_t refused_normal = 0;
  uint32_t refused_count = 0;
  uint32_t refused_flags = 0;
};

inline bool normal_fits(int32_t v) { return v >= kNormalMin && v <= kNormalMax; }

/** Classify a record against the engine's formats. Order matters and is the
 *  order `zhao_part_pop` tests in: flags, then the normal, then the count. */
inline Refusal classify(const Record& r, uint32_t capacity) {
  if ((r.flags & static_cast<uint16_t>(~kFlagsKnown)) != 0u) return Refusal::kReservedFlag;
  if (!normal_fits(r.plane_nx) || !normal_fits(r.plane_ny) || !normal_fits(r.plane_nz))
    return Refusal::kNormalRange;
  if (r.active_count > capacity) return Refusal::kCountRange;
  return Refusal::kNone;
}

/** Apply one record. A refused record changes nothing but a counter. */
inline void apply(Bank& b, const Record& r, uint32_t capacity) {
  switch (classify(r, capacity)) {
    case Refusal::kReservedFlag: ++b.refused_flags; return;
    case Refusal::kNormalRange: ++b.refused_normal; return;
    case Refusal::kCountRange: ++b.refused_count; return;
    case Refusal::kNone: break;
  }
  b.population = r.population;
  b.origin[0] = r.origin_x;
  b.origin[1] = r.origin_y;
  b.origin[2] = r.origin_z;
  b.plane_en = (r.flags & kFlagPlaneEnable) != 0u;
  b.plane_n[0] = r.plane_nx;
  b.plane_n[1] = r.plane_ny;
  b.plane_n[2] = r.plane_nz;
  b.plane_c = r.plane_c;
  b.active_count = r.active_count;
  if ((r.flags & kFlagSeed) != 0u) {
    // A newer seed replaces one the store has not taken yet: the latest
    // committed descriptor is the descriptor, exactly as SetEnvironment's
    // latest committed environment is the environment.
    b.seed_pending = true;
    b.seed_count = r.active_count;
  }
  ++b.taken;
}

}  // namespace population
}  // namespace zref
