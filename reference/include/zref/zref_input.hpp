// zref_input.hpp — INPUT subsystem golden reference (charter 20, plan W2.3).
//
// Law: spec/input_rules.md (ratified 2026-08-14, decision D5). Two oracles:
//
//   zref::PadSnapshot  — the INPUT.SNAPSHOT latch law: atomic snapshot of all
//                        four pads at each frame_tick, per-pad monotonic
//                        sequence (frozen while absent, mod-2^16 wrap), the
//                        absent-pad zero frame, and the (structurally-never-
//                        firing) input_sequence_gaps detector.
//   zref::RumbleBridge — the INPUT.RUMBLE law: DebugRumble 0xF004 decode
//                        (pad_index/enable/strength), frame-gated duty latch,
//                        last-writer-wins with rumble_frames_dropped, hold-
//                        with-no-command, and the free-running PWM carrier
//                        model (phase 0 at reset, NEVER reset — duty changes
//                        never glitch the carrier).
//
// Both oracles are bit-exact mirrors of fpga/rtl/input/{zhao_input_snapshot,
// zhao_input_rumble}.sv; the directed/random differential tests compare them
// every frame. Header-only constexpr-friendly C++17 (zref lane convention).
//
// SEQUENCE CONVENTION (input_rules.md 2.3, pinned here + RTL):
//   after reset sequence = 0 (pads absent, frozen at 0). At each frame_tick a
//   PRESENT pad's sequence first increments (mod 2^16) and the latched frame
//   carries the NEW value — the first present frame therefore carries 1, tick
//   65536 wraps to 0. An absent pad's frame carries the frozen value.

#pragma once

#include <cstdint>

namespace zref {

// ------------------------------------------------------------ helpers -----

// saturating increment (spec/counters.md 4: u64 counters saturate, never wrap)
inline uint64_t sat_inc64(uint64_t v) { return (v == 0xFFFFFFFFFFFFFFFFull) ? v : v + 1; }

// ---------------------------------------------------------------- pads -----

// Raw decoded pad state as it enters INPUT.SNAPSHOT (hardware applies ZERO
// policy: raw sticks, no deadzone/calibration/remap — input_rules.md 1).
struct PadRawState {
  bool present;
  uint32_t buttons;
  int16_t lx, ly, rx, ry;
};

inline PadRawState absentPad() { return PadRawState{false, 0, 0, 0, 0, 0}; }

// ABI mirror of struct PadFrame (20 B little-endian wire, input_rules.md 1).
// Field order = wire order; toWire() emits the exact .zcap CONTROLLER_SNAPSHOT
// entry bytes (byte-identical to the SV zhao_pack_pad_frame layout).
struct PadFrame {
  uint8_t pad_index;
  uint8_t flags;      // bit0 pad_present; bits 1-7 reserved 0
  uint16_t sequence;  // per-pad monotonic (input_rules.md 2.3)
  uint32_t buttons;
  int16_t lx, ly, rx, ry;
  uint32_t rsv;  // reserved, 0

  bool operator==(const PadFrame& o) const {
    return pad_index == o.pad_index && flags == o.flags && sequence == o.sequence &&
           buttons == o.buttons && lx == o.lx && ly == o.ly && rx == o.rx && ry == o.ry &&
           rsv == o.rsv;
  }
  bool operator!=(const PadFrame& o) const { return !(*this == o); }
};

// Exact wire bytes (20) of one PadFrame — the .zcap 0x0004 entry body.
inline void padFrameToWire(const PadFrame& f, uint8_t out[20]) {
  out[0] = f.pad_index;
  out[1] = f.flags;
  out[2] = uint8_t(f.sequence & 0xFF);
  out[3] = uint8_t(f.sequence >> 8);
  out[4] = uint8_t(f.buttons & 0xFF);
  out[5] = uint8_t((f.buttons >> 8) & 0xFF);
  out[6] = uint8_t((f.buttons >> 16) & 0xFF);
  out[7] = uint8_t((f.buttons >> 24) & 0xFF);
  out[8] = uint8_t(uint16_t(f.lx) & 0xFF);
  out[9] = uint8_t(uint16_t(f.lx) >> 8);
  out[10] = uint8_t(uint16_t(f.ly) & 0xFF);
  out[11] = uint8_t(uint16_t(f.ly) >> 8);
  out[12] = uint8_t(uint16_t(f.rx) & 0xFF);
  out[13] = uint8_t(uint16_t(f.rx) >> 8);
  out[14] = uint8_t(uint16_t(f.ry) & 0xFF);
  out[15] = uint8_t(uint16_t(f.ry) >> 8);
  out[16] = out[17] = out[18] = out[19] = 0;  // rsv MUST be 0
}

// INPUT.SNAPSHOT oracle: feed raw per-pad timelines + the tick schedule,
// read the exact latched PadFrame array per frame (input_rules.md 2).
class PadSnapshot {
 public:
  void reset() {
    for (int i = 0; i < 4; ++i) {
      seq_[i] = 0;
      seq_prev_[i] = 0;
      out_[i] = PadFrame{uint8_t(i), 0, 0, 0, 0, 0, 0, 0, 0};
    }
    gaps_ = 0;
    frame_id_ = 0;
  }

