// packet_g_connected_directed.cpp -- connected lease -> CDC -> frame -> echo gate.
#if defined(ZHAO_EXPECT_PACKET_G_CONNECTED_RAW_TERMINAL)
#define ZHAO_CONNECTED_MUTANT_MODE 1
#else
#define ZHAO_CONNECTED_MUTANT_MODE 0
#endif

#include <cstdint>
#include <cstdio>

#include "verilated.h"
#include "Vtb_packet_g_lease_cdc.h"
#include "zhao_sim.hpp"

namespace {

using Dut = Vtb_packet_g_lease_cdc;
constexpr uint8_t kFree = 0, kReady = 2, kDisplayed = 3;
constexpr uint32_t kBase[2] = {0x00000000u, 0x02000000u};
constexpr uint32_t kSpan[3] = {184320u, 153600u, 196608u};

struct Lease {
  bool writer;
  uint8_t slot;
  uint16_t generation;
  uint8_t mode;
  uint32_t base;
  uint32_t span;
};

void check(bool condition, const char* label, uint64_t expected = 1,
           uint64_t actual = 0) {
  zhao::check(condition, label, expected, condition ? expected : actual);
}

void clear_inputs(Dut& d) {
  d.render_req_valid_i = 0;
  d.render_req_slot_i = 0;
  d.render_req_mode_i = 0;
  d.blit_req_valid_i = 0;
  d.blit_req_slot_i = 0;
  d.blit_req_mode_i = 0;
  d.rsp_ready_i = 0;
  d.fault_valid_i = 0;
  d.fault_writer_i = 0;
  d.fault_slot_i = 0;
  d.fault_generation_i = 0;
  d.term_valid_i = 0;
  d.term_writer_i = 0;
  d.term_slot_i = 0;
  d.term_generation_i = 0;
  d.term_publish_i = 0;
  d.term_fault_i = 0;
  d.frame_start_i = 0;
  d.frame_boundary_i = 0;
  d.render_guard_valid_i = 0;
  d.render_guard_addr_i = 0;
  d.render_guard_len_i = 1;
  d.blit_guard_valid_i = 0;
  d.blit_guard_addr_i = 0;
  d.blit_guard_len_i = 1;
}

struct ClockSim {
  Dut& d;
  uint64_t next_gpu = 2;
  uint64_t next_vid = 5;
  uint64_t gpu_period = 6;
  uint64_t vid_period = 10;

  unsigned next_mask() const {
    const uint64_t next = next_gpu < next_vid ? next_gpu : next_vid;
    return (next_gpu == next ? 1u : 0u) | (next_vid == next ? 2u : 0u);
  }

  unsigned event() {
    const unsigned mask = next_mask();
    d.gpu_clk = (mask & 1u) != 0;
    d.vid_clk = (mask & 2u) != 0;
    d.eval();
    d.gpu_clk = 0;
    d.vid_clk = 0;
    d.eval();
    if (mask & 1u) next_gpu += gpu_period;
    if (mask & 2u) next_vid += vid_period;
    return mask;
  }

  void events(int count) {
    for (int i = 0; i < count; ++i) event();
  }

