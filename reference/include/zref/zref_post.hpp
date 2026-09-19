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

// ===========================================================================
// THE LOOK -- SetPost 0x0040 and SetGradeTable 0x0041 (owner rulings R35/R36)
// ===========================================================================
// What a committed SetPost puts on POST.COMPOSITE's look inputs (and on
// POST.ECHO's arm), and what a SetGradeTable writes into its product-vector
// table. CMD.EXEC is differenced against these in
// tests/command/cmd_exec_directed.cpp; the grading-table EMITTER at the bottom is
// the one place a curve set and a matrix become wire records, so the table that
// reaches the RTL is generated by `grade_product_vector` and nothing else.
namespace look {

// The identity look: what the console shows before any SetPost.
struct Look {
  uint8_t bloom_gain = 0;
  bool grade_valid = false;
  bool echo_arm = false;          // R35: capture only when armed
  int16_t bias_r = 0, bias_g = 0, bias_b = 0;
  uint16_t flash = 0;             // rgb565
  uint8_t flash_amount = 0;       // unit8
  uint16_t ink = 0;               // rgb565
};

inline constexpr uint8_t kFlagGradeValid = 0x01;
inline constexpr uint8_t kFlagEchoArm = 0x02;
inline constexpr uint8_t kFlagsAssigned = kFlagGradeValid | kFlagEchoArm;

// POST.COMPOSITE's bias is signed NINE bits. A wider value is REFUSED.
inline bool bias_fits(int16_t v) { return v >= -256 && v <= 255; }

// A SetPost either REPLACES the whole look or is refused and changes nothing.
// Returns false on refusal: an unassigned flag bit or a bias outside nine bits.
inline bool apply_set_post(uint8_t bloom_gain, uint8_t flags, uint8_t flash_amount, int16_t bias_r,
                           int16_t bias_g, int16_t bias_b, uint16_t flash, uint16_t ink,
                           Look* look) {
  if ((flags & static_cast<uint8_t>(~kFlagsAssigned)) != 0) return false;
  if (!bias_fits(bias_r) || !bias_fits(bias_g) || !bias_fits(bias_b)) return false;
  look->bloom_gain = bloom_gain;
  look->grade_valid = (flags & kFlagGradeValid) != 0;
  look->echo_arm = (flags & kFlagEchoArm) != 0;
  look->bias_r = bias_r;
  look->bias_g = bias_g;
  look->bias_b = bias_b;
  look->flash = flash;
  look->flash_amount = flash_amount;
  look->ink = ink;
  return true;
}

// The three curves' table sizes: R[32] G[64] B[32] (RGB565's channel widths).
inline constexpr unsigned kCurveSize[3] = {32, 64, 32};
inline constexpr unsigned kEntriesPerRecord = 8;
inline constexpr unsigned kEntryBytes = 9;       // three signed 24-bit products

inline bool grade_record_ok(uint8_t curve, uint8_t first, uint8_t count) {
  if (curve > 2) return false;
  if (count < 1 || count > kEntriesPerRecord) return false;
  return unsigned(first) + unsigned(count) <= kCurveSize[curve];
}

// Entry k of a record's 72 vector bytes: {outR, outG, outB}, each s24 LE.
inline void grade_entry(const uint8_t* vectors, unsigned k, int32_t p[3]) {
  for (unsigned o = 0; o < 3; ++o) {
    const uint8_t* b = vectors + k * kEntryBytes + 3 * o;
    uint32_t u = uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16);
    if (u & 0x800000u) u |= 0xFF000000u;         // sign-extend 24 -> 32
    p[o] = static_cast<int32_t>(u);
  }
}

// POST.COMPOSITE's `pv_data_i` word, {outB, outG, outR}: returned as the low 64
// bits and the high 8, because a 72-bit value has no C++ scalar.
inline void pack_pv(const int32_t p[3], uint64_t* lo64, uint8_t* hi8) {
  const uint64_t r = uint64_t(uint32_t(p[0]) & 0xFFFFFFu);
  const uint64_t g = uint64_t(uint32_t(p[1]) & 0xFFFFFFu);
  const uint64_t b = uint64_t(uint32_t(p[2]) & 0xFFFFFFu);
  *lo64 = r | (g << 24) | (b << 48);
  *hi8 = static_cast<uint8_t>(b >> 16);
}

