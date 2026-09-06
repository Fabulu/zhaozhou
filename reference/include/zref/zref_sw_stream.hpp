// zref_sw_stream.hpp -- SW.STREAM's frame policy: working set, budget, seal.
//
// Authored 2026-09-06 for step 4 of the world-layer build sequence
// (`reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md` §5.4). Everything here is a
// transcription of rulings T1-T12 in
// `reports/OWNER-RULINGS-BUILDABILITY-20260902.md`; where a ruling does not
// decide something the code says so at the point of the gap and COUNTS it,
// rather than choosing quietly.
//
// ---------------------------------------------------------------------------
// WHAT THIS COMPOSES, AND WHY IT DEFINES ALMOST NOTHING OF ITS OWN
// ---------------------------------------------------------------------------
// This tree has been bitten repeatedly by two definitions of one law drifting.
// `zref::island::visible_set` was EXTRACTED out of `zref::island::Streamer`
// precisely so TERRAIN.VISIBLE's RTL and the streamer could not disagree about
// what a camera sees. Writing a second visible-set loop here would undo that
// on the same afternoon it was done. So:
//
//   * "what a camera sees"       -> zref::island::visible_set   (zref_island.hpp)
//   * "which patches EXIST"      -> zref::island::Directory     (zref_island.hpp)
//   * "a patch's stable identity" -> zref::island::Streamer::resource_index
//                                     and ::key_fits            (zref_island_stream.hpp)
//   * "is this source inside its declared bounds"
//                                -> zref::mem::upload_source_in_arena
//                                                               (zref_mem_upload.hpp)
//   * "the F-sheet journal"      -> zref::terrain::Journal      (zref_fjournal.hpp)
//   * "CRC-32C"                  -> zhao_abi::zhao_crc32c       (generated)
//
// What is genuinely NEW here, because nothing in the tree had it, is the four
// things SW.STREAM alone owns:
//
//   1. the WORKING SET -- T7's union of two views, plus the Moore ring, plus
//      the 30-frame prediction;
//   2. the PAGE BUDGET -- T7's ceiling of 32 whole pages per frame, and what
//      is dropped when the camera outruns it;
//   3. the CANONICAL ORDER and the SEAL -- T5's seven sort keys and the
//      byte-identical list whose CRC is an identity rather than a checksum;
//   4. the STAGE-BEFORE-SEAL barrier -- T12's "never expose a half-built page
//      list to CMD.DMA".
//
// `zref::island::Streamer` models publish/evict/return against a residency
// arena and is NOT superseded; it answers a different question (what is
// resident) and this block explicitly does not choose slots -- T9/T10 own that.
// This model tracks only what it BELIEVES is resident, from the completions it
// was told about, which is exactly the information the real HPS has.

#ifndef ZREF_SW_STREAM_HPP
#define ZREF_SW_STREAM_HPP

#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "zhao_abi.h"  // generated (runtime/include): zhao_crc32c
#include "zref_fjournal.hpp"
#include "zref_island.hpp"
#include "zref_island_stream.hpp"
#include "zref_mem_upload.hpp"

