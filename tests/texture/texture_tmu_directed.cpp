// texture_tmu_directed.cpp — directed vectors for TEXTURE.TMU
// (fpga/rtl/texture/zhao_texture_tmu.sv; contract
// design/contracts/TEXTURE.TMU.md; ledger ZH-027).
//
// Every case drives the RTL and zref::Tmu through the identical request
// sequence and requires the RGB, the alpha, the CLUT index, the source id and
// the mode_error verdict to agree. On top of that each case asserts its own
// law:
//
//   1. formats         — CLUT8, CLUT4, RGB565, ARGB1555, ARGB4444, each with
//                        its frozen or chosen expansion pinned by value
//   2. CLUT index 0    — the alpha-test case: the raw index reaches the
//                        output, and RASTER.FRAGMENT's star_disc_masked kills
//                        exactly that fragment and no other
//   3. wrap modes      — repeat, clamp and mirror at every boundary, negative
//                        coordinates included
//   4. mirror IS §6.2  — swept against zref::terrain::mirror_texel itself,
//                        texel for texel, over a full period
//   5. nearest         — is the bilinear datapath's exact identity (fu=fv=0)
//   6. bilinear        — the endpoints, an asymmetric footprint that catches
//                        swapped weights, and THE ROUNDING TIE that separates
//                        round-half-up from truncate by one LSB
//   7. half-texel bias — bilinear at a texel centre equals nearest there
//   8. mip levels      — the selection boundaries (0x0F/0x10/0x1F/0x20), the
//                        max_level clamp, mip_en off, and the level OFFSET
//                        closed form against zref::Tmu's summation loop
//   9. mode errors     — bilinear on a palette (forced nearest + pulse), a
//                        reserved bit, an over-long mip chain, a bad format
//  10. non-square      — the 16×64 beam ramp: LOG2W and LOG2H are independent
//  11. backpressure    — nine timing patterns change not one byte, and the
//                        ledger's `variable_bounded:16` is measured

#include "texture_tmu_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "zref/zref_fragment.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_tilestore.hpp"

using zhao::check;
using zhao_texture::tmu_describe;
using zhao_texture::tmu_expect;
using zhao_texture::tmu_same;
using zhao_texture::TmuDev;
using zhao_texture::TmuReq;
using zhao_texture::TmuRun;
using zhao_texture::TmuSample;

