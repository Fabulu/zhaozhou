// mixer_tone_random.cpp — W2.4 SW.MIXER tone-switching differential test
// (design/contracts/SW.MIXER.md "Randomized differential tests"; law
// spec/audio_rules.md §4).
//
// PCG tone switching at FRAME boundaries (the only legal switch point for
// the demo/runtime configuration): 1,000 frames of 800 pairs, the tone for
// each frame drawn from the frozen three-tone table. The oracle accumulates
// the phase by hand (u32 wrap) and evaluates every sample from the
// committed golden sin vectors — bit-exact, no zref tone code on the
// expected side.

#include "zref/zref_audio.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace {

uint32_t pcg_perm(uint32_t s) {
  const uint32_t w = static_cast<uint32_t>(((s >> ((s >> 28) + 4)) ^ s) * 277803737u);
  return (w >> 22) ^ w;
}

struct Prng {
  uint32_t state;
  explicit Prng(uint32_t seed) : state(seed) {}
  uint32_t draw() {
    state = state * 747796405u + 2891336453u;
    return pcg_perm(state);
  }
};

int failures = 0;

}  // namespace

int main() {
  // load the exhaustive golden sin vectors (independent oracle values)
  const fs::path p = fs::path(ZHAO_GOLDEN_DIR) / "sin_cos_u16.bin";
  std::ifstream in(p, std::ios::binary);
  if (!in) {
    std::fprintf(stderr, "FAIL: cannot open %s\n", p.string().c_str());
    return 1;
  }
  std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (raw.size() != 65536 * 8) {
    std::fprintf(stderr, "FAIL: golden sin_cos_u16.bin layout\n");
    return 1;
  }
  std::vector<int32_t> golden_sin(65536);
  for (uint32_t a = 0; a < 65536; ++a) {
    uint32_t lo = 0;
    for (int i = 0; i < 4; ++i) {
      lo |= static_cast<uint32_t>(raw[a * 8 + i]) << (8 * i);
    }
    golden_sin[a] = static_cast<int32_t>(lo);
  }

  Prng rng(0x70EE7u);
  zref::MixerTone tone(zref::ToneId::TONE_A4);
  uint32_t oracle_phase = 0;
  uint32_t oracle_inc = zref::tone_increment(zref::ToneId::TONE_A4);
  uint64_t total = 0;

  for (uint32_t frame = 0; frame < 1000; ++frame) {
    // frame-boundary switch: PCG-drawn tone from the frozen closed set
    const uint32_t pick = rng.draw() % 3;
    const zref::ToneId id = zref::kToneTable[pick].id;
    tone.select(id);
    oracle_inc = zref::tone_increment(id);

    for (uint32_t k = 0; k < 800; ++k) {
      const zref::AudioPair got = tone.tick();
      const uint16_t angle = static_cast<uint16_t>(oracle_phase >> 16);
      const int32_t half = golden_sin[angle] >> 1;
      int16_t want;
      if (half > 0x7FFF) {
        want = 0x7FFF;
      } else if (half < -0x8000) {
        want = static_cast<int16_t>(-0x8000);
      } else {
        want = static_cast<int16_t>(half);
      }
      if (got.l != want || got.r != want) {
        std::fprintf(stderr,
                     "FAIL: frame %u tick %u: got (%04x,%04x) want %04x "
                     "(angle %04x)\n",
                     frame, k, static_cast<uint16_t>(got.l), static_cast<uint16_t>(got.r),
                     static_cast<uint16_t>(want), angle);
        ++failures;
        if (failures > 8) return 1;
      }
      oracle_phase += oracle_inc;  // u32 wrap by definition
      ++total;
    }
  }

  if (failures == 0) {
    std::printf("mixer_tone_random: %llu pairs bit-exact over 1000 frames\n",
                static_cast<unsigned long long>(total));
    return 0;
  }
  return 1;
}
