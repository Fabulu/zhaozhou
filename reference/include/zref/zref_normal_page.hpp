// zref_normal_page.hpp -- the DETAIL_NORMAL page (kind 16), frozen.
//
// Owner ruling R243 D-NORMALS-A (2026-09-23): "In v1 -- commission the
// pyramid." `zhao_terrain_normalmap` has held a seven-level signed detail
// pyramid and a write-only upload port since 2026-09-09 and has been
// instantiated NOWHERE, because nothing in the tree could put bytes in it.
// This header is the page that can, and `zhao_terrain_normalloader`
// (fpga/rtl/terrain/zhao_terrain_normalloader.sv) reads exactly it.
//
// `spec/cartridge.md` 4g states the same layout normatively;
// `tools/pack/mkdetailnormal.py` emits it; `tests/golden/detail_normal/
// detail_normal_v1.bin` is the one artefact all three are pinned to. That
// three-way discipline is `tools/pack/mkcreatureladder.py`'s, copied rather
// than re-invented, and it exists because a layout stated in three places
// drifts in two of them otherwise.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE HAS A BUILDER AND zref_terrain_normalmap.hpp DOES NOT
// ---------------------------------------------------------------------------
// The oracle already had the pyramid's ADDRESSER --
// `zref::terrain::normalmap_pyramid_addr` -- and did NOT have its BUILDER. So
// an asset tool that reduced a 64x64 detail tile to the mip tail would have
// been the first and only statement of the reduction law, which is precisely
// the duplication `tools/budget/uncashed_cheques.py` check 3 exists to catch:
// a law authored in a tool is a law no test differences.
//
// `build_pyramid` below is that law, once. The packer calls it through the
// golden; the RTL never needs it, because the RTL only ADDRESSES the pyramid
// (`zhao_terrain_normalmap.sv`, "this block only addresses it").
//
// ---------------------------------------------------------------------------
// NEVER NORMALISE. THIS IS THE WHOLE POINT OF THE REDUCTION.
// ---------------------------------------------------------------------------
// `zhao_terrain_normalmap.sv` and the mipmapping addendum
// (reports/zhaozhou-terrain-mipmapping-architecture-2026-09-05.txt 4) both say
// it in the same words: the pyramid is built by averaging SIGNED dx/dz, never
// re-normalising. The reason is that the detail term is a PERTURBATION summed
// into a light dot product, not a direction:
//
//     dot(n/|n| + s*d, L)  =  dot(n, L)/|n|  +  s*dot(d, L)
//
// `s*dot(d, L)` is linear in `d`, so the average of four texels' contributions
// IS the contribution of the four texels' average. Re-normalising each level
// would break that linearity and -- worse -- would give a FLAT region
// (dx = dz = 0, the correct answer for "no relief here") an arbitrary unit
// direction, so a surface that should go quiet at distance would instead
// acquire a constant tilt. Coarsening detail must converge to NOTHING, and
// signed averaging is what makes it.
//
// ---------------------------------------------------------------------------
// THE LAYOUT, AND WHY EVERY NUMBER IN IT IS 64-BYTE SHAPED
// ---------------------------------------------------------------------------
// MEM.GUARD's read is at most 64 bytes and its shape rule requires the byte
// mask to match the length, so the cheapest honest reader is one that asks for
// whole 64-byte lines -- the same reasoning `spec/cartridge.md` 4b sets out for
// SPECIES_TABLE, and the same constraint, so the same answer.
//
//   HEADER -- 64 bytes, one line, at the page's base
//     u32 magic     'ZDNM' (0x4D4E'445A little-endian on the wire)
//     u16 version   1
//     u16 words     pyramid words that follow, 1..5461
//     u8  rsv[56]   zero
//
//   BODY -- `words` x u16, THIRTY-TWO per line, starting at byte 64
//     each word {s8 dz, s8 dx} -- dx in bits 7:0, dz in bits 15:8
//     in `zref::terrain::normalmap_pyramid_addr` order, flat word 0 first
//
// A word is 2 bytes and a line is 64, so a line holds exactly 32 words and NO
// WORD EVER STRADDLES A READ. There is no padding inside the body; the page as
// a whole is padded to a multiple of 64 because MEM.UPLOAD's length rule
// requires it of every upload.
//
// THE HEADER CARRIES NO LEVEL COUNT, DELIBERATELY. The pyramid's level
// structure is `normalmap_pyramid_addr`'s and the hardware's `LEVELS`
// parameter; a `levels` field would be a second opinion about the same fact,
// and `spec/cartridge.md` 4d already records what a second opinion costs. The
// loader carries a flat word to a flat address and forms no view about which
// level it belongs to.
//
// A page is REFUSED WHOLE on a wrong magic, a wrong version, a `words` above
// the layout's 5,461, or a `words` that runs past the extent the publication
// declared. A half-loaded pyramid is a terrain whose relief changes at a mip
// boundary for no authored reason, which reads as an aliasing bug and is not
// one.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "zref_terrain_normalmap.hpp"  // the ADDRESSER; this file is the builder

