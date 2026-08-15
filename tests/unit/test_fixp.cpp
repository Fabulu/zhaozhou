// test_fixp.cpp — W3 fixed-point library tests (spec/qformats.md).
//
// Coverage (plan W3):
//   - exhaustive sin/cos over all 2^16 angle16 vs the committed golden
//     binary (TS-generated) + the <=1.35 LSB libm bound (qformats.md 7.1)
//   - exhaustive unit8 2^16 pairs vs golden + oracle (qformats.md 3)
//   - boundary corpus: +-0/+-1/+-MAX/MIN/powers of two/1-LSB-from-saturation
//   - __int128 rational oracle over >= 2^20 random pairs per op, exact
//     equality incl. predicted SatLedger counts (qformats.md 3-5)
//   - rcp_u24: committed 2^20 sample vs golden (exact: TS BigInt is an
//     independent implementation of the same frozen integer law) and vs the
//     exact integer inequality (r-1)*m <= 2^47 <= (r+1)*m (qformats.md 6.1);
//     full 2^24-domain sweep + FNV-1a-64 hash behind --rcp-full (nightly)
//   - field_rcp: pinned zero, saturations, power-of-two exactness, >= 2^20
//     random pairs vs the frozen bound (qformats.md 6.2)
//   - angle-wrap identities (exhaustive), saturation monotonicity,
//     SatLedger counts + no-clamp invariance (qformats.md 5)
//   - isqrt exactness, normalize <= 2 LSB vs double oracle (qformats.md 7),
//     smoothstep endpoints, div_exact oracle, mat4 single-rounding row law
//
// Host doubles appear ONLY as test oracles (libm sin, sqrt, division) —
// never inside the deterministic rules under test (charter 29-7).

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "zref/zref_fixp.hpp"
#include "zref/zref_rcp.hpp"
#include "zref/zref_trig.hpp"

using namespace zref;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                               \
  do {                                                            \
    ++g_checks;                                                   \
    if (!(cond)) {                                                \
      ++g_failures;                                               \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                             \
  } while (0)

#define CHECK_EQ(a, b)                                                                     \
  do {                                                                                     \
    ++g_checks;                                                                            \
    auto va_ = (a);                                                                        \
    auto vb_ = (b);                                                                        \
    if (!(va_ == vb_)) {                                                                   \
      ++g_failures;                                                                        \
      std::printf("FAIL %s:%d: %s == %s (got %lld vs %lld)\n", __FILE__, __LINE__, #a, #b, \
                  (long long)va_, (long long)vb_);                                         \
    }                                                                                      \
  } while (0)

// --- deterministic PRNG (xorshift64*, fixed seed) ---------------------------
static uint64_t g_rng = 0x9E3779B97F4A7C15ull;
static uint64_t rng_next() {
  g_rng ^= g_rng >> 12;
  g_rng ^= g_rng << 25;
  g_rng ^= g_rng >> 27;
  return g_rng * 2685821657736338717ull;
}
static uint32_t rng_u32() { return (uint32_t)(rng_next() >> 32); }
static int32_t rng_s32() { return (int32_t)(rng_next() >> 32); }

// --- golden file loading ----------------------------------------------------
static std::vector<uint8_t> read_file(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) {
    std::printf("FATAL: cannot open %s\n", path.c_str());
    std::exit(2);
  }
  std::fseek(f, 0, SEEK_END);
  long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> buf((size_t)sz);
  if (sz > 0 && std::fread(buf.data(), 1, (size_t)sz, f) != (size_t)sz) {
    std::printf("FATAL: short read on %s\n", path.c_str());
    std::exit(2);
  }
  std::fclose(f);
  return buf;
}

static std::string golden(const char* name) { return std::string(ZHAO_GOLDEN_DIR) + "/" + name; }

// --- __int128 rational oracles (qformats.md 3/4) -----------------------------
static int32_t oracle_sat_s32(__int128 v, int* clamped) {
  if (v > (__int128)INT32_MAX) {
    *clamped = 1;
    return INT32_MAX;
  }
  if (v < (__int128)INT32_MIN) {
    *clamped = 1;
    return INT32_MIN;
  }
  *clamped = 0;
  return (int32_t)v;
}
static __int128 oracle_rhu_shift(__int128 x, int k) {  // round-half-up, k >= 1
  return (x + ((__int128)1 << (k - 1))) >> k;
}

