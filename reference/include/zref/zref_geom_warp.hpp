// zref_geom_warp.hpp -- zref::GeomWarp, the scalar reference for GEOM.WARP.
//
// AUTHORITY. Owner directive reports/Zhaozhou_GEOM_WARP_Architecture_2026-09-20.txt
// (owner commit 4c256137), decisions W01-W03, W10, W17 and section 21.1.
// Ratified 2026-09-20; see reports/OWNER-RATIFICATION-20260920-WARP.md. The
// ledger has named `zref::GeomWarp` as GEOM.WARP's reference_model since the
// block was specified (design/blocks.yml) and no such symbol existed -- it is
// item 9 of reports/PHANTOM_REFERENCES.md. This file is that symbol.
//
// WHAT THIS FILE IS NOT. It contains NO opcode switch, no interpreter, no
// second arithmetic library, and no normal-deformation law of its own.
// Directive 21.1: "Its implementation should be a composition of existing
// semantics... It does not contain an opcode switch. zfield::interpret /
// zfield_steps remain the arithmetic authorities."
//
// Everything below is therefore one of three things, and each is labelled:
//   (a) a call into zfield::  -- the Field program's own arithmetic;
//   (b) a call into zref::    -- the project's existing fixed-point law;
//   (c) a COMPOSITION rule stated by the directive, cited by section number.
//
// THE OPERATION (directive section A, W02, W03):
//
//   GEOM.WARP is VIEW-INDEPENDENT, POST-SKIN, PRE-LIGHT/PRE-PROJECTION. It
//   submits a fifteen-lane record to the ONE shared Field v3 fabric, adds the
//   returned displacement to the skinned WORLD position, adopts the returned
//   normal, and publishes one coherent vertex.
//
//   Pout = componentwise canonical saturating ADD(Pin, d)            [5.3]
//   Nout = the program's three normal words, range-reduced for LIGHT [5.4]
//
//   There is NO Jacobian, no finite differencing, no extra normalization, no
//   blend with the old normal, and no amplitude multiplier. W03 forbids each
//   by name. A program that wants a unit normal uses the Field's explicit
//   NORMALIZE3; a program that deforms the surface AUTHORS its normal change.
//   Directive 5.4: "The adapter does not infer derivatives from three position
//   samples and does not claim arbitrary deformations preserve surface
//   normals."
//
// THE FIFTEENTH LANE (W01). spec/form/field-ir.md section 7.1 lists fifteen
// Warp input fields and then says "(14)". The FIELDS are right; the COUNT was
// wrong, and the error had already propagated into fpga/rtl/prod/
// zhao_console_core.sv, which said the profile "is 14 in" and that the host
// pair "moves to 14/7". Sizing the lane bus to 14 drops p3 -- and a dropped
// lane does not read as absent, it reads as whatever the host's register clear
// left behind. `kInLanes` is 15 here and `check_signature` refuses 14, so the
// mistake cannot be re-made silently. Fixture P3_SENTINEL (directive 21.3)
// exists for exactly this.
//
// STATUS IS NOT A BOOLEAN (directive 5.5). Canonical Field saturation and
// reciprocal-of-zero have DEFINED numeric answers; they are not malformed
// programs. This file keeps three status families apart:
//   * `field_status`   -- zfield::Status{sat, rcp0}, the program's own numerics;
//   * `app_saturated`  -- the APPLICATION add saturated (5.3: "Keep
//                         application-add saturation distinct from
//                         Field-program saturation in the instrumentation");
//   * `refusal`        -- a transport/admission fault that POISONS.
// A caller that collapses these loses the distinction the directive spends
// section 5.5 protecting.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "zfield/zfield.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_sat.hpp"

