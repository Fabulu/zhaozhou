// zref_assetfetch.hpp — GEOM.ASSETFETCH's scalar reference.
//
// ---------------------------------------------------------------------------
// WHAT THIS ORACLE OWNS, AND WHAT IT DELIBERATELY DOES NOT
// ---------------------------------------------------------------------------
// `design/contracts/GEOM.ASSETFETCH.md`. The block reads a meshlet's whole
// asset footprint through MEM.GUARD's Phase-3 pool and serves two consumers
// from it: GEOM.ASSEMBLE's `u8` index triplets and GEOM.VDECODE's 32-byte
// vertex records.
//
// It owns exactly three things:
//   * the ADDRESS ARITHMETIC -- offset to absolute, record to byte range;
//   * the FOOTPRINT ADMISSION test, including the pool-end check;
//   * the REFUSAL TAXONOMY, and the rule that a refused meshlet yields NOTHING.
//
// It does NOT own, and must never grow:
//   * vertex decoding      -- `zref::geom` decodes a 32-byte format-0 record;
//   * index validation     -- GEOM.ASSEMBLE refuses out-of-range triplets and
//                             counts them (`refused_index_o`);
//   * the region bounds    -- MEM.GUARD enforces them and a formal proof says
//                             nothing escapes. This file's pool-end check is a
//                             REFUSAL so a bad meshlet is reported at its
//                             source, NOT a second bounds enforcement. If the
//                             two ever disagree the guard wins; it is the one
//                             with the proof.
//
// That last distinction is the one worth stating. On 2026-09-03 a terrain shade
// oracle was written as a second implementation of a ratified law and twelve
// checks passed comparing the duplicate to itself. A bounds check here that
// believed itself authoritative would be the same mistake with worse
// consequences, because the thing it would be shadowing is a safety property.
#pragma once

#include <cstdint>
#include <cstring>

