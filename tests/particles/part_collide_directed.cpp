// part_collide_directed.cpp — PART.COLLIDE against what its contract promises.
//
// ---------------------------------------------------------------------------
// THIS IS A SELF-CONSISTENCY TEST, NOT A DIFFERENTIAL ONE. SAY SO FIRST.
// ---------------------------------------------------------------------------
// `design/blocks.yml` declares `reference_model: zref::ParticleCollide` and
// that symbol DOES NOT EXIST anywhere in the tree — it is entry 10 of
// `reports/PHANTOM_REFERENCES.md`. So there is nothing here to difference
// against, and inventing an oracle would be writing the same law twice and
// calling the agreement evidence.
//
// What the checks below therefore are:
//
//   * PROPERTIES the contract states, which hold independently of how the
//     arithmetic is arranged — the anti-jitter re-test, STICK idempotence, the
//     BOUNCE(e=0) == SLIDE(f=0) cross-check, refusal leaving the record
//     byte-identical, the live-terrain case. These are the strong ones: they
//     could fail against a wrong implementation of the same contract.
//   * HAND-COMPUTED outcomes for five specific contacts, worked out on paper
//     from the ratified formats and written into this file as literals. These
//     pin the arithmetic; they are only as good as the hand working, and the
//     hand working is spelled out in the comments so the next reader can
//     disagree with it.
//
// The ratified record codec is `zref::part::particle_pack`/`particle_unpack`
// and every record here goes through it. Hand-written shifts would be a third
// copy of a frozen layout.
//
// The flag bits are ratified too (`kPartStuck`, `kPartCollidedThisTick`,
// `kPartBornThisTick`, `kPartFlagReserved`). PART.COLLIDE.md does not mention
// flags AT ALL, so there is no disagreement to resolve — only a silence, which
// the ratified header fills.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "verilated.h"

#include "Vzhao_part_collide.h"
#include "zhao_sim.hpp"
#include "zref/zref_particle.hpp"

using zhao::check;
using zref::part::Particle128;
using zref::part::particle_pack;
using zref::part::particle_unpack;

namespace {

// ---- the formats, exactly as the RTL parameterises them -------------------
constexpr int32_t kNrmOne = 1 << 10;   // NRM_Q = 10, a unit normal component
constexpr int32_t kFxOne = 1 << 14;    // FX_Q  = 14, an fx16 coefficient of 1.0
constexpr int32_t kEps = 2;            // CLEAR_EPS, in position LSBs (1/256 m)

// The response enum, owner ruling 2026-08-31 §2.3.
constexpr uint8_t kIgnore = 0;
constexpr uint8_t kDie = 1;
constexpr uint8_t kStick = 2;
constexpr uint8_t kSlide = 3;
constexpr uint8_t kBounce = 4;

constexpr uint8_t kFlagStuck = zref::part::kPartStuck;
constexpr uint8_t kFlagHit = zref::part::kPartCollidedThisTick;
constexpr uint8_t kFlagBorn = zref::part::kPartBornThisTick;
constexpr uint8_t kFlagRsvd = zref::part::kPartFlagReserved;

// Two's complement into an N-bit port. Verilator hands every packed port back
// as an unsigned word, so a signed input has to be masked on the way in.
uint32_t bits(int32_t v, int n) {
  return static_cast<uint32_t>(v) & (n >= 32 ? 0xFFFFFFFFu : ((1u << n) - 1u));
}

// One particle's worth of stimulus. Everything the block reads, in one place,
// so a case reads as a scene rather than as twelve assignments.
struct Stim {
  Particle128 p{};
  uint8_t response = kIgnore;
  int32_t restitution = 0;
  int32_t friction = 0;
  int32_t damping = 0;

  bool t_valid = false;
  int32_t t_height = 0;
  int32_t tnx = 0, tny = kNrmOne, tnz = 0;

