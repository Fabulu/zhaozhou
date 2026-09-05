// zcon_session_directed.cpp — the shared console runtime's boundaries.
// Authored 2026-09-05 (software lane, first slice).
//
// What this pins is not gameplay -- there is none yet. It pins the PROPERTIES
// the roadmap asks for on day one:
//
//   * a fixed-tick loop that polls, advances, records and submits;
//   * inputs and authoritative state hashes recorded from the first tick;
//   * a replay that reproduces a session exactly;
//   * and a deliberate divergence LOCATED at the tick it happened, because
//     "the replay detected something" is not the useful property -- knowing
//     WHICH tick is.
//
// The mock backend also proves the boundary is real in the direction that
// matters: the simulation below never sees it, and could not tell a desktop
// backend from an FPGA transport.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "zcon/zcon.hpp"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

// ---------------------------------------------------------------------------
// A backend that records what it was given. It is a MOCK, not a stub: it
// answers honestly and remembers, so the test can assert the runtime actually
// drove it.
// ---------------------------------------------------------------------------
class MockBackend : public zcon::Backend {
 public:
  std::vector<zcon::InputSnapshot> scripted;
  int submits = 0;
  std::size_t last_command_bytes = 0;
  int publishes = 0;

  zcon::Handle publish(zcon::ResourceKind kind, const uint8_t*, std::size_t) override {
    ++publishes;
    zcon::Handle h;
    h.index = static_cast<uint32_t>(publishes);
    h.generation = 1;  // never 0: 0 means never published
    h.kind = kind;
    return h;
  }

  zcon::InputSnapshot poll(uint32_t tick) override {
    if (tick < scripted.size()) return scripted[tick];
    zcon::InputSnapshot s;
    s.tick = tick;
    return s;
  }

  void submit(const std::vector<uint8_t>& commands) override {
    ++submits;
    last_command_bytes = commands.size();
  }

  const char* name() const override { return "mock"; }
};

// ---------------------------------------------------------------------------
// A minimal deterministic simulation. Two "wizards" with a position each, moved
// by the sticks. Enough to have authoritative state worth hashing and to make a
// divergence possible; deliberately not a game.
// ---------------------------------------------------------------------------
class ToyTruth : public zcon::GameTruth {
 public:
  // `drift_at` injects a divergence at one tick, so the replay comparison can
  // be shown to LOCATE it rather than merely notice it.
  explicit ToyTruth(int drift_at = -1) : drift_at_(drift_at) {}

  void reset(uint64_t seed) override {
    for (int p = 0; p < zcon::InputSnapshot::kPads; ++p) {
      x_[p] = static_cast<int32_t>(seed & 0xFF) + p;
      y_[p] = static_cast<int32_t>((seed >> 8) & 0xFF) - p;
    }
    ticks_ = 0;
  }

  void advance(const zcon::InputSnapshot& in) override {
    for (int p = 0; p < zcon::InputSnapshot::kPads; ++p) {
      x_[p] += in.pad[p].stick_lx;
      y_[p] += in.pad[p].stick_ly;
      if (in.pad[p].buttons & 1u) x_[p] += 1;
    }
    if (static_cast<int>(ticks_) == drift_at_) x_[0] += 1;  // the injected desync
    ++ticks_;
  }

  uint64_t hash() const override {
    // FNV-1a over the authoritative state ONLY. Nothing presentational is in
    // here, which is the boundary rule: if a rendering decision could change
    // this number, rendering would be deciding gameplay.
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](int32_t v) {
      for (int b = 0; b < 4; ++b) {
        h ^= static_cast<uint8_t>((v >> (b * 8)) & 0xFF);
        h *= 1099511628211ull;
      }
    };
    for (int p = 0; p < zcon::InputSnapshot::kPads; ++p) {
      mix(x_[p]);
      mix(y_[p]);
    }
    return h;
  }

  void build_commands(std::vector<uint8_t>* out) const override {
    // Reads state, never writes it. One byte per pad is enough to prove the
    // runtime asked and the backend received.
    for (int p = 0; p < zcon::InputSnapshot::kPads; ++p)
      out->push_back(static_cast<uint8_t>(x_[p] & 0xFF));
  }

 private:
  int32_t x_[zcon::InputSnapshot::kPads] = {0, 0};
  int32_t y_[zcon::InputSnapshot::kPads] = {0, 0};
  uint32_t ticks_ = 0;
  int drift_at_;
};

std::vector<zcon::InputSnapshot> make_script(int n) {
  std::vector<zcon::InputSnapshot> v;
  for (int t = 0; t < n; ++t) {
    zcon::InputSnapshot s;
    s.tick = static_cast<uint32_t>(t);
    s.pad[0].stick_lx = static_cast<int8_t>((t * 7) % 13 - 6);
    s.pad[0].stick_ly = static_cast<int8_t>((t * 5) % 11 - 5);
    s.pad[0].buttons = static_cast<uint16_t>(t % 3 == 0 ? 1 : 0);
    s.pad[1].stick_lx = static_cast<int8_t>((t * 3) % 9 - 4);
    s.pad[1].stick_ly = static_cast<int8_t>((t * 11) % 7 - 3);
    v.push_back(s);
  }
  return v;
}

