// part_update_directed.cpp — PART.UPDATE against the cases its contract names.
//
// ---------------------------------------------------------------------------
// THIS IS A SELF-CONSISTENCY TEST, NOT A DIFFERENTIAL ONE. SAY IT OUT LOUD.
// ---------------------------------------------------------------------------
// `design/blocks.yml` declares `reference_model: zref::ParticleUpdate` for this
// block. THAT SYMBOL DOES NOT EXIST anywhere in the tree — it is a phantom of
// the kind `reports/PHANTOM_REFERENCES.md` catalogues. So every expected number
// below is HAND-COMPUTED from the formulae in `zhao_part_update.sv`'s own
// header and written here as a literal.
//
// That is weaker than a differential test and it is the honest maximum
// available today. Writing a `zref::ParticleUpdate` to compare against would be
// a second implementation by the same hand on the same afternoon, which agrees
// with the first for the same reasons it is wrong — the tree already has a
// chapter about a codec "verified against a transcription of itself".
//
// What IS ratified and IS used differentially: the particle128 codec. Every
// stimulus record is built by `zref::part::particle_pack` and every result is
// read back by `zref::part::particle_unpack`, so the FIELD LAYOUT half of this
// block is checked against the oracle even though the ARITHMETIC half cannot be.
//
// `tests/particles/part_update_random.cpp`, which the ledger also names, is NOT
// built. A randomized differential needs the reference that does not exist; a
// randomized self-comparison would be a loop asserting the RTL equals itself.
//
// ---------------------------------------------------------------------------
// THE CASES ARE THE CONTRACT'S OWN LIST
// ---------------------------------------------------------------------------
//   * each of the twelve recipes in isolation against a hand-computed step;
//   * THE EIGHT-STEP ORDER, proved by a case where two orderings differ: drag
//     and a strong recipe together, where drag-before-recipe and
//     drag-after-recipe give measurably different velocities. Both answers are
//     computed here and the RTL is required to match one and NOT the other;
//   * velocity saturation: clamps, counts, and does not wrap — the sign is
//     asserted, which is what a wrap would destroy;
//   * age at 2^10 − 1 then one more tick: dies;
//   * unknown recipe id: refused, and no motion applied;
//   * a Field/FLOW acceleration at its bound, and at zero, giving results
//     identical to the no-Field path when zero.
//
// Plus the thing the contract does not list and the ledger's rules demand:
// EVERY COUNTER IS FIRED. `velocity_saturations_o`, `position_saturations_o`,
// `particles_died_by_age_o`, `particles_refused_o`, `collisions_applied_o` and
// all twelve histogram bins are each seen to move, because a counter asserted
// zero and never watched is a claim.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Vzhao_part_update.h"
#include "zhao_sim.hpp"
#include "zref/zref_particle.hpp"

using zhao::check;
using zref::part::Particle128;

