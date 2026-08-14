// interpret.ts — the TS Field IR interpreter (the ONE TS implementation of
// op semantics; spec/form/field-ir.md §Grep-audit-law). Subordinate
// differential: golden .zvec vectors are owned by the C++ oracle
// (reference/src/zfield); this interpreter must replay them byte-identically.
//
// Every op is total (field-ir.md §3); evaluation is a pure function of
// (decoded program, input record). int64 paths go through i64.ts limbs.

import {
  U64, Ledger, mulS32, addU64, shl16S32, rescaleSat, satS32,
  acc96Add, acc96Finish, Acc96,
} from './i64.js';
import {
  fxAdd, fxSub, fxMul, fxMad, fxMin, fxMax, fxClamp, fxAbsSat,
  fieldRcp, fxSin, fxCos, noise2Hash, isqrtU64, rcp24Norm, smoothstep,
} from './numeric.js';
import { DecodedProgram, DecodedTable } from './serialize.js';

const TWO32 = 4294967296;

export interface InterpStatus { sat: boolean; rcp0: boolean }

export interface InterpResult {
  outputs: number[];      // output-record lanes (raw i32)
  status: InterpStatus;
}

/** §3.15 pinned branchless 6-step compare/select segment search. */
function segmentSearch(t: DecodedTable, a: number): number {
  const x = t.x;
  const n = x.length;
  const clamped = fxClamp(a, x[0]!, x[n - 1]!);
  let lo = 0;
  for (let k = 5; k >= 0; k--) {
    const mid = lo + (1 << k);
    if (mid <= n - 1 && x[mid]! <= clamped) lo = mid;
  }
  return lo;
}

/** §3.12 normalize lanes (zref::normalize3_approx shape, qformats.md §7.4). */
function normalizeLanes(vals: number[], L: Ledger | null): number[] {
  // n2 = Σ v² exact in a u64 pair (≤ 3·2^62 < 2^64 — no aliasing)
  let n2: U64 = { hi: 0, lo: 0 };
  for (const v of vals) n2 = addU64(n2, mulS32(v, v));
  if (n2.hi === 0 && n2.lo === 0) {
    if (L) L.rcp0 = true;
    return vals.map(() => 0);
  }
  const len = isqrtU64(n2);
  let m = len.hi * TWO32 + len.lo;              // exact, < 2^32
  let e = 0;
  while (m < (1 << 23)) { m *= 2; e -= 1; }
  while (m >= (1 << 24)) { m /= 2; m = Math.floor(m); e += 1; }
  const r = rcp24Norm(m);
  return vals.map((v) => rescaleSat(mulS32(v, r), 31 + e, L));
}

/** §3.11 len over lanes (exact u64 sum of squares → isqrt floor → sat s32). */
function lenOf(vals: number[], L: Ledger | null): number {
  let n2: U64 = { hi: 0, lo: 0 };
  for (const v of vals) n2 = addU64(n2, mulS32(v, v));
  const len = isqrtU64(n2);
  return satS32(len.hi * TWO32 + len.lo, L);
}

function satAbsRaw(a: number, L: Ledger | null): number { return fxAbsSat(a, L); }

/**
 * Interpret a decoded+validated program over one input record.
 * field-ir.md §3 is the law, op by op.
 */
