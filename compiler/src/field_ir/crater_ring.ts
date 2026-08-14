// crater_ring.ts — the W5 acceptance program (spec/form/field-ir.md §12,
// P4 §10 with the ratified DCURVE velocity lane). Earth profile, 28
// instructions + END ≤ ceiling 32.
//
// This module is the single source of the program for the TS tests, the
// wrapper emitter and the fuzz corpus seeds (never hand-duplicated).

import { FieldBuilder } from './builder.js';
import { buildProgram } from './alloc.js';
import { FieldProgram, IoLane, SourceSpan } from './types.js';
import { serializeProgram, programHashOfBytes, decodeZprog } from './serialize.js';

/** kind 3 (field program), module 1, index 1 (capture_format.md §5). */
export const CRATER_RING_SOURCE_ID = 0x30010001;

const L = (line: number, col: number): SourceSpan => ({
  sourceId: CRATER_RING_SOURCE_ID, line, col,
});

/** raw fx helper */
const fx = (v: number): number => Math.round(v * 65536);

function earthInputs(): IoLane[] {
  return [
    { name: 'x', type: 'fx', reg: 0, min: fx(-40), max: fx(40) },
    { name: 'z', type: 'fx', reg: 1, min: fx(-40), max: fx(40) },
    { name: 'age', type: 'u32', reg: 2, min: 0, max: 65535 },
    { name: 'phase', type: 'fx', reg: 3, min: 0, max: fx(1) },
    { name: 'p0', type: 'fx', reg: 4, min: fx(-32), max: fx(32) },   // centre x
    { name: 'p1', type: 'fx', reg: 5, min: fx(-32), max: fx(32) },   // centre z
    { name: 'p2', type: 'fx', reg: 6, min: 0, max: fx(24) },         // r_in
    { name: 'p3', type: 'fx', reg: 7, min: 0, max: fx(24) },         // r_out
    { name: 'p4', type: 'fx', reg: 8, min: 0, max: fx(4) },          // amplitude
    { name: 'p5', type: 'fx', reg: 9, min: fx(-1), max: fx(1) },
    { name: 'p6', type: 'fx', reg: 10, min: fx(-1), max: fx(1) },
    { name: 'p7', type: 'fx', reg: 11, min: fx(-1), max: fx(1) },
  ];
}

/** material tags (u32 output lane) */
export const MAT_SOIL = 1;
export const MAT_CHARRED = 2;

export interface CraterRingBuild {
  program: FieldProgram;
  bytes: Uint8Array;
  hash: number;
  /** PC of the RING instruction in the final allocated code (§12 check 4) */
  ringPc: number;
  ringSpan: SourceSpan;
}

export function buildCraterRing(): CraterRingBuild {
  const b = new FieldBuilder('earth', CRATER_RING_SOURCE_ID, earthInputs());

  // attack/decay envelope over phase (8-entry {x,y} table; dy offline §3.15)
  const ageCurve = b.addTable('curve', [
    { x: 0, y: 0 },
    { x: fx(0.09), y: fx(0.8) },
    { x: fx(0.19), y: fx(1.0) },
    { x: fx(0.31), y: fx(0.9) },
    { x: fx(0.41), y: fx(0.5) },
    { x: fx(0.5), y: fx(0.2) },
    { x: fx(0.75), y: fx(0.05) },
    { x: fx(1.0), y: 0 },
  ]);

  // §12 program
  const x = b.inputVal(0), z = b.inputVal(1), phase = b.inputVal(3);
  const p0 = b.inputVal(4), p1 = b.inputVal(5), p2 = b.inputVal(6);
  const p3 = b.inputVal(7), p4 = b.inputVal(8);

  const d = b.dist2(x, z, p0, p1, L(44, 10));              // d = |p − centre|
  const ringSpan = L(48, 12);
  const band = b.ring(d, p2, p3, ringSpan);                // band-pass
  const walls = b.smoothstep(p3, p2, d, L(52, 10));        // §3.20 macro
  const depth = b.curve(ageCurve, phase, L(56, 10));
  const velocity = b.dcurve(ageCurve, phase, L(57, 10));   // derivative lane

  const h1 = b.mul(band, walls, L(60, 14));
  const h2 = b.mul(h1, depth, L(60, 24));
  const height = b.mul(h2, p4, L(60, 34));

  const k09 = b.ldc(fx(0.9), L(64, 12));                   // 0.9 (P4 wrote
  const hot = b.cmp(depth, k09, 4, L(64, 20));             // 0x0E66 — a typo
  const matCharred = b.ldc(MAT_CHARRED, L(65, 22));        // for 0xE666)
  const matSoil = b.ldc(MAT_SOIL, L(65, 36));
  const material = b.select(hot, matCharred, matSoil, L(66, 17));

  const quarter = b.ldc(0x4000, L(70, 12));                // 0.25
  const nav = b.mul(height, quarter, L(70, 26));           // height / 4

  b.output('height', 'fx', height);
  b.output('velocity', 'fx', velocity);
  b.output('material', 'u32', material);
  b.output('nav_cost', 'fx', nav);
  b.end(L(72, 1));

  const program = buildProgram(b);
  const bytes = serializeProgram(program);
  const hash = programHashOfBytes(bytes);
  const ringPc = program.code.findIndex((i) => i.op === 'RING');
  if (ringPc < 0) throw new Error('crater_ring: RING instruction missing');

  // builder-side self-check: the serialized image must fully re-validate
  const decoded = decodeZprog(bytes);
  if (!decoded.ok) throw new Error(`crater_ring: self-validation failed: ${decoded.errors}`);

  return { program, bytes, hash, ringPc, ringSpan };
}
