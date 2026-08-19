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
//
// THE RTL LANES (2026-08-19, `zhao_forge_cliff`). Lanes 1-5 pin the ORACLE;
// lanes 6-10 pin the BLOCK against it, bit for bit, over the SAME fixtures
// plus the ones only a hardware implementation can get wrong.
//   6. RTL enumeration — the three fixtures above through the block, plus an
//      all-void lattice (nothing emitted) and a single-cell one (four edges).
//      Red on: a wrong side set, a mishandled halo, a page-boundary slip.
//   7. RTL structural worst case — the 32x32 checkerboard page clamps to 512
//      with 1,536 bodies dropped and NOTHING merged, and the worst-page clock
//      count is MEASURED and printed.
//   8. RTL priority — the vdist master exercised four ways: the oracle lane's
//      single nearest spike, a GRADED field with ties on both sides of the
//      cut, NEGATIVE priorities (vdist is signed and the reference compares it
//      signed), and INT32_MIN/INT32_MAX. Red on: an unsigned comparator, a
//      threshold off by one, a tie-break that is not scan order.
//   9. RTL merge under pressure — the 9-page fixture: the greedy merges one
//      run whole and takes a 13-edge PREFIX of the second, because the law
//      sheds the MINIMUM. Red on: merging whole runs, wrong run order, a
//      merge that fires without pressure.
//  10. backpressure and the counter — four stall patterns leave the plan
//      bit-identical, and `triangles_submitted` counts TWO per emitted edge
//      (one wall quad).

#include "forge_cliff_dev.hpp"
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

// ---- 6-10. the RTL lanes ----------------------------------------------------

namespace ct = cliff_test;

void report(const char* what, const ct::Plan& got, const zf::RimPlan& want) {
  std::fprintf(stderr, "FAIL: %s\n", what);
  std::fprintf(stderr, "  RTL   edges=%zu merged=%u dropped=%u%s\n", got.edges.size(), got.merged,
               got.dropped, got.timed_out ? " TIMED OUT" : "");
  std::fprintf(stderr, "  oracle edges=%zu merged=%u dropped=%u\n", want.edges.size(), want.merged,
               want.dropped);
  const size_t n = got.edges.size() < want.edges.size() ? got.edges.size() : want.edges.size();
  for (size_t k = 0; k < n; ++k) {
    if (!(got.edges[k] == want.edges[k])) {
      std::fprintf(stderr,
                   "  first diff at [%zu]: RTL (ci %u cj %u side %u span %u) vs "
                   "oracle (ci %u cj %u side %u span %u)\n",
                   k, got.edges[k].ci, got.edges[k].cj, got.edges[k].side, got.edges[k].span,
                   want.edges[k].ci, want.edges[k].cj, want.edges[k].side, want.edges[k].span);
      break;
    }
  }
}

void expect_same(Vzhao_forge_cliff& dut, const zt::ComposedLattice& lat, const int32_t* vdist,
                 uint32_t stall_mask, const char* what) {
  const ct::Plan got = ct::plan_lattice(dut, lat, vdist, stall_mask);
  const zf::RimPlan want = zf::rim_plan(lat, vdist);
  if (!ct::same(got, want)) {
    report(what, got, want);
    ++failures;
  } else {
    check(true, what);
  }
}

// 6. enumeration — the same three fixtures lane 1 pins on the oracle, now
//    through the block.
void test_rtl_enumeration(Vzhao_forge_cliff& dut) {
  expect_same(dut, make_lat(4, 4, all_solid(4, 4)), nullptr, 0, "RTL: 4x4 solid block");

  std::vector<uint8_t> st = all_solid(4, 4);
  for (int cj = 1; cj <= 2; ++cj)
    for (int ci = 1; ci <= 2; ++ci) st[cj * 4 + ci] = zt::kVoidBreached;
  expect_same(dut, make_lat(4, 4, st), nullptr, 0, "RTL: 4x4 with a centre bite");

  std::vector<uint8_t> ck(64, zt::kVoidAuthored);
  for (int cj = 0; cj < 8; ++cj)
    for (int ci = 0; ci < 8; ++ci)
      if ((ci + cj) % 2 == 0) ck[cj * 8 + ci] = zt::kSolid;
  expect_same(dut, make_lat(8, 8, ck), nullptr, 0, "RTL: 8x8 checkerboard");

  // a lattice with NO solid cells at all, and one that is entirely solid: the
  // two ends of the predicate, neither of which any random mask reliably hits
  expect_same(dut, make_lat(4, 4, std::vector<uint8_t>(16, zt::kVoidAuthored)), nullptr, 0,
              "RTL: an all-void lattice emits nothing");
  expect_same(dut, make_lat(1, 1, all_solid(1, 1)), nullptr, 0,
              "RTL: a single-cell lattice is four rim edges");
}