namespace {

using Mode = zref::Tmu::Mode;
using Fmt = zref::Tmu::Format;
using Wrap = zref::Tmu::Wrap;

TmuDev& dev() {
  static TmuDev d;
  return d;
}

constexpr uint32_t kBase = 0x0300'0000u;
constexpr uint32_t kSize = 0x2000u;

// ---- the pool layout, chosen so a returned value NAMES its own address -----
constexpr uint32_t kTexIdent = kBase + 0x0000u;  // byte[k] = k & 0xFF (an 8x8 CLUT8 mip chain)
constexpr uint32_t kPal = kBase + 0x0400u;       // 256 RGB565 entries
constexpr uint32_t kTex565 = kBase + 0x0800u;    // 2x2 RGB565 + mips
constexpr uint32_t kTexBil = kBase + 0x0880u;    // 2x1 RGB565, the rounding-tie pair
constexpr uint32_t kTex4444 = kBase + 0x08C0u;   // 2x2 ARGB4444
constexpr uint32_t kTex1555 = kBase + 0x0900u;   // 2x2 ARGB1555
constexpr uint32_t kTexClut4 = kBase + 0x0940u;  // 4x4 CLUT4
constexpr uint32_t kTexMir = kBase + 0x1000u;    // 64x1 CLUT8, byte[k] = k
constexpr uint32_t kTexBeam = kBase + 0x1100u;   // 16x64 RGB565 (the beam ramp shape)

zref::TextureMemory build_pool() {
  zref::TextureMemory m;
  m.base = kBase;
  m.bytes.assign(kSize, 0);
  auto put8 = [&m](uint32_t a, uint8_t v) { m.bytes[a - kBase] = v; };
  auto put16 = [&m](uint32_t a, uint16_t v) {
    m.bytes[a - kBase] = static_cast<uint8_t>(v & 0xFFu);
    m.bytes[a - kBase + 1] = static_cast<uint8_t>(v >> 8);
  };

  // An 8x8 CLUT8 mip chain whose every byte is its own offset: a returned
  // index therefore NAMES the address the block computed.
  for (uint32_t k = 0; k < 128u; ++k) put8(kTexIdent + k, static_cast<uint8_t>(k));

  // The palette: entry 0 is black (the transparent index, stars §3), and
  // entry i is a value that survives the 565 round trip distinguishably.
  put16(kPal, 0x0000u);
  for (uint32_t i = 1; i < 256u; ++i)
    put16(kPal + i * 2u, static_cast<uint16_t>((i << 8) | (i ^ 0x5Au)));

  // 2x2 RGB565 with one mip level.
  put16(kTex565 + 0u, 0xF800u);  // red
  put16(kTex565 + 2u, 0x07E0u);  // green
  put16(kTex565 + 4u, 0x001Fu);  // blue
  put16(kTex565 + 6u, 0xFFFFu);  // white
  put16(kTex565 + 8u, 0x8410u);  // level 1 (1x1)

  // The rounding-tie pair: a 2x1 RGB565 texture, red 0 and red 255.
  put16(kTexBil + 0u, 0x0000u);
  put16(kTexBil + 2u, 0xF800u);

  put16(kTex4444 + 0u, 0x1234u);
  put16(kTex4444 + 2u, 0xFEDCu);
  put16(kTex4444 + 4u, 0x0F0Fu);
  put16(kTex4444 + 6u, 0xA5A5u);

  put16(kTex1555 + 0u, 0x8000u);  // opaque black
  put16(kTex1555 + 2u, 0x7FFFu);  // transparent white
  put16(kTex1555 + 4u, 0xFC00u);  // opaque red
  put16(kTex1555 + 6u, 0x03E0u);  // transparent green

  // 4x4 CLUT4: two indices a byte, low nibble first.
  for (uint32_t k = 0; k < 8u; ++k)
    put8(kTexClut4 + k, static_cast<uint8_t>(0x10u * ((k + 1u) & 15u) + (k & 15u)));

  // 64x1 CLUT8 whose byte k is k: the mirrored-fold probe.
  for (uint32_t k = 0; k < 64u; ++k) put8(kTexMir + k, static_cast<uint8_t>(k));

  // 16x64 RGB565 — the beam ramp's shape (sky_and_beams §2).
  for (uint32_t k = 0; k < 16u * 64u; ++k)
    put16(kTexBeam + k * 2u, static_cast<uint16_t>((k * 2654435761u) >> 16));
  return m;
}

const zref::TextureMemory& pool() {
  static const zref::TextureMemory m = build_pool();
  return m;
}

Mode mk_mode(uint8_t fmt, bool bil, uint8_t wu, uint8_t wv, uint8_t log2w, uint8_t log2h,
             uint8_t maxlvl = 0, bool mip = false) {
  Mode m;
  m.fmt = fmt;
  m.bilinear = bil;
  m.wrap_u = wu;
  m.wrap_v = wv;
  m.log2w = log2w;
  m.log2h = log2h;
  m.max_level = maxlvl;
  m.mip_en = mip;
  return m;
}

TmuReq mk(int32_t u, int32_t v, uint32_t base, const Mode& m, uint8_t lod = 0,
          uint32_t pal = kPal) {
  TmuReq q;
  q.u = u;
  q.v = v;
  q.base = base;
  q.pal_base = pal;
  q.mode = m.pack();
  q.lod = lod;
  q.src_id = 0x0710;
  return q;
}

/** Run a batch through both sides and compare every field. */
bool run(const std::vector<TmuReq>& reqs, const char* name, uint32_t in_seed, uint32_t out_seed,
         uint32_t cac_stall, int cac_lat, TmuRun* got_out) {
  std::string err;
  dev().reset();
  const TmuRun got = dev().feed(reqs, pool(), in_seed, out_seed, cac_stall, cac_lat, &err);
  const std::vector<TmuSample> want = tmu_expect(reqs, pool());
  bool ok = err.empty();
  if (!ok) std::printf("  %s: protocol violation: %s\n", name, err.c_str());
  for (size_t i = 0; i < reqs.size(); ++i) {
    if (!tmu_same(want[i], got.out[i])) {
      if (ok) std::printf("  %s: %s\n", name, tmu_describe(i, want[i], got.out[i]).c_str());
      ok = false;
    }
  }
  if (got_out != nullptr) *got_out = got;
  return ok;
}

/** One sample, from the RTL, with the oracle checked alongside. */
TmuSample sample_one(const TmuReq& q, const char* name, bool* ok_out) {
  TmuRun got;
  const bool ok = run({q}, name, 0, 0, 0, 1, &got);
  if (ok_out != nullptr) *ok_out = ok;
  return got.out[0];
}

// --------------------------------------------------------------------- 1 ---
void test_formats() {
  bool ok = false;
  // CLUT8 — texel (0,0) of the identity texture is index 0, whose palette
  // entry is black.
  const Mode c8 = mk_mode(Fmt::kClut8, false, Wrap::kRepeat, Wrap::kRepeat, 3, 3);
  TmuSample s = sample_one(mk(0, 0, kTexIdent, c8), "fmt-clut8", &ok);
  check(ok, "formats: CLUT8 matches zref::Tmu", 1, ok ? 1 : 0);
  check(s.idx == 0, "formats: CLUT8 reports the RAW index", 0, s.idx);
  check(s.a == 255, "formats: a CLUT texel's alpha is 255 (its index carries transparency)", 255,
        s.a);
  // ...and texel (1,0) is index 1.
  s = sample_one(mk(1 << 13, 0, kTexIdent, c8), "fmt-clut8-1", &ok);
  check(ok && s.idx == 1, "formats: CLUT8 index 1 at u = 1/8", 1, (ok && s.idx == 1) ? 1 : 0);

  // RGB565 — the frozen (c5<<3)|(c5>>2) / (c6<<2)|(c6>>4) expansion.
  const Mode m565 = mk_mode(Fmt::kRgb565, false, Wrap::kRepeat, Wrap::kRepeat, 1, 1);
  s = sample_one(mk(0, 0, kTex565, m565), "fmt-565", &ok);
  check(ok, "formats: RGB565 matches zref::Tmu", 1, ok ? 1 : 0);
  check(s.r == 255 && s.g == 0 && s.b == 0, "formats: RGB565 0xF800 expands to pure red", 1,
        (s.r == 255 && s.g == 0 && s.b == 0) ? 1 : 0);
  check(s.a == 255, "formats: RGB565 has no alpha channel, so alpha is 255", 255, s.a);
  s = sample_one(mk(1 << 15, 1 << 15, kTex565, m565), "fmt-565-w", &ok);
  check(ok && s.r == 255 && s.g == 255 && s.b == 255,
        "formats: RGB565 0xFFFF round-trips to full white", 1,
        (ok && s.r == 255 && s.g == 255 && s.b == 255) ? 1 : 0);

  // ARGB4444 — nibble replication.
  const Mode m4444 = mk_mode(Fmt::kArgb4444, false, Wrap::kRepeat, Wrap::kRepeat, 1, 1);
  s = sample_one(mk(0, 0, kTex4444, m4444), "fmt-4444", &ok);
  check(ok, "formats: ARGB4444 matches zref::Tmu", 1, ok ? 1 : 0);
  check(s.a == 0x11 && s.r == 0x22 && s.g == 0x33 && s.b == 0x44,
        "formats: ARGB4444 0x1234 replicates to 11/22/33/44", 1,
        (s.a == 0x11 && s.r == 0x22 && s.g == 0x33 && s.b == 0x44) ? 1 : 0);

  // ARGB1555 — the alpha BIT becomes 0 or 255, nothing between.
  const Mode m1555 = mk_mode(Fmt::kArgb1555, false, Wrap::kRepeat, Wrap::kRepeat, 1, 1);
  s = sample_one(mk(0, 0, kTex1555, m1555), "fmt-1555-a", &ok);
  check(ok && s.a == 255 && s.r == 0 && s.g == 0 && s.b == 0,
        "formats: ARGB1555 0x8000 is opaque black", 1,
        (ok && s.a == 255 && s.r == 0 && s.g == 0 && s.b == 0) ? 1 : 0);
  s = sample_one(mk(1 << 15, 0, kTex1555, m1555), "fmt-1555-b", &ok);
  check(ok && s.a == 0 && s.r == 255 && s.g == 255 && s.b == 255,
        "formats: ARGB1555 0x7FFF is transparent white", 1,
        (ok && s.a == 0 && s.r == 255 && s.g == 255 && s.b == 255) ? 1 : 0);

  // CLUT4 — two indices a byte, low nibble first.
  const Mode c4 = mk_mode(Fmt::kClut4, false, Wrap::kRepeat, Wrap::kRepeat, 2, 2);
  bool all = true;
  for (int t = 0; t < 4; ++t) {
    const int32_t u = static_cast<int32_t>((static_cast<uint32_t>(t) << 16) / 4u);
    s = sample_one(mk(u, 0, kTexClut4, c4), "fmt-clut4", &ok);
    const uint8_t byte = pool().byte_at(kTexClut4 + static_cast<uint32_t>(t) / 2u);
    const uint8_t want =
        (t & 1) ? static_cast<uint8_t>(byte >> 4) : static_cast<uint8_t>(byte & 15u);
    if (!ok || s.idx != want) all = false;
  }
  check(all, "formats: CLUT4 selects the right nibble of the right byte, all four texels", 1,
        all ? 1 : 0);
}

// --------------------------------------------------------------------- 2 ---
// stars §1's alpha-test case, end to end: the TMU's raw index is what
// RASTER.FRAGMENT tests, and it is the ONLY thing that can answer the test.
void test_clut_index_zero_and_the_fragment() {
  const Mode c8 = mk_mode(Fmt::kClut8, false, Wrap::kRepeat, Wrap::kRepeat, 3, 3);
  std::vector<TmuReq> reqs;
  for (int t = 0; t < 8; ++t)
    reqs.push_back(
        mk(static_cast<int32_t>((static_cast<uint32_t>(t) << 16) / 8u), 0, kTexIdent, c8));
  TmuRun got;
  const bool ok = run(reqs, "clut-idx0", 0, 0, 0, 1, &got);
  check(ok, "clut index 0: the row matches zref::Tmu", 1, ok ? 1 : 0);

  bool indices_ok = true;
  for (int t = 0; t < 8; ++t)
    if (got.out[t].idx != static_cast<uint8_t>(t)) indices_ok = false;
  check(indices_ok, "clut index 0: the eight texels report indices 0..7 exactly", 1,
        indices_ok ? 1 : 0);

  // Feed the samples into RASTER.FRAGMENT's oracle under star_disc_masked.
  // Nothing in RASTER.FRAGMENT's interface changes to accept them — the three
  // fields the TMU emits ARE `frag_texel_rgb_i` / `_a_i` / `_idx_i`.
  const uint32_t state = zref::FragmentPipeline::star_disc_masked().pack();
  zref::TileStore::Word dst;
  dst.depth = 0;
  const uint64_t dstw = dst.pack();
  int killed = 0;
  int wrote = 0;
  bool tags_ok = true;
  for (int t = 0; t < 8; ++t) {
    zref::FragmentPipeline::Frag f;
    f.depth = 0x400000u;
    f.state = state;
    f.vr = 200;
    f.vg = 100;
    f.vb = 50;
    f.va = 255;
    f.tr = got.out[t].r;
    f.tg = got.out[t].g;
    f.tb = got.out[t].b;
    f.ta = got.out[t].a;
    f.tidx = got.out[t].idx;
    const zref::FragmentPipeline::Out o = zref::FragmentPipeline::apply(f, dstw);
    if (!o.write) {
      ++killed;
    } else {
      ++wrote;
      const zref::TileStore::Word w = zref::TileStore::Word::unpack(o.word);
      const uint8_t want_tag =
          static_cast<uint8_t>((zref::FragmentPipeline::kGlow << 6) | (got.out[t].idx & 63u));
      if (w.tag != want_tag) tags_ok = false;
    }
  }
  check(killed == 1, "clut index 0: exactly the index-0 texel is alpha-tested away", 1, killed);
  check(wrote == 7, "clut index 0: the other seven write", 7, wrote);
  check(tags_ok, "clut index 0: each survivor's glow tag carries ITS OWN CLUT intensity", 1,
        tags_ok ? 1 : 0);
}

// --------------------------------------------------------------------- 3 ---
void test_wrap_modes() {
  // A 4-wide CLUT8 identity texture (bytes 0..3 of kTexIdent), 1 tall.
  const int32_t kUnit = 1 << 16;
  const int32_t us[5] = {-kUnit / 4, 0, 3 * kUnit / 4, kUnit, kUnit + kUnit / 4};
  const uint8_t want_repeat[5] = {3, 0, 3, 0, 1};
  const uint8_t want_clamp[5] = {0, 0, 3, 3, 3};
  const uint8_t want_mirror[5] = {0, 0, 3, 3, 2};

  struct Case {
    uint8_t mode;
    const uint8_t* want;
    const char* name;
  };
  const Case cases[3] = {{Wrap::kRepeat, want_repeat, "repeat"},
                         {Wrap::kClamp, want_clamp, "clamp"},
                         {Wrap::kMirror, want_mirror, "mirror"}};

  for (const Case& c : cases) {
    const Mode m = mk_mode(Fmt::kClut8, false, c.mode, c.mode, 2, 0);
    std::vector<TmuReq> reqs;
    for (int32_t u : us) reqs.push_back(mk(u, 0, kTexIdent, m));
    TmuRun got;
    const bool ok = run(reqs, c.name, 0, 0, 0, 1, &got);
    check(ok, "wrap: the sweep matches zref::Tmu", 1, ok ? 1 : 0);
    bool all = true;
    for (int i = 0; i < 5; ++i)
      if (got.out[i].idx != c.want[i]) all = false;
    check(all, "wrap: the mode folds every boundary to the right texel", 1, all ? 1 : 0);
  }

  // The two axes are independent: u-mirror with v-clamp is the sky drum's
  // own state (sky_and_beams §1.1).
  const Mode drum = mk_mode(Fmt::kClut8, false, Wrap::kMirror, Wrap::kClamp, 2, 1);
  std::vector<TmuReq> reqs{mk(-kUnit / 4, -kUnit, kTexIdent, drum),
                           mk(kUnit + kUnit / 4, 4 * kUnit, kTexIdent, drum)};
  TmuRun got;
  const bool ok = run(reqs, "drum", 0, 0, 0, 1, &got);
  check(ok, "wrap: per-axis modes (u-mirror, v-clamp) match zref::Tmu", 1, ok ? 1 : 0);
  // v clamps to row 0 below and row 1 above; u mirrors as above.
  check(got.out[0].idx == 0, "wrap: v-clamp below zero lands on row 0", 0, got.out[0].idx);
  check(got.out[1].idx == 4 + 2, "wrap: v-clamp above the top lands on the LAST row", 6,
        got.out[1].idx);
}

// --------------------------------------------------------------------- 4 ---
// MIRROR is not a new law: it is spec/terrain_rules.md §6.2's frozen fold,
// generalised. Swept against the frozen helper itself over a full period, on
// the RTL — not against a restatement.
void test_mirror_is_the_frozen_fold() {
  const Mode m = mk_mode(Fmt::kClut8, false, Wrap::kMirror, Wrap::kRepeat, 6, 0);
  std::vector<TmuReq> reqs;
  std::vector<int32_t> raws;
  for (int i = -140; i < 140; ++i) {
    // One texel is 1024 raw units at 64 texels a wrap; step by a third of a
    // texel so the floor is exercised off-centre as well as on it.
    const int32_t u = i * 341;
    raws.push_back(u);
    reqs.push_back(mk(u, 0, kTexMir, m));
  }
  TmuRun got;
  const bool ok = run(reqs, "mirror-vs-frozen", 0, 0, 0, 1, &got);
  check(ok, "mirror: the sweep matches zref::Tmu", 1, ok ? 1 : 0);
  bool all = true;
  for (size_t i = 0; i < raws.size(); ++i) {
    const int32_t frozen = zref::terrain::mirror_texel(raws[i]);
    if (got.out[i].idx != static_cast<uint8_t>(frozen)) all = false;
  }
  check(all,
        "mirror: 280 coordinates fold exactly as zref::terrain::mirror_texel (terrain_rules §6.2)",
        1, all ? 1 : 0);
}

// --------------------------------------------------------------------- 5 ---
void test_nearest_is_the_bilinear_identity() {
  // FILTER_NEAREST forces fu = fv = 0, so w00 = 65,536 and the filter is the
  // exact identity on tap 0. Asserted by pinning nearest at a coordinate whose
  // sub-texel fraction is NOT zero — a filter that leaked would move.
  const Mode m = mk_mode(Fmt::kRgb565, false, Wrap::kRepeat, Wrap::kRepeat, 1, 1);
  bool ok = false;
  const TmuSample s = sample_one(mk(20000, 20000, kTex565, m), "nearest-identity", &ok);
  check(ok, "nearest: matches zref::Tmu at a fractional coordinate", 1, ok ? 1 : 0);
  check(s.r == 255 && s.g == 0 && s.b == 0,
        "nearest: a fractional coordinate still returns texel (0,0) UNMIXED", 1,
        (s.r == 255 && s.g == 0 && s.b == 0) ? 1 : 0);
}

// --------------------------------------------------------------------- 6 ---
void test_bilinear_weights_and_the_rounding_tie() {
  // The 2x1 RGB565 pair: red 0 and red 255, one texel apart.
  const Mode m = mk_mode(Fmt::kRgb565, true, Wrap::kRepeat, Wrap::kRepeat, 1, 0);

  // THE TIE. u = 0.5 texture units puts the sample point exactly half a texel
  // past texel 0's centre: fu = 128, fv = 0, so the exact sum is
  // 255 * 32768 = 8,355,840 and +32768 lands EXACTLY on 128 * 65,536.
  // Round-half-up gives 128; truncate gives 127.
  bool ok = false;
  TmuSample s = sample_one(mk(32768, 32768, kTexBil, m), "bilinear-tie", &ok);
  check(ok, "bilinear: the tie vector matches zref::Tmu", 1, ok ? 1 : 0);
  check(s.r == 128,
        "bilinear: the exact rounding tie rounds UP (round-half-up, qformats §4) — truncate "
        "would give 127",
        128, s.r);

  // THE ENDPOINTS. At a texel centre the filter is the identity; one texel on,
  // it is the identity on the other texel.
  s = sample_one(mk(16384, 32768, kTexBil, m), "bilinear-end0", &ok);
  check(ok && s.r == 0, "bilinear: at texel 0's centre the filter is the exact identity", 0, s.r);
  s = sample_one(mk(49152, 32768, kTexBil, m), "bilinear-end1", &ok);
  check(ok && s.r == 255, "bilinear: at texel 1's centre likewise", 255, s.r);

  // SWAPPED WEIGHTS are invisible on a symmetric footprint, so this uses the
  // 2x2 texture with an ASYMMETRIC fraction: w10 pairs with t10 and w01 with
  // t01, and exchanging them changes the answer.
  const Mode m2 = mk_mode(Fmt::kRgb565, true, Wrap::kRepeat, Wrap::kRepeat, 1, 1);
  std::vector<TmuReq> reqs;
  for (int i = 0; i < 24; ++i) {
    const int32_t u = 16384 + i * 1723;
    const int32_t v = 16384 + i * 5591;
    reqs.push_back(mk(u, v, kTex565, m2));
  }
  TmuRun got;
  const bool okk = run(reqs, "bilinear-asym", 0, 0, 0, 1, &got);
  check(okk, "bilinear: 24 asymmetric footprints match zref::Tmu", 1, okk ? 1 : 0);
  // ...and the run really did mix (a filter stuck on one tap would be flat).
  bool varied = false;
  for (size_t i = 1; i < got.out.size(); ++i)
    if (got.out[i].r != got.out[0].r || got.out[i].g != got.out[0].g) varied = true;
  check(varied, "bilinear: the asymmetric sweep actually moved the filtered value", 1,
        varied ? 1 : 0);
}

// --------------------------------------------------------------------- 7 ---
void test_half_texel_bias() {
  // The chosen convention: bilinear samples about the texel CENTRE, so the two
  // filters agree there. Without the bias, bilinear at a centre would be a
  // 50/50 mix of two texels instead.
  const Mode nb = mk_mode(Fmt::kRgb565, false, Wrap::kRepeat, Wrap::kRepeat, 1, 1);
  const Mode bl = mk_mode(Fmt::kRgb565, true, Wrap::kRepeat, Wrap::kRepeat, 1, 1);
  bool all = true;
  bool okall = true;
  for (int ty = 0; ty < 2; ++ty) {
    for (int tx = 0; tx < 2; ++tx) {
      // The centre of texel (tx,ty) in a 2x2 texture is (tx + 0.5)/2.
      const int32_t u = static_cast<int32_t>((2 * tx + 1) * 16384);
      const int32_t v = static_cast<int32_t>((2 * ty + 1) * 16384);
      bool ok = false;
      const TmuSample a = sample_one(mk(u, v, kTex565, nb), "bias-n", &ok);
      okall = okall && ok;
      const TmuSample b = sample_one(mk(u, v, kTex565, bl), "bias-b", &ok);
      okall = okall && ok;
      if (a.r != b.r || a.g != b.g || a.b != b.b) all = false;
    }
  }
  check(okall, "half-texel: both filters match zref::Tmu at every texel centre", 1, okall ? 1 : 0);
  check(all, "half-texel: bilinear equals nearest at every texel centre (the half-texel bias)", 1,
        all ? 1 : 0);
}

// --------------------------------------------------------------------- 8 ---
void test_mip_levels() {
  // The 8x8 identity chain: level offsets are 0, 64, 80, 84, and a returned
  // index NAMES the level offset because byte[k] = k.
  const Mode m = mk_mode(Fmt::kClut8, false, Wrap::kRepeat, Wrap::kRepeat, 3, 3, 3, true);

  // Selection BOUNDARIES: lod is U 4.4, level = floor(lod >> 4).
  const uint8_t lods[6] = {0x00, 0x0F, 0x10, 0x1F, 0x20, 0x30};
  const uint8_t want_level[6] = {0, 0, 1, 1, 2, 3};
  std::vector<TmuReq> reqs;
  for (uint8_t l : lods) reqs.push_back(mk(0, 0, kTexIdent, m, l));
  TmuRun got;
  bool ok = run(reqs, "mip-boundaries", 0, 0, 0, 1, &got);
  check(ok, "mip: the lod boundary sweep matches zref::Tmu", 1, ok ? 1 : 0);
  bool all = true;
  for (int i = 0; i < 6; ++i) {
    const uint32_t want = zref::Tmu::level_offset_texels(3, 3, want_level[i]);
    if (got.out[i].idx != static_cast<uint8_t>(want)) all = false;
  }
  check(all,
        "mip: 0x0F selects level 0 and 0x10 selects level 1 (floor), and the RTL's closed-form "
        "level offset equals zref::Tmu's summation loop at every level",
        1, all ? 1 : 0);

  // The max_level clamp: lod far past the chain must stop at max_level.
  const Mode m1 = mk_mode(Fmt::kClut8, false, Wrap::kRepeat, Wrap::kRepeat, 3, 3, 1, true);
  bool ok1 = false;
  TmuSample s = sample_one(mk(0, 0, kTexIdent, m1, 0xF0), "mip-clamp", &ok1);
  check(ok1 && s.idx == static_cast<uint8_t>(zref::Tmu::level_offset_texels(3, 3, 1)),
        "mip: a lod past max_level clamps to max_level, not past the chain", 1,
        (ok1 && s.idx == static_cast<uint8_t>(zref::Tmu::level_offset_texels(3, 3, 1))) ? 1 : 0);

  // MIP_EN off ignores the lod entirely.
  const Mode m0 = mk_mode(Fmt::kClut8, false, Wrap::kRepeat, Wrap::kRepeat, 3, 3, 3, false);
  bool ok0 = false;
  s = sample_one(mk(0, 0, kTexIdent, m0, 0xF0), "mip-off", &ok0);
  check(ok0 && s.idx == 0, "mip: MIP_EN = 0 samples level 0 whatever the lod says", 0, s.idx);

  // The offset closed form at EVERY legal (log2w, log2h, level).
  bool closed = true;
  for (uint8_t w = 0; w <= 10; ++w) {
    for (uint8_t h = 0; h <= 10; ++h) {
      const uint8_t chain = w < h ? w : h;
      uint32_t sum = 0;
      for (uint8_t l = 0; l <= chain; ++l) {
        if (zref::Tmu::level_offset_texels(w, h, l) != sum) closed = false;
        sum += (1u << (w - l)) * (1u << (h - l));
      }
    }
  }
  check(closed, "mip: zref::Tmu's level offset is the running sum of the levels below it", 1,
        closed ? 1 : 0);
}

// --------------------------------------------------------------------- 9 ---
void test_mode_errors() {
  // 1. BILINEAR ON A PALETTE — stars §1's hard law, enforced in fabric.
  const Mode bad = mk_mode(Fmt::kClut8, true, Wrap::kRepeat, Wrap::kRepeat, 3, 3);
  const Mode good = mk_mode(Fmt::kClut8, false, Wrap::kRepeat, Wrap::kRepeat, 3, 3);
  bool ok1 = false;
  bool ok2 = false;
  const TmuSample sb = sample_one(mk(20000, 20000, kTexIdent, bad), "err-bil-clut", &ok1);
  const TmuSample sg = sample_one(mk(20000, 20000, kTexIdent, good), "err-bil-clut-ref", &ok2);
  check(ok1 && ok2, "mode error: both requests match zref::Tmu", 1, (ok1 && ok2) ? 1 : 0);
  check(sb.mode_error, "mode error: bilinear on a palette pulses mode_error_o", 1,
        sb.mode_error ? 1 : 0);
  check(!sg.mode_error, "mode error: the same request as nearest does NOT", 0,
        sg.mode_error ? 1 : 0);
  check(sb.idx == sg.idx && sb.r == sg.r,
        "mode error: and it samples NEAREST — never a filtered palette", 1,
        (sb.idx == sg.idx && sb.r == sg.r) ? 1 : 0);

  // 2. A RESERVED BIT.
  Mode rsvd = mk_mode(Fmt::kRgb565, false, Wrap::kRepeat, Wrap::kRepeat, 1, 1);
  rsvd.rsvd = 1;
  bool ok3 = false;
  const TmuSample sr = sample_one(mk(0, 0, kTex565, rsvd), "err-rsvd", &ok3);
  check(ok3 && sr.mode_error, "mode error: a set reserved bit pulses mode_error_o", 1,
        (ok3 && sr.mode_error) ? 1 : 0);

  // 3. MAX_LEVEL BEYOND THE CHAIN (a 16x64 texture has 5 levels, not 7).
  const Mode over = mk_mode(Fmt::kRgb565, false, Wrap::kRepeat, Wrap::kRepeat, 4, 6, 6, true);
  bool ok4 = false;
  const TmuSample so = sample_one(mk(0, 0, kTexBeam, over, 0x60), "err-chain", &ok4);
  check(ok4 && so.mode_error, "mode error: a max_level past the mip chain pulses mode_error_o", 1,
        (ok4 && so.mode_error) ? 1 : 0);

  // 4. AN UNDEFINED FORMAT CODE.
  Mode badfmt = mk_mode(6, false, Wrap::kRepeat, Wrap::kRepeat, 1, 1);
  bool ok5 = false;
  const TmuSample sf = sample_one(mk(0, 0, kTex565, badfmt), "err-fmt", &ok5);
  check(ok5 && sf.mode_error, "mode error: format code 6 pulses mode_error_o", 1,
        (ok5 && sf.mode_error) ? 1 : 0);
}

// -------------------------------------------------------------------- 10 ---
void test_non_square() {
  // sky_and_beams §2: the beam ramp is 16x64 direct colour and bilinear is
  // MANDATORY. LOG2W and LOG2H are independent fields precisely for this.
  const Mode beam = mk_mode(Fmt::kRgb565, true, Wrap::kClamp, Wrap::kRepeat, 4, 6, 4, true);
  std::vector<TmuReq> reqs;
  uint32_t s = 0xBEA3u;
  for (int i = 0; i < 40; ++i) {
    s = s * 747796405u + 2891336453u;
    const int32_t u = static_cast<int32_t>(s >> 8);
    s = s * 747796405u + 2891336453u;
    const int32_t v = static_cast<int32_t>(s >> 8);
    reqs.push_back(mk(u, v, kTexBeam, beam, static_cast<uint8_t>(i * 7)));
  }
  TmuRun got;
  const bool ok = run(reqs, "beam", 0, 0, 0, 1, &got);
  check(ok, "non-square: the 16x64 bilinear beam ramp matches zref::Tmu", 1, ok ? 1 : 0);
  bool none_err = true;
  for (const TmuSample& t : got.out)
    if (t.mode_error) none_err = false;
  check(none_err, "non-square: a legal 16x64 chain raises no mode error", 1, none_err ? 1 : 0);
}

// -------------------------------------------------------------------- 11 ---
void test_backpressure_and_latency() {
  const Mode c8 = mk_mode(Fmt::kClut8, false, Wrap::kRepeat, Wrap::kRepeat, 3, 3, 3, true);
  const Mode bl = mk_mode(Fmt::kRgb565, true, Wrap::kMirror, Wrap::kClamp, 1, 1);
  std::vector<TmuReq> reqs;
  uint32_t s = 0xBACC5u;
  for (int i = 0; i < 32; ++i) {
    s = s * 747796405u + 2891336453u;
    const int32_t u = static_cast<int32_t>(s);
    s = s * 747796405u + 2891336453u;
    const int32_t v = static_cast<int32_t>(s);
    reqs.push_back((i & 1) ? mk(u, v, kTex565, bl)
                           : mk(u, v, kTexIdent, c8, static_cast<uint8_t>(i)));
  }

  const uint32_t pats[3][3] = {{0, 0, 0}, {0xA1u, 0xB2u, 0}, {0, 0xC3u, 0xD4u}};
  const int lats[3] = {1, 2, 9};
  std::vector<uint8_t> golden;
  bool first = true;
  bool stable = true;
  uint32_t worst_hit_latency = 0;
  for (const uint32_t(&p)[3] : pats) {
    for (int lat : lats) {
      TmuRun got;
      const bool ok = run(reqs, "backpressure", p[0], p[1], p[2], lat, &got);
      check(ok, "backpressure: the batch matches zref::Tmu under every pattern", 1, ok ? 1 : 0);
      std::vector<uint8_t> flat;
      for (const TmuSample& t : got.out) {
        flat.push_back(t.r);
        flat.push_back(t.g);
        flat.push_back(t.b);
        flat.push_back(t.a);
        flat.push_back(t.idx);
      }
      if (first) {
        golden = flat;
        first = false;
      } else if (flat != golden) {
        stable = false;
      }
      if (p[0] == 0 && p[1] == 0 && p[2] == 0 && lat == 1 && got.max_latency > worst_hit_latency)
        worst_hit_latency = got.max_latency;
      check(got.samples == reqs.size(), "backpressure: texture_samples counts every retired sample",
            reqs.size(), got.samples);
    }
  }
  check(stable, "backpressure: not one sampled byte moved across nine timing patterns", 1,
        stable ? 1 : 0);
  std::printf("texture_tmu latency: worst accept-to-retire on a 1-cycle cache is %u cycles\n",
              worst_hit_latency);
  check(worst_hit_latency <= 16,
        "latency: the ledger's `variable_bounded:16` holds when the cache answers in one cycle", 16,
        worst_hit_latency);
}

}  // namespace

int main() {
  test_formats();
  test_clut_index_zero_and_the_fragment();
  test_wrap_modes();
  test_mirror_is_the_frozen_fold();
  test_nearest_is_the_bilinear_identity();
  test_bilinear_weights_and_the_rounding_tie();
  test_half_texel_bias();
  test_mip_levels();
  test_mode_errors();
  test_non_square();
  test_backpressure_and_latency();
  return zhao::report_and_exit("texture_tmu_directed");
}
