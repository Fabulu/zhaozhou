// field_v3_len_directed.cpp — LEN2, LEN3 and DIST2 against the shipped oracle.
//
// One block, three opcodes, because the reference already says they are one:
// `len_of` is the whole of all three and the modes differ only in how many
// components go in and whether a saturating subtract happens first.
//
// The root is exact. `zhao_field_isqrt` is the engine's floor-exact restoring
// unit, so "close enough" is not a passing answer here -- every lane must be
// the integer the software produces.
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_v3_len.h"

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
  uint32_t u32() { return (uint32_t)(next64() >> 19); }
  /** Values that actually break things: extremes, zero, and the ordinary. */
  int32_t interesting() {
    switch (u32() % 8u) {
      case 0:
        return 0;
      case 1:
        return INT32_MAX;
      case 2:
        return INT32_MIN;
      case 3:
        return -1;
      case 4:
        return (int32_t)(1u << 30);
      case 5:
        return -(int32_t)(1u << 30);
      default:
        return (int32_t)u32();
    }
  }
};

// ---- the four-wide bank, modelled as the engine drives it -----------------
struct MulBank {
  // TWO DEEP AND PIPELINED, because zhao_field_v3_mulbank is: "each is two
  // clocks deep and FULLY PIPELINED". A one-at-a-time model measures this
  // service's own scaffolding rather than the machine.
  bool st_v[2] = {false, false};
  int64_t st_p[2][kLanes] = {};
  bool busy = false;
  int cnt = 0;
  int64_t p[kLanes] = {0, 0, 0, 0};
  bool grant = true;
  bool flaky = false;
  int refusals = 0;
  uint64_t rng = 0x243F6A8885A308D3ull;
  uint64_t next() {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
  }
};

int64_t sx33(uint64_t v) { return ((int64_t)(v << 31)) >> 31; }

template <typename W>
void set66(W& w, int64_t p) {
  w[0] = (uint32_t)((uint64_t)p & 0xFFFFFFFFull);
  w[1] = (uint32_t)(((uint64_t)p >> 32) & 0xFFFFFFFFull);
  w[2] = (p < 0) ? 0x3u : 0x0u;
}

void step(Vzhao_field_v3_len& dut, MulBank& mb) {
  if (mb.flaky) mb.grant = (mb.next() % 4u) != 0u;
  dut.mul_ready_i = mb.grant ? 1 : 0;
  if (mb.st_v[1]) {
    set66(dut.mul_p_0_i, mb.st_p[1][0]);
    set66(dut.mul_p_1_i, mb.st_p[1][1]);
    set66(dut.mul_p_2_i, mb.st_p[1][2]);
    set66(dut.mul_p_3_i, mb.st_p[1][3]);
    dut.mul_valid_i = 1;
  } else {
    dut.mul_valid_i = 0;
  }
  dut.eval();
  mb.st_v[1] = mb.st_v[0];
  for (int l = 0; l < kLanes; ++l) mb.st_p[1][l] = mb.st_p[0][l];
  mb.st_v[0] = false;
  if (dut.mul_issue_o && !mb.grant) {
    ++mb.refusals;
  } else if (dut.mul_issue_o) {
    mb.st_p[0][0] = sx33(dut.mul_a_0_o) * sx33(dut.mul_b_0_o);
    mb.st_p[0][1] = sx33(dut.mul_a_1_o) * sx33(dut.mul_b_1_o);
    mb.st_p[0][2] = sx33(dut.mul_a_2_o) * sx33(dut.mul_b_2_o);
    mb.st_p[0][3] = sx33(dut.mul_a_3_o) * sx33(dut.mul_b_3_o);
    mb.st_v[0] = true;
  }
  zhao::tick(dut);
}

void reset(Vzhao_field_v3_len& dut, MulBank& mb) {
  dut.rst_n = 0;
  dut.v_valid_i = 0;
  dut.r_ready_i = 0;
  dut.mul_valid_i = 0;
  dut.mul_ready_i = 1;
  mb = MulBank{};
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);
}

