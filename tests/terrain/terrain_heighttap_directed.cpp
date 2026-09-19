// terrain_heighttap_directed.cpp -- TERRAIN.HEIGHTTAP against the ratified law.
//
// THE ORACLE IS `zref::terrain::column_query` ITSELF, not a transcription of it
// into this file. `spec/terrain_rules.md` 4.3 names that symbol as THE reference
// point query and says "SW.CPUCOLL, PART.COLLIDE and TERRAIN.TESS all cite and
// consume it"; a second implementation living in a test is the exact failure
// CLAUDE.md records for the terrain shade header and for the two projectors. So
// this file links `zhao_zref` and asks the reference, and the RTL has to agree
// with it exactly -- not approximately, and not on a tolerance.
//
// The differential obligation is the spec's own, 4.3: "for random (wx, wz) the
// sim query result must equal the height of the rendered triangle at that point
// exactly (the 'physics equals pixels' test)".
//
// WHAT THE BEHAVIOURAL CACHE MODEL BELOW IS, AND WHAT IT IS NOT. It is
// `zhao_terrain_compcache_front`'s two READ PORTS and nothing else: a registered
// one-cycle read with no handshake, accepting every cycle, answering POISON when
// it has nothing (that block's own line 452 constant). It invents no traffic and
// it is not a stand-in for a producer -- the block under test is a CLIENT of
// those ports, so modelling them is modelling the test's fixture, not the
// design's missing half.
//
// FIVE OF THE SIX COUNTERS ARE FIRED HERE WITH LEGAL STIMULUS -- a void cell, a
// point off the staged patch, a lattice placed at a pitch the island does not
// declare, a pitch outside the frozen set, and the owner starving the tap of
// read cycles. A detector that has not been seen to fire is a claim, not an
// instrument.
//
// THE SIXTH, `interp_overflow_o`, CANNOT BE FIRED BY ANY LEGAL INPUT, and that
// is a result rather than a gap. The interpolation is a convex combination of
// three corner heights (the algebra is in the RTL header), so it cannot leave
// the interval they span. Its positive control is therefore a COMMITTED MUTANT
// with inverted polarity -- tests/mutants/zhao_terrain_heighttap_mutant.sv and
// terrain/terrain_heighttap_mutant_control.cpp -- exactly as CLAUDE.md requires
// for a guard no stimulus can reach. Test 7 below asserts the CORRECT behaviour
// instead: the answer stays between its rails and the counter stays at zero.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_terrain_heighttap.h"
#include "verilated.h"
#include "zhao_sim.hpp"
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

constexpr int LAT = 33;                       // vertices per side
constexpr int CELLS = LAT - 1;                // 32
constexpr int32_t kPoison = 0x5BADF00D;       // compcache_front.sv:452

// ---------------------------------------------------------------------------
// The fixture: the compose cache's two read ports, one cycle deep.
// ---------------------------------------------------------------------------
struct Cache {
  bool served = true;
  std::vector<int32_t> top, bottom, wx, wz;
  std::vector<uint8_t> sub;  // (CELLS*CELLS), 0 = SOLID

  Cache() : top(LAT * LAT, 0), bottom(LAT * LAT, 0), wx(LAT, 0), wz(LAT, 0),
            sub(CELLS * CELLS, 0) {}

  // TERRAIN.PLACE's forward map, zhao_terrain_place.sv:219-233 and :259-264:
  //   wx(i) = (patch_ix * 32 + i) <<< (16 + pitch_log2)
  void place(int patch_ix, int patch_iz, int pitch_log2) {
    const int sh = 16 + pitch_log2;
    for (int i = 0; i < LAT; ++i) {
      wx[static_cast<size_t>(i)] =
          static_cast<int32_t>((static_cast<int64_t>(patch_ix) * 32 + i) << sh);
      wz[static_cast<size_t>(i)] =
          static_cast<int32_t>((static_cast<int64_t>(patch_iz) * 32 + i) << sh);
    }
  }

