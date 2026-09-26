// zref_depth.hpp — the view-depth oracle (spec/qformats.md §8, owner ruling
// 2026-08-31 #1).
//
// DEPTH_PROFILE_NEXT_STEPS.md step 4: RTL must be DIFFERENTIALLY TESTED against
// a reference, not checked against a restatement of the law in the test itself.
// A test that re-derives the formula agrees with its own bug; a test that calls
// this agrees with the thing the compiler and the generator also call.
//
// The constants are NOT here. They live in generated/zref_depth.hpp, emitted by
// tools/fixgen, because DEPTH_PROFILE_NEXT_STEPS.md says plainly:
//
//   "Do not hand-write the scale into RTL. It is generated; a hand-copied
//    constant is how a wrong number becomes an unadjustable one."
//
// This header is the EVALUATION and nothing else.
//
// The law, exactly:
//
//   w      clamped to [wmin, wmax]                  (fx16, S15.16 raw units)
//   s      = smallest shift with (W >> s) < 2^24
//   {r, k} = rcp_u24(W >> s)                        (§6.1, the frozen table)
//   d      = rescale(scale * r, 48 + s - k), round-half-up, saturate 0xFFFFFF
//
// Depth IS the rcp_u24 pipeline output. That is what makes ZRef == RTL
// bit-identity a provable property rather than an aspiration.
#pragma once

#include <cstdint>

#include "zref/generated/zref_depth.hpp"
#include "zref/zref_rcp.hpp"

