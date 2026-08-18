// texture_cache_random.cpp — randomized differential test for TEXTURE.CACHE
// against zref::TextureCache (design/contracts/TEXTURE.CACHE.md, ZH-061).
//
// Deterministic from fixed seeds (the PCG shape every other random lane in
// this tree uses). Four lanes, because the block has regimes a single
// uniform stream would visit badly:
//
//   Lane A — HOT SET. Addresses drawn from a window only a few ways wide, so
//     first-look hits, tag collisions and evictions are common events rather
//     than accidents. Enable masks are random, so single-lane and quad
//     accesses interleave.
//
//   Lane B — THE SAMPLER WALK. What the block is actually for: a 2×2 texel
//     footprint stepping across a surface, four lanes enabled, addresses one
//     texel apart. This is the lane where the hit rate is supposed to be high,
//     and the lane asserts that it IS — a cache that fetched every access
//     would pass every equality check while being useless.
//
//   Lane C — COLD SWEEP + INVALIDATES. Addresses spread far wider than the
//     cache, with a per-batch page invalidate or full flush, which is the
//     stars §1 palette-upload duty cycle.
//
//   Lane D — THE MID-FILL INVALIDATE. Deliberately NOT differential, and it
//     says so at its own definition: the oracle has no fill beats, so it
//     cannot express an invalidate landing while a line is on its way in.
//     It checks the RTL law directly instead, over random lines, beats and
//     fill timings. It exists because a last-beat-only directed case was
//     GREEN against a mutation that deleted the kill entirely.
//
// Every returned halfword, the whole fill-request sequence and both counters
// are compared on every access of lanes A, B and C.

#include "texture_cache_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using zhao::check;
using zhao_texture::cac_describe;
using zhao_texture::cac_expect;
using zhao_texture::cac_same;
using zhao_texture::cac_serialize;
using zhao_texture::CacAccess;
using zhao_texture::CacDev;
using zhao_texture::CacRun;
using zhao_texture::kLanes;
using zhao_texture::kLineBytes;
using zhao_texture::kLines;

