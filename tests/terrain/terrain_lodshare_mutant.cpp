// terrain_lodshare_mutant.cpp -- THE POSITIVE CONTROL FOR `idq_overflow_o`.
// ITS POLARITY IS INVERTED: this driver PASSES when the counter FIRES.
//
// It measures `tests/mutants/zhao_terrain_lodshare_idqoverflow_mutant.sv`, a
// deliberately broken copy of `zhao_terrain_lodshare` whose identity-queue
// full-guard has been removed. It is evidence about the INSTRUMENT, never
// about the design -- the design's behaviour is asserted by
// `terrain_lodshare_directed`, which requires `idq_overflow_o` to be ZERO.
//
// WHY A MUTANT AND NOT STIMULUS. `idq_overflow_o` counts a push into a FULL
// queue. In the shipped block `p_sp_fire_c` requires `lod_sp_valid_o`, which
// carries `!idq_full_c`, so the push and the full condition are mutually
// exclusive: the state is UNREACHABLE BY CONSTRUCTION and no legal input can
// move the counter. That is exactly `wq_overflow_o`'s case in CLAUDE.md --
// "'it can fire' stays an argument forever" -- and the only demonstration is
// to break the guard in a committed file.
//
// THE TWO THINGS THIS ASSERTS, AND THE SECOND IS THE ONE THAT IS EASY TO SKIP:
//
//   1. `idq_overflow_o` is NONZERO. The counter moves when the fault it is
//      named for occurs.
//   2. `idq_full_stalls_o` is ALSO NONZERO. This is the negative control on
//      the run itself. If the stimulus never filled the queue, the counter
//      would read zero for a reason that has nothing to do with the mutation,
//      and the run would read as "the counter cannot fire" -- the flattering
//      direction, and CLAUDE.md's "a gate that cannot reach the state is not
//      evidence about the state". `terrain_lodshare_directed` case 9 proves
//      the HEALTHY block reaches full with the same stimulus shape, so the two
//      halves meet.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_terrain_lodshare_idqoverflow_mutant.h"

#include "zhao_sim.hpp"

int main(int argc, char** argv) {
  // UNBUFFERED, and it is a diagnostic rather than a style choice. A piped
  // run buffers stdout in 4 KB blocks, so a bench that blocks part way
  // through prints NOTHING and a late fault reads as a failure to start --
  // CLAUDE.md's 'buffered output lost in a crash makes a late fault look
  // like an early one'. This bench cost one wedge diagnosed that way.
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_lodshare_idqoverflow_mutant t;

  // ---- quiet ----------------------------------------------------------------
  t.frame_i = 0;
  t.prep_sel_i = 0;
  t.gv_cam0_x_i = 0; t.gv_cam0_y_i = 0; t.gv_cam0_z_i = 0;
  t.gv_cam0_scale_i = 0; t.gv_cam0_en_i = 0;
  t.gv_cam1_x_i = 0; t.gv_cam1_y_i = 0; t.gv_cam1_z_i = 0;
  t.gv_cam1_scale_i = 0; t.gv_cam1_en_i = 0;
  t.gv_hyst_i = 0; t.gv_min_hold_i = 0; t.gv_morph_step_i = 0;
  t.p_sp_valid_i = 0;
  t.p_sp_cx_i = 0; t.p_sp_cy_i = 0; t.p_sp_cz_i = 0;
  t.p_sp_dev1_i = 0; t.p_sp_dev2_i = 0; t.p_sp_dev3_i = 0;
  t.p_sp_prev_level_i = 0; t.p_sp_prev_morph_i = 0; t.p_sp_hold_i = 0;
  t.p_sp_src_id_i = 0; t.p_sp_ix_i = 0; t.p_sp_iz_i = 0; t.p_sp_gen_i = 0;
  t.e_sp_valid_i = 0;
  t.e_sp_cx_i = 0; t.e_sp_cy_i = 0; t.e_sp_cz_i = 0;
  t.e_sp_dev1_i = 0; t.e_sp_dev2_i = 0; t.e_sp_dev3_i = 0;
  t.e_sp_prev_level_i = 0; t.e_sp_prev_morph_i = 0; t.e_sp_hold_i = 0;
  t.e_sp_src_id_i = 0;
  t.lod_sp_ready_i = 1;
  t.lod_out_valid_i = 0;
  t.lod_out_ox_i = 0; t.lod_out_oz_i = 0; t.lod_out_level_i = 0;
  t.lod_out_lvl_nz_i = 0; t.lod_out_lvl_pz_i = 0;
  t.lod_out_lvl_nx_i = 0; t.lod_out_lvl_px_i = 0;
  t.lod_out_morph_i = 0; t.lod_out_surface_i = 0; t.lod_out_dual_i = 0;
  t.lod_out_src_id_i = 0; t.lod_out_hold_i = 0;
  t.f_ready_i = 0;          // NOTHING IS RETIRED: this is what fills the queue
  t.e_out_ready_i = 1;
  t.h_valid_i = 0; t.h_level_i = 0; t.h_morph_i = 0; t.h_hold_i = 0;
  t.h_ready_i = 1;

  t.rst_n = 0;
  for (int i = 0; i < 3; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();

  t.frame_i = 1;
  zhao::tick(t);
  t.frame_i = 0;
  t.prep_sel_i = 1;
  t.eval();

  // Offer descriptors continuously, retiring none. IDQ_DEPTH is 4, so a healthy
  // block accepts four and refuses the rest for ever. The mutant keeps
  // accepting, and every acceptance past the fourth lands on a live entry.
  for (int k = 0; k < 24; ++k) {
    t.p_sp_valid_i = 1;
    t.p_sp_src_id_i = static_cast<uint32_t>(0x100 + k);
    t.p_sp_ix_i = static_cast<int32_t>(k);
    t.p_sp_iz_i = static_cast<int32_t>(k);
    t.eval();
    zhao::tick(t);
    t.eval();
  }
  t.p_sp_valid_i = 0;
  t.eval();
  for (int i = 0; i < 8; ++i) { zhao::tick(t); t.eval(); }

  const uint32_t overflow = t.idq_overflow_o;
  const uint32_t stalls   = t.idq_full_stalls_o;
  const uint32_t pushed   = t.prep_descriptors_o;

  std::printf("mutant: idq_overflow_o = %u, idq_full_stalls_o = %u, prep_descriptors_o = %u\n",
              overflow, stalls, pushed);

  int failures = 0;

  // (2) THE RUN'S OWN NEGATIVE CONTROL, checked FIRST because if the queue never
  // filled then (1) says nothing whatever.
  if (stalls == 0) {
    std::printf("FAIL: idq_full_stalls_o is ZERO -- the queue never reached full, so this run "
                "is not evidence about idq_overflow_o in either direction.\n");
    ++failures;
  }

  // (1) THE INVERTED ASSERTION.
  if (overflow == 0) {
    std::printf("FAIL: idq_overflow_o did NOT fire on a block whose full-guard was removed. "
                "The counter is dead and its silence in production means nothing.\n");
    ++failures;
  } else {
    std::printf("PASS: idq_overflow_o fired %u time(s) on the mutant. The counter is an "
                "instrument, and its ZERO in terrain_lodshare_directed is a reading.\n",
                overflow);
  }

  // The mutant must genuinely have admitted more than the depth, or the
  // "overflow" would be some other event wearing the name.
  if (pushed <= 4) {
    std::printf("FAIL: the mutant accepted only %u descriptors into a 4-deep queue -- the "
                "guard removal did not take effect.\n", pushed);
    ++failures;
  }

  std::printf("\nterrain_lodshare_mutant: %d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