namespace zref {
namespace swstream {

// ===========================================================================
// CONSTANTS -- every one of them a knob with a citation, never a literal
// buried in a loop. CLAUDE.md: "never remove the owner's control in the name
// of fidelity."
// ===========================================================================

// spec/terrain_rules.md §2: page stride 21,376 B, staged WHOLE.
constexpr uint32_t kPageBytes = 21376;

// T7: "Ceiling: 32 whole pages per frame ~= 41 MB/s at 21,376 B and 60 Hz.
// Provisional, not a board claim."
constexpr uint32_t kPageBudgetPerFrame = 32;

// THE BUDGET COUNTS TRANSFERS, NOT LIST ENTRIES, AND THE RULING PROVES IT.
// T7 states the ceiling and then derives 41 MB/s from it: 32 x 21,376 x 60.
// That derivation is only true if each of the 32 is a page actually moved. A
// record naming an already-resident page moves nothing, so it cannot consume a
// unit of a bandwidth budget. This is read off the ruling, not chosen.

// T7: "predicted visible set 30 frames (0.5 s) ahead from camera velocity."
constexpr int32_t kPredictFrames = 30;

// T7: "one-patch Moore ring around it."
constexpr int32_t kRingRadius = 1;

// T5's 32-byte patch-list record.
constexpr std::size_t kRecordBytes = 32;

// T5 flags.
enum PatchFlags : uint16_t {
  kFlagRequired = 1u << 0,
  kFlagPrefetch = 1u << 1,
  kFlagDynamic = 1u << 2,
  kFlagDual = 1u << 3,
  kFlagHasSavedF = 1u << 4,
};

// PRIORITY IS AN INTERPRETATION, AND IT IS A NAMED CONSTANT FOR THAT REASON.
//
// T5 sorts by "smaller priority first" and T7 orders the working set "required
// current, then predicted visible, then neighbour ring". Mapping the second
// onto the first is mechanical for those three. The FOURTH member of T7's
// working set -- "explicitly gameplay-required patches" -- is given no rank at
// all, in T7 or anywhere else.
//
// Ranking it above ordinary required-current is DERIVED from T6, whose
// degradation ladder retains "all player-contact/gameplay/collision-required
// live patches" (step 4) before "remaining visible live patches" (step 5). But
// T6's ladder governs the 256-slot COMPOSED-CACHE pressure, not the 32-page
// LOAD pressure, and no ruling says the two ladders are the same ladder.
//
// So this is an interpretation with a citation, exposed as an editable
// constant, and the gap is reported in `Ledger::gameplay_required_deferred`
// and `Frame::unruled_gameplay_starvation` rather than smoothed over.
constexpr uint8_t kPriorityGameplay = 0;
constexpr uint8_t kPriorityRequiredCurrent = 1;
constexpr uint8_t kPriorityPredicted = 2;
constexpr uint8_t kPriorityRing = 3;

// ===========================================================================
// THE 32-BYTE PATCH-LIST RECORD (T5, verbatim field order)
// ===========================================================================
struct PatchRecord {
  uint32_t island_id = 0;
  int16_t patch_ix = 0;
  int16_t patch_iz = 0;
  uint64_t hps_page_addr = 0;
  uint32_t expected_page_crc32c = 0;
  uint16_t flags = 0;
  uint8_t view_mask = 0;
  uint8_t priority = 0;
  uint32_t source_id = 0;
  uint32_t reserved = 0;
};

// Little-endian, field order exactly as T5 lists it, no padding of our own.
//
// SERIALISED BY HAND RATHER THAN memcpy'd FROM THE STRUCT. A struct's layout
// is the compiler's business -- alignment would insert two bytes after
// `patch_iz` on most ABIs and the sealed bytes would then depend on the host.
// The sealed list is capture data (T5); a capture that replays only on the
// machine that made it is not capture data.
inline void encode_record(const PatchRecord& r, uint8_t out[kRecordBytes]) {
  std::size_t o = 0;
  auto put8 = [&](uint8_t v) { out[o++] = v; };
  auto put16 = [&](uint16_t v) { put8(static_cast<uint8_t>(v)); put8(static_cast<uint8_t>(v >> 8)); };
  auto put32 = [&](uint32_t v) { put16(static_cast<uint16_t>(v)); put16(static_cast<uint16_t>(v >> 16)); };
  auto put64 = [&](uint64_t v) { put32(static_cast<uint32_t>(v)); put32(static_cast<uint32_t>(v >> 32)); };
  put32(r.island_id);
  put16(static_cast<uint16_t>(r.patch_ix));
  put16(static_cast<uint16_t>(r.patch_iz));
  put64(r.hps_page_addr);
  put32(r.expected_page_crc32c);
  put16(r.flags);
  put8(r.view_mask);
  put8(r.priority);
  put32(r.source_id);
  put32(r.reserved);
}

// ===========================================================================
// THE CANONICAL ORDER -- THIS IS THE DETERMINISM (T5)
// ===========================================================================
// "required before prefetch; smaller priority first; view-union key;
//  island_id asc; patch_iz asc; patch_ix asc; source_id asc."
//
// Six of the seven keys are unambiguous. "VIEW-UNION KEY" IS NOT, and this
// model does not pretend otherwise: it sorts `view_mask` ascending and the
// contract records the ambiguity as an open question. Ascending mask puts
// view-0-only (0b01) before view-1-only (0b10) before DUAL (0b11), which at
// least groups by owning view and keeps DUAL contiguous. A ruling that wants
// DUAL first is one comparator line away.
inline bool canonical_less(const PatchRecord& a, const PatchRecord& b) {
  const int ar = (a.flags & kFlagRequired) ? 0 : 1;
  const int br = (b.flags & kFlagRequired) ? 0 : 1;
  if (ar != br) return ar < br;
  if (a.priority != b.priority) return a.priority < b.priority;
  if (a.view_mask != b.view_mask) return a.view_mask < b.view_mask;
  if (a.island_id != b.island_id) return a.island_id < b.island_id;
  if (a.patch_iz != b.patch_iz) return a.patch_iz < b.patch_iz;
  if (a.patch_ix != b.patch_ix) return a.patch_ix < b.patch_ix;
  return a.source_id < b.source_id;
}

// ===========================================================================
// THE SEALED LIST
// ===========================================================================
struct SealedList {
  std::vector<PatchRecord> records;
  std::vector<uint8_t> bytes;  // records.size() * 32, canonical order
  uint32_t list_crc32c = 0;
  uint16_t patch_count = 0;
  uint8_t view_mask = 0;  // union of every record's mask
  bool sealed = false;
};

// T5's SubmitTerrainSet, 32 bytes. Present so the seam the contract describes
// is a thing that exists rather than a table in prose.
struct SubmitTerrainSet {
  uint32_t resource_epoch = 0;
  uint32_t list_offset = 0;
  uint32_t list_bytes = 0;
  uint32_t list_crc32c = 0;
  uint16_t patch_count = 0;
  uint8_t view_mask = 0;
  uint8_t flags = 0;
  uint32_t sequence = 0;
  uint32_t reserved0 = 0;
  uint32_t reserved1 = 0;
};

// T5's TerrainEpoch, 16 bytes.
enum class EpochOp : uint8_t { kBegin = 0, kEndFlush = 1, kAbort = 2 };

// ===========================================================================
// WORKING-SET CLASSES (T7's order, as an ordinal so "strongest wins" is a min)
// ===========================================================================
enum class Cls : uint8_t {
  kGameplay = 0,         // "explicitly gameplay-required patches" (T7)
  kRequiredCurrent = 1,  // "current visible set for both views"
  kPredicted = 2,        // "predicted visible set 30 frames ahead"
  kRing = 3,             // "one-patch Moore ring around it"
};

inline uint8_t priority_of(Cls c) {
  switch (c) {
    case Cls::kGameplay: return kPriorityGameplay;
    case Cls::kRequiredCurrent: return kPriorityRequiredCurrent;
    case Cls::kPredicted: return kPriorityPredicted;
    default: return kPriorityRing;
  }
}

inline bool is_required(Cls c) {
  return c == Cls::kGameplay || c == Cls::kRequiredCurrent;
}

// ===========================================================================
// A CAMERA
// ===========================================================================
// Velocity is in Q10 patches per frame -- 1024 == one patch per frame -- so
// the 30-frame prediction is integer arithmetic with no host float in it.
//
// FLOOR, NOT TRUNCATE-TOWARD-ZERO. Truncation toward zero makes the predicted
// centre lag by up to one patch when travelling in the negative direction and
// not at all in the positive one, i.e. a WORKING SET THAT DEPENDS ON WHICH WAY
// THE PLAYER FACES. That is a determinism bug shaped exactly like a rounding
// preference, so the shift is arithmetic and the asymmetry is gone.
struct Camera {
  island::View view;             // centre + radius in patches
  int32_t vel_ix_q10 = 0;        // Q10 patches/frame, +x
  int32_t vel_iz_q10 = 0;        // Q10 patches/frame, +z
  uint8_t view_bit = 1;          // 1 for view 0, 2 for view 1 (Duo)
};

inline island::View predicted_view(const Camera& c, int32_t frames = kPredictFrames) {
  island::View p = c.view;
  p.centre_ix += static_cast<int32_t>((static_cast<int64_t>(c.vel_ix_q10) * frames) >> 10);
  p.centre_iz += static_cast<int32_t>((static_cast<int64_t>(c.vel_iz_q10) * frames) >> 10);
  return p;
}

// ===========================================================================
// STAGING -- HPS DDR, whole pages, bounds validated BEFORE the copy
// ===========================================================================
enum class StageVerdict : uint8_t {
  kOk = 0,
  kSourceOutsideCartridge = 1,  // T12: validate bounds BEFORE staging
  kDestOutsideStaging = 2,      // the staging arena is finite and says so
  kIncomplete = 3,              // fewer than 21,376 bytes arrived
  kCrcMismatch = 4,             // payload disagrees with the declared CRC
  kKeyUnrepresentable = 5,      // island::Streamer::key_fits said no
  kStagingFull = 6,             // no free staging slot; see kStagingSlots
};

// THE STAGING AREA'S SIZE IS NOT RULED ANYWHERE, AND THIS IS THE KNOB THAT
// SAYS SO. `design/contracts/SW.STREAM.md` lists it among the four things no
// ruling decides. A model that allocated without a ceiling would make an
// unruled resource look infinite; one that hard-codes a guess would make a
// guess look ruled. So it is a parameter, it refuses when exhausted, and the
// refusal is counted.
constexpr uint32_t kStagingSlotsDefault = 512;

// What the cartridge claims about one page, before anybody trusts it.
struct PageSource {
  uint64_t cart_offset = 0;
  uint32_t declared_crc32c = 0;
  uint32_t source_id = 0;
  // Test/injection hooks: the two ways staging fails after bounds pass.
  bool inject_incomplete = false;
  uint32_t inject_actual_crc = 0;  // 0 means "matches declared"
};

// ===========================================================================
// COUNTERS
// ===========================================================================
// Every one of these is a number a picture cannot show. CLAUDE.md, 2026-09-05:
// "A test that checks WHAT came out cannot see HOW MANY TIMES the machine did
// it, and throughput budgets are written against the second number."
struct Ledger {
  uint32_t frames = 0;
  uint32_t candidates_examined = 0;   // before dedup, summed over views/classes
  uint32_t candidates_unique = 0;     // after the view union (T7) dedups them
  uint32_t dual_patches = 0;          // wanted by BOTH views
  uint32_t already_resident = 0;      // cost no bandwidth, consumed no budget
  uint32_t loads_planned = 0;         // pages the sealed lists asked to move
  uint32_t prefetch_deferred = 0;     // legal (T12): prefetch may be deferred
  uint32_t required_deferred = 0;     // required, over budget -> proxy this frame
  uint32_t gameplay_required_deferred = 0;  // UNRULED. See kPriorityGameplay.
  uint32_t proxy_patches = 0;         // T7: declared proxy + record the miss
  uint32_t staged_ok = 0;
  uint32_t staged_reused = 0;         // already in the arena; no copy needed
  uint32_t refused_source_bounds = 0;
  uint32_t refused_staging_full = 0;
  uint32_t staged_incomplete = 0;
  uint32_t staged_crc_fail = 0;
  uint32_t refused_key = 0;
  uint32_t lists_sealed = 0;
  uint32_t list_bytes_sealed = 0;
  uint32_t seal_mutations_refused = 0;  // T12: a sealed REQUIRED list is frozen
  uint32_t f_journalled = 0;
  uint32_t f_acked = 0;
  uint32_t f_restored = 0;
  uint32_t budget_exhausted_frames = 0;
};

// One frame's outcome.
struct Frame {
  SealedList list;
  SubmitTerrainSet submit;
  uint32_t loads_planned = 0;

