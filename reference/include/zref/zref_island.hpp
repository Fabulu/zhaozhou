// zref_island.hpp — the sparse island directory.
//
// Islands are the unit of world composition (spec/terrain_rules.md §1.5). Each
// carries an id, a world origin, a pitch, a patch-grid extent, a tileset, and
// a SPARSE patch directory: `(ix, iz) -> patch page handle`, where an absent
// entry means open sky.
//
// `zref_terrain.hpp` says this piece is "Phase-6 loader work" and it was
// unbuilt. Everything around it existed -- the 33x33 lattice evaluation, the
// residency-set hash over {island_id, patch_ix, patch_iz}, page publication and
// pins -- but nothing said which patches an island HAS, so nothing could ask
// for an island bigger than the resident set.
//
// ---------------------------------------------------------------------------
// WHY SPARSENESS IS THE WHOLE FEATURE, NOT AN OPTIMISATION
// ---------------------------------------------------------------------------
// A patch is 32x32 cells on a 33x33 vertex lattice (charter §11.1). At the
// canonical 2.0 m pitch that is 64 m per side. So an 8 km island is
//
//     8000 m / 64 m = 125 patches per side  ->  15,625 patches
//
// against a residency of 1,024 pages. A dense directory cannot express that
// island at all, and a design that requires every patch to be resident cannot
// have one. terrain_rules §1.4 states the resolution directly:
//
//     "patch residency is sparse. A patch that is entirely sky simply does not
//      exist -- no page, no sheet, no draw. Sacrifice pays for its void; we do
//      not."
//
// So an absent entry is not a cache miss to be filled. It is OPEN SKY, and it
// is the normal case: an island whose solid ground is ~3.25 km² costs about
// 793 patches, which fits the residency with room to spare -- at 8 km ACROSS,
// because the sky between the ground costs nothing.
//
// ---------------------------------------------------------------------------
// WHAT THIS HEADER DOES NOT DO
// ---------------------------------------------------------------------------
// It does not stream, evict, or publish. Residency lifetime belongs to
// `zref_residency.hpp` and the page contents to `zref_terrain.hpp`. This is the
// directory that says which patches EXIST and where they are; asking for a
// patch that exists but is not resident is a residency question and this
// header deliberately answers only the first half, so the two cannot be
// confused. `Lookup` reports them as distinct outcomes for exactly that reason.

