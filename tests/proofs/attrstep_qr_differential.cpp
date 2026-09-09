// attrstep_qr_differential.cpp — the single-branch exact q/r stepping
// representation, differentialed against the repo's ACTUAL implementations.
//
// -----------------------------------------------------------------------------
// WHY THIS FILE EXISTS, AND WHY IT LINKS WHAT IT LINKS
// -----------------------------------------------------------------------------
// Every prior document in this chain restated the reference law instead of
// calling it:
//
//   * fpga/rtl/raster/zhao_raster_attrdiv.sv's header claims rast.cpp rounds
//     half AWAY FROM ZERO, "symmetric about zero";
//   * tests/raster/raster_attrdiv_directed.cpp's oracle (`div_rhu`, line 37)
//     is a re-derivation of that claim, not a call into zref;
//   * tests/proofs/attribute_step_equivalence.cpp says in its own words
//     "restated from rast.cpp rather than included";
//   * tests/proofs/attribute_plane_equivalence.cpp likewise: "Restated rather
//     than included".
//
// Nothing in tests/, sim/ or emulator/ links zref::render::div_rhu_s128. The
// actual function (reference/src/zrender/rast.cpp:31, UNCHANGED since the file
// was created at commit 993d8a9d) is
//
//     if (d < 0) { n = -n; d = -d; }
//     q = floor((n + floor(d/2)) / d)         // "floor semantics (§4)"
//
// which rounds ties toward +INFINITY — exactly the floor form the proof file
// rejected as "not the shipped law", and exactly what spec/qformats.md §1 says
// out loud: "ties round toward +infinity". The two laws disagree on every
// NEGATIVE EXACT HALF (n/d = -k - 1/2): zref gives -k, the RTL gives -k-1.
//
// So this differential has THREE sides and no restatements:
//
//   1. the proposed single-branch q/r representation (the thing under test);
//   2. the ACTUAL RTL divider, Vzhao_raster_attrdiv, verilated from the
//      shipped source — the law the RTL raster chain currently enforces;
//   3. the ACTUAL zref divide, zref::render::div_rhu_s128, compiled from
//      reference/src/zrender/rast.cpp in this very build — the law the golden
//      captures embody.
//
// The claims it checks:
//
//   A. q/r law == RTL law, everywhere, including both tie signs.   (must hold)
//   B. q/r law != zref law EXACTLY on negative exact halves, and the
//      difference is EXACTLY +1 on zref's side.                    (measured)
//   C. the walked recurrence (base pair, x-step pair, y-step pair, row-base
//      copy) reproduces the RTL divider at EVERY pixel of a tile with zero
//      per-pixel divides — across sign crossings, ties, and 2^77 numerators.
//   D. a tile base pair is reachable from a per-triangle anchor pair by exact
//      pair-doubling additions alone (the per-triangle caching claim).
//   E. POSITIVE CONTROL: a deliberately wrong tie term IS caught by side 2.
//      A differential that cannot fail is not evidence (broken-instrument law).
//
// Build (see reports/ATTRSTEP_QR_REARCHITECTURE_20260909.md for the recipe):
// verilate zhao_raster_attrdiv, compile this + reference/src/zrender/rast.cpp,
// link build/reference/libzhao_zref.a for rast.cpp's non-divide dependencies.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "internal.hpp"  // zref::render::div_rhu_s128 — the ACTUAL declaration

#include "Vzhao_raster_attrdiv.h"
#include "verilated.h"

using i128 = __int128;

// ---------------------------------------------------------------------------
// the representation under test — exactly as proposed
// ---------------------------------------------------------------------------
// N = q*A + r with 0 <= r < A and q the FLOOR quotient (negative N included).

struct Pair {
  i128 q;  // floor quotient
  i128 r;  // 0 <= r < A
};

static Pair edecomp(i128 n, i128 A) {  // A > 0
  i128 q = n / A;                      // C++ truncates toward zero
  i128 r = n - q * A;
  if (r < 0) {
    --q;
    r += A;
  }
  return Pair{q, r};
}

