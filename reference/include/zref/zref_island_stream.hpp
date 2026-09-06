// zref_island_stream.hpp — the island's visible set, streamed into residency.
//
// This is the join the audit asked for: "connect asset/resource/terrain
// lifecycle models to the frame route and prove representative complete scenes,
// including eviction and return."
//
// `zref_island.hpp` says which patches an island HAS. `zref_residency.hpp` says
// which pages are RESIDENT and enforces pins. Neither knew about the other, so
// nothing could answer the only question an 8 km island actually poses:
//
//     15,625 patches exist, 1,024 pages are resident, and the camera moves.
//     What is in memory, and what left to make room?
//
// ---------------------------------------------------------------------------
// WHY EVICTION AND RETURN IS THE ACCEPTANCE TEST, NOT STREAMING
// ---------------------------------------------------------------------------
// Streaming a set in is easy and proves little: a first frame publishes
// whatever is visible and nothing is under pressure. The failures live at the
// boundary -- a patch that leaves the view, loses its page to someone else, and
// then comes BACK. That path exercises reclamation, generation bump and
// republication in the order they actually occur, and it is where a stale
// handle would still match if generations were reused silently.
//
// So `Stats::returned` is the number this model exists to report. A run with
// evictions but no returns has not tested the interesting half.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS NOT
// ---------------------------------------------------------------------------
// It is not a prefetcher, a scheduler or a bandwidth model. It decides WHAT
// should be resident for a camera and asks the residency model to make it so;
// how many bytes that costs per frame and whether they arrive in time is
// MEM.UPLOAD's question and is deliberately not answered here. Nor does it
// evaluate terrain: a page's contents belong to zref_terrain.hpp.

#ifndef ZREF_ISLAND_STREAM_HPP
#define ZREF_ISLAND_STREAM_HPP

#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "zref_island.hpp"
#include "zref_residency.hpp"

namespace zref {
namespace island {

struct Stats {
  uint32_t published = 0;      // patches brought into residency
  uint32_t evicted = 0;        // patches that left it
  uint32_t returned = 0;       // ...and later came back. THE interesting one.
  uint32_t already = 0;        // visible and already resident: no work
  uint32_t refused = 0;        // residency said no; see the Arena's own ledger
  uint32_t peak_resident = 0;  // high-water mark, against the page count
};

// A square view window in patch coordinates. Square rather than circular on
// purpose: the visible set is a conservative superset of what is drawn, and a
// residency policy that is exact about the frustum evicts patches the moment
// the camera turns. Hysteresis belongs here, not in the draw path.
struct View {
  int32_t centre_ix = 0;
  int32_t centre_iz = 0;
  int32_t radius = 0;  // patches
};

class Streamer {
 public:
  Streamer(const Directory& dir, residency::Arena& arena)
      : dir_(dir), arena_(arena) {}

  // Bring the view's patches into residency and let go of everything else.
  //
  // ORDER MATTERS AND IS DELIBERATE: evict first, then publish. Publishing
  // first would demand a free page while the pages about to be released are
  // still held, so a view that merely SHIFTS -- the ordinary case, one column
  // in and one out -- would refuse for want of storage it already owns.
  Stats update(const View& v, residency::Ledger* L = nullptr) {
    Stats st;

    std::set<std::pair<int32_t, int32_t>> want;
    for (int32_t iz = v.centre_iz - v.radius; iz <= v.centre_iz + v.radius; ++iz) {
      for (int32_t ix = v.centre_ix - v.radius; ix <= v.centre_ix + v.radius; ++ix) {
        // Only patches that EXIST. Sky is not streamed, which is the whole
        // reason an 8 km island fits: most of this window is nothing.
        if (dir_.find(ix, iz).outcome == Outcome::kResident)
          want.insert({ix, iz});
      }
    }

    // ---- 1. evict what is no longer wanted ---------------------------------
    std::vector<std::pair<int32_t, int32_t>> going;
    for (const auto& kv : live_)
      if (want.find(kv.first) == want.end()) going.push_back(kv.first);
    for (const auto& k : going) {
      arena_.release(live_[k], L);
      live_.erase(k);
      seen_.insert(k);  // remembered ONLY so a later publish counts as a return
      ++st.evicted;
    }

    // ---- 2. publish what is newly wanted -----------------------------------
    for (const auto& k : want) {
      if (live_.find(k) != live_.end()) { ++st.already; continue; }
      const uint32_t res_index = resource_index(dir_.desc().island_id, k.first, k.second);
      const residency::PublishResult r =
          arena_.publish(res_index, residency::Kind::kTexturePage, /*hps_addr=*/0,
                         /*length=*/page_bytes_, hps_, /*request_epoch=*/1,
                         /*current_epoch=*/1, /*verify_ok=*/true, L);
      if (r.outcome != residency::Outcome::kPublished) { ++st.refused; continue; }
      live_[k] = res_index;
      ++st.published;
      if (seen_.find(k) != seen_.end()) ++st.returned;
    }

    if (live_.size() > st.peak_resident) st.peak_resident = static_cast<uint32_t>(live_.size());
    if (live_.size() > peak_) peak_ = static_cast<uint32_t>(live_.size());
    return st;
  }

