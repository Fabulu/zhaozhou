// texture_palette_res_v2_parameter_control.cpp
//
// Inverse-polarity driver for zhao_texture_palette_res_v2's elaboration guards.
// Compile the model with an invalid parameter override.  A test-only nonfatal
// VerilatedContext preserves the exact ZHAO_PALETTE_V2_PARAM_FIRE diagnostic and
// requires gotError; returning without that error is a failed control.
#include <cstdio>

#include "verilated.h"
#include "../harness/zhao_sim.hpp"
#include "Vzhao_texture_palette_res_v2.h"

int main(int argc, char** argv) {
  VerilatedContext context;
  context.commandArgs(argc, argv);
  context.fatalOnError(false);
  Vzhao_texture_palette_res_v2 top{&context};
  top.rst_n = 0;
  top.eval();
  if (!context.gotError()) {
    std::fprintf(stderr, "FAIL: palette-v2 bad-parameter control escaped initial guard\n");
    zhao::exit_hard(2);
  }
  std::printf("PALETTE_V2_BAD_PARAMETER_CONTROL_OK\n");
  context.gotError(false);
  context.gotFinish(false);
  zhao::exit_hard(0);
}
