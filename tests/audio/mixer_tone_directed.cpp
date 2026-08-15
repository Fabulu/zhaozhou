// mixer_tone_directed.cpp — W2.4 SW.MIXER tone-subset directed test
// (design/contracts/SW.MIXER.md "Directed tests"; law spec/audio_rules.md §4).
//
// The tone must be BIT-EXACT against the committed fixed-point goldens —
// the exhaustive sin vectors (tests/golden/fixp/sin_cos_u16.bin, every one
// of the 2^16 angles) are the independent oracle: expected sample =
// sat_s16(golden_fx_sin[angle] >> 1), with the frozen per-tone increments
// accumulated by hand in this file. No new constants, no libm.
//
// Covers: full-frame streams per tone (3 frames = 2400 pairs each, the
// 800-pairs-per-frame accounting law), the saturation boundary, phase law
// (u32 wrap, increment exactness), both-channels-identical, and the PCM
// ring free-space law (memory_rules.md §4.2).

#include "zref/zref_audio.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

void check_eq(uint64_t expected, uint64_t actual, const char* what) {
  if (expected != actual) {
    std::fprintf(stderr, "FAIL: %s (expected %llu, got %llu)\n", what,
                 static_cast<unsigned long long>(expected),
                 static_cast<unsigned long long>(actual));
    ++failures;
  }
}

std::vector<int32_t> load_golden_sin() {
  const fs::path p = fs::path(ZHAO_GOLDEN_DIR) / "sin_cos_u16.bin";
  std::ifstream in(p, std::ios::binary);
  if (!in) {
    std::fprintf(stderr, "FAIL: cannot open %s\n", p.string().c_str());
    ++failures;
    return {};
  }
  std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
  if (raw.size() != 65536 * 8) {
    check(false, "golden sin_cos_u16.bin has the frozen 65536x8B layout");
    return {};
  }
  std::vector<int32_t> sin(65536);
  for (uint32_t a = 0; a < 65536; ++a) {
    uint32_t lo = 0, hi = 0;
    for (int i = 0; i < 4; ++i) {
      lo |= static_cast<uint32_t>(raw[a * 8 + i]) << (8 * i);
      hi |= static_cast<uint32_t>(raw[a * 8 + 4 + i]) << (8 * i);
    }
    sin[a] = static_cast<int32_t>(lo);  // fx_sin first, fx_cos second
    (void)hi;
  }
  return sin;
}

// expected sample straight from the golden bytes (independent of zref code)
int16_t expected_sample(const std::vector<int32_t>& golden_sin, uint16_t angle) {
  const int32_t half = golden_sin[angle] >> 1;  // arithmetic shift toward -inf
  if (half > 0x7FFF) return 0x7FFF;
  if (half < -0x8000) return static_cast<int16_t>(-0x8000);
  return static_cast<int16_t>(half);
}

}  // namespace

