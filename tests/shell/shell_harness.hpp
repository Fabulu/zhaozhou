// shell_harness.hpp — W2.7 shared driver for the Phase-2 console shell.
//
// Drives the Verilated tb_zhao_shell (zhao_shell_top + behavioural SDRAM
// model) as SW.RUNTIME.HPS (plan D10: the harness C++ IS the HPS): it hosts
// the FRAME_RING (descriptor states + slot bodies), the pixel arena, and
// answers bridge bursts with the frozen sim profile (16 gpu cycles to the
// first beat, 1 beat/cycle after). It also owns the clock phases (frozen
// ratios, plan R1):
//
//   gpu_clk : posedge every step
//   vid_clk : gpu/2 — coincident posedges on ODD steps (the W2.2-verified
//             convention: both clocks rise in the same eval, synchronizers
//             sample pre-edge values)
//   audio_clk: gpu/4 — its posedge fires AFTER the gpu edge on every 4th
//             step (the W2.4-verified audio_dev convention)
//
// Collected observables (post-edge, in order): gpu ticks, displayed-frame
// CRC pulses, the counters-window sweeps, PCM pairs, blit/fence pulses.
// The pad-snapshot oracle (zref::PadSnapshot) is stepped at each tick by
// the scenario; pads must be held stable between ticks (the demo law).
//
// Law: spec/memory_rules.md 3-4 (bridge + ring), spec/capture_format.md 3
// (sealed packets), spec/counters.md 3 (sweep after each tick).

#pragma once

#include "Vtb_zhao_shell.h"
#include "verilated.h"

#include "zhao_sim.hpp"
#include "zref/zref_frame.hpp"
#include "zref/zref_input.hpp"
#include "zref/zref_render.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace zhao_shell {

// PCG32 (the repo-standard deterministic generator shape)
struct Pcg32 {
  uint64_t state = 0x853c49e6748fea9bULL;
  uint64_t inc = 0xda3e39cb94b95bdbULL;
  explicit Pcg32(uint64_t seed = 42, uint64_t seq = 54) {
    state = 0;
    inc = (seq << 1) | 1u;
    next();
    state += seed;
    next();
  }
  uint32_t next() {
    const uint64_t old = state;
    state = old * 6364136223846793005ULL + inc;
    const uint32_t xorshifted = uint32_t(((old >> 18) ^ old) >> 27);
    const uint32_t rot = uint32_t(old >> 59);
    return (xorshifted >> rot) | (xorshifted << ((32 - rot) & 31));
  }
  uint32_t below(uint32_t n) { return n ? next() % n : 0; }
};

// ---------------------------------------------------------------- layout --
constexpr uint32_t kRingBase = 0x0;
constexpr uint32_t kDescTable = kRingBase;                  // 3 x 32 B
constexpr uint32_t kSlotBody0 = kRingBase + 4096;           // + s * 1 MiB
constexpr uint32_t kArena0 = 0x00400000;                    // pixel arena A
constexpr uint32_t kArena1 = 0x00440000;                    // pixel arena B
constexpr int kFirstBeatLatency = 16;                       // D10 profile

struct TickEvent {
  uint32_t frame_id;
  bool repeated;
};

struct SweepEvent {           // one counters-window sweep (40 ascending ids)
  std::vector<uint64_t> bank; // index = counter_id
};

class ShellHarness {
 public:
  Vtb_zhao_shell top;
  std::vector<uint8_t> mem;   // HPS DDR (ring + arenas)
  uint64_t n = 0;             // gpu steps since reset

  // ring mirror (what hps_state_i shows; FPGA writes come back via ring_wr)
  uint8_t ring_state[3] = {0, 0, 0};
  uint32_t ring_len[3] = {0, 0, 0};
  int publish_arm[3] = {0, 0, 0};  // publish sequencing: 1 = show ARM_WRITING

  // pad raw state (held stable between ticks by the scenario)
  bool pad_present[4] = {false, false, false, false};
  uint32_t pad_buttons[4] = {0, 0, 0, 0};
  int16_t pad_lx[4] = {0, 0, 0, 0}, pad_ly[4] = {0, 0, 0, 0};
  int16_t pad_rx[4] = {0, 0, 0, 0}, pad_ry[4] = {0, 0, 0, 0};

