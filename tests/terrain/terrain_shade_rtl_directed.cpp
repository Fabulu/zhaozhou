// terrain_shade_rtl_directed.cpp — TERRAIN.SHADE RTL against the COMPILED
// ratified law.
//
// THE ORACLE IS `zref::render::shade_flat_tri_dir_unclamped` ITSELF, linked
// from the compiled reference (zhao_zref / build/reference/libzhao_zref.a),
// not a model written to match it. The contract records why that matters:
// the first shade oracle was a second implementation of the law, and its
// twelve passing checks were checking a duplicate against itself.
//
// Two tiers, deliberately:
//
//   TIER 1 (composed law): random and directed TRIANGLES. The normal fed to
//   the DUT is `zref::terrain::face_normal` — the thin view whose values are
//   what zhao_terrain_normals is UNIT_VERIFIED against (41,731 checks) — and
//   the expected base is the compiled `shade_flat_tri_dir_unclamped` on the
//   VERTICES. Passing means: NORMALS' output driven into SHADE reproduces
//   the ratified vertex-to-light law end to end, bit for bit.
//
//   TIER 2 (full port domain): raw normals with no triangle preimage —
//   rails, single LSBs, degenerates — against the law's own back half
//   assembled from the law's own pieces: `shade_nmag2` / `shade_ndot`
//   (the thin view), `isqrt_u64` (qformats §7.2 header), and the COMPILED
//   `div_rhu_s128` (rast.cpp). That composition is exactly the statement
//   sequence of the ratified function body; nothing is re-derived.
//
// THE M10K ARITHMETIC IS SHOWN, NOT ASSERTED: the quarter-square identity
// floor((a+b)^2/4) - floor((a-b)^2/4) == a*b is checked EXHAUSTIVELY over
// all 65,536 byte pairs before any packet is driven.
//
// EVERY COUNTER IS SEEN TO FIRE with an exact-count assertion against a
// C-side tally: triangles_shaded_o, degenerate_count_o, base_sat_o (the
// law's INT32 clamp — reachable because the LAW's domain is any int32
// light, not just unit suns), and degen_mismatch_o (driven by deliberately
// inconsistent degenerate_i, which port stimulus can always produce). No
// mutant is needed: no counter here guards an unreachable state.
//
// POSITIVE CONTROL: --break-oracle corrupts ONE expected base by +1; the
// suite must then FAIL. A checker never seen to fail is not a checker.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_shade.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_shade.hpp"
#include "zref/zref_trig.hpp"

using zhao::check;
using zref::terrain::FaceNormal;
using zref::terrain::NormalVertex;

// The ratified law and its divide, from the COMPILED reference. Declared
// here rather than by including src-internal headers; the signatures are
// reference/src/zrender/internal.hpp:358 and :176, and the linker holds us
// to them.
namespace zref {
namespace render {
int32_t shade_flat_tri_dir_unclamped(int32_t ax, int32_t ay, int32_t az, int32_t bx, int32_t by,
                                     int32_t bz, int32_t cx, int32_t cy, int32_t cz, int32_t lx,
                                     int32_t ly, int32_t lz, SatLedger* L);
int32_t div_rhu_s128(__int128 n, __int128 d);
}  // namespace render
}  // namespace zref

