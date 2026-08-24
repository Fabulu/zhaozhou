// zref_fieldir.hpp — the Field IR reference, under the name the ledger requires.
//
// A THIN VIEW ONTO AN EXISTING RATIFIED LAW, NOT A SECOND IMPLEMENTATION.
//
// `design/ops.yml` names FORTY `zref::fieldir::*` reference functions and not
// one of them exists. That is the largest single family of phantom references
// in this project. But the situation is not uniform, and treating it as one
// problem is why it has stayed open:
//
//   * `zref::fieldir::interpret` IS REAL under another name. `zfield::interpret`
//     in reference/include/zfield/zfield.hpp is the ONE generic Field IR
//     interpreter — the file's own words — mirrored by the TypeScript
//     interpreter and pinned by committed .zvec goldens and a tri-language
//     parity corpus. It is as well-established as anything in this tree.
//
//   * The PER-OP names (`stamp_max`, `sink_write_nav`, and so on) are a
//     different matter. Some are real laws living elsewhere: the five
//     FIELD.STAMP blends are `zref::surface::blend_apply`, now covered by
//     tests/differential/field_stamp_modes.cpp. Others name no behaviour at
//     all — FIELD.OUT.HEIGHT's own semantics line says "profile output map, no
//     dedicated opcode", so there is nothing for a function to do.
//
// This file forwards the one that is real. It deliberately does NOT invent
// wrappers for the rest: a wrapper around nothing would resolve the ledger's
// check while leaving the reference exactly as absent as before, which is the
// failure mode the phantom-reference rules exist to catch.
#pragma once

#include <cstddef>
#include <cstdint>

#include "zfield/zfield.hpp"

namespace zref {
namespace fieldir {

/** The decoded program, unchanged: `zfield::Decoded`. */
using Decoded = ::zfield::Decoded;

/** The interpreter's verdict, unchanged: `zfield::Status`. */
using Status = ::zfield::Status;

/**
 * FIELD.SEQ.EARTH's and FIELD.SEQ.WARP's reference: evaluate one decoded Field
 * IR program over its input lanes into its output lanes.
 *
 * "RTL matches the oracle" therefore means "RTL matches the interpreter the
 * TypeScript side mirrors and the committed .zvec goldens pin", rather than
 * matching something written beside the RTL by the same hand on the same day.
 */
inline Status interpret(const Decoded& prog, const int32_t* in, std::size_t n_in, int32_t* out,
                        std::size_t n_out) {
  return ::zfield::interpret(prog, in, n_in, out, n_out);
}

/** Load and fully validate a program image. */
inline ::zfield::DecodeResult decode(const uint8_t* bytes, std::size_t n) {
  return ::zfield::decode(bytes, n);
}

/* ---------------------------------------------------------------------------
 * THE THREE EARTH SINKS (owner ruling, docs/OWNER_DOCKET.md 2026-08-24 item 6)
 * ---------------------------------------------------------------------------
 * THE HEADER ABOVE REFUSED TO WRITE THESE, AND WAS RIGHT TO. Its words: "a
 * wrapper around nothing would resolve the ledger check while leaving the
 * reference exactly as absent as before". FIELD.WRITE.MATERIAL / NAV / HAZARD
 * were named in ops.yml and named no behaviour, so there was nothing to
 * forward to.
 *
 * The ruling supplies the behaviour, so these are an IMPLEMENTATION of a
 * decision rather than an invention of one. The distinction is the whole reason
 * the functions did not exist yesterday.
 *
 * ALL THREE ARE LIVE COMPOSITION, NEVER PERSISTENT MUTATION. When a field
 * expires the next composition reverts to authored state plus whatever remains
 * active. Persistence goes through TERRAIN.BAKE, SURFACE.STAMP or explicit
 * simulation commands -- a field evaluated every frame must never rewrite a
 * VRAM page every frame, which is the failure this separation exists to
 * prevent.
 */

/** Material state for one 32x32 terrain cell. */
struct MaterialState {
  uint8_t mat_a = 0;
  uint8_t mat_b = 0;
  uint8_t weight = 0;  // unit8, the A/B blend
};

/** One program's material write. `enabled` is the program's own predicate. */
struct MaterialWrite {
  bool enabled = false;
  MaterialState value{};
};

/**
 * MATERIAL: LAST ENABLED WRITER WINS, in accepted command order.
 *
 * Starts from authored layer E and never modifies it. Priority is expressed by
 * command ORDER, which is deliberate: hardware inventing an implicit material
 * hierarchy would be a game rule smuggled into silicon, and software can
 * already express any precedence it wants by choosing the order it submits.
 */
inline MaterialState compose_material(MaterialState authored, const MaterialWrite* writes,
                                      std::size_t n) {
  MaterialState out = authored;
  for (std::size_t i = 0; i < n; ++i)
    if (writes[i].enabled) out = writes[i].value;  // last enabled wins
  return out;
}

/**
 * NAV: SIGNED movement-cost deltas, combined by SATURATING ADDITION in command
 * order, then clamped at zero below.
 *
 * A delta may make lawful ground cheaper or dearer. It may NOT make VOID, OUT,
 * impossible slope or collision-blocked terrain walkable: hard passability is a
 * separate truth and this value never speaks to it. SW.CPUCOLL consumes the
 * identical composed number, so the CPU and the fabric cannot disagree about
 * what a tile costs.
 */
inline int32_t compose_nav(int32_t authored_cost, const int32_t* deltas, std::size_t n) {
  int64_t acc = authored_cost;
  for (std::size_t i = 0; i < n; ++i) {
    acc += deltas[i];
    if (acc > INT32_MAX) acc = INT32_MAX;
    if (acc < INT32_MIN) acc = INT32_MIN;
  }
  return acc < 0 ? 0 : static_cast<int32_t>(acc);  // cost floors at zero
}

/**
 * HAZARD: severities combine by MAX, never by sum or product.
 *
 * Two independent fields overlapping must not multiply the damage merely
 * because both happened to be active -- that is an emergent rule nobody chose.
 * A program wanting additive interaction computes it internally and writes one
 * value. Zero is neutral, so an inactive field contributes nothing.
 */
inline uint8_t compose_hazard(uint8_t authored, const int32_t* severities, std::size_t n) {
  int32_t best = static_cast<int32_t>(authored);
  for (std::size_t i = 0; i < n; ++i) {
    int32_t s = severities[i];
    if (s < 0) s = 0;                                   // non-negative by law
    if (s > (1 << 16)) s = 1 << 16;                     // clamp to 1.0 in Q16.16
    const int32_t as_u8 = (s * 255 + (1 << 15)) >> 16;  // round-half-up to u8
    if (as_u8 > best) best = as_u8;
  }
  return static_cast<uint8_t>(best);
}

}  // namespace fieldir
}  // namespace zref
