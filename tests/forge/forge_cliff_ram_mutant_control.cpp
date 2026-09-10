// forge_cliff_ram_mutant_control.cpp — the positive control for the
// candidate's `walk_fault_o`, driving a committed MUTANT.
//
// forge_cliff_ram_differential asserts walk_fault_o == 0 on every legal page,
// and on its own that is a hopeful zero: the counter watches a span walk
// reading a ZERO span or landing PAST cnt_r, both unreachable while every
// merge writes exactly the number of entries it consumed. No stimulus can
// move it. The only way to show it alive is to break the merge's span write,
// and there are two committed one-line breaks under tests/mutants/:
//
//   zhao_forge_cliff_ram_mutant.sv        span <- 0         (zero-span trigger)
//   zhao_forge_cliff_ram_over_mutant.sv   span <- take + 1  (overshoot trigger)
//
// This ONE driver serves both; build it against either mutant.
//
// TWO FIXTURES, because the overshoot trigger taught a lesson the first
// version of this control did not know: a span one too long merely skips one
// live UNIT entry and lands back on the table grid, so the walk still ends
// exactly at cnt_r. An overshoot needs the inflated span to STRADDLE the
// table's end — the merged run must be the LAST thing in the table. Fixture
// B builds exactly that: an interior page (solid halos on all four sides, so
// no border edges) whose only run is a vertical bite along the page's east
// edge, cells (62, 40..63) each contributing a single side-3 edge with nothing
// after them, under enough isolated-void pressure in rows 32..39 that the run
// merges WHOLE. Fixture A is the golden suites' 96x96 merge fixture (two
// merged heads mid-table), which the zero-span mutant trips at the first head.
//
// BUILD NOTE: the mutant is verilated with `--prefix Vzhao_forge_cliff` so the
// shared driver header (which includes Vzhao_forge_cliff.h) compiles; in THIS
// executable the class Vzhao_forge_cliff IS the mutant.
//
// THIS TEST PASSES WHEN THE COUNTER FIRES. Inverse polarity, deliberately: it
// is evidence about the instrument, not about the design.
#include "forge_cliff_dev.hpp"
#include "zref/zref_terrain.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
namespace zt = zref::terrain;
namespace zf = zref::forge;
namespace ct = cliff_test;

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

zt::ComposedLattice make_lat(int cw, int ch, const std::vector<uint8_t>& state) {
  zt::ComposedLattice lat;
  lat.w = cw + 1;
  lat.h = ch + 1;
  lat.dual = true;
  lat.wx.resize(lat.w);
  lat.wz.resize(lat.h);
  for (int i = 0; i < lat.w; ++i) lat.wx[i] = ((i - cw / 2) * 2) << 16;
  for (int j = 0; j < lat.h; ++j) lat.wz[j] = ((j - ch / 2) * 2) << 16;
  lat.top.assign(static_cast<size_t>(lat.w) * lat.h, 4 << 16);
  lat.bottom.assign(static_cast<size_t>(lat.w) * lat.h, 0);
  lat.cell_state = state;
  return lat;
}

