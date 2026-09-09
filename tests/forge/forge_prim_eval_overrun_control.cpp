// forge_prim_eval_overrun_control.cpp — the positive control for
// walk_overrun_o, driving a committed MUTANT.
//
// forge_prim_eval_directed asserts walk_overrun_o == 0 under correct
// operation, and on its own that is a hopeful zero: the counter watches a
// state violation that is unreachable while the S_E1 advance compare is
// right, so NO stimulus can move it. The only way to show it alive is to
// break the guard.
//
// tests/mutants/zhao_forge_prim_eval_mutant.sv is that break, kept as a
// separate renamed file rather than a temporary edit — a temporary edit to
// production RTL is a live-tree hazard and leaves no evidence behind.
//
// THIS TEST PASSES WHEN THE COUNTER FIRES. Inverse polarity, deliberately:
// it is evidence about the instrument, not about the design.
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_forge_prim_eval_mutant.h"

#include "zhao_sim.hpp"

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_forge_prim_eval_mutant top;

  top.j_valid_i = 0;
  top.v_ready_i = 0;
  top.view_sel_i = 1;
  top.rst_n = 0;
  top.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
  zhao::tick(top);

  // A tiny legal bolt. In the REAL block this walks 3 points and stops; in
  // the mutant the cursor passes the end and the guard must see it.
  top.j_start_x_i = 0;
  top.j_start_y_i = 0;
  top.j_start_z_i = 0;
  top.j_end_x_i = 10 << 16;
  top.j_end_y_i = 0;
  top.j_end_z_i = 0;
  top.j_perp1_x_i = 0;
  top.j_perp1_y_i = 65536;
  top.j_perp1_z_i = 0;
  top.j_perp2_x_i = 0;
  top.j_perp2_y_i = 0;
  top.j_perp2_z_i = 65536;
  top.j_waxis_x_i = 0;
  top.j_waxis_y_i = 65536;
  top.j_waxis_z_i = 0;
  top.j_half_width_i = 6554;
  top.j_branch_half_width_i = 0;
  top.j_amp_i = 1 << 16;
  top.j_branch_amp_i = 0;
  top.j_seed_i = 0x1234u;
  top.j_tick_phase_i = 7;
  top.j_segments_i = 2;
  top.j_branch_count_i = 0;
  top.j_br0_attach_i = 0;
  top.j_br0_segments_i = 1;
  top.j_br0_end_x_i = 0;
  top.j_br0_end_y_i = 0;
  top.j_br0_end_z_i = 0;
  top.j_br1_attach_i = 0;
  top.j_br1_segments_i = 1;
  top.j_br1_end_x_i = 0;
  top.j_br1_end_y_i = 0;
  top.j_br1_end_z_i = 0;
  top.j_view_mask_i = 3;

  top.j_valid_i = 1;
  top.eval();
  zhao::check(top.j_ready_o == 1, "mutant accepts the job", 1, top.j_ready_o);
  zhao::tick(top);
  top.j_valid_i = 0;
  top.v_ready_i = 1;

  // The mutant's cursor wraps its 7-bit space before re-hitting N, so give it
  // room; the counter fires long before that.
  for (int c = 0; c < 100000 && top.walk_overrun_o == 0; ++c) {
    top.eval();
    zhao::tick(top);
  }

  zhao::check(top.walk_overrun_o > 0,
              "walk_overrun_o FIRED on the broken advance (inverse polarity)", 1,
              top.walk_overrun_o > 0 ? 1 : 0);
  std::printf("[forge_prim_eval_overrun_control] walk_overrun_o = %u after the break\n",
              (unsigned)top.walk_overrun_o);

  return zhao::report_and_exit("forge_prim_eval_overrun_control");
}
