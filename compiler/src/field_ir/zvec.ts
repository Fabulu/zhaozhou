// zvec.ts — .zvec golden/failing vector container + the pinned PCG vector
// input generator and minimize policy.
//
// Spec: spec/form/field-ir.md (FIELD_IR_VERSION 1)
//   §6.1  .zvec layout (magic ZFIV, 32-byte header, CRC over field-zeroed file)
//   §6.2  generation = pure function of (program_hash, seed, N); corners
//         mandatory; PCG constants frozen verbatim from qformats.md §7.5
//   §6.3  failing vectors + per-lane bisection minimize (≤ 64 steps/lane)

import { crc32c } from '../generated/abi.js';
import { ByteReader, ByteWriter } from './serialize.js';

export const ZVEC_MAGIC = 0x5649465a;  // 'Z','F','I','V' LE
export const ZVEC_HEADER_BYTES = 32;

export interface ZvecRecord {
  inputs: number[];      // in_lanes × i32 raw
  expected: number[];    // out_lanes × i32 raw
  status: number;        // bit0 sat, bit1 rcp0
}

export interface ZvecFile {
  programHash: number;
  seed: number;          // u64
  inLanes: number;
  outLanes: number;
  records: ZvecRecord[];
  bytes: Uint8Array;
}

export function encodeZvec(programHash: number, seed: number, records: ZvecRecord[],
                           inLanes: number, outLanes: number): Uint8Array {
  const h = new ByteWriter();
  h.u32(ZVEC_MAGIC);
  h.u16(1);                       // version
  h.u16(32);                      // lane_bits
  h.u32(programHash);
  const seedLo = seed % 4294967296;
  const seedHi = Math.floor(seed / 4294967296);
  h.u32(seedLo);
  h.u32(seedHi);
  h.u32(records.length);
  h.u8(inLanes);
  h.u8(outLanes);
  h.u16(0);                       // rsvd
  h.u32(0);                       // crc placeholder
  const body = new ByteWriter();
  for (const rec of records) {
    for (const v of rec.inputs) body.i32(v);
    for (const v of rec.expected) body.i32(v);
    body.u32(rec.status);
  }
  const file = new ByteWriter();
  file.bytes(h.toUint8Array());
  file.bytes(body.toUint8Array());
  const out = file.toUint8Array();
  const crc = crc32c(0, out);
  new DataView(out.buffer, out.byteOffset).setUint32(28, crc, true);
  return out;
}

export function decodeZvec(file: Uint8Array):
  { ok: true; zvec: ZvecFile } | { ok: false; errors: string[] } {
  const errors: string[] = [];
  if (file.length < ZVEC_HEADER_BYTES) return { ok: false, errors: ['zvec: shorter than header'] };
  const r = new ByteReader(file);
  if (r.u32() !== ZVEC_MAGIC) errors.push('zvec: bad magic');
  if (r.u16() !== 1) errors.push('zvec: bad version');
  if (r.u16() !== 32) errors.push('zvec: bad lane_bits');
  const programHash = r.u32();
  const seed = r.u64();
  const vectorCount = r.u32();
  const inLanes = r.u8();
  const outLanes = r.u8();
  const rsvd = r.u16();
  const crcField = r.u32();
  if (rsvd !== 0) errors.push('zvec: reserved nonzero');
  const expectedLen = ZVEC_HEADER_BYTES +
    vectorCount * (inLanes * 4 + outLanes * 4 + 4);
  if (file.length !== expectedLen) errors.push(`zvec: length ${file.length} != ${expectedLen}`);
  if (errors.length > 0) return { ok: false, errors };
  const zeroed = Uint8Array.from(file);
  new DataView(zeroed.buffer, zeroed.byteOffset).setUint32(28, 0, true);
  if (crc32c(0, zeroed) !== crcField) return { ok: false, errors: ['zvec: CRC mismatch'] };
  const records: ZvecRecord[] = [];
  for (let i = 0; i < vectorCount; i++) {
    const inputs: number[] = [];
    for (let j = 0; j < inLanes; j++) inputs.push(r.i32());
    const expected: number[] = [];
    for (let j = 0; j < outLanes; j++) expected.push(r.i32());
    records.push({ inputs, expected, status: r.u32() });
  }
  return { ok: true, zvec: { programHash, seed, inLanes, outLanes, records, bytes: file } };
}

