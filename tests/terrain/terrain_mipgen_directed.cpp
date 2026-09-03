// terrain_mipgen_directed.cpp — do the page mips select, and do the shared
// vertices survive bit for bit?
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS ACTUALLY GUARDING
// ---------------------------------------------------------------------------
// Ruling T8 makes the height mips a NESTED DECIMATION and says why:
//
//   > Nested decimation keeps shared vertices exact and introduces no
//   > rounding.
//
// The failure it rules out is a seam. Two neighbouring patches drawn at
// different LOD share an edge; if the coarse patch's edge vertices are
// averages of the fine patch's, the two edges disagree by a fraction of a
// metre and the ground cracks open along a line the player can walk to. It is
// not subtle once seen and it is invisible in any single-patch check.
//
// So the checks below are about IDENTITY, not about closeness. There is no
// tolerance anywhere in this file, because the law admits none:
//
//     mip17[i][j] MUST EQUAL fine33[2i][2j]
//     mip9 [i][j] MUST EQUAL fine33[4i][4j]
//
// And the nesting itself is checked: every mip9 vertex must also appear in
// mip17 with the same value, which is what makes the two levels agree with
// each other and not merely with the source.
//
// The stimulus is a lattice where every sample is DISTINCT, so a wrong
// address cannot accidentally produce a right value. A constant or a smooth
// ramp would let an off-by-one in the row stride pass.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_mipgen.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"

