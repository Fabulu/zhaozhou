// audio_fifo_random.cpp — W2.4 AUDIO.FIFO randomized differential test
// (plan W2.4; design/contracts/AUDIO.FIFO.md "Randomized differential
// tests"; law spec/audio_rules.md §7).
//
// PCG-jittered refill timelines: random burst starts/lengths and random
// frame_tick pulses drive the Verilated zhao_audio_fifo and the
// cycle-accurate oracle zref::AudioFifo through the identical dual-clock
// protocol (audio_dev.hpp). The FULL observable behaviour must match
// bit-exactly: every audio tick (valid, L, R, underrun flag, counter), the
// per-cycle occupancy trace, the frame_tick shadows, and the accepted-pair
// count.
//
// Modes: default = 1,000 schedules (CTest fast); --nightly = 100,000
// (CTest nightly). Every failing vector is saved (charter §29-17) via the
// harness serializer.

#include "audio_dev.hpp"
#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using zhao_audio::Burst;
using zhao_audio::OracleDev;
using zhao_audio::RtlDev;
using zhao_audio::RunResult;

// PCG RXS-M-XS (same shape as the committed field-corpus generator — the
// qformats.md §7.5 constants, used here as the general test PRNG pattern).
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
  uint32_t lane(uint32_t lo, uint32_t hi) {  // inclusive
    return lo + (draw() % (hi - lo + 1));
  }
};

std::vector<uint8_t> serialize_schedule(const std::vector<Burst>& bursts, uint32_t cycles,
                                        const std::vector<uint32_t>& ticks) {
  std::vector<uint8_t> v;
  auto put32 = [&v](uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(x >> (8 * i)));
  };
  put32(cycles);
  put32(static_cast<uint32_t>(bursts.size()));
  for (const Burst& b : bursts) {
    put32(b.start_cycle);
    put32(b.len);
  }
  put32(static_cast<uint32_t>(ticks.size()));
  for (uint32_t t : ticks) put32(t);
  return v;
}

}  // namespace

int main(int argc, char** argv) {
  const bool nightly = (argc > 1 && std::strcmp(argv[1], "--nightly") == 0);
  const uint32_t iterations = nightly ? 100000u : 1000u;

  uint32_t fails = 0;
  for (uint32_t it = 0; it < iterations && fails < 8; ++it) {
    Prng rng(0xA11CEEDu + it * 2654435761u);
    const uint32_t cycles = rng.lane(64, nightly ? 640 : 1600);
    std::vector<Burst> bursts;
    const uint32_t nbursts = rng.lane(0, 6);
    for (uint32_t b = 0; b < nbursts; ++b) {
      bursts.push_back(Burst{rng.lane(0, cycles - 1), rng.lane(1, 512)});
    }
    std::vector<uint32_t> ticks;
    for (uint32_t t = 0; t < rng.lane(0, 2); ++t) {
      ticks.push_back(rng.lane(0, cycles - 1));
    }
    // keep ascending (the driver consumes them in order)
    for (size_t i = 1; i < ticks.size(); ++i) {
      if (ticks[i] < ticks[i - 1]) std::swap(ticks[i], ticks[i - 1]);
    }

    RtlDev rtl;
    OracleDev orc;
    const RunResult r = zhao_audio::run_schedule(rtl, bursts, cycles, ticks);
    const RunResult o = zhao_audio::run_schedule(orc, bursts, cycles, ticks);
    std::string where;
    if (!zhao_audio::results_equal(r, o, &where)) {
      ++fails;
      std::fprintf(stderr, "FAIL: iteration %u: %s\n", it, where.c_str());
      zhao::save_failing_vector("audio_fifo_random_iter" + std::to_string(it),
                                serialize_schedule(bursts, cycles, ticks), "oracle: " + where,
                                "rtl: " + where);
    }
    // invariant sanity on the RTL side itself (D4 bounds, every iteration)
    for (uint32_t occ : r.occupancy) {
      if (occ > zref::AudioFifo::kDepth) {
        std::fprintf(stderr, "FAIL: iteration %u: occupancy %u > depth\n", it, occ);
        ++fails;
        break;
      }
    }
  }

  if (fails == 0) {
    std::printf("audio_fifo_random: %u iterations bit-exact (%s)\n", iterations,
                nightly ? "nightly" : "fast");
    zhao::exit_hard(0);  // teardown-deadlock workaround (zhao_sim.hpp)
  }
  std::fprintf(stderr, "audio_fifo_random: %u failing iteration(s)\n", fails);
  zhao::exit_hard(1);  // teardown-deadlock workaround (zhao_sim.hpp)
}
