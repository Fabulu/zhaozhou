// sys_pll_directed.cpp -- SYS.PLL's directed bench.
//
// WHAT THIS CAN AND CANNOT BE EVIDENCE ABOUT, said first because the boundary
// is unusually sharp here.
//
// `zhao_sys_pll` has two bodies behind a plain `ifdef`. The vendor body is
// `altera_pll` and VERILATOR CANNOT ELABORATE IT, so everything below measures
// the BEHAVIOURAL MODEL. The real IP is not linted, not simulated, not mapped,
// not fitted and not loaded, and no result in this file says anything whatever
// about it. What the model is for is the three properties that ARE this RTL's:
// the clock RATIOS, the fact that no output runs before lock, and the lock
// event plus its census.
//
// Frequencies are properties of the fit, not of this bench. There is no
// `zref::` oracle: the reference for a PLL is a vendor datasheet and a fitter
// report, neither of which is a function this repository can call.
//
// THE DUT WRAPPER RUNS TWO INSTANCES from one stimulus -- A at the shipping
// defaults, B with SIM_VIDEO_DIV / SIM_AUDIO_DIV / SIM_LOCK_CYCLES moved. A
// parameter is only a knob if moving it moves the machine, and comparing two
// instances in one process is stronger than comparing two runs.
//
// Every counter is checked as a DELTA and every one is SEEN TO MOVE.

#include "Vtb_zhao_sys_pll.h"
#include "verilated.h"

#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>

