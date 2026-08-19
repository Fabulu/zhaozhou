// terrain_bake_random.cpp — randomized differential for TERRAIN.BAKE.
//
// TWO LANES, because one uniform lane would test almost only the second one:
//
//   Lane A, ISLAND-SHAPED. Authored relief of a few tens of metres over a
//   §3.7 deep keel, small pre-existing scars, authored void bites, no_bake
//   plinths, and stencils the size of a real Erupt or Quake dug in 1..5
//   successive steps — the incremental Volcano cadence terrain_rules §9
//   describes. This is the regime the clamps and the breach law actually
//   decide in, and it must NEVER hit a height16 rail: real terrain does not
//   rail.
//
//   Lane B, DOMAIN-LIMIT. Both height16 rails on every plane, depths at the
//   fx16 word extremes, radii from 1 raw unit to 2^27, inverted envelopes,
//   legacy pages, cell-less pages, and cell bytes drawn over all four
//   substances with random flags. The point is that every rail rails exactly
//   where `bake_dig`'s does.
//
// THE BOUNDARIES ARE CONSTRUCTED, NOT HOPED FOR. Uniform random centres and
// radii essentially never produce `d2 == r*r`, `d2 == 0` or a corner sitting
// exactly on the §3.4 equality, and this tree has now watched four increments
// pass a differential while the coverage counter for the deciding case read
// zero. So both lanes deliberately snap the stencil centre onto a lattice
// vertex (which is the ONLY way d2 == 0, hence s == 65,536, ever happens) and
// set the radius to an exact axis-aligned lattice distance (which is the ONLY
// way d2 == r*r ever happens), and both assert they reached those states.

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

constexpr int32_t kM = 1 << 16;

struct Stats {
  uint32_t patches = 0;
  uint32_t bakes = 0;
  uint64_t touched = 0;
  uint32_t breaches = 0;       // SOLID -> VOID_BREACHED transitions
  uint32_t heals = 0;          // VOID_BREACHED -> SOLID transitions
  uint32_t clamps = 0;         // the no_bake clamp fired
  uint32_t rails = 0;          // a height16 rail or an fx16 rail was recorded
  uint32_t centre_exact = 0;   // the stencil centre landed ON a lattice vertex
  uint32_t edge_exact = 0;     // a vertex sat at d2 == r*r EXACTLY
  uint32_t edge_inside = 0;    // ... and one raw unit of radius brought it in
  uint32_t idle_stamps = 0;    // from == to
  uint32_t inverted = 0;       // an inverted envelope
  uint32_t legacy = 0;         // a page with no layer C
  uint32_t cellless = 0;       // layer C but no layer D
  uint32_t authored_void = 0;  // a VOID_AUTHORED cell was in the sweep
  uint32_t invert_probe = 0;   // radius-1 stencil on a snapped inverted-envelope vertex
};

/** The placed position of one lattice line — the REFERENCE's own lerp. */
int32_t lat_pos(int32_t a, int32_t b, int idx) { return zt::lattice_lerp(a, b, idx, 32); }

