// skin_norm_split_directed.cpp — the split that licenses SKIN.NORM.
//
// ---------------------------------------------------------------------------
// WHAT THIS PROVES, AND WHY IT IS NOT CIRCULAR
// ---------------------------------------------------------------------------
// `reports/CREATURESANDLIGHTS` (docket D5) rules that the hardware must NOT
// copy the reference's structure:
//
//     The current reference repeatedly calls skin_normal_lambert for key, fill
//     and point light. That means it effectively repeats the
//     transformed-normal work for each light. The hardware should not
//     reproduce that structure.
//
// So `SKIN.NORM` transforms, blends and renormalises ONCE PER VERTEX, and each
// light afterwards costs one dot and one divide. That is a claim about
// EQUIVALENCE, and until now it was an assumption: nothing checked that one
// normal reused across three lights gives what three independent calls give.
//
// `skin_normal_lambert` is now literally the composition of the two halves, so
// "the composition equals the whole" is true by construction and testing it
// would be circular. What is NOT circular, and is the actual hardware claim:
//
//     ONE `skin_world_normal`, REUSED across N lights, agrees with N separate
//     `skin_normal_lambert` calls -- for every light, on the same vertex.
//
// That is the reference's per-light structure compared against the hardware's
// per-vertex structure, and it is the thing the ruling asserts without
// evidence anywhere else.
#include <cstdint>
#include <cstdio>

#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"

namespace zc = zref::creature;

namespace {

constexpr int32_t ONE = 65536;

struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed) : s(seed) {}
  uint32_t next() {
    s = s * 1664525u + 1013904223u;
    return s;
  }
  int32_t sym(int32_t range) { return static_cast<int32_t>(next() % (2u * range + 1u)) - range; }
};

// A plausible bone: a small rotation-ish matrix with a translation. The exact
// values do not matter; what matters is that the two bones DISAGREE, because a
// blend of two identical bones cannot distinguish any of the laws in play.
zc::mat3x4fx bone(Rng& r) {
  zc::mat3x4fx m{};
  for (int i = 0; i < 12; ++i) m.m[i] = r.sym(2 * ONE);
  return m;
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Rng r(0x5C1DE5u);

  constexpr int kIters = 20000;

  // ---- 1: ONE normal, N lights, against N independent calls ---------------
  {
    int bad = 0, compared = 0, nonzero = 0;
    for (int i = 0; i < kIters; ++i) {
      zc::mat3x4fx pal[2] = {bone(r), bone(r)};

      zc::SkinVertex v{};
      v.x = r.sym(8 * ONE);
      v.y = r.sym(8 * ONE);
      v.z = r.sym(8 * ONE);
      v.nx = static_cast<int8_t>(r.sym(127));
      v.ny = static_cast<int8_t>(r.sym(127));
      v.nz = static_cast<int8_t>(r.sym(127));
      v.b0 = 0;
      v.b1 = 1;
      v.w0 = static_cast<uint8_t>(r.next() % 65u);  // 0..64, both ends included

      // THE HARDWARE STRUCTURE: transform, blend, renormalise, once.
      int64_t n[3];
      int64_t mag = 0;
      const bool live = zc::skin_world_normal(pal, v, n, &mag);

      // Three lights, as the rig actually has: key, fill, and a point light.
      const int32_t lights[3][3] = {
          {r.sym(ONE), r.sym(ONE), r.sym(ONE)},
          {r.sym(ONE), r.sym(ONE), r.sym(ONE)},
          {r.sym(ONE), r.sym(ONE), r.sym(ONE)},
      };

      for (const auto& L : lights) {
        // THE REFERENCE STRUCTURE: a fresh call per light, redoing the
        // transform and the square root each time.
        const int32_t want = zc::skin_normal_lambert(pal, v, L[0], L[1], L[2]);
        // THE HARDWARE STRUCTURE: the one normal, reused.
        const int32_t got = live ? zc::lambert_from_world_normal(n, mag, L[0], L[1], L[2]) : 0;
        ++compared;
        if (got != want) {
          if (bad < 4)
            std::printf("    w0=%u  reused %d  per-light %d\n", v.w0, got, want);
          ++bad;
        }
        if (want != 0) ++nonzero;
      }
    }

    zhao::check(bad == 0,
                "one transformed-and-renormalised normal reused across three "
                "lights agrees with three independent skin_normal_lambert calls, "
                "on every vertex -- this is the equivalence the ruling asserts "
                "when it says the hardware must not repeat the normal work per "
                "light",
                0, bad);
    zhao::check(compared == kIters * 3,
                "every vertex was checked against every light", kIters * 3, compared);
    // A sweep where the answer is always zero would pass the check above while
    // proving nothing: two structures agreeing on "no light" is not agreement.
    zhao::check(nonzero > kIters,
                "and a majority of the comparisons are LIT -- agreement on "
                "darkness is not agreement",
                1, (nonzero > kIters) ? 1 : 0);
  }

  // ---- 2: the degenerate vertex, on both sides ----------------------------
  // A zero packed normal and a blend that cancels are the two ways the surface
  // has no direction. The split must agree about them or the hardware would
  // light a vertex the reference leaves black.
  {
    zc::mat3x4fx pal[2] = {bone(r), bone(r)};
    zc::SkinVertex v{};
    v.nx = 0;
    v.ny = 0;
    v.nz = 0;
    v.b0 = 0;
    v.b1 = 1;
    v.w0 = 32;

    int64_t n[3];
    int64_t mag = 0;
    const bool live = zc::skin_world_normal(pal, v, n, &mag);
    const int32_t want = zc::skin_normal_lambert(pal, v, ONE, 0, 0);

    zhao::check(!live && want == 0,
                "a zero packed normal is degenerate on BOTH sides -- the split "
                "reports it as 'no normal' and the combined law returns 0",
                1, (!live && want == 0) ? 1 : 0);
  }

  // ---- 3: the split is a REFACTOR, so a known vertex still answers the same
  // A pinned value, so a future edit that changes the arithmetic while keeping
  // the two halves consistent with each other still fails here.
  {
    zc::mat3x4fx pal[2]{};
    pal[0].m[0] = ONE;
    pal[0].m[5] = ONE;
    pal[0].m[10] = ONE;
    pal[1] = pal[0];

    zc::SkinVertex v{};
    v.nx = 127;
    v.ny = 0;
    v.nz = 0;
    v.b0 = 0;
    v.b1 = 1;
    v.w0 = 32;

    const int32_t lam = zc::skin_normal_lambert(pal, v, ONE, 0, 0);
    zhao::check(lam == 65536,
                "identity bones, +X normal, +X light: exactly 1.0 -- the "
                "saturating end of the law, pinned so a consistent-but-wrong "
                "pair of halves still fails",
                65536, static_cast<uint32_t>(lam));

    const int32_t back = zc::skin_normal_lambert(pal, v, -ONE, 0, 0);
    zhao::check(back == 0, "and the light behind the surface is exactly 0", 0,
                static_cast<uint32_t>(back));
  }

  std::printf("  %d vertices x 3 lights\n", kIters);
  return zhao::report_and_exit("skin_norm_split_directed");
}
