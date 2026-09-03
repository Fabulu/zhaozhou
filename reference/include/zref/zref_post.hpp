// zref_post.hpp — the post-effect gather law (ruling R5).
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS AT ALL
// ---------------------------------------------------------------------------
// POST.GATHER.md cited `zref::PostGather` as its scalar reference and no such
// symbol had ever been written. The ledger's V17 caught it the moment the
// block gained real evidence to check against — "a symbol nobody defined is a
// phantom citation".
//
// The right answer to a phantom citation is to write the symbol, not to drop
// the claim, so this is the law the citation was always pointing at.
//
// ---------------------------------------------------------------------------
// WHAT THE LAW IS
// ---------------------------------------------------------------------------
// Accumulate in u16 per channel, saturating, and pack to RGB565 EXACTLY ONCE,
// at tile flush. Accumulating in the packed format would lose the headroom
// that makes bloom read as light rather than as clipping.
//
// Displacement accumulates as signed 8.8 in a wide saturating lane, rounds
// ONCE to integer pixels, and clamps to X in [-8, +8] and Y in [-4, +4].
//
// THOSE CLAMPS ARE NOT THIS BLOCK'S OWN BOUND. They are what POST.COMPOSITE's
// nine-line ring is built against, and nothing in the compositor can detect a
// gather that exceeds them.
#pragma once

#include <cstdint>

namespace zref {
namespace post {

inline constexpr int kDispMaxX = 8;
inline constexpr int kDispMaxY = 4;

// A saturating u16 accumulate of one u8 contribution.
inline uint16_t glow_accumulate(uint16_t acc, uint8_t add) {
  const uint32_t s = static_cast<uint32_t>(acc) + add;
  return static_cast<uint16_t>(s > 0xFFFFu ? 0xFFFFu : s);
}

// The ONE rounding into RGB565, at flush. A channel at or above 255 is full.
inline uint16_t glow_pack565(uint16_t r, uint16_t g, uint16_t b) {
  const uint16_t r5 = (r > 255u) ? 31u : static_cast<uint16_t>(r >> 3);
  const uint16_t g6 = (g > 255u) ? 63u : static_cast<uint16_t>(g >> 2);
  const uint16_t b5 = (b > 255u) ? 31u : static_cast<uint16_t>(b >> 3);
  return static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
}

// signed 8.8 -> integer pixels, round-half-up, then clamp. One rounding.
inline int8_t disp_to_pixels(int16_t v, int limit, bool* clamped) {
  const int32_t r = (static_cast<int32_t>(v) + 128) >> 8;
  const bool c = (r > limit) || (r < -limit);
  if (clamped != nullptr) *clamped = c;
  return static_cast<int8_t>(r > limit ? limit : (r < -limit ? -limit : r));
}

}  // namespace post
}  // namespace zref
