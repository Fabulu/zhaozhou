// input_rumble_sim.hpp — Verilator glue for the zhao_input_rumble tests
// (W2.3): the packed frame_tick word (same layout as input_snapshot_sim.hpp)
// and the rumble module reset. Law: spec/input_rules.md 3.

#pragma once

#include <cstdint>

#include "Vzhao_input_rumble.h"
#include "input_tick_sim.hpp"  // tickWord + edge
#include "zref/zref_input.hpp"

namespace zhao_input {

inline void resetRumble(Vzhao_input_rumble& top) {
  top.clk = 0;
  top.rst_n = 0;
  top.rumble_cmd_valid = 0;
  top.rumble_pad_index = 0;
  top.rumble_enable = 0;
  top.rumble_strength = 0;
  top.frame_tick = tickWord(false, 0, false);
  top.eval();
  edge(top);
  edge(top);
  top.rst_n = 1;
  top.eval();
}

}  // namespace zhao_input
