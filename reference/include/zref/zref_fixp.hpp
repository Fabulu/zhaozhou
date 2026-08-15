// zref_fixp.hpp — strong fixed-point types and core arithmetic.
//
// Spec: spec/qformats.md (QFMT_VERSION 1)
//   §1    notation: S i . f triples, TI-style Qm.n equivalence
//   §2    type table (fx16 S 1.15.16 / fx24 S 1.39.24 / angle16 u16 turns /
//         unit8 U 0.0.8 / height16 S 1.7.8 / screenXY S 12.8) + conversions
//   §3    core arithmetic pseudocode + the single-rounding law (A3b):
//         MUL/MAD compute the exact wide-integer expression, then ONE
//         rescale(.,16) — no double rounding anywhere
//   §4    rescale(): the one rounding primitive (round-half-up)
//   §5    SatLedger explicit-parameter discipline
//   §7.5  noise2_hash: PCG RXS-M-XS constants frozen verbatim (A1)
//   §8    screenXY conversion + provisional ±2048 px guard band (Q4/A3c)
//   §9    height16 <-> fx16 (Q1: S 1.7.8)
//
// Public API is concrete strong types with explicit function calls only — no
// operator overloading, no implicit conversions (charter §20.1; the RTL
// subset rule forbids clever templates in SV, and the reference stays flat
// to mirror it). __int128 is a documented g++ extension used ONLY here in
// the reference (and in the test oracles), never in RTL (Q2).

#pragma once

#include <cstdint>
#include <cstddef>

#include "zref_sat.hpp"

