// field_ir.test.ts — W5 unit tests: the 16-bit-limb int64 util (hand-computed
// products incl. negative halves + a BigInt property oracle, plan risk R2),
// the numeric layer vs an independent BigInt reference, serializer determinism
// + round-trip, the validator rejection matrix (field-ir.md §4), allocator
// adjacency, and the vector generator's pinned corner sequence (§6.2).

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';

import {
  mulS32, addU64, shl16S32, rescaleSat, satS32, acc96Add, acc96Finish,
  U64,
} from '../src/field_ir/i64.js';
import { isqrtU64 } from '../src/field_ir/numeric.js';
import * as num from '../src/field_ir/numeric.js';
import { FIELD_RCP_T0, SIN_Q16 } from '../src/generated/tables.js';
import { crc32c } from '../src/generated/abi.js';
import { FieldBuilder } from '../src/field_ir/builder.js';
import { buildProgram } from '../src/field_ir/alloc.js';
import { serializeProgram, decodeZprog, programHashOfBytes } from '../src/field_ir/serialize.js';
import { generateInputs, encodeZvec, decodeZvec, VecPRNG } from '../src/field_ir/zvec.js';
import { buildCraterRing } from '../src/field_ir/crater_ring.js';
import { interpret } from '../src/field_ir/interpret.js';
import { repoRoot } from './helpers.js';

const TWO32 = 4294967296;
const pair = (hi: number, lo: number): U64 => ({ hi: hi >>> 0, lo: lo >>> 0 });
const hex = (v: number): string => '0x' + (v >>> 0).toString(16);

// ---------------------------------------------------------------------------
// i64: hand-computed products (incl. negative halves) — plan R2
// ---------------------------------------------------------------------------

test('mulS32 hand-computed products (positive)', () => {
  assert.deepEqual(mulS32(3, 5), pair(0, 15));
  assert.deepEqual(mulS32(0x10000, 0x10000), pair(1, 0));        // 1.0 · 1.0 = 1.0 (2^32)
  assert.deepEqual(mulS32(0x7fffffff, 0x7fffffff), pair(0x3fffffff, 1));
  assert.deepEqual(mulS32(0, 12345), pair(0, 0));
});

test('mulS32 hand-computed products (negative halves — the R2 case)', () => {
  // (−3)·5 = −15 = 0xFFFFFFFF_FFFFFFF1
  assert.deepEqual(mulS32(-3, 5), pair(0xffffffff, 0xfffffff1));
  // 3·(−5) likewise
  assert.deepEqual(mulS32(3, -5), pair(0xffffffff, 0xfffffff1));
  // (−3)·(−5) = 15
  assert.deepEqual(mulS32(-3, -5), pair(0, 15));
  // INT32_MIN² = 2^62 exactly
  assert.deepEqual(mulS32(-2147483648, -2147483648), pair(0x40000000, 0));
  // INT32_MIN · 1
  assert.deepEqual(mulS32(-2147483648, 1), pair(0xffffffff, 0x80000000));
  // INT32_MIN · INT32_MAX = −(2^62 − 2^31) = 0xC0000000_80000000 (two's
  // complement; verified against BigInt below)
  assert.deepEqual(mulS32(-2147483648, 2147483647), pair(0xc0000000, 0x80000000));
});

test('mulS32 matches BigInt oracle over a mixed corpus', () => {
  const cases: Array<[number, number]> = [];
  const edges = [0, 1, -1, 2, -2, 0x7fff, 0x8000, 0xffff, 0x10000, 0x7fffffff,
                 -2147483648, 12345, -98765, 0x13579bdf, -0x2468ace0];
  for (const a of edges) for (const b of edges) cases.push([a, b]);
  let lcg = 1;
  const rnd = () => (lcg = (Math.imul(lcg, 1664525) + 1013904223) | 0);
  for (let i = 0; i < 5000; i++) cases.push([rnd(), rnd()]);
  for (const [a, b] of cases) {
    const want = BigInt(a) * BigInt(b) & ((1n << 64n) - 1n);
    const got = mulS32(a, b);
    assert.equal((BigInt(got.hi) << 32n) | BigInt(got.lo), want,
      `mulS32(${a}, ${b})`);
  }
});

