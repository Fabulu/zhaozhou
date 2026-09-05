// zref_fjournal.hpp — the F-sheet journal barrier.
// Authored 2026-09-05 for roadmap gate G2-D.
//
// ---------------------------------------------------------------------------
// WHY LAYER F IS DIFFERENT, AND WHY THIS IS A BARRIER
// ---------------------------------------------------------------------------
// `design/contracts/SW.STREAM.md`:
//
//   > **Layer F has no canonical HPS mirror** (T4). That is why the journal
//   > exists, and why the ordering below is a barrier and not an optimisation:
//   >
//   >     dirty eviction or explicit save
//   >       -> copy exactly F to the HPS terrain journal
//   >       -> WAIT for journal acknowledgement
//   >       -> only then may the slot enter LOADING
//   >       -> reload F from the journal when the page returns
//   >
//   > B and D are never written back on eviction: HPS keeps them current from
//   > the same deterministic commands, so the FPGA copy is a cache of something
//   > the HPS can always reproduce. **F is not, and losing it loses ground the
//   > player destroyed.**
//
// That last sentence is the whole reason this file exists. B and D can be
// recomputed; F cannot. A slot that enters LOADING before its F reaches the
// journal has destroyed a crater the player made, and no later check can tell
// that it happened -- the ground simply comes back flat.
//
// So `dirty_F` blocking LOADING is modelled as a REFUSAL WITH A COUNTER, not as
// an ordering the caller is trusted to respect.
//
// ---------------------------------------------------------------------------
// AND THE PERMISSION IS STATE-AWARE
// ---------------------------------------------------------------------------
//   > the region is deny-by-default in MEM.GUARD with state-aware permissions:
//   > a loader may write only a `LOADING` slot.
//
// Modelled here too, because "the loader only writes loading slots" is exactly
// the kind of rule that holds until someone adds a prefetch.

#ifndef ZREF_FJOURNAL_HPP
#define ZREF_FJOURNAL_HPP

#include <cstdint>
#include <vector>

namespace zref {
namespace terrain {

enum class SlotState : uint8_t {
  kFree = 0,
  kLoading = 1,
  kResident = 2,
  kEvicting = 3,   // F is being journalled; the slot is not reusable yet
};

enum class Refusal : uint8_t {
  kNone = 0,
  kDirtyFNotJournalled = 1,  // the barrier: F must reach the journal first
  kAwaitingAck = 2,          // journalled, but the ACK has not arrived
  kWriteToNonLoadingSlot = 3,
  kNoFreeSlot = 4,
};

struct Ledger {
  uint32_t journalled = 0;
  uint32_t acked = 0;
  uint32_t loads_started = 0;
  uint32_t f_reloaded = 0;
  uint32_t refused_dirty_f = 0;
  uint32_t refused_awaiting_ack = 0;
  uint32_t refused_write_non_loading = 0;
  uint32_t refused_no_slot = 0;
};

struct Slot {
  SlotState state = SlotState::kFree;
  uint32_t page_id = 0;
  bool dirty_f = false;
  bool f_journalled = false;   // copy issued
  bool f_acked = false;        // acknowledgement received
  std::vector<int16_t> f;      // the sheet itself
};

// The HPS side. It is a mirror ONLY for F, because F is the only layer with no
// other canonical source.
class Journal {
 public:
  void write(uint32_t page_id, const std::vector<int16_t>& f) {
    for (auto& e : entries_)
      if (e.first == page_id) {
        e.second = f;
        return;
      }
    entries_.emplace_back(page_id, f);
  }
  bool has(uint32_t page_id) const {
    for (const auto& e : entries_)
      if (e.first == page_id) return true;
    return false;
  }
  const std::vector<int16_t>* read(uint32_t page_id) const {
    for (const auto& e : entries_)
      if (e.first == page_id) return &e.second;
    return nullptr;
  }
  std::size_t size() const { return entries_.size(); }

 private:
  std::vector<std::pair<uint32_t, std::vector<int16_t>>> entries_;
};

class Streamer {
 public:
  explicit Streamer(int slots) { slots_.resize(static_cast<std::size_t>(slots)); }