namespace zref {

// ---- §2 strong types -------------------------------------------------------

struct fx16 {
  int32_t raw;
};  // S 1.15.16, step 2^-16 (~0.015 mm at m-scale)
struct fx24 {
  int64_t raw;
};  // S 1.39.24, sim/world truth accumulator
struct angle16 {
  uint16_t raw;
};  // U 0.0.16 turns, wraps mod 2^16
struct unit8 {
  uint8_t raw;
};  // U 0.0.8, value = raw/256
struct height16 {
  int16_t raw;
};  // S 1.7.8 metres (Q1)

struct vec3fx {
  fx16 x, y, z;
};

constexpr fx16 fx16_from_raw(int32_t raw) { return fx16{raw}; }
constexpr fx24 fx24_from_raw(int64_t raw) { return fx24{raw}; }
constexpr angle16 angle16_from_raw(uint16_t raw) { return angle16{raw}; }

// ---- §4 rescale(): the one rounding primitive ------------------------------

namespace detail {

constexpr int64_t floor_div_s64(int64_t n, int64_t d) {
  int64_t q = n / d;
  int64_t r = n % d;
  return (r != 0 && ((r < 0) != (d < 0))) ? q - 1 : q;
}

// round_half_up(n/d) for signed exact division (oracle/table generation, §4).
constexpr int64_t div_rhu_s64(int64_t n, int64_t d) {
  if (d < 0) {
    n = -n;
    d = -d;
  }
  return floor_div_s64(n + d / 2, d);
}

constexpr int32_t sat_s32_from_s64(int64_t v, SatLedger* L, uint32_t SatLedger::*c) {
  if (v > INT32_MAX) {
    detail::ledger_bump(L, c);
    return INT32_MAX;
  }
  if (v < INT32_MIN) {
    detail::ledger_bump(L, c);
    return INT32_MIN;
  }
  return static_cast<int32_t>(v);
}

constexpr int16_t sat_s16_from_s64(int64_t v, SatLedger* L, uint32_t SatLedger::*c) {
  if (v > INT16_MAX) {
    detail::ledger_bump(L, c);
    return INT16_MAX;
  }
  if (v < INT16_MIN) {
    detail::ledger_bump(L, c);
    return INT16_MIN;
  }
  return static_cast<int16_t>(v);
}

}  // namespace detail

// rescale_s32(x, k): round-half-up shift, then saturate to the fx16 word.
// k == 0 is the identity. Clamps record in L->rescale unless a more specific
// counter is passed by the op wrappers below (§5). The rounding add runs in
// s128: x near INT64_MAX must not wrap before the shift.
constexpr int32_t rescale_s32(int64_t x, int k, SatLedger* L,
                              uint32_t SatLedger::*c = &SatLedger::rescale) {
  if (k == 0) return detail::sat_s32_from_s64(x, L, c);
  __int128 r = (static_cast<__int128>(x) + (static_cast<__int128>(1) << (k - 1))) >> k;
  if (r > INT32_MAX) {
    detail::ledger_bump(L, c);
    return INT32_MAX;
  }
  if (r < INT32_MIN) {
    detail::ledger_bump(L, c);
    return INT32_MIN;
  }
  return static_cast<int32_t>(r);
}

// rescale_s64: fx24 lanes (s128 intermediate; reference-only extension, §3).
constexpr int64_t rescale_s64(__int128 x, int k, SatLedger* L,
                              uint32_t SatLedger::*c = &SatLedger::rescale) {
  if (k == 0) {
    if (x > INT64_MAX) {
      detail::ledger_bump(L, c);
      return INT64_MAX;
    }
    if (x < INT64_MIN) {
      detail::ledger_bump(L, c);
      return INT64_MIN;
    }
    return static_cast<int64_t>(x);
  }
  __int128 r = (x + (static_cast<__int128>(1) << (k - 1))) >> k;
  if (r > INT64_MAX) {
    detail::ledger_bump(L, c);
    return INT64_MAX;
  }
  if (r < INT64_MIN) {
    detail::ledger_bump(L, c);
    return INT64_MIN;
  }
  return static_cast<int64_t>(r);
}

// ---- §3 fx16 arithmetic (single-rounding law: A3b) --------------------------

constexpr fx16 fx_add(fx16 a, fx16 b, SatLedger* L) {
  return fx16{detail::sat_s32_from_s64(static_cast<int64_t>(a.raw) + b.raw, L, &SatLedger::add)};
}

constexpr fx16 fx_sub(fx16 a, fx16 b, SatLedger* L) {
  return fx16{detail::sat_s32_from_s64(static_cast<int64_t>(a.raw) - b.raw, L, &SatLedger::add)};
}

// p = (s64)a*b exact (S 2.30.32); ONE rescale(.,16). (§3)
constexpr fx16 fx_mul(fx16 a, fx16 b, SatLedger* L) {
  int64_t p = static_cast<int64_t>(a.raw) * b.raw;
  return fx16{rescale_s32(p, 16, L, &SatLedger::mul)};
}

// p = (s64)a*b + (s64)c<<16 exact; ONE rescale(.,16). (§3, A3b: no double rounding)
constexpr fx16 fx_mad(fx16 a, fx16 b, fx16 c, SatLedger* L) {
  int64_t p = static_cast<int64_t>(a.raw) * b.raw + (static_cast<int64_t>(c.raw) << 16);
  return fx16{rescale_s32(p, 16, L, &SatLedger::mul)};
}

// Sim-only exact division: round_half_up((s128)a<<16 / b); saturate.
// b == 0 is pinned: 0x7FFF_FFFF / 0x8000_0000 by sign of a, +RCP0. (§3, §6.2)
constexpr fx16 fx_div_exact(fx16 a, fx16 b, SatLedger* L) {
  if (b.raw == 0) {
    detail::ledger_bump(L, &SatLedger::rcp0);
    return fx16{a.raw >= 0 ? INT32_MAX : INT32_MIN};
  }
  __int128 num = static_cast<__int128>(a.raw) << 16;
  int64_t d = b.raw;
  if (d < 0) {
    num = -num;
    d = -d;
  }
  // floor((num + d/2) / d) with s128 floor semantics:
  __int128 h = num + d / 2;
  __int128 q = h / d;
  __int128 r = h % d;
  if (r != 0 && r < 0) q -= 1;
  if (q > INT32_MAX) {
    detail::ledger_bump(L, &SatLedger::mul);
    return fx16{INT32_MAX};
  }
  if (q < INT32_MIN) {
    detail::ledger_bump(L, &SatLedger::mul);
    return fx16{INT32_MIN};
  }
  return fx16{static_cast<int32_t>(q)};
}

// ---- §3 fx24 arithmetic (sim truth; s128 products, never in RTL — Q2) -------

constexpr fx24 fx24_add(fx24 a, fx24 b, SatLedger* L) {
  return fx24{rescale_s64(static_cast<__int128>(a.raw) + b.raw, 0, L, &SatLedger::add)};
}

constexpr fx24 fx24_sub(fx24 a, fx24 b, SatLedger* L) {
  return fx24{rescale_s64(static_cast<__int128>(a.raw) - b.raw, 0, L, &SatLedger::add)};
}

constexpr fx24 fx24_mul(fx24 a, fx24 b, SatLedger* L) {
  __int128 p = static_cast<__int128>(a.raw) * b.raw;
  return fx24{rescale_s64(p, 24, L, &SatLedger::mul)};
}

constexpr fx24 fx24_mad(fx24 a, fx24 b, fx24 c, SatLedger* L) {
  __int128 p = static_cast<__int128>(a.raw) * b.raw + (static_cast<__int128>(c.raw) << 24);
  return fx24{rescale_s64(p, 24, L, &SatLedger::mul)};
}

// ---- §3 unit8 / angle16 ----------------------------------------------------

// ((u32)a*b + 128) >> 8, clamp 255. Result <= 254 for all inputs; the clamp
// is retained defensively per the frozen pseudocode (§3).
constexpr uint8_t unit_mul(unit8 a, unit8 b) {
  uint32_t r = (static_cast<uint32_t>(a.raw) * b.raw + 128) >> 8;
  return static_cast<uint8_t>(r > 255 ? 255 : r);
}

constexpr angle16 ang_add(angle16 a, angle16 b) {  // wraps, never records (§3)
  return angle16{static_cast<uint16_t>(a.raw + b.raw)};
}

constexpr angle16 ang_sub(angle16 a, angle16 b) {  // wraps, never records (§3)
  return angle16{static_cast<uint16_t>(a.raw - b.raw)};
}

constexpr angle16 ang_negate(angle16 a) {  // -a mod 2^16
  return angle16{static_cast<uint16_t>(0u - a.raw)};
}

// ---- §2 conversions --------------------------------------------------------

constexpr fx16 fx_from_unit8(unit8 u) { return fx16{static_cast<int32_t>(u.raw) << 8}; }

constexpr unit8 unit8_from_fx16(fx16 x, SatLedger* L) {
  int32_t r = x.raw;
  if (r < 0) {
    detail::ledger_bump(L, &SatLedger::unit);
    return unit8{0};
  }
  if (r > 0xFFFF) {
    detail::ledger_bump(L, &SatLedger::unit);
    return unit8{255};
  }
  // Review C2 (RUN-20260814-1912): r in [0xFF80, 0xFFFF] gives
  // (r + 128) >> 8 == 256, which truncated to uint8_t wrapped to 0 — a ~1.0
  // weight silently became 0. Clamp to the 255 rail instead (defensive
  // clamp, unrecorded, mirroring unit_mul's retained 255 clamp; §2/§3).
  const int32_t q = (r + 128) >> 8;
  return unit8{static_cast<uint8_t>(q > 255 ? 255 : q)};
}

constexpr fx16 fx_from_height16(height16 h) {  // exact (§9)
  return fx16{static_cast<int32_t>(h.raw) << 8};
}

constexpr height16 height16_from_fx16(fx16 x, SatLedger* L) {  // bake-back (§9)
  return height16{detail::sat_s16_from_s64(rescale_s32(x.raw, 8, nullptr), L, &SatLedger::rescale)};
}

// §8 screenXY: fx16 -> S 12.8 px, round-half-up, clamp to the provisional
// ±2048 px guard band (Q4: widths frozen now, extent re-ratified Phase 4/5).
constexpr int32_t SCREEN_GUARD_PX = 2048;

constexpr int32_t to_screen_xy(fx16 x, SatLedger* L) {
  int32_t px = rescale_s32(x.raw, 8, L);  // rescale(.,8): 16 -> 8 fraction bits
  const int32_t lim = SCREEN_GUARD_PX << 8;
  if (px > lim) {
    detail::ledger_bump(L, &SatLedger::rescale);
    return lim;
  }
  if (px < -lim) {
    detail::ledger_bump(L, &SatLedger::rescale);
    return -lim;
  }
  return px;
}

// ---- §3 pure integer helpers (no rounding, no records) ----------------------

constexpr fx16 fx_min(fx16 a, fx16 b) { return fx16{a.raw < b.raw ? a.raw : b.raw}; }
constexpr fx16 fx_max(fx16 a, fx16 b) { return fx16{a.raw > b.raw ? a.raw : b.raw}; }
constexpr fx16 fx_abs(fx16 a) {
  return fx16{a.raw == INT32_MIN ? INT32_MIN : (a.raw < 0 ? -a.raw : a.raw)};
}
constexpr fx16 fx_clamp(fx16 x, fx16 lo, fx16 hi) { return fx_max(lo, fx_min(hi, x)); }

// ---- §2 mat4fx x vec4 (single rounding per row, A3b) ------------------------

struct vec4fx {
  fx16 x, y, z, w;
};
struct mat4fx {
  fx16 m[4][4];
};  // row-major

// result_i = rescale( sum_j (s128)m[i][j] * v[j], 16 ): four 32x32 products
// per row summed exactly (s128 in the reference — a superset of the frozen
// s64 row sum, identical whenever the s64 sum does not overflow), then ONE
// rescale + saturate per row. (§2)
constexpr vec4fx mat4_vec4(const mat4fx& a, const vec4fx& v, SatLedger* L) {
  const fx16 col[4] = {v.x, v.y, v.z, v.w};
  vec4fx r{};
  fx16* out[4] = {&r.x, &r.y, &r.z, &r.w};
  for (int i = 0; i < 4; ++i) {
    __int128 p = 0;
    for (int j = 0; j < 4; ++j) {
      p += static_cast<__int128>(a.m[i][j].raw) * col[j].raw;
    }
    // one round-half-up shift by 16, then saturate, all in s128
    __int128 q = (p + (static_cast<__int128>(1) << 15)) >> 16;
    if (q > INT32_MAX) {
      detail::ledger_bump(L, &SatLedger::mul);
      *out[i] = fx16{INT32_MAX};
    } else if (q < INT32_MIN) {
      detail::ledger_bump(L, &SatLedger::mul);
      *out[i] = fx16{INT32_MIN};
    } else {
      *out[i] = fx16{static_cast<int32_t>(q)};
    }
  }
  return r;
}

// ---- §7.5 noise2 lattice hash (PCG RXS-M-XS, constants frozen verbatim) ----

constexpr uint32_t noise2_hash(uint32_t x, uint32_t y, uint32_t seed, unsigned lane) {
  uint32_t s = (x * 0x9E3779B1u) ^ ((y * 0x85EBCA77u) ^ seed);  // lattice mix
  s = s + static_cast<uint32_t>(lane) * 0xE1u;                  // lane salt
  s = s * 747796405u + 2891336453u;                             // LCG advance
  uint32_t w = (((s >> ((s >> 28) + 4)) ^ s) * 277803737u);     // RXS-M-XS
  return (w >> 22) ^ w;
}

}  // namespace zref
