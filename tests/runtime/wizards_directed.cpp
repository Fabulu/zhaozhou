// wizards_directed.cpp — the first playable's game truth.
// Authored 2026-09-05 (software lane).
//
// The roadmap's first playable is "two wizards, two controllers, two views,
// one creature, one damaging spell, destructible ground, death and restart".
// This pins the simulation half of that: cast, travel, detonate, damage, die,
// respawn, and a crater that survives.
//
// The check that matters most is the LAST one: the whole match replays to an
// identical hash stream. Everything else could be right and that could still
// fail, and if it fails the console and the desktop are two different games.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "zcon/zcon.hpp"
#include "zgame/wizards.hpp"

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

zcon::InputSnapshot idle(uint32_t t) {
  zcon::InputSnapshot s;
  s.tick = t;
  return s;
}

void step(zgame::Wizards& g, int n) {
  for (int i = 0; i < n; ++i) g.advance(idle(g.tick()));
}

void test_opening_position_is_deterministic_and_symmetric() {
  zgame::Wizards a, b;
  a.reset(1);
  b.reset(999);  // a different seed must NOT move the opening
  check(a.hash() == b.hash(), "the opening position does not depend on the seed",
        1, a.hash() == b.hash() ? 1 : 0);
  check(a.wizard(0).health == zgame::Wizards::kStartHealth,
        "both wizards start at full health", zgame::Wizards::kStartHealth,
        a.wizard(0).health);
  check(a.wizard(0).x != a.wizard(1).x, "and they do not start on top of each other",
        1, a.wizard(0).x != a.wizard(1).x ? 1 : 0);
}

void test_movement_is_fixed_point_and_clamped() {
  zgame::Wizards g;
  g.reset(0);
  const int32_t x0 = g.wizard(0).x;

  zcon::InputSnapshot in = idle(0);
  in.pad[0].stick_lx = 10;
  g.advance(in);
  check(g.wizard(0).x == x0 + 10 * zgame::Wizards::kMoveScale,
        "a stick of 10 moves exactly 10*kMoveScale fx16 units",
        x0 + 10 * zgame::Wizards::kMoveScale, g.wizard(0).x);

  // Walk hard into the edge for long enough to overshoot, then check the clamp.
  for (int i = 0; i < 400; ++i) {
    zcon::InputSnapshot s = idle(g.tick());
    s.pad[0].stick_lx = -128;
    g.advance(s);
  }
  check(g.wizard(0).x == 0, "movement clamps at the ground edge, never wraps", 0,
        g.wizard(0).x);
}

void test_cast_travel_detonate_and_damage() {
  zgame::Wizards g;
  g.reset(0);

  zcon::InputSnapshot cast = idle(0);
  cast.pad[0].buttons = 1;  // aim is zero -> fires toward the opponent
  g.advance(cast);

  check(g.bolt(0).active, "casting produces a live bolt", 1,
        g.bolt(0).active ? 1 : 0);
  check(g.wizard(0).cooldown > 0, "and starts the cooldown", 1,
        g.wizard(0).cooldown > 0 ? 1 : 0);

  const int32_t bx0 = g.bolt(0).x;
  g.advance(idle(g.tick()));
  check(g.bolt(0).x != bx0, "the bolt travels", 1, g.bolt(0).x != bx0 ? 1 : 0);

  // Let it fly into the opponent.
  const int32_t before = g.wizard(1).health;
  for (int i = 0; i < 80 && g.bolt(0).active; ++i) step(g, 1);
  check(!g.bolt(0).active, "the bolt eventually detonates", 0,
        g.bolt(0).active ? 1 : 0);
  check(g.wizard(1).health < before, "and the opponent took damage", 1,
        g.wizard(1).health < before ? 1 : 0);
}

void test_cooldown_prevents_a_second_cast() {
  zgame::Wizards g;
  g.reset(0);
  zcon::InputSnapshot cast = idle(0);
  cast.pad[0].buttons = 1;
  g.advance(cast);
  const int32_t first_x = g.bolt(0).x;

  // Hold the button down: no second bolt while the first is alive or on
  // cooldown. A spell that could be spammed every tick is not a spell.
  for (int i = 0; i < 5; ++i) {
    zcon::InputSnapshot s = idle(g.tick());
    s.pad[0].buttons = 1;
    g.advance(s);
  }
  check(g.bolt(0).owner == 0, "still the same bolt, not a re-cast", 0,
        g.bolt(0).owner);
  check(g.bolt(0).x != first_x, "which has kept travelling", 1,
        g.bolt(0).x != first_x ? 1 : 0);
}

