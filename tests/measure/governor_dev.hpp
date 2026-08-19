// governor_dev.hpp — the shared driver for MEASURE.GOVERNOR.
//
// One place that knows the block's frame handshake, so the directed lane, both
// random lanes and the composition drive it identically. If the port list
// changes, exactly one file needs editing and every lane stops compiling.
//
// THE FRAME SHAPE: `frame_i` is a one-cycle pulse; the block then runs a
// 33-step restoring divide per camera and publishes with a one-cycle
// `targets_valid_o`. The targets are HELD registers, not a stream — TERRAIN.LOD
// samples them with every descriptor and requires them stable across a patch
// job — so `decide()` returns them after the pulse and they stay put until the
// next decision.

#pragma once

#include <cstdint>

#include "Vzhao_measure_governor.h"

#include "zhao_sim.hpp"
#include "zref/zref_measure.hpp"

namespace gov_test {

namespace zm = zref::measure;

/** Everything the block reads at a frame pulse. */
struct Frame {
  zm::GovernorCamera cam[2];
  int view_count = 2;
  uint16_t src_id = 0;
};

/** The published targets, read straight off the ports. */
struct Targets {
  uint16_t scale[2] = {0, 0};
  bool en[2] = {false, false};
  uint16_t hyst = 0;
  uint8_t min_hold = 0;
  uint32_t morph_step = 0;
  uint8_t deg[2] = {0, 0};
  uint16_t src_id = 0;
};

inline Targets read_targets(const Vzhao_measure_governor& dut) {
  Targets t;
  t.scale[0] = dut.cam0_scale_o;
  t.scale[1] = dut.cam1_scale_o;
  t.en[0] = dut.cam0_en_o != 0;
  t.en[1] = dut.cam1_en_o != 0;
  t.hyst = dut.hyst_o;
  t.min_hold = dut.min_hold_o;
  t.morph_step = dut.morph_step_o;
  t.deg[0] = dut.deg0_o;
  t.deg[1] = dut.deg1_o;
  t.src_id = dut.src_id_o;
  return t;
}

inline void reset_dut(Vzhao_measure_governor& dut) {
  dut.rst_n = 0;
  dut.clk = 0;
  dut.frame_i = 0;
  dut.view_count_i = 0;
  dut.px_err0_i = 0;
  dut.px_err1_i = 0;
  dut.proj0_i = 0;
  dut.proj1_i = 0;
  dut.src_id_i = 0;
  dut.starved0_i = 0;
  dut.starved1_i = 0;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);
}

/**
 * Drive one whole decision and return the published targets, plus the number
 * of clocks from the frame pulse to `targets_valid_o` (the ledger's
 * `latency: variable`, measured rather than assumed).
 *
 * `hold_check` is set false if any target output moved before the publish —
 * TERRAIN.LOD requires them stable across a patch job, so a mid-decision
 * change would be a real defect.
 */
inline Targets decide(Vzhao_measure_governor& dut, const Frame& f, int* clocks = nullptr,
                      bool* hold_check = nullptr, int max_wait = 4000) {
  const Targets before = read_targets(dut);

  dut.frame_i = 1;
  dut.view_count_i = static_cast<uint8_t>(f.view_count & 3);
  dut.px_err0_i = f.cam[0].px_err;
  dut.px_err1_i = f.cam[1].px_err;
  dut.proj0_i = f.cam[0].proj;
  dut.proj1_i = f.cam[1].proj;
  dut.starved0_i = f.cam[0].starved ? 1 : 0;
  dut.starved1_i = f.cam[1].starved ? 1 : 0;
  dut.src_id_i = f.src_id;
  zhao::tick(dut);
  dut.frame_i = 0;
  // The caller's operands go away immediately — the block must have latched
  // everything it needs. That is not politeness, it is a test: view 1's
  // numbers are used 35 clocks later.
  dut.px_err0_i = 0;
  dut.px_err1_i = 0;
  dut.proj0_i = 0;
  dut.proj1_i = 0;
  dut.starved0_i = 0;
  dut.starved1_i = 0;
  dut.view_count_i = 0;
  dut.src_id_i = 0;

  bool held = true;
  int n = 1;
  for (; n < max_wait; ++n) {
    if (dut.targets_valid_o) break;
    const Targets mid = read_targets(dut);
    if (mid.scale[0] != before.scale[0] || mid.scale[1] != before.scale[1] ||
        mid.en[0] != before.en[0] || mid.en[1] != before.en[1] || mid.src_id != before.src_id) {
      held = false;
    }
    zhao::tick(dut);
  }
  if (hold_check) *hold_check = held;
  if (clocks) *clocks = n;
  const Targets t = read_targets(dut);
  zhao::tick(dut);  // retire the publish pulse
  return t;
}

inline uint32_t rep_count(const Vzhao_measure_governor& dut, int lane) {
  switch (lane & 3) {
    case 0:
      return dut.lod_rep_count0_o;
    case 1:
      return dut.lod_rep_count1_o;
    case 2:
      return dut.lod_rep_count2_o;
    default:
      return dut.lod_rep_count3_o;
  }
}

}  // namespace gov_test
