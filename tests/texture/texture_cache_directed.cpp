// texture_cache_directed.cpp — directed vectors for TEXTURE.CACHE
// (fpga/rtl/texture/zhao_texture_cache.sv; contract
// design/contracts/TEXTURE.CACHE.md; ledger ZH-061).
//
// Every case drives the RTL and zref::TextureCache through the identical
// access sequence and requires every returned halfword, the fill-request
// sequence and both counters to agree. On top of that each case asserts its
// own law:
//
//   1. reset            — every line invalid, so the first touch of any line
//                         is a miss and exactly one fill
//   2. miss then hit    — the second access to the same line issues NO fill,
//                         and returns the same bytes (there is no bypass: the
//                         data comes out of the array either way)
//   3. the halfword     — little-endian, at `addr & ~1`; every offset in a
//                         line reachable, odd offsets included
//   4. whole line       — one fill serves all 16 bytes of the line
//   5. tag collision    — two addresses with the same index and different
//                         tags evict each other, every time, in ONE lane
//   6. lane isolation   — and never across lanes: a fill in lane k cannot
//                         disturb lane j, and one line may be resident twice
//   7. invalidate       — line and all; a re-access after either refills
//   8. inv beats a fill — an invalidate landing mid-fill cancels the tag
//                         write, so the torn line never becomes valid
//   9. multi-lane miss  — four missing lanes take four fills, lowest lane
//                         first, and the access is accepted only when the
//                         last hole closes
//  10. backpressure     — six stall patterns, three fill latencies and two
//                         beat gaps change not one returned byte
//  11. counters         — hits count the FIRST look, misses count LINES

#include "texture_cache_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using zhao::check;
using zhao_texture::cac_describe;
using zhao_texture::cac_expect;
using zhao_texture::cac_same;
using zhao_texture::CacAccess;
using zhao_texture::CacDev;
using zhao_texture::CacResult;
using zhao_texture::CacRun;
using zhao_texture::kLanes;
using zhao_texture::kLineBytes;
using zhao_texture::kLines;