  void vid_edges(int count) {
    while (count > 0) if (event() & 2u) --count;
  }
};

void reset(Dut& d, ClockSim& sim) {
  clear_inputs(d);
  d.gpu_clk = 0;
  d.vid_clk = 0;
  d.gpu_rst_n = 0;
  d.vid_rst_n = 0;
  d.eval();
  sim.events(8);
  d.gpu_rst_n = 1;
  d.vid_rst_n = 1;
  d.eval();
  for (int i = 0; i < 300; ++i) {
    if (d.gpu_barrier_done_o && d.vid_barrier_done_o) return;
    sim.event();
  }
  check(false, "connected reset barriers complete", 1, 0);
}

Lease request(ClockSim& sim, bool writer, uint8_t slot, uint8_t mode) {
  Dut& d = sim.d;
  if (writer) {
    d.render_req_valid_i = 1;
    d.render_req_slot_i = slot;
    d.render_req_mode_i = mode;
  } else {
    d.blit_req_valid_i = 1;
    d.blit_req_slot_i = slot;
    d.blit_req_mode_i = mode;
  }
  d.eval();
  bool accepted = false;
  for (int i = 0; i < 200 && !accepted; ++i) {
    accepted = (sim.next_mask() & 1u) &&
               (writer ? d.render_req_ready_o : d.blit_req_ready_o);
    sim.event();
  }
  if (writer) d.render_req_valid_i = 0;
  else d.blit_req_valid_i = 0;
  d.eval();
  check(accepted && d.rsp_valid_o && d.rsp_granted_o,
        "connected request produces granted response", 1, accepted);
  Lease lease{d.rsp_writer_o != 0, static_cast<uint8_t>(d.rsp_slot_o),
              static_cast<uint16_t>(d.rsp_generation_o),
              static_cast<uint8_t>(d.rsp_mode_o),
              static_cast<uint32_t>(d.rsp_base_o),
              static_cast<uint32_t>(d.rsp_span_o)};
  d.rsp_ready_i = 1;
  bool response = false;
  for (int i = 0; i < 100 && !response; ++i) {
    response = (sim.next_mask() & 1u) && d.rsp_valid_o;
    sim.event();
  }
  d.rsp_ready_i = 0;
  d.eval();
  check(response && d.lease_valid_o, "connected grant creates live lease", 1,
        response);
  return lease;
}

void terminal(ClockSim& sim, const Lease& lease, bool publish,
              bool fault, bool simultaneous_fault) {
  Dut& d = sim.d;
  d.term_valid_i = 1;
  d.term_writer_i = lease.writer;
  d.term_slot_i = lease.slot;
  d.term_generation_i = lease.generation;
  d.term_publish_i = publish;
  d.term_fault_i = fault;
  d.fault_valid_i = simultaneous_fault;
  d.fault_writer_i = lease.writer;
  d.fault_slot_i = lease.slot;
  d.fault_generation_i = lease.generation;
  d.eval();
  bool accepted = false;
  for (int i = 0; i < 200 && !accepted; ++i) {
    accepted = (sim.next_mask() & 1u) && d.term_ready_o &&
               (!simultaneous_fault || d.fault_ready_o);
    sim.event();
  }
  d.term_valid_i = 0;
  d.term_fault_i = 0;
  d.fault_valid_i = 0;
  d.eval();
  check(accepted, "connected terminal accepted", 1, accepted);
}

struct GuardResult { bool accepted; bool ok; bool violation; bool forwarded; };

GuardResult guard_request(ClockSim& sim, bool render_guard, uint32_t address,
                          uint8_t length) {
  Dut& d = sim.d;
  if (render_guard) {
    d.render_guard_addr_i = address;
    d.render_guard_len_i = length;
    d.render_guard_valid_i = 1;
  } else {
    d.blit_guard_addr_i = address;
    d.blit_guard_len_i = length;
    d.blit_guard_valid_i = 1;
  }
  d.eval();
  bool accepted = false;
  for (int i = 0; i < 100 && !accepted; ++i) {
    accepted = (sim.next_mask() & 1u) &&
               (render_guard ? d.render_guard_ready_o : d.blit_guard_ready_o);
    sim.event();
  }
  if (render_guard) d.render_guard_valid_i = 0;
  else d.blit_guard_valid_i = 0;
  d.eval();
  GuardResult result{
      accepted,
      (render_guard ? d.render_guard_ok_o : d.blit_guard_ok_o) != 0,
      (render_guard ? d.render_guard_violation_o : d.blit_guard_violation_o) != 0,
      (render_guard ? d.render_guard_arb_valid_o : d.blit_guard_arb_valid_o) != 0};
  // Let a forwarded request leave its guard before the next probe.
  while (!(sim.next_mask() & 1u)) sim.event();
  sim.event();
  return result;
}

struct BoundaryResult { bool swap; bool repeated; bool slot; };

BoundaryResult boundary(ClockSim& sim) {
  Dut& d = sim.d;
  d.frame_start_i = 1;
  d.eval();
  while (!(sim.next_mask() & 2u)) sim.event();
  sim.event();
  d.frame_start_i = 0;
  d.eval();
  sim.vid_edges(3);
  d.frame_boundary_i = 1;
  d.eval();
  while (!(sim.next_mask() & 2u)) sim.event();
  const bool swap = d.frame_swap_req_o != 0;
  const bool slot = d.frame_swap_slot_o != 0;
  sim.event();
  d.frame_boundary_i = 0;
  d.eval();
  return BoundaryResult{swap, d.frame_repeated_o != 0, slot};
}

bool wait_pending(ClockSim& sim, bool value, int limit = 500) {
  for (int i = 0; i < limit; ++i) {
    if ((sim.d.video_pending_o != 0) == value) return true;
    sim.event();
  }
  return false;
}

bool wait_swaps(ClockSim& sim, uint32_t count, int limit = 500) {
  for (int i = 0; i < limit; ++i) {
    if (sim.d.manager_swaps_o == count) return true;
    sim.event();
  }
  return false;
}

[[noreturn]] void mutant_result(bool detected) {
  std::printf("[packet_g_connected_raw_terminal] %s\n",
              detected ? "DETECTED" : "MISSED");
  zhao::exit_hard(detected ? 0 : 1);
}

}  // namespace

