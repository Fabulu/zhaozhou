// video_ready_bridge_v2_directed.cpp -- Packet-H video bridge scoreboard.
#if (defined(ZHAO_EXPECT_VIDEO_BRIDGE_DROP_HELD_TUPLE) +    \
     defined(ZHAO_EXPECT_VIDEO_BRIDGE_CHANGE_HELD_TUPLE) +  \
     defined(ZHAO_EXPECT_VIDEO_BRIDGE_WRONG_ONEHOT) +       \
     defined(ZHAO_EXPECT_VIDEO_BRIDGE_BLANK_ACK_BYPASS) +   \
     defined(ZHAO_EXPECT_VIDEO_BRIDGE_EARLY_LEASE_OPEN) +   \
     defined(ZHAO_EXPECT_VIDEO_BRIDGE_RETAIN_RESET_TUPLE) + \
     defined(ZHAO_EXPECT_VIDEO_BRIDGE_UNBLANK_SCANOUT_ONLY)) > 1
#error ZHAO_VIDEO_READY_BRIDGE_V2_CPP_MUTANT_SELECTOR_COLLISION
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "verilated.h"
#include "Vtb_video_ready_bridge_v2.h"

// zhao::exit_hard -- tests/harness/zhao_sim.hpp: a plain return from a
// Verilated main can deadlock in VlThreadPool's destructor at ~0 CPU.
#include "../harness/zhao_sim.hpp"

double sc_time_stamp() { return 0.0; }

