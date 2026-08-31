/**
 * Depth profiles (owner ruling 2026-08-31 #1, spec/qformats.md 8).
 *
 * `tests/proofs/depth_profile_law.cpp` DERIVES and PROVES these; this is the
 * same derivation in TypeScript so the constants become generated artefacts
 * beside the other frozen tables instead of hand-copied numbers.
 * reports/DEPTH_PROFILE_NEXT_STEPS.md step 1, and its own warning:
 *
 *   "Do not hand-write the scale into RTL. It is generated; a hand-copied
 *    constant is how a wrong number becomes an unadjustable one."
 *
 * The law, exactly as the proof states it:
 *
 *   s      = smallest shift with (W >> s) < 2^24        (W = w in fx16 raw)
 *   {r, k} = rcp_u24(W >> s)
 *   d      = rescale(SCALE * r, 48 + s - k), round-half-up, saturate 0xFFFFFF
 *
 * SCALE IS SOLVED FROM THE LAW'S OWN OUTPUT AT wmin, never from the ideal
 * reciprocal. `SCALE = 0xFFFFFF * Wmin` gives 0xFFFFFE -- one short of the pin
 * -- because rcp_u24 carries up to 1 LSB and the pinned input sits exactly
 * where it saturates. So:
 *
 *   {r0, k0} = rcp_u24(Wmin >> s0)
 *   SCALE    = round( 0xFFFFFF * 2^(48 + s0 - k0) / r0 )
 *
 * Everything here is BigInt. The products reach ~2^80 and Number would round
 * them silently, which is the one failure mode that would make a generated
 * constant worse than a hand-copied one.
 */

import { rcp24 } from "./fixp.js";

export const MAX_DEPTH = 0xffffff;

export interface DepthProfile {
  /** Enum name as it appears in generated sources. */
  name: string;
  /** Human-facing near/far in metres, for the comment and the spec table. */
  wminM: number;
  wmaxM: number;
  /** Near/far as fx16 (S15.16) raw units. */
  wminRaw: bigint;
  wmaxRaw: bigint;
  /** The solved per-profile constant. */
  scale: bigint;
  /** d(wmin) -- must be exactly 0xFFFFFF -- and d(wmax), the non-zero floor. */
  dAtMin: number;
  dAtMax: number;
}

/** The three profiles of ruling #1, near/far in metres. */
const SPEC: Array<{ name: string; wminM: number; wmaxM: number }> = [
  { name: "WORLD_LONG", wminM: 1.0, wmaxM: 16384.0 },
  { name: "WORLD_STANDARD", wminM: 0.5, wmaxM: 8192.0 },
  { name: "CLOSE", wminM: 0.25, wmaxM: 2048.0 },
];

/** w in metres -> fx16 raw (S15.16), round-half-up as the proof does. */
export function fx16(m: number): bigint {
  return BigInt(Math.floor(m * 65536.0 + 0.5));
}

/** The smallest shift that brings W inside a u24. */
export function normShift(W: bigint): number {
  let s = 0;
  while (W >> BigInt(s) >= 1n << 24n) s++;
  return s;
}

/** SCALE, solved so that w == wmin lands exactly on 0xFFFFFF. */
export function deriveScale(t24: number[], wminRaw: bigint): bigint {
  const s0 = normShift(wminRaw);
  const rc0 = rcp24(t24, Number(wminRaw >> BigInt(s0)));
  const sh0 = 48 + s0 - rc0.k;
  const num = BigInt(MAX_DEPTH) << BigInt(sh0);
  const r0 = BigInt(rc0.r);
  return (num + r0 / 2n) / r0; // round-to-nearest, as the proof
}

/**
 * The derived pipeline: one clamped w -> invw24.
 * Throws rather than returning a wrong number if the shift leaves range --
 * the three profiles must never do that, so it is a generator bug if it fires.
 */
export function depthOf(t24: number[], W: bigint, p: { wminRaw: bigint; wmaxRaw: bigint; scale: bigint }): number {
  let w = W;
  if (w < p.wminRaw) w = p.wminRaw;
  if (w > p.wmaxRaw) w = p.wmaxRaw;
  const s = normShift(w);
  const nw = Number(w >> BigInt(s));
  const rc = rcp24(t24, nw ? nw : 1);

  const prod = p.scale * BigInt(rc.r);
  const sh = 48 + s - rc.k;
  if (sh < 1 || sh > 126) {
    throw new Error(`depth: shift ${sh} out of range for W=${w} (profile is unusable)`);
  }
  const q = (prod + (1n << BigInt(sh - 1))) >> BigInt(sh);
  if (q < 0n) throw new Error(`depth: negative quotient for W=${w}`);
  return q > BigInt(MAX_DEPTH) ? MAX_DEPTH : Number(q);
}

/**
 * Build all three profiles and CHECK the two properties that make them
 * profiles at all: wmin pins to 0xFFFFFF exactly, and wmax has a non-zero
 * floor. A generator that emits a table failing its own law is worse than no
 * generator, so this throws rather than warns.
 */
export function buildDepthProfiles(t24: number[]): DepthProfile[] {
  return SPEC.map((sp) => {
    const wminRaw = fx16(sp.wminM);
    const wmaxRaw = fx16(sp.wmaxM);
    const scale = deriveScale(t24, wminRaw);
    const p = { wminRaw, wmaxRaw, scale };
    const dAtMin = depthOf(t24, wminRaw, p);
    const dAtMax = depthOf(t24, wmaxRaw, p);
    if (dAtMin !== MAX_DEPTH) {
      throw new Error(`${sp.name}: d(wmin) = 0x${dAtMin.toString(16)}, must be 0xFFFFFF exactly`);
    }
    if (dAtMax === 0) {
      throw new Error(`${sp.name}: d(wmax) = 0, the far floor must be non-zero`);
    }
    return { name: sp.name, wminM: sp.wminM, wmaxM: sp.wmaxM, wminRaw, wmaxRaw, scale, dAtMin, dAtMax };
  });
}
