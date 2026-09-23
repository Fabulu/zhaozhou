// part_project_directed.cpp -- PART.PROJECT against the three things it can get
// wrong, plus the two positive controls its counters owe.
//
// WHAT THIS BLOCK IS. It has no projection arithmetic: it time-multiplexes the
// console's ONE shared projector so that particles get a seat beside geometry
// without a second zhao_project_core. So the bench MODELS the projector rather
// than instantiating it -- a six-deep in-order pipeline with one accept per
// clock and no output backpressure, which is zhao_project_service's client-A
// behaviour with the latency shortened. The latency's VALUE is not what this
// block depends on; its being IN ORDER, ONE-PER-ACCEPT and UNSTALLABLE ON THE
// WAY OUT is, and that is what the model reproduces.
//
// THE THREE DISCRIMINATING FAILURES, stated up front because the rest is
// routing and counting:
//
//   1. A PARTICLE RESULT ROUTED TO GEOMETRY, OR VICE VERSA. The demux is by the
//      rider's top bit and the two streams are interleaved by an arbiter, so a
//      wrong tag does not look like a crash -- it looks like the geometry arena
//      quietly filling with sprite coordinates. Every geometry vertex here
//      carries a payload the bench recognises, and both directions are checked
//      on every beat.
//
//   2. A VERDICT APPLIED TO THE WRONG PARTICLE. The ladder is a separate block
//      reached through a port pair, so the packet waits somewhere while its rung
//      is computed. The model's rung is a FUNCTION OF THE SIZE IT WAS HANDED, so
//      a verdict landing on its neighbour is a mismatch rather than a plausible
//      number. This is the pairing failure this repository has a chapter about,
//      in a new place.
//
//   3. THE SIZE LAW. `half_sub = rescale_s32(fx_mul(radius_fx16, d), 8)` at
//      projection scale 1 -- zref::render::draw_form_marker's world-space
//      branch, which zref_particle.hpp names as the ratified answer. The oracle
//      below is that expression in int64 with the reference's own saturating
//      rescale, and the radius comes from zref::part::particle_radius rather than
//      from a hand-written shift. The trap it guards is in the reference's own
//      words: dividing by `d` instead of multiplying makes particles GROW with
//      distance, and only an ortho matrix hides the inversion -- so the sweep
//      below walks `d` across three decades and also asserts the TREND, which
//      an inversion fails even where a single value might not.
//
// THE POSITIVE CONTROLS. `geom_tag_collision_o` and `ladder_unexpected_o` are
// asserted zero through every functional case and then FIRED deliberately here
// through ports the bench owns -- a geometry rider whose owner field is not
// `OWNER_GEOM`, and a ladder verdict offered with nothing outstanding. Neither
// needs a committed mutant, because neither state is unreachable through a port.
//
// `owner_unroutable_o` WAS THE THIRD OF THOSE AND IS NO LONGER, and the reason
// is this block's own progress rather than any weakening of the law. It fired
// on a result carrying an UNCLAIMED owner encoding. On 2026-09-23 the fourth
// request arm claimed 2'd3, the last one the two-bit field holds; every code
// now routes, so no stimulus at any port of this block can move that counter.
// A detector that cannot fire has a zero that is a tautology, so its positive
// control MOVED OUT of this bench and into a committed mutant --
// `tests/mutants/zhao_part_project_owner_mutant.sv`, which removes the fourth
// RESULT arm while keeping the fourth REQUEST arm, driven with inverted
// polarity by `part_project_owner_mutant_control`. CLAUDE.md's rule in one
// line: a guard you cannot reach with legal stimulus needs a committed mutant.
//
// What THIS bench now owes instead is the other half, and it is checked below:
// that ALL FOUR owner codes are actually DELIVERED to their arms, so the
// detector's silence has a reason and not merely an absence.
//
// WHAT IS NOT CHECKED HERE. Whether the projection itself is right: that is
// zhao_project_core's law and tests/geometry/geom_project_directed.cpp is its
// bench. A second opinion here would be a second implementation.
//
// SLOTS = 4 on purpose. At the shipping 8 the in-flight backpressure is still
// reachable but slower to reach, and at a depth above the projector's latency
// it would not be reachable at all -- the same reason PART.STATE's bench runs
// at CAPACITY = 8.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_part_project.h"
#include "zhao_sim.hpp"
#include "zref/zref_particle.hpp"

using zhao::check;

