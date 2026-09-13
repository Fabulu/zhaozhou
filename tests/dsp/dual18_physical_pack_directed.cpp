// dual18_physical_pack_directed.cpp -- bit-exact functional discriminator for
// the portable/vendor zhao_dual18_mul backends and their registered shells.
//
// This source is compiled directly by tests/dsp/run_dual18_verilator.py.  A
// supported Intel vendor-simulation setup can compile the same source with the
// CYCLONEV backend; the final FNV-1a transcript line is then directly comparable
// with the behavioral run.  Expected values are native widened C++ integer
// products, never the RTL limb formula.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#if defined(DUAL18_TOP_INFERRED)
#include "Vdual18_inferred_pair.h"
using TestTop = Vdual18_inferred_pair;
#define DUAL18_PAIR_TEST 1
#elif defined(DUAL18_TOP_EXPLICIT)
#include "Vdual18_explicit_pair.h"
using TestTop = Vdual18_explicit_pair;
#define DUAL18_PAIR_TEST 1
#elif defined(DUAL18_TOP_CE_MUTANT)
#include "Vdual18_ce_ignore_mutant.h"
using TestTop = Vdual18_ce_ignore_mutant;
#define DUAL18_PAIR_TEST 1
#elif defined(DUAL18_TOP_S32X18)
#include "Vdual18_s32x18_exact.h"
using TestTop = Vdual18_s32x18_exact;
#define DUAL18_S32X18_TEST 1
#elif defined(DUAL18_TOP_PROJECTOR)
#include "Vdual18_s32xu12_projector.h"
using TestTop = Vdual18_s32xu12_projector;
#define DUAL18_PROJECTOR_TEST 1
#else
#error "Select exactly one DUAL18_TOP_* test model"
#endif

#ifndef TEST_AX_SIGNED
#define TEST_AX_SIGNED 0
#endif
#ifndef TEST_AY_SIGNED
#define TEST_AY_SIGNED 0
#endif
#ifndef TEST_BX_SIGNED
#define TEST_BX_SIGNED 0
#endif
#ifndef TEST_BY_SIGNED
#define TEST_BY_SIGNED 0
#endif

// Verilator's standalone runtime references this legacy callback even though
// these tests advance time explicitly with Verilated::timeInc().
double sc_time_stamp() { return 0.0; }

namespace {

constexpr uint64_t kMask18 = (UINT64_C(1) << 18) - 1;
constexpr uint64_t kMask36 = (UINT64_C(1) << 36) - 1;
constexpr uint64_t kMask50 = (UINT64_C(1) << 50) - 1;

struct Prng {
  uint64_t state;
  explicit Prng(uint64_t seed) : state(seed) {}
  uint64_t next() {
    // SplitMix64: deterministic on every C++ implementation with uint64_t.
    state += UINT64_C(0x9E3779B97F4A7C15);
    uint64_t z = state;
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
  }
  uint32_t below(uint32_t limit) { return limit ? static_cast<uint32_t>(next() % limit) : 0; }
};

struct Digest {
  uint64_t value = UINT64_C(14695981039346656037);
  void byte(uint8_t v) {
    value ^= v;
    value *= UINT64_C(1099511628211);
  }
  void u64(uint64_t v) {
    for (int i = 0; i < 8; ++i) byte(static_cast<uint8_t>(v >> (8 * i)));
  }
};

struct Score {
  uint64_t enabled_mismatches = 0;
  uint64_t hold_mismatches = 0;
  uint64_t reset_mismatches = 0;
  uint64_t comparisons = 0;
  uint64_t enabled_outputs = 0;
  uint64_t stalls = 0;
  uint64_t resets = 0;
  uint64_t printed = 0;

  void mismatch(const char* where, const char* field, uint64_t want, uint64_t got,
                uint64_t* bucket) {
    ++*bucket;
    if (printed < 12) {
      std::printf("MISMATCH %-7s %-8s want=%016llx got=%016llx\n", where, field,
                  static_cast<unsigned long long>(want),
                  static_cast<unsigned long long>(got));
      ++printed;
    }
  }