// The emit-side rounding law, exactly as proposed:
//   rounded = q; if (2r > A) ++rounded; else if (2r == A && q >= 0) ++rounded;
static i128 qr_round(const Pair& p, i128 A) {
  i128 v = p.q;
  if (2 * p.r > A) v += 1;
  else if (2 * p.r == A && p.q >= 0) v += 1;
  return v;
}

// The deliberately WRONG tie term for the positive control (section E): drops
// the q >= 0 guard, which is the floor-form (zref) tie. If the differential
// cannot see this, it cannot see anything.
static i128 qr_round_wrong_tie(const Pair& p, i128 A) {
  i128 v = p.q;
  if (2 * p.r >= A) v += 1;
  return v;
}

// One pair advance: the per-pixel recurrence.
static void pair_step(Pair* p, const Pair& d, i128 A) {
  p->q += d.q;
  p->r += d.r;
  if (p->r >= A) {
    p->r -= A;
    p->q += 1;
  }
}

// Pair negation: -(qA + r) = (-q-1)A + (A-r) for r > 0, (-q)A + 0 for r = 0.
static Pair pair_neg(const Pair& p, i128 A) {
  if (p.r == 0) return Pair{-p.q, 0};
  return Pair{-p.q - 1, A - p.r};
}

// Pair doubling: (2q + carry, 2r mod A).
static Pair pair_double(const Pair& p, i128 A) {
  Pair d{2 * p.q, 2 * p.r};
  if (d.r >= A) {
    d.r -= A;
    d.q += 1;
  }
  return d;
}

static Pair pair_add(const Pair& a, const Pair& b, i128 A) {
  Pair s{a.q + b.q, a.r + b.r};
  if (s.r >= A) {
    s.r -= A;
    s.q += 1;
  }
  return s;
}

// pair * t for a signed integer t, by binary double-and-add. This is the
// per-triangle -> per-tile reachability primitive (section D).
static Pair pair_scale(const Pair& p, int64_t t, i128 A) {
  const bool neg = t < 0;
  uint64_t m = neg ? static_cast<uint64_t>(-t) : static_cast<uint64_t>(t);
  Pair acc{0, 0};
  Pair base = p;
  while (m != 0) {
    if (m & 1) acc = pair_add(acc, base, A);
    base = pair_double(base, A);
    m >>= 1;
  }
  return neg ? pair_neg(acc, A) : acc;
}

// ---------------------------------------------------------------------------
// side 3: the actual zref law, with its saturation noted
// ---------------------------------------------------------------------------
// zref::render::div_rhu_s128 SATURATES to int32; the RTL divider FLAGS
// overflow instead. Every case in this file keeps the quotient in range, and
// section A drives one out-of-range case just to document the boundary.

// ---------------------------------------------------------------------------
// side 2: the actual RTL divider
// ---------------------------------------------------------------------------
struct Rtl {
  Vzhao_raster_attrdiv* m;
  uint64_t cycles = 0;

  explicit Rtl() {
    m = new Vzhao_raster_attrdiv;
    m->clk = 0;
    m->rst_n = 0;
    m->v_valid_i = 0;
    m->r_ready_i = 0;
    for (int i = 0; i < 4; ++i) tick();
    m->rst_n = 1;
    for (int i = 0; i < 2; ++i) tick();
  }
  ~Rtl() {
    m->final();
    delete m;
  }
  void tick() {
    m->clk = 0;
    m->eval();
    m->clk = 1;
    m->eval();
    ++cycles;
  }

  struct Res {
    int32_t q;
    bool ovf;
    uint64_t rem;
  };

