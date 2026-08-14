/**
 * fixgen numeric core — the single source of truth for every frozen constant
 * and table in spec/qformats.md v1 (QFMT_VERSION 1).
 *
 * Spec citations: qformats.md sections 4 (rescale), 6 (reciprocals),
 * 7.1 (sin/cos), 7.5 (noise2 hash), and 12 (golden records).
 *
 * Integer discipline: everything below is exact-integer math. Doubles appear
 * ONLY inside buildSinTable() to evaluate sin(pi/2 * i / 256) (the frozen
 * table formula, qformats.md 7.1); every downstream value — including all
 * golden vectors — is pure integer/BigInt arithmetic. BigInt implements the
 * u64/s64 intermediates of the frozen rcp algorithms so the TS generator and
 * the C++ reference (zref_rcp.hpp) are independent implementations of the
 * same law (their agreement is the differential in tests/unit/test_fixp.cpp).
 */

export const QFMT_VERSION = 1;

/** round_half_up(n / d) for non-negative BigInt n, d > 0 (qformats.md 4). */
export function rhuDiv(n: bigint, d: bigint): bigint {
  return (n + d / 2n) / d;
}

/** rescale_u(x, k) — the one rounding primitive, unsigned (qformats.md 4). */
export function rescaleU(x: bigint, k: number): bigint {
  if (k === 0) return x;
  return (x + (1n << BigInt(k - 1))) >> BigInt(k);
}

// ---------------------------------------------------------------------------
// 7.1 sin/cos: 257-entry Q1.16 quarter-wave table + all-integer interpolation
// ---------------------------------------------------------------------------

/** T[i] = round_half_up(sin(pi/2 * i / 256) * 2^16), i = 0..256 (frozen). */
export function buildSinTable(): number[] {
  const t: number[] = [];
  for (let i = 0; i <= 256; i++) {
    // Math.round = round-half-up (ties toward +infinity); values in [0, 65536].
    t.push(Math.round(Math.sin((Math.PI / 2) * (i / 256)) * 65536));
  }
  return t;
}

function quarterWave(sinTab: number[], a13: number): number {
  const i = a13 >> 6;
  const t = a13 & 0x3f;
  const d = sinTab[i + 1] - sinTab[i];
  return sinTab[i] + ((d * t + 32) >> 6); // rescale(d * t, 6), all non-negative
}

/** fx_sin(a): angle16 (u16 turns) -> fx16 raw (s32 stored in JS number). */
export function fxSin(sinTab: number[], a: number): number {
  const q = a >>> 14;
  const a13 = a & 0x3fff;
  const s = q & 1 ? quarterWave(sinTab, 0x4000 - a13) : quarterWave(sinTab, a13);
  return (q & 2 ? -s : s) | 0; // | 0 canonicalizes -0 to 0 (C++ ints have no -0)
}

/** fx_cos(a) = fx_sin(a + 0x4000), exact by construction (qformats.md 7.1). */
export function fxCos(sinTab: number[], a: number): number {
  return fxSin(sinTab, (a + 0x4000) & 0xffff);
}

// ---------------------------------------------------------------------------
// 6.1 rcp_u24: 256-entry Q0.31 initial-guess table + two frozen NR steps
// ---------------------------------------------------------------------------

/** T24[idx] = round_half_up(2^54 / (2^23 + idx*2^15 + 2^14)) (frozen). */
export function buildRcp24Table(): number[] {
  const t: number[] = [];
  for (let idx = 0; idx < 256; idx++) {
    const mid = (1 << 23) + idx * (1 << 15) + (1 << 14);
    t.push(Number(rhuDiv(1n << 54n, BigInt(mid))));
  }
  return t;
}

/**
 * rcp_u24_norm(m): m in [2^23, 2^24) normalized; returns r ~= 2^47/m, u24.
 * Frozen two-step Newton-Raphson (qformats.md 6.1); |r - 2^47/m| <= 1.
 */
export function rcp24Norm(t24: number[], m: number): number {
  const idx = (m - (1 << 23)) >>> 15;
  let x = BigInt(t24[idx]);
  const M = BigInt(m);
  for (let step = 0; step < 2; step++) {
    const w = (M * x) >> 24n;
    x = rescaleU(x * ((2n << 30n) - w), 30);
  }
  const r = Number(rescaleU(x, 7));
  return r > 0xffffff ? 0xffffff : r;
}

