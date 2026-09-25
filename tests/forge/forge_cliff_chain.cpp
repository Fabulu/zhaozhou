// forge_cliff_chain.cpp -- FORGE.CLIFF, PRODUCER TO CONSUMER, on real terrain.
//
// `design/console_inventory.yml` says of `zhao_forge_cliff_ram`: "it is not
// composed into `zhao_console_core` because FORGE.CLIFF CANNOT yet be composed
// by either candidate: its three inputs ... have no producer anywhere in
// fpga/rtl." This file is the measurement that that sentence is now false, and
// it measures the FOURTH leg too -- the rim-edge consumer, which nobody had
// counted and which `design/contracts/FORGE.CLIFF.md` records as unwritten.
//
// What each lane would catch (the "could have been red" statement):
//
//  1. REFERENCE-EXACT. With the halo ring driven VOID, the 34x34 window this
//     chain builds from the cell-state plane is EXACTLY what
//     `zref::forge::rim_plan` sees, so the edges must match the oracle edge for
//     edge, in order, including side and span. Red on: a transposed window (row
//     vs column major), an off-by-one halo, a mis-scaled cell address, a
//     substance encoding read the wrong way round, or the cell-state service
//     being read at the wrong index.
//  2. THE SHIPPED POLICY. With the halo SOLID (the console's default), the
//     perimeter edges must DISAPPEAR and nothing else may change. Red on: the
//     halo policy not reaching the ring, or reaching the interior as well.
//  3. GEOMETRY. Every emitted quad's four vertices are checked against the
//     lattice's own placed wx/wz and its top/bottom surfaces at the endpoints
//     the evaluator's own vdist map names. Red on: a wrong endpoint map, a
//     swapped surface, wx/wz transposed, or the corner store taking one
//     answer for another.
//  4. CONTENTION IS NOT COSMETIC. The whole run is repeated with the incumbent
//     asking on a pseudo-random ~50% of cycles. The output must be BIT
//     IDENTICAL and the denial counters must be nonzero. Red on: a producer
//     that advances its address on a denied cycle -- which is the
//     record-swapping defect this chain was designed against, and lane 4 is
//     the lane that can see it.
//  5. THE PATCH PULLED MID-PAGE. `serve_valid` drops partway through a window.
//     The chain must not hang, the poison detector must FIRE, and the degraded
//     cells must be counted. Red on: a deadlock in StLoad, or a detector that
//     reads zero through a fault it is pointed at.
//  6. THE VDIST PORT IS NEVER READ. With `vdist_en` low the evaluator's
//     `vd_en_o` must stay 0 for the entire run. This is what turns "the block
//     does not use the input we have no producer for" from an argument into a
//     measurement.
//  7. TWO ENDS OF ONE NUMBER. The emit block's own `lat_denied_o` and the
//     sharer's `lat_denied1_o` count the same event from opposite sides and
//     must agree. Red on: a grant that one end believes and the other does not.

#include "Vtb_forge_cliff_chain.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

namespace zt = zref::terrain;
namespace zf = zref::forge;

constexpr int kCells = 32;
constexpr int kLatW = 33;

using Dut = Vtb_forge_cliff_chain;

// ---------------------------------------------------------------------------
// The fixture: one staged patch's worth of real terrain.
// ---------------------------------------------------------------------------
// 32x32 cells with an authored void lake and two straight bites, on a 2 m
// pitch, top at 4 m and a modelled underside at 0 -- the same shape
// `forge_cliff_directed.cpp` builds, at the size the compose cache actually
// stages. The rim count is deliberately well under the 512 budget so the
// degrade never fires and the comparison is against ENUMERATION, which is the
// part this chain is responsible for. The degrade itself is already pinned,
// bit for bit, by `forge_cliff_ram_differential`.
zt::ComposedLattice make_patch() {
  zt::ComposedLattice lat;
  lat.w = kLatW;
  lat.h = kLatW;
  lat.dual = true;
  const int n = lat.w * lat.h;
  lat.wx.resize(lat.w);
  lat.wz.resize(lat.h);
  for (int i = 0; i < lat.w; ++i) lat.wx[i] = ((i - kCells / 2) * 2) << 16;
  for (int j = 0; j < lat.h; ++j) lat.wz[j] = ((j - kCells / 2) * 2) << 16;
  lat.top.resize(n);
  lat.bottom.resize(n);
  for (int j = 0; j < lat.h; ++j) {
    for (int i = 0; i < lat.w; ++i) {
      // A gently varying surface, so a wrong vertex index shows up as a wrong
      // height rather than being hidden by a flat plane.
      lat.top[j * lat.w + i] = (4 << 16) + ((i * 91 + j * 37) << 8);
      lat.bottom[j * lat.w + i] = -((i * 13 + j * 29) << 8);
    }
  }
  std::vector<uint8_t> st(kCells * kCells, zt::kSolid);
  auto set_void = [&](int ci, int cj) {
    if (ci >= 0 && cj >= 0 && ci < kCells && cj < kCells) st[cj * kCells + ci] = 1;  // VOID_AUTHORED
  };
  // a lake
  for (int cj = 9; cj < 17; ++cj)
    for (int ci = 11; ci < 20; ++ci) set_void(ci, cj);
  // two straight bites, which give the run test something to merge if pressed
  for (int ci = 3; ci < 23; ++ci) set_void(ci, 25);
  for (int cj = 2; cj < 8; ++cj) set_void(27, cj);
  lat.cell_state = st;
  return lat;
}