namespace {

constexpr int kProjLatency = 6;
// R68 sub-build 4: the client-A rider is 17 bits and the owner is a two-bit
// FIELD at the top, not a single tag bit. These mirror `zhao_part_project`'s
// own localparams; the block is verilated at its default PAY_W, so a change
// there without a change here is exactly the drift this comment exists to
// make loud. `kOwnerGeom` is ZERO because `zhao_geom_proj_lane` zero-pads.
constexpr int kPayW = 17;
constexpr int kOwnerW = 2;
constexpr int kOwnerLo = kPayW - kOwnerW;  // 15
constexpr uint32_t kOwnerGeom = 0u;
constexpr uint32_t kOwnerPart = 1u;
// OWNER 2 WAS CLAIMED 2026-09-21 (packet FORGECOMP) -- by FORGE.PRIM's vertex
// stream, not by GEOM.LOD, which is the block the reservation was written for.
// The name follows the CLAIMANT because that is what a reader has to trace.
constexpr uint32_t kOwnerForge = 2u;
constexpr uint32_t kOwnerLod = 3u;    // the LAST code; claimed 2026-09-23

// Put `owner` in the field of `rider`, leaving the low bits alone.
static inline uint32_t with_owner(uint32_t rider, uint32_t owner) {
  const uint32_t mask = ((1u << kOwnerW) - 1u) << kOwnerLo;
  return (rider & ~mask) | ((owner << kOwnerLo) & mask);
}
static inline uint32_t owner_of(uint32_t rider) {
  return (rider >> kOwnerLo) & ((1u << kOwnerW) - 1u);
}

// ---------------------------------------------------------------------------
// The size oracle: zref::render::draw_form_marker's world-space branch.
// ---------------------------------------------------------------------------
int32_t sat32(int64_t v) {
  if (v > INT32_MAX) return INT32_MAX;
  if (v < INT32_MIN) return INT32_MIN;
  return static_cast<int32_t>(v);
}

struct SizeOracle {
  uint16_t size16 = 0;
  uint8_t size8 = 0;
  bool saturated = false;
};

SizeOracle size_oracle(int32_t radius_fx16, int32_t d) {
  const int64_t p = static_cast<int64_t>(radius_fx16) * static_cast<int64_t>(d);
  const int64_t r16 = (p + 32768) >> 16;
  SizeOracle o;
  o.saturated = (r16 > INT32_MAX) || (r16 < INT32_MIN);
  const int32_t wfx = sat32(r16);
  const int64_t half = (static_cast<int64_t>(wfx) + 128) >> 8;
  const int64_t a = half < 0 ? -half : half;
  if (a > 32767) {
    o.size16 = 0xFFFF;
    o.saturated = true;
  } else {
    o.size16 = static_cast<uint16_t>(a * 2);
  }
  if ((a >> 3) > 255) {
    o.size8 = 0xFF;
    o.saturated = true;
  } else {
    o.size8 = static_cast<uint8_t>(a >> 3);
  }
  return o;
}

// Verilator may store a signed narrow output sign- or zero-extended in its
// carrier word. Masking to the declared width and sign-extending by hand is
// correct under both; a bench that guessed would be reading the toolchain
// rather than the design.
int64_t sx(uint64_t raw, int wid) {
  const uint64_t mask = (wid >= 64) ? ~0ull : ((1ull << wid) - 1ull);
  const uint64_t v = raw & mask;
  const uint64_t sign = 1ull << (wid - 1);
  return static_cast<int64_t>((v ^ sign) - sign);
}

// ---------------------------------------------------------------------------
// The shared projector, modelled.
// ---------------------------------------------------------------------------
struct ProjBeat {
  bool live = false;
  uint32_t payload = 0;
  int32_t vx = 0, vy = 0, vz = 0;
};

// The result a vertex produces. Deterministic and distinctive, so a misrouted
// beat is recognisable rather than plausible. `vz` IS the 1/w lane, which is
// how the bench sweeps depth.
int32_t res_x(int32_t vx) { return (vx >> 6) + 11; }
int32_t res_y(int32_t vy) { return (vy >> 6) - 7; }
int32_t res_d(int32_t vz) { return vz; }
uint32_t res_w(int32_t vx) { return static_cast<uint32_t>((vx & 0x7FFF) + 3); }
// The depth profile this result was projected under. A FUNCTION OF THE RESULT,
// so the expectation below is this particle's own rather than a constant -- a
// model that returned one value could not tell a carried profile from a
// stuck one. (owner ruling 1, 2026-09-22)
uint8_t res_profile(int32_t vx) { return static_cast<uint8_t>((vx >> 3) & 3); }
bool res_behind(int32_t vz) { return vz == 0; }

struct ProjModel {
  ProjBeat stage[kProjLatency];

  const ProjBeat& out() const { return stage[kProjLatency - 1]; }
  void shift_and_load(bool load, uint32_t payload, int32_t vx, int32_t vy, int32_t vz) {
    for (int i = kProjLatency - 1; i > 0; --i) stage[i] = stage[i - 1];
    ProjBeat b;
    if (load) {
      b.live = true;
      b.payload = payload;
      b.vx = vx;
      b.vy = vy;
      b.vz = vz;
    }
    stage[0] = b;
  }
};

// ---------------------------------------------------------------------------
// PART.LADDER, modelled as the one-deep skid it is:
//     v_ready_o = !r_valid_o || r_ready_i
//     if (v_ready_o) { r_valid_o <= v_valid_i; if (v_valid_i) capture; }
// THE RUNG IS A FUNCTION OF THE SIZE IT WAS HANDED. A verdict that lands on the
// wrong particle therefore fails a check instead of looking reasonable.
// ---------------------------------------------------------------------------
uint8_t model_rung(uint16_t size) { return static_cast<uint8_t>((size >> 2) & 7); }
uint8_t model_hold(uint16_t size) { return static_cast<uint8_t>((size >> 5) & 15); }
bool model_changed(uint16_t size) { return ((size >> 9) & 1) != 0; }

struct LadderModel {
  bool valid = false;
  uint16_t size = 0;
};

// ---------------------------------------------------------------------------
// A particle, as the bench authors it.
// ---------------------------------------------------------------------------
struct Part {
  zref::part::Particle128 rec{};
  uint16_t trail = 0;
  bool narrow = false, protected_ = false, first = false;
  uint8_t gov = 0, prev = 0, hold = 0;
  uint8_t r = 0, g = 0, b = 0;
  uint16_t src_id = 0;
};

struct Emitted {
  uint16_t src_id = 0;
  bool in = false;
  int64_t x = 0, y = 0;
  int32_t d = 0;
  // THE CANONICAL-DEPTH CARRIAGE (owner ruling 1, 2026-09-22). Until this
  // packet `w` was DROPPED at the ladder queue and never reached PART.EXPAND,
  // which is why a polygon particle had no canonical depth at all.
  uint32_t w = 0;
  uint8_t profile = 0;
  uint8_t size8 = 0;
  uint16_t size16 = 0;
  uint8_t r = 0, g = 0, b = 0;
  uint8_t rung = 0, hold = 0;
  bool changed = false;
};

struct LadderSeen {
  uint16_t size = 0, trail = 0;
  bool narrow = false, protected_ = false, first = false;
  uint8_t gov = 0, prev = 0, hold = 0;
};

struct GeomSeen {
  uint32_t payload = 0;
  int64_t x = 0, y = 0;
  int32_t d = 0;
  uint32_t w = 0;
  bool behind = false;
};

// ---------------------------------------------------------------------------
// The bench.
// ---------------------------------------------------------------------------
struct Bench {
  Vzhao_part_project* v;
  ProjModel proj;
  LadderModel lad;

