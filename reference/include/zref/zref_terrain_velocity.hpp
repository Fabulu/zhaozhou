// zref_terrain_velocity.hpp — the TERRAIN.VELOCITY oracle.
//
// A THIN VIEW onto the velocity lane the shipped renderer already records,
// not a second implementation of it. `zref::render::compose_lattice` walks
// live earth field applications over a patch and pushes one
// `TerrainVelocitySample{wx, wz, out[1]}` per (application, covered vertex);
// `zref::render::field_velocity_lane` is the one place that names out-lane 1
// as "velocity". This header states what the LATTICE those samples build up
// to is, for one vertex, because the hardware seam is exactly there:
// FIELD.SEQ.EARTH evaluates, TERRAIN.VELOCITY accumulates and bakes back.
//
// `tests/terrain/terrain_velocity_directed` proves the view is faithful by
// running a real 33x33 patch with two real earth programs through
// compose_lattice, grouping the recorded samples by vertex in command order,
// and requiring this header to reproduce every one of the 1,089 lattice words
// bit-for-bit. Without that cross-check this file would be a second
// implementation of the velocity lane and charter 29-6 would be broken before
// the RTL was written.
//
// Law, in citation order:
//   spec/terrain_rules.md 4.4 — "Velocity is the Earth velocity out-lane
//     ACCUMULATED at lattice vertices (TERRAIN.VELOCITY), interpolated by the
//     same 4.3 rule."  The interpolation is the CONSUMER's (column_query);
//     this block produces the lattice the consumer interpolates.
//   spec/terrain_rules.md 4.2 — "the velocity lattice (height16-scaled,
//     2 B/vertex) 545 KiB", produced once per frame alongside the composed
//     height cache.  That is the storage format, frozen.
//   spec/terrain_rules.md 4.1 — field programs are evaluated ONLY at lattice
//     vertices, by the ONE interpreter (zfield::interpret).  Nothing here
//     evaluates anything.
//   spec/terrain_rules.md 9.1 — the CLOSED-interval footprint test; a lane
//     whose footprint misses the vertex contributes nothing at all.
//   design/ops.yml FIELD.OUT.VELOCITY — `result_q: spec/qformats.md
//     height16`, `rounding: saturating`.  This is the ratified bake-back.
//   spec/form/field-ir.md 7.1 — the earth output record
//     {height:fx, velocity:fx, material:u32, nav_cost:fx}: velocity is
//     out-lane 1, Q16.16.
//   spec/qformats.md 2/9 — fx16 -> height16 is `rescale(x,8)` then saturate
//     s16, i.e. `zref::height16_from_fx16`; 3 — fx_add saturates, and the
//     single-rounding law.
//   design/contracts/TERRAIN.VELOCITY.md — where the two chosen laws below
//     are argued at length.
//
// TWO LAWS CHOSEN, NOT FOUND (both argued in the contract):
//
//   V1. THE ACCUMULATION IS A SATURATING fx_add CHAIN IN COMMAND ORDER OVER
//       COVERING LANES ONLY, WITH EXACTLY ONE BAKE-BACK AT THE END.
//       terrain_rules 4.4 says "accumulated" and stops. compose_lattice does
//       not accumulate at all — it records one sample per application, and
//       nothing downstream of it in this tree consumes them, so the reduction
//       has never been written down. It is chosen to be the SAME reduction
//       terrain_rules 3.4 already applies to the height lane at the same
//       vertex from the same evaluations: fx_add, saturating, command order.
//       REJECTED ALTERNATIVES:
//         (a) last-writer-wins — cheaper, but two overlapping waves would
//             make the ground's measured speed depend on command order in a
//             way the ground's measured HEIGHT does not, and a collision
//             solver reading both would see them disagree;
//         (b) max-magnitude — order-independent, which is attractive, but it
//             is not a velocity: two opposed waves that cancel exactly in
//             height would report the full speed of the larger one;
//         (c) accumulate in height16 (convert each lane, then add) — that is
//             TWO roundings per lane and qformats 3's single-rounding law
//             rejects it outright. The chain is fx16 throughout and the ONE
//             rounding is the final bake-back.
//
//   V2. A VERTEX NO LANE COVERS HAS VELOCITY EXACTLY ZERO.
//       compose_lattice records NO sample for such a vertex, but 4.2's
//       lattice is 2 B for EVERY vertex, so the word must be defined.
//       Ground that no live field touches is not moving, and 0 is what "not
//       moving" is. REJECTED ALTERNATIVE: leave the previous frame's word
//       (a persistence/decay reading). That would give a wave's trailing edge
//       a stale non-zero speed for the rest of the level, and no decay pass
//       exists anywhere in this tree to retire it — see the contract's note
//       on the persistent-vs-healing question, which is NOT ratified.
//
// NOT IN THIS HEADER, deliberately: no field evaluation (4.1 forbids a second
// evaluator), no 4.3 interpolation (the consumer's, and `column_query`
// already owns it), no footprint rectangle test (that is
// `zref::terrain::covers` in zref_terrain_patch.hpp — ONE implementation,
// and this header takes its answer as an input), no clamp at the underside
// (3.4's two clamps are about a SURFACE never punching below the modelled
// bottom; a rate has no such law and inventing one would silently zero the
// downward half of every wave).

