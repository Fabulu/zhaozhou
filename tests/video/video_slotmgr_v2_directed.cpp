// video_slotmgr_v2_directed.cpp -- Packet-G dual-writer lease authority.
#if (defined(ZHAO_EXPECT_SLOT_MUTANT_TERM_OMIT_WRITER) +     \
     defined(ZHAO_EXPECT_SLOT_MUTANT_TERM_SLOT_ONLY) +       \
     defined(ZHAO_EXPECT_SLOT_MUTANT_FAULT_PUBLISHES) +      \
     defined(ZHAO_EXPECT_SLOT_MUTANT_SLOT0_BASE) +           \
     defined(ZHAO_EXPECT_SLOT_MUTANT_SWAP_OMIT_GENERATION) + \
     defined(ZHAO_EXPECT_SLOT_MUTANT_LIVE_READY_WRITER)) > 1
#error ZHAO_VIDEO_SLOTMGR_V2_CPP_MUTANT_SELECTOR_COLLISION
#endif

#include <cstdint>
#include <cstdio>

#include "verilated.h"
#include "Vzhao_video_slotmgr_v2.h"
#include "zhao_sim.hpp"

namespace {

using Dut = Vzhao_video_slotmgr_v2;
constexpr uint8_t kFree = 0, kWriting = 1, kReady = 2, kDisplayed = 3;
constexpr uint32_t kBase[2] = {0x00000000u, 0x02000000u};
constexpr uint32_t kSpan[3] = {184320u, 153600u, 196608u};

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
  d.ready_ready_i = 0;
  d.swap_valid_i = 0;
  d.swap_writer_i = 0;
  d.swap_slot_i = 0;
  d.swap_generation_i = 0;
  d.swap_mode_i = 0;
  d.swap_base_i = 0;
  d.swap_span_i = 0;
}

void reset(Dut& d) {
  clear_inputs(d);
  d.rst_n = 0;
  d.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

uint8_t state(Dut& d, int slot) { return static_cast<uint8_t>(d.slot_state_o[slot]); }

struct Lease {
  bool writer;
  bool granted;
  uint8_t slot;
  uint16_t generation;
  uint8_t mode;
  uint32_t base;
  uint32_t span;
};

Lease request(Dut& d, bool writer, uint8_t slot, uint8_t mode, int hold_response_cycles = 0) {
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
  const bool accepted = writer ? d.render_req_ready_o : d.blit_req_ready_o;
  zhao::check(accepted, "request accepted", 1, accepted ? 1 : 0);
  zhao::tick(d);
  d.render_req_valid_i = 0;
  d.blit_req_valid_i = 0;
  d.eval();

  zhao::check(d.rsp_valid_o, "response produced", 1, d.rsp_valid_o);
  Lease result{d.rsp_writer_o != 0,
               d.rsp_granted_o != 0,
               static_cast<uint8_t>(d.rsp_slot_o),
               static_cast<uint16_t>(d.rsp_generation_o),
               static_cast<uint8_t>(d.rsp_mode_o),
               static_cast<uint32_t>(d.rsp_base_o),
               static_cast<uint32_t>(d.rsp_span_o)};
  for (int i = 0; i < hold_response_cycles; ++i) {
    zhao::tick(d);
    d.eval();
    zhao::check(d.rsp_valid_o && d.rsp_writer_o == result.writer &&
                    d.rsp_granted_o == result.granted && d.rsp_slot_o == result.slot &&
                    d.rsp_generation_o == result.generation && d.rsp_mode_o == result.mode &&
                    d.rsp_base_o == result.base && d.rsp_span_o == result.span,
                "held response tuple stable", 1, 1);
  }
  d.rsp_ready_i = 1;
  zhao::tick(d);
  d.rsp_ready_i = 0;
  d.eval();
  return result;
}

void terminal(Dut& d, const Lease& lease, bool publish, bool fault = false,
              bool writer_override = false, bool use_writer_override = false,
              uint8_t slot_override = 0xff, uint16_t generation_override = 0xffff) {
  d.term_valid_i = 1;
  d.term_writer_i = use_writer_override ? writer_override : lease.writer;
  d.term_slot_i = slot_override == 0xff ? lease.slot : slot_override;
  d.term_generation_i = generation_override == 0xffff ? lease.generation : generation_override;
  d.term_publish_i = publish;
  d.term_fault_i = fault;
  d.eval();
  zhao::check(d.term_ready_o, "terminal accepted", 1, d.term_ready_o);
  zhao::tick(d);
  d.term_valid_i = 0;
  d.term_fault_i = 0;
  d.eval();
}

void matching_fault(Dut& d, const Lease& lease) {
  d.fault_valid_i = 1;
  d.fault_writer_i = lease.writer;
  d.fault_slot_i = lease.slot;
  d.fault_generation_i = lease.generation;
  d.eval();
  zhao::check(d.fault_ready_o, "fault accepted", 1, d.fault_ready_o);
  zhao::tick(d);
  d.fault_valid_i = 0;
  d.eval();
}

void swap(Dut& d, const Lease& lease, bool writer, uint8_t slot, uint16_t generation, uint8_t mode,
          uint32_t base, uint32_t span) {
  d.swap_valid_i = 1;
  d.swap_writer_i = writer;
  d.swap_slot_i = slot;
  d.swap_generation_i = generation;
  d.swap_mode_i = mode;
  d.swap_base_i = base;
  d.swap_span_i = span;
  d.eval();
  zhao::check(d.swap_ready_o, "swap accepted", 1, d.swap_ready_o);
  zhao::tick(d);
  d.swap_valid_i = 0;
  d.eval();
}

void swap_exact(Dut& d, const Lease& lease) {
  swap(d, lease, lease.writer, lease.slot, lease.generation, lease.mode, lease.base, lease.span);
}

[[noreturn]] void mutant_result(const char* name, bool detected) {
  std::printf("[%s] %s\n", name, detected ? "DETECTED" : "MISSED");
  zhao::exit_hard(detected ? 0 : 1);
}

}  // namespace

