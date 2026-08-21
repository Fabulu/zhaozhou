// field_out_velocity.cpp — the FIELD.OUT.VELOCITY op, RTL against the oracle.
//
// WHY THIS FILE EXISTS. `design/ops.yml` declares FIELD.OUT.VELOCITY and names
// this path as its differential coverage. The file did not exist, and ledger
// rule V10 surfaced that the moment TERRAIN.VELOCITY was advanced past
// SPECIFIED. It was the block's ONLY remaining blocker.
//
// The op cites `zref::fieldir::sink_out_velocity`, which does not exist — one of
// the twenty-five phantoms in reports/PHANTOM_REFERENCES.md. Like
// FIELD.OUT.HEIGHT, this is kind 1: the law was already implemented under
// another name. The velocity out-lane is `lane[i]` in
// `zref::terrain::velocity_vertex`, and the op IS the routing of that lane into
// the §4.4 accumulation.
//
// SCOPE. `terrain_velocity_directed.cpp` and its random lanes already sweep whole
// lattices against the oracle. This file tests only what the OP owns, and every
// one of the four laws below is a place where the velocity lane behaves
// DIFFERENTLY from the height lane it sits beside — which is exactly the kind of
// thing an implementer copies across and gets wrong:
//
//   1. THERE IS NO CLAMP. The height chain clamps at the modelled underside;
//      a RATE has no such law. The reference says so outright: inventing one
//      "would silently zero the downward half of every wave". A block that
//      copied §3.4's clamp across would make every wave rise and never fall.
//   2. THE BAKE-BACK TO height16 HAPPENS ONCE, at the end. The chain runs in
//      fx16 and is narrowed exactly once (qformats §3's single-rounding law).
//      Narrowing per lane is the natural mistake and is wrong by an LSB.
//   3. A LANE THAT DOES NOT COVER IS SKIPPED, NOT ADDED AS ZERO. Value-identical
//      by design, so it is checked on the block's `covered` output.
//   4. THE TWO SATURATION LEDGERS ARE SEPARATE. `add` counts the fx_add chain,
//      `rescale` counts the bake-back. The reference header asks for both to be
//      exposed precisely so a differential can check saturation BEHAVIOUR rather
//      than only the value — a block that saturated in the wrong place could
//      still produce the right number here and be wrong everywhere else.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_velocity.h"

#include "velocity_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_terrain_velocity.hpp"

namespace {

using zhao::check;

/** Compare a whole sweep, on every lane the op owns. */
void compare(const vdev::SweepOut& want, const vdev::SweepOut& got, const zref::SatLedger& L,
             const char* what) {
  const std::string t(what);
  check(!got.timed_out, (t + ": the sweep completed").c_str(), 0, got.timed_out ? 1 : 0);

  uint64_t vel_bad = 0, mov_bad = 0, cov_bad = 0;
  int first = -1;
  for (size_t i = 0; i < want.velocity.size(); ++i) {
    if (got.velocity[i] != want.velocity[i]) {
      if (first < 0) first = static_cast<int>(i);
      ++vel_bad;
    }
    if (got.moving[i] != want.moving[i]) ++mov_bad;
    if (got.covered[i] != want.covered[i]) ++cov_bad;
  }
  check(vel_bad == 0, (t + ": every velocity word").c_str(), 0, vel_bad);
  if (vel_bad && first >= 0) {
    std::printf("  first divergence at vertex %d: want %d got %d\n", first,
                want.velocity[static_cast<size_t>(first)],
                got.velocity[static_cast<size_t>(first)]);
  }
  check(mov_bad == 0, (t + ": every moving flag").c_str(), 0, mov_bad);
  check(cov_bad == 0, (t + ": every covered flag").c_str(), 0, cov_bad);
  check(got.moving_mask == want.moving_mask, (t + ": the subpatch moving mask").c_str(),
        want.moving_mask, got.moving_mask);

  // Law 4: the two ledgers are separate and both are the oracle's.
  check(got.add_sats == L.add, (t + ": SatLedger::add — the fx_add chain").c_str(), L.add,
        got.add_sats);
  check(got.rescale_sats == L.rescale, (t + ": SatLedger::rescale — the bake-back").c_str(),
        L.rescale, got.rescale_sats);
}

void run(Vzhao_terrain_velocity& dut, const vdev::LanePlane& plane, const char* what,
         int stall_lane = 0, int stall_sink = 0) {
  zref::SatLedger L{};
  const vdev::SweepOut want = vdev::oracle_sweep(plane, 0x77, &L);
  const vdev::SweepOut got = vdev::run_sweep(dut, plane, 5, 0x77, stall_lane, stall_sink);
  compare(want, got, L, what);
}

const int32_t kOne = 1 << 16;

}  // namespace

