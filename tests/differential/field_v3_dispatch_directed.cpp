// field_v3_dispatch_directed.cpp — the long-op dispatcher: gather four
// contexts into a four-point request, hold what the reply needs, drain the
// results one register per clock, and release each context after its last
// write.
//
// WHAT THE ORACLE IS HERE, AND WHAT IT IS NOT
// -------------------------------------------
// Every other Field differential runs an FPLAN through the RTL and through
// zfield::execute_point, because the reference defines the ANSWER. This block
// computes no answer. It is plumbing: grouping, tagging, routing and release.
// zfield has no notion of a group, so there is nothing in the reference to
// differ against, and pretending otherwise would be worse than saying so.
//
// The oracle is therefore the CONTRACT, checked as properties:
//
//   1. WHAT GOES IN COMES OUT, IN THE RIGHT PLACE. Each accepted context's
//      operands appear in its own request lane, and that lane's results are
//      written to that context's registers -- never another's.
//   2. A PARTIAL GROUP IS ISSUED, NOT WAITED FOR. One, two or three contexts
//      plus `flush_i` must produce a request, because waiting for a fourth
//      context that has already finished its program is a deadlock.
//   3. PADDED LANES ARE VISIBLY NOT DATA, AND NEVER WRITTEN. The pad is 3/5/7,
//      the same constants zhao_probe_v3_engine ties its unused bank lanes to.
//      A padded lane must produce NO writeback at all.
//   4. A CONTEXT IS RELEASED AFTER ITS LAST REGISTER, not before. Releasing
//      early would let it re-issue and read a register the drain has not
//      reached.
//   5. THE DRAIN IS SERIAL AND LOSSLESS UNDER BACKPRESSURE. The register file
//      has one write port; holding wb_ready_i low must stall, not drop.
//   6. AN OFFER ARRIVING WITH `flush_i` IS REFUSED, NOT SWALLOWED. This is a
//      real hole that existed for one lint cycle: accepted on the flush clock,
//      the context would land outside the snapshot and then be cleared -- a
//      LOST instruction. Section 6 asserts the offer is still standing
//      afterwards and joins the next group.
//   7. AN UNKNOWN OPCODE IS REFUSED. An unknown destination width would write
//      the wrong NUMBER of registers, which is corruption rather than an
//      error.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_field_v3_dispatch.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

constexpr int kLanes = 4;

// The op table's dst_width, mirrored here so the test and the RTL agree on
// what each opcode costs. These are the generated table's values, not new
// ones: reference/include/zfield/generated/zfield_optable.hpp.
constexpr uint8_t OP_CURVE = 0x1A;
constexpr uint8_t OP_NOISE2 = 0x1C;
constexpr uint8_t OP_DCURVE = 0x1D;
constexpr uint8_t OP_RIDGE = 0x22;
constexpr uint8_t OP_ROT3 = 0x29;
constexpr uint8_t OP_ADD = 0x03;  // a SHORT op: this block must refuse it

int width_of(uint8_t op) {
  switch (op) {
    case OP_CURVE:
    case OP_DCURVE:
    case OP_RIDGE:
      return 1;
    case OP_NOISE2:
      return 2;
    case OP_ROT3:
      return 3;
    default:
      return 0;
  }
}

constexpr int32_t kPadA = 3, kPadB = 5, kPadC = 7;

struct Point {
  int ctx;
  int32_t s0, s1, s2;
};

/** One write the RTL asked for, recorded in the order it asked. */
struct Wrote {
  int ctx, reg;
  int32_t data;
};

struct Dut {
  Vzhao_field_v3_dispatch& t;
  std::vector<Wrote> writes;
  std::vector<int> released;
  // HOW MANY OF ITS OWN REGISTERS A CONTEXT HAD WRITTEN WHEN IT WAS RELEASED.
  //
  // Mutant D21 -- releasing on member 0 instead of the last member -- SURVIVED
  // the first sweep, because for a width-1 op those are the same clock and for
  // wider ops the release COUNT and ORDER are unchanged. Only the timing
  // moves, and nothing was looking at timing. This is what looks at it.
  std::vector<std::pair<int, int>> rel_after;
  int wrote_for[16] = {0};

