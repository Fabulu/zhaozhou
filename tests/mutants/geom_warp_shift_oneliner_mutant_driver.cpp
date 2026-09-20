// geom_warp_shift_oneliner_mutant_driver.cpp -- THE DRIVER FOR A COMMITTED
// POSITIVE CONTROL. ITS POLARITY IS INVERTED: IT PASSES WHEN THE MUTANT IS
// CAUGHT.
//
// WHAT IS UNDER TEST HERE IS THE TEST, NOT THE DESIGN.
// `tests/mutants/zhao_geom_warp_shift_oneliner_mutant.sv` is a copy of
// `fpga/rtl/geometry/zhao_geom_warp.sv` with one line changed: the normal
// range-reduction's second stage reads the ORIGINAL magnitude instead of the
// one the first stage produced. This driver asks a single question --
//
//     can geom_warp_rtl_directed's FT096 actually SEE that?
//
// -- and it answers it by running the mutant against `zref::geom_warp::
// apply_outputs`, the same oracle the real directed test uses.
//
// WHY THE FIRE RATE IS MEASURED AND PRINTED. Ruling R123: a lane wrote the
// canonical `>=`->`>` mutation, measured ZERO fires in 360,000 pairs, and threw
// it away rather than commit a green control attached to nothing. A mutant is
// itself an instrument and can be blind. So this driver reports TWO numbers:
//
//   * the RANDOM sweep's fire count, which is expected to be ~0 because the
//     fault is reachable by exactly one 32-bit value out of 2^32; and
//   * the DIRECTED value's result, which must differ.
//
// Those two numbers together are the argument for FT096 containing
// -2147483647 EXPLICITLY. A random differential, however long you run it, will
// not find this. If the random sweep ever starts firing, the mutation stopped
// being the narrow one described and this file's reasoning needs re-reading.
//
// THE NEGATIVE CONTROL IS NOT IN THIS FILE, DELIBERATELY. It is
// geom_warp_rtl_directed FT096, where PRODUCTION agrees with the oracle on the
// same value. Neither run is evidence alone: this one shows the check CAN fail,
// that one shows it does not. A driver that contained both would be able to
// pass by comparing a thing against itself.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_geom_warp_shift_oneliner_mutant.h"
#include "zhao_sim.hpp"
#include "zref/zref_geom_warp.hpp"

namespace gw = zref::geom_warp;

using Dut = Vzhao_geom_warp_shift_oneliner_mutant;

static void resetDut(Dut& d) {
  d.rst_n = 0;
  d.v_valid_i = 0;
  d.o_p_ready_i = 0;
  d.o_n_ready_i = 0;
  d.f_ans_valid_i = 0;
  d.f_warp_valid_i = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  zhao::tick(d);
}

/**
 * Push one normal triple through and return the published direction.
 * Returns false if nothing was published (which this stimulus never causes --
 * the bound is wide open and the profile is correct).
 */
static bool pushNormal(Dut& d, const std::int32_t n_out[3], std::int32_t got[3]) {
  d.v_px_i = 0; d.v_py_i = 0; d.v_pz_i = 0;
  d.v_nx_i = 1; d.v_ny_i = 1; d.v_nz_i = 1;
  d.v_n_degenerate_i = 0;
  d.v_src_id_i = 0;
  d.v_attr_i[0] = 0; d.v_attr_i[1] = 0; d.v_attr_i[2] = 0; d.v_attr_i[3] = 0;
  d.d_warp_en_i = 1;
  d.d_slot_i = 0;
  d.d_slot_valid_i = 1;
  d.d_profile_i = 1;
  d.d_time_i = 0;
  d.d_par_i[0] = 0; d.d_par_i[1] = 0; d.d_par_i[2] = 0; d.d_par_i[3] = 0;
  d.d_bx_i = 0x7FFFFFFF; d.d_by_i = 0x7FFFFFFF; d.d_bz_i = 0x7FFFFFFF;
  d.v_valid_i = 1;

  bool p_done = false, n_done = false, accepted = false;
  for (int guard = 0; guard < 200; ++guard) {
    if (d.f_vtx_valid_o) {
      d.f_ans_valid_i = 1;
      d.f_warp_valid_i = 1;
      d.f_dx_i = 0; d.f_dy_i = 0; d.f_dz_i = 0;
      d.f_onx_i = n_out[0]; d.f_ony_i = n_out[1]; d.f_onz_i = n_out[2];
    } else {
      d.f_ans_valid_i = 0;
      d.f_warp_valid_i = 0;
    }
    d.o_p_ready_i = p_done ? 0 : 1;
    d.o_n_ready_i = n_done ? 0 : 1;
    if (d.o_p_valid_o && d.o_p_ready_i) p_done = true;
    if (d.o_n_valid_o && d.o_n_ready_i) {
      got[0] = d.o_nx_o; got[1] = d.o_ny_o; got[2] = d.o_nz_o;
      n_done = true;
    }
    if (d.v_ready_o && d.v_valid_i) accepted = true;
    zhao::tick(d);
    if (accepted) d.v_valid_i = 0;
    if (d.poison_valid_o) break;
    if (p_done && n_done) break;
  }
  d.v_valid_i = 0;
  d.o_p_ready_i = 0;
  d.o_n_ready_i = 0;
  d.f_ans_valid_i = 0;
  d.f_warp_valid_i = 0;
  zhao::tick(d);
  return p_done && n_done;
}