/** Compare one bake against `bake_dig` + `apply_breach_law`, bit for bit. */
void one_bake(Vzhao_terrain_bake& dut, zref::render::TerrainPatch& cur, const bdev::StampRec& st,
              Stats& s, int stall_mod) {
  std::vector<zt::BreachEvent> ev;
  const zref::render::TerrainPatch ref = bdev::oracle_bake(cur, st, &ev);
  const bdev::BakeOut got = bdev::run_bake(dut, cur, st, stall_mod);
  check(!got.timed_out, "the bake completes without a handshake hang", 0, got.timed_out ? 1 : 0);

  int bad = 0;
  for (int k = 0; k < bdev::kVerts; ++k)
    if (got.scar[static_cast<size_t>(k)] != ref.scar[static_cast<size_t>(k)]) ++bad;
  if (bad != 0) {
    std::vector<uint8_t> vec(28);
    std::memcpy(vec.data(), &st.cx, 4);
    std::memcpy(vec.data() + 4, &st.cz, 4);
    std::memcpy(vec.data() + 8, &st.radius, 4);
    std::memcpy(vec.data() + 12, &st.depth_from, 4);
    std::memcpy(vec.data() + 16, &st.depth_to, 4);
    std::memcpy(vec.data() + 20, &cur.env_x0, 4);
    std::memcpy(vec.data() + 24, &cur.env_x1, 4);
    zhao::save_failing_vector("terrain_bake_random_scar", vec, "bake_dig layer B",
                              "zhao_terrain_bake layer B");
  }
  check(bad == 0, "every layer-B word matches bake_dig", 0, static_cast<uint64_t>(bad));

  const bool cells_ran = cur.bottom.size() == static_cast<size_t>(bdev::kVerts) &&
                         cur.cell_state.size() == static_cast<size_t>(bdev::kCellCount);
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
    for (size_t e = 0; e < n; ++e)
      if (got.events[e].ci != ev[e].ci || got.events[e].cj != ev[e].cj ||
          got.events[e].state != ev[e].state)
        ++bade;
    check(bade == 0, "the transitions match in cell scan order", 0, static_cast<uint64_t>(bade));
    for (size_t e = 0; e < got.events.size(); ++e) {
      if (got.events[e].state == zt::kVoidBreached) ++s.breaches;
      if (got.events[e].state == zt::kSolid) ++s.heals;
    }
    cur.cell_state = got.cell_state;
  }
  cur.scar = got.scar;

  // the counter is a fact, not a mood: it must equal the covered-vertex count
  uint32_t covered = 0;
  for (int k = 0; k < bdev::kVerts; ++k) covered += got.touched[static_cast<size_t>(k)] ? 1u : 0u;
  check(got.texels_touched == covered,
        "surface_texels_touched counts exactly the vertices inside the stencil", covered,
        got.texels_touched);

  s.touched += covered;
  s.clamps += got.nobake_clamps;
  s.rails += got.saturations;
  ++s.bakes;
  if (st.depth_from == st.depth_to) ++s.idle_stamps;
}

