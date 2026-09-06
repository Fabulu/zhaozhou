// zref_terrain_page.hpp - the terrain PAGE as a transfer: its shape, its CRC,
// and the verdict on one HPS-DDR -> local-SDRAM page load.
//
// ---------------------------------------------------------------------------
// WHAT IS NEW HERE AND WHAT IS DELIBERATELY BORROWED
// ---------------------------------------------------------------------------
// Almost nothing here is new, and that is the point. `zref_mem_upload.hpp`
// already owns the acceptance law for an HPS->VRAM transfer -- alignment,
// 64-bit containment of BOTH source and destination, epoch staleness, and the
// ORDER the tests run in -- and says in as many words that "a refusal is not a
// clamp". `zhao_abi::zhao_crc32c` is the one CRC-32C in the tree.
//
// So this header adds exactly three things that are terrain's and nobody
// else's:
//
//   1. the page's SHAPE -- 21,376 B stride, 21,320 B body, 64 B header,
//      334 bursts (spec/terrain_rules.md sec 2 table and sec 7);
//   2. the page's CRC RANGE -- bytes [64, 21320), which is neither the whole
//      page nor the whole body (spec/terrain_rules.md:126);
//   3. the page HEADER's own fields, which restate the key the loader was
//      given and are therefore a corruption check (spec/terrain_rules.md
//      sec 2.1: "redundancy is a corruption check").
//
// The verdict enum EXTENDS `zref::mem::UploadVerdict` rather than restating it,
// and a static_assert below pins every shared value. Two enumerations that
// agree until they do not is the defect class this tree keeps recording.
//
// ---------------------------------------------------------------------------
// WHY A REFUSAL MUST STILL REPORT
// ---------------------------------------------------------------------------
// Ruling T10: "loader completion carries success/failure and CRC identity."
// A loader that simply stays silent on a bad page leaves the residency slot in
// LOADING forever -- the page never becomes resident, which is safe, and the
// slot never becomes reusable, which is not. So `page_load` ALWAYS produces a
// result; `ok` is what the directory reads, and `verdict` is what a human
// reads.
//
// Ruling T7: "a half-loaded or CRC-failed page is never rendered." That is a
// property of `ok`, not of the bytes: the bytes of a faulted page are garbage
// sitting in a slot nothing may look at.

#ifndef ZREF_TERRAIN_PAGE_HPP
#define ZREF_TERRAIN_PAGE_HPP

#include <cstdint>

#include "zhao_abi.h"  // generated (runtime/include): zhao_crc32c
#include "zref/zref_mem_upload.hpp"

namespace zref {
namespace terrain {

// ---------------------------------------------------------------- the shape --
// spec/terrain_rules.md sec 2: the layer table totals 21,320 B of body under a
// 64 B header, in a page whose STRIDE is 21,376 B -- "334 x 64-B bursts, 56 B
// pad". Every one of these is a law with a citation, not a tuning knob.
inline constexpr uint32_t kPageBytes = 21376;     // stride, sec 2 / sec 7
inline constexpr uint32_t kPageBodyEnd = 21320;   // end of layer H
inline constexpr uint32_t kPageHeaderBytes = 64;  // sec 2.1
inline constexpr uint32_t kPageCrcLo = 64;        // sec 2.1: over [64, 21320)
inline constexpr uint32_t kPageCrcHi = 21320;
inline constexpr uint32_t kPageBurstBytes = mem::kUploadBurstBytes;    // 64
inline constexpr uint32_t kPageBursts = kPageBytes / kPageBurstBytes;  // 334

// The CRC range is BEAT-ALIGNED on the bridge's 64-bit beats, and that is not a
// coincidence worth leaving unstated: 64 / 8 = 8 and 21320 / 8 = 2665, so every
// one of the 2,672 beats of a page is wholly inside the CRC range or wholly
// outside it. Hardware therefore never folds a PARTIAL beat, which removes the
// entire class of "the tail byte count was off by one" defect.
inline constexpr uint32_t kPageBeats = kPageBytes / 8;      // 2,672
inline constexpr uint32_t kPageCrcBeatLo = kPageCrcLo / 8;  // 8
inline constexpr uint32_t kPageCrcBeatHi = kPageCrcHi / 8;  // 2,665
static_assert(kPageCrcLo % 8 == 0 && kPageCrcHi % 8 == 0, "CRC range must be beat aligned");
static_assert(kPageBytes % kPageBurstBytes == 0, "page must be a whole number of bursts");
static_assert(kPageBursts == 334, "spec/terrain_rules.md sec 7 says 334 bursts");

// TERRAIN.PAGE_POOL, ruling T2 / spec/memory_rules.md sec 5b:
//   0x0400_0000 .. 0x054D_FFFF   1,024 x 21,376 B
inline constexpr uint32_t kPagePoolBase = 0x04000000u;
inline constexpr uint32_t kPagePoolSlots = 1024;
static_assert(kPagePoolBase + kPagePoolSlots * kPageBytes == 0x054E0000u,
              "the pool must end exactly where T2's next region begins");

// --------------------------------------------------------------- the header --
// spec/terrain_rules.md sec 2.1, all little-endian.
struct PageHeader {
  uint16_t format_version = 0;  // +0,  must be 1
  int8_t pitch_log2 = 0;        // +2,  -1..+2, must match the island table
  uint8_t flags = 0;            // +3
  uint32_t island_id = 0;       // +4
  int16_t patch_ix = 0;         // +8
  int16_t patch_iz = 0;         // +10
  uint32_t tileset_id = 0;      // +12
  uint32_t page_crc32c = 0;     // +32, over [64, 21320)
};

inline uint16_t page_rd16(const uint8_t* p) {
  return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8));
}

