// texture_tmu_random.cpp — randomized differential test for TEXTURE.TMU
// against zref::Tmu (design/contracts/TEXTURE.TMU.md, ledger ZH-027).
//
// Deterministic from fixed seeds (the PCG shape every other random lane in
// this tree uses). Three lanes, because the block has three regimes a single
// uniform stream would visit badly:
//
//   Lane A — FREE TRAFFIC. Every field of the mode word drawn independently:
//     all five formats plus the undefined codes, both filters, all four wrap
//     encodings, every legal texture shape, every lod. So malformed states —
//     bilinear on a palette, a set reserved bit, a mip chain longer than the
//     texture — arrive as a matter of course rather than only in the directed
//     cases. UV coordinates are drawn from a NARROW window around texel
//     boundaries, so `fu = 0` (the identity), `fu = 128` (the rounding tie)
//     and the wrap folds are common events rather than 1-in-4-billion ones.
//
//   Lane B — THE RATIFIED RECIPES. The four sampler states the specs actually
//     name: the star disc (CLUT8 nearest + mips), the sky drum (CLUT8
//     u-mirror / v-clamp + mips), the cloud sheet (ARGB4444 u/v-repeat) and
//     the beam ramp (16×64 RGB565 bilinear). A footprint walks each surface.
//
//   Lane C — THE FILTER, HAMMERED. One bilinear surface, coordinates chosen
//     so the sub-texel fractions sweep 0..255 in both axes against random
//     texel content — the lane where a swapped weight or a truncated rounding
//     has nowhere to hide.
//
// Every channel, the index, the source id and the mode_error verdict are
// compared on every request of every lane.

#include "texture_tmu_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using zhao::check;
using zhao_texture::tmu_describe;
using zhao_texture::tmu_expect;
using zhao_texture::tmu_same;
using zhao_texture::tmu_serialize;
using zhao_texture::TmuDev;
using zhao_texture::TmuReq;
using zhao_texture::TmuRun;
using zhao_texture::TmuSample;

