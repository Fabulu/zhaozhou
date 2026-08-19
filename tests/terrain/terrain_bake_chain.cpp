// terrain_bake_chain.cpp — TERRAIN.BAKE -> TERRAIN.PATCH, both blocks REAL.
//
// This is the `baked_scars` seam: `design/blocks.yml` gives TERRAIN.PATCH
// `inputs: [dispatch, field_results, baked_scars]` with `upstream: [...,
// TERRAIN.BAKE]`, and until this increment layer B had no producer in RTL at
// all — TERRAIN.PATCH's contract records "Integration capture cases: none yet"
// for exactly that reason. So the two blocks are run against each other rather
// than each against its own model.
//
// WHAT THIS CATCHES THAT NEITHER STANDALONE SUITE CAN. Composition has found
// four real defects in this tree (a 16x tile-index-versus-pixel error, a
// transposed neighbour index both blocks agreed on, a dropped slot field that
// decayed to slot 0, and a page-2 write pointer that emitted nothing), and the
// shape of all four was "both blocks self-consistent, the pair wrong". Here the
// candidates are a lattice index the two blocks number differently, a height16
// word one treats as unsigned, and a scar the bake writes for a vertex the
// composer reads for another.
//
// AND IT PROVES ONE THING NEITHER BLOCK CAN STATE ALONE. terrain_rules §3.4
// defines a breach as "compose_top == bottom at all four corner vertices".
// TERRAIN.BAKE decides that on the height16 grid, from `base + scar <= bottom`;
// TERRAIN.PATCH computes `compose_top` in fx16 through its own §3.4 clamp and
// never sees layer D. The claim that those are THE SAME FACT is a cross-block
// invariant, and it is checked here on every one of the 1,024 cells: a cell the
// bake marked VOID_BREACHED is exactly a cell whose four corners the composer
// clamped onto the underside.
//
// NOT CHAINED, and recorded rather than skipped quietly:
//   * SURFACE.SHEET -> TERRAIN.BAKE. `stamp_results` names two different wires
//     in this tree (see zhao_terrain_bake.sv's header): SURFACE.STAMP's landed
//     port is a per-TEXEL layer-F stream, while TERRAIN.BAKE.md's packet table
//     and `zref::terrain::bake_dig` both speak stamp RECORDS. Closing it needs
//     a strength -> height16 mapping and a 64x64 -> 33x33 resample, neither of
//     which exists anywhere; fabricating them here would produce a green test
//     asserting an invention.
//   * TERRAIN.PATCH's live-field lane is driven EMPTY. Composing a real field
//     lane needs a zfield program through FIELD.SEQ.EARTH, which is not built;
//     `terrain_patch_directed` already composes real programs against
//     `compose_lattice` on the reference side.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_bake.h"
#include "Vzhao_terrain_patch.h"

#include "bake_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_terrain.hpp"
#include "zrender/internal.hpp"  // white-box: compose_lattice

using zhao::check;
namespace zt = zref::terrain;
namespace zr = zref::render;

