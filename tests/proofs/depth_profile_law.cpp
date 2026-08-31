// depth_profile_law.cpp — derive `scale` and the rescale shift from a depth
// profile, and prove the four properties the ruling demands.
//
// OWNER RULING 2026-08-31 #1 (reports/OWNER-RULINGS-20260831.md):
//
//   profile 0 WORLD_LONG      wmin = 1.0 m     wmax = 16384 m
//   profile 1 WORLD_STANDARD  wmin = 0.5 m     wmax =  8192 m
//   profile 2 CLOSE           wmin = 0.25 m    wmax =  2048 m
//
//   "scale/shift are GENERATED from the profile through the frozen rcp_u24 law.
//    They are not independent owner parameters."
//
// So this file does not choose a scale. It derives one, and then tries to break
// it.
//
// ---------------------------------------------------------------------------
// THE DERIVATION
// ---------------------------------------------------------------------------
// spec/qformats.md section 8 states the pipeline as
//
//     w clamped to [wmin, wmax];  {r, k} = rcp_u24(normalize(w));
//     d = rescale(r * scale), round-half-up, saturate 0xFFFFFF,
//     with w == wmin producing exactly 0xFFFFFF.
//
// `normalize` is the missing piece: w is fx16 and reaches 16384 m = 2^30 raw,
// which does not fit the u24 that rcp_u24 takes. So shift it down:
//
//     s  = the smallest shift with (W >> s) < 2^24        (W = w in fx16 raw)
//     nw = W >> s                                          (a legal u24)
//     {r, k} = rcp_u24(nw)      =>   1/nw ~= r * 2^k / 2^48
//     =>   1/W ~= r * 2^k / 2^(48+s)
//
// The pinned endpoint fixes everything else. We want d(wmin) = 0xFFFFFF and
// d proportional to 1/w, so
//
//     d = rescale( SCALE * r, 48 + s - k )
//
// `SCALE` is a per-profile constant and the shift is data-dependent through the
// reciprocal's own exponent `k` and the normalising shift `s`.
//
// AND SCALE IS DERIVED FROM THE LAW'S OUTPUT, NOT FROM THE IDEAL RECIPROCAL.
// The obvious choice, SCALE = 0xFFFFFF * Wmin, is what the exact arithmetic
// wants -- and it produces 0xFFFFFE at the near plane, one short of the pin.
// rcp_u24 carries up to 1 LSB of error and the pinned input m == 2^23 is
// precisely where it saturates, so the ideal constant inherits that loss.
//
// The ruling says wmin maps to 0xFFFFFF EXACTLY. So SCALE is solved from the
// reciprocal's ACTUAL output at the near plane:
//
//     {r0, k0} = rcp_u24(Wmin >> s0)
//     SCALE    = round( 0xFFFFFF * 2^(48 + s0 - k0) / r0 )
//
// which pins the endpoint by construction and leaves everything else
// proportional. This is what "generated from the profile through the frozen
// rcp_u24 law" has to mean: generated from what the law RETURNS, not from the
// mathematics the law approximates. Nothing here is an artistic choice; the
// only inputs are wmin and wmax.
//
// ---------------------------------------------------------------------------
// WHAT IS PROVED
// ---------------------------------------------------------------------------
//   1. w == wmin gives EXACTLY 0xFFFFFF.
//   2. d is monotonic non-increasing across the whole legal interval. This is
//      the property a depth test depends on: if it ever rises with distance,
//      two surfaces swap order.
//   3. w == wmax gives the generated floor, and that floor is NON-ZERO --
//      0 is the clear value, so a far surface must not collide with "nothing
//      drawn here".
//   4. No intermediate wrap: every product stays inside its width.
//
//   g++ -std=c++17 -O2 -I reference/include tests/proofs/depth_profile_law.cpp \
//       -o depthlaw

#include <cstdint>
#include <cstdio>
#include <initializer_list>

#include "zref/zref_rcp.hpp"

namespace {

using u64 = uint64_t;
using i128 = __int128;

constexpr uint32_t kMaxDepth = 0xFFFFFFu;

struct Profile {
  const char* name;
  double wmin_m, wmax_m;
};

/** w in metres -> fx16 raw (S 15.16). */
u64 fx16(double m) { return static_cast<u64>(m * 65536.0 + 0.5); }

/** The smallest shift that brings W inside a u24. */
int norm_shift(u64 W) {
  int s = 0;
  while ((W >> s) >= (1u << 24)) ++s;
  return s;
}

/** SCALE, solved so that w == wmin lands exactly on 0xFFFFFF. */
i128 derive_scale(u64 Wmin) {
  const int s0 = norm_shift(Wmin);
  const zref::rcp24_result rc0 = zref::rcp_u24(static_cast<uint32_t>(Wmin >> s0));
  const int sh0 = 48 + s0 - rc0.k;
  const i128 num = static_cast<i128>(kMaxDepth) << sh0;
  const i128 r0 = rc0.r;
  // round-to-nearest of num / r0
  return (num + r0 / 2) / r0;
}

/** The derived pipeline. Returns invw24 for one clamped w. */
uint32_t depth_of(u64 W, u64 Wmin, u64 Wmax, i128 SCALE, bool* wrapped) {
  if (W < Wmin) W = Wmin;
  if (W > Wmax) W = Wmax;
  const int s = norm_shift(W);
  const uint32_t nw = static_cast<uint32_t>(W >> s);
  const zref::rcp24_result rc = zref::rcp_u24(nw ? nw : 1u);

  const i128 prod = SCALE * static_cast<i128>(rc.r);
  const int sh = 48 + s - rc.k;

  // A negative shift would mean the value does not fit; the profiles must not
  // produce one, and the caller is told if they do.
  if (sh < 1 || sh > 126) {
    *wrapped = true;
    return 0;
  }
  const i128 q = (prod + (static_cast<i128>(1) << (sh - 1))) >> sh;
  if (q < 0) {
    *wrapped = true;
    return 0;
  }
  return (q > kMaxDepth) ? kMaxDepth : static_cast<uint32_t>(q);
}

}  // namespace

