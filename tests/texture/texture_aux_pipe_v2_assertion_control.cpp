// texture_aux_pipe_v2_assertion_control.cpp
//
// One driver, seven separately compiled production-equivalent mutants.  Each
// mutant changes exactly one guard/state transition, retains the shipped named
// predicate/counter/assertion, and is verilated with the deliberately short
// common class prefix Vauxv2_assert so Windows generated paths remain bounded.
// A nonfatal context keeps
// the exact assertion label observable while allowing credit_fault_o to be read.
#include <cstdint>
#include <cstdio>

#include "verilated.h"
#include "Vauxv2_assert.h"
#include "zhao_sim.hpp"

int main(int argc, char** argv) {
  VerilatedContext context;
  context.commandArgs(argc, argv);
  int selected = 0;
  for (int i = 1; i < argc; ++i) {
    int parsed = 0;
    if (std::sscanf(argv[i], "--control=%d", &parsed) == 1) selected = parsed;
  }
  if (selected < 1 || selected > 7) {
    std::fprintf(stderr, "FAIL: use --control=1..7\n");
    zhao::exit_hard(2);
  }

  context.fatalOnError(false);
  Vauxv2_assert top{&context};
  top.frame_fault_clear_i = 0;
  top.job_valid_i = 0;
  top.job_wx_i = 512;
  top.job_wz_i = 768;
  top.job_env_x0_i = 0;
  top.job_env_x1_i = 1024;
  top.job_env_z0_i = 0;
  top.job_env_z1_i = 1024;
  top.job_sheet_handle_i = 0x12345678u;
  top.job_owner_i = 0x1234u;
  top.job_force_refuse_i = 0;
  top.req_ready_i = 0;
  top.pg_valid_i = 0;
  top.pg_op_i = 0;
  top.pg_status_i = 0;
  top.pg_tag_i = 0;
  top.pg_strength_i = 0;
  top.pg_src_id_i = 0;
  top.out_ready_i = 0;
  top.rst_n = 0;
  for (int i = 0; i < 6; ++i) zhao::tick(top);
  if (context.gotError()) {
    std::fprintf(stderr, "FAIL: control %d asserted during reset\n", selected);
    zhao::exit_hard(2);
  }

  top.rst_n = 1;
  bool submitted = false;
  bool fired = false;
  for (int cycle = 0; cycle < 100 && !fired; ++cycle) {
    if (selected >= 5 && !submitted) {
      top.job_valid_i = 1;
      top.job_force_refuse_i = selected == 7 ? 1 : 0;
    } else {
      top.job_valid_i = 0;
    }
    top.req_ready_i = selected == 6 ? 1 : 0;
    top.eval();
    const bool accepted = top.job_valid_i && top.job_ready_o;
    zhao::tick(top);
    if (accepted) submitted = true;
    fired = context.gotError();
  }
  top.job_valid_i = 0;
  top.req_ready_i = 0;
  top.eval();

  if (!fired || top.credit_fault_o == 0 || top.frame_fault_o != 1) {
    std::fprintf(stderr, "FAIL: control %d expected assertion/counter/set, got %d/%u/%u\n",
                 selected, fired ? 1 : 0, top.credit_fault_o, top.frame_fault_o);
    zhao::exit_hard(2);
  }

  // The independent owed/no-room level is edge-counted.  Keep the mutant's
  // impossible state held for five more clocks: assertions may corroborate the
  // held level again, but the durable counter must remain at one occurrence.
  if (selected == 6) {
    const uint32_t before = top.credit_fault_o;
    context.gotError(false);
    for (int hold = 0; hold < 5; ++hold) zhao::tick(top);
    if (before != 1 || top.credit_fault_o != before) {
      std::fprintf(stderr, "FAIL: owed-room one-shot expected held counter 1, got %u -> %u\n",
                   before, top.credit_fault_o);
      zhao::exit_hard(2);
    }
  }

  std::printf("AUX_ASSERT_CONTROL_OK control=%d assertion=1 credit_fault=%u frame_fault=1\n",
              selected, top.credit_fault_o);
  context.gotError(false);
  context.gotFinish(false);
  zhao::exit_hard(0);
}
