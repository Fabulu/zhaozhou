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
#include <vector>

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

// ===========================================================================
// THE LAYER OFFSET TABLE -- and why it did not exist until now
// ===========================================================================
// spec/terrain_rules.md sec 2 gives each layer an EXTENT and gives no layer an
// OFFSET. Every reader who has needed one has summed the column by hand, and a
// column summed by hand is a column that will one day be summed differently.
// TERRAIN.WRITEBACK needs layer F's offset to the byte, so the sum lives here
// once, with a static_assert on every running total and on the grand total
// against `kPageBodyEnd` -- so a wrong entry cannot quietly agree with itself.
//
// The order is the table's order, which IS the layout order: the table's own
// Total row is 21,320 and it only comes out to 21,320 if the rows are laid end
// to end under the 64-byte header.
inline constexpr uint32_t kLayerABytes = 2178;  // A top base height   33x33 h16
inline constexpr uint32_t kLayerBBytes = 2178;  // B top scar delta    33x33 h16
inline constexpr uint32_t kLayerCBytes = 2178;  // C bottom height     33x33 h16
inline constexpr uint32_t kLayerDBytes = 1024;  // D cell state        32x32 u8
inline constexpr uint32_t kLayerEBytes = 3072;  // E base material     32x32 x3
inline constexpr uint32_t kLayerFBytes = 8192;  // F surface sheet     64x64 x2
inline constexpr uint32_t kLayerGBytes = 256;   // G gameplay grid     8x8   x4
inline constexpr uint32_t kLayerHBytes = 2178;  // H vertex tint       33x33 RGB565

inline constexpr uint32_t kLayerAOff = kPageHeaderBytes;           //     64
inline constexpr uint32_t kLayerBOff = kLayerAOff + kLayerABytes;  //  2,242
inline constexpr uint32_t kLayerCOff = kLayerBOff + kLayerBBytes;  //  4,420
inline constexpr uint32_t kLayerDOff = kLayerCOff + kLayerCBytes;  //  6,598
inline constexpr uint32_t kLayerEOff = kLayerDOff + kLayerDBytes;  //  7,622
inline constexpr uint32_t kLayerFOff = kLayerEOff + kLayerEBytes;  // 10,694
inline constexpr uint32_t kLayerGOff = kLayerFOff + kLayerFBytes;  // 18,886
inline constexpr uint32_t kLayerHOff = kLayerGOff + kLayerGBytes;  // 19,142

static_assert(kLayerFOff == 10694, "layer F starts at page byte 10,694");
static_assert(kLayerFOff + kLayerFBytes == 18886, "...and ends at 18,886");
static_assert(kLayerHOff + kLayerHBytes == kPageBodyEnd,
              "the layer table must sum to the body end the spec's own Total row gives");

// ---------------------------------------------------------------------------
// THE THREE HEIGHT PLANES, AS A LATTICE -- the oracle for TERRAIN.PAGESTREAM
// ---------------------------------------------------------------------------
// A, B and C are each 33x33 height16, row-major, and `TERRAIN.PATCH`'s compose
// lane wants all three at ONE vertex on one beat. So the natural reading of a
// page is not "three planes" but "1,089 triples in scan order", and that is
// what this returns.
//
// EVERY OFFSET IS EVEN AND THAT IS THE WHOLE ALIGNMENT ARGUMENT. 64, 2,242 and
// 4,420 are even and the element is two bytes, so sample k of plane P sits at
// page byte `P + 2k`, also even -- which means a 16-bit read never straddles a
// 64-bit word and never straddles a 64-byte burst (the lane is 0..62 and
// 62 + 2 = 64 exactly). Layer F needed a re-laner; these do not, and the
// difference is arithmetic rather than luck. The static_asserts below are what
// keep it that way when the layout is next revised.
static_assert((kLayerAOff % 2) == 0, "plane A must start on an even byte");
static_assert((kLayerBOff % 2) == 0, "plane B must start on an even byte");
static_assert((kLayerCOff % 2) == 0, "plane C must start on an even byte");

inline constexpr int kLatticeEdge = 33;
inline constexpr int kLatticeVerts = kLatticeEdge * kLatticeEdge;   // 1,089