  // Records whose page was ALREADY there. Present so the caller can assert the
  // list's only invariant that matters: `patch_count == loads_planned +
  // already_resident`. Every record in a sealed list is either a page being
  // moved this frame or a page already in memory; a record that is neither is
  // a promise the frame cannot keep, and it is invisible in every other
  // number the block reports.
  uint32_t already_resident = 0;
  uint32_t required_deferred = 0;
  uint32_t prefetch_deferred = 0;
  uint32_t gameplay_required_deferred = 0;

  // REFUSE LOUDLY RATHER THAN INVENT. Set when a patch the game declared
  // gameplay-required could not be loaded inside T7's budget. T7 says an
  // ordinary streaming miss renders the declared proxy and does NOT freeze the
  // frame; it says nothing about a patch the player is standing on, and the
  // consequence of getting that wrong is a player falling through the world.
  // The model reports the condition; it does not choose the behaviour.
  bool unruled_gameplay_starvation = false;
};

// ===========================================================================
// THE MODEL
// ===========================================================================
class WorldStreamer {
 public:
  WorldStreamer(const island::Directory& dir, terrain::Journal& journal)
      : dir_(dir), journal_(journal) {}

  // The cartridge the pages come from, and the HPS DDR staging arena they land
  // in. Both are GuardRegions so the bound check is `zref::mem`'s, not a fresh
  // comparison written here.
  void configure(const mem::GuardRegion& cartridge, const mem::GuardRegion& staging,
                 uint32_t resource_epoch,
                 uint32_t staging_slots = kStagingSlotsDefault) {
    cartridge_ = cartridge;
    staging_ = staging;
    epoch_ = resource_epoch;
    // The arena cannot promise more whole pages than it has room for. Taking
    // the MINIMUM rather than trusting the argument means a caller who asks
    // for 512 slots in a 64-page arena gets 64, not 448 addresses outside it.
    const uint32_t fits = staging.bytes / kPageBytes;
    const uint32_t n = staging_slots < fits ? staging_slots : fits;
    slot_.clear();
    free_slots_.clear();
    for (uint32_t i = 0; i < n; ++i) free_slots_.insert(i);
  }

