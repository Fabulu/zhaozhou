// zref_trig.hpp — trig, sqrt, normalize, smoothstep.
//
// Spec: spec/qformats.md (QFMT_VERSION 1)
//   §7.1  sin/cos: 257-entry Q1.16 quarter-wave table, u16-turns angle16,
//         all-integer interpolation; |fx_sin(a)/2^16 - sin(2*pi*a/2^16)| <=
//         1.3 LSB (derived 1.31, measured 1.1772), exhaustively asserted vs
//         the committed golden vectors (tests/golden/fixp/sin_cos_u16.bin)
//   §7.2  isqrt_u32/isqrt_u64: exact floor sqrt, restoring digit recurrence;
//         property r^2 <= n < (r+1)^2
//   §7.3  smoothstep: field_rcp + single-rounded MUL chain, t^2(3-2t)
//   §7.4  normalize3_approx: exact s128 sum of squares -> isqrt_u64 ->
//         rcp_u24_norm -> ONE rescale per component; <= 2 LSB per component
//         when n2 >= 2^48 (measured 0.51 LSB)
//
// The table is the committed generated constant (tools/fixgen, §11).

#pragma once

#include <cstdint>

#include "zref_fixp.hpp"
#include "zref_rcp.hpp"
#include "generated/zref_tables.hpp"

namespace zref {

namespace detail {

// §7.1 quarter-wave interpolation on the low 14 bits (index 8 + sub-tick 6).
constexpr int32_t sin_quarter(uint32_t a13) {
    const uint32_t i = a13 >> 6;
    const uint32_t t = a13 & 0x3Fu;
    const int32_t d = static_cast<int32_t>(gen::SIN_Q16[i + 1]) -
                      static_cast<int32_t>(gen::SIN_Q16[i]);
    return static_cast<int32_t>(gen::SIN_Q16[i]) +
           static_cast<int32_t>((static_cast<int64_t>(d) * t + 32) >> 6);
}

}  // namespace detail

// §7.1 fx_sin: angle16 (u16 turns) -> fx16. Exact identities (asserted in
// tests): sin(-a) = -sin(a); sin(0x8000 - a) = sin(a); sin(0x4000) = 0x10000.
constexpr fx16 fx_sin(angle16 a) {
    const uint32_t q = a.raw >> 14;
    const uint32_t a13 = a.raw & 0x3FFFu;
    const int32_t s = (q & 1u) ? detail::sin_quarter(0x4000u - a13)
                               : detail::sin_quarter(a13);
    return fx16{(q & 2u) ? -s : s};
}

// §7.1 fx_cos(a) = fx_sin(a + 0x4000) — exact by construction.
constexpr fx16 fx_cos(angle16 a) {
    return fx_sin(angle16{static_cast<uint16_t>(a.raw + 0x4000u)});
}

// §7.2 exact floor sqrt, restoring digit recurrence (binary longhand).
constexpr uint32_t isqrt_u32(uint32_t n) {
    uint32_t num = n;
    uint32_t res = 0;
    uint32_t bit = 1u << 30;
    while (bit > num) bit >>= 2;  // align to the highest digit pair
    while (bit != 0) {
        if (num >= res + bit) {
            num -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;  // res^2 <= n < (res+1)^2
}

// §7.2/§7.4 u64 variant (reference-only helper for normalize3_approx).
constexpr uint64_t isqrt_u64(uint64_t n) {
    uint64_t num = n;
    uint64_t res = 0;
    uint64_t bit = 1ull << 62;
    while (bit > num) bit >>= 2;
    while (bit != 0) {
        if (num >= res + bit) {
            num -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

// §7.4 normalize3_approx: out ~= v/|v|, <= 2 LSB per component when
// x^2+y^2+z^2 >= 2^48. len == 0 is pinned: returns the zero vector.
constexpr vec3fx normalize3_approx(vec3fx v, SatLedger* L) {
    const unsigned __int128 n2 =
        static_cast<unsigned __int128>(static_cast<int64_t>(v.x.raw)) * v.x.raw +
        static_cast<unsigned __int128>(static_cast<int64_t>(v.y.raw)) * v.y.raw +
        static_cast<unsigned __int128>(static_cast<int64_t>(v.z.raw)) * v.z.raw;
    if (n2 == 0) {
        return vec3fx{fx16{0}, fx16{0}, fx16{0}};
    }
    const uint64_t len = isqrt_u64(n2);  // exact floor
    int e = 0;
    uint64_t m = len;
    while (m < (1ull << 23)) { m <<= 1; --e; }
    while (m >= (1ull << 24)) { m >>= 1; ++e; }
    const uint32_t r = rcp_u24_norm(static_cast<uint32_t>(m));
    const int shift = 31 + e;  // out_raw = rescale(v_raw * r, 31 + e), ONE rounding
    return vec3fx{
        fx16{rescale_s32(static_cast<int64_t>(v.x.raw) * r, shift, L)},
        fx16{rescale_s32(static_cast<int64_t>(v.y.raw) * r, shift, L)},
        fx16{rescale_s32(static_cast<int64_t>(v.z.raw) * r, shift, L)},
    };
}

// §7.3 smoothstep(e0, e1, x): 0 at x<=e0, 1<<16 at x>=e1, t^2(3-2t) between
// (the t^2 factor is load-bearing: t(3-2t) is NOT the Hermite weight and is
// decreasing past t=0.75 — defect caught by the monotonicity test).
// d == 0 hits the pinned field_rcp zero rule (0x7FFFFFFF + RCP0).
constexpr fx16 smoothstep(fx16 e0, fx16 e1, fx16 x, SatLedger* L) {
    const fx16 d = fx_sub(e1, e0, L);
    const fx16 r = field_rcp(d, L);
    fx16 t = fx_mul(fx_sub(x, e0, L), r, L);
    t = fx_clamp(t, fx16{0}, fx16{1 << 16});
    const fx16 t2 = fx_mul(t, t, L);
    return fx_mul(t2, fx_sub(fx16{3 << 16}, fx_mul(fx16{2 << 16}, t, L), L), L);
}

}  // namespace zref
