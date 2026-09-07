// raster_ticketq_rh_directed.cpp
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS SEPARATELY FROM raster_rcp24_v3_directed
// ---------------------------------------------------------------------------
// `zhao_raster_ticketq_rh` is exercised through rcp24 V3's directed bench, which
// passes 52 checks with it in place. That is coverage of the CONSUMER, not of
// the wrapper's own contract, and the two are different questions:
//
//   * rcp24's DONE queue is never deliberately saturated, so an off-by-two in
//     `full_o` -- the exact defect S5.3 names, "a body of 64 plus two heads must
//     not silently advertise 66 logical owner credits" -- would not be reached
//     by it. The accounting here is freshly written and deserves its own test.
//
//   * the whole justification for a head/spare pair rather than a single
//     registered head is a RATE. A wrapper that delivered 0.5 pops per clock
//     would satisfy every correctness check rcp24 makes, and would silently cost
//     the throughput gate the moment traffic saturated.
//
// So: capacity, the empty/full round trip, and the sustained rate, each checked
// directly and each able to fail.
#include "Vzhao_raster_ticketq_rh.h"

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

void tick(Vzhao_raster_ticketq_rh* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

}  // namespace

int main() {
  Vzhao_raster_ticketq_rh* d = new Vzhao_raster_ticketq_rh;

  // The shape rcp24 V3 instantiates: W = CW, D = NCTX = 16.
  const int kDepth = 16;

  d->rst_n = 0;
  d->push_i = 0;
  d->din_i = 0;
  d->pop_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);
  d->eval();

  zhao::check(d->empty_o == 1, "a reset queue is empty", 1, d->empty_o);
  zhao::check(d->full_o == 0, "and not full", 0, d->full_o);
  zhao::check(d->err_o == 0, "and reports no error", 0, d->err_o);

  // ---- one item makes a visible head, and it is REGISTERED ----------------
  // The whole point of the wrapper: the head must appear at a clock edge, not
  // combinationally out of the body array.
  d->push_i = 1;
  d->din_i = 0xA5;
  d->eval();
  zhao::check(d->empty_o == 1,
              "a push in the same cycle does not make a head appear -- the "
              "boundary is registered, which is the entire point",
              1, d->empty_o);
  tick(d);
  d->push_i = 0;

  int cycles_to_head = 0;
  for (int i = 0; i < 8 && !d->empty_o == 0; ++i) {
    d->eval();
    if (!d->empty_o) break;
    tick(d);
    ++cycles_to_head;
  }
  d->eval();
  zhao::check(d->empty_o == 0, "the pushed item does reach the head", 0,
              d->empty_o);
  zhao::check(d->dout_o == 0xA5, "with its payload intact", 0xA5, d->dout_o);

  // Drain it.
  d->pop_i = 1;
  d->eval();
  tick(d);
  d->pop_i = 0;
  d->eval();
  zhao::check(d->empty_o == 1, "and the queue returns to empty after one pop",
              1, d->empty_o);

  // ---- S5.3's CAPACITY CONTRACT -------------------------------------------
  // The body holds D. The heads hold up to two more. If `full_o` came from the
  // body alone the wrapper would advertise D+2 credits, and the consumer would
  // hand it two tickets it cannot hold. Count what is actually accepted.
  int accepted = 0;
  int fill_cycles = 0;
  for (int i = 0; i < 200; ++i) {
    d->push_i = !d->full_o;
    d->din_i = static_cast<uint8_t>(0x10 + (i & 0x0F));
    d->pop_i = 0;
    d->eval();
    if (d->push_i) ++accepted;
    tick(d);
    ++fill_cycles;
    d->eval();
    if (d->full_o) break;
  }

  zhao::check(fill_cycles < 200,
              "full_o actually asserts -- the fill ended on the flag and not on "
              "the loop bound (a full_o stuck low passes every other check here)",
              1, fill_cycles < 200 ? 1 : 0);
  zhao::check(accepted == kDepth,
              "the queue accepts EXACTLY D tickets -- not D+2, which is what a "
              "body-only full_o would advertise once the heads are counted",
              kDepth, accepted);
  zhao::check(d->err_o == 0, "and no error was latched while filling", 0,
              d->err_o);

  // Hold at full: the flag must not glitch low and re-open credit.
  int full_held = 0;
  for (int i = 0; i < 8; ++i) {
    d->push_i = 0;
    d->pop_i = 0;
    d->eval();
    if (d->full_o) ++full_held;
    tick(d);
  }
  zhao::check(full_held == 8,
              "full_o stays asserted for eight idle cycles at capacity", 8,
              full_held);

  // ---- the round trip back to empty ---------------------------------------
  int drained = 0;
  for (int i = 0; i < 300 && d->empty_o == 0; ++i) {
    d->push_i = 0;
    d->pop_i = 1;
    d->eval();
    if (!d->empty_o) ++drained;
    tick(d);
    d->eval();
  }
  d->pop_i = 0;
  d->eval();
  zhao::check(drained == kDepth,
              "exactly as many tickets come back out as went in", kDepth,
              drained);
  zhao::check(d->empty_o == 1, "and the queue returns to empty", 1, d->empty_o);
  zhao::check(d->full_o == 0, "with full_o released", 0, d->full_o);
  zhao::check(d->err_o == 0, "and still no error", 0, d->err_o);

  // ---- THE RATE, which is the reason for the spare slot --------------------
  // Continuous supply and continuous demand. After warmup every cycle without a
  // head is a bubble, and a single-head design would bubble every other cycle.
  {
    const int kWarmup = 8;
    const int kMeasure = 300;
    int bubbles = 0;
    int pops = 0;
    for (int i = 0; i < kWarmup + kMeasure; ++i) {
      d->push_i = !d->full_o;
      d->din_i = static_cast<uint8_t>(0x20 + (i & 0x0F));
      // POP ONLY WHEN THERE IS A HEAD. The first version of this loop drove
      // pop_i high unconditionally and the wrapper latched err_o during warmup
      // -- correctly: `pop_i && empty_o` is a consumer protocol violation, and
      // both this wrapper and the body it wraps flag it. The test was wrong and
      // the detector was right, which is the outcome to want from a detector.
      d->pop_i = !d->empty_o;
      d->eval();
      if (i >= kWarmup) {
        if (d->empty_o) {
          ++bubbles;
        } else {
          ++pops;
        }
      }
      tick(d);
    }
    d->push_i = 0;
    d->pop_i = 0;
    d->eval();

    zhao::check(pops > 0, "the sustained-rate window actually popped", 1,
                pops > 0 ? 1 : 0);
    zhao::check(bubbles == 0,
                "one pop per clock with continuous supply and demand -- no "
                "bubble in 300 cycles, which is what the SPARE slot buys and a "
                "single registered head would not",
                0, bubbles);
    zhao::check(d->err_o == 0, "and the sustained run latched no error", 0,
                d->err_o);
  }

  // ---- S16.3: READY DROPPING JUST AFTER A READ LAUNCHES ---------------------
  // S16.3 lists the tests this seam needs: "distinct U/V, shuffled completions,
  // zero/nonzero interleaving, slot reuse, and READY DROPPING JUST AFTER A READ
  // LAUNCHES." The last is the one that stresses the free-on-transfer law
  // hardest, because it is exactly when a body read is in flight toward a head
  // that the consumer stops taking.
  //
  // The head/spare reservation exists for this: a read may only launch if a
  // destination is reserved for its return, so a pop that disappears mid-flight
  // must neither lose the returning token nor let it land twice.
  //
  // Driven as an adversarial pop pattern rather than a fixed one, and checked on
  // the property that matters: every token pushed comes out EXACTLY ONCE and IN
  // ORDER. A lost token and a duplicated token both fail this; a merely slow
  // queue does not.
  {
    // Start from empty.
    for (int i = 0; i < 300 && !d->empty_o; ++i) {
      d->push_i = 0;
      d->pop_i = 1;
      d->eval();
      tick(d);
      d->eval();
    }
    d->pop_i = 0;
    d->eval();

    uint32_t rng = 0xBEEF01u;
    int next_push = 0;   // payload counter, so order is checkable
    int next_pop = 0;    // what we expect out
    int pushed = 0, popped = 0, order_errors = 0;

    for (int i = 0; i < 4000; ++i) {
      // Supply: offer most cycles, so reads are frequently in flight.
      rng = rng * 1664525u + 1013904223u;
      const bool offer = ((rng >> 17) & 7u) != 0u;
      // Demand: drop ready in short unpredictable bursts -- the "just after a
      // read launches" case arrives by construction rather than by timing luck.
      const bool take = ((rng >> 23) & 3u) != 0u;

      d->push_i = (offer && !d->full_o) ? 1 : 0;
      d->din_i = static_cast<uint8_t>(next_push & 0xFF);
      d->pop_i = (take && !d->empty_o) ? 1 : 0;
      d->eval();

      if (d->pop_i) {
        if (d->dout_o != static_cast<uint8_t>(next_pop & 0xFF)) ++order_errors;
        ++next_pop;
        ++popped;
      }
      if (d->push_i) {
        ++next_push;
        ++pushed;
      }
      tick(d);
      d->eval();
    }

    // Drain whatever is still held so the totals can be compared.
    for (int i = 0; i < 400 && !d->empty_o; ++i) {
      d->push_i = 0;
      d->pop_i = 1;
      d->eval();
      if (d->dout_o != static_cast<uint8_t>(next_pop & 0xFF)) ++order_errors;
      ++next_pop;
      ++popped;
      tick(d);
      d->eval();
    }
    d->pop_i = 0;
    d->eval();

    zhao::check(pushed > 1000,
                "the adversarial pattern actually moved traffic (not vacuous)",
                1, pushed > 1000 ? 1 : 0);
    zhao::check(popped == pushed,
                "every token pushed comes out EXACTLY ONCE under ready dropping "
                "-- none lost in a launched read, none delivered twice",
                pushed, popped);
    zhao::check(order_errors == 0,
                "and in order: the head/spare pair never reorders a token whose "
                "read was in flight when ready fell",
                0, order_errors);
    zhao::check(d->err_o == 0,
                "and the queue latched no protocol error throughout", 0,
                d->err_o);
  }

  const int rc = zhao::report_and_exit("raster_ticketq_rh_directed");
  delete d;
  zhao::exit_hard(rc);
}
