// zref_texture.hpp — TEXTURE.CACHE and TEXTURE.TMU reference models
// (phase 5, ZH-061 and ZH-027).
//
// The two scalar oracles named by design/blocks.yml (`reference_model:
// zref::TextureCache` and `zref::Tmu`) and by design/contracts/
// TEXTURE.CACHE.md / TEXTURE.TMU.md.
//
// WHAT THEY DELEGATE AND WHAT THEY RESTATE. Like zref::TileStore and unlike
// zref::EdgeWalk, these are second implementations of CONTRACTS rather than
// views onto pre-existing frozen functions: no earlier code in this
// repository maintains a tag array or filters a texel. They are therefore
// written in the plainest way that can be checked by eye, and everything they
// CAN delegate they do:
//
//   * the RGB565 -> RGB888 expansion is zref::sky::rgb565::to_rgb888, the
//     frozen `c8 = (c5<<3)|(c5>>2)` / `(c6<<2)|(c6>>4)` law of
//     spec/stars_and_flares.md 2 -- not a local copy;
//   * the mirrored-repeat wrap is the generalisation of
//     zref::terrain::mirror_texel (spec/terrain_rules.md 6.2, frozen), and
//     tests/texture/texture_tmu_directed.cpp asserts the two agree
//     texel-for-texel on the 64-wide case the frozen helper covers, rather
//     than trusting the generalisation;
//   * the mip LEVEL OFFSET is a plain summation loop here, while the RTL uses
//     a base-4-repunit closed form -- so "RTL == oracle" is a real check of
//     that identity and not a shared clever expression.
//
// What they do NOT share with the RTL is any structure. The cache model has
// no response stage, no fill FSM and no halfword beats; the TMU model has no
// state machine, no cache handshake and no registered mode word. So
// "RTL == oracle" tests the RTL's mechanics rather than a shared idea.
//
// THE BILINEAR FILTER IS A DERIVED LAW, NOT A CITED ONE. No spec in this
// repository defines bilinear weights or their rounding; the derivation from
// spec/qformats.md 2 (unit8 = raw/256), 3 (the single-rounding law) and 4
// (rescale_u) is written out in fpga/rtl/texture/zhao_texture_bilerp.sv and
// recorded in design/contracts/TEXTURE.TMU.md. `Tmu::bilerp` below is that
// derivation and the RTL module is its mirror.
//
// THE MODE WORD is the one thing these files and the RTL must agree on
// bit-for-bit, and it is defined in fpga/rtl/texture/zhao_texture_tmu.sv and
// design/contracts/TEXTURE.TMU.md. `Mode::pack` / `Mode::unpack` are that
// layout transcribed, and nothing else re-derives a bit position.
//
// NOTE ON `mode == 0`: it is `CLUT8, nearest, repeat/repeat, level 0` -- a
// 1x1-wrap 8-bit paletted point sample, this machine's most common texel and
// the first format in charter 15's implementation order.

#pragma once

#include <cstdint>
#include <vector>

namespace zref {

/**
 * The flat backing store a cache fill reads from. Stands in for the VRAM
 * texture pool (charter 7.1) without modelling MEM.GUARD: the guard's region
 * check is its own block's law, and TEXTURE.CACHE only ever emits an address.
 */
struct TextureMemory {
  uint32_t base = 0;           // byte address of bytes[0]
  std::vector<uint8_t> bytes;  // the pool

  /** One byte; 0 outside the modelled range (an unmapped read is not an error here). */
  uint8_t byte_at(uint32_t addr) const;
  /** The LITTLE-ENDIAN halfword at `addr & ~1` -- exactly what a cache lane returns. */
  uint16_t halfword(uint32_t addr) const;
};

/** M10K-backed texture / palette / material cache with per-lane tag checks. */
struct TextureCache {
  /** One lane per bilinear tap; see the RTL's FOUR LANES section. */
  static constexpr int kLanes = 4;
  /** Direct-mapped lines per lane, and the line size. CHOSEN, not cited. */
  static constexpr int kLines = 16;
  static constexpr int kLineBytes = 16;

  /** One access: up to kLanes independent lookups resolved together. */
  struct Access {
    bool en[kLanes] = {};
    uint32_t addr[kLanes] = {};
    uint16_t src_id = 0;
  };

  /** What the access produced, plus the evidence the tests compare. */
  struct Out {
    uint16_t data[kLanes] = {};
    uint8_t first_hits = 0;           // enabled lanes resident at the FIRST look
    uint8_t fills = 0;                // lines fetched to serve this access
    uint32_t fill_addr[kLanes] = {};  // line addresses, in issue order (lowest lane first)
  };

  TextureCache() { reset(); }

  /** Power-on / reset: every line invalid, both counters zero. */
  void reset();

  /** The stars 1 resource-epoch flush: every valid bit in every lane. */
  void invalidate_all();

  /** The stars 1 palette-page invalidate: the line containing `addr`, in every lane. */
  void invalidate_line(uint32_t addr);