namespace zref {
namespace assetfetch {

// ---------------------------------------------------------------------------
// Sizes. Every one of these is a KNOB, and each mirrors a frozen ruling rather
// than being chosen here -- if a ruling moves, this is the file that moves with
// it, in one place, visibly.
// ---------------------------------------------------------------------------
inline constexpr int kIndexBytesPerTriangle = 3;  // u8 a, b, c
inline constexpr int kVertexRecordBytes = 32;     // GEOM.VDECODE format 0
inline constexpr int kLineBytes = 64;             // one aligned burst

// GEOM.MESHFETCH.md ruling limits. u8 local indices are WHY these exist.
inline constexpr int kMaxVertices = 64;
inline constexpr int kMaxTriangles = 126;

// ---------------------------------------------------------------------------
// ALIGNMENT, and it is an ENFORCEMENT of an existing ruling rather than a new
// one. GEOM.VDECODE's contract already says vertex records are "32 bytes per
// vertex, NATURALLY ALIGNED"; requiring `vertex_offset % 32 == 0` is that
// sentence, checked. `index_offset % 8 == 0` is the same idea one size down.
//
// It was found by laying out the RTL's buffers, and it is worth the note:
// an unaligned 32-byte record spans five 64-bit words, so serving it needs a
// 320-to-256 funnel shifter PER VERTEX. Aligned, a record is exactly four
// consecutive words and the shifter disappears. A triplet still straddles --
// 3 bytes at byte 3n cannot be helped -- so that one reads two words and
// selects, which is cheap.
//
// The asset builder pays nothing for this; padding a stream to 32 bytes is
// free at authoring time and the silicon is not.
inline constexpr uint32_t kVertexAlign = 32;
inline constexpr uint32_t kIndexAlign = 8;

// spec/memory_rules.md 5f. Named, not inlined: the pool is a knob and a moved
// pool must not require finding a hex literal in an oracle.
inline constexpr uint32_t kAssetPoolBase = 0x06A00000u;
inline constexpr uint32_t kAssetPoolSpan = 0x01600000u;  // 22 MiB

// The worst-case footprint, which is what the block's buffer must hold. Derived
// rather than written down, so it cannot drift from the limits above.
inline constexpr int kMaxIndexBytes = kMaxTriangles * kIndexBytesPerTriangle;  // 378
inline constexpr int kMaxVertexBytes = kMaxVertices * kVertexRecordBytes;      // 2048
inline constexpr int kMaxFootprint = kMaxIndexBytes + kMaxVertexBytes;         // 2426

// ---------------------------------------------------------------------------
// The refusal taxonomy. Ordered, and the ORDER IS PART OF THE LAW: a meshlet
// that is both over-count and out-of-pool reports the over-count, because that
// is the fault the author can act on. An arbitrary order here would make the
// counters unstable under unrelated edits.
// ---------------------------------------------------------------------------
enum class Refusal : uint8_t {
  kNone = 0,
  kVertexCount,    // vertex_count   > kMaxVertices
  kTriangleCount,  // triangle_count > kMaxTriangles
  kOutsidePool,    // the footprint leaves the pool -- refused BEFORE any beat
  kMisaligned,     // an offset violates kVertexAlign / kIndexAlign
};

struct Request {
  uint32_t vertex_offset;  // byte offset into the pool
  uint32_t index_offset;   // byte offset into the pool
  uint8_t vertex_count;
  uint8_t triangle_count;
};

struct Plan {
  Refusal refusal = Refusal::kNone;
  bool admitted = false;
  uint32_t index_addr = 0;   // ABSOLUTE
  uint32_t vertex_addr = 0;  // ABSOLUTE
  uint32_t index_bytes = 0;
  uint32_t vertex_bytes = 0;
  uint32_t beats = 0;  // 64-byte lines the guard will be asked for
};

// A 64-bit-safe end address. The block computes in 32 bits and the guard's own
// wrap defect (the blit clamp) is the standing reminder that `base + span` is
// where address arithmetic goes wrong, so the comparison is done WIDE here and
// the RTL is required to agree with a wide answer, not with a wrapped one.
inline uint64_t end_of(uint32_t base, uint32_t bytes) {
  return static_cast<uint64_t>(base) + static_cast<uint64_t>(bytes);
}

// How many aligned 64-byte lines cover [addr, addr+bytes)? The first and last
// lines are usually partial; a record stream is NOT line-aligned in general.
inline uint32_t lines_covering(uint32_t addr, uint32_t bytes) {
  if (bytes == 0) return 0;
  const uint64_t first = static_cast<uint64_t>(addr) / kLineBytes;
  const uint64_t last = (end_of(addr, bytes) - 1) / kLineBytes;
  return static_cast<uint32_t>(last - first + 1);
}

// ---------------------------------------------------------------------------
// Admission. This is the whole decision the block makes before it reads a byte.
// ---------------------------------------------------------------------------
inline Plan plan(const Request& r) {
  Plan p;

  if (r.vertex_count > kMaxVertices) {
    p.refusal = Refusal::kVertexCount;
    return p;
  }
  if (r.triangle_count > kMaxTriangles) {
    p.refusal = Refusal::kTriangleCount;
    return p;
  }

  // Alignment before arithmetic: a misaligned offset is a malformed asset, and
  // saying so is more useful than reporting where its unaligned footprint fell.
  if ((r.vertex_offset % kVertexAlign) != 0 || (r.index_offset % kIndexAlign) != 0) {
    p.refusal = Refusal::kMisaligned;
    return p;
  }

  p.index_bytes = static_cast<uint32_t>(r.triangle_count) * kIndexBytesPerTriangle;
  p.vertex_bytes = static_cast<uint32_t>(r.vertex_count) * kVertexRecordBytes;

  // Offsets are pool-relative; the absolute address is formed HERE and nowhere
  // upstream, so moving the pool is a one-constant edit (contract, "the law").
  p.index_addr = kAssetPoolBase + r.index_offset;
  p.vertex_addr = kAssetPoolBase + r.vertex_offset;

  const uint64_t pool_end = end_of(kAssetPoolBase, kAssetPoolSpan);

  // Wide comparisons on purpose, and the START is checked as well as the end:
  // an offset large enough to wrap 32 bits lands BELOW the base, which an
  // end-only test would admit. That is the same shape as the guard's blit-wrap
  // defect, which is why it is tested rather than reasoned about.
  const bool idx_ok =
      (p.index_addr >= kAssetPoolBase) && (end_of(p.index_addr, p.index_bytes) <= pool_end);
  const bool vtx_ok =
      (p.vertex_addr >= kAssetPoolBase) && (end_of(p.vertex_addr, p.vertex_bytes) <= pool_end);

  if (!idx_ok || !vtx_ok) {
    p.refusal = Refusal::kOutsidePool;
    return p;
  }

  p.beats =
      lines_covering(p.index_addr, p.index_bytes) + lines_covering(p.vertex_addr, p.vertex_bytes);
  p.admitted = true;
  return p;
}

// ---------------------------------------------------------------------------
// Serving. `pool_abs` is the pool's contents indexed by ABSOLUTE address, which
// is how the differential test presents it -- the RTL sees the same bytes
// through the guard, so an addressing error shows up as a byte mismatch rather
// than as a plausible-looking wrong triangle.
// ---------------------------------------------------------------------------
struct Triplet {
  uint8_t a, b, c;
};

// Triplet n of an ADMITTED meshlet. Out-of-range n is a caller error, not a
// refusal: GEOM.ASSEMBLE never asks past triangle_count, and inventing a
// refusal for it here would be a law this block does not own.
inline Triplet triplet(const Plan& p, const uint8_t* pool_abs, uint32_t n) {
  const uint32_t at = p.index_addr + n * kIndexBytesPerTriangle;
  return Triplet{pool_abs[at], pool_abs[at + 1], pool_abs[at + 2]};
}

// Vertex record v, 32 bytes, copied out exactly as they lie. This block never
// interprets them -- `zref::geom` does.
inline void vertex_record(const Plan& p, const uint8_t* pool_abs, uint32_t v,
                          uint8_t out[kVertexRecordBytes]) {
  std::memcpy(out, pool_abs + p.vertex_addr + v * kVertexRecordBytes, kVertexRecordBytes);
}

}  // namespace assetfetch
}  // namespace zref
