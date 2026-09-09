// vertex_arena_dense_seal_control.cpp — THE POSITIVE CONTROL FOR
// arena_misses_o IN DENSE MODE, driving a committed MUTANT.
//
// THIS TEST PASSES WHEN THE MISS COUNTER FIRES. Inverse polarity,
// deliberately: it is evidence about the INSTRUMENT, not about the design.
//
// vertex_arena_dense_directed asserts arena_misses_o == 0 under correct
// operation, and on its own that is a hopeful zero: in VALID_MODE=1 a legal
// lookup structurally cannot miss (proved -- a_dense_no_miss, prove_dense),
// so NO stimulus can move the counter while the seal guard is right. The only
// way to show the counter alive is to break the guard, which is what
// tests/mutants/zhao_vertex_arena_dense_mutant.sv does: its seal_gate_c
// accepts `<= DEPTH` instead of `== DEPTH`, sealing an incomplete arena.
//
// The drive below fills 2 of 81 rows, seals (the mutant accepts), and looks
// up row 50 with the CURRENT generation and a legal index -- a lookup every
// refusal lets through. On the real block that lookup cannot be issued
// against a sealed-incomplete arena because that state does not exist; on the
// mutant it lands on slot_written_c low, replies MISS (not hit, not refuse),
// and the counter moves. Per the checker law this file does NOT assert the
// bug's payload behaviour -- the correct-behaviour assertions live in
// vertex_arena_dense_directed (which also FAILS against this mutant, the
// checker's own positive control); this file only proves the detector fires.

#include <cstdint>
#include <cstdio>

#include "Vzhao_vertex_arena_dense_mutant.h"
#include "zhao_sim.hpp"

using zhao::check;

namespace {
constexpr int kDepth = 81;

struct Dut {
  Vzhao_vertex_arena_dense_mutant* v;
  explicit Dut(Vzhao_vertex_arena_dense_mutant* d) : v(d) {}

  void tick() {
    v->clk = 0;
    v->eval();
    v->clk = 1;
    v->eval();
  }
  void idle() {
    v->open_i = 0;
    v->org_we_i = 0;
    v->fill_valid_i = 0;
    v->seal_i = 0;
    v->look_valid_i = 0;
  }
  void reset() {
    idle();
    v->rst_n = 0;
    tick();
    tick();
    v->rst_n = 1;
    tick();
  }
};
}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Verilated::traceEverOn(false);
  auto* top = new Vzhao_vertex_arena_dense_mutant;
  Dut d(top);
  d.reset();

  // Open arena 0; the new generation is gen_q+1 = 1.
  d.idle();
  top->open_i = 1;
  top->open_arena_i = 0;
  d.tick();
  d.idle();

  // Fill only rows 0 and 1 of 81, in order.
  for (int i = 0; i < 2; ++i) {
    top->fill_valid_i = 1;
    top->fill_arena_i = 0;
    top->fill_index_i = i;
    top->fill_payload_i = 0x1000u + static_cast<uint64_t>(i);
    d.tick();
    d.idle();
  }

  // Seal at count = 2. The REAL block refuses this (arena_seal_short_o goes
  // sticky); the MUTANT accepts it. Assert the mutation took hold, so a
  // regenerated-but-unbroken copy cannot pass this control vacuously.
  top->seal_i = 1;
  top->seal_arena_i = 0;
  d.tick();
  d.idle();
  top->eval();
  check(top->arena_seal_short_o == 0,
        "the mutant ACCEPTED the short seal (mutation is live)", 0,
        top->arena_seal_short_o);

  // Look up an unwritten row with the current generation and a legal index.
  top->look_valid_i = 1;
  top->look_arena_i = 0;
  top->look_gen_i = 1;
  top->look_index_i = 50;
  d.tick();
  d.idle();
  top->eval();

  check(top->rep_valid_o == 1, "the lookup replies", 1, top->rep_valid_o);
  check(top->rep_refuse_o == 0, "and is NOT refused (the guard is broken)", 0,
        top->rep_refuse_o);
  check(top->rep_hit_o == 0, "and is not a hit -- it is the impossible MISS", 0,
        top->rep_hit_o);

  // THE POINT: the miss counter FIRED.
  check(top->arena_misses_o == 1,
        "arena_misses_o FIRED -- the dense-mode detector is alive", 1,
        top->arena_misses_o);

  delete top;
  return zhao::report_and_exit("vertex_arena_dense_seal_control");
}
