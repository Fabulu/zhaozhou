// field_earth_adapter_cycle_census.cpp -- HOW MANY CLOCKS DOES ONE LANE COST?
// Measured, not argued, and COMMITTED so the number is reproducible.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// Packet EARTHRAM turned this adapter's 5,120-bit uniform payload from a
// flip-flop array read COMBINATIONALLY through a dynamic index into a
// synchronous-read RAM. A synchronous read hands its data over one cycle after
// the address, so the owner's instruction allowed for exactly that cost --
// "add one state to an ~80-100-cycle field run".
//
// THE CLAIM UNDER TEST IS THAT IT COST ZERO. That is a surprising claim and
// therefore the one to measure rather than assert: `req_valid_o` is
// `(state == E_REQ)`, and `state` leaves `E_IDLE` on the very edge that loads
// the bank's read register, so the payload lands exactly as the request is
// first offered. CLAUDE.md's own law -- "a measurement that did not move after
// a change that must have moved it is the tell" -- cuts both ways: a latency
// that did NOT move across a change to the read timing is exactly the kind of
// result that needs a probe rather than a paragraph.
//
// `CLAUDE.md` says commit the probe, after a ground-contact measurement was
// taken once with a throwaway script and its numbers became unreproducible.
// This is that rule applied to a cycle count.
//
// ---------------------------------------------------------------------------
// WHAT IT MEASURES, AND WHY THESE TWO NUMBERS
// ---------------------------------------------------------------------------
//   * ACCEPT-TO-OFFER: clocks from the consumer's vertex accept (`vtx_fire_i`)
//     until `req_valid_o` first rises. This is the number the change could
//     have moved, because it is the span that contains the bank read.
//   * ACCEPT-TO-ANSWER: clocks from the same accept until `ans_valid_o` rises,
//     against an engine model with a FIXED, declared response latency. This is
//     the per-lane figure the adapter's header's "order 80-100 clocks" refers
//     to, and it is reported so a future latency regression has a baseline.
//
// The engine model's latency is a NAMED CONSTANT below, not a magic 3, because
// the total is only meaningful relative to it -- the adapter owns the
// difference, not the sum.
//
// ---------------------------------------------------------------------------
// HOW THE BEFORE/AFTER PAIR WAS TAKEN
// ---------------------------------------------------------------------------
// This same source was verilated against BOTH revisions of
// `fpga/rtl/field/zhao_field_earth_adapter.sv` -- the flop-bank version at the
// packet's base commit and the RAM version -- with nothing else changed. It is
// not a copy of the RTL and cannot go stale in the mutant-copy sense; it reads
// whichever adapter it is built against. The recorded pair is in the packet's
// FINDINGS.
//
// THE ASSERTION IS THE CORRECT BEHAVIOUR, NOT THE BUG (CLAUDE.md): it asserts
// the latency the design is SUPPOSED to have. If a future change adds a state,
// this test goes red and names the number, which is the point.
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "Vzhao_field_earth_adapter.h"
#include "verilated.h"

using Dut = Vzhao_field_earth_adapter;

// Verilator's runtime references this; `zhao_harness` normally supplies it.
// Defined here so this census carries NO link dependency, which is what lets
// it be built a second time against a different revision of the RTL.
double sc_time_stamp() { return 0.0; }

// The engine model's response latency, in clocks after it accepts a request.
// Named because ACCEPT-TO-ANSWER is only interpretable against it.
static const int kEngineLatency = 3;

static int g_fail = 0;

static void check_eq(const char* what, long expect, long got) {
  const bool ok = (expect == got);
  if (!ok) ++g_fail;
  std::printf("%-6s %-58s expect=%ld got=%ld\n", ok ? "ok" : "FAIL", what, expect, got);
}

static void tick(Dut& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
}

static void reset(Dut& d) {
  d.clk = 0;
  d.rst_n = 0;
  d.tick_i = 0;
  d.rec_valid_i = 0;
  d.rec_start_tick_i = 0;
  d.rec_duration_i = 0;
  d.rec_last_i = 0;
  d.patch_open_i = 0;
  d.add_fire_i = 0;
  d.add_obj_i = 0;
  d.add_resident_i = 0;
  d.vtx_fire_i = 0;
  d.vtx_wx_i = 0;
  d.vtx_wz_i = 0;
  d.lanes_i = 0;
  d.lane_covers_i = 0;
  d.req_ready_i = 0;
  d.resp_valid_i = 0;
  d.resp_status_i = 0;
  d.resp_present_i = 0;
  d.ans_ready_i = 0;
  for (int i = 0; i < 8; ++i) d.rec_params_i[i] = 0;
  for (int i = 0; i < 7; ++i) d.resp_out_i[i] = 0;
  for (int i = 0; i < 4; ++i) tick(d);
  d.rst_n = 1;
  tick(d);
}

