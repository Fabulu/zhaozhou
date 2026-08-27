// zfield_interpret.cpp — the ONE generic Field IR interpreter (field-ir.md
// §3; §Grep-audit-law). Every numeric primitive is a frozen zref:: call
// (spec/qformats.md) — nothing here re-derives arithmetic. The TS interpreter
// (compiler/src/field_ir/interpret.ts) mirrors this file; golden .zvec are
// C++-owned and TS must replay them byte-identically (Csmith differential).
//
// REFACTORED 2026-08-27 (Field v3 Phase 2, reports/Fieldv3.md): the per-op
// semantic bodies moved VERBATIM into zfield/zfield_steps.hpp (steps::exec_op)
// so that full interpretation, exact uniform preparation and the FPLAN vector
// reference executor call the SAME step functions — the brief's hard rule.
// The operand gather/scatter here is driven by the GENERATED canonical
// operation table (zfield/generated/zfield_optable.hpp), whose static_asserts
// pin every code to the Op enum: no second hand-maintained numbering or shape
// table can exist on the C++ side. Behaviour is bit-identical to the pre-
// refactor switch; the golden .zvec replays and every RTL differential lane
// are the witnesses.

#include "zfield/zfield.hpp"

#include "zfield/generated/zfield_optable.hpp"
#include "zfield/zfield_steps.hpp"

namespace zfield {

using zref::SatLedger;

Status interpret(const Decoded& prog, const int32_t* in, size_t n_in, int32_t* out, size_t n_out) {
  return interpret(prog, in, n_in, out, n_out, nullptr);
}

Status interpret(const Decoded& prog, const int32_t* in, size_t n_in, int32_t* out, size_t n_out,
                 SatLedger* ledger_out) {
  int32_t reg[REG_COUNT] = {0};
  for (size_t i = 0; i < prog.in_lanes.size() && i < n_in; ++i) {
    reg[prog.in_lanes[i].reg] = in[i];
  }
  SatLedger L = {};

  for (const Instr& ins : prog.instrs) {
    if (ins.op == OP_END) break;
    const optable::OpShape* sh = optable::shape_of(ins.op);
    // unreachable on a decoded (validated) program:
    if (sh == nullptr) __builtin_unreachable();

    int32_t src[9] = {0};  // LDC has no sources; exec_op reads only the shape's members
    int k = 0;
    const uint8_t starts[3] = {ins.a, ins.b, ins.c};
    for (int g = 0; g < sh->n_groups; ++g) {
      for (int w = 0; w < sh->group_width[g]; ++w) src[k++] = reg[starts[g] + w];
    }
    int32_t dst[3] = {0};
    steps::exec_op(ins.op, ins.imm, prog.tables, src, dst, &L);
    for (int w = 0; w < sh->dst_width; ++w) reg[ins.dst + w] = dst[w];
  }

  for (size_t i = 0; i < prog.out_lanes.size() && i < n_out; ++i) {
    out[i] = reg[prog.out_lanes[i].reg];
  }
  if (ledger_out != nullptr) *ledger_out = L;
  return Status{L.add || L.mul || L.rescale || L.unit || L.rcp, L.rcp0 != 0};
}

}  // namespace zfield
