// terrain_bake_directed.cpp — directed tests for TERRAIN.BAKE.
//
// The oracle is `zref::terrain::bake_dig` followed by
// `zref::terrain::apply_breach_law` — the executed reference itself, not a
// second implementation of it (charter §29-6). Everything below either drives
// a boundary the law actually has, or drives a boundary the reference's own
// comments flag as having bitten.
//
// THE BOUNDARIES ARE CONSTRUCTED, NEVER SAMPLED. `d2 == r2` (the stencil's
// closed/open edge), `d2 == 0` (s at its 65,536 maximum), the exact height16
// rails, the no_bake clamp's `bottom + 1 - base`, a breach that needs its
// FOURTH corner, and the truncating envelope divide's one-raw-unit
// disagreement with an arithmetic shift are all built by construction here.
// Four increments running in this tree have watched uniform random input miss
// exactly this class of event.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_bake.h"

#include "bake_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_terrain.hpp"

using zhao::check;
namespace zt = zref::terrain;

namespace {

constexpr int32_t kM = 1 << 16;     // one world metre in fx16 raw
constexpr int32_t kSpan = 64 * kM;  // a 64 m patch: 2 m pitch over 32 cells
constexpr int32_t kPitch = 2 * kM;  // the lattice spacing that follows

int32_t g_stat_touched = 0;
int32_t g_stat_events = 0;
int32_t g_stat_clamps = 0;
int32_t g_stat_rails = 0;

/**
 * An authored island patch: a domed relief over a deep keel, a ring of
 * authored void at the rim, one no_bake plinth, and small pre-existing scars.
 * The regime terrain_rules §3.7's derivation says a real patch is in.
 */
zref::render::TerrainPatch make_island() {
  zref::render::TerrainPatch p = bdev::make_patch(0, 0, kSpan);
  p.bottom.assign(bdev::kVerts, 0);
  p.cell_state.assign(bdev::kCellCount, zt::kSolid);
  for (int j = 0; j < bdev::kLat; ++j) {
    for (int i = 0; i < bdev::kLat; ++i) {
      const size_t k = static_cast<size_t>(j) * bdev::kLat + i;
      const int dx = i - 16, dz = j - 16;
      const int d2 = dx * dx + dz * dz;
      // relief in the 0.25 m authoring grid: height16 is S 1.7.8 metres
      const int32_t rel = static_cast<int32_t>((512 - d2) * 6);  // ~ +/- 12 m
      p.heights[k] = static_cast<int16_t>(rel);
      p.bottom[k] = static_cast<int16_t>(rel - (50 * 256) + d2 * 4);  // a deep keel
      p.scar[k] = static_cast<int16_t>(-(d2 % 7) * 32);
    }
  }
  // an authored void bite in one corner, and a protected plinth in the middle
  for (int cj = 0; cj < 3; ++cj)
    for (int ci = 0; ci < 3; ++ci)
      p.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] = zt::kVoidAuthored;
  p.cell_state[static_cast<size_t>(16) * bdev::kCells + 16] |= zt::kNoBakeBit;
  return p;
}

/** Bit-for-bit comparison of one bake against the reference. */
void compare(Vzhao_terrain_bake& dut, const zref::render::TerrainPatch& p, const bdev::StampRec& st,
             const char* what, int stall_mod = 0) {
  std::vector<zt::BreachEvent> ev;
  const zref::render::TerrainPatch ref = bdev::oracle_bake(p, st, &ev);
  const bdev::BakeOut got = bdev::run_bake(dut, p, st, stall_mod);
  check(!got.timed_out, "the bake completes without a handshake hang", 0, got.timed_out ? 1 : 0);

  int bad = 0;
  for (int k = 0; k < bdev::kVerts; ++k)
    if (got.scar[static_cast<size_t>(k)] != ref.scar[static_cast<size_t>(k)]) ++bad;
  check(bad == 0, what, 0, static_cast<uint64_t>(bad));

  const bool cells_ran = p.bottom.size() == static_cast<size_t>(bdev::kVerts) &&
                         p.cell_state.size() == static_cast<size_t>(bdev::kCellCount);
  check(got.breach_ran == cells_ran, "the breach phase runs iff layers C and D are both present",
        cells_ran ? 1 : 0, got.breach_ran ? 1 : 0);
  if (cells_ran) {
    int badc = 0;
    for (int k = 0; k < bdev::kCellCount; ++k)
      if (got.cell_state[static_cast<size_t>(k)] != ref.cell_state[static_cast<size_t>(k)]) ++badc;
    check(badc == 0, "every layer-D cell matches apply_breach_law", 0, static_cast<uint64_t>(badc));
    check(got.events.size() == ev.size(), "the transition count matches apply_breach_law",
          static_cast<uint64_t>(ev.size()), static_cast<uint64_t>(got.events.size()));
    const size_t n = got.events.size() < ev.size() ? got.events.size() : ev.size();
    int bade = 0;
    for (size_t e = 0; e < n; ++e) {
      if (got.events[e].ci != ev[e].ci || got.events[e].cj != ev[e].cj ||
          got.events[e].state != ev[e].state)
        ++bade;
    }
    check(bade == 0, "the transitions match in cell scan order, cell for cell and state for state",
          0, static_cast<uint64_t>(bade));
  }
  g_stat_touched += static_cast<int32_t>(got.texels_touched);
  g_stat_events += static_cast<int32_t>(got.breach_events);
  g_stat_clamps += static_cast<int32_t>(got.nobake_clamps);
  g_stat_rails += static_cast<int32_t>(got.saturations);
}