int main() {
  const std::vector<int32_t> golden_sin = load_golden_sin();
  check(!golden_sin.empty(), "golden sin table loaded");

  // ---- full-frame streams per tone, bit-exact vs the goldens --------------
  for (const zref::ToneSpec& spec : zref::kToneTable) {
    zref::MixerTone tone(spec.id);
    check_eq(tone.increment(), spec.increment, "tone increment is the frozen value");
    uint32_t phase = 0;
    for (uint32_t f = 0; f < 3; ++f) {
      const std::vector<zref::AudioPair> fr = tone.frame();
      check_eq(fr.size(), 800, "frame() emits exactly 800 pairs (accounting law)");
      for (uint32_t k = 0; k < fr.size(); ++k) {
        const uint16_t angle = static_cast<uint16_t>(phase >> 16);
        const int16_t want = expected_sample(golden_sin, angle);
        if (fr[k].l != want || fr[k].r != want) {
          char buf[160];
          std::snprintf(buf, sizeof buf,
                        "%s frame %u tick %u: (%04x,%04x) vs golden (%04x)",
                        spec.name, f, k, static_cast<uint16_t>(fr[k].l),
                        static_cast<uint16_t>(fr[k].r),
                        static_cast<uint16_t>(want));
          check(false, buf);
          break;
        }
        phase += spec.increment;  // hand accumulation (u32 wrap by definition)
      }
    }
    check_eq(tone.phase(), phase, "tone phase == hand-accumulated u32 phase");
  }

  // ---- saturation boundary + tone law corners (phase set directly) --------
  {
    zref::MixerTone tone(zref::ToneId::TONE_A4);
    struct Corner {
      uint32_t phase;
      int16_t want;
      const char* what;
    };
    const Corner corners[] = {
        {0x00000000u, 0x0000, "phase 0 -> zero sample"},
        {0x40000000u, 0x7FFF, "peak (+0x10000 halved +0x8000 saturates +0x7FFF)"},
        {0xC0000000u, static_cast<int16_t>(0x8000),
         "trough (-0x10000 halved -0x8000 fits exactly)"},
        {0x80000000u, 0x0000, "half turn -> zero sample"},
    };
    for (const Corner& c : corners) {
      tone.set_phase(c.phase);
      const zref::AudioPair p = tone.tick();
      check(p.l == c.want && p.r == c.want, c.what);
    }
    // dynamic corners: every angle that maps to the extreme table entries
    for (const uint16_t angle : {uint16_t(0x3FFF), uint16_t(0x4001),
                                 uint16_t(0xBFFF), uint16_t(0xC001)}) {
      tone.set_phase(static_cast<uint32_t>(angle) << 16);
      const zref::AudioPair p = tone.tick();
      const int16_t want = expected_sample(golden_sin, angle);
      check(p.l == want && p.r == want, "angle corner matches golden fx_sin");
    }
  }

  // ---- phase law: select() keeps the accumulator running ------------------
  {
    zref::MixerTone tone(zref::ToneId::TONE_A4);
    for (int i = 0; i < 100; ++i) tone.tick();
    const uint32_t before = tone.phase();
    tone.select(zref::ToneId::TONE_C4);
    check_eq(tone.phase(), before, "tone switch keeps the phase accumulator");
    tone.tick();
    check_eq(tone.phase(), before + zref::tone_increment(zref::ToneId::TONE_C4),
             "post-switch tick advances by the new increment (mod 2^32)");
  }

  // ---- PCM ring free-space law (memory_rules.md §4.2) ---------------------
  {
    zref::PcmRing ring(8);
    check_eq(ring.free_pairs(), 8, "ring starts empty (full free space)");
    uint32_t seq = 0;
    auto next_pair = [&seq] {
      ++seq;
      return zref::AudioPair{static_cast<int16_t>(seq * 3),
                             static_cast<int16_t>(-seq * 5)};
    };
    for (int i = 0; i < 8; ++i) {
      check(ring.write(next_pair()), "ring write succeeds while free space");
    }
    check_eq(ring.free_pairs(), 0, "ring full after capacity writes");
    check(!ring.write(zref::AudioPair{1, 2}),
          "ring REFUSES the overwrite (free-space law)");
    check_eq(ring.host_write_ptr(), 8, "host_write_ptr frozen on refusal");
    // FPGA reads 3 (it owns fpga_read_ptr only)
    zref::AudioPair p0 = ring.fpga_read();
    zref::AudioPair p1 = ring.fpga_read();
    zref::AudioPair p2 = ring.fpga_read();
    check(p0.l == 3 && p0.r == -5, "ring returns pairs FIFO order (1)");
    check(p1.l == 6 && p1.r == -10, "ring returns pairs FIFO order (2)");
    check(p2.l == 9 && p2.r == -15, "ring returns pairs FIFO order (3)");
    check_eq(ring.filled_pairs(), 5, "filled = host_written - fpga_read");
    check_eq(ring.free_pairs(), 3, "free space restored by FPGA reads");
    for (int i = 0; i < 3; ++i) {
      check(ring.write(next_pair()), "ring accepts again after reads");
    }
    check(!ring.write(zref::AudioPair{3, 4}),
          "free-space law holds across the cycle");
    // drain the FULL content (pairs 4..11: the 5 unread from the first fill
    // precede the 3 refilled): global FIFO order preserved end to end
    for (uint32_t k = 4; k <= 11; ++k) {
      const zref::AudioPair p = ring.fpga_read();
      check(p.l == static_cast<int16_t>(k * 3) &&
                p.r == static_cast<int16_t>(-k * 5),
            "ring total order: every pair read exactly once, in order");
    }
    check_eq(ring.filled_pairs(), 0, "ring drained");
    check_eq(ring.fpga_read_ptr(), ring.host_write_ptr(), "pointers agree");
  }

  if (failures == 0) {
    std::printf("mixer_tone_directed: all checks green\n");
    return 0;
  }
  std::printf("mixer_tone_directed: %d failure(s)\n", failures);
  return 1;
}
