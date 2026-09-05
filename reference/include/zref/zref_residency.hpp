// zref_residency.hpp — the upload TRANSACTION and resource lifetime.
// Authored 2026-09-05 for roadmap gate G2-B.
//
// ---------------------------------------------------------------------------
// WHAT THIS ADDS, AND WHAT IT DELIBERATELY DOES NOT REPEAT
// ---------------------------------------------------------------------------
// `zref_mem_upload.hpp` already owns the VALIDATION half -- alignment, 64-bit
// containment of both destination and source, epoch staleness, the verdict
// ORDER -- and says in as many words that it "does NOT own bandwidth,
// arbitration, residency policy or eviction".
//
// This is the residency and lifetime half. It calls `upload_verdict` rather
// than re-deriving any of it, because two validators that agree until they do
// not is the defect class this repository keeps recording.
//
// ---------------------------------------------------------------------------
// THE LIFECYCLE, WHICH IS RULED AND NOT NEGOTIABLE
// ---------------------------------------------------------------------------
//     allocate fresh, unpinned destination
//       -> validate source and destination
//       -> copy immutable bytes
//       -> retire every destination write
//       -> verify integrity
//       -> publish mapping atomically
//       -> pin for consuming frames
//       -> reclaim old storage only after its last pin
//
// The sentence that makes it a lifecycle rather than a sequence:
//
//     **Generations do not make overwriting an old live page safe; fresh
//     storage and pinning do.**
//
// A generation-tagged cache (D-3) guarantees that a STALE READER CANNOT MATCH.
// It says nothing about a reader that is mid-frame on the old bytes. So this
// model refuses to write into any page that is pinned, and refuses to reclaim
// one until its last pin is released -- and both refusals are counted, because
// a silent one is how a torn frame becomes an intermittent bug.
//
// ---------------------------------------------------------------------------
// WHAT IS MODELLED AND WHAT IS NOT
// ---------------------------------------------------------------------------
// Modelled: allocation, publication atomicity, pinning, reclamation order,
// generation threading, and every refusal.
//
// NOT modelled: actual DMA, bandwidth, arbitration between clients, or the
// physical arena allocator's placement policy. Those belong to MEM.GUARD and
// the platform, and inventing them here would put a second answer beside the
// real one.

#ifndef ZREF_RESIDENCY_HPP
#define ZREF_RESIDENCY_HPP

#include <cstdint>
#include <vector>

#include "zref/zref_mem_upload.hpp"

namespace zref {
namespace residency {

enum class Kind : uint8_t { kTexturePage = 10, kMaterialSet = 11, kMeshStream = 12 };

enum class Outcome : uint8_t {
  kPublished = 0,
  kRefusedValidation = 1,      // upload_verdict said no; see `verdict`
  kRefusedNoStorage = 2,       // no fresh, unpinned page available
  kRefusedIntegrity = 3,       // the bytes did not verify after the copy
  kRefusedGenerationWrap = 4,  // 16-bit generation would wrap; needs an epoch
  // The upload is longer than one page, and this allocator hands out exactly
  // one. Distinct from the two refusals it would otherwise be mistaken for: a
  // page WAS available, so it is not kRefusedNoStorage, and `upload_verdict`
  // genuinely ACCEPTED the bounds, so it is not kRefusedValidation.
  kRefusedOversize = 5,
};

struct Page {
  uint32_t vram_addr = 0;
  uint32_t length = 0;
  bool occupied = false;
  uint32_t pins = 0;
  uint32_t resource_index = 0;
  uint16_t generation = 0;
  Kind kind = Kind::kTexturePage;
};

struct Ledger {
  uint32_t published = 0;
  uint32_t refused_validation = 0;
  uint32_t refused_no_storage = 0;
  uint32_t refused_integrity = 0;
  uint32_t refused_generation_wrap = 0;
  uint32_t refused_oversize = 0;  // longer than one page; see kRefusedOversize
  uint32_t reclaim_blocked_by_pin = 0;  // an attempt to reclaim a pinned page
  uint32_t write_blocked_by_pin = 0;    // an attempt to write into a pinned page
};

struct PublishResult {
  Outcome outcome = Outcome::kRefusedValidation;
  mem::UploadVerdict verdict = mem::kUploadOk;
  uint32_t resource_index = 0;
  uint16_t generation = 0;
  int page = -1;
  bool ok() const { return outcome == Outcome::kPublished; }
};

// A resource's current mapping. Publishing REPLACES the mapping atomically:
// readers see either the old page or the new one, never a half-written page,
// because the new bytes land somewhere else entirely.
struct Mapping {
  uint32_t resource_index = 0;
  Kind kind = Kind::kTexturePage;
  int page = -1;
  uint16_t generation = 0;
};

class Arena {
 public:
  Arena(uint32_t base, uint32_t page_bytes, int page_count) : base_(base), page_bytes_(page_bytes) {
    pages_.resize(static_cast<std::size_t>(page_count));
    for (int i = 0; i < page_count; ++i) {
      pages_[static_cast<std::size_t>(i)].vram_addr = base + static_cast<uint32_t>(i) * page_bytes;
    }
    guard_.base = base;
    guard_.bytes = page_bytes * static_cast<uint32_t>(page_count);
  }