// THE GRADING-TABLE EMITTER. A curve set and a Q2.14 matrix become the sixteen
// SetGradeTable payloads that load the whole table, through
// `grade_product_vector` -- the ONE generator of the product vectors, never a
// second implementation. Nothing here rounds (see that function's note).
struct GradeRecord {
  uint8_t curve = 0, first = 0, count = 0;
  uint8_t vectors[kEntriesPerRecord * kEntryBytes] = {};
};
inline void put_s24(uint8_t* b, int32_t v) {
  const uint32_t u = uint32_t(v);
  b[0] = uint8_t(u);
  b[1] = uint8_t(u >> 8);
  b[2] = uint8_t(u >> 16);
}
template <typename Sink>
inline void emit_grade_table(const uint8_t curve_r[32], const uint8_t curve_g[64],
                             const uint8_t curve_b[32], const int16_t m[9], Sink&& sink) {
  const uint8_t* curves[3] = {curve_r, curve_g, curve_b};
  for (unsigned c = 0; c < 3; ++c) {
    for (unsigned first = 0; first < kCurveSize[c]; first += kEntriesPerRecord) {
      GradeRecord rec;
      rec.curve = uint8_t(c);
      rec.first = uint8_t(first);
      rec.count = uint8_t(kEntriesPerRecord);
      for (unsigned k = 0; k < kEntriesPerRecord; ++k) {
        int32_t p[3];
        grade_product_vector(m, int(c), curves[c][first + k], p);
        for (unsigned o = 0; o < 3; ++o) put_s24(rec.vectors + k * kEntryBytes + 3 * o, p[o]);
      }
      sink(rec);
    }
  }
}

}  // namespace look

