// forge_jitter_rom_directed.cpp — every JITTER_Q16 entry against the oracle.
//
// The table is GENERATED (tools/forge/gen_jitter_rom.py) into two emissions —
// the SV ROM and the zref header — and a generated file goes stale as easily
// as a copied one, because the generator is not run by the build. This test
// walks all 256 entries through both read ports and compares them against
// zref::forge::kEvalJitterTable, the same discipline field_sin_directed
// applies to SIN_Q16. A wrong entry would not fail loudly anywhere else: it
// puts one kink slightly off, which reads as a style choice.
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_forge_jitter_rom.h"

#include "zhao_sim.hpp"
#include "zref/generated/zref_forge_jitter_table.hpp"

namespace {
int32_t sext18(uint32_t raw) {
  return (int32_t)(raw << 14) >> 14;  // 18-bit two's complement to s32
}
}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_forge_jitter_rom top;

  for (int k = 0; k < zref::forge::kEvalJitterEntries; ++k) {
    top.idx_a_i = k & 0xFF;
    top.idx_b_i = (255 - k) & 0xFF;  // port B walks backwards — both ports live
    zhao::tick(top);                 // synchronous read: values after the edge
    top.eval();
    zhao::check(sext18(top.val_a_o) == zref::forge::kEvalJitterTable[k],
                "port A entry", (uint32_t)zref::forge::kEvalJitterTable[k],
                (uint32_t)sext18(top.val_a_o));
    zhao::check(sext18(top.val_b_o) == zref::forge::kEvalJitterTable[255 - k],
                "port B entry", (uint32_t)zref::forge::kEvalJitterTable[255 - k],
                (uint32_t)sext18(top.val_b_o));
  }

  return zhao::report_and_exit("forge_jitter_rom_directed");
}