  explicit Dut(Vzhao_field_v3_dispatch& top) : t(top) {}

  void reset() {
    t.rst_n = 0;
    t.long_valid_i = 0;
    t.long_imm_i = 0;
    t.flush_i = 0;
    t.svc_ready_i = 0;
    t.rsp_valid_i = 0;
    t.wb_ready_i = 0;
    t.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
    zhao::tick(t);
    writes.clear();
    released.clear();
    rel_after.clear();
    for (int i = 0; i < 16; ++i) wrote_for[i] = 0;
  }

  /** One cycle, recording anything the DUT published on the way past. */
  void step() {
    t.eval();
    if (t.wb_valid_o && t.wb_ready_i) {
      writes.push_back({(int)t.wb_ctx_o, (int)t.wb_reg_o, (int32_t)t.wb_data_o});
      ++wrote_for[(int)t.wb_ctx_o & 15];
    }
    // The release is combinational with the accepted write, so on the last
    // write's clock BOTH fire. Counting the write first is what makes
    // `rel_after` read "this many registers had landed, including this one".
    if (t.rel_valid_o) {
      released.push_back((int)t.rel_ctx_o);
      rel_after.push_back({(int)t.rel_ctx_o, wrote_for[(int)t.rel_ctx_o & 15]});
    }
    zhao::tick(t);
  }

  /** Offer one context and wait for it to be taken. Returns false on timeout. */
  bool offer(const Point& p, uint8_t op, int dst, int guard_max = 64, uint32_t imm = 0u) {
    t.long_valid_i = 1;
    t.long_ctx_i = (uint8_t)p.ctx;
    t.long_op_i = op;
    t.long_dst_i = (uint8_t)dst;
    t.long_imm_i = imm;
    t.long_s0_i = (uint32_t)p.s0;
    t.long_s1_i = (uint32_t)p.s1;
    t.long_s2_i = (uint32_t)p.s2;
    t.long_s3_i = 0;
    int guard = 0;
    t.eval();
    while (!t.long_ready_o && guard++ < guard_max) step();
    if (!t.long_ready_o) {
      t.long_valid_i = 0;
      t.eval();
      return false;
    }
    step();  // the accept
    t.long_valid_i = 0;
    t.eval();
    return true;
  }

  /** Run until a request is on the wire, then take it. */
  bool take_request(int guard_max = 64) {
    int guard = 0;
    t.svc_ready_i = 1;
    t.eval();
    while (!t.svc_valid_o && guard++ < guard_max) step();
    if (!t.svc_valid_o) {
      t.svc_ready_i = 0;
      t.eval();
      return false;
    }
    return true;  // caller reads the ports, then calls accept_request()
  }

  void accept_request() {
    step();
    t.svc_ready_i = 0;
    t.eval();
  }

  /** Hand back four lanes of results with the given tag. */
  void reply(uint8_t tag, const int32_t r0[kLanes], const int32_t r1[kLanes],
             const int32_t r2[kLanes]) {
    t.rsp_valid_i = 1;
    t.rsp_tag_i = tag;
    for (int l = 0; l < kLanes; ++l) {
      t.rsp_r0_i[l] = (uint32_t)r0[l];
      t.rsp_r1_i[l] = (uint32_t)r1[l];
      t.rsp_r2_i[l] = (uint32_t)r2[l];
    }
    int guard = 0;
    t.eval();
    while (!t.rsp_ready_o && guard++ < 64) step();
    step();
    t.rsp_valid_i = 0;
    t.eval();
  }

  /** Let the drain run to completion. */
  void drain(int guard_max = 64) {
    t.wb_ready_i = 1;
    t.eval();
    int guard = 0;
    while (t.wb_valid_o && guard++ < guard_max) step();
    // a couple of settling clocks so the last release is recorded
    step();
    t.wb_ready_i = 0;
    t.eval();
  }
};