/** a[3][4] are operand a's components; b[2][4] are operand b's. */
int run(Vzhao_field_v3_len& dut, MulBank& mb, int mode, const int32_t a[3][kLanes],
        const int32_t b[2][kLanes], uint8_t tag, int32_t out[kLanes], uint8_t* sat,
        uint8_t* out_tag) {
  dut.v_valid_i = 1;
  dut.mode_i = (uint8_t)mode;
  dut.a0_0_i = (uint32_t)a[0][0];
  dut.a0_1_i = (uint32_t)a[0][1];
  dut.a0_2_i = (uint32_t)a[0][2];
  dut.a0_3_i = (uint32_t)a[0][3];
  dut.a1_0_i = (uint32_t)a[1][0];
  dut.a1_1_i = (uint32_t)a[1][1];
  dut.a1_2_i = (uint32_t)a[1][2];
  dut.a1_3_i = (uint32_t)a[1][3];
  dut.a2_0_i = (uint32_t)a[2][0];
  dut.a2_1_i = (uint32_t)a[2][1];
  dut.a2_2_i = (uint32_t)a[2][2];
  dut.a2_3_i = (uint32_t)a[2][3];
  dut.b0_0_i = (uint32_t)b[0][0];
  dut.b0_1_i = (uint32_t)b[0][1];
  dut.b0_2_i = (uint32_t)b[0][2];
  dut.b0_3_i = (uint32_t)b[0][3];
  dut.b1_0_i = (uint32_t)b[1][0];
  dut.b1_1_i = (uint32_t)b[1][1];
  dut.b1_2_i = (uint32_t)b[1][2];
  dut.b1_3_i = (uint32_t)b[1][3];
  dut.tag_i = tag;
  dut.r_ready_i = 1;
  dut.eval();

  int guard = 0;
  while (!dut.v_ready_o && guard++ < 4000) step(dut, mb);
  if (!dut.v_ready_o) return -1;
  step(dut, mb);
  dut.v_valid_i = 0;
  dut.eval();

  guard = 0;
  while (!dut.r_valid_o && guard++ < 8000) step(dut, mb);
  if (!dut.r_valid_o) return -1;

  out[0] = (int32_t)dut.o0_0_o;
  out[1] = (int32_t)dut.o0_1_o;
  out[2] = (int32_t)dut.o0_2_o;
  out[3] = (int32_t)dut.o0_3_o;
  if (sat) *sat = (uint8_t)dut.sat_rescale_o;
  if (out_tag) *out_tag = (uint8_t)dut.tag_o;
  step(dut, mb);
  dut.r_ready_i = 0;
  dut.eval();
  return guard;
}

uint8_t op_of(int mode) {
  return mode == 1 ? zfield::OP_LEN3 : (mode == 2 ? zfield::OP_DIST2 : zfield::OP_LEN2);
}

/** The oracle's flattened src[] for a mode and a lane. */
void osrc_of(int mode, const int32_t a[3][kLanes], const int32_t b[2][kLanes], int l,
             int32_t out[4], int& n) {
  if (mode == 1) {
    out[0] = a[0][l];
    out[1] = a[1][l];
    out[2] = a[2][l];
    n = 3;
  } else if (mode == 2) {
    // DIST2's flattened list is a's two members then b's two.
    out[0] = a[0][l];
    out[1] = a[1][l];
    out[2] = b[0][l];
    out[3] = b[1][l];
    n = 4;
  } else {
    out[0] = a[0][l];
    out[1] = a[1][l];
    n = 2;
  }
}

int32_t oracle(int mode, const int32_t a[3][kLanes], const int32_t b[2][kLanes], int l,
               bool* sat_out) {
  int32_t src[4] = {};
  int n = 0;
  osrc_of(mode, a, b, l, src, n);
  (void)n;
  zref::SatLedger L{};
  int32_t dst = 0;
  zfield::steps::exec_op(op_of(mode), 0u, {}, src, &dst, &L);
  if (sat_out) *sat_out = L.rescale > 0;
  return dst;
}

