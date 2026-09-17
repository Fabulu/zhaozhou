// shell_v2_lease_path_directed.cpp -- one legal Packet-H frame, end to end.
//
// The step before `zhao_shell_top_v2.sv`, per
// reports/PACKET-H-CHANNEL-MAP-20260917.md. It drives a renderer frame request
// through the composed lease path and asserts the lifecycle COUNTS, not the
// appearance of activity:
//
//     frame request -> render_req -> manager grants -> response accepted
//     -> V3 frame-clear handshake -> frame admitted
//     -> terminal return via the adapter -> publication
//     -> ready published -> swap echoed -> displayed
//
// WHY COUNTS. This repository has a chapter on a machine doing several times
// the work while producing byte-identical output, found only because somebody
// asserted an exact per-recipe job count. A test that checks a frame came out
// cannot see the manager granting two leases for it, and the throughput budget
// is written against the second number.
//
// WHAT IT DOES NOT COVER, stated so the next reader does not over-read it:
// the frame-clear handshake's ORDERING relative to old-work drain, the
// reset-barrier entry for the five structural faults, and the sequence-abort
// RELEASE control are behaviour with their own gate clauses, and none of them
// is exercised here. This shows the channels carry one clean frame. That is
// the prerequisite for composing them, not the Packet-H gate.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_shell_v2_lease_path.h"

#include "zhao_sim.hpp"

using zhao::check;