#pragma once

#include <cstdint>

#include "zref/zref_fixp.hpp"

namespace zref {
namespace terrain {

/** One vertex of the 4.2 velocity lattice. */
struct VelocityOut {
  int32_t accum_fx = 0;  // the command-order fx_add chain, fx16 raw
  int16_t velocity = 0;  // the stored lattice word, height16 (4.2)
  bool moving = false;   // velocity != 0: this vertex's ground has a speed
  bool covered = false;  // at least one lane's footprint covered the vertex
};

/**
 * `velocity_vertex` — the 4.4 accumulation at ONE lattice vertex.
 *
 * `lane[i]` is out-lane 1 (velocity, Q16.16) of accepted field-list entry `i`
 * evaluated at this vertex — FIELD.SEQ.EARTH's job, by the one interpreter.
 * `covers[i]` is `zref::terrain::covers(list[i], wx, wz)`, the CLOSED-interval
 * footprint test of terrain_rules 9.1, decided once by the block that owns the
 * field list (TERRAIN.PATCH) and travelling with the lane. A lane that does
 * not cover is SKIPPED, not added as zero — identical in value, and identical
 * in SatLedger records too, which is why it is written this way and not as a
 * multiply by a mask.
 *
 * The chain runs in fx16 and is bake-backed to height16 exactly ONCE
 * (qformats 3's single-rounding law; ops.yml FIELD.OUT.VELOCITY's
 * `result_q: height16, rounding: saturating`). `fx_add` records in
 * `L->add`; the bake-back records in `L->rescale`. The RTL exposes both as
 * separate counters so the differential can check the saturation BEHAVIOUR
 * and not merely the value.
 */
inline VelocityOut velocity_vertex(const int32_t* lane, const bool* covers, int n,
                                   SatLedger* L = nullptr) {
  VelocityOut out;
  int32_t acc = 0;
  for (int i = 0; i < n; ++i) {
    if (!covers[i]) continue;  // 9.1: skipped, not added as zero
    out.covered = true;
    acc = fx_add(fx16{acc}, fx16{lane[i]}, L).raw;
  }
  out.accum_fx = acc;
  // V2: a vertex no lane covers falls through with acc == 0, and
  // height16_from_fx16(0) == 0 — the "not moving" word, written every frame.
  out.velocity = height16_from_fx16(fx16{acc}, L).raw;
  out.moving = out.velocity != 0;
  return out;
}

}  // namespace terrain
}  // namespace zref