  int find_resident(uint32_t page_id) const {
    for (std::size_t i = 0; i < slots_.size(); ++i)
      if (slots_[i].state != SlotState::kFree && slots_[i].page_id == page_id)
        return static_cast<int>(i);
    return -1;
  }

  // Begin evicting a slot. If F is dirty this issues the journal copy; the slot
  // does NOT become reusable here -- it enters kEvicting and waits for the ACK.
  Refusal begin_evict(int slot, Journal* j, Ledger* L = nullptr) {
    Slot& s = slots_[static_cast<std::size_t>(slot)];
    if (!s.dirty_f) {
      // B and D are never written back: the HPS can reproduce them from the
      // same deterministic commands.
      s.state = SlotState::kFree;
      return Refusal::kNone;
    }
    j->write(s.page_id, s.f);
    s.f_journalled = true;
    s.f_acked = false;
    s.state = SlotState::kEvicting;
    if (L) L->journalled++;
    return Refusal::kNone;
  }

  // The HPS acknowledges. Only now may the slot be reused.
  void ack(int slot, Ledger* L = nullptr) {
    Slot& s = slots_[static_cast<std::size_t>(slot)];
    if (!s.f_journalled) return;
    s.f_acked = true;
    s.dirty_f = false;
    s.state = SlotState::kFree;
    if (L) L->acked++;
  }

  // THE BARRIER. A slot with dirty_F cannot enter LOADING before the ACK.
  Refusal begin_load(int slot, uint32_t page_id, Ledger* L = nullptr) {
    Slot& s = slots_[static_cast<std::size_t>(slot)];
    if (s.dirty_f && !s.f_journalled) {
      if (L) L->refused_dirty_f++;
      return Refusal::kDirtyFNotJournalled;
    }
    if (s.f_journalled && !s.f_acked) {
      if (L) L->refused_awaiting_ack++;
      return Refusal::kAwaitingAck;
    }
    s.state = SlotState::kLoading;
    s.page_id = page_id;
    s.dirty_f = false;
    s.f_journalled = false;
    s.f_acked = false;
    s.f.clear();
    if (L) L->loads_started++;
    return Refusal::kNone;
  }

  // Deny-by-default, state-aware: the loader may write only a LOADING slot.
  Refusal loader_write(int slot, const std::vector<int16_t>& f, Ledger* L = nullptr) {
    Slot& s = slots_[static_cast<std::size_t>(slot)];
    if (s.state != SlotState::kLoading) {
      if (L) L->refused_write_non_loading++;
      return Refusal::kWriteToNonLoadingSlot;
    }
    s.f = f;
    return Refusal::kNone;
  }

  // F reloads exactly on return: if the journal holds this page, the reloaded
  // sheet is the journalled one, not a fresh flat sheet.
  Refusal finish_load(int slot, const Journal& j, Ledger* L = nullptr) {
    Slot& s = slots_[static_cast<std::size_t>(slot)];
    if (s.state != SlotState::kLoading) return Refusal::kWriteToNonLoadingSlot;
    if (const std::vector<int16_t>* saved = j.read(s.page_id)) {
      s.f = *saved;
      if (L) L->f_reloaded++;
    }
    s.state = SlotState::kResident;
    return Refusal::kNone;
  }

  void deform(int slot, std::size_t at, int16_t delta) {
    Slot& s = slots_[static_cast<std::size_t>(slot)];
    if (at >= s.f.size()) s.f.resize(at + 1, 0);
    s.f[at] = static_cast<int16_t>(s.f[at] + delta);
    s.dirty_f = true;  // the player destroyed ground; F is now unreproducible
  }

  int find_free() const {
    for (std::size_t i = 0; i < slots_.size(); ++i)
      if (slots_[i].state == SlotState::kFree) return static_cast<int>(i);
    return -1;
  }

  Slot& slot(int i) { return slots_[static_cast<std::size_t>(i)]; }
  const Slot& slot(int i) const { return slots_[static_cast<std::size_t>(i)]; }
  int slots() const { return static_cast<int>(slots_.size()); }

 private:
  std::vector<Slot> slots_;
};

}  // namespace terrain
}  // namespace zref

#endif  // ZREF_FJOURNAL_HPP
