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
  dut.frame_fault_clear_ready_i = 0;
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
  dut.frame_fault_clear_ready_i = 1;  // V3 accepts the clear
  dut.frame_ready_i = 1;              // the render path accepts the frame
  dut.ready_ready_i = 1;              // the CDC accepts the publication

  const int to_lease = wait_for(dut, [&] { return dut.leases_granted_o != 0; });
  check(to_lease >= 0, "a lease is granted once the barrier opens", 1, to_lease >= 0);
  check(dut.requests_accepted_o == 1, "exactly ONE request was accepted", 1,
        dut.requests_accepted_o);
  check(dut.leases_granted_o == 1, "exactly ONE lease was granted", 1, dut.leases_granted_o);

  // ---- FACT 2: the live lease is tagged to the renderer -------------------
  check(dut.lease_valid_o == 1, "the manager holds a live lease", 1, dut.lease_valid_o);
  check(dut.lease_writer_o == 1, "the live lease is tagged writer 1 (renderer)", 1,
        dut.lease_writer_o);

  // ---- the frame is admitted ----------------------------------------------
  const int to_frame = wait_for(dut, [&] { return dut.frame_valid_o != 0; });
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

  std::printf(
      "[shell_v2_lease_path] lease->frame %d cycles, frame->term %d, term->ready %d, "
      "ready->displayed %d\n",
      to_lease, to_frame, to_term, to_disp);

  dut.final();
  zhao::exit_hard(zhao::report_and_exit("shell_v2_lease_path_directed"));
}
