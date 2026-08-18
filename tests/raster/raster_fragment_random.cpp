// raster_fragment_random.cpp — randomized differential test for
// RASTER.FRAGMENT against zref::FragmentPipeline (contract
// design/contracts/RASTER.FRAGMENT.md, ledger ZH-025).
//
// Deterministic from fixed seeds (the PCG shape every other random lane here
// uses). Three lanes, because the block has three regimes a single uniform
// stream would visit badly:
//
//   Lane A — FREE TRAFFIC. Every field random, including the STATE word, over
//     a WIDE address range. This is the lane that reaches odd state
//     combinations no recipe asks for (a stencilled additive with a masked
//     texel and forced-far depth) and it is the reason the state encoding has
//     no reserved holes: every 32-bit draw is legal, so nothing is filtered.
//
//   Lane B — THE SAME PIXEL. Addresses drawn from a window of 1..4, so
//     read-after-write at one pixel is the common case rather than a
//     1-in-256 accident. This is where the block's no-forwarding claim is
//     actually tested: it rests entirely on RASTER.TILESTORE's write-first
//     rule, and a chain of blends at one address is what notices if that
//     assumption is wrong.
//
//   Lane C — THE RECIPES. Only the six ratified states, over destinations
//     drawn from the full colour/depth/stencil space. Lane A visits each
//     recipe by accident once in millions; this lane makes them the traffic.
//
// Every lane compares the ORDERED write list, the whole 256-word tile and
// both counters, at three write-port stall densities.

#include "raster_fragment_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using zhao::check;
using zhao_raster::fr_describe;
using zhao_raster::fr_expect;
using zhao_raster::fr_word;
using zhao_raster::FrDev;
using zhao_raster::FrExpect;
using zhao_raster::FrFrag;
using zhao_raster::FrRun;
using zhao_raster::FrState;
using zhao_raster::kFrWords;

using zref::FragmentPipeline;