// =============================================================================
// sin/cos: exhaustive 2^16 vs golden + identities + libm bound (qformats.md 7.1)
// =============================================================================
static void test_sin_cos() {
  const auto bin = read_file(golden("sin_cos_u16.bin"));
  CHECK(bin.size() == 65536ull * 8);
  const int32_t* rec = reinterpret_cast<const int32_t*>(bin.data());

  double max_lsb = 0.0;
  for (uint32_t a = 0; a < 65536; ++a) {
    const int32_t s = fx_sin(angle16{(uint16_t)a}).raw;
    const int32_t c = fx_cos(angle16{(uint16_t)a}).raw;
    CHECK_EQ(s, rec[2 * a]);
    CHECK_EQ(c, rec[2 * a + 1]);

    const double ang = 2.0 * 3.14159265358979323846 * a / 65536.0;
    const double e1 = std::fabs(s / 65536.0 - std::sin(ang)) * 65536.0;
    const double e2 = std::fabs(c / 65536.0 - std::cos(ang)) * 65536.0;
    if (e1 > max_lsb) max_lsb = e1;
    if (e2 > max_lsb) max_lsb = e2;
  }
  CHECK(max_lsb <= 1.35);  // derived bound 1.31 LSB (qformats.md 7.1)

  // exact identities (qformats.md 7.1), exhaustive
  CHECK_EQ(fx_sin(angle16{0}).raw, 0);
  CHECK_EQ(fx_sin(angle16{0x4000}).raw, 0x10000);
  CHECK_EQ(fx_sin(angle16{0x8000}).raw, 0);
  CHECK_EQ(fx_sin(angle16{0xC000}).raw, -0x10000);
  for (uint32_t a = 1; a < 65536; ++a) {
    const int32_t s = fx_sin(angle16{(uint16_t)a}).raw;
    CHECK_EQ(fx_sin(angle16{(uint16_t)(65536 - a)}).raw, -s);
    CHECK_EQ(fx_sin(angle16{(uint16_t)(32768 - a)}).raw, s);
  }
  std::printf("  sin/cos: exhaustive 2^16 golden OK, max vs libm %.4f LSB\n", max_lsb);
}

// =============================================================================
// sin table endpoint: the quarter-wave index must never read T[257]
// (qformats.md 7.1 "Endpoint guard")
// =============================================================================
// The four angles whose mirrored index is a13 = 0x4000 -> i = 256. Evaluating
// them as CONSTANT EXPRESSIONS is the real regression detector: an unguarded
// SIN_Q16[i + 1] is a hard compile error here ("array subscript 257 is outside
// the bounds of uint32_t [257]"), so a reintroduction cannot even build. The
// exhaustive golden loop above cannot see it — t is 0, so the garbage read is
// multiplied away and every VALUE is correct.
static_assert(fx_sin(angle16{0x4000}).raw == 0x10000, "sin(1/4 turn) = 1");
static_assert(fx_sin(angle16{0xC000}).raw == -0x10000, "sin(3/4 turn) = -1");
static_assert(fx_cos(angle16{0}).raw == 0x10000, "cos(0) = 1");
static_assert(fx_cos(angle16{0x8000}).raw == -0x10000, "cos(1/2 turn) = -1");
// the guard returns T[256] itself, which the table pins at exactly 1.0
static_assert(gen::SIN_Q16[256] == 0x10000u, "quarter-wave endpoint T[256] = 1.0");

static void test_sin_table_endpoint() {
  // the same four at runtime (constant-folding is not the only build mode)
  CHECK_EQ(fx_sin(angle16{0x4000}).raw, 0x10000);
  CHECK_EQ(fx_sin(angle16{0xC000}).raw, -0x10000);
  CHECK_EQ(fx_cos(angle16{0}).raw, 0x10000);
  CHECK_EQ(fx_cos(angle16{0x8000}).raw, -0x10000);
  // fx_cos(a) = fx_sin(a + 0x4000) must still hold ACROSS the guarded index
  CHECK_EQ(fx_cos(angle16{0}).raw, fx_sin(angle16{0x4000}).raw);
  CHECK_EQ(fx_cos(angle16{0x8000}).raw, fx_sin(angle16{0xC000}).raw);
  // the neighbourhood is unaffected: the peak is symmetric about 0x4000, and
  // the interpolated entries below it still climb to it. (0x3FFF itself
  // ROUNDS to 0x10000 — T[255] = 0xFFFF plus the t = 63 sub-tick — so the
  // strict inequality only holds a few ticks out.)
  CHECK_EQ(fx_sin(angle16{0x3FFF}).raw, fx_sin(angle16{0x4001}).raw);
  CHECK_EQ(fx_sin(angle16{0x3F00}).raw, fx_sin(angle16{0x4100}).raw);
  CHECK(fx_sin(angle16{0x3F00}).raw < 0x10000);
  CHECK(fx_sin(angle16{0x3F00}).raw > 0xFF00);
  std::printf("  sin endpoint: i == 256 guarded (constexpr + runtime)\n");
}

// =============================================================================
// unit8: exhaustive 2^16 pairs vs golden + oracle (qformats.md 3)
// =============================================================================
static void test_unit8() {
  const auto bin = read_file(golden("unit8_mul_u8.bin"));
  CHECK(bin.size() == 65536);
  for (uint32_t a = 0; a < 256; ++a) {
    for (uint32_t b = 0; b < 256; ++b) {
      const uint8_t got = unit_mul(unit8{(uint8_t)a}, unit8{(uint8_t)b});
      CHECK_EQ(got, bin[a * 256 + b]);
      uint32_t oracle = ((a * b + 128) >> 8);
      if (oracle > 255) oracle = 255;
      CHECK_EQ(got, oracle);
    }
  }
  std::printf("  unit8: exhaustive 2^16 pairs OK\n");
}