// ===========================================================================
// THE TAG-TO-GATHER LAW -- PROPOSED, owner ruling R37 (2026-09-19)
// ===========================================================================
// R37: "This is an ART law. The packet proposes it from `stars_and_flares.md`
// §1 with every coefficient in a named, editable constant (CLAUDE.md rule 6)
// and a zref model, and renders a before/after for the OWNER TO JUDGE BY EYE."
// So this is a PROPOSAL with a model, not a ratified law, and nothing in
// `fpga/rtl` implements it yet. The render is
// `tools/post/gather_law_render.cpp` -> `tools/post/gather_law_sheet.py` ->
// reports/post-gather-law/gather_law_contact.png.
//
// WHAT IS GIVEN, AND IT IS ONE SENTENCE. `spec/stars_and_flares.md` §1 freezes
// the effect tag and nothing else:
//
//   "Effect-tag convention (frozen): tag = (channel << 6) | strength,
//    GLOW = 0b01, strength = source texel's CLUT intensity (0..63)"
//
// One channel is defined. POST.GATHER wants three things per fragment -- glow
// RGB, a signed 8.8 displacement pair and an ink bit -- and the tag carries an
// intensity. The gap between those is the art law, and the honest shape of it
// is narrow:
//
//   * THE GLOW'S COLOUR IS THE FRAGMENT'S OWN COLOUR. A star's halo is the
//     colour of the star. Nothing else in the tag could supply a hue without
//     inventing a second palette, and §1's whole thesis is that "intensity is
//     drawn, and a per-frame ARM-rebuilt palette colourises it" -- the
//     colourising has ALREADY happened by resolve, so the bloom borrows it
//     rather than re-deriving it.
//   * THE STRENGTH IS A KNEE, NOT A SCALE. Every texel of a CLUT page carries
//     some intensity; if all of them bloomed, the bloom would be a haze over
//     the whole image. `kGlowKnee` is where a texel starts to be a light
//     source, and it is the one constant the picture is most sensitive to.
//   * DISPLACEMENT AND INK HAVE NO PRODUCER IN v1, AND ARE NOT INVENTED HERE.
//     Channels 0b10 and 0b11 are unallocated in the spec; a fragment carrying
//     one contributes nothing and is COUNTED (`reserved_channel`), so the day
//     refraction is specified, the counter says whether anything was already
//     drawing it. POST.GATHER's `c_disp_*` and `c_ink_o` are therefore zero
//     under this law -- which is a statement, not an omission.
namespace gather {

// ---- THE KNOBS. Every one is an ART value; change them and re-render. -----
inline constexpr unsigned kChannelNone = 0;
inline constexpr unsigned kChannelGlow = 1;      // stars_and_flares.md §1, frozen
inline constexpr unsigned kChannelReserved2 = 2;
inline constexpr unsigned kChannelReserved3 = 3;

// Below this strength a texel is lit, not a light: it contributes no glow.
// 0..63 in the tag's own units. The picture is most sensitive to this one.
inline constexpr uint8_t kGlowKnee = 24;
// How fast a texel becomes a light above the knee, Q4.4 (0x10 = 1.0 per unit
// of strength). 63 - 24 = 39 units of headroom, so 0x1C saturates near 55.
inline constexpr uint8_t kGlowSlope = 0x1C;
// A per-channel weight on the borrowed colour, unit8 (255 = keep). The warm
// bias a CRT halo has: leave R alone, pull G and B back a little.
inline constexpr uint8_t kGlowTint[3] = {255, 236, 224};
// The whole law's master gain, unit8, applied last. This is the one the owner
// is most likely to want on a slider; `SetPost.bloom_gain` scales the RESULT
// again at composite stage 4, per frame.
inline constexpr uint8_t kGlowMaster = 255;

// unit8 multiply, qformats §2: round-half-up, saturating at 255.
inline uint8_t unit_mul(uint8_t a, uint8_t b) {
  const uint32_t p = (uint32_t(a) * uint32_t(b) + 128u) >> 8;
  return uint8_t(p > 255u ? 255u : p);
}
inline uint8_t exp5(uint8_t v5) { return uint8_t((v5 << 3) | (v5 >> 2)); }
inline uint8_t exp6(uint8_t v6) { return uint8_t((v6 << 2) | (v6 >> 4)); }

inline unsigned tag_channel(uint8_t tag) { return unsigned(tag >> 6); }
inline unsigned tag_strength(uint8_t tag) { return unsigned(tag & 0x3Fu); }

// The knee ramp: strength (0..63) -> unit8 gain. Integer, no host floats.
inline uint8_t glow_gain(unsigned strength) {
  if (strength <= kGlowKnee) return 0;
  const uint32_t g = ((strength - kGlowKnee) * uint32_t(kGlowSlope)) >> 4;
  return uint8_t(g > 255u ? 255u : g);
}

// What one resolved fragment contributes to POST.GATHER's per-fragment input.
struct Fragment {
  uint8_t glow_r = 0, glow_g = 0, glow_b = 0;
  int16_t disp_x = 0, disp_y = 0;   // signed 8.8; zero under this law
  bool ink = false;                 // no v1 producer under this law
  bool reserved_channel = false;    // an unallocated channel was carried
};

inline Fragment tag_to_fragment(uint8_t tag, uint16_t rgb565) {
  Fragment f;
  const unsigned ch = tag_channel(tag);
  if (ch == kChannelGlow) {
    const uint8_t gain = unit_mul(glow_gain(tag_strength(tag)), kGlowMaster);
    const uint8_t r = exp5(uint8_t((rgb565 >> 11) & 0x1Fu));
    const uint8_t g = exp6(uint8_t((rgb565 >> 5) & 0x3Fu));
    const uint8_t b = exp5(uint8_t(rgb565 & 0x1Fu));
    f.glow_r = unit_mul(unit_mul(r, gain), kGlowTint[0]);
    f.glow_g = unit_mul(unit_mul(g, gain), kGlowTint[1]);
    f.glow_b = unit_mul(unit_mul(b, gain), kGlowTint[2]);
  } else if (ch != kChannelNone) {
    f.reserved_channel = true;
  }
  return f;
}

}  // namespace gather

}  // namespace post
}  // namespace zref