/** rcp_u24 wrapper: any nonzero d; returns {r, k} with 1/(d/2^24) ~= (r/2^24)*2^k. */
export function rcp24(t24: number[], d: number): { r: number; k: number } {
  let m = d >>> 0;
  let e = 0;
  while ((m & (1 << 23)) === 0) {
    m = (m << 1) >>> 0;
    e++;
  }
  return { r: rcp24Norm(t24, m), k: e + 1 };
}

// ---------------------------------------------------------------------------
// 6.2 field_rcp: 256-entry Q0.17 initial-guess table + one pinned correction
// ---------------------------------------------------------------------------

/** TF[idx] = round_half_up(2^47 / (2^31 + idx*2^23 + 2^22)) (frozen). */
export function buildFieldRcpTable(): number[] {
  const t: number[] = [];
  for (let idx = 0; idx < 256; idx++) {
    const mid = 2n ** 31n + BigInt(idx) * 2n ** 23n + 2n ** 22n;
    t.push(Number(rhuDiv(1n << 47n, mid)));
  }
  return t;
}

/**
 * field_rcp magnitude on n = |a| in [1, 2^31]: returns round(2^32/n) approx
 * (may exceed s32 range for n <= 1; caller saturates). Frozen one-step
 * pinned linear correction (qformats.md 6.2).
 */
export function fieldRcpMag(tf: number[], n: number): bigint {
  let m = n >>> 0;
  let e = 31;
  while ((m & (1 << 31)) === 0) {
    m = (m << 1) >>> 0;
    e--;
  }
  const idx = (m - (1 << 31)) >>> 23;
  let x = BigInt(tf[idx]);
  const M = BigInt(m);
  const p = M * x;
  x = rescaleU(x * ((1n << 48n) - p), 47);
  return rescaleU(x << 16n, e);
}

/** field_rcp(a) for fx16 raw a: saturated s32 result + rcp0 flag (qformats.md 6.2). */
export function fieldRcp(tf: number[], a: number): { r: number; rcp0: boolean } {
  if (a === 0) return { r: 0x7fffffff, rcp0: true };
  const neg = a < 0;
  const n = neg ? -a : a;
  const mag = fieldRcpMag(tf, n);
  // saturate to s32
  let r: number;
  if (mag > 2147483647n) r = 2147483647;
  else r = Number(mag);
  return { r: neg ? -r : r, rcp0: false };
}

// ---------------------------------------------------------------------------
// 3 unit8 / 7.5 noise2
// ---------------------------------------------------------------------------

/** unit_mul(a,b) = ((a*b + 128) >> 8), clamp 255 (qformats.md 3). */
export function unitMul(a: number, b: number): number {
  const r = ((a * b + 128) >>> 0) >> 8;
  return r > 255 ? 255 : r;
}

/**
 * noise2_hash(x, y, seed, lane) — PCG RXS-M-XS lattice hash, constants frozen
 * verbatim (qformats.md 7.5 / ratified decision A1). All ops u32 wrapping.
 */
export function noise2Hash(x: number, y: number, seed: number, lane: number): number {
  let s = (Math.imul(x, 0x9e3779b1) ^ (Math.imul(y, 0x85ebca77) ^ seed)) >>> 0;
  s = (s + Math.imul(lane, 0xe1)) >>> 0;
  s = (Math.imul(s, 747796405) + 2891336453) >>> 0;
  const w = Math.imul(((s >>> ((s >>> 28) + 4)) ^ s) >>> 0, 277803737) >>> 0;
  return ((w >>> 22) ^ w) >>> 0;
}

// ---------------------------------------------------------------------------
// 12 golden record generators (pure integer; LE byte dumps built by golden.ts)
// ---------------------------------------------------------------------------

/** d for record i of rcp24_sample.bin: (i*16 + (i & 15)) | 1 (qformats.md 12). */
export function rcp24SampleInput(i: number): number {
  return ((i * 16 + (i & 15)) | 1) >>> 0;
}

/** noise2 KAT inputs for record i (qformats.md 12). */
export function noise2KatInputs(i: number): { x: number; y: number; seed: number } {
  const x = (i * 2654435761) >>> 0; // floor(i * golden-ratio fraction), wraps u32
  const y = (i * 40503) >>> 0;
  const seed = ((Math.imul(i, 0x9e3779b1) >>> 0) + 1) >>> 0;
  return { x, y, seed };
}
