// part_clipfeed_emitstate_mutant_control.cpp -- INVERTED POLARITY. This passes
// when the emitted fragment-state word DISAGREES with the accepted record.
//
// WHY IT EXISTS
// -------------
// `zhao_part_clipfeed` carries `zhao_part_expand`'s pass-7 law -- Z_TEST_EN and
// Z_WRITE_DIS -- to GEOM.CLIPDOOR, closing core entry I51's last open clause.
// In this console that law is the CONSTANT pair 1/0 for every particle
// PART.EXPAND has ever produced. So a block that captured the law with its
// particle and a block that read the live input ports at the emit produce
// BYTE-IDENTICAL output under every stimulus the console can generate.
//
// `part_clipfeed_directed` section 6 is the only thing that separates them, and
// it does so by driving the two bits PER PARTICLE and shutting the sink, so the
// ring holds a queue of records whose laws disagree with the one being offered.
// That section is green. A green whose red has never been seen is a claim, not
// a measurement -- CLAUDE.md's broken-instrument law, and the reason this file
// exists.
//
// WHAT THIS MEASURES AND WHAT IT DOES NOT
// ---------------------------------------
// It measures THE INSTRUMENT. `tests/mutants/zhao_part_clipfeed_emitstate_
// mutant.sv` is production with one line changed -- `o_frag_state_o` built from
// `p_depth_test_i` / `p_depth_write_i` rather than from the ring record's own
// `hd_ztest_c` / `hd_zwdis_c`. Nothing here is evidence about the shipped
// design; it is evidence that section 6's comparison can fail.
//
// AND IT IS NOT A TEST THAT ASSERTS THE BUG. The assertion that ships is
// section 6's "the record holds", in the directed file, against production.
// This is its separate positive control, exactly as CLAUDE.md prescribes:
// "Assert the correct behaviour (the record holds) and keep the detector's
// positive control separate."
//
// THE NEGATIVE CONTROL is `part_clipfeed_directed` itself: the identical
// stimulus shape against unmutated production must read ZERO disagreements.
// Both halves are required before section 6's zero may be quoted.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_part_clipfeed_emitstate_mutant.h"

#include "zhao_sim.hpp"

namespace {

int checks = 0;
int fails = 0;

void check(bool ok, const char* what) {
  ++checks;
  if (!ok) {
    ++fails;
    std::printf("  FAIL: %s\n", what);
  }
}

typedef Vzhao_part_clipfeed_emitstate_mutant Dut;

uint32_t raw22(int32_t v) { return (uint32_t)v & 0x3FFFFFu; }

// The word the block SHOULD emit for a particle: [0] Z_TEST_EN, [1]
// Z_WRITE_DIS, over the default base of zero. Note the inversion on write --
// the producer's port is an enable, the word's bit is a disable.
uint32_t frag_state_of(uint8_t depth_test, uint8_t depth_write) {
  return (uint32_t)(depth_test ? 1u : 0u) | (uint32_t)(depth_write ? 0u : 2u);
}

struct Law {
  uint8_t test;
  uint8_t write;
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  Dut dut;
  dut.clk = 0;
  dut.rst_n = 0;
  dut.p_valid_i = 0;
  dut.o_ready_i = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) {
    dut.clk = 1; dut.eval();
    dut.clk = 0; dut.eval();
  }
  dut.rst_n = 1;
  dut.clk = 1; dut.eval();
  dut.clk = 0; dut.eval();

  // Four laws, cycled -- the same stimulus shape section 6 uses. The sink is
  // SHUT while they are offered, so the ring accumulates records whose laws
  // differ from whatever is on the ports when each finally leaves.
  const Law kLaws[4] = {{1, 0}, {1, 1}, {0, 0}, {0, 1}};
  const uint32_t kWs[4] = {0x0001'0000u, 0x0004'0000u, 0x0010'0000u, 0x0100'0000u};

  uint32_t queued[64];
  int offered = 0;
  for (int guard = 0; guard < 2000 && offered < 8; ++guard) {
    const Law& law = kLaws[offered % 4];
    const int32_t x = (int32_t)(offered * 137) - 2000;
    const int32_t y = (int32_t)(offered * 91) - 1500;
    const int32_t side = 16 + (offered % 7) * 48;
    dut.p_valid_i = 1;
    dut.p_ax_i = raw22(x);                dut.p_ay_i = raw22(y - side);
    dut.p_bx_i = raw22(x - side * 3 / 4); dut.p_by_i = raw22(y + side / 2);
    dut.p_cx_i = raw22(x + side * 3 / 4); dut.p_cy_i = raw22(y + side / 2);
    dut.p_w_i = kWs[offered % 4];
    dut.p_profile_i = (uint8_t)(offered % 2);
    dut.p_r_i = (uint8_t)(offered * 17 + 3);
    dut.p_g_i = (uint8_t)(offered * 29 + 11);
    dut.p_b_i = (uint8_t)(offered * 43 + 7);
    dut.p_src_id_i = (uint16_t)(0x1000 + offered);
    dut.p_depth_test_i = law.test;
    dut.p_depth_write_i = law.write;
    dut.eval();
    const bool taken = dut.p_ready_o != 0;
    if (taken) {
      queued[offered] = frag_state_of(law.test, law.write);
      ++offered;
    }
    dut.clk = 1; dut.eval();
    dut.clk = 0; dut.eval();
  }
  check(offered >= 8, "a queue of particles with DIFFERING laws was taken");

  // THE PORTS NOW CONTRADICT EVERY QUEUED RECORD, and nothing more is offered.
  dut.p_valid_i = 0;
  dut.p_depth_test_i = 0;
  dut.p_depth_write_i = 1;   // held word: Z_TEST_EN=0, Z_WRITE_DIS=0 -> 0x0
  const uint32_t held = frag_state_of(0, 1);
  dut.eval();

  dut.o_ready_i = 1;
  int emitted = 0;
  int disagreements = 0;
  int held_value_seen = 0;
  for (int guard = 0; guard < 20000 && emitted < offered; ++guard) {
    dut.eval();
    if (dut.o_valid_o && dut.o_ready_i) {
      const uint32_t got = dut.o_frag_state_o;
      if (got != queued[emitted]) ++disagreements;
      if (got == held) ++held_value_seen;
      ++emitted;
    }
    dut.clk = 1; dut.eval();
    dut.clk = 0; dut.eval();
  }

  std::printf("[part_clipfeed_emitstate_mutant_control] offered=%d emitted=%d "
              "disagreements=%d held_value_seen=%d (held word 0x%X)\n",
              offered, emitted, disagreements, held_value_seen, held);

  check(emitted == offered, "the mutant still drained the ring -- the fault is "
                            "in the FIELD, not in the handshake");
  // THE INVERSION. Production must make this ZERO; the mutant must not.
  check(disagreements > 0,
        "INVERTED: the mutant emitted a word that disagreed with the accepted "
        "record, so section 6's comparison CAN fail");
  check(held_value_seen == emitted,
        "INVERTED: and every beat carried the word held on the PORTS, which is "
        "precisely the read-at-the-emit fault rather than some other noise");

  std::printf("part_clipfeed_emitstate_mutant_control: %d check(s), %d failure(s)\n",
              checks, fails);
  zhao::exit_hard(fails == 0 ? 0 : 1);
}
