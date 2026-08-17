// input_snapshot_sim.hpp — Verilator glue for the zhao_input_snapshot tests
// (W2.3). Wraps the port-level access (the packed zhao_frame_tick_t word,
// the 640-bit packed PadFrame array, unpacked array ports) behind small
// helpers so the directed/random tests read like the law they check
// (spec/input_rules.md 1-2).

#pragma once

#include <cstdint>

#include "Vzhao_input_snapshot.h"
#include "input_tick_sim.hpp"
#include "zref/zref_input.hpp"

namespace zhao_input {

// ---- raw pad state -> module input pins ----------------------------------
inline void drivePad(Vzhao_input_snapshot& top, int i, const zref::PadRawState& s) {
  const uint32_t mask = ~(1u << i);
  top.pad_present = (top.pad_present & mask) | (s.present ? (1u << i) : 0u);
  top.pad_buttons[i] = s.buttons;
  top.pad_lx[i] = static_cast<uint16_t>(s.lx);
  top.pad_ly[i] = static_cast<uint16_t>(s.ly);
  top.pad_rx[i] = static_cast<uint16_t>(s.rx);
  top.pad_ry[i] = static_cast<uint16_t>(s.ry);
}

inline void drivePads(Vzhao_input_snapshot& top, const zref::PadRawState pads[4]) {
  for (int i = 0; i < 4; ++i) drivePad(top, i, pads[i]);
}

// ---- packed PadFrame array -> zref::PadFrame ------------------------------
// flat layout = generated zhao_pack_pad_frame (LSB-first per slot):
//   [0+:8] pad_index  [8+:8] flags  [16+:16] sequence  [32+:32] buttons
//   [64+:16] lx  [80+:16] ly  [96+:16] rx  [112+:16] ry  [128+:32] rsv
inline uint64_t flatBits(const Vzhao_input_snapshot& top, int lo, int width) {
  uint64_t v = 0;
  for (int b = 0; b < width; ++b) {
    const int idx = lo + b;
    v |= static_cast<uint64_t>((top.pad_frame_flat[idx / 32] >> (idx % 32)) & 1u) << b;
  }
  return v;
}

inline zref::PadFrame readPadFrame(const Vzhao_input_snapshot& top, int i) {
  const int base = i * 160;
  zref::PadFrame f;
  f.pad_index = static_cast<uint8_t>(flatBits(top, base + 0, 8));
  f.flags = static_cast<uint8_t>(flatBits(top, base + 8, 8));
  f.sequence = static_cast<uint16_t>(flatBits(top, base + 16, 16));
  f.buttons = static_cast<uint32_t>(flatBits(top, base + 32, 32));
  f.lx = static_cast<int16_t>(flatBits(top, base + 64, 16));
  f.ly = static_cast<int16_t>(flatBits(top, base + 80, 16));
  f.rx = static_cast<int16_t>(flatBits(top, base + 96, 16));
  f.ry = static_cast<int16_t>(flatBits(top, base + 112, 16));
  f.rsv = static_cast<uint32_t>(flatBits(top, base + 128, 32));
  return f;
}

// byte identity: the ABI wire bytes of a frame vs the packed output
inline bool frameBytesMatchFlat(const zref::PadFrame& f, const Vzhao_input_snapshot& top, int i) {
  uint8_t wire[20];
  zref::padFrameToWire(f, wire);
  for (int b = 0; b < 20; ++b) {
    if (wire[b] != flatBits(top, i * 160 + b * 8, 8)) return false;
  }
  return true;
}

// ---- module reset (async rst_n, inputs parked) ----------------------------
inline void resetSnapshot(Vzhao_input_snapshot& top) {
  top.clk = 0;
  top.rst_n = 0;
  top.pad_present = 0;
  for (int i = 0; i < 4; ++i) {
    top.pad_buttons[i] = 0;
    top.pad_lx[i] = 0;
    top.pad_ly[i] = 0;
    top.pad_rx[i] = 0;
    top.pad_ry[i] = 0;
  }
  top.frame_tick = tickWord(false, 0, false);
  top.eval();
  edge(top);
  edge(top);
  top.rst_n = 1;
  top.eval();
}

}  // namespace zhao_input
