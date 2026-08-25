// field_sin_directed.cpp — OP_SIN and OP_COS, RTL against `zref::fx_sin`/`fx_cos`.
//
// EXHAUSTIVE, because the input space is small enough to simply try. An
// `angle16` is a u16 of turns: 65,536 angles, times two opcodes. There is no
// sampling argument to make and no random lane worth writing — every input is
// tested, so the differential is a proof rather than evidence.
//
// The table is checked separately and entry by entry, because it is GENERATED
// into `zhao_field_sin_rom.sv` and a generated file goes stale as easily as a
// copied one: the generator is not run by the build.
//
// FOUR LAWS, and the identities that make them checkable without the RTL:
//
//   * sin(-a) == -sin(a)
//   * sin(0x8000 - a) == sin(a)
//   * sin(0x4000) == 0x10000 EXACTLY -- the full 1.0, not one ulp below
//   * cos(a) == sin(a + 0x4000), with the add WRAPPING in sixteen bits
//
// The reference asserts the first three itself; they are re-asserted here
// against the RTL, because an implementation can match the oracle on the angles
// a test happens to pick and still break an identity that holds for all of them.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "verilated.h"

#include "Vzhao_field_sin.h"

#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_trig.hpp"
#include "zref/generated/zref_tables.hpp"

namespace {

using zhao::check;

// WAVE 8: the result is REGISTERED, so it lands one clock after the angle is
// presented rather than in the same delta cycle. `eval()` alone would return
// the answer to the PREVIOUS question -- the exact symptom the register file
// produced in field_seq_directed.cpp when it became block memory.
int32_t run(Vzhao_field_sin& dut, uint16_t angle, bool is_cos) {
  dut.angle_i = angle;
  dut.is_cos_i = is_cos ? 1 : 0;
  zhao::tick(dut);
  zhao::tick(dut);  // wave 10: the table read is registered too, so latency 2
  return static_cast<int32_t>(dut.result_o);
}

// BACK-TO-BACK ISSUE, which is the property the pipeline must not lose: ROT
// reads cosine and sine on CONSECUTIVE clocks off one held angle. A block that
// answered correctly only when its input stood still for two cycles would pass
// every other case in this file and still break ROT.
void run_pair(Vzhao_field_sin& dut, uint16_t a0, bool cos0, uint16_t a1, bool cos1, int32_t* r0,
              int32_t* r1) {
  // LATENCY 2, INITIATION INTERVAL 1. a1 is issued on the clock AFTER a0, so the
  // two requests OVERLAP in the pipeline. That overlap is the whole point: it is
  // the only arrangement that catches a decode which failed to travel with its
  // own table read, because such a block answers correctly whenever the input
  // stands still.
  dut.angle_i = a0;
  dut.is_cos_i = cos0 ? 1 : 0;
  zhao::tick(dut);
  dut.angle_i = a1;  // issued while a0 is still in flight
  dut.is_cos_i = cos1 ? 1 : 0;
  zhao::tick(dut);
  *r0 = static_cast<int32_t>(dut.result_o);  // a0 lands here
  zhao::tick(dut);
  *r1 = static_cast<int32_t>(dut.result_o);  // a1 one clock behind it
}

int32_t oracle(uint16_t angle, bool is_cos) {
  return is_cos ? zref::fx_cos(zref::angle16{angle}).raw : zref::fx_sin(zref::angle16{angle}).raw;
}

}  // namespace

