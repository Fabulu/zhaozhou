// terrain_velocity_directed.cpp — TERRAIN.VELOCITY against its oracle, and the
// oracle against the ratified lane it claims to be a view of.
//
// THREE LAYERS, IN THIS ORDER, BECAUSE THE MIDDLE ONE IS THE ONE THAT USUALLY
// GETS SKIPPED:
//
//   1. `zref::terrain::velocity_vertex` reproduces the velocity samples
//      `zref::render::compose_lattice` ACTUALLY RECORDS, over a real 33x33
//      patch with two real earth programs on overlapping footprints, at every
//      one of the 1,089 vertices. compose_lattice pushes one sample per
//      (application, covered vertex) and never reduces them; terrain_rules
//      §4.4 says the lattice is those samples "accumulated" and stops. Layer 1
//      is where the chosen reduction (contract V1) is held against the shipped
//      renderer instead of merely asserted — without it the oracle would be a
//      second implementation of the velocity lane and charter §29-6 would be
//      broken before the RTL was written.
//   2. The RTL against the oracle: the fx_add chain, the closed-interval
//      gate's answer, the single bake-back, both saturations, both counters.
//   3. The RTL against the laws that are NOT arithmetic: the z-then-x sweep is
//      complete and in order, the §4.2 lattice word exists for every vertex
//      including uncovered ones (V2), the moving mask is the §11.1 subpatch
//      rule, backpressure on either side changes nothing but timing, and the
//      measured rate is PRINTED rather than derived.
//
// Every exact-equality boundary below is CONSTRUCTED. Uniform random input
// never lands on `acc + 128 == 0 (mod 256)` at a chosen value, never lands on
// the last accumulator that does not saturate the height16 rail, and never
// lands on INT32_MAX exactly — four increments running have shipped coverage
// counters reading zero while every differential passed.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_velocity.h"

#include "render_helpers.hpp"  // rtest::make_earth_prog, bump_patch, xform_identity
#include "velocity_dev.hpp"
#include "zfield/zfield.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_terrain_patch.hpp"
#include "zref/zref_terrain_velocity.hpp"
#include "zrender/internal.hpp"  // white-box: compose_lattice, FieldApp

using zhao::check;
namespace zt = zref::terrain;
namespace zr = zref::render;