namespace {

using Dut = Vtb_video_ready_bridge_v2;

struct Tuple {
  bool writer;
  bool slot;
  uint16_t generation;
  uint8_t mode;
  uint32_t base;
  uint32_t span;
};

bool operator==(const Tuple& a, const Tuple& b) {
  return a.writer == b.writer && a.slot == b.slot && a.generation == b.generation &&
         a.mode == b.mode && a.base == b.base && a.span == b.span;
}

bool operator!=(const Tuple& a, const Tuple& b) { return !(a == b); }

template <typename Wide>
void drive_tuple(Wide& port, const Tuple& value) {
  port[0] = value.span;
  port[1] = value.base;
  port[2] = static_cast<uint32_t>(value.mode & 3u) |
            (static_cast<uint32_t>(value.generation) << 2) |
            (static_cast<uint32_t>(value.slot) << 18) | (static_cast<uint32_t>(value.writer) << 19);
}

template <typename Wide>
Tuple sample_tuple(const Wide& port) {
  const uint32_t hi = port[2];
  return Tuple{((hi >> 19) & 1u) != 0,
               ((hi >> 18) & 1u) != 0,
               static_cast<uint16_t>((hi >> 2) & 0xffffu),
               static_cast<uint8_t>(hi & 3u),
               static_cast<uint32_t>(port[1]),
               static_cast<uint32_t>(port[0])};
}

Tuple tuple(unsigned n, bool slot) {
  return Tuple{(n & 1u) != 0,
               slot,
               static_cast<uint16_t>(0x4100u + n),
               static_cast<uint8_t>(n % 3u),
               0x02000000u * static_cast<uint32_t>(slot) + 0x100u * n,
               0x00025800u + 0x20u * n};
}

int failures = 0;
int checks = 0;

void check(bool condition, const char* what) {
  ++checks;
  if (!condition) {
    ++failures;
    std::printf("FAIL: %s\n", what);
  }
}

[[noreturn]] void finish(const char* name) {
  if (failures == 0)
    std::printf("[%s] passed %d checks\n", name, checks);
  else
    std::printf("[%s] FAILED %d of %d checks\n", name, failures, checks);
  std::fflush(nullptr);
  std::_Exit(failures == 0 ? 0 : 1);
}

[[noreturn]] void mutant_result(const char* name, bool detected) {
  std::printf("[%s] %s\n", name, detected ? "DETECTED" : "MISSED");
  std::fflush(nullptr);
  std::_Exit(detected ? 0 : 1);
}

void clear_inputs(Dut& d) {
  d.gpu_clk = 0;
  d.vid_clk = 0;
  d.gpu_barrier_done_i = 0;
  d.vid_barrier_done_i = 0;
  d.blank_cmd_i = 0;
  d.cdc_ready_valid_i = 0;
  drive_tuple(d.cdc_ready_tuple_i, tuple(0, false));
  d.cdc_swap_ready_i = 0;
  d.frame_swap_valid_i = 0;
  d.frame_swap_slot_i = 0;
  d.scanout_ack_i = 0;
  d.scanout_valid_i = 1;
  d.scanout_rgb_i = 0x5a6bu;
  d.scanout_x_i = 17;
  d.scanout_y_i = 23;
  d.scanout_hsync_i = 1;
  d.scanout_vsync_i = 0;
  d.scanout_hblank_i = 0;
  d.scanout_vblank_i = 1;
}

void raw_gpu_edge(Dut& d) {
  d.gpu_clk = 0;
  d.eval();
  d.gpu_clk = 1;
  d.eval();
  d.gpu_clk = 0;
  d.eval();
}

void raw_vid_edge(Dut& d) {
  d.vid_clk = 0;
  d.eval();
  d.vid_clk = 1;
  d.eval();
  d.vid_clk = 0;
  d.eval();
}

void assert_pair_reset(Dut& d) {
  clear_inputs(d);
  // Verilator initializes derived reset chains low.  First release both chains
  // so the following pair-reset assertion creates a real local-reset negedge,
  // including for reset-to-one state such as blank_active.
  d.gpu_rst_n = 1;
  d.vid_rst_n = 1;
  d.eval();
  for (int i = 0; i < 4; ++i) {
    raw_gpu_edge(d);
    raw_vid_edge(d);
  }
  d.gpu_rst_n = 0;
  d.vid_rst_n = 0;
  d.eval();
}

void release_pair(Dut& d, bool barriers = true) {
  d.gpu_rst_n = 1;
  d.vid_rst_n = 1;
  d.gpu_barrier_done_i = barriers;
  d.vid_barrier_done_i = barriers;
  d.eval();
}

void raw_release_video(Dut& d, bool barriers = true) {
  release_pair(d, barriers);
  // Three edges fill the release synchronizer.  The state bank still observes
  // reset on edge three and first executes normal logic on edge four.
  for (int i = 0; i < 4; ++i) raw_vid_edge(d);
}

void raw_accept_ready(Dut& d, const Tuple& value) {
  drive_tuple(d.cdc_ready_tuple_i, value);
  d.cdc_ready_valid_i = 1;
  d.eval();
  raw_vid_edge(d);
  d.cdc_ready_valid_i = 0;
  d.eval();
}

void raw_swap(Dut& d, bool slot) {
  d.frame_swap_slot_i = slot;
  d.frame_swap_valid_i = 1;
  d.eval();
  raw_vid_edge(d);
  d.frame_swap_valid_i = 0;
  d.eval();
}

void raw_make_unblanked(Dut& d) {
  assert_pair_reset(d);
  raw_release_video(d, true);
  const Tuple first = tuple(1, false);
  raw_accept_ready(d, first);
  raw_swap(d, first.slot);
  d.cdc_swap_ready_i = 1;
  d.scanout_ack_i = 1;
  raw_vid_edge(d);
  d.cdc_swap_ready_i = 0;
  d.scanout_ack_i = 0;
  raw_vid_edge(d);  // registered stream now carries the live coloured pixel
}

struct Scoreboard {
  bool pending_valid = false;
  Tuple pending{};
  bool echo_valid = false;
  Tuple echo{};
  bool scanout_wait = false;
  Tuple scanout_tuple{};
  bool blank_active = true;
  bool blank_ack_q = false;
  bool candidate_valid = false;
  Tuple candidate{};
  bool candidate_echo_seen = false;
  bool candidate_scanout_seen = false;
  std::vector<Tuple> accepted_echoes;

  void reset() {
    pending_valid = false;
    pending = Tuple{};
    echo_valid = false;
    echo = Tuple{};
    scanout_wait = false;
    scanout_tuple = Tuple{};
    blank_active = true;
    blank_ack_q = false;
    candidate_valid = false;
    candidate = Tuple{};
    candidate_echo_seen = false;
    candidate_scanout_seen = false;
    accepted_echoes.clear();
  }

