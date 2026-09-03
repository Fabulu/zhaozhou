// terrain_residency_v2_directed.cpp — the thirteen cases T10 names, plus the
// two the prototype could not express at all.
//
// ---------------------------------------------------------------------------
// WHY A SECOND SUITE RATHER THAN AN EXTENSION OF THE FIRST
// ---------------------------------------------------------------------------
// `terrain_residency_directed.cpp` tests a direct map keyed on {px, py}. Two of
// the cases below cannot be written against that interface at all, because the
// thing they check is a distinction it does not have:
//
//   * TWO OVERLAPPING ISLANDS at identical local coordinates. T1 says this is
//     legal and that no software restriction against it exists. A {px, py}
//     directory answers island A's lookup with island B's ground, and the
//     picture is wrong in a way no single frame reveals.
//   * A DIRTY VICTIM WITH A DELAYED WRITEBACK ACK. The prototype reports a
//     dirty eviction and moves on. T4 makes the journal acknowledgement a
//     BARRIER: the slot may not enter LOADING until the scars are safe.
//
// The rest are the state machine, the replacement order, and the identity
// rules that make a stale event harmless.
//
// ---------------------------------------------------------------------------
// THE ONE THAT MATTERS MOST IS STILL DIRTY EVICTION
// ---------------------------------------------------------------------------
// A dirty page holds permanent scars: ground the player destroyed. If the
// directory reuses a dirty slot before the journal has it, that terrain
// silently heals. No frame is wrong, no single-frame check can see it, and it
// is unrecoverable because the data is already overwritten.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_residency_v2.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"

namespace {

constexpr uint32_t kEpoch = 7u;

template <typename T>
inline void zho_unused(const T&) {}

struct Handle {
  uint32_t slot = 0;
  uint32_t gen = 0;
  bool same = false;
  bool refused = false;
  bool evicted = false;
  bool evicted_dirty = false;
  uint32_t evicted_island = 0;
  int evicted_ix = 0, evicted_iz = 0;
};

class Dir {
 public:
  explicit Dir(Vzhao_terrain_residency_v2& t) : t_(t) {}

  void idle() {
    t_.lu_valid_i = 0;
    t_.cl_valid_i = 0;
    t_.fin_valid_i = 0;
    t_.dm_valid_i = 0;
    t_.pin_valid_i = 0;
    t_.unpin_valid_i = 0;
    t_.wb_valid_i = 0;
    t_.chk_valid_i = 0;
  }

  // Reset, then WAIT FOR THE SWEEP. A test that presents a request while
  // ready_o is low is testing nothing, and would pass by accident.
  void reset() {
    idle();
    t_.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(t_);
    t_.rst_n = 1;
    int clocks = 0;
    do {
      zhao::tick(t_);
      ++clocks;
      t_.eval();
    } while (t_.ready_o == 0 && clocks < 4000);
    sweep_clocks_ = clocks;
  }

  int sweep_clocks() const { return sweep_clocks_; }

  Handle claim(uint32_t island, int ix, int iz, uint32_t crc = 0xABCD1234u,
               uint32_t epoch = kEpoch) {
    idle();
    t_.cl_valid_i = 1;
    t_.cl_epoch_i = epoch;
    t_.cl_island_i = island;
    t_.cl_ix_i = static_cast<int16_t>(ix);
    t_.cl_iz_i = static_cast<int16_t>(iz);
    t_.cl_expect_crc_i = crc;
    t_.cl_seq_i = seq_++;
    // hold until accepted (the same-set hazard can stall one clock)
    for (int i = 0; i < 64; ++i) {
      t_.eval();
      if (t_.cl_ready_o) break;
      zhao::tick(t_);
    }
    zhao::tick(t_);
    idle();
    // the answer is registered one clock later
    zhao::tick(t_);
    t_.eval();
    Handle h;
    h.slot = t_.cl_slot_o;
    h.gen = t_.cl_gen_o;
    h.same = t_.cl_same_o != 0;
    h.refused = t_.cl_refused_o != 0;
    h.evicted = t_.cl_evicted_o != 0;
    h.evicted_dirty = t_.cl_evicted_dirty_o != 0;
    h.evicted_island = t_.cl_evicted_island_o;
    h.evicted_ix = static_cast<int16_t>(t_.cl_evicted_ix_o);
    h.evicted_iz = static_cast<int16_t>(t_.cl_evicted_iz_o);
    return h;
  }

