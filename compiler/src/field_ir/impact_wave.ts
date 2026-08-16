// impact_wave.ts — impact on the terrain membrane (earth profile): the
// strike, an annular wave EXPANDING outward from the impact point, the
// centre rebounding like rubber, and the settle back to rest.
//
//   front   = d − speed·phase              (metres relative to the wave front)
//   wave    = wavelet(front) · ringdecay(phase)
//   centre  = smoothstep(p3, 0, d) · bounce(phase)
//   height  = amp · (wave + centre)
//
// The wavelet CURVE table is the radial cross-section of the travelling
// ring: a crest at the front with damped trailing ripples behind it — the
// donor's expanding waveLoc, expressed as one table lookup over a moving
// coordinate. bounce(phase) is the impact point itself: sharp depression,
// then a damped rebound oscillation (the rubbery settle). ringdecay(phase)
// carries the strike attack and the amplitude loss as the ring spreads;
// both phase tables end at 0 so the membrane returns exactly to rest.
//
// All inside the frozen v1 ISA: DIST2, MUL/SUB/ADD, three CURVE tables, the
// §3.20 smoothstep macro, and DCURVE for the honest velocity lane.

import { FieldBuilder } from './builder.js';
import { buildProgram } from './alloc.js';
import { FieldProgram, IoLane, SourceSpan } from './types.js';
import { serializeProgram, programHashOfBytes, decodeZprog } from './serialize.js';
import { MAT_SOIL } from './crater_ring.js';

/** kind 3 (field program), module 1, index 3 (capture_format.md §5). */
export const IMPACT_WAVE_SOURCE_ID = 0x30010003;

const L = (line: number, col: number): SourceSpan => ({
  sourceId: IMPACT_WAVE_SOURCE_ID, line, col,
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
    { name: 'p2', type: 'fx', reg: 6, min: 0, max: fx(30) },        // speed m/loop
    { name: 'p3', type: 'fx', reg: 7, min: 0, max: fx(24) },        // dome radius m
    { name: 'p4', type: 'fx', reg: 8, min: 0, max: fx(4) },         // amplitude m
    { name: 'p5', type: 'fx', reg: 9, min: fx(-1), max: fx(1) },    // unused
    { name: 'p6', type: 'fx', reg: 10, min: fx(-1), max: fx(1) },   // unused
    { name: 'p7', type: 'fx', reg: 11, min: fx(-1), max: fx(1) },   // unused
  ];
}

export interface ImpactWaveBuild {
  program: FieldProgram;
  bytes: Uint8Array;
  hash: number;
  /** PC of the first CURVE instruction (the wavelet lookup) */
  waveletPc: number;
  waveletSpan: SourceSpan;
}

export function buildImpactWave(): ImpactWaveBuild {
  const b = new FieldBuilder('earth', IMPACT_WAVE_SOURCE_ID, earthInputs());

  // The travelling ring's radial cross-section over front ∈ [−6, +1.5] m:
  // crest at the front, leading toe, damped trailing ripples. End knots are
  // 0 — CURVE clamps outside the span, so the wave is naturally local.
  const waveletTbl = b.addTable('curve', [
    { x: fx(-6.0), y: 0 },
    { x: fx(-4.8), y: fx(0.06) },
    { x: fx(-3.6), y: fx(-0.14) },
    { x: fx(-2.5), y: fx(0.3) },
    { x: fx(-1.6), y: fx(-0.42) },
    { x: fx(-0.9), y: fx(0.4) },
    { x: fx(-0.3), y: fx(1.0) },
    { x: fx(0.3), y: fx(0.8) },
    { x: fx(1.0), y: fx(0.2) },
    { x: fx(1.8), y: 0 },
  ]);

  // ring amplitude over phase: fades IN as the front leaves the splash dome
  // (below ~the dome radius a full-strength ring reads as a solid plug, not
  // a wave), peaks as it emerges, then spreads and loses amplitude; returns
  // exactly to 0
  const ringDecayTbl = b.addTable('curve', [
    { x: 0, y: 0 },
    { x: fx(0.18), y: fx(0.06) },
    { x: fx(0.3), y: fx(1.0) },
    { x: fx(0.5), y: fx(0.62) },
    { x: fx(0.7), y: fx(0.33) },
    { x: fx(0.87), y: fx(0.12) },
    { x: fx(1.0), y: 0 },
  ]);

  // the impact point: sharp depression, damped rebound oscillation, rest
  // (the rebound overshoot peaks as the ring departs the dome)
  const bounceTbl = b.addTable('curve', [
    { x: 0, y: 0 },
    { x: fx(0.06), y: fx(-1.0) },
    { x: fx(0.18), y: fx(0.6) },
    { x: fx(0.3), y: fx(-0.36) },
    { x: fx(0.42), y: fx(0.22) },
    { x: fx(0.54), y: fx(-0.12) },
    { x: fx(0.66), y: fx(0.06) },
    { x: fx(0.78), y: fx(-0.02) },
    { x: fx(0.9), y: 0 },
    { x: fx(1.0), y: 0 },
  ]);

  const x = b.inputVal(0), z = b.inputVal(1), phase = b.inputVal(3);
  const p0 = b.inputVal(4), p1 = b.inputVal(5), p2 = b.inputVal(6);
  const p3 = b.inputVal(7), p4 = b.inputVal(8);

  const d = b.dist2(x, z, p0, p1, L(52, 10));          // d = |p − centre|
  const adv = b.mul(phase, p2, L(56, 12));             // the front's radius
  const front = b.sub(d, adv, L(56, 24));              // moving coordinate
  const waveletSpan = L(57, 12);
  const wavelet = b.curve(waveletTbl, front, waveletSpan);
  const ringdecay = b.curve(ringDecayTbl, phase, L(58, 12));
  const wave = b.mul(wavelet, ringdecay, L(59, 12));

  const zero = b.ldc(0, L(63, 12));
  const cenv = b.smoothstep(p3, zero, d, L(63, 20));   // 1 at centre → 0 at p3
  const bounce = b.curve(bounceTbl, phase, L(64, 12));
  const centre = b.mul(cenv, bounce, L(65, 12));

  const hsum = b.add(wave, centre, L(67, 12));
  const height = b.mul(hsum, p4, L(67, 24));

  // honest velocity: the centre's ∂bounce/∂phase through the same envelope
  const dbounce = b.dcurve(bounceTbl, phase, L(71, 12));
  const velocity = b.mul(cenv, dbounce, L(71, 26));

  const material = b.ldc(MAT_SOIL, L(74, 18));

  const quarter = b.ldc(0x4000, L(77, 12));            // 0.25
  const nav = b.mul(height, quarter, L(77, 26));       // height / 4

  b.output('height', 'fx', height);
  b.output('velocity', 'fx', velocity);
  b.output('material', 'u32', material);
  b.output('nav_cost', 'fx', nav);
  b.end(L(79, 1));

  const program = buildProgram(b);
  const bytes = serializeProgram(program);
  const hash = programHashOfBytes(bytes);
  const waveletPc = program.code.findIndex((i) => i.op === 'CURVE');
  if (waveletPc < 0) throw new Error('impact_wave: CURVE instruction missing');

  // builder-side self-check: the serialized image must fully re-validate
  const decoded = decodeZprog(bytes);
  if (!decoded.ok) throw new Error(`impact_wave: self-validation failed: ${decoded.errors}`);

  return { program, bytes, hash, waveletPc, waveletSpan };
}