// =============================================================================
// noise2 KAT vs golden (qformats.md 7.5, A1)
// =============================================================================
static void test_noise2() {
  const auto bin = read_file(golden("noise2_kat.bin"));
  CHECK(bin.size() == 1024ull * 20);
  for (uint32_t i = 0; i < 1024; ++i) {
    const uint32_t* rec = reinterpret_cast<const uint32_t*>(&bin[i * 20]);
    const uint32_t x = (uint32_t)((uint64_t)i * 2654435761ull);  // mod 2^32
    const uint32_t y = (uint32_t)((uint64_t)i * 40503ull);
    const uint32_t seed = (uint32_t)((uint64_t)i * 0x9E3779B1ull) + 1u;  // mod 2^32
    CHECK_EQ(x, rec[0]);
    CHECK_EQ(y, rec[1]);
    CHECK_EQ(seed, rec[2]);
    CHECK_EQ(noise2_hash(x, y, seed, 0), rec[3]);
    CHECK_EQ(noise2_hash(x, y, seed, 1), rec[4]);
  }
  std::printf("  noise2: 1024 KAT records OK\n");
}

// =============================================================================
// boundary corpus + __int128 rational oracle for add/sub/mul/mad (qformats.md 3)
// =============================================================================
static std::vector<int32_t> boundary_corpus() {
  std::vector<int32_t> v;
  const int64_t spots[] = {0,
                           1,
                           -1,
                           2,
                           -2,
                           3,
                           -3,
                           0x10000,
                           -0x10000,
                           0x8000,
                           -0x8000,
                           INT32_MAX,
                           INT32_MIN,
                           INT32_MAX - 1,
                           INT32_MIN + 1,
                           0x7FFF0000,
                           -0x80000000,
                           0x55555555,
                           -0x55555555,
                           0x7FFFFFFE,
                           -0x7FFFFFFF,
                           0x2AAAAAAA,
                           -0x2AAAAAAB};
  for (int64_t s : spots) v.push_back((int32_t)s);
  for (int s = 0; s < 31; ++s) {
    v.push_back((int32_t)(INT64_C(1) << s));
    v.push_back((int32_t) - (INT64_C(1) << s));
    v.push_back((int32_t)((INT64_C(1) << s) - 1));
    v.push_back((int32_t)(-(INT64_C(1) << s) + 1));
  }
  return v;
}

static void check_add(int32_t a, int32_t b) {
  SatLedger L;
  const fx16 r = fx_add(fx16{a}, fx16{b}, &L);
  int clamped = 0;
  const int32_t want = oracle_sat_s32((__int128)a + b, &clamped);
  CHECK_EQ(r.raw, want);
  CHECK_EQ(L.add, (uint32_t)clamped);
  CHECK_EQ(L.total(), L.add);                             // nothing else recorded
  CHECK_EQ(fx_add(fx16{a}, fx16{b}, nullptr).raw, want);  // no-ledger invariance
}

static void check_mul(int32_t a, int32_t b) {
  SatLedger L;
  const fx16 r = fx_mul(fx16{a}, fx16{b}, &L);
  int clamped = 0;
  const int32_t want = oracle_sat_s32(oracle_rhu_shift((__int128)a * b, 16), &clamped);
  CHECK_EQ(r.raw, want);
  CHECK_EQ(L.mul, (uint32_t)clamped);
  CHECK_EQ(fx_mul(fx16{a}, fx16{b}, nullptr).raw, want);
}

static void check_mad(int32_t a, int32_t b, int32_t c) {
  SatLedger L;
  const fx16 r = fx_mad(fx16{a}, fx16{b}, fx16{c}, &L);
  int clamped = 0;
  const __int128 exact = (__int128)a * b + ((__int128)c << 16);  // single rounding (A3b)
  const int32_t want = oracle_sat_s32(oracle_rhu_shift(exact, 16), &clamped);
  CHECK_EQ(r.raw, want);
  CHECK_EQ(L.mul, (uint32_t)clamped);
  CHECK_EQ(fx_mad(fx16{a}, fx16{b}, fx16{c}, nullptr).raw, want);
}

static void test_fx16_oracles() {
  const auto corpus = boundary_corpus();
  for (int32_t a : corpus)
    for (int32_t b : corpus) {
      check_add(a, b);
      check_mul(a, b);
      check_mad(a, b, b);
      check_mad(a, b, a);
    }

  // >= 2^20 random pairs per op (plan W3)
  for (int i = 0; i < (1 << 20); ++i) {
    const int32_t a = rng_s32(), b = rng_s32(), c = rng_s32();
    check_add(a, b);
    check_mul(a, b);
    check_mad(a, b, c);
  }
  // 1-LSB-from-saturation stress: products that land within 1 of the clamp
  for (int i = 0; i < (1 << 16); ++i) {
    const int32_t big = INT32_MAX - (i & 0xFF);
    const int32_t neg = INT32_MIN + (i & 0xFF);
    check_mul(big, big);
    check_mul(neg, neg);
    check_mad(big, big, big);
    check_mad(neg, neg, neg);
    check_add(big, 1);
    check_add(neg, -1);
  }
  std::printf("  fx16 add/sub/mul/mad: boundary corpus + 2^20 rational-oracle pairs OK\n");
}

