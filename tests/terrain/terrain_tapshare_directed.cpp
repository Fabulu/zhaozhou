// terrain_tapshare_directed.cpp -- the two-client arbiter for the ONE
// `zhao_terrain_heighttap` requester port.
//
// WHY THE BLOCK EXISTS, in one line: `design/contracts/FORGE.SHADOW.md` marked
// the `tap_*` port group "genuinely clear" because the tap is composed and its
// ports mirror FORGE.SHADOW's. The tap has exactly ONE requester port and
// PART.TERRAIN_TAP already holds every wire of it.
//
// WHAT ACTUALLY DISCRIMINATES HERE, named up front, because a bench that only
// shows a request going in and an answer coming out would pass against an
// arbiter with no owner register at all:
//
//   1. THE ANSWER GOES TO THE CLIENT THAT ASKED, AND TO NO OTHER. The service
//      returns its eighteen data outputs with NO TAG AND NO RIDER, so the only
//      thing that can route them is an owner this block holds. The case that
//      separates a correct latch from a broken one is client 0 outstanding
//      while client 1 is ASSERTING VALID: a block that routed by "who is asking
//      now" answers client 1. We assert the CORRECT behaviour -- the answer
//      reaches 0 and never 1 -- rather than asserting the bug (CLAUDE.md: "do
//      not write a test that asserts the bug").
//
//   2. NOTHING IS OFFERED WHILE A REQUEST IS OUTSTANDING. The owner is one
//      register because `zhao_terrain_heighttap` is strictly single-in-flight
//      (`req_ready_o = (st_q == S_IDLE)`). If this block ever offered a second
//      request it would overwrite the owner and the service could not tell.
//      Checked with the service's ready HELD HIGH, so the tap is not the thing
//      enforcing it -- otherwise the test measures the tap, not the arbiter.
//
//   3. THE LOSER IS HELD, NOT DROPPED. A client that loses arbitration keeps
//      its valid asserted and must be granted on the next turn with the SAME
//      x and z. A dropped request looks identical to a served one in any
//      counter that only counts answers.
//
//   4. THE ROTATION IS EXERCISED, NOT ASSERTED. With both clients asking
//      forever, grants must ALTERNATE exactly -- the N-1 bound made concrete --
//      and `contended_o` must move. A fairness law nothing contends is a claim.
//
//   5. `stray_rsp_o` IS A NEGATIVE CONTROL HERE. It must stay at zero under
//      every legal stimulus above; its POSITIVE control is the committed mutant
//      `tests/mutants/zhao_terrain_tapshare_mutant.sv`, because no legal input
//      can make a single-in-flight service answer when nothing is outstanding.
//
//   6. THE REQUEST PAYLOAD IS THE WINNER'S. Both clients offer DIFFERENT x/z,
//      so a mux that picked the wrong source, or that presented client 0's
//      coordinates under client 1's grant, fails. Two clients offering the same
//      numbers is the commonest way this check is written and it discriminates
//      nothing.
#include <cstdint>
#include <cstdio>

#include "Vtb_terrain_tapshare.h"
#include "zhao_sim.hpp"

using zhao::check;
using zhao::tick;