// 7. the structural worst case — 2,048 edges clamped to 512 with no merges.
void test_rtl_checkerboard_clamp(Vzhao_forge_cliff& dut) {
  std::vector<uint8_t> ck(32 * 32, zt::kVoidAuthored);
  for (int cj = 0; cj < 32; ++cj)
    for (int ci = 0; ci < 32; ++ci)
      if ((ci + cj) % 2 == 0) ck[cj * 32 + ci] = zt::kSolid;
  const zt::ComposedLattice lat = make_lat(32, 32, ck);
  const ct::Plan got = ct::plan_lattice(dut, lat, nullptr, 0);
  const zf::RimPlan want = zf::rim_plan(lat, nullptr);
  if (!ct::same(got, want)) {
    report("RTL: the 32x32 checkerboard page", got, want);
    ++failures;
    return;
  }
  check(got.edges.size() == 512, "RTL: clamped to the 512 per-page budget");
  check(got.dropped == 1536, "RTL: 1,536 bodies counted beyond the budget");
  check(got.merged == 0, "RTL: a checkerboard has no mergeable runs at all");
  std::printf("  cliff worst page: %ld clocks (32x32 checkerboard, 2,048 -> 512)\n",
              got.worst_page);
}

// 8. near-camera priority — the frozen tie-break, and the one lane that
//    exercises the vdist master at all.
void test_rtl_priority(Vzhao_forge_cliff& dut) {
  std::vector<uint8_t> ck(32 * 32, zt::kVoidAuthored);
  for (int cj = 0; cj < 32; ++cj)
    for (int ci = 0; ci < 32; ++ci)
      if ((ci + cj) % 2 == 0) ck[cj * 32 + ci] = zt::kSolid;
  const zt::ComposedLattice lat = make_lat(32, 32, ck);

  // exactly the oracle lane's fixture: ONE late-scan edge marked nearest
  std::vector<int32_t> vd(static_cast<size_t>(lat.w) * lat.h, 0);
  vd[31 * lat.w + 32] = 1 << 16;
  expect_same(dut, lat, vd.data(), 0, "RTL: one nearest edge survives 1,536 rivals");
  const ct::Plan got = ct::plan_lattice(dut, lat, vd.data(), 0);
  bool late = false;
  for (size_t k = 0; k < got.edges.size(); ++k) {
    if (got.edges[k].ci == 31 && got.edges[k].cj == 31 && got.edges[k].side == 3) late = true;
  }
  check(late, "RTL: the nearest-camera edge survives the clamp (governor priority)");

  // a GRADED vdist, so the threshold search has to find a real cut value with
  // ties on both sides of it rather than a single spike
  std::vector<int32_t> graded(static_cast<size_t>(lat.w) * lat.h, 0);
  for (size_t k = 0; k < graded.size(); ++k) {
    graded[k] = static_cast<int32_t>((k * 37) % 11) << 16;
  }
  expect_same(dut, lat, graded.data(), 0, "RTL: a graded vdist with ties across the cut");

  // NEGATIVE priorities: vdist is a signed Q16.16 and the reference compares
  // it signed, so the biased-key search must span the sign.
  std::vector<int32_t> negs(static_cast<size_t>(lat.w) * lat.h, 0);
  for (size_t k = 0; k < negs.size(); ++k) {
    negs[k] = static_cast<int32_t>((k * 29) % 7) - 3;  // -3..3
  }
  expect_same(dut, lat, negs.data(), 0, "RTL: negative vdist values sort correctly (signed)");

  // the extremes of the key space, which the biased comparator must handle
  std::vector<int32_t> rails(static_cast<size_t>(lat.w) * lat.h, 0);
  for (size_t k = 0; k < rails.size(); ++k) {
    rails[k] = (k % 3 == 0) ? INT32_MIN : ((k % 3 == 1) ? INT32_MAX : 0);
  }
  expect_same(dut, lat, rails.data(), 0, "RTL: INT32_MIN/INT32_MAX priorities");
}