// ---------------------------------------------------------------------------
// 1. the reference cross-check on a real island patch
// ---------------------------------------------------------------------------
void test_island(Vzhao_terrain_bake& dut) {
  const zref::render::TerrainPatch p = make_island();
  bdev::StampRec st;
  st.patch_id = 0x2C01;
  st.cx = 30 * kM;
  st.cz = 34 * kM;
  st.radius = 11 * kM;
  st.depth_from = 0;
  st.depth_to = 4 * kM;  // dig 4 m down
  st.src_id = 0x51;
  compare(dut, p, st, "every layer-B word matches bake_dig over a real island patch");

  const bdev::BakeOut got = bdev::run_bake(dut, p, st, 0);
  // The sweep is the block's own, so the order it REQUESTS is a fact worth
  // pinning: z-then-x, every vertex once, every cell once.
  check(got.vtx_order.size() == static_cast<size_t>(bdev::kVerts),
        "the dig phase requests exactly 1,089 vertices", static_cast<uint64_t>(bdev::kVerts),
        static_cast<uint64_t>(got.vtx_order.size()));
  int oob = 0;
  for (size_t k = 0; k < got.vtx_order.size(); ++k)
    if (got.vtx_order[k] != static_cast<uint32_t>(k)) ++oob;
  check(oob == 0, "the vertex sweep is z-then-x scan order with no gaps and no repeats", 0,
        static_cast<uint64_t>(oob));
  check(got.cell_order.size() == static_cast<size_t>(bdev::kCellCount),
        "the breach phase requests exactly 1,024 cells", static_cast<uint64_t>(bdev::kCellCount),
        static_cast<uint64_t>(got.cell_order.size()));
  int oobc = 0;
  for (size_t k = 0; k < got.cell_order.size(); ++k)
    if (got.cell_order[k] != static_cast<uint32_t>(k)) ++oobc;
  check(oobc == 0, "the cell sweep is z-then-x scan order with no gaps and no repeats", 0,
        static_cast<uint64_t>(oobc));

  // Backpressure on all four handshakes must not change a single word.
  const bdev::BakeOut slow = bdev::run_bake(dut, p, st, 3);
  int diff = 0;
  for (int k = 0; k < bdev::kVerts; ++k)
    if (slow.scar[static_cast<size_t>(k)] != got.scar[static_cast<size_t>(k)]) ++diff;
  for (int k = 0; k < bdev::kCellCount; ++k)
    if (slow.cell_state[static_cast<size_t>(k)] != got.cell_state[static_cast<size_t>(k)]) ++diff;
  check(diff == 0, "stalling every handshake changes nothing in layers B or D", 0,
        static_cast<uint64_t>(diff));
}

