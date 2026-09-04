// zref_particle.hpp — PART.EXPAND's reference model.
//
// ---------------------------------------------------------------------------
// SUPERSEDED ASSUMPTION — READ BEFORE TRUSTING `size` (2026-09-03)
// ---------------------------------------------------------------------------
// Everything below reads particle size as **U 0.4.4 PIXELS, an 8-bit byte of
// sixteenths of a pixel**, and cites qformats §10 for it. That citation was
// true when this block was written and is NOT true now.
//
// Amendment C2 (QFMT_VERSION 3, ruling R3) replaced §10 whole. Size is:
//
//     size u6 -- U 2.4 RELATIVE RADIUS MULTIPLIER
//     radius = base_radius_fx16 * size / 16, ONE final round-half-up
//     WORLD SCALE, NEVER CAMERA-SPACE PIXELS
//
// Three differences, each of which changes the picture:
//   * SIX bits, not eight;
//   * a MULTIPLIER on the species' base radius, not a size;
//   * a WORLD length, not a screen length -- so it must be projected, and
//     `size << 4` is not a projection.
//
// THIS BLOCK IS NOT FIXED HERE, deliberately. Turning a world-space radius into
// a screen-space half-side is a projection, and inventing one to make an
// amendment fit would be exactly the kind of plausible wrong number this
// project has shipped before. It needs the same treatment GEOM.PROJECT's
// attribute carry got: a decision, then an implementation.
//
// What IS fixed here is that the next reader is told. The amendment was
// committed and nothing pointed at the block that had already believed the old
// text -- "instructions are not delivered until they are read", in a new place.
// ---------------------------------------------------------------------------
//
// The ledger declared `zref::ParticleExpand` and that symbol never existed: one
// of the twenty-five phantoms in reports/PHANTOM_REFERENCES.md. This one is
// KIND 1 — the law was already shipped, inline, inside a function that does more
// than this block does.
//
// `zref::render::draw_population` (reference/src/zrender/sprites.cpp) projects
// each particle and then, on its `tris` branch, expands it into a three-vertex
// screen-space fan and rasterises it. THIS block is that expansion and nothing
// else: the projection is GEOM.PROJECT's and the rasterisation is GEOM.SETUP's
// and RASTER's.
//
// ---------------------------------------------------------------------------
// EXTRACTING AN INLINE LAW IS THE RISKY KIND OF REFERENCE, SO IT IS CHECKED
// ---------------------------------------------------------------------------
// The forty `zref::fieldir::*` names could be forwarded to a real callable. This
// one cannot: `draw_population` computes the fan inside a loop body and then
// immediately rasterises it, so there is no function to forward to and the law
// has to be RESTATED here.
//
// A restated law is a second implementation, and a second implementation that
// only its author ever compares against the first is exactly the failure the
// phantom-reference rules exist to catch. So
// `tests/particles/part_expand_directed.cpp` does not merely check RTL against
// this header — it also renders the same particles BOTH ways, once through
// `draw_population` and once by feeding this function's vertices to the same
// `raster_tri`, and requires the two surfaces to be identical pixel for pixel.
// If this header ever drifts from sprites.cpp, that check fails.
//
// ---------------------------------------------------------------------------
// THE LAW, verbatim from the `tris` branch
// ---------------------------------------------------------------------------
//     side_sub = size << 4          // U 0.4.4 px -> S 12.8 subpixels
//     a  = { x,                    y - side_sub    }
//     b  = { x - (side_sub*3)/4,   y + side_sub/2  }
//     c  = { x + (side_sub*3)/4,   y + side_sub/2  }
//
// All three carry the particle's own depth `d` and its colour. Three things
// about it are load-bearing and none of them is obvious:
//
//   1. THE FAN IS NOT EQUILATERAL AND MUST NOT BE "CORRECTED". The half-width is
//      3/4 of a side and the drop is half a side. Making it a true equilateral
//      triangle would be better geometry and would change every particle on
//      screen.
//      (Both divisions are EXACT for every legal input, which is not obvious:
//      `side_sub` is `size << 4`, hence a multiple of 16, so neither the /4 nor
//      the /2 ever discards anything. A mutation that rounded instead of
//      truncating survives the whole suite because no input distinguishes them
//      -- an equivalent mutant, recorded so the next reader does not go looking
//      for the missing test.)
//   2. `side_sub` IS `size << 4`, NOT `size << 8`. Particle size is U 0.4.4
//      pixels (qformats §10), so the raw byte is sixteenths of a pixel; shifting
//      by 4 lands it in S 12.8 subpixels. Shifting by 8 would treat it as whole
//      pixels and make every particle sixteen times too big.
//   3. DEPTH IS TESTED, NEVER WRITTEN. `draw_population` sets
//      `depth_write = false` on its TriMode, with the comment "pass-7 law: test
//      only, no write". Particles occlude nothing behind them; a particle that
//      wrote depth would carve a hole in whatever drew after it.
//
// A particle BEHIND THE EYE is skipped entirely (`if (!c.in) continue`), which
// is why `expand_polygon` reports whether it produced anything rather than
// emitting a degenerate triangle.
#pragma once

