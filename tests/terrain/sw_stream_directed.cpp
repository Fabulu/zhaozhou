// sw_stream_directed.cpp -- SW.STREAM's frame policy at ISLAND SCALE.
//
// Step 4 of `reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md` §5. The oracle is
// `zref::swstream::WorldStreamer` (reference/include/zref/zref_sw_stream.hpp).
//
// ---------------------------------------------------------------------------
// WHY THIS TEST IS NOT ABOUT ONE PAGE
// ---------------------------------------------------------------------------
// `pageloader_rtl_directed.cpp` already proves one page moves correctly. Every
// property SW.STREAM adds is invisible at that scale and only appears when the
// island is bigger than the budget:
//
//   * a budget of 32 pages/frame is not a property of a frame that wants 12;
//   * "union the views before deduplication" needs two views wanting one patch;
//   * a canonical order is not testable on a list of one;
//   * "staged complete before publication" needs a page that fails to stage
//     while its neighbours succeed;
//   * a dirty F sheet surviving eviction needs the patch to LEAVE and RETURN.
//
// So the camera here crosses an 8 km island FASTER THAN THE BUDGET ALLOWS,
// which is the case reports/Missingterrain describes as the one the console
// has never been able to do at all.

#include <cstdint>
#include <cstdio>
#include <set>
#include <utility>
#include <vector>

#include "zref/zref_fjournal.hpp"
#include "zref/zref_sw_stream.hpp"

namespace {

int g_checks = 0;
bool g_failed = false;

void check(bool ok, const char* what, long want, long got) {
  ++g_checks;
  if (!ok) {
    g_failed = true;
    std::printf("FAIL: %s (expected %ld, got %ld)\n", what, want, got);
  }
}

void check_eq(long got, long want, const char* what) { check(got == want, what, want, got); }

namespace sws = zref::swstream;
namespace isl = zref::island;

constexpr int32_t kSide = 125;      // 8 km at 64 m patches
constexpr int32_t kBandLo = 40;     // a solid band of ground across the island
constexpr int32_t kBandHi = 85;
constexpr uint64_t kCartBase = 0x8000'0000ull;
constexpr uint32_t kCartBytes = 0x0800'0000u;   // 128 MiB of cartridge
constexpr uint32_t kStageBase = 0x2000'0000u;
constexpr uint32_t kStageSlots = 4096;
constexpr uint32_t kStageBytes = kStageSlots * sws::kPageBytes;

// Build the island and register a cartridge source for every patch that
// exists. Sources are laid out contiguously so an out-of-bounds offset has to
// be injected on purpose rather than happening by accident.
void build(isl::Directory& dir, sws::WorldStreamer& ws) {
  uint32_t h = 1;
  for (int32_t iz = kBandLo; iz < kBandHi; ++iz)
    for (int32_t ix = 0; ix < kSide; ++ix) dir.set(ix, iz, h++);

  uint64_t off = 0;
  uint32_t src = 0x1000;
  for (int32_t iz = kBandLo; iz < kBandHi; ++iz)
    for (int32_t ix = 0; ix < kSide; ++ix) {
      sws::PageSource s;
      s.cart_offset = off;
      s.declared_crc32c = 0xC0DE'0000u ^ (static_cast<uint32_t>(iz) << 8) ^
                          static_cast<uint32_t>(ix);
      s.source_id = src++;
      ws.set_source(ix, iz, s);
      off += sws::kPageBytes;
    }
}

}  // namespace