  void event(int which, uint32_t slot, uint32_t gen, uint32_t epoch = kEpoch,
             bool ok = true, uint32_t crc = 0xABCD1234u, bool bd = false,
             bool f = false, bool mips = false) {
    idle();
    switch (which) {
      case 0:  // fin
        t_.fin_valid_i = 1; t_.fin_slot_i = slot; t_.fin_gen_i = gen;
        t_.fin_epoch_i = epoch; t_.fin_ok_i = ok; t_.fin_crc_i = crc;
        break;
      case 1:  // dirty
        t_.dm_valid_i = 1; t_.dm_slot_i = slot; t_.dm_gen_i = gen;
        t_.dm_epoch_i = epoch; t_.dm_bd_i = bd; t_.dm_f_i = f; t_.dm_mips_i = mips;
        break;
      case 2:  // pin
        t_.pin_valid_i = 1; t_.pin_slot_i = slot; t_.pin_gen_i = gen;
        t_.pin_epoch_i = epoch;
        break;
      case 3:  // unpin
        t_.unpin_valid_i = 1; t_.unpin_slot_i = slot; t_.unpin_gen_i = gen;
        t_.unpin_epoch_i = epoch;
        break;
      case 4:  // writeback ack
        t_.wb_valid_i = 1; t_.wb_slot_i = slot; t_.wb_gen_i = gen;
        t_.wb_epoch_i = epoch;
        break;
      default: break;
    }
    for (int i = 0; i < 64; ++i) {
      t_.eval();
      const bool rdy = (which == 0 && t_.fin_ready_o) || (which == 1 && t_.dm_ready_o) ||
                       (which == 2 && t_.pin_ready_o) || (which == 3 && t_.unpin_ready_o) ||
                       (which == 4 && t_.wb_ready_o);
      if (rdy) break;
      zhao::tick(t_);
    }
    zhao::tick(t_);
    idle();
    zhao::tick(t_);
  }

  // The loader finishes a page: mips are stale after a claim, so a page needs
  // a load completion AND a mip completion before it is ground.
  void load_and_mip(uint32_t slot, uint32_t gen, uint32_t crc = 0xABCD1234u) {
    event(0, slot, gen, kEpoch, true, crc);   // -> MIPGEN
    event(0, slot, gen, kEpoch, true, crc);   // -> RESIDENT_CLEAN
  }

  bool lookup(uint32_t island, int ix, int iz, uint32_t epoch = kEpoch) {
    idle();
    t_.lu_valid_i = 1;
    t_.lu_epoch_i = epoch;
    t_.lu_island_i = island;
    t_.lu_ix_i = static_cast<int16_t>(ix);
    t_.lu_iz_i = static_cast<int16_t>(iz);
    zhao::tick(t_);
    idle();
    zhao::tick(t_);
    t_.eval();
    return t_.lu_valid_o && t_.lu_hit_o;
  }

  bool stale(uint32_t slot, uint32_t gen, uint32_t epoch = kEpoch) {
    idle();
    t_.chk_valid_i = 1;
    t_.chk_slot_i = slot;
    t_.chk_gen_i = gen;
    t_.chk_epoch_i = epoch;
    zhao::tick(t_);
    idle();
    zhao::tick(t_);
    t_.eval();
    return t_.chk_valid_o && t_.chk_stale_o;
  }

 private:
  Vzhao_terrain_residency_v2& t_;
  uint16_t seq_ = 1;
  int sweep_clocks_ = 0;
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_residency_v2 top;
  Dir d(top);

  // ---- 0: the reset sweep happens, and ready gates it ---------------------
  {
    d.reset();
    zhao::check(d.sweep_clocks() >= 256,
                "ready stays LOW for a full 256-set init sweep -- no async reset "
                "of an inferred RAM",
                1, d.sweep_clocks() >= 256 ? 1 : 0);
    std::printf("  sweep took %d clocks\n", d.sweep_clocks());
  }

  // ---- 0b: THE SET INDEX IS THE COMMITTED HASH ----------------------------
  // The RTL defines CRC-8/ATM over {island_id, patch_ix, patch_iz} with the
  // epoch byte xored in. A hash defined ONLY by the RTL that implements it
  // cannot be wrong, which is exactly the problem with it -- and T9's last
  // clause has two readings, so a silent choice here would be unquestionable
  // by being invisible.
  //
  // `zref::terrain::residency_set_index` is that hash written down separately.
  // The slot a claim returns must land in the set it names.
  {
    d.reset();
    int bad_set = 0, checked = 0;
    for (uint32_t isl = 1; isl <= 40; ++isl)
      for (int ix = -3; ix <= 3; ++ix) {
        const Handle h = d.claim(isl, ix, ix * 7);
        const uint32_t got_set = h.slot >> 2;   // slot = {set, way}
        const uint8_t want_set = zref::terrain::residency_set_index(
            isl, static_cast<int16_t>(ix), static_cast<int16_t>(ix * 7), kEpoch);
        if (got_set != want_set) ++bad_set;
        ++checked;
      }
    zho_unused(checked);
    zhao::check(bad_set == 0,
                "every claim lands in the set zref::terrain::residency_set_index "
                "names -- the hash has a definition outside the RTL",
                0, bad_set);
  }

