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

}  // namespace fieldir
}  // namespace zref