void run_lane(Vzhao_terrain_bake& dut, bdev::Rng& rng, int patches, bool island, Stats& s) {
  for (int p = 0; p < patches; ++p) {
    ++s.patches;
    zref::render::TerrainPatch cur;
    cur.width = bdev::kLat;
    cur.height = bdev::kLat;
    cur.heights.assign(bdev::kVerts, 0);
    cur.scar.assign(bdev::kVerts, 0);

    // ---- the envelope --------------------------------------------------
    if (island) {
      cur.env_x0 = rng.range(-2000, 2000) * kM;
      cur.env_z0 = rng.range(-2000, 2000) * kM;
      cur.env_x1 = cur.env_x0 + 64 * kM;  // the frozen 2 m pitch page
      cur.env_z1 = cur.env_z0 + 64 * kM;
    } else {
      cur.env_x0 = rng.range(-(1 << 26), 1 << 26);
      cur.env_z0 = rng.range(-(1 << 26), 1 << 26);
      const bool invert = rng.chance(4);
      const int32_t span = rng.range(1, 1 << 26);
      cur.env_x1 = invert ? (cur.env_x0 - span) : (cur.env_x0 + span);
      cur.env_z1 = cur.env_z0 + rng.range(1, 1 << 26);
      if (invert) ++s.inverted;
    }

    // ---- the planes ------------------------------------------------------
    const bool dual = island ? true : !rng.chance(4);
    const bool cells = island ? true : (dual && !rng.chance(4));
    if (!dual) ++s.legacy;
    if (dual && !cells) ++s.cellless;
    if (dual) cur.bottom.assign(bdev::kVerts, 0);
    if (cells) cur.cell_state.assign(bdev::kCellCount, zt::kSolid);

    for (int j = 0; j < bdev::kLat; ++j) {
      for (int i = 0; i < bdev::kLat; ++i) {
        const size_t k = static_cast<size_t>(j) * bdev::kLat + i;
        if (island) {
          const int dxi = i - 16, dzj = j - 16;
          const int d2 = dxi * dxi + dzj * dzj;
          const int32_t rel = static_cast<int32_t>((512 - d2) * rng.range(3, 8));
          cur.heights[k] = static_cast<int16_t>(rel);
          cur.bottom[k] = static_cast<int16_t>(rel - 50 * 256 + d2 * rng.range(2, 6));
          cur.scar[k] = static_cast<int16_t>(-rng.range(0, 400));
        } else {
          cur.heights[k] = static_cast<int16_t>(rng.range(-32768, 32767));
          if (dual) cur.bottom[k] = static_cast<int16_t>(rng.range(-32768, 32767));
          cur.scar[k] = static_cast<int16_t>(rng.range(-32768, 32767));
        }
      }
    }
    // CONSTRUCTED: put some corners EXACTLY on the §3.4 breach equality
    // (base + scar == bottom). Uniform 16-bit planes hit that with
    // probability 2^-16 per vertex; the breach law turns on nothing else.
    if (cells) {
      // Whole CELLS are snapped onto the equality, not scattered corners: the
      // §3.4 law needs ALL FOUR, so a per-corner sprinkle births nothing and
      // the coverage counter reads zero while the differential stays green —
      // which is the exact failure mode this tree has hit four times.
      const int hits = rng.range(6, 40);
      for (int t = 0; t < hits; ++t) {
        const int ci = rng.range(0, 31), cj = rng.range(0, 31);
        const bool near_miss = rng.chance(4);
        const int spoil = rng.range(0, 3);
        for (int corner = 0; corner < 4; ++corner) {
          const int i = ci + (corner & 1), j = cj + ((corner >> 1) & 1);
          const size_t k = static_cast<size_t>(j) * bdev::kLat + i;
          const int32_t want =
              cur.bottom[k] - cur.heights[k] + ((near_miss && corner == spoil) ? 1 : 0);
          if (want >= -32768 && want <= 32767) cur.scar[k] = static_cast<int16_t>(want);
        }
        // a third of the snapped cells are PROTECTED, so digging through one
        // fires the no_bake clamp instead of a breach
        if (rng.chance(3))
          cur.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] =
              static_cast<uint8_t>(zt::kSolid | zt::kNoBakeBit);
      }
      // some already-breached cells so the HEAL arm has something to heal,
      // some authored void, some protected plinths
      for (int t = 0; t < rng.range(2, 24); ++t) {
        const size_t c = static_cast<size_t>(rng.range(0, bdev::kCellCount - 1));
        const int pick = rng.range(0, 3);
        if (pick == 0)
          cur.cell_state[c] = zt::kVoidBreached;
        else if (pick == 1) {
          cur.cell_state[c] = zt::kVoidAuthored;
          ++s.authored_void;
        } else if (pick == 2)
          cur.cell_state[c] = static_cast<uint8_t>(zt::kSolid | zt::kNoBakeBit);
        else
          cur.cell_state[c] = static_cast<uint8_t>(rng.range(0, 255));
      }
    }

    // ---- 1..5 successive bakes, the incremental cadence -----------------
    const int nbakes = island ? rng.range(1, 5) : rng.range(1, 3);
    int32_t depth = 0;
    for (int b = 0; b < nbakes; ++b) {
      bdev::StampRec st;
      st.patch_id = static_cast<uint16_t>(rng.range(0, 65535));
      st.src_id = static_cast<uint16_t>(rng.range(0, 65535));

      // CONSTRUCTION 1: snap the centre onto a lattice vertex often, which is
      // the ONLY way d2 == 0 (and therefore s == 65,536) is ever reached.
      const bool snap = rng.chance(2);
      const int si = rng.range(0, 32), sj = rng.range(0, 32);
      if (snap) {
        st.cx = lat_pos(cur.env_x0, cur.env_x1, si);
        st.cz = lat_pos(cur.env_z0, cur.env_z1, sj);
      } else if (island) {
        st.cx = cur.env_x0 + rng.range(-16, 80) * kM / 2;
        st.cz = cur.env_z0 + rng.range(-16, 80) * kM / 2;
      } else {
        st.cx = rng.range(-(1 << 27), 1 << 27);
        st.cz = rng.range(-(1 << 27), 1 << 27);
      }

      // CONSTRUCTION 2: an EXACT axis-aligned lattice distance as the radius,
      // which is the only way d2 == r*r ever happens. `bake_dig`'s test is
      // `d2 >= r2 -> skip`, so that vertex must be OUTSIDE.
      int probe_i = -1, probe_j = -1;
      bool probe_expect_in = false;
      if (snap && rng.chance(2)) {
        const int di = rng.range(1, 8) * (rng.chance(2) ? 1 : -1);
        const int ti = si + di;
        if (ti >= 0 && ti <= 32) {
          const int64_t d = static_cast<int64_t>(lat_pos(cur.env_x0, cur.env_x1, ti)) - st.cx;
          const int64_t mag = d < 0 ? -d : d;
          if (mag > 0 && mag < (1 << 27)) {
            probe_expect_in = rng.chance(2);
            st.radius = static_cast<int32_t>(mag) + (probe_expect_in ? 1 : 0);
            probe_i = ti;
            probe_j = sj;
          }
        }
      }
      // CONSTRUCTION 3: on an INVERTED envelope a radius-1 stencil on a
      // snapped centre is FATAL to a lerp that floors instead of truncating —
      // the one raw unit is the whole difference. The mutation sweep found the
      // hole: before this, flooring `lattice_lerp` cost the random lanes
      // exactly ONE check out of 874, because a one-unit vertex shift under a
      // large stencil changes no scar word at all.
      const bool inverted_env = cur.env_x1 < cur.env_x0;
      if (!island && snap && inverted_env && rng.chance(2)) {
        st.radius = 1;
        probe_i = -1;
        ++s.invert_probe;
      } else if (probe_i < 0) {
        // a third of the island bakes cover the WHOLE page, which is what puts
        // the snapped cells (and their no_bake plinths) under the stencil
        if (island)
          st.radius = rng.chance(3) ? 50 * kM : rng.range(1, 40) * kM / 2;
        else
          st.radius = rng.chance(6) ? rng.range(-(1 << 27), 0) : rng.range(1, 1 << 27);
      }

      // depth: the island ramps monotonically down like a Volcano; the domain
      // lane takes the fx16 word.
      st.depth_from = depth;
      if (island) {
        depth = depth + rng.range(0, 6) * kM;  // 0 sometimes: the idle stamp
      } else {
        depth = static_cast<int32_t>(rng.next());
        if (rng.chance(8)) depth = st.depth_from;
      }
      st.depth_to = depth;

      one_bake(dut, cur, st, s, rng.chance(3) ? 3 : 0);

      if (snap && st.radius > 0) ++s.centre_exact;
      if (probe_i >= 0) {
        // Re-derive the verdict the block gave for the probe vertex.
        const bdev::BakeOut chk = bdev::run_bake(dut, cur, st, 0);
        const size_t k = static_cast<size_t>(probe_j) * bdev::kLat + probe_i;
        if (probe_expect_in) {
          ++s.edge_inside;
          check(chk.touched[k] != 0,
                "one raw unit past the exact lattice distance brings the vertex INSIDE", 1,
                chk.touched[k]);
        } else {
          ++s.edge_exact;
          check(chk.touched[k] == 0, "a vertex at d2 == r*r EXACTLY is outside the stencil", 0,
                chk.touched[k]);
        }
        // the probe bake is idempotent in `from == to` terms only, so restore
        // the running state from it rather than double-applying
        cur.scar = chk.scar;
        if (chk.breach_ran) cur.cell_state = chk.cell_state;
      }
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  Vzhao_terrain_bake dut;
  bdev::reset_dut(dut);

  const int n_island = nightly ? 300 : 26;
  const int n_limit = nightly ? 240 : 20;

  Stats a, b;
  bdev::Rng rng_a(0xBA4E'0001ULL);
  bdev::Rng rng_b(0xBA4E'0002ULL);
  run_lane(dut, rng_a, n_island, true, a);
  run_lane(dut, rng_b, n_limit, false, b);

  std::printf(
      "[terrain_bake] lane A (island): %u patches, %u bakes, %llu texels, %u breaches, %u heals,"
      " %u clamps, %u rails, %u centre-exact, %u edge-exact, %u edge-inside, %u idle\n",
      a.patches, a.bakes, static_cast<unsigned long long>(a.touched), a.breaches, a.heals, a.clamps,
      a.rails, a.centre_exact, a.edge_exact, a.edge_inside, a.idle_stamps);
  std::printf(
      "[terrain_bake] lane B (limit):  %u patches, %u bakes, %llu texels, %u breaches, %u heals,"
      " %u rails, %u inverted, %u legacy, %u cell-less, %u authored-void, %u invert-probe\n",
      b.patches, b.bakes, static_cast<unsigned long long>(b.touched), b.breaches, b.heals, b.rails,
      b.inverted, b.legacy, b.cellless, b.authored_void, b.invert_probe);

  // ---- each lane must have REACHED what it exists for --------------------
  check(a.touched > 4000, "lane A dug real ground", 1, a.touched > 4000 ? 1 : 0);
  check(a.breaches > 0, "lane A birthed breaches", 1, a.breaches > 0 ? 1 : 0);
  check(a.heals > 0, "lane A healed breached cells", 1, a.heals > 0 ? 1 : 0);
  check(a.clamps > 0, "lane A fired the no_bake clamp", 1, a.clamps > 0 ? 1 : 0);
  check(a.centre_exact > 0, "lane A placed centres exactly on lattice vertices (s == 65,536)", 1,
        a.centre_exact > 0 ? 1 : 0);
  check(a.edge_exact > 0, "lane A probed d2 == r*r exactly", 1, a.edge_exact > 0 ? 1 : 0);
  check(a.edge_inside > 0, "lane A probed one raw unit inside that edge", 1,
        a.edge_inside > 0 ? 1 : 0);
  check(a.idle_stamps > 0, "lane A issued idle (from == to) stamps", 1, a.idle_stamps > 0 ? 1 : 0);
  check(a.rails == 0, "lane A NEVER railed — an authored island does not saturate height16", 0,
        a.rails);

  check(b.rails > 0, "lane B railed", 1, b.rails > 0 ? 1 : 0);
  check(b.inverted > 0, "lane B ran inverted envelopes", 1, b.inverted > 0 ? 1 : 0);
  check(b.legacy > 0, "lane B ran legacy pages", 1, b.legacy > 0 ? 1 : 0);
  check(b.cellless > 0, "lane B ran pages with layer C and no layer D", 1, b.cellless > 0 ? 1 : 0);
  check(b.authored_void > 0, "lane B put VOID_AUTHORED cells under the stencil", 1,
        b.authored_void > 0 ? 1 : 0);
  check(b.invert_probe > 0,
        "lane B probed an inverted envelope with a radius-1 stencil on a snapped vertex", 1,
        b.invert_probe > 0 ? 1 : 0);
  check(b.breaches > 0, "lane B birthed breaches", 1, b.breaches > 0 ? 1 : 0);
  check(b.heals > 0, "lane B healed breached cells", 1, b.heals > 0 ? 1 : 0);

  const int rc = zhao::report_and_exit("terrain_bake_random");
  zhao::exit_hard(rc);
}