  std::vector<Emitted> emitted;
  std::vector<LadderSeen> ladder_seen;
  std::vector<GeomSeen> geom_seen;

  // Stimulus knobs, applied by step().
  bool proj_ready = true;
  bool q_ready = true;
  bool ladder_enabled = true;
  bool force_rng_valid = false;  // the ladder positive control
  // Overrides the returning rider's owner field: -1 leaves it exactly as the
  // projector model produced it; 0..3 forces one of the four codes.
  //
  // IT IS NO LONGER A POSITIVE CONTROL FOR `owner_unroutable_o`, and that is
  // a change worth stating rather than discovering. It was one while 2'd3 was
  // unclaimed. The fourth arm claims it, the two-bit field is now FULL, and
  // every code routes -- so no value this hook can force reaches the detector
  // and its zero became a tautology. Its positive control is the committed
  // mutant `tests/mutants/zhao_part_project_owner_mutant.sv`, driven with
  // inverted polarity by `part_project_owner_mutant_control`. What this hook
  // proves now is the other half: that all FOUR codes are delivered.
  int force_a_owner = -1;

  explicit Bench(Vzhao_part_project* d) : v(d) {}

  void clear_offers() {
    v->p_valid_i = 0;
    v->g_valid_i = 0;
  }

  // Apply the consumer-side knobs and settle, so that `g_ready_o`/`p_ready_o`
  // can be READ before the offer is committed to.
  //
  // This is not decoration. `g_ready_o` is `a_ready_i && !sel_p`, so probing it
  // while `a_ready_i` still holds the previous cycle's value reads a ready that
  // is about to change -- the bench's bookkeeping would then disagree with the
  // DUT about which vertices were accepted, and every ordering check downstream
  // would be comparing two different sequences. Found by the first run.
  void pre() {
    v->a_ready_i = proj_ready ? 1 : 0;
    v->q_ready_i = q_ready ? 1 : 0;
    v->eval();
  }

  void reset() {
    v->rst_n = 0;
    v->clk = 0;
    v->cfg_base_radius_i = 0;
    v->cfg_view_i = 0;
    clear_offers();
    v->a_ready_i = 0;
    v->a_valid_i = 0;
    v->lad_ready_i = 0;
    v->rng_valid_i = 0;
    v->q_ready_i = 0;
    v->eval();
    for (int i = 0; i < 3; ++i) zhao::tick(*v);
    v->rst_n = 1;
    v->eval();
    proj = ProjModel{};
    lad = LadderModel{};
  }

  void load_particle(const Part& p) {
    uint64_t lo = 0, hi = 0;
    zref::part::particle_pack(p.rec, &lo, &hi);
    v->p_record_i[0] = static_cast<uint32_t>(lo);
    v->p_record_i[1] = static_cast<uint32_t>(lo >> 32);
    v->p_record_i[2] = static_cast<uint32_t>(hi);
    v->p_record_i[3] = static_cast<uint32_t>(hi >> 32);
    v->p_trail_i = p.trail;
    v->p_narrow_i = p.narrow;
    v->p_protected_i = p.protected_;
    v->p_gov_floor_i = p.gov;
    v->p_prev_rung_i = p.prev;
    v->p_hold_i = p.hold;
    v->p_first_i = p.first;
    v->p_r_i = p.r;
    v->p_g_i = p.g;
    v->p_b_i = p.b;
    v->p_src_id_i = p.src_id;
  }

  void load_geometry(int32_t vx, int32_t vy, int32_t vz, uint32_t payload) {
    v->g_vx_i = static_cast<uint32_t>(vx);
    v->g_vy_i = static_cast<uint32_t>(vy);
    v->g_vz_i = static_cast<uint32_t>(vz);
    v->g_view_i = 0;
    v->g_payload_i = payload;
  }