// =============================================================================
// fx24 lanes (s128 products; reference-only, qformats.md 3)
// =============================================================================
static void test_fx24() {
  const int64_t spots[] = {0,
                           1,
                           -1,
                           0x1000000,
                           -0x1000000,
                           INT64_MAX,
                           INT64_MIN,
                           INT64_MAX / 2 + 1,
                           INT64_MIN / 2,
                           0x7FFFFFFFFFFFFF00ll};
  for (int64_t a : spots)
    for (int64_t b : spots) {
      SatLedger L;
      const fx24 r = fx24_mul(fx24{a}, fx24{b}, &L);
      const __int128 shifted = oracle_rhu_shift((__int128)a * b, 24);
      int clamped = 0;
      const int64_t want = shifted > (__int128)INT64_MAX   ? (clamped = 1, INT64_MAX)
                           : shifted < (__int128)INT64_MIN ? (clamped = 1, INT64_MIN)
                                                           : (int64_t)shifted;
      CHECK_EQ(r.raw, want);
      CHECK_EQ(L.mul, (uint32_t)clamped);
    }
  for (int i = 0; i < (1 << 20); ++i) {
    const int64_t a = (int64_t)rng_next(), b = (int64_t)rng_next();
    SatLedger L;
    const fx24 r = fx24_mul(fx24{a}, fx24{b}, &L);
    __int128 q = oracle_rhu_shift((__int128)a * b, 24);
    const int64_t want = q > INT64_MAX ? INT64_MAX : q < INT64_MIN ? INT64_MIN : (int64_t)q;
    CHECK_EQ(r.raw, want);
  }
  // fx24_add / fx24_mad spot + random
  for (int i = 0; i < (1 << 20); ++i) {
    const int64_t a = (int64_t)rng_next(), b = (int64_t)rng_next(), c = (int64_t)rng_next();
    CHECK_EQ(fx24_add(fx24{a}, fx24{b}, nullptr).raw, a > 0 && b > INT64_MAX - a   ? INT64_MAX
                                                      : a < 0 && b < INT64_MIN - a ? INT64_MIN
                                                                                   : a + b);
    __int128 q = oracle_rhu_shift((__int128)a * b + ((__int128)c << 24), 24);
    const int64_t want = q > INT64_MAX ? INT64_MAX : q < INT64_MIN ? INT64_MIN : (int64_t)q;
    CHECK_EQ(fx24_mad(fx24{a}, fx24{b}, fx24{c}, nullptr).raw, want);
  }
  std::printf("  fx24 mul/mad/add: corpus + 2^20 oracle pairs OK\n");
}

// =============================================================================
// fx_div_exact (qformats.md 3)
// =============================================================================
static void test_div_exact() {
  SatLedger L;
  const fx16 pin_pos = fx_div_exact(fx16{5}, fx16{0}, &L);
  const fx16 pin_neg = fx_div_exact(fx16{-5}, fx16{0}, &L);
  CHECK_EQ(pin_pos.raw, INT32_MAX);
  CHECK_EQ(pin_neg.raw, INT32_MIN);
  CHECK_EQ(L.rcp0, 2u);

  for (int i = 0; i < (1 << 20); ++i) {
    int32_t a = rng_s32();
    int32_t b = rng_s32();
    if (a == 0) a = 1 << 16;  // sprinkle exact 1.0 numerators
    if (b == 0) b = 1;
    const fx16 r = fx_div_exact(fx16{a}, fx16{b}, nullptr);
    // oracle: round_half_up((s128)a << 16 / b)
    __int128 num = (__int128)a << 16;
    int64_t d = b;
    if (d < 0) {
      num = -num;
      d = -d;
    }
    __int128 h = num + d / 2;
    __int128 q = h / d;
    if (h % d != 0 && h % d < 0) q -= 1;
    const int32_t want = q > INT32_MAX ? INT32_MAX : q < INT32_MIN ? INT32_MIN : (int32_t)q;
    CHECK_EQ(r.raw, want);
  }
  std::printf("  fx_div_exact: pinned zero + 2^20 oracle pairs OK\n");
}

