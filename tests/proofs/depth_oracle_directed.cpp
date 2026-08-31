// depth_oracle_directed.cpp — the depth oracle agrees with the GENERATED table
// and with the proof, over the whole domain.
//
// DEPTH_PROFILE_NEXT_STEPS.md step 4 asks for `zref::depth_of` so RTL can be
// differentially tested. This gate is what makes that oracle trustworthy, and
// it deliberately does NOT re-derive the law: a test that restates the formula
// agrees with its own bug. Instead it checks the oracle against
//
//   * the GENERATED table's pinned endpoints (a different implementation, in
//     TypeScript, that solved `scale` independently), and
//   * the PROPERTIES the law must have, which are falsifiable without knowing
//     any particular value.
//
// The properties are the point. "d(1 m) == 0xFFFFFF" can be satisfied by a
// stopped clock; "monotonic non-increasing over 200,000 consecutive raw units,
// with no wrap and a non-zero floor" cannot.
#include <cstdint>
#include <cstdio>

#include "zref/zref_depth.hpp"

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("   FAIL: %s\n", what);
    ++g_failures;
  }
}

}  // namespace

int main() {
  std::printf("depth_oracle_directed: %u generated profiles\n", zref::gen::DEPTH_PROFILE_COUNT);

  for (uint32_t i = 0; i < zref::gen::DEPTH_PROFILE_COUNT; ++i) {
    const zref::gen::DepthProfile& p = zref::gen::DEPTH_PROFILES[i];
    std::printf("\n%u %s: wmin=%llu wmax=%llu scale=%llu\n", i, p.name,
                (unsigned long long)p.wmin_raw, (unsigned long long)p.wmax_raw,
                (unsigned long long)p.scale);

    // ---- 1. the two pinned endpoints, against the GENERATED values --------
    // Cross-implementation: those numbers were solved in TypeScript, these are
    // computed in C++, and neither consulted the other.
    const uint32_t at_min = zref::depth_of_raw(p.wmin_raw, p);
    const uint32_t at_max = zref::depth_of_raw(p.wmax_raw, p);
    std::printf("   d(wmin)=0x%06X (table 0x%06X)   d(wmax)=%u (table %u)\n", at_min, p.d_at_wmin,
                at_max, p.d_at_wmax);
    check(at_min == p.d_at_wmin, "d(wmin) disagrees with the generated table");
    check(at_max == p.d_at_wmax, "d(wmax) disagrees with the generated table");
    check(at_min == zref::kDepthMax, "d(wmin) is not 0xFFFFFF exactly");
    check(at_max != 0, "the far floor reached zero");

    // ---- 2. the clamp is a CLAMP, not a clip -----------------------------
    // Geometry beyond wmax is legal and shares the floor; nearer than wmin
    // shares the pin. Neither may wrap, cull, or produce something else.
    check(zref::depth_of_raw(p.wmin_raw / 2, p) == at_min,
          "nearer than wmin did not clamp to the pin");
    check(zref::depth_of_raw(p.wmax_raw * 4, p) == at_max,
          "beyond wmax did not clamp to the floor");
    check(zref::depth_of_raw(0, p) == at_min, "w = 0 did not clamp to the pin");

    // ---- 3. monotonic non-increasing, geometrically over the whole range --
    // Depth must never rise as things get further away. A single inversion is
    // a z-fighting bug that no screenshot would show.
    {
      uint32_t prev = zref::kDepthMax;
      long steps = 0, breaks = 0;
      for (uint64_t W = p.wmin_raw; W <= p.wmax_raw; W += 1 + W / 1024) {
        const uint32_t d = zref::depth_of_raw(W, p);
        if (d > prev) ++breaks;
        prev = d;
        ++steps;
      }
      std::printf("   monotonic over %ld geometric samples: %s\n", steps, breaks ? "NO" : "yes");
      check(breaks == 0, "depth rose with distance (geometric sweep)");
    }

    // ---- 4. the near band, one raw unit at a time -------------------------
    // Where the reciprocal is steepest and a rounding error is most likely.
    {
      uint32_t prev = zref::kDepthMax;
      long breaks = 0;
      const uint64_t end = p.wmin_raw + 200000;
      for (uint64_t W = p.wmin_raw; W <= end && W <= p.wmax_raw; ++W) {
        const uint32_t d = zref::depth_of_raw(W, p);
        if (d > prev) ++breaks;
        prev = d;
      }
      std::printf("   monotonic over 200000 consecutive near-plane units: %s\n",
                  breaks ? "NO" : "yes");
      check(breaks == 0, "depth rose with distance (near band)");
    }

    // ---- 5. the metres helper agrees with the raw one ---------------------
    check(zref::depth_of(p.wmin_raw / 65536.0, i) == zref::depth_of_raw(p.wmin_raw, p),
          "depth_of(metres) disagrees with depth_of_raw");
  }

  // ---- 6. the test is strict, and ties FAIL --------------------------------
  // Decals use an explicit bias. An epsilon here would make a scene one
  // rounding mode away from flickering.
  check(zref::depth_test(100, 50), "a nearer fragment did not pass");
  check(!zref::depth_test(50, 100), "a further fragment passed");
  check(!zref::depth_test(50, 50), "a TIE passed -- the test must be strict");
  check(zref::depth_test(1, zref::kDepthClear), "nothing beat the clear value");

  // ---- 7. profiles genuinely differ ---------------------------------------
  // If two profiles agreed everywhere, one of them would be dead weight and
  // the ABI decision in step 3 would be pointless.
  {
    const uint64_t w = zref::depth_fx16(4.0);
    const uint32_t a = zref::depth_of_raw(w, 0);
    const uint32_t b = zref::depth_of_raw(w, 2);
    std::printf("\nw = 4 m: WORLD_LONG 0x%06X vs CLOSE 0x%06X\n", a, b);
    check(a != b, "two profiles produced identical depth at 4 m");
  }

  std::printf("\n%s\n", g_failures == 0 ? "[depth_oracle_directed] all checks passed"
                                        : "[depth_oracle_directed] FAILURES ABOVE");
  return g_failures == 0 ? 0 : 1;
}
