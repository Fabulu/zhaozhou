// input_tick_sim.hpp — verilated-free glue shared by both W2.3 input model
// helpers: the packed zhao_frame_tick_t word layout and one clock edge.

#pragma once

#include <cstdint>

namespace zhao_input {

// frame_tick word — packed struct declared {pulse, frame_id, repeated} packs
// MSB-first (SV law), so the wire bit map is {pulse[33], frame_id[32:1],
// repeated[0]}:
inline uint64_t tickWord(bool pulse, uint32_t frame_id, bool repeated) {
  return (static_cast<uint64_t>(pulse ? 1u : 0u) << 33) |
         (static_cast<uint64_t>(frame_id) << 1) |
         static_cast<uint64_t>(repeated ? 1u : 0u);
}

// one clock edge (set inputs first; they sample at the rising edge)
template <typename Top>
inline void edge(Top& top) {
  top.clk = 0;
  top.eval();
  top.clk = 1;
  top.eval();
  top.clk = 0;
  top.eval();
}

}  // namespace zhao_input