namespace {

constexpr int kFine = 33;
constexpr int kC17 = 17;
constexpr int kC9 = 9;
constexpr int kSurfs = 2;

// Every sample distinct, and distinct across surfaces too.
uint16_t sample_of(int surf, int row, int col) {
  const uint32_t v = static_cast<uint32_t>(surf) * 20011u +
                     static_cast<uint32_t>(row) * 137u +
                     static_cast<uint32_t>(col) * 7u + 0x1234u;
  return static_cast<uint16_t>(v ^ (v >> 7));
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_mipgen top;

  auto idle = [&]() {
    top.start_i = 0;
    top.fine_valid_i = 0;
  };

  auto reset = [&]() {
    idle();
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  // ---- 1: a full scan, with the outputs captured --------------------------
  std::vector<std::vector<int32_t>> m17(kSurfs, std::vector<int32_t>(kC17 * kC17, -1));
  std::vector<std::vector<int32_t>> m9(kSurfs, std::vector<int32_t>(kC9 * kC9, -1));
  int m17_double_write = 0, m9_double_write = 0;
  // A wrong row stride puts an address past the end of the mip. Bounds-checked
  // and COUNTED rather than indexed blindly: the first version of this file
  // crashed on a std::vector bounds assertion when the stride was deliberately
  // broken, which detects the fault but reports it as a test that fell over
  // rather than as a law that was violated.
  int m17_oor = 0, m9_oor = 0;
  {
    auto capture = [&]() {
      if (top.m17_valid_o) {
        const int a = static_cast<int>(top.m17_addr_o);
        if (a >= kC17 * kC17) ++m17_oor;
        else {
          if (m17[top.m17_surf_o][a] >= 0) ++m17_double_write;
          m17[top.m17_surf_o][a] = top.m17_h_o;
        }
      }
      if (top.m9_valid_o) {
        const int a = static_cast<int>(top.m9_addr_o);
        if (a >= kC9 * kC9) ++m9_oor;
        else {
          if (m9[top.m9_surf_o][a] >= 0) ++m9_double_write;
          m9[top.m9_surf_o][a] = top.m9_h_o;
        }
      }
    };

    reset();
    top.start_i = 1;
    zhao::tick(top);
    top.start_i = 0;

    // Feed the lattice in scan order, WITH GAPS. A block that only works when
    // fed every clock is not finished, and the gaps also prove the outputs are
    // tied to the sample rather than to the clock.
    uint32_t g = 0x5A5Au;
    for (int surf = 0; surf < kSurfs; ++surf)
      for (int row = 0; row < kFine; ++row)
        for (int col = 0; col < kFine; ++col) {
          // an occasional idle clock
          g = g * 1664525u + 1013904223u;
          if ((g >> 28) < 4u) {
            top.fine_valid_i = 0;
            top.eval();
            zhao::tick(top);
            top.eval();
            capture();
          }
          top.fine_valid_i = 1;
          top.fine_h_i = sample_of(surf, row, col);
          top.eval();
          zhao::tick(top);
          top.fine_valid_i = 0;
          top.eval();
          capture();
        }
    // let the last write land
    for (int c = 0; c < 4; ++c) { top.eval(); zhao::tick(top); }
  }

  zhao::check(top.samples_o == static_cast<uint32_t>(kSurfs * kFine * kFine),
              "every fine sample was consumed", kSurfs * kFine * kFine,
              static_cast<int>(top.samples_o));
  zhao::check(top.m17_writes_o == static_cast<uint32_t>(kSurfs * kC17 * kC17),
              "and exactly 17x17 per surface came out of the 17 port",
              kSurfs * kC17 * kC17, static_cast<int>(top.m17_writes_o));
  zhao::check(top.m9_writes_o == static_cast<uint32_t>(kSurfs * kC9 * kC9),
              "and exactly 9x9 per surface out of the 9 port", kSurfs * kC9 * kC9,
              static_cast<int>(top.m9_writes_o));
  zhao::check(m17_oor == 0 && m9_oor == 0,
              "no mip write lands outside its own level -- a wrong row stride "
              "shows up here first",
              0, m17_oor + m9_oor);
  zhao::check(m17_double_write == 0 && m9_double_write == 0,
              "and no mip cell is written twice -- a repeated address would "
              "mean the row stride is wrong somewhere",
              0, m17_double_write + m9_double_write);

  // ---- 2: THE LAW, with no tolerance --------------------------------------
  // Against `zref::terrain::mipgen`, the committed oracle, rather than against
  // the law re-typed here. Re-typing it would compare the RTL with my memory
  // of the ruling, and both could be wrong the same way.
  int bad17 = 0, bad9 = 0, unfilled = 0;
  for (int s = 0; s < kSurfs; ++s) {
    std::vector<uint16_t> fine(kFine * kFine);
    for (int r = 0; r < kFine; ++r)
      for (int c = 0; c < kFine; ++c) fine[r * kFine + c] = sample_of(s, r, c);
    std::vector<uint16_t> want17(kC17 * kC17), want9(kC9 * kC9);
    zref::terrain::mipgen(fine.data(), want17.data(), want9.data());

    for (int i = 0; i < kC17 * kC17; ++i) {
      const int32_t got = m17[s][i];
      if (got < 0) { ++unfilled; continue; }
      if (static_cast<uint16_t>(got) != want17[i]) ++bad17;
    }
    for (int i = 0; i < kC9 * kC9; ++i) {
      const int32_t got = m9[s][i];
      if (got < 0) { ++unfilled; continue; }
      if (static_cast<uint16_t>(got) != want9[i]) ++bad9;
    }
  }
  zhao::check(unfilled == 0, "every mip cell was written", 0, unfilled);
  zhao::check(bad17 == 0, "mip17 matches zref::terrain::mipgen, bit for bit", 0, bad17);
  zhao::check(bad9 == 0, "mip9 matches zref::terrain::mipgen, bit for bit", 0, bad9);

  // ---- 3: THE NESTING -- the two levels agree with EACH OTHER --------------
  // This is the seam property stated between the mips rather than against the
  // source: a vertex present at both levels must be the same vertex. It is
  // implied by case 2 and checked separately because it is the thing that
  // actually cracks if it is ever untrue.
  int nest_bad = 0;
  for (int s = 0; s < kSurfs; ++s)
    for (int i = 0; i < kC9; ++i)
      for (int j = 0; j < kC9; ++j)
        if (m9[s][i * kC9 + j] != m17[s][(2 * i) * kC17 + (2 * j)]) ++nest_bad;
  zhao::check(nest_bad == 0,
              "every mip9 vertex IS the mip17 vertex at the same place -- "
              "shared vertices are exact, which is why seams cannot crack",
              0, nest_bad);

  // ---- 4: the surfaces do not bleed into one another ----------------------
  int cross = 0;
  for (int i = 0; i < kC17 * kC17; ++i)
    if (m17[0][i] == m17[1][i]) ++cross;
  zhao::check(cross == 0,
              "top and bottom are separate surfaces, not one written twice", 0,
              cross);

  // ---- 5: an aborted scan is abandoned and counted ------------------------
  // T11 makes an aborted load legal. What must not happen is a half-finished
  // mip being mistaken for a complete one.
  {
    reset();
    top.start_i = 1;
    zhao::tick(top);
    top.start_i = 0;
    for (int n = 0; n < 100; ++n) {
      top.fine_valid_i = 1;
      top.fine_h_i = 0x4242;
      top.eval();
      zhao::tick(top);
    }
    top.fine_valid_i = 0;
    const uint32_t aborts_before = top.aborts_o;
    top.start_i = 1;
    zhao::tick(top);
    top.start_i = 0;
    top.eval();
    zhao::check(top.aborts_o == aborts_before + 1,
                "restarting mid-scan is counted as an abort, not silently "
                "blended into the next page",
                1, static_cast<int>(top.aborts_o - aborts_before));
    zhao::check(top.busy_o == 1, "and the new scan is running", 1, top.busy_o);
  }

  std::printf("  %u samples -> %u mip17 + %u mip9 writes, %u aborts\n",
              top.samples_o, top.m17_writes_o, top.m9_writes_o, top.aborts_o);

  return zhao::report_and_exit("terrain_mipgen_directed");
}