  void check(const char* where, const char* field, uint64_t want, uint64_t got,
             uint64_t* bucket) {
    ++comparisons;
    if (want != got) mismatch(where, field, want, got, bucket);
  }
};

void tick(TestTop& top) {
  top.clk_i = 0;
  top.eval();
  Verilated::timeInc(1);
  top.clk_i = 1;
  top.eval();
  Verilated::timeInc(1);
  top.clk_i = 0;
  top.eval();
  Verilated::timeInc(1);
}

int64_t decode18(uint32_t raw, bool is_signed) {
  const uint32_t v = raw & static_cast<uint32_t>(kMask18);
  if (is_signed && (v & (1u << 17))) return static_cast<int64_t>(v) - (INT64_C(1) << 18);
  return static_cast<int64_t>(v);
}

uint32_t raw18(int64_t value) {
  return static_cast<uint32_t>(static_cast<uint64_t>(value) & kMask18);
}

uint32_t embed8(uint8_t raw, bool is_signed) {
  if (!is_signed) return raw;
  return raw18(static_cast<int8_t>(raw));
}

std::vector<uint32_t> boundary18(bool is_signed) {
  if (is_signed) {
    return {raw18(-131072), raw18(-131071), raw18(-2), raw18(-1), raw18(0),
            raw18(1), raw18(2), raw18(131070), raw18(131071)};
  }
  return {0u, 1u, 2u, 131071u, 131072u, 262142u, 262143u};
}

#if defined(DUAL18_PAIR_TEST)

struct PairVector {
  uint32_t ax = 0;
  uint32_t ay = 0;
  uint32_t bx = 0;
  uint32_t by = 0;
  uint32_t tag = 0;
  bool valid = false;
};

struct PairExpected {
  uint64_t a = 0;
  uint64_t b = 0;
  uint32_t tag = 0;
  bool valid = false;
};

PairExpected pair_oracle(const PairVector& v) {
  const int64_t ax = decode18(v.ax, TEST_AX_SIGNED != 0);
  const int64_t ay = decode18(v.ay, TEST_AY_SIGNED != 0);
  const int64_t bx = decode18(v.bx, TEST_BX_SIGNED != 0);
  const int64_t by = decode18(v.by, TEST_BY_SIGNED != 0);
  PairExpected e;
  e.a = static_cast<uint64_t>(ax * ay) & kMask36;
  e.b = static_cast<uint64_t>(bx * by) & kMask36;
  e.tag = v.tag;
  e.valid = v.valid;
  return e;
}

struct PairObserved {
  uint64_t a;
  uint64_t b;
  uint32_t tag;
  bool valid;
};

PairObserved observe_pair(const TestTop& top) {
  return {static_cast<uint64_t>(top.resulta_o) & kMask36,
          static_cast<uint64_t>(top.resultb_o) & kMask36,
          static_cast<uint32_t>(top.tag_o), top.valid_o != 0};
}

class PairDriver {
 public:
  PairDriver(TestTop& top, Score& score, Digest& digest, bool inject_lane_swap)
      : top_(top), score_(score), digest_(digest), inject_lane_swap_(inject_lane_swap) {}

