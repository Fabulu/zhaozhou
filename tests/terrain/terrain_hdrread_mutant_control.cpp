// terrain_hdrread_mutant_control.cpp -- THE POSITIVE CONTROL for
// `terrain_hdrread_directed.cpp:test_the_job_port_may_advance_during_the_burst`.
//
// ITS POLARITY IS INVERTED. It PASSES when the forwarded job carries the WRONG
// patch, because it is built against
// `tests/mutants/zhao_terrain_hdrread_live_forward_mutant.sv`, whose five
// forwarding assignments read the LIVE job port instead of the block's latched
// copy. A green run here says the ordering check in the real test is
// load-bearing; a red one says that check would pass on a block with the fault
// in it and is worth nothing.
//
// WHY A MUTANT AND NOT STIMULUS. The four other refusals this block makes are
// all reachable from its own input ports and are fired with legal stimulus in
// the directed test -- a denied guard, a short burst, a wrong island, an
// out-of-pool slot. THIS one cannot be: a correct block has no input that makes
// it forward the wrong identity, so "the ordering check can fail" would stay an
// argument forever. CLAUDE.md's rule is that such a guard needs a COMMITTED
// mutant, renamed so no source list can elaborate it by mistake, with a driver
// whose polarity is inverted.
//
// THE NEGATIVE CONTROL, and it is the one CLAUDE.md says every macro-selected
// mutant owes. Verilator's `-D` cannot override a function-like `define and
// says nothing when it fails to, so a mutant build can silently compile
// production and pass while testing nothing. `tb_terrain_hdrread.sv` therefore
// selects the DUT with a PLAIN `ifdef, which -D does reach, and the proof the
// selector engaged is that this file FAILS when built WITHOUT
// ZHAO_HDRREAD_LIVE_FORWARD_MUTANT -- the real block forwards the right patch,
// so the inverted assertion goes red. That was run.

#include <cstdint>
#include <cstdio>

#include "verilated.h"
#include "Vtb_terrain_hdrread.h"
#include "zhao_sim.hpp"

namespace {

int g_failed = 0;

// The first patch, and the one TERRAIN.SEQ moves on to while the header burst
// is still in flight.
constexpr uint32_t kSlotA = 5;
constexpr uint32_t kGenA = 0x21;
constexpr uint32_t kEpoch = 0x1234;
constexpr uint32_t kSrcA = 0xABCD1234;
constexpr uint32_t kFlagsA = 0x0008;
constexpr uint32_t kIslandA = 0x00C0FFEE;

constexpr uint32_t kSlotB = 900;
constexpr uint32_t kGenB = 0xFE;

// A minimal MEM.GUARD: level `ready`, pulsed `ok` one cycle later, then eight
// beats of a header naming patch A. Deliberately its own copy rather than the
// directed test's -- a control that shared the thing under test would prove
// less than it looks.
struct Guard {
  int phase = 0;
  int beat = 0;
  uint64_t header[8] = {};

  void build() {
    uint8_t b[64] = {};
    b[0] = 1;                                    // format_version = 1
    b[2] = 1;                                    // pitch_log2 = +1
    for (int i = 0; i < 4; ++i) b[4 + i] = (kIslandA >> (8 * i)) & 0xff;
    b[8] = 3; b[9] = 0;                          // patch_ix = 3
    b[10] = 0; b[11] = 0;                        // patch_iz = 0
    for (int k = 0; k < 8; ++k) {
      uint64_t w = 0;
      for (int i = 0; i < 8; ++i) w |= static_cast<uint64_t>(b[k * 8 + i]) << (8 * i);
      header[k] = w;
    }
  }

  void step(Vtb_terrain_hdrread& d) {
    d.g_ready = 0; d.g_ok = 0; d.g_violation = 0;
    d.beat_valid = 0; d.beat_last = 0;
    if (phase == 0) {
      if (d.g_valid) { d.g_ready = 1; phase = 1; }
    } else if (phase == 1) {
      d.g_ok = 1; beat = 0; phase = 2;
    } else {
      d.beat_valid = 1;
      d.beat_data = header[beat];
      d.beat_last = (beat == 7) ? 1 : 0;
      if (beat == 7) phase = 0;
      ++beat;
    }
  }
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  Vtb_terrain_hdrread d;
  Guard g;
  g.build();

  d.rst_n = 0;
  d.cfg_epoch = kEpoch;
  d.j_valid = 0;
  d.j_slot = 0; d.j_gen = 0; d.j_epoch = 0; d.j_src_id = 0; d.j_flags = 0;
  d.j_island = 0; d.j_ix = 0; d.j_iz = 0;
  d.g_ready = 0; d.g_ok = 0; d.g_violation = 0;
  d.beat_valid = 0; d.beat_data = 0; d.beat_last = 0;
  d.h_ready = 0; d.f_ready = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(d);
  d.rst_n = 1;
  zhao::tick(d);

  // Patch A at the door.
  d.j_valid = 1;
  d.j_slot = kSlotA; d.j_gen = kGenA; d.j_epoch = kEpoch; d.j_src_id = kSrcA;
  d.j_flags = kFlagsA; d.j_island = kIslandA; d.j_ix = 3; d.j_iz = 0;
  d.h_ready = 1;
  d.f_ready = 1;

  bool mutated = false;
  bool forwarded = false;
  uint32_t f_slot = 0, f_gen = 0;

  for (int c = 0; c < 80 && !forwarded; ++c) {
    g.step(d);
    const bool accept_j = d.j_valid && d.j_ready;
    if (d.f_valid && d.f_ready) {
      forwarded = true;
      f_slot = d.f_slot;
      f_gen = d.f_gen;
    }
    zhao::tick(d);
    if (accept_j) d.j_valid = 0;
    if (!mutated && d.beat_valid) {
      // TERRAIN.SEQ presents the next patch the moment its ready came.
      d.j_slot = kSlotB;
      d.j_gen = kGenB;
      d.j_epoch = 0x9999;
      d.j_src_id = 0x55555555;
      d.j_flags = 0xFFFF;
      d.j_island = 0x0BADF00D;
      d.j_ix = static_cast<uint16_t>(-321);
      d.j_iz = 321;
      mutated = true;
    }
  }

  if (!forwarded) {
    std::printf("FAIL: the mutant never forwarded a job at all -- the control proves nothing\n");
    ++g_failed;
  } else {
    // INVERTED: the mutant MUST show the second patch's identity here.
    if (f_slot != kSlotB || f_gen != kGenB) {
      std::printf("FAIL: the mutant forwarded slot %u gen %u; the control expects the LIVE "
                  "port's %u / %u. Either the mutation did not engage (check that "
                  "ZHAO_HDRREAD_LIVE_FORWARD_MUTANT reached the build) or the fault this "
                  "control exists to model is no longer reachable.\n",
                  f_slot, f_gen, kSlotB, kGenB);
      ++g_failed;
    } else {
      std::printf("terrain_hdrread_mutant_control: the live-forward fault IS visible to the "
                  "ordering check (forwarded slot %u, gen %u -- patch B, not patch A)\n",
                  f_slot, f_gen);
    }
  }

  std::fflush(stdout);
  // TEARDOWN-DEADLOCK WORKAROUND, documented in tests/harness/zhao_sim.hpp.
  zhao::exit_hard(g_failed == 0 ? 0 : 1);
}
