// debug_counters_directed.cpp — DEBUG.COUNTERS directed vectors (plan W2.6 /
// design/contracts/DEBUG.COUNTERS.md "Directed tests" / spec/counters.md).
//
//   1. snapshot protocol: provider values capture at frame_tick; the sweep
//      streams every catalog id ASCENDING; ownerless ids read 0
//   2. catalog indices: provider ids are the blocks.yml catalog positions
//      (0 frame_cycles, 1 deadline_faults, 2 commands, 31 audio_underruns)
//   3. read window: ready/valid backpressure; a tick mid-sweep RESTARTS the
//      ascending sweep
//   4. reading never disturbs: live provider changes between tick and sweep
//      do NOT leak into the shadow bank
//   5. out-of-catalog provider id: cat_violation sticky, never a fallback
//   6. bit-exact vs zref::DebugCounters (sweep + the .zcap section bytes)

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"
#include "Vzhao_debug_counters.h"

#include "zhao_sim.hpp"
#include "zref/zref_cmd2.hpp"

using zhao::check;
using zref::CounterBeat;

namespace {

constexpr int kProvN = 4;
constexpr uint16_t kCatalog = zref::DebugCounters::kCatalogIds;  // 40

uint64_t tickWord(bool pulse) {
  return pulse ? (1ull << 33) : 0ull;  // {pulse, frame_id, repeated}
}

struct Snap {
  uint16_t id;
  uint64_t value;
};

class Dev {
 public:
  Dev() : top_(new Vzhao_debug_counters) { reset(); }
  ~Dev() { top_->final(); delete top_; }
  Dev(const Dev&) = delete;
  Dev& operator=(const Dev&) = delete;

  void reset() {
    top_->rst_n = 0;
    park();
    top_->eval();
    for (int i = 0; i < 2; ++i) edge();
    top_->rst_n = 1;
    top_->eval();
  }

  void park() {
    for (int p = 0; p < kProvN; ++p) {
      top_->prov_i[p][0] = 0;
      top_->prov_i[p][1] = 0;
      top_->prov_i[p][2] = 0;  // valid = 0
    }
    top_->frame_tick_i = 0;
    top_->snap_ready_i = 1;
  }

  void provide(int p, bool valid, uint16_t id, uint64_t value) {
    top_->prov_i[p][2] = (valid ? 0x10000u : 0u) | id;
    top_->prov_i[p][0] = static_cast<uint32_t>(value);
    top_->prov_i[p][1] = static_cast<uint32_t>(value >> 32);
  }

  void cycle(bool tick, bool snap_ready = true) {
    top_->frame_tick_i = tickWord(tick);
    top_->snap_ready_i = snap_ready ? 1 : 0;
    top_->eval();
    if (top_->snap_valid_o && top_->snap_ready_i) {
      const uint16_t id = static_cast<uint16_t>(top_->snap_o[2]);
      const uint64_t v = static_cast<uint64_t>(top_->snap_o[0]) |
                         (static_cast<uint64_t>(top_->snap_o[1]) << 32);
      swept_.push_back(Snap{id, v});
    }
    edge();
  }

  // drain the whole sweep (with an optional mid-sweep stall pattern)
  std::vector<Snap> sweep(bool stall_mid = false) {
    swept_.clear();
    int n = 0;
    for (int i = 0; i < kCatalog * 8 + 32 && (top_->window_open_o || n < kCatalog); ++i) {
      // periodic backpressure keyed on the CYCLE index (a consumer that
      // stalls every 5th cycle; keying on accepted beats would deadlock
      // the pattern against its own progress counter)
      const bool ready = !stall_mid || (i % 5) != 3;
      cycle(false, ready);
      if (ready && top_->snap_valid_o) ++n;
      if (!top_->window_open_o && n >= kCatalog) break;
    }
    return swept_;
  }

  bool violation() const { return top_->cat_violation_o != 0; }

  Vzhao_debug_counters* top_;
  std::vector<Snap> swept_;

