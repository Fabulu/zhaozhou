// attrsetup_mul_narrow_differential.cpp -- the 2026-09-26 DSP repair emits the
// same bits as the arithmetic it replaced, over the full declared port range.
//
// ---------------------------------------------------------------------------
// WHAT IS ACTUALLY AT RISK
// ---------------------------------------------------------------------------
// `zhao_geom_attrsetup` cost 45 DSP blocks -- 40% of the shipping
// 5CSEBA6U23I7 -- and 9 of them were being spent on a single property of how
// the x partials were written:
//
//     dndx_c = ((-(72'(cy_by))) * 72'(va_i) + ...) <<< PIXEL_SHIFT;
//
// Negating the SIGN-EXTENDED value makes `-(sext(x,72))` a 72-bit subtract from
// zero, after which Quartus can no longer see that the top 50 bits are a
// replication of bit 21, so it multiplies a genuine 72-bit operand. The repair
// multiplies at the operands' true widths instead: 45 DSP -> 36, and 940
// combinational ALUTs -> 836.
//
// THE BLOCK'S WHOLE JUSTIFICATION IS THAT IT EMITS EXACTLY THE ORACLE'S
// NUMERATOR, proved over 32,805 pixel-attributes in
// tests/proofs/attribute_plane_equivalence.cpp. Narrowing a MULTIPLY is legal;
// narrowing a RESULT is not. So the repair owes a proof, not an assertion, and
// "the oracle test still passes" is the weaker of the two available proofs --
// that test drives five triangles on a canvas, and the question here is whether
// two ARITHMETIC EXPRESSIONS agree on every input the ports can carry.
//
// ---------------------------------------------------------------------------
// WHY THIS IS AN RTL-VS-RTL DIFFERENTIAL AND NOT A C++ RESTATEMENT
// ---------------------------------------------------------------------------
// The obvious shape is to model the old 72-bit expressions in __int128 and
// compare. That proves the DUT agrees with MY TRANSCRIPTION of the old code,
// which is the thing most likely to be wrong -- the transcription is written by
// the same person, at the same time, from the same reading.
//
// Vattrsetup_old is not a transcription and not a copy. It is
// tests/probes/zhao_attrsetup_mul_probe.sv at VARIANT=0, the probe arm whose
// entire purpose was to be the packet's POSITIVE CONTROL: it reproduced the
// pre-repair block on all seven measured numbers (45 DSP, 24 Independent 27x27,
// 15 Two Independent 18x18, 6 Sum of two 18x18, 225 registers, 940
// combinational ALUTs, 468 virtual pins) against
// zhao_geom_attrsetup@gz-base, digest bd84da4c2517.
//
// THAT MAKES ARM 0 LOAD-BEARING AND IT MUST NOT BE "UPDATED TO MATCH
// PRODUCTION". A committed copy goes stale in the flattering direction, and the
// usual repair is to refresh it -- here the opposite is true. Arm 0 is
// deliberately the arithmetic production NO LONGER HAS, it is the baseline
// every row in the eight-arm table was measured against, and refreshing it
// would silently turn this differential into a comparison of the new block with
// itself, which passes forever and proves nothing. The probe's header says so
// too.
//
// ---------------------------------------------------------------------------
// THE STIMULUS, AND WHY IT IS THE DECLARED RANGE RATHER THAN A PLAUSIBLE ONE
// ---------------------------------------------------------------------------
// GEOM.CLIP hands this block winding-normalised on-canvas triangles. This test
// deliberately does NOT restrict itself to those: the arithmetic is
// unconditional -- there is no valid/area guard anywhere in the datapath -- so
// the honest question is whether the two expressions agree for every value the
// PORTS can carry. Every coordinate sweeps the full signed 21 bits and every
// attribute the full signed 32, including both extremes of each.
//
// Bits are compared as RAW WORDS with the top partial word masked, not through
// a sign-extended __int128. A comparison that routes both sides through the same
// widening helper cannot see a fault in the widening helper.
//
// ENFORCED-BY: tests/CMakeLists.txt : attrsetup_mul_narrow_differential

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vattrsetup_new.h"
#include "Vattrsetup_old.h"

#include "zhao_sim.hpp"

