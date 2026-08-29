// field_v3_trig_directed.cpp — OP_SIN and OP_COS, against the shipped oracle.
//
// `zhao_field_sin` is already trusted: the rotation service has used it since
// it closed at 24 mutants. What is unproven here is the SEQUENCER around it --
// four points walked through one lookup unit with a latency of two, which is
// the exact shape that has now produced an off-by-one twice in this engine.
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_v3_trig.h"

#include "zfield/zfield.hpp"
#include "zfield/zfield_steps.hpp"
#include "zhao_sim.hpp"

namespace {

constexpr int kLanes = 4;

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint64_t next64() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  }
  uint32_t u32() { return (uint32_t)(next64() >> 21); }
};

void reset(Vzhao_field_v3_trig& t) {
  t.rst_n = 0;
  t.v_valid_i = 0;
  t.r_ready_i = 0;
  t.is_cos_i = 0;
  t.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();
  zhao::tick(t);
}

/** One group. Returns clocks, or -1 if it never replied. */
int run(Vzhao_field_v3_trig& t, bool is_cos, const int32_t a[kLanes], uint8_t tag,
        int32_t out[kLanes], uint8_t* out_tag) {
  t.v_valid_i = 1;
  t.is_cos_i = is_cos ? 1 : 0;
  t.a0_0_i = (uint32_t)a[0];
  t.a0_1_i = (uint32_t)a[1];
  t.a0_2_i = (uint32_t)a[2];
  t.a0_3_i = (uint32_t)a[3];
  t.tag_i = tag;
  t.r_ready_i = 1;
  t.eval();

  int guard = 0;
  while (!t.v_ready_o && guard++ < 64) zhao::tick(t);
  if (!t.v_ready_o) return -1;
  zhao::tick(t);  // the accept
  t.v_valid_i = 0;
  t.eval();

  guard = 0;
  while (!t.r_valid_o && guard++ < 256) zhao::tick(t);
  if (!t.r_valid_o) return -1;

  out[0] = (int32_t)t.o0_0_o;
  out[1] = (int32_t)t.o0_1_o;
  out[2] = (int32_t)t.o0_2_o;
  out[3] = (int32_t)t.o0_3_o;
  if (out_tag) *out_tag = (uint8_t)t.tag_o;
  zhao::tick(t);  // the reply is taken
  t.r_ready_i = 0;
  t.eval();
  return guard;
}

