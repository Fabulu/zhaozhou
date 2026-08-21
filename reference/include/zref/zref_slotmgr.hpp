// zref_slotmgr.hpp — VIDEO.SLOTMGR's reference model: who owns a framebuffer
// slot, and when.
//
// Design: reports/DEBUG.FRAMEBLIT_Integration_Corrections.md §9, Step 2.
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK HAD TO EXIST BEFORE ANYTHING COULD BE WIRED
// ---------------------------------------------------------------------------
// DEBUG.FRAMEBLIT writes speculatively into a slot and publishes only if
// everything went right. That is safe ONLY because the slot it writes to is not
// being looked at — and until now nothing in the machine actually decided that.
// The shell granted the guard a window at dispatch and cleared it at done, with
// no generation, no notion of DISPLAYED, and no way to refuse a stale event.
//
// This model is the missing authority. It answers exactly one question, and it
// is the question every other framebuffer rule reduces to:
//
//     WHO OWNS THIS SLOT RIGHT NOW, AND IS THAT STILL THE SAME OWNER?
//
// ---------------------------------------------------------------------------
// THE STATES
// ---------------------------------------------------------------------------
//     FREE      nobody owns it
//     WRITING   leased to a blit that has not finished
//     READY     a completed frame, waiting to be shown
//     DISPLAYED on screen
//
// and the only lawful transitions:
//
//     FREE      -> WRITING     a lease is granted        (generation++)
//     WRITING   -> READY       a MATCHING publication
//     WRITING   -> FREE        a MATCHING release
//     READY     -> DISPLAYED   frame control swaps to it
//     DISPLAYED -> FREE        another slot becomes displayed
//
// ---------------------------------------------------------------------------
// THE THREE LAWS THAT ARE NOT OBVIOUS
// ---------------------------------------------------------------------------
// 1. **THE GENERATION INCREMENTS ON EVERY ENTRY INTO WRITING**, not on every
//    lease request and not per frame. It is what makes a publication
//    attributable: an event carrying generation 41 is refused once the slot has
//    moved on to 42, even though the slot number is the same and the state is
//    the same. Without it a blit that lost its lease and a blit that never lost
//    it are indistinguishable at the moment they publish.
//
// 2. **A STALE EVENT CHANGES NOTHING — AND IS COUNTED.** Refusing it silently
//    would turn a lease bug into a frame that mysteriously never appears. The
//    count is the difference between "the machine is protecting itself" and
//    "something is wrong and nobody can tell".
//
// 3. **ONLY A `FREE` SLOT IS LEASABLE**, which is the single condition that
//    subsumes "not displayed, not READY, not already being written, not
//    committed to the next swap". Those are not four checks; they are four
//    names for `state != FREE`. Writing them separately is how one of them ends
//    up missing.
#pragma once

#include <cstdint>

namespace zref {
namespace video {

enum class SlotState : uint8_t {
  kFree = 0,
  kWriting = 1,
  kReady = 2,
  kDisplayed = 3,
};

/** Why an event was refused. Zero is the only acceptance. */
enum class SlotEvent : uint8_t {
  kAccepted = 0,
  kWrongState = 1,       // the slot was not WRITING
  kWrongGeneration = 2,  // an ABA re-grant, or a very late event
  kBadSlot = 3,          // slot index outside {0,1}
};

constexpr int kSlotCount = 2;

/**
 * The framebuffer slot manager.
 *
 * One clock domain owns this state. Frame control's swap arrives as a
 * synchronized event and the readiness it publishes leaves as one; nothing here
 * is split across domains, because a two-domain ownership machine is a race
 * with extra steps.
 */
class SlotManager {
 public:
  void reset() {
    for (int i = 0; i < kSlotCount; ++i) {
      state_[i] = SlotState::kFree;
      generation_[i] = 0;
    }
    displayed_valid_ = false;
    displayed_ = 0;
    lease_active_ = false;
    lease_slot_ = 0;
    lease_generation_ = 0;
    stale_events_ = 0;
    leases_granted_ = 0;
  }