// ---------------------------------------------------------------------------
// The independent enumeration, under a DECLARED halo policy.
// ---------------------------------------------------------------------------
// Written from `spec/terrain_rules.md` section 5 directly rather than from the
// RTL: one edge per SOLID cell side facing a non-solid neighbour, scan order
// cj outer, ci inner, side 0..3. `halo_solid` says what lies outside the page.
std::vector<zf::RimEdge> enumerate_rim(const zt::ComposedLattice& lat, bool halo_solid) {
  std::vector<zf::RimEdge> out;
  auto solid_at = [&](int ci, int cj) -> bool {
    if (ci < 0 || cj < 0 || ci >= kCells || cj >= kCells) return halo_solid;
    return lat.substance(ci, cj) == zt::kSolid;
  };
  static const int dci[4] = {0, 0, -1, 1};
  static const int dcj[4] = {-1, 1, 0, 0};
  for (int cj = 0; cj < kCells; ++cj) {
    for (int ci = 0; ci < kCells; ++ci) {
      if (!solid_at(ci, cj)) continue;
      for (int s = 0; s < 4; ++s) {
        if (!solid_at(ci + dci[s], cj + dcj[s])) {
          zf::RimEdge e;
          e.ci = static_cast<uint16_t>(ci);
          e.cj = static_cast<uint16_t>(cj);
          e.side = static_cast<uint8_t>(s);
          e.span = 1;
          out.push_back(e);
        }
      }
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// One run of the chain.
// ---------------------------------------------------------------------------
struct Vertex {
  int32_t x = 0, y = 0, z = 0;
};
struct Quad {
  Vertex v[4];
  uint16_t src_id = 0;
  uint16_t material = 0;
  uint16_t tri[2][3] = {{0, 0, 0}, {0, 0, 0}};
};

struct RunResult {
  std::vector<Quad> quads;
  bool timed_out = false;
  bool vd_en_seen = false;
  long clocks = 0;
};

struct RunOpts {
  bool halo_solid = true;
  uint32_t incumbent_mask = 0;  // bit n set = the incumbent asks on clock n%32
  long drop_serve_at = -1;      // clock at which serve_valid goes low, -1 = never
};

// `zhao::reset` drives `in_valid`/`in_data`, which this top does not have, so
// the reset is written out rather than borrowed.
void reset_chain(Dut& dut) {
  dut.rst_n = 0;
  dut.fill_lat_we_i = 0;
  dut.fill_cs_we_i = 0;
  dut.arm_i = 0;
  dut.serve_valid_i = 0;
  dut.o0_cs_req_i = 0;
  dut.o0_lat_req_i = 0;
  dut.v_ready_i = 0;
  dut.t_ready_i = 0;
  dut.eval();
  for (int i = 0; i < 8; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);
}

RunResult run_chain(Dut& dut, const zt::ComposedLattice& lat, const RunOpts& opt) {
  RunResult r;

  reset_chain(dut);

  // ---- load the terrain planes --------------------------------------------
  dut.arm_i = 0;
  dut.serve_valid_i = 0;
  dut.serve_src_id_i = 0;
  dut.o0_cs_req_i = 0;
  dut.o0_lat_req_i = 0;
  dut.v_ready_i = 0;
  dut.t_ready_i = 0;
  dut.halo_substance_i = opt.halo_solid ? 0 : 1;  // 0 = SOLID (terrain_rules 3.3)
  dut.vdist_en_i = 0;
  dut.material_id_i = 0x00C1;

  for (int j = 0; j < kLatW; ++j) {
    for (int i = 0; i < kLatW; ++i) {
      const int idx = j * kLatW + i;
      dut.fill_lat_we_i = 1;
      dut.fill_lat_idx_i = static_cast<uint16_t>(idx);
      dut.fill_lat_top_i = lat.top[idx];
      dut.fill_lat_bot_i = lat.bottom[idx];
      dut.fill_lat_wx_i = lat.wx[i];
      dut.fill_lat_wz_i = lat.wz[j];
      if (i < kCells && j < kCells) {
        dut.fill_cs_we_i = 1;
        dut.fill_cs_idx_i = static_cast<uint16_t>(j * kCells + i);
        dut.fill_cs_sub_i = static_cast<uint8_t>(lat.cell_state[j * kCells + i] & 0x3);
      } else {
        dut.fill_cs_we_i = 0;
      }
      zhao::tick(dut);
    }
  }
  dut.fill_lat_we_i = 0;
  dut.fill_cs_we_i = 0;

  // ---- go ------------------------------------------------------------------
  dut.serve_valid_i = 1;
  dut.serve_src_id_i = 0x0B1E;
  dut.arm_i = 1;

  Quad cur;
  int vn = 0, tn = 0;
  bool saw_page_done = false;
  long idle_for = 0;
  const long limit = 2000000;

  while (r.clocks < limit) {
    const bool inc = opt.incumbent_mask != 0 &&
                     (((opt.incumbent_mask >> (r.clocks & 31)) & 1u) != 0);
    dut.o0_cs_req_i = inc ? 1 : 0;
    dut.o0_lat_req_i = inc ? 1 : 0;
    dut.v_ready_i = 1;
    dut.t_ready_i = 1;
    if (opt.drop_serve_at >= 0 && r.clocks >= opt.drop_serve_at) dut.serve_valid_i = 0;

    dut.eval();

    if (dut.cliff_vd_en_o) r.vd_en_seen = true;

    if (dut.v_valid_o && dut.v_ready_i) {
      if (vn < 4) {
        cur.v[vn].x = static_cast<int32_t>(dut.v_x_o);
        cur.v[vn].y = static_cast<int32_t>(dut.v_y_o);
        cur.v[vn].z = static_cast<int32_t>(dut.v_z_o);
      }
      ++vn;
      if (dut.v_last_o) check(vn == 4, "a cliff job emitted exactly four vertices");
    }
    if (dut.t_valid_o && dut.t_ready_i) {
      if (tn < 2) {
        cur.tri[tn][0] = dut.t_i0_o;
        cur.tri[tn][1] = dut.t_i1_o;
        cur.tri[tn][2] = dut.t_i2_o;
      }
      cur.src_id = dut.t_src_id_o;
      cur.material = dut.t_material_o;
      ++tn;
      if (dut.t_last_o) {
        check(tn == 2, "a cliff job emitted exactly two triples");
        r.quads.push_back(cur);
        vn = 0;
        tn = 0;
      }
    }
    if (dut.cliff_page_done_o) saw_page_done = true;

    const bool quiet = dut.cliff_idle_o && !dut.feed_busy_o && !dut.emit_busy_o;
    idle_for = quiet ? (idle_for + 1) : 0;
    if (saw_page_done && idle_for > 64) break;

    zhao::tick(dut);
    ++r.clocks;
  }
  r.timed_out = (r.clocks >= limit);
  return r;
}

// The endpoint map, transcribed from `zhao_forge_cliff_ram.sv:439-467` a SECOND
// time here on purpose: the test's copy is written from the RTL's source lines,
// so a silent edit to either is a red rather than a matching pair of wrongs.
void endpoints(const zf::RimEdge& e, int* avi, int* avj, int* bvi, int* bvj) {
  const int ci = e.ci, cj = e.cj, sp = e.span;
  switch (e.side) {
    case 0: *avi = ci; *avj = cj; *bvi = ci + sp; *bvj = cj; break;
    case 1: *avi = ci + sp; *avj = cj + 1; *bvi = ci; *bvj = cj + 1; break;
    case 2: *avi = ci; *avj = cj + sp; *bvi = ci; *bvj = cj; break;
    default: *avi = ci + 1; *avj = cj; *bvi = ci + 1; *bvj = cj + sp; break;
  }
}

bool same_edges(const std::vector<zf::RimEdge>& a, const std::vector<zf::RimEdge>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (!(a[i] == b[i])) return false;
  }
  return true;
}

// The chain does not publish rim edges -- it publishes QUADS. Recovering the
// edge from the quad would need the endpoint map inverted, which is not
// uniquely invertible for a span-1 edge. So the comparison runs the other way:
// the expected edges are turned into expected quads and matched in order.
std::vector<Quad> expected_quads(const zt::ComposedLattice& lat,
                                 const std::vector<zf::RimEdge>& edges) {
  std::vector<Quad> out;
  out.reserve(edges.size());
  for (const zf::RimEdge& e : edges) {
    int avi, avj, bvi, bvj;
    endpoints(e, &avi, &avj, &bvi, &bvj);
    Quad q;
    const int ai = avj * kLatW + avi, bi = bvj * kLatW + bvi;
    q.v[0] = {lat.wx[avi], lat.top[ai], lat.wz[avj]};
    q.v[1] = {lat.wx[bvi], lat.top[bi], lat.wz[bvj]};
    q.v[2] = {lat.wx[bvi], lat.bottom[bi], lat.wz[bvj]};
    q.v[3] = {lat.wx[avi], lat.bottom[ai], lat.wz[avj]};
    out.push_back(q);
  }
  return out;
}

bool compare_quads(const std::vector<Quad>& got, const std::vector<Quad>& want, const char* tag) {
  if (got.size() != want.size()) {
    std::fprintf(stderr, "FAIL: %s: %zu quads, expected %zu\n", tag, got.size(), want.size());
    ++failures;
    return false;
  }
  for (size_t k = 0; k < got.size(); ++k) {
    for (int v = 0; v < 4; ++v) {
      if (got[k].v[v].x != want[k].v[v].x || got[k].v[v].y != want[k].v[v].y ||
          got[k].v[v].z != want[k].v[v].z) {
        std::fprintf(stderr,
                     "FAIL: %s: quad %zu vertex %d: got (%d,%d,%d) want (%d,%d,%d)\n",
                     tag, k, v, got[k].v[v].x, got[k].v[v].y, got[k].v[v].z,
                     want[k].v[v].x, want[k].v[v].y, want[k].v[v].z);
        ++failures;
        return false;
      }
    }
    if (got[k].tri[0][0] != 0 || got[k].tri[0][1] != 1 || got[k].tri[0][2] != 2 ||
        got[k].tri[1][0] != 0 || got[k].tri[1][1] != 2 || got[k].tri[1][2] != 3) {
      std::fprintf(stderr, "FAIL: %s: quad %zu triples are not (0,1,2)+(0,2,3)\n", tag, k);
      ++failures;
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  const zt::ComposedLattice lat = make_patch();
  Dut dut;

  // ---- lane 1: REFERENCE-EXACT, halo VOID ---------------------------------
  {
    const zf::RimPlan want = zf::rim_plan(lat, nullptr);
    const std::vector<zf::RimEdge> mine = enumerate_rim(lat, /*halo_solid=*/false);
    check(same_edges(mine, want.edges),
          "the test's own enumeration agrees with zref::forge::rim_plan (void halo)");
    check(want.edges.size() < zf::kRimBudgetPerPage,
          "the fixture is under budget, so enumeration is what is being measured");

    RunOpts o;
    o.halo_solid = false;
    const RunResult r = run_chain(dut, lat, o);
    check(!r.timed_out, "lane 1 completed");
    std::printf("lane 1 (void halo): %zu quads in %ld clocks, oracle wants %zu edges\n",
                r.quads.size(), r.clocks, want.edges.size());
    compare_quads(r.quads, expected_quads(lat, want.edges),
                  "lane 1: chain quads against zref::forge::rim_plan");
    check(!r.vd_en_seen, "lane 6: vd_en_o never asserted with vdist disabled");
    check(dut.feed_windows_done_o == 1, "lane 1: exactly one window streamed");
    check(dut.feed_cs_reads_o == static_cast<uint32_t>(kCells * kCells),
          "lane 1: exactly one cell-state read per interior cell (1024)");
    check(dut.cliff_walk_fault_o == 0, "lane 1: the evaluator's walk fault stayed silent");
    check(dut.emit_clamped_o == 0, "lane 1: no endpoint left the staged lattice");
    check(dut.cliff_tri_submitted_o == 2u * r.quads.size(),
          "lane 1: triangles_submitted counts two per emitted edge (law C3)");
  }

  // ---- lane 2 + 3: THE SHIPPED POLICY, halo SOLID -------------------------
  std::vector<Quad> baseline;
  {
    const std::vector<zf::RimEdge> want = enumerate_rim(lat, /*halo_solid=*/true);
    RunOpts o;
    o.halo_solid = true;
    const RunResult r = run_chain(dut, lat, o);
    check(!r.timed_out, "lane 2 completed");
    std::printf("lane 2 (solid halo): %zu quads in %ld clocks, expected %zu edges\n",
                r.quads.size(), r.clocks, want.size());
    compare_quads(r.quads, expected_quads(lat, want), "lane 2+3: solid-halo chain quads");
    check(r.quads.size() == want.size(), "lane 2: the perimeter edges are gone");
    check(!want.empty(), "lane 2: the fixture still has interior cliffs to draw");
    for (const Quad& q : r.quads) {
      check(q.src_id == 0x0B1E, "lane 2: every triple carries the staged patch's src_id");
      check(q.material == 0x00C1, "lane 2: every triple carries the owner's material id");
    }
    baseline = r.quads;
  }

  // ---- lane 4: CONTENTION ---------------------------------------------------
  {
    RunOpts o;
    o.halo_solid = true;
    o.incumbent_mask = 0xB6DB6DB6u;  // ~ 2 in 3 cycles taken by the incumbent
    const RunResult r = run_chain(dut, lat, o);
    check(!r.timed_out, "lane 4 completed");
    std::printf("lane 4 (contended): %zu quads in %ld clocks, cs denied %u, lat denied %u\n",
                r.quads.size(), r.clocks, dut.feed_cs_denied_o, dut.lat_denied1_o);
    check(dut.feed_cs_denied_o > 0, "lane 4: the cell-state client really was denied");
    check(dut.lat_denied1_o > 0, "lane 4: the lattice client really was denied");
    check(dut.emit_lat_denied_o == dut.lat_denied1_o,
          "lane 7: both ends of the lattice denial count agree");
    compare_quads(r.quads, baseline, "lane 4: contended output is bit-identical");
  }

  // ---- lane 5: THE PATCH PULLED MID-PAGE -----------------------------------
  {
    RunOpts o;
    o.halo_solid = true;
    o.drop_serve_at = 1400;  // partway through the 1,156-beat window
    const RunResult r = run_chain(dut, lat, o);
    check(!r.timed_out, "lane 5: the chain did not hang when the patch was pulled");
    std::printf("lane 5 (patch pulled): poison %u, cells degraded %u, pages degraded %u\n",
                dut.cs_poison1_o, dut.feed_cells_degraded_o, dut.feed_pages_degraded_o);
    check(dut.feed_cells_degraded_o > 0, "lane 5: degraded cells were counted");
    check(dut.feed_pages_degraded_o == 1, "lane 5: the page was marked degraded");
    check(dut.feed_windows_done_o == 1, "lane 5: the window still completed");
  }

  // ---- lane 5b: WHY THE POISON COUNTER READS ZERO HERE, ASSERTED ---------
  // `zhao_forge_cliff_feed` gates its request on `serve_valid_i`, so when the
  // patch is pulled it stops ASKING -- no refusal is ever in flight and the
  // sharer's `poison1_o` has nothing to see. Asserting zero here would be
  // quoting a detector's silence as if it had looked, which is the exact thing
  // CLAUDE.md forbids. So this lane asserts the REASON instead: the feed
  // stopped reading, and the cells it could not read were counted. The
  // detector's own positive control is `forge_cliff_srvshare_unit`, where
  // legal stimulus at the sharer's port does reach the state.
  {
    RunOpts o;
    o.halo_solid = true;
    o.drop_serve_at = 1400;
    const RunResult r = run_chain(dut, lat, o);
    check(!r.timed_out, "lane 5b completed");
    const uint32_t reads_when_pulled = dut.feed_cs_reads_o;
    check(reads_when_pulled < static_cast<uint32_t>(kCells * kCells),
          "lane 5b: the feed STOPPED reading when the patch went away "
          "(which is why poison cannot fire through it)");
    // MEASURED, and the first form of this assertion was WRONG in an
    // instructive way. `cs_reads_o` counts reads ISSUED, not cells answered
    // from one, so the single read that was in flight when the patch went away
    // is counted BOTH as a read and as a degraded cell: 446 + 579 = 1025
    // against 1024 interior cells. That is the correct behaviour -- the read
    // was really issued and its answer really was discarded -- and it is
    // exactly one cell, because the feed is single-flight. The assertion says
    // so precisely rather than being loosened to an inequality that would also
    // pass if fifty cells went missing.
    const uint32_t accounted = dut.feed_cells_degraded_o + reads_when_pulled;
    const uint32_t cells = static_cast<uint32_t>(kCells * kCells);
    check(accounted == cells || accounted == cells + 1,
          "lane 5b: every interior cell was read or degraded, with at most the "
          "ONE in-flight read counted twice");
    std::printf("lane 5b: %u cells read, %u degraded, %u+%u = %d\n",
                reads_when_pulled, dut.feed_cells_degraded_o, reads_when_pulled,
                dut.feed_cells_degraded_o, kCells * kCells);
  }

  if (failures == 0) {
    std::printf("forge_cliff_chain: OK\n");
  }
  return failures == 0 ? 0 : 1;
}