namespace zref {
namespace normal_page {

/** `spec/cartridge.md` 3's page-kind registry. Allocated by R243 D-NORMALS-A. */
inline constexpr uint8_t kPageKind = 16;
/** `spec/cartridge.md` 2's section-type table. */
inline constexpr uint16_t kSectionType = 0x0014;

inline constexpr uint32_t kMagic = 0x4D4E'445Au;  // 'Z','D','N','M' little-endian
inline constexpr uint16_t kVersion = 1;
inline constexpr size_t kHeaderBytes = 64;
inline constexpr size_t kWordBytes = 2;
inline constexpr size_t kWordsPerLine = 64 / kWordBytes;  // 32

/** The level-0 tile is 64x64; the flat pyramid is 5,461 words. Both are
 *  `zref::terrain`'s, restated as names rather than as literals. */
inline constexpr int kTileDim = 64;
inline constexpr int kLevel0Words = kTileDim * kTileDim;                 // 4096
inline constexpr int kMaxWords = zref::terrain::kNormalmapPyramidWords;  // 5461

/** One detail texel. `dx`/`dz` are s8 with value raw/128, and they are a
 *  PERTURBATION of the surface normal in world XZ -- not a direction. */
struct Texel {
  int8_t dx = 0;
  int8_t dz = 0;
};

enum class Verdict : uint8_t {
  kOk = 0,
  kBadMagic = 1,
  kBadVersion = 2,
  kTruncated = 3,   // `words` does not fit in the page's declared length
  kTooManyWords = 4 // `words` above the layout's 5,461
};

// ---------------------------------------------------------------------------
// THE REDUCTION
// ---------------------------------------------------------------------------

/** Average four signed components, round HALF AWAY FROM ZERO.
 *
 *  Symmetric rounding, and the symmetry is load-bearing: negating a whole
 *  detail tile must negate its whole pyramid exactly, or a relief and its
 *  mirror image would coarsen differently and a mirrored piece of terrain
 *  would shimmer where its twin did not. Truncation toward zero and
 *  floor-division both break that; round-half-away-from-zero does not.
 *
 *  The result cannot leave s8: four values in [-128, 127] sum into
 *  [-512, 508], and (-512 - 2) / 4 = -128 (C++ truncating division),
 *  (508 + 2) / 4 = 127. */
inline int8_t avg4_signed(int a, int b, int c, int d) {
  const int sum = a + b + c + d;
  const int rounded = (sum >= 0) ? ((sum + 2) / 4) : ((sum - 2) / 4);
  return static_cast<int8_t>(rounded);
}

/** Build the flat pyramid from a 64x64 level-0 tile, row-major (v*64 + u).
 *
 *  Level L has side 64 >> L and its texel (u, v) is the signed average of the
 *  four level-(L-1) texels at (2u, 2v), (2u+1, 2v), (2u, 2v+1), (2u+1, 2v+1).
 *  NEVER RE-NORMALISED -- see this file's header for why that is the point
 *  rather than an omission.
 *
 *  `levels` is 1..7: 1 is the bare contract tile with no mip tail, 7 is the
 *  full pyramid down to 1x1. It is the AUTHOR'S knob and matches the hardware
 *  parameter of the same name; it is not derived from the tile.
 *
 *  The result is indexed by `zref::terrain::normalmap_pyramid_addr`, which is
 *  the layout's single definition -- this function places words THROUGH it
 *  rather than recomputing the offsets, so the two cannot disagree. */
inline std::vector<Texel> build_pyramid(const std::vector<Texel>& level0, int levels = 7) {
  if (levels < 1) levels = 1;
  if (levels > zref::terrain::kNormalmapLevels) levels = zref::terrain::kNormalmapLevels;

  // Words for the requested depth: the base of the first level NOT present.
  const int words = (levels == zref::terrain::kNormalmapLevels)
                        ? kMaxWords
                        : zref::terrain::kNormalmapLevelBase[levels];

  std::vector<Texel> pyr(static_cast<size_t>(words));

  // Level 0, straight in. A short input is zero-filled rather than refused:
  // a zero texel is "no relief here", which is the neutral value and the one
  // an absent authored region should have.
  for (int v = 0; v < kTileDim; ++v) {
    for (int u = 0; u < kTileDim; ++u) {
      const size_t src = static_cast<size_t>(v) * kTileDim + u;
      if (src < level0.size())
        pyr[static_cast<size_t>(zref::terrain::normalmap_pyramid_addr(0, u, v))] = level0[src];
    }
  }

  // Levels 1..levels-1, each reduced from the one above it.
  //
  // NOTE THE ADDRESSER'S CONVENTION: `normalmap_pyramid_addr(L, u6, v6)` takes
  // LEVEL-0 coordinates and shifts them down itself. So a level-L texel (u, v)
  // is addressed as (u << L, v << L), and reading a level-(L-1) texel (2u, 2v)
  // means addr(L-1, (2u) << (L-1), (2v) << (L-1)) -- which is the same thing as
  // addr(L-1, u << L, v << L). Writing it that way rather than recomputing
  // bases is what keeps this function from becoming a second layout law.
  for (int lvl = 1; lvl < levels; ++lvl) {
    const int side = kTileDim >> lvl;
    for (int v = 0; v < side; ++v) {
      for (int u = 0; u < side; ++u) {
        const int pu = u << lvl;  // level-0 coordinate of this texel
        const int pv = v << lvl;
        const int half = 1 << (lvl - 1);  // level-0 step between the four parents
        const Texel& t00 = pyr[static_cast<size_t>(
            zref::terrain::normalmap_pyramid_addr(lvl - 1, pu, pv))];
        const Texel& t10 = pyr[static_cast<size_t>(
            zref::terrain::normalmap_pyramid_addr(lvl - 1, pu + half, pv))];
        const Texel& t01 = pyr[static_cast<size_t>(
            zref::terrain::normalmap_pyramid_addr(lvl - 1, pu, pv + half))];
        const Texel& t11 = pyr[static_cast<size_t>(
            zref::terrain::normalmap_pyramid_addr(lvl - 1, pu + half, pv + half))];
        Texel out;
        out.dx = avg4_signed(t00.dx, t10.dx, t01.dx, t11.dx);
        out.dz = avg4_signed(t00.dz, t10.dz, t01.dz, t11.dz);
        pyr[static_cast<size_t>(zref::terrain::normalmap_pyramid_addr(lvl, pu, pv))] = out;
      }
    }
  }
  return pyr;
}

// ---------------------------------------------------------------------------
// THE PAGE
// ---------------------------------------------------------------------------

/** The upload word the hardware takes: {s8 dz, s8 dx}, dx in the low byte. */
inline uint16_t pack_word(const Texel& t) {
  return static_cast<uint16_t>((static_cast<uint16_t>(static_cast<uint8_t>(t.dz)) << 8) |
                               static_cast<uint16_t>(static_cast<uint8_t>(t.dx)));
}

inline Texel unpack_word(uint16_t w) {
  Texel t;
  t.dx = static_cast<int8_t>(w & 0xFFu);
  t.dz = static_cast<int8_t>((w >> 8) & 0xFFu);
  return t;
}

/** Build a page from an already-reduced pyramid. The result is padded to a
 *  multiple of 64 bytes, which is what MEM.UPLOAD's length rule requires of
 *  every upload. */
inline std::vector<uint8_t> build(const std::vector<Texel>& pyramid) {
  const size_t n_words = (pyramid.size() > static_cast<size_t>(kMaxWords))
                             ? static_cast<size_t>(kMaxWords)
                             : pyramid.size();
  size_t n = kHeaderBytes + kWordBytes * n_words;
  if (n % 64u != 0u) n += 64u - (n % 64u);
  std::vector<uint8_t> page(n, 0u);
  page[0] = static_cast<uint8_t>(kMagic & 0xFFu);
  page[1] = static_cast<uint8_t>((kMagic >> 8) & 0xFFu);
  page[2] = static_cast<uint8_t>((kMagic >> 16) & 0xFFu);
  page[3] = static_cast<uint8_t>((kMagic >> 24) & 0xFFu);
  page[4] = static_cast<uint8_t>(kVersion & 0xFFu);
  page[5] = static_cast<uint8_t>((kVersion >> 8) & 0xFFu);
  page[6] = static_cast<uint8_t>(n_words & 0xFFu);
  page[7] = static_cast<uint8_t>((n_words >> 8) & 0xFFu);
  for (size_t i = 0; i < n_words; ++i) {
    const uint16_t w = pack_word(pyramid[i]);
    page[kHeaderBytes + kWordBytes * i] = static_cast<uint8_t>(w & 0xFFu);
    page[kHeaderBytes + kWordBytes * i + 1] = static_cast<uint8_t>((w >> 8) & 0xFFu);
  }
  return page;
}

/** Build a page straight from a level-0 tile: reduce, then wrap. */
inline std::vector<uint8_t> build_from_tile(const std::vector<Texel>& level0, int levels = 7) {
  return build(build_pyramid(level0, levels));
}

/** Decode a page exactly as `zhao_terrain_normalloader` does. `out` is left
 *  EMPTY on any verdict but kOk -- a refused page is refused whole. The
 *  returned words are in flat pyramid order, so `out[a]` is the word the
 *  loader writes to `tw_addr_i = a`. */
inline Verdict decode(const uint8_t* page, size_t bytes, std::vector<uint16_t>& out) {
  out.clear();
  if (bytes < kHeaderBytes) return Verdict::kTruncated;
  const uint32_t magic = static_cast<uint32_t>(page[0]) |
                         (static_cast<uint32_t>(page[1]) << 8) |
                         (static_cast<uint32_t>(page[2]) << 16) |
                         (static_cast<uint32_t>(page[3]) << 24);
  if (magic != kMagic) return Verdict::kBadMagic;
  const uint16_t version =
      static_cast<uint16_t>(page[4] | (static_cast<uint16_t>(page[5]) << 8));
  if (version != kVersion) return Verdict::kBadVersion;
  const uint16_t words =
      static_cast<uint16_t>(page[6] | (static_cast<uint16_t>(page[7]) << 8));
  if (words > static_cast<uint16_t>(kMaxWords)) return Verdict::kTooManyWords;
  if (kHeaderBytes + kWordBytes * static_cast<size_t>(words) > bytes)
    return Verdict::kTruncated;
  out.reserve(words);
  for (uint16_t i = 0; i < words; ++i)
    out.push_back(static_cast<uint16_t>(page[kHeaderBytes + kWordBytes * i] |
                                        (static_cast<uint16_t>(
                                             page[kHeaderBytes + kWordBytes * i + 1])
                                         << 8)));
  return Verdict::kOk;
}

}  // namespace normal_page
}  // namespace zref