namespace {

constexpr int kSpeciesN = 8;  // -GSPECIES_N=8: an out-of-range species is reachable

// Two's-complement field packers for the narrow signed ports. Verilator hands
// an s11 port as an 11-bit unsigned storage word, so the sign has to be put
// there by hand; getting this wrong reads as a physics bug, which is the exact
// failure the saturate-don't-wrap rule exists to prevent.
uint32_t u11(int v) { return static_cast<uint32_t>(v) & 0x7FFu; }
uint32_t u18(int v) { return static_cast<uint32_t>(v) & 0x3FFFFu; }

// One particle plus everything the block reads about it in the accept cycle.
struct Stim {
  Particle128 p{};
  uint8_t recipe = 0;
  uint16_t lifetime = 0;  // 0 = unbounded
  uint16_t age_mark = 0;  // 0 = no marker
  uint8_t drag = 0;
  int grav = 0;
  int strength = 64;
  int cx = 0, cy = 0, cz = 0;
  int p0 = 100, p1 = -50, p2 = 25;
  bool fld_valid = false;
  int fax = 0, fay = 0, faz = 0;
  bool col_valid = false;
  int cvx = 0, cvy = 0, cvz = 0;
  uint8_t crv_size = 0;
  uint8_t crv_colour = 0;
};

struct Result {
  Particle128 p{};
  bool survive = false;
  bool refused = false;
  bool colour_en = false;
  uint8_t colour = 0;
  uint8_t events = 0;
  uint8_t spc_index = 0;
  uint8_t crv_index = 0;
  uint64_t rec_lo = 0, rec_hi = 0;
};

struct Counters {
  uint32_t updated, refused, died, vsat, psat, collisions;
};

Vzhao_part_update* g = nullptr;

void tick() {
  g->clk = 0;
  g->eval();
  g->clk = 1;
  g->eval();
}

void idle() {
  g->in_valid_i = 0;
  g->out_ready_i = 0;
  g->fld_valid_i = 0;
  g->col_valid_i = 0;
  g->hist_sel_i = 0;
  g->eval();
}

void reset() {
  idle();
  g->rst_n = 0;
  tick();
  tick();
  g->rst_n = 1;
  tick();
  idle();
}

void apply(const Stim& s) {
  uint64_t lo = 0, hi = 0;
  zref::part::particle_pack(s.p, &lo, &hi);
  g->in_record_i[0] = static_cast<uint32_t>(lo);
  g->in_record_i[1] = static_cast<uint32_t>(lo >> 32);
  g->in_record_i[2] = static_cast<uint32_t>(hi);
  g->in_record_i[3] = static_cast<uint32_t>(hi >> 32);

  g->spc_recipe_i = s.recipe;
  g->spc_lifetime_i = s.lifetime;
  g->spc_age_mark_i = s.age_mark;
  g->spc_drag_i = s.drag;
  g->spc_grav_i = static_cast<uint16_t>(u11(s.grav));
  g->spc_strength_i = static_cast<uint16_t>(u11(s.strength));
  g->spc_cx_i = u18(s.cx);
  g->spc_cy_i = u18(s.cy);
  g->spc_cz_i = u18(s.cz);
  g->spc_p0_i = static_cast<uint16_t>(u11(s.p0));
  g->spc_p1_i = static_cast<uint16_t>(u11(s.p1));
  g->spc_p2_i = static_cast<uint16_t>(u11(s.p2));

  g->fld_valid_i = s.fld_valid ? 1 : 0;
  g->fld_ax_i = static_cast<uint16_t>(u11(s.fax));
  g->fld_ay_i = static_cast<uint16_t>(u11(s.fay));
  g->fld_az_i = static_cast<uint16_t>(u11(s.faz));

  g->col_valid_i = s.col_valid ? 1 : 0;
  g->col_vx_i = static_cast<uint16_t>(u11(s.cvx));
  g->col_vy_i = static_cast<uint16_t>(u11(s.cvy));
  g->col_vz_i = static_cast<uint16_t>(u11(s.cvz));

  g->crv_size_i = s.crv_size;
  g->crv_colour_i = s.crv_colour;
}

// Push one particle through and read its verdict. The consumer is held SHUT
// while the result is read, which is also the only way the stall is exercised
// at all: `in_ready_o` must fall while a result is waiting.
Result run_one(const Stim& s) {
  Result r;
  apply(s);
  g->in_valid_i = 1;
  g->out_ready_i = 0;
  g->eval();

  // The species and curve indices are combinational from the record on the
  // wire, so they are read HERE -- before the edge -- which is the cycle the
  // table has to answer in.
  r.spc_index = g->spc_index_o;
  r.crv_index = g->crv_index_o;

  tick();  // accept
  g->in_valid_i = 0;
  g->eval();

  r.rec_lo =
      static_cast<uint64_t>(g->out_record_o[0]) | (static_cast<uint64_t>(g->out_record_o[1]) << 32);
  r.rec_hi =
      static_cast<uint64_t>(g->out_record_o[2]) | (static_cast<uint64_t>(g->out_record_o[3]) << 32);
  zref::part::particle_unpack(r.rec_lo, r.rec_hi, &r.p);
  r.survive = g->out_survive_o != 0;
  r.refused = g->out_refused_o != 0;
  r.colour_en = g->out_colour_en_o != 0;
  r.colour = g->out_colour_o;
  r.events = g->out_events_o;

  g->out_ready_i = 1;
  g->eval();
  tick();  // drain
  idle();
  return r;
}

Counters snap() {
  return Counters{g->particles_updated_o,    g->particles_refused_o,    g->particles_died_by_age_o,
                  g->velocity_saturations_o, g->position_saturations_o, g->collisions_applied_o};
}

uint32_t hist(int bin) {
  g->hist_sel_i = static_cast<uint8_t>(bin);
  g->eval();
  return g->hist_val_o;
}

// The baseline particle. Every value distinct and none of them at a boundary,
// so a transposed axis cannot alias and a wrong sign is visible.
Stim baseline() {
  Stim s;
  s.p.pos[0] = 1024;
  s.p.pos[1] = 512;
  s.p.pos[2] = -256;
  s.p.vel[0] = 16;
  s.p.vel[1] = -8;
  s.p.vel[2] = 4;
  s.p.age = 5;
  s.p.species = 1;
  s.p.size = 20;
  s.p.spin = 9;
  s.p.flags = 0;
  s.p.variation = 0x5A;
  return s;
}

void check_vec(const char* tag, const char* what, const int32_t got[3], int e0, int e1, int e2) {
  char buf[200];
  std::snprintf(buf, sizeof(buf), "%s %s x", tag, what);
  check(got[0] == e0, buf, static_cast<uint64_t>(static_cast<int64_t>(e0)),
        static_cast<uint64_t>(static_cast<int64_t>(got[0])));
  std::snprintf(buf, sizeof(buf), "%s %s y", tag, what);
  check(got[1] == e1, buf, static_cast<uint64_t>(static_cast<int64_t>(e1)),
        static_cast<uint64_t>(static_cast<int64_t>(got[1])));
  std::snprintf(buf, sizeof(buf), "%s %s z", tag, what);
  check(got[2] == e2, buf, static_cast<uint64_t>(static_cast<int64_t>(e2)),
        static_cast<uint64_t>(static_cast<int64_t>(got[2])));
}

// One row of the twelve-recipe table: the recipe, and the velocity and position
// the header's formula produces for `baseline()`. Every one of these numbers was
// computed by hand from the formula and the round-half-away-from-zero rule.
struct RecipeCase {
  uint8_t id;
  const char* name;
  int vx, vy, vz;
  int px, py, pz;
};

const RecipeCase kRecipes[12] = {
    // id  name           velocity after         position after
    {0, "integrate", 16, -8, 4, 1040, 504, -252},
    {1, "gravity", 16, 56, 4, 1040, 568, -252},
    {2, "linear drag", 12, -6, 3, 1036, 506, -253},
    {3, "attraction", -240, -136, 68, 784, 376, -188},
    {4, "repulsion", 272, 120, -60, 1296, 632, -316},
    {5, "orbit", 80, -8, 260, 1104, 504, 4},
    {6, "vortex", -48, 8, 292, 976, 520, 36},
    {7, "wind", 41, -21, 10, 1065, 491, -246},
    {8, "shockwave", 1023, 1016, -508, 2047, 1528, -764},
    {9, "spline flow", 37, -19, 9, 1061, 493, -247},
    {10, "colour curve", 16, -8, 4, 1040, 504, -252},
    {11, "size curve", 16, -8, 4, 1040, 504, -252},
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Verilated::traceEverOn(false);
  g = new Vzhao_part_update;  // heap + exit_hard: see zhao_sim.hpp
  reset();

  // =========================================================================
  // 1. THE TWELVE RECIPES, EACH AGAINST A HAND-COMPUTED STEP.
  // =========================================================================
  for (int k = 0; k < 12; ++k) {
    const RecipeCase& rc = kRecipes[k];
    Stim s = baseline();
    s.recipe = rc.id;
    s.crv_size = 63;
    s.crv_colour = 0xC3;
    const Result r = run_one(s);

    char tag[64];
    std::snprintf(tag, sizeof(tag), "recipe %u (%s)", rc.id, rc.name);
    check_vec(tag, "velocity", r.p.vel, rc.vx, rc.vy, rc.vz);
    check_vec(tag, "position", r.p.pos, rc.px, rc.py, rc.pz);

    char what[160];
    std::snprintf(what, sizeof(what), "%s: age advanced by exactly one tick", tag);
    check(r.p.age == 6, what, 6, r.p.age);
    std::snprintf(what, sizeof(what), "%s: survives", tag);
    check(r.survive && !r.refused, what, 1, (r.survive && !r.refused) ? 1 : 0);
    std::snprintf(what, sizeof(what), "%s: species is carried, never rewritten", tag);
    check(r.p.species == 1, what, 1, r.p.species);
    std::snprintf(what, sizeof(what), "%s: variation is carried (stateless randomness)", tag);
    check(r.p.variation == 0x5A, what, 0x5A, r.p.variation);
    std::snprintf(what, sizeof(what), "%s: spin is the renderer's and is untouched", tag);
    check(r.p.spin == 9, what, 9, r.p.spin);

    // Step 7. Only recipe 11 takes the size curve and only recipe 10 the colour.
    const uint8_t want_size = (rc.id == 11) ? 63 : 20;
    std::snprintf(what, sizeof(what), "%s: size curve applies ONLY at recipe 11", tag);
    check(r.p.size == want_size, what, want_size, r.p.size);
    const bool want_cen = (rc.id == 10);
    std::snprintf(what, sizeof(what), "%s: colour curve enables ONLY at recipe 10", tag);
    check(r.colour_en == want_cen, what, want_cen ? 1 : 0, r.colour_en ? 1 : 0);
    std::snprintf(what, sizeof(what), "%s: colour value", tag);
    check(r.colour == (want_cen ? 0xC3 : 0x00), what, want_cen ? 0xC3 : 0x00, r.colour);

    std::snprintf(what, sizeof(what), "%s: no events on an ordinary tick", tag);
    check(r.events == 0, what, 0, r.events);
  }

  // Shockwave is the saturation case, and the property that matters is the SIGN.
  // A wrap of +2064 into s11 gives -2032; a clamp gives +1023. Those differ in
  // sign, which is exactly what the contract says to assert.
  {
    const Counters c = snap();
    check(c.vsat >= 1,
          "velocity_saturations_o MOVED on shockwave -- the counter is a "
          "detector, not a hopeful zero",
          1, c.vsat >= 1 ? 1 : 0);
    check(kRecipes[8].vx == 1023 && kRecipes[8].vx > 0,
          "shockwave's saturated axis kept its SIGN: a wrap of 2064 would read -2032", 1023,
          kRecipes[8].vx);
  }

  // The histogram: twelve bins, each seen once, and the unknown bins still zero.
  {
    int moved = 0;
    for (int b = 0; b < 12; ++b) {
      char what[120];
      std::snprintf(what, sizeof(what), "recipe_histogram[%d] counted its one particle", b);
      const uint32_t v = hist(b);
      check(v == 1, what, 1, v);
      if (v == 1) ++moved;
    }
    check(moved == 12, "all twelve histogram bins moved", 12, moved);
    for (int b = 12; b < 16; ++b) {
      char what[120];
      std::snprintf(what, sizeof(what), "recipe_histogram[%d] is unreachable and reads zero", b);
      check(hist(b) == 0, what, 0, hist(b));
    }
  }

  // =========================================================================
  // 2. THE EIGHT-STEP ORDER. Drag is applied to the OLD velocity and the
  //    recipe's acceleration is added AFTER it. Both answers are computed here
  //    and the RTL must match one and NOT the other -- a check that asserted
  //    only the right answer could not tell you the wrong one was reachable.
  // =========================================================================
  {
    Stim s = baseline();
    s.recipe = 4;  // repulsion, a strong recipe
    s.drag = 128;  // half the velocity survives
    const Result r = run_one(s);

    // drag(vel) = (8, -4, 2); acc = (256, 128, -64)
    check_vec("order", "drag BEFORE recipe, velocity", r.p.vel, 264, 124, -62);
    check_vec("order", "drag BEFORE recipe, position", r.p.pos, 1288, 636, -318);

    // drag(vel + acc) would be (136, 60, -30). If the RTL produced this, the
    // steps are swapped and every trajectory in the frame is wrong.
    const bool swapped = (r.p.vel[0] == 136 && r.p.vel[1] == 60 && r.p.vel[2] == -30);
    check(!swapped,
          "and NOT drag-after-recipe (136, 60, -30) -- the two orderings are "
          "measurably different on this particle, which is what makes the step "
          "order testable at all",
          0, swapped ? 1 : 0);
  }

  // =========================================================================
  // 3. THE FIELD/FLOW SAMPLE. At zero it must be bit-identical to no Field at
  //    all; at its bound it adds.
  // =========================================================================
  {
    Stim a = baseline();
    a.recipe = 0;
    const Result no_field = run_one(a);

    Stim b = baseline();
    b.recipe = 0;
    b.fld_valid = true;
    b.fax = 0;
    b.fay = 0;
    b.faz = 0;
    const Result zero_field = run_one(b);

    check(no_field.rec_lo == zero_field.rec_lo && no_field.rec_hi == zero_field.rec_hi,
          "a Field acceleration of ZERO gives a BIT-IDENTICAL record to no Field", 1,
          (no_field.rec_lo == zero_field.rec_lo && no_field.rec_hi == zero_field.rec_hi) ? 1 : 0);

    Stim c = baseline();
    c.recipe = 0;
    c.fld_valid = true;
    c.fax = 300;
    c.fay = -200;
    c.faz = 100;
    const Result with_field = run_one(c);
    check_vec("field", "velocity", with_field.p.vel, 316, -208, 104);
    check_vec("field", "position", with_field.p.pos, 1340, 304, -152);
  }

  // The always-applied species gravity, which is a different port from the
  // gravity RECIPE and must not be confused with it.
  {
    Stim s = baseline();
    s.recipe = 0;
    s.grav = -30;
    const Result r = run_one(s);
    check_vec("spc_grav", "velocity", r.p.vel, 16, -38, 4);
    check_vec("spc_grav", "position", r.p.pos, 1040, 474, -252);
  }

  // =========================================================================
  // 4. THE COLLISION SLOT. The response is PART.COLLIDE's; this block places it
  //    at step 6, AFTER the integration, so the tick's displacement uses the
  //    PRE-collision velocity.
  // =========================================================================
  {
    const Counters before = snap();
    Stim s = baseline();
    s.recipe = 4;
    s.col_valid = true;
    s.cvx = -100;
    s.cvy = 200;
    s.cvz = -300;
    const Result r = run_one(s);

    check_vec("collision", "velocity is the response, verbatim", r.p.vel, -100, 200, -300);
    check_vec("collision", "position used the PRE-collision velocity", r.p.pos, 1296, 632, -316);
    check(r.events == 0x4, "collision event (bit 2) and nothing else", 0x4, r.events);
    check((r.p.flags & zref::part::kPartCollidedThisTick) != 0,
          "kPartCollidedThisTick is set on the record", 1,
          (r.p.flags & zref::part::kPartCollidedThisTick) ? 1 : 0);
    const Counters after = snap();
    check(after.collisions - before.collisions == 1, "collisions_applied_o MOVED", 1,
          after.collisions - before.collisions);
  }

  // =========================================================================
  // 5. AGE AND LIFETIME.
  // =========================================================================
  {
    const Counters before = snap();

    // The field width ends the particle whatever the species says.
    Stim s = baseline();
    s.recipe = 4;  // a strong recipe, to prove it is NOT applied
    s.p.age = 1023;
    const Result r = run_one(s);
    check(!r.survive, "age 2^10-1 then one more tick: the particle DIES", 0, r.survive ? 1 : 0);
    check(r.events == 0x8, "the death event (bit 3) fires", 0x8, r.events);
    check(r.p.age == 1023, "age does not wrap past the field", 1023, r.p.age);
    check_vec("dead", "a dead particle emits NO further motion, velocity", r.p.vel, 16, -8, 4);
    check_vec("dead", "a dead particle emits NO further motion, position", r.p.pos, 1024, 512,
              -256);

    // One tick short of the species lifetime: alive.
    Stim t = baseline();
    t.p.age = 8;
    t.lifetime = 10;
    const Result rt = run_one(t);
    check(rt.survive, "age 8 against lifetime 10: alive", 1, rt.survive ? 1 : 0);
    check(rt.p.age == 9, "and advanced to 9", 9, rt.p.age);

    // The tick that reaches it: dead.
    Stim u = baseline();
    u.p.age = 9;
    u.lifetime = 10;
    const Result ru = run_one(u);
    check(!ru.survive, "age 9 reaching lifetime 10: DIES", 0, ru.survive ? 1 : 0);
    check(ru.p.age == 10, "with the age still advanced", 10, ru.p.age);

    const Counters after = snap();
    check(after.died - before.died == 2,
          "particles_died_by_age_o MOVED twice -- field width and species lifetime", 2,
          after.died - before.died);
  }

  // The bounded age marker, and the birth event that fires exactly once.
  {
    Stim s = baseline();
    s.p.age = 5;
    s.age_mark = 6;
    const Result r = run_one(s);
    check(r.events == 0x2, "the age-marker event (bit 1) fires on the exact tick", 0x2, r.events);

    Stim b = baseline();
    b.p.flags = zref::part::kPartBornThisTick;
    const Result rb = run_one(b);
    check(rb.events == 0x1, "the birth event (bit 0) fires", 0x1, rb.events);
    check((rb.p.flags & zref::part::kPartBornThisTick) == 0,
          "and kPartBornThisTick is CLEARED, so it can only fire once", 0,
          rb.p.flags & zref::part::kPartBornThisTick);

    // kPartStuck is carried; kPartFlagReserved is zero in and zero out.
    Stim k = baseline();
    k.p.flags = zref::part::kPartStuck;
    const Result rk = run_one(k);
    check(rk.p.flags == zref::part::kPartStuck,
          "kPartStuck is carried and kPartFlagReserved stays zero (amendment C2)",
          zref::part::kPartStuck, rk.p.flags);
  }

  // =========================================================================
  // 6. REFUSALS. "Never fall back to integrate -- a silently different motion
  //    is worse than a missing particle."
  // =========================================================================
  {
    const Counters before = snap();

    Stim s = baseline();
    s.recipe = 12;  // outside the closed vocabulary
    s.p.pos[0] = 1024;
    uint64_t lo = 0, hi = 0;
    zref::part::particle_pack(s.p, &lo, &hi);
    const Result r = run_one(s);
    check(r.refused, "an unknown recipe id REFUSES the particle", 1, r.refused ? 1 : 0);
    check(!r.survive, "and it does not survive", 0, r.survive ? 1 : 0);
    check(r.rec_lo == lo && r.rec_hi == hi,
          "and the record leaves BIT-IDENTICAL -- no motion applied, not even the age", 1,
          (r.rec_lo == lo && r.rec_hi == hi) ? 1 : 0);

    Stim t = baseline();
    t.p.species = kSpeciesN + 1;  // outside the table
    t.recipe = 0;
    uint64_t tlo = 0, thi = 0;
    zref::part::particle_pack(t.p, &tlo, &thi);
    const Result rt = run_one(t);
    check(rt.refused, "an out-of-range species REFUSES the particle", 1, rt.refused ? 1 : 0);
    check(rt.rec_lo == tlo && rt.rec_hi == thi, "and that record is bit-identical too", 1,
          (rt.rec_lo == tlo && rt.rec_hi == thi) ? 1 : 0);
    check(rt.spc_index == kSpeciesN + 1,
          "spc_index_o still offered the index -- refusal is this block's verdict, not a "
          "silent table miss",
          kSpeciesN + 1, rt.spc_index);

    const Counters after = snap();
    check(after.refused - before.refused == 2, "particles_refused_o MOVED twice", 2,
          after.refused - before.refused);
    check(after.updated == before.updated, "and neither refusal was counted as an update",
          before.updated, after.updated);
  }

  // =========================================================================
  // 7. POSITION SATURATION. Not in the contract's table; clamped and counted
  //    here on the contract's own reasoning, so the counter is fired too.
  // =========================================================================
  {
    const Counters before = snap();
    Stim s = baseline();
    s.recipe = 0;
    s.p.pos[0] = 131000;
    s.p.pos[1] = 0;
    s.p.pos[2] = 0;
    s.p.vel[0] = 1000;
    s.p.vel[1] = 0;
    s.p.vel[2] = 0;
    const Result r = run_one(s);
    check(r.p.pos[0] == 131071,
          "position CLAMPS at the s18 rail instead of wrapping to the far edge", 131071,
          static_cast<uint64_t>(static_cast<int64_t>(r.p.pos[0])));
    const Counters after = snap();
    check(after.psat - before.psat == 1, "position_saturations_o MOVED", 1,
          after.psat - before.psat);
  }

  // =========================================================================
  // 8. THE CURVE INDEX is the ADVANCED age's bucket, answered in the accept
  //    cycle. A curve read off the OLD age is one tick stale in every frame.
  // =========================================================================
  {
    Stim s = baseline();
    s.p.age = 200;  // 201 >> 6 == 3
    s.recipe = 11;
    s.crv_size = 41;
    const Result r = run_one(s);
    check(r.crv_index == 3, "crv_index_o is the ADVANCED age's bucket", 3, r.crv_index);
    check(r.p.size == 41, "and the size curve's value landed in the record", 41, r.p.size);
    check(r.spc_index == 1, "spc_index_o is the record's own species", 1, r.spc_index);
  }

  // =========================================================================
  // 9. HANDSHAKE HYGIENE. `in_ready_o` must fall while a result is waiting, or
  //    the block would overwrite an unread verdict under backpressure.
  // =========================================================================
  {
    Stim s = baseline();
    apply(s);
    g->in_valid_i = 1;
    g->out_ready_i = 0;
    g->eval();
    check(g->in_ready_o != 0, "idle: in_ready_o accepts", 1, g->in_ready_o);
    tick();
    g->in_valid_i = 0;  // or the drain below immediately reloads
    g->eval();
    check(g->out_valid_o != 0, "one clock later the verdict is out -- FIXED latency 1", 1,
          g->out_valid_o);
    check(g->in_ready_o == 0, "and in_ready_o is LOW while the consumer is shut", 0, g->in_ready_o);

    const uint32_t held0 = g->out_record_o[0];
    tick();
    g->eval();
    check(g->out_valid_o != 0 && g->out_record_o[0] == held0,
          "a beat that was not taken is still there, unchanged", 1,
          (g->out_valid_o && g->out_record_o[0] == held0) ? 1 : 0);

    g->out_ready_i = 1;
    g->eval();
    tick();
    g->eval();
    check(g->out_valid_o == 0, "and it drains on ready", 0, g->out_valid_o);
    idle();
  }

  const Counters f = snap();
  std::printf(
      "[part_update_directed] updated=%u refused=%u died=%u vsat=%u psat=%u "
      "collisions=%u\n",
      f.updated, f.refused, f.died, f.vsat, f.psat, f.collisions);
  zhao::exit_hard(zhao::report_and_exit("part_update_directed"));
}