  int32_t h(int vi, int vj, int surface) const {
    if (!served || vi >= LAT || vj >= LAT) return kPoison;
    const size_t k = static_cast<size_t>(vj) * LAT + static_cast<size_t>(vi);
    return surface ? bottom[k] : top[k];
  }
  int32_t cx(int vi) const { return (!served || vi >= LAT) ? kPoison : wx[static_cast<size_t>(vi)]; }
  int32_t cz(int vj) const { return (!served || vj >= LAT) ? kPoison : wz[static_cast<size_t>(vj)]; }
  uint8_t substance(int ci, int cj) const {
    if (!served || ci >= CELLS || cj >= CELLS) return 3;  // the no-answer code
    return sub[static_cast<size_t>(cj) * CELLS + static_cast<size_t>(ci)];
  }

  // The same lattice, in the reference's own shape.
  zref::terrain::ComposedLattice as_reference(bool dual) const {
    zref::terrain::ComposedLattice lat;
    lat.w = LAT;
    lat.h = LAT;
    lat.dual = dual;
    lat.wx = wx;
    lat.wz = wz;
    lat.top = top;
    lat.bottom = bottom;
    lat.cell_state = sub;
    return lat;
  }
};

using Dut = Vzhao_terrain_heighttap;

// One cycle, with the cache answering the previous cycle's request. The
// response is driven AFTER the edge and holds until the next one, which is what
// a registered read does.
void step(Dut& d, const Cache& c) {
  d.eval();
  const bool lreq = d.c_lat_req_o != 0;
  const int vi = d.c_lat_vi_o, vj = d.c_lat_vj_o, sf = d.c_lat_surface_o;
  const bool creq = d.c_cs_req_o != 0;
  const int ci = d.c_cs_ci_o, cj = d.c_cs_cj_o;

  zhao::tick(d);

  d.c_lat_h_i = static_cast<uint32_t>(lreq ? c.h(vi, vj, sf) : kPoison);
  d.c_lat_wx_i = static_cast<uint32_t>(lreq ? c.cx(vi) : kPoison);
  d.c_lat_wz_i = static_cast<uint32_t>(lreq ? c.cz(vj) : kPoison);
  d.c_cs_substance_i = creq ? c.substance(ci, cj) : 3;
  d.eval();
}

void reset(Dut& d, int pitch_log2) {
  d.rst_n = 0;
  d.pitch_log2_i = static_cast<uint8_t>(static_cast<int8_t>(pitch_log2));
  d.req_valid_i = 0;
  d.req_x_i = 0;
  d.req_z_i = 0;
  d.req_surface_i = 0;
  d.o_lat_req_i = 0;
  d.o_lat_vi_i = 0;
  d.o_lat_vj_i = 0;
  d.o_lat_surface_i = 0;
  d.o_cs_req_i = 0;
  d.o_cs_ci_i = 0;
  d.o_cs_cj_i = 0;
  d.c_lat_h_i = static_cast<uint32_t>(kPoison);
  d.c_lat_wx_i = static_cast<uint32_t>(kPoison);
  d.c_lat_wz_i = static_cast<uint32_t>(kPoison);
  d.c_cs_substance_i = 3;
  Cache empty;
  for (int i = 0; i < 4; ++i) step(d, empty);
  d.rst_n = 1;
  step(d, empty);
}

// Offer one tap and run until it answers. Returns false on a hang.
struct Answer {
  bool valid = false;
  int32_t height = 0;
  bool no_ground = false;
  int32_t nx = 0, ny = 0, nz = 0;   // owner ruling R1's collision normal
};

Answer tap(Dut& d, const Cache& c, int32_t x, int32_t z, int surface, int max_wait = 400) {
  Answer a;
  d.req_x_i = static_cast<uint32_t>(x);
  d.req_z_i = static_cast<uint32_t>(z);
  d.req_surface_i = static_cast<uint8_t>(surface);
  d.req_valid_i = 1;
  int waited = 0;
  while (d.req_ready_o == 0 && waited++ < max_wait) step(d, c);
  step(d, c);            // the accepting edge
  d.req_valid_i = 0;
  waited = 0;
  while (d.rsp_valid_o == 0 && waited++ < max_wait) step(d, c);
  if (d.rsp_valid_o != 0) {
    a.valid = true;
    a.height = static_cast<int32_t>(d.rsp_height_o);
    a.no_ground = d.rsp_no_ground_o != 0;
    a.nx = static_cast<int32_t>(d.rsp_nx_o);
    a.ny = static_cast<int32_t>(d.rsp_ny_o);
    a.nz = static_cast<int32_t>(d.rsp_nz_o);
  }
  return a;
}

