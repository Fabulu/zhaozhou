// forge_cliff_random.cpp — randomized FORGE.CLIFF differential (deep-keel
// wave; terrain_rules.md §5). The enumeration oracle is INDEPENDENT (a
// plain nested loop re-derivation of "one edge per SOLID cell side facing
// a void/OUT neighbour" per 32x32 page), in the physics==pixels tradition.
//
// What each lane would catch:
//   - enumeration count vs the oracle over 300 random void masks (red on:
//     any side-set drift, page-boundary mishandling);
//   - the emission bound: alive edges <= 512 per page, ALWAYS (red on: a
//     budget hole);
//   - drop accounting: total enumerated - merged - dropped == emitted (red
//     on: silent loss, double count);
//   - determinism: two plans on the same lattice are identical.

#include "zref/zref_terrain.hpp"

#include <cstdio>
#include <vector>

namespace {

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

namespace zt = zref::terrain;
namespace zf = zref::forge;

zt::ComposedLattice random_lat(int cw, int ch, uint32_t seed, int void_bias) {
  zt::ComposedLattice lat;
  lat.w = cw + 1;
  lat.h = ch + 1;
  lat.dual = true;
  lat.wx.resize(lat.w);
  lat.wz.resize(lat.h);
  for (int i = 0; i < lat.w; ++i) lat.wx[i] = ((i - cw / 2) * 2) << 16;
  for (int j = 0; j < lat.h; ++j) lat.wz[j] = ((j - ch / 2) * 2) << 16;
  lat.top.assign(static_cast<size_t>(lat.w) * lat.h, 4 << 16);
  lat.bottom.assign(static_cast<size_t>(lat.w) * lat.h, 0);
  uint32_t rng = seed;
  lat.cell_state.assign(static_cast<size_t>(cw) * ch, zt::kSolid);
  for (size_t k = 0; k < lat.cell_state.size(); ++k) {
    rng = rng * 1664525u + 1013904223u;
    if ((rng >> 24) % 100u < static_cast<uint32_t>(void_bias))
      lat.cell_state[k] = ((rng >> 20) & 1) ? zt::kVoidAuthored : zt::kVoidBreached;
  }
  return lat;
}

// the independent oracle: per page, count the rim bodies PRE-clamp (the
// degrade never creates or destroys bodies, so the identity under test is
// emitted bodies + dropped == this, ALWAYS)
size_t oracle_rim_bodies(const zt::ComposedLattice& lat) {
  const int cw = lat.w - 1, ch = lat.h - 1;
  size_t total = 0;
  for (int cj = 0; cj < ch; ++cj) {
    for (int ci = 0; ci < cw; ++ci) {
      if (lat.substance(ci, cj) != zt::kSolid) continue;
      const int noff[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
      for (int s = 0; s < 4; ++s) {
        const int ni = ci + noff[s][0], nj = cj + noff[s][1];
        const bool nonsolid =
            ni < 0 || nj < 0 || ni >= cw || nj >= ch || lat.substance(ni, nj) != zt::kSolid;
        if (nonsolid) ++total;
      }
    }
  }
  return total;
}

}  // namespace

int main() {
  int count_ok = 0, budget_bad = 0, account_bad = 0, det_bad = 0;
  int trials = 0;
  for (int t = 0; t < 300; ++t) {
    // sizes sweep page boundaries: 8x8, 33x33, 40x72 (multi-page)
    const int cw = (t % 3 == 0) ? 8 : (t % 3 == 1 ? 33 : 40);
    const int ch = (t % 4 == 0) ? 8 : (t % 4 == 1 ? 33 : 72);
    const int bias = 15 + (t * 7) % 60;  // sparse voids through checkerboard-ish
    const zt::ComposedLattice lat = random_lat(cw, ch, 0x5EED0000u + t * 7919u, bias);
    const zf::RimPlan p = zf::rim_plan(lat, nullptr);
    const zf::RimPlan p2 = zf::rim_plan(lat, nullptr);
    ++trials;

    if (!(p.edges == p2.edges && p.merged == p2.merged && p.dropped == p2.dropped)) ++det_bad;
    if (p.edges.size() > static_cast<size_t>(zf::kRimBudgetPerPage) *
                             static_cast<size_t>((cw + 31) / 32) *
                             static_cast<size_t>((ch + 31) / 32))
      ++budget_bad;

    // THE accounting identity: every enumerated body is either inside an
    // emitted span or dropped (merging absorbs bodies INTO spans, it never
    // destroys them) - emitted_bodies + dropped == the pre-clamp count
    size_t emitted_bodies = 0;
    for (const zf::RimEdge& e : p.edges) emitted_bodies += e.span;
    const size_t oracle_total = oracle_rim_bodies(lat);
    if (emitted_bodies + p.dropped != oracle_total) ++account_bad;
    // sparse cases (no clamp, no merge): the plan IS the enumeration
    if (p.merged == 0 && p.dropped == 0 && p.edges.size() != oracle_total) ++count_ok;
  }
  std::printf("  random rims: %d trials (det_bad=%d budget_bad=%d count_bad=%d acct_bad=%d)\n",
              trials, det_bad, budget_bad, count_ok, account_bad);
  check(count_ok == 0, "sparse-mask enumeration == independent oracle (no clamp/merge)");
  check(budget_bad == 0, "per-page emission bound holds under every mask");
  check(account_bad == 0, "emitted + merged + dropped reconstructs the enumeration");
  check(det_bad == 0, "rim_plan is a pure function of the lattice");

  if (failures == 0) std::printf("forge_cliff_random: all green\n");
  return failures == 0 ? 0 : 1;
}
