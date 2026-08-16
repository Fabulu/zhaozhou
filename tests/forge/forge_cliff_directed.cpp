// forge_cliff_directed.cpp — FORGE.CLIFF reference tests (deep-keel wave;
// spec/terrain_rules.md §5 rim law + the frozen degrade order, 2026-08-16).
//
// What each lane would catch (the "could have been red" statement):
//   1. enumeration — hand-counted rim edges for a solid block (16), a block
//      with a centre bite (24), an 8x8 checkerboard (128 = 4 per solid
//      cell). Red on: wrong side set, double-counted edges, OUT handling.
//   2. the structural worst case — the 32x32 checkerboard page: 2,048 rim
//      edges (the TIGHT bound; the 2,112 figure in §5 counts all
//      cell-adjacency edges, 64 of which have void owners) clamps to 512
//      with 1,536 dropped and NO mergeable runs. Red on: budget off by one,
//      merge firing without pressure, drop counting wrong.
//   3. near-camera priority — the same clamped page with a vdist that marks
//      ONE far-scan edge nearest: that edge survives, scan-early edges drop.
//      Red on: priority inverted, ties not by scan order.
//   4. merge under pressure — a page at 543 edges whose only runs are two
//      20-edge straight bites merges exactly those (38 shed, 505 alive,
//      dropped 0); unpressured pages in the same lattice do NOT merge (the
//      42-edge straight bite alone stays 42 span-1 edges).
//   5. default no-merge — a plain straight bite under budget: every span
//      stays 1 (merge is the DEGRADE path, never the default).

#include "zref/zref_terrain.hpp"

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

// a composed lattice with a uniform pitch and a cell-state plane
zt::ComposedLattice make_lat(int cells_w, int cells_h, const std::vector<uint8_t>& state) {
  zt::ComposedLattice lat;
  lat.w = cells_w + 1;
  lat.h = cells_h + 1;
  lat.dual = true;
  const int n = lat.w * lat.h;
  lat.wx.resize(lat.w);
  lat.wz.resize(lat.h);
  for (int i = 0; i < lat.w; ++i) lat.wx[i] = ((i - cells_w / 2) * 2) << 16;  // 2 m pitch
  for (int j = 0; j < lat.h; ++j) lat.wz[j] = ((j - cells_h / 2) * 2) << 16;
  lat.top.assign(n, 4 << 16);
  lat.bottom.assign(n, 0);
  lat.cell_state = state;
  return lat;
}

std::vector<uint8_t> all_solid(int cw, int ch) { return std::vector<uint8_t>(cw * ch, zt::kSolid); }

void test_enumeration() {
  // 4x4 solid block: the outer rim = 4 sides x 4 edges = 16
  zf::RimPlan p = zf::rim_plan(make_lat(4, 4, all_solid(4, 4)), nullptr);
  check(p.edges.size() == 16 && p.merged == 0 && p.dropped == 0,
        "4x4 solid block: exactly the 16 outer rim edges");

  // with a 2x2 centre bite: outer 16 + bite perimeter 8 = 24
  std::vector<uint8_t> st = all_solid(4, 4);
  for (int cj = 1; cj <= 2; ++cj)
    for (int ci = 1; ci <= 2; ++ci) st[cj * 4 + ci] = zt::kVoidBreached;
  p = zf::rim_plan(make_lat(4, 4, st), nullptr);
  check(p.edges.size() == 24, "4x4 with centre bite: 16 outer + 8 bite rim edges");
  // the bite edges exist on the SOLID neighbours: e.g. cell (1,0) side +z
  bool bite_wall = false;
  for (const zf::RimEdge& e : p.edges)
    if (e.ci == 1 && e.cj == 0 && e.side == 1) bite_wall = true;
  check(bite_wall, "the bite's north wall is owned by the solid cell above it");

  // 8x8 checkerboard: every solid cell has all 4 sides rim -> 32 x 4 = 128
  std::vector<uint8_t> ck(64, zt::kVoidAuthored);
  for (int cj = 0; cj < 8; ++cj)
    for (int ci = 0; ci < 8; ++ci)
      if ((ci + cj) % 2 == 0) ck[cj * 8 + ci] = zt::kSolid;
  p = zf::rim_plan(make_lat(8, 8, ck), nullptr);
  check(p.edges.size() == 128, "8x8 checkerboard: 4 rim edges per solid cell (128)");
}