  // audio feed (steady pacing per the W2.4-verified law)
  bool audio_enable = true;
  uint32_t audio_prime = 512;      // pairs fed back-to-back at start
  uint32_t audio_period = 4;       // then one pair per N gpu cycles
  uint32_t audio_fed = 0;
  // audio identity is checked ON THE FLY against a bounded ring of the fed
  // pairs (the FIFO holds <= 2048+prime pairs, far below the ring): storing
  // every pair made a 10,000-frame soak need gigabytes.
  static constexpr uint32_t kAudRing = 8192;   // power of two
  std::vector<uint32_t> aud_ring = std::vector<uint32_t>(kAudRing, 0);
  uint64_t aud_sent_count = 0;
  uint64_t pcm_pop_count = 0;
  uint64_t pcm_mismatches = 0;
  uint64_t aud_ring_overruns = 0;   // lag exceeded the ring (never lawful)
  uint64_t audio_next_at = 0;
  zref::MixerTone tone{zref::ToneId::TONE_A4};

  // collectors
  std::vector<TickEvent> ticks;       // appended as observed
  std::vector<uint32_t> crcs;         // displayed-frame CRC pulses, in order
  std::vector<SweepEvent> sweeps;     // one per tick (40-beat window)
  std::vector<uint8_t> fences;        // fence pulses: (ok<<7)|status? no:
  struct Fence { uint8_t slot; bool ok; uint8_t status; };
  std::vector<Fence> fence_log;
  struct BlitDone { uint8_t status; };
  std::vector<BlitDone> blit_log;
  uint32_t crc_size_errs = 0;

  // in-flight sweep assembly
  SweepEvent cur_sweep;
  bool sweep_open = false;

  // bridge responder state
  bool hps_busy = false;
  uint32_t hps_addr = 0;
  uint8_t hps_len = 0;
  int hps_latency = 0;
  uint32_t hps_beat = 0;

  ShellHarness() : mem(8u << 20, 0) {}

  void reset(int cycles = 8) {
    top.rst_n = 0;
    top.gpu_clk = 0;
    top.vid_clk = 0;
    top.audio_clk = 0;
    for (int s = 0; s < 3; ++s) {
      top.hps_state_i[s] = 0;
      top.hps_byte_len_i[s] = 0;
    }
    top.ring_wr_ready_i = 1;
    top.hps_req_grant_i = 0;
    top.hps_rd_valid_i = 0;
    top.hps_rd_data_i = 0;
    top.hps_rd_last_i = 0;
    top.aud_wr_valid_i = 0;
    top.aud_wr_l_i = 0;
    top.aud_wr_r_i = 0;
    top.cnt_snap_ready_i = 1;
    for (int p = 0; p < 4; ++p) {
      top.pad_buttons_i[p] = 0;
      top.pad_lx_i[p] = 0;
      top.pad_ly_i[p] = 0;
      top.pad_rx_i[p] = 0;
      top.pad_ry_i[p] = 0;
    }
    top.pad_present_i = 0;
    top.peek_en = 0;
    top.peek_waddr = 0;
    top.eval();
    for (int i = 0; i < cycles; ++i) {
      // clock all domains under reset
      top.gpu_clk = 1;
      top.vid_clk = (i & 1) ? 1 : 0;
      top.audio_clk = ((i & 3) == 3) ? 1 : 0;
      top.eval();
      top.gpu_clk = 0;
      top.vid_clk = 0;
      top.audio_clk = 0;
      top.eval();
    }
    top.rst_n = 1;
    top.eval();
    n = 0;
  }

  // ---- HPS memory helpers -------------------------------------------------
  void mem_write(uint32_t addr, const uint8_t* p, size_t len) {
    if (addr + len > mem.size()) mem.resize(addr + len + 4096, 0);
    std::memcpy(mem.data() + addr, p, len);
  }
  void mem_write(uint32_t addr, const std::vector<uint8_t>& v) {
    mem_write(addr, v.data(), v.size());
  }

  // publish a sealed packet into ring slot s (FREE -> ARM_WRITING -> READY;
  // the ARM state shows for exactly one observed cycle)
  bool publish(int s, const std::vector<uint8_t>& pkt) {
    if (ring_state[s] != 0) return false;   // not FREE: protocol misuse
    mem_write(kSlotBody0 + uint32_t(s) * zhao_abi::FRAME_SLOT_BYTES, pkt);
    ring_len[s] = uint32_t(pkt.size());
    publish_arm[s] = 2;   // 2 cycles of ARM_WRITING, then READY
    return true;
  }

  // ---- one gpu step -------------------------------------------------------
  // Post-step, the collectors hold everything observed so far; tick_seen()
  // reports a tick observed in the LAST step (the scenario pumps on it).
  bool tick_seen_last_step = false;

