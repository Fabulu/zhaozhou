// raster_earlyz_directed.cpp — directed vectors for RASTER.EARLYZ
// (fpga/rtl/raster/zhao_raster_earlyz.sv; contract
// design/contracts/RASTER.EARLYZ.md; ledger ZH-059).
//
// Every case drives the RTL and zref::EarlyZ through the identical fragment
// sequence and requires every decision, every carried field, both counters,
// the bin mask and the depth floor to agree. On top of that each case asserts
// its own law:
//
//   1. reset + tile_begin  — the floor starts at 0 and becomes exactly the
//                            tile's clear depth; a second tile resets it
//                            DOWNWARD as well as upward
//   2. the test is off     — spec/sky_and_beams.md 1.1's sky_backdrop, whose
//                            "Z-test off" must make rejection impossible no
//                            matter how deep the floor is
//   3. strictness          — spec/qformats.md 8's test is `d_new > d_old`, so
//                            a fragment exactly AT the floor loses. floor,
//                            floor+1 and floor-1 are all pinned
//   4. the floor rises     — only when EVERY pixel has taken a qualifying
//                            depth write, and then only to the SMALLEST of
//                            them; 255 of 256 pixels must move it not at all
//   5. qualification       — a blended, depth-write-disabled, alpha-tested or
//                            stencilled fragment is not evidence about depth
//                            and must never raise the floor
//   6. z_force_far         — sky_backdrop writes the far constant, so the
//                            evidence is 0 and not the fragment's own depth
//   7. the coarse bins     — bin = depth[23:21], the mask accumulates, and
//                            tile_begin empties it
//   8. the payload         — 88 opaque bits ride through untouched, including
//                            all-zeroes and all-ones
//   9. backpressure        — six stall patterns change not one decision, and
//                            a stream of pure rejects still retires one per
//                            clock (the ledger's "1 reject decision per clock")
//  10. counters            — covered_fragments counts every accepted fragment,
//                            early_z_rejects counts exactly the rejects

#include "raster_earlyz_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using zhao::check;
using zhao_raster::ez_describe;
using zhao_raster::ez_expect;
using zhao_raster::ez_same;
using zhao_raster::EzDecision;
using zhao_raster::EzDev;
using zhao_raster::EzFrag;
using zhao_raster::EzRun;
using zhao_raster::EzState;
using zhao_raster::kEzWords;