  void cycle(const PairVector& v, bool ce, bool rst) {
    const PairObserved before = observe_pair(top_);
    top_.ce_i = ce;
    top_.rst_i = rst;
    top_.valid_i = v.valid;
    top_.tag_i = v.tag;
    top_.ax_i = v.ax & kMask18;
    top_.ay_i = v.ay & kMask18;
    top_.bx_i = v.bx & kMask18;
    top_.by_i = v.by & kMask18;

    const PairExpected emit = pending_;
    tick(top_);
    PairObserved got = observe_pair(top_);

    if (rst) {
      ++score_.resets;
      score_.check("reset", "resulta", 0, got.a, &score_.reset_mismatches);
      score_.check("reset", "resultb", 0, got.b, &score_.reset_mismatches);
      score_.check("reset", "valid", 0, got.valid, &score_.reset_mismatches);
      score_.check("reset", "tag", 0, got.tag, &score_.reset_mismatches);
      pending_ = {};
      return;
    }

    if (!ce) {
      ++score_.stalls;
      score_.check("hold", "resulta", before.a, got.a, &score_.hold_mismatches);
      score_.check("hold", "resultb", before.b, got.b, &score_.hold_mismatches);
      score_.check("hold", "valid", before.valid, got.valid, &score_.hold_mismatches);
      score_.check("hold", "tag", before.tag, got.tag, &score_.hold_mismatches);
      return;
    }

    ++score_.enabled_outputs;
    if (inject_lane_swap_) {
      const uint64_t tmp = got.a;
      got.a = got.b;
      got.b = tmp;
    }
    score_.check("enabled", "resulta", emit.a, got.a, &score_.enabled_mismatches);
    score_.check("enabled", "resultb", emit.b, got.b, &score_.enabled_mismatches);
    score_.check("enabled", "valid", emit.valid, got.valid, &score_.enabled_mismatches);
    score_.check("enabled", "tag", emit.tag, got.tag, &score_.enabled_mismatches);
    digest_.u64(static_cast<uint64_t>(top_.resulta_o) & kMask36);
    digest_.u64(static_cast<uint64_t>(top_.resultb_o) & kMask36);
    digest_.u64(static_cast<uint32_t>(top_.tag_o));
    digest_.byte(top_.valid_o ? 1 : 0);
    pending_ = pair_oracle(v);
  }