  void step() {
    tick_seen_last_step = false;

    // ---- present inputs for this cycle ------------------------------------
    for (int s = 0; s < 3; ++s) {
      uint8_t st = ring_state[s];
      if (publish_arm[s] > 0) st = 1;                 // ARM_WRITING
      top.hps_state_i[s] = st;
      top.hps_byte_len_i[s] = ring_len[s];
    }
    uint8_t presentm = 0;
    for (int p = 0; p < 4; ++p) {
      if (pad_present[p]) presentm |= uint8_t(1u << p);
      top.pad_buttons_i[p] = pad_buttons[p];
      top.pad_lx_i[p] = uint16_t(pad_lx[p]);
      top.pad_ly_i[p] = uint16_t(pad_ly[p]);
      top.pad_rx_i[p] = uint16_t(pad_rx[p]);
      top.pad_ry_i[p] = uint16_t(pad_ry[p]);
    }
    top.pad_present_i = presentm;

    // audio feed: prime back-to-back, then steady 1 pair / audio_period
    top.aud_wr_valid_i = 0;
    if (audio_enable) {
      const bool due = (audio_fed < audio_prime) || (n >= audio_next_at);
      if (due) {
        top.eval();
        if (top.aud_wr_ready_o) {
          const zref::AudioPair pr = tone.tick();
          top.aud_wr_valid_i = 1;
          top.aud_wr_l_i = uint16_t(pr.l);
          top.aud_wr_r_i = uint16_t(pr.r);
          aud_ring[aud_sent_count & (kAudRing - 1)] =
              (uint32_t(uint16_t(pr.l)) << 16) | uint16_t(pr.r);
          ++aud_sent_count;
          ++audio_fed;
          if (audio_fed >= audio_prime) audio_next_at = n + audio_period;
        }
      }
    }

    // ---- bridge responder (D10 profile) -----------------------------------
    top.eval();
    top.hps_req_grant_i = 0;
    top.hps_rd_valid_i = 0;
    top.hps_rd_last_i = 0;
    if (!hps_busy) {
      if (top.hps_req_valid_o) {
        top.hps_req_grant_i = 1;
        hps_busy = true;
        hps_addr = top.hps_req_addr_o;
        hps_len = uint8_t(top.hps_req_len_o);
        hps_latency = kFirstBeatLatency;
        hps_beat = 0;
        // Phase 2 issues only reads through the bridge
      }
    } else if (hps_latency > 0) {
      --hps_latency;
    } else {
      const uint32_t off = hps_addr + hps_beat * 8;
      uint64_t data = 0;
      for (int b = 7; b >= 0; --b) {
        const uint32_t a = off + uint32_t(b);
        data = (data << 8) | (a < mem.size() ? mem[a] : 0);
      }
      const bool last = (hps_beat + 1) * 8 >= hps_len;
      top.hps_rd_valid_i = 1;
      top.hps_rd_data_i = data;
      top.hps_rd_last_i = last ? 1 : 0;
      ++hps_beat;
      if (last) hps_busy = false;
    }

    // ---- ring write-back (FPGA-owned state transitions) -------------------
    top.eval();
    if (top.ring_wr_valid_o) {
      ring_state[top.ring_wr_slot_o] = uint8_t(top.ring_wr_state_o);
    }

    // ---- clock edges (frozen phases) --------------------------------------
    ++n;
    top.gpu_clk = 0;
    top.eval();
    if (n & 1) {
      top.gpu_clk = 1;
      top.vid_clk = 1;
      top.eval();
    } else {
      top.gpu_clk = 1;
      top.vid_clk = 0;
      top.eval();
    }
    top.gpu_clk = 0;
    top.eval();
    bool audio_fired = false;
    if ((n & 3) == 0) {
      top.audio_clk = 1;
      top.eval();
      top.audio_clk = 0;
      top.eval();
      audio_fired = true;
    }

    // publish sequencing advances one observed cycle at a time
    for (int s = 0; s < 3; ++s) {
      if (publish_arm[s] > 0) {
        if (--publish_arm[s] == 0) ring_state[s] = 2;   // READY
      }
    }

    // ---- post-edge observations -------------------------------------------
    if (top.gpu_tick_o) {
      ticks.push_back(TickEvent{top.gpu_tick_frame_id_o,
                                top.gpu_tick_repeated_o != 0});
      tick_seen_last_step = true;
      // a tick (re)starts the counters sweep
      if (sweep_open && !cur_sweep.bank.empty()) sweeps.push_back(cur_sweep);
      cur_sweep = SweepEvent{};
      sweep_open = true;
    }
    if (sweep_open && top.cnt_snap_valid_o) {
      // ready is tied 1: one beat per cycle, ascending ids
      cur_sweep.bank.push_back(top.cnt_snap_value_o);
      if (cur_sweep.bank.size() >= 40) {
        sweeps.push_back(cur_sweep);
        cur_sweep = SweepEvent{};
        sweep_open = false;
      }
    }
    if (top.crc_valid_o) {
      crcs.push_back(top.crc_frame_o);
      if (top.crc_size_err_o) ++crc_size_errs;
    }
    if (top.fence_valid_o) {
      fence_log.push_back(Fence{uint8_t(top.fence_slot_o),
                                top.fence_ok_o != 0,
                                uint8_t(top.fence_status_o)});
    }
    if (top.blit_done_o) {
      blit_log.push_back(BlitDone{uint8_t(top.blit_status_o)});
    }
    if (audio_fired && top.pcm_valid_o) {
      const uint32_t got = (uint32_t(top.pcm_l_o) << 16) | top.pcm_r_o;
      if (pcm_pop_count >= aud_sent_count ||
          aud_sent_count - pcm_pop_count > kAudRing) {
        ++aud_ring_overruns;
      } else if (aud_ring[pcm_pop_count & (kAudRing - 1)] != got) {
        ++pcm_mismatches;
      }
      ++pcm_pop_count;
    }
  }