static_assert(kLatticeVerts * 2 == static_cast<int>(kLayerABytes),
              "the 33x33 lattice must account for every byte of plane A");

/** One vertex of a page's height lattice: the three planes, untouched. */
struct LatticeVertex {
  int16_t base = 0;    // layer A
  int16_t scar = 0;    // layer B
  int16_t bottom = 0;  // layer C
  // vi IS THE COLUMN AND vj IS THE ROW, which is this tree's convention:
  // `zhao_terrain_patch.sv:148` says "vi_i // lattice column" and
  // `zhao_terrain_compcache_front.sv:376` addresses `vidx = vj * LAT_W + vi`,
  // so vi has stride 1 and vj has stride 33 in both consumers.
  //
  // THIS HEADER SAID THE OPPOSITE FOR A DAY, and so did the RTL it is the
  // oracle for. It cost a transposed `subpatch_dirty_o` mask -- the wrong
  // quarter of a patch requested, terrain in the wrong place in the right
  // shape -- and it was the composed bench's PLACEMENT readback that found
  // it, because nothing notices a swapped pair of indices until something
  // downstream interprets them rather than passing them along.
  int vi = 0;          // 0..32, COLUMN, stride 1
  int vj = 0;          // 0..32, ROW,    stride kLatticeEdge

  bool operator==(const LatticeVertex& o) const {
    return base == o.base && scar == o.scar && bottom == o.bottom && vi == o.vi && vj == o.vj;
  }
};

/** height16 at page byte `off`, little-endian, signed. */
inline int16_t page_h16(const uint8_t* page, uint32_t off) {
  return static_cast<int16_t>(static_cast<uint16_t>(page[off]) |
                              (static_cast<uint16_t>(page[off + 1]) << 8));
}

/**
 * `page_lattice` -- the 1,089 vertices of one page, ROW-MAJOR, the COLUMN
 * varying fastest, so vertex k is `{vi = k % 33, vj = k / 33}` and
 * `k == vj * 33 + vi` -- the same expression TERRAIN.COMPCACHE addresses its
 * lattice with.
 *
 * THE ORDER IS THE ADDRESS. TERRAIN.MIPGEN's fine port carries no coordinate
 * at all and derives everything from scan position, so a consumer that trusted
 * a coordinate against a differently-ordered scan would decimate the wrong
 * vertices while every count agreed. `vi`/`vj` are returned because the
 * subpatch mask needs them, not as a second source of truth.
 *
 * IT DOES NOT COMPOSE. terrain_rules SS3.4's
 * `compose_top = max(fx(base) + fx(scar), fx(bottom))` is
 * `zref::terrain::compose_vertex`'s law and TERRAIN.PATCH's block. A second
 * implementation of that saturating clamp is a second thing to keep in step
 * with SS3.4, and its first divergence is ground subtly in the wrong place with
 * every counter agreeing.
 */
