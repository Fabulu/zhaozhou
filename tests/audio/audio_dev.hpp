// audio_dev.hpp — W2.4 shared test driver for zhao_audio_fifo.
//
// One generic runner drives BOTH devices through the SAME protocol —
//   for gpu cycle c = 0, 1, 2, ...: set inputs, gpu rising edge, then (when
//   (c+1) % 4 == 0) audio rising edge (gpu edge first, plan R1 seam:
//   audio_clk = gpu_clk/4, the spec-defined sim ratio) —
// so the differential tests compare real cycle behaviour, never a re-timed
// abstraction. The oracle side (zref::AudioFifo) implements the identical
// protocol natively.
//
// Law: spec/audio_rules.md (§2 D4 geometry), tests -> contracts
// AUDIO.FIFO.md "Directed tests"/"Randomized differential tests".

#pragma once

#include "Vzhao_audio_fifo.h"
#include "verilated.h"

#include "zref/zref_audio.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace zhao_audio {

// ------------------------------------------------------------ devices ------

// Verilated zhao_audio_fifo wrapper.
class RtlDev {
 public:
  RtlDev() : top_(new Vzhao_audio_fifo) { reset(); }
  ~RtlDev() { top_->final(); delete top_; }

  void reset() {
    top_->rst_gpu_n = 0;
    top_->rst_audio_n = 0;
    top_->wr_valid_i = 0;
    top_->wr_l_i = 0;
    top_->wr_r_i = 0;
    top_->frame_tick_i = 0;
    top_->clk_gpu = 0;
    top_->clk_audio = 0;
    top_->eval();
    for (int i = 0; i < 2; ++i) {  // both domains clocked under reset
      gpu_edge();
      audio_edge();
    }
    top_->rst_gpu_n = 1;
    top_->rst_audio_n = 1;
    top_->eval();
    cycle_ = 0;
    audio_edge_fired = false;
  }

  // One gpu cycle (gpu edge, then the audio edge when due). Inputs are
  // sampled at the gpu rising edge like real hardware. Returns whether the
  // pair write was accepted.
  bool cycle(bool wr_valid, uint16_t l, uint16_t r, bool frame_tick) {
    top_->wr_valid_i = wr_valid ? 1 : 0;
    top_->wr_l_i = l;
    top_->wr_r_i = r;
    top_->frame_tick_i = frame_tick ? 1 : 0;
    top_->eval();  // combinational settle: wr_ready_o is valid pre-edge
    const bool accept = wr_valid && top_->wr_ready_o;
    gpu_edge();
    audio_edge_fired = false;
    ++cycle_;
    if ((cycle_ % 4) == 0) {
      audio_edge();
      audio_edge_fired = true;
    }
    pcm_valid = top_->pcm_valid_o != 0;
    pcm_l = static_cast<uint16_t>(top_->pcm_l_o);
    pcm_r = static_cast<uint16_t>(top_->pcm_r_o);
    underrun_status = top_->underrun_status_o != 0;
    return accept;
  }

  // gpu-domain views (post-edge)
  uint32_t occupancy() const { return top_->occupancy_o; }
  bool     wr_ready() const { return top_->wr_ready_o != 0; }
  bool     refill_req() const { return top_->refill_req_o != 0; }
  uint32_t underruns() const { return top_->audio_underruns_o; }

  // counter snapshot (zhao_counter_snap_t: valid MSB, id, u64 value LE)
  bool     snap_valid() const { return top_->cnt_snap_o[2] & 0x10000; }
  uint16_t snap_id() const { return static_cast<uint16_t>(top_->cnt_snap_o[2]); }
  uint64_t snap_value() const {
    return static_cast<uint64_t>(top_->cnt_snap_o[0]) |
           (static_cast<uint64_t>(top_->cnt_snap_o[1]) << 32);
  }

  // audio-tick outputs (post-edge; stable until the next audio edge)
  bool audio_edge_fired;
  bool pcm_valid;
  uint16_t pcm_l = 0;
  uint16_t pcm_r = 0;
  bool underrun_status;

 private:
  void gpu_edge() {
    top_->clk_gpu = 1;
    top_->eval();
    top_->clk_gpu = 0;
    top_->eval();
  }
  void audio_edge() {
    top_->clk_audio = 1;
    top_->eval();
    top_->clk_audio = 0;
    top_->eval();
  }

  Vzhao_audio_fifo* top_;
  uint64_t cycle_;
};

// zref::AudioFifo wrapper with the identical device interface.
class OracleDev {
 public:
  void reset() { f.reset(); }
  bool cycle(bool wr_valid, uint16_t l, uint16_t r, bool frame_tick) {
    const bool accept = f.gpu_cycle(wr_valid, l, r, frame_tick);
    audio_edge_fired = f.audio_edge_fired();
    pcm_valid = f.pcm_valid();
    pcm_l = f.pcm_l();
    pcm_r = f.pcm_r();
    underrun_status = f.underrun_status();
    return accept;
  }
  uint32_t occupancy() const { return f.occupancy(); }
  bool     wr_ready() const { return f.wr_ready(); }
  bool     refill_req() const { return f.refill_req(); }
  uint32_t underruns() const { return f.audio_underruns(); }
  bool     snap_valid() const { return true; }  // shadow latched this cycle
  uint16_t snap_id() const { return 31; }       // ZHAO_CNT_AUDIO_UNDERRUNS
  uint64_t snap_value() const { return f.audio_underruns_shadow(); }

