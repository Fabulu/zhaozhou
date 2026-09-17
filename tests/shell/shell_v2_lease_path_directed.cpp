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
// The binding-page seal, modelled once and shared with the binding resolver's
// own directed test rather than folded a second time here.
#include "../harness/zhao_binding_seal.hpp"

using zhao::check;

namespace {

using Dut = Vzhao_shell_v2_lease_path;

// TWO CLOCKS NOW. The bridge is the CDC, and the shell's frozen ratio is
// vid = gpu/2, so the video edge lands on every other GPU edge. The ratio is
// not cosmetic: the bridge's reset-release chains and its barrier are three
// flops in each domain, and a video clock that never ticks leaves the lease
// gate shut forever.
int g_gpu_edges = 0;

void tick(Dut& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
  if ((++g_gpu_edges & 1) == 0) {
    d.vid_clk = 0;
    d.eval();
    d.vid_clk = 1;
    d.eval();
  }
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
  dut.vid_clk = 0;
  dut.vid_rst_n = 0;

  dut.blank_cmd_i = 0;
  dut.scanout_ack_i = 0;
  dut.frame_swap_valid_i = 0;
  dut.frame_swap_slot_i = 0;
  dut.frame_req_valid_i = 0;
  dut.frame_req_mode_i = 0;
  dut.bin_frame_end_i = 0;
  dut.bin_grid_w_i = 2;
  dut.bin_grid_h_i = 2;
  dut.cfg_valid_i = 0;
  dut.cfg_op_i = 0;
  dut.cfg_page_generation_i = 0;
  dut.cfg_selector_i = 0;
  dut.cfg_row_i[0] = 0;
  dut.cfg_row_i[1] = 0;
  dut.cfg_row_i[2] = 0;
  dut.cfg_crc32_i = 0;
  dut.cfg_rsp_ready_i = 1;
  dut.pal_load_valid_i = 0;
  dut.pal_load_op_i = 0;
  dut.pal_load_slot_i = 0;
  dut.pal_load_gen_i = 0;
  dut.pal_load_idx_i = 0;
  dut.pal_load_rgb565_i = 0;
  dut.pal_load_crc_ok_i = 0;
  dut.fault_inject_valid_i = 0;
  dut.fault_inject_writer_i = 0;
  dut.fault_inject_slot_i = 0;
  dut.fault_inject_generation_i = 0;
  dut.frame_ready_i = 0;
  dut.term_valid_i = 0;
  dut.term_slot_i = 0;
  dut.term_generation_i = 0;
  dut.term_publish_i = 0;
  dut.term_fault_i = 0;

  dut.blit_dispatch_valid_i = 0;
  dut.blit_dispatch_slot_i = 0;
  dut.blit_dispatch_mode_i = 0;
  dut.fb_req_ready_i = 0;
  dut.blit_done_i = 0;
  dut.eval();
  tick(dut);
  tick(dut);
  dut.rst_n = 1;
  // THE VIDEO DOMAIN STAYS IN RESET for the barrier-closed case below.
  // `zhao_fb_ready_cdc_v2` raises `vid_barrier_done_o` from its own
  // release chain, and the bridge will not open the lease gate without it,
  // so a video domain still in reset is exactly why the barrier is shut --
  // a real state, and the one the reset-epoch clause is about. Releasing
  // both resets together opened the gate in FOUR cycles and the case had
  // nothing left to observe.
  dut.eval();
  tick(dut);

  // The barriers stay UNDONE here on purpose: that is the state the
  // barrier-closed case below is about, and declaring them done at reset
  // opened the gate in zero cycles and made that case pass for no reason.

  check(dut.requests_accepted_o == 0, "manager starts with no accepted requests", 0,
        dut.requests_accepted_o);
  check(dut.leases_granted_o == 0, "manager starts with no leases granted", 0,
        dut.leases_granted_o);
  check(dut.adapter_idle_o == 1, "adapter starts idle", 1, dut.adapter_idle_o);

  // =======================================================================
  // THE V3 PROGRAMMING CHANNEL, RUN FOR REAL
  // =======================================================================
  //
  // Twenty of the fifty-seven new inputs are this channel, and NOTHING in the
  // tree drives it for real: zhao_prod_top feeds it from generated stimulus
  // slices and the V3 fit top fabricates a sequence so the fitter has
  // something to measure. This runs a legal one -- palette BEGIN, 256 WRITEs,
  // END, then config BEGIN / ROW / END with each response checked -- through
  // the composed bin pipe.
  //
  // The statuses are the point. A config response carrying a non-zero status
  // is how the block reports a rejected program, and the fit top treats it as
  // a setup fault; a test that only checked the handshakes would call a
  // rejected palette a success.
  {
    auto pal = [&](uint8_t op, uint8_t idx, uint16_t rgb) {
      dut.pal_load_op_i = op;
      dut.pal_load_slot_i = 0;
      dut.pal_load_gen_i = 1;
      dut.pal_load_idx_i = idx;
      dut.pal_load_rgb565_i = rgb;
      dut.pal_load_crc_ok_i = 1;
      dut.pal_load_valid_i = 1;
      for (int i = 0; i < 20000; ++i) {
        dut.eval();
        if (dut.pal_load_ready_o) {
          tick(dut);
          dut.pal_load_valid_i = 0;
          dut.eval();
          return true;
        }
        tick(dut);
      }
      dut.pal_load_valid_i = 0;
      return false;
    };

    bool pal_ok = pal(0, 0, 0);  // BEGIN
    for (int idx = 0; idx < 256 && pal_ok; ++idx) {
      // Index 5 gets a distinguishable green, as the V3 fit top does, so the
      // palette is not 256 identical writes that any addressing bug survives.
      pal_ok = pal(1, static_cast<uint8_t>(idx), idx == 5 ? 0x07e0 : 0x0000);
    }
    if (pal_ok) pal_ok = pal(2, 0, 0);  // END
    check(pal_ok, "the palette programs: BEGIN, 256 writes, END", 1, pal_ok);

    uint8_t cfg_gen = 1;
    uint32_t cfg_seal = 0;
    auto cfg = [&](uint8_t op, uint8_t selector, uint32_t row_lo, uint32_t row_hi) {
      dut.cfg_op_i = op;
      dut.cfg_page_generation_i = cfg_gen;
      dut.cfg_selector_i = selector;
      dut.cfg_row_i[0] = row_lo;
      dut.cfg_row_i[1] = row_hi;
      dut.cfg_row_i[2] = (op == 1) ? 0x404u : 0u;
      dut.cfg_crc32_i = cfg_seal;
      dut.cfg_valid_i = 1;
      bool sent = false;
      for (int i = 0; i < 20000 && !sent; ++i) {
        dut.eval();
        if (dut.cfg_ready_o) {
          tick(dut);
          dut.cfg_valid_i = 0;
          dut.eval();
          sent = true;
          break;
        }
        tick(dut);
      }
      dut.cfg_valid_i = 0;
      if (!sent) return -1;
      for (int i = 0; i < 20000; ++i) {
        dut.eval();
        if (dut.cfg_rsp_valid_o) return static_cast<int>(dut.cfg_rsp_status_o);
        tick(dut);
      }
      return -2;
    };

    // row: {11'h404, 32'd0, 32'h0000_2000}, as the V3 fit top programs it.
    const int st_begin = cfg(0, 0, 0, 0);
    check(st_begin == 0, "config BEGIN is accepted with status 0", 0, st_begin);
    tick(dut);
    const int st_row = cfg(1, 1, 0x00002000u, 0);
    check(st_row == 0, "config ROW is accepted with status 0", 0, st_row);
    tick(dut);
    // END SEALS THE TABLE, AND THE SEAL IS ENFORCED. With `cfg_crc32_i` left
    // at zero the block answers **CFG_BAD_CRC (5)** and refuses to activate
    // the page. That is the right answer to an unsealed table and it is the
    // most useful thing this case could have found: the channel is not a
    // pipe that accepts whatever it is handed.
    //
    // So the negative case comes FIRST and is asserted as a refusal. A test
    // that only ever presented a correct seal could not tell a channel that
    // checks it from one that ignores it.
    const int st_end = cfg(2, 0, 0, 0);
    check(st_end == 5, "config END refuses an unsealed table (CFG_BAD_CRC)", 5, st_end);
    for (int i = 0; i < 20; ++i) tick(dut);

    check(dut.active_page_generation_o == 0, "and no page generation activates on a refused seal",
          0, dut.active_page_generation_o);

    // ---- AND NOW A CORRECTLY SEALED PAGE, WHICH ACTIVATES -----------------
    //
    // The seal is `zhao_binding_seal::page_crc`, modelled once in
    // tests/harness/zhao_binding_seal.hpp and shared with the binding
    // resolver's own directed test -- folding it a second time here is the
    // duplication this repository keeps paying for.
    //
    // The row is the one programmed above: `cfg_row_i` = {0x404, 0, 0x2000} at
    // selector 1, which decodes as base 0x2000, palette generation 1, valid.
    // The other 255 selectors are absent and each still folds TEN ZERO BYTES,
    // which is the part of the algorithm that is impossible to guess from
    // looking at one row.
    //
    // A NEW GENERATION, because the refused page cleared its valid mask and
    // the loader will not re-seal a generation it has already rejected.
    cfg_gen = 2;
    zhao_binding_seal::Row sealed_row;
    sealed_row.base = 0x00002000u;
    sealed_row.mode = 0;
    sealed_row.palette_slot = 0;
    sealed_row.palette_generation = 1;
    sealed_row.valid = true;
    const uint32_t seal = zhao_binding_seal::page_crc_single(cfg_gen, 1, sealed_row);

    const int st_begin2 = cfg(0, 0, 0, 0);
    check(st_begin2 == 0, "a second config BEGIN is accepted", 0, st_begin2);
    tick(dut);
    const int st_row2 = cfg(1, 1, 0x00002000u, 0);
    check(st_row2 == 0, "the row is accepted into the new generation", 0, st_row2);
    tick(dut);
    cfg_seal = seal;
    const int st_end2 = cfg(2, 0, 0, 0);
    cfg_seal = 0;
    check(st_end2 == 0, "config END ACCEPTS a correctly sealed table", 0, st_end2);

    // The seal passing is not the page activating. `zhao_texture_binding_resolver_v2`
    // parks in LOAD_SEAL_PENDING and swaps banks only on structural data
    // quiet, which is the whole point of the two-bank design -- work already
    // admitted keeps reading the old page.
    const int to_active = wait_for(
        dut, [&] { return dut.active_page_generation_o == cfg_gen; }, 4000);
    check(to_active >= 0, "the sealed page activates", 1, to_active >= 0);
    check(dut.active_page_generation_o == 2, "the active generation is the one that sealed", 2,
          dut.active_page_generation_o);
    std::printf(
        "[shell_v2_lease_path] v3 programming: palette ok=%d, cfg BEGIN/ROW/END %d/%d/%d, "
        "sealed END %d, seal 0x%08x, active generation %u after %d cycles\n",
        pal_ok ? 1 : 0, st_begin, st_row, st_end, st_end2, seal, dut.active_page_generation_o,
        to_active);
  }

  // ---- FACT 1: the barrier gates CREATION ---------------------------------
  // With lease_open low, a frame request must not become a manager request.
  dut.frame_req_valid_i = 1;
  dut.frame_req_mode_i = 1;  // mode 3 is not a canvas and is never accepted
  for (int i = 0; i < 20; ++i) tick(dut);
  check(dut.requests_accepted_o == 0, "barrier closed: no request reaches the manager", 0,
        dut.requests_accepted_o);

  // ---- wait for the BRIDGE to open the barrier ----------------------------
  // Release the video domain. Nothing here declares the barrier DONE: `zhao_fb_ready_cdc_v2` raises
  // both `barrier_done` outputs from its own reset-release chains and the
  // bridge consumes them, so the whole reset-epoch barrier is self-driven
  // and the test only ever releases the two resets. An earlier version drove
  // the barriers directly and the gate opened in ZERO cycles, which made the
  // barrier-closed case above pass for no reason at all.
  dut.vid_rst_n = 1;
  dut.eval();
  const int to_open = wait_for(
      dut, [&] { return dut.lease_open_o != 0; }, 4000);
  check(to_open >= 0, "the bridge opens the lease gate", 1, to_open >= 0);
  std::printf("[shell_v2_lease_path] lease gate opened after %d cycles\n", to_open);
  // The clear is no longer accepted by the test. `zhao_geom_bin_pipe_v2`
  // accepts it, and only once the binner and tile path are quiet.
  //
  dut.frame_ready_i = 1;  // the render path accepts the frame
  // `ready_ready` is the BRIDGE's now, not the test's.

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
  // Measured from HERE, not from reset: the V3 programming sequence above
  // runs first and takes long enough that the binner is usually already up,
  // in which case this reads 0 and means "already done" rather than
  // "instant".
  std::printf("[shell_v2_lease_path] binner cold init: %d more cycles\n", to_init);

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
  // THE ECHO IS NO LONGER HAND-BUILT. The test used to assemble the six swap
  // fields itself and hand them to the manager, which meant the echo agreed
  // with the publication because the same driver wrote both. Now the bridge
  // holds the published tuple, VIDEO says "I swapped to this slot", and the
  // bridge returns that exact tuple through the reverse CDC. If the layout
  // were wrong, THIS is the path that would carry a plausible wrong slot.
  //
  // The bridge advertises which slot it is holding, one-hot, and refuses a
  // swap for any other -- so the test reads the advertisement rather than
  // assuming it matches the frame.
  const int to_ready_slot = wait_for(
      dut, [&] { return dut.frame_slot_ready_o != 0; }, 4000);
  check(to_ready_slot >= 0, "the bridge advertises a ready slot", 1, to_ready_slot >= 0);
  // The bridge is HOLDING the published tuple, which is the fact the echo
  // depends on. It was 0 here while `ready_events_o` read 1, and that gap is
  // what exposed the missing CDC.
  check(dut.bridge_pending_o == 1, "the bridge holds the published tuple", 1, dut.bridge_pending_o);
  std::printf(
      "[shell_v2_lease_path] bridge: pending=%u slot_ready=%u blank_active=%u "
      "ready_enq=%u ready_deq=%u\n",
      dut.bridge_pending_o, dut.frame_slot_ready_o, dut.blank_active_o, dut.ready_enqueued_o,
      dut.ready_dequeued_o);
  const uint8_t advertised = (dut.frame_slot_ready_o & 0x2u) ? 1 : 0;
  check(advertised == slot, "the advertised slot is the frame's", slot, advertised);

  dut.frame_swap_slot_i = advertised;
  dut.frame_swap_valid_i = 1;
  dut.scanout_ack_i = 1;

  const int to_disp = wait_for(dut, [&] { return dut.displayed_valid_o != 0; });
  check(to_disp >= 0, "the echoed swap becomes the displayed record", 1, to_disp >= 0);
  tick(dut);
  dut.frame_swap_valid_i = 0;
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

  // =======================================================================
  // A STRUCTURAL FAULT SUPPRESSES PUBLICATION AND RELEASES THE LEASE
  // =======================================================================
  //
  // The gate: each of the five structural faults bypasses normal
  // quiet/clear, RELEASES the lease, and produces no READY and no
  // publication. None of them is reachable from a quiescent bin pipe, so the
  // fault is injected at the shell's fault port -- the same port the bin
  // pipe's own structural outputs are aggregated into.
  //
  // THE IDENTITY IS THE POINT. The manager latches a fault only when it
  // MATCHES the live lease. An unmatched fault is silently ignored, the
  // publication proceeds, and the ruined frame is shown -- so the case
  // below drives the live lease's own writer/slot/generation, and a
  // mismatched one is checked first to prove the match is doing work.
  {
    // A fresh lease to ruin.
    dut.frame_req_valid_i = 1;
    dut.frame_req_mode_i = 1;
    const int to_lease2 = wait_for(
        dut, [&] { return dut.lease_valid_o != 0; }, 4000);
    check(to_lease2 >= 0, "a lease is live for the fault case", 1, to_lease2 >= 0);
    dut.frame_req_valid_i = 0;

    const uint32_t pubs_before = dut.publications_o;
    const uint32_t ready_before = dut.ready_events_o;
    const uint32_t faults_before = dut.faults_latched_o;
    const uint32_t pulses_before = dut.fault_pulses_o;
    const uint8_t live_slot = dut.lease_slot_o;
    const uint16_t live_gen = dut.lease_generation_o;

    // FIRST, A FAULT THAT DOES NOT MATCH. It must latch nothing -- otherwise
    // the match below proves only that the port is connected.
    dut.fault_inject_writer_i = 1;
    dut.fault_inject_slot_i = live_slot;
    dut.fault_inject_generation_i = static_cast<uint16_t>(live_gen ^ 0xFFFFu);
    dut.fault_inject_valid_i = 1;
    for (int i = 0; i < 6; ++i) tick(dut);
    dut.fault_inject_valid_i = 0;
    for (int i = 0; i < 4; ++i) tick(dut);
    check(dut.faults_latched_o == faults_before,
          "a fault with the wrong generation latches nothing", faults_before, dut.faults_latched_o);

    // NOW THE MATCHING ONE.
    dut.fault_inject_generation_i = live_gen;
    dut.fault_inject_valid_i = 1;
    for (int i = 0; i < 6; ++i) tick(dut);
    dut.fault_inject_valid_i = 0;
    for (int i = 0; i < 4; ++i) tick(dut);
    // EXACTLY ONCE, not once per cycle. The manager has no "already faulted"
    // guard on this counter and its ready is simply `rst_n`, so a level held
    // across six cycles reads SIX faults -- which is what this measured
    // before the shell edge-detected the port. The bin pipe's structural
    // outputs are levels that stay high until reset, so a shell that wires
    // one straight through reports a fault count of however many cycles the
    // machine sat in the fault.
    check(dut.faults_latched_o == faults_before + 1,
          "a matching fault is latched EXACTLY ONCE, not once per cycle", faults_before + 1,
          dut.faults_latched_o);
    // A DELTA, NOT A TOTAL. The programming phase raises a structural level
    // of its own -- the refused seal -- and the aggregator edge-detects it,
    // so the absolute count is 3 rather than 2. That is correct behaviour and
    // the absolute check was the wrong shape: it measured everything that had
    // ever happened in order to say something about two injections. Note the
    // identity match did its job through all of it: `faults_latched_o` still
    // moved by exactly one, because the programming fault does not carry the
    // live lease's generation.
    check(dut.fault_pulses_o == pulses_before + 2,
          "exactly two fault pulses were presented here (one unmatched, one matched)",
          pulses_before + 2, dut.fault_pulses_o);
    check(dut.lease_fault_o == 1, "the live lease is marked faulted", 1, dut.lease_fault_o);

    // A clean publication offered AFTER the fault must not become one.
    dut.term_slot_i = static_cast<uint8_t>(live_slot);
    dut.term_generation_i = live_gen;
    dut.term_publish_i = 1;
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
    for (int i = 0; i < 30; ++i) tick(dut);

    check(dut.publications_o == pubs_before, "the faulted frame produces NO publication",
          pubs_before, dut.publications_o);
    check(dut.ready_events_o == ready_before, "the faulted frame produces NO ready event",
          ready_before, dut.ready_events_o);
    // NOTE THE ORDER, because this check reads as though the FAULT released
    // the lease and it does not. The publication offer above is what released
    // it -- a terminal event arriving on a lease already marked faulted. The
    // reset-barrier case below holds a faulted lease for 400 cycles with no
    // terminal and it stays live, which is how this was found.
    check(dut.lease_valid_o == 0, "the faulted lease is released", 0, dut.lease_valid_o);
    std::printf(
        "[shell_v2_lease_path] fault: latched %u->%u, publications held at %u, "
        "ready held at %u\n",
        faults_before, dut.faults_latched_o, dut.publications_o, dut.ready_events_o);
  }

  // =======================================================================
  // THE RESET BARRIER RE-ARMS THE FAULT PATH
  // =======================================================================
  //
  // The gate clause asks for the structural faults "each through the reset
  // barrier", and the reason it is phrased that way is specific to this
  // machine: every structural fault in the shell's OR is a STICKY LEVEL,
  // cleared only by its local reset. A fault is therefore not an event the
  // machine recovers from by itself -- once one is up, the OR is up forever,
  // the edge detector will never see another rising edge, and every
  // subsequent fault is invisible.
  //
  // That makes "it latched exactly once" a claim about ONE fault in the life
  // of the machine. The property that matters for a console that has to keep
  // running is the next one: after reset, the latch is clear, the level is
  // down, and the path can fire AGAIN. A machine that faults once and then
  // silently stops reporting is indistinguishable, from the counter, from a
  // machine that never faults again.
  //
  // This is also the only way the six terms are individually meaningful. They
  // are not separately reachable from a quiescent bin pipe -- the test above
  // says so, and injecting six force bits into one OR would be six copies of
  // one test, not six tests. What IS checkable is that the aggregate path is
  // re-armable, and `packet_h_fault_or_parity` carries the other half: that
  // the shell's OR names exactly the terms the harness's does.
  {
    const uint32_t faults_at_reset = dut.faults_latched_o;
    check(faults_at_reset > 0, "a fault was latched before the reset barrier", 1,
          faults_at_reset > 0);

    dut.rst_n = 0;
    dut.vid_rst_n = 0;
    dut.frame_req_valid_i = 0;
    dut.fault_inject_valid_i = 0;
    dut.term_valid_i = 0;
    for (int i = 0; i < 16; ++i) tick(dut);

    check(dut.faults_latched_o == 0, "reset clears the latched fault count", 0,
          dut.faults_latched_o);
    check(dut.lease_valid_o == 0, "reset leaves no live lease", 0, dut.lease_valid_o);

    dut.rst_n = 1;
    dut.vid_rst_n = 1;
    dut.eval();

    // The whole reset epoch runs again from scratch: the CDC raises both
    // barrier-done chains, the bridge opens the gate, and the binner redoes
    // its cold init before the lease will admit anything. Nothing here drives
    // any of that -- driving the barriers directly is what once made the
    // barrier-closed case open in zero cycles and pass for no reason.
    const int to_reopen = wait_for(
        dut, [&] { return dut.lease_open_o != 0; }, 8000);
    check(to_reopen >= 0, "the barrier reopens after the reset", 1, to_reopen >= 0);
    const int to_reinit = wait_for(
        dut, [&] { return dut.bin_initialized_o != 0; }, 8000);
    check(to_reinit >= 0, "the binner redoes its cold init", 1, to_reinit >= 0);

    dut.frame_ready_i = 1;
    dut.frame_req_valid_i = 1;
    dut.frame_req_mode_i = 1;
    const int to_lease3 = wait_for(
        dut, [&] { return dut.lease_valid_o != 0; }, 8000);
    check(to_lease3 >= 0, "a lease is granted after the reset barrier", 1, to_lease3 >= 0);
    dut.frame_req_valid_i = 0;

    const uint32_t pubs_before = dut.publications_o;
    const uint32_t faults_before = dut.faults_latched_o;
    check(faults_before == 0, "no fault is latched on the re-armed machine", 0, faults_before);

    // THE SECOND FAULT, on the second lease, after the barrier. If the level
    // had stayed up across reset this would latch nothing at all.
    dut.fault_inject_writer_i = 1;
    dut.fault_inject_slot_i = dut.lease_slot_o;
    dut.fault_inject_generation_i = dut.lease_generation_o;
    dut.fault_inject_valid_i = 1;
    for (int i = 0; i < 6; ++i) tick(dut);
    dut.fault_inject_valid_i = 0;
    for (int i = 0; i < 8; ++i) tick(dut);

    check(dut.faults_latched_o == faults_before + 1,
          "a fault AFTER the reset barrier is latched, exactly once", faults_before + 1,
          dut.faults_latched_o);
    check(dut.lease_fault_o == 1, "the second lease is marked faulted", 1, dut.lease_fault_o);
    // THE FAULT ALONE DOES NOT RELEASE THE LEASE, and finding that out here
    // corrects what the first fault case appeared to show. That case injected
    // the fault, then offered a publication, then checked `lease_valid_o == 0`
    // -- so the release read as a consequence of the FAULT when it is actually
    // a consequence of the TERMINAL EVENT arriving on a lease already marked
    // faulted. Waiting 400 cycles here with no terminal offer releases nothing.
    //
    // Which is correct behaviour, and better behaviour than the reading it
    // replaces: a fault does not abandon the slot on its own, because the
    // writer may still be mid-transfer into it. The terminal event is what
    // says the writer is finished, and only then is the slot safe to retire.
    // The fault decides the frame is not PUBLISHED; the terminal decides the
    // lease is DONE.
    check(dut.lease_valid_o == 1, "the faulted lease is still held until its terminal arrives", 1,
          dut.lease_valid_o);

    // AND WHILE IT IS HELD, NOTHING ELSE CAN BE GRANTED. This is the other
    // half of the same fact and it is the half with teeth.
    //
    // `zhao_video_slotmgr_v2` clears `lease_valid_q` in exactly one place
    // outside reset: `term_fire_c && term_match_c`. There is no timeout, no
    // fault-driven release, no reclaim. And `request_granted_c` requires
    // `!lease_valid_q`. So a lease whose terminal never arrives is not a
    // stalled frame -- it is a PERMANENTLY WEDGED RENDERER, with
    // `faults_latched_o` reading 1 and every other counter looking healthy.
    //
    // Today that state is unreachable in this shell: the only producer that
    // could fail to emit a terminal is the V3 return path, and it is tied off
    // until Packet J. This check asserts the dependency so that when J wires
    // that producer up, the sequence-abort RELEASE control is not optional --
    // it is what stands between a sequence abort and a dead console.
    const uint32_t granted_before = dut.leases_granted_o;
    dut.frame_req_valid_i = 1;
    dut.frame_req_mode_i = 1;
    for (int i = 0; i < 300; ++i) tick(dut);
    dut.frame_req_valid_i = 0;
    check(dut.leases_granted_o == granted_before,
          "no new lease is granted while a faulted one is held", granted_before,
          dut.leases_granted_o);

    dut.term_slot_i = static_cast<uint8_t>(dut.lease_slot_o);
    dut.term_generation_i = dut.lease_generation_o;
    dut.term_publish_i = 1;
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
    const int to_release = wait_for(
        dut, [&] { return dut.lease_valid_o == 0; }, 400);
    check(to_release >= 0, "the second faulted lease is released", 1, to_release >= 0);
    check(dut.publications_o == pubs_before, "the second faulted frame produces NO publication",
          pubs_before, dut.publications_o);
    std::printf(
        "[shell_v2_lease_path] reset barrier: latched %u -> cleared -> %u, "
        "reopen %d cycles, re-init %d, lease %d, release %d\n",
        faults_at_reset, dut.faults_latched_o, to_reopen, to_reinit, to_lease3, to_release);
  }

  std::printf(
      "[shell_v2_lease_path] lease->frame %d cycles, frame->term %d, term->ready %d, "
      "ready->displayed %d\n",
      to_lease, to_frame, to_term, to_disp);

  dut.final();
  zhao::exit_hard(zhao::report_and_exit("shell_v2_lease_path_directed"));
}
