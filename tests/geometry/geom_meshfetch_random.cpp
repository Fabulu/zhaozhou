// geom_meshfetch_random.cpp — randomized properties of zref::MeshFetch.
//
// ---------------------------------------------------------------------------
// WHY THIS IS NOT A DIFFERENTIAL, AND WHY IT STILL EARNS ITS NAME
// ---------------------------------------------------------------------------
// The contract's "Randomized differential tests" section says "RTL against
// `zref::meshfetch`", and `zhao_geom_meshfetch.sv` does not exist. A
// differential needs two implementations; there is one.
//
// So this file fuzzes the oracle against its own INVARIANTS instead. That is a
// weaker claim than a differential and it is stated rather than blurred: it
// cannot catch an oracle that is confidently wrong in the same way everywhere.
// What it CAN catch is the class a directed test structurally cannot — a rule
// that holds on the eight descriptors someone thought of and fails on the
// ninth.
//
// When the RTL lands, the DUT goes in beside these generators and the
// properties below become the differential's coverage guard: every refusal
// class is asserted REACHED, so a differential that never exercised one would
// say so instead of passing quietly.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "zhao_sim.hpp"
#include "zref/zref_meshfetch.hpp"

using MF = zref::MeshFetch;
namespace mf = zref::meshfetch;
namespace zc = zref::cull;

namespace {

constexpr int32_t ONE = 65536;
constexpr uint8_t kFormat = 1;
constexpr uint16_t kGeneration = 0x2A2A;

// A named, seeded LCG. `Math.random`-style nondeterminism in a test that runs
// in CI is a bug report nobody can reproduce.
struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed) : s(seed) {}
  uint32_t next() {
    s = s * 1664525u + 1013904223u;
    return s;
  }
  uint32_t below(uint32_t n) { return next() % n; }
  int32_t signed32() { return static_cast<int32_t>(next()); }
};

void wr16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
}
void wr32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

// A random but LEGAL descriptor: every field inside its ruling limits, reserved
// bytes zero, CRC stamped last over the frozen window.
void legal(uint8_t* b, Rng& r) {
  std::memset(b, 0, mf::kDescBytes);
  b[0] = kFormat;
  b[1] = static_cast<uint8_t>(r.below(8));  // flags b0..b2 only
  b[2] = static_cast<uint8_t>(1 + r.below(mf::kMaxVertexCount));
  b[3] = static_cast<uint8_t>(1 + r.below(mf::kMaxTriangleCount));
  wr16(b + 4, static_cast<uint16_t>(r.next()));
  wr16(b + 6, static_cast<uint16_t>(r.next()));
  for (int i = 0; i < 3; ++i)
    wr32(b + 8 + 4 * i, static_cast<uint32_t>((r.signed32() % 4096) * ONE));
  wr32(b + 20, static_cast<uint32_t>(1 + r.below(64u * 65536u)));  // never zero
  wr32(b + 24, r.next());
  wr32(b + 28, r.next());
  wr16(b + 32, kGeneration);
  wr16(b + 34, static_cast<uint16_t>(r.next()));
  wr32(b + mf::kCrcOff, zhao_abi::zhao_crc32c(0, b, mf::kCrcCovered));
}

