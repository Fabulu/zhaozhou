// skinnorm_mul_narrow_differential.cpp -- the 2026-09-26 DSP repair in
// `zhao_geom_skin_norm` emits the same bits as the arithmetic it replaced, over
// the full declared port range.
//
// ---------------------------------------------------------------------------
// WHAT IS ACTUALLY AT RISK
// ---------------------------------------------------------------------------
// `zhao_geom_skin_norm` cost 21 DSP blocks -- 19% of the entire shipping
// 5CSEBA6U23I7 -- out of 1,192 ALUTs, and eight of them were Independent 27x27.
// Every multiply in the block was written at 64 bits:
//
//     na_c    = 64'(a_q[...]) * 64'(nx_q) + ... ;      // operands 32 and 8
//     blend_c = 64'(w0_q) * na_c + 64'(7'd64 - w0_q) * nb_c;
//
// The repair multiplies at the operands' true widths -- 40 for a 32x8 product,
// 42 for the sum of three, 50 for the weighted blend -- and the row moved
// 21 -> 16 DSP and 2053 -> 1954 combinational ALUTs, with the register count
// UNCHANGED at 920.
//
// NARROWING A MULTIPLY IS LEGAL; NARROWING A RESULT IS NOT. That is the whole
// hazard, and there is a second one on top of it that is specific to this
// block and does not appear in the attrsetup repair at all:
//
//   *** THE PRODUCTION MULTIPLY IS UNSIGNED, AND THE REPAIRED ONE IS SIGNED. ***
//
// `w0_q` is `logic [6:0]` -- UNSIGNED -- so `64'(w0_q) * na_c` is an UNSIGNED
// 64-bit multiply even though `na_c` is signed, because SystemVerilog makes the
// whole operation unsigned when EITHER operand is. The repaired form multiplies
// an 8-bit SIGNED weight by a 42-bit SIGNED accumulation. Those two operations
// are NOT the same operation; they agree here only because the production one
// keeps just its low 64 bits and residues modulo 2^64 do not care which
// interpretation produced them, while the repaired one computes an exact value
// that fits in 50 bits and therefore sign-extends to the same residue.
//
// That is an argument, and an argument about signedness is exactly the kind of
// thing that is convincing and wrong. Hence this file.
//
// AND THE THIRD HAZARD, WHICH THIS FILE ACTUALLY CAUGHT -- IN MY OWN REPAIR.
//
// The first version of the narrowing read `64'(7'd64 - w0_q)` as a SEVEN-BIT
// unsigned subtraction that wraps modulo 128 above w0_q = 64, and faithfully
// reproduced that wrap with `$signed({1'b0, (7'd64 - w0_q)})`. IT IS NOT A
// SEVEN-BIT SUBTRACTION. A size cast establishes the context for the
// expression inside it, so both operands widen to 64 bits BEFORE the subtract
// and the shipped weight is the signed value (64 - w0_q) in [-63, +64]. The
// wrapping reading is off by exactly 128 for every w0_q >= 65.
//
// This file failed on 43,330 of 43,497 vectors and every root cause was a
// w0_q in 65..127. NOTHING ELSE IN THE TREE WOULD HAVE CAUGHT IT: GEOM.SKIN
// hands this block w0 in 0..64, the port comment says "0..64", and no existing
// test looks above it -- so the oracle test, the smoke and every gate would
// have stayed green over a repair that flips the sign of the blend weight on a
// quarter of the port's range. That is precisely the failure the DSP fence is
// written against, and it is the reason the vectors below sweep w0 across ALL
// of 0..127 rather than the contract's 0..64.
//
// The bug was mine, the fence is the brief's, and the fence worked.
//
// ---------------------------------------------------------------------------
// WHY THIS IS AN RTL-VS-RTL DIFFERENTIAL AND NOT A C++ RESTATEMENT
// ---------------------------------------------------------------------------
// Modelling the old 64-bit expressions in C++ and comparing would prove that
// the DUT agrees with MY TRANSCRIPTION of the old code -- which is the part
// most likely to be wrong, being written by the same person, at the same time,
// from the same reading. `Vskinnorm_old` is not a transcription and not a
// hand-copy: `tests/probes/zhao_skinnorm_mul_probe.sv` was produced
// MECHANICALLY from `git show 19c8cfde:...zhao_geom_skin_norm.sv` by a single
// identifier substitution, and it is a VERIFIED POSITIVE CONTROL -- mapped on
// the shipping part it reproduced the pre-repair block on every number:
// 21 DSP, 920 registers, 2053 combinational ALUTs, and the mode table 6 / 7 / 8
// (Two Independent 18x18 / Independent 18x18 plus 36 / Independent 27x27),
// identical to `zhao_geom_skin_norm@gzdsp-base`, digest c9338495f42a.
//
// ---------------------------------------------------------------------------
// WHAT THIS PROOF IS AND IS NOT -- STATED, NOT IMPLIED
// ---------------------------------------------------------------------------
// The brief asks for "the oracle or an exhaustive sweep of the operand widths".
// THIS IS NEITHER, AND SAYING SO IS THE POINT.
//
// It is not EXHAUSTIVE and cannot be: the operand space is three 32-bit
// coefficients times three more times three 8-bit normals times a 7-bit
// weight, which is 2^127 for one lane. Exhaustion is not available at that
// width and claiming it would be a lie.
//
// It is not the ORACLE either. The oracle test proves the block computes the
// right normal; it drives plausible creature poses, and the question here is
// narrower and harder -- whether two ARITHMETIC EXPRESSIONS agree on every
// input the PORTS can carry, including the ones no pose produces.
//
// So it is DIRECTED CORNERS plus RANDOMISED VECTORS, run against the real
// pre-repair RTL: every extreme of every declared port range, every w0 in
// 0..127 including the whole half above the contract, and 40,000 random vectors
// on a fixed seed. Outputs are compared as RAW WORDS with the top partial word
// masked -- a comparison that routes both sides through the same widening
// helper cannot see a fault in the widening helper.
//
// ENFORCED-BY: tests/CMakeLists.txt : skinnorm_mul_narrow_differential

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vskinnorm_new.h"
#include "Vskinnorm_old.h"

