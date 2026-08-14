// i64.ts — the ONE shared 16-bit-limb int64 arithmetic util for the TS
// interpreter (spec/form/field-ir.md §10, plan risk R2). No BigInt here —
// this is interpreter runtime path; BigInt appears only in tests as the
// independent oracle.
//
// Representation: a 64-bit two's-complement value as {hi, lo}, both unsigned
// 32-bit halves (hi = bits 32..63). All operations are exact. Two's-complement
// wrapping arithmetic is exact whenever the true mathematical result lies in
// (-2^64, 2^64) — the invariant every caller here respects (field-ir.md §3.10
// notes the one exception, DOT2/3, which uses the 96-bit accumulator below).
//
// Spec: field-ir.md §3 (single-rounding law), qformats.md §4 (rescale).

export interface U64 { hi: number; lo: number }  // hi, lo in [0, 2^32)

const TWO32 = 4294967296;
export const I32_MIN = -2147483648;
export const I32_MAX = 2147483647;

/**
 * Exact product of two 32-bit lanes via 16-bit limb schoolbook (a = a1:a0,
 * b = b1:b0; product = p00 + mid·2^16 + p11·2^32 with p00,p01,p10 < 2^32),
 * kept to the low 64 bits.
 *
 * Signedness: with a = aU − 2^32·[a<0] and b likewise, the signed product is
 *   a·b ≡ aU·bU − 2^32·([a<0]·bU + [b<0]·aU)   (mod 2^64)
 * so the two sign corrections below on the HIGH half only. Non-negative
 * Number inputs of any magnitude get pure unsigned semantics (isqrt/normalize
 * rely on this) — the correction keys off the Number's own sign.
 */
export function mulS32(a: number, b: number): U64 {
  const aU = a >>> 0;
  const bU = b >>> 0;
  const a0 = aU & 0xffff;
  const a1 = aU >>> 16;
  const b0 = bU & 0xffff;
  const b1 = bU >>> 16;
  const p00 = a0 * b0;              // < 2^32
  const mid = a0 * b1 + a1 * b0;    // < 2^33 — CANNOT touch with bitwise ops
  const p11 = a1 * b1;              // < 2^30
  const l0 = p00 & 0xffff;
  const l1 = (p00 >>> 16) + (mid % 0x10000);  // < 2^17
  const lo = (l0 | ((l1 & 0xffff) << 16)) >>> 0;
  let hi = (p11 + Math.floor(mid / 0x10000) + (l1 >>> 16)) >>> 0;
  if (a < 0) hi = (hi - bU) >>> 0;
  if (b < 0) hi = (hi - aU) >>> 0;
  return { hi, lo };
}

/** Wrapping (mod 2^64) add — exact when the true sum is in (-2^64, 2^64). */
export function addU64(p: U64, q: U64): U64 {
  const loSum = p.lo + q.lo;                       // < 2^33
  const carry = loSum >= TWO32 ? 1 : 0;
  return { hi: (p.hi + q.hi + carry) % TWO32, lo: loSum % TWO32 };
}

/** The s64 value `c << 16` as a U64 pair (sign-extended). */
export function shl16S32(c: number): U64 {
  return { hi: (c >> 16) >>> 0, lo: (c << 16) >>> 0 };
}

/** Shift a U64 right by k (0..32), unsigned, no rounding. */
export function shrU64(p: U64, k: number): number {
  if (k === 0) return p.lo;
  if (k >= 32) return p.hi >>> (k - 32);
  return ((p.hi & ((1 << k) - 1)) * TWO32 + p.lo) / Math.pow(2, k);
}

/** True value of the pair as an exact Number (only valid when |v| < 2^53). */
export function toNumber(p: U64): number {
  return (p.hi >= 0x80000000 ? p.hi - TWO32 : p.hi) * TWO32 + p.lo;
}

export interface Ledger { sat: boolean; rcp0: boolean }  // sticky status feed

/**
 * rescale_s32(p, k): round-half-up arithmetic shift by k (1..47), then
 * saturate to s32 (qformats.md §4). p must hold the TRUE value (no aliasing).
 */