namespace {

uint32_t g_saved = 0;
uint32_t g_failures = 0;

// Coverage the lanes assert on themselves.
uint32_t g_wrote = 0;
uint32_t g_killed_z = 0;
uint32_t g_killed_sten = 0;
uint32_t g_killed_atest = 0;
uint32_t g_blend_seen[4] = {0, 0, 0, 0};
uint32_t g_sat_hi = 0;     // an additive blend actually railed at 255
uint32_t g_z_tie = 0;      // depth exactly equal to the destination (must fail)
uint32_t g_raw_chain = 0;  // two fragments in a row at ONE address

uint32_t next(uint32_t* s) {
  *s = (*s) * 747796405u + 2891336453u;
  const uint32_t w = ((*s >> ((*s >> 28) + 4)) ^ *s) * 277803737u;
  return (w >> 22) ^ w;
}

uint32_t random_state(uint32_t* rng) {
  const uint32_t r = next(rng);
  FrState s;
  s.z_test_en = (r >> 0) & 1u;
  s.z_write_dis = (r >> 1) & 1u;
  s.z_force_far = (r >> 2) & 1u;
  s.blend = static_cast<uint8_t>((r >> 3) & 3u);
  s.shade_mod = (r >> 5) & 1u;
  s.alpha_mod = (r >> 6) & 1u;
  s.atest_en = (r >> 7) & 1u;
  // A reference biased to 0 (the ratified sentinel) but not pinned there.
  s.atest_ref = ((r >> 20) & 1u) ? 0 : static_cast<uint8_t>(next(rng));
  s.sten_func = static_cast<uint8_t>((r >> 8) & 3u);
  s.sten_op = static_cast<uint8_t>((r >> 10) & 3u);
  s.tag_write_dis = (r >> 12) & 1u;
  s.tag_from_texel = (r >> 13) & 1u;
  s.tag_channel = static_cast<uint8_t>((r >> 14) & 3u);
  s.sten_mask = ((r >> 21) & 1u) ? 0xFF : static_cast<uint8_t>(next(rng));
  return s.pack();
}

FrFrag random_frag(uint32_t* rng, uint8_t addr, uint32_t state) {
  FrFrag f;
  f.addr = addr;
  f.state = state;
  f.depth = next(rng) & 0xFFFFFFu;
  const uint32_t c = next(rng);
  f.vr = static_cast<uint8_t>(c);
  f.vg = static_cast<uint8_t>(c >> 8);
  f.vb = static_cast<uint8_t>(c >> 16);
  f.va = static_cast<uint8_t>(c >> 24);
  const uint32_t d = next(rng);
  f.tag = static_cast<uint8_t>(d);
  f.sten_ref = static_cast<uint8_t>(d >> 8);
  f.tidx = ((next(rng) & 3u) == 0u) ? 0 : static_cast<uint8_t>(d >> 16);  // index 0 often
  const uint32_t e = next(rng);
  f.tr = static_cast<uint8_t>(e);
  f.tg = static_cast<uint8_t>(e >> 8);
  f.tb = static_cast<uint8_t>(e >> 16);
  f.ta = static_cast<uint8_t>(e >> 24);
  // Rail the colour and alpha channels often, so the blend's saturation and
  // its unit8 endpoints are common rather than astronomically unlikely.
  const uint32_t rail = next(rng);
  if ((rail & 7u) == 0u) {
    f.vr = 255;
    f.vg = 255;
    f.vb = 255;
  }
  if ((rail & 0x70u) == 0u) f.va = 255;
  if ((rail & 0x700u) == 0u) f.va = 0;
  return f;
}

void tally(const uint64_t* tile, const std::vector<FrFrag>& frags) {
  uint64_t t[kFrWords];
  for (int i = 0; i < kFrWords; ++i) t[i] = tile[i];
  uint8_t prev_addr = 0;
  bool have_prev = false;
  for (const FrFrag& f : frags) {
    const FrState st = FrState::unpack(f.state);
    const zref::TileStore::Word dst = zref::TileStore::Word::unpack(t[f.addr]);
    ++g_blend_seen[st.blend & 3u];
    if (have_prev && prev_addr == f.addr) ++g_raw_chain;
    prev_addr = f.addr;
    have_prev = true;

    if (st.z_test_en && f.depth == dst.depth) ++g_z_tie;

    const bool a_ok = !st.atest_en || (f.tidx != st.atest_ref);
    bool s_ok = true;
    if (st.sten_func == FragmentPipeline::kEqual)
      s_ok = (dst.stencil & st.sten_mask) == (f.sten_ref & st.sten_mask);
    else if (st.sten_func == FragmentPipeline::kNotEqual)
      s_ok = (dst.stencil & st.sten_mask) != (f.sten_ref & st.sten_mask);
    else if (st.sten_func == FragmentPipeline::kNever)
      s_ok = false;
    const bool z_ok = !st.z_test_en || (f.depth > dst.depth);

    if (!a_ok)
      ++g_killed_atest;
    else if (!s_ok)
      ++g_killed_sten;
    else if (!z_ok)
      ++g_killed_z;

    const zref::FragmentPipeline::Out o = zref::FragmentPipeline::apply(f, t[f.addr]);
    if (o.write) {
      ++g_wrote;
      const zref::TileStore::Word w = zref::TileStore::Word::unpack(o.word);
      if ((st.blend == FragmentPipeline::kAdd || st.blend == FragmentPipeline::kAddMod) &&
          (w.r == 255 || w.g == 255 || w.b == 255))
        ++g_sat_hi;
      t[f.addr] = o.word;
    }
  }
}

bool diff(FrDev* dev, const uint64_t* tile, const std::vector<FrFrag>& frags, uint32_t in_seed,
          uint32_t wr_seed, const char* lane, uint32_t iter) {
  std::string err;
  const FrRun got = dev->run(tile, frags, in_seed, wr_seed, &err);
  bool ok = err.empty();
  if (!ok) {
    if (g_saved < 6) std::printf("  %s[%u]: protocol violation: %s\n", lane, iter, err.c_str());
    ++g_failures;
  }

  const FrExpect want = fr_expect(tile, frags);

  if (want.writes.size() != got.writes.size()) {
    ok = false;
    ++g_failures;
    if (g_saved < 6) {
      std::printf("  %s[%u]: oracle wrote %zu, rtl wrote %zu\n", lane, iter, want.writes.size(),
                  got.writes.size());
      ++g_saved;
    }
  } else {
    for (size_t i = 0; i < want.writes.size(); ++i) {
      if (want.writes[i].addr == got.writes[i].addr && want.writes[i].data == got.writes[i].data)
        continue;
      ok = false;
      ++g_failures;
      if (g_saved < 6) {
        const std::string body = fr_describe(i, want.writes[i].data, got.writes[i].data);
        std::printf("  %s[%u]: write %zu differs\n    %s\n", lane, iter, i, body.c_str());
        zhao::save_failing_vector("raster_fragment_random", zhao_raster::fr_serialize(frags),
                                  "zref::FragmentPipeline", body);
        ++g_saved;
      }
      break;
    }
  }

  for (int i = 0; i < kFrWords; ++i) {
    if (want.tile[i] == got.tile[i]) continue;
    ok = false;
    ++g_failures;
    if (g_saved < 6) {
      const std::string body = fr_describe(static_cast<size_t>(i), want.tile[i], got.tile[i]);
      std::printf("  %s[%u]: final tile differs\n    %s\n", lane, iter, body.c_str());
      zhao::save_failing_vector("raster_fragment_random", zhao_raster::fr_serialize(frags),
                                "zref::FragmentPipeline", body);
      ++g_saved;
    }
    break;
  }

  if (got.blended != want.blended || got.covered != frags.size() || got.error) {
    ok = false;
    ++g_failures;
    if (g_saved < 6) {
      std::printf("  %s[%u]: counters blended %u/%u covered %u/%zu error %d\n", lane, iter,
                  want.blended, got.blended, got.covered, frags.size(),
                  static_cast<int>(got.error));
      ++g_saved;
    }
  }
  return ok;
}

void fill_random_tile(uint32_t* rng, uint64_t* tile) {
  for (int i = 0; i < kFrWords; ++i) {
    const uint32_t a = next(rng);
    const uint32_t b = next(rng);
    tile[i] = fr_word(static_cast<uint8_t>(a), static_cast<uint8_t>(a >> 8),
                      static_cast<uint8_t>(a >> 16), static_cast<uint8_t>(a >> 24), b & 0xFFFFFFu,
                      static_cast<uint8_t>(next(rng)));
  }
}

// ------------------------------------------------------------------ lane A --
void lane_a(FrDev* dev, int batches) {
  uint32_t rng = 0xF00DBA51u;
  for (int b = 0; b < batches; ++b) {
    uint64_t tile[kFrWords] = {};
    fill_random_tile(&rng, tile);
    std::vector<FrFrag> frags;
    const int n = 20 + static_cast<int>(next(&rng) % 40u);
    for (int i = 0; i < n; ++i)
      frags.push_back(random_frag(&rng, static_cast<uint8_t>(next(&rng)), random_state(&rng)));
    tally(tile, frags);
    const uint32_t wr = (b % 3 == 0) ? 0u : (next(&rng) | 1u);
    const uint32_t in = (b % 2 == 0) ? 0u : (next(&rng) | 1u);
    diff(dev, tile, frags, in, wr, "A", static_cast<uint32_t>(b));
  }
}

// ------------------------------------------------------------------ lane B --
// One to four addresses, so read-after-write at one pixel is the traffic.
void lane_b(FrDev* dev, int batches) {
  uint32_t rng = 0x1DEA5EEDu;
  for (int b = 0; b < batches; ++b) {
    uint64_t tile[kFrWords] = {};
    fill_random_tile(&rng, tile);
    const uint8_t base = static_cast<uint8_t>(next(&rng));
    const uint8_t span = static_cast<uint8_t>(1u + (next(&rng) & 3u));
    std::vector<FrFrag> frags;
    const int n = 16 + static_cast<int>(next(&rng) % 32u);
    for (int i = 0; i < n; ++i) {
      const uint8_t addr = static_cast<uint8_t>(base + (next(&rng) % span));
      // Depths clustered so the strict test is a real contest at one pixel.
      FrFrag f = random_frag(&rng, addr, random_state(&rng));
      f.depth = 0x800000u + (next(&rng) & 0x3Fu);
      frags.push_back(f);
    }
    tally(tile, frags);
    diff(dev, tile, frags, 0u, (b & 1) ? (next(&rng) | 1u) : 0u, "B", static_cast<uint32_t>(b));
  }
}

// ------------------------------------------------------------------ lane C --
// Only the six ratified recipes, which lane A would otherwise visit once in
// millions of draws.
void lane_c(FrDev* dev, int batches) {
  uint32_t rng = 0x2ECC1DEDu;
  const uint32_t recipes[6] = {
      FragmentPipeline::sky_backdrop().pack(),     FragmentPipeline::sky_cloud_fade().pack(),
      FragmentPipeline::sun_additive().pack(),     FragmentPipeline::beam_additive_fade().pack(),
      FragmentPipeline::star_disc_masked().pack(), FragmentPipeline::star_halo_additive().pack()};
  for (int b = 0; b < batches; ++b) {
    uint64_t tile[kFrWords] = {};
    fill_random_tile(&rng, tile);
    std::vector<FrFrag> frags;
    const int n = 24 + static_cast<int>(next(&rng) % 32u);
    for (int i = 0; i < n; ++i) {
      const uint32_t st = recipes[next(&rng) % 6u];
      FrFrag f = random_frag(&rng, static_cast<uint8_t>(next(&rng) & 0x3Fu), st);
      frags.push_back(f);
    }
    tally(tile, frags);
    diff(dev, tile, frags, (b & 1) ? (next(&rng) | 1u) : 0u, (b & 2) ? (next(&rng) | 1u) : 0u, "C",
         static_cast<uint32_t>(b));
  }
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  FrDev dev;
  const int a = nightly ? 4000 : 260;
  const int b = nightly ? 3000 : 200;
  const int c = nightly ? 3000 : 200;
  lane_a(&dev, a);
  lane_b(&dev, b);
  lane_c(&dev, c);

  std::printf(
      "raster_fragment_random: lanes %d/%d/%d batches; %u writes; killed z=%u sten=%u atest=%u; "
      "blend REPLACE/ALPHA/ADD/ADD_MOD = %u/%u/%u/%u; %u additive rails; %u depth ties; "
      "%u same-pixel chains\n",
      a, b, c, g_wrote, g_killed_z, g_killed_sten, g_killed_atest, g_blend_seen[0], g_blend_seen[1],
      g_blend_seen[2], g_blend_seen[3], g_sat_hi, g_z_tie, g_raw_chain);

  check(g_failures == 0, "raster_fragment_random: every write matches zref::FragmentPipeline", 0,
        g_failures);

  // Self-asserted coverage: a differential lane that never killed a fragment,
  // never railed an additive blend or never met a depth tie would pass while
  // proving nothing about those paths.
  check(g_wrote > 0, "coverage: fragments were written", 1, g_wrote > 0 ? 1 : 0);
  check(g_killed_z > 0, "coverage: fragments were killed by the DEPTH test", 1,
        g_killed_z > 0 ? 1 : 0);
  check(g_killed_sten > 0, "coverage: fragments were killed by the STENCIL test", 1,
        g_killed_sten > 0 ? 1 : 0);
  check(g_killed_atest > 0, "coverage: fragments were killed by the ALPHA (index) test", 1,
        g_killed_atest > 0 ? 1 : 0);
  bool all_blends = true;
  for (int i = 0; i < 4; ++i)
    if (g_blend_seen[i] == 0) all_blends = false;
  check(all_blends, "coverage: all four blend modes were exercised", 1, all_blends ? 1 : 0);
  check(g_sat_hi > 0, "coverage: an additive blend actually RAILED at 255", 1,
        g_sat_hi > 0 ? 1 : 0);
  check(g_z_tie > 0, "coverage: a fragment landed EXACTLY at the destination depth (ties fail)", 1,
        g_z_tie > 0 ? 1 : 0);
  check(g_raw_chain > 0, "coverage: consecutive fragments hit the SAME pixel", 1,
        g_raw_chain > 0 ? 1 : 0);

  return zhao::report_and_exit("raster_fragment_random");
}
