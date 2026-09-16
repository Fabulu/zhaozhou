// renderer_lease_v2_directed.cpp -- Packet-H H2/H3 lease/clear controller.
#if (defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_LEASE_OPEN_BYPASS) + \
     defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_SLOT_CHOICE) + \
     defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_REQUEST_HOLD) + \
     defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_WRITER_RESPONSE) + \
     defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_SKIP_CLEAR) + \
     defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_BAD_STRIDE) + \
     defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_DUO_BOUNDARY) + \
     defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_STALE_IDENTITY)) > 1
#error ZHAO_RENDERER_LEASE_V2_CPP_MUTANT_SELECTOR_COLLISION
#endif

#include <cstdint>
#include <cstdio>

#include "verilated.h"
#include "Vtb_renderer_lease_v2.h"
#include "zhao_sim.hpp"

namespace {

using Dut = Vtb_renderer_lease_v2;
constexpr uint8_t kFree = 0;
constexpr uint8_t kWriting = 1;
constexpr uint8_t kReady = 2;
constexpr uint32_t kBase[2] = {0x00000000u, 0x02000000u};

struct Geometry {
  uint16_t width;
  uint16_t height;
  uint16_t stride;
  uint16_t view1_y;
  uint32_t view1_offset;
  uint32_t span;
};

constexpr Geometry kGeometry[3] = {
    {384, 240, 768, 0, 0x00000000u, 184320u},
    {320, 240, 640, 0, 0x00000000u, 153600u},
    {256, 384, 512, 192, 0x00018000u, 196608u},
};

struct Identity {
  bool writer;
  bool granted;
  uint8_t slot;
  uint16_t generation;
  uint8_t mode;
  uint32_t base;
  uint32_t span;
};

uint32_t oracle_byte_offset(uint8_t mode, uint16_t x, uint16_t y) {
  const Geometry& g = kGeometry[mode];
  zhao::check(x < g.width && y < g.height,
              "oracle coordinate is inside stored surface", 1, 1);
  return static_cast<uint32_t>(y) * g.stride +
         static_cast<uint32_t>(x) * 2u;
}

void clear_inputs(Dut& d) {
  d.lease_open_i = 1;
  d.frame_req_valid_i = 0;
  d.frame_req_mode_i = 0;
  d.lease_valid_i = 0;
  d.slot0_state_i = kFree;
  d.slot1_state_i = kFree;
  d.render_req_ready_i = 0;
  d.rsp_valid_i = 0;
  d.rsp_writer_i = 0;
  d.rsp_granted_i = 0;
  d.rsp_slot_i = 0;
  d.rsp_generation_i = 0;
  d.rsp_mode_i = 0;
  d.rsp_base_i = 0;
  d.rsp_span_i = 0;
  d.frame_fault_clear_ready_i = 0;
  d.frame_ready_i = 0;
}

void reset(Dut& d) {
  clear_inputs(d);
  d.rst_n = 0;
  d.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

void check_reset_closed(Dut& d, const char* label) {
  d.rst_n = 0;
  d.eval();
  zhao::check(!d.frame_req_ready_o && !d.render_req_valid_o && !d.rsp_ready_o &&
                  !d.frame_fault_clear_valid_o && !d.frame_valid_o,
              label, 1, 1);
  zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

void capture_frame(Dut& d, uint8_t mode, uint8_t slot0_state,
                   uint8_t slot1_state, uint8_t expected_slot) {
  d.lease_valid_i = 0;
  d.slot0_state_i = slot0_state;
  d.slot1_state_i = slot1_state;
  d.frame_req_mode_i = mode;
  d.frame_req_valid_i = 1;
  d.eval();
  zhao::check(d.frame_req_ready_o, "legal frame request accepted", 1,
              d.frame_req_ready_o);
  zhao::tick(d);
  d.frame_req_valid_i = 0;
  d.eval();
  zhao::check(d.render_req_valid_o && d.render_req_slot_o == expected_slot &&
                  d.render_req_mode_o == mode,
              "captured frame becomes exact manager request", 1, 1);
}

void accept_manager_request(Dut& d, uint8_t expected_slot,
                            uint8_t expected_mode) {
  d.eval();
  zhao::check(d.render_req_valid_o && d.render_req_slot_o == expected_slot &&
                  d.render_req_mode_o == expected_mode,
              "manager request present before acceptance", 1, 1);
  d.render_req_ready_i = 1;
  zhao::tick(d);
  d.render_req_ready_i = 0;
  d.eval();
  zhao::check(!d.render_req_valid_o,
              "manager request drops after accepted edge", 1,
              !d.render_req_valid_o);
}

void drive_response(Dut& d, const Identity& id) {
  d.rsp_valid_i = 1;
  d.rsp_writer_i = id.writer;
  d.rsp_granted_i = id.granted;
  d.rsp_slot_i = id.slot;
  d.rsp_generation_i = id.generation;
  d.rsp_mode_i = id.mode;
  d.rsp_base_i = id.base;
  d.rsp_span_i = id.span;
  d.eval();
}

void accept_renderer_response(Dut& d, const Identity& id) {
  drive_response(d, id);
  zhao::check(d.rsp_ready_o, "renderer response accepted", 1, d.rsp_ready_o);
  zhao::tick(d);
  d.rsp_valid_i = 0;
  d.eval();
}

void scribble_response_bus(Dut& d, const Identity& id) {
  d.rsp_writer_i = !id.writer;
  d.rsp_granted_i = !id.granted;
  d.rsp_slot_i = !id.slot;
  d.rsp_generation_i = static_cast<uint16_t>(id.generation ^ 0x5a5au);
  d.rsp_mode_i = static_cast<uint8_t>((id.mode + 1u) % 3u);
  d.rsp_base_i = id.base ^ 0x01234560u;
  d.rsp_span_i = id.span ^ 0x000055aau;
  d.eval();
}

void check_frame(const Dut& d, const Identity& id) {
  const Geometry& g = kGeometry[id.mode];
  zhao::check(d.frame_valid_o, "admitted frame held valid", 1, d.frame_valid_o);
  zhao::check(d.frame_writer_o == id.writer && d.frame_slot_o == id.slot &&
                  d.frame_generation_o == id.generation &&
                  d.frame_mode_o == id.mode && d.frame_base_o == id.base &&
                  d.frame_span_o == id.span,
              "admitted frame preserves complete manager identity", 1, 1);
  zhao::check(d.frame_width_o == g.width && d.frame_height_o == g.height &&
                  d.frame_stride_o == g.stride &&
                  d.frame_view1_y_o == g.view1_y &&
                  d.frame_view1_offset_o == g.view1_offset,
              "admitted frame geometry matches independent host oracle", 1, 1);
  zhao::check(static_cast<uint32_t>(d.frame_width_o) * 2u ==
                  d.frame_stride_o,
              "stored rows have no padding", d.frame_stride_o,
              static_cast<uint32_t>(d.frame_width_o) * 2u);
  zhao::check(static_cast<uint32_t>(d.frame_stride_o) * d.frame_height_o ==
                  d.frame_span_o,
              "derived geometry exactly fills manager span", d.frame_span_o,
              static_cast<uint32_t>(d.frame_stride_o) * d.frame_height_o);
}

void accept_clear_then_frame(Dut& d, const Identity& id,
                             int clear_stall_cycles = 3,
                             int frame_stall_cycles = 3) {
  d.lease_valid_i = 1;
  scribble_response_bus(d, id);
  zhao::check(d.frame_fault_clear_valid_o && !d.frame_valid_o,
              "grant starts held clear before frame admission", 1, 1);
  for (int i = 0; i < clear_stall_cycles; ++i) {
    zhao::check(d.frame_fault_clear_valid_o && !d.frame_valid_o,
                "clear request holds while V3 is nonquiet", 1, 1);
    zhao::tick(d);
  }
  d.frame_fault_clear_ready_i = 1;
  d.eval();
  zhao::check(d.frame_fault_clear_valid_o && !d.frame_valid_o,
              "clear acceptance edge still withholds frame", 1, 1);
  zhao::tick(d);
  d.frame_fault_clear_ready_i = 0;
  d.eval();
  zhao::check(!d.frame_fault_clear_valid_o,
              "exactly one clear request retires after handshake", 1,
              !d.frame_fault_clear_valid_o);
  check_frame(d, id);

  for (int i = 0; i < frame_stall_cycles; ++i) {
    check_frame(d, id);
    zhao::tick(d);
  }
  d.frame_ready_i = 1;
  zhao::tick(d);
  d.frame_ready_i = 0;
  d.eval();
  zhao::check(!d.frame_valid_o && !d.frame_fault_clear_valid_o,
              "one frame acceptance creates no duplicate frame or clear", 1, 1);
}

void run_legal_frame(Dut& d, uint8_t mode, uint8_t slot0_state,
                     uint8_t slot1_state, uint8_t expected_slot,
                     uint16_t generation) {
  reset(d);
  capture_frame(d, mode, slot0_state, slot1_state, expected_slot);

  // Once captured, neither changing source pins nor same-edge slot-state changes
  // may rewrite the held manager packet.
  d.frame_req_mode_i = static_cast<uint8_t>((mode + 1u) % 3u);
  d.slot0_state_i = expected_slot ? kFree : kWriting;
  d.slot1_state_i = expected_slot ? kWriting : kFree;
  for (int i = 0; i < 4; ++i) {
    d.eval();
    zhao::check(d.render_req_valid_o && d.render_req_slot_o == expected_slot &&
                    d.render_req_mode_o == mode,
                "manager request holds slot and mode under backpressure", 1, 1);
    zhao::tick(d);
  }
  accept_manager_request(d, expected_slot, mode);

  // The response bus is shared. A blitter response must remain untouched for its
  // own consumer and cannot create clear or frame work here.
  const Identity wrong_writer{false, true, expected_slot,
                              static_cast<uint16_t>(generation - 1u), mode,
                              kBase[expected_slot], kGeometry[mode].span};
  drive_response(d, wrong_writer);
  zhao::check(!d.rsp_ready_o && !d.frame_fault_clear_valid_o && !d.frame_valid_o,
              "writer-zero response is not consumed by renderer leaf", 1, 1);
  zhao::tick(d);
  zhao::check(!d.rsp_ready_o,
              "held writer-zero response still belongs to blitter", 1,
              !d.rsp_ready_o);
  d.rsp_valid_i = 0;
  d.eval();

  const Identity grant{true, true, expected_slot, generation, mode,
                       kBase[expected_slot], kGeometry[mode].span};
  accept_renderer_response(d, grant);
  accept_clear_then_frame(d, grant);

  // The current lease remains live after frame admission; this leaf cannot
  // capture another frame until terminal handling elsewhere clears it.
  d.frame_req_valid_i = 1;
  d.frame_req_mode_i = mode;
  d.slot0_state_i = kFree;
  d.slot1_state_i = kFree;
  d.eval();
  zhao::check(!d.frame_req_ready_o,
              "live lease blocks a second frame capture", 1,
              !d.frame_req_ready_o);
  d.lease_valid_i = 0;
  d.eval();
  zhao::check(d.frame_req_ready_o,
              "cleared lease and FREE slot reopen frame capture", 1,
              d.frame_req_ready_o);
  d.frame_req_valid_i = 0;
}

[[noreturn]] void mutant_result(const char* name, bool detected) {
  std::printf("[%s] %s\n", name, detected ? "DETECTED" : "MISSED");
  zhao::exit_hard(detected ? 0 : 1);
}

void setup_to_response(Dut& d, uint8_t mode = 2) {
  reset(d);
  capture_frame(d, mode, kFree, kFree, 0);
  accept_manager_request(d, 0, mode);
}

Identity duo_grant(uint16_t generation = 0x1234u) {
  return Identity{true, true, 0, generation, 2, kBase[0], kGeometry[2].span};
}

}  // namespace

int main() {
  Dut d;

#if defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_LEASE_OPEN_BYPASS)
  reset(d);
  d.lease_open_i = 0;
  d.frame_req_valid_i = 1;
  d.frame_req_mode_i = 0;
  d.eval();
  const bool accepted_while_closed = d.frame_req_ready_o;
  zhao::tick(d);
  d.eval();
  mutant_result("renderer_lease_lease_open_bypass",
                accepted_while_closed && d.render_req_valid_o);
#elif defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_SLOT_CHOICE)
  reset(d);
  d.frame_req_valid_i = 1;
  d.frame_req_mode_i = 0;
  d.eval();
  zhao::tick(d);
  d.frame_req_valid_i = 0;
  d.eval();
  mutant_result("renderer_lease_slot_choice",
                d.render_req_valid_o && d.render_req_slot_o == 1);
#elif defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_REQUEST_HOLD)
  reset(d);
  capture_frame(d, 0, kFree, kFree, 0);
  d.frame_req_mode_i = 1;
  d.slot0_state_i = kWriting;
  d.slot1_state_i = kFree;
  d.eval();
  mutant_result("renderer_lease_request_hold",
                d.render_req_slot_o == 1 && d.render_req_mode_o == 1);
#elif defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_WRITER_RESPONSE)
  setup_to_response(d, 0);
  const Identity blit{false, true, 0, 9, 0, kBase[0], kGeometry[0].span};
  drive_response(d, blit);
  mutant_result("renderer_lease_writer_response", d.rsp_ready_o);
#elif defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_SKIP_CLEAR)
  setup_to_response(d);
  const Identity grant = duo_grant();
  accept_renderer_response(d, grant);
  d.eval();
  mutant_result("renderer_lease_skip_clear",
                d.frame_valid_o && !d.frame_fault_clear_valid_o);
#elif defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_BAD_STRIDE)
  setup_to_response(d);
  const Identity grant = duo_grant();
  accept_renderer_response(d, grant);
  d.frame_fault_clear_ready_i = 1;
  zhao::tick(d);
  d.frame_fault_clear_ready_i = 0;
  d.eval();
  mutant_result("renderer_lease_bad_stride", d.frame_stride_o == 640);
#elif defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_DUO_BOUNDARY)
  setup_to_response(d);
  const Identity grant = duo_grant();
  accept_renderer_response(d, grant);
  d.frame_fault_clear_ready_i = 1;
  zhao::tick(d);
  d.frame_fault_clear_ready_i = 0;
  d.eval();
  mutant_result("renderer_lease_duo_boundary",
                d.frame_view1_offset_o == 0x00017ffeu);
#elif defined(ZHAO_EXPECT_RENDERER_LEASE_MUTANT_STALE_IDENTITY)
  setup_to_response(d);
  const Identity grant = duo_grant();
  accept_renderer_response(d, grant);
  d.frame_fault_clear_ready_i = 1;
  zhao::tick(d);
  d.frame_fault_clear_ready_i = 0;
  scribble_response_bus(d, grant);
  mutant_result("renderer_lease_stale_identity",
                d.frame_generation_o != grant.generation &&
                    d.frame_base_o != grant.base && d.frame_mode_o != grant.mode);
#else
  // A request held across reset and a closed reset-epoch barrier cannot reach
  // the manager. The first open edge captures it once. Closing the gate after
  // that edge cannot retract the now-owned request or any later response,
  // clear, or admitted frame.
  reset(d);
  d.frame_req_valid_i = 1;
  d.frame_req_mode_i = 2;
  d.slot0_state_i = kFree;
  d.slot1_state_i = kFree;
  d.lease_open_i = 1;
  d.rst_n = 0;
  d.eval();
  zhao::check(!d.frame_req_ready_o && !d.render_req_valid_o,
              "reset blocks barrier-open upstream request", 1, 1);
  zhao::tick(d);
  d.lease_open_i = 0;
  d.rst_n = 1;
  d.eval();

  unsigned upstream_accepts = 0;
  unsigned manager_accepts = 0;
  for (int i = 0; i < 3; ++i) {
    d.eval();
    upstream_accepts += d.frame_req_valid_i && d.frame_req_ready_o;
    manager_accepts += d.render_req_valid_o && d.render_req_ready_i;
    zhao::check(!d.frame_req_ready_o && !d.render_req_valid_o,
                "closed epoch exposes no frame or manager acceptance", 1, 1);
    zhao::tick(d);
  }
  d.lease_open_i = 1;
  d.eval();
  zhao::check(d.frame_req_ready_o && !d.render_req_valid_o,
              "open epoch admits held upstream request before manager offer", 1,
              1);
  upstream_accepts += d.frame_req_valid_i && d.frame_req_ready_o;
  zhao::tick(d);
  d.frame_req_valid_i = 0;
  d.lease_open_i = 0;
  d.eval();
  zhao::check(d.render_req_valid_o && d.render_req_slot_o == 0 &&
                  d.render_req_mode_o == 2,
              "owned manager request survives later epoch closure", 1, 1);
  for (int i = 0; i < 3; ++i) {
    zhao::check(d.render_req_valid_o && d.render_req_slot_o == 0 &&
                    d.render_req_mode_o == 2,
                "closed epoch does not drop owned stalled request", 1, 1);
    zhao::tick(d);
  }
  d.render_req_ready_i = 1;
  d.eval();
  manager_accepts += d.render_req_valid_o && d.render_req_ready_i;
  zhao::tick(d);
  d.render_req_ready_i = 0;
  d.eval();
  zhao::check(!d.render_req_valid_o,
              "owned request retires once while epoch remains closed", 1,
              !d.render_req_valid_o);
  const Identity gated_grant{true, true, 0, 0x0077u, 2, kBase[0],
                             kGeometry[2].span};
  accept_renderer_response(d, gated_grant);
  zhao::check(d.frame_fault_clear_valid_o,
              "closed epoch does not block owned response-to-clear progress", 1,
              d.frame_fault_clear_valid_o);
  accept_clear_then_frame(d, gated_grant, 1, 1);
  zhao::check(upstream_accepts == 1 && manager_accepts == 1,
              "held request creates exactly one upstream and manager acceptance",
              1, upstream_accepts == 1 && manager_accepts == 1);
  d.lease_valid_i = 0;
  d.frame_req_valid_i = 1;
  d.frame_req_mode_i = 0;
  d.slot0_state_i = kFree;
  d.slot1_state_i = kFree;
  d.eval();
  zhao::check(!d.frame_req_ready_o && !d.render_req_valid_o,
              "closed epoch gates the next frame after owned work retires", 1, 1);
  d.lease_open_i = 1;
  d.eval();
  zhao::check(d.frame_req_ready_o,
              "reopened epoch permits the next distinct frame", 1,
              d.frame_req_ready_o);
  d.frame_req_valid_i = 0;

  reset(d);

  // Reset, epoch barrier, mode legality, lease liveness, and FREE observation
  // all close the upstream acceptance gate independently.
  d.frame_req_valid_i = 1;
  d.frame_req_mode_i = 3;
  d.eval();
  zhao::check(!d.frame_req_ready_o, "illegal canvas mode is not captured", 1,
              !d.frame_req_ready_o);
  d.frame_req_mode_i = 0;
  d.lease_valid_i = 1;
  d.eval();
  zhao::check(!d.frame_req_ready_o, "live lease closes frame capture", 1,
              !d.frame_req_ready_o);
  d.lease_valid_i = 0;
  d.slot0_state_i = kWriting;
  d.slot1_state_i = kReady;
  d.eval();
  zhao::check(!d.frame_req_ready_o, "no FREE slot closes frame capture", 1,
              !d.frame_req_ready_o);
  check_reset_closed(d, "reset closes every ready/valid output");

  // Every mode is checked against a host-authored geometry table. Slot 0 wins
  // when both are FREE; slot 1 is selected only when slot 0 is unavailable.
  run_legal_frame(d, 0, kFree, kFree, 0, 0x0101u);
  run_legal_frame(d, 1, kWriting, kFree, 1, 0x0202u);
  run_legal_frame(d, 2, kFree, kFree, 0, 0x0303u);

  // The four Duo sentinels pin packed vertical views and both RGB565 edges.
  zhao::check(oracle_byte_offset(2, 0, 0) == 0x00000000u,
              "Duo first pixel offset", 0x00000000u,
              oracle_byte_offset(2, 0, 0));
  zhao::check(oracle_byte_offset(2, 255, 191) == 0x00017ffeu,
              "Duo view-0 final pixel offset", 0x00017ffeu,
              oracle_byte_offset(2, 255, 191));
  zhao::check(oracle_byte_offset(2, 0, 192) == 0x00018000u,
              "Duo view-1 first pixel offset", 0x00018000u,
              oracle_byte_offset(2, 0, 192));
  zhao::check(oracle_byte_offset(2, 255, 383) == 0x0002fffeu,
              "Duo stored-surface final pixel offset", 0x0002fffeu,
              oracle_byte_offset(2, 255, 383));
  zhao::check(oracle_byte_offset(2, 255, 383) + 2u == kGeometry[2].span,
              "Duo final RGB565 word ends exactly at span", kGeometry[2].span,
              oracle_byte_offset(2, 255, 383) + 2u);

  // Refusal cannot create clear/frame work. The original mode stays pending,
  // and retry waits for both no live lease and a currently FREE slot.
  reset(d);
  capture_frame(d, 2, kFree, kFree, 0);
  accept_manager_request(d, 0, 2);
  d.lease_valid_i = 1;
  const Identity refusal{true, false, 0, 0x4040u, 2, kBase[0],
                         kGeometry[2].span};
  accept_renderer_response(d, refusal);
  zhao::check(!d.frame_fault_clear_valid_o && !d.frame_valid_o &&
                  !d.render_req_valid_o,
              "refusal creates no frame and no immediate retry", 1, 1);
  d.slot0_state_i = kWriting;
  d.slot1_state_i = kFree;
  d.frame_req_mode_i = 0;
  for (int i = 0; i < 3; ++i) {
    zhao::tick(d);
    zhao::check(!d.render_req_valid_o,
                "retry waits while any live lease remains", 1,
                !d.render_req_valid_o);
  }
  d.lease_valid_i = 0;
  d.slot0_state_i = kWriting;
  d.slot1_state_i = kReady;
  for (int i = 0; i < 2; ++i) {
    zhao::tick(d);
    zhao::check(!d.render_req_valid_o,
                "retry waits while no slot is FREE", 1,
                !d.render_req_valid_o);
  }
  d.slot1_state_i = kFree;
  d.lease_open_i = 0;
  for (int i = 0; i < 2; ++i) {
    zhao::tick(d);
    zhao::check(!d.render_req_valid_o,
                "retry retains pending frame while epoch gate is closed", 1,
                !d.render_req_valid_o);
  }
  d.lease_open_i = 1;
  zhao::tick(d);
  d.eval();
  zhao::check(d.render_req_valid_o && d.render_req_slot_o == 1 &&
                  d.render_req_mode_o == 2,
              "retry reselects lowest FREE slot and preserves captured mode", 1,
              1);
  accept_manager_request(d, 1, 2);
  const Identity retry_grant{true, true, 1, 0x4041u, 2, kBase[1],
                             kGeometry[2].span};
  accept_renderer_response(d, retry_grant);
  accept_clear_then_frame(d, retry_grant, 1, 1);

  // Asynchronous reset discards each kind of held work; no pre-reset clear or
  // frame can reappear after release.
  reset(d);
  capture_frame(d, 0, kFree, kFree, 0);
  check_reset_closed(d, "reset discards held manager request");
  zhao::check(!d.render_req_valid_o && !d.frame_fault_clear_valid_o &&
                  !d.frame_valid_o,
              "request does not survive reset", 1, 1);

  setup_to_response(d, 1);
  const Identity reset_clear{true, true, 0, 0x5151u, 1, kBase[0],
                             kGeometry[1].span};
  accept_renderer_response(d, reset_clear);
  zhao::check(d.frame_fault_clear_valid_o,
              "clear is held before reset control", 1,
              d.frame_fault_clear_valid_o);
  check_reset_closed(d, "reset discards held V3 clear");
  zhao::check(!d.frame_fault_clear_valid_o && !d.frame_valid_o,
              "clear does not survive reset", 1, 1);

  setup_to_response(d, 0);
  const Identity reset_frame{true, true, 0, 0x6161u, 0, kBase[0],
                             kGeometry[0].span};
  accept_renderer_response(d, reset_frame);
  d.frame_fault_clear_ready_i = 1;
  zhao::tick(d);
  d.frame_fault_clear_ready_i = 0;
  d.eval();
  zhao::check(d.frame_valid_o, "frame is held before reset control", 1,
              d.frame_valid_o);
  check_reset_closed(d, "reset discards held admitted frame");
  zhao::check(!d.frame_valid_o, "frame does not survive reset", 1,
              !d.frame_valid_o);

  return zhao::report_and_exit("renderer_lease_v2_directed");
#endif
}