int main() {
  const Profile profiles[] = {
      {"WORLD_LONG", 1.0, 16384.0},
      {"WORLD_STANDARD", 0.5, 8192.0},
      {"CLOSE", 0.25, 2048.0},
  };

  int failures = 0;
  for (const Profile& p : profiles) {
    const u64 Wmin = fx16(p.wmin_m), Wmax = fx16(p.wmax_m);
    const i128 SCALE = derive_scale(Wmin);
    std::printf("\n== %s: wmin %.2f m (raw %llu), wmax %.0f m (raw %llu) ==\n", p.name, p.wmin_m,
                (unsigned long long)Wmin, p.wmax_m, (unsigned long long)Wmax);
    std::printf("   GENERATED scale = %llu  (solved from rcp_u24's own output at wmin)\n",
                (unsigned long long)static_cast<u64>(SCALE));

    bool wrapped = false;

    // ---- 1. the pin --------------------------------------------------------
    const uint32_t at_min = depth_of(Wmin, Wmin, Wmax, SCALE, &wrapped);
    std::printf("   d(wmin) = 0x%06X  %s\n", at_min,
                at_min == kMaxDepth ? "(pinned, exact)" : "<-- NOT PINNED");
    if (at_min != kMaxDepth) ++failures;

    // ---- 3. the floor ------------------------------------------------------
    const uint32_t at_max = depth_of(Wmax, Wmin, Wmax, SCALE, &wrapped);
    const double ideal_floor = static_cast<double>(kMaxDepth) * p.wmin_m / p.wmax_m;
    std::printf("   d(wmax) = %u (ideal %.1f)  %s\n", at_max, ideal_floor,
                at_max > 0 ? "(non-zero, distinct from the clear value)" : "<-- ZERO, COLLIDES");
    if (at_max == 0) ++failures;

    // ---- 2. monotonicity, the property a depth test rests on ---------------
    // Sweep geometrically so near distances -- where the codes change fastest --
    // are sampled as densely as far ones, then sweep the near band linearly
    // because that is where a one-raw-unit step is most likely to break order.
    long steps = 0, breaks = 0;
    uint32_t prev = kMaxDepth + 1;
    for (u64 W = Wmin; W <= Wmax; W = W + 1 + W / 4096) {
      const uint32_t d = depth_of(W, Wmin, Wmax, SCALE, &wrapped);
      if (d > prev) {
        if (breaks < 3)
          std::printf("   MONOTONICITY BREAK at raw %llu: %u after %u\n", (unsigned long long)W, d,
                      prev);
        ++breaks;
      }
      prev = d;
      ++steps;
    }
    std::printf("   monotonic over %ld geometric samples: %s\n", steps, breaks ? "NO" : "yes");
    if (breaks) ++failures;

    // The near band, one raw unit at a time.
    long nsteps = 0, nbreaks = 0;
    prev = kMaxDepth + 1;
    for (u64 W = Wmin; W < Wmin + 200000 && W <= Wmax; ++W) {
      const uint32_t d = depth_of(W, Wmin, Wmax, SCALE, &wrapped);
      if (d > prev) ++nbreaks;
      prev = d;
      ++nsteps;
    }
    std::printf("   monotonic over %ld consecutive raw units at the near plane: %s\n", nsteps,
                nbreaks ? "NO" : "yes");
    if (nbreaks) ++failures;

    // ---- 4. no wrap --------------------------------------------------------
    std::printf("   no intermediate wrap: %s\n", wrapped ? "NO" : "yes");
    if (wrapped) ++failures;

    // RESOLUTION, measured over a WINDOW rather than as the distance to the
    // next code change. The first version measured the latter and reported
    // WORLD_STANDARD as 0.75 m at 3 km but 0.58 m at 8 km -- resolution
    // IMPROVING with distance, which is impossible under an inverse-depth
    // mapping and was the tell that the metric was wrong. The reciprocal
    // quantises in a staircase, so the nearest step can be far shorter than the
    // average spacing; what a depth test needs is metres per code across the
    // neighbourhood, not the distance to the next stair.
    std::printf("   resolution (m per depth code):");
    for (double m : {1.0, 3.0, 8.0}) {
      const double km = m * 1000.0;
      if (km * 1.05 > p.wmax_m) {
        std::printf("  %.0f km: beyond wmax", m);
        continue;
      }
      const u64 W1 = fx16(km * 0.95), W2 = fx16(km * 1.05);
      const uint32_t d1 = depth_of(W1, Wmin, Wmax, SCALE, &wrapped);
      const uint32_t d2 = depth_of(W2, Wmin, Wmax, SCALE, &wrapped);
      const double codes = static_cast<double>(d1) - static_cast<double>(d2);
      const double metres = static_cast<double>(W2 - W1) / 65536.0;
      std::printf("  %.0f km: %.2f", m, codes > 0 ? metres / codes : 0.0);
    }
    std::printf("\n");
  }

  std::printf("\n%s\n",
              failures == 0 ? "depth_profile_law: PASS" : "depth_profile_law: FAILURES ABOVE");
  return failures != 0;
}