 private:
  void edge() {
    top_->clk = 0;
    top_->eval();
    top_->clk = 1;
    top_->eval();
    top_->clk = 0;
    top_->eval();
  }
};

bool snapsEqual(const std::vector<Snap>& a, const std::vector<Snap>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].id != b[i].id || a[i].value != b[i].value) return false;
  }
  return true;
}

bool sweepsEqual(const std::vector<Snap>& got, const std::vector<CounterBeat>& want) {
  if (got.size() != want.size()) return false;
  for (size_t i = 0; i < got.size(); ++i) {
    if (got[i].id != want[i].id || got[i].value != want[i].value) return false;
  }
  return true;
}

}  // namespace

// ---- random lane: PCG provider sets + tick schedules vs the oracle --------
static int runRandom(uint32_t frames, uint64_t seed) {
  struct Pcg32 {
    uint64_t state;
    uint64_t inc;
    uint32_t next() {
      const uint64_t old = state;
      state = old * 6364136223846793005ull + inc;
      const uint32_t xorshifted = static_cast<uint32_t>(((old >> 18) ^ old) >> 27);
      const uint32_t rot = static_cast<uint32_t>(old >> 59);
      return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }
    uint32_t operator()(uint32_t bound) { return next() % bound; }
  };
  Pcg32 rng{seed, (seed << 1) | 1u};
  Dev t;
  zref::DebugCounters orc;
  orc.reset();
  for (uint32_t f = 0; f < frames; ++f) {
    // random provider set (catalog indices; some providers invalid)
    for (int p = 0; p < kProvN; ++p) {
      const bool v = rng(4) != 0;
      const uint16_t id = static_cast<uint16_t>(rng(kCatalog));
      const uint64_t val = (static_cast<uint64_t>(rng.next()) << 32) | rng.next();
      t.provide(p, v, id, val);
      if (v) orc.provide(id, val);
    }
    t.cycle(true);
    orc.tick();
    const std::vector<Snap> got = t.sweep();
    check(got.size() == kCatalog, "rand: full sweep", kCatalog, got.size());
    check(sweepsEqual(got, orc.sweep()), "rand: sweep bit-exact vs oracle", 1, 0);
    for (int i = 0; i < static_cast<int>(rng(6)); ++i) t.cycle(false);  // drift
  }
  std::printf("debug_counters random: %u frames (newline-escaped) done", frames);
  return 0;
}

