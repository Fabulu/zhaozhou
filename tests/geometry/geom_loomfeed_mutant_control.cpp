// geom_loomfeed_mutant_control.cpp -- THE POSITIVE CONTROL for
// `zhao_geom_loomfeed`'s `ret_overflow_o`. INVERTED POLARITY: this test passes
// when the counter FIRES.
//
// The DUT is `tests/mutants/zhao_geom_loomfeed_mutant.sv`, whose one
// substantive change is the removal of the return credit. Production's law 7
// reserves a return slot when a post is CONSUMED, which makes the overflow
// state unreachable by every legal stimulus -- so the production test can only
// assert that the counter reads zero, and a zero nobody has seen move is a
// claim rather than a measurement (CLAUDE.md: "a detector that has not been
// shown to FIRE has not been tested").
//
// This file is what turns that claim into evidence. It is about the
// INSTRUMENT, not about the design, and it must NOT be read as a test of
// anything that ships.
//
// The stimulus is ordinary: stage six one-node streams, post them, and never
// take a return. With the credit gone the drain consumes posts anyway and the
// fifth return record overruns a four-deep queue.

#include <cstdint>
#include <cstdio>
#include <map>

#include "verilated.h"

#include "Vtb_geom_loomfeed_mutant.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

using Dut = Vtb_geom_loomfeed_mutant;

constexpr uint32_t kMagic = 0x4D4F4F4Cu;

std::map<uint32_t, uint64_t> g_mem;
uint32_t g_addr = 0;
int g_phase = 0;
int g_beat = 0;
int g_wait = 0;

void drive(Dut& d) {
  d.grant_i = 0;
  d.err_i = 0;
  d.beat_valid_i = 0;
  d.beat_data_i = 0;
  d.beat_last_i = 0;
  if (g_phase == 0) {
    if (d.req_valid_o) {
      g_addr = d.req_addr_o;
      d.grant_i = 1;
      g_phase = 1;
      g_wait = 2;
      g_beat = 0;
    }
    return;
  }
  if (g_wait > 0) {
    --g_wait;
    return;
  }
  auto it = g_mem.find(g_addr + 8u * static_cast<uint32_t>(g_beat));
  d.beat_valid_i = 1;
  d.beat_data_i = (it == g_mem.end()) ? 0ull : it->second;
  d.beat_last_i = (g_beat == 7) ? 1 : 0;
  ++g_beat;
  if (g_beat == 8) g_phase = 0;
}

void step(Dut& d) {
  drive(d);
  zhao::tick(d);
}

// One header plus one ROOT node, at `base`.
void stage_one(uint32_t base) {
  g_mem[base + 0] = static_cast<uint64_t>(kMagic) | (1ull << 32);
  for (uint32_t w = 1; w < 8; ++w) g_mem[base + 8u * w] = 0;
  const uint32_t n = base + 64u;
  g_mem[n + 0] = 0;  // index 0, parent 0, kind ROOT
  for (uint32_t w = 1; w < 8; ++w) g_mem[n + 8u * w] = 0;
  g_mem[n + 8] = static_cast<uint64_t>(0x00010000u);  // param[0] = 1.0
  g_mem[n + 24] = static_cast<uint64_t>(0x00010000u) << 32;  // param[5]
  g_mem[n + 48] = static_cast<uint64_t>(0x00010000u);  // param[10]
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;

  dut.rst_n = 0;
  dut.cfg_plan_base_i = 0x1111'2222u;
  dut.post_valid_i = 0;
  dut.post_base_i = 0;
  dut.post_ticket_i = 0;
  dut.ret_ready_i = 0;  // NOTHING is ever drained. That is the stimulus.
  dut.grant_i = 0;
  dut.err_i = 0;
  dut.beat_valid_i = 0;
  dut.beat_data_i = 0;
  dut.beat_last_i = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);

  for (int s = 0; s < 8; ++s) {
    stage_one(0x0010'0000u + 0x1000u * static_cast<uint32_t>(s));
  }

  int posted = 0;
  for (int i = 0; i < 40000 && posted < 8; ++i) {
    dut.post_base_i = 0x0010'0000u + 0x1000u * static_cast<uint32_t>(posted);
    dut.post_ticket_i = 0xB000u + static_cast<uint32_t>(posted);
    dut.post_valid_i = 1;
    dut.eval();
    if (dut.post_ready_o) {
      step(dut);
      ++posted;
      dut.post_valid_i = 0;
    } else {
      step(dut);
    }
  }
  dut.post_valid_i = 0;
  for (int i = 0; i < 20000; ++i) step(dut);

  check(posted == 8, "the mutant took eight posts", 8, posted);
  check(dut.posts_o >= 5, "at least five posts were consumed", 1,
        dut.posts_o >= 5 ? 1 : 0);
  // THE CHECK THIS FILE EXISTS FOR.
  check(dut.ret_overflow_o >= 1,
        "INVERTED POLARITY: `ret_overflow_o` FIRED with the credit removed", 1,
        dut.ret_overflow_o >= 1 ? 1 : 0);
  std::printf("[loomfeed mutant] posts=%u streams=%u ret_overflow=%u\n",
              static_cast<unsigned>(dut.posts_o),
              static_cast<unsigned>(dut.streams_o),
              static_cast<unsigned>(dut.ret_overflow_o));

  const int rc = zhao::report_and_exit("geom_loomfeed_mutant_control");
  zhao::exit_hard(rc);
}