  bool pl_en = false;
  int32_t pnx = 0, pny = kNrmOne, pnz = 0;
  int32_t pl_c = 0;
};

struct Res {
  Particle128 p{};
  uint64_t lo = 0, hi = 0;
  bool alive = false, contact = false, refused = false;
  uint8_t response = 0;
};

// Snapshot of every counter, so a case can assert a DELTA rather than a total.
// A total is an accumulation of everything that came before it; a delta is the
// claim the case is actually making.
struct Counters {
  uint32_t ignore, die, stick, slide, bounce;
  uint32_t terrain, plane, inside, unavail, refused, clamps;
};

struct Dut {
  Vzhao_part_collide* v;
  explicit Dut(Vzhao_part_collide* d) : v(d) {}

  template <typename W>
  static void put128(W& dst, uint64_t lo, uint64_t hi) {
    dst[0] = static_cast<uint32_t>(lo);
    dst[1] = static_cast<uint32_t>(lo >> 32);
    dst[2] = static_cast<uint32_t>(hi);
    dst[3] = static_cast<uint32_t>(hi >> 32);
  }
  template <typename W>
  static void get128(const W& src, uint64_t* lo, uint64_t* hi) {
    *lo = static_cast<uint64_t>(src[0]) | (static_cast<uint64_t>(src[1]) << 32);
    *hi = static_cast<uint64_t>(src[2]) | (static_cast<uint64_t>(src[3]) << 32);
  }

  void tick() {
    v->clk = 0;
    v->eval();
    v->clk = 1;
    v->eval();
  }

  void apply(const Stim& s) {
    uint64_t lo = 0, hi = 0;
    particle_pack(s.p, &lo, &hi);
    put128(v->p_record_i, lo, hi);
    v->d_response_i = s.response;
    v->d_restitution_i = bits(s.restitution, 16);
    v->d_friction_i = bits(s.friction, 16);
    v->d_damping_i = bits(s.damping, 16);
    v->t_valid_i = s.t_valid ? 1 : 0;
    v->t_height_i = bits(s.t_height, 18);
    v->t_nx_i = bits(s.tnx, 12);
    v->t_ny_i = bits(s.tny, 12);
    v->t_nz_i = bits(s.tnz, 12);
    v->pl_en_i = s.pl_en ? 1 : 0;
    v->pl_nx_i = bits(s.pnx, 12);
    v->pl_ny_i = bits(s.pny, 12);
    v->pl_nz_i = bits(s.pnz, 12);
    v->pl_c_i = bits(s.pl_c, 32);
  }

  void reset() {
    Stim quiet;
    apply(quiet);
    v->p_valid_i = 0;
    v->c_ready_i = 1;
    v->rst_n = 0;
    v->eval();
    tick();
    tick();
    v->rst_n = 1;
    tick();
  }

  // One particle through the block. Fixed latency: accepted on one edge,
  // presented on the next.
  Res run(const Stim& s) {
    apply(s);
    v->p_valid_i = 1;
    v->c_ready_i = 1;
    v->eval();
    check(v->p_ready_o == 1, "p_ready_o high with a drained output", 1, v->p_ready_o);
    tick();  // accept
    v->p_valid_i = 0;
    v->eval();

    Res r;
    check(v->c_valid_o == 1, "one input beat produces one output beat", 1, v->c_valid_o);
    get128(v->c_record_o, &r.lo, &r.hi);
    particle_unpack(r.lo, r.hi, &r.p);
    r.alive = v->c_alive_o != 0;
    r.contact = v->c_contact_o != 0;
    r.refused = v->c_refused_o != 0;
    r.response = v->c_response_o;
    tick();  // retire
    v->eval();
    return r;
  }