int32_t oracle(bool is_cos, int32_t src) {
  zref::SatLedger L;
  int32_t dst = 0;
  zfield::steps::exec_op(is_cos ? zfield::OP_COS : zfield::OP_SIN, 0u, {}, &src, &dst, &L);
  return dst;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_field_v3_trig dut;

  printf("== section 1: the quadrant boundaries, both modes ==\n");
  {
    // Where a sign or a table-index error actually shows. 0x4000 is a quarter
    // turn, 0x8000 a half, 0xC000 three quarters -- the exact angles at which
    // sin and cos swap roles and change sign.
    const int32_t a[kLanes] = {0x0000, 0x4000, 0x8000, 0xC000};
    for (int mode = 0; mode < 2; ++mode) {
      reset(dut);
      int32_t got[kLanes] = {};
      uint8_t tag = 0;
      const int c = run(dut, mode != 0, a, (uint8_t)(0x40 + mode), got, &tag);
      zhao::check(c >= 0, mode ? "COS group completes" : "SIN group completes", 1, c >= 0 ? 1 : 0);
      zhao::check(tag == (uint8_t)(0x40 + mode), "and its tag comes back", (uint8_t)(0x40 + mode),
                  tag);
      for (int l = 0; l < kLanes; ++l) {
        const int32_t want = oracle(mode != 0, a[l]);
        char what[96];
        snprintf(what, sizeof what, "%s lane %d angle %04X", mode ? "COS" : "SIN", l,
                 (unsigned)a[l]);
        zhao::check(got[l] == want, what, (uint32_t)want, (uint32_t)got[l]);
      }
      printf("   MEASURED: %s group in %d clocks\n", mode ? "COS" : "SIN", c);
    }
  }

  printf("== section 2: the TOP HALF of the source is ignored, by law ==\n");
  {
    // The same law the rotation service states for its angle port. A caller
    // that leaves rubbish above bit 15 must get a defined answer and the same
    // one the software gives -- so this is asserted, not assumed.
    reset(dut);
    const int32_t clean[kLanes] = {0x1234, 0x5678, 0x9ABC, 0xDEF0};
    const int32_t dirty[kLanes] = {(int32_t)0xAAAA1234u, (int32_t)0x00015678u, (int32_t)0x7FFF9ABCu,
                                   (int32_t)0xFFFFDEF0u};
    int32_t g1[kLanes] = {}, g2[kLanes] = {};
    (void)run(dut, false, clean, 0x51, g1, nullptr);
    (void)run(dut, false, dirty, 0x52, g2, nullptr);
    int diff = 0;
    for (int l = 0; l < kLanes; ++l)
      if (g1[l] != g2[l]) ++diff;
    zhao::check(diff == 0, "rubbish above bit 15 changes nothing", 0, diff);
    for (int l = 0; l < kLanes; ++l)
      zhao::check(g1[l] == oracle(false, clean[l]), "and the answer is still the oracle's",
                  (uint32_t)oracle(false, clean[l]), (uint32_t)g1[l]);
  }

  printf("== section 3: many angles, both modes, lanes all different ==\n");
  {
    // Every lane gets a DIFFERENT angle every group. Equal angles would pass
    // even if the walk captured one lane's answer into all four -- which is
    // exactly the failure a latency-2 walk produces when it is got wrong.
    reset(dut);
    Prng rng(0xC0FFEE);
    int wrong = 0, groups = 0;
    for (int g = 0; g < 64; ++g) {
      const bool is_cos = (g & 1) != 0;
      int32_t a[kLanes];
      for (int l = 0; l < kLanes; ++l) a[l] = (int32_t)(rng.u32() & 0xFFFFu);
      int32_t got[kLanes] = {};
      if (run(dut, is_cos, a, (uint8_t)g, got, nullptr) < 0) {
        ++wrong;
        continue;
      }
      ++groups;
      for (int l = 0; l < kLanes; ++l)
        if (got[l] != oracle(is_cos, a[l])) ++wrong;
    }
    zhao::check(wrong == 0, "64 random groups, 256 lanes, all match the oracle", 0, wrong);
    printf("   MEASURED: %d groups back to back\n", groups);
  }

  printf("== section 4: a slow consumer does not lose the answer ==\n");
  {
    // The reply is held until it is taken. A service that dropped it while the
    // dispatcher was busy elsewhere would lose a VALUE, not merely time.
    reset(dut);
    const int32_t a[kLanes] = {0x0111, 0x2222, 0x3333, 0x4444};
    dut.v_valid_i = 1;
    dut.is_cos_i = 0;
    dut.a0_0_i = (uint32_t)a[0];
    dut.a0_1_i = (uint32_t)a[1];
    dut.a0_2_i = (uint32_t)a[2];
    dut.a0_3_i = (uint32_t)a[3];
    dut.tag_i = 0x77;
    dut.r_ready_i = 0;  // deliberately NOT ready
    dut.eval();
    zhao::tick(dut);
    dut.v_valid_i = 0;
    dut.eval();

    int guard = 0;
    while (!dut.r_valid_o && guard++ < 256) zhao::tick(dut);
    zhao::check(dut.r_valid_o == 1, "the answer becomes valid", 1, (uint32_t)dut.r_valid_o);

    for (int i = 0; i < 40; ++i) zhao::tick(dut);
    zhao::check(dut.r_valid_o == 1, "and is STILL held 40 clocks later", 1,
                (uint32_t)dut.r_valid_o);
    int held = 0;
    for (int l = 0; l < kLanes; ++l) {
      const int32_t got = (int32_t)(l == 0   ? dut.o0_0_o
                                    : l == 1 ? dut.o0_1_o
                                    : l == 2 ? dut.o0_2_o
                                             : dut.o0_3_o);
      if (got == oracle(false, a[l])) ++held;
    }
    zhao::check(held == kLanes, "with all four answers intact", kLanes, held);
    zhao::check(dut.tag_o == 0x77, "and the tag intact", 0x77, (uint32_t)dut.tag_o);
  }

  return zhao::report_and_exit("field_v3_trig_directed");
}