  // One cycle: present, settle, sample, tick, advance the models.
  //
  // TWO EVALS, AND THE REASON IS STRUCTURAL. `lad_ready_i` is the ladder's
  // `v_ready_o`, which reads the DUT's `rng_ready_o`; `rng_ready_o` is
  // `q_ready_i && !lq_empty` and therefore does NOT read `lad_ready_i`. So the
  // dependency is one-way and a settle-then-refine is exact. Driving
  // `lad_ready_i` from last cycle's `rng_ready_o` instead would be modelling a
  // ladder this design does not have.
  void step() {
    const ProjBeat& o = proj.out();
    v->a_valid_i = o.live;
    v->a_x_i = static_cast<uint32_t>(res_x(o.vx)) & 0x1FFFFFu;
    v->a_y_i = static_cast<uint32_t>(res_y(o.vy)) & 0x1FFFFFu;
    v->a_d_i = static_cast<uint32_t>(res_d(o.vz));
    v->a_w_i = res_w(o.vx) & 0x7FFFFFFFu;
    v->a_profile_i = res_profile(o.vx);
    v->a_behind_i = res_behind(o.vz) ? 1 : 0;
    v->a_payload_i = (force_a_owner < 0)
                         ? o.payload
                         : with_owner(o.payload,
                                      static_cast<uint32_t>(force_a_owner));
    v->a_ready_i = proj_ready ? 1 : 0;
    v->q_ready_i = q_ready ? 1 : 0;
    v->rng_valid_i = (lad.valid || force_rng_valid) ? 1 : 0;
    v->rng_rung_i = model_rung(lad.size);
    v->rng_hold_i = model_hold(lad.size);
    v->rng_changed_i = model_changed(lad.size);
    v->lad_ready_i = 0;
    v->eval();

    const bool lad_take = ladder_enabled && (!lad.valid || v->rng_ready_o != 0);
    v->lad_ready_i = lad_take ? 1 : 0;
    v->eval();

    // --- sample -----------------------------------------------------------
    const bool a_fire = v->a_valid_o && proj_ready;
    const uint32_t a_pay = v->a_payload_o;
    const int32_t a_vx = static_cast<int32_t>(v->a_vx_o);
    const int32_t a_vy = static_cast<int32_t>(v->a_vy_o);
    const int32_t a_vz = static_cast<int32_t>(v->a_vz_o);

    if (v->h_valid_o) {
      GeomSeen g;
      g.payload = v->h_payload_o;
      g.x = sx(v->h_x_o, 21);
      g.y = sx(v->h_y_o, 21);
      g.d = static_cast<int32_t>(v->h_d_o);
      g.w = v->h_w_o;
      g.behind = v->h_behind_o != 0;
      geom_seen.push_back(g);
    }

    const bool lad_fire = v->lad_valid_o && lad_take;
    LadderSeen ls;
    if (lad_fire) {
      ls.size = v->lad_size_o;
      ls.trail = v->lad_trail_o;
      ls.narrow = v->lad_narrow_o != 0;
      ls.protected_ = v->lad_protected_o != 0;
      ls.gov = v->lad_gov_floor_o;
      ls.prev = v->lad_prev_rung_o;
      ls.hold = v->lad_hold_o;
      ls.first = v->lad_first_o != 0;
    }

    const bool q_fire = v->q_valid_o && q_ready;
    Emitted e;
    if (q_fire) {
      e.src_id = v->q_src_id_o;
      e.in = v->q_in_o != 0;
      e.x = sx(v->q_x_o, 21);
      e.y = sx(v->q_y_o, 21);
      e.d = static_cast<int32_t>(v->q_d_o);
      e.w = v->q_w_o;
      e.profile = v->q_profile_o;
      e.size8 = v->q_size_o;
      e.size16 = v->q_size16_o;
      e.r = v->q_r_o;
      e.g = v->q_g_o;
      e.b = v->q_b_o;
      e.rung = v->q_rung_o;
      e.hold = v->q_hold_new_o;
      e.changed = v->q_changed_o != 0;
    }

    zhao::tick(*v);

    // --- advance the models on the handshakes that just fired -------------
    proj.shift_and_load(a_fire, a_pay, a_vx, a_vy, a_vz);
    if (lad_take) {
      lad.valid = lad_fire;
      if (lad_fire) lad.size = ls.size;
    }
    if (lad_fire) ladder_seen.push_back(ls);
    if (q_fire) emitted.push_back(e);
  }

  void idle(int n) {
    clear_offers();
    for (int i = 0; i < n; ++i) step();
  }
};

// A particle whose depth lane, colour and identity are all functions of one
// index, so every check below can name the particle it is about.
Part make_part(int i, uint8_t size6, int32_t z) {
  Part p;
  p.rec.pos[0] = 100 + 7 * i;
  p.rec.pos[1] = -60 - 5 * i;
  p.rec.pos[2] = z;
  p.rec.species = static_cast<uint8_t>(i & 0x7F);
  p.rec.size = size6;
  p.rec.variation = static_cast<uint8_t>(0xA0 + i);
  p.trail = static_cast<uint16_t>(300 + 13 * i);
  p.narrow = (i & 1) != 0;
  p.protected_ = (i & 2) != 0;
  p.first = (i & 4) != 0;
  p.gov = static_cast<uint8_t>(i % 6);
  p.prev = static_cast<uint8_t>((i + 2) % 6);
  p.hold = static_cast<uint8_t>(i % 4);
  p.r = static_cast<uint8_t>(0x10 + i);
  p.g = static_cast<uint8_t>(0x40 + i);
  p.b = static_cast<uint8_t>(0x80 + i);
  p.src_id = static_cast<uint16_t>(0x5000 + i);
  return p;
}

// The projector's `vz` for this particle: the record's z at the fx16 world
// scale POS_SHIFT selects, which is also the depth lane the model returns.
int32_t part_d(const Part& p) { return p.rec.pos[2] << 8; }

}  // namespace