  Counters counters() const {
    return Counters{v->contacts_ignore_o,  v->contacts_die_o,
                    v->contacts_stick_o,   v->contacts_slide_o,
                    v->contacts_bounce_o,  v->contacts_terrain_o,
                    v->contacts_plane_o,   v->already_inside_at_entry_o,
                    v->terrain_sample_unavailable_o, v->response_refused_o,
                    v->field_clamps_o};
  }
};

void check_pos(const char* what, const Particle128& p, int32_t x, int32_t y, int32_t z) {
  char b[128];
  std::snprintf(b, sizeof(b), "%s pos.x", what);
  check(p.pos[0] == x, b, static_cast<uint64_t>(x), static_cast<uint64_t>(p.pos[0]));
  std::snprintf(b, sizeof(b), "%s pos.y", what);
  check(p.pos[1] == y, b, static_cast<uint64_t>(y), static_cast<uint64_t>(p.pos[1]));
  std::snprintf(b, sizeof(b), "%s pos.z", what);
  check(p.pos[2] == z, b, static_cast<uint64_t>(z), static_cast<uint64_t>(p.pos[2]));
}

void check_vel(const char* what, const Particle128& p, int32_t x, int32_t y, int32_t z) {
  char b[128];
  std::snprintf(b, sizeof(b), "%s vel.x", what);
  check(p.vel[0] == x, b, static_cast<uint64_t>(x), static_cast<uint64_t>(p.vel[0]));
  std::snprintf(b, sizeof(b), "%s vel.y", what);
  check(p.vel[1] == y, b, static_cast<uint64_t>(y), static_cast<uint64_t>(p.vel[1]));
  std::snprintf(b, sizeof(b), "%s vel.z", what);
  check(p.vel[2] == z, b, static_cast<uint64_t>(z), static_cast<uint64_t>(p.vel[2]));
}

// Every particle below carries the same non-zero decoration in the fields this
// block must not touch, so a dropped or transposed field cannot hide.
Particle128 make(int32_t px, int32_t py, int32_t pz, int32_t vx, int32_t vy, int32_t vz) {
  Particle128 p{};
  p.pos[0] = px; p.pos[1] = py; p.pos[2] = pz;
  p.vel[0] = vx; p.vel[1] = vy; p.vel[2] = vz;
  p.age = 613;
  p.species = 0x55;
  p.size = 37;
  p.spin = 21;
  p.flags = kFlagBorn | kFlagRsvd;   // 0xC: neither is this block's to move
  p.variation = 0xA5;
  return p;
}

void check_untouched(const char* what, const Particle128& p) {
  char b[128];
  std::snprintf(b, sizeof(b), "%s age preserved", what);
  check(p.age == 613, b, 613, p.age);
  std::snprintf(b, sizeof(b), "%s species preserved", what);
  check(p.species == 0x55, b, 0x55, p.species);
  std::snprintf(b, sizeof(b), "%s size preserved", what);
  check(p.size == 37, b, 37, p.size);
  std::snprintf(b, sizeof(b), "%s spin preserved", what);
  check(p.spin == 21, b, 21, p.spin);
  std::snprintf(b, sizeof(b), "%s variation preserved", what);
  check(p.variation == 0xA5, b, 0xA5, p.variation);
  // "zero in, preserved zero" is the reserved bit's whole rule, and the other
  // direction matters just as much: a ONE in must come out, because this block
  // has no business clearing a bit it does not own.
  std::snprintf(b, sizeof(b), "%s kPartFlagReserved survives", what);
  check((p.flags & kFlagRsvd) != 0, b, 1, (p.flags & kFlagRsvd) != 0);
  std::snprintf(b, sizeof(b), "%s kPartBornThisTick survives", what);
  check((p.flags & kFlagBorn) != 0, b, 1, (p.flags & kFlagBorn) != 0);
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Verilated::traceEverOn(false);

  auto* top = new Vzhao_part_collide;   // heap + exit_hard: see zhao_sim.hpp
  Dut d(top);
  d.reset();

  Counters c0 = d.counters();
  check(c0.bounce == 0 && c0.terrain == 0 && c0.refused == 0 && c0.clamps == 0,
        "all counters clear after reset", 0,
        c0.bounce + c0.terrain + c0.refused + c0.clamps);

  // =========================================================================
  // 1. BOUNCE on a flat plane, hand computed.
  //
  //   plane n = (0,1,0), c = 0, so the surface is y = 0 and d = py.
  //   p = (500,-100,-700), v = (300,-200,0), restitution 0.5, damping 1.0.
  //
  //   v.n = -200.  tangential = (300,0,0), normal part = (0,-200,0).
  //   v' = damping*tangential + (-restitution)*normal
  //      = 1.0*(300,0,0) + (-0.5)*(0,-200,0) = (300, 100, 0)
  //   push = EPS - d = 2 - (-100) = 102, along the plane normal:
  //   p' = (500, -100+102, -700) = (500, 2, -700), i.e. sitting at +EPS.
  // =========================================================================
  {
    Stim s;
    s.p = make(500, -100, -700, 300, -200, 0);
    s.response = kBounce;
    s.restitution = kFxOne / 2;
    s.damping = kFxOne;
    s.pl_en = true;                 // plane only; terrain sample absent
    Counters a = d.counters();
    Res r = d.run(s);
    Counters b = d.counters();

    check_vel("BOUNCE/plane", r.p, 300, 100, 0);
    check_pos("BOUNCE/plane", r.p, 500, 2, -700);
    check_untouched("BOUNCE/plane", r.p);
    check(r.alive, "BOUNCE leaves the particle alive", 1, r.alive);
    check(r.contact, "BOUNCE reports a contact", 1, r.contact);
    check(!r.refused, "BOUNCE is not a refusal", 0, r.refused);
    check(r.response == kBounce, "response echoed", kBounce, r.response);
    check((r.p.flags & kFlagHit) != 0, "kPartCollidedThisTick set on contact", 1,
          (r.p.flags & kFlagHit) != 0);
    check((r.p.flags & kFlagStuck) == 0, "BOUNCE does not set kPartStuck", 0,
          (r.p.flags & kFlagStuck) != 0);
    check(b.bounce - a.bounce == 1, "contacts_by_response[BOUNCE] moved", 1, b.bounce - a.bounce);
    check(b.plane - a.plane == 1, "contacts_plane moved", 1, b.plane - a.plane);
    check(b.terrain - a.terrain == 0, "contacts_terrain did not move on a plane hit", 0,
          b.terrain - a.terrain);
    check(b.unavail - a.unavail == 1, "terrain_sample_unavailable moved with t_valid low", 1,
          b.unavail - a.unavail);
  }

  // =========================================================================
  // 2. SLIDE on a heightfield slope, hand computed.
  //
  //   n = (614,819,0)/1024 (~0.6,0.8), h = 0, p = (0,-80,0), v = (0,-300,0),
  //   friction 0.5.
  //
  //   v.n  = -300 * 819/1024 = -239.94
  //   v_n  = v.n * n         = (-143.86, -191.90, 0)
  //   v_t  = v - v_n         = ( 143.86, -108.10, 0)
  //   v'   = 0.5 * v_t       = (  71.93,  -54.05, 0) -> (72, -54, 0)
  //   The heightfield placement is VERTICAL: p'.y = h + EPS = 2, x and z stay.
  // =========================================================================
  {
    Stim s;
    s.p = make(0, -80, 0, 0, -300, 0);
    s.response = kSlide;
    s.friction = kFxOne / 2;
    s.t_valid = true;
    s.t_height = 0;
    s.tnx = 614; s.tny = 819; s.tnz = 0;
    Counters a = d.counters();
    Res r = d.run(s);
    Counters b = d.counters();

    check_vel("SLIDE/terrain slope", r.p, 72, -54, 0);
    check_pos("SLIDE/terrain slope", r.p, 0, kEps, 0);
    check_untouched("SLIDE/terrain slope", r.p);
    check(b.slide - a.slide == 1, "contacts_by_response[SLIDE] moved", 1, b.slide - a.slide);
    check(b.terrain - a.terrain == 1, "contacts_terrain moved", 1, b.terrain - a.terrain);
    check(b.plane - a.plane == 0, "contacts_plane still", 0, b.plane - a.plane);
    check(b.unavail - a.unavail == 0, "terrain_sample_unavailable still when the sample arrived",
          0, b.unavail - a.unavail);
    check(b.inside - a.inside == 0,
          "a particle that fell in THIS tick is not already_inside_at_entry", 0,
          b.inside - a.inside);

    // ---- the ANTI-JITTER property, on the sloped heightfield --------------
    // "the contact point is computed so that re-testing it in the same tick
    // would not report a contact."
    Stim again = s;
    again.p = r.p;
    Res r2 = d.run(again);
    check(!r2.contact, "re-testing the contact point reports NO contact (terrain)", 0,
          r2.contact);
    check(r2.p.pos[1] == kEps, "re-test leaves the particle where it was", kEps, r2.p.pos[1]);
    check((r2.p.flags & kFlagHit) == 0, "kPartCollidedThisTick CLEARED on a tick with no contact",
          0, (r2.p.flags & kFlagHit) != 0);
  }

  // =========================================================================
  // 3. The anti-jitter property on a TILTED PLANE, where the push has three
  //    components and the rounding is not exact.
  //
  //   n = (614,819,0)/1024, c = 0, p = (0,-80,0).
  //   d = p.n = -80*819/1024 = -64.0  (Q10: -65,520)
  //   push = EPS - d = 67,568 Q10, so the displacement is
  //     dx = round(67568*614 / 2^20) = 40,  dy = round(67568*819 / 2^20) = 53
  //   p' = (40, -27, 0) and the re-test is 40*614 - 27*819 = +2,447 > 0.
  // =========================================================================
  {
    Stim s;
    s.p = make(0, -80, 0, 0, -300, 0);
    s.response = kSlide;
    s.friction = kFxOne / 2;
    s.pl_en = true;
    s.pnx = 614; s.pny = 819; s.pnz = 0;
    s.pl_c = 0;
    Res r = d.run(s);
    check_pos("SLIDE/tilted plane", r.p, 40, -27, 0);

    Stim again = s;
    again.p = r.p;
    Res r2 = d.run(again);
    check(!r2.contact, "re-testing the contact point reports NO contact (tilted plane)", 0,
          r2.contact);
  }

  // =========================================================================
  // 4. STICK really stops, and stays stopped.
  //
  //   "zero relative velocity, and the particle is still at the same point many
  //    ticks later." Re-offering the stuck particle must be IDEMPOTENT: the
  //   contact point is outside, so no contact, so nothing moves — and after the
  //   collided-this-tick bit clears, the record is byte-stable forever.
  // =========================================================================
  {
    Stim s;
    s.p = make(0, -50, 0, 0, -300, 0);
    s.response = kStick;
    s.t_valid = true;
    s.t_height = 0;
    Counters a = d.counters();
    Res r = d.run(s);
    Counters b = d.counters();

    check_vel("STICK", r.p, 0, 0, 0);
    check_pos("STICK", r.p, 0, kEps, 0);
    check((r.p.flags & kFlagStuck) != 0, "STICK sets kPartStuck", 1,
          (r.p.flags & kFlagStuck) != 0);
    check(b.stick - a.stick == 1, "contacts_by_response[STICK] moved", 1, b.stick - a.stick);

    Stim again = s;
    again.p = r.p;
    Res settled = d.run(again);
    check(!settled.contact, "a stuck particle does not re-contact", 0, settled.contact);
    check((settled.p.flags & kFlagStuck) != 0, "kPartStuck persists", 1,
          (settled.p.flags & kFlagStuck) != 0);

    uint64_t lo0 = settled.lo, hi0 = settled.hi;
    bool stable = true;
    for (int t = 0; t < 12; ++t) {
      again.p = settled.p;
      settled = d.run(again);
      if (settled.lo != lo0 || settled.hi != hi0) stable = false;
      if (settled.p.pos[1] != kEps || settled.p.vel[1] != 0) stable = false;
    }
    check(stable, "STICK: byte-identical record and zero velocity 12 ticks later", 1, stable);
  }

  // =========================================================================
  // 5. BOUNCE with restitution 0 == SLIDE with friction 0, head on.
  //
  //   Two paths, one answer. Head-on means the tangential part is zero, so
  //   BOUNCE reduces to -0 * v_n and SLIDE to 0 * v_t: both must produce a
  //   dead stop at the same contact point. A sign error in the normal split
  //   breaks one of them and not the other.
  //
  //   The damping is deliberately NOT zero (0.25) so the two expressions are
  //   genuinely different arithmetic that happens to agree.
  // =========================================================================
  {
    Stim base;
    base.p = make(0, -50, 0, 0, -300, 0);
    base.pl_en = true;
    base.pl_c = 0;

    Stim sb = base;
    sb.response = kBounce;
    sb.restitution = 0;
    sb.damping = kFxOne / 4;
    Res rb = d.run(sb);

    Stim ss = base;
    ss.response = kSlide;
    ss.friction = 0;
    Res rs = d.run(ss);

    check(rb.p.vel[0] == 0 && rb.p.vel[1] == 0 && rb.p.vel[2] == 0,
          "BOUNCE(e=0) head-on stops the particle", 0,
          static_cast<uint64_t>(rb.p.vel[1]));
    check(rb.lo == rs.lo, "BOUNCE(e=0) == SLIDE(f=0) low 64 bits", rs.lo, rb.lo);
    check(rb.hi == rs.hi, "BOUNCE(e=0) == SLIDE(f=0) high 64 bits", rs.hi, rb.hi);
  }

  // =========================================================================
  // 6. A particle already inside at entry: resolved AND counted.
  //
  //   p.y = -400 with v.y = -10. One tick of that velocity moves 10 position
  //   LSBs (the two ratified scales agree), so 400 LSBs of penetration cannot
  //   have happened this tick. A previous tick's response should have prevented
  //   it, and counting it is how that regression becomes visible.
  //
  //   The pairing with case 2 is the point: that one penetrated 80 with a
  //   velocity of 300 and must NOT be counted. A counter that fires on both
  //   is not a regression signal, it is a contact signal with a second name.
  // =========================================================================
  {
    Stim s;
    s.p = make(0, -400, 0, 0, -10, 0);
    s.response = kSlide;
    s.friction = kFxOne;
    s.t_valid = true;
    s.t_height = 0;
    Counters a = d.counters();
    Res r = d.run(s);
    Counters b = d.counters();

    check(b.inside - a.inside == 1, "already_inside_at_entry moved", 1, b.inside - a.inside);
    check(r.contact, "already-inside is still RESOLVED, not dropped", 1, r.contact);
    check_pos("already inside", r.p, 0, kEps, 0);
  }

  // =========================================================================
  // 7. Unknown response enum: REFUSED, no motion.
  //
  //   "Never default to IGNORE: a particle that should have died and instead
  //   flew on is a visible bug with no error." So the record comes back
  //   byte-identical AND the particle is not alive — handing it on unchanged
  //   and alive would BE the IGNORE default under another name.
  // =========================================================================
  {
    Stim s;
    s.p = make(0, -50, 0, 0, -300, 0);
    s.response = 5;               // not one of the five
    s.t_valid = true;
    s.t_height = 0;
    uint64_t lo = 0, hi = 0;
    particle_pack(s.p, &lo, &hi);

    Counters a = d.counters();
    Res r = d.run(s);
    Counters b = d.counters();

    check(r.refused, "unknown response is refused", 1, r.refused);
    check(!r.alive, "a refused particle is not alive", 0, r.alive);
    check(!r.contact, "a refused particle reports no contact", 0, r.contact);
    check(r.lo == lo, "refused record byte-identical, low 64", lo, r.lo);
    check(r.hi == hi, "refused record byte-identical, high 64", hi, r.hi);
    check(b.refused - a.refused == 1, "response_refused moved", 1, b.refused - a.refused);
    check(b.slide - a.slide == 0 && b.bounce - a.bounce == 0 && b.ignore - a.ignore == 0,
          "a refusal is not counted as a contact", 0,
          (b.slide - a.slide) + (b.bounce - a.bounce) + (b.ignore - a.ignore));
    check(b.terrain - a.terrain == 0, "a refusal is not counted as a terrain contact", 0,
          b.terrain - a.terrain);
  }

  // =========================================================================
  // 8. DIE removes on first accepted contact, and does not move the particle.
  // 9. IGNORE does nothing at all, and is still counted.
  // =========================================================================
  {
    Stim s;
    s.p = make(0, -50, 0, 0, -300, 0);
    s.response = kDie;
    s.t_valid = true;
    Counters a = d.counters();
    Res r = d.run(s);
    Counters b = d.counters();
    check(!r.alive, "DIE removes the particle", 0, r.alive);
    check(r.contact, "DIE reports the contact it died on", 1, r.contact);
    check_pos("DIE", r.p, 0, -50, 0);
    check_vel("DIE", r.p, 0, -300, 0);
    check(b.die - a.die == 1, "contacts_by_response[DIE] moved", 1, b.die - a.die);

    s.response = kIgnore;
    a = d.counters();
    r = d.run(s);
    b = d.counters();
    check(r.alive, "IGNORE leaves the particle alive", 1, r.alive);
    check_pos("IGNORE", r.p, 0, -50, 0);
    check_vel("IGNORE", r.p, 0, -300, 0);
    check(b.ignore - a.ignore == 1, "contacts_by_response[IGNORE] moved", 1,
          b.ignore - a.ignore);
    check(b.inside - a.inside == 0,
          "IGNORE promises nothing about placement, so it is not an entry regression", 0,
          b.inside - a.inside);
  }

  // =========================================================================
  // 10. Terrain sample unavailable: no contact, counted, no stall.
  // =========================================================================
  {
    Stim s;
    s.p = make(0, -50, 0, 0, -300, 0);
    s.response = kBounce;
    s.restitution = kFxOne / 2;
    s.t_valid = false;            // TERRAIN.PATCH did not answer
    s.pl_en = false;
    uint64_t lo = 0, hi = 0;
    particle_pack(s.p, &lo, &hi);
    Counters a = d.counters();
    Res r = d.run(s);
    Counters b = d.counters();
    check(!r.contact, "an absent terrain sample is no contact", 0, r.contact);
    check(r.alive, "an absent terrain sample does not kill the particle", 1, r.alive);
    check(r.lo == lo && r.hi == hi, "no contact leaves the record alone", lo, r.lo);
    check(b.unavail - a.unavail == 1, "terrain_sample_unavailable moved", 1,
          b.unavail - a.unavail);
  }

  // =========================================================================
  // 11. THE LIVE DEFORMED TERRAIN — the case that justifies the feature.
  //
  //   The same particle, at the same position, against three different surfaces
  //   in three consecutive ticks. Debris must land on the surface that exists
  //   NOW, not on the one that was there before the crater.
  // =========================================================================
  {
    Stim s;
    s.p = make(0, 100, 0, 0, -50, 0);
    s.response = kSlide;
    s.friction = 0;
    s.t_valid = true;

    s.t_height = 0;                       // flat ground, particle is above it
    Res flat = d.run(s);
    check(!flat.contact, "live terrain: no contact above the old surface", 0, flat.contact);
    check(flat.p.pos[1] == 100, "live terrain: nothing moves with no contact", 100,
          static_cast<uint64_t>(flat.p.pos[1]));

    s.t_height = 200;                     // a wave rose THIS tick
    Res risen = d.run(s);
    check(risen.contact, "live terrain: the risen surface contacts", 1, risen.contact);
    check(risen.p.pos[1] == 200 + kEps,
          "live terrain: the particle lands on the NEW surface", 200 + kEps,
          static_cast<uint64_t>(risen.p.pos[1]));

    s.t_height = -300;                    // a crater opened under it
    Res crater = d.run(s);
    check(!crater.contact, "live terrain: a fresh crater removes the contact", 0,
          crater.contact);
    check(crater.p.pos[1] == 100, "live terrain: the particle keeps falling", 100,
          static_cast<uint64_t>(crater.p.pos[1]));
  }

  // =========================================================================
  // 12. A coefficient above 1.0 CLAMPS and says so; it does not wrap.
  //
  //   restitution 1.996 on a head-on approach of -1000 gives +1996, which does
  //   not fit the ratified 11-bit velocity field. Wrapping would put it at
  //   -2100 & 0x7FF -> a particle driven INTO the surface at speed, which reads
  //   as a physics bug rather than a numeric one. So: clamp, keep the sign, and
  //   move a counter.
  // =========================================================================
  {
    Stim s;
    s.p = make(0, -50, 0, 0, -1000, 0);
    s.response = kBounce;
    s.restitution = 32700;        // 1.996 in Q1.14
    s.damping = kFxOne;
    s.pl_en = true;
    Counters a = d.counters();
    Res r = d.run(s);
    Counters b = d.counters();
    check(r.p.vel[1] == 1023, "a superball clamps to the field maximum", 1023,
          static_cast<uint64_t>(r.p.vel[1]));
    check(r.p.vel[1] > 0, "the clamp preserves the SIGN; a wrap would not", 1,
          r.p.vel[1] > 0);
    check(b.clamps - a.clamps == 1, "field_clamps moved", 1, b.clamps - a.clamps);
  }

  // =========================================================================
  // 13. Throughput: one collision test per clock, back to back.
  //
  //   A test that checks WHAT came out cannot see HOW MANY TIMES the machine
  //   did it, and the contract's throughput budget is written against the
  //   second number. Eight particles offered on eight consecutive clocks must
  //   produce eight output beats on eight consecutive clocks.
  // =========================================================================
  {
    Stim s;
    s.p = make(0, -50, 0, 0, -300, 0);
    s.response = kSlide;
    s.friction = kFxOne / 2;
    s.t_valid = true;
    d.apply(s);
    top->p_valid_i = 1;
    top->c_ready_i = 1;

    const int kN = 8;
    int accepted = 0, emitted = 0, clocks = 0;
    for (int guard = 0; guard < 64 && emitted < kN; ++guard) {
      top->p_valid_i = (accepted < kN) ? 1 : 0;
      top->eval();
      const bool acc = top->p_valid_i && top->p_ready_o;
      const bool emi = top->c_valid_o && top->c_ready_i;
      d.tick();
      if (acc) ++accepted;
      if (emi) ++emitted;
      ++clocks;
    }
    check(emitted == kN, "eight particles produce eight output beats", kN, emitted);
    check(clocks == kN + 1, "and take one clock each plus one of latency", kN + 1, clocks);
    top->p_valid_i = 0;
    d.tick();
  }

  // =========================================================================
  // Every counter this block exposes has now been SEEN TO MOVE. A counter
  // asserted zero and never demonstrated is not evidence, so the totals are
  // checked as non-zero rather than merely left alone.
  // =========================================================================
  {
    Counters f = d.counters();
    check(f.ignore > 0, "contacts_ignore fired", 1, f.ignore);
    check(f.die > 0, "contacts_die fired", 1, f.die);
    check(f.stick > 0, "contacts_stick fired", 1, f.stick);
    check(f.slide > 0, "contacts_slide fired", 1, f.slide);
    check(f.bounce > 0, "contacts_bounce fired", 1, f.bounce);
    check(f.terrain > 0, "contacts_terrain fired", 1, f.terrain);
    check(f.plane > 0, "contacts_plane fired", 1, f.plane);
    check(f.inside > 0, "already_inside_at_entry fired", 1, f.inside);
    check(f.unavail > 0, "terrain_sample_unavailable fired", 1, f.unavail);
    check(f.refused > 0, "response_refused fired", 1, f.refused);
    check(f.clamps > 0, "field_clamps fired", 1, f.clamps);

    std::printf("[part_collide_directed] ignore=%u die=%u stick=%u slide=%u bounce=%u "
                "terrain=%u plane=%u inside=%u unavail=%u refused=%u clamps=%u\n",
                f.ignore, f.die, f.stick, f.slide, f.bounce, f.terrain, f.plane,
                f.inside, f.unavail, f.refused, f.clamps);
  }

  zhao::exit_hard(zhao::report_and_exit("part_collide_directed"));
}