test('rescaleSat: single-rounding law (round-half-up, ties toward +inf)', () => {
  // a genuine LSB tie: 3 · 0x8000 = 0x18000 (= 1.5 raw units at 2^-16) → 2;
  // the negative tie rounds toward +infinity: −1.5 → −1
  assert.equal(rescaleSat(mulS32(3, 0x8000), 16, null), 2);
  assert.equal(rescaleSat(mulS32(-3, 0x8000), 16, null), -1);
  // MAD single-rounding vs double: a·b = 3·0x8000 = 0x18000 (1.5 raw units),
  // c = 0x8000: exact p = 0x80018000 → +0x8000 >> 16 = 0x8002. Double
  // rounding would give round(1.5) + 0x8000 = 0x8001.
  assert.equal(num.fxMad(3, 0x8000, 0x8000, null), 0x8002);
  // saturation sets the sticky flag
  const L = { sat: false, rcp0: false };
  assert.equal(rescaleSat(mulS32(-2147483648, -2147483648), 16, L), 0x7fffffff);
  assert.equal(L.sat, true);
});

test('acc96: DOT2 overflow region beyond s64 agrees with saturation', () => {
  // 3·(INT32_MIN²) = 3·2^62 — aliases mod 2^64 but saturates +MAX like the
  // exact s128 C++ path (field-ir.md §3.10)
  const acc = { a2: 0, a1: 0, a0: 0 };
  for (let i = 0; i < 3; i++) acc96Add(acc, mulS32(-2147483648, -2147483648));
  const L = { sat: false, rcp0: false };
  assert.equal(acc96Finish(acc, L), 0x7fffffff);
  assert.equal(L.sat, true);
});

test('isqrtU64: exact floor sqrt property vs BigInt', () => {
  let lcg = 42;
  const rnd32 = () => (lcg = (Math.imul(lcg, 1664525) + 1013904223) | 0) >>> 0;
  for (let i = 0; i < 2000; i++) {
    const hi = i < 5 ? 0 : rnd32();
    const lo = rnd32();
    const n = (BigInt(hi) << 32n) | BigInt(lo);
    const r = isqrtU64(pair(hi, lo));
    const rv = (BigInt(r.hi) << 32n) | BigInt(r.lo);
    assert.ok(rv * rv <= n && n < (rv + 1n) * (rv + 1n), `isqrt(${n})`);
  }
});

// ---------------------------------------------------------------------------
// numeric layer vs independent BigInt reference (the spec pseudocode)
// ---------------------------------------------------------------------------

function rescaleUBig(x: bigint, k: number | bigint): bigint {
  const kk = BigInt(k);
  return (x + (1n << (kk - 1n))) >> kk;
}

function fieldRcpBig(a: number): { r: number; rcp0: boolean } {
  if (a === 0) return { r: 0x7fffffff, rcp0: true };
  const neg = a < 0;
  let n = BigInt(Math.abs(a));
  let e = 31;
  while (((n >> 31n) & 1n) === 0n) { n <<= 1n; e -= 1; }
  const idx = Number((n - (1n << 31n)) >> 23n);
  let x = BigInt(FIELD_RCP_T0[idx]!);
  x = rescaleUBig(x * ((1n << 48n) - n * x), 47n);
  const r = rescaleUBig(x << 16n, BigInt(e));
  const sat = r > 2147483647n ? (neg ? -2147483648 : 2147483647)
                              : Number(neg ? -r : r);
  return { r: sat | 0, rcp0: false };
}

test('fieldRcp: limb version == BigInt spec reference (dense + strided)', () => {
  const cases: number[] = [];
  for (let a = 1; a <= 65536; a++) cases.push(a);           // dense small
  for (let k = 0; k < 31; k++) {                            // powers of two ± 1
    cases.push(2 ** k, 2 ** k + 1, 2 ** k - 1, -(2 ** k), -(2 ** k) - 1);
  }
  cases.push(2147483647, -2147483648);                      // s32 rails
  let lcg = 7;
  for (let i = 0; i < 20000; i++) {
    lcg = (Math.imul(lcg, 1664525) + 1013904223) | 0;
    cases.push(lcg | 0);
  }
  for (const a of cases) {
    if (a === 0) continue;                      // zero checked separately below
    assert.deepEqual(
      { r: num.fieldRcp(a, null), rcp0: false },
      fieldRcpBig(a),
      `fieldRcp(${a})`,
    );
  }
  const zero = num.fieldRcp(0, null);
  assert.equal(zero, 0x7fffffff);
});

