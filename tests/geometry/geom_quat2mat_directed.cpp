// geom_quat2mat_directed.cpp — GEOM.POSE's quaternion step against
// `zref::creature::quat16_to_mat3`.
//
// Every case is a differential: the Verilated `zhao_geom_quat2mat` and the
// reference run the same quaternion and every one of the twelve matrix elements
// must be identical. The oracle is the function the reference renderer poses
// every creature with, so agreement means the hardware puts bones where the
// shipped pictures put them.
//
// Two laws here are easy to "improve" into being wrong, and both get their own
// section:
//
//   * NO RENORMALIZATION (creature_rules 2.2). A quantized quaternion is not
//     exactly unit. An implementation that normalizes would produce a *better*
//     rotation matrix and would disagree with every pose the reference has ever
//     produced. Section 5 drives deliberately non-unit quaternions so that a
//     renormalizing implementation fails rather than looking tidy.
//
//   * ROUND-HALF-UP ON NEGATIVES. Five of the nine element expressions are
//     differences, so negative intermediates are the common case, not the
//     corner. A bare arithmetic shift floors instead, and disagrees at every
//     exact half.

#include "Vzhao_geom_quat2mat.h"
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

/** Drive one quaternion and compare all twelve elements against the oracle. */
void diff(Vzhao_geom_quat2mat& dut, int16_t w, int16_t x, int16_t y, int16_t z,
          const char* what) {
  zc::quat16 q;
  q.q[0] = w; q.q[1] = x; q.q[2] = y; q.q[3] = z;
  zc::mat3x4fx want{};
  zc::quat16_to_mat3(q, want, nullptr);

  dut.q_valid_i = 1;
  dut.m_ready_i = 1;
  dut.q_w_i = static_cast<uint16_t>(w);
  dut.q_x_i = static_cast<uint16_t>(x);
  dut.q_y_i = static_cast<uint16_t>(y);
  dut.q_z_i = static_cast<uint16_t>(z);
  dut.q_bone_i = 0x5A;
  dut.eval();
  zhao::tick(dut);
  dut.q_valid_i = 0;
  dut.eval();

  const std::string t(what);
  for (int i = 0; i < 12; ++i) {
    const int32_t got = static_cast<int32_t>(dut.m_o[i]);
    char lbl[160];
    std::snprintf(lbl, sizeof lbl, "%s: m[%d]", t.c_str(), i);
    check(got == want.m[i], lbl, static_cast<uint64_t>(static_cast<uint32_t>(want.m[i])),
          static_cast<uint32_t>(got));
  }
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
  Vzhao_geom_quat2mat dut;
  dut.rst_n = 0;
  dut.q_valid_i = 0;
  dut.m_ready_i = 1;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  const int16_t ONE = 16384;  // zc::kQuatOne, S1.0.14

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    Prng rng(0x9A7Fu);
    for (uint32_t k = 0; k < iters && zhao::check_failures() == 0; ++k) {
      // The FULL s16 range, not a unit-quaternion range. The block must agree
      // with the oracle on every input the port can carry, and the width
      // argument in the RTL header claims exactly that; sampling only plausible
      // quaternions would leave the claim untested.
      const int16_t w = static_cast<int16_t>(rng.next());
      const int16_t x = static_cast<int16_t>(rng.next());
      const int16_t y = static_cast<int16_t>(rng.next());
      const int16_t z = static_cast<int16_t>(rng.next());
      char tag[96];
      std::snprintf(tag, sizeof tag, "random[%u] (%d,%d,%d,%d)", k, w, x, y, z);
      diff(dut, w, x, y, z, tag);
    }
    dut.final();
    return zhao::report_and_exit("geom_quat2mat_random");
  }

  // ---- 1. identity: the pose every bad clip id falls back to ---------------
  diff(dut, ONE, 0, 0, 0, "identity quaternion");
  {
    // Asserted directly, not just differentially: the reference could agree
    // with the RTL and both be wrong about what identity means.
    check(static_cast<int32_t>(dut.m_o[0]) == 65536, "identity: m[0] is 1.0", 65536,
          static_cast<uint32_t>(dut.m_o[0]));
    check(static_cast<int32_t>(dut.m_o[5]) == 65536, "identity: m[5] is 1.0", 65536,
          static_cast<uint32_t>(dut.m_o[5]));
    check(static_cast<int32_t>(dut.m_o[10]) == 65536, "identity: m[10] is 1.0", 65536,
          static_cast<uint32_t>(dut.m_o[10]));
    check(dut.m_o[1] == 0 && dut.m_o[2] == 0 && dut.m_o[4] == 0 && dut.m_o[6] == 0 &&
              dut.m_o[8] == 0 && dut.m_o[9] == 0,
          "identity: every off-diagonal is zero", 0, 1);
  }

  // ---- 2. the half-turns, where the signs are easiest to transpose ---------
  // A transposed matrix is the inverse rotation: a creature's limb bends the
  // wrong way. These three pin the sign layout element by element.
  diff(dut, 0, ONE, 0, 0, "180 deg about X");
  diff(dut, 0, 0, ONE, 0, "180 deg about Y");
  diff(dut, 0, 0, 0, ONE, "180 deg about Z");

  // ---- 3. quarter turns: the off-diagonals are non-zero and asymmetric -----
  // cos(45) == sin(45) == 11585 in S1.0.14 (16384 / sqrt(2), rounded).
  {
    const int16_t H = 11585;
    diff(dut, H, H, 0, 0, "90 deg about X");
    diff(dut, H, 0, H, 0, "90 deg about Y");
    diff(dut, H, 0, 0, H, "90 deg about Z");
    diff(dut, H, -H, 0, 0, "-90 deg about X");
    diff(dut, H, 0, -H, 0, "-90 deg about Y");
    diff(dut, H, 0, 0, -H, "-90 deg about Z");
  }

  // ---- 4. the translation column is structurally zero ----------------------
  // Not an input, not a computation. The rest translation is inserted by the
  // stage above; if these ever became non-zero the bone would drift on rotation
  // alone.
  {
    diff(dut, 9000, -7000, 5000, -3000, "arbitrary rotation");
    check(dut.m_o[3] == 0 && dut.m_o[7] == 0 && dut.m_o[11] == 0,
          "translation column stays zero for a non-trivial rotation", 0, 1);
  }

  // ---- 5. NO RENORMALIZATION ----------------------------------------------
  // These quaternions are deliberately not unit -- one far too short, one far
  // too long, one degenerate. The reference does not correct them and neither
  // may the RTL. An implementation that normalized would pass every case above
  // and fail here, which is the entire point of the section.
  {
    diff(dut, ONE / 2, 0, 0, 0, "non-unit: half length");
    diff(dut, ONE * 2 - 1, 0, 0, 0, "non-unit: double length");
    diff(dut, 100, 100, 100, 100, "non-unit: tiny, all lanes equal");
    diff(dut, 0, 0, 0, 0, "degenerate: the all-zero quaternion");
    check(static_cast<int32_t>(dut.m_o[0]) == 65536 && static_cast<int32_t>(dut.m_o[5]) == 65536 &&
              static_cast<int32_t>(dut.m_o[10]) == 65536,
          "the zero quaternion yields identity, uncorrected", 1, 1);
  }

  // ---- 6. the s16 extremes: the no-saturation bound, driven -----------------
  // The RTL header argues that saturation can never fire for any s16 input.
  // These are the corners that argument rests on, so they are driven rather
  // than trusted.
  {
    const int16_t MX = 32767, MN = -32768;
    diff(dut, MX, MX, MX, MX, "extreme: all +32767");
    diff(dut, MN, MN, MN, MN, "extreme: all -32768");
    diff(dut, MN, MX, MN, MX, "extreme: alternating rails");
    diff(dut, MX, MN, MX, MN, "extreme: alternating rails, swapped");
    diff(dut, 0, MN, 0, MN, "extreme: -32768 in two lanes");
  }

  // ---- 7. the rounding boundary --------------------------------------------
  // A product landing on an exact multiple of 1024 (half of 2^11) is where
  // round-half-up and a plain shift disagree. 32 * 32 == 1024, so these land
  // exactly on the half; the negative cases separate round-toward-+inf from
  // floor, and five of the nine elements are differences.
  {
    diff(dut, 0, 32, 32, 0, "rounding: product exactly one half");
    diff(dut, 0, 32, -32, 0, "rounding: negative exact half");
    diff(dut, 32, 32, 32, 32, "rounding: every product on a half");
    diff(dut, 0, 33, 32, 0, "rounding: just above the half");
    diff(dut, 0, 31, 32, 0, "rounding: just below the half");
    diff(dut, -32, 32, -32, 32, "rounding: mixed signs on halves");
  }

  // ---- 8. the interface laws ----------------------------------------------
  // Learned from GEOM.SKIN, where a mutation zeroing the tag survived every
  // arithmetic check because all of them compared numbers and nothing else.
  {
    const uint32_t before = dut.bones_decoded_o;
    dut.q_valid_i = 1;
    dut.m_ready_i = 1;
    dut.q_w_i = static_cast<uint16_t>(ONE);
    dut.q_x_i = 0; dut.q_y_i = 0; dut.q_z_i = 0;
    dut.q_bone_i = 0x2B;
    dut.eval();
    zhao::tick(dut);
    dut.q_valid_i = 0;
    dut.eval();
    check(dut.m_bone_o == 0x2B, "the bone tag rides through with its matrix", 0x2B, dut.m_bone_o);
    check(dut.m_valid_o == 1, "an accepted bone raises m_valid", 1, dut.m_valid_o);
    check(dut.bones_decoded_o == before + 1, "an accepted bone advances the counter by one",
          before + 1, dut.bones_decoded_o);

    const uint32_t held = dut.bones_decoded_o;
    dut.q_valid_i = 1;
    dut.m_ready_i = 0;         // downstream stalls
    dut.q_bone_i = 0x77;
    dut.eval();
    check(dut.q_ready_o == 0, "a full output register refuses a new bone", 0, dut.q_ready_o);
    zhao::tick(dut);
    dut.eval();
    check(dut.bones_decoded_o == held, "a stalled bone is not counted as decoded", held,
          dut.bones_decoded_o);
    check(dut.m_bone_o == 0x2B, "a stalled bone does not overwrite the held result", 0x2B,
          dut.m_bone_o);

    dut.q_valid_i = 0;
    dut.m_ready_i = 1;
    dut.eval();
    zhao::tick(dut);
    dut.eval();
    check(dut.m_valid_o == 0, "m_valid drops once the matrix is taken", 0, dut.m_valid_o);
  }

  dut.final();
  return zhao::report_and_exit("geom_quat2mat_directed");
}
