// blit_lease_v2_directed.cpp -- Packet-H writer-0 lease front end.
#if (defined(ZHAO_EXPECT_BLIT_LEASE_MUTANT_ADMISSION_BYPASS) + \
     defined(ZHAO_EXPECT_BLIT_LEASE_MUTANT_WRITER_RESPONSE) +  \
     defined(ZHAO_EXPECT_BLIT_LEASE_MUTANT_REQUEST_HOLD) +     \
     defined(ZHAO_EXPECT_BLIT_LEASE_MUTANT_LIVE_RECORD) +      \
     defined(ZHAO_EXPECT_BLIT_LEASE_MUTANT_ISSUE_EARLY) +      \
     defined(ZHAO_EXPECT_BLIT_LEASE_MUTANT_REFUSED_VALID)) > 1
#error "define at most one blit-lease mutant expectation"
#endif

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_video_blit_lease_v2.h"

#include "zhao_sim.hpp"

namespace {

using Dut = Vzhao_video_blit_lease_v2;

void clear_inputs(Dut& d) {
  d.lease_open_i = 1;
  d.dispatch_valid_i = 0;
  d.dispatch_slot_i = 0;
  d.dispatch_mode_i = 0;
  d.blit_req_ready_i = 0;
  d.blit_done_i = 0;
  d.mgr_req_ready_i = 0;
  d.rsp_valid_i = 0;
  d.rsp_writer_i = 0;
  d.rsp_granted_i = 0;
  d.rsp_slot_i = 0;
  d.rsp_generation_i = 0;
}

void reset(Dut& d) {
  clear_inputs(d);
  d.rst_n = 0;
  d.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

// Accept one dispatch. Returns with the leaf holding the manager request.
void dispatch(Dut& d, uint8_t slot, uint8_t mode) {
  d.dispatch_slot_i = slot;
  d.dispatch_mode_i = mode;
  d.dispatch_valid_i = 1;
  d.eval();
  zhao::tick(d);
  d.dispatch_valid_i = 0;
  d.eval();
}

void accept_manager_request(Dut& d) {
  d.mgr_req_ready_i = 1;
  d.eval();
  zhao::tick(d);
  d.mgr_req_ready_i = 0;
  d.eval();
}

// Offer one shared response. Leaves the response pins SCRAMBLED afterwards, on
// purpose: everything downstream must read the frozen record, never the bus.
void give_response(Dut& d, bool writer, bool granted, uint8_t slot, uint16_t generation) {
  d.rsp_valid_i = 1;
  d.rsp_writer_i = writer;
  d.rsp_granted_i = granted;
  d.rsp_slot_i = slot;
  d.rsp_generation_i = generation;
  d.eval();
}

void retire_blit(Dut& d) {
  d.blit_req_ready_i = 1;
  d.eval();
  zhao::tick(d);
  d.blit_req_ready_i = 0;
  d.blit_done_i = 1;
  d.eval();
  zhao::tick(d);
  d.blit_done_i = 0;
  d.eval();
}

[[noreturn]] void mutant_result(const char* name, bool detected) {
  std::printf("[%s] %s\n", name, detected ? "DETECTED" : "MISSED");
  zhao::exit_hard(detected ? 0 : 1);
}

}  // namespace

int main() {
  Dut d;

#if defined(ZHAO_EXPECT_BLIT_LEASE_MUTANT_ADMISSION_BYPASS)
  reset(d);
  d.lease_open_i = 0;
  d.dispatch_valid_i = 1;
  d.eval();
  const bool accepted_while_closed = d.dispatch_ready_o;
  zhao::tick(d);
  d.eval();
  mutant_result("blit_lease_admission_bypass", accepted_while_closed && d.mgr_req_valid_o);

#elif defined(ZHAO_EXPECT_BLIT_LEASE_MUTANT_WRITER_RESPONSE)
  reset(d);
  dispatch(d, 0, 1);
  accept_manager_request(d);
  give_response(d, /*writer=*/true, /*granted=*/true, 0, 0x1234u);
  mutant_result("blit_lease_writer_response", d.rsp_ready_o);

#elif defined(ZHAO_EXPECT_BLIT_LEASE_MUTANT_REQUEST_HOLD)
  reset(d);
  dispatch(d, 0, 1);
  // The manager is not ready, so the request is held. Move the dispatch pins
  // underneath it; a correct leaf is still asking for what it accepted.
  d.dispatch_slot_i = 1;
  d.dispatch_mode_i = 2;
  d.eval();
  mutant_result("blit_lease_request_hold",
                d.mgr_req_valid_o && d.mgr_req_slot_o == 1 && d.mgr_req_mode_o == 2);

#elif defined(ZHAO_EXPECT_BLIT_LEASE_MUTANT_LIVE_RECORD)
  reset(d);
  dispatch(d, 1, 1);
  accept_manager_request(d);
  give_response(d, /*writer=*/false, /*granted=*/true, /*slot=*/1, /*generation=*/0xBEEFu);
  zhao::tick(d);
  d.rsp_valid_i = 0;
  d.rsp_slot_i = 0;          // the bus moves on; the record must not
  d.rsp_generation_i = 0x0000u;
  d.eval();
  mutant_result("blit_lease_live_record",
                d.fb_lease_slot_o == 0 || d.fb_lease_generation_o == 0x0000u);

#elif defined(ZHAO_EXPECT_BLIT_LEASE_MUTANT_ISSUE_EARLY)
  reset(d);
  dispatch(d, 0, 1);
  accept_manager_request(d);
  give_response(d, /*writer=*/false, /*granted=*/true, 0, 0x1234u);
  // The blitter latches the generation on the edge it accepts the request. If
  // the request is already offered while the answer is still landing, that
  // latch takes the PRE-GRANT record.
  mutant_result("blit_lease_issue_early", d.rsp_ready_o && d.blit_req_valid_o);

#elif defined(ZHAO_EXPECT_BLIT_LEASE_MUTANT_REFUSED_VALID)
  reset(d);
  dispatch(d, 0, 1);
  accept_manager_request(d);
  give_response(d, /*writer=*/false, /*granted=*/false, 0, 0x4321u);
  zhao::tick(d);
  d.rsp_valid_i = 0;
  d.eval();
  mutant_result("blit_lease_refused_valid", d.fb_lease_valid_o);

#else
  using zhao::check;

  reset(d);
  check(d.idle_o == 1, "leaf starts idle", 1, d.idle_o);
  check(d.mgr_req_valid_o == 0, "no manager request out of reset", 0, d.mgr_req_valid_o);
  check(d.blit_req_valid_o == 0, "no blitter request out of reset", 0, d.blit_req_valid_o);
  check(d.fb_lease_valid_o == 0, "no lease record out of reset", 0, d.fb_lease_valid_o);
  check(d.leases_acquired_o == 0, "acquired counter starts at zero", 0, d.leases_acquired_o);
  check(d.leases_refused_o == 0, "refused counter starts at zero", 0, d.leases_refused_o);
  check(d.blits_dispatched_o == 0, "dispatched counter starts at zero", 0,
        d.blits_dispatched_o);

  // ---- the barrier gates CREATION -----------------------------------------
  d.lease_open_i = 0;
  d.dispatch_valid_i = 1;
  d.eval();
  check(d.dispatch_ready_o == 0, "barrier closed: dispatch is refused", 0, d.dispatch_ready_o);
  for (int i = 0; i < 20; ++i) zhao::tick(d);
  d.eval();
  check(d.mgr_req_valid_o == 0, "barrier closed: no manager request is created", 0,
        d.mgr_req_valid_o);
  check(d.idle_o == 1, "barrier closed: the leaf stays idle", 1, d.idle_o);
  d.dispatch_valid_i = 0;
  d.lease_open_i = 1;
  d.eval();

  // ---- one clean blit ------------------------------------------------------
  dispatch(d, /*slot=*/1, /*mode=*/1);
  check(d.mgr_req_valid_o == 1, "an accepted dispatch becomes a manager request", 1,
        d.mgr_req_valid_o);
  check(d.mgr_req_slot_o == 1, "the request carries the accepted slot", 1, d.mgr_req_slot_o);
  check(d.mgr_req_mode_o == 1, "the request carries the accepted mode", 1, d.mgr_req_mode_o);
  check(d.idle_o == 0, "the leaf is no longer idle", 0, d.idle_o);

  // A STALLED REQUEST FOLLOWS THE RECORD, NOT THE PINS. This is the shape that
  // made a metadata bank ship a record-swapping defect with a live counter
  // reading zero beside it: the held thing tracked what was being OFFERED.
  d.dispatch_slot_i = 0;
  d.dispatch_mode_i = 2;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.eval();
  check(d.mgr_req_slot_o == 1, "a stalled request keeps the accepted slot", 1,
        d.mgr_req_slot_o);
  check(d.mgr_req_mode_o == 1, "a stalled request keeps the accepted mode", 1,
        d.mgr_req_mode_o);

  accept_manager_request(d);

  // ---- the shared channel is not this leaf's to consume --------------------
  give_response(d, /*writer=*/true, /*granted=*/true, /*slot=*/0, /*generation=*/0x0BADu);
  check(d.rsp_ready_o == 0, "the renderer's response is not accepted here", 0, d.rsp_ready_o);
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.eval();
  check(d.blit_req_valid_o == 0, "a renderer response does not advance the blit", 0,
        d.blit_req_valid_o);
  check(d.fb_lease_valid_o == 0, "a renderer response does not create a lease record", 0,
        d.fb_lease_valid_o);

  // ---- the one-cycle law ---------------------------------------------------
  // zhao_debug_frameblit latches fb_lease_generation_i on the SAME edge it
  // accepts a request, so the request must NOT be on offer while the answer is
  // still landing. Asserted on the accepting edge itself rather than inferred
  // from the result.
  give_response(d, /*writer=*/false, /*granted=*/true, /*slot=*/1, /*generation=*/0xBEEFu);
  check(d.rsp_ready_o == 1, "the blitter's own response is accepted", 1, d.rsp_ready_o);
  check(d.blit_req_valid_o == 0,
        "the request is NOT offered on the edge the response lands", 0, d.blit_req_valid_o);
  zhao::tick(d);
  d.rsp_valid_i = 0;
  d.rsp_slot_i = 0;            // scramble the bus: the record is frozen now
  d.rsp_generation_i = 0x0000u;
  d.eval();

  check(d.blit_req_valid_o == 1, "the request reaches the blitter one state later", 1,
        d.blit_req_valid_o);
  check(d.fb_lease_valid_o == 1, "a granted lease presents a valid record", 1,
        d.fb_lease_valid_o);
  check(d.fb_lease_slot_o == 1, "the record holds the granted slot", 1, d.fb_lease_slot_o);
  check(d.fb_lease_generation_o == 0xBEEFu, "the record holds the granted generation", 0xBEEF,
        d.fb_lease_generation_o);
  check(d.leases_acquired_o == 1, "exactly one lease acquired", 1, d.leases_acquired_o);
  check(d.leases_refused_o == 0, "no refusal on a granted lease", 0, d.leases_refused_o);

  d.blit_req_ready_i = 1;
  d.eval();
  zhao::tick(d);
  d.blit_req_ready_i = 0;
  d.eval();
  check(d.fb_lease_valid_o == 1, "the record is held while the blit runs", 1,
        d.fb_lease_valid_o);
  check(d.fb_lease_generation_o == 0xBEEFu, "the held record is still the granted one", 0xBEEF,
        d.fb_lease_generation_o);
  check(d.blits_dispatched_o == 1, "exactly one blit dispatched", 1, d.blits_dispatched_o);

  d.blit_done_i = 1;
  d.eval();
  zhao::tick(d);
  d.blit_done_i = 0;
  d.eval();
  check(d.idle_o == 1, "the leaf returns to idle", 1, d.idle_o);
  check(d.fb_lease_valid_o == 0, "the record is dropped on retirement", 0, d.fb_lease_valid_o);

  // ---- a REFUSED lease still issues ---------------------------------------
  // Deliberate, and inherited: a refused lease is not the shell's to judge, the
  // blitter answers it with ST_NO_LEASE, and that is a status somebody can
  // read. Dropping the request instead would make a refusal silent.
  dispatch(d, /*slot=*/0, /*mode=*/1);
  accept_manager_request(d);
  give_response(d, /*writer=*/false, /*granted=*/false, /*slot=*/0, /*generation=*/0x4321u);
  check(d.rsp_ready_o == 1, "a refusal is accepted from the channel", 1, d.rsp_ready_o);
  zhao::tick(d);
  d.rsp_valid_i = 0;
  d.eval();
  check(d.blit_req_valid_o == 1, "a refused lease still issues the request", 1,
        d.blit_req_valid_o);
  check(d.fb_lease_valid_o == 0, "a refused lease presents NO valid record", 0,
        d.fb_lease_valid_o);
  check(d.leases_refused_o == 1, "exactly one refusal counted", 1, d.leases_refused_o);
  check(d.leases_acquired_o == 1, "a refusal does not count as an acquisition", 1,
        d.leases_acquired_o);
  retire_blit(d);
  check(d.blits_dispatched_o == 2, "the refused blit was dispatched too", 2,
        d.blits_dispatched_o);

  // ---- closing the epoch does not drop work already owned -----------------
  dispatch(d, /*slot=*/1, /*mode=*/0);
  check(d.mgr_req_valid_o == 1, "the request is live before the epoch closes", 1,
        d.mgr_req_valid_o);
  d.lease_open_i = 0;
  for (int i = 0; i < 6; ++i) zhao::tick(d);
  d.eval();
  check(d.mgr_req_valid_o == 1, "a closed epoch does not retract an owned request", 1,
        d.mgr_req_valid_o);
  accept_manager_request(d);
  give_response(d, /*writer=*/false, /*granted=*/true, /*slot=*/1, /*generation=*/0x0077u);
  zhao::tick(d);
  d.rsp_valid_i = 0;
  d.eval();
  check(d.fb_lease_valid_o == 1, "owned work still completes under a closed epoch", 1,
        d.fb_lease_valid_o);
  retire_blit(d);
  check(d.leases_acquired_o == 2, "two leases acquired in total", 2, d.leases_acquired_o);
  check(d.blits_dispatched_o == 3, "three blits dispatched in total", 3,
        d.blits_dispatched_o);

  // ...and the closed epoch still refuses anything NEW.
  d.dispatch_valid_i = 1;
  d.eval();
  check(d.dispatch_ready_o == 0, "the closed epoch still refuses new work", 0,
        d.dispatch_ready_o);
  d.dispatch_valid_i = 0;
  d.eval();

  std::printf("[blit_lease_v2] acquired=%u refused=%u dispatched=%u\n", d.leases_acquired_o,
              d.leases_refused_o, d.blits_dispatched_o);

  d.final();
  zhao::exit_hard(zhao::report_and_exit("blit_lease_v2_directed"));
#endif
}
