// zref_hps_boundary.hpp — the HPS/runtime boundary as a protocol, not a favour.
// Authored 2026-09-05 for roadmap gate G2-C.
//
// ---------------------------------------------------------------------------
// THE PROBLEM THIS NAMES
// ---------------------------------------------------------------------------
// The roadmap is exact about what is wrong today:
//
//   > The shell currently receives ring state, lengths, decoded controllers and
//   > PCM through explicit harness-side ports. The production system needs
//   > actual descriptor handling, completion reporting, register access,
//   > software launch/teardown and a coherent staging arena -- not a C++
//   > testbench providing these values magically.
//
// A testbench that hands the shell a length has answered the question the
// hardware must answer for itself. Every such favour is a place where the
// simulation is easier than the machine, and those are exactly the places a
// board bring-up fails.
//
// ---------------------------------------------------------------------------
// WHAT IS HERE AND WHAT IS NOT
// ---------------------------------------------------------------------------
// Here: the DESCRIPTOR RING and the COMPLETION protocol -- submit, claim,
// complete, reap -- with ownership that is explicit at every step, plus the
// staging-arena discipline that makes "a userspace pointer is not a DMA
// address" a checkable rule rather than a warning.
//
// Not here: the MiSTer DDR adapter, arbiter clients, cache-coherency
// primitives, or anything that requires the board. Those are G7, and the
// roadmap says to test these interfaces against mocks BEFORE board access --
// which is what this makes possible.
//
// ---------------------------------------------------------------------------
// THE OWNERSHIP RULE
// ---------------------------------------------------------------------------
// A descriptor slot is owned by exactly one side at any moment, and the owner
// is derivable from its state -- never from a convention two implementations
// might read differently:
//
//     kFree      -> the ARM owns it; it may write a request here
//     kSubmitted -> the DEVICE owns it; the ARM must not touch it
//     kClaimed   -> the DEVICE owns it and is working
//     kComplete  -> the ARM owns it again; it must reap before reuse
//
// Every illegal transition is REFUSED and counted. A silently tolerated one is
// how a descriptor gets rewritten under a DMA that is still reading it.

#ifndef ZREF_HPS_BOUNDARY_HPP
#define ZREF_HPS_BOUNDARY_HPP

#include <cstdint>
#include <vector>

namespace zref {
namespace hps {

enum class SlotState : uint8_t {
  kFree = 0,
  kSubmitted = 1,
  kClaimed = 2,
  kComplete = 3,
};

enum class Fault : uint8_t {
  kNone = 0,
  kNotOwned = 1,          // the caller does not own this slot in this state
  kRingFull = 2,          // no free slot
  kUnalignedPointer = 3,  // a userspace pointer is not a DMA address
  kOutsideArena = 4,      // source is not inside the registered staging arena
  kEpochStale = 5,        // the epoch this descriptor belonged to has closed
};

struct Descriptor {
  uint64_t src = 0;      // HPS address, must be inside the staging arena
  uint32_t dst = 0;      // device-side destination
  uint32_t length = 0;
  uint16_t epoch = 0;
  uint32_t tag = 0;      // the ARM's own identifier, returned untouched
};

struct Completion {
  uint32_t tag = 0;
  bool ok = false;
  Fault fault = Fault::kNone;
};

struct Ledger {
  uint32_t submitted = 0;
  uint32_t claimed = 0;
  uint32_t completed = 0;
  uint32_t reaped = 0;
  uint32_t refused_not_owned = 0;
  uint32_t refused_ring_full = 0;
  uint32_t refused_unaligned = 0;
  uint32_t refused_outside_arena = 0;
  uint32_t refused_stale_epoch = 0;
};

struct Arena {
  uint64_t base = 0;
  uint64_t bytes = 0;
  bool contains(uint64_t addr, uint32_t len) const {
    // 64-bit throughout: `addr + len` in 32 bits is exactly the arithmetic
    // accident a containment check exists to refuse.
    if (len == 0) return false;
    return addr >= base && (addr + static_cast<uint64_t>(len)) <= (base + bytes);
  }
};

// The ring. Both sides drive it through this one object in the model; in the
// machine they are separated by a bus, which is precisely why the OWNERSHIP has
// to be explicit rather than implied by who happens to call first.
class Ring {
 public:
  Ring(int slots, Arena staging, uint32_t dma_alignment = 64)
      : staging_(staging), align_(dma_alignment) {
    state_.assign(static_cast<std::size_t>(slots), SlotState::kFree);
    desc_.resize(static_cast<std::size_t>(slots));
    result_.resize(static_cast<std::size_t>(slots));
  }