  // run until the next tick (bounded); returns false on timeout
  bool run_to_tick(uint64_t max_steps = 2'000'000) {
    for (uint64_t i = 0; i < max_steps; ++i) {
      step();
      if (tick_seen_last_step) return true;
    }
    return false;
  }

  // sticky-integrity snapshot (all must be zero for a lawful run)
  uint64_t sticky_errors() const {
    uint64_t e = 0;
    e |= top.shell_err_wfifo_o;
    e |= uint64_t(top.shell_err_route_o) << 1;
    e |= uint64_t(top.shell_err_cdc_o) << 2;
    e |= uint64_t(top.shell_err_framer_o) << 3;
    e |= uint64_t(top.model_error) << 4;
    e |= uint64_t(top.scaler_violation_o) << 5;
    e |= uint64_t(top.cnt_cat_violation_o) << 6;
    e |= uint64_t(top.guard_violations_o != 0) << 7;
    e |= uint64_t(top.hps_err_count_o != 0) << 8;
    e |= uint64_t(top.crc_size_err_o) << 9;
    return e;
  }
};

// ============================ frame content law =============================
// The Duo marker demo law (plan W2.7):
//   pos += clamp(analog >> 12, -8..+8) px/frame, wall-clamped to the view;
//   8x8 marker pattern; P1 = view 0 (left), P2 = view 1 (right).
// The plan sketch placed "48 interface lines = checker + frame counter" in
// the border rows — the RATIFIED spec (video_rules.md 3.1) makes those rows
// hardware-black at scanout, so the interface strip lives in the TOP 8 ROWS
// OF EACH VIEW instead (checker + frame-counter bits). Deviation recorded
// in reports/status/phase2_wave2.md.

struct MarkerState {
  int x = 0, y = 0;
};

inline int clamp_step(int16_t analog) {
  int d = analog >> 12;             // arithmetic: [-8, 7]
  if (d > 8) d = 8;
  if (d < -8) d = -8;
  return d;
}

inline void marker_move(MarkerState& m, int16_t ax, int16_t ay) {
  m.x += clamp_step(ax);
  m.y += clamp_step(ay);
  if (m.x < 0) m.x = 0;
  if (m.x > 256 - 8) m.x = 256 - 8;
  if (m.y < 0) m.y = 0;
  if (m.y > 192 - 8) m.y = 192 - 8;
}

// one 256x192 view canvas (RGB565 LE) into out[0x18000]
inline void compose_view(uint8_t* out, uint32_t frame, int view,
                         const MarkerState& m) {
  const uint16_t bg0 = view ? 0x2104 : 0x1082;   // dark checker tones
  const uint16_t bg1 = view ? 0x3186 : 0x2945;
  const uint16_t mk = view ? 0xF800 : 0x07E0;    // P2 red, P1 green
  for (int y = 0; y < 192; ++y) {
    for (int x = 0; x < 256; ++x) {
      uint16_t c;
      if (y < 8) {
        // interface strip: frame-counter bits (32 px per bit block) over
        // a fine checker
        const int bit = (frame >> (7 - (x >> 5))) & 1u;
        c = bit ? uint16_t(0xFFFF) : uint16_t(((x ^ y) & 1) ? 0x4208 : 0x0000);
      } else {
        c = (((x >> 4) ^ (y >> 4)) & 1) ? bg1 : bg0;
      }
      if (x >= m.x && x < m.x + 8 && y >= m.y && y < m.y + 8) {
        // 8x8 marker pattern: ring with an X
        const int px = x - m.x, py = y - m.y;
        const bool edge = (px == 0 || px == 7 || py == 0 || py == 7);
        const bool diag = (px == py) || (px + py == 7);
        if (edge || diag) c = mk;
      }
      const size_t off = (size_t(y) * 256 + size_t(x)) * 2;
      out[off] = uint8_t(c & 0xFF);
      out[off + 1] = uint8_t(c >> 8);
    }
  }
}

