// zref_mem_upload.hpp — the acceptance law for an HPS→VRAM resource upload.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS BEFORE THE RTL
// ---------------------------------------------------------------------------
// `design/blocks.yml`'s schema requires every block to cite a `zref::` symbol,
// so registering MEM.UPLOAD with no oracle would have manufactured exactly the
// kind of phantom citation `reports/PHANTOM-CITATIONS-AUDIT.md` exists to
// count. The right answer to a phantom is to write the symbol, so here it is,
// before the RTL rather than after.
//
// ---------------------------------------------------------------------------
// WHAT IS OWNED HERE, AND WHAT IS NOT
// ---------------------------------------------------------------------------
// This owns the parts that can be SILENTLY wrong: whether a request may be
// executed at all, and how a {address, length} pair decomposes into bursts.
// Both are the kind of thing that looks right in every ordinary case and is
// wrong at a boundary.
//
// It does NOT own bandwidth, arbitration, residency policy or eviction. Those
// are the composed block's and the HPS's, and none of them has a scalar law
// that could be checked against a number.
//
// ---------------------------------------------------------------------------
// THE ONE THAT MATTERS: A REFUSAL IS NOT A CLAMP
// ---------------------------------------------------------------------------
// Every predicate below returns a VERDICT, never a corrected value. An upload
// with a destination one byte outside the guard map must be refused, not
// nudged into range — a clamped address writes real bytes into a real slot
// belonging to something else, and nothing downstream can tell.
#pragma once

#include <cstdint>

namespace zref {
namespace mem {

// Bursts are 64 bytes because that is the shape the console already moves HPS
// bytes in (DEBUG.FRAMEBLIT). One transfer granularity, not two.
constexpr uint32_t kUploadBurstBytes = 64;

enum UploadVerdict : int {
  kUploadOk = 0,
  kUploadUnaligned = 1,     // address or length not a multiple of the burst
  kUploadZeroLength = 2,    // not a transfer
  kUploadOutsideGuard = 3,  // destination not inside the declared region
  kUploadEpochStale = 4,    // the epoch this belonged to has closed
  kUploadCrcFail = 5,       // decided after the bytes arrive, not here
  // Added 2026-09-03 from the owner brief: the first version checked the
  // DESTINATION and not the source, which is a capability hole rather than a
  // missing bounds check. The bridge can see a broad HPS range; a descriptor
  // may read only from the staging arena registered for the active epoch.
  kUploadSourceOutsideArena = 6,
  // The bridge exports a 32-bit HPS address and the MiSTer core-side interface
  // a 29-bit 64-bit-word address. An hps_addr with a non-zero upper half is
  // REFUSED; it is never truncated.
  kUploadSourceUnreachable = 7
};

// A destination region, as MEM.GUARD declares it.
struct GuardRegion {
  uint32_t base;
  uint32_t bytes;
};

inline bool upload_aligned(uint64_t hps_addr, uint32_t vram_addr, uint32_t length) {
  return (hps_addr % kUploadBurstBytes) == 0 && (vram_addr % kUploadBurstBytes) == 0 &&
         (length % kUploadBurstBytes) == 0;
}

// Containment is checked in 64 bits deliberately. `vram_addr + length` in 32
// bits can WRAP, and a wrapped sum compares as a small number — so a request
// that runs off the end of the arena would read as comfortably inside it.
// That is the specific arithmetic accident this function exists to refuse.
inline bool upload_in_guard(const GuardRegion& r, uint32_t vram_addr, uint32_t length) {
  const uint64_t lo = vram_addr;
  const uint64_t hi = lo + static_cast<uint64_t>(length);
  const uint64_t rlo = r.base;
  const uint64_t rhi = rlo + static_cast<uint64_t>(r.bytes);
  return lo >= rlo && hi <= rhi;
}

// The HPS staging arena the active epoch registered. Same 64-bit containment
// arithmetic and the same reason: a 32-bit sum can wrap and read as small.
inline bool upload_source_in_arena(const GuardRegion& arena, uint64_t hps_addr, uint32_t length) {
  const uint64_t lo = hps_addr;
  const uint64_t hi = lo + static_cast<uint64_t>(length);
  const uint64_t alo = arena.base;
  const uint64_t ahi = alo + static_cast<uint64_t>(arena.bytes);
  return lo >= alo && hi <= ahi;
}

// The order of these tests is part of the law: a malformed request is reported
// as malformed even when it is ALSO stale, so a producer bug is never hidden
// behind an epoch that happened to close.
inline UploadVerdict upload_verdict(const GuardRegion& region, const GuardRegion& hps_arena,
                                    uint64_t hps_addr, uint32_t vram_addr, uint32_t length,
                                    uint16_t request_epoch, uint16_t current_epoch) {
  if (length == 0) return kUploadZeroLength;
  if (!upload_aligned(hps_addr, vram_addr, length)) return kUploadUnaligned;
  // The upper half must be zero BEFORE anything else looks at the address, so
  // a 64-bit descriptor can never be quietly narrowed into a legal-looking
  // 32-bit one.
  if ((hps_addr >> 32) != 0) return kUploadSourceUnreachable;
  if (!upload_source_in_arena(hps_arena, hps_addr, length)) return kUploadSourceOutsideArena;
  if (!upload_in_guard(region, vram_addr, length)) return kUploadOutsideGuard;
  if (request_epoch != current_epoch) return kUploadEpochStale;
  return kUploadOk;
}

// How many bursts a legal request becomes. Exact, not rounded up: an unaligned
// length is refused above rather than padded, because padding would write
// bytes the producer never staged.
inline uint32_t upload_bursts(uint32_t length) { return length / kUploadBurstBytes; }

// The address of burst `n`. Kept as a function rather than left to the caller
// because a stride computed at each call site is a stride that can disagree
// with itself between the reader and the writer.
inline uint64_t upload_src_of(uint64_t hps_addr, uint32_t n) {
  return hps_addr + static_cast<uint64_t>(n) * kUploadBurstBytes;
}

inline uint32_t upload_dst_of(uint32_t vram_addr, uint32_t n) {
  return vram_addr + n * kUploadBurstBytes;
}

// THE ATOMICITY LAW, as a predicate a test can hold the RTL to.
//
// CORRECTED 2026-09-03: a generation bit alone does NOT make an in-place
// upload atomic. If new bytes are written over the old slot while it still
// advertises the old generation, a consumer holding that generation reads a
// MIXTURE. The generation does not protect the underlying memory.
//
// So the upload lands in a FRESH unpublished slot and publication is a MAPPING
// update. `upload_visible_generation` therefore describes what a consumer sees
// through the mapping, and it is only sound because the destination bytes are
// somewhere the consumer is not looking.
//
// A half-uploaded clip page is bytes that LOOK like animation: the decoder
// would read them, produce a pose, and draw a creature bent into a shape no
// artist authored, with nothing reporting an error anywhere.
inline uint16_t upload_visible_generation(uint16_t old_generation, uint16_t new_generation,
                                          bool complete) {
  return complete ? new_generation : old_generation;
}

}  // namespace mem
}  // namespace zref
