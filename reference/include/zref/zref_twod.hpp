// zref_twod.hpp — the 2D sprite law (owner ruling 2026-08-31 §3.2).
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// TWOD.SPRITE.md cited `zref::HudSprites` as its scalar reference and no such
// symbol had ever been written — the fifth phantom citation hit in a row while
// building blocks today, and the reason `reports/PHANTOM-CITATIONS-AUDIT.md`
// exists. The right answer to a phantom is to write the symbol.
//
// ---------------------------------------------------------------------------
// WHAT THE LAW IS, AND WHAT IT DELIBERATELY IS NOT
// ---------------------------------------------------------------------------
// The ruling is mostly deletions: no private HUD sampler, no text rasterizer,
// no glyph cache. Text is glyph sprites, windows are tiled sprites, cursors
// are sprites, and the game authors layout in software.
//
// So there is no font metric here, no kerning, no line breaking. What is left
// is an affine map from a pixel inside a sprite to a UV, and the rule about
// which descriptors are walked at all.
#pragma once

#include <cstdint>

namespace zref {
namespace twod {

// UV at pixel (px, py) of a descriptor. This is the CLOSED FORM; the hardware
// steps it with adds, and the two must agree exactly at every pixel.
//
// Stepping it the serpentine way — carrying the running coordinate from the
// last pixel of one row into the first of the next — is also just adds, also
// correct on a 1xN sprite, and wrong for every sprite wider than one pixel.
// The cross terms a01 and a10 are what tell the two apart.
inline int32_t sprite_u(int32_t u0, int32_t a00, int32_t a01, int px, int py) {
  return u0 + a00 * px + a01 * py;
}

inline int32_t sprite_v(int32_t v0, int32_t a10, int32_t a11, int px, int py) {
  return v0 + a10 * px + a11 * py;
}

// A descriptor with no area is REFUSED, not walked for zero pixels: the two
// are indistinguishable downstream and only one of them is a caller bug.
inline bool sprite_degenerate(uint16_t w, uint16_t h) { return w == 0 || h == 0; }

// The two player HUD regions are a compositing concern. Here they are a mask,
// and a descriptor that does not name this view is skipped — which is normal,
// and is counted separately from a refusal, which is not.
inline bool sprite_for_view(uint8_t view_mask, uint8_t view_sel) {
  return (view_mask & view_sel) != 0;
}

// ---- the plane engine (owner ruling 2026-08-31 §3.1, roles by R4) ---------
//
// Two slots, ONE engine, and a feature list that is a ceiling: CLUT8/RGB565,
// NEAREST only, affine, line scroll, repeat and clamp, view masks.
//
// Nearest is why there is no fractional output here either: the texel a
// coordinate falls in IS the texel.
inline constexpr uint8_t kRoleBackdrop = 0;
inline constexpr uint8_t kRoleAtmosphere = 1;
inline constexpr uint8_t kBlendReplace = 0;

// Roles 2 and 3 are reserved and the descriptor is refused. A BACKDROP sits
// beneath the resolved world, so an alpha-blended one has nothing under it to
// blend with -- malformed rather than merely odd.
inline bool plane_role_legal(uint8_t role) {
  return role == kRoleBackdrop || role == kRoleAtmosphere;
}
inline bool plane_blend_legal(uint8_t role, uint8_t blend) {
  return role != kRoleBackdrop || blend == kBlendReplace;
}

// u = u0 + a*x + b*y + line_scroll, in fx16; the texel is the integer part.
inline int32_t plane_u(int32_t u0, int32_t a, int32_t b, int32_t line_scroll, int x, int y) {
  const int64_t f = static_cast<int64_t>(u0) + static_cast<int64_t>(a) * x +
                    static_cast<int64_t>(b) * y + line_scroll;
  return static_cast<int32_t>(f >> 16);
}

inline int32_t plane_v(int32_t v0, int32_t c, int32_t d, int x, int y) {
  const int64_t f =
      static_cast<int64_t>(v0) + static_cast<int64_t>(c) * x + static_cast<int64_t>(d) * y;
  return static_cast<int32_t>(f >> 16);
}

// REPEAT by ONE conditional correction -- no divider. Exact while the
// coordinate is out of range by at most one size; beyond that the caller has
// asked for a step larger than the plane, and `failed` says so rather than the
// picture tiling wrongly.
inline uint16_t plane_wrap(int32_t t, uint16_t size, bool clamp_mode, bool* failed) {
  if (failed != nullptr) *failed = false;
  const int32_t sz = static_cast<int32_t>(size);
  if (clamp_mode) return static_cast<uint16_t>(t < 0 ? 0 : (t >= sz ? size - 1 : t));
  int32_t r = t;
  if (r < 0)
    r += sz;
  else if (r >= sz)
    r -= sz;
  if (r < 0 || r >= sz) {
    if (failed != nullptr) *failed = true;
    return 0;
  }
  return static_cast<uint16_t>(r);
}

// ---------------------------------------------------------------------------
// TWOD.ASSET's verdict (owner completion ruling 2026-09-22, item 3)
// ---------------------------------------------------------------------------
// The four clauses `zhao_twod_asset` judges a PublishResource of cartridge kind
// 15 by, and the destination map of `spec/cartridge.md` 4f. Every clause is an
// EXISTING PublishResource law; what this adds is where the bytes land.
//
// THE ORDER IS FAIL-SAFE AND IT IS THE RECORD'S OWN -- destination, extent,
// source, epoch -- so a refusal reads back to the FIELD that caused it rather
// than to "the loader said no". The RTL evaluates the same clauses in the same
// order and each increments one counter; a differential test that only asked
// "was it refused" could not see the two swapping.
enum class AssetVerdict : uint8_t {
  kAccept = 0,
  kSlotRefused,     // dst_slot outside the map
  kLenRefused,      // zero, not a multiple of 64, or past the slot
  kAddrRefused,     // above 4 GiB, or not 64-byte aligned
  kEpochRefused,    // a closed epoch
};

struct AssetDest {
  bool     is_palette = false;
  uint32_t first_word = 0;   // in the page store, or in the palette array
  uint32_t capacity   = 0;   // words the destination region holds
};

// `page_words` and `pal_slots` are the SAMPLER's parameters, and `page_slots`
// the map's. Give them the same numbers the RTL is elaborated with or the two
// describe different machines.
inline AssetDest asset_dest(uint8_t dst_slot, uint32_t page_words,
                            uint32_t pal_slots, uint32_t page_slots) {
  AssetDest d;
  if (dst_slot >= 16) {
    d.is_palette = true;
    const uint32_t idx = static_cast<uint32_t>(dst_slot) - 16u;
    d.capacity   = (idx < pal_slots) ? 256u : 0u;
    d.first_word = idx * 256u;
  } else {
    const uint32_t per = page_words / page_slots;
    d.capacity   = (dst_slot < page_slots) ? per : 0u;
    d.first_word = static_cast<uint32_t>(dst_slot) * per;
  }
  return d;
}

inline AssetVerdict asset_verdict(uint8_t dst_slot, uint32_t length_bytes,
                                  uint64_t hps_addr, uint16_t epoch,
                                  uint16_t open_epoch, uint32_t page_words,
                                  uint32_t pal_slots, uint32_t page_slots) {
  const bool is_pal = (dst_slot >= 16);
  const uint32_t idx = is_pal ? (static_cast<uint32_t>(dst_slot) - 16u)
                              : static_cast<uint32_t>(dst_slot);
  const bool dst_ok = is_pal ? (idx < pal_slots) : (idx < page_slots);
  if (!dst_ok) return AssetVerdict::kSlotRefused;

  const AssetDest d = asset_dest(dst_slot, page_words, pal_slots, page_slots);
  const uint32_t words = length_bytes >> 1;
  // A multiple of 64 is a REFUSAL, never a pad -- PublishResource's own law.
  // The capacity test is `<=` because a transfer exactly filling its slot is
  // legal and one word more is not; that boundary is where an off-by-one
  // would put the next slot's first word under this transfer's control.
  if (length_bytes == 0u || (length_bytes & 63u) != 0u || words > d.capacity)
    return AssetVerdict::kLenRefused;

  // MEM.UPLOAD's kUploadSourceUnreachable: a source above 4 GiB is REFUSED
  // rather than narrowed, because narrowing it on the wire is what the 64-bit
  // field exists to prevent. The 64-byte alignment is the bridge's burst.
  if ((hps_addr >> 32) != 0u || (hps_addr & 63u) != 0u)
    return AssetVerdict::kAddrRefused;

  if (epoch != open_epoch) return AssetVerdict::kEpochRefused;
  return AssetVerdict::kAccept;
}

}  // namespace twod
}  // namespace zref