namespace zref {

using depth_u128 = unsigned __int128;

constexpr uint32_t kDepthMax = 0xFFFFFFu;
constexpr uint32_t kDepthClear = 0u;

/** w in metres -> fx16 raw (S15.16). Round-half-up, matching the proof. */
inline uint64_t depth_fx16(double metres) { return static_cast<uint64_t>(metres * 65536.0 + 0.5); }

/** The smallest shift that brings W inside a u24. */
constexpr int depth_norm_shift(uint64_t W) {
  int s = 0;
  while ((W >> s) >= (1u << 24)) ++s;
  return s;
}

/**
 * `invw24` for one w, in fx16 raw units, under profile `p`.
 *
 * Clamping is part of the law, not a caller courtesy: `wmax` is a depth CLAMP
 * and not a far-clip plane, so a w beyond it is legal geometry that shares the
 * floor depth rather than being culled.
 *
 * The product reaches ~2^80, hence the 128-bit intermediate. A 64-bit
 * accumulator here would truncate silently and produce a plausible wrong depth
 * -- the exact failure the generator's BigInt arithmetic also avoids.
 */
inline uint32_t depth_of_raw(uint64_t W, const gen::DepthProfile& p) {
  if (W < p.wmin_raw) W = p.wmin_raw;
  if (W > p.wmax_raw) W = p.wmax_raw;

  const int s = depth_norm_shift(W);
  const uint32_t nw = static_cast<uint32_t>(W >> s);
  const rcp24_result rc = rcp_u24(nw ? nw : 1u);

  const depth_u128 prod = static_cast<depth_u128>(p.scale) * static_cast<depth_u128>(rc.r);
  const int sh = 48 + s - rc.k;
  // The three shipped profiles never produce this; a fourth one that did would
  // be unusable, and returning a wrong number quietly is worse than clamping
  // loudly at the floor.
  if (sh < 1 || sh > 126) return 0u;

  const depth_u128 q = (prod + (static_cast<depth_u128>(1) << (sh - 1))) >> sh;
  return (q > kDepthMax) ? kDepthMax : static_cast<uint32_t>(q);
}

/** Same, selecting the profile by index. */
inline uint32_t depth_of_raw(uint64_t W, uint32_t profile) {
  return depth_of_raw(W, gen::DEPTH_PROFILES[profile]);
}

/** Convenience for tests and tools that think in metres. */
inline uint32_t depth_of(double metres, uint32_t profile) {
  return depth_of_raw(depth_fx16(metres), profile);
}

/**
 * The per-VERTEX perspective attribute: `u_over_w` (or `v_over_w`), S8.24, of
 * a vertex texture coordinate under its view's `invw24`.
 *
 * spec/qformats.md 8 fixes only the per-PIXEL recovery,
 *     u = rescale((s64)u_over_w * rcp_u24(invw24_interp)),
 * and `zhao_raster_perspuv` derives that shift from three published formats
 * (u_over_w S8.24, invw24 U0.24, the TMU coordinate S15.16). This is the
 * inverse on the SAME three formats, so it is derived rather than chosen:
 *
 *     value(u_over_w) = value(u) * value(invw24)
 *     raw(u_over_w)   = rescale_s(sext(u) * invw24, 16)     (qformats 4)
 *
 * `uv` is the format-0 record's s16 UV field (fx16, GEOM.VDECODE's layout),
 * sign-extended to the TMU's S15.16. |uv| < 2^15 and invw24 < 2^24 keep the
 * product below 2^39, so the result is s24 and there is no saturation case.
 * GEOM.VATTR (fpga/rtl/geometry/zhao_geom_vattr.sv) computes exactly this per
 * landed vertex, and tests/geometry/geom_vattr_directed.cpp differences it.
 * Added 2026-09-19 with that block (owner rulings R11, R31; entry I46).
 */
inline int32_t geom_over_w(int16_t uv, uint32_t invw24) {
  const int64_t p = static_cast<int64_t>(uv) * static_cast<int64_t>(invw24 & 0xFFFFFFu);
  return static_cast<int32_t>((p + (int64_t{1} << 15)) >> 16);
}

/**
 * THE SAME LAW ON A WIDE COORDINATE, WITH THE SATURATION THE TYPE CARRIES.
 *
 * `geom_over_w` above takes an `int16_t` because the mesh path's coordinate IS
 * the vertex record's s16 field, and on that domain the product cannot leave
 * s32 -- which is why GEOM.VATTR has no saturate and is right not to.
 *
 * TERRAIN'S COORDINATE IS s32. `zhao_terrain_uvlane`'s `terr_uv_*` is a Q16.16
 * value in TILE units, so the product reaches 2^55, `rescale_s(.,16)` reaches
 * 2^39, and the S8.24 bound |value| < 128 becomes an edge a real island
 * crosses. `spec/qformats.md:75` has always said what happens there --
 * `u/v_over_w | s32 | S 8.24 | SATURATE | round-half-up` -- and 4's
 * `rescale_s` is "round-half-up ... followed by saturation to the destination
 * width". This function is that sentence made executable, and
 * `zhao_geom_overw_sat` is its RTL.
 *
 * It is a strict EXTENSION, not a second law: over the whole `int16_t` domain
 * it returns exactly what `geom_over_w` returns and never sets `*saturated`.
 * `tests/geometry/geom_overw_sat_directed.cpp` proves that exhaustively before
 * it quotes either of them, so that the wide form cannot drift into being a
 * rival statement of ratified arithmetic -- the failure GEOM.LIGHT's contract
 * names and TERRAIN.SHADE's header records shipping once.
 *
 * `saturated` may be null. Added 2026-09-26 (packet SHADELADDER, entry I13).
 */
inline int32_t geom_over_w_wide(int32_t uv, uint32_t invw24, bool* saturated = nullptr) {
  const int64_t p = static_cast<int64_t>(uv) * static_cast<int64_t>(invw24 & 0xFFFFFFu);
  const int64_t r = (p + (int64_t{1} << 15)) >> 16;  // arithmetic shift: floor, per 4
  const bool hi = r > int64_t{INT32_MAX};
  const bool lo = r < int64_t{INT32_MIN};
  if (saturated != nullptr) *saturated = hi || lo;
  if (hi) return INT32_MAX;
  if (lo) return INT32_MIN;
  return static_cast<int32_t>(r);
}

/**
 * The depth test, spec §8: pass iff strictly nearer. TIES FAIL -- decals use an
 * explicit bias rather than an epsilon, so that a scene is reproducible instead
 * of being one rounding mode away from flickering.
 */
constexpr bool depth_test(uint32_t d_new, uint32_t d_old) { return d_new > d_old; }

}  // namespace zref