 private:
  TestTop& top_;
  Score& score_;
  Digest& digest_;
  bool inject_lane_swap_;
  PairExpected pending_{};
};

PairVector random_pair(Prng& rng, uint32_t tag, bool valid = true) {
  return {static_cast<uint32_t>(rng.next()) & static_cast<uint32_t>(kMask18),
          static_cast<uint32_t>(rng.next()) & static_cast<uint32_t>(kMask18),
          static_cast<uint32_t>(rng.next()) & static_cast<uint32_t>(kMask18),
          static_cast<uint32_t>(rng.next()) & static_cast<uint32_t>(kMask18), tag, valid};
}

void pair_reset_and_flow(PairDriver& driver) {
  Prng rng(UINT64_C(0xCE18C0FFEE));
  uint32_t tag = 1;

  // Reset priority is checked while valid and stalled, then once while enabled.
  driver.cycle(random_pair(rng, tag++, true), false, true);
  driver.cycle(random_pair(rng, tag++, true), false, false);
  driver.cycle(random_pair(rng, tag++, true), true, true);

  // Back-to-back valids, an isolated bubble, and recovery.
  for (int i = 0; i < 24; ++i) driver.cycle(random_pair(rng, tag++, true), true, false);
  driver.cycle(random_pair(rng, tag++, false), true, false);
  driver.cycle(random_pair(rng, tag++, true), true, false);

  // Declared stall lengths 1, 2, and long.  Inputs and tags change on every
  // disabled edge; a source that politely held still could not fire the guard.
  for (int length : {1, 2, 19}) {
    driver.cycle(random_pair(rng, tag++, true), true, false);
    for (int i = 0; i < length; ++i)
      driver.cycle(random_pair(rng, tag++, (i & 1) == 0), false, false);
    driver.cycle(random_pair(rng, tag++, true), true, false);
  }

  // Long deterministic randomized bursts include another reset asserted while
  // CE is low and valid is high.
  for (int i = 0; i < 600; ++i) {
    const bool reset_now = (i == 311);
    const bool ce = reset_now ? false : (rng.below(100) >= 37);
    const bool valid = reset_now ? true : (rng.below(100) < 79);
    driver.cycle(random_pair(rng, tag++, valid), ce, reset_now);
  }
  for (int i = 0; i < 3; ++i) driver.cycle(random_pair(rng, tag++, false), true, false);
}

void pair_exhaustive_8bit(PairDriver& driver) {
  uint32_t tag = UINT32_C(0x08000000);
  std::vector<uint8_t> lane_b_seen(UINT32_C(1) << 16, 0);
  uint32_t lane_b_unique = 0;
  for (uint32_t x = 0; x < 256; ++x) {
    for (uint32_t y = 0; y < 256; ++y) {
      // This affine map has determinant 1 modulo 256, so it is a bijection of
      // all 65,536 8-bit operand pairs rather than merely a varied projection.
      const uint8_t bx = static_cast<uint8_t>((x + 2u * y + 19u) & 255u);
      const uint8_t by = static_cast<uint8_t>((2u * x + 5u * y + 167u) & 255u);
      const uint32_t lane_b_key = (static_cast<uint32_t>(bx) << 8) | by;
      if (lane_b_seen[lane_b_key]) {
        std::fprintf(stderr,
                     "lane-B 8-bit permutation duplicate key=%04x at x=%u y=%u\n",
                     lane_b_key, x, y);
        std::exit(2);
      }
      lane_b_seen[lane_b_key] = 1;
      ++lane_b_unique;
      PairVector v;
      v.ax = embed8(static_cast<uint8_t>(x), TEST_AX_SIGNED != 0);
      v.ay = embed8(static_cast<uint8_t>(y), TEST_AY_SIGNED != 0);
      v.bx = embed8(bx, TEST_BX_SIGNED != 0);
      v.by = embed8(by, TEST_BY_SIGNED != 0);
      v.tag = tag++;
      v.valid = true;
      driver.cycle(v, true, false);
    }
  }
  if (lane_b_unique != (UINT32_C(1) << 16)) {
    std::fprintf(stderr, "lane-B 8-bit permutation cardinality=%u, expected=65536\n",
                 lane_b_unique);
    std::exit(2);
  }
}

void pair_boundaries(PairDriver& driver) {
  const auto ax = boundary18(TEST_AX_SIGNED != 0);
  const auto ay = boundary18(TEST_AY_SIGNED != 0);
  const auto bx = boundary18(TEST_BX_SIGNED != 0);
  const auto by = boundary18(TEST_BY_SIGNED != 0);
  uint32_t tag = UINT32_C(0x18000000);

  // First make lane A's complete boundary Cartesian product independently
  // visible; then do the same for lane B.  Neither lane is a constant observer.
  for (size_t i = 0; i < ax.size(); ++i) {
    for (size_t j = 0; j < ay.size(); ++j) {
      PairVector v{ax[i], ay[j], bx[(5 * i + 3 * j + 1) % bx.size()],
                   by[(7 * i + 11 * j + 2) % by.size()], tag++, true};
      driver.cycle(v, true, false);
    }
  }
  for (size_t i = 0; i < bx.size(); ++i) {
    for (size_t j = 0; j < by.size(); ++j) {
      PairVector v{ax[(3 * i + 5 * j + 2) % ax.size()],
                   ay[(11 * i + 7 * j + 1) % ay.size()], bx[i], by[j], tag++, true};
      driver.cycle(v, true, false);
    }
  }
}

void pair_random(PairDriver& driver) {
  Prng rng(UINT64_C(0xD0185A17B17));
  for (uint32_t i = 0; i < 20000; ++i)
    driver.cycle(random_pair(rng, UINT32_C(0xA5000000) + i, (i % 13) != 0), true, false);
}

#endif  // DUAL18_PAIR_TEST

#if defined(DUAL18_S32X18_TEST)

struct WideVector {
  uint32_t a = 0;
  uint32_t b = 0;
  uint32_t tag = 0;
  bool valid = false;
};
struct WideExpected {
  uint64_t result = 0;
  uint32_t tag = 0;
  bool valid = false;
};

WideExpected wide_oracle(const WideVector& v) {
  const int64_t a = static_cast<int32_t>(v.a);
  const int64_t b = decode18(v.b, true);
  return {static_cast<uint64_t>(a * b) & kMask50, v.tag, v.valid};
}

class WideDriver {
 public:
  WideDriver(TestTop& top, Score& score, Digest& digest)
      : top_(top), score_(score), digest_(digest) {}
  void cycle(const WideVector& v, bool ce, bool rst) {
    const uint64_t before_result = static_cast<uint64_t>(top_.result_o) & kMask50;
    const uint32_t before_tag = top_.tag_o;
    const bool before_valid = top_.valid_o != 0;
    const WideExpected emit = pending_;
    top_.ce_i = ce;
    top_.rst_i = rst;
    top_.valid_i = v.valid;
    top_.tag_i = v.tag;
    top_.a_i = v.a;
    top_.b_i = v.b & kMask18;
    tick(top_);
    const uint64_t got_result = static_cast<uint64_t>(top_.result_o) & kMask50;
    const uint32_t got_tag = top_.tag_o;
    const bool got_valid = top_.valid_o != 0;
    if (rst) {
      ++score_.resets;
      score_.check("reset", "result", 0, got_result, &score_.reset_mismatches);
      score_.check("reset", "valid", 0, got_valid, &score_.reset_mismatches);
      score_.check("reset", "tag", 0, got_tag, &score_.reset_mismatches);
      pending_ = {};
    } else if (!ce) {
      ++score_.stalls;
      score_.check("hold", "result", before_result, got_result, &score_.hold_mismatches);
      score_.check("hold", "valid", before_valid, got_valid, &score_.hold_mismatches);
      score_.check("hold", "tag", before_tag, got_tag, &score_.hold_mismatches);
    } else {
      ++score_.enabled_outputs;
      score_.check("enabled", "result", emit.result, got_result, &score_.enabled_mismatches);
      score_.check("enabled", "valid", emit.valid, got_valid, &score_.enabled_mismatches);
      score_.check("enabled", "tag", emit.tag, got_tag, &score_.enabled_mismatches);
      digest_.u64(got_result);
      digest_.u64(got_tag);
      digest_.byte(got_valid ? 1 : 0);
      pending_ = wide_oracle(v);
    }
  }