  // ---- 1: a claimed page is not resident until loaded AND mipped ----------
  {
    d.reset();
    const Handle h = d.claim(1, 10, 20);
    zhao::check(!d.lookup(1, 10, 20),
                "a claimed page is NOT resident until the loader finishes it", 0, 1);
    d.event(0, h.slot, h.gen);   // load done -> MIPGEN, mips still stale
    zhao::check(!d.lookup(1, 10, 20),
                "and NOT resident while its mips are still stale", 0, 1);
    d.event(0, h.slot, h.gen);   // mipgen done
    zhao::check(d.lookup(1, 10, 20), "and IS resident once both complete", 1, 1);
  }

  // ---- 2: TWO OVERLAPPING ISLANDS -- the case v1 cannot express -----------
  // T1: "Two islands may legally overlap in local patch coordinates."
  {
    d.reset();
    const Handle a = d.claim(1, 5, 6);
    d.load_and_mip(a.slot, a.gen);
    const Handle b = d.claim(2, 5, 6);   // SAME local coordinates, other island
    d.load_and_mip(b.slot, b.gen);

    zhao::check(a.slot != b.slot,
                "two islands at the same local coordinates take DIFFERENT slots",
                1, a.slot != b.slot ? 1 : 0);
    zhao::check(d.lookup(1, 5, 6) && d.lookup(2, 5, 6),
                "and both stay resident and separately findable", 1,
                (d.lookup(1, 5, 6) && d.lookup(2, 5, 6)) ? 1 : 0);
    zhao::check(!b.evicted,
                "the second island does not displace the first", 0,
                b.evicted ? 1 : 0);
  }

  // ---- 3: re-claiming the SAME patch does not advance the generation ------
  // A visible-set rebuild re-submits resident patches every frame. Advancing
  // here would tell every in-flight job it is stale, every frame, forever.
  {
    d.reset();
    const Handle a = d.claim(3, 1, 2);
    d.load_and_mip(a.slot, a.gen);
    const Handle b = d.claim(3, 1, 2);
    zhao::check(b.same, "re-claiming a resident patch reports SAME", 1, b.same ? 1 : 0);
    zhao::check(b.gen == a.gen, "and does not advance the generation", a.gen, b.gen);
    zhao::check(!b.evicted, "and reports no eviction", 0, b.evicted ? 1 : 0);
    zhao::check(d.lookup(3, 1, 2), "and the page stays resident", 1, 1);
  }

  // ---- 4: four ways in one set, then a fifth ------------------------------
  // Adversarial keys that collide in one set. Found by walking island ids
  // until four hash to the same set, which is what a real adversary does.
  {
    d.reset();
    std::vector<uint32_t> same_set;
    uint32_t set0 = 0;
    for (uint32_t isl = 1; isl < 4000 && same_set.size() < 5; ++isl) {
      const Handle h = d.claim(isl, 0, 0);
      const uint32_t set = h.slot >> 2;
      if (same_set.empty()) { set0 = set; same_set.push_back(isl); }
      else if (set == set0) same_set.push_back(isl);
    }
    zhao::check(same_set.size() >= 5,
                "five adversarial keys colliding in one set were found", 1,
                same_set.size() >= 5 ? 1 : 0);

    // fresh: fill the set, pin every way, then try a fifth
    d.reset();
    std::vector<Handle> hs;
    for (int i = 0; i < 4; ++i) {
      Handle h = d.claim(same_set[i], 0, 0);
      d.load_and_mip(h.slot, h.gen);
      d.event(2, h.slot, h.gen);   // pin
      hs.push_back(h);
    }
    const Handle fifth = d.claim(same_set[4], 0, 0);
    zhao::check(fifth.refused,
                "with all four ways PINNED, a fifth claim is REFUSED, not "
                "silently granted someone else's slot",
                1, fifth.refused ? 1 : 0);
    zhao::check(top.refused_all_pinned_o == 1, "and the refusal is counted", 1,
                static_cast<int>(top.refused_all_pinned_o));

    // unpin one; the fifth now fits
    d.event(3, hs[0].slot, hs[0].gen);
    const Handle again = d.claim(same_set[4], 0, 0);
    zhao::check(!again.refused, "unpinning one way admits it", 0,
                again.refused ? 1 : 0);
    zhao::check(again.slot == hs[0].slot,
                "and it takes the way that was released", hs[0].slot, again.slot);
  }

