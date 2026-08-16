// wave_pool.ts — travelling radial wave over the terrain membrane (earth
// profile). The Sacrifice-identity deformation: ground behaving like a
// rubbery membrane with waves running through it, not a static dent.
//
// h(x, z, phase) = amp · env(d) · sin(k·d − n·phase)
//
// where d is the distance to the wave centre, k is the spatial frequency in
// TURNS PER METRE (SIN consumes the low 16 bits of its Q16.16 lane as an
// angle16 turn, field-ir.md §3.14 — so 1.0 in the lane is one full cycle),
// and n is the number of temporal cycles over one duration. With n an
// INTEGER the field is exactly periodic in phase: phase = 1 wraps to
// phase = 0 bit-for-bit, so a captured loop closes seamlessly.
//
// No new ops, no ISA change: DIST2 + MUL/SUB + SIN + the §3.20 smoothstep
// macro express the travelling wave inside the frozen v1 ISA.
//
// Built from the same builder/alloc/serialize chain as crater_ring (the
// single sanctioned program source path — never hand-serialized).

import { FieldBuilder } from './builder.js';
import { buildProgram } from './alloc.js';
import { FieldProgram, IoLane, SourceSpan } from './types.js';
import { serializeProgram, programHashOfBytes, decodeZprog } from './serialize.js';
import { MAT_SOIL } from './crater_ring.js';

/** kind 3 (field program), module 1, index 2 (capture_format.md §5). */
export const WAVE_POOL_SOURCE_ID = 0x30010002;

const L = (line: number, col: number): SourceSpan => ({
  sourceId: WAVE_POOL_SOURCE_ID, line, col,
});

/** raw fx helper */
const fx = (v: number): number => Math.round(v * 65536);

function earthInputs(): IoLane[] {
  return [
    { name: 'x', type: 'fx', reg: 0, min: fx(-40), max: fx(40) },
    { name: 'z', type: 'fx', reg: 1, min: fx(-40), max: fx(40) },
    { name: 'age', type: 'u32', reg: 2, min: 0, max: 65535 },
    { name: 'phase', type: 'fx', reg: 3, min: 0, max: fx(1) },
    { name: 'p0', type: 'fx', reg: 4, min: fx(-32), max: fx(32) },  // centre x
    { name: 'p1', type: 'fx', reg: 5, min: fx(-32), max: fx(32) },  // centre z
    { name: 'p2', type: 'fx', reg: 6, min: 0, max: fx(2) },         // k turns/m
    { name: 'p3', type: 'fx', reg: 7, min: 0, max: fx(8) },         // n cycles/loop
    { name: 'p4', type: 'fx', reg: 8, min: 0, max: fx(4) },         // amplitude m
    { name: 'p5', type: 'fx', reg: 9, min: 0, max: fx(24) },        // env inner r
    { name: 'p6', type: 'fx', reg: 10, min: 0, max: fx(24) },       // env outer r
    { name: 'p7', type: 'fx', reg: 11, min: fx(-1), max: fx(1) },   // unused
  ];
}

export interface WavePoolBuild {
  program: FieldProgram;
  bytes: Uint8Array;
  hash: number;
  /** PC of the SIN instruction in the final allocated code */
  sinPc: number;
  sinSpan: SourceSpan;
}

export function buildWavePool(): WavePoolBuild {
  const b = new FieldBuilder('earth', WAVE_POOL_SOURCE_ID, earthInputs());

  const x = b.inputVal(0), z = b.inputVal(1), phase = b.inputVal(3);
  const p0 = b.inputVal(4), p1 = b.inputVal(5), p2 = b.inputVal(6);
  const p3 = b.inputVal(7), p4 = b.inputVal(8), p5 = b.inputVal(9);
  const p6 = b.inputVal(10);

  const d = b.dist2(x, z, p0, p1, L(44, 10));         // d = |p − centre|
  const kd = b.mul(d, p2, L(48, 12));                 // k·d   (turns)
  const nt = b.mul(phase, p3, L(49, 12));             // n·phase (turns)
  const sinSpan = L(50, 12);
  const ang = b.sub(kd, nt, L(50, 22));               // travelling angle
  const s = b.sin(ang, sinSpan);                      // the wave
  const c = b.cos(ang, L(51, 12));                    // quadrature lane

  // envelope: 1 inside p5, falls to 0 at p6 (same reversed-edge trick as
  // crater_ring's walls) — pins the rim so the island edge does not flap
  const env = b.smoothstep(p6, p5, d, L(55, 10));

  const se = b.mul(s, env, L(59, 14));
  const height = b.mul(se, p4, L(59, 24));

  // velocity ∝ ∂h/∂phase up to the constant −2π·n (recorded lane, §7.1)
  const ce = b.mul(c, env, L(63, 14));
  const velocity = b.mul(ce, p4, L(63, 24));

  const material = b.ldc(MAT_SOIL, L(67, 18));

  const quarter = b.ldc(0x4000, L(70, 12));           // 0.25
  const nav = b.mul(height, quarter, L(70, 26));      // height / 4

  b.output('height', 'fx', height);
  b.output('velocity', 'fx', velocity);
  b.output('material', 'u32', material);
  b.output('nav_cost', 'fx', nav);
  b.end(L(72, 1));

  const program = buildProgram(b);
  const bytes = serializeProgram(program);
  const hash = programHashOfBytes(bytes);
  const sinPc = program.code.findIndex((i) => i.op === 'SIN');
  if (sinPc < 0) throw new Error('wave_pool: SIN instruction missing');

  // builder-side self-check: the serialized image must fully re-validate
  const decoded = decodeZprog(bytes);
  if (!decoded.ok) throw new Error(`wave_pool: self-validation failed: ${decoded.errors}`);

  return { program, bytes, hash, sinPc, sinSpan };
}