inline uint32_t page_rd32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

inline PageHeader parse_page_header(const uint8_t* page) {
  PageHeader h;
  h.format_version = page_rd16(page + 0);
  h.pitch_log2 = static_cast<int8_t>(page[2]);
  h.flags = page[3];
  h.island_id = page_rd32(page + 4);
  h.patch_ix = static_cast<int16_t>(page_rd16(page + 8));
  h.patch_iz = static_cast<int16_t>(page_rd16(page + 10));
  h.tileset_id = page_rd32(page + 12);
  h.page_crc32c = page_rd32(page + 32);
  return h;
}

// THE ONE CRC. Delegates to `zhao_abi::zhao_crc32c` -- the generated table used
// by CMD.DMA, DEBUG.FRAMEBLIT and GEOM.MESHFETCH -- so the terrain page cannot
// end up with a private polynomial.
inline uint32_t page_payload_crc(const uint8_t* page) {
  return zhao_abi::zhao_crc32c(0, page + kPageCrcLo, kPageCrcHi - kPageCrcLo);
}

// Where slot `n` of the pool lives. A FUNCTION rather than a stride left to
// each call site, for the reason `upload_src_of` gives: a stride computed twice
// is a stride that can disagree with itself.
inline uint32_t page_vram_addr(uint32_t slot, uint32_t pool_base = kPagePoolBase) {
  return pool_base + slot * kPageBytes;
}

// -------------------------------------------------------------- the verdict --
// 0..7 ARE `zref::mem::UploadVerdict`, value for value. 8 and 9 are the two
// outcomes that only exist once bytes are actually moving, and they sit above
// the borrowed range so a widening of the shared enum collides loudly.
enum PageLoadVerdict : int {
  kPageOk = mem::kUploadOk,                                  // 0
  kPageUnaligned = mem::kUploadUnaligned,                    // 1
  kPageZeroLength = mem::kUploadZeroLength,                  // 2
  kPageOutsidePool = mem::kUploadOutsideGuard,               // 3
  kPageEpochStale = mem::kUploadEpochStale,                  // 4
  kPageCrcFail = mem::kUploadCrcFail,                        // 5
  kPageSourceOutsideArena = mem::kUploadSourceOutsideArena,  // 6
  kPageSourceUnreachable = mem::kUploadSourceUnreachable,    // 7
  // The header restates the key the job was issued against. A mismatch means
  // the staged bytes are SOMEBODY ELSE'S PAGE -- which a CRC cannot catch,
  // because another island's page has a perfectly valid CRC of its own.
  kPageHeaderIdent = 8,
  // The transfer stopped: a bridge `err`, or MEM.GUARD refusing a write. The
  // slot holds a prefix of a page. Counted, never published.
  kPageIncomplete = 9,
};

static_assert(static_cast<int>(kPageCrcFail) == 5, "CRC fail keeps mem_upload's value");
static_assert(static_cast<int>(kPageHeaderIdent) > static_cast<int>(kPageSourceUnreachable),
              "the terrain-only verdicts must sit above the borrowed range");

struct PageLoadRequest {
  uint32_t slot = 0;
  uint8_t generation = 0;
  uint32_t epoch = 0;
  uint32_t island_id = 0;
  int16_t patch_ix = 0;
  int16_t patch_iz = 0;
  uint64_t hps_addr = 0;
  uint32_t expect_crc = 0;  // SW.STREAM's `expected_page_crc32c` (ruling T5)
  uint32_t src_id = 0;
};

struct PageLoadResult {
  int verdict = kPageOk;
  bool ok = false;             // what TERRAIN.RESIDENCY reads as fin_ok
  uint32_t crc_seen = 0;       // what it reads as fin_crc, ALWAYS reported
  uint32_t bytes_written = 0;  // bytes that actually reached the pool
};