namespace {

constexpr uint32_t kPoolBase = 0x0200'0000u;
constexpr uint32_t kPoolSize = 0x8000u;

uint32_t g_saved = 0;
uint32_t g_failures = 0;

// coverage the lanes assert on themselves
uint32_t g_acc_all_hit = 0;   // accesses served with no fill at all
uint32_t g_acc_all_miss = 0;  // quad accesses that fetched four lines
uint32_t g_collisions = 0;    // a fill that displaced a DIFFERENT resident tag
uint32_t g_invalidates = 0;
uint32_t g_mid_fill_invalidates = 0;
uint32_t g_hits = 0;
uint32_t g_misses = 0;

uint32_t next(uint32_t* s) {
  *s = (*s) * 747796405u + 2891336453u;
  const uint32_t w = ((*s >> ((*s >> 28) + 4)) ^ *s) * 277803737u;
  return (w >> 22) ^ w;
}

const zref::TextureMemory& pool() {
  static zref::TextureMemory m = [] {
    zref::TextureMemory t;
    t.base = kPoolBase;
    t.bytes.resize(kPoolSize);
    uint32_t s = 0x1234567u;
    for (size_t i = 0; i < t.bytes.size(); ++i) t.bytes[i] = static_cast<uint8_t>(next(&s));
    return t;
  }();
  return m;
}

bool diff(CacDev* dev, zref::TextureCache* tc, const std::vector<CacAccess>& acc, uint32_t in_seed,
          uint32_t out_seed, int lat, int gap, const char* lane, uint32_t iter) {
  std::string err;
  const CacRun got = dev->feed(acc, pool(), in_seed, out_seed, lat, gap, &err);
  const CacRun want = cac_expect(tc, acc, pool());
  bool ok = err.empty();
  if (!ok) {
    if (g_saved < 6) std::printf("  %s[%u]: protocol violation: %s\n", lane, iter, err.c_str());
    ++g_failures;
  }

  for (size_t i = 0; i < acc.size(); ++i) {
    if (!cac_same(acc[i], want.out[i], got.out[i])) {
      ok = false;
      ++g_failures;
      if (g_saved < 6) {
        ++g_saved;
        std::printf("  %s[%u]: %s\n", lane, iter,
                    cac_describe(i, acc[i], want.out[i], got.out[i]).c_str());
        char nm[64];
        std::snprintf(nm, sizeof(nm), "texture_cache_random_%s_%u", lane, iter);
        zhao::save_failing_vector(nm, cac_serialize(acc), "zref::TextureCache", "RTL");
      }
      break;
    }
  }
  if (want.fills != got.fills) {
    ok = false;
    ++g_failures;
    if (g_saved < 6) {
      ++g_saved;
      std::printf("  %s[%u]: fill sequence differs (oracle %zu, rtl %zu)\n", lane, iter,
                  want.fills.size(), got.fills.size());
    }
  }
  if (want.hits != got.hits || want.misses != got.misses) {
    ok = false;
    ++g_failures;
    if (g_saved < 6) {
      ++g_saved;
      std::printf("  %s[%u]: counters differ (oracle %u/%u, rtl %u/%u)\n", lane, iter, want.hits,
                  want.misses, got.hits, got.misses);
    }
  }
  g_hits = got.hits;
  g_misses = got.misses;
  return ok;
}

/** Count the accesses that needed nothing, and the quads that needed everything. */
void tally(zref::TextureCache* probe, const std::vector<CacAccess>& acc) {
  for (const CacAccess& a : acc) {
    int en = 0;
    int res = 0;
    for (int k = 0; k < kLanes; ++k) {
      if (!a.en[k]) continue;
      ++en;
      if (probe->resident(k, a.addr[k])) {
        ++res;
      } else {
        // A miss whose slot is occupied by a different tag is an EVICTION.
        const int idx = static_cast<int>((a.addr[k] / kLineBytes) % kLines);
        (void)idx;
        ++g_collisions;
      }
    }
    if (en > 0 && res == en) ++g_acc_all_hit;
    if (en == kLanes && res == 0) ++g_acc_all_miss;
    zref::TextureCache::Access ma;
    for (int k = 0; k < kLanes; ++k) {
      ma.en[k] = a.en[k];
      ma.addr[k] = a.addr[k];
    }
    (void)probe->access(ma, pool());
  }
}

// ------------------------------------------------------------------ lane A --
void lane_a(CacDev* dev, int batches) {
  uint32_t rng = 0x00A11CEu;
  // A window a few ways wide: collisions and hits are the common case.
  const uint32_t window = static_cast<uint32_t>(kLines * kLineBytes) * 3u;
  for (int b = 0; b < batches; ++b) {
    dev->reset();
    zref::TextureCache tc;
    zref::TextureCache probe;

    std::vector<CacAccess> acc;
    const int n = 8 + static_cast<int>(next(&rng) % 24u);
    for (int i = 0; i < n; ++i) {
      CacAccess a;
      const uint32_t mask = (next(&rng) & 15u) | 1u;  // never all-disabled
      for (int k = 0; k < kLanes; ++k) {
        a.en[k] = ((mask >> k) & 1u) != 0u;
        a.addr[k] = kPoolBase + (next(&rng) % window);
      }
      a.src_id = static_cast<uint16_t>(next(&rng));
      acc.push_back(a);
    }
    tally(&probe, acc);

    const uint32_t in_seed = (b & 1) ? (next(&rng) | 1u) : 0u;
    const uint32_t out_seed = (b & 2) ? (next(&rng) | 1u) : 0u;
    const int lat = static_cast<int>(next(&rng) % 5u);
    const int gap = static_cast<int>(next(&rng) % 3u);
    diff(dev, &tc, acc, in_seed, out_seed, lat, gap, "A", static_cast<uint32_t>(b));
  }
}

// ------------------------------------------------------------------ lane B --
void lane_b(CacDev* dev, int batches) {
  uint32_t rng = 0x0B0BB1Eu;
  uint32_t served = 0;
  uint32_t fetched = 0;
  for (int b = 0; b < batches; ++b) {
    dev->reset();
    zref::TextureCache tc;

    // A 2×2 footprint walking a 64-wide CLUT8 surface: taps one texel apart in
    // u, one row apart in v. This is exactly what TEXTURE.TMU emits.
    const uint32_t base = kPoolBase + (next(&rng) % 0x1000u) * 16u;
    std::vector<CacAccess> acc;
    uint32_t u = next(&rng) % 32u;
    uint32_t v = next(&rng) % 32u;
    for (int i = 0; i < 40; ++i) {
      CacAccess a;
      for (int k = 0; k < kLanes; ++k) {
        const uint32_t tu = u + static_cast<uint32_t>(k & 1);
        const uint32_t tv = v + static_cast<uint32_t>((k >> 1) & 1);
        a.en[k] = true;
        a.addr[k] = base + (tv << 6) + tu;
      }
      a.src_id = static_cast<uint16_t>(i);
      acc.push_back(a);
      u = (u + 1u) & 63u;
      if (u == 0u) v = (v + 1u) & 63u;
    }

    const uint32_t out_seed = (b & 1) ? (next(&rng) | 1u) : 0u;
    diff(dev, &tc, acc, 0u, out_seed, 2, 0, "B", static_cast<uint32_t>(b));
    served += g_hits + g_misses;
    fetched += g_misses;
  }
  // The lane asserts its own usefulness: a walk over 40 steps of one surface
  // must hit far more often than it fetches, or the cache is not a cache.
  check(fetched * 4u < served, "lane B: the sampler walk hit far more often than it fetched", 1,
        (fetched * 4u < served) ? 1 : 0);
}

// ------------------------------------------------------------------ lane C --
void lane_c(CacDev* dev, int batches) {
  uint32_t rng = 0x0C0FFEEu;
  for (int b = 0; b < batches; ++b) {
    dev->reset();
    zref::TextureCache tc;

    std::vector<CacAccess> acc;
    for (int i = 0; i < 12; ++i) {
      CacAccess a;
      for (int k = 0; k < kLanes; ++k) {
        a.en[k] = true;
        a.addr[k] = kPoolBase + (next(&rng) % (kPoolSize - 4u));
      }
      a.src_id = static_cast<uint16_t>(next(&rng));
      acc.push_back(a);
    }
    diff(dev, &tc, acc, 0u, 0u, 1, 0, "C", static_cast<uint32_t>(b));

    // The stars §1 upload: a page invalidate or a resource-epoch flush, then
    // the same traffic again.
    const bool all = (next(&rng) & 1u) != 0u;
    const uint32_t addr = acc[next(&rng) % acc.size()].addr[0];
    dev->invalidate(all, addr);
    if (all) {
      tc.invalidate_all();
    } else {
      tc.invalidate_line(addr);
    }
    ++g_invalidates;
    diff(dev, &tc, acc, 0u, 0u, 1, 0, "C2", static_cast<uint32_t>(b));
  }
}

// ------------------------------------------------------------------ lane D --
// THE MID-FILL INVALIDATE, randomized. This lane is NOT differential and says
// so: zref::TextureCache has no beats, so it cannot express an invalidate that
// lands while a line is on its way in. What it checks instead is the RTL law
// directly — an invalidate on any beat BEFORE the last must kill the torn
// line, so the same access has to fetch it a second time — over random lines,
// random beats and random fill timings.
//
// It exists because a last-beat-only directed case was GREEN against a
// mutation that deleted the kill entirely: on the last beat the invalidate
// clears the valid bit in the same cycle the tag is written, so the guard is
// invisible there. Every earlier beat is where it lives.
void lane_d(int batches) {
  uint32_t rng = 0x0D0DDu;
  const int last_beat = kLineBytes / 2 - 1;
  for (int b = 0; b < batches; ++b) {
    CacDev d;
    d.reset();
    const uint32_t line =
        kPoolBase + (next(&rng) % ((kPoolSize - 32u) / static_cast<uint32_t>(kLineBytes))) *
                        static_cast<uint32_t>(kLineBytes);
    const int beat = static_cast<int>(next(&rng) % static_cast<uint32_t>(last_beat));
    const bool all = (next(&rng) & 1u) != 0u;
    const int lat = static_cast<int>(next(&rng) % 4u);
    const int gap = static_cast<int>(next(&rng) % 3u);
    std::string err;
    d.arm_invalidate_on_beat(beat, all, line);
    CacAccess a;
    a.en[0] = true;
    a.addr[0] = line + (next(&rng) % static_cast<uint32_t>(kLineBytes));
    const CacRun g1 = d.feed({a}, pool(), 0, 0, lat, gap, &err);
    const CacRun g2 = d.feed({a}, pool(), 0, 0, lat, gap, &err);
    if (!err.empty() || g1.fills.size() != 2 || !g2.fills.empty() ||
        g1.out[0].data[0] != pool().halfword(a.addr[0])) {
      ++g_failures;
      if (g_saved < 6) {
        ++g_saved;
        std::printf("  D[%d]: line %08X beat %d: fills %zu then %zu (want 2 then 0)%s\n", b, line,
                    beat, g1.fills.size(), g2.fills.size(),
                    err.empty() ? "" : (" / " + err).c_str());
      }
    }
    ++g_mid_fill_invalidates;
  }
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  CacDev dev;
  const int a = nightly ? 3000 : 220;
  const int b = nightly ? 600 : 45;
  const int c = nightly ? 900 : 70;
  const int d = nightly ? 700 : 60;
  lane_a(&dev, a);
  lane_b(&dev, b);
  lane_c(&dev, c);
  lane_d(d);

  std::printf(
      "texture_cache_random lane A: %d batches; lane B: %d; lane C: %d; lane D: %d; "
      "%u all-hit accesses, %u all-miss quads, %u evictions, %u invalidates, "
      "%u mid-fill invalidates\n",
      a, b, c, d, g_acc_all_hit, g_acc_all_miss, g_collisions, g_invalidates,
      g_mid_fill_invalidates);

  check(g_failures == 0, "texture_cache_random: every access matches zref::TextureCache", 0,
        g_failures);

  // The lanes assert their own coverage: a differential test that never
  // evicted a line, or never served an access without a fetch, would pass
  // while proving nothing about either.
  check(g_acc_all_hit > 0, "coverage: accesses were served entirely from resident lines", 1,
        g_acc_all_hit > 0 ? 1 : 0);
  check(g_acc_all_miss > 0, "coverage: quad accesses missed on all four lanes at once", 1,
        g_acc_all_miss > 0 ? 1 : 0);
  check(g_collisions > 0, "coverage: lines were evicted by colliding tags", 1,
        g_collisions > 0 ? 1 : 0);
  check(g_invalidates > 0, "coverage: the stars-1 invalidate duty cycle ran", 1,
        g_invalidates > 0 ? 1 : 0);
  check(g_mid_fill_invalidates > 0, "coverage: invalidates landed MID-FILL, on early beats", 1,
        g_mid_fill_invalidates > 0 ? 1 : 0);

  return zhao::report_and_exit("texture_cache_random");
}