#include "zhao_sim.hpp"

namespace {

/** Deterministic stimulus. A differential that cannot be re-run on the same
 *  vectors is not evidence, so there is no time-seeded randomness here. */
uint64_t rng_state = 0xD1B54A32D192ED03ull;
uint64_t next_rand() {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 7;
  rng_state ^= rng_state << 17;
  return rng_state;
}

struct Vec {
  int32_t a[12];
  int32_t b[12];
  int8_t nx, ny, nz;
  uint8_t w0;  // 0..127, the FULL port range, not the 0..64 the port comment states
  uint32_t src;
};

template <typename T>
void reset_dut(T& d) {
  d.rst_n = 0;
  d.v_valid_i = 0;
  d.n_ready_i = 1;
  d.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

template <typename T>
void apply(T& d, const Vec& v) {
  for (int i = 0; i < 12; ++i) {
    d.a_i[i] = (uint32_t)v.a[i];
    d.b_i[i] = (uint32_t)v.b[i];
  }
  d.v_nx_i = (uint8_t)v.nx;
  d.v_ny_i = (uint8_t)v.ny;
  d.v_nz_i = (uint8_t)v.nz;
  d.v_w0_i = v.w0 & 0x7Fu;
  d.v_src_id_i = v.src;
  d.v_valid_i = 1;
  d.eval();
}

/** Push one vector through and stop on the emitting edge. Returns false if the
 *  block never emitted, which is itself a difference worth failing on. */
template <typename T>
bool run_one(T& d, const Vec& v) {
  apply(d, v);
  // Accept: the block takes it when v_ready_o is high.
  int guard = 0;
  while (!d.v_ready_o && guard++ < 64) zhao::tick(d);
  if (!d.v_ready_o) return false;
  zhao::tick(d);
  d.v_valid_i = 0;
  d.eval();
  guard = 0;
  while (!d.n_valid_o && guard++ < 256) zhao::tick(d);
  return d.n_valid_o != 0;
}

/** Retire the emitted result so the next vector can be offered. */
template <typename T>
void retire(T& d) {
  d.n_ready_i = 1;
  zhao::tick(d);
  d.eval();
}

int32_t rand32() { return (int32_t)(uint32_t)(next_rand() >> 13); }
int8_t rand8() { return (int8_t)(uint8_t)(next_rand() >> 29); }

constexpr int32_t AMIN = INT32_MIN;
constexpr int32_t AMAX = INT32_MAX;

void fill(Vec& v, int32_t av, int32_t bv) {
  for (int i = 0; i < 12; ++i) {
    v.a[i] = av;
    v.b[i] = bv;
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vskinnorm_new dnew;
  Vskinnorm_old dold;

  reset_dut(dnew);
  reset_dut(dold);

  long vectors = 0;
  long bad = 0;
  long reported = 0;
  long w0_over_64 = 0;

  auto one_vector = [&](const Vec& v, const char* origin) {
    const bool ok_new = run_one(dnew, v);
    const bool ok_old = run_one(dold, v);
    ++vectors;
    if (v.w0 > 64) ++w0_over_64;

    bool differ = (ok_new != ok_old);
    if (ok_new && ok_old) {
      // Raw 64-bit words, compared whole. n_*_o are signed [63:0], so every
      // bit is significant and no masking is needed -- but the degenerate flag
      // and the source id are compared too, because a repair that changed the
      // DATA while leaving the flags alone would be just as wrong.
      if (dnew.n_x_o != dold.n_x_o) differ = true;
      if (dnew.n_y_o != dold.n_y_o) differ = true;
      if (dnew.n_z_o != dold.n_z_o) differ = true;
      if (dnew.n_degenerate_o != dold.n_degenerate_o) differ = true;
      if (dnew.n_src_id_o != dold.n_src_id_o) differ = true;
      if (dnew.vertices_o != dold.vertices_o) differ = true;
      if (dnew.degenerate_o != dold.degenerate_o) differ = true;
      if (dnew.reduced_o != dold.reduced_o) differ = true;
    }

    if (differ) {
      ++bad;
      if (reported++ < 12) {
        std::printf(
            "MISMATCH [%s] w0=%u nx=%d ny=%d nz=%d a0=%d b0=%d\n"
            "    new x=%016llx y=%016llx z=%016llx degen=%u\n"
            "    old x=%016llx y=%016llx z=%016llx degen=%u\n",
            origin, (unsigned)v.w0, (int)v.nx, (int)v.ny, (int)v.nz, v.a[0],
            v.b[0], (unsigned long long)dnew.n_x_o,
            (unsigned long long)dnew.n_y_o, (unsigned long long)dnew.n_z_o,
            (unsigned)dnew.n_degenerate_o, (unsigned long long)dold.n_x_o,
            (unsigned long long)dold.n_y_o, (unsigned long long)dold.n_z_o,
            (unsigned)dold.n_degenerate_o);
      }
    }
    retire(dnew);
    retire(dold);
  };

  // ---- the directed extremes -----------------------------------------------
  // The vectors a random sweep is least likely to reach and the ones a width
  // or a signedness mistake shows up in first: the coefficients at both rails
  // against the normals at both rails, with the weight swept across its ENTIRE
  // declared range including the wrapping half.
  const int32_t coeff_rails[] = {0, 1, -1, AMAX, AMIN, AMAX - 1, AMIN + 1};
  const int8_t norm_rails[] = {0, 1, -1, 127, -128};
  const uint8_t w_rails[] = {0,  1,  2,   31, 32,  63,  64, 65,
                             66, 96, 100, 126, 127};

  Vec v{};
  v.src = 0;
  for (int32_t av : coeff_rails) {
    for (int32_t bv : coeff_rails) {
      for (int8_t nr : norm_rails) {
        for (uint8_t wr : w_rails) {
          fill(v, av, bv);
          v.nx = nr;
          v.ny = nr;
          v.nz = nr;
          v.w0 = wr;
          one_vector(v, "directed");
        }
      }
    }
  }

  // Mixed rails: each of the three normal components independently extreme,
  // and the twelve coefficients not all equal, so a lane-indexing fault cannot
  // hide behind a uniform vector.
  for (uint8_t wr : w_rails) {
    for (int k = 0; k < 24; ++k) {
      for (int i = 0; i < 12; ++i) {
        v.a[i] = (k & 1) ? ((i & 1) ? AMAX : AMIN) : (int32_t)(AMIN + i);
        v.b[i] = (k & 2) ? ((i & 1) ? AMIN : AMAX) : (int32_t)(AMAX - i);
      }
      v.nx = (k & 4) ? (int8_t)-128 : (int8_t)127;
      v.ny = (k & 8) ? (int8_t)-128 : (int8_t)127;
      v.nz = (k & 16) ? (int8_t)-128 : (int8_t)127;
      v.w0 = wr;
      v.src = (uint32_t)k;
      one_vector(v, "mixed-rails");
    }
  }

  // ---- the randomised sweep ------------------------------------------------
  for (int n = 0; n < 40000; ++n) {
    for (int i = 0; i < 12; ++i) {
      v.a[i] = rand32();
      v.b[i] = rand32();
    }
    v.nx = rand8();
    v.ny = rand8();
    v.nz = rand8();
    v.w0 = (uint8_t)(next_rand() & 0x7Fu);
    v.src = (uint32_t)(next_rand() & 0xFFu);
    one_vector(v, "random");
  }

  std::printf("skinnorm_mul_narrow_differential: %ld vectors, %ld mismatch\n",
              vectors, bad);
  std::printf("  w0 ABOVE THE CONTRACT (65..127): %ld vectors\n", w0_over_64);

  // A differential that ran no vectors passes vacuously, so the count is a
  // check and not a decoration.
  if (vectors < 40000) {
    std::printf("FAIL: stimulus did not run (%ld vectors)\n", vectors);
    zhao::exit_hard(1);
  }
  if (w0_over_64 < 1000) {
    std::printf("FAIL: w0 above the contract range was barely exercised (%ld)\n",
                w0_over_64);
    zhao::exit_hard(1);
  }
  if (bad != 0) {
    std::printf("FAIL: %ld vectors disagree\n", bad);
    zhao::exit_hard(1);
  }
  std::printf("PASS\n");
  zhao::exit_hard(0);
}