// =============================================================================
// rcp_u24 (qformats.md 6.1)
// =============================================================================
static void test_rcp24_sample() {
  const auto bin = read_file(golden("rcp24_sample.bin"));
  CHECK(bin.size() == (1ull << 20) * 8);
  const uint32_t* rec = reinterpret_cast<const uint32_t*>(bin.data());
  for (uint32_t i = 0; i < (1u << 20); ++i) {
    const uint32_t d = rec[2 * i];
    const uint32_t want = rec[2 * i + 1];
    CHECK(d >= 1 && d <= 0xFFFFFF);
    const rcp24_result got = rcp_u24(d);
    CHECK_EQ(got.r, want);  // exact: independent TS-BigInt implementation

    // integer inequality |r - 2^47/m| <= 1 for the normalized core
    uint32_t m = d;
    while ((m & (1u << 23)) == 0) m <<= 1;
    const __int128 target = (__int128)1 << 47;
    CHECK((__int128)(got.r - 1) * m <= target);
    CHECK(target <= (__int128)(got.r + 1) * m);
  }
  // pinned saturating input: m == 2^23 (d a single bit at 23 after norm)
  CHECK_EQ(rcp_u24_norm(1u << 23), 0xFFFFFFu);
  // wrapper exponent reconstruction on powers of ten-ish values (double oracle)
  for (uint32_t d = 1; d <= 0xFFFFFF; d = d * 3 + 7) {
    const rcp24_result r = rcp_u24(d);
    const double got = (double)r.r / 16777216.0 * std::pow(2.0, r.k);
    const double ex = 16777216.0 / (double)d;
    CHECK(std::fabs(got - ex) / ex < 1e-6);
  }
  std::printf("  rcp_u24: 2^20 golden sample exact + bound + wrapper OK\n");
}

static void test_rcp24_full() {
  uint64_t h = 14695981039346656037ull;
  const __int128 target = (__int128)1 << 47;
  uint64_t max_viol = 0;
  for (uint64_t d = 1; d < (1ull << 24); ++d) {
    const uint32_t r = rcp_u24((uint32_t)d).r;
    for (int b = 0; b < 3; ++b) {
      h ^= (r >> (8 * b)) & 0xFF;
      h *= 1099511628211ull;
    }
    // bound on the normalized core
    uint32_t m = (uint32_t)d;
    while ((m & (1u << 23)) == 0) m <<= 1;
    if (!((__int128)(r - 1) * m <= target && target <= (__int128)(r + 1) * m)) {
      ++max_viol;
    }
  }
  CHECK_EQ(max_viol, 0ull);
  if (h != RCP24_FULL_HASH) {
    ++g_failures;
    std::printf("FAIL rcp_u24 full-domain hash %016llx != frozen %016llx\n", (unsigned long long)h,
                (unsigned long long)RCP24_FULL_HASH);
    ++g_checks;
  } else {
    ++g_checks;
  }
  std::printf("  rcp_u24: full 2^24 domain OK, hash %016llx\n", (unsigned long long)h);
}

// =============================================================================
// field_rcp (qformats.md 6.2)
// =============================================================================
static void test_field_rcp() {
  SatLedger L;
  const fx16 zero = field_rcp(fx16{0}, &L);
  CHECK_EQ(zero.raw, INT32_MAX);  // pinned 0x7FFFFFFF
  CHECK_EQ(L.rcp0, 1u);
  CHECK_EQ(L.rcp, 0u);

  // powers of two are exact (2^32 / 2^s), saturating only at |a| <= 2
  for (int s = 2; s <= 30; ++s) {
    const int64_t a = (INT64_C(1) << s);
    const int64_t exact = (INT64_C(1) << 32) / a;
    CHECK_EQ(field_rcp(fx16{(int32_t)a}, nullptr).raw, (int32_t)exact);
    CHECK_EQ(field_rcp(fx16{-(int32_t)a}, nullptr).raw, -(int32_t)exact);
  }
  CHECK_EQ(field_rcp(fx16{2}, nullptr).raw, INT32_MAX);   // 2^31 saturates +1 error
  CHECK_EQ(field_rcp(fx16{-2}, nullptr).raw, INT32_MIN);  // exact -2^31
  CHECK_EQ(field_rcp(fx16{1}, nullptr).raw, INT32_MAX);
  CHECK_EQ(field_rcp(fx16{-1}, nullptr).raw, INT32_MIN);
  CHECK_EQ(field_rcp(fx16{INT32_MAX}, nullptr).raw, 2);
  CHECK_EQ(field_rcp(fx16{INT32_MIN}, nullptr).raw, -2);

  // frozen bound: |r - 2^32/a| <= |2^32/a| * 2^-14 + 1  (double oracle)
  double max_rel = 0.0;
  for (int i = 0; i < (1 << 20); ++i) {
    int32_t a = rng_s32();
    if (a == 0) a = 1;
    if (i < 64) a = 1 + (int32_t)(rng_u32() % 0x100000);  // dense small-|a| region
    const double ex = 4294967296.0 / (double)a;
    const double got = (double)field_rcp(fx16{a}, nullptr).raw;
    const double err = std::fabs(got - ex);
    CHECK(err <= std::fabs(ex) * (1.0 / 16384.0) + 1.0);  // bound uses |ex|
    if (ex >= 65536.0 && err / ex > max_rel) max_rel = err / ex;
  }
  std::printf(
      "  field_rcp: pinned/sat/exact powers + 2^20 bound pairs OK (max rel %.2e at |r|>=1)\n",
      max_rel);
}

