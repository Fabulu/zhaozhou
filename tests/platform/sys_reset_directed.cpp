// sys_reset_directed.cpp -- SYS.RESET's directed bench.
//
// The block's whole content is TIMING against five clocks, so there is no
// `zref::` oracle and this bench IS the specification, expressed as assertions.
// Building a C++ model of "rst_n goes low when either input goes low" would be
// a second implementation of one line of RTL.
//
// THE FIVE CLOCKS run at the frozen sim profile with coincident posedges:
// ref = gpu = sdram, video = ref/2, audio = ref/4 (plan R1, and the same
// ratios `zhao_shell_top.sv` and `zhao_console_core.sv` declare). The stagger
// counter lives in ref, the release synchronisers live in each domain, so the
// RELATIVE rates are the thing under test and they are driven explicitly.
//
// THE DUT WRAPPER RUNS TWO INSTANCES from one stimulus -- A at the shipping
// stagger (0/3/6/9) and B with ALL FOUR STEPS AT ZERO. The contract calls the
// order "a judgement dressed as four constants"; B is what keeps that honest,
// because an order that cannot be changed is a law nobody declared.
//
// Every counter is checked as a DELTA and is SEEN TO MOVE.

#include "Vtb_zhao_sys_reset.h"
#include "verilated.h"

#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>