  void check_state(Dut& d, const char* phase) {
    const bool released = d.vid_reset_released_o;
    const bool echo_room = !echo_valid || d.cdc_swap_ready_i;
    uint8_t expected_ready = 0;
    if (released && d.vid_barrier_done_i && !d.blank_cmd_i && pending_valid && !scanout_wait &&
        echo_room)
      expected_ready = pending.slot ? 2u : 1u;

    if (static_cast<bool>(d.pending_o) != pending_valid) {
      std::printf("  phase: %s\n", phase);
      check(false, "scoreboard pending-valid agrees");
    } else {
      check(true, "scoreboard pending-valid agrees");
    }
    if (pending_valid)
      check(sample_tuple(d.pending_tuple_o) == pending,
            "scoreboard pending tuple preserves all 84 bits");
    check(static_cast<bool>(d.echo_hold_o) == echo_valid,
          "scoreboard reverse-storage occupancy agrees");
    check(static_cast<bool>(d.cdc_swap_valid_o) == (released && echo_valid),
          "reverse valid is held exactly while echo is owned");
    if (echo_valid && released)
      check(sample_tuple(d.cdc_swap_tuple_o) == echo,
            "scoreboard echo tuple preserves all 84 bits");
    check(static_cast<uint8_t>(d.frame_slot_ready_o) == expected_ready,
          "FRAMECTL receives the exact eligible one-hot slot");
    check((d.frame_slot_ready_o == 0) || (d.frame_slot_ready_o == 1) || (d.frame_slot_ready_o == 2),
          "FRAMECTL slot-ready is one-hot-or-zero");
    const bool expected_input_ready =
        released && d.vid_barrier_done_i && !d.blank_cmd_i && !pending_valid;
    check(static_cast<bool>(d.cdc_ready_ready_o) == expected_input_ready,
          "forward READY acceptance matches one-entry pending capacity");
    check(static_cast<bool>(d.blank_active_o) == blank_active, "scoreboard blank epoch agrees");
    check(static_cast<bool>(d.scanout_wait_o) == scanout_wait,
          "scoreboard scanout completion wait agrees");
    check(static_cast<bool>(d.unblank_candidate_o) == candidate_valid,
          "scoreboard unblank candidate occupancy agrees");
    check(static_cast<bool>(d.unblank_echo_seen_o) == candidate_echo_seen,
          "scoreboard exact echo-acceptance fact agrees");
    check(static_cast<bool>(d.unblank_scanout_seen_o) == candidate_scanout_seen,
          "scoreboard exact scanout-ack fact agrees");
    if (candidate_valid)
      check(sample_tuple(d.unblank_tuple_o) == candidate,
            "unblank candidate preserves the exact current swap tuple");
    check(!candidate_echo_seen || candidate_valid, "echo fact never exists without a candidate");
    check(!candidate_scanout_seen || candidate_valid,
          "scanout fact never exists without a candidate");
    if (blank_active) check(d.output_rgb_o == 0, "blank epoch keeps registered RGB black");
    if (d.blank_ack_o)
      check(d.output_rgb_o == 0, "blank acknowledgement never leads registered black");
  }