  Res divide(i128 n, uint64_t area) {
    // num_i is a 96-bit signed port: three 32-bit words, little-endian.
    const unsigned __int128 un = static_cast<unsigned __int128>(n);
    m->num_i[0] = static_cast<uint32_t>(un);
    m->num_i[1] = static_cast<uint32_t>(un >> 32);
    m->num_i[2] = static_cast<uint32_t>(un >> 64);
    m->area_i = area;  // 47-bit port
    m->v_valid_i = 1;
    // wait for the accept edge
    for (int guard = 0; ; ++guard) {
      m->eval();
      const bool fire = m->v_valid_i && m->v_ready_o;
      tick();
      if (fire) break;
      if (guard > 200) {
        std::printf("FATAL: divider never accepted\n");
        std::exit(2);
      }
    }
    m->v_valid_i = 0;
    m->r_ready_i = 1;
    Res res{};
    for (int guard = 0; ; ++guard) {
      m->eval();
      if (m->r_valid_o) {
        res.q = static_cast<int32_t>(m->q_o);
        res.ovf = m->q_overflow_o != 0;
        res.rem = m->rem_o;
        tick();  // consume
        break;
      }
      tick();
      if (guard > 200) {
        std::printf("FATAL: divider never answered\n");
        std::exit(2);
      }
    }
    m->r_ready_i = 0;
    return res;
  }
};

// ---------------------------------------------------------------------------
// bookkeeping
// ---------------------------------------------------------------------------
struct Counters {
  long checks = 0;
  long qr_vs_rtl_mismatch = 0;
  long qr_vs_zref_mismatch = 0;
  long zref_diff_not_neg_tie = 0;  // a zref difference that is NOT the
                                   // characterized negative-exact-half case
  long ties_pos = 0;
  long ties_neg = 0;
  long crossings = 0;   // rows whose N changes sign mid-row
  long wraps = 0;       // r >= A wrap events in the walk
  long rtl_divides = 0;
};