inline std::vector<LatticeVertex> page_lattice(const uint8_t* page) {
  std::vector<LatticeVertex> out;
  out.reserve(kLatticeVerts);
  for (int vj = 0; vj < kLatticeEdge; ++vj) {        // ROW, the slow axis
    for (int vi = 0; vi < kLatticeEdge; ++vi) {      // COLUMN, the fast axis
      const uint32_t k = static_cast<uint32_t>(vj * kLatticeEdge + vi);
      LatticeVertex v;
      v.base = page_h16(page, kLayerAOff + 2u * k);
      v.scar = page_h16(page, kLayerBOff + 2u * k);
      v.bottom = page_h16(page, kLayerCOff + 2u * k);
      v.vi = vi;
      v.vj = vj;
      out.push_back(v);
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// THE SHEET IS SIX BYTES OFF A BURST BOUNDARY, AND THAT IS A DATAPATH FACT
// ---------------------------------------------------------------------------
// 10,694 = 64 * 167 + 6. So hardware cannot issue an aligned 64-B read that
// starts at F: it reads the aligned superset from `kSheetChunkStart` and
// realigns by the constant lane `kSheetLane`. These constants live here so the
// RTL's parameters and the test's expectations come from ONE derivation.
inline constexpr uint32_t kSheetChunkStart =
    (kLayerFOff / kPageBurstBytes) * kPageBurstBytes;                 // 10,688
inline constexpr uint32_t kSheetLane = kLayerFOff % 8;                // 6
inline constexpr uint32_t kSheetBeats = kLayerFBytes / 8;             // 1,024
// One extra source beat is needed whenever the lane is non-zero: out[j] spans
// source beats j and j+1. That single beat is what makes the chunk count 129
// against 128 write bursts, and the off-by-one lives exactly there.
inline constexpr uint32_t kSheetSrcBeats = kSheetBeats + (kSheetLane != 0 ? 1u : 0u);  // 1,025
inline constexpr uint32_t kSheetWriteBursts = kLayerFBytes / kPageBurstBytes;          // 128
// ONE MORE CHUNK THAN BURSTS, UNCONDITIONALLY. The pipeline prefetches chunk 0
// and then reads chunk g+1 to write burst g, so it always touches WR_BURSTS + 1
// chunks. At lane 0 the last chunk's bytes would be unused -- one wasted 64-B
// read out of 129 -- and the alternative is a second pipeline shape for a
// layout that does not exist. Stated rather than made conditional, so the RTL
// and this header cannot disagree about a count the suite asserts exactly.
inline constexpr uint32_t kSheetReadChunks = kSheetWriteBursts + 1u;  // 129
static_assert(kSheetChunkStart == 10688 && kSheetLane == 6, "the v1 layout's alignment");
static_assert(kSheetReadChunks == 129 && kSheetWriteBursts == 128,
              "129 chunks in, 128 bursts out -- asserted, not derived at a call site");
// The scheme assumes the sheet begins in the FIRST BEAT of its chunk. True of
// the v1 layout (10,694 % 64 = 6 < 8); the RTL $fatals if an override breaks it.
static_assert((kLayerFOff % kPageBurstBytes) < 8,
              "the sheet must begin in its chunk's first beat");

// The payload oracle: exactly the 8,192 bytes T4 says to copy, and nothing else.
inline void sheet_extract(const uint8_t* page, uint8_t* out) {
  for (uint32_t i = 0; i < kLayerFBytes; ++i) out[i] = page[kLayerFOff + i];
}

// ===========================================================================
// TERRAIN.WRITEBACK -- the F sheet out to the HPS journal, behind an ACK
// ===========================================================================
// Ruling T4. B and D are NEVER written back (the HPS keeps the canonical mirror
// current from the same deterministic commands); layer F has no canonical
// mirror and therefore MUST be, and the slot may not enter LOADING until the
// journal acknowledges.
//
// The verdict enum BORROWS `mem::UploadVerdict` for 0..7 exactly as
// `PageLoadVerdict` does, so the two directions of terrain traffic cannot end
// up with two refusal taxonomies. Value 5 (kUploadCrcFail) is deliberately NOT
// borrowed: layer F carries no CRC of its own, and the page's CRC covers the
// body as it was at LOAD time, which is precisely what a stamped sheet is no
// longer. There is nothing to check the bytes against and the model does not
// pretend there is.
enum SheetWritebackVerdict : int {
  kSheetOk = mem::kUploadOk,                              // 0
  kSheetUnaligned = mem::kUploadUnaligned,                // 1
  kSheetZeroLength = mem::kUploadZeroLength,              // 2 (unreachable)
  kSheetOutsidePool = mem::kUploadOutsideGuard,           // 3
  kSheetEpochStale = mem::kUploadEpochStale,              // 4
  kSheetOutsideJournal = mem::kUploadSourceOutsideArena,  // 6
  kSheetUnreachable = mem::kUploadSourceUnreachable,      // 7
  // The slot holds a valid page OF ANOTHER PATCH. No content test can catch
  // that -- another patch's page is internally perfect -- so the 64-byte header
  // is read first and its restated key is what refuses.
  kSheetHeaderIdent = 8,
  // A guard denial or a bridge `err` stopped the transfer.
  kSheetIncomplete = 9,
  // The `seq` is already outstanding. Two live tickets with one sequence make
  // the ACK match ambiguous, and an ambiguous barrier is not a barrier.
  kSheetSeqInFlight = 10,
  // The journal acknowledged with ok = 0. The slot is NOT released.
  kSheetJournalNak = 11,
};

static_assert(static_cast<int>(kSheetOutsideJournal) == 6,
              "the arena verdict keeps mem_upload's value");
static_assert(static_cast<int>(kSheetHeaderIdent) > static_cast<int>(kSheetUnreachable),
              "the terrain-only verdicts must sit above the borrowed range");
static_assert(static_cast<int>(kSheetJournalNak) == 11,
              "the ACK verdicts are the top of the table");

struct SheetWritebackRequest {
  uint32_t slot = 0;
  uint8_t generation = 0;
  uint32_t epoch = 0;
  uint32_t island_id = 0;
  int16_t patch_ix = 0;
  int16_t patch_iz = 0;
  uint64_t journal_addr = 0;  // where SW.STREAM wants this sheet
  uint32_t seq = 0;           // the ticket the journal echoes in its ACK
  uint32_t src_id = 0;
};

struct SheetWritebackResult {
  int verdict = kSheetOk;
  bool ok = false;           // the job succeeded end to end
  bool wb_released = false;  // a `wb_*` was presented to TERRAIN.RESIDENCY
  uint32_t bytes_written = 0;
  uint32_t guard_reqs = 0;  // 1 header + kSheetReadChunks, or 0 if refused
  uint32_t write_bursts = 0;
};

// Field for field the counter set the RTL exports, so a test compares ledgers
// rather than spot-checking non-zero.
struct SheetWritebackLedger {
  uint32_t sheets_written = 0;
  uint32_t sheets_refused = 0;  // judged from the job alone; nothing touched
  uint32_t sheets_faulted = 0;  // the block had already touched memory
  uint32_t hdr_ident_fails = 0;
  uint32_t guard_denied = 0;
  uint32_t bridge_errs = 0;
  uint32_t acks_ok = 0;
  uint32_t acks_nak = 0;
  uint32_t acks_unmatched = 0;
  uint32_t acks_after_epoch = 0;
  uint32_t seq_conflicts = 0;
  uint32_t wb_bytes = 0;
};

// Where entry `n` of a journal arena lives. A FUNCTION rather than a stride left
// to each call site, for `upload_src_of`'s reason: a stride computed twice is a
// stride that can disagree with itself.
inline uint64_t sheet_journal_addr(uint32_t entry, uint64_t journal_base) {
  return journal_base + static_cast<uint64_t>(entry) * kLayerFBytes;
}

// THE PRE-TRANSFER VERDICT, delegating rather than re-deriving.
//
// `upload_verdict` is called with the ROLES REVERSED against the loader's use:
// the `region` it bounds-checks is where the SOURCE lives (the page pool) and
// the `hps_arena` is where the DESTINATION lives (the journal). That is the
// whole point of borrowing it -- the ORDER of the tests is the law, and this
// direction gets the same order rather than a second one written for it.
//
// `seq_in_flight` is the caller's knowledge, not the model's: whether a
// sequence number is outstanding is a property of a hardware table, and a
// scalar model that kept its own would be a second table to disagree with.
inline int sheet_pre_verdict(const SheetWritebackRequest& r,
                             const mem::GuardRegion& journal_arena, uint32_t pool_base,
                             uint32_t pool_slots, uint32_t current_epoch, bool seq_in_flight) {
  // A slot outside the pool is refused BEFORE the address is formed, so a wild
  // slot index can never become a wild address.
  if (r.slot >= pool_slots) return kSheetOutsidePool;
  const mem::GuardRegion pool{pool_base, pool_slots * kPageBytes};
  // The epoch is tested BELOW at full 32 bits, in upload_verdict's own
  // position, for the reason page_pre_verdict gives: upload_verdict takes
  // uint16 epochs and T1's key carries resource_epoch:u32, so truncating here
  // would make two epochs that differ only above bit 15 compare EQUAL in the
  // model and unequal in the hardware.
  const int v =
      mem::upload_verdict(pool, journal_arena, r.journal_addr,
                          page_vram_addr(r.slot, pool_base) + kSheetChunkStart,
                          kLayerFBytes, 0, 0);
  if (v != mem::kUploadOk) return v;
  if (r.epoch != current_epoch) return kSheetEpochStale;
  // Last, because it is the only test whose answer is not a property of the
  // request: a duplicate sequence is a producer bug, and a producer bug behind
  // a malformed address is still reported as the malformed address.
  if (seq_in_flight) return kSheetSeqInFlight;
  return kSheetOk;
}

// The whole writeback. `page` is the 21,376 resident bytes of the SLOT the job
// names -- which is not necessarily the job's own page, and catching that is
// the header check's job. `journal_out`, if given, receives the 8,192 bytes.
//
// `transfer_complete` and `ack` stand in for the guard, the bridge and the far
// side: what a scalar model cannot derive and hardware absolutely can.
//   ack < 0 : no acknowledgement yet (the barrier is still held)
//   ack == 0: the journal refused (kSheetJournalNak)
//   ack > 0 : acknowledged good
inline SheetWritebackResult sheet_writeback(const SheetWritebackRequest& r, const uint8_t* page,
                                            const mem::GuardRegion& journal_arena,
                                            uint32_t pool_base, uint32_t pool_slots,
                                            uint32_t current_epoch, bool seq_in_flight,
                                            bool transfer_complete, int ack,
                                            bool check_header_ident,
                                            uint8_t* journal_out = nullptr,
                                            SheetWritebackLedger* L = nullptr) {
  SheetWritebackResult out;

  const int pre =
      sheet_pre_verdict(r, journal_arena, pool_base, pool_slots, current_epoch, seq_in_flight);
  if (pre != kSheetOk) {
    out.verdict = pre;
    if (L) {
      L->sheets_refused++;
      if (pre == kSheetSeqInFlight) L->seq_conflicts++;
    }
    return out;
  }

  // THE HEADER IS READ FIRST AND COSTS ONE BURST. Identity before payload: with
  // MEM.GUARD admitting reads of the WHOLE pool, "journalled another patch's
  // scars" is reachable, and the page header is the only thing in the tree that
  // can see it. Zero journal bytes move on this path -- a corruption check that
  // fires after the corruption is filed has not checked anything.
  out.guard_reqs = 1;
  const PageHeader h = parse_page_header(page);
  if (check_header_ident && (h.format_version != 1 || h.island_id != r.island_id ||
                             h.patch_ix != r.patch_ix || h.patch_iz != r.patch_iz)) {
    out.verdict = kSheetHeaderIdent;
    if (L) {
      L->sheets_faulted++;
      L->hdr_ident_fails++;
    }
    return out;
  }

  out.guard_reqs = 1 + kSheetReadChunks;
  if (!transfer_complete) {
    out.verdict = kSheetIncomplete;
    if (L) L->sheets_faulted++;
    return out;
  }

  out.write_bursts = kSheetWriteBursts;
  out.bytes_written = kLayerFBytes;
  if (journal_out) sheet_extract(page, journal_out);
  // `sheets_written` counts BYTES THAT RETIRED ON THE BRIDGE, not barriers
  // released -- the two differ by exactly the outstanding tickets, and a single
  // counter for both would make "how many sheets are still unacknowledged"
  // unanswerable at the very moment it matters.
  if (L) {
    L->wb_bytes += kLayerFBytes;
    L->sheets_written++;
  }

  // THE BARRIER. Every `wb_released` below is caused by an acknowledgement; the
  // model has no path that sets it otherwise, which is the property the block
  // exists to have.
  if (ack < 0) {
    out.verdict = kSheetOk;  // the bytes are away; the job is not finished
    out.ok = false;
    return out;
  }
  if (ack == 0) {
    out.verdict = kSheetJournalNak;
    if (L) {
      L->sheets_faulted++;
      L->acks_nak++;
    }
    return out;
  }

  out.verdict = kSheetOk;
  out.ok = true;
  out.wb_released = true;
  if (L) L->acks_ok++;
  return out;
}

}  // namespace terrain
}  // namespace zref

#endif  // ZREF_TERRAIN_PAGE_HPP
