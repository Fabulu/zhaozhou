// light_stream_guard_mutant.cpp -- INVERSE POLARITY. This test PASSES when
// the counter under examination FIRES.
//
// It is evidence about the INSTRUMENT, not about the design. Two guards in
// `zhao_light_stream` are asserted zero by the directed bench and cannot be
// reached by any legal stimulus, so their silence is worth nothing until each
// has been seen to move:
//
//   ZHAO_MUT_COUNTER == 0 : root_queue_overflow_o, fired by RQ_ROOM_MARGIN = 1
//   ZHAO_MUT_COUNTER == 1 : tag_mismatch_o,        fired by SIDE_SKEW = 1
//
// The wrappers live in tests/mutants/zhao_light_stream_guard_mutants.sv and
// instantiate PRODUCTION, so they cannot drift away from it the way a copied
// body does.
//
// NOTE ON WHAT THIS DOES NOT SHOW. That a counter can fire is not evidence
// that the design is correct; the directed bench is. This is the other half:
// evidence that the directed bench's "it stayed at zero" means anything.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#ifndef ZHAO_MUT_COUNTER
#error "define ZHAO_MUT_COUNTER as 0 (root queue overflow) or 1 (tag mismatch)"
#endif

#if ZHAO_MUT_COUNTER == 0
#include "Vzhao_light_stream_rqfull_mutant.h"
using Dut = Vzhao_light_stream_rqfull_mutant;
static const char* kName = "light_stream_rqfull_mutant";
static const char* kWhat = "root_queue_overflow_o";
#else
#include "Vzhao_light_stream_skew_mutant.h"
using Dut = Vzhao_light_stream_skew_mutant;
static const char* kName = "light_stream_skew_mutant";
static const char* kWhat = "tag_mismatch_o";
#endif

#include "zhao_sim.hpp"

using zhao::check;

namespace {

void tk(Dut& d) { zhao::tick(d); }

void cfg_write(Dut& d, uint32_t light, uint32_t half, uint32_t word, uint32_t data) {
  d.cfg_we_i = 1;
  d.cfg_addr_i = static_cast<uint8_t>(((light & 0xF) << 4) | ((half & 1) << 3) | (word & 7));
  d.cfg_data_i = data;
  tk(d);
  d.cfg_we_i = 0;
  d.cfg_addr_i = 0;
  d.cfg_data_i = 0;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut* dp = new Dut;  // heap-allocated, harness rule
  Dut& d = *dp;

  d.rst_n = 0;
  d.cfg_we_i = 0;
  d.cfg_commit_i = 0;
  d.cfg_addr_i = 0;
  d.cfg_data_i = 0;
  d.v_valid_i = 0;
  d.r_ready_i = 0;
  d.n_x_i = 0;
  d.n_y_i = 0;
  d.n_z_i = 0;
  d.n_mag_valid_i = 0;
  d.n_mag_i = 0;
  d.n_degenerate_i = 0;
  d.n_profile_i = 0;
  d.n_lights_i = 0;
  d.n_src_id_i = 0;
  d.eval();
  for (int i = 0; i < 3; ++i) tk(d);
  d.rst_n = 1;
  d.eval();
  tk(d);

  // Four lights, unit red gain, a direction the dot product actually uses.
  for (uint32_t li = 0; li < 4; ++li) {
    cfg_write(d, li, 0, 0, 0x00001000 * (li + 1));
    cfg_write(d, li, 0, 1, 0x00010000);
    cfg_write(d, li, 0, 2, 0xFFFFF000);
    cfg_write(d, li, 0, 3, static_cast<uint32_t>(0x100 * (li + 1)));  // a detail term
    cfg_write(d, li, 1, 0, 0x000FFFFF);
    cfg_write(d, li, 1, 1, 0);
    cfg_write(d, li, 1, 2, 0);
    cfg_write(d, li, 1, 3, 0);
  }
  d.cfg_commit_i = 1;
  tk(d);
  d.cfg_commit_i = 0;
  d.eval();

  // An ordinary, entirely legal stream. Nothing below is illegal stimulus --
  // that is the point: the fault is in the BLOCK, not in the traffic.
  const uint32_t kN = 600;
  uint32_t sent = 0, got = 0;
  uint64_t guard = 0;
  uint64_t rng = 0xC2B2AE3D27D4EB4FULL;
  while (got < kN) {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    d.r_ready_i = 1;
    if (sent < kN) {
      d.v_valid_i = 1;
      d.n_x_i = static_cast<uint32_t>(static_cast<int32_t>(rng % 131073) - 65536);
      d.n_y_i = static_cast<uint32_t>(static_cast<int32_t>((rng >> 20) % 131073) - 65536);
      d.n_z_i = static_cast<uint32_t>(static_cast<int32_t>((rng >> 40) % 131073) - 65536);
      d.n_mag_valid_i = 0;
      d.n_degenerate_i = 0;
      d.n_profile_i = 0;
      d.n_lights_i = 4;
      d.n_src_id_i = static_cast<uint16_t>(sent);
    } else {
      d.v_valid_i = 0;
    }
    d.eval();
    if (sent < kN && d.v_ready_o) ++sent;
    if (d.r_valid_o) ++got;
    tk(d);
    if (++guard > 200000) break;  // the rqfull mutant WEDGES; that is its fault
  }

#if ZHAO_MUT_COUNTER == 0
  const uint32_t fired = d.root_queue_overflow_o;
#else
  const uint32_t fired = d.tag_mismatch_o;
#endif

  std::printf("[%s] sent=%u emitted=%u clocks=%llu | %s = %u\n", kName, sent, got,
              static_cast<unsigned long long>(guard), kWhat, fired);

  // INVERSE POLARITY: the mutant passes when the counter MOVES.
  check(fired > 0, "the guard FIRED on the mutation it exists to catch", 1, (fired > 0) ? 1 : 0);

  d.final();
  zhao::exit_hard(zhao::report_and_exit(kName));
}
