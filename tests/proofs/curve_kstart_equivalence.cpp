// curve_kstart_equivalence.cpp -- the proof behind zhao_probe_curve_svc's
// shortened segment search.
//
// THE CLAIM. §3.15's segment search is a pinned six-step compare/select over
// k = 5..0, each step guarded by `mid <= n - 1`. While no step has been taken
// `lo` is still 0, so every LEADING step with (1 << k) > n - 1 fails its guard
// and cannot move `lo`. Those steps are provably no-ops, and the search may
// begin at the largest k with (1 << k) <= n - 1.
//
// WHY IT IS WORTH PROVING RATHER THAN ARGUING. The service's initiation
// interval is 2 x steps + 1, and the Earth programs' curve tables hold seven,
// eight and nine entries -- k_start 2, 2 and 3, so three or four steps instead
// of six. That took the measured II from 13 to 8 and impact_wave from 574,444
// to 543,764 clocks per frame. A wrong "provably" here would be a silently
// wrong segment on some table shape, which no timing measurement would catch.
//
// This is a standalone proof, not a CTest gate: it takes no DUT and answers a
// question about the reference law alone. Run it when the search changes.
//
//     g++ -std=c++17 -O2 tests/proofs/curve_kstart_equivalence.cpp -o kstart
//
// Result 2026-08-30: 28,032 comparisons, zero disagreements.

#include <cstdio>
#include <cstdint>
#include <vector>
// The reference law, verbatim from zfield_steps.hpp.
static int ref_search(const std::vector<int32_t>& x, int32_t clamped) {
  const int n = (int)x.size();
  int lo = 0;
  for (int k = 5; k >= 0; --k) {
    const int mid = lo + (1 << k);
    if (mid <= n - 1 && x[mid] <= clamped) lo = mid;
  }
  return lo;
}
// The same search started at the highest k that can possibly succeed.
static int fast_search(const std::vector<int32_t>& x, int32_t clamped, int k_start) {
  const int n = (int)x.size();
  int lo = 0;
  for (int k = k_start; k >= 0; --k) {
    const int mid = lo + (1 << k);
    if (mid <= n - 1 && x[mid] <= clamped) lo = mid;
  }
  return lo;
}
static int k_start_for(int n) {
  // largest k in 0..5 with (1 << k) <= n - 1; 0 when n < 2.
  for (int k = 5; k >= 0; --k)
    if ((1 << k) <= n - 1) return k;
  return 0;
}
int main() {
  long bad = 0, cases = 0;
  for (int n = 1; n <= 64; ++n) {
    std::vector<int32_t> x(n);
    // Several ascending shapes, including duplicates and huge gaps.
    for (int shape = 0; shape < 4; ++shape) {
      for (int i = 0; i < n; ++i) {
        switch (shape) {
          case 0:
            x[i] = i * 1000;
            break;
          case 1:
            x[i] = i;
            break;
          case 2:
            x[i] = (i / 2) * 7;
            break;  // duplicates
          case 3:
            x[i] = (int32_t)(i * 100000007);
            break;  // wide, wraps sign
        }
      }
      const int ks = k_start_for(n);
      // Probe every entry, every midpoint, and outside both ends.
      for (int i = -2; i <= n + 1; ++i) {
        for (int d = -1; d <= 1; ++d) {
          const int32_t probe = (i < 0) ? INT32_MIN : (i >= n ? INT32_MAX : (int32_t)(x[i] + d));
          if (ref_search(x, probe) != fast_search(x, probe, ks)) ++bad;
          ++cases;
        }
      }
    }
  }
  printf("n = 1..64, four table shapes, every entry +/-1 and both rails:\n");
  printf("  %ld comparisons, %ld disagreements\n", cases, bad);
  printf("  k_start:  n=7 -> %d, n=8 -> %d, n=9 -> %d, n=64 -> %d\n", k_start_for(7),
         k_start_for(8), k_start_for(9), k_start_for(64));
  return bad != 0;
}
