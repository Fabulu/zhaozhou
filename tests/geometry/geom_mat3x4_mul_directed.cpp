// geom_mat3x4_mul_directed.cpp — the pose chain's matrix product against
// `zref::creature::mat3x4_mul`.
//
// Differential on all twelve elements. This block is sequential — twelve cycles,
// one element each, three 32x32 products instead of thirty-six — so the test has
// to prove two separate things: that the arithmetic matches the oracle, and that
// the twelve-cycle walk assembles a whole matrix rather than a partial one.
// Section 7 is about the second.
//
// Matrix multiplication is NOT commutative, and the decode chain depends on the
// order: `A_parent * LR` is a bone hanging off its parent, `LR * A_parent` is
// nonsense. A swapped-operand implementation passes every identity and
// pure-translation case, so section 3 exists to fail it.

#include "Vzhao_geom_mat3x4_mul.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_creature.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

using zhao::check;
namespace zc = zref::creature;

const int32_t ONE = 1 << 16;

void setMat(VlUnpacked<IData, 12>& dst, const zc::mat3x4fx& m) {
  for (int i = 0; i < 12; ++i) dst[i] = static_cast<uint32_t>(m.m[i]);
}

zc::mat3x4fx mat(int32_t a, int32_t b, int32_t c, int32_t tx, int32_t d, int32_t e, int32_t f,
                 int32_t ty, int32_t g, int32_t h, int32_t i, int32_t tz) {
  return zc::mat3x4fx{{a, b, c, tx, d, e, f, ty, g, h, i, tz}};
}

/**
 * Drive one product and spin until it lands. Returns the cycles the block took
 * between accept and valid, so the caller can assert the walk length.
 */
int run(Vzhao_geom_mat3x4_mul& dut, const zc::mat3x4fx& A, const zc::mat3x4fx& B, uint8_t tag,
        zc::mat3x4fx& got) {
  dut.in_valid_i = 1;
  dut.out_ready_i = 1;
  setMat(dut.a_m_i, A);
  setMat(dut.b_m_i, B);
  dut.in_tag_i = tag;
  dut.eval();
  // Wait for the accept handshake, then drop valid so nothing is offered twice.
  int guard = 0;
  while (!dut.in_ready_o && guard++ < 64) {
    zhao::tick(dut);
    dut.eval();
  }
  zhao::tick(dut);
  dut.in_valid_i = 0;
  dut.eval();

  // Counts posedges AFTER the accept edge, so the value is the number of
  // element-cycles the walk took -- not the accept itself.
  int cycles = 0;
  while (!dut.out_valid_o && cycles < 64) {
    zhao::tick(dut);
    dut.eval();
    ++cycles;
  }
  for (int i = 0; i < 12; ++i) got.m[i] = static_cast<int32_t>(dut.out_m_o[i]);
  return cycles;
}

void diff(Vzhao_geom_mat3x4_mul& dut, const zc::mat3x4fx& A, const zc::mat3x4fx& B,
          const char* what) {
  zc::mat3x4fx want{};
  zc::mat3x4_mul(A, B, want, nullptr);

  zc::mat3x4fx got{};
  run(dut, A, B, 0x3C, got);

  const std::string t(what);
  for (int i = 0; i < 12; ++i) {
    char lbl[160];
    std::snprintf(lbl, sizeof lbl, "%s: m[%d]", t.c_str(), i);
    check(got.m[i] == want.m[i], lbl, static_cast<uint64_t>(static_cast<uint32_t>(want.m[i])),
          static_cast<uint32_t>(got.m[i]));
  }

  // Consume the result so the next call starts from an empty output register.
  dut.out_ready_i = 1;
  dut.eval();
  zhao::tick(dut);
  dut.eval();
}

// PCG RXS-M-XS, the committed test PRNG shape (qformats §7.5).
struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t v0 = s;
    s = v0 * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((v0 >> 22) ^ v0) >> 29);
    const uint32_t v = (static_cast<uint32_t>(v0 >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
};

}  // namespace