// A lattice with some relief on it, deterministic so a failure is reproducible.
uint32_t rnd(uint32_t& s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

void fill_relief(Cache& c, uint32_t seed) {
  uint32_t s = seed;
  for (int j = 0; j < LAT; ++j) {
    for (int i = 0; i < LAT; ++i) {
      const size_t k = static_cast<size_t>(j) * LAT + static_cast<size_t>(i);
      // a few metres of relief in fx16, signed, plus a ramp so the two
      // triangles of a cell are genuinely different planes
      c.top[k] = static_cast<int32_t>((rnd(s) % 400000u)) - 200000 + (i - j) * 1500;
      c.bottom[k] = c.top[k] - 300000;
    }
  }
}

// THE NORMAL, owner ruling R1: normalize3_approx(face_normal(t)) of the picked
// triangle, asked of the REFERENCE (`zref::terrain::collision_normal` over
// `column_pick`, the same pick `column_query` uses), never transcribed here.
void check_normal(const zref::terrain::ComposedLattice& ref, int32_t x, int32_t z, bool bottom,
                  const Answer& a, const char* which) {
  const zref::terrain::ColumnPick p =
      zref::terrain::column_pick(ref, zref::fx16{x}, zref::fx16{z});
  const zref::terrain::CollisionNormal n = zref::terrain::collision_normal(ref, p, bottom);
  char what[96];
  std::snprintf(what, sizeof what, "%s normal x equals collision_normal", which);
  check(a.nx == n.n.x.raw, what, n.n.x.raw, a.nx);
  std::snprintf(what, sizeof what, "%s normal y equals collision_normal", which);
  check(a.ny == n.n.y.raw, what, n.n.y.raw, a.ny);
  std::snprintf(what, sizeof what, "%s normal z equals collision_normal", which);
  check(a.nz == n.n.z.raw, what, n.n.z.raw, a.nz);
  // +y UP, which is what the emit order buys: a heightfield face's y lane is
  // D*D and never zero, so a unit normal of it is strictly positive in y.
  check(a.ny > 0, "the collision normal points up", 1, a.ny > 0 ? 1 : 0);
}

// THE CELL the answer came from -- the fields PART.TERRAIN_TAP caches. They
// are held until the next request is captured, so one step after
// `rsp_valid_o` they are still the answer's cell.
void check_cell(const Dut& d, const Cache& c, int32_t x, int32_t z, int surface, int sh) {
  const int ci = x >> sh, cj = z >> sh;
  check(static_cast<int32_t>(d.rsp_h00_o) == c.h(ci, cj, surface), "cell h00",
        c.h(ci, cj, surface), static_cast<int32_t>(d.rsp_h00_o));
  check(static_cast<int32_t>(d.rsp_h10_o) == c.h(ci + 1, cj, surface), "cell h10",
        c.h(ci + 1, cj, surface), static_cast<int32_t>(d.rsp_h10_o));
  check(static_cast<int32_t>(d.rsp_h01_o) == c.h(ci, cj + 1, surface), "cell h01",
        c.h(ci, cj + 1, surface), static_cast<int32_t>(d.rsp_h01_o));
  check(static_cast<int32_t>(d.rsp_h11_o) == c.h(ci + 1, cj + 1, surface), "cell h11",
        c.h(ci + 1, cj + 1, surface), static_cast<int32_t>(d.rsp_h11_o));
  check(static_cast<int32_t>(d.rsp_wx00_o) == c.cx(ci), "cell wx00", c.cx(ci),
        static_cast<int32_t>(d.rsp_wx00_o));
  check(static_cast<int32_t>(d.rsp_wz00_o) == c.cz(cj), "cell wz00", c.cz(cj),
        static_cast<int32_t>(d.rsp_wz00_o));
  check(static_cast<int>(d.rsp_sh_o) == sh, "cell shift", sh,
        static_cast<long long>(d.rsp_sh_o));
}

// ---------------------------------------------------------------------------
// 1. THE DIFFERENTIAL. The whole point of the block.
// ---------------------------------------------------------------------------
void test_matches_the_reference_on_random_interior_points() {
  // Every pitch in the frozen set, spec/terrain_rules.md 1.3.
  const int pitches[4] = {-1, 0, 1, 2};
  for (int pi = 0; pi < 4; ++pi) {
    const int pl = pitches[pi];
    const int sh = 16 + pl;
    Dut d;
    Cache c;
    c.place(0, 0, pl);
    fill_relief(c, 0x1234567u + static_cast<uint32_t>(pi) * 7919u);
    const zref::terrain::ComposedLattice ref = c.as_reference(true);
    reset(d, pl);

    uint32_t s = 0xC0FFEEu + static_cast<uint32_t>(pi);
    for (int n = 0; n < 24; ++n) {
      // strictly interior: cell 0..31, fraction 0..(D-1)
      const int32_t D = static_cast<int32_t>(1) << sh;
      const int ci = static_cast<int>(rnd(s) % 32u);
      const int cj = static_cast<int>(rnd(s) % 32u);
      const int32_t fx = static_cast<int32_t>(rnd(s) % static_cast<uint32_t>(D));
      const int32_t fz = static_cast<int32_t>(rnd(s) % static_cast<uint32_t>(D));
      const int32_t x = (static_cast<int32_t>(ci) << sh) + fx;
      const int32_t z = (static_cast<int32_t>(cj) << sh) + fz;

      const zref::terrain::ColumnResult want =
          zref::terrain::column_query(ref, zref::fx16{x}, zref::fx16{z});
      check(want.cls == zref::terrain::ColumnClass::kSolid,
            "oracle should report solid on an interior point", 2,
            static_cast<long long>(want.cls));

      const Answer top = tap(d, c, x, z, 0);
      check(top.valid, "tap answered (top)", 1, top.valid ? 1 : 0);
      check(!top.no_ground, "solid cell is not no_ground", 0, top.no_ground ? 1 : 0);
      check(top.height == want.top.raw, "top height equals zref::terrain::column_query",
            want.top.raw, top.height);
      check_normal(ref, x, z, false, top, "top");
      check_cell(d, c, x, z, 0, sh);

      const Answer bot = tap(d, c, x, z, 1);
      check(bot.height == want.bottom.raw, "bottom height equals the reference",
            want.bottom.raw, bot.height);
      check_normal(ref, x, z, true, bot, "bottom");
    }
    check(d.taps_answered_o == 48, "every tap counted as answered", 48,
          static_cast<long long>(d.taps_answered_o));
    check(d.place_mismatch_o == 0, "no placement fault on a correct lattice", 0,
          static_cast<long long>(d.place_mismatch_o));
    check(d.interp_overflow_o == 0, "no overflow on ordinary relief", 0,
          static_cast<long long>(d.interp_overflow_o));
  }
}

// ---------------------------------------------------------------------------
// 2. FLOOR PUTS THE SHARED EDGE IN THE HIGHER PATCH, and that is a decision.
// ---------------------------------------------------------------------------
void test_the_far_edge_belongs_to_the_next_patch() {
  const int pl = 1, sh = 17;
  Dut d;
  Cache c;
  c.place(0, 0, pl);
  fill_relief(c, 0xABCDEFu);
  reset(d, pl);

  // x exactly on the patch's far edge is patch 1's origin under floor
  // (zref_island_scene.hpp:119-125), so with only patch 0 staged it has no
  // ground -- and the block discovers that from the STORED placement, never
  // from being told which patch is served.
  const Answer a = tap(d, c, static_cast<int32_t>(32) << sh, 1 << sh, 0);
  check(a.valid, "the far edge still answers", 1, a.valid ? 1 : 0);
  check(a.no_ground, "the far edge is off this patch", 1, a.no_ground ? 1 : 0);
  check(d.taps_off_patch_o == 1, "counted as off-patch, not as a fault", 1,
        static_cast<long long>(d.taps_off_patch_o));

  // A point squarely inside a DIFFERENT patch, same story, by a wide margin.
  const Answer b = tap(d, c, static_cast<int32_t>(70) << sh, 3 << sh, 0);
  check(b.no_ground, "a point in another patch has no ground here", 1, b.no_ground ? 1 : 0);
  check(d.taps_off_patch_o == 2, "off-patch counted again", 2,
        static_cast<long long>(d.taps_off_patch_o));
  check(d.place_mismatch_o == 0, "off-patch is not a placement fault", 0,
        static_cast<long long>(d.place_mismatch_o));
}

// ---------------------------------------------------------------------------
// 3. NO PATCH STAGED AT ALL -- the cache poisons, and that is no ground.
// ---------------------------------------------------------------------------
void test_poison_is_no_ground_and_not_a_fault() {
  const int pl = 1;
  Dut d;
  Cache c;
  c.place(0, 0, pl);
  fill_relief(c, 0x5EED01u);
  c.served = false;  // serve_valid_q low: everything comes back 0x5BADF00D
  reset(d, pl);

  const Answer a = tap(d, c, 3 << 17, 5 << 17, 0);
  check(a.valid, "a tap with nothing staged still answers", 1, a.valid ? 1 : 0);
  check(a.no_ground, "nothing staged means no ground", 1, a.no_ground ? 1 : 0);
  check(d.taps_off_patch_o == 1, "poison is an off-patch refusal", 1,
        static_cast<long long>(d.taps_off_patch_o));
  check(d.place_mismatch_o == 0, "poison is not blamed on the pitch", 0,
        static_cast<long long>(d.place_mismatch_o));
}

// ---------------------------------------------------------------------------
// 4. A VOID CELL. A correct refusal, counted apart from the faults.
// ---------------------------------------------------------------------------
void test_a_void_cell_answers_no_ground() {
  const int pl = 1, sh = 17;
  Dut d;
  Cache c;
  c.place(0, 0, pl);
  fill_relief(c, 0x0B0B0Bu);
  c.sub[static_cast<size_t>(4) * CELLS + 6] = 1;  // cell (6,4) is not SOLID
  reset(d, pl);

  const Answer a = tap(d, c, (6 << sh) + 900, (4 << sh) + 900, 0);
  check(a.no_ground, "a void cell has no ground", 1, a.no_ground ? 1 : 0);
  check(d.taps_void_o == 1, "counted as void", 1, static_cast<long long>(d.taps_void_o));
  check(d.taps_answered_o == 0, "a void tap is not an answer", 0,
        static_cast<long long>(d.taps_answered_o));

  const Answer b = tap(d, c, (7 << sh) + 900, (4 << sh) + 900, 0);
  check(!b.no_ground, "the neighbouring cell is still solid", 0, b.no_ground ? 1 : 0);
  check(d.taps_void_o == 1, "void did not over-count", 1,
        static_cast<long long>(d.taps_void_o));
}

// ---------------------------------------------------------------------------
// 5. THE PLACEMENT FAULT. The lattice does not hold what the pitch says.
// ---------------------------------------------------------------------------
void test_a_lattice_that_disagrees_with_the_pitch_is_a_fault() {
  const int pl = 1, sh = 17;
  Dut d;
  Cache c;
  c.place(0, 0, 0);  // placed at pitch_log2 = 0 ...
  fill_relief(c, 0xFEED77u);
  reset(d, pl);      // ... while the island says 1. D is wrong by 2x.

  const Answer a = tap(d, c, (3 << sh) + 77, (2 << sh) + 77, 0);
  check(a.no_ground, "a disagreeing lattice cannot answer", 1, a.no_ground ? 1 : 0);
  check(d.place_mismatch_o == 1, "counted as a PLACEMENT FAULT", 1,
        static_cast<long long>(d.place_mismatch_o));
  check(d.taps_off_patch_o == 0, "a fault does not hide in the correct-refusal bucket", 0,
        static_cast<long long>(d.taps_off_patch_o));
}

// ---------------------------------------------------------------------------
// 6. A PITCH OUTSIDE THE FROZEN SET.
// ---------------------------------------------------------------------------
void test_a_pitch_outside_the_frozen_set_is_a_fault() {
  Dut d;
  Cache c;
  c.place(0, 0, 1);
  fill_relief(c, 0x31415u);
  reset(d, 3);  // spec/terrain_rules.md 1.3 allows -1..+2 only

  const Answer a = tap(d, c, 1 << 17, 1 << 17, 0);
  check(a.no_ground, "an illegal pitch cannot answer", 1, a.no_ground ? 1 : 0);
  check(d.pitch_bad_o == 1, "counted as a PITCH FAULT", 1,
        static_cast<long long>(d.pitch_bad_o));
  check(d.taps_answered_o == 0, "and nothing was answered", 0,
        static_cast<long long>(d.taps_answered_o));
}

// ---------------------------------------------------------------------------
// 7. THE RAILS. The interpolation is a CONVEX COMBINATION, so it cannot leave
//    the interval its corners span -- and the overflow guard therefore CANNOT
//    FIRE on legal stimulus.
//
//    This test asserts the CORRECT behaviour (the answer stays between the
//    corners and the fault counter stays at zero), not the fault. Asserting the
//    fault here would be a test that passes only while the bug exists. The
//    guard's positive control is separate and inverted:
//    tests/mutants/zhao_terrain_heighttap_mutant.sv.
//
//    The first version of this test DID try to fire it, with corners at both
//    s32 rails, and failed to -- which is what sent somebody to do the algebra
//    in the RTL header instead of tuning the stimulus until it went red.
// ---------------------------------------------------------------------------
void test_extreme_corners_stay_between_their_rails() {
  const int pl = 2, sh = 18;  // the widest cell, so un/vn are largest
  Dut d;
  Cache c;
  c.place(0, 0, pl);
  for (int j = 0; j < LAT; ++j)
    for (int i = 0; i < LAT; ++i) {
      const size_t k = static_cast<size_t>(j) * LAT + static_cast<size_t>(i);
      c.top[k] = ((i + j) & 1) ? 0x7FFFFFFF : static_cast<int32_t>(0x80000000);
      c.bottom[k] = c.top[k];
    }
  const zref::terrain::ComposedLattice ref = c.as_reference(true);
  reset(d, pl);

  uint32_t s = 0xBADCAFEu;
  for (int n = 0; n < 12; ++n) {
    const int32_t D = static_cast<int32_t>(1) << sh;
    const int ci = static_cast<int>(rnd(s) % 32u);
    const int cj = static_cast<int>(rnd(s) % 32u);
    const int32_t x = (static_cast<int32_t>(ci) << sh) +
                      static_cast<int32_t>(rnd(s) % static_cast<uint32_t>(D));
    const int32_t z = (static_cast<int32_t>(cj) << sh) +
                      static_cast<int32_t>(rnd(s) % static_cast<uint32_t>(D));
    const zref::terrain::ColumnResult want =
        zref::terrain::column_query(ref, zref::fx16{x}, zref::fx16{z});
    const Answer a = tap(d, c, x, z, 0);
    check(!a.no_ground, "a rail-to-rail lattice still has ground", 0, a.no_ground ? 1 : 0);
    check(a.height == want.top.raw, "and it is still the reference's answer", want.top.raw,
          a.height);
    // face_normal's rescale SATURATES on a rail-to-rail lattice, in the
    // reference exactly as here, so the normal is still the reference's.
    check_normal(ref, x, z, false, a, "rails");
  }
  // THE NORMAL-SATURATION CENSUS, fired with legal stimulus: a dh of ~2^32 at
  // SH = 18 is ~2^34 after the rescale, far past s32.
  check(d.normal_sats_o == 12, "every rail-to-rail answer counted a normal saturation", 12,
        static_cast<long long>(d.normal_sats_o));
  check(d.interp_overflow_o == 0, "the overflow guard stays silent on legal stimulus", 0,
        static_cast<long long>(d.interp_overflow_o));
}

// ---------------------------------------------------------------------------
// 8. THE OWNER IS NEVER DELAYED. The load-bearing property of the pass-through.
// ---------------------------------------------------------------------------
void test_the_owner_is_never_delayed_and_the_tap_stalls_instead() {
  const int pl = 1, sh = 17;
  Dut d;
  Cache c;
  c.place(0, 0, pl);
  fill_relief(c, 0x777333u);
  reset(d, pl);

  // TERRAIN.TESS asks on every single cycle for a while. The tap must get no
  // cycle at all, must not corrupt the owner's address, and must not answer.
  d.req_x_i = static_cast<uint32_t>((3 << sh) + 500);
  d.req_z_i = static_cast<uint32_t>((2 << sh) + 500);
  d.req_surface_i = 0;
  d.req_valid_i = 1;
  d.o_lat_req_i = 1;
  d.o_cs_req_i = 1;
  bool accepted = false;
  for (int n = 0; n < 40; ++n) {
    d.o_lat_vi_i = static_cast<uint8_t>(n % 33);
    d.o_lat_vj_i = static_cast<uint8_t>((n * 3) % 33);
    d.o_lat_surface_i = static_cast<uint8_t>(n & 1);
    d.o_cs_ci_i = static_cast<uint8_t>(n % 32);
    d.o_cs_cj_i = static_cast<uint8_t>((n * 5) % 32);
    d.eval();
    if (d.req_valid_i && d.req_ready_o) accepted = true;
    // the owner's request reaches the cache unchanged, every cycle
    check(d.c_lat_req_o == 1, "owner request is never gated", 1,
          static_cast<long long>(d.c_lat_req_o));
    check(d.c_lat_vi_o == d.o_lat_vi_i, "owner vi passes through", d.o_lat_vi_i,
          static_cast<long long>(d.c_lat_vi_o));
    check(d.c_lat_vj_o == d.o_lat_vj_i, "owner vj passes through", d.o_lat_vj_i,
          static_cast<long long>(d.c_lat_vj_o));
    check(d.c_lat_surface_o == d.o_lat_surface_i, "owner surface passes through",
          d.o_lat_surface_i, static_cast<long long>(d.c_lat_surface_o));
    check(d.c_cs_ci_o == d.o_cs_ci_i, "owner ci passes through", d.o_cs_ci_i,
          static_cast<long long>(d.c_cs_ci_o));
    check(d.c_cs_cj_o == d.o_cs_cj_i, "owner cj passes through", d.o_cs_cj_i,
          static_cast<long long>(d.c_cs_cj_o));
    step(d, c);
    if (accepted) d.req_valid_i = 0;
  }
  check(accepted, "the tap request was accepted", 1, accepted ? 1 : 0);
  check(d.rsp_valid_o == 0, "the tap did not answer while starved", 0,
        static_cast<long long>(d.rsp_valid_o));
  check(d.tap_stall_clocks_o > 0, "the tap counted the cycles it lost", 1,
        static_cast<long long>(d.tap_stall_clocks_o > 0));

  // Let it go: the same tap now completes, and the answer is the reference's.
  d.o_lat_req_i = 0;
  d.o_cs_req_i = 0;
  int waited = 0;
  while (d.rsp_valid_o == 0 && waited++ < 400) step(d, c);
  check(d.rsp_valid_o != 0, "the starved tap completes once the owner is quiet", 1,
        static_cast<long long>(d.rsp_valid_o));
  const zref::terrain::ComposedLattice ref = c.as_reference(true);
  const zref::terrain::ColumnResult want = zref::terrain::column_query(
      ref, zref::fx16{(3 << sh) + 500}, zref::fx16{(2 << sh) + 500});
  check(static_cast<int32_t>(d.rsp_height_o) == want.top.raw,
        "a starved tap answers the SAME value as an unstarved one", want.top.raw,
        static_cast<long long>(static_cast<int32_t>(d.rsp_height_o)));
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  test_matches_the_reference_on_random_interior_points();
  test_the_far_edge_belongs_to_the_next_patch();
  test_poison_is_no_ground_and_not_a_fault();
  test_a_void_cell_answers_no_ground();
  test_a_lattice_that_disagrees_with_the_pitch_is_a_fault();
  test_a_pitch_outside_the_frozen_set_is_a_fault();
  test_extreme_corners_stay_between_their_rails();
  test_the_owner_is_never_delayed_and_the_tap_stalls_instead();

  std::printf("terrain_heighttap_directed: %d checks, %d failed\n", g_checks, g_failed);
  std::fflush(stdout);
  // The harness's documented teardown deadlock: a plain return hangs at ~0 CPU.
  zhao::exit_hard(g_failed == 0 ? 0 : 1);
}