// ---------------------------------------------------------------------------
// 2. the stencil edge: d2 == r*r is OUTSIDE, constructed exactly
// ---------------------------------------------------------------------------
void test_stencil_edge(Vzhao_terrain_bake& dut) {
  zref::render::TerrainPatch p = bdev::make_patch(0, 0, kSpan);
  // A flat patch so the only thing that can move a scar word is the stencil.
  bdev::StampRec st;
  st.cx = 4 * kM;  // exactly lattice vertex i = 2
  st.cz = 4 * kM;  // exactly lattice vertex j = 2
  st.depth_from = 0;
  st.depth_to = 8 * kM;

  // radius == the exact lattice pitch: the neighbour sits at d2 == r*r, which
  // `bake_dig`'s `if (d2 >= r2) continue` EXCLUDES. One raw unit more includes
  // it. This is the whole closed/open decision, on the nose.
  st.radius = kPitch;
  bdev::BakeOut a = bdev::run_bake(dut, p, st, 0);
  const size_t centre = static_cast<size_t>(2) * bdev::kLat + 2;
  const size_t east = centre + 1;
  check(a.touched[centre] != 0, "the vertex at d2 == 0 is inside the stencil", 1,
        a.touched[centre]);
  check(a.touched[east] == 0, "the vertex at d2 == r*r EXACTLY is outside (the >= is not a >)", 0,
        a.touched[east]);
  check(a.scar[east] == 0, "an excluded vertex keeps its layer-B word untouched", 0,
        static_cast<uint64_t>(static_cast<uint16_t>(a.scar[east])));
  compare(dut, p, st, "the exact-edge stencil matches bake_dig everywhere");

  st.radius = kPitch + 1;
  bdev::BakeOut b = bdev::run_bake(dut, p, st, 0);
  check(b.touched[east] != 0, "one raw unit more radius brings the edge vertex inside", 1,
        b.touched[east]);
  // s at that vertex is 1 raw Q16 unit or so, and the delta rounds to zero:
  // being INSIDE is still observable through the counter, which is why the
  // counter is checked and not just the value.
  check(b.texels_touched == a.texels_touched + 4,
        "the four axis neighbours cross the edge together (the stencil is radial)",
        a.texels_touched + 4, b.texels_touched);
  compare(dut, p, st, "one unit past the edge matches bake_dig everywhere");

  // d2 == 0 puts s at its structural maximum, 65,536 — the divider's top
  // quotient bit, and the only input for which the 17th bit is ever set.
  st.radius = 8 * kM;
  bdev::BakeOut c = bdev::run_bake(dut, p, st, 0);
  zref::SatLedger L;
  const int32_t g_from = zref::rescale_s32(zref::rescale_s32(0LL * 65536, 16, &L), 8, &L);
  const int32_t g_to =
      zref::rescale_s32(zref::rescale_s32(static_cast<int64_t>(8 * kM) * 65536, 16, &L), 8, &L);
  check(c.scar[centre] == static_cast<int16_t>(g_from - g_to),
        "s == 65,536 at the centre: the scar word is exactly g(from) - g(to)",
        static_cast<uint64_t>(static_cast<uint16_t>(static_cast<int16_t>(g_from - g_to))),
        static_cast<uint64_t>(static_cast<uint16_t>(c.scar[centre])));

  // radius 1: only a vertex at distance ZERO can be inside.
  st.radius = 1;
  bdev::BakeOut d1 = bdev::run_bake(dut, p, st, 0);
  check(d1.texels_touched == 1, "radius 1 touches exactly the one coincident vertex", 1,
        d1.texels_touched);
  compare(dut, p, st, "radius 1 matches bake_dig");
}

// ---------------------------------------------------------------------------
// 3. radius <= 0, and the idle stamp
// ---------------------------------------------------------------------------
void test_degenerate(Vzhao_terrain_bake& dut) {
  zref::render::TerrainPatch p = make_island();
  bdev::StampRec st;
  st.cx = 32 * kM;
  st.cz = 32 * kM;
  st.depth_from = 0;
  st.depth_to = 30 * kM;

  for (int32_t r : {0, -1, -(9 * kM)}) {
    st.radius = r;
    const bdev::BakeOut got = bdev::run_bake(dut, p, st, 0);
    int moved = 0;
    for (int k = 0; k < bdev::kVerts; ++k)
      if (got.scar[static_cast<size_t>(k)] != p.scar[static_cast<size_t>(k)]) ++moved;
    check(moved == 0, "radius <= 0 writes not one layer-B word (bake_dig returns first)", 0,
          static_cast<uint64_t>(moved));
    check(got.texels_touched == 0, "radius <= 0 touches no texel", 0, got.texels_touched);
    check(got.breach_ran, "radius <= 0 still runs the breach law — the caller does (chosen B5)", 1,
          got.breach_ran ? 1 : 0);
    compare(dut, p, st, "radius <= 0 matches bake_dig + apply_breach_law");
  }

  // from == to: an idle stamp writes nothing, everywhere, at every stencil
  // value. This is the interruption law's fixed point.
  st.radius = 20 * kM;
  st.depth_from = 7 * kM + 12345;
  st.depth_to = 7 * kM + 12345;
  const bdev::BakeOut idle = bdev::run_bake(dut, p, st, 0);
  int moved = 0;
  for (int k = 0; k < bdev::kVerts; ++k)
    if (idle.scar[static_cast<size_t>(k)] != p.scar[static_cast<size_t>(k)]) ++moved;
  check(moved == 0, "from == to writes nothing even where the stencil covers", 0,
        static_cast<uint64_t>(moved));
  check(idle.texels_touched > 250,
        "the idle stamp still COVERED the whole stencil disc, so it is not vacuous", 1,
        idle.texels_touched > 250 ? 1 : 0);
}