 private:
  TestTop& top_;
  Score& score_;
  Digest& digest_;
  WideExpected pending_{};
};

WideVector random_wide(Prng& rng, uint32_t tag, bool valid = true) {
  return {static_cast<uint32_t>(rng.next()),
          static_cast<uint32_t>(rng.next()) & static_cast<uint32_t>(kMask18), tag, valid};
}

void wide_corpus(WideDriver& driver) {
  Prng rng(UINT64_C(0x3218E7AC7));
  uint32_t tag = 1;
  driver.cycle(random_wide(rng, tag++, true), false, true);
  driver.cycle(random_wide(rng, tag++, true), false, false);
  for (int i = 0; i < 20; ++i) driver.cycle(random_wide(rng, tag++, true), true, false);
  for (int length : {1, 2, 23}) {
    for (int i = 0; i < length; ++i) driver.cycle(random_wide(rng, tag++, true), false, false);
    driver.cycle(random_wide(rng, tag++, true), true, false);
  }
  driver.cycle(random_wide(rng, tag++, true), false, true);
  // Refill a live pipeline, then prove synchronous reset wins even when CE is
  // asserted and a valid transfer would otherwise advance both shell stages.
  driver.cycle(random_wide(rng, tag++, true), true, false);
  driver.cycle(random_wide(rng, tag++, true), true, false);
  driver.cycle(random_wide(rng, tag++, true), true, true);

  const std::vector<int64_t> avals = {INT32_MIN, static_cast<int64_t>(INT32_MIN) + 1,
                                      -2, -1, 0, 1, 2,
                                      static_cast<int64_t>(INT32_MAX) - 1, INT32_MAX};
  const std::vector<int64_t> bvals = {-131072, -131071, -2, -1, 0, 1, 2, 131070, 131071};
  for (int64_t a : avals) {
    for (int64_t b : bvals) {
      driver.cycle({static_cast<uint32_t>(a), raw18(b), tag++, true}, true, false);
    }
  }
  for (uint32_t i = 0; i < 50000; ++i)
    driver.cycle(random_wide(rng, UINT32_C(0x71000000) + i, (i % 17) != 0), true, false);
  for (int i = 0; i < 3; ++i) driver.cycle(random_wide(rng, tag++, false), true, false);
}

#endif  // DUAL18_S32X18_TEST

#if defined(DUAL18_PROJECTOR_TEST)

struct ProjectVector {
  uint32_t n = 0;
  uint16_t viewport = 0;
  uint32_t tag = 0;
  bool valid = false;
};
struct ProjectExpected {
  uint64_t result = 0;
  uint32_t tag = 0;
  bool valid = false;
};

ProjectExpected project_oracle(const ProjectVector& v) {
  const int64_t n = static_cast<int32_t>(v.n);
  const int64_t extent = v.viewport & 0x0FFFu;
  // Multiplication by 2^15, rather than a left shift of a negative signed
  // value, keeps the independent C++ oracle free of undefined behavior.
  const int64_t result = n * extent * (INT64_C(1) << 15);
  return {static_cast<uint64_t>(result), v.tag, v.valid};
}

class ProjectDriver {
 public:
  ProjectDriver(TestTop& top, Score& score, Digest& digest)
      : top_(top), score_(score), digest_(digest) {}
  void cycle(const ProjectVector& v, bool ce, bool rst) {
    const uint64_t before_result = static_cast<uint64_t>(top_.result_o);
    const uint32_t before_tag = top_.tag_o;
    const bool before_valid = top_.valid_o != 0;
    const ProjectExpected emit = pending_;
    top_.ce_i = ce;
    top_.rst_i = rst;
    top_.valid_i = v.valid;
    top_.tag_i = v.tag;
    top_.n_i = v.n;
    top_.viewport_i = v.viewport & 0x0FFFu;
    tick(top_);
    const uint64_t got_result = static_cast<uint64_t>(top_.result_o);
    const uint32_t got_tag = top_.tag_o;
    const bool got_valid = top_.valid_o != 0;
    if (rst) {
      ++score_.resets;
      score_.check("reset", "result", 0, got_result, &score_.reset_mismatches);
      score_.check("reset", "valid", 0, got_valid, &score_.reset_mismatches);
      score_.check("reset", "tag", 0, got_tag, &score_.reset_mismatches);
      pending_ = {};
    } else if (!ce) {
      ++score_.stalls;
      score_.check("hold", "result", before_result, got_result, &score_.hold_mismatches);
      score_.check("hold", "valid", before_valid, got_valid, &score_.hold_mismatches);
      score_.check("hold", "tag", before_tag, got_tag, &score_.hold_mismatches);
    } else {
      ++score_.enabled_outputs;
      score_.check("enabled", "result", emit.result, got_result, &score_.enabled_mismatches);
      score_.check("enabled", "valid", emit.valid, got_valid, &score_.enabled_mismatches);
      score_.check("enabled", "tag", emit.tag, got_tag, &score_.enabled_mismatches);
      digest_.u64(got_result);
      digest_.u64(got_tag);
      digest_.byte(got_valid ? 1 : 0);
      pending_ = project_oracle(v);
    }
  }

