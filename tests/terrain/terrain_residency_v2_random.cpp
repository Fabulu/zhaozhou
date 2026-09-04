// terrain_residency_v2_random.cpp — a multi-island traversal against an
// INDEPENDENT model of the rules, not of the RTL.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE, WHEN THE DIRECTED SUITE ALREADY PASSES
// ---------------------------------------------------------------------------
// The directed suite tests the cases someone thought of. What the world layer
// is specified against is a SEQUENCE: T9's "fly rapidly across the world,
// force residency churn, deform patches, leave them, return later". That is a
// sequence property, and it needs a camera that keeps moving while loads land
// late, pages go dirty, and slots are re-taken underneath in-flight work.
//
// The model below is written from the CONTRACT -- resident means claimed AND
// loaded AND mipped AND not since displaced -- and from
// `zref::terrain::residency_set_index` for the mapping. It is not a
// transcription of the RTL; if it were, it would agree with the RTL about the
// same mistakes.
//
// ---------------------------------------------------------------------------
// THE PROPERTY WORTH THE FILE
// ---------------------------------------------------------------------------
// DIRTY EVICTION, still. A dirty page holds permanent scars: ground the player
// destroyed. Layer F has no canonical HPS mirror (T4), so if the directory
// displaces a dirty page WITHOUT flagging it for writeback, that terrain
// silently heals -- the player returns to a crater they made and finds it
// gone. No frame is wrong, no single-frame check can see it, and it is
// unrecoverable because the data is already overwritten.
//
// The second property, which the prototype could not even express: TWO ISLANDS
// MAY OVERLAP IN LOCAL COORDINATES (T1). The traversal below deliberately
// walks four islands over the same patch coordinates.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_residency_v2.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"

namespace {

constexpr uint32_t kEpoch = 3u;
constexpr int kSets = 256;
constexpr int kWays = 4;

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

struct Key {
  uint32_t island;
  int16_t ix, iz;
  bool operator<(const Key& o) const {
    if (island != o.island) return island < o.island;
    if (ix != o.ix) return ix < o.ix;
    return iz < o.iz;
  }
  bool operator==(const Key& o) const { return island == o.island && ix == o.ix && iz == o.iz; }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_residency_v2 top;

  auto idle = [&]() {
    top.lu_valid_i = 0;
    top.cl_valid_i = 0;
    top.fin_valid_i = 0;
    top.dm_valid_i = 0;
    top.pin_valid_i = 0;
    top.unpin_valid_i = 0;
    top.wb_valid_i = 0;
    top.chk_valid_i = 0;
  };

  idle();
  top.rst_n = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  for (int i = 0; i < 400 && top.ready_o == 0; ++i) {
    zhao::tick(top);
    top.eval();
  }

  // ---- the model -----------------------------------------------------------
  struct Slot {
    bool occupied = false;
    Key key{};
    uint32_t gen = 0;
    bool loaded = false, mipped = false, dirty_f = false, evict_pending = false;
  };
  std::vector<Slot> model(kSets * kWays);

  uint16_t seq = 1;
  uint32_t s = 0xC0DE55u;

  int bad_set = 0, bad_evict_flag = 0, bad_resident = 0;
  int claims = 0, evictions = 0, dirty_evictions = 0, refusals = 0;
  int overlap_pairs = 0;

  struct Pending {
    uint32_t slot, gen;
    int due;
  };
  std::vector<Pending> pending;

  auto do_claim = [&](Key k) {
    idle();
    top.cl_valid_i = 1;
    top.cl_epoch_i = kEpoch;
    top.cl_island_i = k.island;
    top.cl_ix_i = k.ix;
    top.cl_iz_i = k.iz;
    top.cl_expect_crc_i = 0xA5A5A5A5u;
    top.cl_seq_i = seq++;
    for (int i = 0; i < 64; ++i) {
      top.eval();
      if (top.cl_ready_o) break;
      zhao::tick(top);
    }
    zhao::tick(top);
    idle();
    zhao::tick(top);
    top.eval();
    ++claims;

    // THE MAPPING, against the committed hash rather than against the RTL.
    const uint8_t want_set = zref::terrain::residency_set_index(k.island, k.ix, k.iz, kEpoch);
    if (!top.cl_refused_o && (top.cl_slot_o >> 2) != want_set) ++bad_set;

    if (top.cl_refused_o) {
      ++refusals;
      return;
    }

    const uint32_t slot = top.cl_slot_o;
    if (top.cl_evicted_o) {
      ++evictions;
      // THE ONE THAT MATTERS: the model knows whether the displaced page was
      // dirty, and the hardware must have said so.
      const bool want_dirty = model[slot].occupied && model[slot].dirty_f;
      if ((top.cl_evicted_dirty_o != 0) != want_dirty) ++bad_evict_flag;
      if (want_dirty) ++dirty_evictions;
      // and it must name the page it displaced
      if (top.cl_evicted_island_o != model[slot].key.island ||
          static_cast<int16_t>(top.cl_evicted_ix_o) != model[slot].key.ix ||
          static_cast<int16_t>(top.cl_evicted_iz_o) != model[slot].key.iz)
        ++bad_evict_flag;
    }

    if (top.cl_same_o) return;  // already here: no new generation, no reset

    model[slot].occupied = true;
    model[slot].key = k;
    model[slot].gen = top.cl_gen_o;
    model[slot].loaded = false;
    model[slot].mipped = false;
    model[slot].dirty_f = false;
    model[slot].evict_pending = (top.cl_evicted_dirty_o != 0);
    pending.push_back({slot, top.cl_gen_o, static_cast<int>(rnd(&s) % 24u)});
  };