int main(int argc, char** argv) {
  Vzhao_geom_mat3x4_mul dut;
  dut.rst_n = 0;
  dut.in_valid_i = 0;
  dut.out_ready_i = 1;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  const zc::mat3x4fx I = zc::mat3x4_identity();

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    Prng rng(0x3A4Bu);
    for (uint32_t k = 0; k < iters && zhao::check_failures() == 0; ++k) {
      zc::mat3x4fx A{}, B{};
      for (int i = 0; i < 12; ++i) {
        // Pose-plausible magnitudes: a bone matrix is a rotation with a
        // translation, so the 3x3 stays near unit scale and the translation
        // carries the range. Sampling the whole s32 word would put nearly every
        // iteration on the saturation rail instead of on the arithmetic, and the
        // rail is covered directly by section 5.
        A.m[i] = static_cast<int32_t>(rng.next()) >> ((i % 4 == 3) ? 10 : 15);
        B.m[i] = static_cast<int32_t>(rng.next()) >> ((i % 4 == 3) ? 10 : 15);
      }
      char tag[64];
      std::snprintf(tag, sizeof tag, "random[%u]", k);
      diff(dut, A, B, tag);
    }
    dut.final();
    return zhao::report_and_exit("geom_mat3x4_mul_random");
  }

  // ---- 1. the identities --------------------------------------------------
  diff(dut, I, I, "identity x identity");
  {
    const zc::mat3x4fx M = mat(2 * ONE, ONE / 2, -ONE, 7 * ONE, -ONE / 4, 3 * ONE, ONE, -2 * ONE,
                               ONE, -ONE, ONE / 8, 11 * ONE);
    diff(dut, I, M, "identity x M");
    diff(dut, M, I, "M x identity");
  }

  // ---- 2. translations compose by addition --------------------------------
  {
    const zc::mat3x4fx T1 = mat(ONE, 0, 0, 5 * ONE, 0, ONE, 0, -3 * ONE, 0, 0, ONE, 2 * ONE);
    const zc::mat3x4fx T2 = mat(ONE, 0, 0, -ONE, 0, ONE, 0, 8 * ONE, 0, 0, ONE, ONE);
    diff(dut, T1, T2, "translation x translation");
  }

  // ---- 3. ORDER MATTERS ---------------------------------------------------
  // A rotation and a translation do not commute, and the chain depends on which
  // way round they go: `A_parent * LR` hangs a bone off its parent, the reverse
  // is meaningless. An implementation with the operands swapped passes every
  // case above; these two are what fail it.
  {
    const zc::mat3x4fx R = mat(0, -ONE, 0, 0, ONE, 0, 0, 0, 0, 0, ONE, 0);  // 90 deg about Z
    const zc::mat3x4fx T = mat(ONE, 0, 0, 4 * ONE, 0, ONE, 0, 0, 0, 0, ONE, 0);

    zc::mat3x4fx rt{}, tr{};
    zc::mat3x4_mul(R, T, rt, nullptr);
    zc::mat3x4_mul(T, R, tr, nullptr);
    bool differ = false;
    for (int i = 0; i < 12; ++i) differ = differ || (rt.m[i] != tr.m[i]);
    check(differ, "the fixture actually distinguishes the two orders", 1, differ ? 1 : 0);

    diff(dut, R, T, "rotate-then-translate (R x T)");
    diff(dut, T, R, "translate-then-rotate (T x R)");
  }

  // ---- 4. the rounding boundary -------------------------------------------
  // A product landing on an exact multiple of 2^15 is where round-half-up and a
  // plain shift disagree; the negative cases separate rounding toward +inf from
  // flooring.
  {
    const zc::mat3x4fx A = mat(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
    const zc::mat3x4fx B = mat(1 << 15, 1 << 15, 1 << 15, 1 << 15, 0, 0, 0, 0, 0, 0, 0, 0);
    diff(dut, A, B, "rounding: exact half");
    const zc::mat3x4fx N = mat(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
    diff(dut, N, B, "rounding: negative exact half");
    const zc::mat3x4fx B2 =
        mat((1 << 15) + 1, (1 << 15) - 1, 1 << 15, 3 << 15, 0, 0, 0, 0, 0, 0, 0, 0);
    diff(dut, A, B2, "rounding: either side of the half");
  }

  // ---- 5. the saturation rails --------------------------------------------
  // Unlike the quaternion block, this one genuinely can saturate: a large
  // translation chained through a large rotation exceeds s32. The reference
  // saturates too, so the two agree -- but the rail is real and is driven here
  // rather than left to the random lane, which is deliberately aimed away from it.
  {
    const int32_t BIG = 0x4000'0000;
    const zc::mat3x4fx P = mat(BIG, BIG, BIG, BIG, BIG, BIG, BIG, BIG, BIG, BIG, BIG, BIG);
    const zc::mat3x4fx M =
        mat(-BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG, -BIG);
    diff(dut, P, P, "saturate: positive rail");
    diff(dut, M, M, "saturate: negative rail via two negatives");
    diff(dut, P, M, "saturate: negative rail");
  }

  // ---- 6. the decode chain's own second step ------------------------------
  // `S_b = A_b * inv_rest[b]`. inv_rest comes from mat3x4_invert_rigid, whose
  // translation column is the negated rotated translation -- a shape the random
  // lane will not generate on its own.
  {
    const zc::mat3x4fx rest = mat(0, -ONE, 0, 3 * ONE, ONE, 0, 0, -5 * ONE, 0, 0, ONE, 2 * ONE);
    zc::mat3x4fx inv{};
    zc::mat3x4_invert_rigid(rest, inv, nullptr);
    diff(dut, rest, inv, "rest x inv_rest (should be near identity)");

    const zc::mat3x4fx A_b = mat(ONE, 0, 0, ONE, 0, 0, -ONE, 2 * ONE, 0, ONE, 0, -ONE);
    diff(dut, A_b, inv, "A_b x inv_rest, the real second step");
  }

  // ---- 7. the sequential walk ---------------------------------------------
  // The arithmetic sections above would all pass on a block that produced a
  // correct matrix in some other number of cycles, or that raised valid while
  // still walking. These assert the walk itself.
  {
    zc::mat3x4fx got{};
    const int cycles = run(dut, I, I, 0xA5, got);
    check(cycles == 12, "one element per cycle: twelve cycles from accept to valid", 12,
          static_cast<uint64_t>(cycles));
    check(dut.out_tag_o == 0xA5, "the tag rides through with its product", 0xA5, dut.out_tag_o);

    // Mid-walk the block must refuse new work. A block that accepted here would
    // overwrite the operands it is still reading.
    dut.out_ready_i = 1;
    dut.eval();
    zhao::tick(dut);
    dut.in_valid_i = 1;
    setMat(dut.a_m_i, I);
    setMat(dut.b_m_i, I);
    dut.eval();
    zhao::tick(dut);  // accepted; the walk starts
    dut.eval();
    check(dut.in_ready_o == 0, "mid-walk the block refuses new operands", 0, dut.in_ready_o);
    check(dut.out_valid_o == 0, "no result is offered mid-walk", 0, dut.out_valid_o);

    dut.in_valid_i = 0;
    dut.eval();
    int guard = 0;
    while (!dut.out_valid_o && guard++ < 64) {
      zhao::tick(dut);
      dut.eval();
    }
    const uint32_t done = dut.products_done_o;
    check(done >= 2, "the counter records completed products", 1, done >= 2 ? 1 : 0);

    // A completed product held against backpressure must not be recounted.
    dut.out_ready_i = 0;
    dut.eval();
    zhao::tick(dut);
    zhao::tick(dut);
    dut.eval();
    check(dut.products_done_o == done, "a held result is not counted twice", done,
          dut.products_done_o);
    dut.out_ready_i = 1;
    dut.eval();
    zhao::tick(dut);
    dut.eval();
    check(dut.out_valid_o == 0, "out_valid drops once the product is taken", 0, dut.out_valid_o);
  }

  dut.final();
  return zhao::report_and_exit("geom_mat3x4_mul_directed");
}