  // ---- 5: THE BARRIER -- a dirty victim with a delayed writeback ACK ------
  {
    d.reset();
    std::vector<uint32_t> same_set;
    uint32_t set0 = 0;
    for (uint32_t isl = 1; isl < 4000 && same_set.size() < 5; ++isl) {
      const Handle h = d.claim(isl, 0, 0);
      const uint32_t set = h.slot >> 2;
      if (same_set.empty()) { set0 = set; same_set.push_back(isl); }
      else if (set == set0) same_set.push_back(isl);
    }

    d.reset();
    std::vector<Handle> hs;
    for (int i = 0; i < 4; ++i) {
      Handle h = d.claim(same_set[i], 0, 0);
      d.load_and_mip(h.slot, h.gen);
      hs.push_back(h);
    }
    // scar the first one
    d.event(1, hs[0].slot, hs[0].gen, kEpoch, true, 0, false, /*f=*/true, false);
    zhao::check(d.lookup(same_set[0], 0, 0),
                "a scarred page is still resident (RESIDENT_DIRTY_F)", 1, 1);

    // a fifth claim must displace something; every clean way goes first
    const Handle fifth = d.claim(same_set[4], 0, 0);
    zhao::check(fifth.evicted, "the fifth claim displaces a page", 1,
                fifth.evicted ? 1 : 0);
    zhao::check(!fifth.evicted_dirty,
                "and it takes a CLEAN way first -- rule 3 before rule 4", 0,
                fifth.evicted_dirty ? 1 : 0);

    // now force the dirty one out: fill the rest again
    d.reset();
    hs.clear();
    for (int i = 0; i < 4; ++i) {
      Handle h = d.claim(same_set[i], 0, 0);
      d.load_and_mip(h.slot, h.gen);
      hs.push_back(h);
    }
    for (int i = 0; i < 4; ++i)
      d.event(1, hs[i].slot, hs[i].gen, kEpoch, true, 0, false, true, false);
    const Handle dirty_evict = d.claim(same_set[4], 0, 0);
    zhao::check(dirty_evict.evicted_dirty,
                "with every way DIRTY, the eviction is flagged for writeback",
                1, dirty_evict.evicted_dirty ? 1 : 0);
    zhao::check(top.dirty_evictions_o >= 1,
                "NO dirty page is displaced without being flagged", 1,
                top.dirty_evictions_o >= 1 ? 1 : 0);

    // THE BARRIER: the loader may not complete this slot before the ACK.
    d.event(0, dirty_evict.slot, dirty_evict.gen);
    d.event(0, dirty_evict.slot, dirty_evict.gen);
    zhao::check(!d.lookup(same_set[4], 0, 0),
                "a slot in EVICT_PENDING does NOT become resident before the "
                "journal acknowledges -- the scars are not safe yet",
                0, 1);

    d.event(4, dirty_evict.slot, dirty_evict.gen);   // writeback ACK
    d.event(0, dirty_evict.slot, dirty_evict.gen);
    d.event(0, dirty_evict.slot, dirty_evict.gen);
    zhao::check(d.lookup(same_set[4], 0, 0),
                "and DOES once the ACK arrives", 1, 1);
  }

  // ---- 6: a CRC failure faults the page, it never becomes ground ----------
  {
    d.reset();
    const Handle h = d.claim(11, 3, 4, 0xDEADBEEFu);
    d.event(0, h.slot, h.gen, kEpoch, true, 0x00000000u);   // wrong CRC
    zhao::check(!d.lookup(11, 3, 4),
                "a CRC-failed page is FAULTED and never rendered", 0, 1);
    zhao::check(top.crc_failures_o == 1, "and the failure is counted", 1,
                static_cast<int>(top.crc_failures_o));
  }

  // ---- 7: a failed load faults it too ------------------------------------
  {
    d.reset();
    const Handle h = d.claim(12, 3, 4);
    d.event(0, h.slot, h.gen, kEpoch, /*ok=*/false);
    zhao::check(!d.lookup(12, 3, 4), "an aborted load is FAULTED, not resident",
                0, 1);
  }

