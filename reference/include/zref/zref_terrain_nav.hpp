// zref_terrain_nav.hpp — the NAVIGATION lattice oracle (FIELD.WRITE.NAV's
// per-vertex reduction), the sibling of zref_terrain_velocity.hpp.
//
// A THIN VIEW onto `zref::fieldir::compose_nav`, not a second implementation
// of it. That function is the owner ruling of 2026-08-24 and it already owns
// the whole arithmetic: signed Q16.16 deltas, SATURATING accumulation in
// COMMAND ORDER clamped to the int32 range after every delta, and ONE floor at
// zero on the way out. Nothing in this header re-derives a single add. It
// states only the two things compose_nav's signature leaves to its caller —
// WHICH lanes become deltas, and in WHICH order — because that is exactly the
// seam, and it is where a hand-transcription would otherwise appear.
//
// WHY IT EXISTS. `zref::render::compose_lattice` is the ONE §4.1 evaluation:
// it walks the live earth applications over a patch's lattice and consumes
// out-lane 0 (height) and records out-lane 1 (velocity). Out-lane 3 (nav_cost,
// spec/form/field-ir.md §7.1) was evaluated and DISCARDED — the lane existed,
// the number was computed, and nothing in the tree took it. This header is the
// reduction that turns the recorded lane into the per-vertex navigation cost
// the CPU navigation service (SW.CPUCOLL) answers queries from.
//
// Law, in citation order:
//   docs/OWNER_DOCKET.md 2026-08-24 item 6 / `zref::fieldir::compose_nav` —
//     "SIGNED movement-cost deltas, combined by SATURATING ADDITION in command
//     order, then clamped at zero below." THE arithmetic; called, never copied.
//   reports/OWNER-DECISION-20260926-I34-NAV.md — navigation truth and its
//     query service belong to SW.CPUCOLL / the CPU simulation runtime.
//     `FIELD.WRITE.NAV` and its semantics are PRESERVED; only the FPGA
//     PUBLICATION of a nav lattice into SDRAM is superseded.
//   spec/terrain_rules.md §4.1 — "collision, velocity, normals, nav — reads
//     the same composed lattice values and interpolates them on the same
//     triangulation (§4.3)." So nav is COMPOSED at lattice vertices and
//     INTERPOLATED by the consumer; this header produces the former.
//   spec/terrain_rules.md §4.1 — field programs are evaluated ONLY at lattice
//     vertices, by the ONE interpreter. Nothing here evaluates anything.
//   spec/terrain_rules.md §9.1 — the CLOSED-interval footprint test and the
//     bounded intake (kMaxPatchFields = 16). `zref::terrain::covers` owns the
//     test; this header takes its answer as an input.
//   spec/form/field-ir.md §7.1 — the earth output record
//     {height:fx, velocity:fx, material:u32, nav_cost:fx}: nav is out-lane 3.
//   design/ops.yml FIELD.WRITE.NAV — `reference_function:
//     zref::fieldir::compose_nav`, `rounding: saturating`.
//
// TWO LAWS CHOSEN, NOT FOUND. Both are stated here because compose_nav's
// signature does not cover them and somebody would otherwise choose them twice,
// differently, in the service and in the RTL differential.
//
//   N1. A LANE WHOSE FOOTPRINT DOES NOT COVER THE VERTEX IS SKIPPED, NOT ADDED
//       AS ZERO. Identical in value (0 is the additive identity, which is the
//       whole reason `zhao_field_sinks.sv` gives NAV no enable bit) and
//       identical in the delta COUNT, which is the part that matters: a
//       consumer asking "did any field speak about this cell" must not be told
//       yes by a field that missed it. This is `zref::terrain::velocity_vertex`
//       law V1's covering-lanes-only rule, verbatim, for the same reason.
//
//   N2. AN ABSENT OUT-LANE IS NOT A WRITE OF ZERO. A program that declares
//       fewer than four out-lanes never writes nav. `zfield::interpret` leaves
//       the caller's `out[3]` at whatever it was initialised to, so a caller
//       that does not track PRESENCE cannot tell "wrote 0" from "wrote
//       nothing". Numerically the two agree; in the counts they do not, and the
//       owner decision names optional-output presence as a PRESERVED semantic
//       in the same breath as command order and the zero floor. So `present[i]`
//       gates a lane out of the delta list entirely.
//       This mirrors `zhao_field_earth_adapter.sv`'s `ans_present_o`, which is
//       the same statement in fabric: "bit j set when lane j is a VALUE this
//       evaluation wrote and clear when it is a HOLE".
//
// AND THE ONE LAW THIS HEADER REFUSES TO STATE: nothing here says whether a
// cell is PASSABLE. compose_nav's own docstring is explicit — "A delta may make
// lawful ground cheaper or dearer. It may NOT make VOID, OUT, impossible slope
// or collision-blocked terrain walkable: hard passability is a separate truth
// and this value never speaks to it." Passability is decided by the column
// class (§3.2) and the collision normal (§4.4, ruling R1), by the service, from
// the same pick — never by a cost. A header that returned a "blocked" flag from
// an arithmetic this docstring forbids to speak to it would be the first step
// toward a negative delta opening a hole in the world.

