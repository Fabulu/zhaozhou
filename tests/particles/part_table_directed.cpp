// part_table_directed.cpp -- PART.TABLE against the four read shapes its
// consumers already declare, and against its own authored load word.
//
// WHY THE PARAMETERS ARE SMALL. SPECIES_N = 8 and CRV_N = 8, for the reason
// PART.STATE's bench runs at CAPACITY = 8: at the production tier the index
// fields address the table EXACTLY (7 bits -> 128 species, 4 bits -> 16
// buckets), so an out-of-range index is unreachable and the refusal that
// handles it is not evidence about anything. Shrinking the table is the only
// way to reach it with legal stimulus.
//
// THE TWO CHECKS THAT ACTUALLY DISCRIMINATE, stated up front because the other
// sixty are ordinary read-back:
//
//   1. AN OUT-OF-RANGE READ MUST RETURN ZERO, NOT A WRAPPED ENTRY. Index 11
//      with SPECIES_N = 8 masks to 3, and species 3 is loaded with a real
//      descriptor throughout this bench. An implementation that simply masks
//      the address hands PART.UPDATE species 3's physics for species 11 -- a
//      wrong answer that looks completely correct, which is the worst kind. The
//      index chosen here is 11 for exactly that reason.
//
//   2. AN OUT-OF-RANGE LOAD MUST WRITE NOTHING. Same masking, same index, same
//      collision with species 3 -- but destructive: a wrapped write silently
//      replaces a species that IS in range with the contents meant for one that
//      is not. The bench reloads species 3, offers a load at 11 with a
//      distinctive word, and requires species 3 to be untouched.
//
// THE PACKING WALK is the other thing worth naming. The load word's bit
// offsets are AUTHORED in zhao_part_table.sv -- no ratified species-descriptor
// layout exists anywhere in the tree, and zref_particle.hpp says so. So the
// offsets have no oracle, and the only honest check is structural: load a word
// with EXACTLY ONE field set to all-ones and require that field to read
// all-ones and every other field of that slice to read zero. Any overlap,
// any off-by-one in a running offset, and any field too narrow for its port
// fails immediately. Twenty-one of the checks below are that walk.
//
// What is NOT checked here, because this block does not own it: whether any
// descriptor VALUE is legal. PART.UPDATE refuses an out-of-vocabulary recipe,
// PART.SPAWN refuses a count above 16 and an out-of-range child species, and
// PART.STATE refuses an out-of-range species -- three blocks with three
// counters and three tests. A fourth opinion here would be a second
// implementation that can disagree with them.

#include <cstdint>
#include <cstdio>

#include "Vzhao_part_table.h"
#include "zhao_sim.hpp"

using zhao::check;

namespace {

constexpr int kSpeciesN = 8;  // small on purpose: out-of-range must be reachable
constexpr int kCurveN = 8;

// The load selector, as zhao_part_table.sv declares it.
constexpr uint8_t kSelUpd = 0;
constexpr uint8_t kSelCol = 1;
constexpr uint8_t kSelSpw = 2;
constexpr uint8_t kSelCrv = 3;

// THE LOAD WORD MAP, mirroring zhao_part_table.sv's localparams at the default
// widths (AGE_W 10, POS_W 18, VEL_W 11, FX_W 16). Restated here rather than
// derived, so that a change to either side shows up as a failure instead of
// being tracked silently by a shared header.
struct Field {
  int off;
  int wid;
};

constexpr Field kU[12] = {
    {0, 4},     // recipe
    {4, 10},    // lifetime
    {14, 10},   // age_mark
    {24, 8},    // drag
    {32, 11},   // grav
    {43, 11},   // strength
    {54, 18},   // cx
    {72, 18},   // cy
    {90, 18},   // cz
    {108, 11},  // p0
    {119, 11},  // p1
    {130, 11},  // p2
};
constexpr int kUpdW = 141;

constexpr Field kC[4] = {
    {0, 3},    // response
    {3, 16},   // restitution
    {19, 16},  // friction
    {35, 16},  // damping
};

constexpr Field kS[3] = {
    {0, 7},   // child species
    {7, 5},   // count
    {12, 1},  // known
};

constexpr Field kV[2] = {
    {0, 6},  // size
    {6, 8},  // colour
};

const char* kUName[12] = {"recipe", "lifetime", "age_mark", "drag", "grav", "strength",
                          "cx",     "cy",       "cz",       "p0",   "p1",   "p2"};
const char* kCName[4] = {"response", "restitution", "friction", "damping"};
const char* kSName[3] = {"child_spc", "count", "known"};
const char* kVName[2] = {"size", "colour"};

// The 141-bit load bus, as five 32-bit words -- the shape Verilator gives a
// wide port.
struct LoadWord {
  uint32_t w[5];
};

LoadWord zero_word() { return LoadWord{{0, 0, 0, 0, 0}}; }

void put(LoadWord& d, const Field& f, uint64_t value) {
  for (int b = 0; b < f.wid; ++b) {
    const int bit = f.off + b;
    const uint32_t mask = 1u << (bit & 31);
    if ((value >> b) & 1u) {
      d.w[bit >> 5] |= mask;
    } else {
      d.w[bit >> 5] &= ~mask;
    }
  }
}

uint64_t all_ones(int wid) { return (wid >= 64) ? ~0ull : ((1ull << wid) - 1ull); }

// Verilator may store a signed narrow output either sign-extended or
// zero-extended in its carrier word. Masking to the declared width and then
// sign-extending by hand is correct under both, and a bench that guessed would
// be reading the toolchain rather than the design.
int64_t sx(uint64_t raw, int wid) {
  const uint64_t mask = all_ones(wid);
  const uint64_t v = raw & mask;
  const uint64_t sign = 1ull << (wid - 1);
  return static_cast<int64_t>((v ^ sign) - sign);
}

struct Dut {
  Vzhao_part_table* v;

