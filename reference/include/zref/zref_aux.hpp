// zref_aux.hpp — TEXTURE.AUX's oracle (`zref::AuxSource`), authored 2026-08-19.
//
// ---------------------------------------------------------------------------
// THIS ORACLE DID NOT EXIST, AND THAT IS THE FIRST THING TO SAY
// ---------------------------------------------------------------------------
// design/blocks.yml names `reference_model: zref::AuxSource` for TEXTURE.AUX.
// Checked 2026-08-19: the string `aux` appears NOWHERE under reference/ — no
// header, no .cpp, no struct, no comment — and the two test files the ledger
// points at (tests/texture/texture_aux_{directed,random}.cpp) did not exist
// either. The ledger promised an oracle nobody had written. This file is it.
//
// So the three layers of this header, in the shape zref_surface.hpp set:
//
//   (1) FOUND and already EXECUTED. `zref::render::sample_sheet`
//       (reference/src/zrender/terrain.cpp) is the machine's ratified
//       world -> sheet-texel mapping, called every frame by the software
//       console's terrain path. Its arithmetic is reproduced here EXACTLY —
//       the s64 intermediates, the *64 before the divide, the C++ truncating
//       division, the independent per-axis clamp to [0,63], and the
//       degenerate-envelope early return of 0. Nothing is reinterpreted.
//
//   (2) FOUND but stated only in prose. ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER
//       15 lists the restricted auxiliary source's four uses — terrain
//       surface sheet, light/mask map, shadow compare, distortion map — and
//       ends "It must not become a second unrestricted full TMU."
//       spec/terrain_rules.md 6.5 ratifies the budget: "ONE aux consumer on
//       terrain fragments, because tint moved to vertices." Charter 26 puts
//       "auxiliary filtering" at cut-order 2 and "terrain surface sheets" on
//       the NEVER CUT list. Those four facts together are the whole written
//       law about this block, and they are quoted at THE RESTRICTION below.
//
//   (3) CHOSEN, because nothing in the tree says it. The request/response
//       shape, the {tag, strength} pair (sample_sheet returns strength ONLY,
//       and terrain_rules 6.5 asks for "tag/strength effects" — plural), and
//       what a non-resident sheet answers. Each is argued below and again in
//       design/contracts/TEXTURE.AUX.md and fpga/rtl/texture/zhao_texture_aux.sv.
//
// ---------------------------------------------------------------------------
// CHARTER 29-6: THE SEMANTICS ARE NOT IMPLEMENTED TWICE
// ---------------------------------------------------------------------------
// `axis_texel` below is the per-axis half of `sample_sheet`, extracted so a
// hardware block can be differentiated against it one axis at a time. It is a
// VIEW, not a second law, and tests/texture/texture_aux_directed.cpp proves
// the view faithful before anything else runs: it drives a real
// `zref::render::SurfaceSheet` through `sample_sheet` and through
// `axis_texel` over a sweep of world positions and envelopes and asserts they
// agree on every one. If that first lane is red, nothing after it means
// anything. (Exactly the discipline tests/surface/surface_stamp_directed.cpp
// applies to `zref::surface`'s view of `stamp_surface`.)

#pragma once

#include <cstdint>