// =============================================================================
// angle wrap + monotonicity + ledger discipline (qformats.md 3/5)
// =============================================================================
static void test_angle_wrap() {
  for (uint32_t a = 0; a < 65536; ++a) {
    const uint16_t au = (uint16_t)a;
    for (const uint16_t b : {uint16_t(0), uint16_t(1), uint16_t(0x4000), uint16_t(0x8000),
                             uint16_t(0xC000), uint16_t(0xFFFF)}) {
      CHECK_EQ(ang_add(angle16{au}, angle16{b}).raw, (uint16_t)(au + b));
      CHECK_EQ(ang_sub(angle16{au}, angle16{b}).raw, (uint16_t)(au - b));
      CHECK_EQ(ang_sub(angle16{au}, angle16{au}).raw, 0);
    }
    // quarter-turn rotation turns sin into cos exactly
    CHECK_EQ(fx_sin(ang_add(angle16{au}, angle16{0x4000})).raw, fx_cos(angle16{au}).raw);
  }
  std::printf("  angle16: wrap identities over all 2^16 angles OK\n");
}

static void test_monotonicity() {
  for (int i = 0; i < (1 << 18); ++i) {
    const int32_t a = rng_s32();
    const int32_t b = rng_s32();
    const int32_t c1 = (int32_t)rng_u32();
    const int32_t c2 = c1 + 1;
    CHECK(fx_add(fx16{a}, fx16{c1}, nullptr).raw <= fx_add(fx16{a}, fx16{c2}, nullptr).raw);
    CHECK(fx_sub(fx16{a}, fx16{c2}, nullptr).raw <= fx_sub(fx16{a}, fx16{c1}, nullptr).raw);
    if (b > 0) {  // mul by a positive constant is non-decreasing in a
      CHECK(fx_mul(fx16{c1}, fx16{b}, nullptr).raw <= fx_mul(fx16{c2}, fx16{b}, nullptr).raw);
    }
    // rescale monotone (k = 16)
    const int64_t x1 = rng_next() >> 8;
    const int64_t x2 = x1 + 1;
    CHECK(rescale_s32(x1, 16, nullptr) <= rescale_s32(x2, 16, nullptr));
  }
  std::printf("  monotonicity: add/sub/mul/rescale 2^18 triples OK\n");
}

static void test_sat_ledger() {
  SatLedger L;
  fx_add(fx16{INT32_MAX}, fx16{1}, &L);  // add clamp
  fx_sub(fx16{INT32_MIN}, fx16{1}, &L);  // add clamp
  CHECK_EQ(L.add, 2u);
  fx_mul(fx16{INT32_MAX}, fx16{INT32_MAX}, &L);  // mul clamp
  CHECK_EQ(L.mul, 1u);
  CHECK_EQ(rescale_s32(INT64_MAX, 16, &L), INT32_MAX);
  CHECK_EQ(L.rescale, 1u);
  unit8_from_fx16(fx16{-1}, &L);
  unit8_from_fx16(fx16{0x20000}, &L);  // > 1.0 clamps
  CHECK_EQ(L.unit, 2u);
  field_rcp(fx16{1}, &L);  // saturates
  CHECK_EQ(L.rcp, 1u);
  field_rcp(fx16{0}, &L);  // pinned zero
  CHECK_EQ(L.rcp0, 1u);
  CHECK_EQ(L.total(), 2u + 1u + 1u + 2u + 1u + 1u);

  // no-clamp invariance: identical results, zero counts
  SatLedger L0;
  CHECK_EQ(fx_add(fx16{123}, fx16{456}, &L0).raw, fx_add(fx16{123}, fx16{456}, nullptr).raw);
  CHECK_EQ(L0.total(), 0u);
  std::printf("  SatLedger: scripted counts + no-clamp invariance OK\n");
}

// =============================================================================
// isqrt / normalize / smoothstep / conversions (qformats.md 2/7/9)
// =============================================================================
static void test_isqrt() {
  const uint64_t spots[] = {0,
                            1,
                            2,
                            3,
                            4,
                            5,
                            8,
                            9,
                            15,
                            16,
                            24,
                            25,
                            0xFFFFFFFFull,
                            0xFFFFFFFEull,
                            (1ull << 32) - 2,
                            1000000007ull};
  for (uint64_t n : spots) {
    const uint32_t r = isqrt_u32((uint32_t)n);
    CHECK((uint64_t)r * r <= n && n < (uint64_t)(r + 1) * (r + 1));
    const uint64_t r64 = isqrt_u64(n);
    CHECK(r64 * r64 <= n && n < (r64 + 1) * (r64 + 1));
  }
  for (int i = 0; i < (1 << 20); ++i) {
    const uint32_t n = rng_u32();
    const uint32_t r = isqrt_u32(n);
    CHECK((uint64_t)r * r <= n && n < (uint64_t)(r + 1) * (r + 1));
    const uint64_t n64 = rng_next();
    const uint64_t r64 = isqrt_u64(n64);
    CHECK(r64 * r64 <= n64 && n64 < (r64 + 1) * (r64 + 1));
  }
  // exact squares
  for (int i = 0; i < 65536; ++i) {
    const uint32_t r = (uint32_t)(rng_u32() & 0xFFFF);
    const uint64_t n = (uint64_t)r * r;
    CHECK_EQ(isqrt_u32((uint32_t)n), r);
  }
  std::printf("  isqrt: property r^2<=n<(r+1)^2 over 2^20 + boundaries OK\n");
}