// rim bodies inside one 32x32 page of the lattice (pre-degrade edge count)
size_t page_bodies(const zt::ComposedLattice& lat, int pi, int pj) {
  const int cw = lat.w - 1, ch = lat.h - 1;
  size_t total = 0;
  for (int cj = pj; cj < pj + 32 && cj < ch; ++cj) {
    for (int ci = pi; ci < pi + 32 && ci < cw; ++ci) {
      if (lat.substance(ci, cj) != zt::kSolid) continue;
      const int noff[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
      for (int s = 0; s < 4; ++s) {
        const int ni = ci + noff[s][0], nj = cj + noff[s][1];
        if (ni < 0 || nj < 0 || ni >= cw || nj >= ch || lat.substance(ni, nj) != zt::kSolid) ++total;
      }
    }
  }
  return total;
}

// Fixture A: the golden suites' 96x96 merge fixture (20 + 13, two heads).
zt::ComposedLattice fixture_a() {
  const int CW = 96, CH = 96;
  std::vector<uint8_t> st(static_cast<size_t>(CW) * CH, zt::kSolid);
  for (int ci = 40; ci <= 59; ++ci) st[48 * CW + ci] = zt::kVoidBreached;
  int n = 0;
  for (int cj = 33; cj <= 46 && n < 127; ++cj)
    for (int ci = 32; ci <= 63 && n < 127; ++ci)
      if ((ci + cj) % 2 == 1) {
        st[cj * CW + ci] = zt::kVoidBreached;
        ++n;
      }
  return make_lat(CW, CH, st);
}

// Fixture B: the run is the LAST thing in the centre page's table.
//   - column 63, rows 40..63 void: cells (62, 40..63) contribute ONE side-3
//     edge each and nothing else in those rows contributes (solid, solid
//     halos), so the 24 entries are consecutive and final;
//   - rows 32..39 (and the halo row 31 / halo columns 31 and 64 beside them)
//     carry isolated checkerboard voids: every solid cell there contributes
//     up to four edges of DIFFERENT sides, so no run forms, and the count
//     (printed) exceeds 511 so that need >= 23 and the run merges WHOLE.
zt::ComposedLattice fixture_b() {
  const int CW = 96, CH = 96;
  std::vector<uint8_t> st(static_cast<size_t>(CW) * CH, zt::kSolid);
  for (int cj = 40; cj <= 63; ++cj) st[cj * CW + 63] = zt::kVoidBreached;
  for (int cj = 31; cj <= 39; ++cj)
    for (int ci = 31; ci <= 64; ++ci)
      if ((ci + cj) % 2 == 1) st[cj * CW + ci] = zt::kVoidAuthored;
  return make_lat(CW, CH, st);
}

// plan_lattice RESETS the block before each lattice, so walk_fault_o is read
// absolutely per fixture (the first version differenced it and printed 2^32-1).
unsigned run_fixture(Vzhao_forge_cliff& dut, const zt::ComposedLattice& lat, const char* name) {
  const ct::Plan got = ct::plan_lattice(dut, lat, nullptr, 0);
  const zf::RimPlan want = zf::rim_plan(lat, nullptr);
  const unsigned faults = static_cast<unsigned>(dut.walk_fault_o);
  std::printf("  %s: oracle merged=%u dropped=%u edges=%zu | mutant plan %s the oracle, %s | "
              "walk_fault_o +%u\n",
              name, want.merged, want.dropped, want.edges.size(),
              ct::same(got, want) ? "MATCHES" : "differs from", got.timed_out ? "TIMED OUT" : "completed",
              faults);
  check(!got.timed_out, "the mutant must not hang (the advance-by-one guard)");
  check(!ct::same(got, want), "the differential must see a plan built from broken spans");
  return faults;
}
}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_forge_cliff dut;  // the MUTANT, by prefix (see BUILD NOTE)

  // Fixture B's construction is a claim; check it on the ORACLE before using it.
  const zt::ComposedLattice b = fixture_b();
  const size_t centre = page_bodies(b, 32, 32);
  const zf::RimPlan wb = zf::rim_plan(b, nullptr);
  std::printf("  fixture B centre page: %zu edges pre-merge (need >= 535 for a whole 24-run merge); "
              "oracle merged=%u\n",
              centre, wb.merged);
  check(centre >= 535, "fixture B: enough pressure that the 24-run must merge whole");
  check(wb.merged == 23, "fixture B: the oracle merges exactly the 24-run (23 bodies shed)");

  const unsigned fa = run_fixture(dut, fixture_a(), "fixture A (96x96, two mid-table heads)");
  const unsigned fb = run_fixture(dut, b, "fixture B (run is the table's last entry)");

  check(fa + fb > 0, "walk_fault_o fired on at least one fixture");
  std::printf("  walk_fault_o total = %u (A %u, B %u)\n", fa + fb, fa, fb);

  if (failures == 0) std::printf("forge_cliff_ram_mutant_control: walk_fault_o FIRED (as it must)\n");
  zhao::exit_hard(failures == 0 ? 0 : 1);
}