// 9. merge under pressure — the frozen R1 rule (shed the MINIMUM, take a
//    PREFIX), which is the single most reimplementation-hostile line of the
//    reference.
void test_rtl_merge_under_pressure(Vzhao_forge_cliff& dut) {
  const int CW = 96, CH = 96;
  std::vector<uint8_t> st = all_solid(CW, CH);
  const auto void_at = [&](int ci, int cj) { st[cj * CW + ci] = zt::kVoidBreached; };
  for (int ci = 40; ci <= 59; ++ci) void_at(ci, 48);
  int n = 0;
  for (int cj = 33; cj <= 46 && n < 127; ++cj)
    for (int ci = 32; ci <= 63 && n < 127; ++ci)
      if ((ci + cj) % 2 == 1) {
        void_at(ci, cj);
        ++n;
      }
  const zt::ComposedLattice lat = make_lat(CW, CH, st);
  const ct::Plan got = ct::plan_lattice(dut, lat, nullptr, 0);
  const zf::RimPlan want = zf::rim_plan(lat, nullptr);
  if (!ct::same(got, want)) {
    report("RTL: the 9-page merge fixture", got, want);
    ++failures;
    return;
  }
  check(got.merged == 31, "RTL: minimum merge — 31 bodies shed (19 + a 12-edge prefix)");
  check(got.dropped == 0, "RTL: the merge alone brought the page inside budget");
  int s20 = 0, s13 = 0;
  for (size_t k = 0; k < got.edges.size(); ++k) {
    if (got.edges[k].span == 20) ++s20;
    if (got.edges[k].span == 13) ++s13;
  }
  check(s20 == 1 && s13 == 1, "RTL: the straight bite is one 20-span + one 13-PREFIX span");

  // the control: the same bite with no pressure merges NOTHING
  std::vector<uint8_t> st2 = all_solid(CW, CH);
  for (int ci = 40; ci <= 59; ++ci) st2[48 * CW + ci] = zt::kVoidBreached;
  const zt::ComposedLattice lat2 = make_lat(CW, CH, st2);
  const ct::Plan g2 = ct::plan_lattice(dut, lat2, nullptr, 0);
  const zf::RimPlan w2 = zf::rim_plan(lat2, nullptr);
  if (!ct::same(g2, w2)) {
    report("RTL: the unpressured control", g2, w2);
    ++failures;
    return;
  }
  check(g2.merged == 0 && g2.dropped == 0, "RTL: no clamp and no merge without pressure");
  bool all_single = true;
  for (size_t k = 0; k < g2.edges.size(); ++k) {
    if (g2.edges[k].span != 1) all_single = false;
  }
  check(all_single, "RTL: merge is the DEGRADE path, never the default");
}

// 10. backpressure and the counter.
void test_rtl_handshake(Vzhao_forge_cliff& dut) {
  std::vector<uint8_t> ck(32 * 32, zt::kVoidAuthored);
  for (int cj = 0; cj < 32; ++cj)
    for (int ci = 0; ci < 32; ++ci)
      if ((ci + cj) % 2 == 0) ck[cj * 32 + ci] = zt::kSolid;
  const zt::ComposedLattice lat = make_lat(32, 32, ck);
  const ct::Plan base = ct::plan_lattice(dut, lat, nullptr, 0);
  check(dut.idle_o != 0, "RTL: idle once the page drains");
  check(dut.triangles_submitted_o == 1024,
        "RTL: triangles_submitted counts TWO per emitted edge (a wall quad)");

  const uint32_t masks[4] = {0xFFFFFFFEu, 0xAAAAAAAAu, 0x0F0F0F0Fu, 0x80000001u};
  for (int m = 0; m < 4; ++m) {
    const ct::Plan g = ct::plan_lattice(dut, lat, nullptr, masks[m]);
    bool ok = g.edges.size() == base.edges.size() && g.merged == base.merged &&
              g.dropped == base.dropped && !g.timed_out;
    for (size_t k = 0; ok && k < g.edges.size(); ++k) ok = (g.edges[k] == base.edges[k]);
    check(ok, "RTL: a stalling consumer changes nothing in the plan");
  }
}

}  // namespace

int main() {
  test_enumeration();
  test_checkerboard_clamp();
  test_merge_under_pressure();

  Vzhao_forge_cliff dut;
  test_rtl_enumeration(dut);
  test_rtl_checkerboard_clamp(dut);
  test_rtl_priority(dut);
  test_rtl_merge_under_pressure(dut);
  test_rtl_handshake(dut);

  if (failures == 0) std::printf("forge_cliff_directed: all green\n");
  zhao::exit_hard(failures == 0 ? 0 : 1);
}
