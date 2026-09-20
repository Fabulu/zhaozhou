// geom_loomfeed_directed.cpp -- GEOM.LOOM's NODE-STREAM CARRIER: does a stream
// the ARM staged in HPS DDR reach the loom, compose, and come back answered?
//
// The block is `zhao_geom_loomfeed`, core entry I50's closure, owner rulings
// R58 and R69. The DUT is `tb_geom_loomfeed_chain`: the carrier and the REAL
// `zhao_geom_loom` behind it. What is played here is the HPS-DDR bridge, so a
// case can hand over a stream, a malformed header, or a refusal on cue.
//
// THE LOOM IS NOT MOCKED, and cases 5 and 8 are why. Both of the carrier's
// hard laws are statements about the loom's own behaviour -- that S_DRAIN
// swallows beats until `last`, and that S_RUN and S_DRAIN are released by a
// `last` and by nothing else -- and a mock would have been written to agree
// with whatever the carrier does. The first draft of law 4 was BACKWARDS for
// exactly that reason, and a mock would have passed it.
//
// EVERY CASE IS A COUNTER THAT MUST MOVE, and every assertion is on the CORRECT
// behaviour rather than on a defect (CLAUDE.md: "do not write a test that
// asserts the bug").
//
//   1  a staged stream traverses and COMPOSES                 streams_o, bursts_o
//   2  a misaligned base is refused before ANY burst          posts_refused_align_o
//   3  a wrong magic is refused after exactly ONE burst       headers_refused_o
//   4  a count of 0 and a count over MAX_NODES are refused,   headers_refused_o
//      never CLAMPED
//   5  the loom refuses a stream, the carrier PLAYS ON, and   streams_refused_o
//      THE NEXT STREAM STILL COMPOSES  <- law 4
//   6  a post into a full mailbox is HELD, never dropped      post_stalls_o
//   7  a TRANSIENT bridge refusal is re-offered and the       bridge_errs_o
//      stream still composes                                  <- law 5
//   8  a PERMANENT bridge refusal faults the stream, and      streams_faulted_o,
//      THE NEXT STREAM IS FLUSHED, REPLAYED AND COMPOSES      streams_replayed_o
//                                                              <- law 6
//   9  `feed_wait_cycles_o` is wired and moves
//
// `ret_overflow_o` is the tenth and it CANNOT be fired with legal stimulus: the
// return credit is what makes the state unreachable. That is the committed
// mutant's job -- tests/mutants/zhao_geom_loomfeed_mutant.sv -- and case 10
// here asserts only that it stays zero, which is the claim the mutant turns
// into a measurement.

#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vtb_geom_loomfeed_chain.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

using Dut = Vtb_geom_loomfeed_chain;

constexpr uint32_t kMagic = 0x4D4F4F4Cu;  // "LOOM", the block's STREAM_MAGIC
constexpr uint32_t kPlan = 0xABCD1234u;
constexpr int kMaxNodes = 16;

// The carrier's return reasons, in its own order.
constexpr uint8_t kRrNone = 0;
constexpr uint8_t kRrAlign = 1;
constexpr uint8_t kRrMagic = 2;
constexpr uint8_t kRrCount = 3;
constexpr uint8_t kRrLoom = 4;
constexpr uint8_t kRrBridge = 5;

// GEOM.LOOM's refusal reasons, in its own order.
constexpr uint8_t kLoomNotSorted = 0;
constexpr uint8_t kLoomFraming = 5;

constexpr uint8_t kKindRoot = 0;
constexpr uint8_t kKindRigid = 1;

// ---------------------------------------------------------------------------
// THE HPS-DDR SIDE, PLAYED
// ---------------------------------------------------------------------------
// A word-addressed store plus the bridge's own timing shape: a grant PULSE at
// acceptance, then beats one per cycle after a fixed first-beat latency. The
// latency is the frozen sim profile `zhao_pkg` names (16 gpu cycles), shortened
// here because this test measures behaviour, not bandwidth.
class Ddr {
 public:
  void write64(uint32_t addr, uint64_t v) { mem_[addr] = v; }