static void check_one(Rtl& rtl, Counters& c, i128 n, uint64_t area) {
  const i128 A = static_cast<i128>(area);
  const Pair p = edecomp(n, A);
  const i128 mine = qr_round(p, A);
  const Rtl::Res r = rtl.divide(n, area);
  ++c.rtl_divides;
  ++c.checks;
  if (2 * p.r == A) {
    if (p.q >= 0) ++c.ties_pos;
    else ++c.ties_neg;
  }
  if (r.ovf) {
    std::printf("FATAL: unexpected RTL overflow (case construction bug)\n");
    std::exit(2);
  }
  if (mine != static_cast<i128>(r.q)) {
    ++c.qr_vs_rtl_mismatch;
    if (c.qr_vs_rtl_mismatch <= 5)
      std::printf("  QR vs RTL MISMATCH: n=%lld... area=%llu qr=%lld rtl=%ld\n",
                  static_cast<long long>(n), (unsigned long long)area,
                  static_cast<long long>(mine), static_cast<long>(r.q));
  }
  const int32_t z = zref::render::div_rhu_s128(n, A);
  if (mine != static_cast<i128>(z)) {
    ++c.qr_vs_zref_mismatch;
    // characterize: must be a negative exact half, and zref = qr + 1
    const bool neg_tie = (2 * p.r == A) && (p.q < 0);
    if (!neg_tie || static_cast<i128>(z) != mine + 1) ++c.zref_diff_not_neg_tie;
  }
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Rtl rtl;
  Counters c;
  std::mt19937_64 rng(0x5A0'2026'0909ULL);
  int fails = 0;

  // =========================================================================
  std::printf("== A. the law: q/r vs ACTUAL RTL divider vs ACTUAL zref ==\n");
  // directed ties on even areas, both signs, small and enormous
  {
    const uint64_t areas_even[] = {2, 4, 6, 256, 1000, 1u << 20,
                                   (1ULL << 40), (1ULL << 46) - 2};
    for (uint64_t A : areas_even) {
      for (int64_t k : {0LL, 1LL, 2LL, 7LL, 1000LL, 2000000000LL}) {
        const i128 half = static_cast<i128>(A) / 2;
        check_one(rtl, c, static_cast<i128>(k) * static_cast<i128>(A) + half, A);
        check_one(rtl, c, -(static_cast<i128>(k) * static_cast<i128>(A) + half), A);
      }
    }
    // odd areas cannot tie; drive neighbourhood-of-half cases anyway
    const uint64_t areas_odd[] = {1, 3, 7, 255, (1u << 20) | 1, (1ULL << 45) | 1};
    for (uint64_t A : areas_odd) {
      for (int64_t k : {0LL, 1LL, 12345LL}) {
        for (int64_t e : {-1LL, 0LL, 1LL}) {
          const i128 n = static_cast<i128>(k) * static_cast<i128>(A) +
                         static_cast<i128>(A) / 2 + e;
          check_one(rtl, c, n, A);
          check_one(rtl, c, -n, A);
        }
      }
    }
  }
  // random: constructed as q*A + r so the quotient always fits 32 bits
  for (int i = 0; i < 4000; ++i) {
    const int mag = 1 + static_cast<int>(rng() % 46);
    const uint64_t A = (rng() % ((1ULL << mag))) + 1;
    const int64_t q = static_cast<int64_t>(static_cast<int32_t>(rng()));
    const uint64_t r = rng() % A;
    const i128 n = static_cast<i128>(q) * static_cast<i128>(A) + static_cast<i128>(r);
    check_one(rtl, c, n, A);
  }
  std::printf("   %ld checks: qr-vs-RTL mismatches %ld (MUST be 0)\n", c.checks,
              c.qr_vs_rtl_mismatch);
  std::printf("   ties driven: %ld positive, %ld negative (both MUST be > 0)\n",
              c.ties_pos, c.ties_neg);
  std::printf("   qr-vs-zref differences %ld, uncharacterized %ld (MUST be 0)\n",
              c.qr_vs_zref_mismatch, c.zref_diff_not_neg_tie);
  if (c.qr_vs_rtl_mismatch != 0 || c.ties_pos == 0 || c.ties_neg == 0 ||
      c.zref_diff_not_neg_tie != 0)
    ++fails;
  if (c.qr_vs_zref_mismatch == 0) {
    // The zref difference is REAL; a zero here means the tie cases above did
    // not reach it, i.e. this instrument is broken.
    std::printf("   BROKEN INSTRUMENT: no zref divergence seen at driven negative ties\n");
    ++fails;
  }

  // the saturation boundary, documented rather than assumed.
  //
  // MEASURED DEFECT (2026-09-09, found by this differential): the RTL's
  // overflow detector is `|q_r[QPOS-1:32]`, which fires only for quotient
  // magnitudes >= 2^32 — but q_o is 32-bit SIGNED, whose ceiling is 2^31 - 1.
  // A quotient magnitude in [2^31, 2^32) therefore WRAPS INTO THE WRONG SIGN
  // with q_overflow_o = 0. zref saturates the same case to INT32_MAX. Legal
  // S 8.24 attributes span 33 signed bits, so the header's own convexity
  // argument does not exclude this band. Recorded here as measurement; the
  // repair belongs in zhao_raster_attrdiv (check |q_r[QPOS-1:31]|, sign-aware).
  {
    const uint64_t A = 1000;
    const i128 big = (static_cast<i128>(INT32_MAX) + 7) * static_cast<i128>(A);
    const Rtl::Res r = rtl.divide(big, A);
    const int32_t z = zref::render::div_rhu_s128(big, static_cast<i128>(A));
    std::printf("   boundary q=2^31+6: RTL ovf=%d q=%ld ; zref saturates to %ld\n",
                r.ovf ? 1 : 0, static_cast<long>(r.q), static_cast<long>(z));
    if (!r.ovf && r.q < 0)
      std::printf("   ^ DEFECT CONFIRMED: positive quotient in [2^31,2^32) wrapped negative, unflagged\n");
    if (z != INT32_MAX) ++fails;  // zref's saturation is the stable side
  }

  // =========================================================================
  std::printf("== B. the walked recurrence vs the ACTUAL RTL, whole tiles ==\n");
  // Each tile: decompose base/x-step/y-step ONCE, then walk 16x16 with adds
  // only — single branch, no sign tracking, no reseed — and compare EVERY
  // pixel against the RTL divider fed the exact numerator.
  long tile_pixels = 0, tile_zref_diffs = 0;
  long walk_rtl_mismatch = 0;
  auto walk_tile = [&](i128 n_base, i128 dndx, i128 dndy, uint64_t area) {
    const i128 A = static_cast<i128>(area);
    const Pair pbase = edecomp(n_base, A);
    const Pair pdx = edecomp(dndx, A);
    const Pair pdy = edecomp(dndy, A);
    Pair prow = pbase;  // the row base: COPIED at each row start, never re-divided
    for (int y = 0; y < 16; ++y) {
      Pair p = prow;  // row-base copy
      const i128 n_row = n_base + static_cast<i128>(y) * dndy;
      bool was_neg = n_row < 0;
      for (int x = 0; x < 16; ++x) {
        const i128 n = n_row + static_cast<i128>(x) * dndx;
        if ((n < 0) != was_neg) {
          ++c.crossings;
          was_neg = (n < 0);
        }
        // invariants of the representation itself
        if (p.q * A + p.r != n || p.r < 0 || p.r >= A) {
          std::printf("  REPRESENTATION BROKEN at (%d,%d)\n", x, y);
          ++walk_rtl_mismatch;
        }
        const i128 mine = qr_round(p, A);
        const Rtl::Res r = rtl.divide(n, area);
        ++c.rtl_divides;
        ++tile_pixels;
        if (r.ovf || mine != static_cast<i128>(r.q)) {
          ++walk_rtl_mismatch;
          if (walk_rtl_mismatch <= 5)
            std::printf("  WALK vs RTL MISMATCH at (%d,%d): qr=%lld rtl=%ld ovf=%d\n",
                        x, y, static_cast<long long>(mine),
                        static_cast<long>(r.q), r.ovf ? 1 : 0);
        }
        const int32_t z = zref::render::div_rhu_s128(n, A);
        if (static_cast<i128>(z) != mine) {
          ++tile_zref_diffs;
          const bool neg_tie = (2 * p.r == A) && (p.q < 0);
          if (!neg_tie || static_cast<i128>(z) != mine + 1) ++c.zref_diff_not_neg_tie;
        }
        if (x != 15) {
          const i128 r_before = p.r;
          pair_step(&p, pdx, A);
          if (p.r < r_before + pdx.r) {}  // (informational; wrap counted below)
          if (r_before + pdx.r >= A) ++c.wraps;
        }
      }
      pair_step(&prow, pdy, A);  // y-advance of the ROW BASE, adds only
    }
  };

  // directed: sign crossings inside rows, ties woven through the walk,
  // numerators pushed toward the 2^77 setup bound
  walk_tile(-7 * 1000 - 500, 1000, -3000, 2000);          // crosses zero mid-row
  walk_tile(-(static_cast<i128>(1) << 38) - 128, (static_cast<i128>(1) << 33),
            (static_cast<i128>(1) << 34) + 256, 256);      // big steps, crossings
  walk_tile(128, 256, 256, 256);                           // tie every pixel, A=2^8
  walk_tile(-128 - 256 * 8, 256, 256, 512);                // negative ties in-walk
  {
    // near the numerator ceiling: |N| ~ 2^77, area near 2^46
    const uint64_t A = (1ULL << 46) - 4;
    const i128 q0 = -(static_cast<i128>(1) << 30);
    const i128 base = q0 * static_cast<i128>(A) + 12345;
    walk_tile(base, (static_cast<i128>(1) << 45), (static_cast<i128>(1) << 44) + 7, A);
  }
  // random tiles
  for (int t = 0; t < 24; ++t) {
    const int mag = 8 + static_cast<int>(rng() % 38);
    const uint64_t A = (rng() % (1ULL << mag)) + 2;
    const int64_t q0 = static_cast<int64_t>(static_cast<int32_t>(rng())) / 4;
    const i128 base = static_cast<i128>(q0) * static_cast<i128>(A) +
                      static_cast<i128>(rng() % A);
    // steps sized so 15 of them keep the quotient inside 32 bits
    const i128 dx = static_cast<i128>(static_cast<int64_t>(rng() % (A * 3 + 7))) -
                    static_cast<i128>(A);
    const i128 dy = static_cast<i128>(static_cast<int64_t>(rng() % (A * 3 + 7))) -
                    static_cast<i128>(A);
    walk_tile(base, dx, dy, A);
  }
  std::printf("   %ld tile pixels, ZERO walk divides: walk-vs-RTL mismatches %ld (MUST be 0)\n",
              tile_pixels, walk_rtl_mismatch);
  std::printf("   sign crossings walked %ld (MUST be > 0), r-wraps %ld, ties %ld/%ld\n",
              c.crossings, c.wraps, c.ties_pos, c.ties_neg);
  std::printf("   zref per-pixel differences on walked tiles: %ld (uncharacterized %ld)\n",
              tile_zref_diffs, c.zref_diff_not_neg_tie);
  if (walk_rtl_mismatch != 0 || c.crossings == 0 || c.wraps == 0 ||
      c.zref_diff_not_neg_tie != 0)
    ++fails;

  // =========================================================================
  std::printf("== C. per-triangle anchor -> tile base by pair additions only ==\n");
  long pair_checks = 0, pair_mismatch = 0;
  for (int t = 0; t < 2000; ++t) {
    const int mag = 4 + static_cast<int>(rng() % 42);
    const uint64_t A = (rng() % (1ULL << mag)) + 1;
    const i128 Ai = static_cast<i128>(A);
    // ATTRSETUP gradients are multiples of 256 (PIXEL_SHIFT), so the half
    // steps below are exact — the same precondition RASTER.INTERP relies on.
    const i128 dndx = (static_cast<i128>(static_cast<int64_t>(rng())) >> 20) << 8;
    const i128 dndy = (static_cast<i128>(static_cast<int64_t>(rng())) >> 20) << 8;
    const i128 n0 = static_cast<i128>(static_cast<int64_t>(rng()));
    const i128 anchor = n0 + (dndx >> 1) + (dndy >> 1);  // pixel-centre anchor
    const Pair pa = edecomp(anchor, Ai);
    const Pair px = edecomp(dndx, Ai);
    const Pair py = edecomp(dndy, Ai);
    const int64_t tx = static_cast<int64_t>(rng() % 4096) - 2048;
    const int64_t ty = static_cast<int64_t>(rng() % 4096) - 2048;
    const Pair got = pair_add(pa, pair_add(pair_scale(px, tx, Ai),
                                           pair_scale(py, ty, Ai), Ai), Ai);
    const Pair want = edecomp(anchor + dndx * tx + dndy * ty, Ai);
    ++pair_checks;
    if (got.q != want.q || got.r != want.r) {
      ++pair_mismatch;
      if (pair_mismatch <= 3) std::printf("  PAIR-REACH MISMATCH at t=%d\n", t);
    }
  }
  std::printf("   %ld anchor->tile reconstructions, %ld mismatches (MUST be 0)\n",
              pair_checks, pair_mismatch);
  if (pair_mismatch != 0) ++fails;

  // =========================================================================
  std::printf("== D. positive control: the differential CAN fail ==\n");
  // The wrong tie law (zref's own, in fact) must be caught against the RTL on
  // the directed negative ties. If it is not, sections A-C proved nothing.
  {
    long caught = 0;
    const uint64_t A = 256;
    for (int64_t k = 0; k < 8; ++k) {
      const i128 n = -(static_cast<i128>(k) * 256 + 128);
      const Pair p = edecomp(n, static_cast<i128>(A));
      const Rtl::Res r = rtl.divide(n, A);
      ++c.rtl_divides;
      if (qr_round_wrong_tie(p, static_cast<i128>(A)) != static_cast<i128>(r.q)) ++caught;
    }
    std::printf("   wrong-tie law caught on %ld/8 negative ties (MUST be 8)\n", caught);
    if (caught != 8) ++fails;
  }

  std::printf("\n%ld RTL divides driven, %llu sim cycles\n", c.rtl_divides,
              (unsigned long long)rtl.cycles);
  if (fails == 0) {
    std::printf("ALL SECTIONS PASS\n");
    std::printf("  * q/r law == ACTUAL RTL divider everywhere driven, ties included\n");
    std::printf("  * walked recurrence == ACTUAL RTL at every pixel, zero walk divides\n");
    std::printf("  * q/r law != ACTUAL zref at negative exact halves ONLY (zref = qr + 1)\n");
    return 0;
  }
  std::printf("FAILURES: %d\n", fails);
  return 1;
}
