// zref_terrain_seq.hpp -- TERRAIN.SEQ, the terrain command sequencer.
//
// Law: design/contracts/TERRAIN.SEQ.md
//      reports/OWNER-RULINGS-BUILDABILITY-20260902.md  T5, T6, T7, T4, T9/T10
//      reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md     2.6, 3, 4
//
// ===========================================================================
// WHAT THIS IS, AND WHAT IT DELIBERATELY IS NOT
// ===========================================================================
// `reports/Missingterrain` names the hole in the owner's own words:
//
//     "Nothing currently does: camera moved -> inspect island directory ->
//      determine visible patch coordinates -> union the two players' working
//      sets -> prefetch missing pages -> allocate local-SDRAM residency slots
//      -> preserve dirty scars from evicted pages -> issue all visible patches
//      to the terrain engine."
//
// The first half of that sentence is now built and verified elsewhere:
// `zref::island::visible_set` determines the coordinates, and
// `zref::swstream::WorldStreamer` unions the views, applies T7's prefetch
// policy and SEALS the list. THIS model is the second half -- the pump that
// walks the sealed list and drives residency, writeback, load, compose-slot
// allocation and issue, in list order, for one frame.
//
// ===========================================================================
// IT COMPOSES THE RECORD. IT DOES NOT REDEFINE IT.
// ===========================================================================
// The input is `zref::swstream::PatchRecord` and `zref::swstream::PatchFlags`
// verbatim -- T5's 32-byte record, already written down once, already sorted
// by `zref::swstream::canonical_less`, already serialised by
// `zref::swstream::encode_record`. A second declaration of those fields here
// would be a second thing to keep in step with T5, and its first divergence
// would be silent: a flag bit read at the wrong offset draws the wrong ground
// in the right place. The sibling lane EXTRACTED `visible_set` out of
// `Streamer::update` for exactly this reason. Extending that discipline is
// cheaper than repeating the lesson.
//
// So the only law this file adds is the SEQUENCING law: given a record and
// what the directory answered about it, what does the machine do next.
//
// ===========================================================================
// THE ORACLE IS THE SEQUENCE, NOT THE STORE
// ===========================================================================
// `step()` takes the residency's ANSWER as an argument rather than modelling a
// directory and deriving it. That is deliberate and it is the same boundary
// TERRAIN.COMPCACHE draws ("the oracle is the STORE's contents and addressing,
// not the composition").
//
// A model that owned its own set-associative directory would have to agree
// with `zhao_terrain_residency_v2` about victim choice, generation bumps and
// pin accounting before it could say anything at all about sequencing -- and
// the first disagreement would be reported as a SEQUENCING defect. Feeding
// both the model and the RTL the identical answer stream removes that entire
// failure class: whatever the directory says, the two must do the same thing
// with it. TERRAIN.RESIDENCY has its own differential and is UNIT_VERIFIED;
// this one is about the pump.
#ifndef ZREF_TERRAIN_SEQ_HPP
#define ZREF_TERRAIN_SEQ_HPP

#include <cstdint>

#include "zref/zref_sw_stream.hpp"

namespace zref {
namespace terrain {
namespace seq {

// ===========================================================================
// BUDGETS -- both are ruled numbers, and both are knobs with a citation
// ===========================================================================

// T6: "The 256 slots are for patches needing LIVE COMPOSED height/velocity this
// frame, NOT a cap on all visible terrain." A static/baked visible page renders
// from its resident page layers and consumes no slot.
inline constexpr uint32_t kComposeSlots = 256;

// T7's page ceiling, taken from the block that owns the policy rather than
// restated as a literal here. SW.STREAM decides what to defer BEFORE sealing;
// this is the hardware backstop that keeps a malformed or hostile list from
// asking the loader for four hundred pages in one frame.
inline constexpr uint32_t kLoadBudgetPerFrame = swstream::kPageBudgetPerFrame;

// ===========================================================================
// WHAT THE DIRECTORY ANSWERED ABOUT ONE RECORD
// ===========================================================================
// One struct carries both transactions -- the lookup and, on a miss, the claim
// -- because a record generates at most one of each and pairing them keeps the
// oracle a pure function of (record, answer).
struct ResAnswer {
  // ---- lookup ----
  bool hit = false;      // RESIDENT_CLEAN or RESIDENT_DIRTY_F
  uint16_t slot = 0;
  uint8_t gen = 0;