 private:
  TestTop& top_;
  Score& score_;
  Digest& digest_;
  ProjectExpected pending_{};
};

ProjectVector random_project(Prng& rng, uint32_t tag, bool valid = true) {
  return {static_cast<uint32_t>(rng.next()), static_cast<uint16_t>(rng.next() & 0x0FFFu), tag,
          valid};
}

void projector_corpus(ProjectDriver& driver) {
  Prng rng(UINT64_C(0x320012C0DE));
  uint32_t tag = 1;
  driver.cycle(random_project(rng, tag++, true), false, true);
  driver.cycle(random_project(rng, tag++, true), false, false);
  for (int i = 0; i < 20; ++i) driver.cycle(random_project(rng, tag++, true), true, false);
  for (int length : {1, 2, 29}) {
    for (int i = 0; i < length; ++i)
      driver.cycle(random_project(rng, tag++, (i & 1) == 0), false, false);
    driver.cycle(random_project(rng, tag++, true), true, false);
  }
  driver.cycle(random_project(rng, tag++, true), false, true);
  // This duplicated wide-product shell must give synchronous reset priority
  // over an enabled valid transfer, not only over a disabled edge.
  driver.cycle(random_project(rng, tag++, true), true, false);
  driver.cycle(random_project(rng, tag++, true), true, false);
  driver.cycle(random_project(rng, tag++, true), true, true);

  const std::vector<int64_t> nvals = {INT32_MIN, static_cast<int64_t>(INT32_MIN) + 1,
                                      -2, -1, 0, 1, 2,
                                      static_cast<int64_t>(INT32_MAX) - 1, INT32_MAX};
  const std::vector<uint16_t> extents = {0, 1, 2, 2047, 4094, 4095};
  for (int64_t n : nvals) {
    for (uint16_t extent : extents)
      driver.cycle({static_cast<uint32_t>(n), extent, tag++, true}, true, false);
  }
  for (uint32_t i = 0; i < 50000; ++i)
    driver.cycle(random_project(rng, UINT32_C(0x72000000) + i, (i % 19) != 0), true, false);
  for (int i = 0; i < 3; ++i) driver.cycle(random_project(rng, tag++, false), true, false);
}

#endif  // DUAL18_PROJECTOR_TEST

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  bool control_only = false;
  bool inject_lane_swap = false;
  bool expect_ce_failure = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if (arg == "--control-only") control_only = true;
    else if (arg == "--inject-lane-swap") inject_lane_swap = true;
    else if (arg == "--expect-ce-failure") expect_ce_failure = true;
    else {
      std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
      return 2;
    }
  }

