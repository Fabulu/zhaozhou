// gen_optable.ts — GENERATE the canonical-operation table consumed by the C++
// Field v3 planner (reports/Fieldv3.md Phase 2).
//
// THE RULE THIS FILE ENFORCES: the canonical->uop translation is generated
// from the ONE canonical operation table (OP_INFO, pinned to
// spec/form/field-ir.md section 2), never maintained as another hand-written
// opcode switch. The v2 private encoding collided with canonical opcode
// values as valid-but-different operations precisely because a second
// hand-maintained numbering existed (tests/differential/
// field_v2_front_directed.cpp, to_ref()). Here every emitted code is OP_INFO's
// code, every emitted shape is OP_INFO's shape, and the emitted header
// static_asserts each code against the zfield::Op enum so the C++ side cannot
// drift silently either.
//
// Service classes are keyed by NAME (never by number): the v3 fabric routing
// for each canonical operation. Changing a route is a one-line edit HERE and
// a regeneration, never an edit in the generated file.
//
// Usage:  node compiler/dist/src/field_ir/gen_optable.js --write | --check
// Output: reference/include/zfield/generated/zfield_optable.hpp (committed).

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { OP_INFO, OpName, ImmUse } from './types.js';

// v3 service routing, by canonical op NAME (Fieldv3.md section 6):
//   VALU  — lane-private single-cycle datapath (ALU ops, pipelined SIN/COS)
//   VMUL  — the four-wide vector multiply/MAD bank (II 1)
//   CURVE — the barrel curve service (four-point II <= 14)
//   DIST  — the two-bank exact distance service (four-point II <= 20)
//   COLD  — exact scalar cold lane (uncertified for the max live workload)
// A varying RING with uniform radii is lowered to a prepared-ring
// microsequence on VMUL by the planner; the monolithic RING stays COLD.
const SVC: Record<OpName, 'VALU' | 'VMUL' | 'CURVE' | 'DIST' | 'COLD' | 'END'> = {
  END: 'END',
  MOV: 'VALU', LDC: 'VALU', ADD: 'VALU', SUB: 'VALU', MIN: 'VALU', MAX: 'VALU',
  ABS: 'VALU', CLAMP: 'VALU', SELECT: 'VALU', CMP: 'VALU',
  MUL: 'VMUL', MAD: 'VMUL', DOT2: 'VMUL', DOT3: 'VMUL',
  SIN: 'VALU', COS: 'VALU',
  RCP: 'COLD',
  CURVE: 'CURVE', DCURVE: 'CURVE', SPLINE: 'COLD',
  LEN2: 'DIST', LEN3: 'DIST', DIST2: 'DIST',
  NORMALIZE2: 'COLD', NORMALIZE3: 'COLD',
  NOISE2: 'COLD', RIDGE: 'COLD',
  RING: 'COLD', ROT2: 'COLD', ROT3: 'COLD',
};

const IMM_KIND: Record<ImmUse, number> = {
  none: 0, raw: 1, cmp_mode: 2, table: 3, seed: 4, rot3_axis: 5,
};
const SVC_CODE = { VALU: 0, VMUL: 1, CURVE: 2, DIST: 3, COLD: 4, END: 5 } as const;

export function emitOptableHpp(): string {
  const names = Object.keys(OP_INFO) as OpName[];
  const rows: string[] = [];
  const asserts: string[] = [];
  for (const n of names) {
    const o = OP_INFO[n];
    const g = [o.srcGroups[0] ?? 0, o.srcGroups[1] ?? 0, o.srcGroups[2] ?? 0];
    const nsrc = o.srcGroups.reduce((a, b) => a + b, 0);
    rows.push(
      `    {0x${o.code.toString(16).padStart(2, '0')}, "${n}", ${o.dstWidth}, ` +
      `${o.srcGroups.length}, {${g.join(', ')}}, ${nsrc}, ` +
      `${IMM_KIND[o.imm]}, ${SVC_CODE[SVC[n]]}},  // ${SVC[n]}`);
    asserts.push(`static_assert(zfield::OP_${n} == 0x${o.code.toString(16).padStart(2, '0')}, "canonical code drift: ${n}");`);
  }
  return `// GENERATED FILE - compiler/src/field_ir/gen_optable.ts - DO NOT EDIT.
// The canonical Field IR operation table (spec/form/field-ir.md section 2)
// as consumed by the C++ v3 planner. Regenerate:
//   cd compiler && npm run build && node dist/src/field_ir/gen_optable.js --write
// and commit. The static_asserts below pin every code to the zfield::Op enum:
// a second numbering CANNOT exist on the C++ side without failing to compile.
#pragma once

#include <cstdint>

#include "zfield/zfield.hpp"

namespace zfield {
namespace optable {

inline constexpr uint8_t IMM_NONE = 0, IMM_RAW = 1, IMM_CMP = 2, IMM_TABLE = 3,
                         IMM_SEED = 4, IMM_ROT3_AXIS = 5;
inline constexpr uint8_t SVC_VALU = 0, SVC_VMUL = 1, SVC_CURVE = 2,
                         SVC_DIST = 3, SVC_COLD = 4, SVC_END = 5;

struct OpShape {
  uint8_t code;
  const char* name;
  uint8_t dst_width;
  uint8_t n_groups;
  uint8_t group_width[3];
  uint8_t n_src;  // sum of group widths (flattened operand count)
  uint8_t imm_kind;
  uint8_t svc;
};

inline constexpr OpShape OPS[] = {
${rows.join('\n')}
};
inline constexpr int OP_COUNT = ${names.length};

/** Shape by canonical opcode; nullptr for a non-canonical byte. */
inline constexpr const OpShape* shape_of(uint8_t code) {
  for (const OpShape& s : OPS) {
    if (s.code == code) return &s;
  }
  return nullptr;
}

${asserts.join('\n')}

}  // namespace optable
}  // namespace zfield
`;
}

const HERE = dirname(fileURLToPath(import.meta.url));
// dist/src/field_ir -> zhaozhou repo root is four levels up from the dist file.
const REPO = resolve(HERE, '..', '..', '..', '..');
const OUT = resolve(REPO, 'reference', 'include', 'zfield', 'generated', 'zfield_optable.hpp');

const mode = process.argv[2];
if (mode === '--write' || mode === '--check') {
  const want = emitOptableHpp();
  let have: string | null = null;
  try { have = readFileSync(OUT, 'utf-8'); } catch { /* absent */ }
  if (mode === '--check') {
    if (have !== want) {
      console.error('zfield_optable.hpp is STALE - regenerate with --write and commit: ' + OUT);
      process.exit(1);
    }
    console.log('zfield_optable.hpp is current (' + OUT + ')');
  } else {
    if (have === want) {
      console.log('zfield_optable.hpp unchanged');
    } else {
      mkdirSync(dirname(OUT), { recursive: true });
      writeFileSync(OUT, want, 'utf-8');
      console.log('wrote ' + OUT);
    }
  }
}