namespace {

// ---------------------------------------------------------------------------
// The bake-back boundary constants, derived once here and asserted against the
// oracle so a typo cannot make the whole file agree with itself.
//
//   rescale(x, 8) = (x + 128) >> 8  (arithmetic; round-half-up, ties toward
//   +infinity), then saturate to s16.
// ---------------------------------------------------------------------------
constexpr int32_t kTiePos = 128;            // -> 1   (the +0.5 tie rounds UP)
constexpr int32_t kTieNeg = -128;           // -> 0   (the -0.5 tie ALSO rounds up)
constexpr int32_t kTieNegBelow = -129;      // -> -1  (one LSB below the tie)
constexpr int32_t kRailHiLast = 8388479;    // -> 32767, the last value that fits
constexpr int32_t kRailHiFirst = 8388480;   // -> 32768 -> SATURATES
constexpr int32_t kRailLoLast = -8388736;   // -> -32768, the last value that fits
constexpr int32_t kRailLoFirst = -8388737;  // -> -32769 -> SATURATES

struct Boundary {
  int32_t acc;
  int16_t expect;
  bool sat;
  const char* what;
};

const Boundary kBoundaries[] = {
    {0, 0, false, "zero accumulator -> the V2 not-moving word"},
    {kTieNeg, 0, false, "the -0.5 tie rounds toward +infinity, to 0"},
    {kTieNegBelow, -1, false, "one LSB below the -0.5 tie rounds to -1"},
    {127, 0, false, "one LSB below the +0.5 tie rounds to 0"},
    {kTiePos, 1, false, "the +0.5 tie rounds up, to 1"},
    {255, 1, false, "just below the next tie"},
    {256, 1, false, "exactly one height16 LSB"},
    {383, 1, false, "one LSB below the 1.5 tie"},
    {384, 2, false, "the 1.5 tie rounds up"},
    {kRailHiLast, 32767, false, "the LAST accumulator that reaches the rail without saturating"},
    {kRailHiFirst, 32767, true, "the FIRST accumulator that saturates the high rail"},
    {kRailLoLast, -32768, false, "the LAST accumulator that reaches the low rail cleanly"},
    {kRailLoFirst, -32768, true, "the FIRST accumulator that saturates the low rail"},
    {INT32_MAX, 32767, true, "the fx16 ceiling saturates the height16 rail"},
    {INT32_MIN, -32768, true, "the fx16 floor saturates the height16 rail"},
};

/** Build a lane plane whose single lane carries `v` at every covered vertex. */
vdev::LanePlane uniform_plane(int lanes, int32_t v, bool covered) {
  vdev::LanePlane p;
  p.resize(lanes);
  for (int j = 0; j < vdev::kLat; ++j)
    for (int i = 0; i < vdev::kLat; ++i)
      for (int k = 0; k < lanes; ++k) {
        p.velocity[p.at(i, j, k)] = v;
        p.covers[p.at(i, j, k)] = covered ? 1 : 0;
      }
  return p;
}

/** RTL vs oracle over one whole sweep, every vertex, every side channel. */
void compare_sweep(const vdev::SweepOut& rtl, const vdev::SweepOut& ora, const char* tag) {
  int vel_bad = 0, mov_bad = 0, cov_bad = 0;
  int first_bad = -1;
  for (int k = 0; k < vdev::kVerts; ++k) {
    if (rtl.velocity[static_cast<size_t>(k)] != ora.velocity[static_cast<size_t>(k)]) {
      ++vel_bad;
      if (first_bad < 0) first_bad = k;
    }
    if (rtl.moving[static_cast<size_t>(k)] != ora.moving[static_cast<size_t>(k)]) ++mov_bad;
    if (rtl.covered[static_cast<size_t>(k)] != ora.covered[static_cast<size_t>(k)]) ++cov_bad;
  }
  if (vel_bad != 0) {
    std::printf("[terrain_velocity] %s: first mismatch at vertex %d, rtl %d oracle %d\n", tag,
                first_bad, rtl.velocity[static_cast<size_t>(first_bad)],
                ora.velocity[static_cast<size_t>(first_bad)]);
  }
  check(vel_bad == 0, "every §4.2 lattice word matches the oracle", 0,
        static_cast<uint64_t>(vel_bad));
  check(mov_bad == 0, "every moving bit matches the oracle", 0, static_cast<uint64_t>(mov_bad));
  check(cov_bad == 0, "every covered bit matches the oracle", 0, static_cast<uint64_t>(cov_bad));
  check(rtl.moving_mask == ora.moving_mask, "the 4x4 moving mask matches the §11.1 subpatch rule",
        ora.moving_mask, rtl.moving_mask);
}

// ---------------------------------------------------------------------------
// 1. THE ORACLE AGAINST THE EXECUTED REFERENCE
// ---------------------------------------------------------------------------
void test_oracle_is_the_recorded_lane() {
  std::printf("--- 1. the oracle reproduces compose_lattice's recorded velocity lane\n");
  constexpr int W = 33;

  zr::TerrainPatch patch;
  patch.width = patch.height = W;
  patch.env_x0 = patch.env_z0 = -(32 << 16);
  patch.env_x1 = patch.env_z1 = (32 << 16);
  patch.heights.resize(static_cast<size_t>(W) * W);
  patch.bottom.assign(static_cast<size_t>(W) * W, static_cast<int16_t>(-2048));
  for (int j = 0; j < W; ++j)
    for (int i = 0; i < W; ++i)
      patch.heights[static_cast<size_t>(j) * W + static_cast<size_t>(i)] =
          static_cast<int16_t>(1280 + ((i * 7) % 11) * 32);

  const std::vector<uint8_t> prog_bytes = rtest::make_earth_prog();
  const zfield::DecodeResult dec = zfield::decode(prog_bytes.data(), prog_bytes.size());
  check(dec.error == zfield::DecodeError::kOk, "the earth fixture decodes", 0,
        static_cast<uint64_t>(dec.error));
  if (dec.error != zfield::DecodeError::kOk) return;

  // TWO applications on OVERLAPPING footprints, so the sweep sees vertices
  // covered by NEITHER, by ONE, and by BOTH — the three cases the reduction has
  // to get right, and the only configuration in which "accumulated" is
  // observable at all.
  // fp0 and fp1 OVERLAP in [-4, +4] and together leave (+20, +32] uncovered,
  // so all three classes exist by construction rather than by luck.
  const int32_t fp[2][4] = {{-(32 << 16), -(32 << 16), (4 << 16), (32 << 16)},
                            {-(4 << 16), -(32 << 16), (20 << 16), (32 << 16)}};
  std::vector<zr::FieldApp> apps(2);
  for (int a = 0; a < 2; ++a) {
    apps[static_cast<size_t>(a)].prog = &dec.prog;
    std::memset(&apps[static_cast<size_t>(a)].cmd, 0, sizeof(apps[static_cast<size_t>(a)].cmd));
    apps[static_cast<size_t>(a)].cmd.program = static_cast<uint32_t>(a + 1);
    apps[static_cast<size_t>(a)].cmd.footprint.x0 = fp[a][0];
    apps[static_cast<size_t>(a)].cmd.footprint.y0 = fp[a][1];
    apps[static_cast<size_t>(a)].cmd.footprint.x1 = fp[a][2];
    apps[static_cast<size_t>(a)].cmd.footprint.y1 = fp[a][3];
    apps[static_cast<size_t>(a)].cmd.start_tick = 0;
    apps[static_cast<size_t>(a)].cmd.duration_ticks = 100;
  }

  zref::SatLedger L_ref;
  std::vector<zr::TerrainVelocitySample> samples;
  const zt::ComposedLattice lat =
      zr::compose_lattice(patch, rtest::xform_identity(), apps, 50, &samples, &L_ref);
  check(lat.w == W && lat.h == W, "the reference composed a 33x33 lattice", W,
        static_cast<uint64_t>(lat.w));
  check(!samples.empty(), "the reference recorded velocity samples", 1, samples.empty() ? 0 : 1);

  // Replay the recording order rather than matching on coordinates: the
  // reference walks applications OUTER and vertices INNER (z-then-x) and pushes
  // one sample per COVERED vertex, so consuming the stream in that order both
  // groups it by vertex AND proves the order is what it is documented to be.
  zt::FieldList list;
  for (int a = 0; a < 2; ++a) {
    zt::FieldRecord r;
    r.x0 = fp[a][0];
    r.z0 = fp[a][1];
    r.x1 = fp[a][2];
    r.z1 = fp[a][3];
    r.cmd_index = static_cast<uint16_t>(a);
    check(list.offer(r, 1), "the intake accepts the record", 1, 1);
  }
  std::vector<int32_t> recorded_lane(static_cast<size_t>(W) * W * 2, 0);
  std::vector<uint8_t> recorded_hit(static_cast<size_t>(W) * W * 2, 0);
  size_t cursor = 0;
  int order_bad = 0;
  for (int a = 0; a < 2; ++a) {
    for (int j = 0; j < W; ++j) {
      for (int i = 0; i < W; ++i) {
        const int32_t wx = lat.wx[static_cast<size_t>(i)];
        const int32_t wz = lat.wz[static_cast<size_t>(j)];
        if (!zt::covers(list[a], wx, wz)) continue;
        if (cursor >= samples.size()) {
          ++order_bad;
          continue;
        }
        const zr::TerrainVelocitySample& s = samples[cursor++];
        if (s.world_x != wx || s.world_z != wz) ++order_bad;
        const size_t slot =
            (static_cast<size_t>(j) * W + static_cast<size_t>(i)) * 2 + static_cast<size_t>(a);
        recorded_lane[slot] = s.velocity;
        recorded_hit[slot] = 1;
      }
    }
  }
  check(order_bad == 0, "the recorded stream is app-major, vertex z-then-x, covered only", 0,
        static_cast<uint64_t>(order_bad));
  check(cursor == samples.size(), "and it holds exactly the samples the footprints imply",
        samples.size(), cursor);

  int disagree = 0, both = 0, one = 0, none = 0, cov_bad = 0;
  for (int j = 0; j < W; ++j) {
    for (int i = 0; i < W; ++i) {
      const int32_t wx = lat.wx[static_cast<size_t>(i)];
      const int32_t wz = lat.wz[static_cast<size_t>(j)];
      const size_t base = (static_cast<size_t>(j) * W + static_cast<size_t>(i)) * 2;
      int32_t lane[2] = {recorded_lane[base], recorded_lane[base + 1]};
      bool cov[2] = {recorded_hit[base] != 0, recorded_hit[base + 1] != 0};
      // The oracle's own gate must be the SAME closed-interval test that
      // decided whether the reference recorded anything here.
      for (int a = 0; a < 2; ++a)
        if (cov[a] != zt::covers(list[a], wx, wz)) ++cov_bad;
      const zt::VelocityOut v = zt::velocity_vertex(lane, cov, 2, nullptr);

      // The reduction, spelled out against the recorded stream: the saturating
      // fx_add chain in command order, then ONE bake-back.
      int32_t acc = 0;
      for (int a = 0; a < 2; ++a)
        if (cov[a]) acc = zref::fx_add(zref::fx16{acc}, zref::fx16{lane[a]}, nullptr).raw;
      const int16_t want = zref::height16_from_fx16(zref::fx16{acc}, nullptr).raw;
      if (v.velocity != want) {
        ++disagree;
        if (disagree == 1)
          std::printf("[terrain_velocity] oracle %d vs recorded-sum %d at (%d,%d)\n", v.velocity,
                      want, i, j);
      }
      const int n_cov = (cov[0] ? 1 : 0) + (cov[1] ? 1 : 0);
      if (n_cov == 2) ++both;
      if (n_cov == 1) ++one;
      if (n_cov == 0) ++none;
    }
  }
  check(cov_bad == 0, "the recorded stream's coverage IS the closed-interval test", 0,
        static_cast<uint64_t>(cov_bad));
  check(disagree == 0, "the oracle reproduces the recorded velocity lane at every vertex", 0,
        static_cast<uint64_t>(disagree));
  // The three coverage classes are REACHED, not hoped for — otherwise the
  // cross-check proves only that two functions agree on the empty case.
  check(both > 0, "COVERAGE: vertices covered by BOTH applications exist", 1, both > 0 ? 1 : 0);
  check(one > 0, "COVERAGE: vertices covered by exactly one exist", 1, one > 0 ? 1 : 0);
  check(none > 0, "COVERAGE: vertices covered by neither exist", 1, none > 0 ? 1 : 0);
  std::printf("[terrain_velocity] coverage classes: both %d, one %d, none %d (samples %zu)\n", both,
              one, none, samples.size());

  // And the RTL runs the SAME plane the reference just produced. This is the
  // only place in the suite where the lane words are the shipped renderer's
  // own rather than the test's.
  vdev::LanePlane plane;
  plane.resize(2);
  for (int j = 0; j < W; ++j)
    for (int i = 0; i < W; ++i)
      for (int a = 0; a < 2; ++a) {
        const size_t base = (static_cast<size_t>(j) * W + static_cast<size_t>(i)) * 2;
        plane.velocity[plane.at(i, j, a)] = recorded_lane[base + static_cast<size_t>(a)];
        plane.covers[plane.at(i, j, a)] = recorded_hit[base + static_cast<size_t>(a)];
      }
  Vzhao_terrain_velocity dut;
  vdev::reset_dut(dut);
  const vdev::SweepOut rtl = vdev::run_sweep(dut, plane, 0x100, 0x11);
  zref::SatLedger Lo;
  const vdev::SweepOut ora = vdev::oracle_sweep(plane, 0x11, &Lo);
  compare_sweep(rtl, ora, "the reference's own lane plane");
}

// ---------------------------------------------------------------------------
// 2. THE CONSTRUCTED BAKE-BACK BOUNDARIES
// ---------------------------------------------------------------------------
void test_bakeback_boundaries(Vzhao_terrain_velocity& dut) {
  std::printf("--- 2. the constructed rescale/rail boundaries\n");

  // First: the boundary table agrees with the ORACLE, so the table is not a
  // private copy of the RTL's opinion.
  for (const Boundary& b : kBoundaries) {
    zref::SatLedger L{};
    const int16_t got = zref::height16_from_fx16(zref::fx16{b.acc}, &L).raw;
    check(got == b.expect, b.what, static_cast<uint64_t>(static_cast<uint16_t>(b.expect)),
          static_cast<uint64_t>(static_cast<uint16_t>(got)));
    check((L.rescale != 0) == b.sat, "the oracle's rescale saturation matches the boundary",
          b.sat ? 1 : 0, L.rescale != 0 ? 1 : 0);
  }

  // Then the RTL, one boundary per lattice row so a single sweep covers them
  // all with a single covering lane (acc == the lane word, exactly).
  vdev::LanePlane p;
  p.resize(1);
  const int n_b = static_cast<int>(sizeof(kBoundaries) / sizeof(kBoundaries[0]));
  for (int j = 0; j < vdev::kLat; ++j)
    for (int i = 0; i < vdev::kLat; ++i) {
      const Boundary& b = kBoundaries[(j * vdev::kLat + i) % n_b];
      p.velocity[p.at(i, j, 0)] = b.acc;
      p.covers[p.at(i, j, 0)] = 1;
    }
  const vdev::SweepOut rtl = vdev::run_sweep(dut, p, 0x0BE1, 0x0042);
  check(!rtl.timed_out, "the boundary sweep completed", 0, rtl.timed_out ? 1 : 0);
  zref::SatLedger L{};
  const vdev::SweepOut ora = vdev::oracle_sweep(p, 0x0042, &L);
  compare_sweep(rtl, ora, "boundaries");
  check(rtl.rescale_sats == L.rescale, "the RTL's rescale-saturation count is the SatLedger's",
        L.rescale, rtl.rescale_sats);
  check(rtl.add_sats == L.add, "the RTL's add-saturation count is the SatLedger's", L.add,
        rtl.add_sats);
  check(L.rescale > 0, "COVERAGE: the constructed rail saturations actually fired", 1,
        L.rescale > 0 ? 1 : 0);
  std::printf("[terrain_velocity] boundary sweep: %u rescale saturations, %u add saturations\n",
              rtl.rescale_sats, rtl.add_sats);
}

// ---------------------------------------------------------------------------
// 3. THE fx_add CHAIN AND ITS OWN SATURATION
// ---------------------------------------------------------------------------
void test_add_chain(Vzhao_terrain_velocity& dut) {
  std::printf("--- 3. the command-order fx_add chain (V1)\n");

  // (a) Two lanes that cancel EXACTLY: the sum is 0, so the vertex is covered
  //     but not moving. That distinction only exists because the reduction is
  //     an add — under a max-magnitude rule this vertex would report a speed.
  vdev::LanePlane cancel;
  cancel.resize(2);
  for (int j = 0; j < vdev::kLat; ++j)
    for (int i = 0; i < vdev::kLat; ++i) {
      const int32_t v = 1 << 16;
      cancel.velocity[cancel.at(i, j, 0)] = v;
      cancel.velocity[cancel.at(i, j, 1)] = -v;
      cancel.covers[cancel.at(i, j, 0)] = 1;
      cancel.covers[cancel.at(i, j, 1)] = 1;
    }
  vdev::SweepOut r = vdev::run_sweep(dut, cancel, 1, 7);
  zref::SatLedger Lc{};
  vdev::SweepOut o = vdev::oracle_sweep(cancel, 7, &Lc);
  compare_sweep(r, o, "exact cancellation");
  check(r.velocity[500] == 0, "two exactly opposed lanes cancel to a zero word", 0,
        static_cast<uint64_t>(static_cast<uint16_t>(r.velocity[500])));
  check(r.covered[500] == 1, "and the vertex is still COVERED", 1, r.covered[500]);
  check(r.moving[500] == 0, "and NOT moving", 0, r.moving[500]);
  check(r.moving_mask == 0, "a wholly still patch marks no subpatch", 0, r.moving_mask);

  // (b) The fx_add saturation itself, CONSTRUCTED: INT32_MAX then +1.
  vdev::LanePlane satp;
  satp.resize(2);
  for (int j = 0; j < vdev::kLat; ++j)
    for (int i = 0; i < vdev::kLat; ++i) {
      satp.velocity[satp.at(i, j, 0)] = INT32_MAX;
      satp.velocity[satp.at(i, j, 1)] = 1;
      satp.covers[satp.at(i, j, 0)] = 1;
      satp.covers[satp.at(i, j, 1)] = 1;
    }
  r = vdev::run_sweep(dut, satp, 2, 8);
  zref::SatLedger Ls{};
  o = vdev::oracle_sweep(satp, 8, &Ls);
  compare_sweep(r, o, "fx_add saturation");
  check(Ls.add == static_cast<uint32_t>(vdev::kVerts),
        "the oracle saturated the add at every vertex", vdev::kVerts, Ls.add);
  check(r.add_sats == Ls.add, "the RTL counted the same add saturations", Ls.add, r.add_sats);
  check(r.rescale_sats == Ls.rescale, "and the same rescale saturations", Ls.rescale,
        r.rescale_sats);
  check(r.velocity[0] == 32767, "a saturated accumulator bakes back to the high rail", 32767,
        static_cast<uint64_t>(static_cast<uint16_t>(r.velocity[0])));

  // (c) A lane that does NOT cover is skipped, not added as zero — value AND
  //     SatLedger identical. Constructed so the two rules differ observably:
  //     the uncovered lane carries INT32_MIN, which would saturate if added.
  vdev::LanePlane skip;
  skip.resize(2);
  for (int j = 0; j < vdev::kLat; ++j)
    for (int i = 0; i < vdev::kLat; ++i) {
      skip.velocity[skip.at(i, j, 0)] = 4 << 16;
      skip.covers[skip.at(i, j, 0)] = 1;
      skip.velocity[skip.at(i, j, 1)] = INT32_MIN;
      skip.covers[skip.at(i, j, 1)] = 0;  // MISSES
    }
  r = vdev::run_sweep(dut, skip, 3, 9);
  zref::SatLedger Lk{};
  o = vdev::oracle_sweep(skip, 9, &Lk);
  compare_sweep(r, o, "uncovered lane skipped");
  check(r.velocity[0] == 1024, "the covering lane alone decides the word (4 m -> 1024)", 1024,
        static_cast<uint64_t>(static_cast<uint16_t>(r.velocity[0])));
  check(r.add_sats == 0, "the uncovered INT32_MIN lane recorded NO add saturation", 0, r.add_sats);
  check(Lk.add == 0, "and the oracle agrees it recorded none", 0, Lk.add);
}

// ---------------------------------------------------------------------------
// 4. V2: THE ZERO-LANE PATCH, AND THE UNCOVERED VERTEX
// ---------------------------------------------------------------------------
void test_v2_zero_word(Vzhao_terrain_velocity& dut) {
  std::printf("--- 4. V2: a vertex no lane covers has velocity exactly zero\n");

  vdev::LanePlane none;
  none.resize(0);
  vdev::SweepOut r = vdev::run_sweep(dut, none, 4, 0xAB);
  check(!r.timed_out, "the zero-lane sweep completed", 0, r.timed_out ? 1 : 0);
  check(r.samples == static_cast<uint32_t>(vdev::kVerts),
        "a patch with no live field still writes all 1,089 lattice words", vdev::kVerts, r.samples);
  int nonzero = 0, cov = 0;
  for (int k = 0; k < vdev::kVerts; ++k) {
    if (r.velocity[static_cast<size_t>(k)] != 0) ++nonzero;
    if (r.covered[static_cast<size_t>(k)] != 0) ++cov;
  }
  check(nonzero == 0, "every word is exactly zero", 0, static_cast<uint64_t>(nonzero));
  check(cov == 0, "and no vertex claims coverage", 0, static_cast<uint64_t>(cov));
  check(r.moving_mask == 0, "and no subpatch is marked moving", 0, r.moving_mask);
  check(r.vtx_order.size() == static_cast<size_t>(vdev::kVerts),
        "the zero-lane sweep still visits every vertex", vdev::kVerts, r.vtx_order.size());

  // A lane list that exists but covers NOTHING: same lattice, different path.
  vdev::LanePlane miss = uniform_plane(3, 12345678, false);
  r = vdev::run_sweep(dut, miss, 5, 0xAC);
  nonzero = 0;
  for (int k = 0; k < vdev::kVerts; ++k)
    if (r.velocity[static_cast<size_t>(k)] != 0) ++nonzero;
  check(nonzero == 0, "three lanes that all miss give the same all-zero lattice", 0,
        static_cast<uint64_t>(nonzero));
  check(r.add_sats == 0, "and record nothing", 0, r.add_sats);
}

// ---------------------------------------------------------------------------
// 5. V3: THE SWEEP IS THE BLOCK'S OWN, COMPLETE, AND z-THEN-x
// ---------------------------------------------------------------------------
void test_sweep_order(Vzhao_terrain_velocity& dut) {
  std::printf("--- 5. V3: the 33x33 z-then-x sweep\n");

  vdev::LanePlane p = uniform_plane(1, 3 << 16, true);
  const vdev::SweepOut r = vdev::run_sweep(dut, p, 6, 0xC0);
  check(r.vtx_order.size() == static_cast<size_t>(vdev::kVerts),
        "the block asked for exactly 1,089 vertices", vdev::kVerts, r.vtx_order.size());
  int out_of_order = 0;
  for (size_t k = 0; k < r.vtx_order.size(); ++k)
    if (r.vtx_order[k] != static_cast<uint32_t>(k)) ++out_of_order;
  check(out_of_order == 0, "in z-then-x order — the order compose_lattice records in", 0,
        static_cast<uint64_t>(out_of_order));
  check(r.done_pulsed, "patch_done_o pulsed once the lattice was complete", 1,
        r.done_pulsed ? 1 : 0);
  check(dut.idle_o != 0, "and the block returned to idle", 1, dut.idle_o);
  check(dut.trace_patch_id_o == 6, "trace_patch_id_o names the patch under sweep", 6,
        dut.trace_patch_id_o);
  int wrong_src = 0;
  for (int k = 0; k < vdev::kVerts; ++k)
    if (r.src_ids[static_cast<size_t>(k)] != 0xC0) ++wrong_src;
  check(wrong_src == 0, "src_id rides every word", 0, static_cast<uint64_t>(wrong_src));
}

// ---------------------------------------------------------------------------
// 6. V5: THE MOVING MASK IS THE §11.1 SUBPATCH RULE
// ---------------------------------------------------------------------------
void test_moving_mask(Vzhao_terrain_velocity& dut) {
  std::printf("--- 6. V5: the 4x4 moving mask\n");

  struct Case {
    int vi, vj;
    const char* what;
  };
  const Case cases[] = {
      {4, 4, "an INTERIOR vertex marks exactly one subpatch"},
      {8, 4, "a vertex on a column border marks BOTH neighbours"},
      {8, 8, "a CORNER vertex marks four"},
      {0, 0, "the patch's own corner marks exactly one"},
      {32, 32, "the far corner marks exactly one"},
  };
  for (const Case& c : cases) {
    vdev::LanePlane p;
    p.resize(1);
    p.velocity[p.at(c.vi, c.vj, 0)] = 1 << 16;  // 1 m -> 256, comfortably non-zero
    p.covers[p.at(c.vi, c.vj, 0)] = 1;
    const vdev::SweepOut r = vdev::run_sweep(dut, p, 7, 1);
    const uint16_t want = zt::subpatch_mask(c.vi, c.vj);
    check(r.moving_mask == want, c.what, want, r.moving_mask);
  }

  // A vertex that MOVES BY LESS THAN ONE height16 LSB marks nothing: the mask
  // follows the stored word, not the accumulator, because the stored word is
  // what a consumer will read.
  vdev::LanePlane tiny;
  tiny.resize(1);
  tiny.velocity[tiny.at(16, 16, 0)] = 127;  // rounds to 0
  tiny.covers[tiny.at(16, 16, 0)] = 1;
  const vdev::SweepOut r = vdev::run_sweep(dut, tiny, 8, 1);
  check(r.moving_mask == 0, "sub-LSB motion rounds away and marks no subpatch", 0, r.moving_mask);
  check(r.covered[16 * vdev::kLat + 16] == 1, "though the vertex is still covered", 1,
        r.covered[16 * vdev::kLat + 16]);
  tiny.velocity[tiny.at(16, 16, 0)] = 128;  // the tie: rounds UP to 1
  const vdev::SweepOut r2 = vdev::run_sweep(dut, tiny, 9, 1);
  check(r2.moving_mask == zt::subpatch_mask(16, 16),
        "one LSB more — the +0.5 tie — and the subpatch IS marked", zt::subpatch_mask(16, 16),
        r2.moving_mask);
}

// ---------------------------------------------------------------------------
// 7. BACKPRESSURE ON EITHER SIDE CHANGES TIMING, NEVER VALUES
// ---------------------------------------------------------------------------
void test_backpressure(Vzhao_terrain_velocity& dut) {
  std::printf("--- 7. backpressure\n");

  vdev::LanePlane p;
  p.resize(4);
  vdev::Rng rng(0x5EEDULL);
  for (int j = 0; j < vdev::kLat; ++j)
    for (int i = 0; i < vdev::kLat; ++i)
      for (int k = 0; k < 4; ++k) {
        p.velocity[p.at(i, j, k)] = rng.range(-(1 << 20), 1 << 20);
        p.covers[p.at(i, j, k)] = rng.chance(3) ? 0 : 1;
      }
  zref::SatLedger L{};
  const vdev::SweepOut ora = vdev::oracle_sweep(p, 0x33, &L);

  const vdev::SweepOut free_run = vdev::run_sweep(dut, p, 10, 0x33, 0, 0);
  compare_sweep(free_run, ora, "free running");
  const vdev::SweepOut lane_stall = vdev::run_sweep(dut, p, 11, 0x33, 3, 0);
  compare_sweep(lane_stall, ora, "lane stalled");
  const vdev::SweepOut sink_stall = vdev::run_sweep(dut, p, 12, 0x33, 0, 4);
  compare_sweep(sink_stall, ora, "sink stalled");
  const vdev::SweepOut both_stall = vdev::run_sweep(dut, p, 13, 0x33, 3, 4);
  compare_sweep(both_stall, ora, "both stalled");
  check(lane_stall.cycles > free_run.cycles, "stalling the lane really did cost cycles", 1,
        lane_stall.cycles > free_run.cycles ? 1 : 0);
  check(sink_stall.cycles > free_run.cycles, "stalling the sink really did cost cycles", 1,
        sink_stall.cycles > free_run.cycles ? 1 : 0);
  check(free_run.samples == static_cast<uint32_t>(vdev::kVerts),
        "every configuration produced 1,089 samples", vdev::kVerts, free_run.samples);
  check(both_stall.samples == static_cast<uint32_t>(vdev::kVerts),
        "including the doubly-stalled one", vdev::kVerts, both_stall.samples);
}

// ---------------------------------------------------------------------------
// 8. BACK-TO-BACK PATCHES
// ---------------------------------------------------------------------------
void test_back_to_back(Vzhao_terrain_velocity& dut) {
  std::printf("--- 8. back-to-back patches\n");

  vdev::LanePlane a;
  a.resize(1);
  a.velocity[a.at(2, 2, 0)] = 1 << 16;
  a.covers[a.at(2, 2, 0)] = 1;
  const uint32_t before = dut.terrain_samples_evaluated_o;
  const vdev::SweepOut r1 = vdev::run_sweep(dut, a, 20, 1);
  check(r1.moving_mask == zt::subpatch_mask(2, 2), "patch 1's mask", zt::subpatch_mask(2, 2),
        r1.moving_mask);

  vdev::LanePlane b;
  b.resize(1);  // nothing moves at all
  const vdev::SweepOut r2 = vdev::run_sweep(dut, b, 21, 2);
  check(r2.moving_mask == 0, "the mask CLEARS at the next patch start", 0, r2.moving_mask);
  check(dut.trace_patch_id_o == 21, "and the trace id follows", 21, dut.trace_patch_id_o);
  check(dut.terrain_samples_evaluated_o - before == 2u * vdev::kVerts,
        "the sample counter is frame-life and accumulates across patches", 2u * vdev::kVerts,
        dut.terrain_samples_evaluated_o - before);
}

// ---------------------------------------------------------------------------
// 9. THE MEASURED RATE — reported, not derived
// ---------------------------------------------------------------------------
void test_measured_rate(Vzhao_terrain_velocity& dut) {
  std::printf("--- 9. the measured sustained rate\n");

  double per[5] = {0, 0, 0, 0, 0};
  const int lane_counts[5] = {0, 1, 2, 4, 16};
  for (int c = 0; c < 5; ++c) {
    const vdev::LanePlane p = uniform_plane(lane_counts[c], 1 << 15, true);
    const vdev::SweepOut r = vdev::run_sweep(dut, p, static_cast<uint16_t>(30 + c), 1, 0, 0);
    check(!r.timed_out, "the rate sweep completed", 0, r.timed_out ? 1 : 0);
    per[c] = static_cast<double>(r.cycles) / vdev::kVerts;
  }
  std::printf(
      "[terrain_velocity] MEASURED sustained rate, sink always ready:\n"
      "  0 lanes (no live field) %.3f clocks/sample\n"
      "  1 lane  (THE WAKE)      %.3f clocks/sample\n"
      "  2 lanes                 %.3f clocks/sample\n"
      "  4 lanes                 %.3f clocks/sample\n"
      " 16 lanes (§9.1 ceiling)  %.3f clocks/sample\n",
      per[0], per[1], per[2], per[3], per[4]);

  // The ledger asks for "1 velocity sample per clock". The measurement is the
  // claim; these bounds are what "met" and "linear beyond it" mean, asserted
  // so a regression cannot quietly halve the wake rate.
  check(per[0] <= 1.02, "0 lanes meets the ledger's 1 sample/clock", 1, per[0] <= 1.02 ? 1 : 0);
  check(per[1] <= 1.02, "1 lane — a wake — SUSTAINS 1 sample/clock", 1, per[1] <= 1.02 ? 1 : 0);
  check(per[2] <= 2.02, "2 lanes costs one clock per lane", 1, per[2] <= 2.02 ? 1 : 0);
  check(per[4] <= 16.02, "16 lanes — the §9.1 ceiling — stays linear", 1, per[4] <= 16.02 ? 1 : 0);

  // A whole patch is 1,089 vertices; a wake at 1 lane therefore costs 1,089
  // clocks per patch per frame, which is the number the contract quotes.
  const vdev::LanePlane wake = uniform_plane(1, 1 << 15, true);
  const vdev::SweepOut w = vdev::run_sweep(dut, wake, 40, 1, 0, 0);
  std::printf("[terrain_velocity] one wake patch: %llu clocks for %d vertices\n",
              static_cast<unsigned long long>(w.cycles), vdev::kVerts);
  check(w.cycles <= 1110, "a whole wake patch fits in ~1,089 clocks", 1, w.cycles <= 1110 ? 1 : 0);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_velocity dut;
  vdev::reset_dut(dut);

  test_oracle_is_the_recorded_lane();
  test_bakeback_boundaries(dut);
  test_add_chain(dut);
  test_v2_zero_word(dut);
  test_sweep_order(dut);
  test_moving_mask(dut);
  test_backpressure(dut);
  test_back_to_back(dut);
  test_measured_rate(dut);

  const int rc = zhao::report_and_exit("terrain_velocity_directed");
  zhao::exit_hard(rc);
}
