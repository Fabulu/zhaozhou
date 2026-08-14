// zref_rcp.hpp — the two frozen reciprocals (ratified decision A3a).
//
// Spec: spec/qformats.md (QFMT_VERSION 1)
//   §6.1  rcp_u24 — raster/depth path (U 0.0.24): 256-entry Q0.31 initial
//         table + two frozen Newton-Raphson steps; |r - 2^47/m| <= 1 LSB,
//         exhaustively proven; full-domain FNV-1a-64 hash
//         RCP24_FULL_HASH = 0xd624beb8659baf83 (asserted nightly)
//   §6.2  field_rcp — Field IR RCP opcode (Q16.16): 256-entry Q0.17 table +
//         ONE pinned linear correction; |r - 2^32/a| <= |2^32/a|*2^-14 + 1;
//         rcp(0) = 0x7FFF_FFFF pinned, records RCP0 in the ledger
//
// Tables are the committed, generated constants in zref/generated/zref_tables.hpp
// (tools/fixgen, §11) — never recomputed, never hand-written.

#pragma once

#include <cstdint>

#include "zref_fixp.hpp"
#include "generated/zref_tables.hpp"

namespace zref {

// rcp_u24 full-domain hash over all d = 1..0xFFFFFF (3 LE bytes of each r),
// FNV-1a-64 (offset 14695981039346656037, prime 1099511628211). (§6.1)
constexpr uint64_t RCP24_FULL_HASH = 0xd624beb8659baf83ull;

struct rcp24_result {
    uint32_t r;  // normalized mantissa of the reciprocal, [2^23, 2^24]
    int k;       // reciprocal value ~= (r / 2^24) * 2^k
};

namespace detail {

constexpr uint64_t rescale_u64_nosat(uint64_t x, int k) {
    return k == 0 ? x : (x + (UINT64_C(1) << (k - 1))) >> k;
}

}  // namespace detail

// §6.1 core: m normalized to [2^23, 2^24) (bit 23 set). r ~= 2^47/m.
// The only saturating input is the pinned m == 2^23 (exact 2^24 -> 0xFFFFFF,
// error exactly 1 LSB, recorded nowhere — it is pinned law, not overflow).
constexpr uint32_t rcp_u24_norm(uint32_t m) {
    const uint32_t idx = (m - (1u << 23)) >> 15;
    uint32_t x = gen::RCP24_T0[idx];
    const uint64_t M = m;
    for (int step = 0; step < 2; ++step) {
        uint64_t p = M * x;
        uint64_t w = p >> 24;
        x = static_cast<uint32_t>(
            detail::rescale_u64_nosat(static_cast<uint64_t>(x) * ((2ull << 30) - w), 30));
    }
    uint32_t r = static_cast<uint32_t>(detail::rescale_u64_nosat(x, 7));
    return r > 0xFFFFFFu ? 0xFFFFFFu : r;
}

// §6.1 wrapper: any nonzero u24 d. Returns r plus the exponent k so that
// 1/(d/2^24) ~= (r/2^24) * 2^k exactly reconstructs at the caller.
constexpr rcp24_result rcp_u24(uint32_t d) {
    uint32_t m = d;
    int e = 0;
    while ((m & (1u << 23)) == 0) {
        m <<= 1;
        ++e;
    }
    return rcp24_result{rcp_u24_norm(m), e + 1};
}

// §6.2 field_rcp(a): fx16 in, fx16 out, sticky RCP0 in the ledger.
constexpr fx16 field_rcp(fx16 a, SatLedger* L) {
    if (a.raw == 0) {
        detail::ledger_bump(L, &SatLedger::rcp0);
        return fx16{INT32_MAX};  // pinned 0x7FFFFFFF (qformats.md 6.2)
    }
    const bool neg = a.raw < 0;
    uint32_t n = neg ? static_cast<uint32_t>(-static_cast<int64_t>(a.raw))
                     : static_cast<uint32_t>(a.raw);
    int e = 31;
    while ((n & (1u << 31)) == 0) {
        n <<= 1;
        --e;
    }
    const uint32_t idx = (n - (1u << 31)) >> 23;
    uint64_t x = gen::FIELD_RCP_T0[idx];
    uint64_t p = static_cast<uint64_t>(n) * x;
    x = detail::rescale_u64_nosat(x * ((1ull << 48) - p), 47);  // one pinned correction
    uint64_t r = detail::rescale_u64_nosat(x << 16, e);
    if (r > static_cast<uint64_t>(INT32_MAX)) {
        detail::ledger_bump(L, &SatLedger::rcp);
        return fx16{neg ? INT32_MIN : INT32_MAX};
    }
    return fx16{neg ? -static_cast<int32_t>(r) : static_cast<int32_t>(r)};
}

}  // namespace zref
