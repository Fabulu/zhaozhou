// texture_v3rq_directed.cpp -- the ready queue's occupancy must never lie.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `zhao_texture_v3rq` had a LINT lane and NO directed test at all. The owner's
// control-fabric recovery architecture (2026-09-07, §5) names a defect in it
// by hand:
//
//   > zhao_texture_v3rq.occ_o counts body entries plus its two visible head
//   > registers but omits ld_q, the outstanding synchronous body-read return.
//   > Consequently it can report zero while it still owns a ticket.
//
// Confirmed in the source, and the block's OWN internal accounting already
// gets it right -- `reserved_c = h_v_q + s_v_q + ld_q` includes the pending
// read; only the exported `occ_o` omits it. The port's own comment states the
// exact trap it then falls into:
//
//   > TOTAL tickets held, body plus head registers. The drain/quiescence test
//   > needs "this queue holds nothing", and a body-only occupancy answers a
//   > different question while looking like the right one.
//
// THE MECHANISM, which is why one cycle is enough to lose a ticket:
// `rp_q` advances when the read is ISSUED (`if (ld_c) rp_q <= rp_q + 1`), so
// the entry leaves `body_occ_c` on that edge. It only arrives in a head
// register on the NEXT edge, when `ld_q` is set and `rd_data_c` is placed. In
// between it is counted in neither term.
//
// §5.2 asks for exactly this: "include ld_q in occupancy and add the
// three-edge counterexample to the queue's directed test".
//
//   edge 1  one push accepted        body 1, ld 0, heads empty
//   edge 2  read issued, rp advances body 0, ld 1, heads empty  <-- occ_o == 0
//   edge 3  data lands in the head   body 0, ld 0, head valid
//
// The item is owned throughout. `occ_o`'s only consumer is `quiet_c` in
// `zhao_texture_v3own`, the generation-wrap drain, so a queue that announces
// it holds nothing while holding something is announcing it into the one
// place that decides whether the owner namespace may wrap.
//
// This asserts the INVARIANT rather than the three named states: between an
// accepted push and its pop, occupancy is never zero. That is the property the
// drain depends on, and it does not require the test to know which internal
// cycle is the bad one.
#include "Vzhao_texture_v3rq.h"

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

void tick(Vzhao_texture_v3rq* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

}  // namespace