int main() {
  isl::Desc d;
  d.island_id = 0x5Au;
  d.pitch_log2 = 1;  // canonical 2 m
  d.extent_ix = static_cast<uint16_t>(kSide);
  d.extent_iz = static_cast<uint16_t>(kSide);

  isl::Directory dir(d);
  zref::terrain::Journal journal;
  sws::WorldStreamer ws(dir, journal);
  ws.configure(zref::mem::GuardRegion{static_cast<uint32_t>(kCartBase), kCartBytes},
               zref::mem::GuardRegion{kStageBase, kStageBytes}, /*epoch=*/7, kStageSlots);
  build(dir, ws);

  std::printf("island %dx%d, ground %zu patches, budget %u pages/frame\n", kSide, kSide,
              dir.resident_count(), ws.budget());
  check(static_cast<long>(dir.resident_count()) > 20L * static_cast<long>(sws::kPageBudgetPerFrame),
        "the island's ground is more than twenty budgets deep, so a traversal "
        "MUST outrun the page budget rather than incidentally fitting inside it",
        1, dir.resident_count() > 20u * sws::kPageBudgetPerFrame ? 1 : 0);

  sws::Ledger L{};

  // =========================================================================
  // 1. THE BUDGET, UNDER A CAMERA THAT OUTRUNS IT.
  // =========================================================================
  // THE CAMERA IS SIZED TO OUTRUN THE BUDGET IN STEADY STATE, NOT MERELY ON
  // FRAME ZERO. A first frame is cold and defers required pages however slowly
  // it moves; that proves only that a cold start is cold. A radius-6 window is
  // 13 rows deep, so at three patches per frame the leading edge presents
  // 3 x 13 = 39 NEW required patches every frame against a ceiling of 32 -- a
  // deficit of seven per frame that never catches up.
  //
  // Measured first at one patch per frame with a radius-4 window: required
  // pages were deferred on exactly ONE of sixty frames. The suite passed while
  // exercising a cold start, which is CLAUDE.md's "check the heuristic against
  // a case you can verify by hand" in miniature -- 45 required minus a 32-page
  // budget is 13, and 13 was the number printed.
  constexpr int32_t kStepPatches = 3;
  constexpr int32_t kWindowRadius = 6;
  std::vector<sws::Camera> cams(1);
  cams[0].view = isl::View{0, 62, kWindowRadius};
  cams[0].vel_ix_q10 = kStepPatches * 1024;
  cams[0].view_bit = 1;

  uint32_t worst_loads = 0;
  uint32_t frames_over_budget = 0;
  uint32_t total_required_deferred = 0;
  uint32_t frames_with_proxy = 0;

  constexpr int32_t kFrames = 36;
  for (int32_t frame = 0; frame < kFrames; ++frame) {
    cams[0].view.centre_ix = frame * kStepPatches;
    const sws::Frame f = ws.build_frame(cams, &L);

    if (f.loads_planned > worst_loads) worst_loads = f.loads_planned;
    if (f.loads_planned > sws::kPageBudgetPerFrame) ++frames_over_budget;
    total_required_deferred += f.required_deferred;
    if (f.required_deferred > 0) ++frames_with_proxy;

    // The frame's list is sealed and its bytes are exactly 32 per record.
    check_eq(static_cast<long>(f.list.bytes.size()),
             static_cast<long>(f.list.patch_count) * static_cast<long>(sws::kRecordBytes),
             "the sealed list is exactly 32 bytes per record");

    // THE LIST'S ONE REAL INVARIANT, and the suite did not have it until a
    // fire-test went looking. Perturbing the model to COUNT a prefetch record
    // as deferred and then seal it anyway fired nothing: the counter and the
    // list had diverged, and every result-checking assertion stayed green.
    // Every record is either a page being moved this frame or one already in
    // memory. A record that is neither is a promise the frame cannot keep.
    check_eq(static_cast<long>(f.list.patch_count),
             static_cast<long>(f.loads_planned) + static_cast<long>(f.already_resident),
             "every sealed record is either loading now or already resident: a "
             "deferred record must be ABSENT, not merely counted");
    long unstaged = 0;
    for (const sws::PatchRecord& r : f.list.records)
      if (r.hps_page_addr == 0) ++unstaged;
    check_eq(unstaged, 0, "no sealed record names a page that was never staged "
                          "-- T12's 'never expose a half-built page list to "
                          "CMD.DMA'");

    // Report the loads back as completed, so the next frame's "already
    // resident" accounting is real rather than a fresh cold start every time.
    for (const sws::PatchRecord& r : f.list.records)
      if (r.hps_page_addr != 0) ws.note_load_complete(r.patch_ix, r.patch_iz, true);
  }

  std::printf("  %d frames: worst loads/frame %u, frames over budget %u,\n"
              "             required deferred %u over %u frames, prefetch deferred %u\n",
              kFrames, worst_loads, frames_over_budget, total_required_deferred,
              frames_with_proxy, L.prefetch_deferred);

  check_eq(frames_over_budget, 0,
           "NO FRAME EXCEEDS T7's 32-PAGE CEILING -- the ruling is a ceiling per "
           "frame, not an average, because the bridge cannot borrow from a "
           "frame that has already gone");
  check(worst_loads == sws::kPageBudgetPerFrame,
        "and the budget is actually REACHED, so the check above is testing a "
        "constraint that binds rather than one the workload never approached",
        static_cast<long>(sws::kPageBudgetPerFrame), static_cast<long>(worst_loads));
  check(total_required_deferred > 0,
        "a camera that outruns the budget DOES defer required pages -- they "
        "render as T7's declared proxy and are counted, and the frame is NOT "
        "faulted (T6's frame fault is the composed-cache pressure, not this one)",
        1, total_required_deferred > 0 ? 1 : 0);
  check(frames_with_proxy > static_cast<uint32_t>(kFrames / 2),
        "and it defers them on MOST frames, not just the cold first one: a "
        "deficit that appears only at start-up is a cold start, not a camera "
        "outrunning the bandwidth",
        kFrames / 2 + 1, static_cast<long>(frames_with_proxy));
  check_eq(L.proxy_patches, total_required_deferred,
           "every deferred required patch is counted as a proxy patch: a miss "
           "that is not counted is the failure mode T7 exists to prevent");
  check(L.prefetch_deferred > 0,
        "and PREFETCH is what absorbs the pressure first -- T12 permits "
        "deferring prefetch and forbids mutating a sealed required list",
        1, L.prefetch_deferred > 0 ? 1 : 0);

  // The budget is a knob, and moving it must move the answer. A test whose
  // result is the same at 32 and at 8 is not measuring the budget.
  {
    isl::Directory d2(d);
    zref::terrain::Journal j2;
    sws::WorldStreamer w2(d2, j2);
    w2.configure(zref::mem::GuardRegion{static_cast<uint32_t>(kCartBase), kCartBytes},
                 zref::mem::GuardRegion{kStageBase, kStageBytes}, 7, kStageSlots);
    build(d2, w2);
    w2.set_budget(8);
    sws::Ledger L2{};
    uint32_t worst8 = 0;
    for (int32_t frame = 0; frame < 10; ++frame) {
      cams[0].view.centre_ix = frame;
      const sws::Frame f = w2.build_frame(cams, &L2);
      if (f.loads_planned > worst8) worst8 = f.loads_planned;
      for (const sws::PatchRecord& r : f.list.records)
        if (r.hps_page_addr != 0) w2.note_load_complete(r.patch_ix, r.patch_iz, true);
    }
    check_eq(worst8, 8, "the budget is a live parameter: at 8 pages/frame the "
                        "worst frame loads 8, not 32");
  }

  // =========================================================================
  // 2. TWO VIEWS ARE UNIONED BEFORE DEDUPLICATION (T7), AND DUAL IS THE PROOF.
  // =========================================================================
  // Two cameras with overlapping windows. A patch both want must appear ONCE,
  // with both view bits set. Deduplicating per view and then concatenating
  // would produce it twice; deduplicating after the union without OR-ing the
  // mask would produce it once with one bit, and the DUAL flag -- which is how
  // the hardware knows a page serves split-screen -- would be lost silently.
  {
    isl::Directory d3(d);
    zref::terrain::Journal j3;
    sws::WorldStreamer w3(d3, j3);
    w3.configure(zref::mem::GuardRegion{static_cast<uint32_t>(kCartBase), kCartBytes},
                 zref::mem::GuardRegion{kStageBase, kStageBytes}, 7, kStageSlots);
    build(d3, w3);
    sws::Ledger L3{};

    std::vector<sws::Camera> duo(2);
    duo[0].view = isl::View{60, 62, 3};
    duo[0].view_bit = 1;
    duo[1].view = isl::View{62, 62, 3};  // offset by two: a wide overlap
    duo[1].view_bit = 2;

    const sws::Frame f = w3.build_frame(duo, &L3);

    // No key appears twice.
    long dupes = 0;
    for (std::size_t i = 1; i < f.list.records.size(); ++i)
      for (std::size_t k = 0; k < i; ++k)
        if (f.list.records[i].patch_ix == f.list.records[k].patch_ix &&
            f.list.records[i].patch_iz == f.list.records[k].patch_iz)
          ++dupes;
    check_eq(dupes, 0, "a patch both views want appears EXACTLY ONCE in the "
                       "sealed list: the union happens before deduplication");

    long dual = 0, only0 = 0, only1 = 0;
    for (const sws::PatchRecord& r : f.list.records) {
      if (r.view_mask == 0x3) ++dual;
      else if (r.view_mask == 0x1) ++only0;
      else if (r.view_mask == 0x2) ++only1;
    }
    check(dual > 0, "the overlap is flagged DUAL", 1, dual > 0 ? 1 : 0);
    check(only0 > 0 && only1 > 0,
          "and the non-overlapping edges are NOT: a run where everything came "
          "back DUAL would pass the check above while proving nothing",
          1, (only0 > 0 && only1 > 0) ? 1 : 0);
    for (const sws::PatchRecord& r : f.list.records)
      if (r.view_mask == 0x3)
        check_eq(r.flags & sws::kFlagDual, sws::kFlagDual,
                 "every dual-masked record carries the DUAL flag");
  }

  // =========================================================================
  // 3. THE CANONICAL ORDER IS THE DETERMINISM (T5).
  // =========================================================================
  // Two independently constructed streamers, fed the same frame, must seal
  // BYTE-IDENTICAL lists with the same CRC. That is what makes list_crc32c an
  // identity rather than a checksum, and what makes the sealed bytes capture
  // data that a replay can be compared against.
  {
    auto seal_once = [&](uint32_t stage_slots) {
      isl::Directory dd(d);
      zref::terrain::Journal jj;
      sws::WorldStreamer w(dd, jj);
      w.configure(zref::mem::GuardRegion{static_cast<uint32_t>(kCartBase), kCartBytes},
                  zref::mem::GuardRegion{kStageBase, kStageBytes}, 7, stage_slots);
      build(dd, w);
      std::vector<sws::Camera> c(2);
      c[0].view = isl::View{70, 60, 3};
      c[0].view_bit = 1;
      c[0].vel_iz_q10 = 2048;
      c[1].view = isl::View{73, 64, 3};
      c[1].view_bit = 2;
      c[1].vel_ix_q10 = -1536;
      // WARM IT FIRST, DELIBERATELY. On a cold frame the 32-page budget is
      // consumed entirely by required-current and NOT ONE prefetch record
      // reaches the list -- so a determinism check on a cold frame would be
      // checking the ordering of a list containing only one of the two
      // classes. Measured: the first version of this sub-test found zero
      // prefetch records and said so. Six warm frames put required-current in
      // residency, which frees the budget for the prefetch the ordering check
      // needs.
      for (int wi = 0; wi < 6; ++wi) {
        const sws::Frame wf = w.build_frame(c, nullptr);
        for (const sws::PatchRecord& r : wf.list.records)
          if (r.hps_page_addr != 0) w.note_load_complete(r.patch_ix, r.patch_iz, true);
      }
      return w.build_frame(c, nullptr).list;
    };
    const sws::SealedList a = seal_once(kStageSlots);
    const sws::SealedList b = seal_once(kStageSlots);
    check_eq(static_cast<long>(a.bytes.size()), static_cast<long>(b.bytes.size()),
             "two runs of the same frame seal the same number of bytes");
    long diff = 0;
    for (std::size_t i = 0; i < a.bytes.size() && i < b.bytes.size(); ++i)
      if (a.bytes[i] != b.bytes[i]) ++diff;
    check_eq(diff, 0, "two runs of the same frame seal BYTE-IDENTICAL list bytes");
    check_eq(static_cast<long>(a.list_crc32c), static_cast<long>(b.list_crc32c),
             "...and therefore the same list_crc32c");
    check(a.list_crc32c != 0, "the CRC is not the empty-list constant, i.e. the "
                              "comparison above ran on real bytes",
          1, a.list_crc32c != 0 ? 1 : 0);

    // The order itself, checked against the comparator's own law rather than
    // against a golden dump: every adjacent pair must be in canonical order.
    long inversions = 0;
    for (std::size_t i = 1; i < a.records.size(); ++i)
      if (sws::canonical_less(a.records[i], a.records[i - 1])) ++inversions;
    check_eq(inversions, 0, "the sealed list is in T5's canonical order end to end");

    // And REQUIRED really does precede PREFETCH -- the first sort key, which
    // an order that happened to be sorted by coordinates alone would fail.
    bool seen_prefetch = false;
    long required_after_prefetch = 0;
    for (const sws::PatchRecord& r : a.records) {
      if (r.flags & sws::kFlagPrefetch) seen_prefetch = true;
      else if (seen_prefetch) ++required_after_prefetch;
    }
    check_eq(required_after_prefetch, 0,
             "every REQUIRED record precedes every PREFETCH record");
    check(seen_prefetch, "and prefetch records exist, so the check above is not "
                         "vacuous", 1, seen_prefetch ? 1 : 0);
  }

  // =========================================================================
  // 4. STAGED COMPLETE BEFORE PUBLICATION (T12 / T7).
  // =========================================================================
  // Three patches inside the window are sabotaged: one whose cartridge source
  // runs off the end of the cartridge, one that arrives short, one whose bytes
  // disagree with the declared CRC. None may appear in the sealed list, and
  // their neighbours must be unaffected -- a staging failure that took the
  // frame down with it would be a worse bug than the one being tested.
  {
    isl::Directory d4(d);
    zref::terrain::Journal j4;
    sws::WorldStreamer w4(d4, j4);
    w4.configure(zref::mem::GuardRegion{static_cast<uint32_t>(kCartBase), kCartBytes},
                 zref::mem::GuardRegion{kStageBase, kStageBytes}, 7, kStageSlots);
    build(d4, w4);

    sws::PageSource bad_bounds;
    bad_bounds.cart_offset = kCartBytes - 8;  // the page's tail is past the end
    bad_bounds.declared_crc32c = 0x1111'1111u;
    bad_bounds.source_id = 0xBAD0;
    w4.set_source(60, 62, bad_bounds);

    sws::PageSource short_page;
    short_page.cart_offset = 0;
    short_page.declared_crc32c = 0x2222'2222u;
    short_page.source_id = 0xBAD1;
    short_page.inject_incomplete = true;
    w4.set_source(61, 62, short_page);

    sws::PageSource bad_crc;
    bad_crc.cart_offset = 0;
    bad_crc.declared_crc32c = 0x3333'3333u;
    bad_crc.inject_actual_crc = 0x3333'3334u;  // one bit out
    bad_crc.source_id = 0xBAD2;
    w4.set_source(62, 62, bad_crc);

    sws::Ledger L4{};
    std::vector<sws::Camera> c(1);
    c[0].view = isl::View{61, 62, 2};
    c[0].view_bit = 1;
    const sws::Frame f = w4.build_frame(c, &L4);

    long sabotaged_in_list = 0;
    long neighbours_in_list = 0;
    for (const sws::PatchRecord& r : f.list.records) {
      const bool sab = r.patch_iz == 62 && r.patch_ix >= 60 && r.patch_ix <= 62;
      if (sab) ++sabotaged_in_list;
      else if (r.hps_page_addr != 0) ++neighbours_in_list;
    }
    check_eq(sabotaged_in_list, 0,
             "NOT ONE of the three pages that failed to stage completely "
             "reaches the sealed list -- T7's 'a half-loaded or CRC-failed "
             "page is never rendered' is enforced BEFORE the seal, not after");
    check(neighbours_in_list > 0,
          "and their neighbours staged normally: a staging failure does not "
          "take the frame with it",
          1, neighbours_in_list > 0 ? 1 : 0);
    check_eq(L4.refused_source_bounds, 1,
             "the out-of-cartridge page is refused on BOUNDS, before staging");
    check_eq(L4.staged_incomplete, 1, "the short page is counted as incomplete");
    check_eq(L4.staged_crc_fail, 1, "the CRC-mismatched page is counted as such");
    check_eq(L4.proxy_patches, 3, "all three render as proxy and are counted");

    // Every address in the list is inside the staging arena, and no two
    // records share one. A page staged on top of another is a wrong-ground bug
    // that renders perfectly.
    long outside = 0, collisions = 0;
    for (std::size_t i = 0; i < f.list.records.size(); ++i) {
      const uint64_t a = f.list.records[i].hps_page_addr;
      if (a == 0) continue;
      if (a < kStageBase || a + sws::kPageBytes > kStageBase + (uint64_t)kStageBytes) ++outside;
      for (std::size_t k = 0; k < i; ++k)
        if (f.list.records[k].hps_page_addr == a) ++collisions;
    }
    check_eq(outside, 0, "every staged address lies inside the staging arena");
    check_eq(collisions, 0, "no two records name the same staged page");
  }

  // =========================================================================
  // 5. THE SEAL IS A ONE-WAY DOOR (T12).
  // =========================================================================
  {
    sws::Ledger L5{};
    std::vector<sws::Camera> c(1);
    c[0].view = isl::View{50, 62, 2};
    c[0].view_bit = 1;
    sws::Frame f = ws.build_frame(c, &L5);
    check(f.list.sealed, "a built frame's list is sealed", 1, f.list.sealed ? 1 : 0);
    check_eq(ws.try_mutate_after_seal(f.list, &L5) ? 1 : 0, 0,
             "a sealed list REFUSES mutation: T12 forbids mutating a sealed "
             "REQUIRED list, and the refusal is a counter rather than an "
             "ordering the caller is trusted to respect");
    check_eq(L5.seal_mutations_refused, 1, "and the refusal is counted");
  }

  // =========================================================================
  // 6. EVICTION OF A DIRTY PAGE DOES NOT LOSE STATE (T4).
  // =========================================================================
  // The property this whole layer is worth writing for: a dirty F sheet lost
  // without a journal ACK silently heals terrain the player destroyed. No
  // frame is wrong, no single-frame check can see it, and it is unrecoverable
  // because the data is already overwritten.
  //
  // Driven at ISLAND SCALE: the camera deforms a patch, flies away far enough
  // that the patch leaves the working set entirely, and comes back. The FPGA
  // side is `zref::terrain::Streamer` -- the model that already owns the
  // barrier -- rather than a second slot machine written here.
  {
    isl::Directory d6(d);
    zref::terrain::Journal j6;
    sws::WorldStreamer w6(d6, j6);
    w6.configure(zref::mem::GuardRegion{static_cast<uint32_t>(kCartBase), kCartBytes},
                 zref::mem::GuardRegion{kStageBase, kStageBytes}, 7, kStageSlots);
    build(d6, w6);
    sws::Ledger L6{};
    zref::terrain::Ledger FL{};
    zref::terrain::Streamer slots(64);

    const int32_t hix = 20, hiz = 62;
    const uint32_t page_id = isl::Streamer::resource_index(d.island_id, hix, hiz);

    // Load it, deform it. The crater is the state that must survive.
    const int s0 = slots.find_free();
    check(s0 >= 0, "a free slot exists to load the deformed patch into", 1, s0 >= 0 ? 1 : 0);
    slots.begin_load(s0, page_id, &FL);
    slots.loader_write(s0, std::vector<int16_t>(64, 0), &FL);
    slots.finish_load(s0, j6, &FL);
    slots.deform(s0, 17, -900);
    slots.deform(s0, 18, -1200);
    check(slots.slot(s0).dirty_f, "deforming the patch marks F dirty", 1,
          slots.slot(s0).dirty_f ? 1 : 0);

    // THE BARRIER: the slot may not enter LOADING for a new page before F has
    // reached the journal and been acknowledged.
    check_eq(static_cast<long>(slots.begin_load(s0, page_id + 1, &FL)),
             static_cast<long>(zref::terrain::Refusal::kDirtyFNotJournalled),
             "a dirty-F slot REFUSES to start loading another page: this is the "
             "barrier that stops a crater being overwritten by the next patch");

    // SW.STREAM takes the sheet into the journal and acknowledges.
    //
    // CHECKED HERE, BEFORE THE FPGA-SIDE MODEL ALSO WRITES IT. The first
    // version of this test asserted the journal's contents only at the end,
    // after `zref::terrain::Streamer::begin_evict` had written the same entry
    // -- so deleting SW.STREAM's own journal write entirely changed nothing
    // and the fire-test reported "NOTHING FIRED". Two writers of one fact, one
    // of them under test, is not a test of that one.
    w6.note_dirty_evict(hix, hiz, slots.slot(s0).f, &L6);
    {
      const std::vector<int16_t>* immediately = w6.saved_f(hix, hiz, nullptr);
      check(immediately != nullptr,
            "SW.STREAM's OWN journal write lands the sheet in the journal -- "
            "this is the HPS half of T4's barrier and nothing else performs it",
            1, immediately ? 1 : 0);
      if (immediately) {
        check_eq((*immediately)[17], -900, "with the crater in it");
        check_eq((*immediately)[18], -1200, "both cells");
      }
      check_eq(L6.f_journalled, 1, "and the journal write is counted once");
    }
    slots.begin_evict(s0, &j6, &FL);
    check_eq(static_cast<long>(slots.begin_load(s0, page_id + 1, &FL)),
             static_cast<long>(zref::terrain::Refusal::kAwaitingAck),
             "journalled but not yet ACKed is STILL a refusal: the copy is not "
             "the acknowledgement");
    slots.ack(s0, &FL);
    w6.note_journal_ack(&L6);
    w6.note_evicted(hix, hiz);
    check_eq(static_cast<long>(slots.begin_load(s0, page_id + 1, &FL)),
             static_cast<long>(zref::terrain::Refusal::kNone),
             "and only after the ACK may the slot take a new page");

    // Fly away: 40 patches, far beyond the radius-4 window plus its ring and
    // its 30-frame prediction, so the patch genuinely leaves the working set.
    // A radius-2 window: 25 required patches, inside the 32-page budget, so
    // the patch's RETURN is decided by the working set rather than by a budget
    // cut. (At radius 4 the 81-patch window overflows the budget and the
    // returning patch simply loses the cut -- correct behaviour that would
    // have made this test report "the sheet was lost" when nothing was lost.)
    std::vector<sws::Camera> c(1);
    c[0].view = isl::View{hix, hiz, 2};
    c[0].view_bit = 1;
    c[0].vel_ix_q10 = 1024;
    for (int32_t frame = 0; frame < 40; ++frame) {
      c[0].view.centre_ix = hix + frame;
      const sws::Frame f = w6.build_frame(c, &L6);
      for (const sws::PatchRecord& r : f.list.records)
        if (r.hps_page_addr != 0) w6.note_load_complete(r.patch_ix, r.patch_iz, true);
      if (frame == 39) {
        long still_listed = 0;
        for (const sws::PatchRecord& r : f.list.records)
          if (r.patch_ix == hix && r.patch_iz == hiz) ++still_listed;
        check_eq(still_listed, 0,
                 "after 40 patches of travel the deformed patch has left the "
                 "working set entirely -- so its return below is a real return, "
                 "not a patch that never went away");
      }
    }

    // COME BACK.
    c[0].view.centre_ix = hix;
    c[0].vel_ix_q10 = 0;
    const sws::Frame back = w6.build_frame(c, &L6);

    bool found = false, has_saved_f = false;
    for (const sws::PatchRecord& r : back.list.records)
      if (r.patch_ix == hix && r.patch_iz == hiz) {
        found = true;
        has_saved_f = (r.flags & sws::kFlagHasSavedF) != 0;
      }
    check(found, "the deformed patch is back in the sealed list", 1, found ? 1 : 0);
    check(has_saved_f,
          "and its record carries HAS_SAVED_F -- the hardware is TOLD there is "
          "a sheet to restore, which is the only way it can know",
          1, has_saved_f ? 1 : 0);

    // The sheet itself, byte for byte.
    const std::vector<int16_t>* saved = w6.saved_f(hix, hiz, &L6);
    check(saved != nullptr, "the journal still holds the sheet", 1, saved ? 1 : 0);
    if (saved) {
      check_eq(static_cast<long>(saved->size()), 64, "the whole sheet, not a prefix");
      check_eq((*saved)[17], -900, "the crater's first cell survived eviction");
      check_eq((*saved)[18], -1200, "and its second");
      long nonzero = 0;
      for (int16_t v : *saved) if (v != 0) ++nonzero;
      check_eq(nonzero, 2, "and nothing else was disturbed");
    }

    // Restored into a slot on return, from the journal, exactly.
    const int s1 = slots.find_free();
    slots.begin_load(s1, page_id, &FL);
    slots.loader_write(s1, std::vector<int16_t>(64, 0), &FL);  // the flat page
    slots.finish_load(s1, j6, &FL);
    check_eq(slots.slot(s1).f[17], -900,
             "the page reloads with its crater, not flat: an F sheet restored "
             "from the journal beats the flat bytes the loader just wrote");
    check_eq(static_cast<long>(FL.f_reloaded), 1, "and the restore is counted once");
  }

  // =========================================================================
  // 7. THE UNRULED CASE IS REPORTED, NOT DECIDED.
  // =========================================================================
  // A patch the game declares gameplay-required, in a frame whose budget is
  // already gone. T7 gives ordinary misses a proxy and forbids freezing the
  // frame; it says nothing about a patch the player is standing on. The model
  // must SAY SO rather than pick a behaviour.
  {
    isl::Directory d7(d);
    zref::terrain::Journal j7;
    sws::WorldStreamer w7(d7, j7);
    w7.configure(zref::mem::GuardRegion{static_cast<uint32_t>(kCartBase), kCartBytes},
                 zref::mem::GuardRegion{kStageBase, kStageBytes}, 7, kStageSlots);
    build(d7, w7);
    w7.set_budget(1);  // one page: the second required patch cannot fit

    std::set<std::pair<int32_t, int32_t>> g;
    g.insert({100, 62});
    g.insert({101, 62});
    g.insert({102, 62});
    w7.set_gameplay_required(g);

    sws::Ledger L7{};
    std::vector<sws::Camera> c(1);
    c[0].view = isl::View{10, 62, 3};  // nowhere near the gameplay patches
    c[0].view_bit = 1;
    const sws::Frame f = w7.build_frame(c, &L7);

    check(f.unruled_gameplay_starvation,
          "a gameplay-required patch that cannot fit the page budget raises "
          "unruled_gameplay_starvation -- the model REFUSES LOUDLY instead of "
          "inventing a policy no ruling states",
          1, f.unruled_gameplay_starvation ? 1 : 0);
    check(f.gameplay_required_deferred >= 2,
          "and counts exactly how many were starved",
          2, static_cast<long>(f.gameplay_required_deferred));

    // Gameplay-required sorts ahead of ordinary required-current, which is an
    // INTERPRETATION derived from T6's ladder, not a ruling. It is asserted
    // here so that if a ruling later contradicts it, this test fails loudly
    // rather than the constant changing under a green suite.
    check_eq(f.list.records.empty() ? -1L : static_cast<long>(f.list.records[0].priority),
             static_cast<long>(sws::kPriorityGameplay),
             "the one page that DID fit is a gameplay-required one (an "
             "interpretation of T6's ladder, see kPriorityGameplay)");
  }

  std::printf("\nsw_stream_directed: %d checks, %s\n", g_checks, g_failed ? "FAILED" : "ok");
  std::printf("  ledger: frames %u, unique candidates %u, dual %u, loads %u,\n"
              "          staged %u (reused %u), proxy %u, sealed %u lists / %u bytes\n",
              L.frames, L.candidates_unique, L.dual_patches, L.loads_planned, L.staged_ok,
              L.staged_reused, L.proxy_patches, L.lists_sealed, L.list_bytes_sealed);
  return g_failed ? 1 : 0;
}
