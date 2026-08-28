// field_walk_earth_directed.cpp — the differential for the Earth lattice
// walker (fpga/rtl/synth/zhao_probe_walk_earth.sv), Field v3 Phase 4.
//
// WHAT IS BEING PROVED, AND AGAINST WHAT
// ---------------------------------------
// The walker's whole claim is that GENERATING the lattice points in fabric
// produces exactly the set of vertex applications `compose_lattice` produces
// by walking them in software. So the oracle here is not a hand-written
// expectation: it is the reference's own two rules, used verbatim.
//
//   * the coordinates come from `zref::terrain::lattice_lerp`, the same
//     rounded-divide the reference uses per interior line -- NOT an
//     `origin + i*pitch` accumulation, which drifts;
//   * the coverage test is spec/terrain_rules.md 9.1's CLOSED interval in
//     both axes, so a footprint-border vertex is INSIDE.
//
// The test builds the tables the way the ARM would, loads them, drives
// associations, and compares the walker's emitted (vertex, x, z, covered)
// stream against the software walk element by element.
//
// THE FOUR THINGS THAT WOULD OTHERWISE GO WRONG SILENTLY
// -------------------------------------------------------
//  1. THE BOX IS A HINT. The descriptor's covered index box exists only to
//     stop a nine-vertex field costing a full-patch walk. If the RTL ever
//     used the box INSTEAD of the per-vertex test, an oversized box would
//     mark vertices outside the footprint as covered. So the box is
//     deliberately inflated in one case and the covered set must not move.
//  2. THE GATE IS 297, NOT 273. Row-major over a 33-wide lattice cannot pack
//     four-wide groups across a row boundary, so a full patch is 9*33 = 297
//     groups, not the 273 of the aligned flat packing the accumulator's INIT
//     and DRAIN use. Budgeting at 273 under-provisions the executor by 8.8%,
//     so the number is MEASURED here rather than asserted in a comment.
//  3. ONE Z PER GROUP is only true because a group never straddles a row.
//     Every emitted group is checked to lie in a single row.
//  4. NOTHING IS DROPPED UNDER BACKPRESSURE. The consumer stalls on a
//     pseudo-random schedule and the stream must be identical to the
//     unstalled one.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_probe_walk_earth.h"

#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_terrain.hpp"

namespace {

using zhao::check;

constexpr int kLat = 33;
constexpr int kVerts = kLat * kLat;

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint64_t next64() {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    uint64_t x = s;
    x ^= x >> 33;
    x *= 0xFF51AFD7ED558CCDULL;
    x ^= x >> 33;
    return x;
  }
  uint32_t below(uint32_t n) { return n ? (uint32_t)(next64() % n) : 0; }
  int32_t range(int32_t lo, int32_t hi) { return lo + (int32_t)below((uint32_t)(hi - lo + 1)); }
};

// One emitted vector group, as both sides describe it.
struct Group {
  int iv;
  uint8_t mask;
  int32_t z;
  int32_t x[4];
  bool last;
  bool operator!=(const Group& o) const {
    return iv != o.iv || mask != o.mask || z != o.z || last != o.last ||
           std::memcmp(x, o.x, sizeof x) != 0;
  }
};

// The prepared lattice, built exactly as the reference builds it.
struct Lattice {
  int32_t wx[kLat], wz[kLat];
  Lattice(int32_t ex0, int32_t ex1, int32_t ez0, int32_t ez1) {
    for (int i = 0; i < kLat; ++i) wx[i] = zref::terrain::lattice_lerp(ex0, ex1, i, kLat - 1);
    for (int j = 0; j < kLat; ++j) wz[j] = zref::terrain::lattice_lerp(ez0, ez1, j, kLat - 1);
  }
};

struct Assoc {
  int32_t x0, x1, z0, z1;  // closed footprint, fx16 raw
  int i0, i1, j0, j1;      // prepared box (a hint)
};