  // ---- claim (only consulted on a lookup miss) ----
  bool claim_refused = false;  // T9 rule 5: every way in the set is pinned
  bool claim_same = false;     // the entry was already present: no new generation
  uint16_t claim_slot = 0;
  uint8_t claim_gen = 0;
  bool evicted_dirty = false;  // T4: the victim's F sheet must be written back
  uint32_t ev_island = 0;
  int16_t ev_ix = 0;
  int16_t ev_iz = 0;
  uint8_t ev_gen = 0;
};

// ===========================================================================
// WHAT HAPPENED TO ONE RECORD
// ===========================================================================
enum class Disp : uint8_t {
  kIssued = 0,              // resident and required: drawn this frame
  kPrefetchResident = 1,    // prefetch-only record, page already there: nothing to do
  kSkippedNotResident = 2,  // miss: work was requested for a later frame, not drawn now
  kFaulted = 3,             // THIS record exhausted the composed cache (T6)
  kDrained = 4,             // consumed after the frame had already faulted
};

// The actions the record produced, in the order the hardware performs them.
struct Step {
  Disp disp = Disp::kIssued;

  bool did_lookup = false;
  bool did_claim = false;
  bool did_writeback = false;  // strictly BEFORE did_load -- T4's barrier
  bool did_load = false;
  bool did_pin = false;
  bool did_issue = false;

  bool load_budget_deferred = false;  // T7 backstop tripped: NOT a fault
  bool claim_refused = false;
  bool claim_same = false;

  bool compose_slot_valid = false;
  uint16_t compose_slot = 0;
};

// ===========================================================================
// THE LEDGER -- EVERY FIELD COUNTS EVENTS, NOT CYCLES
// ===========================================================================
// Said here because a sibling block shipped two counters that counted cycles
// while their names claimed events, and one of them reported the producer's
// patience (115 offers over 1,783 cycles) rather than the refusals. Every
// number below increments on exactly one accepted handshake or one consumed
// record. None of them is a duration.
struct Ledger {
  uint32_t records_consumed = 0;  // records accepted off the sealed list
  uint32_t patches_issued = 0;    // patch jobs handed to the terrain engine
  uint32_t prefetch_resident = 0; // prefetch records whose page was already there
  uint32_t skipped_not_resident = 0;  // REQUIRED records whose page was missing
  uint32_t claims_issued = 0;
  uint32_t claims_refused = 0;    // T9 rule 5
  uint32_t claims_same = 0;       // lookup said miss, claim said already present
  uint32_t loads_issued = 0;
  uint32_t loads_deferred = 0;    // T7 backstop
  uint32_t writebacks_issued = 0; // T4
  uint32_t compose_slots_used = 0;
  uint32_t pins_issued = 0;
  uint32_t drained = 0;           // records consumed after a fault
  uint32_t frame_faults = 0;      // T6
};

// The identity T6 requires be recorded when a frame is rejected: "record
// rejected source IDs and keys."
struct Fault {
  bool active = false;
  uint32_t source_id = 0;
  uint32_t island_id = 0;
  int16_t patch_ix = 0;
  int16_t patch_iz = 0;
};

// ===========================================================================
// THE SEQUENCER
// ===========================================================================
class Sequencer {
 public:
  explicit Sequencer(uint32_t compose_slots = kComposeSlots,
                     uint32_t load_budget = kLoadBudgetPerFrame)
      : compose_slots_(compose_slots), load_budget_(load_budget) {}

  // FRAME-SCOPED, AND THE RESET IS THE WHOLE POINT.
  //
  // The composed-cache allocator carries NO history: slot n goes to the n-th
  // record of THIS frame that needs composition. Architecture 2.5 records the
  // rejected alternative -- a coordinate-hashed persistent cache would let an
  // unchanged patch skip re-composition across frames -- and rejects it
  // because terrain_rules 4.2's law is already "produced once per frame" and a
  // persistent cache adds cross-frame state that replay must reconstruct.
  //
  // The load budget resets here too, and for a different reason: T7's ceiling
  // is "32 whole pages PER FRAME", so carrying a remainder across the boundary
  // would make one frame's traffic depend on the last one's.
  void begin_frame(uint32_t resource_epoch) {
    epoch_ = resource_epoch;
    compose_next_ = 0;
    loads_this_frame_ = 0;
    fault_ = Fault{};
  }