void test_session_drives_the_backend_every_tick() {
  MockBackend be;
  be.scripted = make_script(64);
  ToyTruth truth;
  zcon::Session s(&truth, &be);
  s.start(0xC0FFEE);
  for (int i = 0; i < 64; ++i) s.tick();

  check(s.tick_index() == 64, "the session advanced 64 fixed ticks", 64, s.tick_index());
  check(be.submits == 64, "presentation work was submitted once per tick", 64, be.submits);
  check(be.last_command_bytes == 2, "commands were built from state each tick", 2,
        static_cast<long long>(be.last_command_bytes));
  check(s.inputs().size() == 64, "every input snapshot was recorded", 64,
        static_cast<long long>(s.inputs().size()));
  // 65: the pre-input hash at tick 0, plus one per tick.
  check(s.hashes().size() == 65, "hashes recorded from BEFORE the first input", 65,
        static_cast<long long>(s.hashes().size()));
}

void test_replay_reproduces_the_session_exactly() {
  MockBackend be;
  be.scripted = make_script(120);
  ToyTruth truth;
  zcon::Session s(&truth, &be);
  s.start(0x1234abcd);
  for (int i = 0; i < 120; ++i) s.tick();

  const int diverged = s.replay_and_compare(s.inputs(), s.hashes());
  check(diverged == -1, "a recorded session replays with identical hashes", -1, diverged);
}

void test_a_divergence_is_located_at_the_tick_it_happened() {
  // Record a clean session.
  MockBackend be;
  be.scripted = make_script(100);
  ToyTruth clean;
  zcon::Session s(&clean, &be);
  s.start(0x55aa55aa);
  for (int i = 0; i < 100; ++i) s.tick();

  // Replay the SAME inputs against a simulation that drifts at tick 42.
  ToyTruth drifting(42);
  MockBackend be2;
  zcon::Session s2(&drifting, &be2);
  s2.start(0x55aa55aa);
  const int at = s2.replay_and_compare(s.inputs(), s.hashes());

  // The drift is applied during advance() of tick index 42, so the hash
  // recorded AFTER that advance -- entry 43 -- is the first that differs.
  check(at == 43, "the divergence is located at the exact tick, not merely seen", 43, at);
  check(at != -1, "and it is not reported as identical", 1, at != -1 ? 1 : 0);
}

void test_handles_carry_a_generation_and_zero_means_unpublished() {
  MockBackend be;
  zcon::Handle never;
  check(!never.valid(), "a default handle is INVALID -- generation 0", 0, never.valid() ? 1 : 0);

  const uint8_t bytes[4] = {1, 2, 3, 4};
  const zcon::Handle h = be.publish(zcon::ResourceKind::kMeshStream, bytes, 4);
  check(h.valid(), "a published handle is valid", 1, h.valid() ? 1 : 0);
  check(h.kind == zcon::ResourceKind::kMeshStream, "and remembers its family", 2,
        static_cast<long long>(h.kind));
}

void test_the_simulation_cannot_see_the_backend() {
  // The same recorded inputs must produce the same hashes through a DIFFERENT
  // backend instance. If the simulation could observe the platform, this would
  // not hold -- and that is the boundary the roadmap says must be real.
  MockBackend a;
  a.scripted = make_script(50);
  ToyTruth t1;
  zcon::Session s1(&t1, &a);
  s1.start(7);
  for (int i = 0; i < 50; ++i) s1.tick();

  MockBackend b;  // a different instance, different call history
  b.scripted = a.scripted;
  b.publish(zcon::ResourceKind::kTexturePage, nullptr, 0);  // extra traffic
  ToyTruth t2;
  zcon::Session s2(&t2, &b);
  s2.start(7);
  for (int i = 0; i < 50; ++i) s2.tick();

  bool same = s1.hashes().size() == s2.hashes().size();
  for (std::size_t i = 0; same && i < s1.hashes().size(); ++i)
    if (s1.hashes()[i] != s2.hashes()[i]) same = false;
  check(same, "identical inputs give identical state through any backend", 1, same ? 1 : 0);
}

}  // namespace

int main() {
  test_session_drives_the_backend_every_tick();
  test_replay_reproduces_the_session_exactly();
  test_a_divergence_is_located_at_the_tick_it_happened();
  test_handles_carry_a_generation_and_zero_means_unpublished();
  test_the_simulation_cannot_see_the_backend();

  if (g_failed) {
    std::printf("[zcon_session_directed] %d/%d checks FAILED\n", g_failed, g_checks);
    return 1;
  }
  std::printf("[zcon_session_directed] %d checks passed\n", g_checks);
  return 0;
}
