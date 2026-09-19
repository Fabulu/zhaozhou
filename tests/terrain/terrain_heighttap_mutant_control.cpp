// terrain_heighttap_mutant_control.cpp -- INVERTED POLARITY. It PASSES when
// `interp_overflow_o` FIRES.
//
// This is evidence about the INSTRUMENT, not about the design. The guard it
// drives cannot be reached with any legal input to the real block -- the
// interpolation is a convex combination of three corner heights, so it cannot
// leave the interval they span -- and CLAUDE.md's rule for a guard no stimulus
// can reach is a committed mutant with an inverted driver rather than an
// argument that never ends.
//
// tests/mutants/zhao_terrain_heighttap_mutant.sv removes ONE shift: the divide
// by the cell width D at the rounding stage. The expression then extrapolates by
// a factor of D instead of interpolating, and an ordinary few-metre relief
// lattice overshoots the s32 rail by four orders of magnitude.
//
// WHAT THIS FILE ASSERTS, and the order matters:
//   1. the counter FIRES (if it does not, the detector is dead and the
//      production block's silence means nothing);
//   2. the block REFUSES rather than publishing the wrapped value -- `no_ground`
//      is high and `taps_answered_o` does not advance. A detector that fires and
//      then ships the bad number anyway is not a guard.
//
// It deliberately does NOT assert anything about the production block. The
// correct behaviour is asserted in terrain_heighttap_directed.cpp, separately,
// so that repairing a fault can never make a control pass for the wrong reason.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_terrain_heighttap_mutant.h"
#include "verilated.h"
#include "zhao_sim.hpp"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

constexpr int LAT = 33;
constexpr int CELLS = LAT - 1;
constexpr int32_t kPoison = 0x5BADF00D;

using Dut = Vzhao_terrain_heighttap_mutant;

// The same fixture as the directed suite: the compose cache's two read ports,
// one cycle deep. Kept a copy rather than shared, because a control that shares
// a header with the thing it controls goes stale with it silently.
struct Cache {
  std::vector<int32_t> top, wx, wz;
  std::vector<uint8_t> sub;
  Cache() : top(LAT * LAT, 0), wx(LAT, 0), wz(LAT, 0), sub(CELLS * CELLS, 0) {}
  void place(int pitch_log2) {
    const int sh = 16 + pitch_log2;
    for (int i = 0; i < LAT; ++i) {
      wx[static_cast<size_t>(i)] = static_cast<int32_t>(static_cast<int64_t>(i) << sh);
      wz[static_cast<size_t>(i)] = static_cast<int32_t>(static_cast<int64_t>(i) << sh);
    }
  }
  int32_t h(int vi, int vj) const {
    if (vi >= LAT || vj >= LAT) return kPoison;
    return top[static_cast<size_t>(vj) * LAT + static_cast<size_t>(vi)];
  }
  int32_t cx(int vi) const { return vi >= LAT ? kPoison : wx[static_cast<size_t>(vi)]; }
  int32_t cz(int vj) const { return vj >= LAT ? kPoison : wz[static_cast<size_t>(vj)]; }
  uint8_t substance(int ci, int cj) const {
    return (ci >= CELLS || cj >= CELLS) ? 3 : sub[static_cast<size_t>(cj) * CELLS + ci];
  }
};

void step(Dut& d, const Cache& c) {
  d.eval();
  const bool lreq = d.c_lat_req_o != 0;
  const int vi = d.c_lat_vi_o, vj = d.c_lat_vj_o;
  const bool creq = d.c_cs_req_o != 0;
  const int ci = d.c_cs_ci_o, cj = d.c_cs_cj_o;
  zhao::tick(d);
  d.c_lat_h_i = static_cast<uint32_t>(lreq ? c.h(vi, vj) : kPoison);
  d.c_lat_wx_i = static_cast<uint32_t>(lreq ? c.cx(vi) : kPoison);
  d.c_lat_wz_i = static_cast<uint32_t>(lreq ? c.cz(vj) : kPoison);
  d.c_cs_substance_i = creq ? c.substance(ci, cj) : 3;
  d.eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  const int pl = 1, sh = 17;
  Dut d;
  Cache c;
  c.place(pl);
  // A few metres of relief, the same shape the directed suite uses. Nothing
  // extreme: the point is that the mutant overflows on ORDINARY content, which
  // is what makes the control unambiguous.
  for (int j = 0; j < LAT; ++j)
    for (int i = 0; i < LAT; ++i)
      c.top[static_cast<size_t>(j) * LAT + static_cast<size_t>(i)] =
          static_cast<int32_t>((i * 37 - j * 23) * 9001);

  d.rst_n = 0;
  d.pitch_log2_i = static_cast<uint8_t>(static_cast<int8_t>(pl));
  d.req_valid_i = 0;
  d.req_x_i = 0;
  d.req_z_i = 0;
  d.req_surface_i = 0;
  d.o_lat_req_i = 0;
  d.o_lat_vi_i = 0;
  d.o_lat_vj_i = 0;
  d.o_lat_surface_i = 0;
  d.o_cs_req_i = 0;
  d.o_cs_ci_i = 0;
  d.o_cs_cj_i = 0;
  d.c_lat_h_i = static_cast<uint32_t>(kPoison);
  d.c_lat_wx_i = static_cast<uint32_t>(kPoison);
  d.c_lat_wz_i = static_cast<uint32_t>(kPoison);
  d.c_cs_substance_i = 3;
  for (int i = 0; i < 4; ++i) step(d, c);
  d.rst_n = 1;
  step(d, c);

  // One tap, squarely inside a solid cell, at a fraction that is neither 0 nor
  // on the diagonal -- so both products are non-zero and the missing divide has
  // something to magnify.
  d.req_x_i = static_cast<uint32_t>((7 << sh) + (1 << (sh - 2)));
  d.req_z_i = static_cast<uint32_t>((11 << sh) + (1 << (sh - 3)));
  d.req_valid_i = 1;
  int waited = 0;
  while (d.req_ready_o == 0 && waited++ < 400) step(d, c);
  step(d, c);
  d.req_valid_i = 0;
  waited = 0;
  while (d.rsp_valid_o == 0 && waited++ < 400) step(d, c);

  check(d.rsp_valid_o != 0, "the mutant still answers", 1,
        static_cast<long long>(d.rsp_valid_o));
  check(d.interp_overflow_o >= 1, "THE GUARD FIRED (this control's whole purpose)", 1,
        static_cast<long long>(d.interp_overflow_o));
  check(d.rsp_no_ground_o != 0, "and it refused rather than shipping the wrapped value", 1,
        static_cast<long long>(d.rsp_no_ground_o));
  check(d.taps_answered_o == 0, "an overflowed tap is not counted as an answer", 0,
        static_cast<long long>(d.taps_answered_o));
  check(d.place_mismatch_o == 0, "and it is not blamed on the placement", 0,
        static_cast<long long>(d.place_mismatch_o));
  check(d.taps_off_patch_o == 0, "nor on the patch", 0,
        static_cast<long long>(d.taps_off_patch_o));

  std::printf("terrain_heighttap_mutant_control: %d checks, %d failed\n", g_checks, g_failed);
  std::fflush(stdout);
  zhao::exit_hard(g_failed == 0 ? 0 : 1);
}
