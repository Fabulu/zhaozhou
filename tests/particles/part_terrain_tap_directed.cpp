// part_terrain_tap_directed.cpp -- console entry I6, the chain end to end:
// TERRAIN.HEIGHTTAP -> PART.TERRAIN_TAP -> PART.COLLIDE, all three REAL.
//
// THE ORACLE IS THE REFERENCE, NOT A TRANSCRIPTION. Every sample the collider is
// handed is differenced against `zref::terrain::column_query` (the height) and
// `zref::terrain::collision_normal` over `zref::terrain::column_pick` (owner
// ruling R1's normal), taken through the particle frame of spec/qformats.md 10:
//
//     world     = origin + (local <<< 8)
//     t_height  = sat_s18( (top - origin_y + 128) >>> 8 )     one rounding
//     t_n       = (n_fx16 + 32) >>> 6                          one rounding
//
// WHAT "THE VALUE TRAVERSES" MEANS HERE, concretely: a particle placed below the
// reference's terrain height leaves PART.COLLIDE counted as a TERRAIN contact
// and standing at exactly that height plus CLEAR_EPS -- a number that exists
// nowhere in the bench, only in the lattice the tap read.
//
// EVERY CENSUS IN THE NEW BLOCK IS FIRED WITH LEGAL STIMULUS: ground, no-ground
// (void cell and a patch that is not staged), missed (cold cache), a cell whose
// placement is off the D grid, a world position outside fx16, terrain above the
// population cube, a fill overtaken by an invalidation, and the invalidation
// itself. So no committed mutant is owed for this block.
//
// The fixture is `zhao_terrain_compcache_front`'s two read ports only -- the
// same model `terrain_heighttap_directed.cpp` uses -- because the block under
// test is a client of them; it invents no traffic.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vtb_part_terrain_chain.h"
#include "verilated.h"
#include "zhao_sim.hpp"
#include "zref/zref_particle.hpp"
#include "zref/zref_terrain.hpp"

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

constexpr int LAT = 33;
constexpr int CELLS = LAT - 1;
constexpr int32_t kPoison = 0x5BADF00D;
constexpr uint8_t kSlide = 3;
constexpr int32_t kEps = 2;  // PART.COLLIDE's CLEAR_EPS, position LSBs

struct Cache {
  bool served = true;
  std::vector<int32_t> top, bottom, wx, wz;
  std::vector<uint8_t> sub;
  Cache() : top(LAT * LAT, 0), bottom(LAT * LAT, 0), wx(LAT, 0), wz(LAT, 0),
            sub(CELLS * CELLS, 0) {}
  // TERRAIN.PLACE's forward map, plus an optional offset that puts the lattice
  // OFF the D grid (a placement fault the tap's `ud == D` check cannot see).
  void place(int patch_ix, int patch_iz, int pitch_log2, int32_t off = 0) {
    const int sh = 16 + pitch_log2;
    for (int i = 0; i < LAT; ++i) {
      wx[static_cast<size_t>(i)] =
          static_cast<int32_t>(((static_cast<int64_t>(patch_ix) * 32 + i) << sh) + off);
      wz[static_cast<size_t>(i)] =
          static_cast<int32_t>(((static_cast<int64_t>(patch_iz) * 32 + i) << sh) + off);
    }
  }
  int32_t h(int vi, int vj, int s) const {
    if (!served || vi >= LAT || vj >= LAT) return kPoison;
    const size_t k = static_cast<size_t>(vj) * LAT + static_cast<size_t>(vi);
    return s ? bottom[k] : top[k];
  }
  int32_t cx(int vi) const { return (!served || vi >= LAT) ? kPoison : wx[static_cast<size_t>(vi)]; }
  int32_t cz(int vj) const { return (!served || vj >= LAT) ? kPoison : wz[static_cast<size_t>(vj)]; }
  uint8_t substance(int ci, int cj) const {
    if (!served || ci >= CELLS || cj >= CELLS) return 3;
    return sub[static_cast<size_t>(cj) * CELLS + static_cast<size_t>(ci)];
  }
  zref::terrain::ComposedLattice ref() const {
    zref::terrain::ComposedLattice l;
    l.w = LAT;
    l.h = LAT;
    l.dual = true;
    l.wx = wx;
    l.wz = wz;
    l.top = top;
    l.bottom = bottom;
    l.cell_state = sub;
    return l;
  }
};

