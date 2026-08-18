// zref_fragment.hpp — RASTER.FRAGMENT reference model (phase 4, ZH-025).
//
// The scalar oracle named by design/blocks.yml (`reference_model:
// zref::FragmentPipeline`) and design/contracts/RASTER.FRAGMENT.md.
//
// WHAT IT DELEGATES AND WHAT IT RESTATES. Like zref::TileStore and unlike
// zref::EdgeWalk, this is a second implementation of a CONTRACT rather than a
// view onto a pre-existing frozen function: no earlier code in this repository
// performs a depth/stencil/blend on a charter 8 tile word. It is therefore
// written in the plainest possible way, and everything it CAN delegate it
// does:
//
//   · the unit8 product is zref::unit_mul (spec/qformats.md 3, the one frozen
//     `((u32)a*b + 128) >> 8`) — not a local copy;
//   · the tile word is zref::TileStore::Word, unpacked and packed by that
//     struct, so field positions are defined in exactly one place;
//   · the lerp is spec/qformats.md 4's rescale_s(x, 8) written once, in
//     blend_channel, in the same shape as the ratified fog mix (qformats 8)
//     and global-tint mix (sky_and_beams 4a).
//
// What it does NOT share with the RTL is any structure: the RTL has two
// pipeline stages, a stall that re-issues its own read, and three
// zhao_raster_blend instances; this has one function and a `switch`. So
// "RTL == oracle" tests the RTL's mechanics rather than a shared clever idea.
//
// THE STATE WORD is the one thing this file and the RTL must agree on
// bit-for-bit, and it is defined in fpga/rtl/raster/zhao_raster_fragment.sv
// and design/contracts/RASTER.FRAGMENT.md. `State::pack` / `State::unpack`
// below are that layout transcribed, and the named recipe helpers are the six
// ratified recipes (spec/sky_and_beams.md 1.1/2, spec/stars_and_flares.md 1)
// spelled as state values so that a test says `recipe_sun_additive()` and not
// a magic constant.
//
// NOTE ON `state == 0`: it is the plain opaque write (depth test off, depth
// written, blend REPLACE, no alpha test, stencil ALWAYS + REPLACE, tag
// written). That is deliberate and load-bearing — it is what lets the phase-4
// tile pipe's flat-colour behaviour survive this block's arrival unchanged.

#pragma once

#include <cstdint>

#include "zref/zref_fixp.hpp"
#include "zref/zref_tilestore.hpp"

namespace zref {

/** Depth/stencil/blend for one covered fragment (the charter 4 exemplar). */
struct FragmentPipeline {
  using Word = TileStore::Word;

  // ---- the enumerations, mirroring zhao_raster_fragment.sv ---------------
  enum Blend : uint8_t { kReplace = 0, kAlpha = 1, kAdd = 2, kAddMod = 3 };
  enum StenFunc : uint8_t { kAlways = 0, kEqual = 1, kNotEqual = 2, kNever = 3 };
  enum StenOp : uint8_t { kOpReplace = 0, kOpKeep = 1, kOpIncrSat = 2, kOpDecrSat = 3 };

  /** spec/stars_and_flares.md 1: `tag = (channel << 6) | strength`, GLOW = 0b01. */
  static constexpr uint8_t kGlow = 0x1;
  /** spec/qformats.md 8: the depth clear value, i.e. the far constant. */
  static constexpr uint32_t kDepthFar = 0;

  /** The 32-bit fragment state word, unpacked. */
  struct State {
    bool z_test_en = false;        // [0]
    bool z_write_dis = false;      // [1]
    bool z_force_far = false;      // [2]
    uint8_t blend = kReplace;      // [4:3]
    bool shade_mod = false;        // [5]
    bool alpha_mod = false;        // [6]
    bool atest_en = false;         // [7]
    uint8_t atest_ref = 0;         // [15:8]
    uint8_t sten_func = kAlways;   // [17:16]
    uint8_t sten_op = kOpReplace;  // [19:18]
    bool tag_write_dis = false;    // [20]
    bool tag_from_texel = false;   // [21]
    uint8_t tag_channel = 0;       // [23:22]
    uint8_t sten_mask = 0;         // [31:24]

