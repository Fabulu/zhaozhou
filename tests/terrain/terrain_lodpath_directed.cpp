// terrain_lodpath_directed.cpp -- THE PAGE-LOAD DEVIATION PATH, end to end.
//
// Owner rulings R8/R22 (which reading), R24 (when: at page load) and R59 (how
// big).  The chain under test is
//
//   TERRAIN.MIPFEED's fine stream  ->  zhao_terrain_lodfeed  (buffer + walk)
//                                  ->  zhao_terrain_devstore (the store)
//                                  ->  a read of the sixteen descriptors
//
// WHAT THIS TEST EXISTS TO SHOW, which neither block alone can:
//
//   A VALUE TRAVERSES.  A height16 sample pushed in as a page-load sample comes
//   back out of the store as exactly the deviation `zref::terrain::lod_deviation`
//   computes for the same lattice.  Before this chain the only driver of
//   TERRAIN.LOD's `sp_dev1_i` anywhere in the tree was an LFSR in
//   `zhao_prod_top` (core entry I21), so "the values match" is the whole point
//   and not a formality.
//
// AND FOUR COUNTERS ARE FIRED, not asserted zero:
//   * `read_unwritten_o` -- a slot read before its records exist answers
//     DEV_MAX (full detail) and is COUNTED.  Fired by reading slot 3 first.
//   * `lattices_dropped_o` -- a second page's lattice arriving while the walk
//     is still running is dropped rather than stalling the paging spine.
//     Fired by starting a second lattice mid-walk.
//   * `surface1_samples_o` -- the underside's 1,089 samples are counted and
//     dropped, which is R59's smaller form (TERRAIN.LOD law 7 never reads
//     them).  Fired by streaming both surfaces.
//   * `hist_step_bad_o` -- the history writeback walking out of step with the
//     descriptor read.  This is the counter CLAUDE.md's "detector wired to two
//     operands that move together" rule is about: the two pointers are
//     advanced by two SEPARATE handshakes, so the comparison can fail.  Fired
//     by presenting two history records while the read side is stalled.
//
// The walk's measured COST is printed rather than asserted, because it is a
// budget input (see `zhao_terrain_lodfeed`'s header) and a pinned clock count
// would go stale silently.
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#include "verilated.h"

#include "Vtb_terrain_lodpath.h"
#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_tess.hpp"

namespace zt = zref::terrain;