  // The whole point of the ordering. `verify_ok` stands in for the integrity
  // check that in hardware happens AFTER the bytes land -- it is a parameter
  // rather than a computed CRC because this model owns ordering, not hashing.
  PublishResult publish(uint32_t resource_index, Kind kind, uint64_t hps_addr, uint32_t length,
                        const mem::GuardRegion& hps_arena, uint16_t request_epoch,
                        uint16_t current_epoch, bool verify_ok, Ledger* L = nullptr) {
    PublishResult r;
    r.resource_index = resource_index;

    // 0. IT MUST FIT IN A PAGE. AUDIT R8.
    //
    //    This allocator hands out ONE fixed-size page, and `upload_verdict` is
    //    called below against a guard region covering the WHOLE ARENA:
    //
    //        guard_.bytes = page_bytes * page_count
    //
    //    So without this check a length larger than a page is "in bounds" -- it
    //    simply runs off the end of the page it was given and into the next
    //    one, which may be PINNED. The audit's counterexample: publish A into
    //    page 0, publish and pin B into page 1, republish A elsewhere to free
    //    page 0, then publish C with two pages' worth of length. Page 0 is
    //    chosen, the arena-wide bounds accept it, and C's footprint covers
    //    pinned B -- with every pin counter still reading zero.
    //
    //    Checked BEFORE allocation, so an oversize request does not consume a
    //    free page on its way to being refused.
    //
    //    The bound is `>`, not `>=`: an upload of exactly one page is legal and
    //    ordinary, and refusing it would trade one defect for another.
    if (length > page_bytes_) {
      r.outcome = Outcome::kRefusedOversize;
      if (L) L->refused_oversize++;
      return r;
    }

    // 1. ALLOCATE FRESH, UNPINNED. Chosen before validation so that a refusal
    //    never leaves a half-claimed page, and never touches the live one.
    const int dst = find_free_page();
    if (dst < 0) {
      r.outcome = Outcome::kRefusedNoStorage;
      if (L) L->refused_no_storage++;
      return r;
    }

    // 2. VALIDATE, through the existing oracle -- not a second copy of it.
    r.verdict = mem::upload_verdict(guard_, hps_arena, hps_addr,
                                    pages_[static_cast<std::size_t>(dst)].vram_addr, length,
                                    request_epoch, current_epoch);
    if (r.verdict != mem::kUploadOk) {
      r.outcome = Outcome::kRefusedValidation;
      if (L) L->refused_validation++;
      return r;
    }

    // 3-4. COPY and RETIRE. Modelled as "the page now holds these bytes"; the
    //      burst mechanics belong to MEM.GUARD.
    // 5. VERIFY, after the writes retire and before anything is published.
    if (!verify_ok) {
      r.outcome = Outcome::kRefusedIntegrity;
      if (L) L->refused_integrity++;
      return r;  // the fresh page is simply not published; nothing was disturbed
    }

    // 6. PUBLISH ATOMICALLY. The generation is the existing 16-bit residency
    //    generation, and SILENT WRAP IS FORBIDDEN -- a wrap requires an epoch
    //    transition and global invalidation, so it is refused here rather than
    //    quietly reusing a number a stale handle could still match.
    Mapping* m = find_mapping(resource_index);
    const uint16_t next_gen =
        m ? static_cast<uint16_t>(m->generation + 1) : static_cast<uint16_t>(1);
    if (m && next_gen == 0) {
      r.outcome = Outcome::kRefusedGenerationWrap;
      if (L) L->refused_generation_wrap++;
      return r;
    }

    const int old_page = m ? m->page : -1;

    Page& p = pages_[static_cast<std::size_t>(dst)];
    p.occupied = true;
    p.length = length;
    p.resource_index = resource_index;
    p.generation = next_gen;
    p.kind = kind;
    p.pins = 0;

    if (m) {
      m->page = dst;
      m->generation = next_gen;
      m->kind = kind;
    } else {
      Mapping nm;
      nm.resource_index = resource_index;
      nm.kind = kind;
      nm.page = dst;
      nm.generation = next_gen;
      mappings_.push_back(nm);
    }

    // 8. RECLAIM OLD STORAGE ONLY AFTER ITS LAST PIN. If it is still pinned,
    //    it stays occupied and unreferenced -- reclaimable later, never now.
    if (old_page >= 0) try_reclaim(old_page, L);

    r.outcome = Outcome::kPublished;
    r.generation = next_gen;
    r.page = dst;
    if (L) L->published++;
    return r;
  }

