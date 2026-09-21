// terrain_tapshare_mutant_control.cpp -- THE POSITIVE CONTROL for
// `zhao_terrain_tapshare`'s `stray_rsp_o`. ITS POLARITY IS INVERTED: it passes
// when the counter FIRES.
//
// This is evidence about the INSTRUMENT, not about the design. `stray_rsp_o`
// watches for a response arriving while no owner is held, which is unreachable
// while the owner latch is correct and the service is single-in-flight -- so
// no legal stimulus can move it and "it can fire" would otherwise stay an
// argument forever (CLAUDE.md: "a guard you cannot reach with legal stimulus
// needs a COMMITTED MUTANT").
//
// The mutant clears the owner one cycle after the grant instead of on the
// response (see tests/mutants/zhao_terrain_tapshare_mutant.sv). The stimulus
// below is ORDINARY and LEGAL -- one client, one request, a service that takes
// several cycles to answer, exactly as `zhao_terrain_heighttap`'s thirteen-
// state walk does. Nothing here is a poke at the counter; the fault is in the
// RTL and the counter is what notices.
//
// TWO ASSERTIONS, BOTH REQUIRED, because a counter that fires on everything is
// as useless as one that fires on nothing:
//   * the counter FIRED -- `stray_rsp_o` is non-zero;
//   * the FAULT IT WATCHES FOR IS REAL -- the answer reached NO client, so the
//     request was silently lost. A counter that moved while the datapath was
//     still correct would be measuring something else.
#include <cstdint>
#include <cstdio>

#include "Vtb_terrain_tapshare_mutant.h"
#include "zhao_sim.hpp"

using zhao::check;
using zhao::tick;

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  auto* top = new Vtb_terrain_tapshare_mutant;
  auto& d = *top;

  d.rst_n = 0;
  d.r0_valid_i = 0;
  d.r1_valid_i = 0;
  d.r0_x_i = 0;
  d.r0_z_i = 0;
  d.r1_x_i = 0;
  d.r1_z_i = 0;
  d.r0_surface_i = 0;
  d.r1_surface_i = 0;
  d.t_req_ready_i = 0;
  d.t_rsp_valid_i = 0;
  d.eval();
  for (int i = 0; i < 3; ++i) tick(d);
  d.rst_n = 1;
  d.eval();
  tick(d);

  // ---- one ordinary request from one client -------------------------------
  d.t_req_ready_i = 1;
  d.r0_valid_i = 1;
  d.r0_x_i = 0x0000'1234;
  d.r0_z_i = 0x0000'5678;
  d.eval();
  check(d.t_req_valid_o == 1, "the mutant still offers the request", 1,
        d.t_req_valid_o);
  tick(d);  // grant
  d.r0_valid_i = 0;
  d.eval();

  // ---- the service takes its time, as the real tap does -------------------
  for (int i = 0; i < 6; ++i) tick(d);
  d.eval();

  // ---- the answer comes back ----------------------------------------------
  d.t_rsp_valid_i = 1;
  d.eval();

  const uint32_t r0 = d.r0_rsp_valid_o;
  const uint32_t r1 = d.r1_rsp_valid_o;

  tick(d);
  d.t_rsp_valid_i = 0;
  d.eval();

  check(r0 == 0 && r1 == 0,
        "THE FAULT IS REAL: the answer reached no client at all", 1,
        (r0 == 0 && r1 == 0));
  check(d.stray_rsp_o != 0,
        "INVERTED POLARITY: stray_rsp_o FIRED on a lost owner", 1,
        d.stray_rsp_o != 0);

  std::printf(
      "[terrain_tapshare_mutant_control] stray_rsp=%u (MUST be non-zero; this "
      "control passes when the counter fires)\n",
      d.stray_rsp_o);

  top->final();
  return zhao::report_and_exit("terrain_tapshare_mutant_control");
}