  // ---- ARM side ----------------------------------------------------------

  // Returns the slot index, or -1 with a fault. A refusal never occupies a
  // slot: a rejected descriptor that consumed a ring entry would turn a
  // producer bug into a deadlock.
  int submit(const Descriptor& d, uint16_t current_epoch, Ledger* L = nullptr) {
    // "A userspace pointer is not a DMA address." Alignment is checked before
    // anything else looks at the address.
    if ((d.src % align_) != 0 || (d.dst % align_) != 0 ||
        (d.length % align_) != 0) {
      if (L) L->refused_unaligned++;
      return fault_only(Fault::kUnalignedPointer);
    }
    if (!staging_.contains(d.src, d.length)) {
      if (L) L->refused_outside_arena++;
      return fault_only(Fault::kOutsideArena);
    }
    if (d.epoch != current_epoch) {
      if (L) L->refused_stale_epoch++;
      return fault_only(Fault::kEpochStale);
    }
    const int s = find(SlotState::kFree);
    if (s < 0) {
      if (L) L->refused_ring_full++;
      return fault_only(Fault::kRingFull);
    }
    desc_[static_cast<std::size_t>(s)] = d;
    state_[static_cast<std::size_t>(s)] = SlotState::kSubmitted;
    if (L) L->submitted++;
    last_fault_ = Fault::kNone;
    return s;
  }

  // The ARM reaps a completion and returns the slot to free. Reaping anything
  // not in kComplete is refused: it would either steal a live descriptor or
  // double-free one.
  bool reap(int slot, Completion* out, Ledger* L = nullptr) {
    if (!owned(slot, SlotState::kComplete)) {
      if (L) L->refused_not_owned++;
      last_fault_ = Fault::kNotOwned;
      return false;
    }
    if (out) *out = result_[static_cast<std::size_t>(slot)];
    state_[static_cast<std::size_t>(slot)] = SlotState::kFree;
    if (L) L->reaped++;
    return true;
  }

  // ---- device side --------------------------------------------------------

  int claim(Ledger* L = nullptr) {
    const int s = find(SlotState::kSubmitted);
    if (s < 0) return -1;
    state_[static_cast<std::size_t>(s)] = SlotState::kClaimed;
    if (L) L->claimed++;
    return s;
  }

  bool complete(int slot, bool ok, Fault fault, Ledger* L = nullptr) {
    if (!owned(slot, SlotState::kClaimed)) {
      if (L) L->refused_not_owned++;
      last_fault_ = Fault::kNotOwned;
      return false;
    }
    Completion c;
    c.tag = desc_[static_cast<std::size_t>(slot)].tag;
    c.ok = ok;
    c.fault = fault;
    result_[static_cast<std::size_t>(slot)] = c;
    state_[static_cast<std::size_t>(slot)] = SlotState::kComplete;
    if (L) L->completed++;
    return true;
  }

  // ---- inspection ---------------------------------------------------------
  SlotState state(int slot) const { return state_[static_cast<std::size_t>(slot)]; }
  const Descriptor& descriptor(int slot) const {
    return desc_[static_cast<std::size_t>(slot)];
  }
  Fault last_fault() const { return last_fault_; }
  int slots() const { return static_cast<int>(state_.size()); }
  int count(SlotState s) const {
    int n = 0;
    for (SlotState x : state_)
      if (x == s) ++n;
    return n;
  }

 private:
  int fault_only(Fault f) {
    last_fault_ = f;
    return -1;
  }
  int find(SlotState s) const {
    for (std::size_t i = 0; i < state_.size(); ++i)
      if (state_[i] == s) return static_cast<int>(i);
    return -1;
  }
  bool owned(int slot, SlotState required) const {
    return slot >= 0 && slot < static_cast<int>(state_.size()) &&
           state_[static_cast<std::size_t>(slot)] == required;
  }

  Arena staging_;
  uint32_t align_;
  std::vector<SlotState> state_;
  std::vector<Descriptor> desc_;
  std::vector<Completion> result_;
  Fault last_fault_ = Fault::kNone;
};

}  // namespace hps
}  // namespace zref

#endif  // ZREF_HPS_BOUNDARY_HPP