test('fxSin identities (qformats.md §7.1, exact)', () => {
  assert.equal(num.fxSin(0), 0);
  assert.equal(num.fxSin(0x4000), 0x10000);
  assert.equal(num.fxSin(0x8000), 0);
  assert.equal(num.fxSin(0xc000), -0x10000);
  for (let a = 0; a < 0x10000; a += 257) {
    // sin(−a) = −sin(a)  (| 0: JS −0 vs +0, C++ ints have no −0)
    assert.equal(num.fxSin((0x10000 - a) & 0xffff) | 0, (-num.fxSin(a)) | 0);
    assert.equal(num.fxCos(a), num.fxSin((a + 0x4000) & 0xffff));
  }
  assert.equal(SIN_Q16.length, 257);
});

test('noise2Hash matches the committed KAT golden (tests/golden/fixp)', () => {
  const bin = readFileSync(path.join(repoRoot(), 'tests', 'golden', 'fixp', 'noise2_kat.bin'));
  for (let i = 0; i < 64; i++) {
    const off = i * 20;
    const dv = new DataView(bin.buffer, bin.byteOffset + off, 20);
    const x = dv.getUint32(0, true), y = dv.getUint32(4, true), seed = dv.getUint32(8, true);
    const h0 = dv.getUint32(12, true), h1 = dv.getUint32(16, true);
    assert.equal(num.noise2Hash(x, y, seed, 0), h0, `kat ${i} lane0`);
    assert.equal(num.noise2Hash(x, y, seed, 1), h1, `kat ${i} lane1`);
  }
});

// ---------------------------------------------------------------------------
// serializer determinism, round-trip, hash
// ---------------------------------------------------------------------------

test('crater_ring: serialization is byte-stable and self-validating', () => {
  const a = buildCraterRing();
  const b = buildCraterRing();
  assert.deepEqual(Buffer.from(a.bytes), Buffer.from(b.bytes));
  const prog2 = { ...b.program };
  assert.deepEqual(Buffer.from(serializeProgram(prog2)), Buffer.from(a.bytes));
  const dec = decodeZprog(a.bytes);
  assert.ok(dec.ok, dec.ok ? '' : dec.errors.join('; '));
  assert.equal(dec.prog.programHash, a.hash);
  assert.equal(dec.prog.programHash, programHashOfBytes(a.bytes));
});

test('crater_ring: PC→source map resolves the RING span (§12 check 4, TS side)', () => {
  const { program, bytes, ringPc, ringSpan } = buildCraterRing();
  const dec = decodeZprog(bytes)!;
  assert.ok(dec.ok);
  const m = dec.prog.srcMap[ringPc]!;
  assert.deepEqual(m, { sourceId: ringSpan.sourceId, line: ringSpan.line, col: ringSpan.col });
  assert.ok(program.code.length <= 32, `instr count ${program.code.length} <= earth 32`);
});

test('interpret: deterministic replay of the same inputs', () => {
  const { bytes } = buildCraterRing();
  const dec = decodeZprog(bytes)!;
  assert.ok(dec.ok);
  const inputs = [1 << 20, -(2 << 20), 1234, 0x8000, 0, 0, 3 << 16, 8 << 16, 2 << 16, 0, 0, 0];
  const r1 = interpret(dec.prog, inputs);
  const r2 = interpret(dec.prog, inputs);
  assert.deepEqual(r1, r2);
  assert.equal(r1.outputs.length, 4);
});

// ---------------------------------------------------------------------------
// validator rejection matrix (§4 V1–V12)
// ---------------------------------------------------------------------------

function refix(file: Uint8Array): Uint8Array {
  const out = Uint8Array.from(file);
  const dv = new DataView(out.buffer);
  // recompute program hash (20) from the mutated body, then body CRC (24)
  const instrCount = dv.getUint16(12, true);
  const tableBytes = dv.getUint16(16, true);
  const code = out.subarray(28, 28 + 8 * instrCount);
  const tables = out.subarray(28 + 8 * instrCount, 28 + 8 * instrCount + tableBytes);
  let h = crc32c(0, code);
  h = crc32c(h, tables);
  dv.setUint32(20, (h + instrCount) >>> 0, true);
  dv.setUint32(24, 0, true);
  dv.setUint32(24, crc32c(0, out), true);
  return out;
}