// the full Duo frame occupancy (two packed view blocks, 0x30000 B)
inline void compose_duo_frame(std::vector<uint8_t>& canvas, uint32_t frame,
                              const MarkerState& p1, const MarkerState& p2) {
  canvas.assign(0x30000, 0);
  compose_view(canvas.data(), frame, 0, p1);
  compose_view(canvas.data() + 0x18000, frame, 1, p2);
}

// deterministic full-canvas test pattern for the per-mode goldens
inline void compose_pattern(std::vector<uint8_t>& canvas, zhao_abi::video_mode mode,
                            uint32_t frame) {
  const uint32_t bytes = zref::render::canvas_bytes(mode);
  canvas.assign(bytes, 0);
  for (uint32_t i = 0; i < bytes / 2; ++i) {
    const uint16_t c = uint16_t((i * 31u + frame * 7919u) ^ (i >> 7));
    canvas[2 * i] = uint8_t(c & 0xFF);
    canvas[2 * i + 1] = uint8_t(c >> 8);
  }
}

// ============================ packet builders ==============================

struct PacketSpec {
  uint32_t frame_id = 0;
  uint32_t sequence = 0;
  uint8_t mode = 2;              // video_mode byte
  bool has_blit = false;
  uint8_t blit_dst = 0;
  uint32_t blit_src = 0;
  uint32_t blit_len = 0;
  uint32_t blit_crc = 0;
  bool has_rumble = false;
  uint8_t rumble_pad = 0, rumble_en = 0, rumble_str = 0;
};

inline std::vector<uint8_t> build_packet(const PacketSpec& s) {
  zhao::ZhaoFrameBuilder b;
  b.begin_frame(s.frame_id, /*resource_epoch=*/0, /*flags=*/0,
                /*deadline_cycles=*/0);
  {
    zhao_abi::ZhRecordSetPresentationContract r{};
    r.hdr.opcode = zhao_abi::ZHAO_OP_SET_PRESENTATION_CONTRACT;
    r.hdr.record_bytes = 48;
    r.payload.mode = zhao_abi::video_mode(s.mode);
    r.payload.view_count = (s.mode == 2) ? 2 : 1;
    std::vector<uint8_t> bytes;
    zhao_abi::zhao_pack_set_presentation_contract(r, bytes);
    b.append_record(bytes);
  }
  if (s.has_rumble) {
    zhao_abi::ZhRecordDebugRumble r{};
    r.hdr.opcode = zhao_abi::ZHAO_OP_DEBUG_RUMBLE;
    r.hdr.record_bytes = 32;
    r.payload.pad_index = s.rumble_pad;
    r.payload.enable = s.rumble_en;
    r.payload.strength = s.rumble_str;
    std::vector<uint8_t> bytes;
    zhao_abi::zhao_pack_debug_rumble(r, bytes);
    b.append_record(bytes);
  }
  if (s.has_blit) {
    zhao_abi::ZhRecordDebugFrameBlit r{};
    r.hdr.opcode = zhao_abi::ZHAO_OP_DEBUG_FRAME_BLIT;
    r.hdr.record_bytes = 48;
    r.payload.dst_slot = s.blit_dst;
    r.payload.mode = zhao_abi::video_mode(s.mode);
    r.payload.src_addr_hps = s.blit_src;
    r.payload.byte_len = s.blit_len;
    r.payload.expected_crc32c = s.blit_crc;
    std::vector<uint8_t> bytes;
    zhao_abi::zhao_pack_debug_frame_blit(r, bytes);
    b.append_record(bytes);
  }
  b.end_frame(/*completion_flags=*/0);
  // debug umbrella flag (header flags bit0) required for the debug records
  const uint16_t flags = (s.has_blit || s.has_rumble) ? 0x0001 : 0x0000;
  return b.seal(s.frame_id, s.sequence, /*resource_epoch=*/0,
                /*deadline_cycles=*/0, flags);
}

}  // namespace zhao_shell
