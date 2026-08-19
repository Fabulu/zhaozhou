// forge_cliff_dev.hpp — the shared driver for FORGE.CLIFF.
//
// One place that knows the block's four ports — the page command, the SOLID
// window load, the vdist read master and the rim-edge stream — so the directed
// lane and both random lanes drive it identically. If the port list changes,
// exactly one file needs editing and every lane fails to compile until it is.
// (Same shape as tests/terrain/project_dev.hpp.)
//
// `plan_lattice` walks the 32x32-cell PAGES of a lattice exactly as
// `zref::forge::rim_plan` walks them and runs one page per command, so what it
// returns is directly comparable to the oracle's whole-lattice plan — same
// edges, same order, summed `merged` and `dropped`. The page loop is the
// caller's law (the block's law is what happens INSIDE a page, chosen law F2),
// and this file is where that split is made concrete.

#pragma once

#include <cstdint>
#include <vector>

#include "Vzhao_forge_cliff.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"

namespace cliff_test {

/** What the block emits, in the oracle's own shape so the two can be compared. */
using Edge = zref::forge::RimEdge;

struct Plan {
  std::vector<Edge> edges;
  uint32_t merged = 0;
  uint32_t dropped = 0;
  long clocks = 0;      // total clocks across every page (the measurement)
  long worst_page = 0;  // the worst single page, which is the block's latency
  bool timed_out = false;
};

/**
 * Drive one PAGE and append its edges.
 *
 * `vdist` may be null, which is the reference's own null path: every priority
 * is 0, so a stable sort keeps scan order and the block is told so through
 * `cmd_vdist_en_i` rather than being handed a table of zeros.
 */
inline long run_page(Vzhao_forge_cliff& dut, const zref::terrain::ComposedLattice& lat, int pi,
                     int pj, const int32_t* vdist, uint32_t stall_mask, Plan* out) {
  const int cw_all = lat.w - 1, ch_all = lat.h - 1;
  const int cw = (pi + 32 <= cw_all) ? 32 : (cw_all - pi);
  const int ch = (pj + 32 <= ch_all) ? 32 : (ch_all - pj);

  // the 34x34 SOLID window: the page plus a one-cell halo. Anything off the
  // cell grid loads as 0, which is exactly right because `is_rim_edge` treats
  // OUT and VOID identically (chosen law F1).
  uint8_t win[34 * 34];
  for (int wj = 0; wj < 34; ++wj) {
    for (int wi = 0; wi < 34; ++wi) {
      const int ci = pi + wi - 1, cj = pj + wj - 1;
      const bool inside = ci >= 0 && cj >= 0 && ci < cw_all && cj < ch_all;
      win[wj * 34 + wi] = (inside && lat.substance(ci, cj) == zref::terrain::kSolid) ? 1 : 0;
    }
  }

  // ---- the command ---------------------------------------------------------
  dut.cmd_valid_i = 1;
  dut.cmd_page_ci_i = static_cast<uint16_t>(pi);
  dut.cmd_page_cj_i = static_cast<uint16_t>(pj);
  dut.cmd_cw_i = static_cast<uint8_t>(cw);
  dut.cmd_ch_i = static_cast<uint8_t>(ch);
  dut.cmd_lat_w_i = static_cast<uint16_t>(lat.w);
  dut.cmd_vdist_en_i = (vdist != nullptr) ? 1 : 0;
  dut.cmd_src_id_i = 0x5A5A;
  dut.ld_valid_i = 0;
  dut.edge_ready_i = 0;
  dut.vd_data_i = 0;

  long clocks = 0;
  const long limit = 4000000;
  size_t loaded = 0;
  bool cmd_taken = false;
  bool done = false;
  const size_t vsize = static_cast<size_t>(lat.w) * static_cast<size_t>(lat.h);

  while (!done && clocks < limit) {
    const bool ready = (stall_mask == 0) || (((stall_mask >> (clocks & 31)) & 1u) == 0);
    dut.edge_ready_i = ready ? 1 : 0;
    if (cmd_taken && loaded < 34 * 34) {
      dut.ld_valid_i = 1;
      dut.ld_solid_i = win[loaded];
    } else {
      dut.ld_valid_i = 0;
      dut.ld_solid_i = 0;
    }

    dut.eval();

    const bool take_cmd = dut.cmd_valid_i && dut.cmd_ready_o;
    const bool take_ld = dut.ld_valid_i && dut.ld_ready_o;
    // the vdist master: synchronous, data valid the cycle after the address
    const bool vd_en = dut.vd_en_o != 0;
    const uint32_t vd_addr = dut.vd_addr_o;

    if (dut.edge_valid_o && ready) {
      Edge e;
      e.ci = static_cast<uint16_t>(dut.edge_ci_o);
      e.cj = static_cast<uint16_t>(dut.edge_cj_o);
      e.side = static_cast<uint8_t>(dut.edge_side_o);
      e.span = static_cast<uint16_t>(dut.edge_span_o);
      out->edges.push_back(e);
    }
    if (dut.page_done_o) {
      out->merged += dut.page_merged_o;
      out->dropped += dut.page_dropped_o;
      done = true;
    }

    zhao::tick(dut);
    ++clocks;

    if (take_cmd) {
      cmd_taken = true;
      dut.cmd_valid_i = 0;
    }
    if (take_ld) ++loaded;
    // present the addressed word on the cycle AFTER the address, which is what
    // a synchronous-read memory does
    dut.vd_data_i =
        (vd_en && vdist != nullptr && vd_addr < vsize) ? static_cast<uint32_t>(vdist[vd_addr]) : 0u;
  }
  dut.cmd_valid_i = 0;
  dut.ld_valid_i = 0;
  dut.edge_ready_i = 0;
  dut.eval();
  if (!done) out->timed_out = true;
  return clocks;
}

/** Reset, then run every page of `lat` in the reference's own page order. */
inline Plan plan_lattice(Vzhao_forge_cliff& dut, const zref::terrain::ComposedLattice& lat,
                         const int32_t* vdist = nullptr, uint32_t stall_mask = 0) {
  dut.rst_n = 0;
  dut.cmd_valid_i = 0;
  dut.ld_valid_i = 0;
  dut.edge_ready_i = 0;
  dut.vd_data_i = 0;
  dut.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);

  Plan p;
  if (lat.w < 2 || lat.h < 2) return p;  // the reference's own early return
  const int cw = lat.w - 1, ch = lat.h - 1;
  for (int pj = 0; pj < ch; pj += 32) {
    for (int pi = 0; pi < cw; pi += 32) {
      const long c = run_page(dut, lat, pi, pj, vdist, stall_mask, &p);
      p.clocks += c;
      if (c > p.worst_page) p.worst_page = c;
      if (p.timed_out) return p;
    }
  }
  return p;
}

/** Bit-for-bit comparison with `zref::forge::rim_plan`. */
inline bool same(const Plan& got, const zref::forge::RimPlan& want) {
  if (got.edges.size() != want.edges.size()) return false;
  for (size_t k = 0; k < got.edges.size(); ++k) {
    if (!(got.edges[k] == want.edges[k])) return false;
  }
  return got.merged == want.merged && got.dropped == want.dropped && !got.timed_out;
}

}  // namespace cliff_test
