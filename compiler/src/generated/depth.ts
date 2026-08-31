// GENERATED FILE - tools/fixgen (spec/qformats.md 8) - DO NOT EDIT.
// QFMT_VERSION 2; regenerate with `npm run tables:gen` and commit.

export interface DepthProfile {
  readonly name: string;
  readonly wminRaw: bigint;
  readonly wmaxRaw: bigint;
  readonly scale: bigint;
  readonly dAtWmin: number;
  readonly dAtWmax: number;
}

/**
 * qformats.md 8: view-depth profiles (owner ruling 2026-08-31 #1).
 *   s      = smallest shift with (W >> s) < 2^24     (W = w in fx16 raw)
 *   {r, k} = rcp_u24(W >> s)
 *   d      = rescale(SCALE * r, 48 + s - k), round-half-up, sat 0xFFFFFF
 * SCALE is SOLVED from the law's own output at wmin, not from the ideal
 * reciprocal -- 0xFFFFFF * wmin gives 0xFFFFFE, one short of the pin.
 * wmax is a depth CLAMP, not a far-clip plane.
 */
export const DEPTH_PROFILES: readonly DepthProfile[] = [
    // WORLD_LONG: 1 m .. 16384 m
    { name: "WORLD_LONG", wminRaw: 65536n, wmaxRaw: 1073741824n, scale: 1099511627776n, dAtWmin: 0xFFFFFF, dAtWmax: 1024 },
    // WORLD_STANDARD: 0.5 m .. 8192 m
    { name: "WORLD_STANDARD", wminRaw: 32768n, wmaxRaw: 536870912n, scale: 549755813888n, dAtWmin: 0xFFFFFF, dAtWmax: 1024 },
    // CLOSE: 0.25 m .. 2048 m
    { name: "CLOSE", wminRaw: 16384n, wmaxRaw: 134217728n, scale: 274877906944n, dAtWmin: 0xFFFFFF, dAtWmax: 2048 },
];
