// zref_audio.cpp — W2.4 audio ZRef implementation (law: spec/audio_rules.md,
// header zref_audio.hpp). The AudioFifo half is a register-exact mirror of
// fpga/rtl/audio/zhao_audio_fifo.sv: every non-blocking assignment of the
// RTL appears here in the same evaluation order (decision from pre-edge
// state, then updates), so the differential tests compare real cycle
// behaviour, not an abstraction of it.

#include "zref/zref_audio.hpp"

namespace zref {

// ---- MixerTone -------------------------------------------------------------

int16_t tone_sample(angle16 phase_turns) {
  const int32_t s = fx_sin(phase_turns).raw;  // ±0x10000 (qformats.md §7.1)
  const int32_t half = s >> 1;                // arithmetic shift toward -inf
  if (half > 0x7FFF) return 0x7FFF;           // +0x10000 -> +0x8000 -> saturate
  if (half < -0x8000) return static_cast<int16_t>(-0x8000);  // unreachable, law shape
  return static_cast<int16_t>(half);          // -0x10000 -> -0x8000 fits exactly
}

void MixerTone::reset(ToneId id) {
  id_ = id;
  inc_ = tone_increment(id);
  phase_ = 0;
}

void MixerTone::select(ToneId id) {
  id_ = id;
  inc_ = tone_increment(id);
  // phase accumulator keeps running (the mixer consumes timestamps, not
  // game state — charter §29-8; a tone switch is not a phase reset)
}

void MixerTone::set_phase(uint32_t raw) { phase_ = raw; }

AudioPair MixerTone::tick() {
  const angle16 top{static_cast<uint16_t>(phase_ >> 16)};
  const int16_t s = tone_sample(top);
  phase_ += inc_;  // wraps mod 2^32 BY DEFINITION (exact turns arithmetic)
  return AudioPair{s, s};
}

std::vector<AudioPair> MixerTone::frame() {
  std::vector<AudioPair> out;
  out.reserve(kAudioPairsPerFrame);
  for (uint32_t i = 0; i < kAudioPairsPerFrame; ++i) out.push_back(tick());
  return out;
}

// ---- PcmRing ---------------------------------------------------------------

bool PcmRing::write(AudioPair p) {
  if (free_pairs() == 0) return false;  // free-space law: never overwrite unread
  data_[static_cast<size_t>(host_write_ptr_ % cap_)] = p;
  ++host_write_ptr_;
  return true;
}

AudioPair PcmRing::fpga_read() {
  const AudioPair p = data_[static_cast<size_t>(fpga_read_ptr_ % cap_)];
  ++fpga_read_ptr_;  // FPGA-owned word only
  return p;
}

// ---- AudioFifo oracle ------------------------------------------------------

namespace {
constexpr uint32_t kPtrBits = 12;  // log2(2048) + 1 (wrap bit)
constexpr uint32_t kCntBits = 32;  // underrun counter width (saturating)
constexpr uint32_t kPtrMask = (1u << kPtrBits) - 1;
}  // namespace

AudioFifo::AudioFifo(uint32_t gpu_cycles_per_audio_tick)
    : div_(gpu_cycles_per_audio_tick) {
  reset();
}

void AudioFifo::reset() {
  cycle_ = 0;
  mem_.assign(kDepth, AudioPair{0, 0});
  wr_ptr_ = 0;
  rd_gray_meta_ = 0;
  rd_gray_sync_ = 0;
  cnt_gray_meta_ = 0;
  cnt_gray_sync_ = 0;
  occ_gpu_ = 0;
  shadow_ = 0;
  accepted_ = 0;
  rd_ptr_ = 0;
  wr_gray_meta_ = 0;
  wr_gray_sync_ = 0;
  started_ = false;
  pcm_l_ = 0;
  pcm_r_ = 0;
  pcm_valid_ = false;
  underrun_status_ = false;
  underruns_ = 0;
  emitted_ = 0;
  audio_edge_fired_ = false;
}

uint32_t AudioFifo::ptr_gray(uint32_t bin, uint32_t bits) const {
  bin &= (bits == 32) ? 0xFFFFFFFFu : ((1u << bits) - 1);
  return bin ^ (bin >> 1);
}

uint32_t AudioFifo::gray_ptr(uint32_t gray, uint32_t bits) const {
  uint32_t b = gray & ((bits == 32) ? 0xFFFFFFFFu : ((1u << bits) - 1));
  for (int32_t i = static_cast<int32_t>(bits) - 2; i >= 0; --i) {
    const uint32_t upper = (b >> (i + 1)) & 1u;
    const uint32_t gi = (gray >> i) & 1u;
    b = (b & ~(1u << i)) | ((upper ^ gi) << i);
  }
  return b;
}

bool AudioFifo::gpu_cycle(bool wr_valid, uint16_t wr_l, uint16_t wr_r,
                          bool frame_tick) {
  // ---- gpu-domain edge (inputs sampled at the rising edge) ----------------
  const bool accept = wr_valid && (occ_gpu_ != kDepth);  // wr_ready = !full
  if (accept) {
    mem_[wr_ptr_ & (kDepth - 1)] = AudioPair{static_cast<int16_t>(wr_l),
                                             static_cast<int16_t>(wr_r)};
    wr_ptr_ = (wr_ptr_ + 1) & kPtrMask;
    ++accepted_;
  }
  // 2FF gray sync of the audio-domain read pointer (samples CURRENT rd_ptr)
  rd_gray_sync_ = rd_gray_meta_;
  rd_gray_meta_ = ptr_gray(rd_ptr_, kPtrBits);
  // 2FF gray sync of the audio-domain underrun counter. frame_tick latches
  // the shadow from the PRE-EDGE sync register (the RTL reads und_gray_sync
  // with non-blocking semantics on the same edge), so capture it first.
  const uint32_t cnt_sync_pre = cnt_gray_sync_;
  cnt_gray_sync_ = cnt_gray_meta_;
  cnt_gray_meta_ = ptr_gray(underruns_, kCntBits);
  if (frame_tick) {
    shadow_ = gray_ptr(cnt_sync_pre, kCntBits);
  }
  occ_gpu_ = (wr_ptr_ - gray_ptr(rd_gray_sync_, kPtrBits)) & kPtrMask;

  // ---- audio-domain edge (end of every div_-th gpu cycle, gpu edge first) -
  audio_edge_fired_ = false;
  ++cycle_;
  if ((cycle_ % div_) == 0) {
    audio_edge_fired_ = true;
    // decision from PRE-EDGE state (RTL non-blocking semantics)
    const bool empty = (ptr_gray(rd_ptr_, kPtrBits) == wr_gray_sync_);
    if (!empty) {
      const AudioPair p = mem_[rd_ptr_ & (kDepth - 1)];
      pcm_l_ = static_cast<uint16_t>(p.l);
      pcm_r_ = static_cast<uint16_t>(p.r);
      pcm_valid_ = true;
      started_ = true;
      rd_ptr_ = (rd_ptr_ + 1) & kPtrMask;
      underrun_status_ = false;
      ++emitted_;
    } else if (started_) {
      // underrun: repeat the last pair (registers hold), count ONCE per
      // continuous event (D4 / audio_rules.md §2)
      if (!underrun_status_ && underruns_ != 0xFFFFFFFFu) ++underruns_;
      underrun_status_ = true;
    } else {
      // reset silence: zero pairs, NOT repeats — no "last pair" exists yet
      underrun_status_ = false;
    }
    // 2FF gray sync of the gpu-domain write pointer (post-gpu-edge wr_ptr_)
    wr_gray_sync_ = wr_gray_meta_;
    wr_gray_meta_ = ptr_gray(wr_ptr_, kPtrBits);
  }

  return accept;
}

}  // namespace zref
