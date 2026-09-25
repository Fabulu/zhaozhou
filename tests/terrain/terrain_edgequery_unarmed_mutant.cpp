// terrain_edgequery_unarmed_mutant.cpp -- THE POSITIVE CONTROL FOR
// `descriptor_unarmed_o`.
//
// ITS POLARITY IS INVERTED. This driver PASSES when the counter FIRES and
// FAILS when it reads zero, which is the opposite of
// `tests/prod/terrain_edge_acceptance` section 10 -- that one asserts the
// counter is ZERO on unmutated production. BOTH HALVES ARE REQUIRED before
// either number may be quoted as evidence.
//
// WHY A MUTANT AND NOT STIMULUS. `descriptor_unarmed_o` counts a descriptor
// arriving with no armed answer and no query pending -- the safety valve that
// makes the descriptor gate incapable of deadlocking. On the real block that
// state is UNREACHABLE: `armed_q` is cleared only on a serve edge, and every
// exit from the query FSM sets it again (`q_done_i` in Q_WAIT, `!bank_emit_i`
// in both Q_REQ and Q_WAIT), so `armed_q == 0` implies the FSM is not idle
// while `stuck_c` requires both. No legal input moves it, and "it can fire"
// would stay an argument for ever -- CLAUDE.md's `wq_overflow_o` case exactly.
//
// THE MUTATION is one substantive line in
// `tests/mutants/zhao_terrain_edgequery_unarmed_mutant.sv`: the serve edge no
// longer enters Q_REQ, so the block sits in Q_IDLE with nothing armed.
//
// THE POSITIVE CONTROL ON THIS RUN: `patches_queued_o` must ALSO be nonzero.
// A run that queued nothing and served nothing would report the counter at
// zero for a reason that has nothing to do with the mutation, and would read
// as "the counter cannot fire" -- the flattering direction, and the exact
// shape CLAUDE.md calls a gate that cannot reach the state.
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_terrain_edgequery_unarmed_mutant.h"

#include "zhao_sim.hpp"

namespace {

void tick(Vzhao_terrain_edgequery_unarmed_mutant& t) {
  t.eval();
  t.clk = 1; t.eval();
  t.clk = 0; t.eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_edgequery_unarmed_mutant t;

  t.clk = 0; t.rst_n = 0;
  t.rec_valid_i = 0; t.rec_ix_i = 0; t.rec_iz_i = 0; t.rec_src_id_i = 0;
  t.door_valid_i = 0; t.door_src_id_i = 0;
  t.serve_valid_i = 0; t.serve_src_id_i = 0;
  t.e_sp_valid_i = 0; t.e_sp_cx_i = 0; t.e_sp_cy_i = 0; t.e_sp_cz_i = 0;
  t.e_sp_dev1_i = 0; t.e_sp_dev2_i = 0; t.e_sp_dev3_i = 0;
  t.e_sp_prev_level_i = 0; t.e_sp_prev_morph_i = 0; t.e_sp_hold_i = 0;
  t.e_sp_src_id_i = 0;
  t.o_sp_ready_i = 1;
  t.q_ready_i = 1; t.q_done_i = 0;
  t.q_edge_nz_i = 0; t.q_edge_pz_i = 0; t.q_edge_nx_i = 0; t.q_edge_px_i = 0;
  t.q_edge_real_i = 0;
  // THE BANK IS USABLE. That matters: it is the arm production would take into
  // Q_REQ, and it is the arm the mutation cuts. With `bank_emit_i` low the
  // block would arm the fallback and the mutation would be invisible.
  t.bank_emit_i = 1; t.prep_valid_i = 1;
  for (int i = 0; i < 6; ++i) tick(t);
  t.rst_n = 1;
  for (int i = 0; i < 4; ++i) tick(t);

  // One admitted record, one door, one serve edge.
  t.rec_valid_i = 1; t.rec_ix_i = 3; t.rec_iz_i = 7; t.rec_src_id_i = 0x1234;
  tick(t);
  t.rec_valid_i = 0;
  t.door_valid_i = 1; t.door_src_id_i = 0x1234;
  tick(t);
  t.door_valid_i = 0;
  tick(t);

  t.serve_src_id_i = 0x1234;
  t.serve_valid_i = 1;
  tick(t);
  tick(t);

  // Now offer descriptors. On production the answer would be armed by the
  // query; on the mutant nothing ever arms it, and the valve fires.
  t.e_sp_valid_i = 1;
  t.e_sp_src_id_i = 0x1234;
  for (int i = 0; i < 40; ++i) tick(t);
  t.e_sp_valid_i = 0;
  t.serve_valid_i = 0;
  for (int i = 0; i < 4; ++i) tick(t);

  const uint32_t unarmed = t.descriptor_unarmed_o;
  const uint32_t queued  = t.patches_queued_o;
  const uint32_t issued  = t.queries_issued_o;

  std::printf("mutant: descriptor_unarmed_o = %u, patches_queued_o = %u, "
              "queries_issued_o = %u\n", unarmed, queued, issued);

  int fails = 0;

  if (queued == 0) {
    std::printf("FAIL: patches_queued_o is ZERO -- nothing was ever admitted, so this run "
                "is not evidence about descriptor_unarmed_o in either direction.\n");
    ++fails;
  }
  if (issued != 0) {
    std::printf("FAIL: queries_issued_o is %u -- the mutation did NOT bite; the block still "
                "entered Q_REQ, so this is not the machine the control describes.\n", issued);
    ++fails;
  }
  if (unarmed == 0) {
    std::printf("FAIL: descriptor_unarmed_o did NOT fire on a block whose serve edge never "
                "starts a query. The counter cannot be quoted as an instrument.\n");
    ++fails;
  } else if (fails == 0) {
    std::printf("PASS: descriptor_unarmed_o fired %u time(s) on the mutant. The counter is an "
                "instrument, and its ZERO in terrain_edge_acceptance is a reading.\n", unarmed);
  }

  std::printf("terrain_edgequery_unarmed_mutant: %d failure(s)\n", fails);
  zhao::exit_hard(fails == 0 ? 0 : 1);
}