namespace {

constexpr int32_t kM = 1 << 16;

zhao_abi::ZhTransform2fx identity_xform() {
  zhao_abi::ZhTransform2fx t;
  t.tx = 0;
  t.ty = 0;
  t.r00 = 1 << 16;
  t.r01 = 0;
  t.r10 = 0;
  t.r11 = 1 << 16;
  return t;
}

void reset_patch(Vzhao_terrain_patch& d) {
  d.rst_n = 0;
  d.list_clear_i = 0;
  d.patch_id_i = 0;
  d.fld_add_valid_i = 0;
  d.vtx_valid_i = 0;
  d.fld_valid_i = 0;
  d.st_ready_i = 0;
  d.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
  zhao::tick(d);
}

struct Composed {
  std::vector<int32_t> top{};
  std::vector<int32_t> bottom{};
  std::vector<int32_t> compose_top{};
  std::vector<uint8_t> dirty{};
  uint16_t subpatch = 0;
  bool timed_out = false;
};

/**
 * Stream one 33x33 lattice through the REAL TERRAIN.PATCH with an empty live
 * field list. The scar plane is whatever TERRAIN.BAKE just wrote.
 */
Composed compose_through_patch(Vzhao_terrain_patch& d, const zr::TerrainPatch& p,
                               const std::vector<int16_t>& scar, int stall_mod) {
  Composed o;
  o.top.assign(bdev::kVerts, 0);
  o.bottom.assign(bdev::kVerts, 0);
  o.compose_top.assign(bdev::kVerts, 0);
  o.dirty.assign(bdev::kVerts, 0);
  const bool dual = p.bottom.size() == static_cast<size_t>(bdev::kVerts);

  d.list_clear_i = 1;
  d.patch_id_i = 0x2C01;
  zhao::tick(d);
  d.list_clear_i = 0;

  int sent = 0, got = 0, step = 0;
  int64_t guard = 0;
  while (got < bdev::kVerts) {
    const int i = sent % bdev::kLat, j = sent / bdev::kLat;
    const size_t k = static_cast<size_t>(sent);
    const bool hold = stall_mod > 1 && ((step % stall_mod) == 0);
    ++step;
    if (sent < bdev::kVerts) {
      d.vtx_valid_i = hold ? 0 : 1;
      d.base_i = p.heights[k];
      d.scar_i = scar[k];
      d.bottom_i = dual ? p.bottom[k] : 0;
      d.dual_i = dual ? 1 : 0;
      d.wx_i = zt::lattice_lerp(p.env_x0, p.env_x1, i, bdev::kLat - 1);
      d.wz_i = zt::lattice_lerp(p.env_z0, p.env_z1, j, bdev::kLat - 1);
      d.vi_i = static_cast<uint8_t>(i);
      d.vj_i = static_cast<uint8_t>(j);
      d.src_id_i = 0x77;
    } else {
      d.vtx_valid_i = 0;
    }
    d.st_ready_i = (stall_mod > 1 && ((step % stall_mod) == 1)) ? 0 : 1;
    d.eval();
    if (d.vtx_valid_i && d.vtx_ready_o) ++sent;
    if (d.st_valid_o && d.st_ready_i) {
      o.top[static_cast<size_t>(got)] = static_cast<int32_t>(d.top_o);
      o.bottom[static_cast<size_t>(got)] = static_cast<int32_t>(d.bottom_o);
      o.compose_top[static_cast<size_t>(got)] = static_cast<int32_t>(d.compose_top_o);
      o.dirty[static_cast<size_t>(got)] = static_cast<uint8_t>(d.st_dirty_o);
      ++got;
    }
    zhao::tick(d);
    if (++guard > 100000) {
      o.timed_out = true;
      break;
    }
  }
  d.vtx_valid_i = 0;
  d.st_ready_i = 0;
  d.eval();
  o.subpatch = static_cast<uint16_t>(d.subpatch_dirty_o);
  return o;
}

zr::TerrainPatch make_island(uint32_t seed) {
  bdev::Rng rng(seed);
  zr::TerrainPatch p = bdev::make_patch(0, 0, 64 * kM);
  p.bottom.assign(bdev::kVerts, 0);
  p.cell_state.assign(bdev::kCellCount, zt::kSolid);
  for (int j = 0; j < bdev::kLat; ++j) {
    for (int i = 0; i < bdev::kLat; ++i) {
      const size_t k = static_cast<size_t>(j) * bdev::kLat + i;
      const int dxi = i - 16, dzj = j - 16;
      const int d2 = dxi * dxi + dzj * dzj;
      const int32_t rel = static_cast<int32_t>((512 - d2) * 5);
      p.heights[k] = static_cast<int16_t>(rel);
      // a SHALLOW keel on purpose: the bake must be able to dig through it, or
      // the breach half of this chain would never fire.
      p.bottom[k] = static_cast<int16_t>(rel - 1400 - static_cast<int32_t>(rng.range(0, 200)));
      p.scar[k] = 0;
    }
  }
  for (int cj = 0; cj < 3; ++cj)
    for (int ci = 0; ci < 3; ++ci)
      p.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci] = zt::kVoidAuthored;
  p.cell_state[static_cast<size_t>(20) * bdev::kCells + 20] |= zt::kNoBakeBit;
  return p;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_bake bake;
  Vzhao_terrain_patch patch;
  bdev::reset_dut(bake);
  reset_patch(patch);

  uint32_t total_breached = 0, total_healed = 0, total_dirty = 0, total_moved = 0;