// One uniform record, taken on the joined handshake, then walked through the
// intake's divide. Mirrors `offer_record` in the directed test.
static void offer_record(Dut& d, uint32_t start_tick, uint32_t duration) {
  d.rec_valid_i = 1;
  d.rec_start_tick_i = start_tick;
  d.rec_duration_i = duration;
  d.rec_last_i = 1;
  int guard = 0;
  while (!d.rec_ready_o && guard < 100) {
    tick(d);
    ++guard;
  }
  tick(d);  // the take
  d.rec_valid_i = 0;
  d.rec_last_i = 0;
  // The intake walks I_DIV (seventeen steps) then I_WR before it offers again.
  guard = 0;
  while (!d.rec_ready_o && guard < 100) {
    tick(d);
    ++guard;
  }
}

// The field list's replay, which is where the object index and residency land.
static void replay_one(Dut& d, int obj, bool resident) {
  d.patch_open_i = 1;
  tick(d);
  d.patch_open_i = 0;
  d.add_fire_i = 1;
  d.add_obj_i = (uint8_t)obj;
  d.add_resident_i = resident ? 1 : 0;
  tick(d);
  d.add_fire_i = 0;
  tick(d);
}

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut d;

  reset(d);
  d.tick_i = 1000;
  offer_record(d, 0, 100);   // begun, non-zero duration
  replay_one(d, 2, true);    // resident, object 2

  // The consumer's vertex accept. Cycle counting starts on the clock AFTER it,
  // exactly as `zhao_terrain_patch` raises its `busy` one clock after latching.
  d.vtx_fire_i = 1;
  d.vtx_wx_i = 0x0001'2345;
  d.vtx_wz_i = 0x0006'7890;
  d.lanes_i = 1;
  tick(d);
  d.vtx_fire_i = 0;
  d.ans_ready_i = 1;
  d.lane_covers_i = 1;

  int accept_to_offer = 0;
  while (!d.req_valid_o && accept_to_offer < 200) {
    tick(d);
    ++accept_to_offer;
  }
  if (!d.req_valid_o) {
    std::printf("FAIL   no request was ever offered\n");
    std::fflush(nullptr);
    std::_Exit(1);
  }

  // THE PAYLOAD MUST BE VALID ON THE CYCLE THE REQUEST IS FIRST OFFERED.
  // This is the half of the claim that makes the zero-cost half honest: a
  // latency of zero is worthless if the words are not there yet. `age` is the
  // saturated span -- tick 1000, start 0, duration 100 -- so 100 exactly.
  check_eq("payload age is valid as req_valid_o rises", 100, (long)d.req_in_o[2]);
  check_eq("payload x is valid as req_valid_o rises", 0x00012345,
           (long)(int32_t)d.req_in_o[0]);
  check_eq("req_slot_o is valid as req_valid_o rises", 2, (long)d.req_slot_o);

  int total = accept_to_offer;
  d.req_ready_i = 1;
  tick(d);
  ++total;
  d.req_ready_i = 0;
  for (int k = 0; k < kEngineLatency; ++k) {
    tick(d);
    ++total;
  }
  d.resp_out_i[0] = 0x0000'4321u;
  d.resp_status_i = 0x00;
  d.resp_present_i = 0xF;
  d.resp_valid_i = 1;
  tick(d);
  ++total;
  d.resp_valid_i = 0;
  int guard = 0;
  while (!d.ans_valid_o && guard < 200) {
    tick(d);
    ++total;
    ++guard;
  }
  check_eq("the answer arrives", 1, (long)d.ans_valid_o);
  check_eq("and carries the engine's out-lane 0", 0x4321, (long)d.height_o);

  std::printf("\nCYCLE CENSUS (one covered, begun, resident lane)\n");
  std::printf("  engine model response latency : %d clocks (declared)\n", kEngineLatency);
  std::printf("  ACCEPT-TO-OFFER              : %d clocks\n", accept_to_offer);
  std::printf("  ACCEPT-TO-ANSWER             : %d clocks\n", total);

  // ACCEPT-TO-OFFER IS ONE CLOCK, AND THAT IS THE MEASUREMENT THE PACKET OWES.
  // `vtx_live` is a register, so the first lane of a vertex starts the cycle
  // after its accept -- that single clock is the pre-existing `E_IDLE` decision
  // cycle, and the synchronous bank read happens INSIDE it. The RAM did not add
  // a state: it reused the one that was already there.
  check_eq("ACCEPT-TO-OFFER is 1 clock (the bank read is free)", 1, accept_to_offer);

  // The shadow guard must be silent across a correct single-lane vertex --
  // including the new arm (c), which differences the address the payload was
  // read at against the lane the module believes it is serving.
  check_eq("lane_desync_o silent across a correct run", 0, (long)d.lane_desync_o);

  std::printf("\n%s: %d failure(s)\n", g_fail ? "FAILED" : "PASSED", g_fail);
  // `zhao::exit_hard`'s semantics -- fflush(nullptr) then std::_Exit -- spelled
  // out rather than included. A verilated main must never `return` (static
  // destructors run after the model is gone), and this file must build TWICE,
  // once outside CMake against the previous revision of the RTL for the
  // before/after pair, so it carries no link dependency on `zhao_harness`.
  std::fflush(nullptr);
  std::_Exit(g_fail ? 1 : 0);
}
