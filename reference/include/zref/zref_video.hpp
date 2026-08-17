// zref_video.hpp — ZRef oracle for the W2.2 VIDEO subsystem
// (plan W2.2; law cited per class).
//
//   zref::VideoMode   per-cycle timing trace oracle      (spec/video_rules.md
//                     §1-§2: modes, tables, raster phase, mode latch law)
//   zref::FrameCtl    swap/repeat/tick/fence decision     (spec/video_rules.md
//                     oracle — commit law, deadline window, 60 Hz law §4-§5)
//   zref::Scanout     full scanout pipeline mirror        (spec/video_rules.md
//                     §3-§4: fetch order, ping-pong line buffers, serializer,
//                     swap execution, starvation law)
//   zref::ScalerFeed  pass-through identity, 2-cycle delay (spec §6)
//   zref::VramResponder  deterministic guard/memory responder (harness side:
//                     fixed-latency admission + 8-beat bursts + injectable
//                     starvation; the frozen sim profile, memory_rules.md §1)
//   zref::frame_pixel_crc  displayed-stream CRC-32C over a canvas — a thin
//                     vector adapter DELEGATING to the one composer,
//                     zref::render::displayed_crc32c (charter §29-6: the
//                     branch's own re-implementation was removed at the
//                     merge; the law is implemented exactly once)
//
// The mirror classes reproduce the RTL register semantics EXACTLY (the RTL
// is fpga/rtl/video/*.sv; every mirror field names its RTL counterpart).
// The differential tests step RTL and oracle on the SAME unified gpu-cycle
// timeline (vid posedges on odd gpu steps — the frozen 2:1 phase, plan
// D1/R1), present identical stimulus, and compare every observable every
// cycle. A mismatch is a law violation on one of the two sides — never
// silence it (charter §29-17: save the minimal failing vector).
//
// Determinism: no floats, no host-dependent types; all state is explicit.

#pragma once

#include "zhao_abi.h"  // generated: zhao_crc32c

#include <cstdint>
#include <vector>

namespace zref {

// ------------------------------------------------------------ constants ---
// The timing table exists exactly once in RTL (zhao_pkg.sv ZHAO_TIMING);
// this mirror is the reference-side reading of spec/video_rules.md §2
// (verified against the package by tests/video/video_mode_directed.cpp).

struct VidTiming {
  uint32_t h_active, h_front, h_sync, h_back, h_total;
  uint32_t v_active, v_front, v_sync, v_back, v_total;
  uint32_t frame_gpu_cycles;
};

const VidTiming& vid_timing(uint32_t mode);  // mode 0/1/2

// Bytes a frame OCCUPIES per mode (spec/video_rules.md §1 "Allocation is
// not occupancy": Duo stores 0x30000 packed view bytes of its 0x3C000
// slot). Delegates to zref::render::canvas_bytes — the one C++ definition.
uint32_t canvas_bytes(uint32_t mode);
uint32_t active_width(uint32_t mode);

// displayed-stream composition + CRC (spec §4 Displayed-CRC law):
// Z60/Storm: the canvas bytes as-is; Duo: 512-wide raster AS SCANNED OUT —
// 48 border rows black, each row 24..215 the concatenation of view-0's and
// view-1's row from the two contiguous stored blocks (spec §3.1).
// DELEGATES to zref::render::displayed_crc32c (charter §29-6).
uint32_t frame_pixel_crc(uint32_t mode, const std::vector<uint8_t>& canvas);

// per-mode line geometry used by fetch and the frame composer
bool duo_border_line(uint32_t display_y);     // true for the 48 border rows
uint32_t duo_source_row(uint32_t display_y);  // display y -> view row (>=24)

// ------------------------------------------------------------- VideoMode ---
struct RasterView {
  uint32_t x = 0, y = 0;
  bool hsync = false, vsync = false, hblank = false, vblank = false;
  bool frame_start = false, frame_end = false, vswap_dec = false;
  uint32_t mode = 0, mode_next = 0;
};

/** Cycle-exact mirror of zhao_video_mode.sv (spec/video_rules.md §1-§2). */
class VideoMode {
 public:
  void reset();