  auto do_event = [&](int which, uint32_t slot, uint32_t gen, bool f = false) {
    idle();
    if (which == 0) {
      top.fin_valid_i = 1;
      top.fin_slot_i = slot;
      top.fin_gen_i = gen;
      top.fin_epoch_i = kEpoch;
      top.fin_ok_i = 1;
      top.fin_crc_i = 0xA5A5A5A5u;
    } else if (which == 1) {
      top.dm_valid_i = 1;
      top.dm_slot_i = slot;
      top.dm_gen_i = gen;
      top.dm_epoch_i = kEpoch;
      top.dm_bd_i = 0;
      top.dm_f_i = f;
      top.dm_mips_i = 0;
    } else {
      top.wb_valid_i = 1;
      top.wb_slot_i = slot;
      top.wb_gen_i = gen;
      top.wb_epoch_i = kEpoch;
    }
    for (int i = 0; i < 64; ++i) {
      top.eval();
      const bool rdy = (which == 0 && top.fin_ready_o) || (which == 1 && top.dm_ready_o) ||
                       (which == 2 && top.wb_ready_o);
      if (rdy) break;
      zhao::tick(top);
    }
    zhao::tick(top);
    idle();
    zhao::tick(top);
  };

  auto do_lookup = [&](Key k) {
    idle();
    top.lu_valid_i = 1;
    top.lu_epoch_i = kEpoch;
    top.lu_island_i = k.island;
    top.lu_ix_i = k.ix;
    top.lu_iz_i = k.iz;
    zhao::tick(top);
    idle();
    zhao::tick(top);
    top.eval();
    const bool got = top.lu_valid_o && top.lu_hit_o;

    bool want = false;
    for (const Slot& sl : model)
      if (sl.occupied && sl.key == k && sl.loaded && sl.mipped && !sl.evict_pending) want = true;
    if (got != want) ++bad_resident;
  };

  // Complete a page: loader finish, then mipgen. A page is not ground until
  // both have landed (T8), and a page that never becomes ground can never
  // become dirty either -- which is why MOST loads here complete promptly and
  // only some are left late. The first two versions of this file left every
  // load to a lazy queue, so almost nothing reached RESIDENT_CLEAN, almost
  // nothing could be deformed, and the traversal reported 0 dirty evictions
  // out of ~100 -- reading as "the hardware never does this" when the truth
  // was "the stimulus never asks".
  auto complete = [&](uint32_t slot) {
    Slot& sl = model[slot];
    if (!sl.occupied || sl.evict_pending || sl.mipped) return;
    if (!sl.loaded) {
      do_event(0, slot, sl.gen);
      sl.loaded = true;
    }
    do_event(0, slot, sl.gen);
    sl.mipped = true;
  };