namespace {

int g_fail = 0;
int g_checks = 0;

void cke(uint64_t want, uint64_t got, const char* what) {
  ++g_checks;
  if (want != got) {
    ++g_fail;
    std::printf("FAIL: %s -- expected %llu, got %llu\n", what,
                static_cast<unsigned long long>(want), static_cast<unsigned long long>(got));
  }
}

void ctrue(bool ok, const char* what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

constexpr int kW = 33;
constexpr int kVerts = kW * kW;

// One page's worth of height16 samples, the unit the page actually carries.
std::vector<int16_t> make_page(uint32_t seed) {
  std::vector<int16_t> h(kVerts);
  std::mt19937 rng(seed);
  for (int vj = 0; vj < kW; ++vj) {
    for (int vi = 0; vi < kW; ++vi) {
      // Ridges plus noise: enough relief that the three levels differ, inside
      // height16's S1.7.8 metre range so nothing saturates.
      const int32_t ridge = 256 * ((vi % 7) * 3 - (vj % 5) * 4);
      const int32_t noise = static_cast<int32_t>(rng() % 512) - 256;
      h[static_cast<size_t>(vj) * kW + vi] = static_cast<int16_t>(ridge + noise);
    }
  }
  return h;
}

// height16 -> fx16 is `raw << 8`, EXACT (spec/qformats.md section 9).  The RTL
// does the same conversion; this is the oracle's copy of that one line.
zt::ComposedLattice to_lattice(const std::vector<int16_t>& h) {
  zt::ComposedLattice lat;
  lat.w = kW;
  lat.h = kW;
  lat.dual = false;
  lat.top.resize(kVerts);
  lat.bottom.resize(kVerts);
  for (int k = 0; k < kVerts; ++k) {
    lat.top[static_cast<size_t>(k)] = static_cast<int32_t>(h[static_cast<size_t>(k)]) << 8;
    lat.bottom[static_cast<size_t>(k)] = lat.top[static_cast<size_t>(k)];
  }
  return lat;
}

struct Got {
  uint32_t d1, d2, d3;
  int32_t cy;
  bool fresh;
};

using Dut = Vtb_terrain_lodpath;

// Stream one surface of one page past the observer, exactly as
// TERRAIN.MIPFEED hands samples to TERRAIN.MIPGEN.
void stream_surface(Dut& t, const std::vector<int16_t>& h) {
  for (int k = 0; k < kVerts; ++k) {
    t.f_valid_i = 1;
    t.f_h_i = h[static_cast<size_t>(k)];
    t.eval();
    zhao::tick(t);
  }
  t.f_valid_i = 0;
  t.eval();
}

void start_page(Dut& t, int slot, uint16_t src, uint8_t gen = 0, uint32_t epoch = 0) {
  t.f_start_i = 1;
  t.f_slot_i = static_cast<uint8_t>(slot);
  t.f_gen_i = gen;
  t.f_epoch_i = epoch;
  t.f_src_id_i = src;
  t.eval();
  zhao::tick(t);
  t.f_start_i = 0;
  t.eval();
}

// THE DIRECTORY'S SIDE OF THE HANDLE CHECK, played by the bench.
//
// It deliberately does NOT answer on the cycle the request appears.  The real
// directory shares one address port between a mutation, a lookup and a check,
// and a check that loses is simply not answered that clock -- so a request
// that was a PULSE would be lost, and this waits a few clocks precisely to
// show that it is a LEVEL.
void answer_check(Dut& t, bool stale, int delay = 5) {
  long long guard = 0;
  while (!t.chk_valid_o && guard < 40000) {
    zhao::tick(t);
    t.eval();
    ++guard;
  }
  ctrue(t.chk_valid_o != 0, "the feed asks the directory once the records commit");
  for (int i = 0; i < delay; ++i) {
    zhao::tick(t);
    t.eval();
    ctrue(t.chk_valid_o != 0, "the request is HELD while the directory is busy");
  }
  t.chk_valid_i = 1;
  t.chk_stale_i = stale ? 1 : 0;
  t.eval();
  zhao::tick(t);
  t.chk_valid_i = 0;
  t.chk_stale_i = 0;
  t.eval();
  // The invalidation a stale verdict raises is registered; give it its cycle.
  zhao::tick(t);
  t.eval();
}

// Let the walk finish.  Bounded: a patch is ~13,700 clocks at the mesh reading,
// so 200,000 is a guard and not a schedule.
long long settle(Dut& t) {
  long long n = 0;
  while (t.feed_busy_o && n < 200000) {
    zhao::tick(t);
    t.eval();
    ++n;
  }
  ctrue(n < 200000, "the walk terminates");
  return n;
}

// Read one patch's sixteen descriptors out of the store.
std::vector<Got> read_patch(Dut& t, int slot, bool stall) {
  std::vector<Got> out;
  t.r_start_i = 1;
  t.r_slot_i = static_cast<uint8_t>(slot);
  t.eval();
  ctrue(t.r_ready_o != 0, "the store accepts a read start");
  zhao::tick(t);
  t.r_start_i = 0;
  t.eval();

  long long guard = 0;
  int taken = 0;
  while (taken < 16 && guard < 5000) {
    // A stalled read side is the case the history-step counter needs, and it
    // is also the ordinary backpressure case.
    t.r_ready_i = (stall && (guard & 3)) ? 0 : 1;
    t.eval();
    if (t.r_valid_o && t.r_ready_i) {
      Got g;
      g.d1 = t.r_dev1_o;
      g.d2 = t.r_dev2_o;
      g.d3 = t.r_dev3_o;
      g.cy = static_cast<int16_t>(t.r_cy_o);
      g.fresh = t.r_fresh_o != 0;
      cke(static_cast<uint32_t>(taken), t.r_sp_o, "descriptors arrive in subpatch order");
      out.push_back(g);
      ++taken;
    }
    zhao::tick(t);
    t.eval();
    ++guard;
  }
  t.r_ready_i = 0;
  t.eval();
  ctrue(taken == 16, "sixteen descriptors were read");
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut t;

  t.rst_n = 0;
  t.f_start_i = t.f_valid_i = 0;
  t.f_slot_i = 0;
  t.f_gen_i = 0;
  t.f_epoch_i = 0;
  t.f_src_id_i = 0;
  t.f_h_i = 0;
  t.chk_valid_i = 0;
  t.chk_stale_i = 0;
  t.r_start_i = 0;
  t.r_slot_i = 0;
  t.r_ready_i = 0;
  t.h_valid_i = 0;
  t.h_level_i = 0;
  t.h_morph_i = 0;
  t.h_hold_i = 0;
  t.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();

  // ------------------------------------------------------------------ 1 ----
  // A SLOT READ BEFORE ITS RECORDS EXIST answers DEV_MAX and is counted.  Done
  // first, so the counter cannot be confused with a later fault.
  {
    const auto got = read_patch(t, 3, false);
    cke(1, t.read_unwritten_o, "read_unwritten_o fires on an uncomputed slot");
    for (const auto& g : got) {
      cke(0xFFFFFFu, g.d1, "an uncomputed slot answers DEV_MAX at level 1");
      cke(0xFFFFFFu, g.d3, "an uncomputed slot answers DEV_MAX at level 3");
      ctrue(!g.fresh, "an uncomputed slot is not fresh");
    }
    std::printf("[1] uncomputed slot: DEV_MAX on all sixteen, counted once\n");
  }

  // ------------------------------------------------------------------ 2 ----
  // THE VALUE TRAVERSES.  One page in, both surfaces streamed; the store's
  // sixteen records must be zref's, bit for bit.
  const auto page = make_page(0xC0FFEEu);
  const auto lat = to_lattice(page);
  {
    start_page(t, 5, 0x1234);
    zhao::tick(t);   // inv_valid_o is registered; the counter lands next cycle
    t.eval();
    cke(1, t.invalidations_o, "a page start invalidates the slot it replaces");
    stream_surface(t, page);            // surface 0: buffered
    stream_surface(t, page);            // surface 1: counted and dropped (law 7)
    const long long clocks = settle(t);
    answer_check(t, false);             // the handle held: nothing is withdrawn

    cke(1, t.lattices_walked_o, "one lattice was walked");
    cke(16, t.dev_records_o, "sixteen records were emitted");
    cke(16, t.records_written_o, "sixteen records reached the store");
    cke(1, t.patches_committed_o, "the patch committed");
    cke(static_cast<uint32_t>(kVerts), t.surface1_samples_o,
        "the underside's samples were counted and dropped");
    cke(0, t.stray_samples_o, "no sample arrived outside a lattice");

    const auto got = read_patch(t, 5, false);
    for (int sp = 0; sp < 16; ++sp) {
      const int ox = (sp & 3) * 8, oz = (sp >> 2) * 8;
      const uint32_t w1 = zt::lod_deviation(lat, zt::Surface::kTop, ox, oz, 1,
                                            zt::kLodDevIncludeBoundary);
      const uint32_t w2 = zt::lod_deviation(lat, zt::Surface::kTop, ox, oz, 2,
                                            zt::kLodDevIncludeBoundary);
      const uint32_t w3 = zt::lod_deviation(lat, zt::Surface::kTop, ox, oz, 3,
                                            zt::kLodDevIncludeBoundary);
      // The block clips at 24 bits and says so on `clipped_o`; this page does
      // not reach the rails, which is asserted rather than assumed.
      cke(w1 > 0xFFFFFFu ? 0xFFFFFFu : w1, got[static_cast<size_t>(sp)].d1, "dev1 matches zref");
      cke(w2 > 0xFFFFFFu ? 0xFFFFFFu : w2, got[static_cast<size_t>(sp)].d2, "dev2 matches zref");
      cke(w3 > 0xFFFFFFu ? 0xFFFFFFu : w3, got[static_cast<size_t>(sp)].d3, "dev3 matches zref");
      // The subpatch centre height, read out of the same lattice at (ox+4, oz+4).
      cke(static_cast<uint16_t>(page[static_cast<size_t>(oz + 4) * kW + (ox + 4)]),
          static_cast<uint16_t>(got[static_cast<size_t>(sp)].cy),
          "the subpatch centre height traverses");
      ctrue(got[static_cast<size_t>(sp)].fresh, "a committed slot is fresh");
    }
    cke(0, t.dev_clipped_o, "this page does not reach the 24-bit rails");
    // The deviations must not be all zero, or a store that answered zero would
    // pass every check above on a flat page.
    uint32_t any = 0;
    for (const auto& g : got) any |= g.d1 | g.d2 | g.d3;
    ctrue(any != 0, "the deviations are not uniformly zero");
    std::printf("[2] value traverses: 16 records == zref, walk %lld clocks, %u vertices,"
                " %u lattice reads\n",
                clocks, t.dev_vertices_o, t.dev_lattice_reads_o);
  }

  // ------------------------------------------------------------------ 3 ----
  // A SECOND PAGE DURING A WALK IS DROPPED, NOT STALLED.
  {
    const auto other = make_page(0xBEEF01u);
    start_page(t, 6, 0x2222);
    stream_surface(t, other);
    // The walk is now running.  A third page arrives.
    ctrue(t.feed_busy_o != 0, "the walk is running when the next page starts");
    const uint32_t dropped_before = t.lattices_dropped_o;
    start_page(t, 7, 0x3333);
    cke(dropped_before + 1, t.lattices_dropped_o, "lattices_dropped_o fires on a start while busy");
    settle(t);
    answer_check(t, false);
    // Slot 6's records landed; slot 7's never existed and reads DEV_MAX.
    const uint32_t unwritten_before = t.read_unwritten_o;
    const auto g6 = read_patch(t, 6, false);
    cke(unwritten_before, t.read_unwritten_o, "the walked slot is fresh");
    ctrue(g6[0].fresh, "slot 6 committed");
    const auto g7 = read_patch(t, 7, false);
    cke(unwritten_before + 1, t.read_unwritten_o, "the dropped slot reads unwritten");
    cke(0xFFFFFFu, g7[0].d1, "the dropped slot answers DEV_MAX");
    std::printf("[3] a page arriving mid-walk is dropped and counted; the spine is not stalled\n");
  }

  // ------------------------------------------------------------------ 4 ----
  // THE HISTORY ROUND-TRIPS, and its step counter FIRES.
  {
    // Write a history record per descriptor, in step.
    t.r_start_i = 1;
    t.r_slot_i = 5;
    t.eval();
    zhao::tick(t);
    t.r_start_i = 0;
    t.eval();
    int taken = 0;
    long long guard = 0;
    while (taken < 16 && guard < 5000) {
      t.r_ready_i = 1;
      t.h_valid_i = 1;
      t.h_level_i = static_cast<uint8_t>(taken & 3);
      t.h_morph_i = static_cast<uint32_t>(taken) * 4096u;
      t.h_hold_i = static_cast<uint8_t>(taken + 1);
      t.eval();
      if (t.r_valid_o && t.r_ready_i) ++taken;
      zhao::tick(t);
      t.eval();
      ++guard;
    }
    t.r_ready_i = 0;
    t.h_valid_i = 0;
    t.eval();
    cke(0, t.hist_step_bad_o, "an in-step writeback does not trip the step counter");

    // Read it back: every descriptor must carry the history written for it.
    const auto got = read_patch(t, 5, false);
    (void)got;
    // `read_patch` does not sample the history fields, so re-walk with the
    // history captured.
    t.r_start_i = 1;
    t.r_slot_i = 5;
    t.eval();
    zhao::tick(t);
    t.r_start_i = 0;
    t.eval();
    taken = 0;
    guard = 0;
    while (taken < 16 && guard < 5000) {
      t.r_ready_i = 1;
      t.eval();
      if (t.r_valid_o && t.r_ready_i) {
        cke(static_cast<uint32_t>(taken & 3), t.r_prev_level_o, "prev_level round-trips");
        cke(static_cast<uint32_t>(taken) * 4096u, t.r_prev_morph_o, "prev_morph round-trips");
        cke(static_cast<uint32_t>(taken + 1), t.r_hold_o, "hold round-trips");
        ++taken;
      }
      zhao::tick(t);
      t.eval();
      ++guard;
    }
    t.r_ready_i = 0;
    t.eval();
    std::printf("[4] the history round-trips through the store, in step\n");
  }

  // ------------------------------------------------------------------ 5 ----
  // THE STEP COUNTER FIRES.  Two history records while the read side is
  // stalled: the second belongs to a descriptor that has not been retired, so
  // the two pointers disagree.  This is the POSITIVE CONTROL for a counter
  // that would otherwise be asserted zero forever.
  {
    const uint32_t before = t.hist_step_bad_o;
    t.r_start_i = 1;
    t.r_slot_i = 5;
    t.eval();
    zhao::tick(t);
    t.r_start_i = 0;
    t.eval();
    // Wait for the first descriptor, then hold r_ready low and push twice.
    long long guard = 0;
    while (!t.r_valid_o && guard < 100) {
      zhao::tick(t);
      t.eval();
      ++guard;
    }
    t.r_ready_i = 0;
    for (int i = 0; i < 2; ++i) {
      t.h_valid_i = 1;
      t.h_level_i = 1;
      t.h_morph_i = 7;
      t.h_hold_i = 9;
      t.eval();
      zhao::tick(t);
      t.eval();
    }
    t.h_valid_i = 0;
    t.eval();
    cke(before + 1, t.hist_step_bad_o, "hist_step_bad_o FIRES on an out-of-step writeback");
    std::printf("[5] the history-step detector fires on a fault it should catch\n");
    // Drain the read so the block is left idle.
    guard = 0;
    while (t.store_busy_o && guard < 5000) {
      t.r_ready_i = 1;
      t.eval();
      zhao::tick(t);
      t.eval();
      ++guard;
    }
    t.r_ready_i = 0;
    t.eval();
  }

  // ------------------------------------------------------------------ 6 ----
  // THE HANDLE CHECK ASKS WITH THE HANDLE THE WALK RAN UNDER, and a LIVE
  // verdict withdraws nothing.  This is the NEGATIVE CONTROL for case 7, and
  // it is the half a wire-it-and-ship pass would never write: a checker that
  // invalidated on every answer would also "fire", and would pass case 7
  // alone.
  {
    const uint32_t checked_before = t.handles_checked_o;
    const uint32_t stale_before   = t.handles_stale_o;
    const uint32_t inv_before     = t.invalidations_o;
    const uint32_t waited_before  = t.chk_unanswered_clocks_o;

    const auto p9 = make_page(0x515151u);
    start_page(t, 9, 0x9999, 3, 0xABCD1234u);
    stream_surface(t, p9);
    settle(t);

    // The request is up and it carries the WALK's handle -- not the last one
    // offered, which is what a live read of `slot_q` would have given.
    ctrue(t.chk_valid_o != 0, "a committed patch raises the handle check");
    cke(9, t.chk_slot_o, "the check carries the walk's slot");
    cke(3, t.chk_gen_o, "the check carries the walk's generation");
    cke(0xABCD1234u, t.chk_epoch_o, "the check carries the walk's epoch");

    answer_check(t, false);
    cke(checked_before + 1, t.handles_checked_o, "the check was answered and counted");
    cke(stale_before, t.handles_stale_o, "a LIVE handle does not count as stale");
    cke(inv_before, t.invalidations_o, "a LIVE handle withdraws nothing");
    ctrue(t.chk_unanswered_clocks_o > waited_before,
          "the request waited, so it is a level and not a pulse");
    cke(0, t.chk_overrun_o, "no commit landed on an outstanding check");
    cke(0, t.chk_stray_o, "no answer arrived without a request");

    const auto g9 = read_patch(t, 9, false);
    ctrue(g9[0].fresh, "the records survive a live handle");
    std::printf("[6] the check carries the walk's own {slot, gen, epoch}; a live handle"
                " withdraws nothing\n");
  }

  // ------------------------------------------------------------------ 7 ----
  // A STALE HANDLE WITHDRAWS THE RECORDS -- core entry I27's whole reason.
  // Without it the store holds page A's deviations under the handle of the
  // page that replaced it, `r_fresh_o` reads high, and TERRAIN.LOD decides the
  // new page's tessellation from the old page's terrain with every counter in
  // the chain balancing.
  {
    const uint32_t checked_before = t.handles_checked_o;
    const uint32_t stale_before   = t.handles_stale_o;
    const uint32_t inv_before     = t.invalidations_o;
    const uint32_t unwritten_before = t.read_unwritten_o;

    const auto p10 = make_page(0x7A7A7Au);
    start_page(t, 10, 0xAAAA, 4, 0x00C0FFEEu);
    stream_surface(t, p10);
    settle(t);
    cke(10, t.chk_slot_o, "the check carries slot 10");
    cke(4, t.chk_gen_o, "the check carries generation 4");

    answer_check(t, true);
    cke(checked_before + 1, t.handles_checked_o, "the stale check was answered and counted");
    cke(stale_before + 1, t.handles_stale_o, "handles_stale_o FIRES on a moved handle");
    cke(inv_before + 1, t.invalidations_o, "a stale handle invalidates the slot");

    const auto g10 = read_patch(t, 10, false);
    ctrue(!g10[0].fresh, "the withdrawn slot is no longer fresh");
    cke(0xFFFFFFu, g10[0].d1, "the withdrawn slot answers DEV_MAX");
    cke(unwritten_before + 1, t.read_unwritten_o, "the withdrawn slot reads unwritten");
    cke(0, t.chk_overrun_o, "no commit landed on an outstanding check");
    cke(0, t.chk_stray_o, "no answer arrived without a request");
    std::printf("[7] handles_stale_o fires on a fault it should catch, and the records"
                " are withdrawn\n");
  }

  std::printf("terrain_lodpath_directed: %d checks, %d failures\n", g_checks, g_fail);
  zhao::exit_hard(g_fail == 0 ? 0 : 1);
}