// ---------------------------------------------------------------------------
// §6.2 generation — pure function of (program_hash, seed, N)
// ---------------------------------------------------------------------------

/** PCG RXS-M-XS output permutation on the advanced state (qformats.md §7.5). */
function pcgOutput(s: number): number {
  const w = Math.imul(((s >>> ((s >>> 28) + 4)) ^ s) >>> 0, 277803737);
  return ((w >>> 22) ^ w) >>> 0;
}

export class VecPRNG {
  private state: number;
  constructor(programHash: number, seed: number) {
    this.state = ((seed % 4294967296) ^ programHash) >>> 0;
  }
  draw(): number {
    this.state = (Math.imul(this.state, 747796405) + 2891336453) >>> 0;
    return pcgOutput(this.state);
  }
  /** Uniform raw lane value over [min, max] (§6.2; full-range safe). */
  lane(min: number, max: number): number {
    const r = this.draw();
    const minU = min >>> 0;
    const count = ((max >>> 0) - minU + 1) >>> 0;   // 0 ⇔ full s32 range
    if (count === 0) return r | 0;
    return (minU + (r % count)) | 0;
  }
}

export interface LaneBounds { min: number; max: number }

/**
 * The pinned input-record sequence (§6.2): 3 global corners, per-lane min
 * corner with others uniform, then N uniform records — a single sequential
 * PRNG stream (draw order is part of the frozen definition; TS and C++ must
 * draw identically, which the golden .zvec asserts).
 */
export function generateInputs(programHash: number, seed: number, n: number,
                               bounds: LaneBounds[]): number[][] {
  const prng = new VecPRNG(programHash, seed);
  const uni = (i: number) => prng.lane(bounds[i]!.min, bounds[i]!.max);
  const clampIn = (b: LaneBounds, v: number) =>
    v < b.min ? b.min : (v > b.max ? b.max : v);
  const records: number[][] = [];
  records.push(bounds.map((b) => b.min));
  records.push(bounds.map((b) => b.max));
  records.push(bounds.map((b) => clampIn(b, 0)));
  for (let l = 0; l < bounds.length; l++) {
    records.push(bounds.map((b, i) => (i === l ? b.min : uni(i))));
  }
  for (let k = 0; k < n; k++) {
    records.push(bounds.map((b, i) => uni(i)));
  }
  return records;
}

// ---------------------------------------------------------------------------
// §6.3 minimize — per-lane bisection toward the nearer bound, ≤ 64 steps
// ---------------------------------------------------------------------------

/**
 * Deterministic minimization of a failing input record. `fails(inputs)` must
 * be the replayed divergence predicate (same record → same verdict). Returns
 * the minimized input record.
 */
export function minimizeRecord(inputs: number[], bounds: LaneBounds[],
                               fails: (inputs: number[]) => boolean,
                               maxSteps = 64): number[] {
  let cur = [...inputs];
  for (let i = 0; i < cur.length; i++) {
    const { min, max } = bounds[i]!;
    let steps = 0;
    while (steps < maxSteps) {
      const v = cur[i]!;
      const target = (v - min) <= (max - v) ? min : max;
      if (v === target) break;
      // round-half-up midpoint toward the target
      const mid = target > v
        ? Math.floor((v + target + 1) / 2)
        : Math.ceil((v + target - 1) / 2);
      if (mid === v) break;
      const next = [...cur];
      next[i] = mid;
      if (fails(next)) cur = next;               // keep any still-failing input
      else break;                                 // lost the failure: next lane
      steps++;
    }
  }
  return cur;
}

/** Divergence report (§6.3). */
export interface DivergenceReport {
  vector_index: number;
  first_divergent_lane: number;
  expected: number;
  actual: number;
  status_diff: number;
}

export function findDivergence(rec: ZvecRecord, actual: number[], status: number):
  DivergenceReport | null {
  for (let i = 0; i < rec.expected.length; i++) {
    if (rec.expected[i] !== actual[i]) {
      return {
        vector_index: -1,                        // filled by the caller
        first_divergent_lane: i,
        expected: rec.expected[i]!,
        actual: actual[i]!,
        status_diff: (rec.status ^ status) >>> 0,
      };
    }
  }
  return null;
}