namespace {

using zhao::check;

using Dut = Vtb_zhao_sys_reset;

// Four sim ticks per reference cycle: ref/gpu/sdram period 4, video 8,
// audio 16, every one rising at t == 0 modulo its own period.
constexpr long kTicksPerRef = 4;

long g_t = 0;

void drive_clocks(Dut& d) {
  const int r = ((g_t % 4) < 2) ? 1 : 0;
  d.ref_clk_i = r;
  d.gpu_clk_i = r;
  d.sdram_clk_i = r;
  d.video_clk_i = ((g_t % 8) < 4) ? 1 : 0;
  d.audio_clk_i = ((g_t % 16) < 8) ? 1 : 0;
}

void tick(Dut& d) {
  drive_clocks(d);
  d.eval();
  ++g_t;
}

void ref_cycles(Dut& d, int n) {
  for (int i = 0; i < n * kTicksPerRef; ++i) tick(d);
}

bool all_low(Dut& d) {
  return !d.a_rst_n_gpu_o && !d.a_rst_n_sdram_o && !d.a_rst_n_video_o && !d.a_rst_n_audio_o;
}

bool all_high(Dut& d) {
  return d.a_rst_n_gpu_o && d.a_rst_n_sdram_o && d.a_rst_n_video_o && d.a_rst_n_audio_o;
}

// The ref-cycle index at which each output first rises, measured from the
// moment arst_n rises. -1 means "never within the window".
struct Release {
  int sdram = -1, gpu = -1, video = -1, audio = -1, seq_done = -1;
};

// ---------------------------------------------------------------------------
// 1. POWER-ON, WITH NO CLOCK EDGE AT ALL.
//
// Both inputs low, one eval, nothing toggled. If any output is high here the
// network is not asynchronously asserted and every later test is measuring
// something else.
// ---------------------------------------------------------------------------
void section1_power_on(Dut& d) {
  std::printf("\n-- 1. power-on: every rst_n low with no clock edge --\n");
  d.hard_reset_n_i = 0;
  d.pll_locked_i = 0;
  drive_clocks(d);
  d.eval();

  check(all_low(d), "A: all four rst_n low at power-on", 0, static_cast<uint64_t>(!all_low(d)));
  check(!d.b_rst_n_gpu_o && !d.b_rst_n_audio_o, "B: all four rst_n low at power-on", 0,
        static_cast<uint64_t>(d.b_rst_n_gpu_o || d.b_rst_n_audio_o));
  check(d.a_seq_done_o == 0, "A: seq_done low at power-on", 0, d.a_seq_done_o);
  check(d.a_assertions_o == 0, "A: census starts at zero", 0, d.a_assertions_o);
}

// ---------------------------------------------------------------------------
// 2. NO RELEASE WITHOUT LOCK.
//
// design/blocks.yml's whole reason for this block is that it WAITS for
// pll_locked. A bench that never tested the waiting would be testing the easy
// half: 200 reference cycles of every clock, hard reset released, lock still
// low, and nothing may come out of reset.
// ---------------------------------------------------------------------------
void section2_no_release_without_lock(Dut& d) {
  std::printf("\n-- 2. hard reset released but PLL unlocked: nothing releases --\n");
  d.hard_reset_n_i = 1;
  d.pll_locked_i = 0;
  ref_cycles(d, 200);

  check(all_low(d), "A: 200 cycles without lock, still fully asserted", 0,
        static_cast<uint64_t>(!all_low(d)));
  check(d.a_seq_done_o == 0, "A: the stagger counter never started", 0, d.a_seq_done_o);
  check(d.b_rst_n_sdram_o == 0, "B: 200 cycles without lock, still asserted", 0, d.b_rst_n_sdram_o);
}

// ---------------------------------------------------------------------------
// 3 + 4. THE STAGGER, ITS ORDER, AND THE BOUNDED-LATENCY LAW.
//
// The ledger says `latency: variable_bounded:16`. That 16 is the SEQUENCER's
// bound in reference cycles and seq_done is the observable for it; each domain
// then adds SYNC_STAGES of its OWN clock, and the audio domain's clock is a
// quarter of the reference, so the total window is wider than 16 and the
// contract says so rather than quoting the flattering half.
// ---------------------------------------------------------------------------
void section3_stagger(Dut& d, Release& a_rel, Release& b_rel) {
  std::printf("\n-- 3. staggered release, in the declared order --\n");
  d.pll_locked_i = 1;

  // One pass collects both instances: identical stimulus, two observers.
  Release a;
  Release b;
  for (int c = 1; c <= 40; ++c) {
    ref_cycles(d, 1);
    if (a.sdram < 0 && d.a_rst_n_sdram_o) a.sdram = c;
    if (a.gpu < 0 && d.a_rst_n_gpu_o) a.gpu = c;
    if (a.video < 0 && d.a_rst_n_video_o) a.video = c;
    if (a.audio < 0 && d.a_rst_n_audio_o) a.audio = c;
    if (a.seq_done < 0 && d.a_seq_done_o) a.seq_done = c;
    if (b.sdram < 0 && d.b_rst_n_sdram_o) b.sdram = c;
    if (b.gpu < 0 && d.b_rst_n_gpu_o) b.gpu = c;
    if (b.video < 0 && d.b_rst_n_video_o) b.video = c;
    if (b.audio < 0 && d.b_rst_n_audio_o) b.audio = c;
    if (b.seq_done < 0 && d.b_seq_done_o) b.seq_done = c;
  }

  std::printf("   A release at ref cycle: sdram %d  gpu %d  video %d  audio %d  seq_done %d\n",
              a.sdram, a.gpu, a.video, a.audio, a.seq_done);
  std::printf("   B release at ref cycle: sdram %d  gpu %d  video %d  audio %d  seq_done %d\n",
              b.sdram, b.gpu, b.video, b.audio, b.seq_done);

  check(a.sdram > 0, "A: sdram released", 1, static_cast<uint64_t>(a.sdram > 0));
  check(a.gpu > 0, "A: gpu released", 1, static_cast<uint64_t>(a.gpu > 0));
  check(a.video > 0, "A: video released", 1, static_cast<uint64_t>(a.video > 0));
  check(a.audio > 0, "A: audio released", 1, static_cast<uint64_t>(a.audio > 0));
  check(a.sdram < a.gpu, "A: sdram releases before gpu (MEM.SDRAM's init runs first)", 1,
        static_cast<uint64_t>(a.sdram < a.gpu));
  check(a.gpu < a.video, "A: gpu releases before video", 1, static_cast<uint64_t>(a.gpu < a.video));
  check(a.video < a.audio, "A: video releases before audio (AUDIO.FIFO last)", 1,
        static_cast<uint64_t>(a.video < a.audio));

  std::printf("\n-- 4. the bounded-latency law (ledger: variable_bounded:16) --\n");
  check(a.seq_done > 0 && a.seq_done <= 16,
        "A: seq_done rises within RELEASE_SPAN = 16 reference cycles", 1,
        static_cast<uint64_t>(a.seq_done > 0 && a.seq_done <= 16));
  check(all_high(d), "A: every domain is released by the end of the window", 1,
        static_cast<uint64_t>(all_high(d)));

  std::printf("\n-- 9. the order is a KNOB, not a hidden law --\n");
  check(b.sdram == b.gpu, "B: with all steps 0, sdram and gpu share a clock and release together",
        static_cast<uint64_t>(b.sdram), static_cast<uint64_t>(b.gpu));
  check(b.audio < a.audio, "B: zeroing the steps releases audio EARLIER than the shipping stagger",
        1, static_cast<uint64_t>(b.audio < a.audio));
  check(b.sdram <= b.video && b.video <= b.audio,
        "B: the remaining spread is clock rate alone, still monotonic", 1,
        static_cast<uint64_t>(b.sdram <= b.video && b.video <= b.audio));

  a_rel = a;
  b_rel = b;
}

// ---------------------------------------------------------------------------
// 5. ASSERTION IS ASYNCHRONOUS -- checked with EVERY CLOCK HELD STATIC.
//
// This is the one test a synchronous-assert implementation fails, and it passes
// tests 1 through 4 and 6 through 9. Without it, "asynchronous" is a word in a
// header.
// ---------------------------------------------------------------------------
void section5_async_assert(Dut& d) {
  std::printf("\n-- 5. asynchronous assertion, with the clocks frozen --\n");
  check(all_high(d), "precondition: fully released before the drop", 1,
        static_cast<uint64_t>(all_high(d)));

  const long frozen_at = g_t;
  d.pll_locked_i = 0;
  d.eval();  // NO tick: not one clock edge anywhere

  check(g_t == frozen_at, "no simulated time passed", static_cast<uint64_t>(frozen_at),
        static_cast<uint64_t>(g_t));
  check(all_low(d), "A: every rst_n fell with no clock edge", 1, static_cast<uint64_t>(all_low(d)));
  check(d.a_seq_done_o == 0, "A: the stagger counter was asynchronously cleared too", 0,
        d.a_seq_done_o);
  check(!d.b_rst_n_gpu_o && !d.b_rst_n_audio_o, "B: same, with the other parameterisation", 1,
        static_cast<uint64_t>(!d.b_rst_n_gpu_o && !d.b_rst_n_audio_o));
}

// ---------------------------------------------------------------------------
// 6 + 7 + 8. THE CENSUS, AND THE RE-ARM.
//
// The census is reset by hard_reset_n_i and NEVER by pll_locked_i, so it
// survives the event it counts. Section 7 demonstrates the asymmetry rather
// than asserting it: a census cleared by its own trigger is CLAUDE.md's
// "detector wired to two operands that move together", and the only way to
// know which kind you have is to move each reset separately and watch.
// ---------------------------------------------------------------------------
void section6_census(Dut& d) {
  std::printf("\n-- 6. reset_assertions, fired three times, checked as a delta --\n");

  // The drop in section 5 was the FIRST induced loss; let the synchroniser
  // carry it and read the delta.
  uint64_t prev = 0;
  for (int i = 1; i <= 3; ++i) {
    if (i > 1) {
      d.pll_locked_i = 0;
      d.eval();
    }
    ref_cycles(d, 8);  // the 2-flop synchroniser carries the fall
    const uint64_t now = d.a_assertions_o;
    std::printf("   loss %d: A census %llu -> %llu, B %llu\n", i,
                static_cast<unsigned long long>(prev), static_cast<unsigned long long>(now),
                static_cast<unsigned long long>(d.b_assertions_o));
    check(now == prev + 1, "A reset_assertions delta is exactly 1", prev + 1, now);
    check(d.b_assertions_o == now, "B counts the same events", now, d.b_assertions_o);
    prev = now;

    // Re-release so the next drop is a real released -> asserted transition.
    d.pll_locked_i = 1;
    ref_cycles(d, 40);
    if (i == 1) {
      std::printf("\n-- 8. the sequencer re-arms: the stagger re-runs after a loss --\n");
      check(all_high(d), "A: fully released again after a lock loss", 1,
            static_cast<uint64_t>(all_high(d)));
      check(d.a_seq_done_o == 1, "A: seq_done rose again -- not a one-shot", 1, d.a_seq_done_o);
    }
  }

  check(d.a_assertions_o == 3, "A census reads 3 after three losses", 3, d.a_assertions_o);
  ref_cycles(d, 200);
  check(d.a_assertions_o == 3, "A census is stable across 200 quiet cycles", 3, d.a_assertions_o);

  std::printf("\n-- 7. hard_reset_n clears the census; pll_locked never does --\n");
  d.hard_reset_n_i = 0;
  ref_cycles(d, 4);
  check(d.a_assertions_o == 0, "hard_reset_n_i clears the census", 0, d.a_assertions_o);
  check(all_low(d), "and asserts the network", 1, static_cast<uint64_t>(all_low(d)));

  d.hard_reset_n_i = 1;
  ref_cycles(d, 40);
  check(all_high(d), "released again after the hard reset", 1, static_cast<uint64_t>(all_high(d)));
  check(d.a_assertions_o == 0,
        "the census did NOT count the hard reset -- it is scoped to lock losses", 0,
        d.a_assertions_o);

  d.pll_locked_i = 0;
  ref_cycles(d, 8);
  check(d.a_assertions_o == 1, "and it counts again from the cleared state", 1, d.a_assertions_o);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;

  std::printf("sys_reset_directed -- SYS.RESET, five clocks, two parameterisations.\n");

  section1_power_on(dut);
  section2_no_release_without_lock(dut);

  Release a_rel;
  Release b_rel;
  section3_stagger(dut, a_rel, b_rel);
  section5_async_assert(dut);
  section6_census(dut);

  dut.final();
  const int rc = zhao::report_and_exit("sys_reset_directed");
  zhao::exit_hard(rc);
}