int main(int argc, char** argv) {
  Vzhao_terrain_velocity dut;
  vdev::reset_dut(dut);

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    vdev::Rng rng(0x0E10u);
    for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      vdev::LanePlane plane;
      plane.resize(1 + static_cast<int>(rng.next() % 5));
      for (int vj = 0; vj < vdev::kLat; ++vj) {
        for (int vi = 0; vi < vdev::kLat; ++vi) {
          for (int k = 0; k < plane.lanes; ++k) {
            const size_t idx = plane.at(vi, vj, k);
            // A wide magnitude range so the fx_add chain and the bake-back both
            // reach their rails on a real share of vertices, not just in theory.
            plane.velocity[idx] = static_cast<int32_t>(rng.next()) >>
                                  static_cast<int>(rng.next() % 20);
            plane.covers[idx] = (rng.next() % 4) != 0 ? 1 : 0;
          }
        }
      }
      char tag[80];
      std::snprintf(tag, sizeof tag, "random[%u] lanes=%d", it, plane.lanes);
      // Stall PERIODS, not probabilities: a period of 1 stalls every cycle and
      // the sweep never finishes. 0 means never stall; anything else must be at
      // least 2, which is why the existing random suite uses range(2,7).
      const int sl = (rng.next() % 3 == 0) ? 2 + static_cast<int>(rng.next() % 6) : 0;
      const int ss = (rng.next() % 3 == 0) ? 2 + static_cast<int>(rng.next() % 8) : 0;
      run(dut, plane, tag, sl, ss);
    }
    dut.final();
    return zhao::report_and_exit("field_out_velocity_random");
  }

  // ---- 1. one lane, the plain route ---------------------------------------
  {
    vdev::LanePlane p;
    p.resize(1);
    for (int vj = 0; vj < vdev::kLat; ++vj) {
      for (int vi = 0; vi < vdev::kLat; ++vi) {
        p.velocity[p.at(vi, vj, 0)] = (vi - 16) * kOne / 8;
        p.covers[p.at(vi, vj, 0)] = 1;
      }
    }
    run(dut, p, "one velocity lane reaches the accumulation");
  }

  // ---- 2. NO CLAMP: the downward half of a wave survives -------------------
  // The height chain clamps at the underside. A rate must not. Every vertex here
  // carries a NEGATIVE velocity, so an implementation that copied §3.4's clamp
  // across would zero the entire lattice -- the ground would rise and never
  // fall, and no wave would ever come back down.
  {
    vdev::LanePlane p;
    p.resize(1);
    for (int vj = 0; vj < vdev::kLat; ++vj) {
      for (int vi = 0; vi < vdev::kLat; ++vi) {
        p.velocity[p.at(vi, vj, 0)] = -(1 + vi + vj) * kOne;
        p.covers[p.at(vi, vj, 0)] = 1;
      }
    }
    zref::SatLedger L{};
    const vdev::SweepOut want = vdev::oracle_sweep(p, 0x77, &L);
    // The fixture only means something if the oracle really does keep them.
    bool any_negative = false;
    for (int16_t v : want.velocity) any_negative = any_negative || (v < 0);
    check(any_negative, "the fixture actually produces downward velocities", 1,
          any_negative ? 1 : 0);
    run(dut, p, "a rate is never clamped: downward velocities survive");
  }

  // ---- 3. THE BAKE-BACK HAPPENS ONCE --------------------------------------
  // Several lanes whose fx16 sum narrows differently from the sum of their
  // narrowings. Narrowing per lane is the natural mistake, and it is wrong by an
  // LSB on a large share of vertices -- which on a velocity lattice reads as a
  // faint shimmer in the terrain motion rather than as a break.
  {
    vdev::LanePlane p;
    p.resize(4);
    for (int vj = 0; vj < vdev::kLat; ++vj) {
      for (int vi = 0; vi < vdev::kLat; ++vi) {
        // Deliberately awkward sub-height16 residues: each lane alone rounds to
        // nothing much, but the sum does not.
        p.velocity[p.at(vi, vj, 0)] = 129 + vi * 7;
        p.velocity[p.at(vi, vj, 1)] = 130 + vj * 11;
        p.velocity[p.at(vi, vj, 2)] = -(61 + vi);
        p.velocity[p.at(vi, vj, 3)] = 255;
        for (int k = 0; k < 4; ++k) p.covers[p.at(vi, vj, k)] = 1;
      }
    }
    run(dut, p, "the fx16 chain is baked back to height16 exactly once");
  }

  // ---- 4. SKIPPED, NOT ADDED AS ZERO --------------------------------------
  // Value-identical by design, so the observable is `covered`. A vertex no lane
  // covers must report covered = 0 and still write its zero word -- the "not
  // moving" word is written every frame, it is not an absence.
  {
    vdev::LanePlane p;
    p.resize(3);
    for (int vj = 0; vj < vdev::kLat; ++vj) {
      for (int vi = 0; vi < vdev::kLat; ++vi) {
        // A checkerboard of coverage, one lane that covers nowhere at all, and
        // a third that covers only part of the checkerboard's OFF squares -- so
        // a real share of vertices end up covered by nothing, which is the case
        // this section exists for. (The first draft had lanes 0 and 2 between
        // them covering every vertex, and the fixture-validity check below is
        // what caught it.)
        const bool on = ((vi + vj) & 1) == 0;
        p.velocity[p.at(vi, vj, 0)] = on ? 3 * kOne : 0;
        p.covers[p.at(vi, vj, 0)] = on ? 1 : 0;
        p.velocity[p.at(vi, vj, 1)] = 1000 * kOne;   // huge, and never covers
        p.covers[p.at(vi, vj, 1)] = 0;
        const bool lane2 = !on && (vj % 3 == 0);
        p.velocity[p.at(vi, vj, 2)] = lane2 ? -2 * kOne : 0;
        p.covers[p.at(vi, vj, 2)] = lane2 ? 1 : 0;
      }
    }
    zref::SatLedger L{};
    const vdev::SweepOut want = vdev::oracle_sweep(p, 0x77, &L);
    bool some_covered = false, some_not = false;
    for (uint8_t c : want.covered) { some_covered = some_covered || c; some_not = some_not || !c; }
    check(some_covered && some_not,
          "the fixture has both covered and uncovered vertices", 1,
          (some_covered && some_not) ? 1 : 0);
    run(dut, p, "an uncovered lane is skipped, and every vertex still writes a word");
  }

  // ---- 5. the saturation rails, in both ledgers ---------------------------
  // Values chosen so the fx_add chain saturates AND the bake-back saturates, so
  // both counters are non-zero and a block that saturated in the wrong place
  // would be caught even if the stored word happened to match.
  {
    vdev::LanePlane p;
    p.resize(3);
    const int32_t kMax = 0x7FFF'FFFF;
    for (int vj = 0; vj < vdev::kLat; ++vj) {
      for (int vi = 0; vi < vdev::kLat; ++vi) {
        p.velocity[p.at(vi, vj, 0)] = kMax;
        p.velocity[p.at(vi, vj, 1)] = kMax;
        p.velocity[p.at(vi, vj, 2)] = (vi & 1) ? -kMax : kMax;
        for (int k = 0; k < 3; ++k) p.covers[p.at(vi, vj, k)] = 1;
      }
    }
    zref::SatLedger L{};
    const vdev::SweepOut want = vdev::oracle_sweep(p, 0x77, &L);
    check(L.add > 0, "the fixture saturates the fx_add chain", 1, L.add > 0 ? 1 : 0);
    check(L.rescale > 0, "and saturates the bake-back too", 1, L.rescale > 0 ? 1 : 0);
    run(dut, p, "both saturation ledgers agree with the oracle");
  }

  // ---- 6. an empty field list still writes the lattice --------------------
  // Zero lanes: every vertex is uncovered, every word is the "not moving" zero,
  // and the sweep still completes. A block that skipped the write would leave a
  // stale velocity lattice behind and the ground would keep moving after the
  // spell ended.
  {
    vdev::LanePlane p;
    p.resize(0);
    run(dut, p, "a patch with no live fields writes a zero lattice");
  }

  // ---- 7. the same plane under backpressure gives the same lattice --------
  {
    vdev::LanePlane p;
    p.resize(2);
    for (int vj = 0; vj < vdev::kLat; ++vj) {
      for (int vi = 0; vi < vdev::kLat; ++vi) {
        p.velocity[p.at(vi, vj, 0)] = (vi * 37 - vj * 11) * 1024;
        p.velocity[p.at(vi, vj, 1)] = -(vi * 13 + vj * 29) * 512;
        p.covers[p.at(vi, vj, 0)] = 1;
        p.covers[p.at(vi, vj, 1)] = ((vi + vj) % 3) != 0 ? 1 : 0;
      }
    }
    run(dut, p, "with lane-side backpressure", 3, 0);
    run(dut, p, "with sink-side backpressure", 0, 4);
    run(dut, p, "with both", 2, 5);
  }

  dut.final();
  return zhao::report_and_exit("field_out_velocity");
}