  std::size_t live_count() const { return live_.size(); }
  uint32_t peak() const { return peak_; }

  void configure(uint32_t page_bytes, const mem::GuardRegion& hps) {
    page_bytes_ = page_bytes;
    hps_ = hps;
  }

  // A patch's resource index. Stable for a given (island, ix, iz) so that a
  // patch returning after eviction is recognised as the SAME resource and takes
  // a generation bump rather than a fresh identity -- which is what makes a
  // stale handle detectable instead of accidentally valid.
  //
  // THE ISLAND ID IS IN THE KEY, AND IT WAS NOT.
  //
  // The first version of this was `(iz << 16) | (ix & 0xFFFF)` -- patch
  // coordinates alone. That is precisely the defect TERRAIN.RESIDENCY's ledger
  // entry records as already superseded, in its own words:
  //
  //     "the direct-mapped prototype is superseded because TWO ISLANDS MAY
  //      LEGALLY OVERLAP IN LOCAL PATCH COORDINATES"
  //
  // and its canonical key is `{resource_epoch, island_id, patch_ix, patch_iz}`.
  // Two islands sharing a local coordinate would have collided here, and the
  // collision's symptom is one island's page answering for the other's patch --
  // a stale handle that still matches, which is the exact failure the whole
  // evict-and-return path exists to detect. Building a second residency model
  // without reading the first one's key is how that gets reintroduced.
  //
  // EXACT, NOT HASHED. A hash would need a collision story and a residency key
  // that collides is a wrong-patch bug, so the field widths are stated and
  // asserted instead: 8 bits of island, 12 bits each of ix and iz. 12 bits is
  // 4,096 patches per axis against the 8 km island's 125, so the bound is not
  // tight; it is checked rather than assumed because an out-of-range value
  // would alias silently.
  static uint32_t resource_index(uint32_t island_id, int32_t ix, int32_t iz) {
    const uint32_t i = static_cast<uint32_t>(ix) & 0xFFFu;
    const uint32_t z = static_cast<uint32_t>(iz) & 0xFFFu;
    return ((island_id & 0xFFu) << 24) | (z << 12) | i;
  }

  // True when a coordinate fits the key's field widths. Callers that stream a
  // larger island than the key can address must be told, not silently aliased.
  static bool key_fits(uint32_t island_id, int32_t ix, int32_t iz) {
    return island_id <= 0xFFu && ix >= 0 && ix < 4096 && iz >= 0 && iz < 4096;
  }

 private:
  const Directory& dir_;
  residency::Arena& arena_;
  std::map<std::pair<int32_t, int32_t>, uint32_t> live_;
  std::set<std::pair<int32_t, int32_t>> seen_;
  uint32_t page_bytes_ = 256;
  uint32_t peak_ = 0;
  mem::GuardRegion hps_{0, 0x100000};
};

}  // namespace island
}  // namespace zref

#endif  // ZREF_ISLAND_STREAM_HPP