  // ---- the traversal -------------------------------------------------------
  // FOUR ISLANDS OVER THE SAME COORDINATES. T1 makes that legal, and it is the
  // case the direct-mapped prototype gets wrong by construction.
  for (int step = 0; step < 3000; ++step) {
    const int cam = (step / 4) % 240;
    const uint32_t island = 1u + (rnd(&s) % 4u);
    const int16_t ix = static_cast<int16_t>(cam + static_cast<int>(rnd(&s) % 3u));
    const int16_t iz = static_cast<int16_t>((cam / 3) + static_cast<int>(rnd(&s) % 3u));
    const Key k{island, ix, iz};

    const uint32_t act = rnd(&s) % 100u;
    if (act < 34) {
      do_claim(k);
      // three claims in four are finished at once; the rest stay late, which
      // is what keeps the late-load hazard in the traversal.
      if ((rnd(&s) % 4u) != 0u) {
        for (int w = 0; w < kWays; ++w) {
          const uint32_t slot = static_cast<uint32_t>(
              zref::terrain::residency_set_index(k.island, k.ix, k.iz, kEpoch) * kWays + w);
          if (model[slot].occupied && model[slot].key == k) complete(slot);
        }
      }
    } else if (act < 55) {
      // finish a load that came due; a page needs load AND mipgen
      for (size_t i = 0; i < pending.size(); ++i) {
        if (pending[i].due <= 0) {
          const Pending p = pending[i];
          pending.erase(pending.begin() + static_cast<long>(i));
          Slot& sl = model[p.slot];
          if (sl.occupied && sl.gen == p.gen && !sl.evict_pending) {
            do_event(0, p.slot, p.gen);
            if (!sl.loaded) {
              sl.loaded = true;
              pending.push_back({p.slot, p.gen, 0});
            } else
              sl.mipped = true;
          }
          break;
        }
        --pending[i].due;
      }
    } else if (act < 68) {
      // DEFORM AGGRESSIVELY, and deform a WHOLE SET.
      //
      // The first version marked one random resident page per action and
      // produced 89 evictions of which ZERO were dirty -- not a bug, the
      // opposite: T9's rule 3 takes a clean unpinned way before rule 4 touches
      // a dirty one, and with four ways and occasional deformation a clean way
      // is nearly always there. The traversal never reached the case it was
      // written for, and said so.
      //
      // A dirty eviction requires every way in a set to be dirty, so the
      // deformation is applied per SET rather than per page.
      // And deform the set THE CAMERA IS ABOUT TO CLAIM INTO, not a random one
      // out of 256. A random set is almost never one under churn, which is why
      // the second version of this case still produced zero dirty evictions:
      // it was dirtying ground nobody was about to displace.
      const int set = zref::terrain::residency_set_index(k.island, k.ix, k.iz, kEpoch);
      for (int w = 0; w < kWays; ++w) {
        const size_t i = static_cast<size_t>(set * kWays + w);
        if (model[i].occupied && model[i].loaded && model[i].mipped && !model[i].evict_pending &&
            !model[i].dirty_f) {
          do_event(1, static_cast<uint32_t>(i), model[i].gen, true);
          model[i].dirty_f = true;
        }
      }
    } else if (act < 78) {
      // acknowledge a writeback so an EVICT_PENDING slot may load
      for (size_t i = 0; i < model.size(); ++i)
        if (model[i].occupied && model[i].evict_pending) {
          do_event(2, static_cast<uint32_t>(i), model[i].gen);
          model[i].evict_pending = false;
          break;
        }
    } else {
      do_lookup(k);
    }
  }

  // how often did two islands share a patch coordinate at once?
  {
    std::map<std::pair<int, int>, int> by_coord;
    for (const Slot& sl : model)
      if (sl.occupied) ++by_coord[{sl.key.ix, sl.key.iz}];
    for (const auto& e : by_coord)
      if (e.second > 1) ++overlap_pairs;
  }

  zhao::check(bad_set == 0,
              "every claim across the traversal lands in the set "
              "zref::terrain::residency_set_index names",
              0, bad_set);
  zhao::check(bad_resident == 0,
              "residency always matches the contract model -- claimed AND "
              "loaded AND mipped AND not since displaced",
              0, bad_resident);
  // THE ONE THAT MATTERS.
  zhao::check(bad_evict_flag == 0,
              "NO dirty page is displaced without being flagged for writeback, "
              "and every eviction names the page it displaced",
              0, bad_evict_flag);
  zhao::check(evictions > 20, "the traversal forced real eviction churn", 1,
              evictions > 20 ? 1 : 0);
  zhao::check(dirty_evictions > 0, "including dirty pages being displaced", 1,
              dirty_evictions > 0 ? 1 : 0);
  zhao::check(overlap_pairs > 0,
              "and different islands really did hold the SAME local patch "
              "coordinates at once -- the case the direct map cannot express",
              1, overlap_pairs > 0 ? 1 : 0);

  std::printf(
      "  %d claims, %d evictions (%d dirty), %d refusals, %d overlapping "
      "coordinate pairs resident\n",
      claims, evictions, dirty_evictions, refusals, overlap_pairs);

  return zhao::report_and_exit("terrain_residency_v2_random");
}