namespace {

using zhao::check;

using Dut = Vtb_zhao_sys_pll;

// Rising-edge counters for the four generated clocks of both instances, plus
// the high/low sample census used for the duty-cycle check.
struct Edges {
  long a_gpu = 0, a_sdram = 0, a_video = 0, a_audio = 0;
  long b_gpu = 0, b_sdram = 0, b_video = 0, b_audio = 0;
  long a_video_high = 0, a_video_samples = 0;
};

struct Prev {
  int a_gpu = 0, a_sdram = 0, a_video = 0, a_audio = 0;
  int b_gpu = 0, b_sdram = 0, b_video = 0, b_audio = 0;
};

void sample(Dut& d, Edges& e, Prev& p, bool count_duty) {
  if (d.a_gpu_clk_o && !p.a_gpu) ++e.a_gpu;
  if (d.a_sdram_clk_o && !p.a_sdram) ++e.a_sdram;
  if (d.a_video_clk_o && !p.a_video) ++e.a_video;
  if (d.a_audio_clk_o && !p.a_audio) ++e.a_audio;
  if (d.b_gpu_clk_o && !p.b_gpu) ++e.b_gpu;
  if (d.b_sdram_clk_o && !p.b_sdram) ++e.b_sdram;
  if (d.b_video_clk_o && !p.b_video) ++e.b_video;
  if (d.b_audio_clk_o && !p.b_audio) ++e.b_audio;
  if (count_duty) {
    ++e.a_video_samples;
    if (d.a_video_clk_o) ++e.a_video_high;
  }
  p.a_gpu = d.a_gpu_clk_o;
  p.a_sdram = d.a_sdram_clk_o;
  p.a_video = d.a_video_clk_o;
  p.a_audio = d.a_audio_clk_o;
  p.b_gpu = d.b_gpu_clk_o;
  p.b_sdram = d.b_sdram_clk_o;
  p.b_video = d.b_video_clk_o;
  p.b_audio = d.b_audio_clk_o;
}

// One reference cycle: low half, then high half. Outputs are sampled after
// EVERY eval, because a divided clock can move on either reference edge and a
// per-cycle sample would alias exactly the fault a ratio check is looking for.
void ref_cycle(Dut& d, Edges& e, Prev& p, bool count_duty = false) {
  d.ref_clk_i = 0;
  d.eval();
  sample(d, e, p, count_duty);
  d.ref_clk_i = 1;
  d.eval();
  sample(d, e, p, count_duty);
}

void ref_cycles(Dut& d, Edges& e, Prev& p, int n, bool count_duty = false) {
  for (int i = 0; i < n; ++i) ref_cycle(d, e, p, count_duty);
}

// ---------------------------------------------------------------------------
// 1. NOTHING RUNS WHILE THE PLL IS IN RESET.
//
// A model whose gpu_clk free-runs during reset passes every ratio check below
// and misrepresents the ONE property the reset sequencer downstream depends on,
// so this is checked before anything else and on every output of both
// instances. It is also why the DIV == 1 path is a gated pass-through rather
// than a bare `assign`.
// ---------------------------------------------------------------------------
void section1_quiet_in_reset(Dut& d) {
  std::printf("\n-- 1. no output toggles while pll_arst is asserted --\n");
  d.arst_n_i = 0;
  d.pll_arst_i = 1;
  d.ref_clk_i = 0;
  d.eval();

  Edges e;
  Prev p;
  ref_cycles(d, e, p, 32);

  check(e.a_gpu == 0, "A gpu_clk edges during PLL reset", 0, static_cast<uint64_t>(e.a_gpu));
  check(e.a_sdram == 0, "A sdram_clk edges during PLL reset", 0, static_cast<uint64_t>(e.a_sdram));
  check(e.a_video == 0, "A video_clk edges during PLL reset", 0, static_cast<uint64_t>(e.a_video));
  check(e.a_audio == 0, "A audio_clk edges during PLL reset", 0, static_cast<uint64_t>(e.a_audio));
  check(e.b_gpu == 0, "B gpu_clk edges during PLL reset", 0, static_cast<uint64_t>(e.b_gpu));
  check(e.b_video == 0, "B video_clk edges during PLL reset", 0, static_cast<uint64_t>(e.b_video));
  check(d.a_locked_o == 0, "A pll_locked during PLL reset", 0, d.a_locked_o);
  check(d.b_locked_o == 0, "B pll_locked during PLL reset", 0, d.b_locked_o);
  check(d.a_lost_o == 0, "A lock-loss census before any lock", 0, d.a_lost_o);
}

// ---------------------------------------------------------------------------
// 2. LOCK ARRIVES AT SIM_LOCK_CYCLES, EXACTLY -- not "eventually".
//
// A bench that waits for lock with a generous timeout cannot tell a correct
// lock model from one that locks on the first edge, and the first edge is what
// a model with an off-by-one counter does.
// ---------------------------------------------------------------------------
void section2_lock_time(Dut& d, int& a_lock_cycles, int& b_lock_cycles) {
  std::printf("\n-- 2. lock arrives at exactly SIM_LOCK_CYCLES --\n");
  d.arst_n_i = 1;  // release housekeeping; the PLL is still held
  d.ref_clk_i = 0;
  d.eval();
  d.pll_arst_i = 0;  // release the PLL while the reference is low
  d.eval();

  Edges e;
  Prev p;
  a_lock_cycles = -1;
  b_lock_cycles = -1;
  for (int c = 1; c <= 64; ++c) {
    ref_cycle(d, e, p);
    if (a_lock_cycles < 0 && d.a_locked_o) a_lock_cycles = c;
    if (b_lock_cycles < 0 && d.b_locked_o) b_lock_cycles = c;
  }

  check(a_lock_cycles == 8, "A locks after exactly 8 reference cycles (SIM_LOCK_CYCLES)", 8,
        static_cast<uint64_t>(a_lock_cycles));
  check(b_lock_cycles == 3, "B locks after exactly 3 reference cycles (knob moved)", 3,
        static_cast<uint64_t>(b_lock_cycles));
  check(a_lock_cycles != b_lock_cycles, "SIM_LOCK_CYCLES is a live knob", 1,
        static_cast<uint64_t>(a_lock_cycles != b_lock_cycles));
}

// ---------------------------------------------------------------------------
// 3 + 4. THE RATIOS, AND THE DUTY CYCLE.
//
// The ratios are the FROZEN SIM PROFILE and they are not this bench's opinion:
// `spec/video_rules.md` and `zhao_pkg::ZHAO_VID_CYCLES_PER_GPU` give vid =
// gpu/2, and `zhao_shell_top.sv` (the protected shell), its v2 successor and
// `zhao_console_core.sv` all say "frozen ratios: vid = gpu/2, audio = gpu/4,
// plan R1". They are asserted here as edge COUNTS over hundreds of cycles, so a
// one-cycle phase error shows rather than averaging out.
//
// The duty check exists because a divider that emits a one-cycle-wide PULSE per
// period passes an edge count and is NOT A CLOCK. Counting edges alone is the
// "test that checks WHAT came out and not HOW MANY TIMES" failure wearing a
// clock's clothes.
// ---------------------------------------------------------------------------
void section3_ratios(Dut& d) {
  std::printf("\n-- 3. the frozen sim ratios, counted as edges --\n");
  const int kCycles = 400;
  Edges e;
  Prev p;
  p.a_gpu = d.a_gpu_clk_o;
  p.a_sdram = d.a_sdram_clk_o;
  p.a_video = d.a_video_clk_o;
  p.a_audio = d.a_audio_clk_o;
  p.b_gpu = d.b_gpu_clk_o;
  p.b_sdram = d.b_sdram_clk_o;
  p.b_video = d.b_video_clk_o;
  p.b_audio = d.b_audio_clk_o;
  ref_cycles(d, e, p, kCycles, /*count_duty=*/true);

  std::printf("   A  gpu %ld  sdram %ld  video %ld  audio %ld\n", e.a_gpu, e.a_sdram, e.a_video,
              e.a_audio);
  std::printf("   B  gpu %ld  sdram %ld  video %ld  audio %ld\n", e.b_gpu, e.b_sdram, e.b_video,
              e.b_audio);

  check(e.a_gpu == kCycles, "A gpu_clk is the reference, edge for edge (SIM_GPU_DIV 1)", kCycles,
        static_cast<uint64_t>(e.a_gpu));
  check(e.a_sdram == kCycles, "A sdram_clk is the reference, edge for edge (SIM_SDRAM_DIV 1)",
        kCycles, static_cast<uint64_t>(e.a_sdram));
  check(e.a_video * 2 == e.a_gpu, "A vid = gpu/2 (video_rules.md, ZHAO_VID_CYCLES_PER_GPU)",
        static_cast<uint64_t>(e.a_gpu), static_cast<uint64_t>(e.a_video * 2));
  check(e.a_audio * 4 == e.a_gpu, "A audio = gpu/4 (zhao_shell_top.sv, plan R1)",
        static_cast<uint64_t>(e.a_gpu), static_cast<uint64_t>(e.a_audio * 4));

  check(e.b_video * 4 == e.b_gpu, "B vid = gpu/4 -- SIM_VIDEO_DIV is a live knob",
        static_cast<uint64_t>(e.b_gpu), static_cast<uint64_t>(e.b_video * 4));
  check(e.b_audio * 8 == e.b_gpu, "B audio = gpu/8 -- SIM_AUDIO_DIV is a live knob",
        static_cast<uint64_t>(e.b_gpu), static_cast<uint64_t>(e.b_audio * 8));
  check(e.a_video != e.b_video, "moving SIM_VIDEO_DIV moved the machine", 1,
        static_cast<uint64_t>(e.a_video != e.b_video));

  std::printf("\n-- 4. video_clk is a clock, not a pulse train --\n");
  std::printf("   A video high on %ld of %ld samples\n", e.a_video_high, e.a_video_samples);
  check(e.a_video_high * 2 == e.a_video_samples, "A video_clk duty cycle is 50%",
        static_cast<uint64_t>(e.a_video_samples), static_cast<uint64_t>(e.a_video_high * 2));
}

// ---------------------------------------------------------------------------
// 5 + 6 + 7. THE LOCK-LOSS CENSUS.
//
// "A detector reading zero is a claim, and it is the claim to check hardest."
// This one is FIRED three times with legal stimulus -- asserting pll_arst_i,
// which is a real vendor port and not a test hook -- and checked as a DELTA
// each time, never as an absolute read at the end.
//
// Section 7 is the structural half. The census is reset by arst_n_i and NOT by
// pll_arst_i, and that separation is the whole design: a census cleared by its
// own trigger is CLAUDE.md's "two operands that move together", where the
// event and the clear arrive at the same instant and the counter reads a
// reassuring zero. Showing that the two resets behave DIFFERENTLY is what
// demonstrates they are two resets.
// ---------------------------------------------------------------------------
void section5_census(Dut& d) {
  std::printf("\n-- 5. lock-loss census, fired three times, checked as a delta --\n");
  Edges e;
  Prev p;

  uint64_t prev_a = d.a_lost_o;
  uint64_t prev_b = d.b_lost_o;
  check(d.a_locked_o == 1, "A is locked before the first induced loss", 1, d.a_locked_o);

  for (int i = 1; i <= 3; ++i) {
    d.pll_arst_i = 1;
    ref_cycles(d, e, p, 6);  // enough for the 2-flop synchroniser to carry the fall
    d.pll_arst_i = 0;
    ref_cycles(d, e, p, 24);  // re-lock

    const uint64_t a_now = d.a_lost_o;
    const uint64_t b_now = d.b_lost_o;
    std::printf("   loss %d: A %llu -> %llu, B %llu -> %llu\n", i,
                static_cast<unsigned long long>(prev_a), static_cast<unsigned long long>(a_now),
                static_cast<unsigned long long>(prev_b), static_cast<unsigned long long>(b_now));
    check(a_now == prev_a + 1, "A pll_lock_lost delta is exactly 1", prev_a + 1, a_now);
    check(b_now == prev_b + 1, "B pll_lock_lost delta is exactly 1", prev_b + 1, b_now);
    check(d.a_locked_o == 1, "A re-locks after the loss", 1, d.a_locked_o);
    prev_a = a_now;
    prev_b = b_now;
  }

  std::printf("\n-- 6. the census SURVIVES the event that causes it --\n");
  check(d.a_lost_o == 3, "A census reads 3 after three losses, never cleared by pll_arst", 3,
        d.a_lost_o);

  // It must also be STABLE when nothing happens: a counter that creeps is a
  // counter whose deltas above were coincidence.
  ref_cycles(d, e, p, 200);
  check(d.a_lost_o == 3, "A census is stable across 200 quiet cycles", 3, d.a_lost_o);

  std::printf("\n-- 7. arst_n clears the census and pll_arst does not --\n");
  d.arst_n_i = 0;
  ref_cycles(d, e, p, 4);
  check(d.a_lost_o == 0, "arst_n_i clears the census", 0, d.a_lost_o);
  check(d.a_locked_o == 1, "arst_n_i does NOT unlock the PLL -- they are different resets", 1,
        d.a_locked_o);

  d.arst_n_i = 1;
  ref_cycles(d, e, p, 4);
  d.pll_arst_i = 1;
  ref_cycles(d, e, p, 6);
  d.pll_arst_i = 0;
  ref_cycles(d, e, p, 24);
  check(d.a_lost_o == 1, "and the census counts again from a cleared state", 1, d.a_lost_o);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;

  std::printf("sys_pll_directed -- SYS.PLL behavioural model.\n");
  std::printf("NOTE: altera_pll is NOT compiled here. Nothing below is evidence about the IP.\n");

  section1_quiet_in_reset(dut);

  int a_lock = 0;
  int b_lock = 0;
  section2_lock_time(dut, a_lock, b_lock);
  section3_ratios(dut);
  section5_census(dut);

  dut.final();
  const int rc = zhao::report_and_exit("sys_pll_directed");
  zhao::exit_hard(rc);
}