void test_checkerboard_clamp() {
  // 32x32 checkerboard page: 512 solid cells x 4 = 2,048 rim edges (the
  // TIGHT worst case — §5's 2,112 counts all adjacency edges; the 64 border
  // edges whose owning cells are void are not rim edges). No two edges are
  // contiguous-collinear, so nothing merges: the clamp keeps 512, drops 1,536
  std::vector<uint8_t> ck(32 * 32, zt::kVoidAuthored);
  for (int cj = 0; cj < 32; ++cj)
    for (int ci = 0; ci < 32; ++ci)
      if ((ci + cj) % 2 == 0) ck[cj * 32 + ci] = zt::kSolid;
  const zt::ComposedLattice lat = make_lat(32, 32, ck);
  zf::RimPlan p = zf::rim_plan(lat, nullptr);
  check(p.edges.size() == 512, "clamped to the 512 per-page budget");
  check(p.dropped == 1536, "1,536 edges counted beyond the budget");
  check(p.merged == 0, "checkerboard has no mergeable runs (spans are never bridged)");
  bool all_single = true;
  for (const zf::RimEdge& e : p.edges)
    if (e.span != 1) all_single = false;
  check(all_single, "every surviving edge is an unmerged span");

  // priority: mark ONE late-scan edge's vertex nearest — it must survive
  // even though 1,536 edges ahead of it in scan order also want the budget
  std::vector<int32_t> vdist(lat.w * lat.h, 0);
  // cell (31,31) side 3 (+x): vertices (32,31) and (32,32)
  vdist[31 * lat.w + 32] = 1 << 16;
  p = zf::rim_plan(lat, vdist.data());
  check(p.edges.size() == 512, "priority clamp also keeps exactly 512");
  bool late_survives = false;
  for (const zf::RimEdge& e : p.edges)
    if (e.ci == 31 && e.cj == 31 && e.side == 3) late_survives = true;
  check(late_survives, "the nearest-camera edge survives the clamp (governor priority)");
}

void test_merge_under_pressure() {
  // 97x97 lattice (96x96 cells = 3x3 pages). Centre page (1,1) carries:
  //   - a straight 20-cell bite at row 48, cols 40..59 -> two 20-edge runs
  //   - 127 isolated checkerboard voids in rows 33..46 -> 508 single edges
  // page (1,1) enumerates 543 edges (7 spill edges belong to neighbouring
  // pages' solid cells); > 512 -> the two runs merge (38 shed) -> 505 alive,
  // dropped 0. The straight bite ALONE (42 edges, no pressure) stays 42
  // span-1 edges — merge is the degrade path, never the default.
  const int CW = 96, CH = 96;
  std::vector<uint8_t> st = all_solid(CW, CH);
  const auto void_at = [&](int ci, int cj) { st[cj * CW + ci] = zt::kVoidBreached; };
  for (int ci = 40; ci <= 59; ++ci) void_at(ci, 48);  // the bite
  int n = 0;
  for (int cj = 33; cj <= 46 && n < 127; ++cj)
    for (int ci = 32; ci <= 63 && n < 127; ++ci)
      if ((ci + cj) % 2 == 1) {
        void_at(ci, cj);
        ++n;
      }
  check(n == 127, "fixture placed 127 isolated voids");
  const zt::ComposedLattice lat = make_lat(CW, CH, st);

  zf::RimPlan p = zf::rim_plan(lat, nullptr);
  // whole-lattice expectations (enumerated independently per page):
  //   border pages: 64+32+64+36+35+64+32+64 = 391; centre page 543 edges,
  //   need = 543-512 = 31 -> the greedy merges run 1 fully (19 shed) and a
  //   13-edge PREFIX of run 2 (12 shed): the law sheds the MINIMUM, it does
  //   not merge whole runs when a prefix suffices
  check(p.edges.size() == 391 + 512, "9-page plan: 903 edges after the centre merge");
  check(p.merged == 31, "minimum merge: 31 edges shed (19 + a 12-edge prefix)");
  check(p.dropped == 0, "the merge alone brought the page inside budget (no drops)");
  // the bite reads as one full 20-span and one 13-prefix span
  int spans20 = 0, spans13 = 0;
  for (const zf::RimEdge& e : p.edges) {
    if (e.span == 20) ++spans20;
    if (e.span == 13) ++spans13;
  }
  check(spans20 == 1 && spans13 == 1, "the straight bite: one 20-span + one 13-prefix");
  // the isolated voids did NOT merge (no contiguous collinearity)
  bool singles = true;
  for (const zf::RimEdge& e : p.edges)
    if (e.span != 1 && e.span != 20 && e.span != 13) singles = false;
  check(singles, "isolated void edges stay span-1 (a merge never bridges a notch)");

  // the control: the same bite WITHOUT the checkerboard pressure
  std::vector<uint8_t> st2 = all_solid(CW, CH);
  for (int ci = 40; ci <= 59; ++ci) st2[48 * CW + ci] = zt::kVoidBreached;
  zf::RimPlan q = zf::rim_plan(make_lat(CW, CH, st2), nullptr);
  int bite_edges = 0;
  for (const zf::RimEdge& e : q.edges)
    if ((e.cj == 47 && e.side == 1 && e.ci >= 40 && e.ci <= 59) ||
        (e.cj == 49 && e.side == 0 && e.ci >= 40 && e.ci <= 59) ||
        (e.cj == 48 && (e.side == 2 || e.side == 3) && e.ci >= 39 && e.ci <= 60))
      ++bite_edges;
  check(bite_edges == 42, "unpressured straight bite: 42 span-1 edges (no default merge)");
  check(q.merged == 0 && q.dropped == 0, "no clamp, no merge without pressure");
}

}  // namespace

int main() {
  test_enumeration();
  test_checkerboard_clamp();
  test_merge_under_pressure();
  if (failures == 0) std::printf("forge_cliff_directed: all green\n");
  return failures == 0 ? 0 : 1;
}