export function interpret(prog: DecodedProgram, inputs: number[]): InterpResult {
  const reg = new Array<number>(64).fill(0);
  for (let i = 0; i < prog.inLanes.length && i < inputs.length; i++) {
    reg[prog.inLanes[i]!.reg] = inputs[i]! | 0;
  }
  const L: Ledger = { sat: false, rcp0: false };

  for (const ins of prog.instrs) {
    switch (ins.op) {
      case 0x00:  // END
        break;
      case 0x01: reg[ins.dst] = reg[ins.a]!; break;                     // MOV
      case 0x02: reg[ins.dst] = ins.imm | 0; break;                     // LDC
      case 0x03: reg[ins.dst] = fxAdd(reg[ins.a]!, reg[ins.b]!, L); break;
      case 0x04: reg[ins.dst] = fxSub(reg[ins.a]!, reg[ins.b]!, L); break;
      case 0x05: reg[ins.dst] = fxMul(reg[ins.a]!, reg[ins.b]!, L); break;
      case 0x06: reg[ins.dst] = fxMad(reg[ins.a]!, reg[ins.b]!, reg[ins.c]!, L); break;
      case 0x07: reg[ins.dst] = fxMin(reg[ins.a]!, reg[ins.b]!); break;
      case 0x08: reg[ins.dst] = fxMax(reg[ins.a]!, reg[ins.b]!); break;
      case 0x09: reg[ins.dst] = satAbsRaw(reg[ins.a]!, L); break;       // ABS
      case 0x0a:                                                          // CLAMP
        reg[ins.dst] = fxClamp(reg[ins.a]!, reg[ins.b]!, reg[ins.c]!);
        break;
      case 0x0b:                                                          // SELECT
        reg[ins.dst] = reg[ins.c]! !== 0 ? reg[ins.a]! : reg[ins.b]!;
        break;
      case 0x0c: {                                                        // CMP
        const a = reg[ins.a]!, b = reg[ins.b]!;
        let t = false;
        switch (ins.imm) {
          case 0: t = a === b; break;
          case 1: t = a !== b; break;
          case 2: t = a < b; break;
          case 3: t = a <= b; break;
          case 4: t = a > b; break;
          case 5: t = a >= b; break;
        }
        reg[ins.dst] = t ? 0x10000 : 0;
        break;
      }
      case 0x10: {                                                        // DOT2
        const acc: Acc96 = { a2: 0, a1: 0, a0: 0 };
        acc96Add(acc, mulS32(reg[ins.a]!, reg[ins.b]!));
        acc96Add(acc, mulS32(reg[ins.a + 1]!, reg[ins.b + 1]!));
        reg[ins.dst] = acc96Finish(acc, L);
        break;
      }
      case 0x11: {                                                        // DOT3
        const acc: Acc96 = { a2: 0, a1: 0, a0: 0 };
        for (let k = 0; k < 3; k++) {
          acc96Add(acc, mulS32(reg[ins.a + k]!, reg[ins.b + k]!));
        }
        reg[ins.dst] = acc96Finish(acc, L);
        break;
      }
      case 0x12: reg[ins.dst] = lenOf([reg[ins.a]!, reg[ins.a + 1]!], L); break;
      case 0x13: reg[ins.dst] = lenOf([reg[ins.a]!, reg[ins.a + 1]!, reg[ins.a + 2]!], L); break;
      case 0x14:                                                          // DIST2
        reg[ins.dst] = lenOf([
          fxSub(reg[ins.a]!, reg[ins.b]!, L),
          fxSub(reg[ins.a + 1]!, reg[ins.b + 1]!, L),
        ], L);
        break;
      case 0x15: {                                                        // NORMALIZE2
        const out = normalizeLanes([reg[ins.a]!, reg[ins.a + 1]!], L);
        reg[ins.dst] = out[0]!;
        reg[ins.dst + 1] = out[1]!;
        break;
      }
      case 0x16: {                                                        // NORMALIZE3
        const out = normalizeLanes([reg[ins.a]!, reg[ins.a + 1]!, reg[ins.a + 2]!], L);
        reg[ins.dst] = out[0]!;
        reg[ins.dst + 1] = out[1]!;
        reg[ins.dst + 2] = out[2]!;
        break;
      }
      case 0x17: reg[ins.dst] = fieldRcp(reg[ins.a]!, L); break;        // RCP
      case 0x18: reg[ins.dst] = fxSin(reg[ins.a]! & 0xffff); break;     // SIN
      case 0x19: reg[ins.dst] = fxCos(reg[ins.a]! & 0xffff); break;     // COS
      case 0x1a: {                                                        // CURVE
        const t = prog.tables[ins.imm]!;
        const i = segmentSearch(t, reg[ins.a]!);
        const a = fxClamp(reg[ins.a]!, t.x[0]!, t.x[t.x.length - 1]!);
        reg[ins.dst] = fxMad(fxSub(a, t.x[i]!, L), t.dy[i]!, t.y[i]!, L);
        break;
      }
      case 0x1b: {                                                        // SPLINE
        const t = prog.tables[ins.imm]!;
        const n = t.x.length;
        const i = segmentSearch(t, reg[ins.a]!);
        const a = fxClamp(reg[ins.a]!, t.x[0]!, t.x[n - 1]!);
        // t = clamp(rescale((a − x_i)·dy_i, 16), 0, 1) — dy = Q16.16 of 1/Δx
        const tt = fxClamp(rescaleSat(mulS32(fxSub(a, t.x[i]!, L), t.dy[i]!), 16, L), 0, 1 << 16);
        const y = t.y;
        const p0 = y[Math.max(0, i - 1)]!;
        const p1 = y[i]!;
        const p2 = y[i + 1] ?? y[n - 1]!;
        const p3 = y[Math.min(n - 1, i + 2)]!;
        // doubled CR coefficients, exact s64 then ONE saturate each (§3.15)
        const C1 = satS32(p2 - p0, L);
        const C2 = satS32(2 * p0 - 5 * p1 + 4 * p2 - p3, L);
        const C3 = satS32(-p0 + 3 * p1 - 3 * p2 + p3, L);
        let u = fxMad(tt, C3, C2, L);                      // Horner
        u = fxMad(tt, u, C1, L);
        const v = fxMul(tt, u, L);
        reg[ins.dst] = fxAdd(p1, rescaleSat(shl16S32(v), 1, L), L);
        break;
      }
      case 0x1c: {                                                        // NOISE2
        const ix = (reg[ins.a]! | 0) >> 16;
        const iy = (reg[ins.a + 1]! | 0) >> 16;
        reg[ins.dst] = noise2Hash(ix, iy, ins.imm, 0) >>> 16;
        reg[ins.dst + 1] = noise2Hash(ix, iy, ins.imm, 1) >>> 16;
        break;
      }
      case 0x1d: {                                                        // DCURVE
        const t = prog.tables[ins.imm]!;
        const i = segmentSearch(t, reg[ins.a]!);
        reg[ins.dst] = t.dy[i]!;
        break;
      }
      case 0x21: {                                                        // RING
        const d = reg[ins.a]!, r0 = reg[ins.b]!, r1 = reg[ins.c]!;
        // m = rescale_s32((s64)r0 + r1, 1) — the RAW sum, no <<16 (§3.17)
        const m = rescaleSat(
          { hi: r0 + r1 < 0 ? 0xffffffff : 0, lo: (r0 + r1) >>> 0 }, 1, L);
        const s0 = smoothstep(r0, m, d, L);
        const s1 = smoothstep(m, r1, d, L);
        reg[ins.dst] = fxMul(s0, fxSub(1 << 16, s1, L), L);
        break;
      }
      case 0x22: {                                                        // RIDGE
        const u = noise2Hash((reg[ins.a]! | 0) >> 16, (reg[ins.b]! | 0) >> 16,
                             ins.imm, 0) >>> 16;
        reg[ins.dst] = fxSub(1 << 16, satAbsRaw(fxSub(fxAdd(u, u, L), 1 << 16, L), L), L);
        break;
      }
      case 0x28: {                                                        // ROT2
        const ang = reg[ins.b]! & 0xffff;
        const c = fxCos(ang), s = fxSin(ang);
        const x = reg[ins.a]!, y = reg[ins.a + 1]!;
        reg[ins.dst] = fxSub(fxMul(c, x, L), fxMul(s, y, L), L);
        reg[ins.dst + 1] = fxAdd(fxMul(s, x, L), fxMul(c, y, L), L);
        break;
      }
      case 0x29: {                                                        // ROT3
        const ang = reg[ins.b]! & 0xffff;
        const c = fxCos(ang), s = fxSin(ang);
        const x = reg[ins.a]!, y = reg[ins.a + 1]!, z = reg[ins.a + 2]!;
        if (ins.imm === 0) {              // X axis
          reg[ins.dst + 1] = fxSub(fxMul(c, y, L), fxMul(s, z, L), L);
          reg[ins.dst + 2] = fxAdd(fxMul(s, y, L), fxMul(c, z, L), L);
          reg[ins.dst] = x;
        } else if (ins.imm === 1) {       // Y axis
          reg[ins.dst + 2] = fxSub(fxMul(c, z, L), fxMul(s, x, L), L);
          reg[ins.dst] = fxAdd(fxMul(s, z, L), fxMul(c, x, L), L);
          reg[ins.dst + 1] = y;
        } else {                          // Z axis
          reg[ins.dst] = fxSub(fxMul(c, x, L), fxMul(s, y, L), L);
          reg[ins.dst + 1] = fxAdd(fxMul(s, x, L), fxMul(c, y, L), L);
          reg[ins.dst + 2] = z;
        }
        break;
      }
      default:
        // unreachable on a validated program (V9); the interpreter is only
        // reachable after decode+validate success (§4)
        throw new Error(`interpret: unvalidated opcode 0x${ins.op.toString(16)}`);
    }
    if (ins.op === 0x00) break;
  }

  return {
    outputs: prog.outLanes.map((o) => reg[o.reg]! | 0),
    status: { sat: L.sat, rcp0: L.rcp0 },
  };
}

/** Status word packing (§6.1): bit0 sat, bit1 rcp0. */
export function statusWord(s: InterpStatus): number {
  return (s.sat ? 1 : 0) | (s.rcp0 ? 2 : 0);
}