namespace {

using Dut = Vzhao_shell_v2_lease_path;

void tick(Dut& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
}

// Run until `pred` holds, or give up. Returns the cycles taken, or -1.
template <typename P>
int wait_for(Dut& d, P pred, int budget = 200) {
  for (int i = 0; i < budget; ++i) {
    if (pred()) return i;
    tick(d);
  }
  return -1;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;

  dut.clk = 0;
  dut.rst_n = 0;
  dut.lease_open_i = 0;
  dut.frame_req_valid_i = 0;
  dut.frame_req_mode_i = 0;
  dut.bin_frame_end_i = 0;
  dut.bin_grid_w_i = 2;
  dut.bin_grid_h_i = 2;
  dut.frame_ready_i = 0;
  dut.term_valid_i = 0;
  dut.term_slot_i = 0;
  dut.term_generation_i = 0;
  dut.term_publish_i = 0;
  dut.term_fault_i = 0;
  dut.ready_ready_i = 0;
  dut.swap_valid_i = 0;
  dut.swap_writer_i = 0;
  dut.swap_slot_i = 0;
  dut.swap_generation_i = 0;
  dut.swap_mode_i = 0;
  dut.swap_base_i = 0;
  dut.swap_span_i = 0;
  dut.blit_dispatch_valid_i = 0;
  dut.blit_dispatch_slot_i = 0;
  dut.blit_dispatch_mode_i = 0;
  dut.fb_req_ready_i = 0;
  dut.blit_done_i = 0;
  dut.eval();
  tick(dut);
  tick(dut);
  dut.rst_n = 1;
  dut.eval();
  tick(dut);

  check(dut.requests_accepted_o == 0, "manager starts with no accepted requests", 0,
        dut.requests_accepted_o);
  check(dut.leases_granted_o == 0, "manager starts with no leases granted", 0,
        dut.leases_granted_o);
  check(dut.adapter_idle_o == 1, "adapter starts idle", 1, dut.adapter_idle_o);

  // ---- FACT 1: the barrier gates CREATION ---------------------------------
  // With lease_open low, a frame request must not become a manager request.
  dut.frame_req_valid_i = 1;
  dut.frame_req_mode_i = 1;  // mode 3 is not a canvas and is never accepted
  for (int i = 0; i < 20; ++i) tick(dut);
  check(dut.requests_accepted_o == 0, "barrier closed: no request reaches the manager", 0,
        dut.requests_accepted_o);

  // ---- open the barrier and let the request through ------------------------
  dut.lease_open_i = 1;
  // The clear is no longer accepted by the test. `zhao_geom_bin_pipe_v2`
  // accepts it, and only once the binner and tile path are quiet.
  //
  dut.frame_ready_i = 1;  // the render path accepts the frame
  dut.ready_ready_i = 1;  // the CDC accepts the publication

  // THE BINNER HAS A COLD INIT AND IT GATES THE FIRST FRAME. This is a real
  // property of the composition, not a testbench detail: the bin pipe raises
  // `frame_fault_clear_ready_o` only once `binner_initialized_o` is set, the
  // lease will not admit a frame until that clear handshakes, so nothing
  // renders until the binner has finished initialising. Measured here at a
  // few hundred cycles. The old version of this test drove the clear ready
  // itself and therefore could not see it at all.
  const int to_init = wait_for(
      dut, [&] { return dut.bin_initialized_o != 0; }, 4000);
  check(to_init >= 0, "the binner finishes its cold init", 1, to_init >= 0);
  std::printf("[shell_v2_lease_path] binner cold init: %d cycles\n", to_init);

  const int to_lease = wait_for(
      dut, [&] { return dut.leases_granted_o != 0; }, 4000);
  check(to_lease >= 0, "a lease is granted once the barrier opens", 1, to_lease >= 0);
  check(dut.requests_accepted_o == 1, "exactly ONE request was accepted", 1,
        dut.requests_accepted_o);
  check(dut.leases_granted_o == 1, "exactly ONE lease was granted", 1, dut.leases_granted_o);

  // ---- FACT 2: the live lease is tagged to the renderer -------------------
  check(dut.lease_valid_o == 1, "the manager holds a live lease", 1, dut.lease_valid_o);
  check(dut.lease_writer_o == 1, "the live lease is tagged writer 1 (renderer)", 1,
        dut.lease_writer_o);

  // ---- the frame is admitted ----------------------------------------------
  const int to_frame = wait_for(
      dut, [&] { return dut.frame_valid_o != 0; }, 4000);
  check(to_frame >= 0, "a frame is admitted", 1, to_frame >= 0);
  check(dut.frame_writer_o == 1, "the admitted frame is writer 1", 1, dut.frame_writer_o);
  check(dut.frame_slot_o == dut.lease_slot_o, "the admitted frame carries the live lease's slot",
        dut.lease_slot_o, dut.frame_slot_o);
  check(dut.frame_generation_o == dut.lease_generation_o,
        "the admitted frame carries the live lease's generation", dut.lease_generation_o,
        dut.frame_generation_o);

  // ---- FACT 4: geometry is derived here -----------------------------------
  check(dut.frame_width_o != 0, "frame width is derived, not zero", 1, dut.frame_width_o != 0);
  check(dut.frame_height_o != 0, "frame height is derived, not zero", 1, dut.frame_height_o != 0);
  check(dut.frame_stride_o != 0, "frame stride is derived, not zero", 1, dut.frame_stride_o != 0);

  const uint16_t slot = dut.frame_slot_o;
  const uint16_t generation = dut.frame_generation_o;

  // ---- FACT 3: the renderer returns THROUGH THE ADAPTER -------------------
  dut.frame_req_valid_i = 0;
  dut.term_valid_i = 1;
  dut.term_slot_i = static_cast<uint8_t>(slot);
  dut.term_generation_i = generation;
  dut.term_publish_i = 1;  // publish=1 requests READY
  dut.term_fault_i = 0;

  // A PROPER VALID/READY HANDSHAKE, and the first version was not one. It
  // waited for the manager's accepted-count to move and then ticked once more
  // before dropping `term_valid_i` -- so the adapter took a SECOND terminal,
  // and `manager_terms_accepted_o` read 2. Nothing about the frame looked
  // wrong; the count is the only thing that could see it. That is this
  // repository's own lesson about a machine doing the work twice while
  // producing identical output, and it caught the test's driver rather than
  // the design.
  int to_term = -1;
  for (int i = 0; i < 200; ++i) {
    if (dut.term_valid_i && dut.term_ready_o) {  // this edge accepts it
      tick(dut);
      dut.term_valid_i = 0;
      dut.eval();
      to_term = i;
      break;
    }
    tick(dut);
  }
  check(to_term >= 0, "the adapter accepts the terminal return", 1, to_term >= 0);
  wait_for(dut, [&] { return dut.manager_terms_accepted_o != 0; });
  check(dut.manager_terms_accepted_o == 1, "exactly ONE terminal was accepted", 1,
        dut.manager_terms_accepted_o);
  check(dut.blit_events_refused_o == 0, "the idle blit side refused nothing", 0,
        dut.blit_events_refused_o);

  // ---- the publication becomes a ready event -------------------------------
  const int to_ready = wait_for(dut, [&] { return dut.ready_events_o != 0; });
  check(to_ready >= 0, "the clean publication is published as ready", 1, to_ready >= 0);
  check(dut.publications_o == 1, "exactly ONE publication", 1, dut.publications_o);
  check(dut.ready_events_o == 1, "exactly ONE ready event", 1, dut.ready_events_o);
  check(dut.faults_latched_o == 0, "no fault was latched on a clean frame", 0,
        dut.faults_latched_o);

  // ---- video echoes the swap ----------------------------------------------
  dut.swap_valid_i = 1;
  dut.swap_writer_i = 1;
  dut.swap_slot_i = static_cast<uint8_t>(slot);
  dut.swap_generation_i = generation;
  dut.swap_mode_i = 1;
  dut.swap_base_i = dut.ready_base_o;
  dut.swap_span_i = dut.ready_span_o;

  const int to_disp = wait_for(dut, [&] { return dut.displayed_valid_o != 0; });
  check(to_disp >= 0, "the echoed swap becomes the displayed record", 1, to_disp >= 0);
  tick(dut);
  dut.swap_valid_i = 0;
  check(dut.displayed_slot_o == slot, "the displayed record is the frame's slot", slot,
        dut.displayed_slot_o);
  check(dut.displayed_generation_o == generation, "the displayed record is the frame's generation",
        generation, dut.displayed_generation_o);
  check(dut.swaps_o == 1, "exactly ONE swap", 1, dut.swaps_o);

  // ---- and nothing ran twice ----------------------------------------------
  for (int i = 0; i < 40; ++i) tick(dut);
  check(dut.requests_accepted_o == 1, "still exactly one request after settling", 1,
        dut.requests_accepted_o);
  check(dut.leases_granted_o == 1, "still exactly one lease after settling", 1,
        dut.leases_granted_o);
  check(dut.publications_o == 1, "still exactly one publication after settling", 1,
        dut.publications_o);
  check(dut.contentions_o == 0, "no contention with a single requester", 0, dut.contentions_o);
  check(dut.clear_handshakes_o == 1, "still exactly one clear after settling", 1,
        dut.clear_handshakes_o);
  check(dut.frames_admitted_o == 1, "still exactly one admitted frame after settling", 1,
        dut.frames_admitted_o);

  // =======================================================================
  // THE FRAME-CLEAR HANDSHAKE, DRIVEN BY ITS REAL CONSUMER
  // =======================================================================
  //
  // Until now this test held `frame_fault_clear_ready_i` high and the clause
  // was asserted rather than exercised. `zhao_geom_bin_pipe_v2` gates its
  // ready on `binner_initialized_o && !frame_inflight_q && !frame_begin_i` --
  // the Packet-H gate's old-work-drain ordering, in RTL -- so composing it
  // is what turns the clause into a measurement.
  //
  // What the first frame above already proves, now that the ready is real:
  // the lease requested a clear, the bin pipe accepted it only when quiet,
  // and the frame followed. A frame was admitted, so all three happened.
  check(dut.bin_initialized_o == 1, "the binner initialised", 1, dut.bin_initialized_o);

  // EXACTLY ONE, which is the gate's wording and not a synonym for "at least
  // one". Counted in RTL because a driver that ticks in bursts can only
  // honestly report the cycles it looked at.
  check(dut.clear_handshakes_o == 1, "exactly ONE clear handshake for the lease", 1,
        dut.clear_handshakes_o);
  check(dut.frames_admitted_o == 1, "exactly ONE frame admitted", 1, dut.frames_admitted_o);
  check(dut.bin_frame_fault_o == 0, "no frame fault on a clean frame", 0, dut.bin_frame_fault_o);
  check(dut.bin_lifetime_fault_o == 0, "no lifetime structural fault", 0, dut.bin_lifetime_fault_o);

  // AND THE ORDERING ITSELF. Hold a frame in flight and the clear must NOT
  // be accepted: `frame_end_i` has not been seen, so old work has not
  // drained. This is the half a test-driven ready could never see.
  {
    const bool clear_ready_while_inflight = dut.clear_ready_o;
    check(clear_ready_while_inflight == 0, "the clear is refused while a frame is in flight", 0,
          clear_ready_while_inflight);

    // End the frame and let it drain. `frame_end_i` starts the drain; the
    // bin pipe is not quiet until the tile path has emptied, and the clear
    // ready follows that rather than the pulse.
    dut.bin_frame_end_i = 1;
    tick(dut);
    dut.bin_frame_end_i = 0;

    const int to_drain = wait_for(
        dut, [&] { return dut.bin_drain_done_o != 0; }, 4000);
    const int to_quiet = wait_for(
        dut, [&] { return dut.clear_ready_o != 0; }, 4000);
    check(to_quiet >= 0, "the clear is accepted once the frame has drained", 1, to_quiet >= 0);
    std::printf(
        "[shell_v2_lease_path] clear refused in flight; drain_done %d, clear ready %d "
        "cycles later (busy=%u quiet=%u done=%u)\n",
        to_drain, to_quiet, dut.bin_drain_busy_o, dut.bin_quiet_o, dut.bin_drain_done_o);
  }

  // =======================================================================
  // THE SHARED RESPONSE CHANNEL, WITH BOTH WRITERS ASKING
  // =======================================================================
  //
  // `rsp_*` is ONE channel tagged by writer, and all of it is invisible while
  // only the renderer requests -- which is all the case above proves. Both
  // contenders here are now the REAL leaves the shell will contain.
  //
  // AND THAT CHANGES WHAT THE CONTROL CAN BE, which is worth stating rather
  // than quietly dropping. The previous version drove the manager's blit
  // requester by hand and could starve the channel by holding the blit's
  // response-ready low; the counts then stalled at two requests and one
  // response, with the RENDERER stopped behind a writer-0 response nobody
  // took. With the real leaf that state is unreachable from outside:
  // zhao_video_blit_lease_v2 raises ready for its own tag whenever it is
  // waiting for an answer, and it is only ever waiting for one. The deadlock
  // is structurally gone rather than merely untested -- so the control below
  // is the failure that IS still reachable.
  {
    const uint32_t req_before = dut.requests_accepted_o;
    const uint32_t rsp_before = dut.responses_accepted_o;

    dut.fb_req_ready_i = 1;         // the stub blitter accepts its request
    dut.blit_dispatch_valid_i = 1;  // the blit path asks...
    dut.blit_dispatch_slot_i = 0;
    dut.blit_dispatch_mode_i = 1;
    dut.frame_req_valid_i = 1;  // ...and the renderer asks too
    dut.frame_req_mode_i = 1;

    const int to_both = wait_for(
        dut, [&] { return dut.requests_accepted_o >= req_before + 2; }, 400);
    check(to_both >= 0, "both writers' requests are accepted", 1, to_both >= 0);

    const int to_rsp = wait_for(
        dut, [&] { return dut.responses_accepted_o >= rsp_before + 2; }, 400);
    check(to_rsp >= 0, "both responses are accepted -- neither writer strands the channel", 1,
          to_rsp >= 0);

    dut.blit_dispatch_valid_i = 0;
    dut.frame_req_valid_i = 0;
    for (int i = 0; i < 40; ++i) tick(dut);

    // Simultaneous traffic alternates by accepted history, so the manager
    // records it. A zero here would mean the two requests never actually
    // contended and the case proved nothing.
    check(dut.contentions_o > 0, "coverage: the two requesters actually contended", 1,
          dut.contentions_o > 0);

    // And the blit leaf resolved a real lease through the real protocol.
    check(dut.blit_leases_acquired_o + dut.blit_leases_refused_o == 1,
          "the blit leaf resolved exactly one lease request", 1,
          dut.blit_leases_acquired_o + dut.blit_leases_refused_o);
    check(dut.blits_dispatched_o == 1, "exactly one blit was dispatched", 1,
          dut.blits_dispatched_o);

    // THE CLEAR COUNTER HAS TO BE SEEN TO MOVE. It was asserted == 1 twice
    // above, and a counter that is only ever checked against the value it
    // starts a case with is indistinguishable from one that is wired to a
    // constant. The renderer took a second lease here, so it owes a second
    // clear -- and the SECOND one is what says the first was counted rather
    // than coincidental.
    check(dut.clear_handshakes_o == 2, "the second lease produced a second clear handshake", 2,
          dut.clear_handshakes_o);
    check(dut.frames_admitted_o == 2, "and a second admitted frame", 2, dut.frames_admitted_o);

    // RETURN THE RENDERER'S FRAME AS A RELEASE, and the reason is the whole
    // lesson of the failure this replaced. The next case asks whether a
    // stalled blitter blocks the renderer. It reported YES -- and the cause
    // was nothing to do with the blitter: the renderer still held the
    // manager's live lease from this case, and `frame_req_ready_o` needs
    // both no live lease AND a FREE slot. A clean publication leaves the slot
    // READY, not FREE, so publishing would not have fixed it either; with one
    // slot DISPLAYED and one READY the renderer cannot start a third frame at
    // all. Release-class terminal returns the slot to FREE and clears the
    // lease, which is the only state in which the head-of-line question has a
    // meaning. Left as it was, the case measured the SLOT LIFECYCLE and
    // reported it as a deadlock.
    dut.term_slot_i = static_cast<uint8_t>(dut.lease_slot_o);
    dut.term_generation_i = dut.lease_generation_o;
    dut.term_publish_i = 0;  // release-class: back to FREE
    dut.term_fault_i = 0;
    dut.term_valid_i = 1;
    for (int i = 0; i < 200; ++i) {
      if (dut.term_valid_i && dut.term_ready_o) {
        tick(dut);
        dut.term_valid_i = 0;
        dut.eval();
        break;
      }
      tick(dut);
    }
    for (int i = 0; i < 10; ++i) tick(dut);
    check(dut.lease_valid_o == 0, "the released renderer lease is cleared", 0, dut.lease_valid_o);

    // RETIRE IT. The leaf holds its lease until the blitter reports done, so
    // leaving this out parks it in RUN -- and the next case then silently
    // tests nothing, because a busy leaf refuses the dispatch and every
    // subsequent assertion reads "the blit never got anywhere". That is what
    // the first version of this case did, and the failure named the stall it
    // was trying to create rather than the setup that never happened.
    dut.blit_done_i = 1;
    for (int i = 0; i < 4; ++i) tick(dut);
    dut.blit_done_i = 0;
    dut.fb_req_ready_i = 0;
    for (int i = 0; i < 4; ++i) tick(dut);
    check(dut.blit_idle_o == 1, "the blit leaf retires back to idle", 1, dut.blit_idle_o);
    std::printf(
        "[shell_v2_lease_path] contended: requests=%u responses=%u contentions=%u "
        "blit_acquired=%u blit_refused=%u\n",
        dut.requests_accepted_o, dut.responses_accepted_o, dut.contentions_o,
        dut.blit_leases_acquired_o, dut.blit_leases_refused_o);
  }

  // =======================================================================
  // A STALLED BLITTER MUST NOT STOP THE RENDERER
  // =======================================================================
  //
  // The reachable head-of-line failure. fb_req_ready_i low models a blitter
  // that is busy moving bytes -- the ordinary case, not a fault -- and the
  // blit leaf then sits in ISSUE holding a lease it cannot hand over. If any
  // of that reaches the shared channel the renderer stops for a reason that is
  // nowhere near the renderer, which is the symptom this harness exists to
  // make impossible to ship undetected.
  {
    dut.blit_done_i = 0;
    dut.fb_req_ready_i = 0;  // the blitter is busy and stays busy
    dut.blit_dispatch_valid_i = 1;
    dut.blit_dispatch_slot_i = 0;
    dut.blit_dispatch_mode_i = 1;

    const int to_stuck = wait_for(
        dut, [&] { return dut.fb_req_valid_o != 0; }, 200);
    check(to_stuck >= 0, "the blit leaf reaches its blitter request", 1, to_stuck >= 0);
    dut.blit_dispatch_valid_i = 0;
    for (int i = 0; i < 20; ++i) tick(dut);
    check(dut.fb_req_valid_o == 1, "the blit leaf is genuinely stalled", 1, dut.fb_req_valid_o);
    check(dut.blit_idle_o == 0, "coverage: the blit leaf is holding a lease", 0, dut.blit_idle_o);

    const uint32_t req_before = dut.requests_accepted_o;
    dut.frame_req_valid_i = 1;
    dut.frame_req_mode_i = 1;
    const int to_render = wait_for(
        dut, [&] { return dut.requests_accepted_o >= req_before + 1; }, 400);
    check(to_render >= 0, "the renderer is served while the blitter is stalled", 1, to_render >= 0);
    dut.frame_req_valid_i = 0;

    // Release it, so the case leaves nothing held.
    dut.fb_req_ready_i = 1;
    for (int i = 0; i < 4; ++i) tick(dut);
    dut.blit_done_i = 1;
    for (int i = 0; i < 4; ++i) tick(dut);
    dut.blit_done_i = 0;
    dut.fb_req_ready_i = 0;
    for (int i = 0; i < 10; ++i) tick(dut);
    check(dut.blit_idle_o == 1, "the released blit leaf returns to idle", 1, dut.blit_idle_o);
  }

  std::printf(
      "[shell_v2_lease_path] lease->frame %d cycles, frame->term %d, term->ready %d, "
      "ready->displayed %d\n",
      to_lease, to_frame, to_term, to_disp);

  dut.final();
  zhao::exit_hard(zhao::report_and_exit("shell_v2_lease_path_directed"));
}