namespace zref {
namespace geom_warp {

// ---------------------------------------------------------------- constants --

/** W01: FIFTEEN input lanes. Not fourteen. See the header note. */
inline constexpr std::size_t kInLanes = 15;

/** Six output lanes: dx,dy,dz, nx',ny',nz' (directive 5.2). */
inline constexpr std::size_t kOutLanes = 6;

/** zfield::Profile::WARP. Named so the comparison below reads as a law. */
inline constexpr std::uint8_t kWarpProfile = static_cast<std::uint8_t>(zfield::WARP);

/**
 * The range-reduction ceiling, from `zref::skin_world_normal`
 * (reference/src/zcreature/creature_core.cpp): shift the three components
 * together until max(abs(component)) < 2^30.
 *
 * This is NOT a new constant. It is the same 1<<30 that block already uses,
 * named here so the two sites cannot drift apart. Directive 5.4 requires "the
 * SAME common arithmetic-right-shift range reduction used by
 * skin_world_normal".
 */
inline constexpr std::int64_t kNormalReduceCeiling = std::int64_t{1} << 30;

/**
 * At signed-32 input width the reduction needs AT MOST TWO shifts, and the
 * directive says so at 5.4 ("A registered shift-count select or two short
 * stages suffices; it is not a new sqrt/divider").
 *
 * That is a claim about the hardware's cost, so it is worth checking rather
 * than repeating: the largest magnitude a signed 32-bit word can hold is
 * 2^31 (INT32_MIN). 2^31 >> 1 == 2^30, which is still >= the ceiling;
 * 2^30 >> 1 == 2^29, which is below it. Two shifts, exactly, worst case.
 * `reduce_normal` reports the count it actually used and
 * geom_warp_reference_directed asserts it never exceeds this.
 */
inline constexpr std::uint8_t kMaxNormalShifts = 2;

// ------------------------------------------------------------------ records --

/**
 * The canonical fifteen-lane input record, in the SEMANTIC order of directive
 * 5.1 -- which is the lane table, NOT this struct's incidental memory layout
 * (directive 21.2 is explicit about that). `pack_record` is the only thing
 * permitted to turn one into the other.
 *
 *   lane 0..2   px,py,pz   GEOM.SKIN world position, fx (Q16.16)
 *   lane 3..5   nx,ny,nz   GEOM.SKIN.NORM reduced direction, fx, NOT unit
 *   lane 6..9   a0..a3     INLINE4 or STREAM4 attributes, fx
 *   lane 10     time       u32 tick from THIS DrawWarpedForm, not a clock read
 *   lane 11..14 p0..p3     THIS DrawWarpedForm's parameters, fx
 */
struct Inputs {
  std::int32_t position[3] = {0, 0, 0};
  std::int32_t direction[3] = {0, 0, 0};  ///< W02: reduced, NON-unit world direction
  std::int32_t attributes[4] = {0, 0, 0, 0};
  std::int32_t params[4] = {0, 0, 0, 0};
  std::uint32_t time = 0;
};

/**
 * The per-draw declaration a DrawWarpedForm carries (W05, W11). The bound is
 * componentwise, world-space, and NONNEGATIVE; it is checked against the
 * RETURNED displacement (directive 5.6), never used to clamp it.
 */
struct Declaration {
  std::int32_t displacement_bound[3] = {0, 0, 0};
};

/** Why an application produced no publishable vertex. */
enum class Refusal : std::uint8_t {
  kNone = 0,
  kProfileMismatch,        ///< T02: an Earth/Flow program is not Warp
  kInputLaneCount,         ///< T01: W01's fifteen, refused explicitly at 14
  kOutputLaneCount,        ///< not six mapped outputs
  kNegativeBound,          ///< 6.3: bounds must be nonnegative
  kBoundViolation,         ///< 5.6 / W11: |d[k]| > bound[k]; POISON the meshlet
  kNormalInputWidthFault,  ///< 5.4: s64 input did not fit s32; a SEAM fault
};

inline const char* refusalName(Refusal r) {
  switch (r) {
    case Refusal::kNone: return "none";
    case Refusal::kProfileMismatch: return "profile-mismatch";
    case Refusal::kInputLaneCount: return "input-lane-count";
    case Refusal::kOutputLaneCount: return "output-lane-count";
    case Refusal::kNegativeBound: return "negative-bound";
    case Refusal::kBoundViolation: return "bound-violation";
    case Refusal::kNormalInputWidthFault: return "normal-input-width-fault";
  }
  return "?";
}

/**
 * One vertex's outcome.
 *
 * `displacement` is reported ALONGSIDE the applied position deliberately: a
 * bound violation is diagnosed from what the program RETURNED, and directive
 * 5.6 forbids clamping it and "pretending the program computed that clamp".
 * Keeping the raw value means the evidence survives the refusal.
 */
struct Result {
  std::int32_t position[3] = {0, 0, 0};
  std::int32_t direction[3] = {0, 0, 0};     ///< reduced REPLACEMENT normal
  std::int32_t displacement[3] = {0, 0, 0};  ///< as returned, pre-application
  bool degenerate = false;                   ///< all three normal words zero (5.4)
  zfield::Status field_status{};             ///< (a) the PROGRAM's numerics
  bool app_saturated = false;                ///< (c) the APPLICATION add saturated
  std::uint8_t normal_shifts = 0;            ///< how many common shifts 5.4 needed
  Refusal refusal = Refusal::kNone;

