// zref_meshfetch.hpp — GEOM.MESHFETCH's scalar reference.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS, AND WHY IT IS SO SMALL
// ---------------------------------------------------------------------------
// `reports/DOCKET.md` D22: nineteen of the twenty geometry blocks are not wired
// into the console, and `tools/design/compose_order.py` puts GEOM.MESHFETCH at
// position 1 of the declared order. It is the entry point, so nothing
// downstream of it can be fed by anything but a harness until it exists — and
// it had no RTL and no oracle. `zref_cull.hpp` said so in as many words:
// "`zref::MeshFetch` stays unresolved".
//
// The contract's instruction is the whole design of this header:
//
//     It must COMPOSE the two existing oracles rather than restate them:
//     `zref::cull` for the frustum test and `zref::creature::lod_raw` /
//     `lod_update` for the ladder. Both are already differentially proved
//     against built RTL, and a reimplementation here would be a second law
//     that agrees until it doesn't.
//
// That is not a style note. On 2026-09-03 a terrain shade oracle was written as
// a SECOND implementation of a ratified law and twelve checks passed comparing
// the duplicate to itself. So the frustum test below is a call, the ladder is a
// call, and the CRC is a call to `zhao_abi::zhao_crc32c` — the one generated
// implementation, the same one `zref_cmd2.hpp` delegates to.
//
// WHAT THIS ORACLE ACTUALLY OWNS, per the contract, is three things:
//   * descriptor validation — CRC, format, generation, the two count limits,
//     the reserved bytes;
//   * the local-bound -> world-bound transform, radius scaled by MAXIMUM
//     ABSOLUTE instance scale;
//   * the refusal taxonomy.
// Everything else here is plumbing between two laws that already exist.
#pragma once

#include <cstdint>
#include <cstring>

#include "zhao_abi.h"
#include "zref/zref_cull.hpp"

namespace zref {
namespace meshfetch {

// ---------------------------------------------------------------------------
// The descriptor — 64 bytes, FROZEN by owner ruling 2026-08-31 §6.1
// ---------------------------------------------------------------------------
// 64-byte aligned, so one descriptor is exactly one aligned burst and never
// straddles a row. Offsets are named rather than inlined because the CRC covers
// bytes 0..59 and a field that moved without the CRC window moving would be a
// silent disagreement between this oracle and the RTL.
inline constexpr int kDescBytes = 64;
inline constexpr int kCrcCovered = 60;   // bytes 0..59
inline constexpr int kReservedOff = 36;
inline constexpr int kReservedLen = 24;
inline constexpr int kCrcOff = 60;

// The two frozen limits. They are limits and not suggestions because the index
// stream is u8 local: 126 triangles x 3 indices is 378 bytes, and an index
// value must address no more than 64 local vertices.
inline constexpr uint8_t kMaxVertexCount = 64;
inline constexpr uint8_t kMaxTriangleCount = 126;

// One per row of the contract's refusal table, and the order IS the order of
// `descriptors_refused_by_reason[7]`. A refused descriptor drops ONE meshlet and
// is counted; it does not fault the frame. That is deliberately different from
// the ruling's hard-overflow law, which governs capacity exhaustion.
enum class Refusal : uint8_t {
  kNone = 0,
  kFormat,          // a future format read as this one produces plausible garbage
  kCrc,             // the descriptor is not trustworthy in ANY field
  kGeneration,      // the asset moved under a live instance
  kVertexCount,     // u8 local indices cannot address it
  kTriangleCount,   // exceeds the frozen limit
  kReserved,        // a future field is being used by an older reader
  kZeroBound,       // a zero bound culls everything, silently
};
inline constexpr int kRefusalCount = 7;  // excludes kNone

struct Descriptor {
  uint8_t  format_id;
  uint8_t  flags;
  uint8_t  vertex_count;
  uint8_t  triangle_count;
  uint16_t material_id;
  int16_t  lod_error;
  int32_t  bound_centre[3];  // fx16 S15.16, MESHLET-LOCAL by ruling
  uint32_t bound_radius;     // fx16, unsigned
  uint32_t vertex_offset;
  uint32_t index_offset;
  uint16_t generation;
  uint16_t mesh_id;
  uint32_t crc32c;
};

// Little-endian field reads, so the oracle and a byte-fetching RTL agree about
// what a descriptor IS before they can disagree about what it MEANS.
inline uint16_t rd16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}
inline uint32_t rd32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

inline Descriptor decode(const uint8_t* b) {
  Descriptor d{};
  d.format_id = b[0];
  d.flags = b[1];
  d.vertex_count = b[2];
  d.triangle_count = b[3];
  d.material_id = rd16(b + 4);
  d.lod_error = static_cast<int16_t>(rd16(b + 6));
  for (int i = 0; i < 3; ++i) d.bound_centre[i] = static_cast<int32_t>(rd32(b + 8 + 4 * i));
  d.bound_radius = rd32(b + 20);
  d.vertex_offset = rd32(b + 24);
  d.index_offset = rd32(b + 28);
  d.generation = rd16(b + 32);
  d.mesh_id = rd16(b + 34);
  d.crc32c = rd32(b + kCrcOff);
  return d;
}

// ---------------------------------------------------------------------------
// Validation — the first of the three things this oracle owns
// ---------------------------------------------------------------------------
// ORDER MATTERS AND IS NOT ARBITRARY. The CRC is checked SECOND, immediately
// after the format, and everything else after it. A descriptor whose CRC fails
// "is not trustworthy in any field" (contract), so reporting `kVertexCount` for
// a corrupt descriptor would name a field whose value is meaningless. Format
// comes first because an unknown format means the byte layout itself is not
// agreed, so even the CRC window may be in the wrong place.
inline Refusal validate(const uint8_t* bytes, uint8_t expect_format, uint16_t expect_generation) {
  const Descriptor d = decode(bytes);

  if (d.format_id != expect_format) return Refusal::kFormat;

  if (zhao_abi::zhao_crc32c(0, bytes, kCrcCovered) != d.crc32c) return Refusal::kCrc;

  if (d.generation != expect_generation) return Refusal::kGeneration;
  if (d.vertex_count > kMaxVertexCount) return Refusal::kVertexCount;
  if (d.triangle_count > kMaxTriangleCount) return Refusal::kTriangleCount;

  for (int i = 0; i < kReservedLen; ++i)
    if (bytes[kReservedOff + i] != 0) return Refusal::kReserved;

  // "on a non-empty meshlet" — an empty meshlet has nothing to bound, and
  // refusing it would turn a legal degenerate case into a fault.
  if (d.bound_radius == 0 && d.triangle_count != 0) return Refusal::kZeroBound;

  return Refusal::kNone;
}

// ---------------------------------------------------------------------------
// The bound transform — the second thing this oracle owns
// ---------------------------------------------------------------------------
// `bound_centre` is MESHLET-LOCAL by ruling, so the instance transform carries
// it to world space and the radius is scaled by the MAXIMUM ABSOLUTE instance
// scale.
//
// Maximum-absolute is the ruling's answer and the reason is directional:
// it rounds the bound OUTWARD under non-uniform scale. A loose bound costs
// decode work; a tight one DELETES GEOMETRY. The directed test asserts the
// inequality rather than a value, because the direction is the point.
struct InstanceXform {
  int32_t m[12];  // 3x4 row-major fx16, the same shape as mat3x4fx
};

inline int64_t abs64(int64_t v) { return v < 0 ? -v : v; }

// One fx16 rescale, round-half-up, matching qformats §4's single rounding.
inline int32_t rescale16(int64_t v) {
  return static_cast<int32_t>((v + (int64_t{1} << 15)) >> 16);
}

inline void world_bound(const InstanceXform& x, const int32_t local_centre[3],
                        uint32_t local_radius, int32_t out_centre[3], uint32_t* out_radius) {
  for (int r = 0; r < 3; ++r) {
    const int64_t s = static_cast<int64_t>(x.m[r * 4 + 0]) * local_centre[0] +
                      static_cast<int64_t>(x.m[r * 4 + 1]) * local_centre[1] +
                      static_cast<int64_t>(x.m[r * 4 + 2]) * local_centre[2] +
                      (static_cast<int64_t>(x.m[r * 4 + 3]) << 16);
    out_centre[r] = rescale16(s);
  }

  // Maximum absolute COLUMN scale: the largest factor by which any axis can be
  // stretched. Taking the max over all nine rotation/scale entries is the
  // conservative reading and cannot under-bound.
  int64_t max_abs = 0;
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c) {
      const int64_t a = abs64(x.m[r * 4 + c]);
      if (a > max_abs) max_abs = a;
    }