// ---------------------------------------------------------------------------
// 4. the §9 incremental-scaling identities, on the real block
// ---------------------------------------------------------------------------
void test_incremental(Vzhao_terrain_bake& dut) {
  // A patch chosen so NOTHING clamps: no no_bake bit, no rail, deep keel.
  zref::render::TerrainPatch p = bdev::make_patch(0, 0, kSpan);
  p.bottom.assign(bdev::kVerts, -30000);
  p.cell_state.assign(bdev::kCellCount, zt::kSolid);
  for (int k = 0; k < bdev::kVerts; ++k) {
    p.heights[static_cast<size_t>(k)] = static_cast<int16_t>((k % 97) * 3);
    p.scar[static_cast<size_t>(k)] = static_cast<int16_t>(-(k % 13) * 5);
  }

  bdev::StampRec st;
  st.cx = 31 * kM;
  st.cz = 29 * kM;
  st.radius = 17 * kM;

  // one shot: 0 -> 6 m
  st.depth_from = 0;
  st.depth_to = 6 * kM;
  const bdev::BakeOut one = bdev::run_bake(dut, p, st, 0);

  // stepped: 0 -> 1.5 -> 3.7 -> 6 m, each step feeding the next
  zref::render::TerrainPatch stepped = p;
  const int32_t stops[3] = {(3 * kM) / 2, (37 * kM) / 10, 6 * kM};
  int32_t from = 0;
  for (int s = 0; s < 3; ++s) {
    st.depth_from = from;
    st.depth_to = stops[s];
    const bdev::BakeOut r = bdev::run_bake(dut, stepped, st, 0);
    stepped.scar = r.scar;
    from = stops[s];
  }
  int bad = 0;
  for (int k = 0; k < bdev::kVerts; ++k)
    if (stepped.scar[static_cast<size_t>(k)] != one.scar[static_cast<size_t>(k)]) ++bad;
  check(bad == 0,
        "§9.2 law 3: a stepped ramp telescopes BIT-EXACTLY to the one-shot bake "
        "(this is what makes the deferral state-exact)",
        0, static_cast<uint64_t>(bad));

  // un-apply: 0 -> 6 then 6 -> 0 returns layer B to where it started.
  st.depth_from = 6 * kM;
  st.depth_to = 0;
  zref::render::TerrainPatch back = p;
  back.scar = one.scar;
  const bdev::BakeOut undo = bdev::run_bake(dut, back, st, 0);
  int bad2 = 0;
  for (int k = 0; k < bdev::kVerts; ++k)
    if (undo.scar[static_cast<size_t>(k)] != p.scar[static_cast<size_t>(k)]) ++bad2;
  check(bad2 == 0, "§9: an interrupted cast un-applies cleanly — from->to->from is the identity", 0,
        static_cast<uint64_t>(bad2));

  // and it is not vacuous: the one-shot must actually have moved the ground.
  int moved = 0;
  for (int k = 0; k < bdev::kVerts; ++k)
    if (one.scar[static_cast<size_t>(k)] != p.scar[static_cast<size_t>(k)]) ++moved;
  check(moved > 150, "the telescoping cases moved hundreds of vertices, so they are not vacuous", 1,
        moved > 150 ? 1 : 0);
}