/** The whole round trip for one group, checked end to end. */
void run_group(Vzhao_field_v3_dispatch& top, const std::vector<Point>& pts, uint8_t op, int dst,
               const std::string& what, uint32_t imm = 0xDECAFu) {
  Dut d(top);
  d.reset();
  const int n = (int)pts.size();
  const int w = width_of(op);

  for (int i = 0; i < n; ++i) {
    const bool ok = d.offer(pts[(size_t)i], op, dst, 64, imm);
    check(ok, (what + ": context " + std::to_string(i) + " accepted").c_str(), 1, ok ? 1 : 0);
    if (!ok) return;
  }
  if (n < kLanes) {
    d.t.flush_i = 1;
    d.t.eval();
  }

  const bool got = d.take_request();
  check(got, (what + ": a request was issued").c_str(), 1, got ? 1 : 0);
  if (!got) return;

  // Law 1 and law 3: real lanes carry their own context's operands; padded
  // lanes carry the recognisable constants and nothing else.
  for (int l = 0; l < kLanes; ++l) {
    const int32_t s0 = (int32_t)d.t.svc_s0_o[l];
    const int32_t s1 = (int32_t)d.t.svc_s1_o[l];
    const int32_t s2 = (int32_t)d.t.svc_s2_o[l];
    if (l < n) {
      check(s0 == pts[(size_t)l].s0, (what + ": lane " + std::to_string(l) + " s0").c_str(),
            (uint32_t)pts[(size_t)l].s0, (uint32_t)s0);
      check(s1 == pts[(size_t)l].s1, (what + ": lane " + std::to_string(l) + " s1").c_str(),
            (uint32_t)pts[(size_t)l].s1, (uint32_t)s1);
      check(s2 == pts[(size_t)l].s2, (what + ": lane " + std::to_string(l) + " s2").c_str(),
            (uint32_t)pts[(size_t)l].s2, (uint32_t)s2);
    } else {
      check(s0 == kPadA, (what + ": lane " + std::to_string(l) + " is PADDED, not zero").c_str(),
            (uint32_t)kPadA, (uint32_t)s0);
      check(s1 == kPadB, (what + ": pad b lane " + std::to_string(l)).c_str(), (uint32_t)kPadB,
            (uint32_t)s1);
      check(s2 == kPadC, (what + ": pad c lane " + std::to_string(l)).c_str(), (uint32_t)kPadC,
            (uint32_t)s2);
    }
  }
  check(d.t.svc_op_o == op, (what + ": the request carries the op").c_str(), op, d.t.svc_op_o);
  // THE IMMEDIATE IS AN OPERAND, not decoration: NOISE2 and RIDGE take their
  // seed from it, CURVE and SPLINE a table index, ROT3 an axis. A request that
  // loses it describes a different computation.
  check(d.t.svc_imm_o == imm, (what + ": and the instruction's immediate").c_str(), imm,
        d.t.svc_imm_o);
  const uint8_t tag = (uint8_t)d.t.svc_tag_o;
  d.accept_request();
  d.t.flush_i = 0;
  d.t.eval();

  // Results that are trivially attributable: lane L member M gets a value
  // whose top byte is L and whose low byte is M, so a crossed lane or a
  // swapped member is visible in the value itself rather than only in a count.
  int32_t r0[kLanes], r1[kLanes], r2[kLanes];
  for (int l = 0; l < kLanes; ++l) {
    r0[l] = (int32_t)(0xA0000000 | (l << 8) | 0);
    r1[l] = (int32_t)(0xA0000000 | (l << 8) | 1);
    r2[l] = (int32_t)(0xA0000000 | (l << 8) | 2);
  }
  d.reply(tag, r0, r1, r2);
  d.drain();

  // Law 1 and law 3 again, at the far end: exactly the real lanes' registers,
  // in (lane, member) order, and nothing for a padded lane.
  const size_t want_writes = (size_t)(n * w);
  check(d.writes.size() == want_writes, (what + ": write count").c_str(), (uint32_t)want_writes,
        (uint32_t)d.writes.size());
  if (d.writes.size() == want_writes) {
    size_t k = 0;
    for (int l = 0; l < n; ++l) {
      for (int m = 0; m < w; ++m, ++k) {
        const int32_t want = (int32_t)(0xA0000000 | (l << 8) | m);
        check(d.writes[k].ctx == pts[(size_t)l].ctx,
              (what + ": write " + std::to_string(k) + " context").c_str(),
              (uint32_t)pts[(size_t)l].ctx, (uint32_t)d.writes[k].ctx);
        check(d.writes[k].reg == dst + m, (what + ": write " + std::to_string(k) + " reg").c_str(),
              (uint32_t)(dst + m), (uint32_t)d.writes[k].reg);
        check(d.writes[k].data == want, (what + ": write " + std::to_string(k) + " data").c_str(),
              (uint32_t)want, (uint32_t)d.writes[k].data);
      }
    }
  }

  // Law 4: one release per real context, after its last register.
  check(d.released.size() == (size_t)n, (what + ": one release per context").c_str(), (uint32_t)n,
        (uint32_t)d.released.size());
  for (size_t i = 0; i < d.released.size() && i < (size_t)n; ++i) {
    check(d.released[i] == pts[i].ctx, (what + ": release " + std::to_string(i)).c_str(),
          (uint32_t)pts[i].ctx, (uint32_t)d.released[i]);
  }
  // Law 4, with teeth: a context is released after its LAST register, so by
  // the time its release fires exactly `w` of its registers have landed.
  // Checking only the count and the order let D21 -- release on member 0 --
  // survive the first sweep.
  for (size_t i = 0; i < d.rel_after.size(); ++i) {
    check(d.rel_after[i].second == w,
          (what + ": release " + std::to_string(i) + " follows ALL its writes").c_str(),
          (uint32_t)w, (uint32_t)d.rel_after[i].second);
  }
  check(d.t.tag_mismatch_o == 0, (what + ": no tag mismatch").c_str(), 0, (int)d.t.tag_mismatch_o);
  check(d.t.writes_o == (uint32_t)want_writes, (what + ": writes counter").c_str(),
        (uint32_t)want_writes, d.t.writes_o);
  check(d.t.groups_o == 1u, (what + ": one group issued").c_str(), 1, (int)d.t.groups_o);
  check(d.t.partial_o == (n < kLanes ? 1u : 0u), (what + ": partial counter").c_str(),
        n < kLanes ? 1 : 0, (int)d.t.partial_o);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  Vzhao_field_v3_dispatch top;

  printf("== section 1: a full group of four, width 1 ==\n");
  run_group(top, {{0, 100, 200, 300}, {1, 101, 201, 301}, {2, 102, 202, 302}, {3, 103, 203, 303}},
            OP_CURVE, 8, "full CURVE");

  printf("== section 2: widths 2 and 3 drain the right registers ==\n");
  run_group(top, {{5, 10, 11, 12}, {6, 20, 21, 22}, {7, 30, 31, 32}, {0, 40, 41, 42}}, OP_NOISE2,
            12, "full NOISE2");
  run_group(top, {{1, -1, -2, -3}, {2, -4, -5, -6}, {3, -7, -8, -9}, {4, -10, -11, -12}}, OP_ROT3,
            16, "full ROT3");

  printf("== section 3: partial groups issue on flush, and pad the rest ==\n");
  run_group(top, {{2, 77, 78, 79}}, OP_CURVE, 4, "one-context CURVE");
  run_group(top, {{0, 1, 2, 3}, {5, 4, 5, 6}}, OP_RIDGE, 5, "two-context RIDGE");
  run_group(top, {{1, 9, 8, 7}, {3, 6, 5, 4}, {6, 3, 2, 1}}, OP_NOISE2, 20, "three-context NOISE2");

  printf("== section 4: a short op and an unknown op are REFUSED ==\n");
  {
    Dut d(top);
    d.reset();
    const bool add_taken = d.offer({0, 1, 2, 3}, OP_ADD, 0, 8);
    check(!add_taken, "a short op (ADD) is refused by the dispatcher", 0, add_taken ? 1 : 0);
    const bool unk_taken = d.offer({0, 1, 2, 3}, 0xFE, 0, 8);
    check(!unk_taken, "an unknown opcode is refused rather than guessed", 0, unk_taken ? 1 : 0);
    check(d.t.groups_o == 0u, "and nothing was issued", 0, (int)d.t.groups_o);
  }

  printf("== section 5: a different op or destination cannot join the group ==\n");
  {
    Dut d(top);
    d.reset();
    const bool first = d.offer({0, 1, 2, 3}, OP_CURVE, 8);
    check(first, "the first context joins", 1, first ? 1 : 0);
    const bool other_op = d.offer({1, 4, 5, 6}, OP_RIDGE, 8, 8);
    check(!other_op, "a DIFFERENT op cannot join the same request", 0, other_op ? 1 : 0);
    const bool other_dst = d.offer({1, 4, 5, 6}, OP_CURVE, 9, 8);
    check(!other_dst, "a different destination cannot join either", 0, other_dst ? 1 : 0);
  }

  printf("== section 5b: a different IMMEDIATE cannot join the group ==\n");
  {
    // Found by trying to compose this with the noise unit: NOISE2's seed IS
    // the instruction's immediate, and two different NOISE2 instructions with
    // different seeds match on op AND destination. Letting them share a
    // request would hand four points one seed and answer three of them for a
    // different program point -- values that are individually plausible and
    // collectively wrong, which is the failure mode this whole block is shaped
    // to avoid.
    Dut d(top);
    d.reset();
    const bool first = d.offer({0, 1, 2, 3}, OP_NOISE2, 8, 64, 0xAAAAu);
    check(first, "the first context joins", 1, first ? 1 : 0);
    const bool same_imm = d.offer({1, 4, 5, 6}, OP_NOISE2, 8, 64, 0xAAAAu);
    check(same_imm, "and the SAME immediate shares the request", 1, same_imm ? 1 : 0);
  }

  printf("== section 5c: a refused offer CLOSES the group, it does not stall it ==\n");
  {
    // THE CONTRACT CHANGED ON 2026-08-29 AND THIS IS THE NEW HALF. It used to
    // be that a mismatched offer was refused and the group stayed open, so a
    // later matching context could still join. That read as strictly better --
    // more batching, same answers -- and it DEADLOCKED THE MACHINE.
    //
    // The executor holds its pipe at S4 until the dispatcher takes the long op
    // ("A LONG OP AT S4 HOLDS THE PIPE UNTIL THE DISPATCHER TAKES IT", and the
    // line after it already noted the dispatcher refuses when mid-group with a
    // different op). So a refused context BLOCKS EVERY OTHER CONTEXT from
    // reaching S4. The later matching context this section used to check for
    // can never arrive in the composed machine -- only a bench driving the
    // port directly, like this one, can produce that sequence.
    //
    // So closing the group costs no batching that was ever reachable, and
    // leaving it open costs the machine. The group closes.
    Dut d(top);
    d.reset();
    const bool first = d.offer({0, 1, 2, 3}, OP_NOISE2, 8, 64, 0xAAAAu);
    check(first, "the first context joins", 1, first ? 1 : 0);
    const bool other_imm = d.offer({1, 4, 5, 6}, OP_NOISE2, 8, 8, 0xBBBBu);
    check(!other_imm, "a different immediate is still refused", 0, other_imm ? 1 : 0);
    const bool after = d.offer({1, 4, 5, 6}, OP_NOISE2, 8, 8, 0xAAAAu);
    check(!after, "and the group has CLOSED -- even a matching context must wait", 0,
          after ? 1 : 0);
  }

  printf("== section 6: an offer arriving WITH flush is refused, not swallowed ==\n");
  {
    // THE HOLE THIS CLOSES. Accepted on the flush clock, the context would
    // land outside the snapshot and then be cleared -- handshaked and gone.
    Dut d(top);
    d.reset();
    const bool a = d.offer({2, 55, 56, 57}, OP_CURVE, 3);
    check(a, "one context is in the group", 1, a ? 1 : 0);

    d.t.flush_i = 1;
    d.t.long_valid_i = 1;
    d.t.long_ctx_i = 4;
    d.t.long_op_i = OP_CURVE;
    d.t.long_dst_i = 3;
    d.t.long_s0_i = 99;
    d.t.long_s1_i = 98;
    d.t.long_s2_i = 97;
    d.t.long_s3_i = 97;
    d.t.eval();
    check(d.t.long_ready_o == 0, "ready is LOW while flushing -- the offer is refused", 0,
          (int)d.t.long_ready_o);

    // The flush issues the group of one; the standing offer must survive it.
    const bool got = d.take_request();
    check(got, "the partial group issued", 1, got ? 1 : 0);
    check((int32_t)d.t.svc_s0_o[0] == 55, "and it carries the FIRST context, not the refused one",
          55, (int32_t)d.t.svc_s0_o[0]);
    const uint8_t tag6 = (uint8_t)d.t.svc_tag_o;
    d.accept_request();
    d.t.flush_i = 0;
    d.t.eval();

    // ONE SLOT IS OUTSTANDING, so the dispatcher cannot gather a new group
    // until this one has come back and drained. My first version of this
    // section expected the standing offer to be taken immediately and it
    // FAILED -- correctly, because ready must stay low while a group is in
    // flight. Asserting that explicitly is worth more than the check that was
    // wrong.
    for (int i = 0; i < 8; ++i) d.step();
    check(d.t.long_ready_o == 0, "ready stays LOW while a group is outstanding", 0,
          (int)d.t.long_ready_o);

    int32_t r6[kLanes] = {11, 22, 33, 44};
    d.reply(tag6, r6, r6, r6);
    d.drain();

    // Only the ONE real lane may write, and only that context is released.
    check(d.writes.size() == 1u, "the one-context group wrote exactly one register", 1,
          (uint32_t)d.writes.size());
    check(d.released.size() == 1u, "and released exactly one context", 1,
          (uint32_t)d.released.size());

    int guard = 0;
    while (!d.t.long_ready_o && guard++ < 32) d.step();
    check(d.t.long_ready_o == 1, "the refused offer SURVIVED the round trip and is now taken", 1,
          (int)d.t.long_ready_o);
    d.step();
    d.t.long_valid_i = 0;
    d.t.eval();
  }

  printf("== section 7: the drain stalls under backpressure and loses nothing ==\n");
  {
    Dut d(top);
    d.reset();
    const std::vector<Point> pts = {{0, 1, 1, 1}, {1, 2, 2, 2}, {2, 3, 3, 3}, {3, 4, 4, 4}};
    for (const Point& p : pts) d.offer(p, OP_NOISE2, 10);
    d.take_request();
    const uint8_t tag = (uint8_t)d.t.svc_tag_o;
    d.accept_request();
    int32_t r[kLanes];
    for (int l = 0; l < kLanes; ++l) r[l] = (int32_t)(0xB0000000 | l);
    d.reply(tag, r, r, r);

    // Hold ready LOW for a while: the drain must wait, not proceed.
    d.t.wb_ready_i = 0;
    d.t.eval();
    for (int i = 0; i < 20; ++i) d.step();
    check(d.writes.empty(), "nothing is written while wb_ready is low", 0,
          (uint32_t)d.writes.size());
    check(d.t.wb_valid_o == 1, "and the request stays asserted", 1, (int)d.t.wb_valid_o);

    // Then release it one clock at a time, which is the harsher case.
    int guard = 0;
    while (d.t.wb_valid_o && guard++ < 64) {
      d.t.wb_ready_i = 1;
      d.step();
      d.t.wb_ready_i = 0;
      d.step();
    }
    d.step();
    check(d.writes.size() == 8u, "all eight registers arrive, one per grant", 8,
          (uint32_t)d.writes.size());
    check(d.released.size() == 4u, "and all four contexts are released", 4,
          (uint32_t)d.released.size());
  }

  printf("== section 8: a wrong tag is REPORTED ==\n");
  {
    Dut d(top);
    d.reset();
    d.offer({0, 1, 2, 3}, OP_CURVE, 2);
    d.t.flush_i = 1;
    d.t.eval();
    d.take_request();
    const uint8_t tag = (uint8_t)d.t.svc_tag_o;
    d.accept_request();
    d.t.flush_i = 0;
    d.t.eval();
    int32_t r[kLanes] = {1, 2, 3, 4};
    d.reply((uint8_t)(tag + 1), r, r, r);  // deliberately wrong
    check(d.t.tag_mismatch_o == 1, "a reply with the wrong tag sets tag_mismatch_o", 1,
          (int)d.t.tag_mismatch_o);
    d.drain();
  }

  printf("== section 9: a FIFTH context is refused while the group is full ==\n");
  {
    // Mutant D03 -- `fill_r <= 4` -- SURVIVED the first sweep: a fifth context
    // is accepted on the clock the full group is still in D_GATHER, and
    // 4[1:0] is 0, so it OVERWRITES LANE 0. Nothing in the file offered a
    // fifth context, so nothing reached it.
    Dut d(top);
    d.reset();
    const std::vector<Point> four = {
        {0, 900, 901, 902}, {1, 910, 911, 912}, {2, 920, 921, 922}, {3, 930, 931, 932}};
    for (const Point& p : four) d.offer(p, OP_CURVE, 6);

    d.t.long_valid_i = 1;
    d.t.long_ctx_i = 7;
    d.t.long_op_i = OP_CURVE;
    d.t.long_dst_i = 6;
    d.t.long_s0_i = (uint32_t)-1;
    d.t.long_s1_i = (uint32_t)-1;
    d.t.long_s2_i = (uint32_t)-1;
    d.t.long_s3_i = (uint32_t)-1;
    d.t.eval();
    check(d.t.long_ready_o == 0, "a fifth context is REFUSED while four are gathered", 0,
          (int)d.t.long_ready_o);

    const bool got = d.take_request();
    check(got, "the full group still issues", 1, got ? 1 : 0);
    if (got) {
      check((int32_t)d.t.svc_s0_o[0] == 900, "and lane 0 still holds the FIRST context", 900,
            (int32_t)d.t.svc_s0_o[0]);
      check((int32_t)d.t.svc_s0_o[3] == 930, "and lane 3 the fourth", 930,
            (int32_t)d.t.svc_s0_o[3]);
    }
    d.t.long_valid_i = 0;
    d.accept_request();
  }

  printf("== section 10: flush with NOTHING gathered issues nothing ==\n");
  {
    // Mutant D07 -- dropping `fill_r != 0` -- SURVIVED: an EMPTY group is
    // issued on flush. Nothing asserted flush with an empty gather.
    Dut d(top);
    d.reset();
    d.t.flush_i = 1;
    d.t.svc_ready_i = 1;
    d.t.eval();
    for (int i = 0; i < 24; ++i) {
      check(d.t.svc_valid_o == 0, "no request is issued for an EMPTY group", 0,
            (int)d.t.svc_valid_o);
      d.step();
    }
    check(d.t.groups_o == 0u, "and the group counter never moved", 0, (int)d.t.groups_o);
    d.t.flush_i = 0;
    d.t.svc_ready_i = 0;
    d.t.eval();
  }

  printf("== section 11: consecutive groups carry DIFFERENT tags ==\n");
  {
    // Mutant D24 -- the tag never increments -- SURVIVED: every group carried
    // the same tag and nothing compared two of them. One group per instance
    // is exactly the shape that cannot see it.
    Dut d(top);
    d.reset();
    uint8_t tags[3];
    for (int g = 0; g < 3; ++g) {
      const std::vector<Point> pts = {{0, 10 * g + 1, 0, 0}, {1, 10 * g + 2, 0, 0}};
      for (const Point& p : pts) d.offer(p, OP_CURVE, 7);
      d.t.flush_i = 1;
      d.t.eval();
      const bool got = d.take_request();
      check(got, ("group " + std::to_string(g) + " issued").c_str(), 1, got ? 1 : 0);
      tags[g] = (uint8_t)d.t.svc_tag_o;
      d.accept_request();
      d.t.flush_i = 0;
      d.t.eval();
      int32_t r[kLanes] = {1, 2, 3, 4};
      d.reply(tags[g], r, r, r);
      d.drain();
    }
    check(tags[1] != tags[0], "the second group's tag differs from the first", 1,
          tags[1] != tags[0] ? 1 : 0);
    check(tags[2] != tags[1], "and the third from the second", 1, tags[2] != tags[1] ? 1 : 0);
    printf("   MEASURED tags: %u, %u, %u\n", tags[0], tags[1], tags[2]);
    check(d.t.groups_o == 3u, "three groups issued", 3, (int)d.t.groups_o);
  }

  return zhao::report_and_exit("field_v3_dispatch_directed");
}