namespace {

constexpr int32_t kOne = 1 << 16;  // 1.0 in Q16.16

// The law's back half, from the law's own pieces (see file header).
int32_t expected_from_normal(const FaceNormal& n, int32_t lx, int32_t ly, int32_t lz) {
  const uint64_t nmag2 = zref::terrain::shade_nmag2(n);
  if (nmag2 == 0) return 0;
  const __int128 ndot = zref::terrain::shade_ndot(n, lx, ly, lz);
  return zref::render::div_rhu_s128(ndot, static_cast<__int128>(zref::isqrt_u64(nmag2)));
}

template <typename Top>
void reset_dut(Top& top) {
  top.rst_n = 0;
  top.tri_valid_i = 0;
  top.base_ready_i = 0;
  top.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
  zhao::tick(top);
}

struct DriveResult {
  int32_t base;
  bool degen;
  uint16_t src;
  int latency;  // accept edge -> base_valid_o high, in cycles
};

// C-side tallies for the exact-count counter assertions.
struct Tally {
  uint64_t shaded = 0;
  uint64_t degen = 0;
  uint64_t sat = 0;
  uint64_t mismatch = 0;
} g_tally;

/** Drive one packet; stall the output side for `stall` cycles first. */
DriveResult drive(Vzhao_terrain_shade& dut, const FaceNormal& n, bool degen_flag, int32_t lx,
                  int32_t ly, int32_t lz, uint16_t src, int stall = 0) {
  // wait for ready
  int guard = 0;
  while (!dut.tri_ready_o) {
    zhao::tick(dut);
    if (++guard > 2000) {
      check(false, "tri_ready_o never rose", 1, 0);
      zhao::exit_hard(zhao::report_and_exit("terrain_shade_rtl_directed"));
    }
  }
  dut.tri_valid_i = 1;
  dut.n_x_i = n.x;
  dut.n_y_i = n.y;
  dut.n_z_i = n.z;
  dut.degenerate_i = degen_flag ? 1 : 0;
  dut.sun_x_i = lx;
  dut.sun_y_i = ly;
  dut.sun_z_i = lz;
  dut.src_id_i = src;
  dut.base_ready_i = 0;
  zhao::tick(dut);  // accept edge
  dut.tri_valid_i = 0;

  int lat = 0;
  while (!dut.base_valid_o) {
    zhao::tick(dut);
    if (++lat > 2000) {
      check(false, "base_valid_o never rose", 1, 0);
      zhao::exit_hard(zhao::report_and_exit("terrain_shade_rtl_directed"));
    }
  }

  DriveResult r;
  r.base = static_cast<int32_t>(dut.base_o);
  r.degen = dut.degenerate_o != 0;
  r.src = static_cast<uint16_t>(dut.src_id_o);
  r.latency = lat;

  // backpressure: the packet must HOLD while the consumer stalls.
  for (int i = 0; i < stall; ++i) {
    zhao::tick(dut);
    check(dut.base_valid_o == 1, "base_valid_o held under stall", 1, dut.base_valid_o);
    check(static_cast<int32_t>(dut.base_o) == r.base, "base_o stable under stall",
          static_cast<uint32_t>(r.base), dut.base_o);
    check(static_cast<uint16_t>(dut.src_id_o) == r.src, "src_id_o stable under stall", r.src,
          dut.src_id_o);
  }
  dut.base_ready_i = 1;
  zhao::tick(dut);
  dut.base_ready_i = 0;
  return r;
}

// Tally what the LAW says this packet does, so counters can be compared
// exactly. Mirrors ST_FIN's arithmetic through the law's own values.
void tally_packet(const FaceNormal& n, bool degen_flag, int32_t lx, int32_t ly, int32_t lz) {
  const uint64_t nmag2 = zref::terrain::shade_nmag2(n);
  const bool degen_law = (nmag2 == 0);
  g_tally.shaded++;
  if (degen_law) g_tally.degen++;
  if (degen_flag != degen_law) g_tally.mismatch++;
  if (!degen_law) {
    const __int128 ndot = zref::terrain::shade_ndot(n, lx, ly, lz);
    const __int128 mag = static_cast<__int128>(zref::isqrt_u64(nmag2));
    __int128 h = ndot + mag / 2;
    __int128 q = h / mag;
    __int128 r = h % mag;
    if (r != 0 && r < 0) q -= 1;
    if (q > INT32_MAX || q < INT32_MIN) g_tally.sat++;
  }
}

/** One composed-law check: vertices -> face_normal -> DUT vs compiled law. */
void check_tri(Vzhao_terrain_shade& dut, const NormalVertex& a, const NormalVertex& b,
               const NormalVertex& c, int32_t lx, int32_t ly, int32_t lz, const char* what,
               int stall = 0, int32_t inject = 0) {
  const FaceNormal n = zref::terrain::face_normal(a, b, c, nullptr);
  const bool degen = n.degenerate;  // the producer's own verdict, as wired
  const int32_t expected =
      zref::render::shade_flat_tri_dir_unclamped(a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z, lx,
                                                 ly, lz, nullptr) +
      inject;
  const DriveResult r = drive(dut, n, degen, lx, ly, lz, 0x51AD, stall);
  tally_packet(n, degen, lx, ly, lz);
  check(r.base == expected, what, static_cast<uint32_t>(expected),
        static_cast<uint32_t>(r.base));
}

/** One full-domain check: raw normal -> DUT vs the law's back half. */
void check_n(Vzhao_terrain_shade& dut, int32_t nx, int32_t ny, int32_t nz, bool degen_flag,
             int32_t lx, int32_t ly, int32_t lz, const char* what, uint16_t src = 0x0001,
             int stall = 0) {
  FaceNormal n;
  n.x = nx;
  n.y = ny;
  n.z = nz;
  const int32_t expected = expected_from_normal(n, lx, ly, lz);
  const DriveResult r = drive(dut, n, degen_flag, lx, ly, lz, src, stall);
  tally_packet(n, degen_flag, lx, ly, lz);
  check(r.base == expected, what, static_cast<uint32_t>(expected),
        static_cast<uint32_t>(r.base));
  check(r.src == src, "src_id rides the packet", src, r.src);
}

// xorshift, deterministic.
uint64_t g_rng = 0x5AD3C0FFEE1157ULL;
uint64_t rnd() {
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 7;
  g_rng ^= g_rng << 17;
  return g_rng;
}
int32_t rnd_s32() { return static_cast<int32_t>(rnd()); }
// biased small values: the near-degenerate / near-rail band the contract
// names as where the divide and the accumulation break.
int32_t rnd_biased() {
  switch (rnd() & 3) {
    case 0: return static_cast<int32_t>(rnd() & 7) - 3;              // -3..4
    case 1: return rnd_s32() >> 16;                                  // small Q16.16
    case 2: return (rnd() & 1) ? INT32_MAX - static_cast<int32_t>(rnd() & 3)
                               : INT32_MIN + static_cast<int32_t>(rnd() & 3);
    default: return rnd_s32();
  }
}

}  // namespace