namespace zref {
namespace aux {

/** Layer F is 64x64 texels (charter 12, spec/terrain_rules.md 2). */
inline constexpr int kSheetDim = 64;

/**
 * ONE AXIS of `zref::render::sample_sheet`'s world -> texel mapping, verbatim:
 *
 *     int64_t i = ((int64_t)w - e0) * 64 / (e1 - e0);
 *     if (i < 0) i = 0;
 *     if (i > 63) i = 63;
 *
 * `w`, `e0`, `e1` are fx16 raw words (Q16.16 world metres). Returns 0..63.
 *
 * FOUR PROPERTIES OF THIS EXPRESSION THAT LOOK LIKE ACCIDENTS AND ARE NOT.
 * They are kept because `sample_sheet` is executed by the software console
 * today and pinned by committed captures; changing any of them moves every
 * scar the player can see.
 *
 *   1. THE DIVISION TRUNCATES TOWARD ZERO, not toward minus infinity. C++ `/`
 *      on integers is truncation, so for w just left of the envelope the
 *      quotient is 0 rather than -1. It does not matter to the RESULT here —
 *      every negative quotient clamps to 0 anyway — but it matters to anyone
 *      re-deriving the expression, and it is the reason a hardware
 *      implementation may treat "numerator < 0" as "answer 0" without a
 *      floor/truncate correction at all.
 *   2. THE *64 HAPPENS BEFORE THE DIVIDE, in s64. Dividing first and scaling
 *      after would quantise to whole envelopes.
 *   3. THE CLAMP IS PER AXIS AND INDEPENDENT. A world position outside the
 *      envelope in x but inside in z samples the edge column at the correct
 *      row — it does not fall back to a corner or to zero.
 *   4. THERE IS NO HALF-TEXEL BIAS AND NO ROUNDING. This is a FLOOR across the
 *      envelope, and it is deliberately NOT the texel-CENTRE rule
 *      `stamp_surface` uses on the write side (wx = ex0 + (ex1-ex0)*(2i+1)/128).
 *      The two mappings are different on purpose and
 *      design/contracts/SURFACE.STAMP.md says so: "a u/v transposition or a
 *      dropped 64x survives both standalone suites and dies there".
 */
inline int32_t axis_texel(int32_t w, int32_t e0, int32_t e1) {
  if (e1 <= e0) return 0;  // degenerate: sample_sheet's early return
  int64_t i = (static_cast<int64_t>(w) - static_cast<int64_t>(e0)) * 64 /
              (static_cast<int64_t>(e1) - static_cast<int64_t>(e0));
  if (i < 0) i = 0;
  if (i > 63) i = 63;
  return static_cast<int32_t>(i);
}

/** The patch envelope, fx16 world metres (Island Patch header, terrain_rules 2.1). */
struct Envelope {
  int32_t x0 = 0, z0 = 0, x1 = 0, z1 = 0;
  bool degenerate() const { return x1 <= x0 || z1 <= z0; }
};

/** One aux sample: BOTH layer-F bytes, plus the texel they came from. */
struct Sample {
  uint8_t tag = 0;
  uint8_t strength = 0;
  uint8_t u = 0;            // 0..63
  uint8_t v = 0;            // 0..63
  bool degenerate = false;  // the envelope was degenerate; nothing was read
  bool miss = false;        // the sheet was not resident; nothing was read
  bool operator==(const Sample& o) const {
    return tag == o.tag && strength == o.strength && u == o.u && v == o.v &&
           degenerate == o.degenerate && miss == o.miss;
  }
};

/**
 * THE RESTRICTION, and why it is STRUCTURAL rather than a comment.
 *
 * Charter 15 lists four uses for the restricted auxiliary source and charter
 * 26 refuses "a second unrestricted full TMU". design/contracts/TEXTURE.TMU.md
 * already read that refusal correctly for the primary side — "every sampling
 * mode the machine will ever have has to fit through this request channel,
 * because there is nowhere else for one to live" — and made nearest a special
 * case of bilinear rather than a second path. The aux side needs the mirror
 * image of that reasoning, and it is this:
 *
 *   THIS SOURCE RETURNS BYTES AND NEVER INTERPRETS THEM.
 *
 * All four of charter 15's uses are "read a byte pair out of a resident 64x64
 * page at a world position":
 *   · terrain surface sheet — layer F's {tag, strength} (terrain_rules 6.5);
 *   · light/mask map        — the strength byte IS the mask;
 *   · shadow compare        — the COMPARE belongs to RASTER.FRAGMENT, which
 *                             already owns a threshold test (its contract's
 *                             "the alpha test is an INDEX test"). A comparator
 *                             here would be the first component of a second
 *                             sampler;
 *   · distortion map        — the offset arithmetic belongs to whoever
 *                             perturbs a coordinate, not to the fetch.
 *
 * So there is no mode word, no format decoder, no palette, no mip selector and
 * NO FILTER. That last one settles charter 26's cut order by construction:
 * "auxiliary filtering" is cut-order 2, and this source has none to cut, while
 * "terrain surface sheets" is on the NEVER CUT list and survives — both halves
 * of 26 satisfied without a single conditional.
 *
 * REJECTED ALTERNATIVE: a TMU-shaped mode word with a format field, a filter
 * bit and per-axis wrap. It is the natural design and it is exactly what 26
 * forbids: every bit in such a word is a bit somebody later fills in, and the
 * block becomes the second unrestricted TMU one field at a time.
 *
 * CHOSEN, with the rejected alternative, because nothing states them:
 *
 *   A1. BOTH LAYER-F BYTES ARE RETURNED, not just strength. `sample_sheet`
 *       returns strength alone because its ONE caller (terrain.cpp's
 *       `sheet_factor`) needs only that; terrain_rules 6.5 says the aux lane
 *       exists "for tag/strength effects", plural, and charter 12 spends both
 *       bytes. SURFACE.SHEET's read port already hands back both.
 *       REJECTED: strength only, matching `sample_sheet`'s signature — it
 *       would make the tag byte unreachable by any hardware path at all, so
 *       layer F would carry a byte nothing in the machine could ever read.
 *
 *   A2. A NON-RESIDENT SHEET READS AS ZERO and raises `miss`, rather than
 *       stalling until residency. SURFACE.SHEET's own found law is that "a
 *       sheet which has never been stamped reads as ZERO everywhere", so an
 *       absent sheet reading zero is the same picture as an unstamped one —
 *       no scar, which is the truthful answer.
 *       REJECTED: stalling the fragment until the handle becomes resident. The
 *       fragment path cannot wait on a residency fill: SURFACE.SHEET never
 *       evicts (its chosen law C2), so a handle that is not resident may never
 *       become resident, and the stall would be permanent.
 *
 *   A3. THE ENVELOPE RIDES THE REQUEST rather than living in a register file.
 *       Duo draws two views of the same world and the terrain streams many
 *       patches per frame, so a resident envelope register would need a patch
 *       id, a write port and an invalidate — three things the ledger does not
 *       give this block.
 *       REJECTED: an envelope register file addressed by patch handle. It is
 *       the right shape once a patch-descriptor cache exists; today it would
 *       be a cache with one client and no filler.
 */
struct AuxSource {
  /**
   * The whole block, scalar. `tags`/`strengths` are the resident 64x64 layer-F
   * planes in j*64 + i scan order; `resident` false models SURFACE.SHEET
   * answering ST_MISS.
   */
  static Sample sample(const Envelope& e, int32_t wx, int32_t wz, const uint8_t* tags,
                       const uint8_t* strengths, bool resident) {
    Sample s;
    if (e.degenerate()) {
      s.degenerate = true;  // sample_sheet returns 0 and reads nothing
      return s;
    }
    s.u = static_cast<uint8_t>(axis_texel(wx, e.x0, e.x1));
    s.v = static_cast<uint8_t>(axis_texel(wz, e.z0, e.z1));
    if (!resident) {
      s.miss = true;  // A2: zero, never stale
      return s;
    }
    const int idx = static_cast<int>(s.v) * kSheetDim + static_cast<int>(s.u);
    s.tag = tags[idx];
    s.strength = strengths[idx];
    return s;
  }
};

}  // namespace aux

// The ledger spells the model `zref::AuxSource`; the implementation lives in
// `zref::aux` beside the rest of the block's vocabulary, and this alias makes
// the ledger's spelling resolve to a real symbol rather than to nothing.
using AuxSource = aux::AuxSource;

}  // namespace zref