  SlotManager() { reset(); }

  SlotState state(int slot) const { return state_[slot]; }
  uint16_t generation(int slot) const { return generation_[slot]; }
  bool ready(int slot) const { return state_[slot] == SlotState::kReady; }

  bool lease_active() const { return lease_active_; }
  uint8_t lease_slot() const { return lease_slot_; }
  uint16_t lease_generation() const { return lease_generation_; }

  uint32_t stale_events() const { return stale_events_; }
  uint32_t leases_granted() const { return leases_granted_; }

  /**
   * Ask for a lease.
   *
   * Law 3: a slot is leasable exactly when it is FREE. Only ONE lease exists at
   * a time in Phase 2 — there is one blitter — and granting a second while the
   * first is outstanding would put two slots in WRITING with one writer, which
   * is a bookkeeping error rather than a capability.
   *
   * Returns true and fills `slot`/`generation` on success.
   */
  bool request_lease(uint8_t want_slot, uint8_t* slot, uint16_t* generation) {
    if (lease_active_) return false;
    if (want_slot >= kSlotCount) return false;
    if (state_[want_slot] != SlotState::kFree) return false;

    state_[want_slot] = SlotState::kWriting;
    ++generation_[want_slot];  // Law 1: every ENTRY into WRITING
    lease_active_ = true;
    lease_slot_ = want_slot;
    lease_generation_ = generation_[want_slot];
    ++leases_granted_;
    *slot = want_slot;
    *generation = lease_generation_;
    return true;
  }

  /** A completed blit asks for its slot to become READY. */
  SlotEvent publish(uint8_t slot, uint16_t generation) {
    const SlotEvent v = check(slot, generation);
    if (v != SlotEvent::kAccepted) {
      ++stale_events_;
      return v;
    }
    state_[slot] = SlotState::kReady;
    lease_active_ = false;
    return SlotEvent::kAccepted;
  }

  /** A failed blit hands its slot back. */
  SlotEvent release(uint8_t slot, uint16_t generation) {
    const SlotEvent v = check(slot, generation);
    if (v != SlotEvent::kAccepted) {
      ++stale_events_;
      return v;
    }
    state_[slot] = SlotState::kFree;
    lease_active_ = false;
    return SlotEvent::kAccepted;
  }

  /**
   * Frame control swapped to `slot`.
   *
   * Only a READY slot can become DISPLAYED. The slot that was displayed goes
   * FREE — which is the moment its buffer becomes reusable, and not one instant
   * earlier: freeing it when the new frame merely became READY would hand a
   * still-visible buffer to the next blit.
   */
  SlotEvent swap(uint8_t slot) {
    if (slot >= kSlotCount) return SlotEvent::kBadSlot;
    if (state_[slot] != SlotState::kReady) {
      ++stale_events_;
      return SlotEvent::kWrongState;
    }
    if (displayed_valid_ && displayed_ != slot) {
      state_[displayed_] = SlotState::kFree;
    }
    state_[slot] = SlotState::kDisplayed;
    displayed_valid_ = true;
    displayed_ = slot;
    return SlotEvent::kAccepted;
  }

  bool displayed_valid() const { return displayed_valid_; }
  uint8_t displayed() const { return displayed_; }

 private:
  SlotEvent check(uint8_t slot, uint16_t generation) const {
    if (slot >= kSlotCount) return SlotEvent::kBadSlot;
    if (state_[slot] != SlotState::kWriting) return SlotEvent::kWrongState;
    // Law 1/2: the generation is what makes the event attributable.
    if (generation_[slot] != generation) return SlotEvent::kWrongGeneration;
    return SlotEvent::kAccepted;
  }

  SlotState state_[kSlotCount];
  uint16_t generation_[kSlotCount];
  bool displayed_valid_;
  uint8_t displayed_;
  bool lease_active_;
  uint8_t lease_slot_;
  uint16_t lease_generation_;
  uint32_t stale_events_;
  uint32_t leases_granted_;
};

}  // namespace video
}  // namespace zref