  // stimulus latched at the next vid edge (mode_we port)
  void mode_we(uint32_t value, bool valid);

  // one vid edge (caller drives the edge cadence)
  void step();

  // post-edge combinational decodes
  RasterView view() const;

  uint32_t mode_cur() const { return mode_cur_; }
  uint32_t mode_pend() const { return mode_pend_; }

 private:
  uint32_t x_ = 0, y_ = 0;
  uint32_t mode_cur_ = 0, mode_pend_ = 0;
  uint32_t we_value_ = 0;
  bool we_valid_ = false;
};

// -------------------------------------------------------------- FrameCtl ---
/** Inputs sampled by zref::FrameCtl at each vid edge. */
struct FrameCtlIn {
  bool vswap_dec = false;
  bool frame_start = false;
  uint32_t mode = 0;
  uint32_t slot_ready = 0;       // bit i = slot i READY level
  uint32_t deadline_cycles = 0;  // gpu cycles; 0 = mode frame period
};

/** Outputs of one FrameCtl vid cycle (registered) + the combinational swap. */
struct FrameCtlOut {
  // combinational during the cycle (pre-edge state + current vswap_dec)
  bool swap_req = false;
  uint32_t swap_slot = 0;
  // registered pulses/values visible the cycle AFTER the deciding edge
  bool frame_tick = false;
  bool frame_repeated = false;
  uint32_t frame_id = 0;
  uint64_t frame_cycles = 0;
  uint64_t deadline_faults = 0;
  uint32_t deadline_margin = 0;
};

/** Cycle-exact mirror of zhao_video_framectl.sv (spec/video_rules.md §4-§5). */
class FrameCtl {
 public:
  void reset();
  /** One vid edge. `in` is the stimulus DURING this cycle. After the call,
   *  out_comb(in) reflects the NEW state with the CURRENT inputs; the
   *  registered fields lag one cycle exactly like the RTL ports. */
  void step(const FrameCtlIn& in);

  FrameCtlOut out_comb(const FrameCtlIn& in) const;

  uint64_t deadline_faults() const { return deadline_faults_q_; }
  uint64_t frame_cycles() const { return frame_cycles_q_; }
  uint32_t frame_id() const { return frame_id_q_; }
  uint32_t display_slot() const { return cur_slot_; }
  bool tick_tog() const { return tick_tog_; }  // gpu-crossing source
  bool cur_repeated() const { return cur_repeated_; }

 private:
  bool committed_v_ = false;
  uint32_t committed_slot_ = 0;
  uint32_t deadline_left_ = 0;
  uint32_t frame_id_q_ = 0;
  uint64_t frame_cycles_q_ = 0;
  uint64_t deadline_faults_q_ = 0;
  bool tick_tog_ = false;
  uint32_t cur_slot_ = 0;
  bool cur_repeated_ = false;
  // registered outputs
  bool frame_tick_q_ = false, frame_repeated_q_ = false;
  uint32_t frame_id_out_ = 0, deadline_margin_q_ = 0;
};

// ------------------------------------------------------------ ScalerFeed ---
/** Identity with a 2-cycle delay and a consumer-ready freeze
 *  (spec/video_rules.md §6; mirror of zhao_video_scaler.sv). */
struct PxStream {
  bool valid = false;
  uint32_t rgb565 = 0;
  uint32_t x = 0, y = 0;
  bool hsync = false, vsync = false, hblank = false, vblank = false;
};

class ScalerFeed {
 public:
  void reset();
  void step(const PxStream& in, bool out_ready);
  PxStream out() const { return stage2_; }
  bool never_active() const { return violation_q_; }