  // One record. Pure function of (record, answer, frame state).
  Step step(const swstream::PatchRecord& r, const ResAnswer& a) {
    Step s;
    ++led_.records_consumed;

    // T6: "fault the frame, DRAIN, repeat the previous complete frame". The
    // rest of the list is still consumed -- a sealed list is a unit and
    // leaving half of it in the ring would strand the next frame's records
    // behind it -- but nothing more is issued.
    if (fault_.active) {
      s.disp = Disp::kDrained;
      ++led_.drained;
      return s;
    }

    // T5 sorts "required before prefetch". A record without REQUIRED is a
    // record the frame wants RESIDENT but does not want DRAWN, so it never
    // reaches the engine and never consumes a composed slot.
    const bool required = (r.flags & swstream::kFlagRequired) != 0;
    const bool dynamic = (r.flags & swstream::kFlagDynamic) != 0;

    s.did_lookup = true;

    if (a.hit) {
      if (!required) {
        s.disp = Disp::kPrefetchResident;
        ++led_.prefetch_resident;
        return s;
      }
      if (dynamic) {
        if (compose_next_ < compose_slots_) {
          s.compose_slot = static_cast<uint16_t>(compose_next_++);
          s.compose_slot_valid = true;
          ++led_.compose_slots_used;
        } else {
          // T6, verbatim: "If more than 256 REQUIRED dynamic patches remain
          // after legal degradation: fault the frame, drain, repeat the
          // previous complete frame, record rejected source IDs and keys."
          //
          // THE DEGRADATION LADDER IS NOT THIS BLOCK'S. T6's five steps are
          // SW.STREAM's to apply before sealing; architecture 2.6 says this
          // block must not "invent degrade policy". By the time a list is
          // sealed the legal degradation has happened, so an overflow here is
          // the case the ruling says to fault on and nothing else.
          fault_.active = true;
          fault_.source_id = r.source_id;
          fault_.island_id = r.island_id;
          fault_.patch_ix = r.patch_ix;
          fault_.patch_iz = r.patch_iz;
          ++led_.frame_faults;
          s.disp = Disp::kFaulted;
          return s;
        }
      }
      // Static/baked required patches take this path with no slot: T6's
      // "Static/baked visible pages render from resident page layers and
      // consume no dynamic slot."
      s.did_pin = true;
      ++led_.pins_issued;
      s.did_issue = true;
      ++led_.patches_issued;
      s.disp = Disp::kIssued;
      return s;
    }

    // ---- MISS ------------------------------------------------------------
    // Architecture 2.6: "A patch that is not resident when its turn comes is
    // SKIPPED and counted, not stalled on -- the frame must never wait on an
    // 80 microsecond page load mid-walk." So everything below arranges for a
    // LATER frame and this record draws nothing.
    s.disp = Disp::kSkippedNotResident;

    // Only a REQUIRED miss is a skip. A prefetch record that missed is a
    // prefetch record doing its job; counting it as a skip would bury the
    // number that means "ground the player should be seeing is not there"
    // under the number that means "the streamer is streaming".
    if (required) ++led_.skipped_not_resident;

    // T7's ceiling, checked BEFORE the claim. Claiming and then declining to
    // load would leave a slot in LOADING that nobody is filling.
    //
    // NOT A FAULT, and the distinction is load-bearing: T7's overflow is
    // proxy-and-continue, RECORDED; T6's is the frame fault. Conflating them
    // would fault frames the rulings say to render.
    if (loads_this_frame_ >= load_budget_) {
      s.load_budget_deferred = true;
      ++led_.loads_deferred;
      return s;
    }

    s.did_claim = true;
    ++led_.claims_issued;

    if (a.claim_refused) {
      // T9 rule 5: "all pinned: backpressure and count."
      s.claim_refused = true;
      ++led_.claims_refused;
      return s;
    }

    if (a.claim_same) {
      // The lookup said miss and the claim said the entry was already there.
      // That is a directory disagreeing with itself across two cycles, and it
      // is counted rather than smoothed over. The load still goes out: a slot
      // that was already present but did not answer a lookup is not a slot
      // this frame may draw from.
      s.claim_same = true;
      ++led_.claims_same;
    }

    if (a.evicted_dirty) {
      // T4: layer F has no canonical HPS mirror, so the victim's sheet must
      // reach the journal, and T10's rule is "no dirty_F reuse before
      // writeback ACK". The writeback job is emitted BEFORE the load job for
      // the same slot; the ordering is the barrier.
      s.did_writeback = true;
      ++led_.writebacks_issued;
    }

    s.did_load = true;
    ++led_.loads_issued;
    ++loads_this_frame_;
    return s;
  }

  const Ledger& ledger() const { return led_; }
  const Fault& fault() const { return fault_; }
  uint32_t compose_slots_allocated() const { return compose_next_; }
  uint32_t loads_this_frame() const { return loads_this_frame_; }
  uint32_t epoch() const { return epoch_; }

 private:
  uint32_t compose_slots_;
  uint32_t load_budget_;
  uint32_t epoch_ = 0;
  uint32_t compose_next_ = 0;
  uint32_t loads_this_frame_ = 0;
  Fault fault_{};
  Ledger led_{};
};

}  // namespace seq
}  // namespace terrain
}  // namespace zref

#endif  // ZREF_TERRAIN_SEQ_HPP