static void test_normalize() {
  double max_lsb = 0.0;
  for (int i = 0; i < (1 << 20); ++i) {
    // raw components in +-2^31 -> n2 up to 3*2^62 (s128 exact); require
    // n2 >= 2^48 for the declared bound (qformats.md 7.4)
    int32_t x = rng_s32(), y = rng_s32(), z = rng_s32();
    if (i % 2) {
      x >>= 8;
      y >>= 8;
      z >>= 8;
    }
    const unsigned __int128 n2 = (unsigned __int128)(int64_t)x * x +
                                 (unsigned __int128)(int64_t)y * y +
                                 (unsigned __int128)(int64_t)z * z;
    const vec3fx out = normalize3_approx(vec3fx{fx16{x}, fx16{y}, fx16{z}}, nullptr);
    if (n2 < (unsigned __int128)1 << 48) continue;
    const double len = std::sqrt((double)n2);
    const int32_t comp[3] = {out.x.raw, out.y.raw, out.z.raw};
    const int32_t vin[3] = {x, y, z};
    for (int c = 0; c < 3; ++c) {
      const double ex = vin[c] / len * 65536.0;  // expected raw
      const double err = std::fabs(comp[c] - ex);
      if (err > max_lsb) max_lsb = err;
      CHECK(err <= 2.0 + 1e-6);
    }
  }
  const vec3fx z0 = normalize3_approx(vec3fx{fx16{0}, fx16{0}, fx16{0}}, nullptr);
  CHECK_EQ(z0.x.raw, 0);
  CHECK_EQ(z0.y.raw, 0);
  CHECK_EQ(z0.z.raw, 0);
  std::printf("  normalize3: 2 LSB bound OK (max %.3f LSB)\n", max_lsb);
}

static void test_smoothstep() {
  const fx16 e0{0}, e1{1 << 16};
  CHECK_EQ(smoothstep(e0, e1, fx16{-5}, nullptr).raw, 0);
  CHECK_EQ(smoothstep(e0, e1, fx16{0}, nullptr).raw, 0);
  CHECK_EQ(smoothstep(e0, e1, fx16{1 << 16}, nullptr).raw, 1 << 16);
  CHECK_EQ(smoothstep(e0, e1, fx16{5 << 16}, nullptr).raw, 1 << 16);
  CHECK_EQ(smoothstep(e0, e1, fx16{1 << 15}, nullptr).raw, 1 << 15);  // f(1/2) = 1/2
  // monotone sweep (within field_rcp's declared error: the ±2 LSB wobble of
  // the inner rcp can break exact monotonicity between adjacent inputs)
  int32_t prev = -1;
  for (int i = 0; i <= 256; ++i) {
    const int32_t v = smoothstep(e0, e1, fx16{(i << 8)}, nullptr).raw;
    CHECK(v + 4 >= prev);
    prev = v;
  }
  // d == 0 hits pinned field_rcp zero (RCP0 recorded, total result)
  SatLedger L;
  smoothstep(e0, e0, fx16{3}, &L);
  CHECK_EQ(L.rcp0, 1u);
  std::printf("  smoothstep: endpoints/monotone/pinned-zero OK\n");
}

static void test_conversions() {
  // height16 <-> fx16 exact round trip (qformats.md 9)
  for (int i = 0; i < (1 << 20); ++i) {
    const int16_t h = (int16_t)(rng_u32() & 0xFFFF);
    CHECK_EQ(height16_from_fx16(fx_from_height16(height16{h}), nullptr).raw, h);
  }
  CHECK_EQ(height16_from_fx16(fx16{INT32_MAX}, nullptr).raw, INT16_MAX);
  CHECK_EQ(height16_from_fx16(fx16{INT32_MIN}, nullptr).raw, INT16_MIN);

  // unit8 conversions (qformats.md 2)
  for (uint32_t u = 0; u < 256; ++u) {
    const fx16 x = fx_from_unit8(unit8{(uint8_t)u});
    CHECK_EQ(x.raw, (int32_t)(u << 8));
    CHECK_EQ(unit8_from_fx16(x, nullptr).raw, u);
  }
  CHECK_EQ(unit8_from_fx16(fx16{0x10000}, nullptr).raw, 255);  // 1.0 clamps
  CHECK_EQ(unit8_from_fx16(fx16{-1}, nullptr).raw, 0);
  // Review C2 boundary corpus: raws one LSB below / at / at the top of the
  // round-to-256 region. Pre-fix, 0xFF80..0xFFFF wrapped (256 -> uint8 0).
  CHECK_EQ(unit8_from_fx16(fx16{0xFF7F}, nullptr).raw, 255);  // 0.996.. rounds up
  CHECK_EQ(unit8_from_fx16(fx16{0xFF80}, nullptr).raw, 255);  // was 0 (wrap)
  CHECK_EQ(unit8_from_fx16(fx16{0xFFFF}, nullptr).raw, 255);  // was 0 (wrap)
  CHECK_EQ(unit8_from_fx16(fx16{-1}, nullptr).raw, 0);
  CHECK_EQ(unit8_from_fx16(fx16{INT32_MIN}, nullptr).raw, 0);    // bottom rail
  CHECK_EQ(unit8_from_fx16(fx16{INT32_MAX}, nullptr).raw, 255);  // top rail
  {  // negative clamps record exactly one unit counter (qformats.md 5)
    SatLedger L;
    unit8_from_fx16(fx16{-1}, &L);
    unit8_from_fx16(fx16{0xFF80}, &L);  // in-range rail: no record
    CHECK_EQ(L.unit, 1u);
  }

  // screenXY: round-half-up from fx16, guard clamp at +-2048 px (qformats.md 8)
  CHECK_EQ(to_screen_xy(fx16{0}, nullptr), 0);
  CHECK_EQ(to_screen_xy(fx16{100 << 16}, nullptr), 100 << 8);
  CHECK_EQ(to_screen_xy(fx16{(2049 << 16)}, nullptr), 2048 << 8);
  CHECK_EQ(to_screen_xy(fx16{-(2049 << 16)}, nullptr), -(2048 << 8));
  for (int i = 0; i < (1 << 18); ++i) {
    const int32_t px = to_screen_xy(fx16{rng_s32()}, nullptr);
    CHECK(px <= (2048 << 8) && px >= -(2048 << 8));
  }
  std::printf("  conversions: height16/unit8/screenXY OK\n");
}