#pragma once

#include <cstddef>
#include <cstdint>

#include "zref/zref_fieldir.hpp"
#include "zref/zref_terrain_patch.hpp"  // kMaxPatchFields, covers, FieldList

namespace zref {
namespace terrain {

/** One vertex of the composed navigation lattice. */
struct NavOut {
  int32_t cost_fx = 0;     // compose_nav's result, Q16.16 raw, >= 0 by its floor
  bool covered = false;    // at least one ACCEPTED lane's footprint covered this vertex (N1)
  int lanes_applied = 0;   // covering lanes that also PRESENTED an out-lane 3 (N2)
  int lanes_covering = 0;  // covering lanes, present or not — N1's count
};

/**
 * `nav_vertex` — the FIELD.WRITE.NAV reduction at ONE lattice vertex.
 *
 * `lane[i]` is out-lane 3 (nav_cost, Q16.16) of accepted field-list entry `i`
 * evaluated at this vertex — FIELD.SEQ.EARTH's job, by the one interpreter,
 * recorded by `compose_lattice`. `covers[i]` is
 * `zref::terrain::covers(list[i], wx, wz)`, the §9.1 closed-interval test,
 * decided once by the block that owns the field list. `present[i]` is whether
 * the program declared out-lane 3 at all; `nullptr` means "all present", which
 * is only correct for a caller that has already filtered.
 *
 * `authored_cost_fx` is the baseline the deltas act on: the cost the terrain
 * has with NO live field, which is the SERVICE's policy and deliberately not
 * this header's (see `zsim::NavPolicy`). compose_nav with zero deltas returns
 * it unchanged for any non-negative baseline, which is what makes the owner's
 * first acceptance item ("no-field results match the authored baseline") an
 * identity rather than a tolerance.
 *
 * The delta list is bounded by `kMaxPatchFields` because the §9.1 intake is:
 * a caller offering more has already violated the intake law upstream, and
 * truncating here would hide it. `n` is asserted against the bound by the
 * caller (the service), not silently clamped — a silent clamp is how a
 * seventeenth field becomes invisible instead of rejected-and-counted.
 */
inline NavOut nav_vertex(int32_t authored_cost_fx, const int32_t* lane, const bool* covers_in,
                         const bool* present, int n) {
  NavOut out;
  int32_t deltas[kMaxPatchFields];
  int m = 0;
  for (int i = 0; i < n; ++i) {
    if (covers_in != nullptr && !covers_in[i]) continue;  // N1: skipped, not zero
    out.covered = true;
    ++out.lanes_covering;
    if (present != nullptr && !present[i]) continue;  // N2: a HOLE, not a zero
    if (m < kMaxPatchFields) deltas[m++] = lane[i];
  }
  out.lanes_applied = m;
  // THE arithmetic, called. Command order is the order of this loop, which is
  // the order of the accepted field list, which is accepted-command order.
  out.cost_fx = fieldir::compose_nav(authored_cost_fx, deltas, static_cast<std::size_t>(m));
  return out;
}

}  // namespace terrain
}  // namespace zref