  /** W10: a poisoned outcome must never reach visible replay. */
  constexpr bool poison() const { return refusal != Refusal::kNone; }
};

// -------------------------------------------------------------- lane packing --

/**
 * (c) Directive 5.1. Pack the semantic record into the canonical lane order.
 * The program's canonical input registers are R0..R14; a prepared FPLAN may
 * map them elsewhere, and that is the shared planner's job, not an alternative
 * semantic record.
 */
inline void pack_record(const Inputs& in, std::int32_t out[kInLanes]) {
  out[0] = in.position[0];
  out[1] = in.position[1];
  out[2] = in.position[2];
  out[3] = in.direction[0];
  out[4] = in.direction[1];
  out[5] = in.direction[2];
  out[6] = in.attributes[0];
  out[7] = in.attributes[1];
  out[8] = in.attributes[2];
  out[9] = in.attributes[3];
  // `time` is u32 tick DATA. It is bit-cast, not converted: directive 5.1 calls
  // it "u32 tick data, not a wall-clock read", and a lane is 32 bits either way.
  out[10] = static_cast<std::int32_t>(in.time);
  out[11] = in.params[0];
  out[12] = in.params[1];
  out[13] = in.params[2];
  out[14] = in.params[3];
}

// ----------------------------------------------------------- seam narrowing --

/**
 * (c) Directive 5.4. GEOM.SKIN.NORM exports signed 64-bit wires whose reduced
 * components fit signed 32 bits. "Capture the exact low word only AFTER
 * checking that sign extension reconstructs the full input. An out-of-range
 * input is a seam fault, not permission to truncate it."
 *
 * So this returns false rather than narrowing, and the caller raises
 * kNormalInputWidthFault. A silent truncation here would be the flattering
 * failure: it produces a plausible small number from an impossible large one.
 */
inline bool narrow_s64_to_s32(std::int64_t v, std::int32_t* out) {
  const std::int32_t low = static_cast<std::int32_t>(v);
  if (static_cast<std::int64_t>(low) != v) return false;
  *out = low;
  return true;
}

/** Widened absolute magnitude. Safe at INT32_MIN, where -x would overflow. */
inline std::uint64_t abs_widened(std::int64_t v) {
  return (v < 0) ? (~static_cast<std::uint64_t>(v) + 1u) : static_cast<std::uint64_t>(v);
}

// ------------------------------------------------------- normal adaptation ---

/**
 * (b)+(c) The common range reduction, directive 5.4, and it is deliberately
 * the same loop as `zref::skin_world_normal`: shift all three components by
 * the same amount until max(abs) < 2^30.
 *
 * Applying ONE shift to ALL components changes the vector's MAGNITUDE and not
 * the DIRECTION it represents -- which is the whole reason this is legal, and
 * the reason the result stays a non-unit direction (W02). LIGHT owns the
 * magnitude; `zhao_light_stream`'s existing root computes it.
 *
 * Note this is an ARITHMETIC right shift on a signed value, which rounds
 * toward negative infinity, exactly as `skin_world_normal` does. Rounding it
 * half-up instead would be a second law that agrees until it doesn't.
 */
inline void reduce_normal(std::int64_t n[3], std::uint8_t* shifts_out) {
  std::uint8_t shifts = 0;
  for (;;) {
    const std::uint64_t mx = std::max(std::max(abs_widened(n[0]), abs_widened(n[1])),
                                      abs_widened(n[2]));
    if (mx < static_cast<std::uint64_t>(kNormalReduceCeiling)) break;
    n[0] >>= 1;
    n[1] >>= 1;
    n[2] >>= 1;
    ++shifts;
  }
  if (shifts_out != nullptr) *shifts_out = shifts;
}

// ------------------------------------------------------------ the operation --

/**
 * (c) Directive 6.3 / T01 / T02. Does this decoded program actually present
 * the Warp signature?
 *
 * T01 requires that the "14-input legacy typo [is] refused with an explicit
 * signature diagnostic rather than reading past input storage" -- so this is
 * checked BEFORE any lane is read, and the count is compared to kInLanes
 * rather than to whatever the program happens to declare.
 */
inline Refusal check_signature(const zfield::Decoded& prog) {
  if (prog.profile != kWarpProfile) return Refusal::kProfileMismatch;
  if (prog.in_lanes.size() != kInLanes) return Refusal::kInputLaneCount;
  if (prog.out_lanes.size() != kOutLanes) return Refusal::kOutputLaneCount;
  return Refusal::kNone;
}

/**
 * (c) THE APPLICATION HALF, directive 5.3 / 5.4 / 5.6, over six output words
 * that the caller obtained from ANY conforming source -- the canonical
 * interpreter, a prepared FPLAN, or the RTL.
 *
 * Splitting it out is the point. Directive W13 insists the application and the
 * evaluation are separate claims; this lets a differential test feed the
 * HARDWARE's six words through the SAME application law and compare only the
 * half under test, instead of re-deriving the whole operation and hoping the
 * disagreement lands where you are looking.
 *
 * `out6` is {dx, dy, dz, nx', ny', nz'} in the canonical output order of 5.2.
 * Their PHYSICAL register locations come from the decoded program's output
 * map; they are NOT assumed to be contiguous, and this function never asks.
 */
inline Result apply_outputs(const std::int32_t out6[kOutLanes], const Inputs& in,
                            const Declaration& decl, zfield::Status field_status) {
  Result r{};
  r.field_status = field_status;
  for (int k = 0; k < 3; ++k) r.displacement[k] = out6[k];

  // --- 6.3: a nonnegative bound is a validity condition of the COMMAND -------
  for (int k = 0; k < 3; ++k) {
    if (decl.displacement_bound[k] < 0) {
      r.refusal = Refusal::kNegativeBound;
      return r;
    }
  }

  // --- 5.6: the bound applies to the RETURNED displacement -------------------
  // Widened absolute values, so INT32_MIN is diagnosed rather than wrapped.
  // On violation we POISON; we do NOT clamp. Directive 5.6: "Do not clamp
  // displacement to the bound and pretend the program computed that clamp."
  for (int k = 0; k < 3; ++k) {
    if (abs_widened(out6[k]) > abs_widened(decl.displacement_bound[k])) {
      r.refusal = Refusal::kBoundViolation;
      return r;
    }
  }

  // --- 5.3: position is the canonical saturating ADD -------------------------
  // (b) zref::fx_add is the project's existing saturating fx16 add. A local
  // ledger is used so the APPLICATION's saturation is counted separately from
  // the Field program's, which 5.3 requires in so many words.
  zref::SatLedger app_ledger{};
  for (int k = 0; k < 3; ++k) {
    const zref::fx16 sum = zref::fx_add(zref::fx16{in.position[k]},
                                        zref::fx16{out6[k]}, &app_ledger);
    r.position[k] = sum.raw;
  }
  r.app_saturated = (app_ledger.total() != 0);

  // --- 5.4: the REPLACEMENT normal, adapted for LIGHT ------------------------
  // "detect zero iff all three words are zero" -- tested on the words the
  // program returned, BEFORE reduction, because reduction can only shrink them
  // and a vector that reduces to zero was already degenerate.
  if (out6[3] == 0 && out6[4] == 0 && out6[5] == 0) {
    r.degenerate = true;
    r.direction[0] = r.direction[1] = r.direction[2] = 0;
    r.normal_shifts = 0;
    return r;
  }
  std::int64_t n[3] = {static_cast<std::int64_t>(out6[3]), static_cast<std::int64_t>(out6[4]),
                       static_cast<std::int64_t>(out6[5])};
  reduce_normal(n, &r.normal_shifts);
  for (int k = 0; k < 3; ++k) {
    // The reduction guarantees abs < 2^30, so this narrowing cannot fail. It is
    // still CHECKED rather than cast: a guarantee nobody tests is a comment.
    if (!narrow_s64_to_s32(n[k], &r.direction[k])) {
      r.refusal = Refusal::kNormalInputWidthFault;
      return r;
    }
  }
  return r;
}

/**
 * (a)+(c) THE WHOLE OPERATION, directive 21.1's seven steps in order:
 * validate the signature, assemble the exact fifteen-word record, call
 * zfield::interpret on the validated canonical program, compare the returned
 * displacement against the declared bound, apply the canonical saturating ADD,
 * adapt the output normal, and return output + numeric status +
 * application/transport classification.
 *
 * `ledger_out` is optional and forwards zfield's per-cause SatLedger, which
 * directive 12.4 requires the differential to compare lane by lane rather than
 * settling for the collapsed Status.
 */
inline Result apply(const zfield::Decoded& prog, const Inputs& in, const Declaration& decl,
                    zref::SatLedger* ledger_out = nullptr) {
  Result r{};
  const Refusal sig = check_signature(prog);
  if (sig != Refusal::kNone) {
    // Refuse BEFORE reading any lane (T01). Nothing is interpreted, so the
    // returned field_status is honestly empty rather than stale.
    r.refusal = sig;
    return r;
  }

  std::int32_t rec[kInLanes];
  pack_record(in, rec);

  std::int32_t out6[kOutLanes] = {0, 0, 0, 0, 0, 0};
  // (a) THE ARITHMETIC AUTHORITY. Every bend, twist, sine and noise in a Warp
  // program happens here and nowhere else in this file.
  const zfield::Status st = zfield::interpret(prog, rec, kInLanes, out6, kOutLanes, ledger_out);

  return apply_outputs(out6, in, decl, st);
}

// --------------------------------------------------------------- the façade --

/**
 * The name the ledger reserves (`design/blocks.yml`: `reference_model:
 * zref::GeomWarp`). It is a thin façade over the free functions above so that
 * the ledger's symbol resolves and so that callers can say what they mean.
 */
struct GeomWarp {
  static constexpr std::size_t in_lanes = kInLanes;
  static constexpr std::size_t out_lanes = kOutLanes;

  static Refusal signature(const zfield::Decoded& p) { return check_signature(p); }

  static Result evaluate(const zfield::Decoded& p, const Inputs& in, const Declaration& d,
                         zref::SatLedger* ledger = nullptr) {
    return apply(p, in, d, ledger);
  }

  static Result applyOutputs(const std::int32_t out6[kOutLanes], const Inputs& in,
                             const Declaration& d, zfield::Status st) {
    return apply_outputs(out6, in, d, st);
  }
};

}  // namespace geom_warp

/** The ledger's spelling, `zref::GeomWarp`. */
using GeomWarp = geom_warp::GeomWarp;

}  // namespace zref