namespace {

EzDev& dev() {
  static EzDev d;
  return d;
}

uint32_t g_saved = 0;

// ---- the states these cases need, by name ---------------------------------
// Each is a zref::FragmentPipeline::State, so the bit positions live in one
// place and this file names recipes rather than magic numbers.

/** Depth test on, depth written, opaque: the qualifying prefill fragment. */
uint32_t st_opaque_tested() {
  EzState s;
  s.z_test_en = true;
  return s.pack();
}

/** Depth test OFF but still depth-writing and opaque: the sky prefill shape. */
uint32_t st_opaque_untested() { return EzState().pack(); }

/** The six ratified recipes, by their oracle constructors. */
uint32_t st_sky_backdrop() { return zref::FragmentPipeline::sky_backdrop().pack(); }
uint32_t st_beam() { return zref::FragmentPipeline::beam_additive_fade().pack(); }
uint32_t st_star_disc() { return zref::FragmentPipeline::star_disc_masked().pack(); }

EzFrag frag(uint8_t addr, uint32_t depth, uint32_t state) {
  EzFrag f;
  f.addr = addr;
  f.depth = depth;
  f.state = state;
  f.src_id = static_cast<uint16_t>(0x700 + addr);
  f.payload_lo = 0x0123456789ABCDEFull ^ (static_cast<uint64_t>(addr) << 40);
  f.payload_hi = 0xA5C300u ^ addr;
  return f;
}

/** Every one of the tile's 256 pixels, at one depth and one state. */
void push_full_tile(std::vector<EzFrag>* v, uint32_t depth, uint32_t state) {
  for (int i = 0; i < kEzWords; ++i) v->push_back(frag(static_cast<uint8_t>(i), depth, state));
}

// ----------------------------------------------------------- the compare ---
// Drives one sequence through both and diffs it. `ez` is passed in so a case
// can carry tile state across calls exactly as the RTL does.
bool run_seq(zref::EarlyZ& ez, const std::vector<EzFrag>& frags, uint32_t in_seed,
             uint32_t cand_seed, const char* what, EzRun* got) {
  std::string err;
  *got = dev().feed(frags, in_seed, cand_seed, &err);
  bool ok = err.empty();
  if (!ok) std::printf("  %s: protocol violation: %s\n", what, err.c_str());

  const std::vector<EzDecision> want = ez_expect(ez, frags);
  for (size_t i = 0; i < want.size(); ++i) {
    if (ez_same(want[i], got->out[i])) continue;
    ok = false;
    if (g_saved < 6) {
      const std::string body = ez_describe(i, want[i], got->out[i]);
      std::printf("  %s: %s\n", what, body.c_str());
      zhao::save_failing_vector(std::string("raster_earlyz_") + what,
                                zhao_raster::ez_serialize(frags), "zref::EarlyZ", body);
      ++g_saved;
    }
  }
  if (got->z_floor != ez.floor()) {
    ok = false;
    std::printf("  %s: floor oracle %06X rtl %06X\n", what, ez.floor(), got->z_floor);
  }
  if (got->bin_mask != ez.bin_mask()) {
    ok = false;
    std::printf("  %s: bin mask oracle %02X rtl %02X\n", what, ez.bin_mask(), got->bin_mask);
  }
  if (got->counter_rejects != ez.early_z_rejects()) {
    ok = false;
    std::printf("  %s: early_z_rejects oracle %u rtl %u\n", what, ez.early_z_rejects(),
                got->counter_rejects);
  }
  if (got->counter_covered != ez.covered_fragments()) {
    ok = false;
    std::printf("  %s: covered_fragments oracle %u rtl %u\n", what, ez.covered_fragments(),
                got->counter_covered);
  }
  return ok;
}

/** Reset both sides together, so a case starts from a known common state. */
void fresh(zref::EarlyZ* ez) {
  dev().reset();
  ez->reset();
}

void begin_tile(zref::EarlyZ* ez, uint32_t clear_depth) {
  dev().tile_begin(clear_depth);
  ez->tile_begin(clear_depth);
}

// --------------------------------------------------------------------- 1 ---
void test_reset_and_tile_begin() {
  zref::EarlyZ ez;
  fresh(&ez);

  check(dev().z_floor() == 0, "reset: the depth floor starts at the clear value 0", 0,
        dev().z_floor());
  check(dev().bin_mask() == 0, "reset: no coarse bin is occupied", 0, dev().bin_mask());

  // A tile whose clear depth is high: the floor IS that value, exactly.
  begin_tile(&ez, 0x400000u);
  check(dev().z_floor() == 0x400000u, "tile_begin: the floor becomes the tile's clear depth",
        0x400000u, dev().z_floor());

  // ...and a second tile resets it DOWNWARD. A floor that only ever rose
  // would reject the whole of the next tile's far geometry.
  begin_tile(&ez, 0x000010u);
  check(dev().z_floor() == 0x10u, "tile_begin: a new tile resets the floor DOWNWARD too", 0x10u,
        dev().z_floor());
  check(dev().bin_mask() == 0, "tile_begin: the coarse bin mask is emptied", 0, dev().bin_mask());
}

// --------------------------------------------------------------------- 2 ---
// spec/sky_and_beams.md 1.1: the pass-1 backdrop is drawn with "Z-test off".
// Whatever the floor says, it must survive - it is the thing that fills the
// tile, and a backdrop rejected by early-Z is a hole in the sky.
void test_depth_test_off_never_rejects() {
  zref::EarlyZ ez;
  fresh(&ez);
  begin_tile(&ez, 0xFFFFFFu);  // the deepest possible floor

  std::vector<EzFrag> frags;
  frags.push_back(frag(0x00, 0x000000u, st_sky_backdrop()));
  frags.push_back(frag(0x11, 0x800000u, st_sky_backdrop()));
  frags.push_back(frag(0x22, 0xFFFFFFu, st_sky_backdrop()));
  frags.push_back(frag(0x33, 0x000000u, st_opaque_untested()));
  // ...and for contrast, the SAME depth with the test on, which must die.
  frags.push_back(frag(0x44, 0x000000u, st_opaque_tested()));

  EzRun got;
  const bool ok = run_seq(ez, frags, 0u, 0u, "test-off", &got);
  check(ok, "test off: the sequence matches zref::EarlyZ", 1, ok ? 1 : 0);
  check(got.rejects == 1,
        "test off: only the ONE fragment with the depth test enabled was rejected", 1, got.rejects);
  for (size_t i = 0; i < 4; ++i)
    check(got.out[i].keep, "test off: a Z-test-off fragment survives the deepest floor", 1,
          got.out[i].keep ? 1 : 0);
}

// --------------------------------------------------------------------- 3 ---
// spec/qformats.md 8: "pass <=> d_new > d_old (strict; ties fail)". So a
// fragment exactly AT the floor is rejected and one LSB above it is kept.
// That single LSB is the whole margin stars_and_flares.md 3 relies on
// (STAR_DEPTH = "sky-prefill far + 1"), so it is pinned here on both sides.
void test_strictness_at_the_floor() {
  zref::EarlyZ ez;
  fresh(&ez);
  const uint32_t F = 0x123456u;
  begin_tile(&ez, F);

  std::vector<EzFrag> frags;
  frags.push_back(frag(0x00, F - 1u, st_opaque_tested()));  // below: rejected
  frags.push_back(frag(0x01, F, st_opaque_tested()));       // AT: rejected (ties fail)
  frags.push_back(frag(0x02, F + 1u, st_opaque_tested()));  // one LSB above: KEPT
  frags.push_back(frag(0x03, 0xFFFFFFu, st_opaque_tested()));

  EzRun got;
  const bool ok = run_seq(ez, frags, 0u, 0u, "strict", &got);
  check(ok, "strictness: the sequence matches zref::EarlyZ", 1, ok ? 1 : 0);
  check(!got.out[0].keep, "strictness: a fragment BELOW the floor is rejected", 0,
        got.out[0].keep ? 1 : 0);
  check(!got.out[1].keep, "strictness: a fragment exactly AT the floor is rejected (ties fail)", 0,
        got.out[1].keep ? 1 : 0);
  check(got.out[2].keep, "strictness: one LSB above the floor SURVIVES - the STAR_DEPTH margin", 1,
        got.out[2].keep ? 1 : 0);
  check(got.out[3].keep, "strictness: the nearest possible depth survives", 1,
        got.out[3].keep ? 1 : 0);

  // The sky-prefill / star pairing, spelled out: a backdrop at the far
  // constant 0 and a star at 1.
  fresh(&ez);
  begin_tile(&ez, 0);
  std::vector<EzFrag> sky;
  push_full_tile(&sky, 0, st_sky_backdrop());
  sky.push_back(frag(0x80, 1u, st_star_disc()));  // STAR_DEPTH: beats the sky
  sky.push_back(frag(0x81, 0u, st_star_disc()));  // at the sky's own depth: loses
  EzRun got2;
  const bool ok2 = run_seq(ez, sky, 0u, 0u, "star-vs-sky", &got2);
  check(ok2, "strictness: the sky/star pairing matches zref::EarlyZ", 1, ok2 ? 1 : 0);
  check(got2.out[kEzWords].keep, "strictness: STAR_DEPTH (far+1) beats a full sky backdrop", 1,
        got2.out[kEzWords].keep ? 1 : 0);
  check(!got2.out[kEzWords + 1].keep, "strictness: a star AT the sky's depth does not", 0,
        got2.out[kEzWords + 1].keep ? 1 : 0);
}

// --------------------------------------------------------------------- 4 ---
// THE HIERARCHICAL FLOOR. It may rise only when EVERY pixel has taken a
// qualifying depth write, and then only to the SMALLEST of those depths.
// The 255-of-256 case is the one that matters: a floor that rose on partial
// coverage would reject fragments that the exact per-pixel test would keep,
// which is the one thing this block is never allowed to do.
void test_floor_rises_only_on_full_coverage() {
  zref::EarlyZ ez;
  fresh(&ez);
  begin_tile(&ez, 0);

  // 255 of 256 pixels, all at a high depth. The floor must not move.
  std::vector<EzFrag> partial;
  for (int i = 0; i < kEzWords - 1; ++i)
    partial.push_back(frag(static_cast<uint8_t>(i), 0x900000u, st_opaque_tested()));
  EzRun got;
  bool ok = run_seq(ez, partial, 0u, 0u, "floor-partial", &got);
  check(ok, "floor: the partial sweep matches zref::EarlyZ", 1, ok ? 1 : 0);
  check(got.z_floor == 0, "floor: 255 of 256 pixels covered moves the floor NOT AT ALL", 0,
        got.z_floor);

  // A fragment behind those 255 must still be KEPT: the 256th pixel is still
  // at the clear depth, so it could legitimately win there.
  std::vector<EzFrag> behind;
  behind.push_back(frag(0xFF, 0x000005u, st_opaque_tested()));
  ok = run_seq(ez, behind, 0u, 0u, "floor-behind", &got);
  check(ok, "floor: the behind-fragment matches zref::EarlyZ", 1, ok ? 1 : 0);
  check(got.out[0].keep, "floor: a fragment behind 255 covered pixels is still KEPT", 1,
        got.out[0].keep ? 1 : 0);

  // That fragment WAS the 256th pixel and it qualifies, so the tile is now
  // fully covered and the floor rises - to the SMALLEST depth in the round,
  // which is that fragment's own 0x000005, not the 0x900000 of the other 255.
  check(got.z_floor == 0x000005u,
        "floor: full coverage raises the floor to the SMALLEST qualifying depth", 0x000005u,
        got.z_floor);

  // And now the floor really bites.
  std::vector<EzFrag> after;
  after.push_back(frag(0x00, 0x000004u, st_opaque_tested()));
  after.push_back(frag(0x01, 0x000005u, st_opaque_tested()));
  after.push_back(frag(0x02, 0x000006u, st_opaque_tested()));
  ok = run_seq(ez, after, 0u, 0u, "floor-after", &got);
  check(ok, "floor: the post-rise sequence matches zref::EarlyZ", 1, ok ? 1 : 0);
  check(!got.out[0].keep && !got.out[1].keep && got.out[2].keep,
        "floor: the raised floor rejects at and below it, and keeps just above", 1,
        (!got.out[0].keep && !got.out[1].keep && got.out[2].keep) ? 1 : 0);
  check(got.counter_rejects == 2, "floor: exactly two of those three were rejected", 2,
        got.counter_rejects);
}

// --------------------------------------------------------------------- 5 ---
// QUALIFICATION. A fragment that might not write depth is no evidence about
// depth. Each of the four disqualifiers gets its own full-tile sweep, and
// none of them may move the floor by one LSB.
void test_only_certain_writers_raise_the_floor() {
  struct Case {
    const char* name;
    uint32_t state;
  };
  // beam_additive_fade is blended AND depth-write-disabled; star_disc_masked
  // is alpha-tested; the two hand-built states isolate the remaining
  // disqualifiers one at a time.
  EzState blended;
  blended.z_test_en = true;
  blended.blend = zref::FragmentPipeline::kAlpha;

  EzState nowrite;
  nowrite.z_test_en = true;
  nowrite.z_write_dis = true;

  EzState masked;
  masked.z_test_en = true;
  masked.atest_en = true;
  masked.atest_ref = 0;

  EzState stencilled;
  stencilled.z_test_en = true;
  stencilled.sten_func = zref::FragmentPipeline::kEqual;

  const Case cases[5] = {{"blended (not opaque)", blended.pack()},
                         {"depth writes disabled", nowrite.pack()},
                         {"alpha-tested (could be masked away)", masked.pack()},
                         {"stencilled (could be stencilled away)", stencilled.pack()},
                         {"beam_additive_fade", st_beam()}};

  for (const Case& c : cases) {
    zref::EarlyZ ez;
    fresh(&ez);
    begin_tile(&ez, 0);
    std::vector<EzFrag> frags;
    push_full_tile(&frags, 0x700000u, c.state);
    EzRun got;
    const bool ok = run_seq(ez, frags, 0u, 0u, "qualify", &got);
    check(ok, "qualification: the full-tile sweep matches zref::EarlyZ", 1, ok ? 1 : 0);
    if (got.z_floor != 0) std::printf("  qualification: %s moved the floor\n", c.name);
    check(got.z_floor == 0,
          "qualification: a fragment that might not write depth never raises the floor", 0,
          got.z_floor);
  }

  // The control: the SAME sweep with a qualifying state does raise it, so the
  // four checks above are not passing because nothing ever raises the floor.
  zref::EarlyZ ez;
  fresh(&ez);
  begin_tile(&ez, 0);
  std::vector<EzFrag> frags;
  push_full_tile(&frags, 0x700000u, st_opaque_tested());
  EzRun got;
  const bool ok = run_seq(ez, frags, 0u, 0u, "qualify-control", &got);
  check(ok, "qualification: the control sweep matches zref::EarlyZ", 1, ok ? 1 : 0);
  check(got.z_floor == 0x700000u,
        "qualification: the control - an opaque depth-writing sweep DOES raise the floor",
        0x700000u, got.z_floor);
}

// --------------------------------------------------------------------- 6 ---
// spec/sky_and_beams.md 1.1: the backdrop's "Z-write = far constant". The
// evidence it contributes is the depth it WRITES (0), not the depth it
// carries - otherwise a backdrop tagged with a near depth would raise the
// floor above everything the tile is about to draw and erase the frame.
void test_z_force_far_contributes_the_written_depth() {
  zref::EarlyZ ez;
  fresh(&ez);
  begin_tile(&ez, 0);

  std::vector<EzFrag> frags;
  push_full_tile(&frags, 0xFFFFFFu, st_sky_backdrop());  // carries NEAR, writes FAR
  EzRun got;
  const bool ok = run_seq(ez, frags, 0u, 0u, "force-far", &got);
  check(ok, "force-far: the sweep matches zref::EarlyZ", 1, ok ? 1 : 0);
  check(got.z_floor == 0,
        "force-far: a Z-forced-far backdrop raises the floor to 0, not to its carried depth", 0,
        got.z_floor);

  // Everything drawn after it still survives, which is the point.
  std::vector<EzFrag> after;
  after.push_back(frag(0x10, 1u, st_star_disc()));
  after.push_back(frag(0x11, 0x0F0000u, st_opaque_tested()));
  EzRun got2;
  const bool ok2 = run_seq(ez, after, 0u, 0u, "force-far-after", &got2);
  check(ok2, "force-far: the follow-on matches zref::EarlyZ", 1, ok2 ? 1 : 0);
  check(got2.out[0].keep && got2.out[1].keep,
        "force-far: geometry drawn over the backdrop is not rejected", 1,
        (got2.out[0].keep && got2.out[1].keep) ? 1 : 0);
}

// --------------------------------------------------------------------- 7 ---
// THE COARSE TRANSPARENT-DEPTH BINS. bin = depth[23:21], 8 of them, 7 nearest.
// Every bin is visited, the per-tile mask accumulates exactly the bins that
// SURVIVED, and tile_begin empties it.
void test_coarse_depth_bins() {
  zref::EarlyZ ez;
  fresh(&ez);
  begin_tile(&ez, 0);

  std::vector<EzFrag> frags;
  for (int b = 0; b < 8; ++b) {
    const uint32_t d = (static_cast<uint32_t>(b) << 21) | 0x000123u;
    frags.push_back(frag(static_cast<uint8_t>(b), d, st_opaque_untested()));
  }
  EzRun got;
  const bool ok = run_seq(ez, frags, 0u, 0u, "bins", &got);
  check(ok, "bins: the sweep matches zref::EarlyZ", 1, ok ? 1 : 0);

  bool all = true;
  for (int b = 0; b < 8; ++b)
    if (got.out[b].bin != static_cast<uint8_t>(b)) all = false;
  check(all, "bins: bin == depth[23:21] for all eight bins", 1, all ? 1 : 0);
  check(got.bin_mask == 0xFF, "bins: the per-tile mask accumulates every bin visited", 0xFF,
        got.bin_mask);

  // A REJECTED fragment must not mark its bin: it contributes nothing to the
  // translucent pass, so the scheduler must not be told its bin is occupied.
  fresh(&ez);
  begin_tile(&ez, 0xE00000u);
  std::vector<EzFrag> mixed;
  mixed.push_back(frag(0x00, 0x200000u, st_opaque_tested()));  // bin 1, rejected
  mixed.push_back(frag(0x01, 0xE00001u, st_opaque_tested()));  // bin 7, kept
  EzRun got2;
  const bool ok2 = run_seq(ez, mixed, 0u, 0u, "bins-reject", &got2);
  check(ok2, "bins: the reject/keep pair matches zref::EarlyZ", 1, ok2 ? 1 : 0);
  check(got2.bin_mask == 0x80, "bins: a rejected fragment does not occupy its bin", 0x80,
        got2.bin_mask);

  begin_tile(&ez, 0);
  check(dev().bin_mask() == 0, "bins: tile_begin empties the mask", 0, dev().bin_mask());
}

// --------------------------------------------------------------------- 8 ---
// THE PAYLOAD IS OPAQUE. 88 bits of shading data ride through untouched; the
// block decodes only the address, the depth and six state bits. Both rails
// are driven, because a passthrough that dropped or forced a bit would
// otherwise hide behind a payload that happened not to use it.
void test_payload_passthrough() {
  zref::EarlyZ ez;
  fresh(&ez);
  begin_tile(&ez, 0);

  std::vector<EzFrag> frags;
  EzFrag zero = frag(0x00, 0x010000u, st_opaque_untested());
  zero.payload_lo = 0;
  zero.payload_hi = 0;
  frags.push_back(zero);

  EzFrag ones = frag(0x01, 0x020000u, st_opaque_untested());
  ones.payload_lo = 0xFFFFFFFFFFFFFFFFull;
  ones.payload_hi = 0xFFFFFFu;
  frags.push_back(ones);

  EzFrag walk = frag(0x02, 0x030000u, st_opaque_untested());
  walk.payload_lo = 0x8000000000000001ull;
  walk.payload_hi = 0x800001u;
  frags.push_back(walk);

  EzRun got;
  const bool ok = run_seq(ez, frags, 0u, 0u, "payload", &got);
  check(ok, "payload: the sequence matches zref::EarlyZ, payload included", 1, ok ? 1 : 0);
  check(got.out[0].payload_lo == 0 && got.out[0].payload_hi == 0, "payload: all-zero rides through",
        1, (got.out[0].payload_lo == 0 && got.out[0].payload_hi == 0) ? 1 : 0);
  check(got.out[1].payload_lo == 0xFFFFFFFFFFFFFFFFull && got.out[1].payload_hi == 0xFFFFFFu,
        "payload: all-ones rides through - no bit is dropped or forced", 1,
        (got.out[1].payload_lo == 0xFFFFFFFFFFFFFFFFull && got.out[1].payload_hi == 0xFFFFFFu) ? 1
                                                                                               : 0);
  check(got.out[2].payload_lo == 0x8000000000000001ull && got.out[2].payload_hi == 0x800001u,
        "payload: both rails of the 88-bit field survive", 1,
        (got.out[2].payload_lo == 0x8000000000000001ull && got.out[2].payload_hi == 0x800001u) ? 1
                                                                                               : 0);
  // The state and src_id ride out too - the fragment block reads both.
  check(got.out[1].state == st_opaque_untested() && got.out[1].src_id == frags[1].src_id,
        "payload: the state word and source_id ride out unaltered", 1,
        (got.out[1].state == st_opaque_untested() && got.out[1].src_id == frags[1].src_id) ? 1 : 0);
}

// --------------------------------------------------------------------- 9 ---
// BACKPRESSURE. Six stall patterns on the candidate channel and on the offer;
// they may cost cycles and may not change one decision. Then the throughput
// claim the ledger makes: a stream of pure REJECTS retires one per clock,
// because a reject leaves the output stage empty and never queues.
void test_backpressure_and_reject_throughput() {
  std::vector<EzFrag> frags;
  for (int i = 0; i < 200; ++i) {
    const uint32_t d = 0x100000u + static_cast<uint32_t>(i) * 0x3331u;
    frags.push_back(
        frag(static_cast<uint8_t>(i * 7), d, (i & 1) ? st_opaque_tested() : st_opaque_untested()));
  }

  const uint32_t seeds[6][2] = {{0u, 0u},          {0u, 0x1234u},  {0x9ABCu, 0u},
                                {0x9ABCu, 0xDEFu}, {0x55u, 0xAAu}, {0x7F1Eu, 0x3C2Du}};
  EzRun base;
  bool all = true;
  for (int k = 0; k < 6; ++k) {
    zref::EarlyZ ez;
    fresh(&ez);
    begin_tile(&ez, 0x300000u);
    EzRun got;
    const bool ok = run_seq(ez, frags, seeds[k][0], seeds[k][1], "backpressure", &got);
    if (!ok) all = false;
    if (k == 0) {
      base = got;
    } else {
      for (size_t i = 0; i < frags.size(); ++i)
        if (!ez_same(base.out[i], got.out[i])) all = false;
      if (got.z_floor != base.z_floor || got.bin_mask != base.bin_mask) all = false;
    }
  }
  check(all, "backpressure: six stall patterns match the oracle and each other exactly", 1,
        all ? 1 : 0);

  // "1 reject decision per clock": a floor above everything makes every
  // fragment a reject, and 256 rejects offered back to back must cost 256
  // accept cycles plus a small constant - not one per handshake round trip.
  zref::EarlyZ ez;
  fresh(&ez);
  begin_tile(&ez, 0xFFFFFFu);
  std::vector<EzFrag> all_reject;
  push_full_tile(&all_reject, 0x800000u, st_opaque_tested());
  EzRun got;
  const bool ok = run_seq(ez, all_reject, 0u, 0u, "reject-rate", &got);
  check(ok, "reject rate: the all-reject sweep matches zref::EarlyZ", 1, ok ? 1 : 0);
  check(got.rejects == static_cast<uint32_t>(kEzWords), "reject rate: all 256 were rejected",
        kEzWords, got.rejects);
  std::printf("raster_earlyz reject rate: %u rejects in %u cycles\n", got.rejects, got.cycles);
  check(got.cycles <= static_cast<uint32_t>(kEzWords) + 4u,
        "reject rate: 256 rejects retire in 256 cycles plus the fixed:1 tail", kEzWords + 4,
        got.cycles);
}

// -------------------------------------------------------------------- 10 ---
// A REJECTED FRAGMENT MUST NOT REACH THE ACCUMULATOR AT ALL.
//
// Found by tools/sweep_raster_earlyz.sh: E12 drops `!reject_c` from
// hiz_qualify and survived every case in this file.
//
// It is safe in the depth sense -- a rejected fragment has depth <= floor, it
// enters a MIN, so acc_min can never exceed the floor and the `> floor` guard
// blocks any rise. The floor cannot move backwards. What DOES change is
// coverage: the rejected fragment marks its pixel, so the accumulator fills
// and RESETS early, discarding evidence the oracle still holds. The next
// legitimate round then fails to raise the floor, and the reject count
// diverges a whole tile later.
//
// Reaching it needs two phases, which is why nothing here had: the floor must
// be RAISED first, because with the clear depth at 0 and larger-is-closer
// almost nothing is rejected at the start of a tile.
void test_rejected_fragment_never_accumulates() {
  zref::EarlyZ ez;
  fresh(&ez);
  begin_tile(&ez, 0x000000u);

  const uint32_t MID = 0x400000u;

  // Phase 1: cover the whole tile with qualifying opaque fragments at MID, so
  // the floor rises to MID and the accumulator resets clean.
  std::vector<EzFrag> phase1;
  push_full_tile(&phase1, MID, st_opaque_tested());
  EzRun got1;
  check(run_seq(ez, phase1, 0x5EEDu, 0xB17Eu, "reject-acc phase 1", &got1),
        "E12: the floor rises to MID on a full opaque cover", 1, 1);
  check(ez.floor() == MID, "E12: oracle floor is MID after the cover", MID, ez.floor());

  // Phase 2: fragments that are REJECTED (depth <= floor) but otherwise
  // qualify in every way -- opaque, depth-writing, no alpha test, stencil
  // ALWAYS. These are exactly the fragments E12 lets into the accumulator.
  // Half the tile, so the accumulator is left partly marked rather than
  // completing, which is where the two behaviours separate.
  std::vector<EzFrag> phase2;
  for (int i = 0; i < kEzWords / 2; ++i) {
    phase2.push_back(frag(static_cast<uint8_t>(i), MID - 1u, st_opaque_tested()));
  }
  // Then a genuine, deeper-than-floor cover of the WHOLE tile. Under the law
  // this completes an accumulation and raises the floor again; with E12 the
  // accumulator was already polluted and the outcome differs.
  push_full_tile(&phase2, MID + 0x10000u, st_opaque_tested());

  EzRun got2;
  check(run_seq(ez, phase2, 0xC0DEu, 0xFACEu, "reject-acc phase 2", &got2),
        "E12: a rejected fragment contributes nothing to the floor evidence", 1, 1);
  check(ez.floor() == MID + 0x10000u, "E12: the later legitimate cover still raises the floor",
        MID + 0x10000u, ez.floor());
}

void test_counters() {
  zref::EarlyZ ez;
  fresh(&ez);
  begin_tile(&ez, 0x200000u);

  std::vector<EzFrag> frags;
  uint32_t want_rejects = 0;
  for (int i = 0; i < 300; ++i) {
    const uint32_t d = (i % 3 == 0) ? 0x100000u : 0x300000u;  // a third are behind the floor
    if (i % 3 == 0) ++want_rejects;
    frags.push_back(frag(static_cast<uint8_t>(i * 11), d, st_opaque_tested()));
  }

  EzRun got;
  const bool ok = run_seq(ez, frags, 0x2222u, 0x3333u, "counters", &got);
  check(ok, "counters: the sequence matches zref::EarlyZ", 1, ok ? 1 : 0);
  check(got.counter_covered == frags.size(),
        "counters: covered_fragments counts every ACCEPTED fragment, kept or killed", frags.size(),
        got.counter_covered);
  check(got.counter_rejects == want_rejects,
        "counters: early_z_rejects counts exactly the rejected ones", want_rejects,
        got.counter_rejects);
  check(got.counter_rejects == got.rejects,
        "counters: the counter agrees with the z_reject_o pulses observed", got.rejects,
        got.counter_rejects);
  check(want_rejects > 0 && want_rejects < frags.size(),
        "counters: the batch really did both keep and reject", 1,
        (want_rejects > 0 && want_rejects < frags.size()) ? 1 : 0);
}

}  // namespace

int main() {
  test_reset_and_tile_begin();
  test_depth_test_off_never_rejects();
  test_strictness_at_the_floor();
  test_floor_rises_only_on_full_coverage();
  test_only_certain_writers_raise_the_floor();
  test_z_force_far_contributes_the_written_depth();
  test_coarse_depth_bins();
  test_payload_passthrough();
  test_backpressure_and_reject_throughput();
  test_rejected_fragment_never_accumulates();
  test_counters();
  return zhao::report_and_exit("raster_earlyz_directed");
}