  // A consuming frame pins what it reads. Pins are what make "do not reclaim
  // yet" a fact rather than a hope.
  bool pin(uint32_t resource_index) {
    Mapping* m = find_mapping(resource_index);
    if (!m || m->page < 0) return false;
    pages_[static_cast<std::size_t>(m->page)].pins++;
    return true;
  }

  // Releasing the LAST pin is what makes an unreferenced page reclaimable.
  bool unpin(int page, Ledger* L = nullptr) {
    if (page < 0 || page >= static_cast<int>(pages_.size())) return false;
    Page& p = pages_[static_cast<std::size_t>(page)];
    if (p.pins == 0) return false;
    --p.pins;
    if (p.pins == 0 && !is_mapped(page)) try_reclaim(page, L);
    return true;
  }

  // Writing into a pinned page is the error the whole lifecycle exists to
  // prevent, so it is a refusal with a counter rather than an assertion.
  // RELEASE A RESOURCE'S PAGE. The eviction MECHANISM; the policy that decides
  // what to evict belongs to the caller and deliberately does not live here.
  //
  // Until this existed, a page could only be freed as a side effect of
  // REPUBLISHING the same resource -- so a resource that simply stopped being
  // wanted held its page forever. That is survivable while everything is
  // resident and fatal the moment it is not: an 8 km island has 15,625 patches
  // against 1,024 pages, so the camera moving is the ordinary case and letting
  // go is as load-bearing as taking hold.
  //
  // A PINNED PAGE IS NOT RELEASED, and the refusal is counted rather than
  // silent. A pin means a frame in flight is still reading it; dropping it
  // would be the overlap `kRefusedOversize` exists to prevent, arriving by a
  // different route.
  bool release(uint32_t resource_index, Ledger* L = nullptr) {
    for (std::size_t i = 0; i < mappings_.size(); ++i) {
      if (mappings_[i].resource_index != resource_index) continue;
      const int page = mappings_[i].page;
      if (pages_[static_cast<std::size_t>(page)].pins > 0) {
        if (L) L->reclaim_blocked_by_pin++;
        return false;
      }
      try_reclaim(page, L);
      mappings_.erase(mappings_.begin() + static_cast<std::ptrdiff_t>(i));
      return true;
    }
    return false;  // not mapped: releasing what was never taken is not an error
  }

  bool may_write(int page, Ledger* L = nullptr) const {
    if (page < 0 || page >= static_cast<int>(pages_.size())) return false;
    if (pages_[static_cast<std::size_t>(page)].pins > 0) {
      if (L) const_cast<Ledger*>(L)->write_blocked_by_pin++;
      return false;
    }
    return true;
  }

  const Page& page(int i) const { return pages_[static_cast<std::size_t>(i)]; }
  int page_count() const { return static_cast<int>(pages_.size()); }
  const Mapping* mapping(uint32_t resource_index) const {
    for (const Mapping& m : mappings_)
      if (m.resource_index == resource_index) return &m;
    return nullptr;
  }
  int free_pages() const {
    int n = 0;
    for (const Page& p : pages_)
      if (!p.occupied) ++n;
    return n;
  }

 private:
  int find_free_page() const {
    for (std::size_t i = 0; i < pages_.size(); ++i)
      if (!pages_[i].occupied) return static_cast<int>(i);
    return -1;
  }
  Mapping* find_mapping(uint32_t idx) {
    for (Mapping& m : mappings_)
      if (m.resource_index == idx) return &m;
    return nullptr;
  }
  bool is_mapped(int page) const {
    for (const Mapping& m : mappings_)
      if (m.page == page) return true;
    return false;
  }
  void try_reclaim(int page, Ledger* L) {
    Page& p = pages_[static_cast<std::size_t>(page)];
    if (p.pins > 0) {
      if (L) L->reclaim_blocked_by_pin++;
      return;  // still being read by a frame in flight
    }
    p.occupied = false;
    p.length = 0;
    p.resource_index = 0;
    p.generation = 0;
  }

  uint32_t base_;
  uint32_t page_bytes_;
  mem::GuardRegion guard_{};
  std::vector<Page> pages_;
  std::vector<Mapping> mappings_;
};

}  // namespace residency
}  // namespace zref

#endif  // ZREF_RESIDENCY_HPP