  // Refuse the next `n` requests with `err`. Nothing is issued for a refused
  // request, which is the bridge's own law.
  void refuse_next(int n) { refuse_ = n; }

  void reset_state() {
    phase_ = 0;
    beat_ = 0;
    refuse_ = 0;
    reqs_ = 0;
  }

  int requests() const { return reqs_; }

  // Drive the DUT's bridge inputs for THIS cycle, from the DUT's request
  // outputs. Called before every tick.
  void drive(Dut& d) {
    d.grant_i = 0;
    d.err_i = 0;
    d.beat_valid_i = 0;
    d.beat_data_i = 0;
    d.beat_last_i = 0;

    if (phase_ == 0) {
      if (d.req_valid_o) {
        ++reqs_;
        if (refuse_ > 0) {
          --refuse_;
          d.err_i = 1;
          return;
        }
        addr_ = d.req_addr_o;
        d.grant_i = 1;
        phase_ = 1;
        wait_ = kFirstBeatWait;
        beat_ = 0;
      }
      return;
    }

    if (phase_ == 1) {
      if (wait_ > 0) {
        --wait_;
        return;
      }
      auto it = mem_.find(addr_ + 8u * static_cast<uint32_t>(beat_));
      d.beat_valid_i = 1;
      d.beat_data_i = (it == mem_.end()) ? 0ull : it->second;
      d.beat_last_i = (beat_ == 7) ? 1 : 0;
      ++beat_;
      if (beat_ == 8) phase_ = 0;
    }
  }

 private:
  static constexpr int kFirstBeatWait = 3;
  std::map<uint32_t, uint64_t> mem_;
  uint32_t addr_ = 0;
  int phase_ = 0;
  int beat_ = 0;
  int wait_ = 0;
  int refuse_ = 0;
  int reqs_ = 0;
};

Ddr g_ddr;

void step(Dut& d) {
  g_ddr.drive(d);
  zhao::tick(d);
}

