// zref_audio.hpp — W2.4 audio ZRef: the SW.MIXER tone subset + the
// AUDIO.FIFO oracle (plan W2.4).
//
// Law (in citation order):
//   spec/audio_rules.md  — §1 stream contract (48 kHz s16 stereo pairs,
//                          exactly 800 pairs/frame), §2 the FIFO law (D4:
//                          depth 2048 / refill 256 / watermark 512 / underrun
//                          = repeat last pair + audio_underruns / overflow
//                          structurally impossible), §4 the frozen tone
//                          table, §6 the documented gpu<->audio CDC
//   spec/qformats.md     — §2 angle16 (u16 turns), §7.1 fx_sin (the committed
//                          257-entry quarter-wave table; NO new constants
//                          here — every sample derives from gen::SIN_Q16)
//   spec/memory_rules.md — §4.2 PCM_RING descriptor (free-space law)
//   design/contracts/{AUDIO.FIFO,SW.MIXER}.md
//
// The AudioFifo oracle is a CYCLE-ACCURATE mirror of zhao_audio_fifo.sv at
// the documented sim seam (audio_clk = gpu_clk/4, one audio rising edge at
// the END of every 4th gpu cycle, gpu edge first — plan R1 fixed rational
// ratios). It models the two-domain pointer/gray-sync state EXACTLY so the
// differential tests can compare the full output stream bit-for-bit.

#pragma once

#include "zref/zref_fixp.hpp"
#include "zref/zref_trig.hpp"

#include <cstdint>
#include <vector>

namespace zref {

// ---------------------------------------------------------------- pairs ----

struct AudioPair {
  int16_t l;
  int16_t r;
};

// Exactly 800 pairs per displayed frame (48000/60, audio_rules.md §1) — the
// accounting law, shared by the tone frame() helper and the FIFO tests.
inline constexpr uint32_t kAudioPairsPerFrame = 800;

// ------------------------------------------------------------ §4 tones -----

enum class ToneId : uint8_t {
  TONE_A4 = 0,
  TONE_A5 = 1,
  TONE_C4 = 2,
};

// Frozen tone table (spec/audio_rules.md §4 — the ONLY legal Phase-2 tones).
// Increments are floor(freq * 2^32 / 48000), stated once in the spec and
// copied verbatim here; no new constants are derived anywhere.
struct ToneSpec {
  ToneId     id;
  const char* name;
  uint32_t   freq_hz;
  uint32_t   increment;  // per-48kHz-tick phase increment
};

constexpr ToneSpec kToneTable[3] = {
    {ToneId::TONE_A4, "TONE_A4", 440u, 39'370'533u},
    {ToneId::TONE_A5, "TONE_A5", 880u, 78'741'067u},
    {ToneId::TONE_C4, "TONE_C4", 262u, 23'443'363u},
};

constexpr uint32_t tone_increment(ToneId id) {
  return kToneTable[static_cast<uint8_t>(id)].increment;
}

// §4 sample law: s16 = fx_sin(angle16) >> 1 (arithmetic shift toward -inf),
// saturating to the s16 bounds. fx_sin peak is +0x10000 -> +0x8000 raw after
// the halving -> saturates to +0x7FFF; trough -0x10000 -> -0x8000 fits
// exactly. Exact integer path from the committed SIN_Q16 table.
int16_t tone_sample(angle16 phase_turns);

// -------------------------------------------------- MixerTone (SW.MIXER) ---

// The Phase-2 SW.MIXER subset (design/contracts/SW.MIXER.md): a deterministic
// 48 kHz test tone. Phase accumulator u32 wrapping mod 2^32 BY DEFINITION;
// both channels carry the identical sample (no pan); one pair per tick.
class MixerTone {
 public:
  explicit MixerTone(ToneId id) : inc_(tone_increment(id)), id_(id), phase_(0) {}

  void     reset(ToneId id);          // process start / tone selection
  void     select(ToneId id);         // switch tone, phase accumulator KEEPS running
  ToneId   id() const { return id_; }
  uint32_t increment() const { return inc_; }
  uint32_t phase() const { return phase_; }
  void     set_phase(uint32_t raw);   // oracle/test hook (saturation corners)

  // One 48 kHz tick: advances the accumulator, returns the pair.
  AudioPair tick();

  // Exactly 800 pairs (one displayed frame, audio_rules.md §1).
  std::vector<AudioPair> frame();

 private:
  uint32_t inc_;
  ToneId   id_;
  uint32_t phase_;
};

// --------------------------------------------- PCM ring (memory_rules 4.2) --

// The HPS-side PCM ring free-space law: the mixer (host) never overwrites
// unread pairs — host_write_ptr never passes fpga_read_ptr. Pointers are
// free-running u64 (no wrap in Phase 2); each side reads only the other's
// pointer. Test-scaled capacity (the RTL ring descriptor carries
// capacity_pairs; Phase-2 rings are sized by the runtime).
class PcmRing {
 public:
  explicit PcmRing(uint64_t capacity_pairs)
      : cap_(capacity_pairs), host_write_ptr_(0), fpga_read_ptr_(0),
        data_(static_cast<size_t>(capacity_pairs)) {}