// ---------------------------------------------------------------------------
// 5. the height16 rails and the no_bake clamp
// ---------------------------------------------------------------------------
void test_clamps(Vzhao_terrain_bake& dut) {
  zref::render::TerrainPatch p = bdev::make_patch(0, 0, kSpan);
  p.bottom.assign(bdev::kVerts, -32768);
  p.cell_state.assign(bdev::kCellCount, zt::kSolid);
  for (int k = 0; k < bdev::kVerts; ++k) p.scar[static_cast<size_t>(k)] = -30000;

  bdev::StampRec st;
  st.cx = 32 * kM;
  st.cz = 32 * kM;
  st.radius = 40 * kM;  // covers the whole patch
  st.depth_from = 0;
  st.depth_to = 120 * kM;  // far past the height16 floor
  const bdev::BakeOut got = bdev::run_bake(dut, p, st, 0);
  check(got.scar[static_cast<size_t>(16) * bdev::kLat + 16] == -32768,
        "the height16 low rail clamps at -32768 exactly", static_cast<uint64_t>(0xFFFF8000u),
        static_cast<uint64_t>(static_cast<uint16_t>(got.scar[static_cast<size_t>(16) * 33 + 16])));
  check(got.saturations > 0, "a railed write is counted", 1, got.saturations > 0 ? 1 : 0);
  compare(dut, p, st, "the low rail matches bake_dig everywhere");

  // the high rail: dig UP (a negative depth delta raises the scar)
  for (int k = 0; k < bdev::kVerts; ++k) p.scar[static_cast<size_t>(k)] = 30000;
  st.depth_from = 120 * kM;
  st.depth_to = 0;
  compare(dut, p, st, "the high rail matches bake_dig everywhere");

  // ---- the no_bake clamp, and the §3.3 corner shadow --------------------
  zref::render::TerrainPatch q = bdev::make_patch(0, 0, kSpan);
  q.bottom.assign(bdev::kVerts, -100);
  q.cell_state.assign(bdev::kCellCount, zt::kSolid);
  for (int k = 0; k < bdev::kVerts; ++k) q.heights[static_cast<size_t>(k)] = 200;
  // one protected cell in the middle of the stencil
  q.cell_state[static_cast<size_t>(16) * bdev::kCells + 16] |= zt::kNoBakeBit;

  bdev::StampRec s2;
  s2.cx = 32 * kM;
  s2.cz = 32 * kM;
  s2.radius = 30 * kM;
  s2.depth_from = 0;
  s2.depth_to = 90 * kM;  // dig far below the bottom
  const bdev::BakeOut g2 = bdev::run_bake(dut, q, s2, 0);
  // min_scar = bottom + 1 - base = -100 + 1 - 200 = -299 on the four corners
  for (int j = 16; j <= 17; ++j) {
    for (int i = 16; i <= 17; ++i) {
      const size_t k = static_cast<size_t>(j) * bdev::kLat + i;
      check(g2.scar[k] == -299, "a protected cell's corner clamps at bottom + 1 - base",
            static_cast<uint64_t>(static_cast<uint16_t>(static_cast<int16_t>(-299))),
            static_cast<uint64_t>(static_cast<uint16_t>(g2.scar[k])));
      check(g2.clamped[k] != 0, "the clamp is reported on the vertex it fired on", 1,
            g2.clamped[k]);
      check(g2.meets[k] == 0, "a clamped vertex can never satisfy the §3.4 equality", 0,
            g2.meets[k]);
    }
  }
  check(g2.nobake_clamps == 4, "exactly the four corner vertices clamped", 4, g2.nobake_clamps);
  compare(dut, q, s2, "the no_bake clamp matches bake_dig everywhere");

  // the §3.3 halo: the protected cell does not breach, and NEITHER do the
  // eight cells sharing a corner vertex with it — protection is one cell wider
  // in each direction, which is deduced law, not new law.
  std::vector<zt::BreachEvent> ev;
  const zref::render::TerrainPatch ref = bdev::oracle_bake(q, s2, &ev);
  int halo_breached = 0, far_breached = 0;
  for (int cj = 0; cj < bdev::kCells; ++cj) {
    for (int ci = 0; ci < bdev::kCells; ++ci) {
      const uint8_t sub = static_cast<uint8_t>(
          g2.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] & zt::kSubstanceMask);
      const bool halo = ci >= 15 && ci <= 17 && cj >= 15 && cj <= 17;
      if (halo && sub == zt::kVoidBreached) ++halo_breached;
      if (!halo && sub == zt::kVoidBreached) ++far_breached;
    }
  }
  check(halo_breached == 0, "the protected cell and its whole corner halo stay SOLID", 0,
        static_cast<uint64_t>(halo_breached));
  check(far_breached > 100,
        "and the unprotected ground around them DID breach, so the halo "
        "check is not vacuous",
        1, far_breached > 100 ? 1 : 0);
  check(ref.cell_state == std::vector<uint8_t>(g2.cell_state.begin(), g2.cell_state.end()),
        "layer D matches apply_breach_law over the halo case", 1,
        ref.cell_state == std::vector<uint8_t>(g2.cell_state.begin(), g2.cell_state.end()) ? 1 : 0);
}