// THE ORACLE: the walk the reference performs, expressed as the group stream
// the walker must produce. Coverage is the closed-interval test on the
// COORDINATES; the box only decides where the walk starts and stops.
std::vector<Group> expect_groups(const Lattice& lat, const Assoc& a) {
  std::vector<Group> out;
  if (a.i0 > a.i1 || a.j0 > a.j1) return out;
  for (int j = a.j0; j <= a.j1; ++j) {
    const bool in_z = lat.wz[j] >= a.z0 && lat.wz[j] <= a.z1;
    for (int i = a.i0; i <= a.i1; i += 4) {
      Group g{};
      g.iv = j * kLat + i;
      g.z = lat.wz[j];
      g.mask = 0;
      for (int l = 0; l < 4; ++l) {
        const int vi = i + l;
        g.x[l] = vi < kLat ? lat.wx[vi] : lat.wx[0];
        if (vi < kLat && in_z && lat.wx[vi] >= a.x0 && lat.wx[vi] <= a.x1) g.mask |= (1u << l);
      }
      g.last = (i + 4 > a.i1) && (j >= a.j1);
      out.push_back(g);
    }
  }
  return out;
}

void load_tables(Vzhao_probe_walk_earth& t, const Lattice& lat) {
  for (int i = 0; i < kLat; ++i) {
    t.lt_we_i = 1;
    t.lt_sel_i = 0;
    t.lt_idx_i = (uint8_t)i;
    t.lt_val_i = (uint32_t)lat.wx[i];
    zhao::tick(t);
    t.lt_sel_i = 1;
    t.lt_val_i = (uint32_t)lat.wz[i];
    zhao::tick(t);
  }
  t.lt_we_i = 0;
}

// Drive one association and collect its groups. `stall` picks a backpressure
// schedule; 0 means never stall. Returns the clocks the association occupied
// from acceptance to its last group, so the gate can be MEASURED.
std::vector<Group> run_assoc(Vzhao_probe_walk_earth& t, const Assoc& a, Prng* stall, int* clocks) {
  std::vector<Group> got;
  t.as_valid_i = 1;
  t.as_fp_x0_i = (uint32_t)a.x0;
  t.as_fp_x1_i = (uint32_t)a.x1;
  t.as_fp_z0_i = (uint32_t)a.z0;
  t.as_fp_z1_i = (uint32_t)a.z1;
  t.as_box_i0_i = (uint8_t)a.i0;
  t.as_box_i1_i = (uint8_t)a.i1;
  t.as_box_j0_i = (uint8_t)a.j0;
  t.as_box_j1_i = (uint8_t)a.j1;
  t.out_ready_i = 0;
  // Acceptance edge.
  while (!t.as_ready_o) zhao::tick(t);
  zhao::tick(t);
  t.as_valid_i = 0;

  int n = 0;
  int guard = 0;
  bool done = false;
  while (!done && guard++ < 40000) {
    const bool ready = stall == nullptr ? true : (stall->below(4) != 0);
    t.out_ready_i = ready ? 1 : 0;
    if (t.out_valid_o && ready) {
      Group g{};
      g.iv = (int)t.out_iv_o;
      g.mask = (uint8_t)t.out_mask_o;
      g.z = (int32_t)t.out_z_o;
      g.x[0] = (int32_t)t.out_x0_o;
      g.x[1] = (int32_t)t.out_x1_o;
      g.x[2] = (int32_t)t.out_x2_o;
      g.x[3] = (int32_t)t.out_x3_o;
      g.last = t.out_last_o != 0;
      got.push_back(g);
      if (g.last) done = true;
    }
    if (!t.out_valid_o && got.empty() && guard > 8) break;  // empty association
    zhao::tick(t);
    ++n;
  }
  t.out_ready_i = 0;
  if (clocks) *clocks = n;
  return got;
}

bool compare(const std::vector<Group>& want, const std::vector<Group>& got, const char* what) {
  if (want.size() != got.size()) {
    check(false, what, want.size(), got.size());
    return false;
  }
  for (size_t k = 0; k < want.size(); ++k) {
    if (want[k] != got[k]) {
      printf("   first mismatch at group %zu: want iv=%d mask=%u z=%d  got iv=%d mask=%u z=%d\n", k,
             want[k].iv, want[k].mask, want[k].z, got[k].iv, got[k].mask, got[k].z);
      check(false, what, (uint64_t)want[k].iv, (uint64_t)got[k].iv);
      return false;
    }
  }
  check(true, what, want.size(), got.size());
  return true;
}

// ---------------------------------------------------------------------------