int main() {
  Vzhao_part_project top;
  Bench b(&top);

  // =========================================================================
  // A. RESET
  // =========================================================================
  b.reset();
  check(top.a_valid_o == 0, "reset: no request at the shared port", 0, top.a_valid_o);
  check(top.h_valid_o == 0, "reset: no geometry result", 0, top.h_valid_o);
  check(top.q_valid_o == 0, "reset: no projected particle", 0, top.q_valid_o);
  check(top.lad_valid_o == 0, "reset: no ladder offer", 0, top.lad_valid_o);
  check(top.particles_projected_o == 0, "reset: projected", 0, top.particles_projected_o);
  check(top.geom_grants_o == 0, "reset: geom grants", 0, top.geom_grants_o);
  check(top.part_grants_o == 0, "reset: part grants", 0, top.part_grants_o);
  check(top.contended_o == 0, "reset: contended", 0, top.contended_o);
  check(top.geom_tag_collision_o == 0, "reset: tag collisions", 0, top.geom_tag_collision_o);
  check(top.owner_unroutable_o == 0, "reset: unroutable owners", 0,
        top.owner_unroutable_o);
  check(top.ladder_unexpected_o == 0, "reset: ladder unexpected", 0, top.ladder_unexpected_o);
  check(top.slot_pressure_o == 0, "reset: slot pressure", 0, top.slot_pressure_o);
  check(top.size_saturations_o == 0, "reset: size saturations", 0, top.size_saturations_o);

  // =========================================================================
  // B. GEOMETRY ALONE PASSES STRAIGHT THROUGH.
  //
  // With no particle offered, this block must be invisible: every vertex is
  // accepted on the cycle the projector is ready and every result comes back on
  // h_* with its rider intact. A block that inserted a stage here would slow
  // the geometry path it is a guest on.
  // =========================================================================
  {
    const int kN = 6;
    std::vector<uint32_t> want_pay;
    std::vector<int32_t> want_vx;
    int sent = 0;
    for (int cyc = 0; cyc < 60; ++cyc) {
      b.clear_offers();
      if (sent < kN) {
        const int32_t vx = 0x4000 + 0x100 * sent;
        b.load_geometry(vx, -0x2000 - 0x80 * sent, 0x1234 + sent,
                        static_cast<uint32_t>(0x0311 + sent));
        top.g_valid_i = 1;
        b.pre();
        if (top.g_ready_o) {
          want_pay.push_back(static_cast<uint32_t>(0x0311 + sent));
          want_vx.push_back(vx);
          ++sent;
        }
      }
      b.step();
    }
    check(b.geom_seen.size() == static_cast<size_t>(kN), "geometry alone: every vertex returns", kN,
          b.geom_seen.size());
    check(top.geom_grants_o == static_cast<uint32_t>(kN), "geometry alone: grants", kN,
          top.geom_grants_o);
    check(top.part_grants_o == 0, "geometry alone: no particle took the port", 0,
          top.part_grants_o);
    check(top.contended_o == 0, "geometry alone: nothing contended", 0, top.contended_o);
    check(b.emitted.empty(), "geometry alone: no particle emitted", 0, b.emitted.size());
    check(top.particles_projected_o == 0, "geometry alone: none projected", 0,
          top.particles_projected_o);
    bool order_ok = true, field_ok = true;
    for (size_t i = 0; i < b.geom_seen.size() && i < want_pay.size(); ++i) {
      if (b.geom_seen[i].payload != want_pay[i]) order_ok = false;
      if (b.geom_seen[i].x != sx(static_cast<uint32_t>(res_x(want_vx[i])) & 0x1FFFFFu, 21))
        field_ok = false;
      if (b.geom_seen[i].w != (res_w(want_vx[i]) & 0x7FFFFFFFu)) field_ok = false;
    }
    check(order_ok, "geometry alone: riders return in order and unchanged", 1, order_ok);
    check(field_ok, "geometry alone: x and w come back untouched", 1, field_ok);
    check(top.geom_tag_collision_o == 0, "geometry alone: no tag collision", 0,
          top.geom_tag_collision_o);
  }

  // =========================================================================
  // C. PARTICLES ALONE -- the size law, the pairing, and the attribute ride.
  //
  // The depth sweep is three decades of 1/w on a fixed radius. Check 3 in the
  // header is the reason: an implementation that divided by `d` rather than
  // multiplying would make the size grow with distance, and asserting the TREND
  // as well as the values makes that a two-line failure rather than a debate.
  // =========================================================================
  {
    b.reset();
    b.emitted.clear();
    b.ladder_seen.clear();
    b.geom_seen.clear();

    const int32_t kBase = 3 << 16;  // 3.0 world units, fx16
    top.cfg_base_radius_i = static_cast<uint32_t>(kBase);

    // z, and therefore 1/w, across three decades; index 3 is behind the eye.
    const int32_t kZ[8] = {1, 4, 16, 0, 64, 256, 1024, 4096};
    std::vector<Part> parts;
    for (int i = 0; i < 8; ++i) parts.push_back(make_part(i, static_cast<uint8_t>(4 + i), kZ[i]));

    size_t sent = 0;
    for (int cyc = 0; cyc < 400 && (sent < parts.size() || b.emitted.size() < parts.size());
         ++cyc) {
      b.clear_offers();
      if (sent < parts.size()) {
        b.load_particle(parts[sent]);
        top.p_valid_i = 1;
        b.pre();
        if (top.p_ready_o) ++sent;
      }
      b.step();
    }

    check(sent == parts.size(), "particles alone: every particle was accepted", parts.size(), sent);
    check(b.emitted.size() == parts.size(), "particles alone: every particle was emitted",
          parts.size(), b.emitted.size());
    check(top.particles_projected_o == parts.size(), "particles alone: projected count",
          parts.size(), top.particles_projected_o);
    check(top.part_grants_o == parts.size(), "particles alone: grants", parts.size(),
          top.part_grants_o);
    check(top.geom_grants_o == 0, "particles alone: geometry took nothing", 0, top.geom_grants_o);
    check(b.geom_seen.empty(), "particles alone: nothing routed to the geometry port", 0,
          b.geom_seen.size());
    check(top.particles_behind_o == 1, "particles alone: exactly one behind the eye", 1,
          top.particles_behind_o);
    check(top.ladder_unexpected_o == 0, "particles alone: no unpaired verdict", 0,
          top.ladder_unexpected_o);

    bool id_order_ok = true, size_ok = true, rung_ok = true, colour_ok = true, coord_ok = true;
    bool in_ok = true, ladder_attrs_ok = true, carry_ok = true;
    std::vector<uint16_t> seen_size;
    for (size_t i = 0; i < b.emitted.size() && i < parts.size(); ++i) {
      const Part& p = parts[i];
      const Emitted& e = b.emitted[i];
      const int32_t d = part_d(p);
      const int32_t radius = zref::part::particle_radius(kBase, p.rec.size);
      const SizeOracle so = size_oracle(radius, d);

      if (e.src_id != p.src_id) id_order_ok = false;
      if (e.size16 != so.size16 || e.size8 != so.size8) size_ok = false;
      if (e.rung != model_rung(so.size16) || e.hold != model_hold(so.size16) ||
          e.changed != model_changed(so.size16))
        rung_ok = false;
      if (e.r != p.r || e.g != p.g || e.b != p.b) colour_ok = false;
      if (e.x != sx(static_cast<uint32_t>(res_x(p.rec.pos[0] << 8)) & 0x1FFFFFu, 21))
        coord_ok = false;
      if (e.d != d) coord_ok = false;
      // `w` AND ITS PROFILE SURVIVED THE SLOT STORE AND THE LADDER QUEUE, and
      // they are checked against the values driven for THIS PARTICLE'S OWN
      // result rather than against a constant. Both are functions of `vx`, so
      // a queue that handed particle i particle j's w fails here.
      if (e.w != (res_w(p.rec.pos[0] << 8) & 0x7FFFFFFFu)) carry_ok = false;
      if (e.profile != res_profile(p.rec.pos[0] << 8)) carry_ok = false;
      if (e.in != !res_behind(d)) in_ok = false;
      seen_size.push_back(e.size16);

      if (i < b.ladder_seen.size()) {
        const LadderSeen& l = b.ladder_seen[i];
        if (l.size != so.size16 || l.trail != p.trail || l.narrow != p.narrow ||
            l.protected_ != p.protected_ || l.gov != p.gov || l.prev != p.prev ||
            l.hold != p.hold || l.first != p.first)
          ladder_attrs_ok = false;
      }
    }
    check(id_order_ok, "particles alone: identities come back in order", 1, id_order_ok);
    check(size_ok, "particles alone: size8 and size16 match draw_form_marker's law", 1, size_ok);
    check(rung_ok, "particles alone: the verdict belongs to ITS particle", 1, rung_ok);
    check(colour_ok, "particles alone: colour rode the slot store", 1, colour_ok);
    check(coord_ok, "particles alone: screen x and 1/w are the projector's, untouched", 1,
          coord_ok);
    check(in_ok, "particles alone: the in-front verdict is the projector's", 1, in_ok);
    check(carry_ok,
          "particles alone: w and its depth profile rode the slot store and the "
          "ladder queue, each particle carrying its OWN", 1, carry_ok);
    check(ladder_attrs_ok, "particles alone: every ladder input rode with its particle", 1,
          ladder_attrs_ok);

    // The trend. Indices 0,1,2,4,5,6,7 have strictly increasing 1/w at a
    // strictly increasing radius, so the size must be non-decreasing. An
    // implementation that divided instead of multiplying inverts this.
    bool trend_ok = true;
    const int order[7] = {0, 1, 2, 4, 5, 6, 7};
    for (int i = 1; i < 7; ++i) {
      if (seen_size.size() > static_cast<size_t>(order[i]) &&
          seen_size[order[i]] < seen_size[order[i - 1]])
        trend_ok = false;
    }
    check(trend_ok, "particles alone: size RISES with 1/w (the multiply, not the divide)", 1,
          trend_ok);
    check(seen_size.size() > 3 && seen_size[3] == 0, "particles alone: behind the eye has no size",
          0, seen_size.size() > 3 ? seen_size[3] : 0xFFFF);

    // A POSITIVE CONTROL ON THE SWEEP ITSELF. Every size check above compares
    // the DUT to an oracle, and a sweep in which every size came out ZERO would
    // satisfy all of them while testing nothing -- the shape this tree's
    // broken-instrument law warns about, and the reason PART.STATE's tick
    // boundary bench carries two controls on its own sweep. So the spread is
    // asserted: the sweep must have produced several distinct non-zero sizes
    // and must have reached the saturated end.
    int nonzero = 0;
    uint16_t smallest_nonzero = 0xFFFF, largest = 0;
    for (uint16_t s : seen_size) {
      if (s != 0) {
        ++nonzero;
        if (s < smallest_nonzero) smallest_nonzero = s;
      }
      if (s > largest) largest = s;
    }
    check(nonzero >= 5, "sweep control: the depth sweep produced real sizes, not zeros", 5,
          nonzero);
    // Three decades of 1/w must show up as three decades of size. The factor of
    // 64 is deliberately far below the 8,448 the sweep actually produces: the
    // check is "this swept something", not a golden number that has to be
    // re-authored whenever the stimulus is retuned.
    check(largest > 64 * smallest_nonzero, "sweep control: the sizes span three decades", 1,
          largest > 64 * smallest_nonzero);
  }

  // =========================================================================
  // D. CONTENTION -- both clients saturated.
  //
  // The claim under test is the arbitration law, and the number that would
  // otherwise be assumed is `contended_o`. Neither client may starve, so both
  // grant counters must move and the split must be near even: this is a
  // one-per-clock resource and alternating is the fair division of it.
  // =========================================================================
  {
    b.reset();
    b.emitted.clear();
    b.geom_seen.clear();
    b.ladder_seen.clear();
    top.cfg_base_radius_i = static_cast<uint32_t>(2 << 16);

    const Part p = make_part(1, 12, 40);
    for (int cyc = 0; cyc < 200; ++cyc) {
      b.load_particle(p);
      b.load_geometry(0x3000 + cyc, -0x100 - cyc, 0x77 + cyc, 0x0123);
      top.p_valid_i = 1;
      top.g_valid_i = 1;
      b.step();
    }
    b.idle(30);

    check(top.contended_o > 0, "contention: the counter moved", 1, top.contended_o > 0);
    check(top.geom_grants_o > 0, "contention: geometry was granted", 1, top.geom_grants_o > 0);
    check(top.part_grants_o > 0, "contention: particles were granted", 1, top.part_grants_o > 0);
    // Particles are additionally limited by SLOTS and the projector round trip,
    // so geometry legitimately wins more often; what must not happen is either
    // client being shut out, and geometry must not be halved by a guest.
    check(top.geom_grants_o >= top.part_grants_o,
          "contention: geometry is not starved by the guest", 1,
          top.geom_grants_o >= top.part_grants_o);
    check(top.slot_pressure_o > 0, "contention: the slot ring backpressured (nothing dropped)", 1,
          top.slot_pressure_o > 0);
    check(top.particles_projected_o == top.part_grants_o,
          "contention: every granted particle came back", top.part_grants_o,
          top.particles_projected_o);
    check(b.emitted.size() == top.particles_projected_o,
          "contention: every projected particle was emitted", top.particles_projected_o,
          b.emitted.size());
    check(top.geom_tag_collision_o == 0, "contention: no tag collision", 0,
          top.geom_tag_collision_o);
    check(top.ladder_unexpected_o == 0, "contention: no unpaired verdict", 0,
          top.ladder_unexpected_o);
  }

  // =========================================================================
  // E. BACKPRESSURE -- the consumer stops, and NOTHING IS LOST.
  //
  // The correct behaviour is that the producer STALLS: `p_ready_o` goes low and
  // `slot_pressure_o` counts the refusal. That is the property asserted here,
  // rather than an overflow counter reading zero -- the overflow state is gated
  // at the allocation site and is unreachable, so a counter watching it would
  // be exactly the reassuring silence this tree has a chapter about.
  // =========================================================================
  {
    b.reset();
    b.emitted.clear();
    top.cfg_base_radius_i = static_cast<uint32_t>(1 << 16);

    std::vector<Part> parts;
    for (int i = 0; i < 10; ++i)
      parts.push_back(make_part(20 + i, static_cast<uint8_t>(8 + i), 50 + 3 * i));

    b.q_ready = false;
    size_t sent = 0;
    for (int cyc = 0; cyc < 120; ++cyc) {
      b.clear_offers();
      if (sent < parts.size()) {
        b.load_particle(parts[sent]);
        top.p_valid_i = 1;
        b.pre();
        if (top.p_ready_o) ++sent;
      }
      b.step();
    }
    check(b.emitted.empty(), "backpressure: nothing emitted while the consumer is stopped", 0,
          b.emitted.size());
    check(sent < parts.size(), "backpressure: the producer was held short of the whole batch", 1,
          sent < parts.size());
    check(top.slot_pressure_o > 0, "backpressure: the refusal was counted", 1,
          top.slot_pressure_o > 0);
    const uint32_t accepted = top.part_grants_o;

    b.q_ready = true;
    for (int cyc = 0; cyc < 400 && (sent < parts.size() || b.emitted.size() < parts.size());
         ++cyc) {
      b.clear_offers();
      if (sent < parts.size()) {
        b.load_particle(parts[sent]);
        top.p_valid_i = 1;
        b.pre();
        if (top.p_ready_o) ++sent;
      }
      b.step();
    }
    check(sent == parts.size(), "backpressure: the whole batch went in once the consumer resumed",
          parts.size(), sent);
    check(b.emitted.size() == parts.size(), "backpressure: every particle came out -- none dropped",
          parts.size(), b.emitted.size());
    check(top.part_grants_o == parts.size(), "backpressure: grants equal the batch", parts.size(),
          top.part_grants_o);
    check(accepted > 0 && accepted <= parts.size(),
          "backpressure: some but not all had been accepted while stalled", 1,
          accepted > 0 && accepted <= parts.size());
    bool order_ok = true;
    for (size_t i = 0; i < b.emitted.size(); ++i)
      if (b.emitted[i].src_id != parts[i].src_id) order_ok = false;
    check(order_ok, "backpressure: order is preserved across the stall", 1, order_ok);
  }

  // =========================================================================
  // F. SIZE SATURATION -- the reference's own clamps, reached.
  // =========================================================================
  {
    b.reset();
    b.emitted.clear();
    top.cfg_base_radius_i = static_cast<uint32_t>(0x7FFF'0000);

    const Part p = make_part(31, 63, 0x1FFFF);  // the largest legal size and z
    for (int cyc = 0; cyc < 80 && b.emitted.empty(); ++cyc) {
      b.clear_offers();
      if (top.part_grants_o == 0) {
        b.load_particle(p);
        top.p_valid_i = 1;
      }
      b.step();
    }
    check(b.emitted.size() == 1, "saturation: the particle came out", 1, b.emitted.size());
    if (b.emitted.size() == 1) {
      const int32_t radius =
          zref::part::particle_radius(static_cast<int32_t>(0x7FFF0000), p.rec.size);
      const SizeOracle so = size_oracle(radius, part_d(p));
      check(b.emitted[0].size16 == so.size16, "saturation: size16 matches the oracle", so.size16,
            b.emitted[0].size16);
      check(b.emitted[0].size8 == so.size8, "saturation: size8 matches the oracle", so.size8,
            b.emitted[0].size8);
      check(so.saturated, "saturation: the oracle agrees this case clamps", 1, so.saturated);
      check(top.size_saturations_o > 0, "saturation: the counter moved", 1,
            top.size_saturations_o > 0);
    }
  }

  // =========================================================================
  // G. POSITIVE CONTROL 1 -- `geom_tag_collision_o` fires.
  //
  // The rider's top TWO bits are free only because zhao_geom_proj_lane pads at
  // the top and the console's ARENA_W + INDEX_W is 15 of 17. The day that stops
  // being true this counter is what says so, and a counter nobody has seen move
  // is a claim. It is fired here through a port the bench owns.
  //
  // R68 SUB-BUILD 4 RE-AUTHORED WHAT "POISONED" MEANS. Under the old one-bit
  // law this poked bit 15 and that WAS the tag. Bit 15 is now the LOW bit of a
  // two-bit field, so the poke still collides -- but it does so for a different
  // reason, and a version of this test that kept `1u << 15` while the RTL moved
  // to 17 bits would have gone on passing for the wrong reason at width 16 and
  // failed confusingly at 17. The owner is set through `with_owner` so the
  // intent survives the next width change.
  // =========================================================================
  {
    b.reset();
    b.geom_seen.clear();
    const uint32_t poisoned = with_owner(0x0041u, kOwnerPart);
    b.clear_offers();
    b.load_geometry(0x2000, 0x1000, 0x30, poisoned);
    top.g_valid_i = 1;
    b.pre();
    const bool took = top.g_ready_o != 0;
    b.step();
    b.idle(20);
    check(took, "tag control: the poisoned vertex was accepted", 1, took);
    check(top.geom_tag_collision_o == 1, "tag control: the detector FIRED", 1,
          top.geom_tag_collision_o);
    check(b.geom_seen.size() == 1, "tag control: it still returned as GEOMETRY, not as a particle",
          1, b.geom_seen.size());
    check(top.particles_projected_o == 0, "tag control: it was not mistaken for a particle", 0,
          top.particles_projected_o);
    check(top.owner_unroutable_o == 0,
          "tag control: a FORCED-to-GEOM rider is still routable", 0,
          top.owner_unroutable_o);
  }

  // =========================================================================
  // G2. THE OWNER FIELD IS FULL, AND EVERY CODE IS DELIVERED.
  //
  // THIS CASE USED TO BE A POSITIVE CONTROL AND DELIBERATELY IS NOT ANY MORE.
  // It fired `owner_unroutable_o` by forcing an UNCLAIMED owner onto the
  // returning rider. There are no unclaimed owners left: 2'd0 geometry, 2'd1
  // particles, 2'd2 FORGE.PRIM, 2'd3 GEOM.LODSTATE's instance centre. The
  // block's own header says what comes after that -- a fifth owner is a
  // `GEOM_OWNER_W_C` widening that an elaboration guard refuses.
  //
  // So what is asserted here is the CORRECT behaviour, which is also the
  // REASON the detector is now silent: every code reaches an arm. Asserting
  // the old way would have been asserting a bug, which this tree forbids
  // ("do not write a test that asserts the bug"); the instrument's own
  // evidence moved to a committed mutant, which is where an unreachable
  // guard's evidence belongs.
  // =========================================================================
  {
    b.reset();
    b.geom_seen.clear();
    b.clear_offers();
    b.force_a_owner = static_cast<int>(kOwnerLod);
    b.load_geometry(0x2000, 0x1000, 0x30, with_owner(0x0041u, kOwnerGeom));
    top.g_valid_i = 1;
    b.pre();
    b.step();

    bool lod_arm_fired = false;
    for (int i = 0; i < 60; ++i) {
      if (top.rl_valid_o) lod_arm_fired = true;
      b.idle(1);
    }
    b.force_a_owner = -1;

    check(lod_arm_fired,
          "owner 3 REACHES the instance-centre demux arm -- the LAST code the "
          "field holds is routed",
          1, lod_arm_fired);
    check(top.owner_unroutable_o == 0,
          "and the detector stays PUT: with four arms no code is unroutable, "
          "which is why its positive control is now a committed mutant",
          0, top.owner_unroutable_o);
    check(b.geom_seen.empty(),
          "owner 3 was NOT also delivered as geometry", 0, b.geom_seen.size());
    check(top.particles_projected_o == 0,
          "owner 3 was NOT also delivered as a particle", 0,
          top.particles_projected_o);
    check(top.geom_tag_collision_o == 0,
          "and the INGRESS detector stayed quiet (a different fault, on a "
          "different port)",
          0, top.geom_tag_collision_o);
  }

  // =========================================================================
  // G2b. OWNER 2 IS ROUTED NOW, AND THAT IS THE OTHER HALF OF G2.
  //
  // Removing owner 2 from the loop above is only honest if owner 2 is actually
  // DELIVERED rather than merely stopped being counted. A demux arm that was
  // added to the detector's exclusion list and nowhere else would pass G2 and
  // drop every forge vertex silently -- the counter reading zero about a path
  // that does not work, which is the mirror of the fault the counter exists
  // for. So: force owner 2, and require BOTH that the detector stays put AND
  // that the forge arm fires.
  // =========================================================================
  {
    b.reset();
    b.geom_seen.clear();
    b.clear_offers();
    b.force_a_owner = static_cast<int>(kOwnerForge);
    b.load_geometry(0x2000, 0x1000, 0x30, with_owner(0x0041u, kOwnerGeom));
    top.g_valid_i = 1;
    b.pre();
    b.step();

    bool forge_arm_fired = false;
    for (int i = 0; i < 60; ++i) {
      if (top.rf_valid_o) forge_arm_fired = true;
      b.idle(1);
    }
    b.force_a_owner = -1;

    check(forge_arm_fired,
          "owner 2 REACHES the forge demux arm -- it is routed, not merely "
          "excluded from the detector",
          1, forge_arm_fired);
    check(top.owner_unroutable_o == 0,
          "and the unroutable detector stays PUT on a routed owner", 0,
          top.owner_unroutable_o);
    check(b.geom_seen.empty(),
          "owner 2 was NOT also delivered as geometry", 0, b.geom_seen.size());
    check(top.particles_projected_o == 0,
          "owner 2 was NOT also delivered as a particle", 0,
          top.particles_projected_o);
  }

  // The negative control for the detector above: with the owner left exactly
  // as the projector returned it, the same stimulus must route normally and
  // the counter must NOT move. A detector that fires on everything is as
  // useless as one that never fires, and only the pair separates them.
  {
    b.reset();
    b.geom_seen.clear();
    b.clear_offers();
    b.load_geometry(0x2000, 0x1000, 0x30, with_owner(0x0041u, kOwnerGeom));
    top.g_valid_i = 1;
    b.pre();
    b.step();
    b.idle(40);
    check(top.owner_unroutable_o == 0,
          "unroutable negative control: a well-owned rider does NOT fire it", 0,
          top.owner_unroutable_o);
    check(b.geom_seen.size() == 1,
          "unroutable negative control: it routed as geometry", 1,
          b.geom_seen.size());
    check(owner_of(b.geom_seen[0].payload) == kOwnerGeom,
          "unroutable negative control: it came back owned by GEOM",
          kOwnerGeom, owner_of(b.geom_seen[0].payload));
  }

  // =========================================================================
  // H. POSITIVE CONTROL 2 -- `ladder_unexpected_o` fires.
  //
  // A verdict with nothing outstanding cannot come from PART.LADDER, which only
  // answers what it was asked. It CAN come from the port, and an unpaired
  // verdict applied silently would land on the next particle -- so the counter
  // exists and is shown to move.
  // =========================================================================
  {
    b.reset();
    b.force_rng_valid = true;
    b.clear_offers();
    b.step();
    b.force_rng_valid = false;
    check(top.ladder_unexpected_o == 1, "ladder control: the detector FIRED", 1,
          top.ladder_unexpected_o);
    check(top.q_valid_o == 0, "ladder control: no particle was invented from it", 0, top.q_valid_o);
    b.idle(10);
    check(top.ladder_unexpected_o == 1, "ladder control: it did not keep firing afterwards", 1,
          top.ladder_unexpected_o);
  }

  return zhao::report_and_exit("part_project_directed");
}