// ---------------------------------------------------------------------------
// 6. the breach law's four arms, each by construction
// ---------------------------------------------------------------------------
void test_breach_law(Vzhao_terrain_bake& dut) {
  // A patch where NOTHING is dug (radius 0) so layer D is decided purely by
  // the pre-existing base/scar/bottom relation — the cleanest way to place a
  // corner exactly on, and exactly one LSB off, the §3.4 equality.
  zref::render::TerrainPatch p = bdev::make_patch(0, 0, kSpan);
  p.bottom.assign(bdev::kVerts, 0);
  p.cell_state.assign(bdev::kCellCount, zt::kSolid);
  for (int k = 0; k < bdev::kVerts; ++k) {
    p.heights[static_cast<size_t>(k)] = 1000;
    p.scar[static_cast<size_t>(k)] = -900;  // composed 100 > bottom 0: solid
  }
  bdev::StampRec st;
  st.radius = 0;  // decide layer D alone

  // (a) three corners meeting is NOT a breach; the fourth is.
  const int ci = 5, cj = 7;
  const size_t c00 = static_cast<size_t>(cj) * bdev::kLat + ci;
  const size_t c10 = c00 + 1;
  const size_t c01 = c00 + bdev::kLat;
  const size_t c11 = c01 + 1;
  p.scar[c00] = -1000;  // composed 0 == bottom 0: meets
  p.scar[c10] = -1000;
  p.scar[c01] = -1000;
  p.scar[c11] = -999;  // composed 1 > bottom: does NOT meet, by ONE LSB
  bdev::BakeOut a = bdev::run_bake(dut, p, st, 0);
  check((a.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] & zt::kSubstanceMask) ==
            zt::kSolid,
        "three corners at the equality and one ONE LSB above is not a breach", zt::kSolid,
        a.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] & zt::kSubstanceMask);
  check(a.events.empty(), "and it emits no transition", 0, static_cast<uint64_t>(a.events.size()));

  p.scar[c11] = -1000;  // now all four meet, exactly
  bdev::BakeOut b = bdev::run_bake(dut, p, st, 0);
  check((b.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] & zt::kSubstanceMask) ==
            zt::kVoidBreached,
        "the FOURTH corner reaching the equality births the breach", zt::kVoidBreached,
        b.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] & zt::kSubstanceMask);
  check(b.events.size() == 1, "exactly one transition is emitted", 1,
        static_cast<uint64_t>(b.events.size()));
  if (b.events.size() == 1) {
    check(b.events[0].ci == ci && b.events[0].cj == cj, "the transition names the right cell",
          static_cast<uint64_t>(ci), static_cast<uint64_t>(b.events[0].ci));
    check(b.events[0].state == zt::kVoidBreached, "the transition carries VOID_BREACHED",
          zt::kVoidBreached, b.events[0].state);
  }
  // the equality is CLOSED at <=, not just ==: a corner BELOW the bottom also
  // meets (the §3.4 clamp is what makes them the same fact)
  p.scar[c11] = -1200;
  compare(dut, p, st, "a corner strictly below the underside still meets the equality");

  // (b) the heal arm.
  zref::render::TerrainPatch h = p;
  h.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] = zt::kVoidBreached;
  h.scar[c11] = -999;  // one corner strictly above again
  bdev::BakeOut hb = bdev::run_bake(dut, h, st, 0);
  check((hb.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] & zt::kSubstanceMask) ==
            zt::kSolid,
        "a breached cell with ANY corner strictly above heals to SOLID", zt::kSolid,
        hb.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] & zt::kSubstanceMask);
  check(hb.events.size() == 1 && hb.events[0].state == zt::kSolid,
        "the heal is emitted as a transition too", zt::kSolid,
        hb.events.empty() ? 255 : hb.events[0].state);

  // (c) VOID_AUTHORED never becomes ground and never emits.
  zref::render::TerrainPatch v = p;
  v.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] = zt::kVoidAuthored;
  v.scar[c11] = -999;
  bdev::BakeOut vb = bdev::run_bake(dut, v, st, 0);
  check(vb.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] == zt::kVoidAuthored,
        "VOID_AUTHORED never becomes ground", zt::kVoidAuthored,
        vb.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci]);
  check(vb.events.empty(), "and never emits a transition", 0,
        static_cast<uint64_t>(vb.events.size()));

  // (d) a no_bake SOLID cell at the equality does NOT breach; a no_bake
  //     VOID_BREACHED cell CAN heal — the reference's asymmetry, kept.
  zref::render::TerrainPatch n = p;
  n.scar[c11] = -1000;
  n.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] =
      static_cast<uint8_t>(zt::kSolid | zt::kNoBakeBit);
  bdev::BakeOut nb = bdev::run_bake(dut, n, st, 0);
  check((nb.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] & zt::kSubstanceMask) ==
            zt::kSolid,
        "a no_bake cell whose four corners all meet still does not breach", zt::kSolid,
        nb.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] & zt::kSubstanceMask);
  n.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] =
      static_cast<uint8_t>(zt::kVoidBreached | zt::kNoBakeBit);
  n.scar[c11] = -999;
  bdev::BakeOut nh = bdev::run_bake(dut, n, st, 0);
  check((nh.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] & zt::kSubstanceMask) ==
            zt::kSolid,
        "a no_bake cell that IS breached can still heal (the heal arm ignores no_bake)", zt::kSolid,
        nh.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] & zt::kSubstanceMask);
  check((nh.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] & zt::kNoBakeBit) != 0,
        "and the flag bits above the substance field survive the rewrite", 1,
        (nh.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] & zt::kNoBakeBit) ? 1 : 0);

  // (e) substance 3 is reserved: unchanged, no transition.
  zref::render::TerrainPatch r = p;
  r.scar[c11] = -1000;
  r.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] = 0xFB;  // sub = 3, flags set
  bdev::BakeOut rb = bdev::run_bake(dut, r, st, 0);
  check(rb.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] == 0xFB,
        "the reserved substance 3 passes through untouched", 0xFB,
        rb.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci]);
  compare(dut, r, st, "the reserved substance matches apply_breach_law");
}