int64_t abs64(int64_t v) { return v < 0 ? -v : v; }

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Rng r(0xC0FFEEu);

  constexpr int kIters = 20000;

  // ---- P1: a legal descriptor is NEVER refused ----------------------------
  {
    int refused = 0;
    for (int i = 0; i < kIters; ++i) {
      uint8_t b[mf::kDescBytes];
      legal(b, r);
      if (MF::validate(b, kFormat, kGeneration) != mf::Refusal::kNone) ++refused;
    }
    zhao::check(refused == 0,
                "20,000 randomly generated LEGAL descriptors are never refused -- "
                "a validator that is too strict deletes geometry just as surely "
                "as one that is too loose admits corruption",
                0, refused);
  }

  // ---- P2: ANY single-bit corruption of the covered window is refused ------
  // The directed test walks all 60 bytes at bit 0. This walks random bytes at
  // random bits, which is the same property over the other seven planes.
  {
    int accepted = 0;
    for (int i = 0; i < kIters; ++i) {
      uint8_t b[mf::kDescBytes];
      legal(b, r);
      const uint32_t byte = r.below(mf::kCrcCovered);
      const uint32_t bit = r.below(8);
      b[byte] ^= static_cast<uint8_t>(1u << bit);
      if (MF::validate(b, kFormat, kGeneration) == mf::Refusal::kNone) ++accepted;
    }
    zhao::check(accepted == 0,
                "no single-bit corruption anywhere in the 60 covered bytes is "
                "ever accepted -- 20,000 flips across all eight bit planes",
                0, accepted);
  }

  // ---- P3: corruption OUTSIDE the covered window is invisible -------------
  // Bytes 60..63 are the CRC itself; there is nothing else outside the window.
  // Flipping a CRC byte must refuse, and this is the case that would catch a
  // window computed as 0..63 instead of 0..59 -- which would make the CRC
  // cover itself and never match.
  {
    int accepted = 0;
    for (int i = 0; i < 2000; ++i) {
      uint8_t b[mf::kDescBytes];
      legal(b, r);
      b[mf::kCrcOff + r.below(4)] ^= static_cast<uint8_t>(1u << r.below(8));
      if (MF::validate(b, kFormat, kGeneration) == mf::Refusal::kNone) ++accepted;
    }
    zhao::check(accepted == 0,
                "corrupting the stored CRC itself always refuses -- and a window "
                "wrongly computed over 0..63 would cover the CRC field and never "
                "match anything at all",
                0, accepted);
  }

  // ---- P4: every refusal class is REACHABLE, and the reason is the right one
  // A taxonomy nobody can reach is a taxonomy nobody is testing. This is the
  // coverage guard the differential will inherit.
  {
    int seen[8] = {0};
    for (int i = 0; i < kIters; ++i) {
      uint8_t b[mf::kDescBytes];
      legal(b, r);
      mf::Refusal want = mf::Refusal::kNone;
      switch (r.below(7)) {
        case 0:
          b[0] = static_cast<uint8_t>(kFormat + 1 + r.below(200));
          want = mf::Refusal::kFormat;
          break;
        case 1:
          b[mf::kCrcOff] ^= 0xFF;
          want = mf::Refusal::kCrc;
          break;
        case 2:
          wr16(b + 32, static_cast<uint16_t>(kGeneration + 1 + r.below(100)));
          wr32(b + mf::kCrcOff, zhao_abi::zhao_crc32c(0, b, mf::kCrcCovered));
          want = mf::Refusal::kGeneration;
          break;
        case 3:
          b[2] = static_cast<uint8_t>(mf::kMaxVertexCount + 1 + r.below(190));
          wr32(b + mf::kCrcOff, zhao_abi::zhao_crc32c(0, b, mf::kCrcCovered));
          want = mf::Refusal::kVertexCount;
          break;
        case 4:
          b[3] = static_cast<uint8_t>(mf::kMaxTriangleCount + 1 + r.below(128));
          wr32(b + mf::kCrcOff, zhao_abi::zhao_crc32c(0, b, mf::kCrcCovered));
          want = mf::Refusal::kTriangleCount;
          break;
        case 5:
          b[mf::kReservedOff + r.below(mf::kReservedLen)] = static_cast<uint8_t>(1 + r.below(255));
          wr32(b + mf::kCrcOff, zhao_abi::zhao_crc32c(0, b, mf::kCrcCovered));
          want = mf::Refusal::kReserved;
          break;
        default:
          wr32(b + 20, 0);
          wr32(b + mf::kCrcOff, zhao_abi::zhao_crc32c(0, b, mf::kCrcCovered));
          want = mf::Refusal::kZeroBound;
          break;
      }
      const mf::Refusal got = MF::validate(b, kFormat, kGeneration);
      if (got == want)
        seen[static_cast<int>(want)]++;
      else
        seen[0]++;  // a wrong reason lands in the kNone slot and fails below
    }
    int unreached = 0;
    for (int k = 1; k <= mf::kRefusalCount; ++k)
      if (seen[k] == 0) ++unreached;
    zhao::check(unreached == 0 && seen[0] == 0,
                "all seven refusal classes are reached, and each corruption "
                "refuses for its OWN reason -- a wrong reason is as wrong as a "
                "wrong verdict, because the counters are per reason",
                0, unreached + seen[0]);
  }

  // ---- P5: the bound is never TIGHT, under random non-uniform scale --------
  // The ruling chose maximum-absolute scale to round the bound OUTWARD. A
  // random sweep is the right shape for this: the failure mode is one scale
  // combination where the rounding goes the other way, and a directed test
  // picks the combinations someone already thought of.
  {
    int tight = 0;
    for (int i = 0; i < kIters; ++i) {
      MF::InstanceXform x{};
      int64_t max_abs = 0;
      for (int row = 0; row < 3; ++row)
        for (int c = 0; c < 3; ++c) {
          const int32_t v = static_cast<int32_t>((r.signed32() % 8) * ONE);
          x.m[row * 4 + c] = v;
          if (abs64(v) > max_abs) max_abs = abs64(v);
        }
      const int32_t centre[3] = {0, 0, 0};
      const uint32_t rad = 1 + r.below(1u << 20);

      int32_t wc[3];
      uint32_t wr;
      mf::world_bound(x, centre, rad, wc, &wr);

      // The exact bound along the most-stretched axis, rounded DOWN. The world
      // radius must be at least that; equal is fine, smaller deletes geometry.
      const int64_t exact = (static_cast<int64_t>(rad) * max_abs) >> 16;
      if (static_cast<int64_t>(wr) < exact) ++tight;
    }
    zhao::check(tight == 0,
                "across 20,000 random non-uniform scales the world radius is "
                "never SMALLER than the exact transformed bound -- outward is "
                "the ruled direction and a tight bound deletes geometry",
                0, tight);
  }

  std::printf("  %d iterations per property, seed 0xC0FFEE\n", kIters);
  return zhao::report_and_exit("geom_meshfetch_random");
}
