// texture_mosaic_random.cpp — randomized Mosaic/fold invariants (deep-keel
// wave; terrain_rules.md §6.2). Not a re-implementation: an independent
// oracle for the pick arithmetic (integer, hand-rolled in u64) plus
// structural laws the fold must hold at ANY coordinate.
//
// What each lane would catch:
//   - pick vs the independent u64 oracle over 100k random (tx,ty,weight)
//     (red on: any constant/compare drift between header and oracle);
//   - fold range/period laws over 100k random u including negatives (red
//     on: out-of-range texel, period != 2 tiles, discontinuity);
//   - determinism: two identical sweeps agree everywhere (red on: any
//     hidden state).

#include "zref/zref_terrain.hpp"

#include <cstdint>
#include <cstdio>

namespace {

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

uint64_t oracle_pick_p(int32_t tx, int32_t ty) {
  // the frozen law multiplies in u32 (wraps mod 2^32) BEFORE the XOR - the
  // oracle must wrap identically or it is testing a different function
  const uint32_t hx = static_cast<uint32_t>(tx) * 73856093u;
  const uint32_t hy = static_cast<uint32_t>(ty) * 19349663u;
  return static_cast<uint64_t>(hx ^ hy) % 255ull;
}

}  // namespace

int main() {
  uint32_t rng = 0x811C9DC5u;
  const auto next = [&rng]() {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };

  // 1. pick vs the independent oracle (weights across the full range)
  int n = 0, mismatch = 0;
  for (int t = 0; t < 100000; ++t) {
    const int32_t tx = static_cast<int32_t>(next() % 4096u) - 2048;
    const int32_t ty = static_cast<int32_t>(next() % 4096u) - 2048;
    const uint8_t w = static_cast<uint8_t>(next() & 0xFFu);
    const uint8_t got = zref::terrain::mosaic_pick(7, 9, w, tx, ty);
    const uint8_t want = (oracle_pick_p(tx, ty) < w) ? 7 : 9;
    ++n;
    if (got != want) ++mismatch;
  }
  std::printf("  pick oracle: %d samples, %d mismatches\n", n, mismatch);
  check(mismatch == 0, "mosaic_pick == independent u64 oracle everywhere");

  // 2. fold laws: range, exact period-2, and the mirror seam continuity
  int range_bad = 0, period_bad = 0, seam_bad = 0;
  for (int t = 0; t < 100000; ++t) {
    const int32_t u_raw = static_cast<int32_t>(next()) >> 8;  // wide negatives too
    const int32_t tx = zref::terrain::mirror_texel(u_raw);
    if (tx < 0 || tx > 63) ++range_bad;
    if (zref::terrain::mirror_texel(u_raw + (2 << 16)) != tx) ++period_bad;  // +2 tiles
    // continuity across the integer mirror turn: fold is 1-Lipschitz in u
    const int32_t tx2 = zref::terrain::mirror_texel(u_raw + 1);
    const int d = tx2 > tx ? tx2 - tx : tx - tx2;
    if (d > 1) ++seam_bad;
  }
  check(range_bad == 0, "fold output always in [0,63]");
  check(period_bad == 0, "fold period is exactly 2 tiles");
  check(seam_bad == 0, "fold is 1-Lipschitz (no tear at the mirror turn)");

  // 3. determinism: the same sweep twice agrees bit-for-bit
  uint32_t acc1 = 0, acc2 = 0;
  rng = 0xDEADBEEFu;
  for (int t = 0; t < 50000; ++t) {
    const int32_t tx = static_cast<int32_t>(next() % 65536u) - 32768;
    const int32_t ty = static_cast<int32_t>(next() % 65536u) - 32768;
    acc1 = acc1 * 31 + zref::terrain::mosaic_pick(1, 2, 128, tx, ty);
    acc1 = acc1 * 31 + static_cast<uint32_t>(zref::terrain::mirror_texel(tx));
  }
  rng = 0xDEADBEEFu;
  for (int t = 0; t < 50000; ++t) {
    const int32_t tx = static_cast<int32_t>(next() % 65536u) - 32768;
    const int32_t ty = static_cast<int32_t>(next() % 65536u) - 32768;
    acc2 = acc2 * 31 + zref::terrain::mosaic_pick(1, 2, 128, tx, ty);
    acc2 = acc2 * 31 + static_cast<uint32_t>(zref::terrain::mirror_texel(tx));
  }
  check(acc1 == acc2, "sweeps are stateless (checksum identical)");

  if (failures == 0) std::printf("texture_mosaic_random: all green\n");
  return failures == 0 ? 0 : 1;
}