  PadSnapshot() { reset(); }

  // One frame_tick. `pads` is the raw state AT the tick instant (atomicity
  // law: mid-frame changes are invisible). frame_id mirrors the broadcast
  // zhao_frame_tick_t.frame_id of this tick.
  const PadFrame* tick(const PadRawState pads[4], uint32_t frame_id) {
    for (int i = 0; i < 4; ++i) {
      const uint16_t expect = uint16_t(seq_prev_[i] + 1);  // gap law (2.3)
      if (pads[i].present) {
        seq_[i] = expect;
        out_[i].pad_index = uint8_t(i);
        out_[i].flags = 0x01;
        out_[i].sequence = seq_[i];
        out_[i].buttons = pads[i].buttons;
        out_[i].lx = pads[i].lx;
        out_[i].ly = pads[i].ly;
        out_[i].rx = pads[i].rx;
        out_[i].ry = pads[i].ry;
        out_[i].rsv = 0;
        // sequence-gap detector: impossible by construction here (the latch
        // is synchronous); the RTL carries the same comparator and the
        // formal property proves it never fires (input_snapshot_atomic).
        if (seq_[i] != expect) gaps_ = sat_inc64(gaps_);
      } else {
        // absent pad: zero frame, sequence FROZEN (input_rules.md 2.2)
        out_[i].pad_index = uint8_t(i);
        out_[i].flags = 0x00;
        out_[i].sequence = seq_[i];
        out_[i].buttons = 0;
        out_[i].lx = 0;
        out_[i].ly = 0;
        out_[i].rx = 0;
        out_[i].ry = 0;
        out_[i].rsv = 0;
      }
      seq_prev_[i] = seq_[i];
    }
    frame_id_ = frame_id;
    return out_;
  }

  const PadFrame* frames() const { return out_; }
  const PadFrame& frame(int i) const { return out_[i]; }
  uint16_t sequence(int i) const { return out_[i].sequence; }
  uint64_t gaps() const { return gaps_; }
  uint32_t frameId() const { return frame_id_; }

 private:
  PadFrame out_[4];
  uint16_t seq_[4];
  uint16_t seq_prev_[4];
  uint64_t gaps_;
  uint32_t frame_id_;
};

// -------------------------------------------------------------- rumble -----

// INPUT.RUMBLE oracle: feed the DebugRumble command timeline + ticks, read
// the exact latched duty table, drop accounting, and PWM waveform
// (input_rules.md 3).
class RumbleBridge {
 public:
  void reset() {
    for (int i = 0; i < 4; ++i) {
      duty_[i] = 0;
      pend_valid_[i] = false;
      pend_en_[i] = false;
      pend_str_[i] = 0;
    }
    dropped_ = 0;
    dropped_shadow_ = 0;
  }

  RumbleBridge() { reset(); }

  // One executed DebugRumble 0xF004 {pad_index, enable, strength}.
  // Bad index (> 3): dropped entirely + rumble_frames_dropped++.
  // Second command for the same pad within one frame: the pending one is
  // replaced (last-writer-wins at the NEXT tick) and the dropped one counts.
  void command(uint8_t pad_index, uint8_t enable, uint8_t strength) {
    if (pad_index > 3) {
      dropped_ = sat_inc64(dropped_);
      return;
    }
    if (pend_valid_[pad_index]) dropped_ = sat_inc64(dropped_);
    pend_valid_[pad_index] = true;
    pend_en_[pad_index] = (enable != 0);
    pend_str_[pad_index] = strength;
  }

  // One frame_tick: latch pending targets (duty = enable ? strength : 0),
  // hold previous targets for pads without a command, shadow the counter.
  void tick() {
    for (int i = 0; i < 4; ++i) {
      if (pend_valid_[i]) {
        duty_[i] = pend_en_[i] ? pend_str_[i] : 0;
        pend_valid_[i] = false;
      }
    }
    dropped_shadow_ = dropped_;
  }

  const uint8_t* duty() const { return duty_; }
  uint8_t duty(int pad) const { return duty_[pad]; }
  uint64_t dropped() const { return dropped_; }               // live counter
  uint64_t droppedShadow() const { return dropped_shadow_; }  // frame-stable

  // PWM carrier model (input_rules.md 3): 8-bit phase, free-running from
  // reset, NEVER reset (a duty change never glitches the carrier). With
  // PWM_PHASE_DIV = 1 (the Verilator profile), `cycle` is clk cycles since
  // reset; phase = cycle mod 256; high while phase < duty (duty 0 = off).
  static bool pwm(uint8_t duty, uint64_t cycle, uint64_t phase_div = 1) {
    if (duty == 0) return false;
    const uint8_t phase = uint8_t((cycle / phase_div) & 0xFF);
    return phase < duty;
  }

 private:
  uint8_t duty_[4];  // latched targets (the pad PHY out)
  bool pend_valid_[4];
  bool pend_en_[4];
  uint8_t pend_str_[4];
  uint64_t dropped_, dropped_shadow_;
};

}  // namespace zref