#if !defined(DUAL18_PAIR_TEST)
  if (control_only || inject_lane_swap || expect_ce_failure) {
    std::fprintf(stderr, "pair-only control argument used for a wide-product top\n");
    return 2;
  }
#endif
  if (inject_lane_swap && expect_ce_failure) {
    std::fprintf(stderr, "functional and CE controls must run separately\n");
    return 2;
  }

  TestTop top;
  Score score;
  Digest digest;

#if defined(DUAL18_PAIR_TEST)
  PairDriver driver(top, score, digest, inject_lane_swap);
  pair_reset_and_flow(driver);
  if (!control_only) {
    pair_exhaustive_8bit(driver);
    pair_boundaries(driver);
    pair_random(driver);
  }
#elif defined(DUAL18_S32X18_TEST)
  WideDriver driver(top, score, digest);
  wide_corpus(driver);
#elif defined(DUAL18_PROJECTOR_TEST)
  ProjectDriver driver(top, score, digest);
  projector_corpus(driver);
#endif

  top.final();
  std::printf("DUAL18_COUNTS comparisons=%llu enabled_outputs=%llu stalls=%llu resets=%llu "
              "enabled_mismatches=%llu hold_mismatches=%llu reset_mismatches=%llu\n",
              static_cast<unsigned long long>(score.comparisons),
              static_cast<unsigned long long>(score.enabled_outputs),
              static_cast<unsigned long long>(score.stalls),
              static_cast<unsigned long long>(score.resets),
              static_cast<unsigned long long>(score.enabled_mismatches),
              static_cast<unsigned long long>(score.hold_mismatches),
              static_cast<unsigned long long>(score.reset_mismatches));
  std::printf("DUAL18_TRANSCRIPT fnv1a64=%016llx\n",
              static_cast<unsigned long long>(digest.value));

  bool pass = false;
  if (expect_ce_failure) {
    pass = score.hold_mismatches > 0 && score.enabled_mismatches == 0 &&
           score.reset_mismatches == 0;
    std::printf("DUAL18_POSITIVE_CONTROL ce_hold_detector=%s\n", pass ? "FIRED" : "FAILED");
  } else if (inject_lane_swap) {
    pass = score.enabled_mismatches > 0 && score.hold_mismatches == 0 &&
           score.reset_mismatches == 0;
    std::printf("DUAL18_POSITIVE_CONTROL lane_swap_detector=%s\n", pass ? "FIRED" : "FAILED");
  } else {
    pass = score.enabled_mismatches == 0 && score.hold_mismatches == 0 &&
           score.reset_mismatches == 0;
  }
  std::printf("DUAL18_RESULT %s\n", pass ? "PASS" : "FAIL");
  return pass ? 0 : 1;
}