  // A page leaves the staging arena when nothing wants it any more. Freeing is
  // the caller's call because "when is a staged page cold" is a policy no
  // ruling states; the model provides the mechanism and counts the pressure.
  void release_staging(int32_t ix, int32_t iz) {
    const auto it = slot_.find({ix, iz});
    if (it == slot_.end()) return;
    free_slots_.insert(it->second);
    slot_.erase(it);
  }

  std::size_t staged_pages() const { return slot_.size(); }
  std::size_t free_staging_slots() const { return free_slots_.size(); }

  void set_budget(uint32_t pages_per_frame) { budget_ = pages_per_frame; }
  uint32_t budget() const { return budget_; }

  // Register where one patch's page lives in the cartridge. A patch with no
  // source cannot be staged and is skipped -- it is not an error, it is a
  // directory entry whose bytes have not been authored.
  void set_source(int32_t ix, int32_t iz, const PageSource& s) { sources_[{ix, iz}] = s; }

  // What the game says it must have this frame regardless of what is visible
  // (T7's fourth working-set member).
  void set_gameplay_required(const std::set<std::pair<int32_t, int32_t>>& g) {
    gameplay_ = g;
  }

  // -----------------------------------------------------------------------
  // ONE FRAME.
  // -----------------------------------------------------------------------
  Frame build_frame(const std::vector<Camera>& cams, Ledger* L = nullptr) {
    Frame f;
    if (L) ++L->frames;

    // --- 1. the working set, per T7, IN T7's ORDER -----------------------
    // "Union the views before deduplication" -- so every view contributes to
    // every class first, and the merge below is what deduplicates. Doing it
    // the other way (dedup per view, then union) loses the DUAL flag, because
    // a patch's second claimant is exactly the information being thrown away.
    std::map<std::pair<int32_t, int32_t>, Cand> want;

    for (const Camera& c : cams) {
      for (const island::Visible& p : island::visible_set(dir_, c.view))
        offer(want, p, Cls::kRequiredCurrent, c.view_bit, L);
    }
    for (const Camera& c : cams) {
      for (const island::Visible& p : island::visible_set(dir_, predicted_view(c)))
        offer(want, p, Cls::kPredicted, c.view_bit, L);
    }
    // The Moore ring is around the VISIBLE SET, not around the window: T7 says
    // "a one-patch Moore ring around IT", and "it" is the current visible set.
    // On a sparse island those differ -- a window's ring includes cells no
    // ground is adjacent to. Ringing the set keeps the prefetch proportional
    // to the ground rather than to the window's area.
    for (const Camera& c : cams) {
      for (const island::Visible& p : island::visible_set(dir_, c.view)) {
        for (int32_t dz = -kRingRadius; dz <= kRingRadius; ++dz)
          for (int32_t dx = -kRingRadius; dx <= kRingRadius; ++dx) {
            if (dx == 0 && dz == 0) continue;
            const island::Lookup r = dir_.find(p.ix + dx, p.iz + dz);
            if (r.outcome != island::Outcome::kResident) continue;
            island::Visible n{p.ix + dx, p.iz + dz, r.page_handle};
            offer(want, n, Cls::kRing, c.view_bit, L);
          }
      }
    }
    // Gameplay-required last, so it upgrades whatever class a patch already
    // has rather than being outranked by it (the merge keeps the MINIMUM).
    for (const auto& k : gameplay_) {
      const island::Lookup r = dir_.find(k.first, k.second);
      if (r.outcome != island::Outcome::kResident) continue;
      island::Visible n{k.first, k.second, r.page_handle};
      offer(want, n, Cls::kGameplay, /*view_bit=*/0, L);
    }

    if (L) L->candidates_unique += static_cast<uint32_t>(want.size());

    // --- 2. records, still unsorted --------------------------------------
    const uint32_t island_id = dir_.desc().island_id;
    std::vector<PatchRecord> recs;
    std::vector<Cls> cls;
    recs.reserve(want.size());
    for (const auto& kv : want) {
      const Cand& c = kv.second;
      if (!island::Streamer::key_fits(island_id, kv.first.first, kv.first.second)) {
        // T1's key is exact, not hashed; a coordinate the key cannot express
        // would ALIAS onto another patch, which is a wrong-ground bug that
        // renders perfectly. Refuse it.
        if (L) ++L->refused_key;
        continue;
      }
      const auto it = sources_.find(kv.first);
      if (it == sources_.end()) continue;  // no authored bytes; not an error

      PatchRecord r;
      r.island_id = island_id;
      r.patch_ix = static_cast<int16_t>(kv.first.first);
      r.patch_iz = static_cast<int16_t>(kv.first.second);
      r.expected_page_crc32c = it->second.declared_crc32c;
      r.flags = static_cast<uint16_t>(is_required(c.cls) ? kFlagRequired : kFlagPrefetch);
      if (c.view_mask == 0x3) { r.flags |= kFlagDual; if (L) ++L->dual_patches; }
      if (journal_.has(island::Streamer::resource_index(island_id, kv.first.first,
                                                        kv.first.second)))
        r.flags |= kFlagHasSavedF;
      r.view_mask = c.view_mask;
      r.priority = priority_of(c.cls);
      r.source_id = it->second.source_id;
      recs.push_back(r);
      cls.push_back(c.cls);
    }

    // --- 3. canonical order BEFORE the budget cut ------------------------
    // The cut has to be deterministic, so it is taken in the same order the
    // list will be sealed in. Budgeting first and sorting after would make the
    // surviving 32 depend on map iteration order, which is a determinism bug
    // that a single-frame comparison cannot see.
    std::vector<std::size_t> ord(recs.size());
    for (std::size_t i = 0; i < ord.size(); ++i) ord[i] = i;
    stable_sort_indices(recs, ord);

    // --- 4. the budget ---------------------------------------------------
    uint32_t spent = 0;
    std::vector<PatchRecord> kept;
    for (std::size_t i : ord) {
      PatchRecord r = recs[i];
      const std::pair<int32_t, int32_t> k{r.patch_ix, r.patch_iz};
      const bool resident = resident_.find(k) != resident_.end();

      if (resident) {
        // Costs no bandwidth, so it costs no budget -- and it still belongs in
        // the list, because the list is what the frame draws from. Its page is
        // still in the staging arena, so the record still names where.
        r.hps_page_addr = staged_addr(k);
        ++f.already_resident;
        if (L) ++L->already_resident;
        kept.push_back(r);
        continue;
      }
      if (spent >= budget_) {
        if (r.flags & kFlagRequired) {
          // T7: a normal streaming miss uses the island's declared proxy and
          // RECORDS THE MISS. It explicitly does NOT freeze the old camera
          // frame -- only hard internal overflow/corruption does that, and a
          // camera outrunning the bandwidth is neither.
          //
          // T6's frame-fault is a DIFFERENT pressure (more than 256 required
          // DYNAMIC patches after legal degradation, in the composed cache).
          // Reusing it here would fault frames the rulings say to render.
          ++f.required_deferred;
          if (L) { ++L->required_deferred; ++L->proxy_patches; }
          if (r.priority == kPriorityGameplay) {
            ++f.gameplay_required_deferred;
            f.unruled_gameplay_starvation = true;
            if (L) ++L->gameplay_required_deferred;
          }
        } else {
          // T12, verbatim: "SW.STREAM may defer PREFETCH records."
          ++f.prefetch_deferred;
          if (L) ++L->prefetch_deferred;
        }
        continue;
      }

      // --- 5. stage, and only a COMPLETE page earns a record -------------
      const auto sit = sources_.find(k);
      const StageVerdict v = ensure_staged(k, sit->second, L);
      if (v == StageVerdict::kOk) {
        r.hps_page_addr = staged_addr(k);
        kept.push_back(r);
        ++spent;
        ++f.loads_planned;
        if (L) ++L->loads_planned;
      } else {
        // A page that did not stage completely renders as proxy, exactly like
        // one that did not fit the budget. It is NOT in the list, so nothing
        // downstream can publish it -- which is what makes T7's "a half-loaded
        // or CRC-failed page is never rendered" a property of the system.
        if (L) ++L->proxy_patches;
      }
    }
    if (spent >= budget_ && L) ++L->budget_exhausted_frames;

    // --- 6. seal ---------------------------------------------------------
    f.list = seal(kept, L);
    f.submit.resource_epoch = epoch_;
    f.submit.list_bytes = static_cast<uint32_t>(f.list.bytes.size());
    f.submit.list_crc32c = f.list.list_crc32c;
    f.submit.patch_count = f.list.patch_count;
    f.submit.view_mask = f.list.view_mask;
    f.submit.sequence = ++sequence_;
    return f;
  }

