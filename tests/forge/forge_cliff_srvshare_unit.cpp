// forge_cliff_srvshare_unit.cpp -- `zhao_forge_cliff_srvshare` at its own port,
// and in particular THE POSITIVE CONTROL FOR `poison1_o`.
//
// WHY THIS FILE EXISTS, WHICH IS THE INTERESTING PART.
//
// `forge_cliff_chain` pulls the staged patch out from under a live window and
// the poison counter stays at ZERO. That is not a broken detector; it is the
// composition being correct. `zhao_forge_cliff_feed` gates its request on
// `serve_valid_i` (`patch_lost_c`), so it never ASKS while the patch is gone,
// so no refusal is ever in flight, so the detector has nothing to see. The
// counter's silence in the chain is therefore evidence about the FEED, and it
// is not evidence about the detector -- which is exactly the situation
// CLAUDE.md names: "a gate that cannot reach the state is not evidence about
// the state", and "a detector reading zero is a claim, and it is the claim to
// check hardest".
//
// The state IS reachable with legal stimulus at the sharer's own port, because
// `c1_req_i` and `srv_rsp_payload_i` are both inputs there: a client that does
// not guard is a legal client. So no committed mutant is needed -- this is the
// "offer an illegal key" category, not the "break the guard" one -- and the
// demonstration is a driver rather than a broken copy of the block.
//
// Both polarities are asserted. A positive control that is never paired with a
// negative one proves the counter can increment, not that it increments for
// the right reason.
//
// What each lane would catch:
//   1. The incumbent is never denied and never delayed. Red on: any arbitration
//      creeping into client 0's path -- the one thing this block promises
//      TERRAIN.TESS.
//   2. The incumbent's answer is the served answer, unmodified. Red on: a
//      response routed to the wrong client.
//   3. A granted client-1 read is answered exactly one clock later with the
//      served payload. Red on: a routing tag off by a cycle.
//   4. POSITIVE CONTROL: the refusal encoding returned in flight FIRES
//      `poison1_o`.
//   5. NEGATIVE CONTROL: the same encoding returned when NOTHING is in flight
//      does NOT fire it, and neither does a legal value in flight. Red on: a
//      counter that watches the bus instead of the transaction.

#include "Vzhao_forge_cliff_srvshare.h"

#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>

namespace {

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

using Dut = Vzhao_forge_cliff_srvshare;

void idle(Dut& d) {
  d.o0_req_i = 0;
  d.o0_req_payload_i = 0;
  d.c1_req_i = 0;
  d.c1_req_payload_i = 0;
  d.srv_rsp_payload_i = 0;
}

void reset_dut(Dut& d) {
  d.rst_n = 0;
  d.poison_value_i = 3;  // the serve block's refusal encoding for cell state
  idle(d);
  d.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
  zhao::tick(d);
}

}  // namespace

