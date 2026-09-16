// qsq_bytemul_directed.cpp -- zhao_qsq_bytemul against the whole input domain.
//
// THE DOMAIN IS 65,536 PAIRS AND THE TEST DRIVES ALL OF THEM. There is no
// sampling argument to make and no coverage claim to defend: `a_i` and `b_i`
// are eight bits each, so every input this block can ever be handed is driven
// through the RTL and compared against `a * b` computed in C. A quarter-square
// multiplier is exactly the kind of block where a spot check is worthless --
// the identity `a*b = Q[a+b] - Q[|a-b|]` is exact for a reason that depends on
// the PARITY of `a+b`, so a defect would show on half the domain and a test
// that happened to sample the other half would report a clean pass.
//
// WHAT THIS ADDS OVER THE CHECK ALREADY IN terrain_shade_rtl_directed. That one
// checks the IDENTITY, in C, before driving any packet -- floor((a+b)^2/4) -
// floor((a-b)^2/4) == a*b. True and useful, and it says nothing whatever about
// the RTL: not that the recurrence fills the table correctly, not that the
// addresses are formed right, not that the two read ports line up, not that the
// subtraction has the width it needs. This test drives the hardware.
//
// THREE THINGS BEYOND THE PRODUCTS:
//
//   * THE FILL TAKES EXACTLY 512 CYCLES and `table_ready_o` rises once. A table
//     that is one word short is a wrong product for exactly one pair, which is
//     the kind of defect an exhaustive sweep catches and a boundary check does
//     not -- so both are here, and they check different things.
//   * `en_i` LOW HOLDS THE OUTPUT. The primitive's header promises a client
//     with backpressure can gate it; that promise is worth what it is tested
//     at. Issue a pair, drop `en_i` for several cycles with the inputs
//     changing underneath, and the product must not move.
//   * THE TABLE IS NEVER REWRITTEN after `table_ready_o`. Driven implicitly:
//     the exhaustive sweep runs long after the fill and every product is still
//     right.
//
// NO MUTANT IS NEEDED. Nothing here guards an unreachable state: every claim is
// reachable with port stimulus, which is the test the repository's mutant rule
// applies before reaching for a committed copy.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_qsq_bytemul.h"

#include "zhao_sim.hpp"

using zhao::check;

namespace {

void tick(Vzhao_qsq_bytemul& dut) {
  dut.clk = 0;
  dut.eval();
  dut.clk = 1;
  dut.eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_qsq_bytemul dut;

  dut.clk = 0;
  dut.rst_n = 0;
  dut.en_i = 0;
  dut.a_i = 0;
  dut.b_i = 0;
  dut.eval();
  tick(dut);
  tick(dut);
  dut.rst_n = 1;
  dut.eval();

  // ---- the fill is exactly 512 cycles, and ready rises exactly once -------
  {
    int cycles_low = 0;
    int rises = 0;
    int last = dut.table_ready_o;
    for (int i = 0; i < 700; ++i) {
      if (!dut.table_ready_o) ++cycles_low;
      tick(dut);
      if (dut.table_ready_o && !last) ++rises;
      last = dut.table_ready_o;
    }
    check(cycles_low == 512, "table_ready_o low for exactly 512 cycles", 512, cycles_low);
    check(rises == 1, "table_ready_o rises exactly once", 1, rises);
    check(dut.table_ready_o == 1, "table_ready_o is high after the fill", 1, dut.table_ready_o);
  }

  // ---- every one of the 65,536 pairs -------------------------------------
  {
    int mismatches = 0;
    int first_a = -1, first_b = -1, first_got = -1, first_want = -1;
    // THE OFFSET, because the first version of this loop got it backwards and
    // reported 65,279 failures against RTL that was correct. `tick` ends with
    // the posedge already evaluated, so the edge it just ran captured the
    // addresses driven BEFORE it -- the pair set immediately above. p_o after
    // `tick` is therefore THIS pair's product, not the previous one's. The
    // block's one-cycle latency is real; it sits between driving the inputs and
    // the edge, not between the edge and the read.
    //
    // Worth the paragraph because the failure looked exactly like a broken
    // multiplier: "1 * 0 = 0, got 1" is a plausible off-by-one in an adder. The
    // thing that separated a wrong test from wrong RTL was that
    // terrain_shade_rtl_directed -- the real client, driving the same primitive
    // through 6,656 checks against the compiled law -- was green at the same
    // moment. A new test disagreeing with an established one is not
    // automatically the one telling the truth.
    for (int a = 0; a < 256; ++a) {
      for (int b = 0; b < 256; ++b) {
        dut.en_i = 1;
        dut.a_i = static_cast<uint8_t>(a);
        dut.b_i = static_cast<uint8_t>(b);
        tick(dut);
        const int want = a * b;
        const int got = static_cast<int>(dut.p_o);
        if (got != want) {
          if (mismatches == 0) {
            first_a = a;
            first_b = b;
            first_got = got;
            first_want = want;
          }
          ++mismatches;
        }
      }
    }
    if (mismatches != 0) {
      std::printf("[qsq] first mismatch: %d * %d = %d, got %d\n", first_a, first_b, first_want,
                  first_got);
    }
    check(mismatches == 0, "all 65,536 byte products are exact", 0, mismatches);
  }

  // ---- en_i low holds the output -----------------------------------------
  {
    dut.en_i = 1;
    dut.a_i = 200;
    dut.b_i = 173;
    tick(dut);
    const int held = static_cast<int>(dut.p_o);
    check(held == 200 * 173, "the held product was captured", 200 * 173, held);
    dut.en_i = 0;
    for (int i = 0; i < 8; ++i) {
      dut.a_i = static_cast<uint8_t>(3 + i * 29);
      dut.b_i = static_cast<uint8_t>(251 - i * 17);
      tick(dut);
      check(static_cast<int>(dut.p_o) == held, "en_i low holds p_o", held,
            static_cast<int>(dut.p_o));
    }
    dut.en_i = 1;
    dut.a_i = 7;
    dut.b_i = 9;
    tick(dut);
    check(static_cast<int>(dut.p_o) == 63, "en_i high resumes", 63, static_cast<int>(dut.p_o));
  }

  // ---- the corners, named -------------------------------------------------
  {
    struct Corner {
      int a, b;
      const char* why;
    } const corners[] = {
        {0, 0, "0*0: both addresses are Q[0]"},
        {255, 255, "255*255 = 65,025: the widest product and Q[510], the last word"},
        {255, 0, "255*0: |a-b| = a, the largest difference"},
        {1, 1, "1*1: a+b = 2 and |a-b| = 0, the smallest odd-parity pair"},
        {128, 127, "128*127: a+b odd, so BOTH floors drop a quarter and must cancel"},
        {129, 127, "129*127: a+b even, so neither floor drops anything"},
    };
    for (const Corner& c : corners) {
      dut.en_i = 1;
      dut.a_i = static_cast<uint8_t>(c.a);
      dut.b_i = static_cast<uint8_t>(c.b);
      tick(dut);
      check(static_cast<int>(dut.p_o) == c.a * c.b, c.why, c.a * c.b, static_cast<int>(dut.p_o));
    }
  }

  dut.final();
  zhao::exit_hard(zhao::report_and_exit("qsq_bytemul_directed"));
}