  uint64_t capacity_pairs() const { return cap_; }
  uint64_t host_write_ptr() const { return host_write_ptr_; }
  uint64_t fpga_read_ptr() const { return fpga_read_ptr_; }

  // Pairs the FPGA can still consume (host ahead).
  uint64_t filled_pairs() const { return host_write_ptr_ - fpga_read_ptr_; }
  // Free space law: the mixer may only write this many before the FPGA reads.
  uint64_t free_pairs() const { return cap_ - filled_pairs(); }

  // Mixer write. Returns false (pair DROPPED, ring untouched) when the
  // free-space law would be violated — never overwrites an unread pair.
  bool write(AudioPair p);

  // FPGA read (advances fpga_read_ptr; the FPGA owns that word).
  AudioPair fpga_read();

 private:
  uint64_t cap_;
  uint64_t host_write_ptr_;
  uint64_t fpga_read_ptr_;
  std::vector<AudioPair> data_;
};

// ------------------------------------------------- AudioFifo (AUDIO.FIFO) --

// Cycle-accurate oracle of fpga/rtl/audio/zhao_audio_fifo.sv at the frozen
// D4 geometry. The driver protocol mirrors the Verilator harness EXACTLY:
//
//   for gpu cycle c = 0, 1, 2, ...:
//     gpu_cycle(wr_valid, l, r, frame_tick)   // one gpu rising edge, then —
//                                             // when (c+1)%div==0 — one
//                                             // audio rising edge
//
// Inside a cycle the gpu edge fires FIRST (harness drives clk_gpu, then
// clk_audio on every div-th cycle): the audio-domain synchroniser therefore
// samples the post-gpu-edge write pointer, exactly like the RTL.
class AudioFifo {
 public:
  // D4-frozen geometry (spec/audio_rules.md §2).
  static constexpr uint32_t kDepth        = 2048;  // stereo pairs
  static constexpr uint32_t kWatermark    = 512;   // refill request level
  static constexpr uint32_t kRefillBurst  = 256;   // pairs per refill
  static constexpr uint32_t kPairsPerFrame = kAudioPairsPerFrame;

  // Sim seam: gpu cycles per audio tick (audio_clk = gpu_clk/4; the real
  // board ratio arrives post-ZH-016 — the CDC design is ratio-agnostic, the
  // ORACLE is exact only for this fixed rational ratio, plan R1).
  static constexpr uint32_t kGpuCyclesPerAudioTick = 4;

  explicit AudioFifo(uint32_t gpu_cycles_per_audio_tick = kGpuCyclesPerAudioTick);

  void reset();

  // Advance one gpu cycle (both edges in the documented order). Returns
  // whether the pair write was accepted this cycle.
  bool gpu_cycle(bool wr_valid, uint16_t wr_l, uint16_t wr_r,
                 bool frame_tick = false);

  // ---- audio-tick outputs (post-edge values; stable until the next edge) --
  bool     audio_edge_fired() const { return audio_edge_fired_; }
  bool     pcm_valid() const { return pcm_valid_; }
  uint16_t pcm_l() const { return pcm_l_; }
  uint16_t pcm_r() const { return pcm_r_; }
  bool     underrun_status() const { return underrun_status_; }  // this tick repeated
  uint32_t audio_underruns() const { return underruns_; }        // live counter

  // ---- gpu-domain views (post-edge values) --------------------------------
  uint32_t occupancy() const { return occ_gpu_; }   // conservative (synced rd)
  bool     full() const { return occ_gpu_ == kDepth; }
  bool     wr_ready() const { return !full(); }
  bool     refill_req() const { return occ_gpu_ <= kWatermark; }
  uint64_t audio_underruns_shadow() const { return shadow_; }  // frame_tick latch

  // ---- accounting (§1: the 800-pair law is test-side) ---------------------
  uint64_t pairs_accepted() const { return accepted_; }
  uint64_t pairs_emitted() const { return emitted_; }

 private:
  uint32_t ptr_gray(uint32_t bin, uint32_t bits) const;
  uint32_t gray_ptr(uint32_t gray, uint32_t bits) const;

  uint32_t div_;       // gpu cycles per audio tick
  uint64_t cycle_;     // gpu cycles since reset

  // memory (gpu writes, audio reads — whole pairs, never torn)
  std::vector<AudioPair> mem_;

  // gpu-domain state
  uint32_t wr_ptr_;        // PTR_W bits (mod 2*kDepth)
  uint32_t rd_gray_meta_, rd_gray_sync_;   // rd pointer view (2FF)
  uint32_t cnt_gray_meta_, cnt_gray_sync_; // underruns view (2FF)
  uint32_t occ_gpu_;
  uint64_t shadow_;
  uint64_t accepted_;

  // audio-domain state
  uint32_t rd_ptr_;
  uint32_t wr_gray_meta_, wr_gray_sync_;   // wr pointer view (2FF)
  bool     started_;
  uint16_t pcm_l_, pcm_r_;
  bool     pcm_valid_, underrun_status_;
  uint32_t underruns_;
  uint64_t emitted_;
  bool     audio_edge_fired_;
};

}  // namespace zref
