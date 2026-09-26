// wizards.hpp — the first playable's game truth.
// Authored 2026-09-05 (software lane).
//
// ---------------------------------------------------------------------------
// WHAT THIS IS
// ---------------------------------------------------------------------------
// The roadmap names the first playable exactly:
//
//   > Two wizards, two controllers, two views, one creature, one damaging
//   > spell, destructible ground, death and restart.
//
// and is equally exact about the acceptance test:
//
//   > The crucial acceptance test is not how attractive it is. It is that it
//   > uses the same simulation and resource/command interfaces intended for
//   > the console, with no game logic hidden inside the reel renderer.
//
// So this is a `zcon::GameTruth` and nothing else. It has no idea what a
// framebuffer is, it never touches the renderer, and every decision it makes
// is a function of (previous state, input snapshot). That is what makes the
// replay in zcon.hpp able to locate a divergence instead of merely noticing
// one.
//
// ---------------------------------------------------------------------------
// FIXED POINT, BECAUSE THE CONSOLE IS FIXED POINT
// ---------------------------------------------------------------------------
// Positions and health are integers on a 1/256 grid, matching the console's
// fx16 convention (spec/qformats.md). There is no float anywhere in this file
// and there must not be: a desktop float simulation and a console fixed-point
// one would diverge, and the whole point of one shared game truth is that they
// cannot.
//
// ---------------------------------------------------------------------------
// WHAT IS DELIBERATELY NOT HERE YET
// ---------------------------------------------------------------------------
// The creature, and the terrain's connection to the real heightfield. The
// ground here is a small deformable grid with the right OWNERSHIP -- the
// simulation owns it, deformation is canonical and replayable -- so that when
// SW.STREAM lands (G4) this is what feeds it, rather than the terrain growing
// its own private truth. Stated rather than left to be discovered.
//
// ---------------------------------------------------------------------------
// NAVIGATION, ADDED 2026-09-26 (NAVSERVICE)
// ---------------------------------------------------------------------------
// reports/OWNER-DECISION-20260926-I34-NAV.md commissioned a CPU navigation
// query and required it to be reachable by "the runtime interface that Form
// simulation and game AI can actually call". THIS CLASS IS THAT INTERFACE on
// the console side: it is the only `zcon::GameTruth` in the tree, so a
// navigation service nothing here consumed would be exactly the "reference-only
// helper" the decision refuses.
//
// So a wizard's step now asks `zref::nav::Service`: it CANNOT enter an
// impassable cell, and it moves slower where the composed movement cost is
// higher. Both halves matter -- a service whose cost nobody reads is the
// computed-but-unread lane the same decision struck on the FPGA side, one
// layer up.
//
// THE POINTER DEFAULTS TO NULL AND THAT IS A COMPATIBILITY DECISION, not a
// dodge. With no service attached the movement code is byte-identical to the
// pre-2026-09-26 version, so every recorded input stream and state hash stays
// valid and `Session::replay_and_compare` keeps locating divergences. The
// desktop host attaches one (runtime/desktop/desktop_main.cpp), so the shipping
// runtime DOES navigate; a recording made with a service and replayed without
// one will diverge at tick 1, which is the replay machinery reporting a real
// difference in simulation inputs and is correct.
//
// Determinism is preserved because the service is a pure function of
// (canonical terrain, accepted field commands, tick) and holds no clock of its
// own -- the host advances it with `begin_tick` from the same tick counter.

#ifndef ZGAME_WIZARDS_HPP
#define ZGAME_WIZARDS_HPP

#include <cstdint>
#include <vector>

#include "zcon/zcon.hpp"
#include "zref/zref_nav.hpp"

namespace zgame {

// 1/256-m grid, the console's fx16 convention.
constexpr int32_t kOne = 256;

// The destructible ground. Small on purpose: this is the OWNERSHIP boundary,
// not the terrain system. Heights are fx16 metres.
constexpr int kGroundW = 32;
constexpr int kGroundH = 32;

struct Wizard {
  int32_t x = 0, y = 0;      // fx16 position on the ground plane
  int32_t health = 0;        // fx16; 0 means dead
  uint16_t cooldown = 0;     // ticks until the spell may be cast again
  uint16_t deaths = 0;       // survives restart, so a match can be scored
  bool alive = true;
};

// One damaging spell: a bolt that travels and detonates. Modelled as state
// rather than an event so that it is replayable and so that a mid-flight save
// reloads correctly -- an event queue would have made both harder.
struct Bolt {
  bool active = false;
  int32_t x = 0, y = 0;
  int32_t vx = 0, vy = 0;
  uint16_t fuse = 0;   // ticks before it detonates on its own
  uint8_t owner = 0;
};

class Wizards : public zcon::GameTruth {
 public:
  // --- tuning, all fixed point and all named -------------------------------
  static constexpr int32_t kMoveScale = 4;        // stick unit -> fx16 per tick
  static constexpr int32_t kStartHealth = 100 * kOne;
  static constexpr int32_t kBoltSpeed = 3 * kOne;
  static constexpr uint16_t kBoltFuse = 40;       // ticks
  static constexpr uint16_t kCooldown = 30;       // ticks
  static constexpr int32_t kBlastRadius = 4 * kOne;
  static constexpr int32_t kBlastDamage = 34 * kOne;   // three hits kill
  static constexpr int32_t kCraterDepth = kOne / 2;
  static constexpr uint16_t kRespawnTicks = 90;