  // -----------------------------------------------------------------------
  // THE SEAL IS A ONE-WAY DOOR (T12).
  // -----------------------------------------------------------------------
  // "SW.STREAM may defer PREFETCH records but may not mutate a sealed REQUIRED
  //  list. If staging cannot meet the deadline it selects proxy/fallback
  //  BEFORE sealing. Not after."
  //
  // Modelled as a REFUSAL WITH A COUNTER rather than as an ordering the caller
  // is trusted to respect -- the same reason zref_fjournal.hpp gives for the F
  // barrier. Returns false and counts; the list is unchanged.
  bool try_mutate_after_seal(SealedList& l, Ledger* L = nullptr) {
    if (l.sealed) {
      if (L) ++L->seal_mutations_refused;
      return false;
    }
    return true;
  }

  // -----------------------------------------------------------------------
  // Consumed counters (T12): the hardware tells us what actually happened.
  // "Consumed to steer the NEXT frame's set, never to mutate a list already
  //  sealed."
  // -----------------------------------------------------------------------
  void note_load_complete(int32_t ix, int32_t iz, bool ok) {
    if (ok) resident_.insert({ix, iz});
  }

  // T4's barrier, HPS side. SW.STREAM owns the journal; the FPGA slot state
  // machine is zref::terrain::Streamer's and is deliberately not duplicated.
  void note_dirty_evict(int32_t ix, int32_t iz, const std::vector<int16_t>& f_sheet,
                        Ledger* L = nullptr) {
    journal_.write(island::Streamer::resource_index(dir_.desc().island_id, ix, iz), f_sheet);
    if (L) ++L->f_journalled;
  }
  void note_journal_ack(Ledger* L = nullptr) { if (L) ++L->f_acked; }

