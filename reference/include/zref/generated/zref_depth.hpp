// GENERATED FILE - tools/fixgen (spec/qformats.md 8) - DO NOT EDIT.
// QFMT_VERSION 3; regenerate with `npm run tables:gen` and commit.
#pragma once
#include <cstdint>

namespace zref {
namespace gen {

// qformats.md 8: view-depth profiles (owner ruling 2026-08-31 #1).
//   s      = smallest shift with (W >> s) < 2^24     (W = w in fx16 raw)
//   {r, k} = rcp_u24(W >> s)
//   d      = rescale(SCALE * r, 48 + s - k), round-half-up, sat 0xFFFFFF
// SCALE is SOLVED from the law's own output at wmin, not from the ideal
// reciprocal -- 0xFFFFFF * wmin gives 0xFFFFFE, one short of the pin.
// wmax is a depth CLAMP, not a far-clip plane.
struct DepthProfile {
  const char* name;
  uint64_t wmin_raw;  // fx16 (S15.16)
  uint64_t wmax_raw;
  unsigned long long scale;  // solved; a power of two for THESE three only
  uint32_t d_at_wmin;        // pinned 0xFFFFFF
  uint32_t d_at_wmax;        // the non-zero far floor
};

inline constexpr uint32_t DEPTH_PROFILE_COUNT = 3;

inline constexpr DepthProfile DEPTH_PROFILES[3] = {
    // WORLD_LONG: 1 m .. 16384 m
    {"WORLD_LONG", 65536ull, 1073741824ull, 1099511627776ull, 0xFFFFFFu, 1024u},
    // WORLD_STANDARD: 0.5 m .. 8192 m
    {"WORLD_STANDARD", 32768ull, 536870912ull, 549755813888ull, 0xFFFFFFu, 1024u},
    // CLOSE: 0.25 m .. 2048 m
    {"CLOSE", 16384ull, 134217728ull, 274877906944ull, 0xFFFFFFu, 2048u},
};

}  // namespace gen
}  // namespace zref
