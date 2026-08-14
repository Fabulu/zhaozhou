// emit_cpp.ts — the generated typed C++ wrapper emitter (field-ir.md §10:
// generic interpreter + wrapper; the compiler genuinely emits a C++ API from
// the IR). Byte-stable: two runs emit identical text (no timestamps/paths).

import { FieldProgram, OP_INFO } from './types.js';

function camelToUpperSnake(s: string): string {
  return s.replace(/([a-z0-9])([A-Z])/g, '$1_$2').toUpperCase();
}

export interface EmitterOptions {
  namespace: string;             // e.g. 'crater_ring'
  /** lane name -> struct field name for input/output records (default: raw name) */
  sourceName?: string;           // displayed in the header comment
  /** extra constexpr markers: [{name, pc, span}] emitted as k<Name>_Pc/_Span */
  markers?: { name: string; pc: number; sourceId: number; line: number; col: number }[];
}

export function emitCppWrapper(prog: FieldProgram, bytes: Uint8Array,
                               hash: number, opts: EmitterOptions): string {
  // group consecutive p<k> input lanes into one array member (earth p0..p7)
  const inMembers: string[] = [];
  const inList: string[] = [];
  let i = 0;
  while (i < prog.inputs.length) {
    const lane = prog.inputs[i]!;
    const m = /^p(\d+)$/.exec(lane.name);
    if (m) {
      const start = Number(m[1]);
      let end = start;
      while (i + 1 < prog.inputs.length &&
             /^p(\d+)$/.exec(prog.inputs[i + 1]!.name)?.[1] === String(end + 1)) {
        i++; end++;
      }
      const n = end - start + 1;
      inMembers.push(`  int32_t p[${n}];  // p${start}..p${end} (${lane.type})`);
      for (let k = 0; k < n; k++) inList.push(`in.p[${k}]`);
    } else {
      inMembers.push(`  int32_t ${lane.name};  // ${lane.type}`);
      inList.push(`in.${lane.name}`);
    }
    i++;
  }
  const outStruct = prog.outputs.map((l) => `  int32_t ${l.name};  // ${l.type}`);
  const outList = prog.outputs.map((l) => `out.${l.name}`);

  const byteLines: string[] = [];
  for (let i = 0; i < bytes.length; i += 16) {
    byteLines.push('  ' + Array.from(bytes.subarray(i, i + 16))
      .map((b) => `0x${b.toString(16).padStart(2, '0')}`).join(', ') + ',');
  }

  const markers = (opts.markers ?? []).map((m) => `

inline constexpr uint32_t k${camelToUpperSnake(m.name)}_Pc = ${m.pc}u;  // ${OP_INFO[prog.code[m.pc]!.op].code.toString(16)} ${prog.code[m.pc]!.op}
inline constexpr zfield::SourceRef k${camelToUpperSnake(m.name)}_Span = {${m.sourceId}u, ${m.line}u, ${m.col}u};`).join('');

  return `// GENERATED FILE - DO NOT EDIT
// Source: ${opts.sourceName ?? 'Field IR program'} via compiler/src/field_ir
// (compiler/tests emit + commit this file; spec/form/field-ir.md §10).
// program hash = CRC-32C(code|tables) + instr_count = 0x${(hash >>> 0).toString(16).padStart(8, '0')}
// cost: ${prog.cost.instrCount} instrs (ALU ${prog.cost.byClass.ALU} MUL ${prog.cost.byClass.MUL} TABLE ${prog.cost.byClass.TABLE} NOISE ${prog.cost.byClass.NOISE} SPECIAL ${prog.cost.byClass.SPECIAL}), ~${prog.cost.cycles} cycles, ${prog.cost.tableBytes} table bytes, reg high-water ${prog.cost.regHighWater}
#pragma once

#include <cstdint>
#include <array>
#include <cassert>

#include "zfield/zfield.hpp"

namespace zfield_gen {
namespace ${opts.namespace} {

inline constexpr size_t kProgramBytesLen = ${bytes.length}u;
inline constexpr std::array<uint8_t, kProgramBytesLen> kProgramBytes = {{
${byteLines.join('\n')}
}};
inline constexpr uint32_t kProgramHash = 0x${(hash >>> 0).toString(16).padStart(8, '0')}u;
static_assert(zfield::programHashConst(kProgramBytes.data(), kProgramBytesLen) == kProgramHash,
              "generated wrapper: embedded bytes no longer hash to kProgramHash");
${markers}

struct ${capitalize(opts.namespace)}In {
${inMembers.join('\n')}
};

struct ${capitalize(opts.namespace)}Out {
${outStruct.join('\n')}
};

inline zfield::Status eval(const ${capitalize(opts.namespace)}In& in,
                           ${capitalize(opts.namespace)}Out& out) {
  static const zfield::Decoded kProgram = [] {
    zfield::DecodeResult r = zfield::decode(kProgramBytes.data(), kProgramBytesLen);
    assert(r.error == zfield::DecodeError::kOk);  // loader never trusts bytes (4)
    return r.prog;
  }();
  const int32_t inputs[${prog.inputs.length}] = {${inList.join(', ')}};
  int32_t outputs[${prog.outputs.length}] = {0};
  zfield::Status st = zfield::interpret(kProgram, inputs, ${prog.inputs.length},
                                        outputs, ${prog.outputs.length});
${outList.map((l, i) => `  ${l} = outputs[${i}];`).join('\n')}
  return st;
}

}  // namespace ${opts.namespace}
}  // namespace zfield_gen
`;
}

function capitalize(s: string): string {
  const parts = s.split('_').filter(Boolean);
  return parts.map((p) => p[0]!.toUpperCase() + p.slice(1)).join('');
}