 private:
  PxStream stage1_, stage2_;
  bool violation_q_ = false;
};

// --------------------------------------------------------- VramResponder ---
/** Deterministic guard/memory responder (harness side, shared stimulus for
 *  RTL and oracle). Sim profile: admission when idle + service enabled;
 *  `latency` gpu cycles request->first beat, 1 beat/cycle, 8 beats of 64 b
 *  (4 RGB565 px, little-endian halfwords). set_service(false) injects
 *  starvation (never a torn answer — just none). */
class VramResponder {
 public:
  explicit VramResponder(uint32_t latency = 4) : latency_(latency) {}

  void reset();
  void set_canvas(uint32_t slot, const std::vector<uint8_t>& bytes);

  // starvation injection (level; applies from the next step)
  void set_service(bool enabled) { service_ = enabled; }

  struct Out {
    bool ready = false, ok = false, violation = false;
    bool beat_valid = false;
    uint64_t beat_data = 0;
  };

  /** One gpu cycle with the request CURRENTLY on the wires. */
  Out step(bool req_valid, uint32_t req_addr, uint32_t req_len);

 private:
  uint32_t latency_;
  bool service_ = true;
  std::vector<uint8_t> vram_;  // full 2-slot span (0x78000)
  // in-flight burst
  bool busy_ = false;
  uint32_t beat_countdown_ = 0;  // cycles until the next beat
  uint32_t beats_left_ = 0;
  uint32_t burst_base_ = 0;  // accepted request byte address
};

// ---------------------------------------------------------------- Scanout ---
// The full subsystem mirror (mode + framectl + scanout + scaler + the CDC
// fabric of zhao_video_tb): step() is ONE GPU CYCLE; vid edges fire on odd
// steps (the frozen 2:1 phase). Every field names its RTL counterpart in
// fpga/rtl/video/*.sv — the class is the contract-faithful oracle for the
// random differentials (fetch order + displayed stream + swap/repeat).
// Named `Scanout` because that is the symbol design/blocks.yml and the
// VIDEO.SCANOUT contract cite (the branch's `VideoSys` name would have
// been a phantom citation; `VideoSys` remains as an alias below).

struct VideoSysIn {
  bool mode_we = false;
  uint32_t mode_in = 0;
  uint32_t slot_ready = 0;
  uint32_t deadline_cycles = 0;
  // responder outputs (identical for RTL and oracle)
  bool guard_ready = false, guard_ok = false, guard_violation = false;
  bool beat_valid = false;
  uint64_t beat_data = 0;
  bool px_out_ready = true;
};

struct VideoSysOut {
  RasterView raster;
  // fetch client (gpu)
  bool req_valid = false, req_write = false;
  uint32_t req_addr = 0, req_len = 0;
  // pixel stream after the scaler (the displayed stream)
  PxStream px;
  bool scaler_violation = false;
  // framectl (vid) + its gpu broadcast
  bool frame_tick = false, frame_repeated = false, swap_req = false, swap_ack = false;
  uint32_t swap_slot = 0, frame_id = 0, deadline_margin = 0;
  uint64_t deadline_faults = 0, frame_cycles = 0;
  bool gpu_tick = false;
  uint32_t gpu_tick_frame_id = 0;
  bool gpu_tick_repeated = false;
  uint32_t gpu_complete_slot = 0;
  uint64_t starvation = 0;
};

class Scanout {
 public:
  void reset();

  /** One gpu cycle (vid posedge on odd steps — mirrors the harness law). */
  void step(const VideoSysIn& in);

  VideoSysOut out() const;

  // sub-oracle views (for the directed mode/framectl tests)
  const VideoMode& mode() const { return mode_; }
  const FrameCtl& framectl() const { return framectl_; }
  const PxStream& ser() const { return ser_px_; }  // serializer level

  // debug observability (differential bring-up only)
  uint32_t dbg_state() const { return f_state_; }
  uint32_t dbg_line() const { return fetch_line_; }
  uint32_t dbg_bstate(uint32_t i) const { return bstate_[i]; }
  uint32_t dbg_fillbuf() const { return fill_line_buf_; }
  bool dbg_dec_sync() const { return dec_sync_; }
  bool dbg_fs_sync() const { return fs_sync_; }

