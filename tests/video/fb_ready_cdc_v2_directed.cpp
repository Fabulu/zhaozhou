// fb_ready_cdc_v2_directed.cpp -- Packet-G dual-clock READY/swap queues.
#if (defined(ZHAO_EXPECT_CDC_MUTANT_IGNORE_FULL) + defined(ZHAO_EXPECT_CDC_ASSERT_IGNORE_FULL) + \
     defined(ZHAO_EXPECT_CDC_MUTANT_BYPASS_BARRIER) +                                            \
     defined(ZHAO_EXPECT_CDC_MUTANT_ZERO_READY_GENERATION) +                                     \
     defined(ZHAO_EXPECT_CDC_MUTANT_REPEAT_READ)) > 1
#error ZHAO_FB_READY_CDC_V2_CPP_MUTANT_SELECTOR_COLLISION
#endif

#include <cstdint>
#include <cstdio>

#include "verilated.h"
#include "Vzhao_fb_ready_cdc_v2.h"
#include "zhao_sim.hpp"

namespace {

using Dut = Vzhao_fb_ready_cdc_v2;

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

template <typename Wide>
void drive_tuple(Wide& port, const Tuple& t) {
  port[0] = t.span;
  port[1] = t.base;
  port[2] = static_cast<uint32_t>(t.mode & 3u) | (static_cast<uint32_t>(t.generation) << 2) |
            (static_cast<uint32_t>(t.slot) << 18) | (static_cast<uint32_t>(t.writer) << 19);
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

Tuple tuple(unsigned n) {
  return Tuple{(n & 1u) != 0,
               (n & 2u) != 0,
               static_cast<uint16_t>(0x1200u + n),
               static_cast<uint8_t>(n % 3u),
               0x01000000u + 0x101u * n,
               0x00030000u + 4u * n};
}

struct ClockSim {
  Dut& d;
  uint64_t next_gpu = 2;
  uint64_t next_vid = 5;
  uint64_t gpu_period = 6;
  uint64_t vid_period = 10;

  unsigned next_mask() const {
    const uint64_t t = next_gpu < next_vid ? next_gpu : next_vid;
    return (next_gpu == t ? 1u : 0u) | (next_vid == t ? 2u : 0u);
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
};

void clear_inputs(Dut& d) {
  d.gpu_ready_valid_i = 0;
  drive_tuple(d.gpu_ready_tuple_i, tuple(0));
  d.vid_ready_ready_i = 0;
  d.vid_swap_valid_i = 0;
  drive_tuple(d.vid_swap_tuple_i, tuple(0));
  d.gpu_swap_ready_i = 0;
}

void assert_reset(Dut& d) {
  d.gpu_rst_n = 0;
  d.vid_rst_n = 0;
  d.gpu_clk = 0;
  d.vid_clk = 0;
  clear_inputs(d);
  d.eval();
}

bool open_barrier(ClockSim& sim, int max_events = 200) {
  sim.d.gpu_rst_n = 1;
  sim.d.vid_rst_n = 1;
  sim.d.eval();
  for (int i = 0; i < max_events; ++i) {
    if (sim.d.gpu_barrier_done_o && sim.d.vid_barrier_done_o) return true;
    sim.event();
  }
  return false;
}

bool push_ready(ClockSim& sim, const Tuple& value, int max_events = 300) {
  drive_tuple(sim.d.gpu_ready_tuple_i, value);
  sim.d.gpu_ready_valid_i = 1;
  sim.d.eval();
  for (int i = 0; i < max_events; ++i) {
    const bool accept = (sim.next_mask() & 1u) && sim.d.gpu_ready_ready_o;
    sim.event();
    if (accept) {
      sim.d.gpu_ready_valid_i = 0;
      sim.d.eval();
      return true;
    }
  }
  sim.d.gpu_ready_valid_i = 0;
  sim.d.eval();
  return false;
}

bool push_swap(ClockSim& sim, const Tuple& value, int max_events = 300) {
  drive_tuple(sim.d.vid_swap_tuple_i, value);
  sim.d.vid_swap_valid_i = 1;
  sim.d.eval();
  for (int i = 0; i < max_events; ++i) {
    const bool accept = (sim.next_mask() & 2u) && sim.d.vid_swap_ready_o;
    sim.event();
    if (accept) {
      sim.d.vid_swap_valid_i = 0;
      sim.d.eval();
      return true;
    }
  }
  sim.d.vid_swap_valid_i = 0;
  sim.d.eval();
  return false;
}

bool pop_ready(ClockSim& sim, Tuple* value, int max_events = 500) {
  sim.d.vid_ready_ready_i = 1;
  sim.d.eval();
  for (int i = 0; i < max_events; ++i) {
    const bool accept = (sim.next_mask() & 2u) && sim.d.vid_ready_valid_o;
    if (accept && value) *value = sample_tuple(sim.d.vid_ready_tuple_o);
    sim.event();
    if (accept) {
      sim.d.vid_ready_ready_i = 0;
      sim.d.eval();
      return true;
    }
  }
  sim.d.vid_ready_ready_i = 0;
  sim.d.eval();
  return false;
}

bool pop_swap(ClockSim& sim, Tuple* value, int max_events = 500) {
  sim.d.gpu_swap_ready_i = 1;
  sim.d.eval();
  for (int i = 0; i < max_events; ++i) {
    const bool accept = (sim.next_mask() & 1u) && sim.d.gpu_swap_valid_o;
    if (accept && value) *value = sample_tuple(sim.d.gpu_swap_tuple_o);
    sim.event();
    if (accept) {
      sim.d.gpu_swap_ready_i = 0;
      sim.d.eval();
      return true;
    }
  }
  sim.d.gpu_swap_ready_i = 0;
  sim.d.eval();
  return false;
}

[[noreturn]] void mutant_result(const char* name, bool detected) {
  std::printf("[%s] %s\n", name, detected ? "DETECTED" : "MISSED");
  zhao::exit_hard(detected ? 0 : 1);
}

}  // namespace

int main() {
  Dut d;
  assert_reset(d);
  ClockSim sim{d};

#if defined(ZHAO_EXPECT_CDC_MUTANT_BYPASS_BARRIER)
  mutant_result("fb_ready_cdc_v2_bypass_barrier", d.gpu_ready_ready_o || d.vid_swap_ready_o);
#else
  zhao::check(
      !d.gpu_barrier_done_o && !d.vid_barrier_done_o && !d.gpu_ready_ready_o && !d.vid_swap_ready_o,
      "paired reset closes both channels", 1, 1);
  zhao::check(!d.vid_ready_valid_o && !d.gpu_swap_valid_o, "paired reset clears both outputs", 1,
              1);
  zhao::check(open_barrier(sim), "both reset barriers complete", 1, 1);

#if defined(ZHAO_EXPECT_CDC_ASSERT_IGNORE_FULL)
  d.vid_ready_ready_i = 0;
  for (unsigned i = 0; i < 5; ++i)
    if (!push_ready(sim, tuple(i)))
      mutant_result("fb_ready_cdc_v2_ignore_full_assertion_setup", false);
  sim.events(20);
  mutant_result("fb_ready_cdc_v2_ignore_full_assertion", false);
#elif defined(ZHAO_EXPECT_CDC_MUTANT_IGNORE_FULL)
  d.vid_ready_ready_i = 0;
  bool five = true;
  for (unsigned i = 0; i < 5; ++i) five &= push_ready(sim, tuple(i));
  mutant_result("fb_ready_cdc_v2_ignore_full", five && d.ready_enqueued_o == 5);
#elif defined(ZHAO_EXPECT_CDC_MUTANT_ZERO_READY_GENERATION)
  Tuple sent = tuple(7);
  sent.generation = 0x4a5bu;
  Tuple got{};
  const bool moved = push_ready(sim, sent) && pop_ready(sim, &got);
  mutant_result("fb_ready_cdc_v2_zero_ready_generation",
                moved && got.generation == 0 && !(got == sent));
#elif defined(ZHAO_EXPECT_CDC_MUTANT_REPEAT_READ)
  Tuple a = tuple(1), b = tuple(2), x{}, y{};
  const bool moved =
      push_ready(sim, a) && push_ready(sim, b) && pop_ready(sim, &x) && pop_ready(sim, &y);
  mutant_result("fb_ready_cdc_v2_repeat_read", moved && x == a && y == a);
#else
  // Four RAM entries are usable; the fifth is held without overwrite.
  d.vid_ready_ready_i = 0;
  for (unsigned i = 0; i < 4; ++i)
    zhao::check(push_ready(sim, tuple(i)), "READY FIFO accepts depth-four fill", 1, 1);
  drive_tuple(d.gpu_ready_tuple_i, tuple(4));
  d.gpu_ready_valid_i = 1;
  d.eval();
  bool observed_full = false;
  for (int i = 0; i < 40; ++i) {
    if ((sim.next_mask() & 1u) && !d.gpu_ready_ready_o) observed_full = true;
    sim.event();
    if (observed_full) break;
  }
  zhao::check(observed_full && d.ready_enqueued_o == 4,
              "fifth READY is backpressured at exact depth", 4, d.ready_enqueued_o);

  // The source-stability detector has a positive control while genuinely full.
  sim.events(4);
  drive_tuple(d.gpu_ready_tuple_i, tuple(9));
  d.eval();
  while (!(sim.next_mask() & 1u)) sim.event();
  sim.event();
  zhao::check(d.gpu_protocol_fault_o, "GPU stalled-payload drift detector fires", 1,
              d.gpu_protocol_fault_o);
  d.gpu_ready_valid_i = 0;
  d.eval();

  Tuple got{};
  for (unsigned i = 0; i < 4; ++i) {
    zhao::check(pop_ready(sim, &got), "READY FIFO drains", 1, 1);
    zhao::check(got == tuple(i), "READY tuple order and all fields exact", 1, 1);
  }
  sim.events(30);
  zhao::check(d.ready_enqueued_o == 4 && d.ready_dequeued_o == 4,
              "READY enqueue/dequeue accounting exact", d.ready_enqueued_o, d.ready_dequeued_o);

  // The reverse FIFO independently carries four exact tuples in order.
  d.gpu_swap_ready_i = 0;
  for (unsigned i = 10; i < 14; ++i)
    zhao::check(push_swap(sim, tuple(i)), "swap FIFO accepts depth-four fill", 1, 1);
  drive_tuple(d.vid_swap_tuple_i, tuple(14));
  d.vid_swap_valid_i = 1;
  d.eval();
  bool swap_full = false;
  for (int i = 0; i < 40; ++i) {
    if ((sim.next_mask() & 2u) && !d.vid_swap_ready_o) swap_full = true;
    sim.event();
    if (swap_full) break;
  }
  zhao::check(swap_full && d.swap_enqueued_o == 4, "fifth swap is backpressured at exact depth", 4,
              d.swap_enqueued_o);
  sim.events(4);
  drive_tuple(d.vid_swap_tuple_i, tuple(19));
  d.eval();
  while (!(sim.next_mask() & 2u)) sim.event();
  sim.event();
  zhao::check(d.vid_protocol_fault_o, "VID stalled-payload drift detector fires", 1,
              d.vid_protocol_fault_o);
  d.vid_swap_valid_i = 0;
  d.eval();
  for (unsigned i = 10; i < 14; ++i) {
    zhao::check(pop_swap(sim, &got), "swap FIFO drains", 1, 1);
    zhao::check(got == tuple(i), "swap tuple order and all fields exact", 1, 1);
  }
  sim.events(30);
  zhao::check(d.swap_enqueued_o == 4 && d.swap_dequeued_o == 4,
              "swap enqueue/dequeue accounting exact", d.swap_enqueued_o, d.swap_dequeued_o);

  // Pop/reload under continuous destination readiness preserves order.
  for (unsigned i = 20; i < 24; ++i)
    zhao::check(push_ready(sim, tuple(i)), "reload sequence accepted", 1, 1);
  d.vid_ready_ready_i = 1;
  d.eval();
  unsigned seen = 0;
  for (int events = 0; events < 500 && seen < 4; ++events) {
    if ((sim.next_mask() & 2u) && d.vid_ready_valid_o) {
      got = sample_tuple(d.vid_ready_tuple_o);
      zhao::check(got == tuple(20 + seen), "continuous READY pop/reload remains ordered", 1, 1);
      ++seen;
    }
    sim.event();
  }
  d.vid_ready_ready_i = 0;
  d.eval();
  zhao::check(seen == 4, "continuous drain emits every tuple once", 4, seen);

  // A reset with FIFO occupancy discards every pre-reset tuple. Holding a new
  // source offer through reset is accepted only after the paired barrier.
  zhao::check(push_ready(sim, tuple(30)) && push_ready(sim, tuple(31)),
              "pre-reset occupancy created", 1, 1);
  drive_tuple(d.gpu_ready_tuple_i, tuple(32));
  d.gpu_ready_valid_i = 1;
  d.vid_rst_n = 0;
  d.eval();
  zhao::check(
      !d.gpu_ready_ready_o && !d.vid_swap_ready_o && !d.vid_ready_valid_o && !d.gpu_swap_valid_o,
      "one-sided reset immediately closes and flushes pair", 1, 1);
  sim.events(8);
  zhao::check(d.ready_enqueued_o == 0 && d.ready_dequeued_o == 0, "reset clears READY accounting",
              0, d.ready_enqueued_o + d.ready_dequeued_o);
  d.vid_rst_n = 1;
  d.eval();
  bool post_reset_accept = false;
  bool post_reset_barriers = false;
  for (int i = 0; i < 200 && !(post_reset_accept && post_reset_barriers); ++i) {
    const bool accept = !post_reset_accept && (sim.next_mask() & 1u) && d.gpu_ready_ready_o;
    sim.event();
    if (accept) {
      post_reset_accept = true;
      d.gpu_ready_valid_i = 0;
      d.eval();
    }
    post_reset_barriers = d.gpu_barrier_done_o && d.vid_barrier_done_o;
  }
  zhao::check(post_reset_barriers, "barrier reopens after one-sided reset", 1, post_reset_barriers);
  zhao::check(post_reset_accept, "held post-reset offer accepted after barrier", 1,
              post_reset_accept);
  zhao::check(pop_ready(sim, &got) && got == tuple(32), "only post-reset tuple survives", 1, 1);
  sim.events(60);
  zhao::check(!d.vid_ready_valid_o, "no pre-reset READY event reappears", 0, d.vid_ready_valid_o);

  // Reset while a RAM read is pending, then while an output is held.
  zhao::check(push_ready(sim, tuple(40)), "pending-read setup accepted", 1, 1);
  for (int i = 0; i < 300 && (d.vid_idle_o || d.vid_ready_valid_o); ++i) sim.event();
  zhao::check(!d.vid_idle_o && !d.vid_ready_valid_o,
              "pending-read setup becomes visible before issue", 1, 1);
  while (!(sim.next_mask() & 2u)) sim.event();
  sim.event();  // RAM read issued; registered data has not reached the hold yet.
  zhao::check(!d.vid_ready_valid_o, "RAM read is pending for one VID edge", 0, d.vid_ready_valid_o);
  d.gpu_rst_n = 0;
  d.eval();
  sim.events(6);
  d.gpu_rst_n = 1;
  d.eval();
  zhao::check(open_barrier(sim), "barrier recovers from pending-read reset", 1, 1);
  sim.events(60);
  zhao::check(!d.vid_ready_valid_o, "pending pre-reset RAM read cannot survive", 0,
              d.vid_ready_valid_o);

  zhao::check(push_ready(sim, tuple(41)), "held-output setup accepted", 1, 1);
  for (int i = 0; i < 300 && !d.vid_ready_valid_o; ++i) sim.event();
  zhao::check(d.vid_ready_valid_o, "output became held", 1, d.vid_ready_valid_o);
  d.vid_rst_n = 0;
  d.eval();
  zhao::check(!d.vid_ready_valid_o, "held output clears asynchronously", 0, d.vid_ready_valid_o);
  sim.events(6);
  d.vid_rst_n = 1;
  d.eval();
  zhao::check(open_barrier(sim), "barrier recovers from held-output reset", 1, 1);
  sim.events(60);
  zhao::check(!d.vid_ready_valid_o, "held pre-reset output cannot reappear", 0,
              d.vid_ready_valid_o);

  // Mirror every reset-in-flight shape on the reverse swap channel.
  zhao::check(push_swap(sim, tuple(42)) && push_swap(sim, tuple(43)),
              "pre-reset swap occupancy created", 1, 1);
  drive_tuple(d.vid_swap_tuple_i, tuple(44));
  d.vid_swap_valid_i = 1;
  d.gpu_rst_n = 0;
  d.eval();
  zhao::check(
      !d.gpu_ready_ready_o && !d.vid_swap_ready_o && !d.vid_ready_valid_o && !d.gpu_swap_valid_o,
      "GPU reset immediately flushes reverse channel", 1, 1);
  sim.events(8);
  d.gpu_rst_n = 1;
  d.eval();
  bool post_reset_swap_accept = false;
  bool reverse_barriers = false;
  for (int i = 0; i < 200 && !(post_reset_swap_accept && reverse_barriers); ++i) {
    const bool accept = !post_reset_swap_accept && (sim.next_mask() & 2u) && d.vid_swap_ready_o;
    sim.event();
    if (accept) {
      post_reset_swap_accept = true;
      d.vid_swap_valid_i = 0;
      d.eval();
    }
    reverse_barriers = d.gpu_barrier_done_o && d.vid_barrier_done_o;
  }
  zhao::check(reverse_barriers, "reverse occupancy barrier reopens", 1, reverse_barriers);
  zhao::check(post_reset_swap_accept, "held post-reset swap offer accepted after barrier", 1,
              post_reset_swap_accept);
  zhao::check(pop_swap(sim, &got) && got == tuple(44), "only post-reset swap tuple survives", 1, 1);
  sim.events(60);
  zhao::check(!d.gpu_swap_valid_o, "no pre-reset swap event reappears", 0, d.gpu_swap_valid_o);

  zhao::check(push_swap(sim, tuple(45)), "pending swap-read setup accepted", 1, 1);
  for (int i = 0; i < 300 && (d.gpu_idle_o || d.gpu_swap_valid_o); ++i) sim.event();
  zhao::check(!d.gpu_idle_o && !d.gpu_swap_valid_o,
              "pending swap read becomes visible before issue", 1, 1);
  while (!(sim.next_mask() & 1u)) sim.event();
  sim.event();
  zhao::check(!d.gpu_swap_valid_o, "swap RAM read is pending for one GPU edge", 0,
              d.gpu_swap_valid_o);
  d.vid_rst_n = 0;
  d.eval();
  sim.events(6);
  d.vid_rst_n = 1;
  d.eval();
  zhao::check(open_barrier(sim), "barrier recovers from pending swap read", 1, 1);
  sim.events(60);
  zhao::check(!d.gpu_swap_valid_o, "pending pre-reset swap RAM read cannot survive", 0,
              d.gpu_swap_valid_o);

  zhao::check(push_swap(sim, tuple(46)), "held swap-output setup accepted", 1, 1);
  for (int i = 0; i < 300 && !d.gpu_swap_valid_o; ++i) sim.event();
  zhao::check(d.gpu_swap_valid_o, "swap output became held", 1, d.gpu_swap_valid_o);
  d.gpu_rst_n = 0;
  d.eval();
  zhao::check(!d.gpu_swap_valid_o, "held swap output clears asynchronously", 0, d.gpu_swap_valid_o);
  sim.events(6);
  d.gpu_rst_n = 1;
  d.eval();
  zhao::check(open_barrier(sim), "barrier recovers from held swap output", 1, 1);
  sim.events(60);
  zhao::check(!d.gpu_swap_valid_o, "held pre-reset swap output cannot reappear", 0,
              d.gpu_swap_valid_o);

  // Change both clock ratios and phase; order remains a protocol property.
  sim.gpu_period = 14;
  sim.vid_period = 4;
  sim.next_gpu += 3;
  for (unsigned i = 50; i < 58; ++i) {
    zhao::check(push_ready(sim, tuple(i)), "rephased READY accepted", 1, 1);
    zhao::check(pop_ready(sim, &got), "rephased READY emitted", 1, 1);
    zhao::check(got == tuple(i), "rephased READY tuple exact", 1, 1);
  }
  sim.gpu_period = 4;
  sim.vid_period = 16;
  sim.next_vid += 1;
  for (unsigned i = 60; i < 68; ++i) {
    zhao::check(push_swap(sim, tuple(i)), "rephased swap accepted", 1, 1);
    zhao::check(pop_swap(sim, &got), "rephased swap emitted", 1, 1);
    zhao::check(got == tuple(i), "rephased swap tuple exact", 1, 1);
  }

  sim.events(80);
  zhao::check(d.gpu_idle_o && d.vid_idle_o, "both domains report idle after legal drain", 1, 1);
  return zhao::report_and_exit("fb_ready_cdc_v2_directed");
#endif
#endif
}