  explicit Dut(Vzhao_part_table* d) : v(d) {}

  void settle() { v->eval(); }

  void half_low() {
    v->clk = 0;
    v->eval();
  }
  void half_high() {
    v->clk = 1;
    v->eval();
  }
  void tick() {
    half_low();
    half_high();
  }

  void reset() {
    v->rst_n = 0;
    v->ld_valid_i = 0;
    v->ld_sel_i = 0;
    v->ld_index_i = 0;
    v->ld_event_i = 0;
    for (int i = 0; i < 5; ++i) v->ld_data_i[i] = 0;
    v->u_index_i = 0;
    v->v_index_i = 0;
    v->c_index_i = 0;
    v->s_species_i = 0;
    v->s_event_i = 0;
    tick();
    tick();
    v->rst_n = 1;
    tick();
  }

  // Present a load word. Returns with the clock low again and ld_valid_i
  // cleared, so the caller can read the table combinationally.
  void load(uint8_t sel, uint8_t index, uint8_t event, const LoadWord& d) {
    v->ld_valid_i = 1;
    v->ld_sel_i = sel;
    v->ld_index_i = index;
    v->ld_event_i = event;
    for (int i = 0; i < 5; ++i) v->ld_data_i[i] = d.w[i];
    tick();
    v->ld_valid_i = 0;
    half_low();
  }

  void read_update(uint8_t index) {
    v->u_index_i = index;
    settle();
  }
  void read_collide(uint8_t index) {
    v->c_index_i = index;
    settle();
  }
  void read_curve(uint8_t index) {
    v->v_index_i = index;
    settle();
  }
  void read_spawn(uint8_t species, uint8_t event) {
    v->s_species_i = species;
    v->s_event_i = event;
    settle();
  }