  // ---- 8: stale events are rejected on IDENTITY, not beaten on timing ----
  {
    d.reset();
    std::vector<uint32_t> same_set;
    uint32_t set0 = 0;
    for (uint32_t isl = 1; isl < 4000 && same_set.size() < 5; ++isl) {
      const Handle h = d.claim(isl, 0, 0);
      const uint32_t set = h.slot >> 2;
      if (same_set.empty()) { set0 = set; same_set.push_back(isl); }
      else if (set == set0) same_set.push_back(isl);
    }
    d.reset();
    std::vector<Handle> hs;
    for (int i = 0; i < 4; ++i) {
      Handle h = d.claim(same_set[i], 0, 0);
      d.load_and_mip(h.slot, h.gen);
      hs.push_back(h);
    }
    const Handle fresh = d.claim(same_set[4], 0, 0);   // reuses one of them
    const uint32_t before = top.stale_events_o;

    // the OLD occupant's loader finishes, late
    d.event(0, fresh.slot, static_cast<uint32_t>(fresh.gen - 1));
    zhao::check(top.stale_events_o == before + 1,
                "a late FIN on a re-claimed slot is rejected on identity and "
                "counted",
                1, static_cast<int>(top.stale_events_o - before));
    zhao::check(!d.lookup(same_set[4], 0, 0),
                "and does NOT mark the fresh page loaded", 0, 1);

    // the old occupant's deformation, late
    d.event(1, fresh.slot, static_cast<uint32_t>(fresh.gen - 1), kEpoch, true, 0,
            false, true, false);
    zhao::check(top.stale_events_o == before + 2,
                "a late DIRTY on a re-claimed slot is rejected too", 2,
                static_cast<int>(top.stale_events_o - before));
  }

  // ---- 9: handles ---------------------------------------------------------
  {
    d.reset();
    const Handle h = d.claim(21, 7, 8);
    d.load_and_mip(h.slot, h.gen);
    zhao::check(!d.stale(h.slot, h.gen), "a live handle is not stale", 0, 1);
    zhao::check(d.stale(h.slot, h.gen + 1), "a wrong generation IS stale", 1, 1);
    zhao::check(d.stale(h.slot, h.gen, kEpoch + 1),
                "and so is the right generation in the WRONG EPOCH -- a level "
                "reload does not silently reuse last level's ground",
                1, 1);
  }

  // ---- 10: a lookup in the wrong epoch misses -----------------------------
  {
    d.reset();
    const Handle h = d.claim(31, 1, 1);
    d.load_and_mip(h.slot, h.gen);
    zhao::check(d.lookup(31, 1, 1, kEpoch), "resident in its own epoch", 1, 1);
    zhao::check(!d.lookup(31, 1, 1, kEpoch + 1),
                "and NOT resident in a newer epoch -- TerrainEpoch BEGIN starts "
                "with no resident hit from an older epoch",
                0, 1);
  }

  // ---- 11: a handle held under many intervening claims --------------------
  // T10 asks for 255. An 8-bit generation wraps at 256, so 255 is the largest
  // number of reuses for which staleness is still decidable -- which is why
  // the ruling says u8 MINIMUM and picks that number.
  {
    d.reset();
    const Handle h = d.claim(41, 1, 1);
    d.load_and_mip(h.slot, h.gen);
    for (int i = 0; i < 255; ++i) d.claim(1000u + static_cast<uint32_t>(i), 9, 9);
    zhao::check(!d.stale(h.slot, h.gen),
                "a handle survives 255 intervening claims elsewhere", 0, 1);
  }

  // ---- 12: deterministic repeat of the same claim stream ------------------
  // The console's whole verification story is replay. The same stream must
  // produce the same slots, or a capture cannot be compared to anything.
  {
    std::vector<uint32_t> first, second;
    for (int pass = 0; pass < 2; ++pass) {
      d.reset();
      auto& out = (pass == 0) ? first : second;
      for (uint32_t i = 0; i < 200; ++i) {
        const Handle h = d.claim(i % 7u, static_cast<int>(i % 13u),
                                 static_cast<int>(i % 11u));
        out.push_back(h.slot);
        if (!h.same && !h.refused) d.load_and_mip(h.slot, h.gen);
      }
    }
    bool identical = first.size() == second.size();
    for (size_t i = 0; identical && i < first.size(); ++i)
      if (first[i] != second[i]) identical = false;
    zhao::check(identical,
                "the same claim stream produces the SAME slots, twice -- this "
                "is what makes a capture replayable",
                1, identical ? 1 : 0);
  }

  std::printf("  hits %u misses %u claims %u evictions %u (%u dirty) refused %u "
              "stale %u crcfail %u\n",
              top.hits_o, top.misses_o, top.claims_o, top.evictions_o,
              top.dirty_evictions_o, top.refused_all_pinned_o, top.stale_events_o,
              top.crc_failures_o);

  return zhao::report_and_exit("terrain_residency_v2_directed");
}