  bool audio_edge_fired;
  bool pcm_valid;
  uint16_t pcm_l = 0;
  uint16_t pcm_r = 0;
  bool underrun_status;

  zref::AudioFifo f;
};

// ------------------------------------------------------------- schedule ----

struct Burst {
  uint32_t start_cycle;
  uint32_t len;  // pairs offered from start_cycle (held until accepted)
};

struct RunResult {
  struct Tick {
    bool valid;
    uint16_t l;
    uint16_t r;
    bool underrun;      // this tick repeated the last pair
    uint32_t underruns; // counter value after the tick
  };
  std::vector<Tick> stream;
  std::vector<uint32_t> occupancy;   // per gpu cycle (post-edge)
  std::vector<uint64_t> shadows;     // after each frame_tick
  uint64_t accepted = 0;
  uint64_t offered = 0;
  uint32_t underruns_final = 0;
};

// Deterministic pair source: pair #k = {l,r} (distinct, sign-covering).
inline void pair_k(uint64_t k, uint16_t* l, uint16_t* r) {
  *l = static_cast<uint16_t>(k * 7u + 11u);
  *r = static_cast<uint16_t>(~k * 13u + 5u);
}

// Drive `dev` for `cycles` gpu cycles under `bursts` (write credits) and
// `frame_ticks` (gpu cycles carrying a frame_tick pulse), recording the
// complete observable behaviour.
template <typename Dev>
RunResult run_schedule(Dev& dev, const std::vector<Burst>& bursts,
                       uint32_t cycles, const std::vector<uint32_t>& frame_ticks) {
  std::vector<int64_t> credit_delta(cycles + 1, 0);
  for (const Burst& b : bursts) {
    if (b.start_cycle < cycles) credit_delta[b.start_cycle] += b.len;
  }
  std::vector<uint32_t> ticks = frame_ticks;  // ascending
  size_t next_tick = 0;
  int64_t credits = 0;
  uint64_t pair_id = 0;
  RunResult res;
  for (uint32_t c = 0; c < cycles; ++c) {
    credits += credit_delta[c];
    const bool offer = credits > 0;
    uint16_t l, r;
    pair_k(pair_id, &l, &r);
    const bool ft = (next_tick < ticks.size() && ticks[next_tick] == c);
    const bool acc = dev.cycle(offer, l, r, ft);
    if (acc) {
      --credits;
      ++pair_id;
      ++res.accepted;
    }
    if (offer) ++res.offered;
    res.occupancy.push_back(dev.occupancy());
    if (ft) {
      ++next_tick;
      res.shadows.push_back(dev.snap_value());
    }
    if (dev.audio_edge_fired) {
      res.stream.push_back(RunResult::Tick{dev.pcm_valid, dev.pcm_l, dev.pcm_r,
                                           dev.underrun_status, dev.underruns()});
    }
  }
  res.underruns_final = dev.underruns();
  return res;
}

// Bit-exact comparison of two complete runs (stream, occupancy, shadows,
// counters). Returns true equal; on mismatch fills `where`.
inline bool results_equal(const RunResult& a, const RunResult& b,
                          std::string* where) {
  if (a.accepted != b.accepted) {
    *where = "accepted " + std::to_string(a.accepted) + " vs " +
             std::to_string(b.accepted);
    return false;
  }
  if (a.underruns_final != b.underruns_final) {
    *where = "underruns " + std::to_string(a.underruns_final) + " vs " +
             std::to_string(b.underruns_final);
    return false;
  }
  if (a.occupancy.size() != b.occupancy.size()) {
    *where = "occupancy trace length";
    return false;
  }
  for (size_t i = 0; i < a.occupancy.size(); ++i) {
    if (a.occupancy[i] != b.occupancy[i]) {
      *where = "occupancy@" + std::to_string(i) + ": " +
               std::to_string(a.occupancy[i]) + " vs " +
               std::to_string(b.occupancy[i]);
      return false;
    }
  }
  if (a.shadows.size() != b.shadows.size()) {
    *where = "shadow count";
    return false;
  }
  for (size_t i = 0; i < a.shadows.size(); ++i) {
    if (a.shadows[i] != b.shadows[i]) {
      *where = "shadow@" + std::to_string(i) + ": " +
               std::to_string(a.shadows[i]) + " vs " + std::to_string(b.shadows[i]);
      return false;
    }
  }
  if (a.stream.size() != b.stream.size()) {
    *where = "stream length " + std::to_string(a.stream.size()) + " vs " +
             std::to_string(b.stream.size());
    return false;
  }
  for (size_t i = 0; i < a.stream.size(); ++i) {
    const RunResult::Tick& ta = a.stream[i];
    const RunResult::Tick& tb = b.stream[i];
    if (ta.valid != tb.valid || ta.l != tb.l || ta.r != tb.r ||
        ta.underrun != tb.underrun || ta.underruns != tb.underruns) {
      *where = "stream tick " + std::to_string(i) + ": (" +
               std::to_string(ta.valid) + "," + std::to_string(ta.l) + "," +
               std::to_string(ta.r) + "," + std::to_string(ta.underrun) + "," +
               std::to_string(ta.underruns) + ") vs (" + std::to_string(tb.valid) +
               "," + std::to_string(tb.l) + "," + std::to_string(tb.r) + "," +
               std::to_string(tb.underrun) + "," + std::to_string(tb.underruns) + ")";
      return false;
    }
  }
  return true;
}

}  // namespace zhao_audio
