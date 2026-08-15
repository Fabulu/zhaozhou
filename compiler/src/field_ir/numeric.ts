// numeric.ts — the TS interpreter's numeric layer: a mirror of the frozen
// zref law (spec/qformats.md), subordinate to the C++ oracle per
// spec/form/field-ir.md §10. Every function cites its qformats section and
// matches the C++ zref implementation bit-for-bit (proven by the golden
// .zvec replay differential, compiler/tests/field_ts_differential.test.ts).
//
// int64 products/sums go through i64.ts (16-bit limbs) — no BigInt here.

import { SIN_Q16, FIELD_RCP_T0, RCP24_T0 } from '../generated/tables.js';
import {
  U64, Ledger, mulS32, addU64, shl16S32, rescaleSat, satS32, cmpU64,
  I32_MAX, I32_MIN,
} from './i64.js';

const TWO32 = 4294967296;

// ---- §3 fx16 core ----------------------------------------------------------

export function fxAdd(a: number, b: number, L: Ledger | null): number {
  return satS32(a + b, L);                      // s64 sum, saturate
}

export function fxSub(a: number, b: number, L: Ledger | null): number {
  return satS32(a - b, L);
}

/** §3 single rounding: exact s64 product, ONE rescale(·,16). */
export function fxMul(a: number, b: number, L: Ledger | null): number {
  return rescaleSat(mulS32(a, b), 16, L);
}

/** §3/A3b: exact s64 a·b + c<<16, ONE rescale(·,16). */
export function fxMad(a: number, b: number, c: number, L: Ledger | null): number {
  return rescaleSat(addU64(mulS32(a, b), shl16S32(c)), 16, L);
}

export function fxMin(a: number, b: number): number { return a < b ? a : b; }
export function fxMax(a: number, b: number): number { return a > b ? a : b; }
export function fxClamp(x: number, lo: number, hi: number): number {
  return fxMax(lo, fxMin(hi, x));
}

/** §3.7 saturating abs: abs(0x80000000) = 0x7FFFFFFF + SAT. */
export function fxAbsSat(a: number, L: Ledger | null): number {
  if (a === I32_MIN) { if (L) L.sat = true; return I32_MAX; }
  return a < 0 ? -a : a;
}

// ---- §7.5 noise2 hash (PCG RXS-M-XS, constants frozen verbatim) -----------

export function noise2Hash(x: number, y: number, seed: number, lane: number): number {
  let s = (Math.imul(x, 0x9e3779b1) ^ (Math.imul(y, 0x85ebca77) ^ seed)) >>> 0;
  s = (s + Math.imul(lane, 0xe1)) >>> 0;
  s = (Math.imul(s, 747796405) + 2891336453) >>> 0;
  // RXS-M-XS: u32 logical shifts (JS >> would sign-extend past 2^31)
  const w = Math.imul(((s >>> ((s >>> 28) + 4)) ^ s) >>> 0, 277803737);
  return ((w >>> 22) ^ w) >>> 0;
}

// ---- §7.1 sin/cos (257-entry quarter-wave, all-integer interpolation) -----

function sinQuarter(a13: number): number {
  const i = a13 >>> 6;
  const t = a13 & 0x3f;
  const ti = SIN_Q16[i]!;
  // a13 == 0x4000 lands on i == 256, the last of the 257 entries; the slope
  // would read SIN_Q16[257] (undefined -> NaN -> 0 here, a hard compile error
  // in the C++ constexpr twin). Reached by every fxCos(0)/fxCos(0x8000) and
  // by fxSin(0x4000)/fxSin(0xC000). t is 0 there, so the value is exactly
  // T[256] — mirrors the guard in reference/include/zref/zref_trig.hpp.
  if (i === 256) return ti;
  const d = SIN_Q16[i + 1]! - ti;
  return ti + ((d * t + 32) >> 6);
}

export function fxSin(a: number): number {      // a = u16 turns
  const q = a >>> 14;
  const a13 = a & 0x3fff;
  const s = (q & 1) !== 0 ? sinQuarter(0x4000 - a13) : sinQuarter(a13);
  return ((q & 2) !== 0 ? -s : s) | 0;          // | 0 kills JS -0 (C++ has none)
}

export function fxCos(a: number): number {
  return fxSin((a + 0x4000) & 0xffff);
}

// ---- §6.2 field_rcp (256-entry table + ONE pinned linear correction) ------

/**
 * Exact low-64 product of two non-negative Numbers < 2^53 (limb schoolbook,
 * 16-bit limbs, 3+3). Mirrors C++ uint64 wrap-around for results ≥ 2^64
 * (never reached by the frozen algorithms; kept for exact parity).
 */