// Field for field the counter set the RTL exports, so a test compares ledgers
// rather than spot-checking non-zero.
struct PageLoadLedger {
  uint32_t pages_loaded = 0;
  uint32_t pages_faulted = 0;  // bytes moved, then refused
  uint32_t pages_refused = 0;  // refused before a single byte moved
  uint32_t crc_fails = 0;
  uint32_t hdr_ident_fails = 0;
  uint32_t incomplete = 0;
  uint32_t guard_denied = 0;
  uint32_t bridge_errs = 0;
  uint32_t load_bytes = 0;
};

// THE PRE-TRANSFER VERDICT, delegating rather than re-deriving. The pool is
// expressed as a `mem::GuardRegion` so `upload_verdict` does the containment in
// 64 bits -- the wrap it exists to refuse is the same wrap here.
inline int page_pre_verdict(const PageLoadRequest& r, const mem::GuardRegion& hps_arena,
                            uint32_t pool_base, uint32_t pool_slots, uint32_t current_epoch) {
  // A slot outside the pool is refused BEFORE the address is formed, so a wild
  // slot index can never become a wild address. `upload_verdict` would also
  // catch it, but only after computing the address it must not compute.
  if (r.slot >= pool_slots) return kPageOutsidePool;
  const mem::GuardRegion pool{pool_base, pool_slots * kPageBytes};
  // THE EPOCH IS TESTED HERE, AT FULL WIDTH, IN upload_verdict's OWN POSITION.
  // `upload_verdict` takes uint16 epochs; ruling T1's canonical key carries
  // `resource_epoch:u32`. Truncating to 16 bits would make two epochs that
  // differ only above bit 15 compare EQUAL in the oracle and unequal in the
  // hardware -- a divergence in the model, not in the design. So the delegated
  // call is handed matching epochs (its epoch arm becomes a no-op) and the real
  // test is made below it, last, exactly where the borrowed order puts it.
  const int v = mem::upload_verdict(pool, hps_arena, r.hps_addr,
                                    page_vram_addr(r.slot, pool_base), kPageBytes, 0, 0);
  if (v != mem::kUploadOk) return v;
  if (r.epoch != current_epoch) return kPageEpochStale;
  return kPageOk;
}

// The whole load. `page` is the 21,376 staged bytes, or nullptr when the
// caller is only asking about a refusal that happens before any read.
//
// `transfer_complete` stands in for the bridge and the guard: false means the
// transfer stopped part way, which is the one thing a scalar model cannot
// derive and the hardware absolutely can.
inline PageLoadResult page_load(const PageLoadRequest& r, const uint8_t* page,
                                const mem::GuardRegion& hps_arena, uint32_t pool_base,
                                uint32_t pool_slots, uint32_t current_epoch,
                                bool transfer_complete, bool check_header_ident,
                                bool check_header_crc, PageLoadLedger* L = nullptr) {
  PageLoadResult out;

  const int pre = page_pre_verdict(r, hps_arena, pool_base, pool_slots, current_epoch);
  if (pre != kPageOk) {
    out.verdict = pre;
    out.ok = false;
    out.bytes_written = 0;
    if (L) L->pages_refused++;
    return out;
  }

  if (!transfer_complete) {
    out.verdict = kPageIncomplete;
    out.ok = false;
    if (L) {
      L->pages_faulted++;
      L->incomplete++;
    }
    return out;
  }

  out.bytes_written = kPageBytes;
  if (L) L->load_bytes += kPageBytes;
  out.crc_seen = page_payload_crc(page);

  // ORDER: identity before integrity. A page belonging to another patch is a
  // WRONGNESS; a page belonging to this patch with a bad CRC is a corruption.
  // Reporting the corruption first would send the reader to the disk when the
  // staging pointer is what is wrong.
  const PageHeader h = parse_page_header(page);
  if (check_header_ident && (h.format_version != 1 || h.island_id != r.island_id ||
                             h.patch_ix != r.patch_ix || h.patch_iz != r.patch_iz)) {
    out.verdict = kPageHeaderIdent;
    out.ok = false;
    if (L) {
      L->pages_faulted++;
      L->hdr_ident_fails++;
    }
    return out;
  }

  // TWO DECLARED HOLDERS OF ONE NUMBER, and both are checked. The job carries
  // SW.STREAM's `expected_page_crc32c` (T5) and the page carries its own
  // `page_crc32c` (sec 2.1) over the identical range. Nothing in the tree says
  // which one governs, so neither is allowed to govern alone: a disagreement
  // between them is itself a corruption and is refused.
  const bool job_bad = (out.crc_seen != r.expect_crc);
  const bool hdr_bad = check_header_crc && (out.crc_seen != h.page_crc32c);
  if (job_bad || hdr_bad) {
    out.verdict = kPageCrcFail;
    out.ok = false;
    if (L) {
      L->pages_faulted++;
      L->crc_fails++;
    }
    return out;
  }

  out.verdict = kPageOk;
  out.ok = true;
  if (L) L->pages_loaded++;
  return out;
}

}  // namespace terrain
}  // namespace zref

#endif  // ZREF_TERRAIN_PAGE_HPP