int main() {
  Vzhao_field_sin dut;

  // ---- 1. THE ROM IS THE TABLE --------------------------------------------
  // Every index whose entry the interpolation can read. Driven through the only
  // port the RTL has: the angle whose quarter-wave index is `i` and whose
  // sub-tick is zero, so the result IS the table entry.
  {
    uint64_t bad = 0;
    int first = -1;
    for (int i = 0; i <= 256; ++i) {
      const uint16_t angle = static_cast<uint16_t>(i * 64);  // t == 0
      const int32_t got = run(dut, angle, false);
      const int32_t want = static_cast<int32_t>(zref::gen::SIN_Q16[i]);
      if (got != want) {
        if (first < 0) first = i;
        ++bad;
      }
    }
    check(bad == 0, "the generated ROM is SIN_Q16, all 257 entries", 0, bad);
    if (bad) std::printf("  first divergent index: %d\n", first);

    check(zref::gen::SIN_Q16[0] == 0u, "the reference table starts at zero", 0u,
          zref::gen::SIN_Q16[0]);
    check(zref::gen::SIN_Q16[256] == 0x10000u, "and ends at exactly 1.0", 0x10000u,
          zref::gen::SIN_Q16[256]);
  }

  // ---- 2. EXHAUSTIVE: every angle, both opcodes ---------------------------
  {
    uint64_t bad_sin = 0, bad_cos = 0;
    int32_t first_angle = -1, first_want = 0, first_got = 0;
    for (uint32_t a = 0; a < 65536; ++a) {
      const uint16_t angle = static_cast<uint16_t>(a);

      const int32_t gs = run(dut, angle, false);
      const int32_t ws = oracle(angle, false);
      if (gs != ws) {
        if (first_angle < 0) {
          first_angle = static_cast<int32_t>(a);
          first_want = ws;
          first_got = gs;
        }
        ++bad_sin;
      }

      const int32_t gc = run(dut, angle, true);
      const int32_t wc = oracle(angle, true);
      if (gc != wc) {
        if (first_angle < 0) {
          first_angle = static_cast<int32_t>(a);
          first_want = wc;
          first_got = gc;
        }
        ++bad_cos;
      }
    }
    check(bad_sin == 0, "OP_SIN matches the oracle for ALL 65,536 angles", 0, bad_sin);
    check(bad_cos == 0, "OP_COS matches the oracle for ALL 65,536 angles", 0, bad_cos);
    if (bad_sin || bad_cos) {
      std::printf("  first divergence at angle %d: want %d got %d\n", first_angle, first_want,
                  first_got);
    }
  }

  // ---- 3. THE ENDPOINT ----------------------------------------------------
  // i == 256 is reached by sin(0x4000), sin(0xC000), cos(0) and cos(0x8000).
  //
  // What this section CANNOT show, recorded so nobody hunts for the missing
  // case: removing the RTL's i == 256 value guard passes even this exhaustive
  // test. `t` is zero at the endpoint, so the interpolation contributes nothing
  // whatever the slope was -- an equivalent mutant. The reference's guard is
  // C++ MEMORY safety (it would read one past a 257-entry array); the RTL's
  // load-bearing protection is the index CLAMP that keeps the ROM in range, not
  // the value guard beside it.
  {
    check(run(dut, 0x4000, false) == 0x10000, "sin(quarter turn) is EXACTLY 1.0", 0x10000u,
          static_cast<uint32_t>(run(dut, 0x4000, false)));
    check(run(dut, 0xC000, false) == -0x10000, "sin(three quarters) is exactly -1.0",
          static_cast<uint32_t>(-0x10000), static_cast<uint32_t>(run(dut, 0xC000, false)));
    check(run(dut, 0x0000, true) == 0x10000, "cos(0) is exactly 1.0 -- the same endpoint", 0x10000u,
          static_cast<uint32_t>(run(dut, 0x0000, true)));
    check(run(dut, 0x8000, true) == -0x10000, "cos(half turn) is exactly -1.0",
          static_cast<uint32_t>(-0x10000), static_cast<uint32_t>(run(dut, 0x8000, true)));
    // And one tick either side, so a guard that fired too eagerly would show.
    check(run(dut, 0x3FFF, false) == oracle(0x3FFF, false), "one tick before the endpoint",
          static_cast<uint32_t>(oracle(0x3FFF, false)),
          static_cast<uint32_t>(run(dut, 0x3FFF, false)));
    check(run(dut, 0x4001, false) == oracle(0x4001, false), "and one tick after",
          static_cast<uint32_t>(oracle(0x4001, false)),
          static_cast<uint32_t>(run(dut, 0x4001, false)));
  }

  // ---- 4. the exact anchors --------------------------------------------------
  {
    check(run(dut, 0, false) == 0, "sin(0) is zero", 0, static_cast<uint32_t>(run(dut, 0, false)));
    check(run(dut, 0x8000, false) == 0, "sin(half turn) is zero", 0,
          static_cast<uint32_t>(run(dut, 0x8000, false)));
    check(run(dut, 0x4000, true) == 0, "cos(quarter turn) is zero", 0,
          static_cast<uint32_t>(run(dut, 0x4000, true)));
    check(run(dut, 0xC000, true) == 0, "cos(three quarters) is zero", 0,
          static_cast<uint32_t>(run(dut, 0xC000, true)));
  }

  // ---- 5. THE IDENTITIES, over every angle --------------------------------
  // An implementation can match the oracle on the angles a test picks and still
  // break an identity that holds for all of them, so these are swept whole.
  {
    uint64_t bad_odd = 0, bad_mirror = 0, bad_cos = 0;
    for (uint32_t a = 0; a < 65536; ++a) {
      const uint16_t angle = static_cast<uint16_t>(a);
      const int32_t s = run(dut, angle, false);

      // sin(-a) == -sin(a)
      const int32_t sneg = run(dut, static_cast<uint16_t>(-static_cast<int>(a)), false);
      if (sneg != -s) ++bad_odd;

      // sin(0x8000 - a) == sin(a)
      const int32_t smir = run(dut, static_cast<uint16_t>(0x8000u - a), false);
      if (smir != s) ++bad_mirror;

      // cos(a) == sin(a + 0x4000), with the add wrapping
      const int32_t c = run(dut, angle, true);
      const int32_t sshift = run(dut, static_cast<uint16_t>(a + 0x4000u), false);
      if (c != sshift) ++bad_cos;
    }
    check(bad_odd == 0, "sin is ODD for every angle: sin(-a) == -sin(a)", 0, bad_odd);
    check(bad_mirror == 0, "sin mirrors about the half turn for every angle", 0, bad_mirror);
    check(bad_cos == 0, "cos(a) == sin(a + quarter) for every angle, the shift wrapping", 0,
          bad_cos);
  }

  // ---- 6. the quarter wave really does interpolate ------------------------
  // Between two table entries the result must MOVE, and monotonically across the
  // rising quarter. A block that ignored the sub-tick would return the same
  // value for all 64 angles in a step -- and would still pass an anchors-only
  // test.
  {
    uint64_t stuck = 0, non_monotonic = 0;
    int32_t prev = run(dut, 0, false);
    for (uint32_t a = 1; a <= 0x4000; ++a) {
      const int32_t s = run(dut, static_cast<uint16_t>(a), false);
      if (s < prev) ++non_monotonic;
      prev = s;
    }
    check(non_monotonic == 0, "the rising quarter never goes backwards", 0, non_monotonic);
    // Within one table step, the 64 sub-ticks must not all be equal.
    for (int step = 0; step < 8; ++step) {
      const uint32_t b = static_cast<uint32_t>(step) * 64 + 1024;
      bool moved = false;
      const int32_t at0 = run(dut, static_cast<uint16_t>(b), false);
      for (uint32_t k = 1; k < 64; ++k) {
        if (run(dut, static_cast<uint16_t>(b + k), false) != at0) {
          moved = true;
          break;
        }
      }
      if (!moved) ++stuck;
    }
    check(stuck == 0, "the sub-tick is really interpolated, not ignored", 0, stuck);
  }

  dut.final();
  // ---- back-to-back issue, the property ROT depends on ---------------------
  // ROT presents cos on one clock and sin on the next off a HELD angle, then
  // captures the two answers on consecutive clocks. Sweeping the pair across
  // the circle catches a pipeline that only settles when its input is static,
  // and catches a decode that failed to travel with its result.
  {
    uint64_t bad = 0;
    int32_t first_a = -1;
    for (uint32_t a = 0; a < 65536; a += 7) {
      const uint16_t angle = static_cast<uint16_t>(a);
      int32_t gc = 0, gs = 0;
      run_pair(dut, angle, true, angle, false, &gc, &gs);
      if (gc != oracle(angle, true) || gs != oracle(angle, false)) {
        if (first_a < 0) first_a = static_cast<int32_t>(a);
        ++bad;
      }
    }
    check(bad == 0, "cos then sin on CONSECUTIVE clocks, swept across the circle", 0, bad);
    if (bad) std::printf("  first divergent angle: 0x%04X\n", first_a);

    // And the harder direction: two DIFFERENT angles back to back, so a stale
    // decode cannot be masked by the two requests sharing one angle.
    uint64_t bad2 = 0;
    for (uint32_t a = 0; a < 65536; a += 101) {
      const uint16_t x = static_cast<uint16_t>(a);
      const uint16_t y = static_cast<uint16_t>(a * 31u + 12345u);
      int32_t r0 = 0, r1 = 0;
      run_pair(dut, x, false, y, false, &r0, &r1);
      if (r0 != oracle(x, false) || r1 != oracle(y, false)) ++bad2;
    }
    check(bad2 == 0, "two DIFFERENT angles back to back, so a stale decode cannot hide", 0, bad2);
  }

  return zhao::report_and_exit("field_sin_directed");
}