  // --- navigation tuning, all named ---------------------------------------
  // A cell costing `kNavRefCost` is walked at full speed; one costing twice
  // that at half. The reference is the service's own flat_cost by default, so
  // untouched ground is full speed and only a FIELD slows anyone down.
  static constexpr int32_t kNavRefCost = 1 << 16;  // Q16.16 1.0
  // The speed-up floor. A field may drive the composed cost to zero (the law
  // floors it there), and dividing by it would be a crash in the game loop --
  // this is the clamp, and it also caps how fast a "road" spell can make a
  // wizard, which is a game rule and belongs in a knob rather than in a
  // division's failure mode.
  static constexpr int32_t kNavMinCost = kNavRefCost / 4;  // at most 4x speed

  void reset(uint64_t seed) override {
    seed_ = seed;
    tick_ = 0;
    // Deterministic, symmetric start. No RNG in the opening position: a match
    // that starts differently every run cannot be compared against a replay.
    w_[0] = Wizard{8 * kOne, 16 * kOne, kStartHealth, 0, 0, true};
    w_[1] = Wizard{24 * kOne, 16 * kOne, kStartHealth, 0, 0, true};
    bolt_[0] = Bolt{};
    bolt_[1] = Bolt{};
    respawn_[0] = respawn_[1] = 0;
    ground_.assign(kGroundW * kGroundH, 0);
  }

  void advance(const zcon::InputSnapshot& in) override {
    for (int p = 0; p < 2; ++p) advance_wizard(p, in.pad[p]);
    for (int p = 0; p < 2; ++p) advance_bolt(p);
    ++tick_;
  }

  uint64_t hash() const override {
    uint64_t h = 1469598103934665603ull;
    auto mix32 = [&h](int32_t v) {
      for (int b = 0; b < 4; ++b) {
        h ^= static_cast<uint8_t>((v >> (b * 8)) & 0xFF);
        h *= 1099511628211ull;
      }
    };
    for (int p = 0; p < 2; ++p) {
      mix32(w_[p].x);
      mix32(w_[p].y);
      mix32(w_[p].health);
      mix32(w_[p].cooldown);
      mix32(w_[p].deaths);
      mix32(w_[p].alive ? 1 : 0);
      mix32(bolt_[p].active ? 1 : 0);
      mix32(bolt_[p].x);
      mix32(bolt_[p].y);
      mix32(bolt_[p].fuse);
      mix32(respawn_[p]);
    }
    // The ground is authoritative state: a crater must survive a replay, and
    // leaving it out of the hash is how a terrain desync goes unnoticed.
    for (int32_t g : ground_) mix32(g);
    return h;
  }

