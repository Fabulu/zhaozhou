// star_field.cpp — the procedural starfield: the Noctis sector hash
// transliterated VERBATIM from the imported harness oracle, plus the rarity
// gate and the glint magnitude law (spec/stars_and_flares.md §7).
//
// Law:
//   stars_and_flares.md §7  "Existence/position: frozen by transliteration
//     of harness/oracle.c" — 100,000-unit sectors, ≤1 star each, signed
//     32×32→64 multiply-fold (hi+lo), & 0x1FFFF, per-axis −50000; a
//     coordinate landing EXACTLY on the 50000 sentinel ⇒ no star. Golden
//     vectors: tests/golden/starfield/oracle.bin (three-way verified in the
//     external harness before import; the transliteration below must match
//     byte-exactly BEFORE anything renders — starfield_harness_equivalence).
//   stars_and_flares.md §7  rarity gate (float removed), sector-index
//     domain: e = min(15, (|kx| + 30·|ky| + |kz|)/4000); skip if
//     (kx+ky+kz) & ((1<<e)−1). The ×30 y-crush is the milky-way disc.
//   stars_and_flares.md §7  magnitude: intensity6 = 63 − (rz >> 13).
//
// All arithmetic is uint32 so wrapping is well defined; operands are cast
// to int32 ONLY where the original takes a SIGNED product — that signedness
// is load-bearing (a different high word is a different galaxy).

#include "zref/zref_star.hpp"

namespace zref {
namespace sky {

namespace {

// one 32×32→64 signed multiply with the high half folded into the low,
// exactly as the original's imul + "edx += eax" (oracle.c fold_mul)
inline uint32_t fold_mul(int32_t a, int32_t b) {
  const int64_t r = static_cast<int64_t>(a) * static_cast<int64_t>(b);
  const uint32_t lo = static_cast<uint32_t>(r & 0xFFFFFFFFu);
  const uint32_t hi = static_cast<uint32_t>(static_cast<uint64_t>(r) >> 32);
  return hi + lo;
}

}  // namespace

SectorStar starfield(int32_t kx, int32_t ky, int32_t kz) {
  // sector unit coordinates (wrap-defined via uint32, like the oracle's
  // int32 products in their tested range)
  const uint32_t sect_x = static_cast<uint32_t>(kx) * static_cast<uint32_t>(kSectorUnits);
  const uint32_t sect_y = static_cast<uint32_t>(ky) * static_cast<uint32_t>(kSectorUnits);
  const uint32_t sect_z = static_cast<uint32_t>(kz) * static_cast<uint32_t>(kSectorUnits);

  const uint32_t sum_xz = sect_x + sect_z;
  uint8_t flags = 0;

  uint32_t temp_x = (sum_xz & 0x0001FFFFu) + sect_x;
  if (temp_x == static_cast<uint32_t>(kStarCutoff)) flags |= 1u;
  temp_x -= static_cast<uint32_t>(kStarCutoff);

  uint32_t accum = fold_mul(static_cast<int32_t>(temp_x), static_cast<int32_t>(sum_xz));
  const uint32_t idk = sum_xz + accum;

  uint32_t temp_y = (accum & 0x0001FFFFu) + sect_y;
  if (temp_y == static_cast<uint32_t>(kStarCutoff)) flags |= 2u;
  temp_y -= static_cast<uint32_t>(kStarCutoff);

  accum = fold_mul(static_cast<int32_t>(temp_y), static_cast<int32_t>(idk));

  uint32_t temp_z = (accum & 0x0001FFFFu) + sect_z;
  if (temp_z == static_cast<uint32_t>(kStarCutoff)) flags |= 4u;
  temp_z -= static_cast<uint32_t>(kStarCutoff);

  SectorStar out;
  out.x = static_cast<int32_t>(temp_x);
  out.y = static_cast<int32_t>(temp_y);
  out.z = static_cast<int32_t>(temp_z);
  out.netpos = temp_x + temp_y + temp_z;
  out.no_star = flags;
  return out;
}

bool starfield_rarity_skip(int32_t kx, int32_t ky, int32_t kz) {
  const int64_t ax = kx < 0 ? -static_cast<int64_t>(kx) : kx;
  const int64_t ay = ky < 0 ? -static_cast<int64_t>(ky) : ky;
  const int64_t az = kz < 0 ? -static_cast<int64_t>(kz) : kz;
  int64_t e = (ax + 30 * ay + az) / 4000;
  if (e > 15) e = 15;
  const uint32_t mask = (1u << e) - 1u;
  return (static_cast<uint32_t>(kx + ky + kz) & mask) != 0;
}

uint8_t starfield_intensity6(int64_t rz) {
  if (rz < 0) rz = 0;
  const int64_t v = 63 - (rz >> 13);
  if (v <= 0) return 0;  // caller skips
  return static_cast<uint8_t>(v > 63 ? 63 : v);
}

}  // namespace sky
}  // namespace zref