  void note_evicted(int32_t ix, int32_t iz) { resident_.erase({ix, iz}); }

  const std::vector<int16_t>* saved_f(int32_t ix, int32_t iz, Ledger* L = nullptr) const {
    const std::vector<int16_t>* p =
        journal_.read(island::Streamer::resource_index(dir_.desc().island_id, ix, iz));
    if (p && L) ++L->f_restored;
    return p;
  }

  std::size_t believed_resident() const { return resident_.size(); }

 private:
  struct Cand {
    Cls cls = Cls::kRing;
    uint8_t view_mask = 0;
  };

  // The merge: strongest class wins, view masks OR together. THIS is "union
  // the views before deduplication" (T7) -- the union is the OR, and it
  // happens before anything is dropped.
  void offer(std::map<std::pair<int32_t, int32_t>, Cand>& want, const island::Visible& p,
             Cls c, uint8_t view_bit, Ledger* L) {
    if (L) ++L->candidates_examined;
    Cand& e = want[{p.ix, p.iz}];
    if (e.view_mask == 0 || static_cast<uint8_t>(c) < static_cast<uint8_t>(e.cls))
      e.cls = c;
    e.view_mask = static_cast<uint8_t>(e.view_mask | view_bit);
  }

  void stable_sort_indices(const std::vector<PatchRecord>& recs, std::vector<std::size_t>& ord) {
    // Insertion by the canonical comparator. Deliberately not std::sort: the
    // order IS the contract, and a comparator bug that std::sort papers over
    // with an unstable tie is exactly the class of defect this exists to
    // exclude. The list is at most a few hundred records per frame.
    for (std::size_t i = 1; i < ord.size(); ++i) {
      const std::size_t v = ord[i];
      std::size_t j = i;
      while (j > 0 && canonical_less(recs[v], recs[ord[j - 1]])) { ord[j] = ord[j - 1]; --j; }
      ord[j] = v;
    }
  }

