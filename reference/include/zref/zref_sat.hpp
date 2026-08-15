// zref_sat.hpp — saturation ledger and clamp helpers.
//
// Spec: spec/qformats.md
//   §5   SatLedger counter names and rules (explicit parameter, nullptr
//        allowed, no-clamp invariance)
//   §3   overflow policy: saturate and record (charter §29-11)
//   §4   rescale() round-half-up definition
//
// The ledger is the ONLY overflow observability in the machine; counter
// names are mirrored by hardware performance counters and design/blocks.yml.
// No thread-locals, no globals — every recording operation takes SatLedger*
// as an explicit parameter.

#pragma once

#include <cstdint>

namespace zref {

struct SatLedger {
  uint32_t add = 0;      // clamps in fx16/fx24 add/sub (incl. mat4 row sums)
  uint32_t mul = 0;      // clamps in fx_mul/fx_mad/fx_div_exact/fx24 mul/mad
  uint32_t rescale = 0;  // clamps in bare rescale()/conversions (screenXY, height16)
  uint32_t unit = 0;     // clamps in unit8 conversions
  uint32_t rcp = 0;      // clamps in field_rcp output saturation
  uint32_t rcp0 = 0;     // RCP0 events: reciprocal input == 0 (not a saturation)

  constexpr void reset() { add = mul = rescale = unit = rcp = rcp0 = 0; }
  constexpr uint64_t total() const {
    return static_cast<uint64_t>(add) + mul + rescale + unit + rcp + rcp0;
  }
};

namespace detail {

// Record one clamp into the named counter (no-op when no ledger is supplied).
constexpr void ledger_bump(SatLedger* L, uint32_t SatLedger::*counter) {
  if (L != nullptr) {
    ++(L->*counter);
  }
}

}  // namespace detail

}  // namespace zref