int main() {
  Dut d;
  reset(d);

#if defined(ZHAO_EXPECT_SLOT_MUTANT_TERM_OMIT_WRITER)
  const Lease l = request(d, true, 0, 0);
  const uint32_t stale = d.stale_events_o;
  terminal(d, l, true, false, false, true);
  mutant_result("slotmgr_v2_term_omit_writer", state(d, 0) == kReady && d.stale_events_o == stale);
#elif defined(ZHAO_EXPECT_SLOT_MUTANT_TERM_SLOT_ONLY)
  const Lease l = request(d, true, 0, 0);
  const uint32_t stale = d.stale_events_o;
  terminal(d, l, true, false, false, true, 0, l.generation + 7u);
  mutant_result("slotmgr_v2_term_slot_only", state(d, 0) == kReady && d.stale_events_o == stale);
#elif defined(ZHAO_EXPECT_SLOT_MUTANT_FAULT_PUBLISHES)
  const Lease l = request(d, true, 0, 0);
  d.fault_valid_i = 1;
  d.fault_writer_i = l.writer;
  d.fault_slot_i = l.slot;
  d.fault_generation_i = l.generation;
  d.term_valid_i = 1;
  d.term_writer_i = l.writer;
  d.term_slot_i = l.slot;
  d.term_generation_i = l.generation;
  d.term_publish_i = 1;
  d.eval();
  zhao::tick(d);
  mutant_result("slotmgr_v2_fault_publishes", d.ready_valid_o && state(d, 0) == kReady);
#elif defined(ZHAO_EXPECT_SLOT_MUTANT_SLOT0_BASE)
  const Lease l = request(d, false, 1, 0);
  mutant_result("slotmgr_v2_slot0_base", l.base == kBase[0]);
#elif defined(ZHAO_EXPECT_SLOT_MUTANT_SWAP_OMIT_GENERATION)
  const Lease l = request(d, true, 0, 1);
  terminal(d, l, true);
  swap(d, l, l.writer, l.slot, static_cast<uint16_t>(l.generation + 1u), l.mode, l.base, l.span);
  mutant_result("slotmgr_v2_swap_omit_generation", state(d, 0) == kDisplayed);
#elif defined(ZHAO_EXPECT_SLOT_MUTANT_LIVE_READY_WRITER)
  const Lease l = request(d, true, 0, 2);
  terminal(d, l, true);
  mutant_result("slotmgr_v2_live_ready_writer", d.ready_valid_o && !d.ready_writer_o);
#else
  // Reset closes every acceptance path and clears every ownership record.
  d.rst_n = 0;
  d.eval();
  zhao::check(!d.render_req_ready_o && !d.blit_req_ready_o && !d.fault_ready_o && !d.term_ready_o &&
                  !d.swap_ready_o,
              "reset closes all acceptance paths", 1, 1);
  zhao::check(!d.rsp_valid_o && !d.ready_valid_o && !d.lease_valid_o,
              "reset clears held and live records", 1, 1);
  d.rst_n = 1;
  d.eval();

  // Every slot/mode pair derives its own immutable base/span internally.
  for (uint8_t slot = 0; slot < 2; ++slot) {
    for (uint8_t mode = 0; mode < 3; ++mode) {
      reset(d);
      const Lease l = request(d, (slot ^ mode) != 0, slot, mode, 3);
      zhao::check(l.granted, "legal request granted", 1, l.granted);
      zhao::check(l.slot == slot && l.mode == mode, "response identity exact", 1, 1);
      zhao::check(l.base == kBase[slot], "derived slot base exact", kBase[slot], l.base);
      zhao::check(l.span == kSpan[mode], "derived mode span exact", kSpan[mode], l.span);
      zhao::check(d.lease_valid_o && d.lease_writer_o == l.writer && d.lease_slot_o == l.slot &&
                      d.lease_generation_o == l.generation && d.lease_mode_o == l.mode &&
                      d.lease_base_o == l.base && d.lease_span_o == l.span,
                  "accepted response creates exact live lease", 1, 1);
      terminal(d, l, false);
      zhao::check(state(d, slot) == kFree && !d.ready_valid_o, "cancel frees without READY", 1, 1);
    }
  }

  // Illegal mode and occupied/live conflicts return held refusals.
  reset(d);
  const Lease live = request(d, true, 0, 0);
  const Lease refused_live = request(d, false, 1, 1, 2);
  zhao::check(!refused_live.granted && state(d, 1) == kFree, "second live lease refused", 1, 1);
  terminal(d, live, false);
  const Lease refused_mode = request(d, false, 1, 3);
  zhao::check(!refused_mode.granted && refused_mode.span == 0,
              "illegal mode refused with zero span", 1, 1);

  // Contention priority alternates across repeated grants even when each loser
  // remains held through the winning response stall and is then explicitly
  // refused while that winning lease is live.
  reset(d);
  unsigned render_grants = 0, blit_grants = 0;
  for (unsigned round = 0; round < 16; ++round) {
    const bool expect_render = (round & 1u) == 0;
    d.render_req_valid_i = 1;
    d.render_req_slot_i = 0;
    d.render_req_mode_i = 1;
    d.blit_req_valid_i = 1;
    d.blit_req_slot_i = 1;
    d.blit_req_mode_i = 2;
    d.eval();
    zhao::check(
        (d.render_req_ready_o != 0) == expect_render && (d.blit_req_ready_o != 0) == !expect_render,
        "repeated contention alternates accepted winner", 1, 1);
    zhao::tick(d);

    Lease winner{d.rsp_writer_o != 0,
                 d.rsp_granted_o != 0,
                 static_cast<uint8_t>(d.rsp_slot_o),
                 static_cast<uint16_t>(d.rsp_generation_o),
                 static_cast<uint8_t>(d.rsp_mode_o),
                 static_cast<uint32_t>(d.rsp_base_o),
                 static_cast<uint32_t>(d.rsp_span_o)};
    zhao::check(d.rsp_valid_o && winner.granted && winner.writer == expect_render,
                "contention winner receives held grant", 1, 1);
    if (expect_render) {
      ++render_grants;
      d.render_req_valid_i = 0;
    } else {
      ++blit_grants;
      d.blit_req_valid_i = 0;
    }
    d.eval();
    for (int stall = 0; stall < 3; ++stall) {
      zhao::check(!d.render_req_ready_o && !d.blit_req_ready_o,
                  "held loser is backpressured by winning response", 1, 1);
      zhao::check(d.rsp_valid_o && d.rsp_writer_o == winner.writer &&
                      d.rsp_generation_o == winner.generation,
                  "winning grant remains stable under response stall", 1, 1);
      zhao::tick(d);
    }

    d.rsp_ready_i = 1;
    zhao::tick(d);
    d.rsp_ready_i = 0;
    d.eval();
    zhao::check(d.lease_valid_o && d.lease_writer_o == winner.writer &&
                    d.lease_generation_o == winner.generation,
                "accepted winning response creates its lease", 1, 1);

    // The still-held loser now receives a typed refusal, rather than vanishing.
    zhao::check(expect_render ? d.blit_req_ready_o : d.render_req_ready_o,
                "held loser eventually receives request acceptance", 1, 1);
    zhao::tick(d);
    d.render_req_valid_i = 0;
    d.blit_req_valid_i = 0;
    d.eval();
    zhao::check(d.rsp_valid_o && !d.rsp_granted_o && (d.rsp_writer_o != winner.writer) &&
                    d.rsp_slot_o == (expect_render ? 1 : 0) &&
                    d.rsp_mode_o == (expect_render ? 2 : 1),
                "held loser receives exact refusal identity", 1, 1);
    d.rsp_ready_i = 1;
    zhao::tick(d);
    d.rsp_ready_i = 0;
    d.eval();
    terminal(d, winner, false);
  }
  zhao::check(render_grants == 8 && blit_grants == 8,
              "neither writer starves across repeated contentions", 8,
              render_grants < blit_grants ? render_grants : blit_grants);
  zhao::check(d.contentions_o == 16, "every repeated contention counted", 16, d.contentions_o);
  zhao::check(d.requests_accepted_o == 32 && d.responses_accepted_o == 32 &&
                  d.leases_granted_o == 16 && d.leases_refused_o == 16,
              "contention grants/refusals account exactly", 1, 1);

  // Writer, slot, and generation are all part of terminal/fault identity.
  reset(d);
  const Lease key = request(d, true, 0, 1);
  const uint32_t stale0 = d.stale_events_o;
  terminal(d, key, true, false, false, true);
  terminal(d, key, true, false, false, false, 1);
  terminal(d, key, true, false, false, false, 0, static_cast<uint16_t>(key.generation + 1u));
  zhao::check(state(d, 0) == kWriting && d.lease_valid_o,
              "three stale terminals preserve live lease", 1, 1);
  zhao::check(d.stale_events_o == stale0 + 3, "three stale terminals counted", stale0 + 3,
              d.stale_events_o);
  d.fault_valid_i = 1;
  d.fault_writer_i = !key.writer;
  d.fault_slot_i = key.slot;
  d.fault_generation_i = key.generation;
  zhao::tick(d);
  d.fault_valid_i = 0;
  zhao::check(!d.lease_fault_o && d.stale_events_o == stale0 + 4,
              "stale fault does not poison live lease", 1, 1);

  // Sticky fault and same-edge fault both beat publication and emit no READY.
  matching_fault(d, key);
  zhao::check(d.lease_fault_o, "matching fault sticks in lease", 1, d.lease_fault_o);
  terminal(d, key, true);
  zhao::check(state(d, 0) == kFree && !d.ready_valid_o, "sticky fault releases without READY", 1,
              1);
  const Lease race = request(d, false, 1, 2);
  const uint32_t faults = d.faults_latched_o;
  d.fault_valid_i = 1;
  d.fault_writer_i = race.writer;
  d.fault_slot_i = race.slot;
  d.fault_generation_i = race.generation;
  d.term_valid_i = 1;
  d.term_writer_i = race.writer;
  d.term_slot_i = race.slot;
  d.term_generation_i = race.generation;
  d.term_publish_i = 1;
  d.eval();
  zhao::check(d.term_ready_o, "same-edge fault/publication accepted", 1, d.term_ready_o);
  zhao::tick(d);
  d.fault_valid_i = 0;
  d.term_valid_i = 0;
  d.eval();
  zhao::check(state(d, 1) == kFree && !d.ready_valid_o, "same-edge fault wins and no READY appears",
              1, 1);
  zhao::check(d.faults_latched_o == faults + 1, "same-edge matching fault counted", faults + 1,
              d.faults_latched_o);

  // With an empty hold and an accepting CDC, publication and FIFO offer are the
  // same state transition rather than two GPU clocks apart.
  reset(d);
  const Lease direct = request(d, true, 0, 1);
  d.ready_ready_i = 1;
  d.term_valid_i = 1;
  d.term_writer_i = direct.writer;
  d.term_slot_i = direct.slot;
  d.term_generation_i = direct.generation;
  d.term_publish_i = 1;
  d.eval();
  zhao::check(d.term_ready_o && d.ready_valid_o && d.ready_writer_o == direct.writer &&
                  d.ready_generation_o == direct.generation,
              "accepted publication offers READY on same edge", 1, 1);
  zhao::tick(d);
  d.term_valid_i = 0;
  d.ready_ready_i = 0;
  d.eval();
  zhao::check(!d.ready_valid_o && state(d, 0) == kReady,
              "same-edge accepted READY is not duplicated", 1, 1);

  // A clean publication under CDC backpressure instead creates and freezes one
  // held READY tuple.
  reset(d);
  const Lease pub = request(d, true, 1, 2);
  terminal(d, pub, true);
  zhao::check(d.ready_valid_o && d.ready_writer_o == pub.writer && d.ready_slot_o == pub.slot &&
                  d.ready_generation_o == pub.generation && d.ready_mode_o == pub.mode &&
                  d.ready_base_o == pub.base && d.ready_span_o == pub.span,
              "clean publication creates exact READY tuple", 1, 1);
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  zhao::check(d.ready_valid_o && d.ready_generation_o == pub.generation,
              "READY tuple holds under backpressure", 1, 1);

  // Every field of the swap echo is checked. Stale attempts consume and count.
  const uint32_t stale_swap = d.stale_events_o;
  swap(d, pub, !pub.writer, pub.slot, pub.generation, pub.mode, pub.base, pub.span);
  swap(d, pub, pub.writer, !pub.slot, pub.generation, pub.mode, pub.base, pub.span);
  swap(d, pub, pub.writer, pub.slot, static_cast<uint16_t>(pub.generation + 1u), pub.mode, pub.base,
       pub.span);
  swap(d, pub, pub.writer, pub.slot, pub.generation, pub.mode ^ 1u, pub.base, pub.span);
  swap(d, pub, pub.writer, pub.slot, pub.generation, pub.mode, pub.base + 4u, pub.span);
  swap(d, pub, pub.writer, pub.slot, pub.generation, pub.mode, pub.base, pub.span + 4u);
  zhao::check(state(d, 1) == kReady && d.stale_events_o == stale_swap + 6,
              "all six stale swap shapes refused and counted", 1, 1);
  swap_exact(d, pub);
  zhao::check(state(d, 1) == kDisplayed && d.displayed_valid_o &&
                  d.displayed_writer_o == pub.writer &&
                  d.displayed_generation_o == pub.generation && d.displayed_mode_o == pub.mode &&
                  d.displayed_base_o == pub.base && d.displayed_span_o == pub.span,
              "exact swap becomes exact displayed tuple", 1, 1);

  // Generation is modulo 16 bits and wraps without aliasing a live lease.
  reset(d);
  uint16_t last = 0;
  for (uint32_t i = 0; i < 65536u; ++i) {
    const Lease l = request(d, false, 0, 0);
    last = l.generation;
    terminal(d, l, false);
  }
  zhao::check(last == 0, "generation wraps after 65536 accepted leases", 0, last);
  const Lease after_wrap = request(d, false, 0, 0);
  zhao::check(after_wrap.generation == 1, "generation continues after wrap", 1,
              after_wrap.generation);
  terminal(d, after_wrap, false);

  zhao::check(d.requests_accepted_o == d.responses_accepted_o, "drained requests equal responses",
              d.requests_accepted_o, d.responses_accepted_o);
  zhao::check(d.leases_granted_o + d.leases_refused_o == d.responses_accepted_o,
              "every response is grant or refusal", d.responses_accepted_o,
              d.leases_granted_o + d.leases_refused_o);
  return zhao::report_and_exit("video_slotmgr_v2_directed");
#endif
}