int main(int argc, char** argv) {
  bool break_oracle = false;
  for (int i = 1; i < argc; ++i)
    if (!std::strcmp(argv[i], "--break-oracle")) break_oracle = true;

  Verilated::commandArgs(argc, argv);

  // ---- 0: THE TABLE LAW, EXHAUSTIVELY -----------------------------------
  // floor((a+b)^2/4) - floor((a-b)^2/4) == a*b for every byte pair. This is
  // the entire arithmetic content of the block's one M10K.
  {
    uint32_t bad = 0;
    for (uint32_t a = 0; a < 256; ++a) {
      for (uint32_t b = 0; b < 256; ++b) {
        const uint32_t s = a + b;
        const uint32_t d = (a >= b) ? a - b : b - a;
        if ((s * s) / 4 - (d * d) / 4 != a * b) bad++;
      }
    }
    check(bad == 0, "quarter-square identity exact over all 65,536 byte pairs", 0, bad);
  }

  Vzhao_terrain_shade dut;
  reset_dut(dut);

  // ---- 1: COLD FILL — table_ready gates ready, and arrives on time ------
  {
    check(dut.table_ready_o == 0, "table not ready at reset", 0, dut.table_ready_o);
    check(dut.tri_ready_o == 0, "tri_ready_o low during fill", 0, dut.tri_ready_o);
    int fill = 0;
    while (!dut.table_ready_o && fill < 600) {
      zhao::tick(dut);
      ++fill;
    }
    check(dut.table_ready_o == 1, "quarter-square fill completes", 1, dut.table_ready_o);
    check(fill <= 513, "fill takes the declared 512 cycles", 512, static_cast<uint32_t>(fill));
    check(dut.tri_ready_o == 1, "ready after fill", 1, dut.tri_ready_o);
    check(dut.idle_o == 1, "idle after fill", 1, dut.idle_o);
  }

  const int32_t KLX = zref::terrain::kShadeLightX;
  const int32_t KLY = zref::terrain::kShadeLightY;
  const int32_t KLZ = zref::terrain::kShadeLightZ;

  // ---- 2: THE OVERHEAD-SUN CASE (the draft-divider killer) --------------
  // A flat +Y face under L=(0,1,0) must be FULLY lit, 0x10000 exactly. The
  // quarantined normalmap draft produced 0 here for every realistic
  // triangle (quotient bits 63..32), and the whole island would have shaded
  // to ambient with every gate passing.
  {
    const NormalVertex a{0, 0, 0}, b{0, 0, kOne}, c{kOne, 0, 0};
    check_tri(dut, a, b, c, 0, kOne, 0, "flat face, overhead sun: exactly 1.0 (0x10000)",
              /*stall=*/3, break_oracle ? 1 : 0);
  }

  // ---- 3: DIRECTED, COMPOSED LAW ----------------------------------------
  {
    const NormalVertex a{0, 0, 0}, b{0, 0, kOne}, c{kOne, 0, 0};
    // the renderer's own key light on a flat face
    check_tri(dut, a, b, c, KLX, KLY, KLZ, "flat face under the renderer's key light");
    // reversed winding: face turned AWAY — the sign the contract preserves
    check_tri(dut, a, c, b, KLX, KLY, KLZ, "reversed winding: negative base, sign preserved");
    // zero-area (b == a): the law's nmag2==0 arm
    check_tri(dut, a, a, c, KLX, KLY, KLZ, "zero-area triangle shades exactly 0");
    // sub-metre lattice cell (the 0.6 m spacing regime that once shaded
    // black): 0.6 m edges, gentle slope
    const int32_t s06 = 39322;  // 0.6 in Q16.16
    const NormalVertex p{0, 1200, 0}, q{s06, 900, 0}, r{0, 1500, s06};
    check_tri(dut, p, q, r, KLX, KLY, KLZ, "sub-metre lattice cell, key light");
    // steep wall
    const NormalVertex w0{0, 0, 0}, w1{0, kOne, 0}, w2{0, 0, kOne};
    check_tri(dut, w0, w1, w2, KLX, KLY, KLZ, "vertical wall, key light");
  }

  // ---- 4: DIRECTED, FULL PORT DOMAIN ------------------------------------
  {
    // the fx16 rails: nmag2 = 3 * (2^31-1)^2 = 1.38e19, PAST signed 64.
    // A signed accumulation wraps to a small norm and a huge shade.
    check_n(dut, INT32_MAX, INT32_MAX, INT32_MAX, false, KLX, KLY, KLZ,
            "all-rail normal: u64 norm does not wrap", 0x0002);
    check_n(dut, INT32_MIN, INT32_MIN, INT32_MIN, false, KLX, KLY, KLZ,
            "INT32_MIN rails: |INT32_MIN| handled exactly", 0x0003);
    check_n(dut, INT32_MIN, INT32_MAX, INT32_MIN, false, KLX, KLY, KLZ,
            "mixed rails", 0x0004);
    // single-LSB normals: where the divide's small-denominator band lives
    check_n(dut, 0, 1, 0, false, KLX, KLY, KLZ, "one-LSB normal", 0x0005);
    check_n(dut, 1, 1, 1, false, KLX, KLY, KLZ, "diagonal LSB normal", 0x0006);
    check_n(dut, 0, -1, 0, false, KLX, KLY, KLZ, "negative one-LSB normal", 0x0007);
    // round-half-up at both signs: |n|=2 with an odd numerator lands on the
    // exact half; the floor of (h/d) disagrees with truncation on the
    // negative side, which is the rounding law the contract calls out.
    check_n(dut, 0, 2, 0, false, 0, 32769, 0, "half-rounding, positive", 0x0008);
    check_n(dut, 0, -2, 0, false, 0, 32769, 0, "half-rounding, negative", 0x0009);
    // degenerate, flagged consistently: base 0, counted
    check_n(dut, 0, 0, 0, true, KLX, KLY, KLZ, "degenerate, consistent flag", 0x000A);
    // THE SEAM MISMATCH DETECTOR, both directions — two operands that
    // arrive by different paths CAN disagree, and the counter must see it:
    check_n(dut, 0, kOne, 0, true, KLX, KLY, KLZ,
            "degenerate_i=1 with a real normal: base still the law's", 0x000B);
    check_n(dut, 0, 0, 0, false, KLX, KLY, KLZ,
            "degenerate_i=0 with a zero normal: base 0 per the law", 0x000C);
    // THE LAW'S INT32 CLAMP, both rails (any int32 sun is in the law's
    // domain; a unit sun cannot reach the clamp, Cauchy-Schwarz):
    check_n(dut, 1, 1, 0, false, INT32_MAX, INT32_MAX, 0,
            "law clamp, positive rail", 0x000D);
    check_n(dut, 1, 1, 0, false, INT32_MIN, INT32_MIN, 0,
            "law clamp, negative rail", 0x000E);
    // sun of zero: base exactly 0 without degeneracy
    check_n(dut, kOne, kOne, kOne, false, 0, 0, 0, "zero sun, zero base", 0x000F);
  }

  // ---- 5: FIXED LATENCY, MEASURED NOT ASSERTED --------------------------
  {
    FaceNormal n;
    n.x = 0;
    n.y = kOne;
    n.z = 0;
    const DriveResult r1 = drive(dut, n, false, KLX, KLY, KLZ, 0x0101);
    tally_packet(n, false, KLX, KLY, KLZ);
    n.x = INT32_MIN;
    n.y = 3;
    n.z = INT32_MAX;
    const DriveResult r2 = drive(dut, n, false, -12345, KLY, 7, 0x0102);
    tally_packet(n, false, -12345, KLY, 7);
    FaceNormal z{};
    const DriveResult r3 = drive(dut, z, true, KLX, KLY, KLZ, 0x0103);
    tally_packet(z, true, KLX, KLY, KLZ);
    check(r1.latency == r2.latency, "latency is data-independent",
          static_cast<uint32_t>(r1.latency), static_cast<uint32_t>(r2.latency));
    check(r1.latency == r3.latency, "degenerate walks the same fixed latency",
          static_cast<uint32_t>(r1.latency), static_cast<uint32_t>(r3.latency));
    std::printf("[shade] fixed latency accept->valid: %d cycles\n", r1.latency);
  }

  // ---- 6: RANDOM DIFFERENTIAL, TIER 1 (triangles) -----------------------
  {
    for (int i = 0; i < 1200; ++i) {
      NormalVertex a, b, c;
      const bool tight = (rnd() & 3) == 0;  // cluster for near-degeneracy
      auto coord = [&]() -> int32_t {
        if (tight) return static_cast<int32_t>(rnd() & 0xFFF) - 0x800;
        // fx16 world band, +-4096 units (the NORMALS contract's domain lane)
        return static_cast<int32_t>(rnd() % (8192LL << 16)) - (4096 << 16);
      };
      a = {coord(), coord(), coord()};
      b = {coord(), coord(), coord()};
      c = {coord(), coord(), coord()};
      if ((rnd() & 15) == 0) b = a;  // exact degenerates too
      check_tri(dut, a, b, c, KLX, KLY, KLZ, "random triangle, key light",
                (i % 37 == 0) ? 2 : 0);
    }
  }

  // ---- 7: RANDOM DIFFERENTIAL, TIER 2 (full normal x sun domain) --------
  int cov_neg = 0, cov_pos = 0;
  {
    for (int i = 0; i < 2500; ++i) {
      FaceNormal n;
      n.x = rnd_biased();
      n.y = rnd_biased();
      n.z = rnd_biased();
      int32_t lx = KLX, ly = KLY, lz = KLZ;
      const uint64_t mode = rnd() & 3;
      if (mode == 1) {  // random full-domain sun (the law's actual domain)
        lx = rnd_s32();
        ly = rnd_s32();
        lz = rnd_s32();
      } else if (mode == 2) {  // moving-sun band: unit-ish suns
        lx = static_cast<int32_t>(rnd() % 131073) - 65536;
        ly = static_cast<int32_t>(rnd() % 131073) - 65536;
        lz = static_cast<int32_t>(rnd() % 131073) - 65536;
      }
      const bool degen_law = (n.x == 0 && n.y == 0 && n.z == 0);
      const bool flag = ((rnd() & 63) == 0) ? !degen_law : degen_law;  // rare mismatches
      const int32_t expected = expected_from_normal(n, lx, ly, lz);
      if (expected < 0) cov_neg++;
      if (expected > 0) cov_pos++;
      const DriveResult r = drive(dut, n, flag, lx, ly, lz,
                                  static_cast<uint16_t>(rnd() & 0xFFFF),
                                  (i % 41 == 0) ? 1 : 0);
      tally_packet(n, flag, lx, ly, lz);
      check(r.base == expected, "random normal/sun differential",
            static_cast<uint32_t>(expected), static_cast<uint32_t>(r.base));
    }
  }

  // ---- 8: EVERY COUNTER FIRED, AND EVERY COUNT IS EXACT -----------------
  {
    check(dut.triangles_shaded_o == g_tally.shaded, "triangles_shaded_o exact",
          g_tally.shaded, dut.triangles_shaded_o);
    check(dut.degenerate_count_o == g_tally.degen, "degenerate_count_o exact",
          g_tally.degen, dut.degenerate_count_o);
    check(dut.base_sat_o == g_tally.sat, "base_sat_o exact", g_tally.sat, dut.base_sat_o);
    check(dut.degen_mismatch_o == g_tally.mismatch, "degen_mismatch_o exact",
          g_tally.mismatch, dut.degen_mismatch_o);
    check(g_tally.degen > 20, "coverage: degenerates sampled", 1, g_tally.degen > 20);
    check(g_tally.sat >= 2, "coverage: the law's INT32 clamp fired both rails", 1,
          g_tally.sat >= 2);
    check(g_tally.mismatch >= 2, "coverage: the seam mismatch detector fired", 1,
          g_tally.mismatch >= 2);
    check(cov_neg > 100, "coverage: negative bases sampled", 1, cov_neg > 100);
    check(cov_pos > 100, "coverage: positive bases sampled", 1, cov_pos > 100);
    std::printf("[shade] tallies: shaded=%llu degen=%llu sat=%llu mismatch=%llu neg=%d pos=%d\n",
                static_cast<unsigned long long>(g_tally.shaded),
                static_cast<unsigned long long>(g_tally.degen),
                static_cast<unsigned long long>(g_tally.sat),
                static_cast<unsigned long long>(g_tally.mismatch), cov_neg, cov_pos);
  }

  dut.final();
  zhao::exit_hard(zhao::report_and_exit("terrain_shade_rtl_directed"));
}