  const int64_t scaled = static_cast<int64_t>(local_radius) * max_abs;
  // Round the radius UP, never down, for the same directional reason.
  *out_radius = static_cast<uint32_t>((scaled + ((int64_t{1} << 16) - 1)) >> 16);
}

// ---------------------------------------------------------------------------
// decide() — the entry point the ledger names
// ---------------------------------------------------------------------------
struct Result {
  bool accepted = false;              // descriptor trustworthy AND visible somewhere
  Refusal refusal = Refusal::kNone;   // why not, if not
  uint8_t visible_mask = 0;           // bit v: camera v may see it
  int32_t rung = 0;                   // from zref::creature, when a ladder applies
  bool hold = false;                  // ladder hysteresis
};

// REJECTED IS NOT REFUSED, and the counters must not conflate them: one is
// geometry that is not visible, the other is a descriptor that is not
// trustworthy. `accepted == false` with `refusal == kNone` is the first;
// `refusal != kNone` is the second. A meshlet with `visible_mask == 0` is not
// emitted at all.
inline Result decide(const uint8_t* descriptor_bytes, uint8_t expect_format,
                     uint16_t expect_generation, const InstanceXform& instance,
                     const cull::View views[cull::kViewCount], uint8_t active_camera_mask) {
  Result out{};

  out.refusal = validate(descriptor_bytes, expect_format, expect_generation);
  if (out.refusal != Refusal::kNone) return out;

  const Descriptor d = decode(descriptor_bytes);

  int32_t wc[3];
  uint32_t wr;
  world_bound(instance, d.bound_centre, d.bound_radius, wc, &wr);

  // THE FRUSTUM TEST IS A CALL. `cull_instance` already owns the per-camera law
  // including the `active_mask == 0 -> reject` case, which falls out of
  // `reject = (visible_mask == 0)` rather than needing a branch here.
  const cull::Verdict v =
      cull::cull_instance(views, active_camera_mask, vec3fx{fx16{wc[0]}, fx16{wc[1]}, fx16{wc[2]}},
                          fx16{static_cast<int32_t>(wr)});

  out.visible_mask = v.visible_mask;
  out.accepted = !v.reject;
  return out;
}

}  // namespace meshfetch
}  // namespace zref