#ifndef ZREF_ISLAND_HPP
#define ZREF_ISLAND_HPP

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace zref {
namespace island {

// A patch is 32x32 cells on a 33x33 vertex lattice.
constexpr int32_t kPatchCells = 32;
constexpr int32_t kPatchVerts = 33;

// pitch_log2 is constrained to {-1, 0, +1, +2} so world->cell is a shift and
// every lattice x/z stays exactly representable in fx16 (terrain_rules §1.3).
constexpr int8_t kPitchLog2Min = -1;
constexpr int8_t kPitchLog2Max = 2;

constexpr bool pitch_legal(int8_t pitch_log2) {
  return pitch_log2 >= kPitchLog2Min && pitch_log2 <= kPitchLog2Max;
}

// Metres per cell for a legal pitch: 0.5, 1, 2, 4.
constexpr int32_t patch_metres(int8_t pitch_log2) {
  return (pitch_log2 >= 0) ? (kPatchCells << pitch_log2) : (kPatchCells >> (-pitch_log2));
}

enum class Outcome : uint8_t {
  kResident = 0,     // the patch exists and has a page
  kOpenSky = 1,      // no entry: not a miss, this is sky and is normal
  kOutOfExtent = 2,  // the coordinate is outside the island's grid entirely
  kBadPitch = 3,     // pitch_log2 outside the ratified set
};

struct Lookup {
  Outcome outcome = Outcome::kOpenSky;
  uint32_t page_handle = 0;  // meaningful only when kResident
};

struct Ledger {
  uint32_t resident = 0;
  uint32_t open_sky = 0;
  uint32_t out_of_extent = 0;
  uint32_t bad_pitch = 0;
};

// The island descriptor. Heights inside a patch are relative to the island
// DATUM, which is why an island can sit anywhere in the fx16 +/-32 km world
// while height16 stays within +/-128 m of local relief -- that is what lets an
// island float rather than merely being terrain at altitude.
struct Desc {
  uint32_t island_id = 0;
  int32_t origin_x = 0;  // fx16 raw, world
  int32_t origin_y = 0;  // fx16 raw, the datum this island's heights hang from
  int32_t origin_z = 0;  // fx16 raw, world
  int8_t pitch_log2 = 1;  // canonical 2.0 m
  uint16_t extent_ix = 0;
  uint16_t extent_iz = 0;
  uint32_t tileset_id = 0;
};

// Metres across, per axis. u16 extent times up to 128 m exceeds int32 only
// beyond 2^24 patches, so this cannot overflow for any expressible island.
constexpr int64_t extent_metres_x(const Desc& d) {
  return static_cast<int64_t>(d.extent_ix) * patch_metres(d.pitch_log2);
}
constexpr int64_t extent_metres_z(const Desc& d) {
  return static_cast<int64_t>(d.extent_iz) * patch_metres(d.pitch_log2);
}

// The sparse directory itself. Ordered rather than hashed so iteration is
// deterministic -- a reference model that enumerates in an arbitrary order
// produces diffs that depend on the standard library.
class Directory {
 public:
  explicit Directory(const Desc& d) : desc_(d) {}

  const Desc& desc() const { return desc_; }

  // Publish a patch into the directory. Returns false if the coordinate is
  // outside the extent, because an island cannot own a patch it does not span.
  bool set(int32_t ix, int32_t iz, uint32_t page_handle) {
    if (!in_extent(ix, iz)) return false;
    entries_[key(ix, iz)] = page_handle;
    return true;
  }

  bool erase(int32_t ix, int32_t iz) { return entries_.erase(key(ix, iz)) != 0; }

  // The number of patches that EXIST, which is the number that costs anything.
  std::size_t resident_count() const { return entries_.size(); }

  // The number the grid could hold if it were dense -- the figure a
  // non-sparse design would have to pay, kept alongside so the ratio is
  // visible rather than asserted.
  int64_t dense_count() const {
    return static_cast<int64_t>(desc_.extent_ix) * static_cast<int64_t>(desc_.extent_iz);
  }

  bool in_extent(int32_t ix, int32_t iz) const {
    return ix >= 0 && iz >= 0 && ix < static_cast<int32_t>(desc_.extent_ix) &&
           iz < static_cast<int32_t>(desc_.extent_iz);
  }

  Lookup find(int32_t ix, int32_t iz, Ledger* L = nullptr) const {
    Lookup r;
    if (!pitch_legal(desc_.pitch_log2)) {
      r.outcome = Outcome::kBadPitch;
      if (L) L->bad_pitch++;
      return r;
    }
    if (!in_extent(ix, iz)) {
      r.outcome = Outcome::kOutOfExtent;
      if (L) L->out_of_extent++;
      return r;
    }
    const auto it = entries_.find(key(ix, iz));
    if (it == entries_.end()) {
      // NOT A MISS. Sky is the ordinary answer for most of an island's grid,
      // and reporting it as a failure would make the normal case look like an
      // error and hide the ones that are.
      r.outcome = Outcome::kOpenSky;
      if (L) L->open_sky++;
      return r;
    }
    r.outcome = Outcome::kResident;
    r.page_handle = it->second;
    if (L) L->resident++;
    return r;
  }

 private:
  static std::pair<int32_t, int32_t> key(int32_t ix, int32_t iz) { return {ix, iz}; }

  Desc desc_;
  std::map<std::pair<int32_t, int32_t>, uint32_t> entries_;
};


// ===========================================================================
// THE VISIBLE SET -- which patches a camera's window actually has ground in.
// ===========================================================================
// `Directory::find` answers ONE question about ONE patch. Nothing above it
// decided WHICH questions to ask, and reports/Missingterrain names that hole
// exactly:
//
//     "Nothing currently does: camera moved -> inspect island directory ->
//      determine visible patch coordinates -> ... -> issue all visible patches
//      to the terrain engine."
//
// This is the coordinate generator plus filter that closes it, and it lives
// HERE rather than inside the streamer because two things need it -- the
// streamer, which turns it into publish/evict, and TERRAIN.VISIBLE, the RTL
// block that issues it into the terrain engine. When it lived only inside
// `Streamer::update` the RTL had to be written against a transcription of that
// loop, which is precisely the drift this tree keeps paying for.

// A square view window in patch coordinates.
//
// SQUARE RATHER THAN CIRCULAR, ON PURPOSE. The visible set is a conservative
// SUPERSET of what is drawn: a residency policy that is exact about the frustum
// evicts patches the moment the camera turns, and then has to fetch them back
// on the next turn. Hysteresis belongs in the view, not in the draw path.
// Rounding the corners off this window would save 21% of the queries and buy a
// thrash. Do not "improve" it to a circle.
struct View {
  int32_t centre_ix = 0;
  int32_t centre_iz = 0;
  int32_t radius = 0;  // patches; the window is (2*radius+1) on a side
};

// One patch that came back RESIDENT, with the handle the caller will draw from.
struct Visible {
  int32_t ix = 0;
  int32_t iz = 0;
  uint32_t page_handle = 0;
};

// What the window COST, which is not what it produced.
//
// `emitted` is the small number; `examined` is the one the hardware is sized
// against. A window of radius R asks (2R+1)^2 questions and, on an island whose
// grid is 94.9% sky, typically answers a small fraction of them with ground.
// So the tally counts the rejections separately and by reason -- a window that
// found nothing because it is over open water and a window that found nothing
// because the camera left the island are different events and must not share a
// counter.
struct WindowTally {
  uint32_t examined = 0;       // queries ASKED
  uint32_t emitted = 0;        // ...that came back RESIDENT
  uint32_t sky = 0;            // ...OPEN SKY: inside the island, no ground
  uint32_t out_of_extent = 0;  // ...outside the island's grid entirely
  uint32_t bad_pitch = 0;      // ...malformed descriptor; the whole window is
};

// The visible set, in EMISSION ORDER.
//
// ROW-MAJOR: iz outer, ix inner, both ascending. The order is part of the
// contract and not an artefact of how the loop happens to be nested, because a
// downstream consumer depends on it -- a hardware scan that walks a row at a
// time keeps the residency's set index sweeping contiguously, and a test that
// compared only the SET would pass a block that emitted the right patches in
// the wrong order, which is a defect no picture would show.
//
// A malformed descriptor is NOT short-circuited here. Every cell is still
// visited and counted as `bad_pitch`, because this function does not own the
// pitch rule -- `Directory::find` does, and duplicating the gate here is how
// the two would drift. The cost of a wasted window is the price of having one
// definition of legality.
//
// A negative radius yields an empty window rather than an inverted loop.
inline std::vector<Visible> visible_set(const Directory& dir, const View& v,
                                        WindowTally* T = nullptr) {
  std::vector<Visible> out;
  if (v.radius < 0) return out;
  for (int32_t iz = v.centre_iz - v.radius; iz <= v.centre_iz + v.radius; ++iz) {
    for (int32_t ix = v.centre_ix - v.radius; ix <= v.centre_ix + v.radius; ++ix) {
      const Lookup r = dir.find(ix, iz);
      if (T) ++T->examined;
      switch (r.outcome) {
        case Outcome::kResident:
          out.push_back(Visible{ix, iz, r.page_handle});
          if (T) ++T->emitted;
          break;
        case Outcome::kOpenSky:
          if (T) ++T->sky;
          break;
        case Outcome::kOutOfExtent:
          if (T) ++T->out_of_extent;
          break;
        default:
          if (T) ++T->bad_pitch;
          break;
      }
    }
  }
  return out;
}

}  // namespace island
}  // namespace zref

#endif  // ZREF_ISLAND_HPP
