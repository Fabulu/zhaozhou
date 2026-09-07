#pragma once
// zref_fog.hpp — the deterministic fog factor, spec/qformats.md §8.
//
// ---------------------------------------------------------------------------
// WHAT IS FROZEN HERE AND WHAT IS NOT
// ---------------------------------------------------------------------------
// §8's FACTOR computation survives owner ruling D-5 (2026-09-03) untouched, and
// D-5 says so explicitly: "the fog factor is computed once per vertex from the
// frozen view/fog law — that is f_raw / f / f8, unchanged, still frozen."
//
// What D-5 REPLACED is everything downstream: the factor is now a SEPARATE
// INTERPOLANT rather than something pre-mixed into vertex colour, and the mix
// happens per-fragment at the final source colour, after toon quantisation and
// material combination. That mix lives in the rasteriser (zrender/rast.cpp);
// this header owns only the per-vertex number.
//
// The reason the split matters is the reason D-5 exists: a toon ramp is a
// QUANTISER. Handed colour with fog already in it, it cannot tell "less lit"
// from "further away" — both arrive as a smaller number and snap to the same
// band edge, so a smooth depth gradient becomes a staircase that moves with the
// object. Carrying the factor separately keeps the bands describing lighting,
// which is what they are for.
#include <cstdint>

#include "zref/zref_fixp.hpp"
#include "zref/zref_rcp.hpp"

namespace zref {
namespace fog {

/** Per-frame reciprocal: k = field_rcp(fog_far - fog_near), §8.
 *
 *  The denominator is frame-constant, which is why this is computed once per
 *  frame and not per vertex. `fog_far <= fog_near` is the DISABLED case — a
 *  deterministic no-op, not an error (§8) — and callers must test it before
 *  calling this, because field_rcp of a non-positive value is not the law's
 *  domain. */
inline fx16 frame_k(fx16 fog_near, fx16 fog_far, SatLedger* L) {
  return rcp::field_rcp(fx_sub(fog_far, fog_near, L), L);
}

/** Per-vertex clear factor, §8's frozen three lines.
 *
 *      f_raw = fx_mul(fx_sub(fog_far, d), k)   // ONE rounding (§3)
 *      f     = clamp(f_raw, 0, 0x10000)
 *
 *  `d` is the view-space FORWARD distance — the guarded `w` the depth pipeline
 *  already clamps — and deliberately NOT radial distance: radial costs a
 *  per-vertex isqrt_u32 (§7.2) and differs only off-axis.
 *
 *  POLARITY, because it is the one thing easy to get backwards and §8's
 *  superseded mix formula reads as if it were the other way round:
 *      f == 0x10000  ->  CLEAR      (d <= fog_near)
 *      f == 0        ->  FULL FOG   (d >= fog_far)
 *  The rasteriser's mix therefore weights toward the fog colour by the
 *  COMPLEMENT of this value. */
inline int32_t vertex_factor(fx16 d, fx16 fog_near, fx16 fog_far, fx16 k, SatLedger* L) {
  if (fog_far.raw <= fog_near.raw) return 0x10000;  // §8's disabled no-op
  const fx16 f_raw = fx_mul(fx_sub(fog_far, d, L), k, L);
  if (f_raw.raw < 0) return 0;
  if (f_raw.raw > 0x10000) return 0x10000;
  return f_raw.raw;
}

}  // namespace fog
}  // namespace zref