/** The oracle's reduced direction for the same three words. */
static void oracleNormal(const std::int32_t n_out[3], std::int32_t want[3]) {
  const std::int32_t out6[6] = {0, 0, 0, n_out[0], n_out[1], n_out[2]};
  gw::Inputs in{};
  gw::Declaration decl{};
  decl.displacement_bound[0] = 0x7FFFFFFF;
  decl.displacement_bound[1] = 0x7FFFFFFF;
  decl.displacement_bound[2] = 0x7FFFFFFF;
  const gw::Result r = gw::apply_outputs(out6, in, decl, zfield::Status{});
  want[0] = r.direction[0];
  want[1] = r.direction[1];
  want[2] = r.direction[2];
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut top;
  resetDut(top);

  // ---- 1. THE RANDOM SWEEP. Expected to find NOTHING. ----------------------
  // This is the half that makes the control honest. If a random differential
  // could catch this, FT096 would not need to name the value.
  int random_trials = 0, random_fires = 0;
  {
    std::uint32_t lcg = 0xC0FFEEu;
    for (int i = 0; i < 20000; ++i) {
      lcg = lcg * 1664525u + 1013904223u;
      const std::int32_t a = static_cast<std::int32_t>(lcg);
      lcg = lcg * 1664525u + 1013904223u;
      const std::int32_t b = static_cast<std::int32_t>(lcg);
      lcg = lcg * 1664525u + 1013904223u;
      const std::int32_t c = static_cast<std::int32_t>(lcg);
      const std::int32_t n[3] = {a, b, c};
      std::int32_t got[3] = {0, 0, 0}, want[3] = {0, 0, 0};
      if (!pushNormal(top, n, got)) continue;
      oracleNormal(n, want);
      ++random_trials;
      if (got[0] != want[0] || got[1] != want[1] || got[2] != want[2]) ++random_fires;
    }
  }

  // ---- 2. THE DIRECTED VALUE. The mutant MUST be caught here. --------------
  bool directed_fired = false;
  std::int32_t got[3] = {0, 0, 0}, want[3] = {0, 0, 0};
  {
    const std::int32_t n[3] = {-2147483647, 0, 0};
    const bool published = pushNormal(top, n, got);
    oracleNormal(n, want);
    directed_fired =
        published && (got[0] != want[0] || got[1] != want[1] || got[2] != want[2]);
  }

  std::printf(
      "  MUTANT FIRE RATE: %d/%d random triples caught it; the DIRECTED value\n"
      "  -2147483647 %s. oracle wanted %d, mutant produced %d.\n",
      random_fires, random_trials, directed_fired ? "CAUGHT IT" : "DID NOT",
      want[0], got[0]);

  // INVERTED POLARITY. This test passes when the defect is DETECTED.
  zhao::check(directed_fired,
              "POSITIVE CONTROL: the one-liner mutant is CAUGHT by the FT096 value, so "
              "geom_warp_rtl_directed's normal check can see a wrong shift count",
              1, directed_fired ? 1 : 0);

  // And the narrowness, asserted rather than merely printed: a random sweep
  // must NOT find it. If this ever fails, the mutation is no longer the narrow
  // one the header describes and the whole argument needs re-reading.
  zhao::check(random_fires == 0,
              "and a 20,000-triple RANDOM sweep does NOT find it -- which is why FT096 "
              "names the value explicitly instead of trusting a fuzzer",
              0, static_cast<std::uint32_t>(random_fires));
  zhao::check(random_trials > 19000,
              "the random sweep actually ran (a sweep that published nothing would "
              "report zero fires for the wrong reason)",
              1, random_trials > 19000 ? 1 : 0);

  return zhao::report_and_exit("geom_warp_shift_oneliner_mutant");
}
