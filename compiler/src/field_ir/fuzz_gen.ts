// fuzz_gen.ts — bounded-random Field IR program generator (plan W5, plan
// risk R2 follow-up: Csmith-style TS-vs-C++ differential over PROGRAMS, not
// just vectors). Builds programs through the typed builder so validity is by
// construction; the corpus is generated once, COMMITTED under
// tests/fuzz/corpus/field/, and replayed by the C++ nightly test
// test_field_fuzz_parity + the TS side here (compiler tests).
//
// Regeneration: `node dist/tests/field_fuzz_corpus.test.js` (writes on first
// run, byte-compares afterwards — staleness fails the test).

import { FieldBuilder } from './builder.js';
import { buildProgram } from './alloc.js';
import { IoLane, LaneType, TableKind } from './types.js';
import { serializeProgram, decodeZprog, programHashOfBytes } from './serialize.js';
import { generateInputs, encodeZvec } from './zvec.js';
import { interpret } from './interpret.js';

const fx = (v: number): number => Math.round(v * 65536);

class Rng {
  private s: number;
  constructor(seed: number) { this.s = seed >>> 0; }
  next(): number {
    this.s = (Math.imul(this.s, 747796405) + 2891336453) >>> 0;
    return ((this.s >>> ((this.s >>> 28) + 4)) ^ this.s) >>> 0;   // RXS-M-XS
  }
  below(n: number): number { return this.next() % n; }
  pick<T>(arr: T[]): T { return arr[this.below(arr.length)]!; }
  fxRaw(scale: number): number { return (this.below(scale) - scale / 2) | 0; }
}

function earthInputs(): IoLane[] {
  return [
    { name: 'x', type: 'fx', reg: 0, min: fx(-8), max: fx(8) },
    { name: 'z', type: 'fx', reg: 1, min: fx(-8), max: fx(8) },
    { name: 'age', type: 'u32', reg: 2, min: 0, max: 4096 },
    { name: 'phase', type: 'fx', reg: 3, min: 0, max: fx(1) },
    { name: 'p0', type: 'fx', reg: 4, min: fx(-4), max: fx(4) },
    { name: 'p1', type: 'fx', reg: 5, min: fx(-4), max: fx(4) },
    { name: 'p2', type: 'fx', reg: 6, min: 0, max: fx(6) },
    { name: 'p3', type: 'fx', reg: 7, min: 0, max: fx(6) },
    { name: 'p4', type: 'fx', reg: 8, min: 0, max: fx(2) },
    { name: 'p5', type: 'fx', reg: 9, min: fx(-1), max: fx(1) },
    { name: 'p6', type: 'fx', reg: 10, min: fx(-1), max: fx(1) },
    { name: 'p7', type: 'fx', reg: 11, min: fx(-1), max: fx(1) },
  ];
}

/** One bounded-random earth program; returns serialized bytes + hash. */
export function genFieldProgram(seed: number):
    { bytes: Uint8Array; hash: number; name: string } {
  const rng = new Rng(seed);
  const b = new FieldBuilder('earth', 0x30010000 + (seed & 0xffff), earthInputs());
  const span = { sourceId: 0x30010000 + (seed & 0xffff), line: 1, col: 1 };

  // one random 6..9-entry curve table + one uniform 5-entry spline table
  const curveLen = 6 + rng.below(4);
  const curvePts = Array.from({ length: curveLen }, (_, i) => ({
    x: Math.round((fx(1) * i) / Math.max(1, curveLen - 1)),
    y: rng.fxRaw(fx(2)),
  }));
  curvePts[0]!.x = 0;                      // strictly increasing from 0
  const curve = b.addTable('curve' as TableKind, curvePts);
  const step = 1 << (12 + rng.below(4));
  const splinePts = Array.from({ length: 5 }, (_, i) => ({
    x: step * i, y: rng.fxRaw(fx(2)),
  }));
  const spline = b.addTable('spline' as TableKind, splinePts);

  const live: number[] = Array.from({ length: 12 }, (_, i) => b.inputVal(i));
  const outputs: { name: string; type: LaneType; val: number }[] = [];
  const opCount = 10 + rng.below(10);

  for (let i = 0; i < opCount; i++) {
    const v = () => rng.pick(live);
    let out: number;
    switch (rng.below(16)) {
      case 0: out = b.add(v(), v(), span); break;
      case 1: out = b.sub(v(), v(), span); break;
      case 2: out = b.mul(v(), v(), span); break;
      case 3: out = b.mad(v(), v(), v(), span); break;
      case 4: out = b.min(v(), v(), span); break;
      case 5: out = b.max(v(), v(), span); break;
      case 6: out = b.abs(v(), span); break;
      case 7: out = b.clamp(v(), v(), v(), span); break;
      case 8: out = b.select(v(), v(), v(), span); break;
      case 9: out = b.cmp(v(), v(), rng.below(6) as 0 | 1 | 2 | 3 | 4 | 5, span); break;
      case 10: out = b.rcp(v(), span); break;
      case 11: out = b.sin(v(), span); break;
      case 12: out = b.curve(rng.below(2), v(), span); break;
      case 13: out = b.dcurve(0, v(), span); break;
      case 14: out = b.spline(1, v(), span); break;
      default: out = b.ldc(rng.fxRaw(fx(2)), span); break;
    }
    live.push(out);
    if (live.length > 40) live.shift();   // keep the live set bounded
  }
  // exercise a vector op too (NOISE2 pair output)
  try {
    const pair = b.noise2(rng.pick(live), rng.pick(live), rng.next(), span);
    live.push(pair[0], pair[1]);
  } catch {
    /* register pressure: skip */
  }

  const laneTypes: LaneType[] = ['fx', 'fx', 'u32', 'fx'];
  const scratch = live.filter((v) => v >= 12);   // outputs never input regs (V6)
  const used = new Set<number>();                // and never duplicated
  for (let k = 0; k < 4; k++) {
    const pool = scratch.filter((v) => !used.has(v));
    let val: number;
    if (pool.length > 0) {
      val = rng.pick(pool);
      used.add(val);
    } else {
      val = b.ldc(k, span);
    }
    b.output(`out${k}`, laneTypes[k]!, val);
  }
  b.end(span);

  const program = buildProgram(b);
  const bytes = serializeProgram(program);
  const dec = decodeZprog(bytes);
  if (!dec.ok) throw new Error(`fuzz program ${seed} invalid: ${dec.errors.join('; ')}`);
  return { bytes, hash: programHashOfBytes(bytes), name: `fuzz_seed_${seed}` };
}

/** TS-expected vectors for a corpus program (the C++ replay must agree). */
export function genFieldVectors(bytes: Uint8Array, hash: number, seed: number,
                                n: number): Uint8Array {
  const dec = decodeZprog(bytes);
  if (!dec.ok) throw new Error(dec.errors.join('; '));
  const bounds = dec.prog.inLanes.map((l) => ({ min: l.min, max: l.max }));
  const inputs = generateInputs(hash, seed, n, bounds);
  const records = inputs.map((inRec) => {
    const r = interpret(dec.prog, inRec);
    return {
      inputs: inRec,
      expected: r.outputs,
      status: (r.status.sat ? 1 : 0) | (r.status.rcp0 ? 2 : 0),
    };
  });
  return encodeZvec(hash, seed, records, bounds.length,
                    dec.prog.outLanes.length);
}