  // The twelve update fields as unsigned carrier values, in kU order.
  uint64_t u_field(int i) const {
    switch (i) {
      case 0:
        return v->u_recipe_o;
      case 1:
        return v->u_lifetime_o;
      case 2:
        return v->u_age_mark_o;
      case 3:
        return v->u_drag_o;
      case 4:
        return v->u_grav_o;
      case 5:
        return v->u_strength_o;
      case 6:
        return v->u_cx_o;
      case 7:
        return v->u_cy_o;
      case 8:
        return v->u_cz_o;
      case 9:
        return v->u_p0_o;
      case 10:
        return v->u_p1_o;
      default:
        return v->u_p2_o;
    }
  }
  uint64_t c_field(int i) const {
    switch (i) {
      case 0:
        return v->c_response_o;
      case 1:
        return v->c_restitution_o;
      case 2:
        return v->c_friction_o;
      default:
        return v->c_damping_o;
    }
  }
  uint64_t s_field(int i) const {
    switch (i) {
      case 0:
        return v->s_child_spc_o;
      case 1:
        return v->s_count_o;
      default:
        return v->s_known_o;
    }
  }
  uint64_t v_field(int i) const { return (i == 0) ? v->v_size_o : v->v_colour_o; }
};

// The update descriptor species 3 carries for the whole run. Deliberately full
// of NEGATIVE values: a sign bit that does not survive the packing is invisible
// to a bench that only loads positives, and every one of these fields is signed
// on the port PART.UPDATE declares.
LoadWord species3_update() {
  LoadWord d = zero_word();
  put(d, kU[0], 9);                                         // recipe
  put(d, kU[1], 1000);                                      // lifetime
  put(d, kU[2], 777);                                       // age_mark
  put(d, kU[3], 200);                                       // drag
  put(d, kU[4], static_cast<uint64_t>(-1000) & 0x7FF);      // grav, s11
  put(d, kU[5], static_cast<uint64_t>(-7) & 0x7FF);         // strength, s11
  put(d, kU[6], static_cast<uint64_t>(-131072) & 0x3FFFF);  // cx, s18 minimum
  put(d, kU[7], 131071);                                    // cy, s18 maximum
  put(d, kU[8], static_cast<uint64_t>(-1) & 0x3FFFF);       // cz
  put(d, kU[9], static_cast<uint64_t>(-1024) & 0x7FF);      // p0, s11 minimum
  put(d, kU[10], 1023);                                     // p1, s11 maximum
  put(d, kU[11], 0);                                        // p2
  return d;
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Verilated::traceEverOn(false);

  auto* dut = new Vzhao_part_table;  // heap + exit_hard: see zhao_sim.hpp
  Vzhao_part_table& top = *dut;
  Dut d(dut);
  d.reset();

  // ==========================================================================
  // CASE A -- the update slice, loaded and read back COMBINATIONALLY.
  // ==========================================================================
  const uint32_t upd0 = top.loads_update_o;
  d.load(kSelUpd, 3, 0, species3_update());
  d.read_update(3);

  check(top.u_recipe_o == 9, "A: recipe reads back", 9u, top.u_recipe_o);
  check(top.u_lifetime_o == 1000, "A: lifetime reads back", 1000u, top.u_lifetime_o);
  check(top.u_age_mark_o == 777, "A: age_mark reads back", 777u, top.u_age_mark_o);
  check(top.u_drag_o == 200, "A: drag reads back", 200u, top.u_drag_o);
  check(sx(top.u_grav_o, 11) == -1000, "A: grav keeps its sign", static_cast<uint64_t>(-1000),
        static_cast<uint64_t>(sx(top.u_grav_o, 11)));
  check(sx(top.u_strength_o, 11) == -7, "A: strength keeps its sign", static_cast<uint64_t>(-7),
        static_cast<uint64_t>(sx(top.u_strength_o, 11)));
  check(sx(top.u_cx_o, 18) == -131072, "A: cx carries the s18 minimum",
        static_cast<uint64_t>(-131072), static_cast<uint64_t>(sx(top.u_cx_o, 18)));
  check(sx(top.u_cy_o, 18) == 131071, "A: cy carries the s18 maximum", 131071u,
        static_cast<uint64_t>(sx(top.u_cy_o, 18)));
  check(sx(top.u_cz_o, 18) == -1, "A: cz carries -1", static_cast<uint64_t>(-1),
        static_cast<uint64_t>(sx(top.u_cz_o, 18)));
  check(sx(top.u_p0_o, 11) == -1024, "A: p0 carries the s11 minimum", static_cast<uint64_t>(-1024),
        static_cast<uint64_t>(sx(top.u_p0_o, 11)));
  check(sx(top.u_p1_o, 11) == 1023, "A: p1 carries the s11 maximum", 1023u,
        static_cast<uint64_t>(sx(top.u_p1_o, 11)));
  check(sx(top.u_p2_o, 11) == 0, "A: p2 carries zero", 0u,
        static_cast<uint64_t>(sx(top.u_p2_o, 11)));
  check(top.loads_update_o - upd0 == 1, "A: loads_update_o moved by exactly one", 1u,
        top.loads_update_o - upd0);

  // ==========================================================================
  // CASE B -- two species do not bleed into each other, with NO clock between
  // the two reads. The reads are combinational by contract; if a capture
  // register had crept in, the second read would return the first's data.
  // ==========================================================================
  {
    LoadWord other = zero_word();
    put(other, kU[0], 2);
    put(other, kU[1], 33);
    d.load(kSelUpd, 4, 0, other);

    d.read_update(3);
    const uint32_t r3 = top.u_recipe_o;
    d.read_update(4);
    const uint32_t r4 = top.u_recipe_o;
    d.read_update(3);
    const uint32_t r3b = top.u_recipe_o;
    check(r3 == 9 && r4 == 2, "B: two species read their own descriptors in one cycle each", 1,
          (r3 == 9 && r4 == 2) ? 1 : 0);
    check(r3b == 9, "B: the read is combinational, not a held capture", 9u, r3b);
  }

  // ==========================================================================
  // CASE C -- the four arrays are independent. Loading the collide slice at
  // species 3 must not disturb the update slice at species 3.
  // ==========================================================================
  {
    const uint32_t col0 = top.loads_collide_o;
    LoadWord c = zero_word();
    put(c, kC[0], 4);                                      // response
    put(c, kC[1], static_cast<uint64_t>(-4096) & 0xFFFF);  // restitution, negative
    put(c, kC[2], 16384);                                  // friction, +1.0 in Q1.14
    put(c, kC[3], 8192);                                   // damping
    d.load(kSelCol, 3, 0, c);
    d.read_collide(3);
    d.read_update(3);

    check(top.c_response_o == 4, "C: response reads back", 4u, top.c_response_o);
    check(sx(top.c_restitution_o, 16) == -4096, "C: restitution keeps its sign",
          static_cast<uint64_t>(-4096), static_cast<uint64_t>(sx(top.c_restitution_o, 16)));
    check(sx(top.c_friction_o, 16) == 16384, "C: friction reads back", 16384u,
          static_cast<uint64_t>(sx(top.c_friction_o, 16)));
    check(sx(top.c_damping_o, 16) == 8192, "C: damping reads back", 8192u,
          static_cast<uint64_t>(sx(top.c_damping_o, 16)));
    check(top.u_recipe_o == 9, "C: the collide load left the update slice alone", 9u,
          top.u_recipe_o);
    check(top.loads_collide_o - col0 == 1, "C: loads_collide_o moved by exactly one", 1u,
          top.loads_collide_o - col0);
  }

  // ==========================================================================
  // CASE D -- spawn is indexed by {species, event}, four rules at one species.
  // Event 3 is loaded with known = 0: a species that spawns nothing on death is
  // a normal descriptor, not an absent one, and PART.SPAWN refuses on the bit.
  // ==========================================================================
  {
    const uint32_t spw0 = top.loads_spawn_o;
    const int child[4] = {2, 3, 2, 5};
    const int count[4] = {3, 2, 16, 4};
    const int known[4] = {1, 1, 1, 0};
    for (int e = 0; e < 4; ++e) {
      LoadWord s = zero_word();
      put(s, kS[0], static_cast<uint64_t>(child[e]));
      put(s, kS[1], static_cast<uint64_t>(count[e]));
      put(s, kS[2], static_cast<uint64_t>(known[e]));
      d.load(kSelSpw, 3, static_cast<uint8_t>(e), s);
    }
    for (int e = 0; e < 4; ++e) {
      d.read_spawn(3, static_cast<uint8_t>(e));
      const bool ok = (static_cast<int>(top.s_child_spc_o) == child[e]) &&
                      (static_cast<int>(top.s_count_o) == count[e]) &&
                      (static_cast<int>(top.s_known_o) == known[e]);
      char what[96];
      std::snprintf(what, sizeof(what), "D: spawn rule at {species 3, event %d} reads back", e);
      check(ok, what, 1, ok ? 1 : 0);
    }
    check(top.loads_spawn_o - spw0 == 4, "D: loads_spawn_o moved by exactly four", 4u,
          top.loads_spawn_o - spw0);
    d.read_spawn(3, 3);
    check(top.s_known_o == 0, "D: a rule loaded with known=0 does not resolve", 0u, top.s_known_o);
  }

  // ==========================================================================
  // CASE E -- the curve table, indexed by PART.UPDATE's 4-bit bucket.
  // ==========================================================================
  {
    const uint32_t crv0 = top.loads_curve_o;
    const int size[3] = {5, 31, 63};
    const int colour[3] = {1, 128, 255};
    for (int i = 0; i < 3; ++i) {
      LoadWord c = zero_word();
      put(c, kV[0], static_cast<uint64_t>(size[i]));
      put(c, kV[1], static_cast<uint64_t>(colour[i]));
      d.load(kSelCrv, static_cast<uint8_t>(i), 0, c);
    }
    for (int i = 0; i < 3; ++i) {
      d.read_curve(static_cast<uint8_t>(i));
      const bool ok = (static_cast<int>(top.v_size_o) == size[i]) &&
                      (static_cast<int>(top.v_colour_o) == colour[i]);
      char what[96];
      std::snprintf(what, sizeof(what), "E: curve bucket %d reads back", i);
      check(ok, what, 1, ok ? 1 : 0);
    }
    check(top.loads_curve_o - crv0 == 3, "E: loads_curve_o moved by exactly three", 3u,
          top.loads_curve_o - crv0);
  }

  // ==========================================================================
  // CASE F -- AN OUT-OF-RANGE READ RETURNS ZERO, NOT A WRAPPED ENTRY.
  // Index 11 masks to 3 with SPECIES_N = 8, and species 3 is loaded. Returning
  // species 3's descriptor here would be a wrong answer that looks right.
  // ==========================================================================
  {
    d.read_update(11);
    check(top.u_recipe_o == 0, "F: an out-of-range update read is ZERO, not species 3's recipe", 0u,
          top.u_recipe_o);
    check(top.u_cx_o == 0, "F: an out-of-range update read is ZERO in every field", 0u, top.u_cx_o);
    d.read_collide(11);
    check(top.c_response_o == 0, "F: an out-of-range collide read is ZERO", 0u, top.c_response_o);
    d.read_spawn(11, 0);
    check(top.s_known_o == 0, "F: an out-of-range spawn read does not resolve", 0u, top.s_known_o);
    d.read_curve(11);
    check(top.v_size_o == 0, "F: an out-of-range curve read is ZERO", 0u, top.v_size_o);
  }

  // ==========================================================================
  // CASE G -- AN OUT-OF-RANGE LOAD IS REFUSED AND WRITES NOTHING. The
  // destructive twin of CASE F: index 11 masks to 3, so a wrapped write would
  // replace a species that IS in range.
  // ==========================================================================
  {
    const uint32_t ref0 = top.load_refused_o;
    const uint32_t upd1 = top.loads_update_o;
    LoadWord poison = zero_word();
    put(poison, kU[0], 15);
    put(poison, kU[1], 1023);
    d.load(kSelUpd, 11, 0, poison);

    check(top.load_refused_o - ref0 == 1, "G: load_refused_o moved by exactly one", 1u,
          top.load_refused_o - ref0);
    check(top.loads_update_o - upd1 == 0, "G: a refused load does not count as a load", 0u,
          top.loads_update_o - upd1);
    d.read_update(3);
    check(top.u_recipe_o == 9, "G: the refused load did NOT wrap onto species 3", 9u,
          top.u_recipe_o);

    const uint32_t ref1 = top.load_refused_o;
    const uint32_t crv1 = top.loads_curve_o;
    LoadWord cpoison = zero_word();
    put(cpoison, kV[0], 63);
    put(cpoison, kV[1], 255);
    d.load(kSelCrv, 11, 0, cpoison);
    check(top.load_refused_o - ref1 == 1, "G: a curve load out of range is refused too", 1u,
          top.load_refused_o - ref1);
    check(top.loads_curve_o - crv1 == 0, "G: a refused curve load does not count as a load", 0u,
          top.loads_curve_o - crv1);
    d.read_curve(3);
    check(top.v_size_o == 0, "G: the refused curve load did NOT wrap onto bucket 3", 0u,
          top.v_size_o);
  }

  // ==========================================================================
  // CASE H -- the load lands ON THE CLOCK EDGE and not before. A table that
  // wrote combinationally would serve a half-written frame during the load.
  // ==========================================================================
  {
    LoadWord first = zero_word();
    put(first, kU[0], 6);
    d.load(kSelUpd, 5, 0, first);
    d.read_update(5);
    check(top.u_recipe_o == 6, "H: species 5 starts at recipe 6", 6u, top.u_recipe_o);

    LoadWord second = zero_word();
    put(second, kU[0], 11);
    top.ld_valid_i = 1;
    top.ld_sel_i = kSelUpd;
    top.ld_index_i = 5;
    top.ld_event_i = 0;
    for (int i = 0; i < 5; ++i) top.ld_data_i[i] = second.w[i];
    d.half_low();  // inputs presented, NO rising edge yet
    d.read_update(5);
    check(top.u_recipe_o == 6, "H: the table does not change before the edge", 6u, top.u_recipe_o);
    d.half_high();
    top.ld_valid_i = 0;
    d.half_low();
    d.read_update(5);
    check(top.u_recipe_o == 11, "H: the table changes on the edge", 11u, top.u_recipe_o);
  }

  // ==========================================================================
  // CASE I -- ld_ready_o is constant high, which the composer has to be able to
  // rely on: there is nothing in this block that could ever be busy.
  // ==========================================================================
  check(top.ld_ready_o == 1, "I: ld_ready_o is constant high", 1u, top.ld_ready_o);

  // ==========================================================================
  // THE PACKING WALK. One field all-ones, everything else zero, per field, per
  // slice. Any overlap, any off-by-one in a running offset, and any field too
  // narrow for its port fails here and nowhere else.
  // ==========================================================================
  for (int f = 0; f < 12; ++f) {
    LoadWord w = zero_word();
    put(w, kU[f], all_ones(kU[f].wid));
    d.load(kSelUpd, 6, 0, w);
    d.read_update(6);
    bool ok = true;
    for (int g = 0; g < 12; ++g) {
      const uint64_t got = d.u_field(g) & all_ones(kU[g].wid);
      const uint64_t want = (g == f) ? all_ones(kU[g].wid) : 0ull;
      if (got != want) ok = false;
    }
    char what[128];
    std::snprintf(what, sizeof(what), "PACK: update field %s occupies bits %d..%d alone", kUName[f],
                  kU[f].off, kU[f].off + kU[f].wid - 1);
    check(ok, what, 1, ok ? 1 : 0);
  }
  for (int f = 0; f < 4; ++f) {
    LoadWord w = zero_word();
    put(w, kC[f], all_ones(kC[f].wid));
    d.load(kSelCol, 6, 0, w);
    d.read_collide(6);
    bool ok = true;
    for (int g = 0; g < 4; ++g) {
      const uint64_t got = d.c_field(g) & all_ones(kC[g].wid);
      const uint64_t want = (g == f) ? all_ones(kC[g].wid) : 0ull;
      if (got != want) ok = false;
    }
    char what[128];
    std::snprintf(what, sizeof(what), "PACK: collide field %s occupies bits %d..%d alone",
                  kCName[f], kC[f].off, kC[f].off + kC[f].wid - 1);
    check(ok, what, 1, ok ? 1 : 0);
  }
  for (int f = 0; f < 3; ++f) {
    LoadWord w = zero_word();
    put(w, kS[f], all_ones(kS[f].wid));
    d.load(kSelSpw, 6, 1, w);
    d.read_spawn(6, 1);
    bool ok = true;
    for (int g = 0; g < 3; ++g) {
      const uint64_t got = d.s_field(g) & all_ones(kS[g].wid);
      const uint64_t want = (g == f) ? all_ones(kS[g].wid) : 0ull;
      if (got != want) ok = false;
    }
    char what[128];
    std::snprintf(what, sizeof(what), "PACK: spawn field %s occupies bits %d..%d alone", kSName[f],
                  kS[f].off, kS[f].off + kS[f].wid - 1);
    check(ok, what, 1, ok ? 1 : 0);
  }
  for (int f = 0; f < 2; ++f) {
    LoadWord w = zero_word();
    put(w, kV[f], all_ones(kV[f].wid));
    d.load(kSelCrv, 6, 0, w);
    d.read_curve(6);
    bool ok = true;
    for (int g = 0; g < 2; ++g) {
      const uint64_t got = d.v_field(g) & all_ones(kV[g].wid);
      const uint64_t want = (g == f) ? all_ones(kV[g].wid) : 0ull;
      if (got != want) ok = false;
    }
    char what[128];
    std::snprintf(what, sizeof(what), "PACK: curve field %s occupies bits %d..%d alone", kVName[f],
                  kV[f].off, kV[f].off + kV[f].wid - 1);
    check(ok, what, 1, ok ? 1 : 0);
  }

  // Every counter was read as a DELTA above; this is the blunt statement that
  // none of the five is a port that never moved at all.
  {
    const bool all_moved = top.loads_update_o > 0 && top.loads_collide_o > 0 &&
                           top.loads_spawn_o > 0 && top.loads_curve_o > 0 && top.load_refused_o > 0;
    check(all_moved, "every counter on this block was seen to move", 1, all_moved ? 1 : 0);
  }

  std::printf(
      "[part_table_directed] SPECIES_N=%d CRV_N=%d UPD_W=%d  loads upd=%u col=%u spw=%u crv=%u "
      "refused=%u\n",
      kSpeciesN, kCurveN, kUpdW, top.loads_update_o, top.loads_collide_o, top.loads_spawn_o,
      top.loads_curve_o, top.load_refused_o);
  zhao::exit_hard(zhao::report_and_exit("part_table_directed"));
}