void reset(Dut& d, int cycles) {
  d.rst_n = 0;
  d.cfg_plan_base_i = kPlan;
  d.post_valid_i = 0;
  d.post_base_i = 0;
  d.post_ticket_i = 0;
  d.ret_ready_i = 0;
  d.lm_out_ready_i = 1;  // the palette write port never stalls (the console's)
  d.grant_i = 0;
  d.err_i = 0;
  d.beat_valid_i = 0;
  d.beat_data_i = 0;
  d.beat_last_i = 0;
  d.eval();
  for (int i = 0; i < cycles; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
  zhao::tick(d);
  g_ddr.reset_state();
}

// ---------------------------------------------------------------------------
// THE FROZEN RECORD, BUILT HERE FROM THE CONTRACT
// ---------------------------------------------------------------------------
// design/contracts/GEOM.LOOM.STREAM.md section "the record". Written out by
// hand rather than derived from the RTL, so a change to either side is a
// disagreement rather than a silent agreement.
struct Node {
  uint16_t index = 0;
  uint16_t parent = 0;
  uint8_t kind = kKindRoot;
  uint8_t axis = 0;
  bool bodypatch = false;
  uint16_t angle = 0;
  uint16_t src_id = 0;
  int32_t param[12] = {0};
};

void stage_header(uint32_t base, uint16_t count, const int32_t cam[9],
                  uint32_t magic = kMagic) {
  g_ddr.write64(base + 0, static_cast<uint64_t>(magic) |
                              (static_cast<uint64_t>(count) << 32));
  for (int i = 0; i < 4; ++i) {
    g_ddr.write64(base + 8u * static_cast<uint32_t>(i + 1),
                  static_cast<uint64_t>(static_cast<uint32_t>(cam[2 * i])) |
                      (static_cast<uint64_t>(static_cast<uint32_t>(cam[2 * i + 1]))
                       << 32));
  }
  g_ddr.write64(base + 40u, static_cast<uint64_t>(static_cast<uint32_t>(cam[8])));
  g_ddr.write64(base + 48u, 0);
  g_ddr.write64(base + 56u, 0);
}

void stage_node(uint32_t base, int record_index, const Node& n) {
  const uint32_t a = base + 64u * static_cast<uint32_t>(record_index);
  const uint64_t w0 = (static_cast<uint64_t>(n.index) & 0x3FFull) |
                      ((static_cast<uint64_t>(n.parent) & 0x3FFull) << 10) |
                      ((static_cast<uint64_t>(n.kind) & 0xFull) << 20) |
                      ((static_cast<uint64_t>(n.axis) & 0x3ull) << 24) |
                      (static_cast<uint64_t>(n.bodypatch ? 1 : 0) << 26) |
                      (static_cast<uint64_t>(n.angle) << 32) |
                      (static_cast<uint64_t>(n.src_id) << 48);
  g_ddr.write64(a + 0, w0);
  for (int i = 0; i < 6; ++i) {
    g_ddr.write64(a + 8u * static_cast<uint32_t>(i + 1),
                  static_cast<uint64_t>(static_cast<uint32_t>(n.param[2 * i])) |
                      (static_cast<uint64_t>(static_cast<uint32_t>(n.param[2 * i + 1]))
                       << 32));
  }
  g_ddr.write64(a + 56u, 0);
}

// A plain chain: node 0 is a ROOT carrying `root_m`, nodes 1..n-1 are RIGID
// children of the one before, each the identity. The loom's own section-1
// property then says the emitted transform of every node equals `root_m`.
void stage_chain(uint32_t base, int count, const int32_t root_m[12],
                 uint16_t src_id) {
  int32_t cam[9] = {0};
  cam[0] = cam[4] = cam[8] = 1 << 16;
  stage_header(base, static_cast<uint16_t>(count), cam);
  for (int i = 0; i < count; ++i) {
    Node n;
    n.index = static_cast<uint16_t>(i);
    n.parent = static_cast<uint16_t>(i == 0 ? 0 : i - 1);
    n.kind = (i == 0) ? kKindRoot : kKindRigid;
    n.src_id = src_id;
    if (i == 0) {
      for (int e = 0; e < 12; ++e) n.param[e] = root_m[e];
    } else {
      n.param[0] = n.param[5] = n.param[10] = 1 << 16;  // identity 3x4
    }
    stage_node(base, i + 1, n);
  }
}

// Offer a post, holding it until the mailbox takes it.
bool post(Dut& d, uint32_t base, uint32_t ticket, int max_wait = 2000) {
  d.post_base_i = base;
  d.post_ticket_i = ticket;
  d.post_valid_i = 1;
  for (int i = 0; i < max_wait; ++i) {
    d.eval();
    if (d.post_ready_o) {
      step(d);
      d.post_valid_i = 0;
      return true;
    }
    step(d);
  }
  d.post_valid_i = 0;
  return false;
}

struct Ret {
  bool seen = false;
  uint32_t ticket = 0;
  uint8_t ok = 0;
  uint8_t refused = 0;
  uint8_t reason = 0;
  uint8_t loom_reason = 0;
  uint16_t nodes = 0;
  uint32_t plan = 0;
};

// Run the machine until a return record is available, then take it.
Ret await_return(Dut& d, int max_wait = 20000) {
  Ret r;
  d.ret_ready_i = 1;
  for (int i = 0; i < max_wait; ++i) {
    d.eval();
    if (d.ret_valid_o) {
      r.seen = true;
      r.ticket = d.ret_ticket_o;
      r.ok = d.ret_ok_o;
      r.refused = d.ret_refused_o;
      r.reason = d.ret_reason_o;
      r.loom_reason = d.ret_loom_reason_o;
      r.nodes = d.ret_nodes_o;
      r.plan = d.ret_plan_o;
      step(d);
      d.ret_ready_i = 0;
      return r;
    }
    step(d);
  }
  d.ret_ready_i = 0;
  return r;
}

void idle(Dut& d, int n) {
  for (int i = 0; i < n; ++i) step(d);
}

// Run the machine with the return port open until nothing is left in flight.
// Every case ends through here, because a case that leaves a post queued makes
// the NEXT case's stimulus land on the wrong stream -- which is how case 6's
// first draft silently ate case 7's injected bridge refusal and made three
// later cases fail for a reason that had nothing to do with them.
void quiesce(Dut& d, int n = 3000) {
  d.post_valid_i = 0;
  d.ret_ready_i = 1;
  for (int i = 0; i < n; ++i) step(d);
  d.ret_ready_i = 0;
  d.eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;
  reset(dut, 4);

  // Distinct, non-zero matrix elements: a forwarding bug that drops a field
  // looks identical to a zero that was never set.
  int32_t root_m[12];
  for (int e = 0; e < 12; ++e) root_m[e] = 0x00010000 + 0x1111 * (e + 1);

  // ---- 1. a staged stream TRAVERSES and COMPOSES --------------------------
  const uint32_t kBaseA = 0x0010'0000u;
  stage_chain(kBaseA, 4, root_m, 0x1234);

  check(post(dut, kBaseA, 0xA001u), "the mailbox took the first post", 1, 1);
  {
    Ret r = await_return(dut);
    check(r.seen, "case1: the stream was answered", 1, r.seen ? 1 : 0);
    check(r.ticket == 0xA001u, "case1: the return names the post's ticket",
          0xA001u, r.ticket);
    check(r.ok == 1, "case1: the stream composed", 1, r.ok);
    check(r.refused == 0, "case1: it was not refused", 0, r.refused);
    check(r.reason == kRrNone, "case1: reason NONE", kRrNone, r.reason);
    check(r.nodes == 4, "case1: four nodes delivered", 4, r.nodes);
    check(r.plan == kPlan, "case1: the plan identity rides the return", kPlan,
          r.plan);
  }
  check(dut.posts_o == 1, "case1: one post consumed", 1, dut.posts_o);
  check(dut.streams_o == 1, "case1: one stream delivered whole", 1, dut.streams_o);
  check(dut.nodes_o == 4, "case1: four node beats handed over", 4, dut.nodes_o);
  // Five bursts: the header plus one per node. THE NUMBER IS THE LAYOUT --
  // one record is one 64-byte burst, and a record that straddled a burst
  // boundary would show up here as ten.
  check(dut.bursts_o == 5, "case1: five 64-byte bursts (header + 4 nodes)", 5,
        dut.bursts_o);

  // THE VALUE TRAVERSED. The loom composes every RIGID identity child against
  // its parent, so every node's world transform is the ROOT's own parameters
  // -- which is `zhao_geom_loom`'s section-1 property, read here through the
  // carrier. If the record decoded wrongly this is what says so.
  {
    bool saw_out = false;
    int32_t m0 = 0, m3 = 0;
    uint16_t src = 0;
    for (int i = 0; i < 4000 && !saw_out; ++i) {
      dut.eval();
      if (dut.lm_out_valid_o) {
        saw_out = true;
        m0 = static_cast<int32_t>(dut.lm_out_m0_o);
        m3 = static_cast<int32_t>(dut.lm_out_m3_o);
        src = dut.lm_out_src_id_o;
      }
      step(dut);
    }
    check(saw_out, "case1: a transform left the loom", 1, saw_out ? 1 : 0);
    check(m0 == root_m[0], "case1: element 0 is the ROOT's own parameter",
          static_cast<uint64_t>(static_cast<uint32_t>(root_m[0])),
          static_cast<uint64_t>(static_cast<uint32_t>(m0)));
    check(m3 == root_m[3], "case1: element 3 (the x translation) survived",
          static_cast<uint64_t>(static_cast<uint32_t>(root_m[3])),
          static_cast<uint64_t>(static_cast<uint32_t>(m3)));
    check(src == 0x1234, "case1: the src_id survived the record", 0x1234, src);
  }
  // Drain the emission and every return so the next case starts clean.
  quiesce(dut);
  check(dut.lm_streams_composed_o == 1, "case1: the loom composed one stream", 1,
        dut.lm_streams_composed_o);

  // ---- 2. a MISALIGNED base is refused before ANY burst -------------------
  {
    const uint32_t bursts_before = dut.bursts_o;
    check(post(dut, kBaseA + 8u, 0xA002u), "the mailbox took the misaligned post",
          1, 1);
    Ret r = await_return(dut);
    check(r.seen, "case2: the misaligned post was ANSWERED", 1, r.seen ? 1 : 0);
    check(r.refused == 1, "case2: refused", 1, r.refused);
    check(r.reason == kRrAlign, "case2: reason ALIGN", kRrAlign, r.reason);
    check(r.ok == 0, "case2: not ok", 0, r.ok);
    check(dut.posts_refused_align_o == 1, "case2: counted", 1,
          dut.posts_refused_align_o);
    check(dut.bursts_o == bursts_before,
          "case2: NOT ONE BURST was issued for it", bursts_before, dut.bursts_o);
  }

  // ---- 3. a WRONG MAGIC is refused after exactly one burst ----------------
  {
    const uint32_t kBaseB = 0x0020'0000u;
    int32_t cam[9] = {0};
    stage_header(kBaseB, 4, cam, 0xDEADBEEFu);
    const uint32_t bursts_before = dut.bursts_o;
    check(post(dut, kBaseB, 0xA003u), "the mailbox took the bad-magic post", 1, 1);
    Ret r = await_return(dut);
    check(r.seen, "case3: the bad header was ANSWERED", 1, r.seen ? 1 : 0);
    check(r.reason == kRrMagic, "case3: reason MAGIC", kRrMagic, r.reason);
    check(r.refused == 1, "case3: refused", 1, r.refused);
    check(dut.headers_refused_o == 1, "case3: counted", 1, dut.headers_refused_o);
    check(dut.bursts_o == bursts_before + 1,
          "case3: exactly ONE burst -- the header, and no nodes",
          bursts_before + 1, dut.bursts_o);
  }

  // ---- 4. a count of 0, and a count over MAX_NODES, are refused -----------
  // NOT CLAMPED. A count of 2,000 played as 16 would be a picture of the wrong
  // creature with every counter agreeing.
  {
    const uint32_t kBaseC = 0x0030'0000u;
    int32_t cam[9] = {0};
    stage_header(kBaseC, 0, cam);
    check(post(dut, kBaseC, 0xA004u), "the mailbox took the zero-count post", 1, 1);
    Ret r = await_return(dut);
    check(r.reason == kRrCount, "case4a: a zero-node stream is refused", kRrCount,
          r.reason);

    const uint32_t kBaseD = 0x0040'0000u;
    stage_header(kBaseD, kMaxNodes + 1, cam);
    const uint32_t composed_before = dut.lm_streams_composed_o;
    const uint32_t nodes_before = dut.nodes_o;
    check(post(dut, kBaseD, 0xA005u), "the mailbox took the over-count post", 1, 1);
    Ret r2 = await_return(dut);
    check(r2.reason == kRrCount, "case4b: a count over MAX_NODES is refused",
          kRrCount, r2.reason);
    check(dut.nodes_o == nodes_before,
          "case4b: NOT CLAMPED -- no node beat was handed over", nodes_before,
          dut.nodes_o);
    check(dut.lm_streams_composed_o == composed_before,
          "case4b: and the loom composed nothing", composed_before,
          dut.lm_streams_composed_o);
    check(dut.headers_refused_o == 3, "case4: three header refusals now", 3,
          dut.headers_refused_o);
  }

  // ---- 5. THE LOOM REFUSES, THE CARRIER PLAYS ON, THE NEXT ONE COMPOSES ---
  // LAW 4, and the case that catches the backwards version. A node whose
  // parent index is not below its own is NOT_SORTED; the loom refuses it, then
  // SWALLOWS beats in S_DRAIN until the stream's `last`. A carrier that stopped
  // feeding would leave the loom draining, and the NEXT stream would vanish
  // into that drain with no refusal raised at all -- so the assertion that
  // matters is the one about the stream AFTER this one.
  {
    const uint32_t kBaseE = 0x0050'0000u;
    int32_t cam[9] = {0};
    cam[0] = cam[4] = cam[8] = 1 << 16;
    stage_header(kBaseE, 4, cam);
    for (int i = 0; i < 4; ++i) {
      Node n;
      n.index = static_cast<uint16_t>(i);
      n.kind = (i == 0) ? kKindRoot : kKindRigid;
      // Node 2 names node 3 as its parent: forward, so not topologically
      // sorted, and the loom's own precondition.
      n.parent = static_cast<uint16_t>(i == 2 ? 3 : (i == 0 ? 0 : i - 1));
      n.src_id = 0x5555;
      n.param[0] = n.param[5] = n.param[10] = 1 << 16;
      stage_node(kBaseE, i + 1, n);
    }
    check(post(dut, kBaseE, 0xA006u), "the mailbox took the unsorted post", 1, 1);
    Ret r = await_return(dut);
    check(r.seen, "case5: the refused stream was ANSWERED", 1, r.seen ? 1 : 0);
    check(r.ok == 0, "case5: not ok", 0, r.ok);
    check(r.reason == kRrLoom, "case5: reason LOOM", kRrLoom, r.reason);
    check(r.loom_reason == kLoomNotSorted,
          "case5: the LOOM'S OWN reason travels out, not a re-derived one",
          kLoomNotSorted, r.loom_reason);
    // Three, not four: the offending beat WAS handed over (the loom's
    // `in_ready_o` is a pure state decode and never looks at the fault), and
    // the fourth was swallowed by S_DRAIN. Reporting four would tell the ARM
    // the stream nearly worked.
    check(r.nodes == 3,
          "case5: `nodes` is the count AT the refusal, not the drained total",
          3, r.nodes);
    check(dut.streams_refused_o == 1, "case5: counted once", 1,
          dut.streams_refused_o);
    check(dut.streams_o == 1, "case5: NOT counted as delivered", 1, dut.streams_o);
    quiesce(dut);

    // THE ASSERTION THIS CASE EXISTS FOR.
    const uint32_t kBaseF = 0x0060'0000u;
    stage_chain(kBaseF, 3, root_m, 0x6666);
    const uint32_t composed_before = dut.lm_streams_composed_o;
    check(post(dut, kBaseF, 0xA007u), "the mailbox took the follow-up post", 1, 1);
    Ret r2 = await_return(dut);
    check(r2.ok == 1,
          "case5: THE STREAM AFTER A REFUSED ONE STILL COMPOSES (law 4)", 1,
          r2.ok);
    check(r2.nodes == 3, "case5: all three of its nodes reached the loom", 3,
          r2.nodes);
    quiesce(dut);
    check(dut.lm_streams_composed_o == composed_before + 1,
          "case5: and the LOOM says so too", composed_before + 1,
          dut.lm_streams_composed_o);
  }

  // ---- 6. a post into a FULL mailbox is HELD, never dropped ---------------
  // LAW 1, and it needs the drain BLOCKED to reach: with the return port shut,
  // law 7's credit stops the drain after RETQ consumed posts, the POSTS-deep
  // mailbox then fills, and `post_ready_o` goes low. Offering three was not
  // enough -- the drain consumed them as fast as they arrived and the mailbox
  // was never full, which is the first draft of this case reporting "held" as
  // false and being right.
  {
    const uint32_t stalls_before = dut.post_stalls_o;
    const uint32_t posts_before = dut.posts_o;
    const uint32_t kBaseG = 0x0070'0000u;
    constexpr int kOffered = 8;
    for (int s = 0; s < kOffered; ++s) {
      stage_chain(kBaseG + 0x1000u * static_cast<uint32_t>(s), 2, root_m,
                  static_cast<uint16_t>(0x7700 + s));
    }
    dut.ret_ready_i = 0;

    int offered = 0;
    int answered = 0;
    bool held = false;
    for (int i = 0; i < 60000 && offered < kOffered; ++i) {
      dut.post_base_i = kBaseG + 0x1000u * static_cast<uint32_t>(offered);
      dut.post_ticket_i = 0xA100u + static_cast<uint32_t>(offered);
      dut.post_valid_i = 1;
      dut.eval();
      // Returns drained DURING the offering loop are still answers, and not
      // counting them here is how the first draft reported 3 of 8 and looked
      // like five dropped posts.
      if (dut.ret_ready_i && dut.ret_valid_o) ++answered;
      if (dut.post_ready_o) {
        step(dut);
        ++offered;
        dut.post_valid_i = 0;
      } else {
        held = true;
        // Open the return port only once the hold has been WITNESSED, so the
        // hold is what is measured and not the drain's speed.
        if (dut.post_stalls_o > stalls_before + 20) dut.ret_ready_i = 1;
        step(dut);
      }
    }
    dut.post_valid_i = 0;
    check(offered == kOffered, "case6: every post was eventually TAKEN",
          kOffered, offered);
    check(held, "case6: `post_ready_o` went low -- a post was HELD", 1,
          held ? 1 : 0);
    check(dut.post_stalls_o > stalls_before,
          "case6: the HELD CYCLES are counted, not the drop", 1,
          dut.post_stalls_o > stalls_before ? 1 : 0);

    dut.ret_ready_i = 1;
    for (int i = 0; i < 120000 && answered < kOffered; ++i) {
      dut.eval();
      if (dut.ret_valid_o) ++answered;
      step(dut);
    }
    dut.ret_ready_i = 0;
    check(answered == kOffered,
          "case6: every post was ANSWERED, none dropped", kOffered, answered);
    check(dut.posts_o == posts_before + kOffered,
          "case6: and every one was consumed", posts_before + kOffered,
          dut.posts_o);
    quiesce(dut);
  }

  // ---- 7. a TRANSIENT bridge refusal is re-offered ------------------------
  // LAW 5's first half. The bridge refuses once with `err` and issues nothing;
  // `zhao_hps_arbiter_n` re-serves a HELD request, so the carrier takes its
  // request DOWN and offers it again as a new one.
  {
    const uint32_t kBaseH = 0x0080'0000u;
    stage_chain(kBaseH, 3, root_m, 0x8888);
    const uint32_t errs_before = dut.bridge_errs_o;
    const uint32_t composed_before = dut.lm_streams_composed_o;
    g_ddr.refuse_next(1);
    check(post(dut, kBaseH, 0xA00Bu), "the mailbox took the transient post", 1, 1);
    Ret r = await_return(dut);
    check(dut.bridge_errs_o == errs_before + 1,
          "case7: the refusal was COUNTED", errs_before + 1, dut.bridge_errs_o);
    check(r.ok == 1, "case7: and the stream STILL COMPOSED", 1, r.ok);
    check(dut.streams_faulted_o == 0, "case7: it was not a fault", 0,
          dut.streams_faulted_o);
    quiesce(dut);
    check(dut.lm_streams_composed_o == composed_before + 1,
          "case7: the loom composed it", composed_before + 1,
          dut.lm_streams_composed_o);
  }

  // ---- 8. a PERMANENT refusal faults, and the NEXT stream is FLUSHED ------
  // LAW 5's second half and the whole of LAW 6. Four consecutive refusals of
  // one burst abandon the stream mid-play, which leaves the loom waiting for a
  // `last` it will never get. The next stream is therefore played TWICE: once
  // to hand over that `last`, once for real.
  {
    const uint32_t kBaseI = 0x0090'0000u;
    stage_chain(kBaseI, 4, root_m, 0x9999);
    const uint32_t replayed_before = dut.streams_replayed_o;
    // Let the header and the first node through, then refuse the second node's
    // burst permanently -- so beats HAVE reached the loom and it is open.
    check(post(dut, kBaseI, 0xA00Cu), "the mailbox took the fault post", 1, 1);
    // Wait until at least one node beat has been handed over, then jam.
    const uint32_t nodes_at_post = dut.nodes_o;
    for (int i = 0; i < 4000; ++i) {
      dut.eval();
      if (dut.nodes_o > nodes_at_post) break;
      step(dut);
    }
    g_ddr.refuse_next(64);
    Ret r = await_return(dut);
    check(r.seen, "case8: the faulted stream was ANSWERED", 1, r.seen ? 1 : 0);
    check(r.reason == kRrBridge, "case8: reason BRIDGE", kRrBridge, r.reason);
    check(r.ok == 0, "case8: not ok", 0, r.ok);
    check(dut.streams_faulted_o == 1, "case8: counted as a fault", 1,
          dut.streams_faulted_o);
    g_ddr.refuse_next(0);

    // THE ASSERTION LAW 6 EXISTS FOR. The loom is open; the next stream must
    // still compose, and the carrier must say it replayed.
    const uint32_t kBaseJ = 0x00A0'0000u;
    stage_chain(kBaseJ, 3, root_m, 0xAAAA);
    const uint32_t composed_before = dut.lm_streams_composed_o;
    const uint32_t framing_before = dut.lm_refused_framing_o;
    check(post(dut, kBaseJ, 0xA00Du), "the mailbox took the recovery post", 1, 1);
    Ret r2 = await_return(dut);
    check(r2.ok == 1,
          "case8: THE STREAM AFTER A FAULT STILL COMPOSES (law 6)", 1, r2.ok);
    check(r2.nodes == 3, "case8: all three of its nodes reached the loom", 3,
          r2.nodes);
    check(dut.streams_replayed_o == replayed_before + 1,
          "case8: and the carrier SAYS it flushed and replayed",
          replayed_before + 1, dut.streams_replayed_o);
    quiesce(dut);
    check(dut.lm_streams_composed_o == composed_before + 1,
          "case8: the loom composed the replayed stream", composed_before + 1,
          dut.lm_streams_composed_o);
    // Corroboration from the other side: the loom DID see the framing fault the
    // flush pass was there to absorb. This is the instrument, not the claim.
    check(dut.lm_refused_framing_o > framing_before,
          "case8: the loom's FRAMING refusal fired on the flush pass", 1,
          dut.lm_refused_framing_o > framing_before ? 1 : 0);
    check(dut.lm_refused_framing_o >= 1, "case8: (loom framing count)", 1,
          dut.lm_refused_framing_o >= 1 ? 1 : 0);
  }

  // ---- 9. the feed-rate instrument is WIRED -------------------------------
  // One record in flight means the loom is sometimes ready with nothing to
  // give it. A zero here would mean the counter is not connected, which is the
  // reading CLAUDE.md's broken-instrument chapter is about.
  check(dut.feed_wait_cycles_o > 0,
        "case9: `feed_wait_cycles_o` moved -- the rate instrument is live", 1,
        dut.feed_wait_cycles_o > 0 ? 1 : 0);

  // ---- 10. the credit holds ----------------------------------------------
  check(dut.ret_overflow_o == 0,
        "case10: `ret_overflow_o` stays 0 -- see the committed mutant", 0,
        dut.ret_overflow_o);

  // A sanity floor on the whole run: the DDR side was actually exercised.
  check(g_ddr.requests() > 20, "the played bridge served the run", 1,
        g_ddr.requests() > 20 ? 1 : 0);
  check(dut.lm_refused_sorted_o == 1, "the loom's NOT_SORTED fired exactly once",
        1, dut.lm_refused_sorted_o);
  (void)kLoomFraming;

  const int rc = zhao::report_and_exit("geom_loomfeed_directed");
  zhao::exit_hard(rc);
}