int main() {
  Dut d;
  ClockSim sim{d};
  reset(d, sim);

#if ZHAO_CONNECTED_MUTANT_MODE
  const Lease lease = request(sim, true, 0, 0);
  terminal(sim, lease, true, false, true);
  const bool pending = wait_pending(sim, true);
  const BoundaryResult result = boundary(sim);
  sim.events(100);
  mutant_result(pending && result.swap && !result.repeated &&
                d.cdc_swap_enqueued_o == 1 && d.manager_swaps_o == 0 &&
                !d.displayed_valid_o);
#else
  check(d.gpu_barrier_done_o && d.vid_barrier_done_o,
        "connected paired barriers opened", 1, 1);

  // No accepted READY means every boundary repeats and no swap enters either CDC.
  for (int frame = 0; frame < 3; ++frame) {
    const BoundaryResult result = boundary(sim);
    check(!result.swap && result.repeated,
          "empty connected frame boundary repeats without swap", 1, 1);
  }
  check(d.cdc_swap_enqueued_o == 0 && d.manager_swaps_o == 0,
        "empty boundaries emit no swap traffic", 0,
        d.cdc_swap_enqueued_o + d.manager_swaps_o);

  // Renderer owns slot 1 / Duo. Both real guards see the same captured lease.
  const Lease renderer = request(sim, true, 1, 2);
  check(renderer.writer && renderer.slot == 1 && renderer.mode == 2 &&
            renderer.base == kBase[1] && renderer.span == kSpan[2],
        "renderer lease carries exact internally derived window", 1, 1);
  GuardResult gr = guard_request(sim, true, renderer.base, 4);
  check(gr.accepted && gr.ok && !gr.violation && gr.forwarded,
        "renderer guard accepts matching writer inside lease", 1, 1);
  gr = guard_request(sim, false, renderer.base, 4);
  check(gr.accepted && !gr.ok && gr.violation && !gr.forwarded,
        "blitter guard refuses wrong writer inside same window", 1, 1);
  gr = guard_request(sim, true, renderer.base - 1u, 1);
  check(gr.accepted && gr.violation, "renderer guard refuses lower boundary", 1, 1);
  gr = guard_request(sim, true, renderer.base + renderer.span - 4u, 4);
  check(gr.accepted && gr.ok, "renderer guard accepts exact upper interior", 1, 1);
  gr = guard_request(sim, true, renderer.base + renderer.span, 1);
  check(gr.accepted && gr.violation, "renderer guard refuses upper boundary", 1, 1);

  terminal(sim, renderer, true, false, false);
  check(d.manager_publications_o == 1 && d.manager_ready_events_o == 1 &&
            d.manager_releases_o == 0,
        "clean renderer terminal creates one publication and READY", 1, 1);
  check(wait_pending(sim, true), "READY CDC reaches video pending record", 1, 1);
  sim.vid_edges(6);  // pending tuple is retained until a real boundary.
  check(d.video_pending_o && d.manager_swaps_o == 0,
        "video pending tuple holds before boundary", 1, 1);
  BoundaryResult result = boundary(sim);
  check(result.swap && !result.repeated && result.slot,
        "one READY causes one slot-1 frame-control swap", 1, 1);
  check(wait_swaps(sim, 1), "unchanged swap echo returns to manager", 1, 1);
  check(d.displayed_valid_o && d.displayed_writer_o && d.displayed_slot_o &&
            d.displayed_generation_o == renderer.generation &&
            d.displayed_mode_o == renderer.mode &&
            d.displayed_base_o == renderer.base &&
            d.displayed_span_o == renderer.span &&
            d.slot1_state_o == kDisplayed,
        "returned full tuple becomes exact displayed renderer frame", 1, 1);
  check(d.video_ready_captured_o == 1 && d.video_echoes_o == 1 &&
            d.cdc_ready_enqueued_o == 1 && d.cdc_ready_dequeued_o == 1 &&
            d.cdc_swap_enqueued_o == 1 && d.cdc_swap_dequeued_o == 1,
        "connected READY and echo accounting is exactly one", 1, 1);

  for (int frame = 0; frame < 3; ++frame) {
    result = boundary(sim);
    check(!result.swap && result.repeated,
          "post-swap empty boundary repeats without duplicate", 1, 1);
  }
  check(d.manager_swaps_o == 1 && d.cdc_swap_enqueued_o == 1,
        "one publication cannot create duplicate swaps", 1, 1);

  // Blitter owns now-free slot 0 / Storm. Guard polarity reverses with writer.
  const Lease blitter = request(sim, false, 0, 1);
  check(!blitter.writer && blitter.slot == 0 && blitter.mode == 1 &&
            blitter.base == kBase[0] && blitter.span == kSpan[1],
        "blitter lease carries exact internally derived window", 1, 1);
  gr = guard_request(sim, false, blitter.base + blitter.span - 8u, 8);
  check(gr.accepted && gr.ok && gr.forwarded,
        "blitter guard accepts matching writer at upper interior", 1, 1);
  gr = guard_request(sim, true, blitter.base, 4);
  check(gr.accepted && gr.violation && !gr.forwarded,
        "renderer guard refuses wrong writer during blitter lease", 1, 1);
  gr = guard_request(sim, false, blitter.base + blitter.span, 1);
  check(gr.accepted && gr.violation, "blitter guard refuses upper boundary", 1, 1);

  const uint32_t ready_before_fault = d.cdc_ready_enqueued_o;
  terminal(sim, blitter, true, false, true);
  check(d.manager_releases_o == 1 && d.manager_publications_o == 1 &&
            d.manager_ready_events_o == 1 &&
            d.cdc_ready_enqueued_o == ready_before_fault,
        "same-edge fault release creates no READY CDC write", 1, 1);
  sim.events(100);
  check(!d.video_pending_o, "fault release creates no video pending tuple", 0,
        d.video_pending_o);
  result = boundary(sim);
  check(!result.swap && result.repeated && d.manager_swaps_o == 1,
        "fault-release frame repeats without a swap", 1, 1);

  // Reset with a video-pending event discards it before any frame decision.
  const Lease pending_reset = request(sim, true, 0, 0);
  terminal(sim, pending_reset, true, false, false);
  check(wait_pending(sim, true), "connected reset setup reaches pending", 1, 1);
  d.gpu_rst_n = 0;
  d.vid_rst_n = 0;
  d.eval();
  check(!d.video_pending_o && !d.video_swap_hold_o &&
            !d.displayed_valid_o && d.cdc_ready_enqueued_o == 0 &&
            d.cdc_swap_enqueued_o == 0,
        "connected reset flushes manager, adapters, and both CDCs", 1, 1);
  sim.events(8);
  d.gpu_rst_n = 1;
  d.vid_rst_n = 1;
  d.eval();
  for (int i = 0; i < 300 &&
       !(d.gpu_barrier_done_o && d.vid_barrier_done_o); ++i) sim.event();
  result = boundary(sim);
  check(!result.swap && result.repeated && d.manager_swaps_o == 0,
        "pre-reset pending event cannot swap after barrier", 1, 1);

  // Reset the registered echo hold before its next VID edge can enqueue it.
  const Lease echo_reset = request(sim, true, 0, 0);
  terminal(sim, echo_reset, true, false, false);
  check(wait_pending(sim, true), "echo-reset setup reaches pending", 1, 1);
  result = boundary(sim);
  check(result.swap && d.video_swap_hold_o,
        "frame boundary creates a held full-tuple echo", 1, 1);
  d.gpu_rst_n = 0;
  d.vid_rst_n = 0;
  d.eval();
  check(!d.video_swap_hold_o && d.cdc_swap_enqueued_o == 0,
        "reset clears held echo before CDC acceptance", 1, 1);
  sim.events(8);
  d.gpu_rst_n = 1;
  d.vid_rst_n = 1;
  d.eval();
  for (int i = 0; i < 300 &&
       !(d.gpu_barrier_done_o && d.vid_barrier_done_o); ++i) sim.event();
  sim.events(100);
  check(d.manager_swaps_o == 0 && !d.displayed_valid_o,
        "pre-reset held echo cannot reach manager", 1, 1);

  return zhao::report_and_exit("packet_g_connected_directed");
#endif
}
