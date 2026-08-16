// terrain_keel.cpp — the §3.7 keel default tests (deep-keel wave;
// spec/terrain_rules.md §3.7, frozen 2026-08-16).
//
// What each lane would catch (the "could have been red" statement):
//   1. profile — R from SOLID cell centres only (a bigger VOID region does
//      not grow the keel), the 50 m donor floor binding on a small island,
//      R/2 on a big one, the height16 headroom cap cutting BELOW the floor
//      on a tall-spired island. Red on: R measured from vertices or void
//      cells, the floor forgotten, the cap off by the peak sign.
//   2. profile anchors — hand-computed bottoms on the 9x9 island:
//      heart 2560-12800 = -10240; corner (d>=R) -2560; the (4 m,0) vertex
//      -8723 (q16 = 12945, one rounding); the (6 m,0) vertex -6827.
//      Red on: any rounding drift in the thickness product, q off by one.
//   3. shallow is deliberate — the override replaces the depth (a 10 m slab
//      on the small island: heart bottom = 0, rim bottom = 1536), and a
//      missing cell_state refuses (writes nothing).
//   4. breach integration — a deep-keel island does not breach on a
//      49 m dig (through the old 27 m sheet it would have); the same dig at
//      85 m DOES punch through. The keel is what makes breaches read.

#include "render_helpers.hpp"
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
namespace zr = zref::render;

// 9x9 lattice over +-8 m (2 m pitch), flat top `top16`, all 8x8 cells SOLID
zr::TerrainPatch slab9(int16_t top16) {
  zr::TerrainPatch p;
  p.width = p.height = 9;
  p.env_x0 = p.env_z0 = -(8 << 16);
  p.env_x1 = p.env_z1 = (8 << 16);
  p.heights.assign(81, top16);
  p.scar.assign(81, 0);
  p.cell_state.assign(64, zt::kSolid);
  return p;
}

void test_profile() {
  // the small island: cell centres reach sqrt(7^2+7^2) = 9.90 m -> R = 9
  // (isqrt floor); KEEL_DEPTH = max(50, 4) = 50, cap 126-10 = 116 no bind
  zr::TerrainPatch p = slab9(2560);  // flat 10 m top
  const zt::KeelProfile kp = zt::keel_profile(p, 0, 0);
  check(kp.radius_m == 9, "R = 9 (isqrt of 98 m^2, floored)");
  check(kp.peak_m == 10, "peak = 10 m");
  check(kp.depth_m == 50 && kp.depth_raw == 12800, "the 50 m donor floor binds on a small island");

  // a big flat island: 161x161 over +-160 m, all solid, top 20 m:
  // corner cell centre at (159,159) m -> d = 224.8 -> R = 224; R/2 = 112;
  // cap = 126-20 = 106 -> the HEADROOM CAP binds (below R/2, per §3.7)
  zr::TerrainPatch big;
  big.width = big.height = 161;
  big.env_x0 = big.env_z0 = -(160 << 16);
  big.env_x1 = big.env_z1 = (160 << 16);
  big.heights.assign(161 * 161, 5120);
  big.cell_state.assign(160 * 160, zt::kSolid);
  const zt::KeelProfile kb = zt::keel_profile(big, 0, 0);
  check(kb.radius_m == 224, "big island R = 224");
  check(kb.depth_m == 106, "headroom cap 126-peak binds below R/2 (106)");

  // the cap can cut below the floor itself on a spire: top 120 m -> cap 6
  zr::TerrainPatch spire = slab9(120 * 256 / 10 * 10);  // 120 m in raw: 30720
  spire.heights.assign(81, 30720);
  const zt::KeelProfile ks = zt::keel_profile(spire, 0, 0);
  check(ks.depth_m == 6, "a 120 m spire caps the keel at 6 m (rails before beauty)");

  // R comes from SOLID cells only: void the outer ring, R shrinks to
  // sqrt(5^2+5^2) = 7.07 -> 7
  zr::TerrainPatch q = slab9(2560);
  for (int cj = 0; cj < 8; ++cj)
    for (int ci = 0; ci < 8; ++ci)
      if (ci == 0 || cj == 0 || ci == 7 || cj == 7) q.cell_state[cj * 8 + ci] = zt::kVoidAuthored;
  const zt::KeelProfile kq = zt::keel_profile(q, 0, 0);
  check(kq.radius_m == 7, "R measured over SOLID cells only (7 after the ring voids)");
}