  // Presentation work. Reads state, never writes it -- enforced by `const`,
  // which is the cheapest possible version of that boundary.
  void build_commands(std::vector<uint8_t>* out) const override {
    auto put32 = [out](int32_t v) {
      out->push_back(static_cast<uint8_t>(v & 0xFF));
      out->push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
      out->push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
      out->push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    for (int p = 0; p < 2; ++p) {
      out->push_back(w_[p].alive ? 1 : 0);
      put32(w_[p].x);
      put32(w_[p].y);
      put32(w_[p].health);
    }
    for (int p = 0; p < 2; ++p) {
      out->push_back(bolt_[p].active ? 1 : 0);
      put32(bolt_[p].x);
      put32(bolt_[p].y);
    }
  }

  // --- inspection, for tests and for the host ------------------------------
  const Wizard& wizard(int p) const { return w_[p]; }
  const Bolt& bolt(int p) const { return bolt_[p]; }
  int32_t ground_at(int gx, int gy) const {
    if (gx < 0 || gy < 0 || gx >= kGroundW || gy >= kGroundH) return 0;
    return ground_[gy * kGroundW + gx];
  }
  uint32_t tick() const { return tick_; }

  // --- navigation ----------------------------------------------------------
  /**
   * Attach the CPU navigation service. `origin_game` is the point of the game
   * grid that maps to world (0, 0); the conversion is the EXACT `raw << 8` of
   * qformats §2/§9, because the game's 1/256 m grid is height16's step and the
   * service speaks fx16. No rounding exists in the up-conversion, so a wizard
   * standing on a game cell boundary is on a lattice cell boundary too.
   *
   * The service is BORROWED and must outlive this object. Passing nullptr
   * restores the un-navigated movement exactly.
   */
  void set_nav(const zref::nav::Service* nav, int32_t origin_game) {
    nav_ = nav;
    nav_origin_ = origin_game;
  }
  const zref::nav::Service* nav() const { return nav_; }

  /** How many steps navigation has REFUSED. Diagnostic, and it must move. */
  uint32_t nav_refusals() const { return nav_refusals_; }
  /** How many steps navigation has slowed or sped. Diagnostic, and it must move. */
  uint32_t nav_scaled() const { return nav_scaled_; }

  /** The world point (fx16) a game position maps to. Public so a host can
   *  place its terrain against the same mapping instead of guessing it. */
  zref::fx16 world_x(int32_t game_x) const { return zref::fx16{(game_x - nav_origin_) << 8}; }
  zref::fx16 world_z(int32_t game_y) const { return zref::fx16{(game_y - nav_origin_) << 8}; }

 private:
  void advance_wizard(int p, const zcon::PadState& pad) {
    Wizard& w = w_[p];

    if (!w.alive) {
      if (respawn_[p] > 0 && --respawn_[p] == 0) {
        // Restart is part of the slice, so it is simulation, not UI.
        w.x = (p == 0 ? 8 : 24) * kOne;
        w.y = 16 * kOne;
        w.health = kStartHealth;
        w.alive = true;
      }
      return;
    }

    int32_t dx = static_cast<int32_t>(pad.stick_lx) * kMoveScale;
    int32_t dy = static_cast<int32_t>(pad.stick_ly) * kMoveScale;
    if (nav_ != nullptr && nav_->has_terrain()) {
      // Scale by the COMPOSED cost where the wizard stands. Integer only:
      // step * ref / max(cost, floor). Cost 1.0 leaves the step untouched, so
      // untouched ground behaves exactly as it did before navigation existed.
      const zref::nav::Result here = nav_->query(world_x(w.x), world_z(w.y));
      if (here.passable) {
        int32_t c = here.cost < kNavMinCost ? kNavMinCost : here.cost;
        if (c != kNavRefCost) {
          dx = static_cast<int32_t>(static_cast<int64_t>(dx) * kNavRefCost / c);
          dy = static_cast<int32_t>(static_cast<int64_t>(dy) * kNavRefCost / c);
          if (dx != 0 || dy != 0) ++nav_scaled_;
        }
      }
      // Axis-separated, so a wizard walking into a wall SLIDES along it rather
      // than stopping dead -- the diagonal is two independent tests, which is
      // also why a corner cannot trap anyone.
      if (!nav_step_ok(w.x + dx, w.y)) {
        dx = 0;
        ++nav_refusals_;
      }
      if (!nav_step_ok(w.x + dx, w.y + dy)) {
        dy = 0;
        ++nav_refusals_;
      }
    }
    w.x += dx;
    w.y += dy;
    clamp_to_ground(&w.x, &w.y);

    if (w.cooldown > 0) --w.cooldown;

    // Button 0 casts, if off cooldown and the bolt slot is free.
    if ((pad.buttons & 1u) && w.cooldown == 0 && !bolt_[p].active) {
      Bolt& b = bolt_[p];
      b.active = true;
      b.x = w.x;
      b.y = w.y;
      // Aim with the right stick; a zero stick fires toward the opponent, so
      // the spell is always castable and a replay never depends on aim noise.
      int32_t ax = static_cast<int32_t>(pad.stick_rx);
      int32_t ay = static_cast<int32_t>(pad.stick_ry);
      if (ax == 0 && ay == 0) {
        ax = (w_[1 - p].x > w.x) ? 1 : -1;
        ay = 0;
      }
      // Normalise on the L1 norm: no square root, no float, and monotone in
      // the stick -- exact in integers, which is what a replay needs.
      const int32_t mag = (ax < 0 ? -ax : ax) + (ay < 0 ? -ay : ay);
      // `mag` cannot be 0: the branch above replaces a dead stick with
      // ax = +/-1. The clamp is therefore unreachable, and it is written down
      // rather than reasoned about because the thing it protects is a DIVIDE --
      // if that guard is ever narrowed, the failure is a crash in the game
      // loop, not a wrong bolt. Static analysis could not see the connection
      // either, and said so.
      const int32_t n = mag > 0 ? mag : 1;
      b.vx = (ax * kBoltSpeed) / n;
      b.vy = (ay * kBoltSpeed) / n;
      b.fuse = kBoltFuse;
      b.owner = static_cast<uint8_t>(p);
      w.cooldown = kCooldown;
    }
  }

  void advance_bolt(int p) {
    Bolt& b = bolt_[p];
    if (!b.active) return;
    b.x += b.vx;
    b.y += b.vy;

    bool detonate = false;
    if (b.x < 0 || b.y < 0 || b.x >= kGroundW * kOne || b.y >= kGroundH * kOne)
      detonate = true;
    if (b.fuse == 0 || --b.fuse == 0) detonate = true;

    // Contact with a living wizard other than the caster.
    for (int q = 0; q < 2 && !detonate; ++q) {
      if (q == static_cast<int>(b.owner) || !w_[q].alive) continue;
      if (dist_l1(b.x, b.y, w_[q].x, w_[q].y) <= kOne) detonate = true;
    }

    if (detonate) {
      explode(b.x, b.y, b.owner);
      b = Bolt{};
    }
  }

  void explode(int32_t x, int32_t y, uint8_t owner) {
    // Damage every living wizard inside the blast, including the caster: self
    // damage is deliberate, because a spell that cannot hurt its owner teaches
    // players nothing about placement.
    (void)owner;
    for (int q = 0; q < 2; ++q) {
      if (!w_[q].alive) continue;
      if (dist_l1(x, y, w_[q].x, w_[q].y) > kBlastRadius) continue;
      w_[q].health -= kBlastDamage;
      if (w_[q].health <= 0) {
        w_[q].health = 0;
        w_[q].alive = false;
        ++w_[q].deaths;
        respawn_[q] = kRespawnTicks;
      }
    }
    // Destructible ground: a permanent crater. Canonical simulation state, so
    // it is hashed, replayed and (in G4) written back by SW.STREAM rather than
    // owned privately by the terrain renderer.
    const int cx = x / kOne, cy = y / kOne;
    const int r = kBlastRadius / kOne;
    for (int gy = cy - r; gy <= cy + r; ++gy) {
      for (int gx = cx - r; gx <= cx + r; ++gx) {
        if (gx < 0 || gy < 0 || gx >= kGroundW || gy >= kGroundH) continue;
        const int dx = gx - cx, dy = gy - cy;
        if (dx * dx + dy * dy > r * r) continue;
        ground_[gy * kGroundW + gx] -= kCraterDepth;
      }
    }
  }

  static int32_t dist_l1(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
    const int32_t dx = ax - bx, dy = ay - by;
    return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
  }

  /**
   * HARD PASSABILITY, and nothing else decides it. The cost is not consulted
   * here at all: a field that makes ground free must never open a hole, and
   * that is `zref::fieldir::compose_nav`'s own rule rather than this class's
   * opinion. A destination the service cannot answer for (kOut, kNoTerrain)
   * is REFUSED -- the flattering reading would be "no data, let them through",
   * and that walks a wizard off the island.
   */
  bool nav_step_ok(int32_t gx, int32_t gy) const {
    int32_t cx = gx, cy = gy;
    clamp_to_ground(&cx, &cy);
    return nav_->query(world_x(cx), world_z(cy)).passable;
  }

  static void clamp_to_ground(int32_t* x, int32_t* y) {
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    if (*x > (kGroundW - 1) * kOne) *x = (kGroundW - 1) * kOne;
    if (*y > (kGroundH - 1) * kOne) *y = (kGroundH - 1) * kOne;
  }

  Wizard w_[2];
  Bolt bolt_[2];
  uint16_t respawn_[2] = {0, 0};
  std::vector<int32_t> ground_;
  uint64_t seed_ = 0;
  uint32_t tick_ = 0;

  // Navigation is BORROWED state and is deliberately NOT hashed: the service is
  // the host's, is a pure function of terrain/fields/tick, and hashing a
  // pointer would make the hash stream depend on an allocation address. What
  // IS hashed is where the wizards ended up, which is the thing navigation
  // changes and the thing a replay must reproduce.
  const zref::nav::Service* nav_ = nullptr;
  int32_t nav_origin_ = 0;
  uint32_t nav_refusals_ = 0;
  uint32_t nav_scaled_ = 0;
};

}  // namespace zgame

#endif  // ZGAME_WIZARDS_HPP