  uint64_t staged_addr(const std::pair<int32_t, int32_t>& k) const {
    const auto it = slot_.find(k);
    if (it == slot_.end()) return 0;
    return staging_.base + static_cast<uint64_t>(it->second) * kPageBytes;
  }

  // Stage one whole page into HPS DDR, or say exactly why not.
  //
  // THE ORDER OF THESE TESTS IS THE LAW, and it is `zref::mem`'s order, not a
  // new one: source bounds are checked BEFORE anything is allocated, and
  // allocation happens BEFORE any byte moves. T12: "validate cartridge and
  // resource bounds before staging ... nothing partially validated reaches the
  // staging area." A page that fails halfway leaves no slot behind.
  StageVerdict ensure_staged(const std::pair<int32_t, int32_t>& k, const PageSource& s,
                             Ledger* L) {
    if (!mem::upload_source_in_arena(cartridge_, cartridge_.base + s.cart_offset, kPageBytes)) {
      if (L) ++L->refused_source_bounds;
      return StageVerdict::kSourceOutsideCartridge;
    }
    if (slot_.find(k) != slot_.end()) {
      // Already whole in the arena. The FPGA still has to fetch it -- that is
      // the caller's budget unit -- but no cartridge copy is repeated.
      if (L) ++L->staged_reused;
      return StageVerdict::kOk;
    }
    if (free_slots_.empty()) {
      if (L) ++L->refused_staging_full;
      return StageVerdict::kStagingFull;
    }
    // Only now do bytes move.
    if (s.inject_incomplete) {
      if (L) ++L->staged_incomplete;
      return StageVerdict::kIncomplete;
    }
    const uint32_t actual = s.inject_actual_crc ? s.inject_actual_crc : s.declared_crc32c;
    if (actual != s.declared_crc32c) {
      if (L) ++L->staged_crc_fail;
      return StageVerdict::kCrcMismatch;
    }
    // The page is COMPLETE and verified. Only now does it get an address that
    // anything else can name -- which is what "staged complete before
    // publication" means on this side of the seam.
    const uint32_t idx = *free_slots_.begin();
    free_slots_.erase(free_slots_.begin());
    slot_[k] = idx;
    if (L) ++L->staged_ok;
    return StageVerdict::kOk;
  }