int main() {
  Vzhao_texture_v3rq* d = new Vzhao_texture_v3rq;

  d->rst_n = 0;
  d->wr_en_i = 0;
  d->wr_data_i = 0;
  d->pop_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);
  d->eval();

  zhao::check(d->occ_o == 0, "an empty queue reports zero occupancy", 0, d->occ_o);
  zhao::check(d->valid_o == 0, "and shows no head", 0, d->valid_o);

  // ---- the counterexample -------------------------------------------------
  d->wr_en_i = 1;
  d->wr_data_i = 0x2A;
  tick(d);
  d->wr_en_i = 0;

  int zero_cycles = 0;
  int cycles_to_head = 0;
  for (int i = 0; i < 8; ++i) {
    d->eval();
    if (d->valid_o) break;
    if (d->occ_o == 0) ++zero_cycles;
    ++cycles_to_head;
    tick(d);
  }
  d->eval();

  zhao::check(d->valid_o == 1, "the pushed item reaches the visible head", 1,
              d->valid_o);
  zhao::check(d->data_o == 0x2A, "with its payload intact", 0x2A, d->data_o);
  zhao::check(cycles_to_head >= 1,
              "and it took at least one cycle to get there, so the window this "
              "test inspects genuinely exists",
              1, cycles_to_head >= 1 ? 1 : 0);

  // THE DEFECT. Before the ld_q repair this counts one or more cycles in which
  // the queue announced it held nothing while the ticket was in flight between
  // the body and the head.
  zhao::check(zero_cycles == 0,
              "occupancy is NEVER zero between an accepted push and its head "
              "arrival -- a pending synchronous read still owns the ticket",
              0, zero_cycles);

  zhao::check(d->occ_o >= 1, "and reads at least one once the head is valid", 1,
              d->occ_o >= 1 ? 1 : 0);

  // ---- the same invariant under a sustained stream -------------------------
  // A single push is the easiest case to get right by accident. Push and pop
  // continuously and assert the same property every cycle, so a repair that
  // only special-cases the empty queue does not pass.
  int stream_zero_while_owned = 0;
  int pushed = 0;
  int popped = 0;
  for (int i = 0; i < 200; ++i) {
    d->wr_en_i = (i < 100) ? 1 : 0;
    d->wr_data_i = static_cast<uint16_t>(i & 0x3F);
    d->eval();
    if (d->wr_en_i && !d->full_o) ++pushed;
    d->pop_i = d->valid_o;
    d->eval();
    if (d->valid_o && d->pop_i) ++popped;
    if ((pushed - popped) > 0 && d->occ_o == 0) ++stream_zero_while_owned;
    tick(d);
  }
  d->pop_i = 0;
  d->wr_en_i = 0;
  d->eval();

  zhao::check(pushed > 50, "the stream actually pushed work (not a vacuous check)",
              1, pushed > 50 ? 1 : 0);
  zhao::check(popped > 50, "and actually popped it", 1, popped > 50 ? 1 : 0);
  zhao::check(stream_zero_while_owned == 0,
              "occupancy never reads zero while the queue owns tickets, under a "
              "sustained push/pop stream",
              0, stream_zero_while_owned);

  // ---- V3.1 5.7: the empty/full transition, and the CAPACITY contract -------
  // 5.7 asks this suite to check "empty/full transitions" and 5.3 states the
  // failure mode in the queue's own terms:
  //
  //   "For the ready queues, CAPACITY = 64 means 64 logical owner tickets
  //    INCLUDING all heads and pending reads. A body of 64 plus two heads must
  //    not silently advertise 66 logical owner credits."
  //
  // Nothing tested full_o before this. That is the output the registered
  // logical credit exists to drive, so an off-by-two there would have shipped
  // as extra admission credit -- two owners the island does not have -- and
  // every existing check would still have passed.

  // Drain to a known-empty start; a fill test that begins with residue counts
  // the residue.
  for (int i = 0; i < 300 && !d->owned_empty_o; ++i) {
    d->wr_en_i = 0;
    d->pop_i = d->valid_o;
    d->eval();
    tick(d);
  }
  d->pop_i = 0;
  d->eval();
  zhao::check(d->owned_empty_o == 1,
              "the queue drains to owned-empty before the capacity fill", 1,
              d->owned_empty_o);
  zhao::check(d->occ_o == 0, "and reports zero occupancy there", 0, d->occ_o);

  // Fill without popping and count what is actually ACCEPTED.
  const int kCapacity = 64;  // the module's CAPACITY = DEPTH default
  int accepted = 0;
  int fill_cycles = 0;
  for (int i = 0; i < 200; ++i) {
    d->wr_en_i = 1;
    d->wr_data_i = static_cast<uint16_t>(0x100 + (i & 0x3F));
    d->pop_i = 0;
    d->eval();
    if (!d->full_o) ++accepted;
    tick(d);
    ++fill_cycles;
    d->eval();
    if (d->full_o) break;
  }

  zhao::check(fill_cycles < 200,
              "full_o actually asserts -- the fill terminated on the flag and "
              "not on the loop bound (a full_o stuck low would otherwise pass "
              "every other check here)",
              1, fill_cycles < 200 ? 1 : 0);
  zhao::check(accepted == kCapacity,
              "the queue accepts EXACTLY CAPACITY tickets -- not 65, and not "
              "the body-plus-two-heads 66 that 5.3 names",
              kCapacity, accepted);
  zhao::check(d->occ_o == kCapacity,
              "and occupancy at full equals capacity", kCapacity, d->occ_o);
  zhao::check(d->owned_empty_o == 0, "a full queue is not owned-empty", 0,
              d->owned_empty_o);

  // FULL IS A PRODUCER CONTRACT, NOT A SHOCK ABSORBER, and the first version of
  // this block got that wrong. It drove wr_en_i high into a full queue to prove
  // the write was refused, and the module's own assertion stopped the run:
  //
  //   a_rq_no_write_when_full : assert (!(wr_en_i && full_o));
  //
  // So writing while full is a DESIGN ERROR the queue is entitled to assume
  // never happens, not a case it silently absorbs -- and the assertion firing
  // is the RTL stating its interface. (It also demonstrates that assertion
  // fires, which is the only way to know a detector works.)
  //
  // The property worth testing is therefore the one the island depends on: with
  // the producer correctly gated, full_o STAYS asserted and occupancy holds at
  // capacity. A full_o that glitched low for one cycle would let the real
  // admission path push a 65th ticket, and nothing else here would notice.
  int full_held = 0;
  int occ_stable = 0;
  for (int i = 0; i < 8; ++i) {
    d->wr_en_i = !d->full_o;  // the contract the island's producer obeys
    d->wr_data_i = static_cast<uint16_t>(0x3FF);
    d->pop_i = 0;
    d->eval();
    if (d->full_o) ++full_held;
    if (d->occ_o == kCapacity) ++occ_stable;
    tick(d);
  }
  d->wr_en_i = 0;
  d->eval();
  zhao::check(full_held == 8,
              "full_o stays asserted for eight idle cycles at capacity -- it "
              "never glitches low and re-opens admission", 8, full_held);
  zhao::check(occ_stable == 8,
              "and occupancy holds at exactly capacity throughout", 8,
              occ_stable);

  // And back down: the other half of the transition.
  int drained = 0;
  for (int i = 0; i < 300 && !d->owned_empty_o; ++i) {
    d->wr_en_i = 0;
    d->pop_i = d->valid_o;
    d->eval();
    if (d->valid_o && d->pop_i) ++drained;
    tick(d);
  }
  d->pop_i = 0;
  d->eval();
  zhao::check(drained == kCapacity,
              "exactly as many tickets come back out as went in", kCapacity,
              drained);
  zhao::check(d->owned_empty_o == 1, "and the queue returns to owned-empty", 1,
              d->owned_empty_o);
  zhao::check(d->full_o == 0, "with full_o released", 0, d->full_o);

  // ---- V3.1 5.4: one external pop per clock, after warmup --------------------
  // "The two-head organization can sustain one external pop per clock after
  //  warmup with continuous supply. The included model checks that property."
  //
  // Nothing here checked it. Every other check in this file is a CORRECTNESS
  // check, and a queue that delivered 0.9 pops per clock would satisfy all of
  // them -- the two-head-plus-pending-read structure exists precisely to hide
  // the body read's latency, so its whole justification is a rate.
  //
  // Continuous supply AND continuous demand: wr_en_i high whenever not full,
  // pop_i high always. After a warmup allowance, every cycle that shows no head
  // is a BUBBLE and the design says there should be none.
  {
    // Refill from the drained state the previous block left behind.
    const int kWarmup = 16;
    const int kMeasure = 400;
    int bubbles = 0;
    int pops = 0;
    for (int i = 0; i < kWarmup + kMeasure; ++i) {
      d->wr_en_i = !d->full_o;
      d->wr_data_i = static_cast<uint16_t>(0x55 + (i & 0x1F));
      d->pop_i = 1;
      d->eval();
      if (i >= kWarmup) {
        if (d->valid_o) {
          ++pops;
        } else {
          ++bubbles;
        }
      }
      tick(d);
    }
    d->wr_en_i = 0;
    d->pop_i = 0;
    d->eval();

    zhao::check(pops > 0, "the sustained-rate window actually popped", 1,
                pops > 0 ? 1 : 0);
    zhao::check(bubbles == 0,
                "5.4: with continuous supply and continuous demand the queue "
                "sustains ONE POP PER CLOCK after warmup -- no bubble in 400 "
                "cycles",
                0, bubbles);
    zhao::check(pops == kMeasure,
                "and every measured cycle delivered a head", kMeasure, pops);
  }

  const int rc = zhao::report_and_exit("texture_v3rq_directed");
  delete d;
  zhao::exit_hard(rc);
}