  uint64_t vid_steps() const { return vid_steps_; }

 private:
  void vid_step_(const VideoSysIn& in, bool s_full_tog0, bool s_full_tog1);
  void gpu_step_(const VideoSysIn& in, bool s_dec_tog, bool s_fs_tog, uint32_t s_slot,
                 uint32_t s_mnext, uint32_t s_mode, bool s_cons_tog0, bool s_cons_tog1, bool s_ftog,
                 uint32_t s_fslot, bool s_frep, uint32_t s_ffid);
  PxStream decode_px_() const;             // serializer stream decode
  uint32_t lane_pixel_(uint32_t x) const;  // 16-bit lane of the read word

  uint64_t gpu_steps_ = 0, vid_steps_ = 0;

  // ---- VIDEO.MODE (vid) ----
  VideoMode mode_;
  // raster view latched at the last vid edge (combinational decodes)
  RasterView raster_;

  // ---- VIDEO.FRAMECTL (vid + gpu crossing) ----
  FrameCtl framectl_;
  FrameCtlIn fctl_in_;        // stimulus during the current vid cycle
  FrameCtlOut fctl_out_reg_;  // registered view

  // ---- VIDEO.SCANOUT (zhao_video_scanout.sv) ----
  // vid domain
  uint32_t display_slot_ = 0;
  bool swap_ack_ = false;
  bool dec_tog_ = false, fs_tog_ = false;
  // gpu domain crossings
  bool dec_s1_ = false, dec_s2_ = false, dec_s2q_ = false, dec_sync_ = false;
  bool fs_s1_ = false, fs_s2_ = false, fs_s2q_ = false, fs_sync_ = false;
  uint32_t slot_s1_ = 0, slot_s2_ = 0;
  uint32_t mnext_s1_ = 0, mnext_s2_ = 0, mode_s1_ = 0, mode_s2_ = 0;

  // fetch FSM (gpu)
  uint32_t f_state_ = 0;
  uint32_t fetch_mode_ = 0, fetch_slot_ = 0, fetch_line_ = 0;
  uint32_t seg_idx_ = 0, req_idx_ = 0, beat_cnt_ = 0, fill_words_ = 0;
  uint32_t fill_line_buf_ = 0;

  // linebuf (gpu + vid)
  uint32_t bstate_[2] = {0, 0};  // 0 EMPTY / 1 FILLING / 2 FULL
  bool full_toggle_[2] = {false, false};
  bool cons_s1_[2] = {false, false};
  bool cons_s2_[2] = {false, false};
  bool cons_s2q_[2] = {false, false};
  bool consumed_toggle_[2] = {false, false};
  bool full_s1_[2] = {false, false};
  bool full_s2_[2] = {false, false};
  bool last_seen_[2] = {false, false};
  uint64_t mem_[2][128];

  // serializer (vid)
  bool display_buf_ = false, line_fresh_ = false;
  uint32_t last_px_ = 0;
  uint64_t starve_q_ = 0;
  bool consume_start_[2] = {false, false};
  bool consume_done_[2] = {false, false};

  // scaler (vid)
  ScalerFeed scaler_;
  PxStream ser_px_;  // serializer stream during the cycle

  // FRAMECTL gpu broadcast (tog + 2FF + edge; zhao_video_framectl.sv)
  bool f_tog_s1_ = false, f_tog_s2_ = false, f_tog_s2q_ = false;
  uint32_t f_slot_s1_ = 0, f_slot_s2_ = 0;
  bool gpu_tick_q_ = false;
  uint32_t gpu_tick_frame_id_q_ = 0;
  bool gpu_tick_repeated_q_ = false;
  uint32_t gpu_complete_slot_q_ = 0;
};

using VideoSys = Scanout;  // transitional alias (harness code)

}  // namespace zref
