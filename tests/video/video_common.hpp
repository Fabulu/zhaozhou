// video_common.hpp — small shared helpers for the W2.2 video tests:
// the FNV-1a trace hash (run-twice determinism, plan R1) and the
// deterministic xorshift64* PRNG (same family as the wave-1 tests).

#pragma once

#include <cstdint>

namespace zhao_video {

struct Fnv1a {
  uint64_t h = 0xCBF29CE484222325ull;
  void mix(uint64_t v) {
    for (int i = 0; i < 8; ++i) {
      h ^= (v >> (8 * i)) & 0xFFu;
      h *= 0x100000001B3ull;
    }
  }
};

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed ? seed : 1) {}
  uint64_t next() {
    s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
    return s * 2685821657736338717ull;
  }
  uint32_t u32() { return (uint32_t)(next() >> 32); }
};

}  // namespace zhao_video