  /** Serve one access, filling whatever is missing from `mem` first. */
  Out access(const Access& a, const TextureMemory& mem);

  /** True iff lane `lane` currently holds the line containing `addr`. */
  bool resident(int lane, uint32_t addr) const;

  uint32_t cache_hits() const { return hits_; }
  uint32_t cache_misses() const { return misses_; }

 private:
  bool valid_[kLanes][kLines] = {};
  uint32_t tag_[kLanes][kLines] = {};
  uint8_t data_[kLanes][kLines][kLineBytes] = {};
  uint32_t hits_ = 0;
  uint32_t misses_ = 0;
};

/** The one primary texture unit (charter 26 forbids a second unrestricted one). */
struct Tmu {
  /** charter 15's format order. 5..7 are undefined and raise mode_error. */
  enum Format : uint8_t { kClut8 = 0, kRgb565 = 1, kClut4 = 2, kArgb1555 = 3, kArgb4444 = 4 };
  /** charter 15 "wrap/clamp/mirror"; 3 is reserved and behaves as repeat. */
  enum Wrap : uint8_t { kRepeat = 0, kClamp = 1, kMirror = 2 };

  /** The 32-bit sampler mode word, unpacked. */
  struct Mode {
    uint8_t fmt = kClut8;      // [2:0]
    bool bilinear = false;     // [3]
    uint8_t wrap_u = kRepeat;  // [5:4]
    uint8_t wrap_v = kRepeat;  // [7:6]
    uint8_t log2w = 0;         // [11:8]
    uint8_t log2h = 0;         // [15:12]
    uint8_t max_level = 0;     // [19:16]
    bool mip_en = false;       // [20]
    uint16_t rsvd = 0;         // [31:21], must be zero

    uint32_t pack() const;
    static Mode unpack(uint32_t w);
  };

  /** One texture request as RASTER.FRAGMENT presents it. */
  struct Req {
    int32_t u = 0;  // S 15.16 texture units, 1.0 = one full wrap
    int32_t v = 0;
    uint32_t base = 0;      // byte address of level 0
    uint32_t pal_base = 0;  // byte address of the CLUT page (RGB565 entries)
    uint32_t mode = 0;      // the packed mode word
    uint8_t lod = 0;        // U 4.4 (charter 9's Measure, computed upstream)
    uint16_t src_id = 0;
  };

  /**
   * The addressing a request implies, before any memory is touched. Exposed
   * so the directed tests can pin the address plan (mip offsets, wrap folds,
   * lane assignment) without needing a texture in memory to look at.
   */
  struct Plan {
    uint8_t level = 0;
    bool bilinear = false;  // AFTER the stars 1 palette override
    bool clut = false;
    uint8_t lanes = 1;  // enabled cache lanes: 4 for bilinear, else 1
    uint32_t addr[4] = {};
    uint8_t fu = 0;
    uint8_t fv = 0;
    bool byte_sel = false;  // addr[0] bit 0 -- which byte of the halfword
    bool nibble = false;    // CLUT4: which nibble of that byte
    bool mode_error = false;
  };

  /** The sample handed back -- RASTER.FRAGMENT's three texel fields. */
  struct Sample {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
    uint8_t idx = 0;  // CLUT index; 0 for direct colour (there is none)
    bool mode_error = false;
    uint16_t src_id = 0;
  };

  /**
   * charter 15's three wrap modes on one axis. `mask` is size-1 and size is a
   * power of two, so REPEAT is a mask (two's complement AND is the FLOORED
   * modulo) and MIRROR is spec/terrain_rules.md 6.2's frozen fold generalised:
   * per = floored t mod 2S, texel = per < S ? per : 2S-1-per.
   */
  static uint32_t wrap(int32_t t, uint8_t mode, uint32_t mask);

  /**
   * Texel offset of level `level` in a mip chain whose level 0 is
   * (1<<log2w) x (1<<log2h) -- written as the plain summation the RTL's
   * base-4-repunit closed form has to equal.
   */
  static uint32_t level_offset_texels(uint8_t log2w, uint8_t log2h, uint8_t level);

  /**
   * The derived bilinear law (see the file header and
   * fpga/rtl/texture/zhao_texture_bilerp.sv):
   *   w00 = (256-fu)(256-fv), w10 = fu(256-fv), w01 = (256-fu)fv, w11 = fu*fv
   *   out = (t00*w00 + t10*w10 + t01*w01 + t11*w11 + 32768) >> 16
   * One exact wide sum, ONE rescale (spec/qformats.md 3 and 4).
   */
  static uint8_t bilerp(uint8_t t00, uint8_t t10, uint8_t t01, uint8_t t11, uint8_t fu, uint8_t fv);

  /** Everything the request determines before a texel is read. */
  static Plan plan(const Req& r);

  /** The whole block against a flat memory (the always-resident cache). */
  static Sample sample(const Req& r, const TextureMemory& mem);
};

}  // namespace zref