function mulWideLow64(a: number, b: number): U64 {
  // 4 limbs each (operands < 2^53 → top limb 0..7); the j-loop reads all 4,
  // so the arrays must be — a missing limb would poison the sum with NaN.
  const limb4 = (v: number): number[] => [
    v & 0xffff, Math.floor(v / 0x10000) & 0xffff,
    Math.floor(v / 0x100000000) & 0xffff, Math.floor(v / 0x1000000000000) & 0xffff,
  ];
  const al = limb4(a);
  const bl = limb4(b);
  const r = [0, 0, 0, 0];                       // 4 limbs = low 64 bits
  for (let i = 0; i < 4; i++) {
    let carry = 0;
    for (let j = 0; j + i < 4; j++) {
      const t = r[i + j]! + al[i]! * bl[j]! + carry;
      r[i + j] = t & 0xffff;
      carry = Math.floor(t / 0x10000);
    }
  }
  return { hi: r[2]! | (r[3]! << 16), lo: r[0]! | (r[1]! << 16) };
}

/**
 * Unsigned round-half-up rescale of a U64 pair by k (0..47). Result ≤ 2^33
 * for every frozen caller. Precondition: (p.hi + hiAdd)·2^(32−k) ≤ 2^53
 * (true for all call sites: k ≥ 11 full-range, smaller k only with tiny hi).
 */
function rescaleU(p: U64, k: number): number {
  if (k === 0) return p.hi * TWO32 + p.lo;
  const rnd = Math.pow(2, k - 1);
  const loAdd = rnd % TWO32;
  const hiAdd = (rnd - loAdd) / TWO32;
  const loSum = p.lo + loAdd;                   // < 2^33, exact
  const carry = loSum >= TWO32 ? 1 : 0;
  const lo = loSum % TWO32;
  const hi = p.hi + hiAdd + carry;              // ≤ 2^33, exact
  if (k >= 32) {
    const a = Math.floor(hi / Math.pow(2, k - 32));
    const rem = hi - a * Math.pow(2, k - 32);   // < 2^(k-32), exact
    return a + (rem * TWO32 + lo >= Math.pow(2, k) ? 1 : 0);
  }
  // k < 32: (hi)·2^(32−k) exact per precondition; fractional part from lo
  return hi * Math.pow(2, 32 - k) + Math.floor(lo / Math.pow(2, k));
}

export function fieldRcp(a: number, L: Ledger | null): number {
  if (a === 0) { if (L) L.rcp0 = true; return I32_MAX; }   // pinned
  const neg = a < 0;
  let n = neg ? -a : a;                        // |a| <= 2^31, exact
  let e = 31;
  while ((n & 0x80000000) === 0) {             // normalize to bit 31
    n = n * 2;                                 // < 2^31 before shift, exact
    e -= 1;
  }
  const idx = Math.floor((n - 0x80000000) / 0x800000);   // (n − 2^31) >> 23
  let x = FIELD_RCP_T0[idx]!;                  // ~2^47/m in (2^15, 2^16]
  // one pinned linear correction: x = rescale_u(x·(2^48 − m·x), 47) (§6.2)
  const p = n * x;                             // ≤ 2^48, exact in Number
  x = rescaleU(mulWideLow64(x, Math.pow(2, 48) - p), 47);
  // r = rescale_u(x << 16, e)
  const r = rescaleU(shl16S32(x), e);
  if (r > I32_MAX) { if (L) L.sat = true; return neg ? I32_MIN : I32_MAX; }
  return neg ? -r : r;
}

// ---- §6.1 rcp_u24_norm (normalize path) ------------------------------------

export function rcp24Norm(m: number): number {  // m in [2^23, 2^24)
  const idx = (m - (1 << 23)) >> 15;
  let x = RCP24_T0[idx]!;
  for (let step = 0; step < 2; step++) {
    const p = mulS32(m, x);                     // m·x <= 2^55 — via limbs
    const w = p.hi * 256 + (p.lo >>> 24);       // floor(p / 2^24), exact
    // x = rescale_u(x·(2^31 − w), 30)
    x = rescaleU(mulS32(x, (2 * (1 << 30) - w) >>> 0), 30);
  }
  let r = rescaleU({ hi: 0, lo: x }, 7);
  return r > 0xffffff ? 0xffffff : r;
}

// ---- §7.2 exact floor sqrt over u64 (binary search; limb-square compare) --

export function isqrtU64(n: U64): U64 {
  let lo = 0;
  let hi = 0x100000000;                         // len < 2^32
  while (hi - lo > 1) {
    const mid = Math.floor((lo + hi) / 2);
    const sq = mulS32(mid, mid);
    if (cmpU64(sq, n) <= 0) lo = mid; else hi = mid;
  }
  return { hi: Math.floor(lo / TWO32), lo: lo % TWO32 };
}

// ---- §7.3 smoothstep (t²(3−2t), single-rounding chain) --------------------

export function smoothstep(e0: number, e1: number, x: number, L: Ledger | null): number {
  const d = fxSub(e1, e0, L);
  const r = fieldRcp(d, L);
  let t = fxMul(fxSub(x, e0, L), r, L);
  t = fxClamp(t, 0, 1 << 16);
  const t2 = fxMul(t, t, L);
  return fxMul(t2, fxSub(3 << 16, fxMul(2 << 16, t, L), L), L);
}