void test_bottom_anchors() {
  zr::TerrainPatch p = slab9(2560);
  check(zt::generate_bottom(p, 0, 0), "generate_bottom writes layer C");
  // heart vertex (4,4) at (0,0): q = 0 -> t = K -> bottom = 2560-12800
  check(p.bottom[4 * 9 + 4] == -10240, "heart bottom = 10 m top - 50 m keel (-10240)");
  // corner (8,8): d^2 = 128 m^2 >= R^2 = 81 -> q = 1 -> t = 0.4K = 5120
  check(p.bottom[8 * 9 + 8] == -2560, "corner bottom = rim thickness 20 m (-2560)");
  // vertex (6,4) at (4 m, 0): d^2 = 16; q16 = rhu(16*65536, 81) = 12945;
  // t = rhu(12800*(2621440 + 60*(65536-12945)), 6553600) = 11283
  check(p.bottom[4 * 9 + 6] == 2560 - 11283, "4 m-from-heart vertex: -8723 (one rounding)");
  // vertex (7,4) at (6 m, 0): d^2 = 36; q16 = 29127 -> t = 9387
  check(p.bottom[4 * 9 + 7] == 2560 - 9387, "6 m-from-heart vertex: -6827");
  // the profile is monotone: bottom never rises above the rim value toward
  // the heart (the bitten-apple shape)
  bool monotone = true;
  for (int j = 0; j < 9; ++j)
    for (int i = 0; i < 9; ++i) {
      const int d2 = (i - 4) * (i - 4) + (j - 4) * (j - 4);
      if (d2 <= 32 && p.bottom[j * 9 + i] < p.bottom[4 * 9 + 4] - 1) monotone = false;
    }
  check(monotone, "keel deepest at the heart (bitten-apple profile)");

  // the shallow override is DELIBERATE: 10 m slab -> heart bottom = 0,
  // rim bottom = 2560 - 2560*0.4 = 1536
  zr::TerrainPatch s = slab9(2560);
  check(zt::generate_bottom(s, 0, 0, 2560), "override accepted");
  check(s.bottom[4 * 9 + 4] == 0, "10 m slab: heart bottom = 0");
  check(s.bottom[8 * 9 + 8] == 2560 - 1024, "10 m slab: rim bottom = 1536");

  // no cell_state: the generator refuses, bottom stays empty
  zr::TerrainPatch r9 = slab9(2560);
  r9.cell_state.clear();
  check(!zt::generate_bottom(r9, 0, 0), "no SOLID mask: refused (R undefined)");
  check(r9.bottom.empty(), "refusal writes nothing");
}

void test_breach_needs_depth() {
  // a 49 m dig into the default keel breaches nothing (the pre-freeze
  // authoring dug 30 m through a 22 m sheet); 85 m punches through
  zr::TerrainPatch p = slab9(2560);
  zt::generate_bottom(p, 0, 0);
  const zt::DigStamp dig{0, 0, 6 << 16};
  zt::bake_dig(p, dig, zref::fx16{0}, zref::fx16{49 << 16}, nullptr);
  std::vector<zt::BreachEvent> ev = zt::apply_breach_law(p);
  check(ev.empty(), "49 m dig: the deep keel holds");
  zt::bake_dig(p, dig, zref::fx16{49 << 16}, zref::fx16{85 << 16}, nullptr);
  ev = zt::apply_breach_law(p);
  check(!ev.empty(), "85 m dig: through the keel (a hole through a world)");
}

}  // namespace

int main() {
  test_profile();
  test_bottom_anchors();
  test_breach_needs_depth();
  if (failures == 0) std::printf("terrain_keel: all green\n");
  return failures == 0 ? 0 : 1;
}