uint32_t rnd(uint32_t& s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

void relief(Cache& c, uint32_t seed) {
  uint32_t s = seed;
  for (int j = 0; j < LAT; ++j)
    for (int i = 0; i < LAT; ++i) {
      const size_t k = static_cast<size_t>(j) * LAT + static_cast<size_t>(i);
      c.top[k] = static_cast<int32_t>(rnd(s) % 400000u) - 200000 + (i - j) * 1500;
      c.bottom[k] = c.top[k] - 300000;
    }
}

using Dut = Vtb_part_terrain_chain;

struct Bench {
  Dut d;
  Cache c;
  uint64_t cycles = 0;

  void step() {
    d.eval();
    const bool lreq = d.c_lat_req_o != 0;
    const int vi = d.c_lat_vi_o, vj = d.c_lat_vj_o, sf = d.c_lat_surface_o;
    const bool creq = d.c_cs_req_o != 0;
    const int ci = d.c_cs_ci_o, cj = d.c_cs_cj_o;
    d.d_response_i = kSlide;   // PART.TABLE, answering every species alike
    zhao::tick(d);
    ++cycles;
    d.c_lat_h_i = static_cast<uint32_t>(lreq ? c.h(vi, vj, sf) : kPoison);
    d.c_lat_wx_i = static_cast<uint32_t>(lreq ? c.cx(vi) : kPoison);
    d.c_lat_wz_i = static_cast<uint32_t>(lreq ? c.cz(vj) : kPoison);
    d.c_cs_substance_i = creq ? c.substance(ci, cj) : 3;
    d.eval();
  }

  void reset(int pitch, int32_t ox, int32_t oy, int32_t oz) {
    d.rst_n = 0;
    d.pitch_log2_i = static_cast<uint8_t>(static_cast<int8_t>(pitch));
    d.origin_x_i = static_cast<uint32_t>(ox);
    d.origin_y_i = static_cast<uint32_t>(oy);
    d.origin_z_i = static_cast<uint32_t>(oz);
    d.inval_i = 0;
    d.p_valid_i = 0;
    d.p_events_i = 0;
    d.p_side_i = 0;
    d.c_ready_i = 1;
    d.d_response_i = kSlide;
    for (int w = 0; w < 4; ++w) d.p_record_i[w] = 0;
    d.c_lat_h_i = static_cast<uint32_t>(kPoison);
    d.c_lat_wx_i = static_cast<uint32_t>(kPoison);
    d.c_lat_wz_i = static_cast<uint32_t>(kPoison);
    d.c_cs_substance_i = 3;
    for (int i = 0; i < 4; ++i) step();
    d.rst_n = 1;
    step();
  }

  void idle(int n) {
    for (int i = 0; i < n; ++i) step();
  }
};

struct Sample {
  bool t_valid = false;
  int32_t t_height = 0, nx = 0, ny = 0, nz = 0;
  bool contact = false;
  zref::part::Particle128 out{};
  uint64_t accept_cycle = 0;
  uint16_t side = 0;   // what arrived beside the record on the collider's take
};

int32_t sx(uint32_t v, int bits) { return zref::part::sign_extend(v & ((1u << bits) - 1u), bits); }

// One particle through the chain. Returns what PART.COLLIDE was handed on the
// beat it TOOK, and what came out of it.
uint16_t g_side = 0x5A00;

Sample run(Bench& b, const zref::part::Particle128& p) {
  const uint16_t side = ++g_side;
  b.d.p_side_i = side;
  uint64_t lo = 0, hi = 0;
  zref::part::particle_pack(p, &lo, &hi);
  b.d.p_record_i[0] = static_cast<uint32_t>(lo);
  b.d.p_record_i[1] = static_cast<uint32_t>(lo >> 32);
  b.d.p_record_i[2] = static_cast<uint32_t>(hi);
  b.d.p_record_i[3] = static_cast<uint32_t>(hi >> 32);
  b.d.p_valid_i = 1;
  Sample s;
  int guard = 0;
  b.d.eval();
  while (b.d.p_ready_o == 0 && guard++ < 1000) b.step();
  s.accept_cycle = b.cycles;
  b.step();  // accepted
  b.d.p_valid_i = 0;
  guard = 0;
  while (b.d.t_take_o == 0 && guard++ < 1000) b.step();
  s.t_valid = b.d.t_valid_o != 0;
  s.t_height = sx(b.d.t_height_o, 18);
  s.nx = sx(b.d.t_nx_o, 12);
  s.ny = sx(b.d.t_ny_o, 12);
  s.nz = sx(b.d.t_nz_o, 12);
  s.side = static_cast<uint16_t>(b.d.q_side_o);
  // THE JOIN. The console loads facts about a particle on the collider's take;
  // they must be THIS particle's, never the one behind it in the stage.
  check(s.side == side, "the sideband arrives with its own particle", side, s.side);
  b.step();  // the collider registers its answer
  s.contact = b.d.c_contact_o != 0;
  const uint64_t olo = static_cast<uint64_t>(b.d.c_record_o[0]) |
                       (static_cast<uint64_t>(b.d.c_record_o[1]) << 32);
  const uint64_t ohi = static_cast<uint64_t>(b.d.c_record_o[2]) |
                       (static_cast<uint64_t>(b.d.c_record_o[3]) << 32);
  zref::part::particle_unpack(olo, ohi, &s.out);
  return s;
}

zref::part::Particle128 particle_at(int32_t lx, int32_t ly, int32_t lz) {
  zref::part::Particle128 p{};
  p.pos[0] = lx;
  p.pos[1] = ly;
  p.pos[2] = lz;
  p.vel[1] = -4;  // falling, a little
  p.species = 5;
  return p;
}

// The reference, taken through the particle frame.
struct Want {
  bool solid = false;
  int32_t height = 0, nx = 0, ny = 0, nz = 0;
  bool sat = false;
};

Want expect(const Cache& c, int32_t wx, int32_t wz, int32_t oy) {
  Want w;
  const zref::terrain::ComposedLattice l = c.ref();
  const zref::terrain::ColumnResult r = zref::terrain::column_query(l, zref::fx16{wx}, zref::fx16{wz});
  if (r.cls != zref::terrain::ColumnClass::kSolid) return w;
  w.solid = true;
  const int64_t hq = (static_cast<int64_t>(r.top.raw) - oy + 128) >> 8;
  w.sat = hq > 131071 || hq < -131072;
  w.height = static_cast<int32_t>(hq > 131071 ? 131071 : (hq < -131072 ? -131072 : hq));
  const zref::terrain::ColumnPick pk = zref::terrain::column_pick(l, zref::fx16{wx}, zref::fx16{wz});
  const zref::terrain::CollisionNormal n = zref::terrain::collision_normal(l, pk, false);
  w.nx = static_cast<int32_t>((static_cast<int64_t>(n.n.x.raw) + 32) >> 6);
  w.ny = static_cast<int32_t>((static_cast<int64_t>(n.n.y.raw) + 32) >> 6);
  w.nz = static_cast<int32_t>((static_cast<int64_t>(n.n.z.raw) + 32) >> 6);
  return w;
}

struct Census {
  uint32_t particles, ground, no_ground, missed, mismatch, range, hsat, issued, landed, discarded,
      inval, contacts, unavail;
};
Census census(const Dut& d) {
  return Census{d.particles_o,        d.samples_ground_o, d.samples_no_ground_o,
                d.samples_missed_o,   d.cell_mismatch_o,  d.out_of_range_o,
                d.height_sats_o,      d.fills_issued_o,   d.fills_landed_o,
                d.fills_discarded_o,  d.invalidations_o,  d.contacts_terrain_o,
                d.terrain_sample_unavailable_o};
}

void check_invariant(const Dut& d, const char* where) {
  const Census k = census(d);
  char what[128];
  std::snprintf(what, sizeof what, "%s: every particle lands in exactly one bucket", where);
  check(k.particles == k.ground + k.no_ground + k.missed + k.mismatch + k.range, what, k.particles,
        static_cast<long long>(k.ground + k.no_ground + k.missed + k.mismatch + k.range));
  std::snprintf(what, sizeof what, "%s: PART.COLLIDE's 'unavailable' is exactly the non-ground", where);
  check(k.unavail == k.particles - k.ground, what, k.particles - k.ground, k.unavail);
  std::snprintf(what, sizeof what, "%s: every issued fill landed or was discarded", where);
  check(k.issued == k.landed + k.discarded || k.issued == k.landed + k.discarded + 1, what, k.issued,
        static_cast<long long>(k.landed + k.discarded));
}

// ---------------------------------------------------------------------------
// 1. THE DIFFERENTIAL, THROUGH THE FRAME, INTO THE COLLIDER.
// ---------------------------------------------------------------------------
void test_samples_equal_the_reference_and_reach_the_collider() {
  const int pitches[4] = {-1, 0, 1, 2};
  for (int pi = 0; pi < 4; ++pi) {
    const int pl = pitches[pi], sh = 16 + pl;
    const int32_t D = 1 << sh;
    Bench b;
    b.c.place(0, 0, pl);
    relief(b.c, 0x600DF00Du + static_cast<uint32_t>(pi));
    // The origin: on the 1/256-m grid, somewhere inside the patch.
    const int32_t ox = (3 << sh) + (5 << 8), oy = -(2 << 16) + (7 << 8), oz = (4 << sh) + (1 << 8);
    b.reset(pl, ox, oy, oz);

    // Particles clustered in THREE cells, so a four-cell cache holds them all.
    uint32_t s = 0xACE1u + static_cast<uint32_t>(pi) * 31u;
    std::vector<zref::part::Particle128> ps;
    std::vector<Want> ws;
    const int cells[3][2] = {{7, 9}, {8, 9}, {20, 3}};
    for (int n = 0; n < 24; ++n) {
      const int* cc = cells[n % 3];
      // World point inside the cell, on the 1/256-m grid the particle frame has.
      const int32_t wx = (cc[0] << sh) + static_cast<int32_t>((rnd(s) % static_cast<uint32_t>(D >> 8)) << 8);
      const int32_t wz = (cc[1] << sh) + static_cast<int32_t>((rnd(s) % static_cast<uint32_t>(D >> 8)) << 8);
      const int32_t lx = (wx - ox) >> 8, lz = (wz - oz) >> 8;
      const Want w = expect(b.c, wx, wz, oy);
      check(w.solid, "the oracle calls these points solid", 1, w.solid ? 1 : 0);
      // Half the particles start BELOW the reference's surface, half above.
      const int32_t ly = (n & 1) ? w.height - 40 : w.height + 300;
      ps.push_back(particle_at(lx, ly, lz));
      ws.push_back(w);
    }

    // Warm-up: the cache is cold. Nobody waits; each miss the fill engine is
    // free to take starts a fetch behind the particle, so a few passes over
    // the same set bring all three cells in.
    for (int pass = 0; pass < 8 && b.d.fills_landed_o < 3; ++pass) {
      for (const auto& p : ps) (void)run(b, p);
      b.idle(400);
    }
    check(b.d.fills_landed_o == 3, "exactly the three cells were fetched", 3,
          static_cast<long long>(b.d.fills_landed_o));
    check(b.d.fills_issued_o == 3, "and each was asked for once", 3,
          static_cast<long long>(b.d.fills_issued_o));
    check(b.d.samples_missed_o > 0, "a cold cache MISSES rather than stalling", 1,
          b.d.samples_missed_o > 0 ? 1 : 0);

    // Pass 2: every particle is over a cached cell.
    const uint32_t contacts0 = b.d.contacts_terrain_o;
    uint32_t below = 0;
    for (size_t n = 0; n < ps.size(); ++n) {
      const Sample got = run(b, ps[n]);
      const Want& w = ws[n];
      check(got.t_valid, "a cached solid cell yields a VALID sample", 1, got.t_valid ? 1 : 0);
      check(got.t_height == w.height, "t_height == rescale(column_query.top - origin_y, 8)",
            w.height, got.t_height);
      check(got.nx == w.nx, "t_nx == rescale(collision_normal.x, 6)", w.nx, got.nx);
      check(got.ny == w.ny, "t_ny == rescale(collision_normal.y, 6)", w.ny, got.ny);
      check(got.nz == w.nz, "t_nz == rescale(collision_normal.z, 6)", w.nz, got.nz);
      check(got.ny > 0, "the normal handed to the collider points up", 1, got.ny > 0 ? 1 : 0);
      if (n & 1) {
        ++below;
        // THE TRAVERSAL: the collider placed the particle ON the reference's
        // surface. That number exists only in the lattice the tap read.
        check(got.contact, "a particle below the terrain CONTACTS it", 1, got.contact ? 1 : 0);
        check(got.out.pos[1] == w.height + kEps,
              "and leaves standing at the reference's height + CLEAR_EPS", w.height + kEps,
              got.out.pos[1]);
      } else {
        check(!got.contact, "a particle above the terrain does not", 0, got.contact ? 1 : 0);
      }
    }
    check(b.d.contacts_terrain_o - contacts0 == below, "PART.COLLIDE counted every terrain contact",
          below, static_cast<long long>(b.d.contacts_terrain_o - contacts0));
    check(b.d.height_sats_o == 0, "no height saturation inside the population cube", 0,
          static_cast<long long>(b.d.height_sats_o));
    check_invariant(b.d, "pass 2");
  }
}

// ---------------------------------------------------------------------------
// 2. THE PRICE: six clocks per particle, measured, and independent of terrain.
// ---------------------------------------------------------------------------
void test_the_cadence_is_six_clocks_hit_or_miss() {
  const int pl = 0, sh = 16;
  Bench b;
  b.c.place(0, 0, pl);
  relief(b.c, 0x1111u);
  b.reset(pl, 0, 0, 0);
  const zref::part::Particle128 hot = particle_at(((5 << sh) + (1 << 12)) >> 8, 0, ((5 << sh) + (1 << 12)) >> 8);
  (void)run(b, hot);
  b.idle(600);
  // Hits and misses back to back: the cadence must not depend on which.
  const zref::part::Particle128 cold = particle_at(((25 << sh) + 77 * 256) >> 8, 0, ((6 << sh) + 3 * 256) >> 8);
  const uint64_t t0 = run(b, hot).accept_cycle;
  const uint64_t t1 = run(b, cold).accept_cycle;
  const uint64_t t2 = run(b, hot).accept_cycle;
  check(t1 - t0 == t2 - t1, "a miss costs the particle path exactly what a hit does", t2 - t1, t1 - t0);
  // accept, look up, two multiplies, finish, present: six.
  check(t2 - t1 == 6, "one particle every six clocks", 6, static_cast<long long>(t2 - t1));
}

// ---------------------------------------------------------------------------
// 3. COHERENCE: an invalidation empties the cache and discards a fill in flight.
// ---------------------------------------------------------------------------
void test_invalidation_empties_the_cache_and_discards_a_fill_in_flight() {
  const int pl = 1, sh = 17;
  Bench b;
  b.c.place(0, 0, pl);
  relief(b.c, 0x2222u);
  b.reset(pl, 0, 0, 0);
  const zref::part::Particle128 p = particle_at(((4 << sh) + 999 * 256) >> 8, 0, ((4 << sh) + 3 * 256) >> 8);
  (void)run(b, p);
  b.idle(600);
  const Sample pre = run(b, p);
  check(pre.t_valid, "cached before the invalidation", 1, pre.t_valid ? 1 : 0);

  b.d.inval_i = 1;
  b.step();
  b.d.inval_i = 0;
  check(b.d.invalidations_o == 1, "the invalidation is counted", 1,
        static_cast<long long>(b.d.invalidations_o));
  const uint32_t missed0 = b.d.samples_missed_o;
  const Sample after = run(b, p);
  check(!after.t_valid, "the same cell MISSES after the lattice changed", 0, after.t_valid ? 1 : 0);
  check(b.d.samples_missed_o == missed0 + 1, "counted as a miss", missed0 + 1,
        static_cast<long long>(b.d.samples_missed_o));

  // That miss started a fill. Invalidate while it is in flight: its answer
  // describes a lattice that is gone, so it must NOT land.
  const uint32_t disc0 = b.d.fills_discarded_o, land0 = b.d.fills_landed_o;
  b.idle(5);
  b.d.inval_i = 1;
  b.step();
  b.d.inval_i = 0;
  b.idle(600);
  check(b.d.fills_discarded_o == disc0 + 1, "a fill overtaken by an invalidation is DISCARDED",
        disc0 + 1, static_cast<long long>(b.d.fills_discarded_o));
  check(b.d.fills_landed_o == land0, "and did not land", land0,
        static_cast<long long>(b.d.fills_landed_o));
  const Sample still = run(b, p);
  check(!still.t_valid, "so the cell is still not cached", 0, still.t_valid ? 1 : 0);
  check_invariant(b.d, "coherence");
}

// ---------------------------------------------------------------------------
// 4. CORRECT REFUSALS: a void cell, and a patch that is not the served one.
// ---------------------------------------------------------------------------
void test_void_and_off_patch_are_no_ground_not_faults() {
  const int pl = 0, sh = 16;
  Bench b;
  b.c.place(0, 0, pl);
  relief(b.c, 0x3333u);
  b.c.sub[static_cast<size_t>(10) * CELLS + 12] = 1;   // cell (12,10) is void
  b.reset(pl, 0, 0, 0);
  const zref::part::Particle128 v = particle_at(((12 << sh) + 40 * 256) >> 8, -100, ((10 << sh) + 9 * 256) >> 8);
  const zref::part::Particle128 off = particle_at(((70 << sh) + 40 * 256) >> 8, -100, ((3 << sh) + 9 * 256) >> 8);
  (void)run(b, v);
  b.idle(600);
  (void)run(b, off);
  b.idle(600);
  const uint32_t ng0 = b.d.samples_no_ground_o;
  const Sample a = run(b, v);
  const Sample c = run(b, off);
  check(!a.t_valid && !c.t_valid, "no ground means no sample", 0, (a.t_valid || c.t_valid) ? 1 : 0);
  check(!a.contact && !c.contact, "and no contact", 0, (a.contact || c.contact) ? 1 : 0);
  check(b.d.samples_no_ground_o == ng0 + 2, "both counted as NO GROUND, from the cache", ng0 + 2,
        static_cast<long long>(b.d.samples_no_ground_o));
  check(b.d.taps_void_o == 1, "the tap saw the void once", 1, static_cast<long long>(b.d.taps_void_o));
  check(b.d.taps_off_patch_o == 1, "and the other patch once", 1,
        static_cast<long long>(b.d.taps_off_patch_o));
  check(b.d.cell_mismatch_o == 0, "neither is a fault", 0, static_cast<long long>(b.d.cell_mismatch_o));
  check_invariant(b.d, "refusals");
}

// ---------------------------------------------------------------------------
// 5. THE FAULT: a lattice placed OFF the D grid. The tap's `ud == vd == D`
//    check passes (the SPACING is right); the containment test here catches it.
// ---------------------------------------------------------------------------
void test_a_lattice_off_the_grid_is_a_cell_mismatch() {
  const int pl = 0, sh = 16;
  Bench b;
  b.c.place(0, 0, pl, 1 << 15);   // every vertex half a cell late
  relief(b.c, 0x4444u);
  b.reset(pl, 0, 0, 0);
  // Fill from the late half of cell 9, which the tap CAN answer...
  const zref::part::Particle128 late = particle_at(((9 << sh) + (3 << 14)) >> 8, 0, ((9 << sh) + (3 << 14)) >> 8);
  (void)run(b, late);
  b.idle(600);
  check(b.d.fills_landed_o == 1, "the late-half fill landed", 1,
        static_cast<long long>(b.d.fills_landed_o));
  // ...then ask about the EARLY half of the same key, which that placement
  // does not cover.
  const zref::part::Particle128 early = particle_at(((9 << sh) + (1 << 12)) >> 8, 0, ((9 << sh) + (1 << 12)) >> 8);
  const Sample s = run(b, early);
  check(!s.t_valid, "an uncovered point yields no sample", 0, s.t_valid ? 1 : 0);
  check(b.d.cell_mismatch_o == 1, "counted as a CELL MISMATCH fault", 1,
        static_cast<long long>(b.d.cell_mismatch_o));
  check_invariant(b.d, "mismatch");
}

// ---------------------------------------------------------------------------
// 6. THE FRAME'S EDGES: a world position outside fx16, and terrain above the
//    population cube.
// ---------------------------------------------------------------------------
void test_frame_edges_are_counted() {
  const int pl = 0, sh = 16;
  {
    Bench b;
    b.c.place(0, 0, pl);
    relief(b.c, 0x5555u);
    b.reset(pl, 0x7FFF0000, 0, 0);   // origin 32,767 m east
    const Sample s = run(b, particle_at(100000, 0, 0));   // +390 m further
    check(!s.t_valid, "a world position outside fx16 has no sample", 0, s.t_valid ? 1 : 0);
    check(b.d.out_of_range_o == 1, "counted OUT OF RANGE", 1, static_cast<long long>(b.d.out_of_range_o));
    check(b.d.fills_issued_o == 0, "and never asked the tap", 0,
          static_cast<long long>(b.d.fills_issued_o));
    check_invariant(b.d, "range");
  }
  {
    Bench b;
    b.c.place(0, 0, pl);
    relief(b.c, 0x6666u);
    const int32_t oy = -600 * 65536;   // the population sits 600 m below the ground
    b.reset(pl, 0, oy, 0);
    const zref::part::Particle128 p = particle_at(((6 << sh) + 3 * 256) >> 8, 0, ((6 << sh) + 5 * 256) >> 8);
    (void)run(b, p);
    b.idle(600);
    const Sample s = run(b, p);
    check(s.t_valid, "the sample is still delivered", 1, s.t_valid ? 1 : 0);
    check(s.t_height == 131071, "saturated at the top of the cube", 131071, s.t_height);
    check(b.d.height_sats_o == 1, "and counted as a HEIGHT SATURATION", 1,
          static_cast<long long>(b.d.height_sats_o));
  }
}

// ---------------------------------------------------------------------------
// 7. THE FRAME ROUNDING, PINNED AT ITS TIE. Added after a negative control:
//    moving the bias of `(hl + 128) >>> 8` by ONE LSB left case 1 green,
//    because random relief almost never puts the low byte of (top - origin_y)
//    at the one value a +/-1 bias changes. A flat lattice makes the height
//    exact, so the low byte is chosen rather than hoped for: 0x7F (a +1 bias
//    would round it up) and 0x80 (a -1 bias would round it down).
// ---------------------------------------------------------------------------
void test_the_frame_rounding_is_pinned_at_its_tie() {
  const int pl = 0, sh = 16;
  const int32_t lows[2] = {0x7F, 0x80};
  for (int k = 0; k < 2; ++k) {
    Bench b;
    b.c.place(0, 0, pl);
    const int32_t T = (0x1234 << 8) | lows[k];
    for (auto& h : b.c.top) h = T;
    for (auto& h : b.c.bottom) h = T - 300000;
    b.reset(pl, 0, 0, 0);
    const int32_t wx = (11 << sh) + 33 * 256, wz = (13 << sh) + 7 * 256;
    const zref::part::Particle128 p = particle_at(wx >> 8, 0, wz >> 8);
    (void)run(b, p);
    b.idle(600);
    const Sample s = run(b, p);
    const Want w = expect(b.c, wx, wz, 0);
    check(s.t_valid, "the flat cell is sampled", 1, s.t_valid ? 1 : 0);
    check(s.t_height == w.height, "the tie rounds exactly as rescale(., 8) does", w.height,
          s.t_height);
    check(w.height == ((T + 128) >> 8), "and that is round-half-up of the flat height",
          (T + 128) >> 8, w.height);
    // A flat face's normal is exactly straight up.
    check(s.nx == 0 && s.nz == 0 && s.ny == 1024, "a flat face's normal is (0, 1.0, 0)", 1024, s.ny);
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  test_samples_equal_the_reference_and_reach_the_collider();
  test_the_cadence_is_six_clocks_hit_or_miss();
  test_invalidation_empties_the_cache_and_discards_a_fill_in_flight();
  test_void_and_off_patch_are_no_ground_not_faults();
  test_a_lattice_off_the_grid_is_a_cell_mismatch();
  test_frame_edges_are_counted();
  test_the_frame_rounding_is_pinned_at_its_tie();
  std::printf("part_terrain_tap_directed: %d checks, %d failed\n", g_checks, g_failed);
  std::fflush(stdout);
  zhao::exit_hard(g_failed == 0 ? 0 : 1);
}