// ---------------------------------------------------------------------------
// 7. the pages that cannot breach
// ---------------------------------------------------------------------------
void test_pages(Vzhao_terrain_bake& dut) {
  zref::render::TerrainPatch legacy = bdev::make_patch(0, 0, kSpan);
  for (int k = 0; k < bdev::kVerts; ++k) legacy.heights[static_cast<size_t>(k)] = 500;
  bdev::StampRec st;
  st.cx = 32 * kM;
  st.cz = 32 * kM;
  st.radius = 25 * kM;
  st.depth_from = 0;
  st.depth_to = 20 * kM;
  const bdev::BakeOut lg = bdev::run_bake(dut, legacy, st, 0);
  check(!lg.breach_ran, "a legacy page (no layer C) skips the breach phase entirely", 0,
        lg.breach_ran ? 1 : 0);
  check(lg.texels_touched > 400, "but the dig still ran and touched the page", 1,
        lg.texels_touched > 400 ? 1 : 0);
  check(lg.nobake_clamps == 0, "and the no_bake clamp cannot fire without layers C and D", 0,
        lg.nobake_clamps);
  compare(dut, legacy, st, "the legacy page matches bake_dig");

  zref::render::TerrainPatch nocells = legacy;
  nocells.bottom.assign(bdev::kVerts, -20000);
  const bdev::BakeOut nc = bdev::run_bake(dut, nocells, st, 0);
  check(!nc.breach_ran, "a page with layer C but no layer D skips the breach phase too", 0,
        nc.breach_ran ? 1 : 0);
  compare(dut, nocells, st, "the cell-less page matches bake_dig");
}

// ---------------------------------------------------------------------------
// 8. the truncating envelope divide — CONSTRUCTED to disagree with a shift
// ---------------------------------------------------------------------------
void test_inverted_envelope(Vzhao_terrain_bake& dut) {
  // env_x1 < env_x0 makes `lattice_lerp`'s span negative, and its `/ 32` then
  // TRUNCATES TOWARD ZERO where an arithmetic shift would floor. At i = 1:
  //   span = -(64 << 16) = -4194304; (span * 1 + 16) = -4194288
  //   trunc(-4194288 / 32) = -131071        (what the reference does)
  //   floor(-4194288 / 32) = -131072        (what a >>> 5 would do)
  // Placing a radius-1 stencil on -131071 therefore touches exactly one vertex
  // if the divide truncates and NOTHING if it floors. One raw unit, and it is
  // the difference between agreeing with the software console and not.
  zref::render::TerrainPatch p = bdev::make_patch(0, 0, kSpan);
  p.env_x1 = -kSpan;
  for (int k = 0; k < bdev::kVerts; ++k) p.heights[static_cast<size_t>(k)] = 100;

  bdev::StampRec st;
  st.cx = -131071;
  st.cz = 0;
  st.radius = 1;
  st.depth_from = 0;
  st.depth_to = 40 * kM;
  const bdev::BakeOut got = bdev::run_bake(dut, p, st, 0);
  check(got.texels_touched == 1,
        "the inverted-envelope lerp TRUNCATES toward zero: the vertex at -131071 is hit", 1,
        got.texels_touched);
  check(got.touched[1] != 0, "and it is lattice vertex (1, 0)", 1, got.touched[1]);

  st.cx = -131072;
  const bdev::BakeOut floored = bdev::run_bake(dut, p, st, 0);
  check(floored.texels_touched == 0,
        "the floored position is NOT a lattice vertex, so nothing is touched there", 0,
        floored.texels_touched);

  // and the whole inverted patch still matches, vertex for vertex
  st.cx = -32 * kM;
  st.radius = 15 * kM;
  compare(dut, p, st, "an inverted envelope matches bake_dig over the whole lattice");
}

