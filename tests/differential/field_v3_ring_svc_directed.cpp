// field_v3_ring_svc_directed.cpp — the prepared ring joined to its uniforms.
//
// `zhao_field_v3_ring` is closed at 23/23, so the ARITHMETIC is not what is
// under test here. What is unproven is the join: four uniform scalars fetched
// out of a bank by index, over a registered read port, and handed to a unit
// that expects them all present at once.
//
// The oracle is `zfield::steps::ring_prepared`, which is the same function the
// planner's own evaluator calls for UOP_RING_PREP. Its operand order — d, r0,
// m, rA, rB — is also what fixes the immediate's field order, so a packing
// mistake shows up as a wrong answer rather than as a disagreement about
// documentation.
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_v3_ring_svc.h"

#include "zfield/zfield_steps.hpp"
#include "zref/zref_fixp.hpp"
#include "zhao_sim.hpp"

namespace {

constexpr int kLanes = 4;
constexpr int kSlots = 64;

// ---- the four-wide multiplier bank, modelled as the engine drives it ------
struct MulBank {
  // TWO DEEP AND PIPELINED, because zhao_field_v3_mulbank is: "each is two
  // clocks deep and FULLY PIPELINED". A one-at-a-time model measured this
  // service at II 27 and that was the MODEL's limit, not the hardware's --
  // a number describing the scaffolding rather than the thing.
  bool st_v[2] = {false, false};
  int64_t st_p[2][kLanes] = {};
  bool busy = false;
  int cnt = 0;
  int64_t p[kLanes] = {0, 0, 0, 0};
  bool grant = true;
  int refusals = 0;
  // A bank that refuses FOREVER is not a test, it is a deadlock: nothing can
  // finish, so "the answers did not move" is vacuously true of no answers.
  // Refusing on a schedule keeps progress possible while still landing the
  // refusal in the middle of a group, which is where it has to land to prove
  // anything.
  bool flaky = false;
  uint64_t rng = 0x9E3779B97F4A7C15ull;
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

// ---- the uniform bank, modelled with its REGISTERED read ------------------
//
// The lag is the point. `sb_rdata_i` carries the datum for the address the DUT
// presented on the PREVIOUS clock, exactly as zhao_field_v3_sbank does. A
// service that assumes the datum is available in the address's own cycle
// captures the wrong slot, and it captures a plausible one -- the neighbouring
// uniform -- which is why this is modelled rather than short-circuited.
struct SBank {
  int32_t mem[kSlots] = {};
  uint32_t pending = 0;  // the address presented last cycle
};

void step(Vzhao_field_v3_ring_svc& dut, MulBank& mb, SBank& sb) {
  dut.sb_rdata_i = (uint32_t)sb.mem[sb.pending % kSlots];

  if (mb.flaky) mb.grant = (mb.next() % 4u) != 0u;  // refuse about one clock in four
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

  // Latch the address the DUT is presenting NOW; it is answered next clock.
  sb.pending = (uint32_t)dut.sb_raddr_o;

  // Advance the two stages, then accept this clock's request into stage 0.
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

/** The values the planner's PREP block computes once per association. */
struct Prep {
  int32_t r0, r1, m, rA, rB;
};

Prep prepare(int32_t r0, int32_t r1) {
  Prep p{};
  p.r0 = r0;
  p.r1 = r1;
  zref::SatLedger L;
  p.m = zfield::steps::ring_mid(r0, r1, &L);
  p.rA = zref::field_rcp(zref::fx16{(int32_t)(p.m - p.r0)}, &L).raw;
  p.rB = zref::field_rcp(zref::fx16{(int32_t)(r1 - p.m)}, &L).raw;
  return p;
}

uint32_t pack_slots(int s_r0, int s_m, int s_rA, int s_rB) {
  return (uint32_t)((s_r0 & 63) | ((s_m & 63) << 6) | ((s_rA & 63) << 12) | ((s_rB & 63) << 18));
}

void reset(Vzhao_field_v3_ring_svc& dut, MulBank& mb, SBank& sb) {
  dut.rst_n = 0;
  dut.req_valid_i = 0;
  dut.rsp_ready_i = 0;
  dut.mul_valid_i = 0;
  dut.mul_ready_i = 1;
  dut.sb_rdata_i = 0;
  mb = MulBank{};
  sb.pending = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);
}

/** Drive one group and collect its four answers. Returns clocks taken, or -1. */
int run_group(Vzhao_field_v3_ring_svc& dut, MulBank& mb, SBank& sb, const int32_t* d, uint32_t imm,
              uint8_t tag, int32_t* out, uint8_t* out_tag) {
  dut.req_valid_i = 1;
  dut.req_d_0_i = (uint32_t)d[0];
  dut.req_d_1_i = (uint32_t)d[1];
  dut.req_d_2_i = (uint32_t)d[2];
  dut.req_d_3_i = (uint32_t)d[3];
  dut.req_imm_i = imm;
  dut.req_tag_i = tag;
  dut.rsp_ready_i = 1;
  dut.eval();

  int guard = 0;
  while (!dut.req_ready_o && guard++ < 200) step(dut, mb, sb);
  if (!dut.req_ready_o) return -1;
  step(dut, mb, sb);  // the accept
  dut.req_valid_i = 0;
  dut.eval();

  guard = 0;
  while (!dut.rsp_valid_o && guard++ < 2000) step(dut, mb, sb);
  if (!dut.rsp_valid_o) return -1;

  out[0] = (int32_t)dut.rsp_r_0_o;
  out[1] = (int32_t)dut.rsp_r_1_o;
  out[2] = (int32_t)dut.rsp_r_2_o;
  out[3] = (int32_t)dut.rsp_r_3_o;
  if (out_tag) *out_tag = (uint8_t)dut.rsp_tag_o;
  step(dut, mb, sb);  // the reply is taken
  dut.rsp_ready_i = 0;
  dut.eval();
  return guard;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_field_v3_ring_svc dut;
  MulBank mb;
  SBank sb;

  const int32_t d[kLanes] = {(3 << 16), (7 << 16), -(2 << 16), (11 << 16)};

  printf("== section 1: the uniforms are fetched by index, non-consecutively ==\n");
  {
    reset(dut, mb, sb);
    const Prep p = prepare(2 << 16, 9 << 16);

    // DELIBERATELY SCATTERED, and in a different order than the fetch reads
    // them. Consecutive slots would pass even if the service ignored three of
    // the four indices and simply counted upward from the first.
    const int s_r0 = 41, s_m = 7, s_rA = 63, s_rB = 22;
    sb.mem[s_r0] = p.r0;
    sb.mem[s_m] = p.m;
    sb.mem[s_rA] = p.rA;
    sb.mem[s_rB] = p.rB;

    int32_t got[kLanes] = {};
    uint8_t tag = 0;
    const int clocks =
        run_group(dut, mb, sb, d, pack_slots(s_r0, s_m, s_rA, s_rB), 0x5A, got, &tag);
    zhao::check(clocks >= 0, "the group completes", 1, clocks >= 0 ? 1 : 0);
    zhao::check(tag == 0x5A, "and its tag comes back", 0x5A, tag);
    zhao::check(dut.imm_bad_o == 0, "a clean immediate raises nothing", 0, (uint32_t)dut.imm_bad_o);

    for (int l = 0; l < kLanes; ++l) {
      zref::SatLedger L;
      const int32_t want = zfield::steps::ring_prepared(d[l], p.r0, p.m, p.rA, p.rB, &L);
      char what[80];
      snprintf(what, sizeof what, "lane %d matches ring_prepared", l);
      zhao::check(got[l] == want, what, (uint32_t)want, (uint32_t)got[l]);
    }
    printf("   MEASURED: group in %d clocks, slots %d/%d/%d/%d\n", clocks, s_r0, s_m, s_rA, s_rB);
  }

  printf("== section 2: a second group REFETCHES, it does not reuse ==\n");
  {
    // The defect this catches is a service that fetches once and keeps the
    // uniforms. It would pass every single-group test ever written, and it
    // would be wrong the moment a program has two rings with different radii.
    reset(dut, mb, sb);
    const Prep p1 = prepare(2 << 16, 9 << 16);
    const Prep p2 = prepare(1 << 16, 40 << 16);  // deliberately unlike p1

    sb.mem[10] = p1.r0;
    sb.mem[11] = p1.m;
    sb.mem[12] = p1.rA;
    sb.mem[13] = p1.rB;
    sb.mem[30] = p2.r0;
    sb.mem[31] = p2.m;
    sb.mem[32] = p2.rA;
    sb.mem[33] = p2.rB;

    int32_t g1[kLanes] = {}, g2[kLanes] = {};
    (void)run_group(dut, mb, sb, d, pack_slots(10, 11, 12, 13), 0x11, g1, nullptr);
    (void)run_group(dut, mb, sb, d, pack_slots(30, 31, 32, 33), 0x22, g2, nullptr);

    int wrong = 0, same = 0;
    for (int l = 0; l < kLanes; ++l) {
      zref::SatLedger L1, L2;
      const int32_t w1 = zfield::steps::ring_prepared(d[l], p1.r0, p1.m, p1.rA, p1.rB, &L1);
      const int32_t w2 = zfield::steps::ring_prepared(d[l], p2.r0, p2.m, p2.rA, p2.rB, &L2);
      if (g1[l] != w1 || g2[l] != w2) ++wrong;
      if (g1[l] == g2[l]) ++same;
    }
    zhao::check(wrong == 0, "both groups match their OWN prepared radii", 0, wrong);
    zhao::check(same < kLanes, "and the two groups are not identical answers", 1,
                same < kLanes ? 1 : 0);
  }

  printf("== section 3: the reserved immediate bits are a FAULT, not padding ==\n");
  {
    reset(dut, mb, sb);
    const Prep p = prepare(2 << 16, 9 << 16);
    sb.mem[1] = p.r0;
    sb.mem[2] = p.m;
    sb.mem[3] = p.rA;
    sb.mem[4] = p.rB;

    int32_t got[kLanes] = {};
    // BIT 24 IS SMOOTH MODE NOW, so the reserved space starts at 25. Picking a
    // bit that still means nothing is the point of the check; using 24 would
    // now be asserting that a LEGAL request is illegal.
    const uint32_t imm = pack_slots(1, 2, 3, 4) | 0x02000000u;  // one reserved bit
    (void)run_group(dut, mb, sb, d, imm, 0x33, got, nullptr);
    zhao::check(dut.imm_bad_o == 1, "a set reserved bit raises", 1, (uint32_t)dut.imm_bad_o);

    // It still answers. The fault is a REPORT, not a refusal to compute -- the
    // instruction's slots were legible, and silently producing nothing would
    // be a worse failure than producing an answer with a raised flag.
    for (int l = 0; l < kLanes; ++l) {
      zref::SatLedger L;
      const int32_t want = zfield::steps::ring_prepared(d[l], p.r0, p.m, p.rA, p.rB, &L);
      char what[80];
      snprintf(what, sizeof what, "lane %d still answers correctly", l);
      zhao::check(got[l] == want, what, (uint32_t)want, (uint32_t)got[l]);
    }
  }

  printf("== section 3b: SMOOTH MODE is the first smoothstep, exactly ==\n");
  {
    // The whole reason this mode exists: every Earth builder expands
    // `smoothstep(e0, e1, x)` into seven varying uops, and those seven are
    // products P1..P4 of this unit. Contracting them into one request is only
    // worth anything if the answer is IDENTICAL, so it is compared against the
    // reference's own `ring_prepared` with the second branch killed -- which is
    // what the seven uops compute, verified separately across 39,321 edge and
    // span combinations before any of this was built.
    reset(dut, mb, sb);
    const Prep p = prepare(2 << 16, 9 << 16);
    sb.mem[5] = p.r0;
    sb.mem[6] = p.r0;  // m = r0: the dead subtraction is the live one
    sb.mem[7] = p.rA;
    sb.mem[8] = 0;  // rB = 0 kills the second smoothstep

    int32_t got[kLanes] = {};
    const uint32_t imm = pack_slots(5, 6, 7, 8) | 0x01000000u;  // bit 24: smooth
    const int clocks = run_group(dut, mb, sb, d, imm, 0x5A, got, nullptr);
    zhao::check(dut.imm_bad_o == 0, "smooth mode is not a reserved-bit fault", 0,
                (uint32_t)dut.imm_bad_o);
    for (int l = 0; l < kLanes; ++l) {
      zref::SatLedger L;
      const int32_t want = zfield::steps::ring_prepared(d[l], p.r0, p.r0, p.rA, 0, &L);
      char what[80];
      snprintf(what, sizeof what, "smooth lane %d equals the seven uops it replaces", l);
      zhao::check(got[l] == want, what, (uint32_t)want, (uint32_t)got[l]);
    }
    printf("   MEASURED: smooth group in %d clocks (three products, not seven)\n", clocks);
  }

  printf("== section 4: the bank can REFUSE, and the answers do not move ==\n");
  {
    // The multiplier bank is shared and says no. An instruction may not be
    // stalled between issue and product arrival; if this service gets that
    // wrong the answers change under refusal, which is exactly the defect the
    // curve service had before mul_ready_i existed.
    reset(dut, mb, sb);
    const Prep p = prepare(2 << 16, 9 << 16);
    sb.mem[5] = p.r0;
    sb.mem[6] = p.m;
    sb.mem[7] = p.rA;
    sb.mem[8] = p.rB;
    const uint32_t imm = pack_slots(5, 6, 7, 8);

    int32_t clean[kLanes] = {};
    (void)run_group(dut, mb, sb, d, imm, 0x44, clean, nullptr);

    reset(dut, mb, sb);
    sb.mem[5] = p.r0;
    sb.mem[6] = p.m;
    sb.mem[7] = p.rA;
    sb.mem[8] = p.rB;
    int done = 0, wrong = 0;
    mb.flaky = true;
    for (int g = 0; g < 6; ++g) {
      int32_t got[kLanes] = {};
      const int c = run_group(dut, mb, sb, d, imm, (uint8_t)(0x50 + g), got, nullptr);
      if (c < 0) {
        ++wrong;
        continue;
      }
      ++done;
      for (int l = 0; l < kLanes; ++l)
        if (got[l] != clean[l]) ++wrong;
    }
    mb.flaky = false;
    mb.grant = true;
    zhao::check(done == 6, "every group still finishes under a refusing bank", 6, done);
    zhao::check(wrong == 0, "every group under refusal gives the SAME answers", 0, wrong);
    printf("   MEASURED: %d groups completed, %d refusals issued\n", done, mb.refusals);
  }

  printf("== section 5: the INITIATION INTERVAL, STREAMED ==\n");
  {
    // Offer whenever ready, accept whenever it replies, divide by groups
    // RETIRED. Waiting for each reply before offering the next measures
    // latency and calls it throughput.
    reset(dut, mb, sb);
    const Prep p = prepare(2 << 16, 9 << 16);
    sb.mem[5] = p.r0;
    sb.mem[6] = p.m;
    sb.mem[7] = p.rA;
    sb.mem[8] = p.rB;
    const uint32_t imm = pack_slots(5, 6, 7, 8);
    // EVERY GROUP CARRIES ITS OWN DISTANCES.
    //
    // Streaming one distance set past every group compares them all to one
    // answer, so a service that handed group 7's ring to group 3 would pass:
    // every value was the same value. Two ring units share the bank here, which
    // is precisely the machinery a cross-group swap lives in. The tag check
    // does not cover it -- the tag rides the order queue and the data rides the
    // units, so tags can be in perfect order over swapped numbers.
    constexpr int kGroups = 24;
    int32_t gd[kGroups][kLanes];
    int32_t want[kGroups][kLanes];
    for (int g = 0; g < kGroups; ++g)
      for (int l = 0; l < kLanes; ++l) {
        gd[g][l] = (int32_t)((g * 3 + l + 1) << 15);
        zref::SatLedger L;
        want[g][l] = zfield::steps::ring_prepared(gd[g][l], p.r0, p.m, p.rA, p.rB, &L);
      }

    int offered = 0, retired = 0, wrong = 0, clocks = 0, guard = 0;
    dut.rsp_ready_i = 1;
    dut.req_imm_i = imm;
    while (retired < kGroups && guard++ < 20000) {
      const int g = (offered < kGroups) ? offered : (kGroups - 1);
      dut.req_valid_i = (offered < kGroups) ? 1 : 0;
      dut.req_tag_i = (uint8_t)(offered & 0xFF);
      dut.req_d_0_i = (uint32_t)gd[g][0];
      dut.req_d_1_i = (uint32_t)gd[g][1];
      dut.req_d_2_i = (uint32_t)gd[g][2];
      dut.req_d_3_i = (uint32_t)gd[g][3];
      dut.eval();
      const bool took = dut.req_valid_i && dut.req_ready_o;
      const bool gave = dut.rsp_valid_o && dut.rsp_ready_i;
      if (gave) {
        const int32_t got[kLanes] = {(int32_t)dut.rsp_r_0_o, (int32_t)dut.rsp_r_1_o,
                                     (int32_t)dut.rsp_r_2_o, (int32_t)dut.rsp_r_3_o};
        for (int l = 0; l < kLanes; ++l)
          if (got[l] != want[retired][l]) ++wrong;
        if ((int)dut.rsp_tag_o != (retired & 0xFF)) ++wrong;
        ++retired;
      }
      if (took) ++offered;
      step(dut, mb, sb);
      ++clocks;
    }
    dut.req_valid_i = 0;
    dut.eval();
    zhao::check(retired == kGroups, "all 24 streamed groups retire", kGroups, retired);
    zhao::check(wrong == 0, "every streamed answer is right and IN ORDER", 0, wrong);
    const int ii = retired ? (clocks / retired) : 0;
    printf("   MEASURED: %d groups streamed in %d clocks, II = %d clocks/group\n", retired, clocks,
           ii);
  }

  return zhao::report_and_exit("field_v3_ring_svc_directed");
}