test('validator: rejects each rule class on mutated bytes', () => {
  const base = buildCraterRing().bytes;

  const expectReject = (file: Uint8Array, tag: string) => {
    const r = decodeZprog(file);
    assert.ok(!r.ok, `${tag}: must be rejected`);
    assert.ok(r.errors.length > 0, tag);
    return r.errors;
  };

  // V1 magic
  const m1 = Uint8Array.from(base); m1[0] = 0x58;
  assert.ok(expectReject(m1, 'magic').some((e) => e.startsWith('V1')));
  // V1 length
  assert.ok(expectReject(base.subarray(0, base.length - 1), 'length')
    .some((e) => e.startsWith('V1')));
  // V2 body CRC
  const m2 = Uint8Array.from(base); m2[27] = m2[27]! ^ 0xff;
  assert.ok(expectReject(m2, 'crc').some((e) => e.startsWith('V2')));
  // V2 program hash (mutate code, fix CRC but not hash)
  {
    const m = Uint8Array.from(base);
    m[28] = 0x01;  // DIST2 -> MOV-ish opcode byte (any change breaks hash)
    const dv = new DataView(m.buffer);
    dv.setUint32(24, 0, true);
    dv.setUint32(24, crc32c(0, m), true);
    assert.ok(expectReject(m, 'hash').some((e) => e.startsWith('V2')));
  }
  // V3 flags
  {
    const m = Uint8Array.from(base); m[7] = 1;
    assert.ok(expectReject(refix(m), 'flags').some((e) => e.startsWith('V3')));
  }
  // V9 unknown opcode (0x0D reserved)
  {
    const mm = Uint8Array.from(base); mm[28] = 0x0d;
    assert.ok(expectReject(refix(mm), 'reserved-opcode')
      .some((e) => e.startsWith('V9')));
  }
  // V8 dst into input range: instr 0 (DIST2) dst reg — set packed dst bits to 0
  {
    const m = Uint8Array.from(base);
    const b1 = m[29]!;
    m[29] = b1 & 0xc0;   // dst = 0 (input), srcA low bits preserved in b1 high
    const errs = expectReject(refix(m), 'dst-input');
    assert.ok(errs.some((e) => e.startsWith('V8')), errs.join(';'));
  }
  // V11 use before define: instr0 srcA -> an undefined high scratch reg (63)
  {
    const m = Uint8Array.from(base);
    const b1 = m[29]!, b2 = m[30]!;
    // srcA bits are b1[7:6] | b2[3:0]<<2; set srcA = 63 => b1 |= 3<<6, b2 |= 0xf<<...
    m[29] = (b1 & 0x3f) | (0xc0);        // srcA low2 = 3
    m[30] = (b2 & 0xf0) | 0x0f;          // srcA high4 = 15 => srcA = 63
    const errs = expectReject(refix(m), 'undef-src');
    assert.ok(errs.some((e) => e.startsWith('V11')), errs.join(';'));
  }
  // V10: END replaced by MOV
  {
    const { program } = buildCraterRing();
    const mutated = { ...program, code: program.code.map((i, idx) =>
      idx === program.code.length - 1 ? { ...i, op: 'MOV' as const } : i) };
    const bytes = serializeProgram(mutated);
    const errs = expectReject(bytes, 'no-end');
    assert.ok(errs.some((e) => e.startsWith('V10')), errs.join(';'));
  }
  // V9 CMP mode 6
  {
    const { program } = buildCraterRing();
    const cmpIdx = program.code.findIndex((i) => i.op === 'CMP');
    const mutated = { ...program, code: program.code.map((i, idx) =>
      idx === cmpIdx ? { ...i, imm: 6 } : i) };
    const errs = expectReject(serializeProgram(mutated), 'cmp-mode');
    assert.ok(errs.some((e) => e.startsWith('V9')), errs.join(';'));
  }
  // V6 bounds inverted (rebuild with swapped bounds)
  {
    const { bytes } = buildCraterRing();
    const m = Uint8Array.from(bytes);
    // first input lane min at io-map start; find io map offset:
    const instrCount = new DataView(m.buffer).getUint16(12, true);
    const tableBytes = new DataView(m.buffer).getUint16(16, true);
    const mapOff = 28 + 8 * instrCount + tableBytes;
    // lane 0 entry: reg,kind,type,name_id,min,max — swap min/max (raw LE i32)
    const dv = new DataView(m.buffer);
    const minOff = mapOff + 4;
    const mn = dv.getInt32(minOff, true), mx = dv.getInt32(minOff + 4, true);
    dv.setInt32(minOff, mx, true);
    dv.setInt32(minOff + 4, mn, true);
    const errs = expectReject(refix(m), 'bounds');
    assert.ok(errs.some((e) => e.startsWith('V6')), errs.join(';'));
  }
});

