// forge_cliff_ram_rowoff_control.cpp — the storage differential SEEN TO FAIL
// on a deliberate break, driving a committed MUTANT.
//
// The bitmap-RAM candidate's characteristic fault is not a wrong edge but a
// wrong ROW: three live row registers and a prefetched fourth replace a
// 1,156-bit window, and a prefetch one row off leaves every handshake and
// every counter intact while the south-neighbour bit of every cell from page
// row 1 onward reads the wrong row. tests/mutants/
// zhao_forge_cliff_ram_rowoff_mutant.sv is that break (prefetch cj+2 instead
// of cj+3), kept as a separate renamed file rather than a temporary edit.
//
// BUILD NOTE: the mutant is verilated with `--prefix Vzhao_forge_cliff` so the
// shared driver header compiles; here the class Vzhao_forge_cliff IS the mutant.
//
// THIS TEST PASSES WHEN THE DIFFERENTIAL FAILS on a multi-row page, and it
// also requires (a) a one-row page to still MATCH — the break passing a weak
// fixture, which is why "the enumeration lanes were green" would not have
// caught it — and (b) walk_fault_o to stay SILENT: that instrument watches
// the span walk, not the window, and a counter that fired here would be a
// counter that fires on everything.
#include "forge_cliff_dev.hpp"
#include "zref/zref_terrain.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
namespace zt = zref::terrain;
namespace zf = zref::forge;
namespace ct = cliff_test;

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

std::vector<uint8_t> checker(int cw, int ch) {
  std::vector<uint8_t> ck(static_cast<size_t>(cw) * ch, zt::kVoidAuthored);
  for (int cj = 0; cj < ch; ++cj)
    for (int ci = 0; ci < cw; ++ci)
      if ((ci + cj) % 2 == 0) ck[static_cast<size_t>(cj) * cw + ci] = zt::kSolid;
  return ck;
}
}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_forge_cliff dut;  // the MUTANT, by prefix (see BUILD NOTE)
  int failures = 0;

  // (a) the weak fixture: one row. The prefetched row is never consulted, so
  //     the break is invisible and the plan must MATCH.
  {
    std::vector<uint8_t> st = checker(8, 1);
    const zt::ComposedLattice lat = make_lat(8, 1, st);
    const ct::Plan got = ct::plan_lattice(dut, lat, nullptr, 0);
    const zf::RimPlan want = zf::rim_plan(lat, nullptr);
    std::printf("  rowoff mutant, 8x1 (one row): plan %s the oracle\n",
                ct::same(got, want) ? "MATCHES" : "differs from");
    if (!ct::same(got, want)) {
      std::fprintf(stderr, "FAIL: the one-row page should not be able to see a prefetch fault\n");
      ++failures;
    }
  }
  // (b) the real fixture: eight rows. From page row 1 the south row is wrong.
  {
    const zt::ComposedLattice lat = make_lat(8, 8, checker(8, 8));
    const ct::Plan got = ct::plan_lattice(dut, lat, nullptr, 0);
    const zf::RimPlan want = zf::rim_plan(lat, nullptr);
    std::printf("  rowoff mutant, 8x8 checkerboard: RTL %zu edges vs oracle %zu; plan %s the oracle\n",
                got.edges.size(), want.edges.size(), ct::same(got, want) ? "MATCHES" : "differs from");
    if (ct::same(got, want)) {
      std::fprintf(stderr, "FAIL: the differential did not see a prefetch one row off\n");
      ++failures;
    }
    if (got.timed_out) {
      std::fprintf(stderr, "FAIL: the mutant hung\n");
      ++failures;
    }
  }
  // (c) the span-walk instrument is not this fault's
  std::printf("  rowoff mutant: walk_fault_o = %u\n", static_cast<unsigned>(dut.walk_fault_o));
  if (dut.walk_fault_o != 0) {
    std::fprintf(stderr, "FAIL: walk_fault_o fired on a window fault it has no business seeing\n");
    ++failures;
  }

  if (failures == 0)
    std::printf("forge_cliff_ram_rowoff_control: the differential FAILED on the break (as it must)\n");
  zhao::exit_hard(failures == 0 ? 0 : 1);
}
