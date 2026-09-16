// video_terminal_adapter_v2_directed.cpp -- Packet-H terminal join scoreboard.
#if (defined(ZHAO_EXPECT_VIDEO_TERM_DROP_HELD) + \
     defined(ZHAO_EXPECT_VIDEO_TERM_CHANGE_HELD) + \
     defined(ZHAO_EXPECT_VIDEO_TERM_WRONG_WRITER) + \
     defined(ZHAO_EXPECT_VIDEO_TERM_PUBLISH_WINS) + \
     defined(ZHAO_EXPECT_VIDEO_TERM_RENDER_READY) + \
     defined(ZHAO_EXPECT_VIDEO_TERM_SILENT_BLIT_DROP) + \
     defined(ZHAO_EXPECT_VIDEO_TERM_NO_POP_REPLACE)) > 1
#error ZHAO_VIDEO_TERMINAL_ADAPTER_V2_CPP_MUTANT_SELECTOR_COLLISION
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "verilated.h"
#include "Vtb_video_terminal_adapter_v2.h"

double sc_time_stamp() { return 0.0; }

namespace {

using Dut = Vtb_video_terminal_adapter_v2;

struct Term {
  bool writer;
  bool slot;
  uint16_t generation;
  bool publish;
  bool fault;
};

bool operator==(const Term& a, const Term& b) {
  return a.writer == b.writer && a.slot == b.slot &&
         a.generation == b.generation && a.publish == b.publish &&
         a.fault == b.fault;
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

void quiet(Dut& d) {
  d.blit_publish_valid_i = 0;
  d.blit_publish_slot_i = 0;
  d.blit_publish_generation_i = 0;
  d.blit_release_valid_i = 0;
  d.blit_release_slot_i = 0;
  d.blit_release_generation_i = 0;
  d.renderer_term_valid_i = 0;
  d.renderer_term_slot_i = 0;
  d.renderer_term_generation_i = 0;
  d.renderer_term_publish_i = 0;
  d.renderer_term_fault_i = 0;
  d.term_ready_i = 0;
}

void tick(Dut& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
  d.clk = 0;
  d.eval();
}

void reset(Dut& d) {
  quiet(d);
  d.clk = 0;
  d.rst_n = 1;
  d.eval();
  d.rst_n = 0;
  d.eval();
  tick(d);
  d.rst_n = 1;
  d.eval();
}

Term sample(const Dut& d) {
  return Term{d.term_writer_o != 0, d.term_slot_o != 0,
              static_cast<uint16_t>(d.term_generation_o),
              d.term_publish_o != 0, d.term_fault_o != 0};
}

void drive_blit_publish(Dut& d, bool slot, uint16_t generation) {
  d.blit_publish_valid_i = 1;
  d.blit_publish_slot_i = slot;
  d.blit_publish_generation_i = generation;
}

void drive_blit_release(Dut& d, bool slot, uint16_t generation) {
  d.blit_release_valid_i = 1;
  d.blit_release_slot_i = slot;
  d.blit_release_generation_i = generation;
}

void drive_renderer(Dut& d, const Term& value) {
  d.renderer_term_valid_i = 1;
  d.renderer_term_slot_i = value.slot;
  d.renderer_term_generation_i = value.generation;
  d.renderer_term_publish_i = value.publish;
  d.renderer_term_fault_i = value.fault;
}

void check_visible(Dut& d, const Term& expected, const char* what) {
  d.eval();
  check(d.term_valid_o && d.occupied_o && !d.idle_o && sample(d) == expected,
        what);
}

void pop(Dut& d) {
  d.term_ready_i = 1;
  d.eval();
  check(d.term_valid_o, "manager sees valid before terminal acceptance");
  tick(d);
  d.term_ready_i = 0;
  d.eval();
}

struct Model {
  bool occupied = false;
  Term held{};
  uint32_t refused = 0;
  uint32_t captured = 0;
  uint32_t accepted = 0;

  void reset() {
    occupied = false;
    held = Term{};
    refused = 0;
    captured = 0;
    accepted = 0;
  }

  void compare(Dut& d, const char* phase) const {
    d.eval();
    if ((d.occupied_o != 0) != occupied || (d.idle_o != 0) == occupied) {
      std::printf("  phase: %s\n", phase);
      check(false, "scoreboard occupancy agrees");
    } else {
      check(true, "scoreboard occupancy agrees");
    }
    check((d.term_valid_o != 0) == occupied,
          "scoreboard terminal valid agrees");
    if (occupied)
      check(sample(d) == held, "scoreboard complete held tuple agrees");
    check(static_cast<uint32_t>(d.blit_events_refused_o) == refused,
          "scoreboard blitter refusal counter agrees");
    check(static_cast<uint32_t>(d.source_events_captured_o) == captured,
          "scoreboard source capture counter agrees");
    check(static_cast<uint32_t>(d.manager_terms_accepted_o) == accepted,
          "scoreboard manager acceptance counter agrees");
  }

  void edge(Dut& d, const char* phase) {
    d.clk = 0;
    d.eval();
    compare(d, phase);

    const bool pop_now = occupied && d.term_ready_i;
    const bool room = !occupied || pop_now;
    const bool bp = d.blit_publish_valid_i;
    const bool br = d.blit_release_valid_i;
    const bool same = bp && br &&
        (static_cast<bool>(d.blit_publish_slot_i) ==
         static_cast<bool>(d.blit_release_slot_i)) &&
        (static_cast<uint16_t>(d.blit_publish_generation_i) ==
         static_cast<uint16_t>(d.blit_release_generation_i));
    const unsigned blit_count = (!bp && !br) ? 0u : (bp && br && !same ? 2u : 1u);
    const bool renderer_release = d.renderer_term_fault_i ||
                                  !d.renderer_term_publish_i;
    bool select_blit = false;
    bool select_renderer = false;
    if (room) {
      if (br) select_blit = true;
      else if (d.renderer_term_valid_i && renderer_release)
        select_renderer = true;
      else if (bp) select_blit = true;
      else if (d.renderer_term_valid_i) select_renderer = true;
    }
    const bool push = select_blit || select_renderer;
    unsigned refused_delta = 0;
    if (blit_count != 0) {
      if (!select_blit) refused_delta = blit_count;
      else if (blit_count == 2) refused_delta = 1;
    }
    Term next{};
    if (select_blit) {
      if (br) {
        next = Term{false, d.blit_release_slot_i != 0,
                    static_cast<uint16_t>(d.blit_release_generation_i),
                    false, false};
      } else {
        next = Term{false, d.blit_publish_slot_i != 0,
                    static_cast<uint16_t>(d.blit_publish_generation_i),
                    true, false};
      }
    } else if (select_renderer) {
      const bool fault = d.renderer_term_fault_i != 0;
      next = Term{true, d.renderer_term_slot_i != 0,
                  static_cast<uint16_t>(d.renderer_term_generation_i),
                  (d.renderer_term_publish_i != 0) && !fault, fault};
    }

    check((d.renderer_term_ready_o != 0) == select_renderer,
          "scoreboard renderer ready selects exactly one source");
    check((d.blit_refused_o != 0) == (refused_delta != 0),
          "scoreboard blitter refusal pulse agrees");

    d.clk = 1;
    d.eval();
    d.clk = 0;
    d.eval();

    refused += refused_delta;
    if (pop_now) ++accepted;
    if (push) ++captured;
    if (push) {
      occupied = true;
      held = next;
    } else if (pop_now) {
      occupied = false;
      held = Term{};
    }
    compare(d, phase);
  }
};

void run_mutant(Dut& d) {
#if defined(ZHAO_EXPECT_VIDEO_TERM_DROP_HELD)
  reset(d);
  drive_blit_publish(d, false, 0x1010);
  tick(d);
  quiet(d);
  d.eval();
  mutant_result("video_term_drop_held", d.occupied_o && !d.term_valid_o);
#elif defined(ZHAO_EXPECT_VIDEO_TERM_CHANGE_HELD)
  reset(d);
  drive_blit_publish(d, false, 0x2020);
  tick(d);
  quiet(d);
  drive_renderer(d, Term{true, true, 0x3030, false, true});
  d.eval();
  mutant_result("video_term_change_held",
                d.term_valid_o && sample(d) == Term{true, true, 0x3030, false, true});
#elif defined(ZHAO_EXPECT_VIDEO_TERM_WRONG_WRITER)
  reset(d);
  drive_blit_publish(d, true, 0x4040);
  tick(d);
  quiet(d);
  d.eval();
  mutant_result("video_term_wrong_writer", d.term_valid_o && d.term_writer_o);
#elif defined(ZHAO_EXPECT_VIDEO_TERM_PUBLISH_WINS)
  reset(d);
  drive_blit_publish(d, true, 0x5050);
  drive_blit_release(d, true, 0x5050);
  tick(d);
  quiet(d);
  d.eval();
  mutant_result("video_term_publish_wins",
                d.term_valid_o && d.term_publish_o && !d.term_fault_o);
#elif defined(ZHAO_EXPECT_VIDEO_TERM_RENDER_READY)
  reset(d);
  drive_blit_release(d, false, 0x6060);
  drive_renderer(d, Term{true, true, 0x6161, false, true});
  d.eval();
  mutant_result("video_term_render_ready", d.renderer_term_ready_o);
#elif defined(ZHAO_EXPECT_VIDEO_TERM_SILENT_BLIT_DROP)
  reset(d);
  drive_blit_publish(d, false, 0x7070);
  tick(d);
  quiet(d);
  drive_blit_release(d, true, 0x7171);
  d.eval();
  mutant_result("video_term_silent_blit_drop",
                !d.blit_refused_o && d.blit_events_refused_o == 0);
#elif defined(ZHAO_EXPECT_VIDEO_TERM_NO_POP_REPLACE)
  reset(d);
  drive_renderer(d, Term{true, false, 0x8080, true, false});
  tick(d);
  quiet(d);
  d.term_ready_i = 1;
  drive_blit_publish(d, true, 0x8181);
  d.eval();
  mutant_result("video_term_no_pop_replace", d.blit_refused_o);
#else
  (void)d;
#endif
}

void directed(Dut& d) {
  reset(d);
  check(d.idle_o && !d.occupied_o && !d.term_valid_o &&
            !d.blit_refused_o && d.blit_events_refused_o == 0 &&
            d.source_events_captured_o == 0 &&
            d.manager_terms_accepted_o == 0,
        "reset clears storage and all counters");

  const Term blit_publish{false, true, 0x1234, true, false};
  drive_blit_publish(d, blit_publish.slot, blit_publish.generation);
  d.eval();
  check(!d.blit_refused_o, "empty adapter accepts clean blitter publication");
  tick(d);
  quiet(d);
  check_visible(d, blit_publish, "blitter publication retains full identity");
  check(d.source_events_captured_o == 1 && d.manager_terms_accepted_o == 0,
        "capture and manager counters separate held ownership");

  for (unsigned i = 0; i < 4; ++i) {
    d.blit_publish_slot_i = (i & 1u) != 0;
    d.blit_publish_generation_i = static_cast<uint16_t>(0x9000u + i);
    d.blit_release_slot_i = (i & 1u) == 0;
    d.blit_release_generation_i = static_cast<uint16_t>(0xa000u + i);
    d.renderer_term_slot_i = (i & 1u) == 0;
    d.renderer_term_generation_i = static_cast<uint16_t>(0xb000u + i);
    d.renderer_term_publish_i = i & 1u;
    d.renderer_term_fault_i = (i & 2u) != 0;
    d.eval();
    check(d.term_valid_o && sample(d) == blit_publish,
          "stalled manager tuple ignores every changing input bus");
    tick(d);
  }
  quiet(d);
  pop(d);
  check(d.idle_o && d.manager_terms_accepted_o == 1,
        "manager acceptance releases exactly one held terminal");

  const Term render_publish{true, false, 0x2345, true, false};
  drive_renderer(d, render_publish);
  d.eval();
  check(d.renderer_term_ready_o,
        "renderer clean publication sees ready when selected");
  tick(d);
  quiet(d);
  check_visible(d, render_publish, "renderer publication retains writer and key");
  pop(d);

  const Term render_fault{true, true, 0x3456, true, true};
  drive_renderer(d, render_fault);
  d.eval();
  check(d.renderer_term_ready_o, "renderer fault terminal is selected");
  tick(d);
  quiet(d);
  check_visible(d, Term{true, true, 0x3456, false, true},
                "renderer fault suppresses publication and remains explicit");
  pop(d);

  drive_renderer(d, Term{true, false, 0x4567, false, false});
  tick(d);
  quiet(d);
  check_visible(d, Term{true, false, 0x4567, false, false},
                "renderer cancellation becomes a release terminal");
  pop(d);

  drive_blit_publish(d, true, 0x5678);
  drive_blit_release(d, true, 0x5678);
  d.eval();
  check(!d.blit_refused_o,
        "matching blitter publish and release resolve as one event");
  tick(d);
  quiet(d);
  check_visible(d, Term{false, true, 0x5678, false, false},
                "matching release wins and can never enqueue READY");
  pop(d);

  const uint32_t refused_before_mismatch = d.blit_events_refused_o;
  drive_blit_publish(d, false, 0x6001);
  drive_blit_release(d, true, 0x6002);
  d.eval();
  check(d.blit_refused_o,
        "different simultaneous blitter keys report the losing publication");
  tick(d);
  quiet(d);
  check_visible(d, Term{false, true, 0x6002, false, false},
                "different-key release retains its own exact identity");
  check(d.blit_events_refused_o == refused_before_mismatch + 1,
        "different-key pair increments refusal count once");

  const uint32_t refused_before_full = d.blit_events_refused_o;
  drive_blit_publish(d, false, 0x7001);
  drive_blit_release(d, true, 0x7002);
  d.eval();
  check(d.blit_refused_o,
        "occupied adapter explicitly refuses pulse-only blitter traffic");
  tick(d);
  quiet(d);
  check(d.blit_events_refused_o == refused_before_full + 2,
        "two unrelated occupied blitter pulses count as two refusals");
  check_visible(d, Term{false, true, 0x6002, false, false},
                "refused pulses cannot overwrite the held terminal");
  pop(d);

  // Renderer release outranks a clean blitter publication. The unbackpressurable
  // blitter event is explicitly refused rather than silently merged.
  const uint32_t refused_before_cross = d.blit_events_refused_o;
  drive_blit_publish(d, false, 0x8001);
  drive_renderer(d, Term{true, true, 0x8002, false, true});
  d.eval();
  check(d.renderer_term_ready_o && d.blit_refused_o,
        "renderer fault outranks and explicitly refuses clean blitter publish");
  tick(d);
  quiet(d);
  check_visible(d, Term{true, true, 0x8002, false, true},
                "cross-writer fault selection preserves renderer identity");
  check(d.blit_events_refused_o == refused_before_cross + 1,
        "cross-writer refused blit is counted exactly once");
  pop(d);

  // Blitter release wins a release-class tie because its pulse cannot wait. The
  // renderer observes ready low, retains its tuple, and is accepted next.
  const Term held_renderer{true, true, 0x9002, false, true};
  drive_blit_release(d, false, 0x9001);
  drive_renderer(d, held_renderer);
  d.eval();
  check(!d.renderer_term_ready_o && !d.blit_refused_o,
        "blitter release wins tie while renderer remains backpressured");
  tick(d);
  d.blit_release_valid_i = 0;
  d.eval();
  check_visible(d, Term{false, false, 0x9001, false, false},
                "release tie captures the pulse-only blitter key");
  check(!d.renderer_term_ready_o,
        "held renderer cannot be accepted while output remains occupied");
  d.term_ready_i = 1;
  d.eval();
  check(d.renderer_term_ready_o,
        "held renderer is selected on same-edge pop replacement");
  tick(d);
  quiet(d);
  check_visible(d, held_renderer,
                "pop replacement retains the previously losing renderer tuple");
  check(d.occupied_o, "elastic pop replacement has no empty bubble");
  pop(d);

  // Pop and pulse replacement exercises the inverse direction.
  drive_renderer(d, Term{true, false, 0xa001, true, false});
  tick(d);
  quiet(d);
  d.term_ready_i = 1;
  drive_blit_publish(d, true, 0xa002);
  d.eval();
  check(!d.blit_refused_o,
        "same-edge pop creates room for a pulse-only blitter event");
  tick(d);
  quiet(d);
  check_visible(d, Term{false, true, 0xa002, true, false},
                "pop replacement captures exact blitter publication");

  // Asynchronous reset is the only operation allowed to discard an owned term.
  d.rst_n = 0;
  d.eval();
  check(d.idle_o && !d.occupied_o && !d.term_valid_o &&
            d.blit_events_refused_o == 0 &&
            d.source_events_captured_o == 0 &&
            d.manager_terms_accepted_o == 0,
        "reset immediately drops held state and clears observations");
  d.rst_n = 1;
  quiet(d);
  tick(d);
  check(d.idle_o && !d.term_valid_o,
        "pre-reset terminal never leaks into the new epoch");
}

void randomized_scoreboard(Dut& d) {
  reset(d);
  Model model;
  model.reset();
  uint32_t rng = 0xc001d00du;
  bool render_pending = false;
  Term render{};

  auto next = [&]() {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };

  for (unsigned cycle = 0; cycle < 1200; ++cycle) {
    quiet(d);
    d.term_ready_i = ((next() >> 16) & 1u) != 0;

    if (!render_pending && ((next() >> 16) & 7u) == 0) {
      render_pending = true;
      const uint32_t word = next();
      render = Term{true, (word & 1u) != 0,
                    static_cast<uint16_t>(word >> 8),
                    (word & 2u) != 0, (word & 4u) != 0};
    }
    if (render_pending) drive_renderer(d, render);

    const uint32_t pulse = next();
    if (((pulse >> 16) & 7u) == 0)
      drive_blit_publish(d, (pulse & 1u) != 0,
                         static_cast<uint16_t>(next()));
    if (((pulse >> 20) & 15u) == 1)
      drive_blit_release(d, (pulse & 2u) != 0,
                         static_cast<uint16_t>(next()));
    if (((pulse >> 24) & 63u) == 3) {
      // Deliberate matching publish+release resolution point.
      const bool slot = (pulse & 4u) != 0;
      const uint16_t generation = static_cast<uint16_t>(next());
      drive_blit_publish(d, slot, generation);
      drive_blit_release(d, slot, generation);
    }

    d.eval();
    const bool render_fire = render_pending && d.renderer_term_ready_o;
    model.edge(d, "randomized terminal stream");
    if (render_fire) render_pending = false;
  }

  // Stop creating new work and drain both the held renderer and manager entry.
  for (unsigned guard = 0; guard < 8 && (render_pending || model.occupied); ++guard) {
    quiet(d);
    d.term_ready_i = 1;
    if (render_pending) drive_renderer(d, render);
    d.eval();
    const bool render_fire = render_pending && d.renderer_term_ready_o;
    model.edge(d, "randomized drain");
    if (render_fire) render_pending = false;
  }
  check(!render_pending && !model.occupied && d.idle_o,
        "randomized scoreboard drains every accepted source event");
  check(model.captured == model.accepted,
        "randomized accepted-source and manager-retirement counts conserve");
  check(model.captured > 40 && model.refused > 5,
        "randomized run reaches accepted and refused traffic");
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut d;
  d.clk = 0;
  d.rst_n = 0;
  quiet(d);
  d.eval();

#if defined(ZHAO_EXPECT_VIDEO_TERM_DROP_HELD) || \
    defined(ZHAO_EXPECT_VIDEO_TERM_CHANGE_HELD) || \
    defined(ZHAO_EXPECT_VIDEO_TERM_WRONG_WRITER) || \
    defined(ZHAO_EXPECT_VIDEO_TERM_PUBLISH_WINS) || \
    defined(ZHAO_EXPECT_VIDEO_TERM_RENDER_READY) || \
    defined(ZHAO_EXPECT_VIDEO_TERM_SILENT_BLIT_DROP) || \
    defined(ZHAO_EXPECT_VIDEO_TERM_NO_POP_REPLACE)
  run_mutant(d);
  mutant_result("video_term_unknown_mutant", false);
#else
  directed(d);
  randomized_scoreboard(d);
  finish("video_terminal_adapter_v2_directed");
#endif
}