  SealedList seal(const std::vector<PatchRecord>& recs, Ledger* L) {
    SealedList l;
    l.records = recs;
    l.bytes.resize(recs.size() * kRecordBytes);
    for (std::size_t i = 0; i < recs.size(); ++i) {
      encode_record(recs[i], &l.bytes[i * kRecordBytes]);
      l.view_mask = static_cast<uint8_t>(l.view_mask | recs[i].view_mask);
    }
    l.patch_count = static_cast<uint16_t>(recs.size());
    l.list_crc32c = l.bytes.empty()
                        ? zhao_abi::zhao_crc32c(0, nullptr, 0)
                        : zhao_abi::zhao_crc32c(0, l.bytes.data(),
                                                static_cast<uint32_t>(l.bytes.size()));
    l.sealed = true;
    if (L) { ++L->lists_sealed; L->list_bytes_sealed += static_cast<uint32_t>(l.bytes.size()); }
    return l;
  }

  const island::Directory& dir_;
  terrain::Journal& journal_;
  mem::GuardRegion cartridge_{0, 0};
  mem::GuardRegion staging_{0, 0};
  uint32_t epoch_ = 1;
  uint32_t budget_ = kPageBudgetPerFrame;
  uint32_t sequence_ = 0;
  std::map<std::pair<int32_t, int32_t>, uint32_t> slot_;  // patch -> staging slot
  std::set<uint32_t> free_slots_;
  std::map<std::pair<int32_t, int32_t>, PageSource> sources_;
  std::set<std::pair<int32_t, int32_t>> resident_;
  std::set<std::pair<int32_t, int32_t>> gameplay_;
};

}  // namespace swstream
}  // namespace zref

#endif  // ZREF_SW_STREAM_HPP