// ---------------------------------------------------------------------------
// 9. the §9.2 cadence budget
// ---------------------------------------------------------------------------
void test_budget(Vzhao_terrain_bake& dut) {
  bdev::reset_dut(dut);
  zref::render::TerrainPatch p = bdev::make_patch(0, 0, kSpan);
  bdev::StampRec st;
  st.radius = 0;  // the cheapest possible bake: sweep, write nothing

  dut.frame_start_i = 1;
  zhao::tick(dut);
  dut.frame_start_i = 0;
  dut.eval();
  check(dut.budget_full_o == 0, "a fresh frame window accepts bakes", 0, dut.budget_full_o);

  for (int i = 0; i < 64; ++i) {
    dut.eval();
    check(dut.cmd_ready_o != 0, "the block accepts the first 64 patch-bakes of a frame", 1,
          dut.cmd_ready_o);
    (void)bdev::run_bake(dut, p, st, 0, false);
  }
  dut.eval();
  check(dut.bakes_this_frame_o == 64, "the window counted exactly 64 acceptances", 64,
        dut.bakes_this_frame_o);
  check(dut.budget_full_o != 0, "BAKE_PATCH_BUDGET = 64 closes the window", 1, dut.budget_full_o);

  // A 65th record is REFUSED, not dropped: valid stays high and ready stays
  // low, so the record is still at the head of the upstream queue (chosen B4).
  dut.cmd_valid_i = 1;
  dut.cmd_radius_i = 0;
  dut.cmd_dual_i = 0;
  dut.cmd_cells_i = 0;
  for (int i = 0; i < 50; ++i) {
    dut.eval();
    check(dut.cmd_ready_o == 0, "the 65th record is refused for the whole rest of the frame", 0,
          dut.cmd_ready_o);
    zhao::tick(dut);
  }
  dut.frame_start_i = 1;
  zhao::tick(dut);
  dut.frame_start_i = 0;
  dut.eval();
  check(dut.cmd_ready_o != 0, "the next frame window takes the deferred record first", 1,
        dut.cmd_ready_o);
  check(dut.bakes_this_frame_o == 0, "and the window counter restarted", 0, dut.bakes_this_frame_o);
  zhao::tick(dut);  // accept it
  dut.cmd_valid_i = 0;
  dut.eval();
  check(dut.bakes_this_frame_o == 1, "the carried record is the new frame's first acceptance", 1,
        dut.bakes_this_frame_o);
  // drain it
  dut.sc_ready_i = 1;
  dut.vtx_valid_i = 1;
  for (int i = 0; i < 5000 && dut.idle_o == 0; ++i) zhao::tick(dut);
  dut.vtx_valid_i = 0;
  dut.sc_ready_i = 0;
  bdev::reset_dut(dut);
}

// ---------------------------------------------------------------------------
// 10. the measured rate — reported, not derived
// ---------------------------------------------------------------------------
void test_throughput(Vzhao_terrain_bake& dut) {
  zref::render::TerrainPatch p = make_island();
  bdev::StampRec st;
  st.cx = 32 * kM;
  st.cz = 32 * kM;
  st.depth_from = 0;
  st.depth_to = 5 * kM;

  st.radius = 1;  // no vertex covered but the coincident one
  const bdev::BakeOut cheap = bdev::run_bake(dut, p, st, 0);
  st.radius = 100 * kM;  // every vertex covered
  const bdev::BakeOut dear = bdev::run_bake(dut, p, st, 0);
  check(dear.texels_touched == static_cast<uint32_t>(bdev::kVerts),
        "the saturated case really did cover all 1,089 vertices",
        static_cast<uint64_t>(bdev::kVerts), dear.texels_touched);

  const double per_uncovered = static_cast<double>(cheap.cycles_dig) / bdev::kVerts;
  const double per_covered = static_cast<double>(dear.cycles_dig) / bdev::kVerts;
  std::printf(
      "[terrain_bake] MEASURED dig rate: %.2f clocks/vertex uncovered, %.2f clocks/vertex "
      "covered (ledger target: 1 bake texel per clock)\n",
      per_uncovered, per_covered);
  std::printf("[terrain_bake] MEASURED breach rate: %.2f clocks/cell\n",
              static_cast<double>(dear.cycles_total - dear.cycles_dig) / bdev::kCellCount);
  check(per_covered < 21.0, "the covered rate is the 17-step divide plus its two handshake cycles",
        1, per_covered < 21.0 ? 1 : 0);
  check(per_uncovered < 3.0, "an uncovered vertex costs the handshake only", 1,
        per_uncovered < 3.0 ? 1 : 0);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_bake dut;
  bdev::reset_dut(dut);

  check(dut.idle_o != 0, "the block is idle out of reset", 1, dut.idle_o);
  check(dut.surface_texels_touched_o == 0, "reset clears the texel counter", 0,
        dut.surface_texels_touched_o);

  test_island(dut);
  test_stencil_edge(dut);
  test_degenerate(dut);
  test_incremental(dut);
  test_clamps(dut);
  test_breach_law(dut);
  test_pages(dut);
  test_inverted_envelope(dut);
  test_budget(dut);
  test_throughput(dut);

  std::printf(
      "[terrain_bake] directed coverage: %d texels touched, %d transitions, %d no_bake clamps, "
      "%d railed writes\n",
      g_stat_touched, g_stat_events, g_stat_clamps, g_stat_rails);
  check(g_stat_events > 0, "the directed suite actually reached the breach law", 1,
        g_stat_events > 0 ? 1 : 0);
  check(g_stat_clamps > 0, "the directed suite actually reached the no_bake clamp", 1,
        g_stat_clamps > 0 ? 1 : 0);
  check(g_stat_rails > 0, "the directed suite actually reached a height16 rail", 1,
        g_stat_rails > 0 ? 1 : 0);

  const int rc = zhao::report_and_exit("terrain_bake_directed");
  zhao::exit_hard(rc);
}