#include <cstdint>

namespace zref {
namespace part {

/** One expanded screen-space vertex. Mirrors the fields `raster_tri` reads. */
struct ExpandedVertex {
  int32_t x = 0;  // S 12.8 canvas subpixels
  int32_t y = 0;
  int32_t d = 0;  // Q16.16 1/w, the particle's own depth
};

/** What one particle expands into. */
struct PolyExpand {
  bool emitted = false;  // false: the particle was behind the eye
  ExpandedVertex a, b, c;
  uint8_t r = 0, g = 0, b_ = 0;
  // The TriMode law: opaque fill, depth TESTED, depth NOT written.
  static constexpr bool kDepthTest = true;
  static constexpr bool kDepthWrite = false;
};

/**
 * Expand one PROJECTED particle into its three-vertex fan.
 *
 * `sx`/`sy`/`sd` are the projection's screen vertex; `in` is its
 * behind-the-eye verdict. Projection belongs to GEOM.PROJECT and is not redone
 * here — this function is the expansion alone.
 */
inline PolyExpand expand_polygon(bool in, int32_t sx, int32_t sy, int32_t sd, uint8_t size,
                                 uint8_t r, uint8_t g, uint8_t b) {
  PolyExpand o;
  if (!in) return o;  // skipped, exactly as draw_population `continue`s
  const int32_t side_sub = static_cast<int32_t>(size) << 4;
  o.emitted = true;
  o.a = ExpandedVertex{sx, sy - side_sub, sd};
  o.b = ExpandedVertex{sx - (side_sub * 3) / 4, sy + side_sub / 2, sd};
  o.c = ExpandedVertex{sx + (side_sub * 3) / 4, sy + side_sub / 2, sd};
  o.r = r;
  o.g = g;
  o.b_ = b;
  return o;
}

// ---- particle128 v1 codec (qformats §10, amendment C2 / ruling R3) --------
//
//   bits   0..17   position X        18..35  position Y     36..53  position Z
//         54..64   velocity X        65..75  velocity Y     76..86  velocity Z
//         87..96   age               97..103 species       104..109 size
//        110..115  spin             116..119 flags         120..127 variation
//
// This is the ORACLE for `zhao_part_record`. Written from the spec section,
// not from the Verilog -- a codec verified against a transcription of itself
// proves only that two copies agree.
inline constexpr uint32_t kParticleFormatVersion = 1;

// flags
inline constexpr uint8_t kPartStuck = 0x1;
inline constexpr uint8_t kPartCollidedThisTick = 0x2;
inline constexpr uint8_t kPartBornThisTick = 0x4;
inline constexpr uint8_t kPartFlagReserved = 0x8;  // zero in, preserved zero

struct Particle128 {
  int32_t pos[3];     // s18, S 9.8 m relative to the population origin
  int32_t vel[3];     // s11, S 2.8 m/tick
  uint16_t age;       // u10, whole 60 Hz ticks
  uint8_t species;    // u7
  uint8_t size;       // u6, U 2.4 relative radius multiplier
  uint8_t spin;       // u6, U 0.6 turns
  uint8_t flags;      // 4 bits
  uint8_t variation;  // u8, a stateless deterministic code -- NOT PRNG state
};

inline int32_t sign_extend(uint32_t v, int bits) {
  const uint32_t m = 1u << (bits - 1);
  return static_cast<int32_t>((v ^ m) - m);
}

inline void particle_unpack(uint64_t lo, uint64_t hi, Particle128* p) {
  auto fld = [&](int off, int w) -> uint32_t {
    if (off + w <= 64) return static_cast<uint32_t>((lo >> off) & ((1ull << w) - 1));
    if (off >= 64) return static_cast<uint32_t>((hi >> (off - 64)) & ((1ull << w) - 1));
    const int lowbits = 64 - off;
    const uint64_t a = (lo >> off) & ((1ull << lowbits) - 1);
    const uint64_t b = hi & ((1ull << (w - lowbits)) - 1);
    return static_cast<uint32_t>(a | (b << lowbits));
  };
  for (int i = 0; i < 3; ++i) p->pos[i] = sign_extend(fld(i * 18, 18), 18);
  for (int i = 0; i < 3; ++i) p->vel[i] = sign_extend(fld(54 + i * 11, 11), 11);
  p->age = static_cast<uint16_t>(fld(87, 10));
  p->species = static_cast<uint8_t>(fld(97, 7));
  p->size = static_cast<uint8_t>(fld(104, 6));
  p->spin = static_cast<uint8_t>(fld(110, 6));
  p->flags = static_cast<uint8_t>(fld(116, 4));
  p->variation = static_cast<uint8_t>(fld(120, 8));
}

inline void particle_pack(const Particle128& p, uint64_t* lo, uint64_t* hi) {
  *lo = 0;
  *hi = 0;
  auto put = [&](int off, int w, uint32_t v) {
    const uint64_t m = (w == 64) ? ~0ull : ((1ull << w) - 1);
    const uint64_t val = static_cast<uint64_t>(v) & m;
    if (off + w <= 64) {
      *lo |= val << off;
      return;
    }
    if (off >= 64) {
      *hi |= val << (off - 64);
      return;
    }
    const int lowbits = 64 - off;
    *lo |= (val & ((1ull << lowbits) - 1)) << off;
    *hi |= val >> lowbits;
  };
  for (int i = 0; i < 3; ++i) put(i * 18, 18, static_cast<uint32_t>(p.pos[i]));
  for (int i = 0; i < 3; ++i) put(54 + i * 11, 11, static_cast<uint32_t>(p.vel[i]));
  put(87, 10, p.age);
  put(97, 7, p.species);
  put(104, 6, p.size);
  put(110, 6, p.spin);
  put(116, 4, p.flags);
  put(120, 8, p.variation);
}

// radius = base_radius_fx16 * size / 16, ONE final round-half-up.
// One rounding, on the whole product -- not a shift of a rounded multiply.
inline int32_t particle_radius(int32_t base_radius_fx16, uint8_t size) {
  const int64_t prod = static_cast<int64_t>(base_radius_fx16) * size;
  return static_cast<int32_t>((prod + 8) >> 4);
}

// angle16 = spin << 10. The stored phase wraps mod 64.
inline uint16_t particle_angle16(uint8_t spin) {
  return static_cast<uint16_t>((spin & 0x3Fu) << 10);
}

// ---- the representation ladder (owner ruling 2026-08-31 §2.5) -------------
//
//     meshlet -> triangle/shard -> ribbon/streak -> soft sprite -> glint
//             -> culled
//
// The bands OVERLAP on purpose -- triangle is ~6-18 px and soft sprite ~2-8 --
// so a 7 px particle satisfies both. The ladder is an ORDER, and the
// resolution is the first rung that fits, walking coarse to fine.
//
// This is the ORACLE for `zhao_part_ladder`.
enum Rung : uint8_t { kMeshlet = 0, kShard = 1, kRibbon = 2, kSprite = 3, kGlint = 4, kCulled = 5 };

// Sizes are U 8.8 pixels, so 1.0 px is 256.
inline constexpr uint16_t kMeshletMin = 18 * 256;
inline constexpr uint16_t kShardMin = 6 * 256;
inline constexpr uint16_t kRibbonMin = 4 * 256;
inline constexpr uint16_t kSpriteMin = 2 * 256;
inline constexpr uint16_t kGlintMin = 128;
inline constexpr int kHoldFrames = 3;

inline uint8_t ladder_raw(uint16_t size, uint16_t trail, bool narrow) {
  if (size >= kMeshletMin) return kMeshlet;
  if (size >= kShardMin) return kShard;
  if (narrow && trail >= kRibbonMin) return kRibbon;
  if (size >= kSpriteMin) return kSprite;
  if (size >= kGlintMin) return kGlint;
  return kCulled;
}

// The governor is a FLOOR on coarseness -- it may force a particle coarser and
// never finer. Protection is applied AFTER it, so a governor cannot cull a
// protected particle: otherwise the species flag would be a suggestion.
inline uint8_t ladder_want(uint16_t size, uint16_t trail, bool narrow, bool protected_,
                           uint8_t gov_floor) {
  uint8_t r = ladder_raw(size, trail, narrow);
  if (gov_floor > r) r = gov_floor;
  if (protected_ && r == kCulled) r = kGlint;
  return r;
}

struct LadderOut {
  uint8_t rung;
  uint8_t hold;
  bool changed;
};

inline LadderOut ladder_step(uint8_t want, uint8_t prev_rung, uint8_t hold, bool first) {
  if (first) return LadderOut{want, 0, true};
  if (want == prev_rung) return LadderOut{prev_rung, 0, false};
  if (static_cast<int>(hold) + 1 >= kHoldFrames) return LadderOut{want, 0, true};
  return LadderOut{prev_rung, static_cast<uint8_t>(hold + 1), false};
}

}  // namespace part
}  // namespace zref