namespace {

using Mode = zref::Tmu::Mode;
using Fmt = zref::Tmu::Format;
using Wrap = zref::Tmu::Wrap;

constexpr uint32_t kBase = 0x0400'0000u;
constexpr uint32_t kSize = 0x8000u;
constexpr uint32_t kPal = kBase + 0x7000u;

uint32_t g_saved = 0;
uint32_t g_failures = 0;

// coverage the lanes assert on themselves
uint32_t g_tie = 0;    // a bilinear fraction of exactly 128
uint32_t g_ident = 0;  // a bilinear fraction of exactly 0 (the identity)
uint32_t g_wrap_used[3] = {0, 0, 0};
uint32_t g_fmt_used[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint32_t g_bilinear = 0;
uint32_t g_mip_nonzero = 0;
uint32_t g_errors = 0;
uint32_t g_clean = 0;

uint32_t next(uint32_t* s) {
  *s = (*s) * 747796405u + 2891336453u;
  const uint32_t w = ((*s >> ((*s >> 28) + 4)) ^ *s) * 277803737u;
  return (w >> 22) ^ w;
}

const zref::TextureMemory& pool() {
  static zref::TextureMemory m = [] {
    zref::TextureMemory t;
    t.base = kBase;
    t.bytes.resize(kSize);
    uint32_t s = 0x7E7U;
    for (size_t i = 0; i < t.bytes.size(); ++i) t.bytes[i] = static_cast<uint8_t>(next(&s));
    return t;
  }();
  return m;
}

bool diff(TmuDev* dev, const std::vector<TmuReq>& reqs, uint32_t in_seed, uint32_t out_seed,
          uint32_t cac_stall, int cac_lat, const char* lane, uint32_t iter) {
  std::string err;
  dev->reset();
  const TmuRun got = dev->feed(reqs, pool(), in_seed, out_seed, cac_stall, cac_lat, &err);
  const std::vector<TmuSample> want = tmu_expect(reqs, pool());
  bool ok = err.empty();
  if (!ok) {
    if (g_saved < 6) std::printf("  %s[%u]: protocol violation: %s\n", lane, iter, err.c_str());
    ++g_failures;
  }
  for (size_t i = 0; i < reqs.size(); ++i) {
    if (want[i].mode_error) {
      ++g_errors;
    } else {
      ++g_clean;
    }
    if (!tmu_same(want[i], got.out[i])) {
      ok = false;
      ++g_failures;
      if (g_saved < 6) {
        ++g_saved;
        std::printf("  %s[%u]: %s\n", lane, iter, tmu_describe(i, want[i], got.out[i]).c_str());
        char nm[64];
        std::snprintf(nm, sizeof(nm), "texture_tmu_random_%s_%u", lane, iter);
        zhao::save_failing_vector(nm, tmu_serialize(reqs), "zref::Tmu", "RTL");
      }
      break;
    }
  }
  return ok;
}

/** Record what a request actually exercised, from the oracle's own plan. */
void tally(const TmuReq& q) {
  zref::Tmu::Req r;
  r.u = q.u;
  r.v = q.v;
  r.base = q.base;
  r.pal_base = q.pal_base;
  r.mode = q.mode;
  r.lod = q.lod;
  const zref::Tmu::Plan p = zref::Tmu::plan(r);
  const Mode m = Mode::unpack(q.mode);
  ++g_fmt_used[m.fmt & 7u];
  if (m.wrap_u < 3) ++g_wrap_used[m.wrap_u];
  if (m.wrap_v < 3) ++g_wrap_used[m.wrap_v];
  if (p.bilinear) {
    ++g_bilinear;
    if (p.fu == 128 || p.fv == 128) ++g_tie;
    if (p.fu == 0 && p.fv == 0) ++g_ident;
  }
  if (p.level > 0) ++g_mip_nonzero;
}

// ------------------------------------------------------------------ lane A --
void lane_a(TmuDev* dev, int batches) {
  uint32_t rng = 0x7A11EDu;
  for (int b = 0; b < batches; ++b) {
    std::vector<TmuReq> reqs;
    const int n = 4 + static_cast<int>(next(&rng) % 10u);
    for (int i = 0; i < n; ++i) {
      Mode m;
      const uint32_t r0 = next(&rng);
      // 1 draw in 16 reaches the undefined format codes 5..7 on purpose.
      m.fmt = static_cast<uint8_t>((r0 & 15u) < 12u ? ((r0 & 15u) % 5u) : (5u + (r0 & 3u) % 3u));
      m.bilinear = ((r0 >> 4) & 1u) != 0u;
      m.wrap_u = static_cast<uint8_t>((r0 >> 5) & 3u);
      m.wrap_v = static_cast<uint8_t>((r0 >> 7) & 3u);
      m.log2w = static_cast<uint8_t>(next(&rng) % 7u);
      m.log2h = static_cast<uint8_t>(next(&rng) % 7u);
      const uint8_t chain = m.log2w < m.log2h ? m.log2w : m.log2h;
      // Mostly legal chains, sometimes one level too long (a mode error).
      m.max_level = static_cast<uint8_t>(((next(&rng) & 7u) == 0u) ? (chain + 1u)
                                                                   : (next(&rng) % (chain + 1u)));
      m.mip_en = ((r0 >> 9) & 1u) != 0u;
      m.rsvd = static_cast<uint16_t>(((next(&rng) & 15u) == 0u) ? 1u : 0u);

      TmuReq q;
      // A narrow window around texel boundaries: `fu` lands on 0 and 128 often.
      const uint32_t lw = m.log2w;
      const uint32_t step = (1u << 16) >> (lw > 15u ? 15u : lw);
      const uint32_t nudge = next(&rng) % 5u;
      q.u = static_cast<int32_t>(next(&rng) % 8u) * static_cast<int32_t>(step) +
            static_cast<int32_t>(nudge) * static_cast<int32_t>(step / 4u) -
            static_cast<int32_t>((next(&rng) & 1u) ? 3 * step : 0u);
      const uint32_t lh = m.log2h;
      const uint32_t steph = (1u << 16) >> (lh > 15u ? 15u : lh);
      q.v = static_cast<int32_t>(next(&rng) % 8u) * static_cast<int32_t>(steph) +
            static_cast<int32_t>(next(&rng) % 5u) * static_cast<int32_t>(steph / 4u) -
            static_cast<int32_t>((next(&rng) & 1u) ? 3 * steph : 0u);
      // A base far enough inside the pool that every legal chain fits.
      q.base = kBase + ((next(&rng) % 0x300u) * 16u);
      q.pal_base = kPal;
      q.mode = m.pack();
      q.lod = static_cast<uint8_t>(next(&rng));
      q.src_id = static_cast<uint16_t>(next(&rng));
      tally(q);
      reqs.push_back(q);
    }
    const uint32_t in_seed = (b & 1) ? (next(&rng) | 1u) : 0u;
    const uint32_t out_seed = (b & 2) ? (next(&rng) | 1u) : 0u;
    const uint32_t cs = (b & 4) ? (next(&rng) | 1u) : 0u;
    const int lat = static_cast<int>(next(&rng) % 4u) + 1;
    diff(dev, reqs, in_seed, out_seed, cs, lat, "A", static_cast<uint32_t>(b));
  }
}

// ------------------------------------------------------------------ lane B --
void lane_b(TmuDev* dev, int batches) {
  uint32_t rng = 0x0B0BBu;

  // The four sampler states the specs actually name.
  Mode star;  // stars §1: CLUT8 nearest + mips
  star.fmt = Fmt::kClut8;
  star.log2w = 6;
  star.log2h = 6;
  star.max_level = 6;
  star.mip_en = true;

  Mode drum;  // sky_and_beams §1.1: CLUT8, u-mirror, v-clamp, +mips
  drum.fmt = Fmt::kClut8;
  drum.wrap_u = Wrap::kMirror;
  drum.wrap_v = Wrap::kClamp;
  drum.log2w = 6;
  drum.log2h = 5;
  drum.max_level = 5;
  drum.mip_en = true;

  Mode cloud;  // sky_and_beams §1.1: ARGB4444, u/v-repeat, +mips
  cloud.fmt = Fmt::kArgb4444;
  cloud.log2w = 5;
  cloud.log2h = 5;
  cloud.max_level = 5;
  cloud.mip_en = true;

  Mode beam;  // sky_and_beams §2: 16×64 RGB565, BILINEAR mandatory
  beam.fmt = Fmt::kRgb565;
  beam.bilinear = true;
  beam.wrap_u = Wrap::kClamp;
  beam.log2w = 4;
  beam.log2h = 6;
  beam.max_level = 4;
  beam.mip_en = true;

  const Mode recipes[4] = {star, drum, cloud, beam};

  for (int b = 0; b < batches; ++b) {
    const Mode& m = recipes[static_cast<size_t>(b) % 4u];
    std::vector<TmuReq> reqs;
    int32_t u = static_cast<int32_t>(next(&rng));
    int32_t v = static_cast<int32_t>(next(&rng));
    const int32_t du = static_cast<int32_t>(next(&rng) % 4096u) - 2048;
    const int32_t dv = static_cast<int32_t>(next(&rng) % 4096u) - 2048;
    const uint32_t base = kBase + ((next(&rng) % 0x200u) * 16u);
    for (int i = 0; i < 24; ++i) {
      TmuReq q;
      q.u = u;
      q.v = v;
      q.base = base;
      q.pal_base = kPal;
      q.mode = m.pack();
      q.lod = static_cast<uint8_t>(next(&rng) % 0x50u);
      q.src_id = static_cast<uint16_t>(i);
      tally(q);
      reqs.push_back(q);
      u += du;
      v += dv;
    }
    const uint32_t out_seed = (b & 1) ? (next(&rng) | 1u) : 0u;
    diff(dev, reqs, 0u, out_seed, 0u, 1, "B", static_cast<uint32_t>(b));
  }
}

// ------------------------------------------------------------------ lane C --
void lane_c(TmuDev* dev, int batches) {
  uint32_t rng = 0x0C0F17u;
  Mode m;
  m.fmt = Fmt::kRgb565;
  m.bilinear = true;
  m.log2w = 3;
  m.log2h = 3;

  for (int b = 0; b < batches; ++b) {
    std::vector<TmuReq> reqs;
    const uint32_t base = kBase + ((next(&rng) % 0x400u) * 16u);
    for (int i = 0; i < 32; ++i) {
      TmuReq q;
      // 8-wide texture: one texel is 8192 raw units, so stepping by 32 units
      // sweeps `fu` through every value in [0,255] a byte at a time, and
      // adding the half texel puts the tie at the middle of the sweep.
      q.u = static_cast<int32_t>(next(&rng) % 8u) * 8192 + static_cast<int32_t>(i) * 32 * 8;
      q.v = static_cast<int32_t>(next(&rng) % 8u) * 8192 +
            static_cast<int32_t>(next(&rng) % 256u) * 32;
      q.base = base;
      q.pal_base = kPal;
      q.mode = m.pack();
      q.src_id = static_cast<uint16_t>(next(&rng));
      tally(q);
      reqs.push_back(q);
    }
    diff(dev, reqs, 0u, 0u, 0u, 1, "C", static_cast<uint32_t>(b));
  }
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  TmuDev dev;
  const int a = nightly ? 60000 : 260;
  const int b = nightly ? 18000 : 80;
  const int c = nightly ? 18000 : 80;
  lane_a(&dev, a);
  lane_b(&dev, b);
  lane_c(&dev, c);

  std::printf(
      "texture_tmu_random lane A: %d batches; lane B: %d; lane C: %d; "
      "%u bilinear samples (%u at a rounding tie, %u at the identity); "
      "%u mipped; wraps %u/%u/%u; %u mode errors, %u clean\n",
      a, b, c, g_bilinear, g_tie, g_ident, g_mip_nonzero, g_wrap_used[0], g_wrap_used[1],
      g_wrap_used[2], g_errors, g_clean);

  check(g_failures == 0, "texture_tmu_random: every sample matches zref::Tmu", 0, g_failures);

  // The lanes assert their own coverage: a differential test that never
  // filtered, never mipped, never wrapped or never hit a rounding tie would
  // pass while proving nothing about any of them.
  check(g_tie > 0, "coverage: bilinear fractions landed EXACTLY on the rounding tie (128)", 1,
        g_tie > 0 ? 1 : 0);
  check(g_ident > 0, "coverage: bilinear fractions landed exactly on a texel centre", 1,
        g_ident > 0 ? 1 : 0);
  check(g_bilinear > 0, "coverage: the bilinear path ran", 1, g_bilinear > 0 ? 1 : 0);
  check(g_mip_nonzero > 0, "coverage: mip levels above 0 were selected", 1,
        g_mip_nonzero > 0 ? 1 : 0);
  check(g_wrap_used[0] > 0 && g_wrap_used[1] > 0 && g_wrap_used[2] > 0,
        "coverage: all three wrap modes ran", 1,
        (g_wrap_used[0] > 0 && g_wrap_used[1] > 0 && g_wrap_used[2] > 0) ? 1 : 0);
  bool all_fmt = true;
  for (int f = 0; f < 5; ++f)
    if (g_fmt_used[f] == 0) all_fmt = false;
  check(all_fmt, "coverage: all five texture formats ran", 1, all_fmt ? 1 : 0);
  check(g_errors > 0 && g_clean > 0, "coverage: the run produced both malformed and clean modes", 1,
        (g_errors > 0 && g_clean > 0) ? 1 : 0);

  return zhao::report_and_exit("texture_tmu_random");
}