  for (int round = 0; round < 4; ++round) {
    zr::TerrainPatch cur = make_island(0xC4A1'0000u + static_cast<uint32_t>(round));
    // Incremental cadence: five bakes ramping a crater down, each composed
    // through TERRAIN.PATCH, exactly as a frame would.
    int32_t depth = 0;
    for (int b = 0; b < 5; ++b) {
      bdev::StampRec st;
      st.patch_id = 0x2C01;
      st.src_id = 0x51;
      st.cx = (24 + round * 4) * kM;
      st.cz = (30 - round * 3) * kM;
      st.radius = (9 + round * 3) * kM;
      st.depth_from = depth;
      depth += 3 * kM;
      st.depth_to = depth;

      // ---- BAKE, the real block -----------------------------------------
      const bdev::BakeOut baked = bdev::run_bake(bake, cur, st, (b % 2) ? 3 : 0);
      check(!baked.timed_out, "the bake completes", 0, baked.timed_out ? 1 : 0);

      // The reference, for the seam's own sake: BAKE's layer B is what
      // `bake_dig` writes, so anything the composer then disagrees about is a
      // seam fault and not an arithmetic one.
      std::vector<zt::BreachEvent> ev;
      zr::TerrainPatch ref = bdev::oracle_bake(cur, st, &ev);
      int badb = 0;
      for (int k = 0; k < bdev::kVerts; ++k)
        if (baked.scar[static_cast<size_t>(k)] != ref.scar[static_cast<size_t>(k)]) ++badb;
      check(badb == 0, "the bake's layer B is bit-exact before it crosses the seam", 0,
            static_cast<uint64_t>(badb));

      cur.scar = baked.scar;
      cur.cell_state = baked.cell_state;

      // ---- PATCH, the real block, fed the plane BAKE just wrote ---------
      const Composed comp = compose_through_patch(patch, cur, baked.scar, (b % 2) ? 0 : 4);
      check(!comp.timed_out, "the composition completes", 0, comp.timed_out ? 1 : 0);

      // ---- against the ONE ratified composition -------------------------
      zref::SatLedger L;
      const zt::ComposedLattice want =
          zr::compose_lattice(ref, identity_xform(), {}, 0, nullptr, &L);
      int badt = 0, badbot = 0;
      for (int k = 0; k < bdev::kVerts; ++k) {
        if (comp.top[static_cast<size_t>(k)] != want.top[static_cast<size_t>(k)]) ++badt;
        if (comp.bottom[static_cast<size_t>(k)] != want.bottom[static_cast<size_t>(k)]) ++badbot;
      }
      check(badt == 0,
            "BAKE -> PATCH: every composed live_top equals compose_lattice on the baked patch", 0,
            static_cast<uint64_t>(badt));
      check(badbot == 0, "and every underside word survives the seam", 0,
            static_cast<uint64_t>(badbot));

      // ---- the cross-block §3.4 invariant --------------------------------
      // A breach is "compose_top == bottom at all four corners". BAKE decided
      // it on the height16 grid and never computed compose_top; PATCH computed
      // compose_top in fx16 and never saw layer D. They must agree.
      int mismatch = 0, breached_cells = 0, equal_cells = 0;
      for (int cj = 0; cj < bdev::kCells; ++cj) {
        for (int ci = 0; ci < bdev::kCells; ++ci) {
          const uint8_t stc = cur.cell_state[static_cast<size_t>(cj) * bdev::kCells + ci];
          const uint8_t sub = static_cast<uint8_t>(stc & zt::kSubstanceMask);
          if (sub == zt::kVoidAuthored || (stc & zt::kNoBakeBit) != 0) continue;
          const size_t c00 = static_cast<size_t>(cj) * bdev::kLat + ci;
          const bool all4 =
              comp.compose_top[c00] == comp.bottom[c00] &&
              comp.compose_top[c00 + 1] == comp.bottom[c00 + 1] &&
              comp.compose_top[c00 + bdev::kLat] == comp.bottom[c00 + bdev::kLat] &&
              comp.compose_top[c00 + bdev::kLat + 1] == comp.bottom[c00 + bdev::kLat + 1];
          if (all4) ++equal_cells;
          if (sub == zt::kVoidBreached) ++breached_cells;
          if ((sub == zt::kVoidBreached) != all4) ++mismatch;
        }
      }
      check(mismatch == 0,
            "a cell BAKE marked VOID_BREACHED is exactly a cell PATCH clamped onto the underside "
            "at all four corners",
            0, static_cast<uint64_t>(mismatch));
      total_breached += static_cast<uint32_t>(breached_cells);
      for (size_t e = 0; e < baked.events.size(); ++e)
        if (baked.events[e].state == zt::kSolid) ++total_healed;
      check(equal_cells >= breached_cells,
            "every breached cell is among the cells composed onto the underside",
            static_cast<uint64_t>(breached_cells), static_cast<uint64_t>(equal_cells));

      // ---- the dirty mask, end to end ------------------------------------
      // PATCH marks a vertex dirty iff live_top != fx(base), which after a
      // bake with no live field means "the scar moved it". So the bake's own
      // touched set bounds the composer's dirty set.
      int dirty_without_scar = 0;
      for (int k = 0; k < bdev::kVerts; ++k) {
        const bool scar_nonzero = baked.scar[static_cast<size_t>(k)] != 0;
        const bool clamped_up = static_cast<int32_t>(ref.heights[static_cast<size_t>(k)]) <
                                static_cast<int32_t>(ref.bottom[static_cast<size_t>(k)]);
        if (comp.dirty[static_cast<size_t>(k)] && !scar_nonzero && !clamped_up)
          ++dirty_without_scar;
        if (comp.dirty[static_cast<size_t>(k)]) ++total_dirty;
      }
      check(dirty_without_scar == 0,
            "PATCH calls a vertex dirty only where BAKE wrote a scar or the underside clamps it", 0,
            static_cast<uint64_t>(dirty_without_scar));
      for (int k = 0; k < bdev::kVerts; ++k)
        if (baked.touched[static_cast<size_t>(k)]) ++total_moved;
    }
  }

  std::printf(
      "[terrain_bake_chain] %u breached-cell observations, %u dirty vertices, %u texels dug\n",
      total_breached, total_dirty, total_moved);
  check(total_breached > 0,
        "the chain actually birthed breaches, so the cross-block invariant is not vacuous", 1,
        total_breached > 0 ? 1 : 0);
  check(total_dirty > 0, "and the composer actually saw moved ground", 1, total_dirty > 0 ? 1 : 0);

  const int rc = zhao::report_and_exit("terrain_bake_chain");
  zhao::exit_hard(rc);
}