    uint32_t pack() const {
      return (static_cast<uint32_t>(z_test_en) << 0) | (static_cast<uint32_t>(z_write_dis) << 1) |
             (static_cast<uint32_t>(z_force_far) << 2) | (static_cast<uint32_t>(blend & 3u) << 3) |
             (static_cast<uint32_t>(shade_mod) << 5) | (static_cast<uint32_t>(alpha_mod) << 6) |
             (static_cast<uint32_t>(atest_en) << 7) | (static_cast<uint32_t>(atest_ref) << 8) |
             (static_cast<uint32_t>(sten_func & 3u) << 16) |
             (static_cast<uint32_t>(sten_op & 3u) << 18) |
             (static_cast<uint32_t>(tag_write_dis) << 20) |
             (static_cast<uint32_t>(tag_from_texel) << 21) |
             (static_cast<uint32_t>(tag_channel & 3u) << 22) |
             (static_cast<uint32_t>(sten_mask) << 24);
    }

    static State unpack(uint32_t s) {
      State o;
      o.z_test_en = (s >> 0) & 1u;
      o.z_write_dis = (s >> 1) & 1u;
      o.z_force_far = (s >> 2) & 1u;
      o.blend = static_cast<uint8_t>((s >> 3) & 3u);
      o.shade_mod = (s >> 5) & 1u;
      o.alpha_mod = (s >> 6) & 1u;
      o.atest_en = (s >> 7) & 1u;
      o.atest_ref = static_cast<uint8_t>(s >> 8);
      o.sten_func = static_cast<uint8_t>((s >> 16) & 3u);
      o.sten_op = static_cast<uint8_t>((s >> 18) & 3u);
      o.tag_write_dis = (s >> 20) & 1u;
      o.tag_from_texel = (s >> 21) & 1u;
      o.tag_channel = static_cast<uint8_t>((s >> 22) & 3u);
      o.sten_mask = static_cast<uint8_t>(s >> 24);
      return o;
    }
  };

  /** One shaded candidate as RASTER.EARLYZ hands it over. */
  struct Frag {
    uint8_t addr = 0;                // {row[3:0], col[3:0]}
    uint32_t depth = 0;              // invw24, larger is closer
    uint32_t state = 0;              // the packed state word
    uint8_t vr = 0, vg = 0, vb = 0;  // vertex RGB: lit, tinted and ALREADY FOGGED
    uint8_t va = 0;                  // vertex alpha (PART.SOFT's fade rides here)
    uint8_t tag = 0;                 // the constant-tag source
    uint8_t sten_ref = 0;            // stencil reference AND REPLACE value
    uint8_t tr = 0, tg = 0, tb = 0;  // the sampled texel (TEXTURE.TMU's, not built)
    uint8_t ta = 0;
    uint8_t tidx = 0;  // CLUT8 index; 0 is the masked one
  };

  /** What the block does with it. */
  struct Out {
    bool write = false;    // it survived all three tests
    uint64_t word = 0;     // the word written (meaningless when !write)
    bool blended = false;  // it wrote AND combined src with dst (the counter)
  };

  /**
   * spec/qformats.md 4's `rescale_s(x, 8)` = `(x + 128) >> 8` with an
   * ARITHMETIC shift. Written here once, because the sign matters: ties round
   * toward +infinity, so an exact half in a darkening lerp rounds toward zero.
   * Rescaling the magnitude unsigned and re-applying the sign would round
   * those ties the other way and differ by one LSB.
   */
  static int32_t rescale_s8(int32_t x) { return (x + 128) >> 8; }

  /**
   * One blend channel — the mirror of fpga/rtl/raster/zhao_raster_blend.sv.
   *   REPLACE  src
   *   ALPHA    sat_u8(dst + rescale_s((src - dst) * a, 8))  = dst*(1-a)+src*a
   *   ADD      sat_u8(dst + src)
   *   ADD_MOD  sat_u8(dst + unit_mul(src, a))
   * `a` is a unit8: its value is a/256, so a = 255 is 255/256 and NOT 1.0.
   */
  static uint8_t blend_channel(uint8_t mode, uint8_t dst, uint8_t src, uint8_t a);

  /** Apply the whole block to one fragment against one destination word. */
  static Out apply(const Frag& f, uint64_t dst_word);

  // ---- the six ratified recipes, by name --------------------------------
  // spec/sky_and_beams.md 1.1 and 2; spec/stars_and_flares.md 1. Each is a
  // State value and nothing more; the datapath has no per-recipe mode.
  static State sky_backdrop();
  static State sky_cloud_fade();
  static State sun_additive();
  static State beam_additive_fade();
  static State star_disc_masked();
  static State star_halo_additive();
};

}  // namespace zref