namespace {

CacDev& dev() {
  static CacDev d;
  return d;
}

constexpr uint32_t kPoolBase = 0x0100'0000u;
constexpr uint32_t kPoolSize = 0x4000u;

uint32_t pcg(uint32_t* s) {
  *s = (*s) * 747796405u + 2891336453u;
  const uint32_t w = ((*s >> ((*s >> 28) + 4)) ^ *s) * 277803737u;
  return (w >> 22) ^ w;
}

/** A deterministic texture pool: every byte a function of its address. */
const zref::TextureMemory& pool() {
  static zref::TextureMemory m = [] {
    zref::TextureMemory t;
    t.base = kPoolBase;
    t.bytes.resize(kPoolSize);
    uint32_t s = 0xC0FFEEu;
    for (size_t i = 0; i < t.bytes.size(); ++i) t.bytes[i] = static_cast<uint8_t>(pcg(&s));
    return t;
  }();
  return m;
}

CacAccess one(uint32_t addr, uint16_t src = 0x11) {
  CacAccess a;
  a.en[0] = true;
  a.addr[0] = addr;
  a.src_id = src;
  return a;
}

CacAccess quad(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint16_t src = 0x22) {
  CacAccess a;
  for (int k = 0; k < kLanes; ++k) a.en[k] = true;
  a.addr[0] = a0;
  a.addr[1] = a1;
  a.addr[2] = a2;
  a.addr[3] = a3;
  a.src_id = src;
  return a;
}

/** Reset both sides so a case starts from a known cache. */
void fresh(zref::TextureCache* tc) {
  dev().reset();
  tc->reset();
}

/**
 * Run one batch through both and compare responses, fills and counters.
 * Returns false (having reported) on any divergence.
 */
bool run(zref::TextureCache* tc, const std::vector<CacAccess>& acc, const char* name,
         uint32_t in_seed, uint32_t out_seed, int fill_lat, int beat_gap, CacRun* got_out) {
  std::string err;
  const CacRun got = dev().feed(acc, pool(), in_seed, out_seed, fill_lat, beat_gap, &err);
  const CacRun want = cac_expect(tc, acc, pool());
  bool ok = err.empty();
  if (!ok) std::printf("  %s: protocol violation: %s\n", name, err.c_str());

  for (size_t i = 0; i < acc.size(); ++i) {
    if (!cac_same(acc[i], want.out[i], got.out[i])) {
      if (ok)
        std::printf("  %s: %s\n", name, cac_describe(i, acc[i], want.out[i], got.out[i]).c_str());
      ok = false;
    }
  }
  if (want.fills != got.fills) {
    std::printf("  %s: fill sequence differs (oracle %zu, rtl %zu)\n", name, want.fills.size(),
                got.fills.size());
    ok = false;
  }
  if (want.hits != got.hits || want.misses != got.misses) {
    std::printf("  %s: counters differ (oracle %u/%u, rtl %u/%u)\n", name, want.hits, want.misses,
                got.hits, got.misses);
    ok = false;
  }
  if (got_out != nullptr) *got_out = got;
  return ok;
}

// --------------------------------------------------------------------- 1 ---
void test_reset_is_empty() {
  zref::TextureCache tc;
  fresh(&tc);
  CacRun got;
  const bool ok = run(&tc, {one(kPoolBase)}, "reset", 0, 0, 2, 0, &got);
  check(ok, "reset: the first access matches zref::TextureCache", 1, ok ? 1 : 0);
  check(got.misses == 1, "reset: the very first line touch is a miss", 1, got.misses);
  check(got.hits == 0, "reset: and it is not also a hit", 0, got.hits);
  check(got.fills.size() == 1, "reset: exactly one line was fetched", 1, got.fills.size());
  check(got.fills.empty() || got.fills[0] == kPoolBase, "reset: the fetched line is the right one",
        kPoolBase, got.fills.empty() ? 0 : got.fills[0]);
}

// --------------------------------------------------------------------- 2 ---
void test_miss_then_hit() {
  zref::TextureCache tc;
  fresh(&tc);
  std::vector<CacAccess> a{one(kPoolBase + 0x40u), one(kPoolBase + 0x40u), one(kPoolBase + 0x42u)};
  CacRun got;
  const bool ok = run(&tc, a, "miss-then-hit", 0, 0, 3, 0, &got);
  check(ok, "miss/hit: the sequence matches zref::TextureCache", 1, ok ? 1 : 0);
  check(got.fills.size() == 1, "miss/hit: only the FIRST access fetched a line", 1,
        got.fills.size());
  check(got.misses == 1 && got.hits == 2, "miss/hit: one miss, two first-look hits", 1,
        (got.misses == 1 && got.hits == 2) ? 1 : 0);
  // No bypass: the miss response and a later hit response are the same bytes,
  // which they would not be if the fill shortcut to the output.
  check(got.out[0].data[0] == got.out[1].data[0],
        "miss/hit: the filled response equals the resident one (no fill bypass)",
        got.out[1].data[0], got.out[0].data[0]);
}

// --------------------------------------------------------------------- 3 ---
void test_halfword_port() {
  zref::TextureCache tc;
  fresh(&tc);
  std::vector<CacAccess> a;
  for (int b = 0; b < kLineBytes; ++b)
    a.push_back(one(kPoolBase + 0x200u + static_cast<uint32_t>(b)));
  CacRun got;
  const bool ok = run(&tc, a, "halfword", 0, 0, 1, 0, &got);
  check(ok, "halfword: every offset in a line matches zref::TextureCache", 1, ok ? 1 : 0);
  check(got.fills.size() == 1, "halfword: all 16 offsets came from ONE fetched line", 1,
        got.fills.size());
  // Little-endian, at addr & ~1: offsets 2k and 2k+1 return the SAME halfword,
  // whose low byte is the pool's byte at 2k.
  for (int b = 0; b < kLineBytes; b += 2) {
    const uint32_t addr = kPoolBase + 0x200u + static_cast<uint32_t>(b);
    const uint16_t want = pool().halfword(addr);
    check(got.out[b].data[0] == want, "halfword: the even offset returns the LE halfword", want,
          got.out[b].data[0]);
    check(got.out[b + 1].data[0] == want,
          "halfword: the odd offset returns the SAME halfword (addr & ~1)", want,
          got.out[b + 1].data[0]);
  }
}

// --------------------------------------------------------------------- 4 ---
void test_whole_line_from_one_fill() {
  zref::TextureCache tc;
  fresh(&tc);
  std::vector<CacAccess> a;
  // Walk two whole lines; the second must cost exactly one more fill.
  for (int b = 0; b < 2 * kLineBytes; ++b)
    a.push_back(one(kPoolBase + 0x800u + static_cast<uint32_t>(b)));
  CacRun got;
  const bool ok = run(&tc, a, "whole-line", 0, 0, 4, 1, &got);
  check(ok, "whole line: the walk matches zref::TextureCache", 1, ok ? 1 : 0);
  check(got.fills.size() == 2, "whole line: two lines walked cost exactly two fills", 2,
        got.fills.size());
  check(got.misses == 2, "whole line: and exactly two misses", 2, got.misses);
}

// --------------------------------------------------------------------- 5 ---
void test_tag_collision_and_eviction() {
  zref::TextureCache tc;
  fresh(&tc);
  // Same index, different tags: kLines * kLineBytes apart is one full way.
  const uint32_t stride = static_cast<uint32_t>(kLines * kLineBytes);
  const uint32_t a0 = kPoolBase + 0x30u;
  const uint32_t a1 = a0 + stride;
  const uint32_t a2 = a0 + 2u * stride;
  std::vector<CacAccess> a{one(a0), one(a1), one(a0), one(a1), one(a2), one(a0)};
  CacRun got;
  const bool ok = run(&tc, a, "collision", 0, 0, 2, 0, &got);
  check(ok, "collision: the thrash matches zref::TextureCache", 1, ok ? 1 : 0);
  // Direct-mapped: every one of the six accesses evicts the previous tenant.
  check(got.fills.size() == 6, "collision: six colliding accesses cost six fills", 6,
        got.fills.size());
  check(got.hits == 0, "collision: not one of them was a first-look hit", 0, got.hits);
  check(got.misses == 6, "collision: and all six were misses", 6, got.misses);
  // ...and the bytes are still right after every eviction, which is the point:
  // eviction must lose residency, never correctness.
  check(got.out[2].data[0] == pool().halfword(a0),
        "collision: a refetched line still returns the right halfword", pool().halfword(a0),
        got.out[2].data[0]);
}

// --------------------------------------------------------------------- 6 ---
void test_lane_isolation() {
  zref::TextureCache tc;
  fresh(&tc);
  const uint32_t stride = static_cast<uint32_t>(kLines * kLineBytes);
  const uint32_t base = kPoolBase + 0x100u;
  // Four DIFFERENT tags at the SAME index, one per lane: if the lanes shared a
  // tag array this would thrash forever; they do not, so it costs four fills
  // once and nothing thereafter.
  std::vector<CacAccess> a{quad(base, base + stride, base + 2u * stride, base + 3u * stride),
                           quad(base, base + stride, base + 2u * stride, base + 3u * stride)};
  CacRun got;
  const bool ok = run(&tc, a, "lane-isolation", 0, 0, 2, 0, &got);
  check(ok, "lane isolation: the pair matches zref::TextureCache", 1, ok ? 1 : 0);
  check(got.fills.size() == 4, "lane isolation: four colliding tags in four lanes cost four fills",
        4, got.fills.size());
  check(got.hits == 4, "lane isolation: the second access hit on all four lanes", 4, got.hits);

  // The same line resident in two lanes at once is legal duplication.
  zref::TextureCache tc2;
  fresh(&tc2);
  CacRun g2;
  const bool ok2 = run(&tc2, {quad(base, base, base, base), quad(base, base, base, base)},
                       "lane-duplicate", 0, 0, 2, 0, &g2);
  check(ok2, "lane isolation: one line in all four lanes matches zref::TextureCache", 1,
        ok2 ? 1 : 0);
  check(g2.fills.size() == 4, "lane isolation: it is fetched once PER LANE, not once", 4,
        g2.fills.size());
}

// --------------------------------------------------------------------- 7 ---
void test_invalidate() {
  const uint32_t pal = kPoolBase + 0x600u;

  zref::TextureCache tc;
  fresh(&tc);
  std::vector<CacAccess> warm{one(pal), one(pal)};
  CacRun g;
  bool ok = run(&tc, warm, "inv-warm", 0, 0, 2, 0, &g);
  check(ok && g.fills.size() == 1, "invalidate: the page is resident before the upload", 1,
        (ok && g.fills.size() == 1) ? 1 : 0);

  // The stars §1 per-page invalidate.
  dev().invalidate(false, pal);
  tc.invalidate_line(pal);
  ok = run(&tc, {one(pal)}, "inv-line", 0, 0, 2, 0, &g);
  check(ok, "invalidate: the post-invalidate access matches zref::TextureCache", 1, ok ? 1 : 0);
  check(g.fills.size() == 1, "invalidate: the page was refetched — never a stale-frame paint", 1,
        g.fills.size());

  // ...and it does NOT flush a different line.
  const uint32_t other = pal + static_cast<uint32_t>(kLineBytes);
  ok = run(&tc, {one(other), one(other)}, "inv-other", 0, 0, 2, 0, &g);
  check(ok && g.fills.size() == 1, "invalidate: a line invalidate touched only its own index", 1,
        (ok && g.fills.size() == 1) ? 1 : 0);
  dev().invalidate(false, pal);
  tc.invalidate_line(pal);
  ok = run(&tc, {one(other)}, "inv-other-2", 0, 0, 2, 0, &g);
  check(ok && g.fills.empty(), "invalidate: the neighbouring line stayed resident", 1,
        (ok && g.fills.empty()) ? 1 : 0);

  // The resource-epoch flush.
  dev().invalidate(true, 0);
  tc.invalidate_all();
  ok = run(&tc, {one(other), one(pal)}, "inv-all", 0, 0, 2, 0, &g);
  check(ok, "invalidate: the post-flush accesses match zref::TextureCache", 1, ok ? 1 : 0);
  check(g.fills.size() == 2, "invalidate: inv_all emptied every lane", 2, g.fills.size());
}

// --------------------------------------------------------------------- 8 ---
// An invalidate landing while a fill is in flight must cancel that fill's tag
// write: the bytes on the way in are already stale. Driven by hand because it
// is a timing race the batch driver cannot express.
void test_invalidate_beats_a_fill() {
  const uint32_t line = kPoolBase + 0x340u;
  const int last_beat = kLineBytes / 2 - 1;

  // THE CASE THE `fill_kill_r` GUARD ACTUALLY EXISTS FOR: an invalidate on an
  // EARLY beat. Beats 0..b then carry pre-upload bytes and beats b+1..7
  // post-upload ones, so the line in flight is TORN and must never become
  // valid. The LAST beat is NOT this case and does not test the guard — the
  // invalidate clears the valid bit in the same cycle the tag is written, so
  // the line is dead either way. (A mutation removing the guard was GREEN
  // against a last-beat-only test; this loop is the fix.)
  for (int b = 0; b < last_beat; ++b) {
    CacDev d;
    d.reset();
    std::string err;
    d.arm_invalidate_on_beat(b, false, line);
    const CacRun g1 = d.feed({one(line)}, pool(), 0, 0, 2, 0, &err);
    const CacRun g2 = d.feed({one(line)}, pool(), 0, 0, 2, 0, &err);
    check(err.empty(), "inv-vs-fill: no protocol violation on an early-beat invalidate", 1,
          err.empty() ? 1 : 0);
    check(g1.fills.size() == 2,
          "inv-vs-fill: an invalidate on an EARLY beat kills the torn line, so the access "
          "fetches it again",
          2, g1.fills.size());
    check(g2.fills.empty(), "inv-vs-fill: and the second, un-killed fill did stick", 1,
          g2.fills.empty() ? 1 : 0);
    check(g1.out[0].data[0] == pool().halfword(line),
          "inv-vs-fill: the bytes finally served are the un-torn ones", pool().halfword(line),
          g1.out[0].data[0]);
  }

  // The LAST beat is the cycle the tag would be written; an invalidate in that
  // same cycle must win, so the line must NOT become valid. This is not
  // expressible against the oracle (which has no beats), so it asserts the
  // ordering rule directly on the RTL.
  for (int mode = 0; mode < 2; ++mode) {
    CacDev d;
    d.reset();
    std::string err;
    d.arm_invalidate_on_beat(last_beat, mode == 0, mode == 0 ? 0u : line);
    const CacRun g1 = d.feed({one(line)}, pool(), 0, 0, 2, 0, &err);
    const CacRun g2 = d.feed({one(line)}, pool(), 0, 0, 2, 0, &err);
    check(err.empty(), "inv-vs-fill: no protocol violation", 1, err.empty() ? 1 : 0);
    // The killed fill leaves the access UNSERVED, so the block fetches the
    // line again inside the same access — two fills for one lookup, which is
    // the visible proof that the torn line never became valid.
    check(g1.fills.size() == 2,
          "inv-vs-fill: an invalidate on the last beat cancels the tag write, so the SAME "
          "access fetches the line a second time",
          2, g1.fills.size());
    check(g2.fills.empty(), "inv-vs-fill: the second, un-invalidated fill did stick", 1,
          g2.fills.empty() ? 1 : 0);
    check(g1.out[0].data[0] == pool().halfword(line),
          "inv-vs-fill: and the bytes finally served are correct", pool().halfword(line),
          g1.out[0].data[0]);
  }

  // An invalidate on an EARLIER beat is the same ruling; and an invalidate of
  // a DIFFERENT line must not cancel the fill in flight.
  {
    CacDev d;
    d.reset();
    std::string err;
    d.arm_invalidate_on_beat(0, false, line + static_cast<uint32_t>(kLineBytes));
    const CacRun g1 = d.feed({one(line)}, pool(), 0, 0, 2, 0, &err);
    const CacRun g2 = d.feed({one(line)}, pool(), 0, 0, 2, 0, &err);
    check(err.empty(), "inv-vs-fill: no protocol violation on the unrelated invalidate", 1,
          err.empty() ? 1 : 0);
    check(g1.fills.size() == 1 && g2.fills.empty(),
          "inv-vs-fill: invalidating a DIFFERENT line leaves the in-flight fill valid", 1,
          (g1.fills.size() == 1 && g2.fills.empty()) ? 1 : 0);
  }
}

// --------------------------------------------------------------------- 9 ---
void test_four_missing_lanes() {
  zref::TextureCache tc;
  fresh(&tc);
  const uint32_t b = kPoolBase + 0x900u;
  // Four distinct lines, one per lane: four fills, lowest lane first.
  const CacAccess a = quad(b, b + 0x10u, b + 0x20u, b + 0x30u);
  CacRun got;
  const bool ok = run(&tc, {a}, "four-miss", 0, 0, 3, 0, &got);
  check(ok, "four misses: the access matches zref::TextureCache", 1, ok ? 1 : 0);
  check(got.fills.size() == 4, "four misses: one access, four fills", 4, got.fills.size());
  bool ordered = got.fills.size() == 4;
  for (size_t i = 0; ordered && i + 1 < got.fills.size(); ++i)
    ordered = got.fills[i] < got.fills[i + 1];
  check(ordered, "four misses: the fills issue lowest lane first (deterministic order)", 1,
        ordered ? 1 : 0);
  check(got.hits == 0 && got.misses == 4, "four misses: zero first-look hits, four misses", 1,
        (got.hits == 0 && got.misses == 4) ? 1 : 0);

  // A MIXED access: two lanes resident, two not.
  const CacAccess mixed = quad(b, b + 0x10u, b + 0x100u, b + 0x110u);
  const bool ok2 = run(&tc, {mixed}, "mixed-miss", 0, 0, 3, 0, &got);
  check(ok2, "mixed: the access matches zref::TextureCache", 1, ok2 ? 1 : 0);
  check(got.fills.size() == 2, "mixed: only the two missing lanes fetched", 2, got.fills.size());
  // The counters are cumulative across both accesses of this case: 0 + 2 hits
  // and 4 + 2 misses.
  check(got.hits == 2, "mixed: the two resident lanes counted as first-look hits", 2, got.hits);
  check(got.misses == 6, "mixed: and the two cold ones as misses, on top of the first four", 6,
        got.misses);
}

// -------------------------------------------------------------------- 10 ---
void test_backpressure() {
  const uint32_t seeds[6][2] = {{0, 0},         {0xA1u, 0}, {0, 0xB2u},
                                {0xC3u, 0xD4u}, {0xE5u, 0}, {0, 0xF6u}};
  const int lats[3] = {0, 1, 7};
  const int gaps[2] = {0, 2};

  std::vector<CacAccess> a;
  uint32_t s = 0x5EEDu;
  for (int i = 0; i < 48; ++i) {
    const uint32_t addr = kPoolBase + (pcg(&s) % (kPoolSize - 4u));
    if ((i & 1) != 0) {
      a.push_back(quad(addr, addr + 2u, addr + 0x40u, addr + 0x42u));
    } else {
      a.push_back(one(addr));
    }
  }

  std::vector<uint16_t> golden;
  bool first = true;
  bool stable = true;
  for (const uint32_t(&sd)[2] : seeds) {
    for (int lat : lats) {
      for (int gap : gaps) {
        zref::TextureCache tc;
        fresh(&tc);
        CacRun got;
        const bool ok = run(&tc, a, "backpressure", sd[0], sd[1], lat, gap, &got);
        check(ok, "backpressure: the batch matches zref::TextureCache under every pattern", 1,
              ok ? 1 : 0);
        std::vector<uint16_t> flat;
        for (size_t i = 0; i < a.size(); ++i)
          for (int k = 0; k < kLanes; ++k)
            if (a[i].en[k]) flat.push_back(got.out[i].data[k]);
        if (first) {
          golden = flat;
          first = false;
        } else if (flat != golden) {
          stable = false;
        }
      }
    }
  }
  check(stable, "backpressure: not one returned byte moved across 36 timing patterns", 1,
        stable ? 1 : 0);
}

// -------------------------------------------------------------------- 11 ---
void test_counters() {
  zref::TextureCache tc;
  fresh(&tc);
  const uint32_t b = kPoolBase + 0x1200u;
  std::vector<CacAccess> a{one(b), one(b), one(b), one(b + 0x10u),
                           quad(b, b + 0x10u, b, b + 0x10u)};
  CacRun got;
  const bool ok = run(&tc, a, "counters", 0, 0, 2, 0, &got);
  check(ok, "counters: the sequence matches zref::TextureCache", 1, ok ? 1 : 0);
  // 1 miss (b), 2 hits, 1 miss (b+0x10), then a quad: lanes 0 and 2 want b and
  // lanes 1 and 3 want b+0x10 — all four are cold in THEIR OWN lanes except
  // lane 0's b, so the counters are the lane-resident count, not the address
  // count.
  check(got.hits + got.misses == 8,
        "counters: hits + misses is the ENABLED-LANE lookup count (4 single + 4 quad)", 8,
        got.hits + got.misses);
  check(got.misses == static_cast<uint32_t>(got.fills.size()),
        "counters: one miss per LINE FETCHED, exactly", got.fills.size(), got.misses);
  check(got.hits > 0 && got.misses > 0, "counters: the batch really did both", 1,
        (got.hits > 0 && got.misses > 0) ? 1 : 0);
}

}  // namespace

int main() {
  test_reset_is_empty();
  test_miss_then_hit();
  test_halfword_port();
  test_whole_line_from_one_fill();
  test_tag_collision_and_eviction();
  test_lane_isolation();
  test_invalidate();
  test_invalidate_beats_a_fill();
  test_four_missing_lanes();
  test_backpressure();
  test_counters();
  return zhao::report_and_exit("texture_cache_directed");
}