const char* mode_name(int m) { return m == 1 ? "LEN3" : (m == 2 ? "DIST2" : "LEN2"); }

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_field_v3_len dut;
  MulBank mb;

  printf("== section 1: the three modes on ordinary values ==\n");
  {
    const int32_t a[3][kLanes] = {{3 << 16, 5 << 16, -(7 << 16), 0},
                                  {4 << 16, 12 << 16, 24 << 16, 0},
                                  {0, 84 << 16, -(4 << 16), 9 << 16}};
    const int32_t b[2][kLanes] = {{1 << 16, -(2 << 16), 7 << 16, 0},
                                  {1 << 16, 3 << 16, 0, -(9 << 16)}};
    for (int mode = 0; mode < 3; ++mode) {
      reset(dut, mb);
      int32_t got[kLanes] = {};
      uint8_t sat = 0, tag = 0;
      const int c = run(dut, mb, mode, a, b, (uint8_t)(0x60 + mode), got, &sat, &tag);
      char what[96];
      snprintf(what, sizeof what, "%s group completes", mode_name(mode));
      zhao::check(c >= 0, what, 1, c >= 0 ? 1 : 0);
      zhao::check(tag == (uint8_t)(0x60 + mode), "and its tag comes back", (uint8_t)(0x60 + mode),
                  tag);
      for (int l = 0; l < kLanes; ++l) {
        bool wsat = false;
        const int32_t want = oracle(mode, a, b, l, &wsat);
        snprintf(what, sizeof what, "%s lane %d", mode_name(mode), l);
        zhao::check(got[l] == want, what, (uint32_t)want, (uint32_t)got[l]);
        snprintf(what, sizeof what, "%s lane %d saturation flag", mode_name(mode), l);
        zhao::check(((sat >> l) & 1u) == (wsat ? 1u : 0u), what, wsat ? 1u : 0u,
                    (uint32_t)((sat >> l) & 1u));
      }
      printf("   MEASURED: %-5s in %d clocks\n", mode_name(mode), c);
    }
  }

  printf("== section 2: the length that does not fit, and the flag that says so ==\n");
  {
    // sqrt(2) * 2^31 is about 3.03e9 and INT32_MAX is 2.14e9, so LEN2 of two
    // extreme components genuinely clamps. `len_of` bumps the RESCALE lane for
    // that, not the add lane, and getting the wrong lane would be a plausible
    // and wrong ledger.
    reset(dut, mb);
    const int32_t a[3][kLanes] = {
        {INT32_MIN, INT32_MAX, 1 << 30, 0}, {INT32_MIN, INT32_MAX, 1 << 30, 0}, {0, 0, 0, 0}};
    const int32_t b[2][kLanes] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
    int32_t got[kLanes] = {};
    uint8_t sat = 0;
    (void)run(dut, mb, 0, a, b, 0x70, got, &sat, nullptr);
    int clamped = 0;
    for (int l = 0; l < kLanes; ++l) {
      bool wsat = false;
      const int32_t want = oracle(0, a, b, l, &wsat);
      char what[80];
      snprintf(what, sizeof what, "extreme lane %d value", l);
      zhao::check(got[l] == want, what, (uint32_t)want, (uint32_t)got[l]);
      snprintf(what, sizeof what, "extreme lane %d flag", l);
      zhao::check(((sat >> l) & 1u) == (wsat ? 1u : 0u), what, wsat ? 1u : 0u,
                  (uint32_t)((sat >> l) & 1u));
      if (wsat) ++clamped;
    }
    zhao::check(clamped > 0, "and at least one lane really did clamp", 1, clamped > 0 ? 1 : 0);
    printf("   MEASURED: %d of 4 lanes clamped\n", clamped);
  }

  printf("== section 3: DIST2's subtract SATURATES, it does not wrap ==\n");
  {
    // INT32_MAX - INT32_MIN is 2^32-1 and does not fit. A wrapped delta would
    // report two maximally distant points as almost coincident, which is the
    // worst available failure: plausible, and completely wrong.
    reset(dut, mb);
    const int32_t a[3][kLanes] = {
        {INT32_MAX, INT32_MIN, 1 << 30, 0}, {INT32_MAX, INT32_MIN, 0, 0}, {0, 0, 0, 0}};
    const int32_t b[2][kLanes] = {{INT32_MIN, INT32_MAX, -(1 << 30), 0},
                                  {INT32_MIN, INT32_MAX, 0, 0}};
    int32_t got[kLanes] = {};
    uint8_t sat = 0;
    (void)run(dut, mb, 2, a, b, 0x71, got, &sat, nullptr);
    for (int l = 0; l < kLanes; ++l) {
      bool wsat = false;
      const int32_t want = oracle(2, a, b, l, &wsat);
      char what[80];
      snprintf(what, sizeof what, "far-apart lane %d matches the oracle", l);
      zhao::check(got[l] == want, what, (uint32_t)want, (uint32_t)got[l]);
    }
  }

  printf("== section 4: many random groups, all three modes ==\n");
  {
    reset(dut, mb);
    Prng rng(0xBEEF);
    int wrong = 0, groups = 0;
    for (int g = 0; g < 30; ++g) {
      const int mode = g % 3;
      int32_t a[3][kLanes], b[2][kLanes];
      for (int c = 0; c < 3; ++c)
        for (int l = 0; l < kLanes; ++l) a[c][l] = rng.interesting();
      for (int c = 0; c < 2; ++c)
        for (int l = 0; l < kLanes; ++l) b[c][l] = rng.interesting();
      int32_t got[kLanes] = {};
      uint8_t sat = 0;
      if (run(dut, mb, mode, a, b, (uint8_t)g, got, &sat, nullptr) < 0) {
        ++wrong;
        continue;
      }
      ++groups;
      for (int l = 0; l < kLanes; ++l) {
        bool wsat = false;
        const int32_t want = oracle(mode, a, b, l, &wsat);
        if (got[l] != want) ++wrong;
        if (((sat >> l) & 1u) != (wsat ? 1u : 0u)) ++wrong;
      }
    }
    zhao::check(wrong == 0, "30 random groups across the three modes match", 0, wrong);
    printf("   MEASURED: %d groups\n", groups);
  }

  printf("== section 5: the bank refuses, and the answers do not move ==\n");
  {
    const int32_t a[3][kLanes] = {{3 << 16, 5 << 16, -(7 << 16), 11 << 16},
                                  {4 << 16, 12 << 16, 24 << 16, 2 << 16},
                                  {1 << 16, 84 << 16, -(4 << 16), 9 << 16}};
    const int32_t b[2][kLanes] = {{1 << 16, -(2 << 16), 7 << 16, 0},
                                  {1 << 16, 3 << 16, 0, -(9 << 16)}};
    reset(dut, mb);
    int32_t clean[kLanes] = {};
    (void)run(dut, mb, 1, a, b, 0x80, clean, nullptr, nullptr);

    reset(dut, mb);
    mb.flaky = true;
    int wrong = 0, done = 0;
    for (int g = 0; g < 4; ++g) {
      int32_t got[kLanes] = {};
      if (run(dut, mb, 1, a, b, (uint8_t)(0x81 + g), got, nullptr, nullptr) < 0) {
        ++wrong;
        continue;
      }
      ++done;
      for (int l = 0; l < kLanes; ++l)
        if (got[l] != clean[l]) ++wrong;
    }
    mb.flaky = false;
    zhao::check(done == 4, "every group finishes under a refusing bank", 4, done);
    zhao::check(wrong == 0, "and gives the SAME answers", 0, wrong);
    printf("   MEASURED: %d groups, %d refusals issued\n", done, mb.refusals);
  }

  printf("== section 6: the INITIATION INTERVAL, STREAMED ==\n");
  {
    // THE FIRST VERSION OF THIS SECTION MEASURED THE WRONG THING. It called
    // run(), which offers a group and then WAITS for its reply before offering
    // the next -- so it never had two groups in flight and reported latency
    // while calling it an initiation interval. It read 146, then 42, then 43,
    // and 43 was the tell: the two-bank rebuild could not possibly have left
    // throughput unchanged.
    //
    // Measuring a projection of the thing instead of the thing is the failure
    // this project's art law names, and it applies to a testbench exactly as it
    // does to a creature's proportions.
    //
    // So this streams: offer whenever the service is ready, accept whenever it
    // replies, and divide elapsed clocks by groups RETIRED.
    reset(dut, mb);

    // EVERY GROUP CARRIES ITS OWN NUMBERS, AND THAT IS THE WHOLE POINT.
    //
    // This section used to set one pair of operands outside the loop and stream
    // thirty-two IDENTICAL groups past them. It measured the initiation
    // interval correctly, and it could not have seen a cross-group mix-up at
    // all: if the service had handed group 7's answer to group 3, every value
    // still matched, because every value was the same value. The order check
    // on `tag_o` did not cover it either -- the tag comes from the order queue
    // and the data from the banks, so the tags can be in perfect order while
    // the numbers underneath them are swapped.
    //
    // The composed Earth gate found wrong answers that appear only when two
    // groups are in flight at once. Two groups in flight is exactly what this
    // section streams, and identical data is exactly what hid it.
    constexpr int kGroups = 32;
    int32_t ga[kGroups][3][kLanes];
    int32_t gb[kGroups][2][kLanes];
    int32_t want[kGroups][kLanes];
    for (int g = 0; g < kGroups; ++g) {
      for (int l = 0; l < kLanes; ++l) {
        ga[g][0][l] = (int32_t)((g * 7 + l * 3 + 1) << 16);
        ga[g][1][l] = (int32_t)(((g * 5 + l * 11 + 2) << 16)) * ((l & 1) ? -1 : 1);
        ga[g][2][l] = 0;
        gb[g][0][l] = (int32_t)(((g * 3 + l * 2) << 16)) * ((g & 1) ? -1 : 1);
        gb[g][1][l] = (int32_t)((g + l * 4) << 16);
      }
      for (int l = 0; l < kLanes; ++l) want[g][l] = oracle(2, ga[g], gb[g], l, nullptr);
    }

    int offered = 0, retired = 0, wrong = 0, clocks = 0;
    dut.r_ready_i = 1;
    dut.mode_i = 2;
    int guard = 0;
    while (retired < kGroups && guard++ < 20000) {
      const int g = (offered < kGroups) ? offered : (kGroups - 1);
      dut.v_valid_i = (offered < kGroups) ? 1 : 0;
      dut.tag_i = (uint8_t)(offered & 0xFF);
      dut.a0_0_i = (uint32_t)ga[g][0][0];
      dut.a0_1_i = (uint32_t)ga[g][0][1];
      dut.a0_2_i = (uint32_t)ga[g][0][2];
      dut.a0_3_i = (uint32_t)ga[g][0][3];
      dut.a1_0_i = (uint32_t)ga[g][1][0];
      dut.a1_1_i = (uint32_t)ga[g][1][1];
      dut.a1_2_i = (uint32_t)ga[g][1][2];
      dut.a1_3_i = (uint32_t)ga[g][1][3];
      dut.a2_0_i = (uint32_t)ga[g][2][0];
      dut.a2_1_i = (uint32_t)ga[g][2][1];
      dut.a2_2_i = (uint32_t)ga[g][2][2];
      dut.a2_3_i = (uint32_t)ga[g][2][3];
      dut.b0_0_i = (uint32_t)gb[g][0][0];
      dut.b0_1_i = (uint32_t)gb[g][0][1];
      dut.b0_2_i = (uint32_t)gb[g][0][2];
      dut.b0_3_i = (uint32_t)gb[g][0][3];
      dut.b1_0_i = (uint32_t)gb[g][1][0];
      dut.b1_1_i = (uint32_t)gb[g][1][1];
      dut.b1_2_i = (uint32_t)gb[g][1][2];
      dut.b1_3_i = (uint32_t)gb[g][1][3];
      dut.eval();
      const bool took = dut.v_valid_i && dut.v_ready_o;
      const bool gave = dut.r_valid_o && dut.r_ready_i;
      if (gave) {
        for (int l = 0; l < kLanes; ++l) {
          const int32_t got = (int32_t)(l == 0   ? dut.o0_0_o
                                        : l == 1 ? dut.o0_1_o
                                        : l == 2 ? dut.o0_2_o
                                                 : dut.o0_3_o);
          if (got != want[retired][l]) ++wrong;
        }
        if ((int)dut.tag_o != (retired & 0xFF)) ++wrong;  // ACCEPT ORDER
        ++retired;
      }
      if (took) ++offered;
      step(dut, mb);
      ++clocks;
    }
    dut.v_valid_i = 0;
    dut.eval();

    zhao::check(retired == kGroups, "all 32 streamed groups retire", kGroups, retired);
    zhao::check(wrong == 0, "every streamed group gets ITS OWN right answer, in order", 0, wrong);
    const int ii = retired ? (clocks / retired) : 0;
    printf("   MEASURED: %d groups streamed in %d clocks, II = %d clocks/group\n", retired, clocks,
           ii);
    printf("   Earth budget is 24 clocks/group for the WHOLE program.\n");
    zhao::check(ii > 0, "an initiation interval was measured", 1, ii > 0 ? 1 : 0);
  }

  return zhao::report_and_exit("field_v3_len_directed");
}
