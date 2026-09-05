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

#ifndef ZGAME_WIZARDS_HPP
#define ZGAME_WIZARDS_HPP

#include <cstdint>
#include <vector>

#include "zcon/zcon.hpp"

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

    w.x += static_cast<int32_t>(pad.stick_lx) * kMoveScale;
    w.y += static_cast<int32_t>(pad.stick_ly) * kMoveScale;
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
      const int32_t n = (ax < 0 ? -ax : ax) + (ay < 0 ? -ay : ay);
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
};

}  // namespace zgame

#endif  // ZGAME_WIZARDS_HPP