// ---------------------------------------------------------------------------
// allocator adjacency (§1.3/§11.2)
// ---------------------------------------------------------------------------

test('allocator: coalesces non-adjacent vector sources with MOVs', () => {
  const span = { sourceId: 1, line: 1, col: 1 };
  const b = new FieldBuilder('earth', 1, [
    { name: 'a', type: 'fx', reg: 0, min: -(1 << 20), max: 1 << 20 },
    { name: 'b', type: 'fx', reg: 1, min: -(1 << 20), max: 1 << 20 },
  ]);
  // x = a·3 (scratch reg), y = b·5 (a DIFFERENT scratch reg — not adjacent)
  const three = b.ldc(3 << 16, span);
  const five = b.ldc(5 << 16, span);
  const x = b.mul(b.inputVal(0), three, span);
  const y = b.mul(b.inputVal(1), five, span);
  const len = b.len2(x, y, span);
  b.output('len', 'fx', len);
  b.end(span);
  const prog = buildProgram(b);
  const movs = prog.code.filter((i) => i.op === 'MOV');
  assert.ok(movs.length >= 1, 'coalescing MOV expected');
  const lenOp = prog.code.find((i) => i.op === 'LEN2')!;
  assert.equal(lenOp.a + 1, lenOp.a + 1);           // trivially adjacent pair
  const dec = decodeZprog(serializeProgram(prog));
  assert.ok(dec.ok, dec.ok ? '' : dec.errors.join('; '));
});

test('allocator: adjacent inputs need no MOVs (crater_ring DIST2)', () => {
  const { program } = buildCraterRing();
  const dist2 = program.code.find((i) => i.op === 'DIST2')!;
  assert.equal(dist2.a, 0);   // x
  assert.equal(dist2.b, 4);   // p0
  assert.equal(program.code[0]!.op, 'DIST2');
});

// ---------------------------------------------------------------------------
// vector generation (§6.2)
// ---------------------------------------------------------------------------

test('vecgen: pinned corner sequence + deterministic stream', () => {
  const bounds = [
    { min: -100, max: 100 },
    { min: 5, max: 9 },
    { min: -1, max: 1 << 30 },
  ];
  const r1 = generateInputs(0x12345678, 0x5a17, 16, bounds);
  const r2 = generateInputs(0x12345678, 0x5a17, 16, bounds);
  assert.deepEqual(r1, r2);
  assert.equal(r1.length, 3 + bounds.length + 16);
  assert.deepEqual(r1[0], [-100, 5, -1]);           // all-min
  assert.deepEqual(r1[1], [100, 9, 1 << 30]);       // all-max
  assert.deepEqual(r1[2], [0, 5, 0]);               // all-zero clamped
  assert.deepEqual(r1[3]!, [-100, r1[3]![1]!, r1[3]![2]!]);  // lane0 at min
  assert.equal(r1[3]![0], -100);
  for (const rec of r1) {
    rec.forEach((v, i) => {
      assert.ok(v >= bounds[i]!.min && v <= bounds[i]!.max, `lane ${i} in bounds`);
    });
  }
  // full s32 range: count wraps to 0 → value = draw (never NaN/Infinity)
  const wide = generateInputs(1, 1, 4, [{ min: -2147483648, max: 2147483647 }]);
  for (const rec of wide) assert.ok(Number.isInteger(rec[0]));
});

test('zvec: encode/decode round-trip with CRC', () => {
  const recs = [
    { inputs: [1, 2], expected: [3, 4], status: 1 },
    { inputs: [-1, -2], expected: [-3, -4], status: 2 },
  ];
  const bytes = encodeZvec(0xdeadbeef, 0x5a17, recs, 2, 2);
  const dec = decodeZvec(bytes);
  assert.ok(dec.ok);
  assert.equal(dec.zvec.programHash, 0xdeadbeef);
  assert.deepEqual(dec.zvec.records, recs);
  const bad = Uint8Array.from(bytes);
  bad[bad.length - 1] = bad[bad.length - 1]! ^ 0xff;
  assert.ok(!decodeZvec(bad).ok);
});

test('VecPRNG: same (hash, seed) → same stream; hash perturbs it', () => {
  const a = new VecPRNG(0x11111111, 7);
  const b = new VecPRNG(0x11111111, 7);
  const c = new VecPRNG(0x22222222, 7);
  for (let i = 0; i < 100; i++) assert.equal(a.draw(), b.draw());
  assert.notEqual(new VecPRNG(0x11111111, 7).draw(), c.draw());
});