export function rescaleSat(p: U64, k: number, L: Ledger | null): number {
  if (k === 0) {
    // plain saturation
    const hi = p.hi | 0;
    if (hi === 0 && p.lo <= 0x7fffffff) return p.lo | 0;
    if (hi === -1 && p.lo >= 0x80000000) return p.lo | 0;
    if (L) L.sat = true;
    return hi < 0 ? I32_MIN : I32_MAX;
  }
  // rounding add 2^(k-1) (fits exactly in Number for k <= 47)
  const rnd = Math.pow(2, k - 1);
  const loAdd = rnd % TWO32;
  const hiAdd = (rnd - loAdd) / TWO32;
  const loSum = p.lo + loAdd;
  const carry = loSum >= TWO32 ? 1 : 0;
  const lo = loSum % TWO32;
  const hi = ((p.hi + hiAdd + carry) % TWO32) >>> 0;
  if (k >= 32) {
    // result = (signed hi) >> (k-32) — always fits s32 (|v| < 2^64)
    return k === 32 ? (hi | 0) : ((hi | 0) >> (k - 32)) | 0;
  }
  const hiS = hi | 0;
  const wHigh = hiS >> k;                       // bits k+32..63 of v, sign-ext
  // low 32 bits of (v >> k): hi's low k bits land above lo's top bits (exact)
  const wLow = ((hi & ((1 << k) - 1)) * Math.pow(2, 32 - k) + (lo >>> k)) >>> 0;
  if (wHigh === 0 && wLow <= 0x7fffffff) return wLow | 0;
  if (wHigh === -1 && wLow >= 0x80000000) return wLow | 0;
  if (L) L.sat = true;
  return wHigh < 0 ? I32_MIN : I32_MAX;
}

/** Signed saturation of a Number already known to be an exact integer. */
export function satS32(v: number, L: Ledger | null): number {
  if (v > I32_MAX) { if (L) L.sat = true; return I32_MAX; }
  if (v < I32_MIN) { if (L) L.sat = true; return I32_MIN; }
  return v | 0;
}

// ---------------------------------------------------------------------------
// 96-bit accumulator for DOT2/DOT3 (field-ir.md §3.10)
// ---------------------------------------------------------------------------

export interface Acc96 { a2: number; a1: number; a0: number }  // unsigned halves

/** Add a signed U64 pair into the 96-bit accumulator (exact for |sum| < 2^95). */
export function acc96Add(acc: Acc96, p: U64): void {
  const l = acc.a0 + p.lo;
  const c0 = l >= TWO32 ? 1 : 0;
  acc.a0 = l % TWO32;
  const m = acc.a1 + p.hi + c0;
  const c1 = m >= TWO32 ? 1 : 0;
  acc.a1 = m % TWO32;
  // sign-extension of p.hi into bits 64..95, plus the end-around concept —
  // a2 is kept as a SIGNED small integer (-4..3)
  const signExt = (p.hi | 0) < 0 ? -1 : 0;
  acc.a2 = (acc.a2 + signExt + c1) | 0;
}

/**
 * Finish a DOT2/3 accumulation: t = round_half_up(v / 2^16), saturate s32.
 * When |v| < 2^53 the value is reconstructed exactly in Number; anything
 * larger necessarily saturates (|t| would exceed 2^37 >> 2^31), so the sign
 * of the top half decides. This is exactly what the C++ __int128 path
 * computes (rescale_s32 on the exact s128 sum).
 */
export function acc96Finish(acc: Acc96, L: Ledger | null): number {
  const { a2, a1, a0 } = acc;
  let v: number | null = null;
  if (a2 === 0 && a1 <= 0x001fffff) {
    v = a1 * TWO32 + a0;                       // exact, < 2^53
  } else if (a2 === -1 && a1 >= 0xffe00000) {
    v = -TWO32 * TWO32 + a1 * TWO32 + a0;      // exact, > -2^53
  } else if (a2 > 0 || (a2 === 0 && a1 > 0x001fffff)) {
    if (L) L.sat = true;
    return I32_MAX;
  } else {
    if (L) L.sat = true;
    return I32_MIN;
  }
  return satS32(Math.floor((v + 0x8000) / 65536), L);
}

/** Unsigned compare p < q (halves). */
export function cmpU64(p: U64, q: U64): number {
  if (p.hi !== q.hi) return p.hi < q.hi ? -1 : 1;
  if (p.lo !== q.lo) return p.lo < q.lo ? -1 : 1;
  return 0;
}