  void vid_edge(Dut& d, const char* phase) {
    d.vid_clk = 0;
    d.eval();
    check_state(d, phase);

    const bool released_before = d.vid_reset_released_o;
    const bool command = d.blank_cmd_i;
    const bool old_blank = blank_active;
    const bool old_candidate = candidate_valid;
    const bool old_output_black = d.output_rgb_o == 0;
    const bool pop = released_before && echo_valid && d.cdc_swap_valid_o && d.cdc_swap_ready_i;
    const Tuple popped = sample_tuple(d.cdc_swap_tuple_o);
    const bool push = released_before && d.cdc_ready_valid_i && d.cdc_ready_ready_o;
    const Tuple pushed = sample_tuple(d.cdc_ready_tuple_i);
    const bool room = !echo_valid || pop;
    const bool take = released_before && d.vid_barrier_done_i && !command && d.frame_swap_valid_i &&
                      pending_valid && !scanout_wait && room &&
                      (static_cast<bool>(d.frame_swap_slot_i) == pending.slot);
    const Tuple taken = pending;
    const bool scanout_complete = released_before && scanout_wait && d.scanout_ack_i;
    const bool echo_event = old_candidate && pop && (popped == candidate);
    const bool scanout_event = old_candidate && scanout_complete && (scanout_tuple == candidate);
    const bool candidate_complete = old_candidate && (candidate_echo_seen || echo_event) &&
                                    (candidate_scanout_seen || scanout_event);
    const bool start_candidate = take && old_blank && !old_candidate;

    if (pop) {
      check(popped == echo, "accepted reverse tuple matches independent scoreboard head");
      accepted_echoes.push_back(popped);
    }

    const bool in_valid = d.scanout_valid_i;
    const uint16_t in_rgb = d.scanout_rgb_i;
    const uint16_t in_x = d.scanout_x_i;
    const uint8_t in_y = d.scanout_y_i;
    const bool in_hsync = d.scanout_hsync_i;
    const bool in_vsync = d.scanout_vsync_i;
    const bool in_hblank = d.scanout_hblank_i;
    const bool in_vblank = d.scanout_vblank_i;

    d.vid_clk = 1;
    d.eval();
    d.vid_clk = 0;
    d.eval();

    if (!released_before) {
      reset();
    } else {
      blank_ack_q = old_blank && old_output_black;
      if (pop) echo_valid = false;
      if (push) {
        pending_valid = true;
        pending = pushed;
      }
      if (scanout_complete) scanout_wait = false;

      if (command) {
        blank_active = true;
        candidate_valid = false;
        candidate = Tuple{};
        candidate_echo_seen = false;
        candidate_scanout_seen = false;
      } else {
        if (echo_event) candidate_echo_seen = true;
        if (scanout_event) candidate_scanout_seen = true;
        if (candidate_complete) {
          blank_active = false;
          candidate_valid = false;
          candidate_echo_seen = false;
          candidate_scanout_seen = false;
        }
        if (take) {
          pending_valid = false;
          echo_valid = true;
          echo = taken;
          scanout_wait = true;
          scanout_tuple = taken;
          if (start_candidate) {
            candidate_valid = true;
            candidate = taken;
            candidate_echo_seen = false;
            candidate_scanout_seen = false;
          }
        }
      }
    }

    if (!released_before) {
      check(!d.output_valid_o && d.output_rgb_o == 0 && d.output_x_o == 0 && d.output_y_o == 0 &&
                !d.output_hsync_o && !d.output_vsync_o && !d.output_hblank_o && !d.output_vblank_o,
            "video state remains reset through synchronized release edge");
      check(!d.blank_ack_o, "blank ACK cannot rise while video local reset is held");
    } else {
      check(d.output_valid_o == in_valid,
            "registered pixel valid stays aligned with timing metadata");
      check(d.output_x_o == in_x && d.output_y_o == in_y,
            "registered pixel coordinates stay aligned");
      check(d.output_hsync_o == in_hsync && d.output_vsync_o == in_vsync &&
                d.output_hblank_o == in_hblank && d.output_vblank_o == in_vblank,
            "registered sync and blank metadata stay aligned");
      const uint16_t expected_rgb = (old_blank || command) ? 0 : in_rgb;
      check(d.output_rgb_o == expected_rgb, "registered pixel mux selects exact black or live RGB");
      check(static_cast<bool>(d.blank_ack_o) == blank_ack_q,
            "blank acknowledgement follows observed registered black");
    }
    check_state(d, phase);
  }
};

void scoreboard_accept_ready(Dut& d, Scoreboard& s, const Tuple& value, const char* phase) {
  drive_tuple(d.cdc_ready_tuple_i, value);
  d.cdc_ready_valid_i = 1;
  d.eval();
  check(d.cdc_ready_ready_o, "READY source sees capacity before acceptance");
  s.vid_edge(d, phase);
  d.cdc_ready_valid_i = 0;
  d.eval();
}

void scoreboard_swap(Dut& d, Scoreboard& s, bool slot, const char* phase) {
  d.frame_swap_slot_i = slot;
  d.frame_swap_valid_i = 1;
  d.eval();
  s.vid_edge(d, phase);
  d.frame_swap_valid_i = 0;
  d.eval();
}

void scoreboard_command_blank(Dut& d, Scoreboard& s) {
  check(d.output_rgb_o != 0 && !d.blank_ack_o, "pre-command output is live and coloured");
  d.blank_cmd_i = 1;
  d.eval();
  check(!d.blank_ack_o && d.output_rgb_o != 0,
        "blank command cannot bypass the registered black proof");
  s.vid_edge(d, "blank command registers black");
  check(d.output_rgb_o == 0 && !d.blank_ack_o, "first blank-command edge turns black before ACK");
  s.vid_edge(d, "held blank command receives proof");
  check(d.blank_ack_o && d.blank_active_o, "blank ACK follows one observed black register cycle");
  d.blank_cmd_i = 0;
  d.eval();
}

void run_mutant(Dut& d) {
#if defined(ZHAO_EXPECT_VIDEO_BRIDGE_EARLY_LEASE_OPEN)
  assert_pair_reset(d);
  release_pair(d, true);
  for (int i = 0; i < 8; ++i) raw_gpu_edge(d);  // no video edge: no blank ACK
  mutant_result("video_bridge_early_lease_open", d.lease_open_o && !d.blank_ack_o);
#elif defined(ZHAO_EXPECT_VIDEO_BRIDGE_WRONG_ONEHOT)
  assert_pair_reset(d);
  raw_release_video(d, true);
  raw_accept_ready(d, tuple(3, true));
  mutant_result("video_bridge_wrong_onehot", d.frame_slot_ready_o == 3);
#elif defined(ZHAO_EXPECT_VIDEO_BRIDGE_DROP_HELD_TUPLE)
  assert_pair_reset(d);
  raw_release_video(d, true);
  const Tuple a = tuple(4, false);
  raw_accept_ready(d, a);
  d.cdc_swap_ready_i = 0;
  raw_swap(d, a.slot);
  mutant_result("video_bridge_drop_held_tuple", d.echo_hold_o && !d.cdc_swap_valid_o);
#elif defined(ZHAO_EXPECT_VIDEO_BRIDGE_CHANGE_HELD_TUPLE)
  assert_pair_reset(d);
  raw_release_video(d, true);
  const Tuple a = tuple(5, false);
  const Tuple b = tuple(6, true);
  raw_accept_ready(d, a);
  d.cdc_swap_ready_i = 0;
  raw_swap(d, a.slot);
  drive_tuple(d.cdc_ready_tuple_i, b);
  d.eval();
  mutant_result("video_bridge_change_held_tuple", d.cdc_swap_valid_o &&
                                                      sample_tuple(d.cdc_swap_tuple_o) == b &&
                                                      sample_tuple(d.cdc_swap_tuple_o) != a);
#elif defined(ZHAO_EXPECT_VIDEO_BRIDGE_BLANK_ACK_BYPASS)
  raw_make_unblanked(d);
  const bool coloured = d.output_rgb_o != 0 && !d.blank_ack_o;
  d.blank_cmd_i = 1;
  d.eval();  // no video edge: output register is still coloured
  mutant_result("video_bridge_blank_ack_bypass", coloured && d.blank_ack_o && d.output_rgb_o != 0);
#elif defined(ZHAO_EXPECT_VIDEO_BRIDGE_RETAIN_RESET_TUPLE)
  assert_pair_reset(d);
  raw_release_video(d, true);
  const Tuple a = tuple(7, true);
  raw_accept_ready(d, a);
  d.gpu_rst_n = 0;
  d.eval();  // asynchronous pair reset must clear without either clock
  mutant_result("video_bridge_retain_reset_tuple",
                d.pending_o && sample_tuple(d.pending_tuple_o) == a);
#elif defined(ZHAO_EXPECT_VIDEO_BRIDGE_UNBLANK_SCANOUT_ONLY)
  assert_pair_reset(d);
  raw_release_video(d, true);
  const Tuple a = tuple(8, false);
  raw_accept_ready(d, a);
  d.cdc_swap_ready_i = 0;
  raw_swap(d, a.slot);
  d.scanout_ack_i = 1;
  raw_vid_edge(d);
  mutant_result("video_bridge_unblank_scanout_only",
                !d.blank_active_o && d.echo_hold_o && !d.cdc_swap_ready_i);
#else
  (void)d;
#endif
}

}  // namespace