int main() {
  // TWO DELIBERATE STARTUP PROBES, kept rather than removed. This binary hung
  // three times at ~0 CPU with no output at all, and "hung before main" and
  // "hung at exit-time static destruction" look IDENTICAL from outside when
  // stdout is unflushed -- zhao_sim.hpp documents the second as systemic to
  // every Verilated exe on this toolchain. These two lines cost nothing and
  // tell the next reader which one it is.
  std::printf("srvshare: main entered\n");
  std::fflush(nullptr);
  Dut d;
  std::printf("srvshare: model constructed\n");
  std::fflush(nullptr);
  reset_dut(d);

  // ---- lane 1 + 2: the incumbent wins, unconditionally ---------------------
  {
    d.o0_req_i = 1;
    d.o0_req_payload_i = 0x155;
    d.c1_req_i = 1;
    d.c1_req_payload_i = 0x0AA;
    d.srv_rsp_payload_i = 1;
    d.eval();
    check(d.srv_req_o == 1, "lane 1: the served port is asked");
    check(d.srv_req_payload_o == 0x155, "lane 1: the INCUMBENT's address is the one served");
    check(d.c1_grant_o == 0, "lane 1: client 1 is not granted while the incumbent asks");
    check(d.o0_rsp_payload_o == d.srv_rsp_payload_i,
          "lane 2: the incumbent's answer is the served answer, unmodified");
    zhao::tick(d);
    check(d.denied1_o == 1, "lane 1: the denial was counted");
    check(d.grants0_o == 1, "lane 1: the incumbent's ask was counted");
    idle(d);
    zhao::tick(d);
  }

  // ---- lane 3: a granted client-1 read is answered one clock later ---------
  {
    const uint32_t g0 = d.grants1_o;
    d.c1_req_i = 1;
    d.c1_req_payload_i = 0x0AA;
    d.eval();
    check(d.c1_grant_o == 1, "lane 3: client 1 is granted when the incumbent is idle");
    check(d.srv_req_payload_o == 0x0AA, "lane 3: client 1's address reaches the served port");
    check(d.c1_rsp_valid_o == 0, "lane 3: no answer on the request cycle");
    zhao::tick(d);
    d.c1_req_i = 0;
    d.srv_rsp_payload_i = 2;
    d.eval();
    check(d.c1_rsp_valid_o == 1, "lane 3: the answer arrives exactly one clock later");
    check(d.c1_rsp_payload_o == 2, "lane 3: and it is the served payload");
    zhao::tick(d);
    check(d.grants1_o == g0 + 1, "lane 3: the grant was counted");
    idle(d);
    zhao::tick(d);
  }

  // ---- lane 5 (negative, part A): the refusal value on an IDLE bus ---------
  {
    const uint32_t p0 = d.poison1_o;
    for (int i = 0; i < 8; ++i) {
      idle(d);
      d.srv_rsp_payload_i = 3;  // the refusal encoding, with nothing in flight
      d.eval();
      zhao::tick(d);
    }
    check(d.poison1_o == p0,
          "lane 5A: the refusal value with nothing in flight does NOT fire poison");
  }

  // ---- lane 5 (negative, part B): a LEGAL value in flight ------------------
  {
    const uint32_t p0 = d.poison1_o;
    d.c1_req_i = 1;
    d.c1_req_payload_i = 0x011;
    d.eval();
    check(d.c1_grant_o == 1, "lane 5B: granted");
    zhao::tick(d);
    d.c1_req_i = 0;
    d.srv_rsp_payload_i = 0;  // SOLID: a perfectly good answer
    d.eval();
    check(d.c1_rsp_valid_o == 1, "lane 5B: answered");
    zhao::tick(d);
    check(d.poison1_o == p0, "lane 5B: a legal answer in flight does NOT fire poison");
    idle(d);
    zhao::tick(d);
  }

  // ---- lane 4: THE POSITIVE CONTROL ----------------------------------------
  // A client that does not guard on residency asks while no patch is served;
  // the serve block answers with its refusal encoding; the detector must see
  // it. This is the fault `zhao_forge_cliff_feed` prevents and the reason the
  // composed chain reads zero.
  {
    const uint32_t p0 = d.poison1_o;
    d.c1_req_i = 1;
    d.c1_req_payload_i = 0x0FF;
    d.eval();
    check(d.c1_grant_o == 1, "lane 4: granted");
    zhao::tick(d);
    d.c1_req_i = 0;
    d.srv_rsp_payload_i = 3;  // no patch is being served
    d.eval();
    check(d.c1_rsp_valid_o == 1, "lane 4: answered");
    check(d.c1_rsp_payload_o == 3, "lane 4: the refusal reached the client");
    zhao::tick(d);
    check(d.poison1_o == p0 + 1, "lane 4: POSITIVE CONTROL -- poison1_o FIRED");
    std::printf("srvshare: poison1_o went %u -> %u on a refusal in flight\n", p0, d.poison1_o);
    idle(d);
    zhao::tick(d);
  }

  if (failures == 0) {
    std::printf("forge_cliff_srvshare_unit: OK\n");
  }
  // MANDATORY ON THIS TOOLCHAIN, and this file is the evidence for why.
  // zhao_sim.hpp records that Verilator 5.051 + winlibs libwinpthread
  // intermittently deadlocks in VlThreadPool::~VlThreadPool() during exit-time
  // static destruction -- "a hang with ~0 CPU in WaitForSingleObject" -- and
  // that "every Verilated main must end through here". This binary did exactly
  // that three times, with its verdict still sitting in an unflushed buffer,
  // which is indistinguishable from a process that never started.
  zhao::exit_hard(failures == 0 ? 0 : 1);
}