// A full-patch association: the whole lattice, footprint wide open.
void test_full_patch_gate(Vzhao_probe_walk_earth& t) {
  printf("-- full patch: the 297-group gate\n");
  const Lattice lat(0, 32 << 16, 0, 32 << 16);
  load_tables(t, lat);
  Assoc a{INT32_MIN / 2, INT32_MAX / 2, INT32_MIN / 2, INT32_MAX / 2, 0, kLat - 1, 0, kLat - 1};
  int clocks = 0;
  const std::vector<Group> got = run_assoc(t, a, nullptr, &clocks);
  const std::vector<Group> want = expect_groups(lat, a);

  printf("   MEASURED: full 33x33 walk, %zu groups in %d clocks\n", got.size(), clocks);
  check(want.size() == 297, "the oracle itself says 297 groups (9 per row x 33)", 297, want.size());
  compare(want, got, "full-patch group stream matches the reference walk");
  check((int)got.size() == clocks, "THE GATE: one group per clock, no stall", (int)got.size(),
        clocks);

  int covered = 0;
  for (const Group& g : got) covered += __builtin_popcount(g.mask);
  check(covered == kVerts, "every lattice vertex covered exactly once", kVerts, covered);

  // THE COUNTERS ARE PORTS, AND A PORT NO TEST READS IS NOT TESTED. The
  // first sweep of this block survived W17 -- "the covered-vertex counter
  // counts groups rather than lanes" -- because coverage was only ever
  // recomputed from the emitted masks on this side. 297 groups and 1,089
  // covered vertices are different numbers, so reading both distinguishes
  // them.
  check((int)t.groups_emitted_o == (int)got.size(), "groups_emitted_o counts the groups",
        (int)got.size(), (int)t.groups_emitted_o);
  check((int)t.verts_covered_o == covered, "verts_covered_o counts covered LANES, not groups",
        covered, (int)t.verts_covered_o);
}

// Every group must lie in ONE row -- the property that lets a group carry a
// single z for all four lanes.
void test_group_never_straddles_a_row(Vzhao_probe_walk_earth& t) {
  printf("-- one z per group\n");
  const Lattice lat(0, 32 << 16, 0, 32 << 16);
  load_tables(t, lat);
  Assoc a{INT32_MIN / 2, INT32_MAX / 2, INT32_MIN / 2, INT32_MAX / 2, 0, kLat - 1, 0, kLat - 1};
  const std::vector<Group> got = run_assoc(t, a, nullptr, nullptr);
  bool ok = true;
  for (const Group& g : got) {
    const int row = g.iv / kLat;
    for (int l = 0; l < 4; ++l) {
      if (!(g.mask & (1u << l))) continue;
      if ((g.iv + l) / kLat != row) ok = false;
      if (g.z != lat.wz[row]) ok = false;
    }
  }
  check(ok, "no group straddles a row, and its z is that row's", 1, ok ? 1 : 0);
}

// THE BOX IS A HINT. An inflated box must not add coverage; a tight box must
// not lose any.
void test_box_is_a_hint_not_the_law(Vzhao_probe_walk_earth& t) {
  printf("-- the box is a hint, the closed-interval test is the law\n");
  const Lattice lat(0, 32 << 16, 0, 32 << 16);
  load_tables(t, lat);

  // A footprint covering vertices i in [8,12], j in [4,6] exactly.
  const int32_t x0 = lat.wx[8], x1 = lat.wx[12];
  const int32_t z0 = lat.wz[4], z1 = lat.wz[6];

  Assoc tight{x0, x1, z0, z1, 8, 12, 4, 6};
  Assoc loose{x0, x1, z0, z1, 0, kLat - 1, 0, kLat - 1};  // deliberately inflated

  const std::vector<Group> g_tight = run_assoc(t, tight, nullptr, nullptr);
  const std::vector<Group> g_loose = run_assoc(t, loose, nullptr, nullptr);
  compare(expect_groups(lat, tight), g_tight, "tight box: stream matches the oracle");
  compare(expect_groups(lat, loose), g_loose, "loose box: stream matches the oracle");

  int cov_tight = 0, cov_loose = 0;
  for (const Group& g : g_tight) cov_tight += __builtin_popcount(g.mask);
  for (const Group& g : g_loose) cov_loose += __builtin_popcount(g.mask);
  check(cov_tight == 5 * 3, "the tight box covers the 15 vertices in footprint", 15, cov_tight);
  check(cov_loose == cov_tight,
        "INFLATING the box changes cost, never coverage (the box is not the law)", cov_tight,
        cov_loose);
  check(g_loose.size() > g_tight.size(), "the inflated box does cost more groups", 1,
        g_loose.size() > g_tight.size() ? 1 : 0);
}

