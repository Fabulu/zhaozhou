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

// ---------------------------------------------------------------------------
// THE EXACT GRADING PREPARATION -- POST.COMPOSITE, owner plan 2026-09-18 11.2
// ---------------------------------------------------------------------------
// This half of the file belongs to POST.COMPOSITE, not POST.GATHER. It lives
// here because `zref::post` is where the post subsystem's ratified arithmetic
// lives, and because a table generator that exists only inside a test header is
// a law with no owner -- the shape that produced two projector cores.
//
// The plan's arrangement: "stores unrounded matrix-product vectors for each
// curve entry, sums them and performs the ORIGINAL final bias/round/saturate".
// The last clause is the load-bearing one. This is NOT a new colour convention;
// it is the SAME curve+matrix law with the multiply hoisted out of the per-pixel
// path and into table generation, and the finalize step is untouched.
//
// WHY 24 SIGNED BITS IS EXACT, not merely roomy. A matrix coefficient is Q2.14
// in s16, so |m| <= 32768; a curve entry is a u8, so k <= 255. The largest
// product is 32,768 * 255 = 8,355,840, and a signed 24-bit field holds
// [-8,388,608, +8,388,607]. It fits with 32,767 to spare, and it would NOT fit
// in 23. Three of them per entry is 72 bits; 128 entries is 9,216 logical bits.
//
// NOTHING HERE ROUNDS. That is the whole point -- rounding a product before the
// sum is exactly what would make the table an approximation instead of an
// identity, and it is the error this arrangement is most likely to acquire.

// A curve entry's unrounded product vector: that entry's value times its own
// COLUMN of the matrix. Column 0 is the R curve, 1 the G curve, 2 the B curve;
// `p[o]` is the contribution to OUTPUT channel o. Getting the row/column
// transpose backwards produces a plausible-looking picture with the channels
// cross-mixed, so the indexing is spelled out rather than implied.
inline void grade_product_vector(const int16_t m[9], int col, uint8_t k, int32_t p[3]) {
  p[0] = static_cast<int32_t>(m[(0 * 3) + col]) * static_cast<int32_t>(k);
  p[1] = static_cast<int32_t>(m[(1 * 3) + col]) * static_cast<int32_t>(k);
  p[2] = static_cast<int32_t>(m[(2 * 3) + col]) * static_cast<int32_t>(k);
}

// The ORIGINAL final step, shared by both paths and changed by neither: one
// round-half-up, then the bias, then one saturate.
inline uint8_t grade_finalize(int32_t acc, int bias) {
  int32_t q = (acc + 8192) >> 14;
  q += bias;
  return static_cast<uint8_t>(q < 0 ? 0 : (q > 255 ? 255 : q));
}

// The nine-multiplier path, kept as the REFERENCE the table is proved against.
// It is not a second implementation of the law -- it is the law, and the table
// is a precomputation of it that has to be shown to agree.
inline uint8_t grade_channel_mul(const int16_t m[9], int row, uint8_t kr, uint8_t kg, uint8_t kb,
                                 int bias) {
  const int32_t acc = (static_cast<int32_t>(m[(row * 3) + 0]) * kr) +
                      (static_cast<int32_t>(m[(row * 3) + 1]) * kg) +
                      (static_cast<int32_t>(m[(row * 3) + 2]) * kb);
  return grade_finalize(acc, bias);
}

// The table path: sum the three vectors' component for this output channel,
// then the same finalize. Equal to `grade_channel_mul` by construction, which
// is precisely why it must be CHECKED -- the failure available is a packing,
// sign-extension or transpose error, not an algebraic one.
//
// Note what CANNOT go wrong here, so nobody writes a test for it: the order of
// `pr`, `pg` and `pb` is immaterial, because they are summed. A mutation that
// swaps two of them is not a fault, and a check that claims to catch one is
// claiming something it cannot do. The transpose that DOES matter lives in
// `grade_product_vector` above, and that is where the positive control aims.
inline uint8_t grade_channel_table(const int32_t pr[3], const int32_t pg[3], const int32_t pb[3],
                                   int row, int bias) {
  return grade_finalize(pr[row] + pg[row] + pb[row], bias);
}

// ===========================================================================
// POST.ECHO -- the capture law (owner ruling R7, design/contracts/POST.ECHO.md)
// ===========================================================================
// The echo writes POST.COMPOSITE's post-ink/pre-HUD pixel, unchanged, into a
// capture window. There is no colour law here -- only WHERE a pixel goes, which
// passes are refused, and the granularity at which a starved echo drops. Those
// three are what the RTL can get wrong, so they are what this owns.
namespace echo {

// zhao_pkg ZHAO_POST_ECHO_BASE / _SPAN (spec/memory_rules.md 5g).
inline constexpr uint32_t kCaptureBase = 0x05C00000u;
inline constexpr uint32_t kCaptureSpan = 0x0003C000u;
// A chunk is one RASTER.FBWRITE row burst: 16 pixels, 32 bytes. Drops are whole
// chunks, so a starved echo leaves clean holes and never a torn burst.
inline constexpr unsigned kChunkPx = 16;

// A pass is capturable only if its rows split into whole chunks and the stacked
// image fits the window. Anything else is refused at pass start.
inline bool geometry_ok(unsigned w, unsigned h, unsigned views) {
  if (w == 0 || h == 0 || views == 0) return false;
  if (w % kChunkPx != 0) return false;
  return uint64_t(views) * h * w * 2u <= kCaptureSpan;
}

// Views are STACKED vertically: view v's row y is capture row v*h + y. The
// stride is the view width, so no multiplier is needed to reach view 1.
inline uint32_t capture_addr(unsigned view, unsigned x, unsigned y, unsigned w, unsigned h) {
  return kCaptureBase + ((uint32_t(view) * h + y) * w + x) * 2u;
}

// Which chunk of its row a pixel belongs to, and whether it opens one.
inline unsigned chunk_of(unsigned x) { return x / kChunkPx; }
inline bool opens_chunk(unsigned x) { return (x % kChunkPx) == 0; }

}  // namespace echo

}  // namespace post
}  // namespace zref