int main(int argc, char** argv) {
  uint32_t random_frames = 0;
  uint64_t seed = 0xCCCC07020260815ull;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && i + 1 < argc) {
      random_frames = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = std::strtoull(argv[++i], nullptr, 0);
    }
  }
  if (random_frames > 0) {
    runRandom(random_frames, seed);
    return zhao::report_and_exit("debug_counters_random");
  }

  // ---- 1/2/6: snapshot protocol + catalog indices + oracle bit-exactness --
  {
    Dev t;
    zref::DebugCounters orc;
    orc.reset();

    // inject a known event count per provider (catalog indices, D9)
    t.provide(0, true, 0, 7);      // frame_cycles = 7
    t.provide(1, true, 1, 3);      // deadline_faults = 3
    t.provide(2, true, 2, 1234);   // commands = 1234
    t.provide(3, true, 31, 9);     // audio_underruns = 9
    orc.provide(0, 7);
    orc.provide(1, 3);
    orc.provide(2, 1234);
    orc.provide(31, 9);

    t.cycle(false);
    t.cycle(true);  // THE tick: shadows latch, window opens
    orc.tick();

    std::vector<Snap> got = t.sweep();
    std::vector<CounterBeat> want = orc.sweep();
    check(got.size() == kCatalog, "snap: whole catalog swept", kCatalog, got.size());
    check(sweepsEqual(got, want), "snap: sweep bit-exact vs oracle", 1, 0);
    // ascending order (capture_format 4.2 law)
    for (size_t i = 1; i < got.size(); ++i) {
      check(got[i - 1].id < got[i].id, "snap: ascending counter_id", got[i].id,
            got[i - 1].id);
    }
    // ownerless counters read 0 (spec/counters.md 5)
    check(got[28].id == 28 && got[28].value == 0, "snap: ownerless id 28 reads 0", 0,
          got[28].value);
    check(got[0].value == 7 && got[2].value == 1234 && got[31].value == 9,
          "snap: catalog values at their indices", 7, got[0].value);

    // .zcap COUNTERS section composition (oracle-side, byte law)
    const std::vector<uint8_t> body = zref::DebugCounters::zcapSection(want);
    check(body.size() == 4 + 40 * 12, "snap: section byte size", 484, body.size());
    uint32_t count = 0;
    for (int i = 0; i < 4; ++i) {
      count |= static_cast<uint32_t>(body[i]) << (8 * i);
    }
    check(count == 40, "snap: section count = catalog", 40, count);
  }

  // ---- 3: read window + backpressure + tick-mid-sweep restart -------------
  {
    Dev t;
    t.provide(0, true, 0, 42);
    t.cycle(true);                       // tick: capture + window opens
    std::vector<Snap> a = t.sweep(false);
    check(a.size() == kCatalog, "window: full sweep", kCatalog, a.size());

    t.cycle(true);                       // a fresh window for the stall test
    std::vector<Snap> b = t.sweep(true); // heavy periodic backpressure
    check(b.size() == kCatalog, "window: full sweep under backpressure", kCatalog,
          b.size());
    check(snapsEqual(a, b), "window: backpressure changes nothing", 1, 0);

    // tick mid-sweep: restarts ascending from id 0 with the CAPTURED shadow
    t.provide(0, true, 0, 100);
    t.cycle(true);                       // open a window
    for (int i = 0; i < 10; ++i) {       // consume part of it
      t.cycle(false);
    }
    t.cycle(true);                       // mid-sweep tick: capture + restart
    std::vector<Snap> c = t.sweep(false);
    check(c.size() == kCatalog, "window: restart sweeps the full catalog", kCatalog,
          c.size());
    check(c[0].id == 0, "window: restart begins at id 0", 0, c[0].id);
    check(c[0].value == 100, "window: restart uses the CAPTURED shadow", 100,
          c[0].value);
  }

  // ---- 4: reading never disturbs the live set ------------------------------
  {
    Dev t;
    zref::DebugCounters orc;
    orc.reset();
    t.provide(0, true, 0, 5);
    orc.provide(0, 5);
    t.cycle(true);   // tick: bank[0] = 5
    orc.tick();
    // consume only the first few beats, then change the LIVE provider value
    for (int i = 0; i < 5; ++i) t.cycle(false);
    t.provide(0, true, 0, 999);          // live change INSIDE the window
    orc.provide(0, 999);
    std::vector<Snap> rest = t.sweep(false);
    check(rest.size() == kCatalog - 5, "quiet: rest of the window drains", kCatalog - 5,
          rest.size());
    check(rest[0].value == 0 && rest[0].id == 5, "quiet: mid-window beats are shadow",
          5, rest[0].id);
    t.cycle(true);   // the NEXT tick captures the new value
    orc.tick();
    std::vector<Snap> c = t.sweep();
    check(c.size() == kCatalog, "quiet: next sweep full", kCatalog, c.size());
    check(c[0].value == 999, "quiet: next tick captures the new value", 999,
          c[0].value);
    check(sweepsEqual(c, orc.sweep()), "quiet: still bit-exact vs oracle", 1, 0);
  }

  // ---- 5: out-of-catalog provider id ---------------------------------------
  {
    Dev t;
    check(!t.violation(), "viol: clean before", 0, t.violation());
    t.provide(0, true, 40, 1);  // id 40 is OUT of the 40-entry catalog
    t.cycle(true);
    check(t.violation(), "viol: flagged (sticky)", 1, t.violation());
    t.provide(0, true, 0, 1);
    t.cycle(true);
    check(t.violation(), "viol: sticky until reset", 1, t.violation());
  }

  return zhao::report_and_exit("debug_counters_directed");
}