namespace {

void hard_reset(Vtb_terrain_tapshare& d) {
  d.rst_n = 0;
  d.r0_valid_i = 0;
  d.r1_valid_i = 0;
  d.r0_x_i = 0;
  d.r0_z_i = 0;
  d.r1_x_i = 0;
  d.r1_z_i = 0;
  d.r0_surface_i = 0;
  d.r1_surface_i = 0;
  d.t_req_ready_i = 0;
  d.t_rsp_valid_i = 0;
  d.eval();
  for (int i = 0; i < 3; ++i) tick(d);
  d.rst_n = 1;
  d.eval();
  tick(d);
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  auto* top = new Vtb_terrain_tapshare;
  auto& d = *top;

  hard_reset(d);

  check(d.busy_o == 0, "idle after reset: no owner outstanding", 0, d.busy_o);
  check(d.t_req_valid_o == 0, "no offer with no client asking", 0,
        d.t_req_valid_o);

  // =========================================================================
  // 1. ONE CLIENT, END TO END -- and the answer reaches only that client
  // =========================================================================
  d.t_req_ready_i = 1;
  d.r0_valid_i = 1;
  d.r0_x_i = 0x0011'2233;
  d.r0_z_i = 0x0044'5566;
  d.r0_surface_i = 1;
  d.eval();

  check(d.t_req_valid_o == 1, "a lone client is offered to the service", 1,
        d.t_req_valid_o);
  check((uint32_t)d.t_req_x_o == 0x0011'2233u, "the offer carries client 0's x",
        0x0011'2233u, (uint32_t)d.t_req_x_o);
  check((uint32_t)d.t_req_z_o == 0x0044'5566u, "the offer carries client 0's z",
        0x0044'5566u, (uint32_t)d.t_req_z_o);
  check(d.t_req_surface_o == 1, "the offer carries client 0's surface bit", 1,
        d.t_req_surface_o);
  check(d.r0_ready_o == 1, "client 0 sees ready on the granted cycle", 1,
        d.r0_ready_o);
  check(d.r1_ready_o == 0, "client 1 does not see ready", 0, d.r1_ready_o);

  tick(d);  // the grant handshake
  d.r0_valid_i = 0;
  d.eval();

  check(d.busy_o == 1, "the owner is held after the grant", 1, d.busy_o);

  // =========================================================================
  // 2. NOTHING IS OFFERED WHILE OUTSTANDING -- with the service READY HIGH,
  //    so the arbiter is what refuses, not the tap
  // =========================================================================
  d.r1_valid_i = 1;
  d.r1_x_i = 0x7777'7777;
  d.r1_z_i = 0x6666'6666;
  d.eval();
  check(d.t_req_valid_o == 0,
        "no second offer while a request is outstanding, service ready HIGH", 0,
        d.t_req_valid_o);
  check(d.r1_ready_o == 0, "the waiting client is not accepted meanwhile", 0,
        d.r1_ready_o);

  // =========================================================================
  // 3. THE DISCRIMINATING CASE: the answer routes by the HELD owner, while a
  //    DIFFERENT client is asking right now -- and AFTER A MULTI-CYCLE WALK.
  //
  //    The delay is not decoration. `zhao_terrain_heighttap` runs a thirteen-
  //    state walk, so the owner must survive many cycles in which nothing
  //    happens and another client is asking. An owner register that decays,
  //    or that is cleared by anything other than the response, fails HERE and
  //    nowhere else -- which is why the committed mutant breaks exactly this.
  // =========================================================================
  for (int i = 0; i < 6; ++i) {
    tick(d);
    d.eval();
    check(d.busy_o == 1, "the owner survives every cycle of the service walk",
          1, d.busy_o);
    check(d.t_req_valid_o == 0, "and nothing is offered during the walk", 0,
          d.t_req_valid_o);
  }

  d.t_rsp_valid_i = 1;
  d.eval();
  check(d.r0_rsp_valid_o == 1, "the answer reaches the client that asked", 1,
        d.r0_rsp_valid_o);
  check(d.r1_rsp_valid_o == 0,
        "the answer does NOT reach the client asking at that moment", 0,
        d.r1_rsp_valid_o);
  check(d.stray_rsp_o == 0, "an owned answer is not counted stray", 0,
        d.stray_rsp_o);

  tick(d);  // retire the owner, and grant client 1 in the same cycle
  d.t_rsp_valid_i = 0;
  d.eval();

  // =========================================================================
  // 4. THE LOSER WAS HELD, NOT DROPPED -- and its OWN payload is presented
  // =========================================================================
  check(d.busy_o == 1, "the held client was granted on the retiring cycle", 1,
        d.busy_o);
  const uint32_t g0_after_first = d.grants0_o;
  check(g0_after_first == 1, "client 0 has exactly one grant", 1,
        g0_after_first);
  check(d.grants1_o == 1, "the held client 1 was granted next", 1, d.grants1_o);

  d.t_rsp_valid_i = 1;
  d.eval();
  check(d.r1_rsp_valid_o == 1, "client 1's answer reaches client 1", 1,
        d.r1_rsp_valid_o);
  check(d.r0_rsp_valid_o == 0, "and not client 0", 0, d.r0_rsp_valid_o);
  tick(d);
  d.t_rsp_valid_i = 0;
  d.r1_valid_i = 0;
  d.eval();

  // =========================================================================
  // 5. THE ROTATION, EXERCISED. Both clients ask forever; grants must
  //    ALTERNATE exactly, which is the N-1 bound made concrete.
  // =========================================================================
  // ---- drain to idle before measuring the rotation -------------------------
  // Necessary because the arbiter retires and re-grants on the SAME cycle: a
  // client still asserting valid when its answer arrives is granted again
  // immediately. That is the behaviour we want, and it means the bench cannot
  // just stop driving -- it has to answer whatever is outstanding with nobody
  // asking. Doubling as a check: an arbiter that could not reach idle would
  // hang here rather than report a wrong number.
  d.r0_valid_i = 0;
  d.r1_valid_i = 0;
  d.t_rsp_valid_i = 0;
  d.eval();
  for (int i = 0; i < 20 && d.busy_o; ++i) {
    d.t_rsp_valid_i = 1;
    d.eval();
    tick(d);
    d.t_rsp_valid_i = 0;
    d.eval();
  }
  check(d.busy_o == 0, "the arbiter returns to idle when no client is asking",
        0, d.busy_o);

  const uint32_t g0_base = d.grants0_o;
  const uint32_t g1_base = d.grants1_o;
  const uint32_t cont_base = d.contended_o;

  d.r0_valid_i = 1;
  d.r1_valid_i = 1;
  d.r0_x_i = 0x0000'00AA;
  d.r0_z_i = 0x0000'00BB;
  d.r1_x_i = 0x0000'00CC;
  d.r1_z_i = 0x0000'00DD;

  // A cycle-accurate driver rather than a hand-sequenced one: the arbiter may
  // retire an owner and grant the next client ON THE SAME CYCLE, so "grant,
  // then wait, then answer" is not a sequence the bench gets to impose. It
  // observes grants where they actually happen and schedules each answer a
  // fixed few cycles later, which is what the real service does.
  constexpr int kWalk = 3;   // cycles the stand-in service takes to answer
  constexpr int kWant = 20;  // grants to observe

  int pending = -1;    // cycles until the outstanding answer; -1 = none
  int owner_now = -1;  // who that answer belongs to
  int last_owner = -1;
  int alternations = 0;
  int served = 0;

  for (int cyc = 0; cyc < 400 && served < kWant; ++cyc) {
    d.t_rsp_valid_i = (pending == 0) ? 1 : 0;
    d.eval();

    const bool granted = (d.t_req_valid_o != 0) && (d.t_req_ready_i != 0);
    int winner = -1;
    if (granted) {
      // Whose payload is on the bus tells us who won, and the two clients offer
      // DIFFERENT numbers precisely so this is answerable.
      winner = ((uint32_t)d.t_req_x_o == 0x0000'00AAu) ? 0 : 1;
      check((uint32_t)d.t_req_z_o ==
                (winner == 0 ? 0x0000'00BBu : 0x0000'00DDu),
            "the offer's z belongs to the same client as its x",
            winner == 0 ? 0x0000'00BBu : 0x0000'00DDu, (uint32_t)d.t_req_z_o);
    }

    if (pending == 0) {
      check((owner_now == 0 ? d.r0_rsp_valid_o : d.r1_rsp_valid_o) == 1,
            "each answer reaches the client that asked for it", 1,
            (owner_now == 0 ? d.r0_rsp_valid_o : d.r1_rsp_valid_o));
      check((owner_now == 0 ? d.r1_rsp_valid_o : d.r0_rsp_valid_o) == 0,
            "and never the other client", 0,
            (owner_now == 0 ? d.r1_rsp_valid_o : d.r0_rsp_valid_o));
    }

    tick(d);

    if (pending > 0) {
      --pending;
    } else if (pending == 0) {
      pending = -1;
    }
    if (granted) {
      if (last_owner >= 0 && winner != last_owner) ++alternations;
      last_owner = winner;
      owner_now = winner;
      pending = kWalk;
      ++served;
    }
  }
  d.r0_valid_i = 0;
  d.r1_valid_i = 0;
  d.t_rsp_valid_i = 0;
  d.eval();

  check(served == kWant, "the driver observed every grant it asked for",
        (uint64_t)kWant, (uint64_t)served);
  check(alternations == served - 1,
        "grants ALTERNATE on every contended round (the N-1 bound, exercised)",
        (uint64_t)(served - 1), (uint64_t)alternations);
  check(d.grants0_o - g0_base == 10, "client 0 took exactly half the rounds", 10,
        d.grants0_o - g0_base);
  check(d.grants1_o - g1_base == 10, "client 1 took exactly half the rounds", 10,
        d.grants1_o - g1_base);
  check(d.contended_o - cont_base == 20,
        "every one of those rounds was recorded as contended", 20,
        d.contended_o - cont_base);

  // =========================================================================
  // 6. THE NEGATIVE CONTROL. Nothing above may have moved `stray_rsp_o`.
  // =========================================================================
  check(d.stray_rsp_o == 0,
        "stray_rsp_o stayed at ZERO under every legal stimulus (its positive "
        "control is tests/mutants/zhao_terrain_tapshare_mutant.sv)",
        0, d.stray_rsp_o);

  check(d.grants0_o > 0 && d.grants1_o > 0 && d.contended_o > 0,
        "every counter reachable by legal stimulus was FIRED", 1,
        d.grants0_o > 0 && d.grants1_o > 0 && d.contended_o > 0);

  std::printf(
      "[terrain_tapshare_directed] grants0=%u grants1=%u contended=%u "
      "stray_rsp=%u (negative control; mutant is its positive control)\n",
      d.grants0_o, d.grants1_o, d.contended_o, d.stray_rsp_o);

  top->final();
  return zhao::report_and_exit("terrain_tapshare_directed");
}