// The border is INSIDE (spec/terrain_rules.md 9.1).
void test_footprint_border_is_inside(Vzhao_probe_walk_earth& t) {
  printf("-- closed interval: a border vertex is inside\n");
  const Lattice lat(0, 32 << 16, 0, 32 << 16);
  load_tables(t, lat);
  // Exactly one vertex, on all four borders at once.
  Assoc a{lat.wx[7], lat.wx[7], lat.wz[9], lat.wz[9], 0, kLat - 1, 0, kLat - 1};
  const std::vector<Group> got = run_assoc(t, a, nullptr, nullptr);
  compare(expect_groups(lat, a), got, "single-border-vertex stream matches the oracle");
  int covered = 0;
  for (const Group& g : got) covered += __builtin_popcount(g.mask);
  check(covered == 1, "the vertex ON the border is covered", 1, covered);

  // One ulp inside on the low side and the vertex leaves.
  Assoc b{lat.wx[7] + 1, lat.wx[7] + 1, lat.wz[9], lat.wz[9], 0, kLat - 1, 0, kLat - 1};
  const std::vector<Group> got_b = run_assoc(t, b, nullptr, nullptr);
  int covered_b = 0;
  for (const Group& g : got_b) covered_b += __builtin_popcount(g.mask);
  check(covered_b == 0, "one ulp past the border and it is outside", 0, covered_b);
}

// An empty box costs one acceptance and no groups -- and must not hang.
// Offer one association and count the groups it emits, without waiting for a
// `last` that an empty box will never produce.
int emitted_groups_for(Vzhao_probe_walk_earth& t, const Assoc& a, int clocks) {
  t.as_valid_i = 1;
  t.as_fp_x0_i = (uint32_t)a.x0;
  t.as_fp_x1_i = (uint32_t)a.x1;
  t.as_fp_z0_i = (uint32_t)a.z0;
  t.as_fp_z1_i = (uint32_t)a.z1;
  t.as_box_i0_i = (uint8_t)a.i0;
  t.as_box_i1_i = (uint8_t)a.i1;
  t.as_box_j0_i = (uint8_t)a.j0;
  t.as_box_j1_i = (uint8_t)a.j1;
  t.out_ready_i = 1;
  int emitted = 0;
  for (int k = 0; k < clocks; ++k) {
    if (t.out_valid_o) ++emitted;
    zhao::tick(t);
  }
  t.as_valid_i = 0;
  t.out_ready_i = 0;
  zhao::tick(t);
  return emitted;
}

// An empty box costs one acceptance and no groups -- and must not hang.
//
// BOTH AXES, and the second one is not decoration. The emptiness test is
// `(i0 > i1) || (j0 > j1)`, and a version that checks only the COLUMN half
// SURVIVED the first mutation sweep of this block (W15): the original case
// drove `i0 > i1` alone, so dropping the row half changed nothing it could
// see. An empty ROW range must be refused by its own term.
void test_empty_box_does_not_hang(Vzhao_probe_walk_earth& t) {
  printf("-- an empty association is accepted and emits nothing, on either axis\n");
  const Lattice lat(0, 32 << 16, 0, 32 << 16);
  load_tables(t, lat);

  const Assoc empty_cols{0, 1 << 16, 0, 1 << 16, 20, 4, 0, kLat - 1};  // i0 > i1
  const Assoc empty_rows{0, 1 << 16, 0, 1 << 16, 0, kLat - 1, 25, 3};  // j0 > j1
  const Assoc empty_both{0, 1 << 16, 0, 1 << 16, 20, 4, 25, 3};

  check(emitted_groups_for(t, empty_cols, 40) == 0, "an empty COLUMN range emits no groups", 0,
        emitted_groups_for(t, empty_cols, 40));
  check(t.as_ready_o != 0, "and the walker is still ready after it", 1, t.as_ready_o != 0 ? 1 : 0);

  check(emitted_groups_for(t, empty_rows, 40) == 0, "an empty ROW range emits no groups", 0,
        emitted_groups_for(t, empty_rows, 40));
  check(t.as_ready_o != 0, "and the walker is still ready after that too", 1,
        t.as_ready_o != 0 ? 1 : 0);

  check(emitted_groups_for(t, empty_both, 40) == 0, "a box empty on both axes emits no groups", 0,
        emitted_groups_for(t, empty_both, 40));
  check(t.as_ready_o != 0, "and the walker is still ready for the next association", 1,
        t.as_ready_o != 0 ? 1 : 0);
}