// =============================================================================
// mat4 x vec4 single-rounding row law (qformats.md 2)
// =============================================================================
static void test_mat4() {
  mat4fx id{};
  id.m[0][0] = id.m[1][1] = id.m[2][2] = id.m[3][3] = fx16{1 << 16};
  const vec4fx v{fx16{3 << 16}, fx16{-4 << 16}, fx16{5 << 8}, fx16{1 << 14}};
  const vec4fx r = mat4_vec4(id, v, nullptr);
  CHECK_EQ(r.x.raw, 3 << 16);
  CHECK_EQ(r.y.raw, -4 << 16);
  CHECK_EQ(r.z.raw, 5 << 8);
  CHECK_EQ(r.w.raw, 1 << 14);

  SatLedger L;
  mat4fx big{};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) big.m[i][j] = fx16{INT32_MAX};
  const vec4fx vbig{fx16{INT32_MAX}, fx16{INT32_MAX}, fx16{INT32_MAX}, fx16{INT32_MAX}};
  const vec4fx s = mat4_vec4(big, vbig, &L);  // row sums ~2^64 -> clamp
  CHECK_EQ(s.x.raw, INT32_MAX);
  CHECK_EQ(L.mul, 4u);  // one clamp per row

  // random vs oracle: exact s128 row sum + ONE rescale
  for (int i = 0; i < (1 << 16); ++i) {
    mat4fx m{};
    vec4fx w{};
    fx16* cols[4] = {&w.x, &w.y, &w.z, &w.w};
    for (int j = 0; j < 4; ++j) *cols[j] = fx16{rng_s32()};
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b) m.m[a][b] = fx16{rng_s32()};
    const vec4fx got = mat4_vec4(m, w, nullptr);
    const fx16* out[4] = {&got.x, &got.y, &got.z, &got.w};
    for (int a = 0; a < 4; ++a) {
      __int128 p = 0;
      for (int b = 0; b < 4; ++b) p += (__int128)m.m[a][b].raw * cols[b]->raw;
      __int128 q = oracle_rhu_shift(p, 16);
      const int32_t want = q > INT32_MAX ? INT32_MAX : q < INT32_MIN ? INT32_MIN : (int32_t)q;
      CHECK_EQ(out[a]->raw, want);
    }
  }
  std::printf("  mat4_vec4: identity/saturation + 2^16 oracle matrices OK\n");
}

int main(int argc, char** argv) {
  const bool full = argc > 1 && 0 == std::strcmp(argv[1], "--rcp-full");
  std::printf("test_fixp (qformats.md QFMT_VERSION %u)%s\n", gen::QFMT_VERSION,
              full ? " [full rcp sweep]" : "");
  CHECK_EQ(gen::QFMT_VERSION, 1u);

  test_sin_cos();
  test_sin_table_endpoint();
  test_unit8();
  test_noise2();
  test_fx16_oracles();
  test_fx24();
  test_div_exact();
  test_rcp24_sample();
  test_field_rcp();
  test_angle_wrap();
  test_monotonicity();
  test_sat_ledger();
  test_isqrt();
  test_normalize();
  test_smoothstep();
  test_conversions();
  test_mat4();
  if (full) {
    test_rcp24_full();  // nightly label: all 2^24 rcp inputs + frozen hash
  }

  std::printf("%s: %d checks, %d failures\n", full ? "test_fixp_full" : "test_fixp", g_checks,
              g_failures);
  return g_failures == 0 ? 0 : 1;
}