int main() {
  Dut d;
  clear_inputs(d);

#if defined(ZHAO_EXPECT_VIDEO_BRIDGE_DROP_HELD_TUPLE) ||    \
    defined(ZHAO_EXPECT_VIDEO_BRIDGE_CHANGE_HELD_TUPLE) ||  \
    defined(ZHAO_EXPECT_VIDEO_BRIDGE_WRONG_ONEHOT) ||       \
    defined(ZHAO_EXPECT_VIDEO_BRIDGE_BLANK_ACK_BYPASS) ||   \
    defined(ZHAO_EXPECT_VIDEO_BRIDGE_EARLY_LEASE_OPEN) ||   \
    defined(ZHAO_EXPECT_VIDEO_BRIDGE_RETAIN_RESET_TUPLE) || \
    defined(ZHAO_EXPECT_VIDEO_BRIDGE_UNBLANK_SCANOUT_ONLY)
  run_mutant(d);
  mutant_result("video_bridge_unknown_mutant", false);
#else
  Scoreboard s;
  assert_pair_reset(d);
  s.reset();
  check(!d.gpu_reset_released_o && !d.vid_reset_released_o,
        "pair reset asynchronously asserts both local resets");
  check(!d.pending_o && !d.echo_hold_o && d.frame_slot_ready_o == 0,
        "pair reset immediately drops pending and echo state");
  check(d.output_rgb_o == 0 && d.blank_active_o && !d.blank_ack_o,
        "pair reset immediately forces black before acknowledging it");
  check(!d.lease_open_o && !d.cdc_ready_ready_o && !d.cdc_swap_valid_o,
        "pair reset closes lease and both bridge handshakes");

  // Deassert pair reset without clocks.  Neither local reset may release
  // asynchronously.  Each witness must move only on its own third clock edge.
  release_pair(d, false);
  check(!d.gpu_reset_released_o && !d.vid_reset_released_o,
        "pair deassertion alone releases neither domain");
  raw_gpu_edge(d);
  check(!d.gpu_reset_released_o && !d.vid_reset_released_o,
        "first GPU edge leaves both release chains closed");
  raw_gpu_edge(d);
  check(!d.gpu_reset_released_o, "second GPU edge still holds GPU local reset");
  raw_gpu_edge(d);
  check(d.gpu_reset_released_o && !d.vid_reset_released_o,
        "third GPU edge releases only the GPU domain");

  s.vid_edge(d, "video release edge one");
  check(!d.vid_reset_released_o, "first video edge holds video local reset");
  s.vid_edge(d, "video release edge two");
  check(!d.vid_reset_released_o, "second video edge holds video local reset");
  s.vid_edge(d, "video release edge three");
  check(d.vid_reset_released_o && !d.blank_ack_o,
        "third video edge releases synchronously without executing state");
  s.vid_edge(d, "first active video edge proves black");
  check(d.blank_ack_o, "blank ACK rises only on the first post-release active edge");

  for (int i = 0; i < 4; ++i) raw_gpu_edge(d);
  check(!d.lease_open_o, "blank ACK without CDC barriers cannot open leases");
  d.gpu_barrier_done_i = 1;
  for (int i = 0; i < 6; ++i) raw_gpu_edge(d);
  check(!d.lease_open_o, "GPU barrier alone cannot open leases");
  d.vid_barrier_done_i = 1;
  for (int i = 0; i < 7; ++i) raw_gpu_edge(d);
  check(d.lease_open_o, "both barriers plus synchronized blank ACK open the reset epoch");

  // ---------------------------------------------------------------------- A --
  // Scanout-first ordering with a long reverse-CDC stall.  Scanout ACK alone
  // records one fact but must leave blank asserted.
  const Tuple a = tuple(10, false);
  const Tuple b = tuple(11, true);
  scoreboard_accept_ready(d, s, a, "capture READY A");
  check(d.frame_slot_ready_o == 1, "slot-zero tuple advertises only FRAMECTL bit zero");
  scoreboard_swap(d, s, a.slot, "swap READY A");
  check(d.unblank_candidate_o && sample_tuple(d.unblank_tuple_o) == a && !d.unblank_echo_seen_o &&
            !d.unblank_scanout_seen_o,
        "swap A creates one exact unblank candidate with no facts");

  d.scanout_ack_i = 1;
  s.vid_edge(d, "scanout-first ACK for A");
  d.scanout_ack_i = 0;
  check(d.blank_active_o && d.unblank_candidate_o && !d.unblank_echo_seen_o &&
            d.unblank_scanout_seen_o,
        "scanout-only fact cannot unblank A");
  for (int i = 0; i < 12; ++i) {
    drive_tuple(d.cdc_ready_tuple_i, tuple(20u + static_cast<unsigned>(i), (i & 1) != 0));
    s.vid_edge(d, "long reverse stall after scanout-first A");
  }
  check(d.blank_active_o && d.echo_hold_o && sample_tuple(d.cdc_swap_tuple_o) == a,
        "long echo stall keeps blank and exact A identity");

  // B can occupy pending storage.  When CDC finally accepts A, FRAMECTL may
  // elastically replace the reverse hold with B on that same edge.  A's stored
  // scanout fact plus its exact echo acceptance—not B's capture—unblanks.
  scoreboard_accept_ready(d, s, b, "capture READY B behind stalled A");
  d.cdc_swap_ready_i = 1;
  d.frame_swap_slot_i = b.slot;
  d.frame_swap_valid_i = 1;
  d.eval();
  check(d.frame_slot_ready_o == 2, "A acceptance makes same-edge reverse room for slot-one B");
  s.vid_edge(d, "accept A and elastically replace with B");
  d.frame_swap_valid_i = 0;
  d.cdc_swap_ready_i = 0;
  d.eval();
  check(!d.blank_active_o && !d.unblank_candidate_o,
        "exact A echo acceptance joins stored A scanout fact to unblank");
  check(d.echo_hold_o && sample_tuple(d.cdc_swap_tuple_o) == b && d.scanout_wait_o,
        "elastic replacement independently holds B and awaits B scanout");
  check(s.accepted_echoes.size() == 1 && s.accepted_echoes[0] == a,
        "host scoreboard retires exact A once before B");
  s.vid_edge(d, "first live pixel after exact A join");
  check(d.output_rgb_o == d.scanout_rgb_i && !d.blank_ack_o,
        "live pixel follows only after both A proofs");
  check(d.lease_open_o, "lease epoch remains open after blank ACK falls");

  d.cdc_swap_ready_i = 1;
  d.scanout_ack_i = 1;
  s.vid_edge(d, "drain B echo and scanout together");
  d.cdc_swap_ready_i = 0;
  d.scanout_ack_i = 0;
  check(!d.echo_hold_o && !d.scanout_wait_o, "B drains without disturbing already-open display");

  // ---------------------------------------------------------------------- C --
  // Echo-first ordering with a long scanout stall.  Echo acceptance alone must
  // remain blank until the matching scanout ACK arrives.
  scoreboard_command_blank(d, s);
  const Tuple c = tuple(30, false);
  scoreboard_accept_ready(d, s, c, "capture READY C");
  scoreboard_swap(d, s, c.slot, "swap READY C");
  d.cdc_swap_ready_i = 1;
  s.vid_edge(d, "echo-first acceptance for C");
  d.cdc_swap_ready_i = 0;
  check(d.blank_active_o && d.unblank_candidate_o && d.unblank_echo_seen_o &&
            !d.unblank_scanout_seen_o,
        "echo-only fact cannot unblank C");
  for (int i = 0; i < 12; ++i) s.vid_edge(d, "long scanout stall after echo-first C");
  check(d.blank_active_o && d.scanout_wait_o && !d.echo_hold_o,
        "long scanout stall remains black after C echo acceptance");
  d.scanout_ack_i = 1;
  s.vid_edge(d, "scanout completes echo-first C");
  d.scanout_ack_i = 0;
  check(!d.blank_active_o && !d.unblank_candidate_o,
        "C scanout ACK joins stored C echo fact to unblank");
  s.vid_edge(d, "first live pixel after C join");
  check(d.output_rgb_o == d.scanout_rgb_i, "echo-first C exposes live pixels only after scanout");

  // ---------------------------------------------------------------------- D --
  // Both facts on one edge must complete exactly once.
  scoreboard_command_blank(d, s);
  const Tuple d_tuple = tuple(40, true);
  scoreboard_accept_ready(d, s, d_tuple, "capture READY D");
  scoreboard_swap(d, s, d_tuple.slot, "swap READY D");
  d.cdc_swap_ready_i = 1;
  d.scanout_ack_i = 1;
  s.vid_edge(d, "same-edge echo and scanout facts for D");
  d.cdc_swap_ready_i = 0;
  d.scanout_ack_i = 0;
  check(!d.blank_active_o && !d.unblank_candidate_o && !d.echo_hold_o && !d.scanout_wait_o,
        "same-edge D facts unblank and drain both holds exactly once");
  check(!s.accepted_echoes.empty() && s.accepted_echoes.back() == d_tuple,
        "same-edge path accepts exact D tuple");
  s.vid_edge(d, "first live pixel after same-edge D join");

  // ------------------------------------------------------------------ reset --
  // Blanking itself may not revoke tuple ownership.  Populate both stores,
  // command blank, then prove only asynchronous reset discards them.
  const Tuple e = tuple(50, false);
  const Tuple f = tuple(51, true);
  scoreboard_accept_ready(d, s, e, "pre-reset capture E");
  scoreboard_swap(d, s, e.slot, "pre-reset echo E");
  scoreboard_accept_ready(d, s, f, "pre-reset pending F");
  d.blank_cmd_i = 1;
  s.vid_edge(d, "blank command while E echo and F pending are occupied");
  d.blank_cmd_i = 0;
  d.eval();
  check(d.pending_o && sample_tuple(d.pending_tuple_o) == f && d.echo_hold_o &&
            sample_tuple(d.cdc_swap_tuple_o) == e,
        "blank command preserves pending F and reverse-held E");

  d.gpu_rst_n = 0;
  d.eval();
  s.reset();
  check(!d.gpu_reset_released_o && !d.vid_reset_released_o,
        "GPU reset asynchronously reasserts both local resets");
  check(!d.pending_o && !d.echo_hold_o && !d.cdc_swap_valid_o && d.frame_slot_ready_o == 0,
        "GPU reset asynchronously discards pending and echo tuples");
  check(d.output_rgb_o == 0 && d.blank_active_o && !d.blank_ack_o && !d.lease_open_o,
        "GPU reset asynchronously blacks output and closes lease epoch");

  d.gpu_barrier_done_i = 0;
  d.vid_barrier_done_i = 0;
  d.gpu_rst_n = 1;
  d.eval();
  check(!d.gpu_reset_released_o && !d.vid_reset_released_o,
        "one-sided reset deassertion is synchronous in both domains");
  for (int i = 0; i < 3; ++i) raw_gpu_edge(d);
  check(d.gpu_reset_released_o && !d.vid_reset_released_o,
        "one-sided recovery releases GPU only on its third edge");
  for (int i = 0; i < 4; ++i) s.vid_edge(d, "one-sided reset video release and no leakage");
  check(d.vid_reset_released_o && d.blank_ack_o,
        "one-sided recovery releases video and reproves registered black");
  check(!d.pending_o && !d.echo_hold_o && d.frame_slot_ready_o == 0,
        "no pre-reset E or F tuple leaks after synchronized release");

  d.gpu_barrier_done_i = 1;
  d.vid_barrier_done_i = 1;
  for (int i = 0; i < 7; ++i) raw_gpu_edge(d);
  check(d.lease_open_o, "fresh barriers and synchronized blank proof reopen lease epoch");

  const Tuple g = tuple(60, false);
  scoreboard_accept_ready(d, s, g, "only post-reset READY G");
  scoreboard_swap(d, s, g.slot, "post-reset swap G");
  check(sample_tuple(d.cdc_swap_tuple_o) == g && sample_tuple(d.unblank_tuple_o) == g,
        "only post-reset G reaches echo and unblank candidate");
  d.cdc_swap_ready_i = 1;
  d.scanout_ack_i = 1;
  s.vid_edge(d, "complete both post-reset G facts");
  d.cdc_swap_ready_i = 0;
  d.scanout_ack_i = 0;
  s.vid_edge(d, "live pixel after post-reset G");
  check(d.output_rgb_o != 0, "post-reset output stays black until exact G completes both facts");

  d.vid_rst_n = 0;
  d.eval();
  s.reset();
  check(!d.gpu_reset_released_o && !d.vid_reset_released_o,
        "video reset asynchronously reasserts both local resets");
  check(!d.pending_o && !d.echo_hold_o && !d.cdc_swap_valid_o,
        "video reset asynchronously discards all tuple state");
  check(d.output_rgb_o == 0 && d.blank_active_o && !d.blank_ack_o && !d.lease_open_o,
        "video reset asynchronously blacks output and closes lease epoch");

  finish("video_ready_bridge_v2_directed");
#endif

  // Implicit `return 0` deadlocks the same way -- see zhao_sim.hpp.
  zhao::exit_hard(0);
}