// Backpressure must not change the stream.
void test_backpressure_drops_nothing(Vzhao_probe_walk_earth& t) {
  printf("-- backpressure changes timing, never content\n");
  const Lattice lat(-7 << 16, 25 << 16, 3 << 16, 40 << 16);
  load_tables(t, lat);
  Assoc a{lat.wx[3], lat.wx[27], lat.wz[2], lat.wz[30], 0, kLat - 1, 0, kLat - 1};
  const std::vector<Group> clean = run_assoc(t, a, nullptr, nullptr);
  Prng p(0xB0FFED);
  const std::vector<Group> stalled = run_assoc(t, a, &p, nullptr);
  compare(clean, stalled, "the stalled stream is identical to the unstalled one");
  compare(expect_groups(lat, a), clean, "and both match the oracle");
}

// Randomized: arbitrary envelopes, footprints and boxes.
void test_random(Vzhao_probe_walk_earth& t, int iters) {
  printf("-- randomized differential, %d associations\n", iters);
  Prng p(0x5EED17);
  int bad = 0;
  for (int k = 0; k < iters; ++k) {
    const int32_t ex0 = p.range(-40, 40) << 16;
    const int32_t ex1 = ex0 + (p.range(1, 60) << 16);
    const int32_t ez0 = p.range(-40, 40) << 16;
    const int32_t ez1 = ez0 + (p.range(1, 60) << 16);
    const Lattice lat(ex0, ex1, ez0, ez1);
    load_tables(t, lat);

    const int ia = p.below(kLat), ib = p.below(kLat);
    const int ja = p.below(kLat), jb = p.below(kLat);
    Assoc a{};
    a.x0 = lat.wx[ia < ib ? ia : ib] - (int32_t)p.below(1 << 15);
    a.x1 = lat.wx[ia < ib ? ib : ia] + (int32_t)p.below(1 << 15);
    a.z0 = lat.wz[ja < jb ? ja : jb] - (int32_t)p.below(1 << 15);
    a.z1 = lat.wz[ja < jb ? jb : ja] + (int32_t)p.below(1 << 15);
    // The box is prepared conservatively: correct, or wider than correct.
    a.i0 = 0;
    a.i1 = kLat - 1;
    a.j0 = 0;
    a.j1 = kLat - 1;

    const std::vector<Group> got = run_assoc(t, a, (k & 1) ? &p : nullptr, nullptr);
    const std::vector<Group> want = expect_groups(lat, a);
    if (want.size() != got.size()) {
      ++bad;
      continue;
    }
    for (size_t q = 0; q < want.size(); ++q)
      if (want[q] != got[q]) {
        ++bad;
        break;
      }
  }
  check(bad == 0, "every randomized association matches the reference walk", 0, bad);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--random" && i + 1 < argc) iters = std::atoi(argv[++i]);
  }

  Vzhao_probe_walk_earth top;
  // zhao::reset() drives in_valid/in_data, which belong to the byte-stream
  // blocks; this probe's intake is a descriptor port. Reset it directly.
  top.rst_n = 0;
  top.lt_we_i = 0;
  top.as_valid_i = 0;
  top.out_ready_i = 0;
  top.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();
  zhao::tick(top);

  if (iters > 0) {
    test_random(top, iters);
  } else {
    test_full_patch_gate(top);
    test_group_never_straddles_a_row(top);
    test_box_is_a_hint_not_the_law(top);
    test_footprint_border_is_inside(top);
    test_empty_box_does_not_hang(top);
    test_backpressure_drops_nothing(top);
  }
  return zhao::report_and_exit("FIELD.WALK.EARTH");
}