void test_death_and_restart() {
  zgame::Wizards g;
  g.reset(0);

  // Three blasts kill: kBlastDamage is a third of health by construction.
  for (int i = 0; i < 600 && g.wizard(1).alive; ++i) {
    zcon::InputSnapshot s = idle(g.tick());
    if (g.wizard(0).cooldown == 0 && !g.bolt(0).active) {
      s.pad[0].buttons = 1;
    }
    g.advance(s);
  }
  check(!g.wizard(1).alive, "the opponent dies", 0, g.wizard(1).alive ? 1 : 0);
  check(g.wizard(1).deaths == 1, "and the death is recorded", 1,
        g.wizard(1).deaths);
  check(g.wizard(1).health == 0, "health floors at zero, never negative", 0,
        g.wizard(1).health);

  // Respawn is simulation, not UI.
  step(g, zgame::Wizards::kRespawnTicks + 1);
  check(g.wizard(1).alive, "and respawns after kRespawnTicks", 1,
        g.wizard(1).alive ? 1 : 0);
  check(g.wizard(1).health == zgame::Wizards::kStartHealth,
        "at full health", zgame::Wizards::kStartHealth, g.wizard(1).health);
  check(g.wizard(1).deaths == 1, "with the death still on the scoreboard", 1,
        g.wizard(1).deaths);
}

void test_ground_is_destructible_and_permanent() {
  zgame::Wizards g;
  g.reset(0);

  int32_t sum_before = 0;
  for (int y = 0; y < zgame::kGroundH; ++y)
    for (int x = 0; x < zgame::kGroundW; ++x) sum_before += g.ground_at(x, y);
  check(sum_before == 0, "the ground starts flat", 0, sum_before);

  zcon::InputSnapshot cast = idle(0);
  cast.pad[0].buttons = 1;
  g.advance(cast);
  for (int i = 0; i < 80 && g.bolt(0).active; ++i) step(g, 1);

  int32_t sum_after = 0;
  for (int y = 0; y < zgame::kGroundH; ++y)
    for (int x = 0; x < zgame::kGroundW; ++x) sum_after += g.ground_at(x, y);
  check(sum_after < 0, "a detonation craters the ground", 1, sum_after < 0 ? 1 : 0);

  // Permanence: 500 quiet ticks must not heal it.
  step(g, 500);
  int32_t sum_later = 0;
  for (int y = 0; y < zgame::kGroundH; ++y)
    for (int x = 0; x < zgame::kGroundW; ++x) sum_later += g.ground_at(x, y);
  check(sum_later == sum_after, "and the crater is PERMANENT", sum_after,
        sum_later);
}

// ---------------------------------------------------------------------------
// The one that matters: a whole match replays exactly.
// ---------------------------------------------------------------------------
class NullBackend : public zcon::Backend {
 public:
  std::vector<zcon::InputSnapshot> scripted;
  zcon::Handle publish(zcon::ResourceKind, const uint8_t*, std::size_t) override {
    return zcon::Handle{};
  }
  zcon::InputSnapshot poll(uint32_t t) override {
    return t < scripted.size() ? scripted[t] : idle(t);
  }
  void submit(const std::vector<uint8_t>&) override {}
  const char* name() const override { return "null"; }
};

void test_a_whole_match_replays_to_an_identical_hash_stream() {
  NullBackend be;
  // A scripted match with movement, casting and enough ticks for kills.
  // The script must actually produce kills, or the replay proves nothing. The
  // first version moved both wizards vertically while bolts fly horizontally
  // (zero aim fires toward the opponent), so every shot missed and the
  // anti-vacuity check below caught it -- which is exactly what it is for.
  // Lateral movement only, and cast whenever the cooldown allows.
  for (int t = 0; t < 900; ++t) {
    zcon::InputSnapshot s = idle(static_cast<uint32_t>(t));
    s.pad[0].stick_lx = static_cast<int8_t>((t * 7) % 11 - 5);
    s.pad[0].stick_ly = static_cast<int8_t>((t % 97 == 0) ? 1 : 0);
    s.pad[0].buttons = static_cast<uint16_t>((t % 31 == 0) ? 1 : 0);
    s.pad[1].stick_lx = static_cast<int8_t>((t * 5) % 9 - 4);
    s.pad[1].stick_ly = static_cast<int8_t>((t % 89 == 0) ? 1 : 0);
    s.pad[1].buttons = static_cast<uint16_t>((t % 33 == 0) ? 1 : 0);
    be.scripted.push_back(s);
  }

  zgame::Wizards truth;
  zcon::Session s(&truth, &be);
  s.start(0xDEADBEEF);
  for (int t = 0; t < 900; ++t) s.tick();

  check(truth.wizard(0).deaths + truth.wizard(1).deaths > 0,
        "the scripted match actually killed somebody -- otherwise this proves "
        "nothing interesting", 1,
        truth.wizard(0).deaths + truth.wizard(1).deaths > 0 ? 1 : 0);

  const int diverged = s.replay_and_compare(s.inputs(), s.hashes());
  check(diverged == -1, "900 ticks of match replay to an IDENTICAL hash stream",
        -1, diverged);
}

}  // namespace

int main() {
  test_opening_position_is_deterministic_and_symmetric();
  test_movement_is_fixed_point_and_clamped();
  test_cast_travel_detonate_and_damage();
  test_cooldown_prevents_a_second_cast();
  test_death_and_restart();
  test_ground_is_destructible_and_permanent();
  test_a_whole_match_replays_to_an_identical_hash_stream();

  if (g_failed) {
    std::printf("[wizards_directed] %d/%d checks FAILED\n", g_failed, g_checks);
    return 1;
  }
  std::printf("[wizards_directed] %d checks passed\n", g_checks);
  return 0;
}