namespace {

/** Deterministic stimulus. A differential that cannot be re-run on the same
 *  vectors is not evidence, so there is no time-seeded randomness here. */
uint64_t rng_state = 0x9E3779B97F4A7C15ull;
uint64_t next_rand() {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 7;
  rng_state ^= rng_state << 17;
  return rng_state;
}

constexpr uint32_t COORD_MASK = 0x1FFFFFu;  // signed [20:0]

struct Vec {
  int32_t ax, ay, bx, by, cx, cy;  // 21-bit signed, as stored
  int32_t va, vb, vc;              // 32-bit signed
};

/** Compare `bits` bits of two Verilator wide-port word arrays, exactly. */
bool words_equal(const uint32_t* a, const uint32_t* b, int bits) {
  const int words = (bits + 31) / 32;
  for (int i = 0; i < words; ++i) {
    const int lo = i * 32;
    const int valid = (bits - lo) >= 32 ? 32 : (bits - lo);
    const uint32_t mask = (valid >= 32) ? 0xFFFFFFFFu : ((1u << valid) - 1u);
    if ((a[i] & mask) != (b[i] & mask)) return false;
  }
  return true;
}

template <typename T>
void reset_dut(T& d) {
  d.rst_n = 0;
  d.v_valid_i = 0;
  d.r_ready_i = 1;
  d.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

template <typename T>
void drive(T& d, const Vec& v) {
  d.ax_i = (uint32_t)v.ax & COORD_MASK;
  d.ay_i = (uint32_t)v.ay & COORD_MASK;
  d.bx_i = (uint32_t)v.bx & COORD_MASK;
  d.by_i = (uint32_t)v.by & COORD_MASK;
  d.cx_i = (uint32_t)v.cx & COORD_MASK;
  d.cy_i = (uint32_t)v.cy & COORD_MASK;
  d.va_i = (uint32_t)v.va;
  d.vb_i = (uint32_t)v.vb;
  d.vc_i = (uint32_t)v.vc;
  d.v_valid_i = 1;
  zhao::tick(d);
  d.v_valid_i = 0;
  d.eval();
}

/** Clear the accepted result so the next vector can be offered. */
template <typename T>
void retire(T& d) {
  zhao::tick(d);
  d.eval();
}

int32_t rand_coord() {
  // Full signed 21-bit range, sign-extended into int32_t.
  const uint32_t raw = (uint32_t)(next_rand() & COORD_MASK);
  return (raw & 0x100000u) ? (int32_t)(raw | ~COORD_MASK) : (int32_t)raw;
}

int32_t rand_attr() { return (int32_t)(uint32_t)(next_rand() >> 11); }

constexpr int32_t CMIN = -1048576;  // -2^20
constexpr int32_t CMAX = 1048575;   //  2^20 - 1
constexpr int32_t AMIN = INT32_MIN;
constexpr int32_t AMAX = INT32_MAX;

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vattrsetup_new dnew;
  Vattrsetup_old dold;

  reset_dut(dnew);
  reset_dut(dold);

  // ---- the directed extremes -----------------------------------------------
  // These are the vectors a random sweep is least likely to reach and the ones
  // a width mistake shows up in first: the coordinate differences at their
  // maximum magnitude in both directions, against the attributes at theirs.
  // cy_by reaches -(2^21 - 1) and +(2^21 - 1) here, which is the widest the
  // 22-bit difference can be for ANY 21-bit inputs -- see the note at the end.
  const Vec directed[] = {
      {0, 0, 0, 0, 0, 0, 0, 0, 0},
      {CMAX, CMAX, CMAX, CMAX, CMAX, CMAX, AMAX, AMAX, AMAX},
      {CMIN, CMIN, CMIN, CMIN, CMIN, CMIN, AMIN, AMIN, AMIN},
      // every partial at maximum POSITIVE magnitude, attributes at both rails
      {CMIN, CMIN, CMIN, CMIN, CMAX, CMAX, AMAX, AMIN, AMAX},
      {CMIN, CMIN, CMIN, CMIN, CMAX, CMAX, AMIN, AMAX, AMIN},
      // every partial at maximum NEGATIVE magnitude
      {CMAX, CMAX, CMAX, CMAX, CMIN, CMIN, AMAX, AMIN, AMAX},
      {CMAX, CMAX, CMAX, CMAX, CMIN, CMIN, AMIN, AMAX, AMIN},
      // one partial extreme at a time, the others zero
      {0, 0, 0, CMAX, 0, CMIN, AMAX, AMAX, AMAX},
      {0, 0, 0, CMIN, 0, CMAX, AMIN, AMIN, AMIN},
      {CMIN, CMAX, CMAX, CMIN, CMIN, CMAX, AMIN, AMAX, 0},
      // attributes at the rails with small coordinates: exercises the sign of
      // the product rather than its magnitude
      {1, 2, 3, 4, 5, 6, AMIN, AMIN, AMIN},
      {1, 2, 3, 4, 5, 6, AMAX, AMIN, 0},
      {-1, -2, -3, -4, -5, -6, AMIN, 0, AMAX},
  };

  long vectors = 0;
  long bad_n0 = 0, bad_dx = 0, bad_dy = 0, bad_valid = 0;
  long reported = 0;

  auto one_vector = [&](const Vec& v, const char* origin) {
    drive(dnew, v);
    drive(dold, v);

    if (dnew.r_valid_o != 1 || dold.r_valid_o != 1) {
      ++bad_valid;
    } else {
      const bool ok_n0 = words_equal(dnew.n0_o.data(), dold.n0_o.data(), 96);
      const bool ok_dx = words_equal(dnew.dndx_o.data(), dold.dndx_o.data(), 72);
      const bool ok_dy = words_equal(dnew.dndy_o.data(), dold.dndy_o.data(), 72);
      if (!ok_n0) ++bad_n0;
      if (!ok_dx) ++bad_dx;
      if (!ok_dy) ++bad_dy;
      if ((!ok_n0 || !ok_dx || !ok_dy) && reported < 5) {
        ++reported;
        printf(
            "      MISMATCH (%s) a=(%d,%d) b=(%d,%d) c=(%d,%d) v=(%d,%d,%d)"
            "  n0=%d dndx=%d dndy=%d\n",
            origin, v.ax, v.ay, v.bx, v.by, v.cx, v.cy, v.va, v.vb, v.vc, (int)ok_n0, (int)ok_dx,
            (int)ok_dy);
      }
    }
    retire(dnew);
    retire(dold);
    ++vectors;
  };

  for (const Vec& v : directed) one_vector(v, "directed");
  const long directed_count = vectors;

  // ---- the randomised sweep, full declared range ---------------------------
  for (int i = 0; i < 20000; ++i) {
    Vec v{};
    v.ax = rand_coord();
    v.ay = rand_coord();
    v.bx = rand_coord();
    v.by = rand_coord();
    v.cx = rand_coord();
    v.cy = rand_coord();
    v.va = rand_attr();
    v.vb = rand_attr();
    v.vc = rand_attr();
    one_vector(v, "random");
  }

  zhao::check(bad_valid == 0, "both machines accept and produce on every vector", 0,
              (uint32_t)bad_valid);
  zhao::check(bad_n0 == 0, "n0_o is bit-identical to the pre-repair arithmetic, all 96 bits", 0,
              (uint32_t)bad_n0);
  zhao::check(bad_dx == 0, "dndx_o is bit-identical to the pre-repair arithmetic, all 72 bits", 0,
              (uint32_t)bad_dx);
  zhao::check(bad_dy == 0, "dndy_o is bit-identical to the pre-repair arithmetic, all 72 bits", 0,
              (uint32_t)bad_dy);

  // THE NEGATIVE CONTROL. Everything above is an assertion that two machines
  // AGREE, and a differential whose two sides are secretly the same module
  // agrees forever while proving nothing -- the exact way a refreshed positive
  // control stops being one. So: feed the two DUTs DIFFERENT stimulus and
  // require the comparison to FAIL. If this does not fail, the harness is not
  // comparing anything and every green above is worthless.
  {
    const Vec p{10, 20, 30, 40, 50, 60, 111111, 222222, 333333};
    const Vec q{10, 20, 30, 40, 50, 61, 111111, 222222, 333333};  // cy differs by 1
    drive(dnew, p);
    drive(dold, q);
    const bool same = words_equal(dnew.n0_o.data(), dold.n0_o.data(), 96) &&
                      words_equal(dnew.dndx_o.data(), dold.dndx_o.data(), 72) &&
                      words_equal(dnew.dndy_o.data(), dold.dndy_o.data(), 72);
    retire(dnew);
    retire(dold);
    zhao::check(!same, "NEGATIVE CONTROL: differing stimulus is seen to differ", 1,
                same ? 0u : 1u);
  }

  printf("   MEASURED: %ld vectors (%ld directed + %ld random), %ld output words compared\n",
         vectors, directed_count, vectors - directed_count, vectors * 3);
  printf(
      "   NOTE: a 22-bit difference of two 21-bit signed coordinates cannot reach -2^21 for\n"
      "   ANY port values (its range is +/-(2^21 - 1)), so the 23rd bit in the repaired\n"
      "   negation is headroom, not a reachable case. It is kept because it makes the\n"
      "   expression exact for the full DECLARED width of cy_by without depending on a range\n"
      "   argument about its producer, and because arm 6 measured it to cost nothing.\n");
  return zhao::report_and_exit("attrsetup_mul_narrow_differential");
}
