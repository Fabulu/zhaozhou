// texture_cache_pipe_directed.cpp — does one line get fetched once, and does
// the consumer's ready still reach the accept line?
//
// ---------------------------------------------------------------------------
// THE TWO GATES
// ---------------------------------------------------------------------------
//   > Four lanes requesting one line cause one fill.
//   > No cache output-ready signal reaches TMU request acceptance in one
//     combinational path.
//
// The first is measurable and unambiguous: point all four taps inside one
// 16-byte line and count fetches. The shipped cache picks the lowest-numbered
// needing lane, fills it, re-checks, and repeats -- four fetches for one line.
// A bilinear footprint on an interior texel has all four taps in one line most
// of the time, so that is 4x the memory traffic in the COMMON case, not a
// corner.
//
// The second is the one a plausible implementation gets wrong. Splitting the
// cache into stages but leaving `acc_ready_o` reading `smp_ready_i` reproduces
// the defect exactly while looking pipelined, so the test stalls the consumer
// permanently and requires accesses to keep being accepted until local storage
// fills -- and no further.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_texture_cache_pipe.h"

#include "zhao_sim.hpp"

namespace {

constexpr int REQN = 4;

// A trivial memory: every halfword at byte address a reads back (a >> 1) + 0x100.
uint16_t mem_at(uint32_t byte_addr) {
  return static_cast<uint16_t>(((byte_addr >> 1) + 0x100) & 0xFFFF);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_cache_pipe top;

  // The fill model: 8 halfword beats per line, streamed from `fill_addr_o`.
  int fill_beats_left = 0;
  uint32_t fill_base = 0;
  int fill_beat = 0;

  auto step = [&](bool stall_mem) {
    // offer beats when a fill is outstanding
    top.fill_data_valid_i = 0;
    if (fill_beats_left > 0 && !stall_mem) {
      top.fill_data_valid_i = 1;
      top.fill_data_i = mem_at(fill_base + static_cast<uint32_t>(fill_beat) * 2u);
      ++fill_beat;
      --fill_beats_left;
    }
    top.fill_ready_i = 1;
    top.eval();
    if (top.fill_valid_o && top.fill_ready_i && fill_beats_left == 0 && fill_beat == 0) {
      fill_base = top.fill_addr_o;
      fill_beats_left = 8;
    }
    zhao::tick(top);
    if (fill_beats_left == 0 && fill_beat == 8) fill_beat = 0;
  };

  auto reset = [&]() {
    top.acc_valid_i = 0;
    top.smp_ready_i = 1;
    top.fill_data_valid_i = 0;
    top.fill_ready_i = 1;
    fill_beats_left = 0;
    fill_beat = 0;
    top.rst_n = 0;
    for (int i = 0; i < 8; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  // ---- 1: FOUR LANES, ONE LINE -> ONE FILL --------------------------------
  {
    reset();
    // All four taps inside the same 16-byte line (base 0x2000).
    top.acc_valid_i = 1;
    top.acc_en_i = 0xF;
    top.acc_addr_i[0] = 0x2000;
    top.acc_addr_i[1] = 0x2002;
    top.acc_addr_i[2] = 0x2004;
    top.acc_addr_i[3] = 0x2006;
    top.acc_src_id_i = 0x11;
    top.eval();
    zhao::tick(top);
    top.acc_valid_i = 0;

    for (int c = 0; c < 200 && !top.smp_valid_o; ++c) step(false);

    zhao::check(top.fills_o == 1, "four lanes inside ONE line cause exactly ONE line fetch", 1,
                top.fills_o);
    zhao::check(top.smp_valid_o == 1, "and the access completes", 1, top.smp_valid_o);

    // and the data is what the memory held
    bool data_ok = true;
    for (int k = 0; k < 4; ++k) {
      const uint16_t want = mem_at(0x2000 + static_cast<uint32_t>(k) * 2u);
      // smp_data_o is LANES*16 = 64 bits, so Verilator gives one QData -- not
      // a word array. Indexing it like an array compiled as a pointer subscript
      // and failed loudly, which is the good case; a 128-bit port would have
      // been an array and the same code would have silently read lane 2.
      const uint16_t got = static_cast<uint16_t>((top.smp_data_o >> (16 * k)) & 0xFFFFu);
      if (got != want) data_ok = false;
    }
    zhao::check(data_ok, "and every lane reads the byte the memory held", 1, data_ok ? 1 : 0);
    std::printf("  one line, four taps: %u fill(s), %u lane misses\n", top.fills_o,
                top.cache_misses_o);
  }

  // ---- 2: FOUR LANES, FOUR LINES -> FOUR FILLS ----------------------------
  // The multicast must not over-merge: distinct lines still cost distinct
  // fetches, or the cache would be returning one line's data for another's.
  {
    reset();
    top.acc_valid_i = 1;
    top.acc_en_i = 0xF;
    top.acc_addr_i[0] = 0x3000;
    top.acc_addr_i[1] = 0x3010;  // next line
    top.acc_addr_i[2] = 0x3020;
    top.acc_addr_i[3] = 0x3030;
    top.acc_src_id_i = 0x22;
    top.eval();
    zhao::tick(top);
    top.acc_valid_i = 0;

    for (int c = 0; c < 400 && !top.smp_valid_o; ++c) step(false);
    zhao::check(top.fills_o == 4, "four lanes in FOUR different lines cause four fetches", 4,
                top.fills_o);
    std::printf("  four lines, four taps: %u fills\n", top.fills_o);
  }

  // ---- 3: THE ACCEPT LINE IS LOCAL ---------------------------------------
  // Consumer permanently stalled. Accesses must keep being accepted until the
  // local request FIFO fills, and then stop -- not stop immediately.
  {
    reset();
    top.smp_ready_i = 0;  // the consumer never takes anything
    int accepted = 0;
    for (int c = 0; c < 40; ++c) {
      top.acc_valid_i = 1;
      top.acc_en_i = 0xF;
      for (int k = 0; k < 4; ++k) top.acc_addr_i[k] = 0x4000 + static_cast<uint32_t>(k) * 2u;
      top.acc_src_id_i = static_cast<uint16_t>(c);
      top.eval();
      if (top.acc_ready_o) ++accepted;
      step(false);
    }
    top.acc_valid_i = 0;
    zhao::check(accepted >= REQN,
                "with the consumer STALLED, accesses are still accepted into "
                "local storage",
                1, accepted >= REQN ? 1 : 0);
    std::printf("  consumer stalled: %d accesses accepted\n", accepted);
  }

  // ---- 4: random memory stalls must not change the outcome ---------------
  {
    reset();
    top.acc_valid_i = 1;
    top.acc_en_i = 0xF;
    for (int k = 0; k < 4; ++k) top.acc_addr_i[k] = 0x5000 + static_cast<uint32_t>(k) * 2u;
    top.acc_src_id_i = 0x33;
    top.eval();
    zhao::tick(top);
    top.acc_valid_i = 0;

    uint32_t s = 0x777u;
    for (int c = 0; c < 400 && !top.smp_valid_o; ++c) {
      s = s * 1664525u + 1013904223u;
      step(((s >> 16) & 3u) == 0u);  // stall the memory a quarter of the time
    }
    bool data_ok = true;
    for (int k = 0; k < 4; ++k) {
      const uint16_t want = mem_at(0x5000 + static_cast<uint32_t>(k) * 2u);
      // smp_data_o is LANES*16 = 64 bits, so Verilator gives one QData -- not
      // a word array. Indexing it like an array compiled as a pointer subscript
      // and failed loudly, which is the good case; a 128-bit port would have
      // been an array and the same code would have silently read lane 2.
      const uint16_t got = static_cast<uint16_t>((top.smp_data_o >> (16 * k)) & 0xFFFFu);
      if (got != want) data_ok = false;
    }
    zhao::check(top.smp_valid_o == 1 && data_ok,
                "random memory stalls change the timing, not the data", 1,
                (top.smp_valid_o == 1 && data_ok) ? 1 : 0);
  }

  // ---- SUSTAINED ALL-HIT THROUGHPUT: one access a clock -------------------
  //
  // The C0..C4 rebuild put three stages between a request and its answer, and
  // a miss now REWINDS the issue pointer and squashes what is in flight. Both
  // are new ways to lose throughput, and no earlier case here would notice:
  // every one of them measures whether the right bytes come back, not how
  // often they come back.
  //
  // TEXTURE.TMU's II = 2 sample rate rests on this cache taking one access a
  // clock on the hit path. So warm one line, then hammer it with the consumer
  // always ready.
  {
    reset();
    // warm the line: one access, let the fill run
    top.acc_valid_i = 1;
    top.acc_en_i = 0xF;
    top.acc_addr_i[0] = 0x5000;
    top.acc_addr_i[1] = 0x5002;
    top.acc_addr_i[2] = 0x5004;
    top.acc_addr_i[3] = 0x5006;
    top.acc_src_id_i = 0x55;
    top.eval();
    zhao::tick(top);
    top.acc_valid_i = 0;
    for (int c = 0; c < 400 && !top.smp_valid_o; ++c) step(false);
    top.smp_ready_i = 1;
    step(false);

    const uint32_t fills_warm = top.fills_o;
    const uint32_t replays_warm = top.replays_o;

    // now the same line, over and over, nothing in the way
    const int WANT = 256;
    int accepted = 0, answered = 0, clocks = 0;
    for (int c = 0; c < 4000 && answered < WANT; ++c) {
      top.acc_valid_i = (accepted < WANT);
      top.acc_en_i = 0xF;
      top.acc_addr_i[0] = 0x5000;
      top.acc_addr_i[1] = 0x5002;
      top.acc_addr_i[2] = 0x5004;
      top.acc_addr_i[3] = 0x5006;
      top.acc_src_id_i = 0x56;
      top.smp_ready_i = 1;
      top.fill_data_valid_i = 0;
      top.fill_ready_i = 1;
      top.eval();
      if (top.acc_valid_i && top.acc_ready_o) ++accepted;
      if (top.smp_valid_o && top.smp_ready_i) ++answered;
      zhao::tick(top);
      ++clocks;
    }
    top.acc_valid_i = 0;

    zhao::check(answered == WANT, "every warm access was answered", WANT, answered);
    zhao::check(top.fills_o == fills_warm,
                "a warm line is never refetched -- 256 accesses, no new fill", fills_warm,
                top.fills_o);
    zhao::check(top.replays_o == replays_warm, "and nothing is replayed, because nothing missed",
                replays_warm, top.replays_o);
    // The pipeline depth is a fixed cost paid once; after that it is one a
    // clock. Anything near 2x would mean the rebuild bought timing by
    // spending throughput, which is not a trade this block may make.
    zhao::check(clocks <= WANT + 16,
                "256 all-hit accesses complete within 256 clocks plus the "
                "pipeline depth -- the rebuild did not cost throughput",
                1, clocks <= WANT + 16 ? 1 : 0);
    std::printf("  sustained: %d answered in %d clocks (%.2f each)\n", answered, clocks,
                static_cast<double>(clocks) / answered);
  }

  return zhao::report_and_exit("texture_cache_pipe_directed");
}
