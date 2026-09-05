// zref_material.hpp — TEXTURE.COMBINE's oracle (`zref::material::combine`),
// authored 2026-09-05 for roadmap gate G1-C.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS, AND WHY IT IS THE BLOCKING PIECE
// ---------------------------------------------------------------------------
// `design/contracts/TEXTURE.COMBINE.md` has said since 2026-09-03:
//
//     Reference: `zref::material::combine` — PLANNED AND NOT WRITTEN
//
// and `MATERIAL.RESOLVE.md` says why that matters more than a missing block
// usually does:
//
//   > The surviving TEXJOIN behaviour returns **sample 0 for every recipe**,
//   > and the three-sample terrain recipes were absent from that RTL entirely.
//   > So when this block starts returning real `sample_count` and `recipe`
//   > values, the combiner must exist to consume them — shipping the resolver
//   > alone would make the machine confidently fetch samples nothing combines.
//
// The texture island therefore fetches up to three samples per fragment today
// and combines none of them. Fetching three textures without implementing
// their combination is not three-sample material support, and terrain material
// work has been blocked on an organ nobody had written.
//
// This file is the scalar law. The RTL is still not built; when it is, it is
// differentiated against this.
//
// ---------------------------------------------------------------------------
// THE ONE ARITHMETIC RULE THAT MATTERS
// ---------------------------------------------------------------------------
// unit8 semantics, `spec/qformats.md` §2/§3: **value = raw/256, so 255 is the
// largest representable value and NOT 1.0.** A modulate by 255 therefore
// darkens very slightly. That is ratified behaviour and must not be "fixed".
//
// The product is `zref::unit_mul` — `((u32)a*b + 128) >> 8`, clamp 255 — and it
// is CALLED, never restated. The contract is explicit about why:
//
//   > A second unit8 multiply is the same defect class as the duplicated
//   > flat-shade law found earlier: two arithmetics that agree until they do
//   > not, with nothing to say which is right.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE DOES NOT DECIDE
// ---------------------------------------------------------------------------
// * **No fog.** Owner ruling D-5 places fog on the FINAL SOURCE COLOUR after
//   material combination — i.e. downstream of here — and the contract forbids
//   this block from pre-empting that ordering.
// * **No toon quantisation** (RASTER.TOON), **no framebuffer blend**
//   (RASTER.FRAGMENT: that combines the result with what is already there;
//   this combines sources with each other), **no sampling**, **no resolution**.
//
// ---------------------------------------------------------------------------
// ONE THING THE CONTRACT LEAVES OPEN, STATED RATHER THAN INVENTED
// ---------------------------------------------------------------------------
// The contract's overflow section says `sample_count == 0` "produces the
// fragment's vertex colour unchanged", but its In-packet list carries no
// vertex-colour field — the only non-sample colour in the packet is
// `has_aux / aux_rgb / aux_a`.
//
// This model therefore takes the untextured colour as an EXPLICIT parameter
// (`base`) rather than guessing which packet field carries it. When the RTL is
// written, whichever field it turns out to be is passed here and the two agree
// by construction. Inventing the binding in the oracle would hard-code a guess
// into the thing that is supposed to arbitrate.

#ifndef ZREF_MATERIAL_HPP
#define ZREF_MATERIAL_HPP

#include <cstdint>

#include "zref_fixp.hpp"

namespace zref {
namespace material {

// The eight ratified encodings (islandrearchitecture5.md §15.1). The numbering
// matches the constants already in `zhao_raster_texjoin_v2.sv` and
// `zhao_texture_fragrob.sv` so nothing is renumbered by this file's existence.
//
// 6 AND 7 WERE MISSING UNTIL 2026-09-05, and their absence was not harmless:
// this file REFUSED recipe 6 as illegal while §15.4's whole two-lane capacity
// argument is built on DETAIL_LIGHT being the worst case. Six recipes' worth of
// tests passed and reported coverage of the six they knew about. Recorded as
// docket D19q.
enum Recipe : uint8_t {
  kPassthru = 0,            // sample 0 unchanged
  kModulate = 1,            // s0 * s1
  kModulate2x = 2,          // s0 * s1 * 2, saturating
  kLerp = 3,                // lerp(s0, s1, weight)
  kAddSat = 4,              // s0 + s1, saturating
  kMask = 5,                // s0 where s1 passes, else transparent
  kTerrainDetailLight = 6,  // (s0 * s1) * s2, three samples
  kTerrainDetailMask = 7,   // (s0 * s1) RGB, alpha masked by s2
  kRecipeCount = 8
};

// ---------------------------------------------------------------------------
// HOW 6 AND 7 WERE DERIVED, AND THE ONE THING THAT IS STILL OPEN
// ---------------------------------------------------------------------------
// §15.1 names them and their sample counts; it does NOT write out their
// arithmetic. §15.3 gives the product-job counts, and those pin the shape:
//
//   DETAIL_LIGHT  3 first-layer RGB products + 3 second-layer RGB products
//   DETAIL_MASK   3 first-layer RGB products + 1 alpha product
//
// Six RGB products over three samples admits one composition: two chained
// per-channel products, ((s0 * s1) * s2). Three RGB plus one ALPHA product
// admits one too: an RGB product of s0 and s1, and a single alpha product --
// and §15.1's alpha sentence says which alpha, explicitly:
//
//   > Sample 0 owns base alpha EXCEPT WHERE THE RECIPE NAMES A MASK.
//
// DETAIL_MASK names a mask, so its alpha is the product s0.a * s2.a; that is
// the one alpha job. DETAIL_LIGHT names none, so its alpha is s0.a untouched,
// which is why it has no alpha job. Both readings are forced by the counts
// rather than chosen, which is the only reason they are written here at all.
//
// TWO ROUNDINGS, not one. §15.6 C2 says the pipeline captures product results,
// rounds exactly, and enqueues continuations -- so the second layer receives an
// already-rounded 8-bit intermediate. That is a structural consequence of the
// continuation queue, not a numeric preference.
//
// STILL OPEN, and deliberately not decided here: whether the DETAIL layer is
// the plain product or MODULATE2X. Detail maps are classically 2x so that 128
// is neutral, but §15.3 counts both layers as ordinary "RGB products" and
// MODULATE2X exists as its own recipe with its own rounding. The conservative
// reading -- plain `unit_mul` for both layers -- is implemented, and it is one
// line to change in each of the two cases below if the owner rules otherwise.
// Flagged in docket D19q rather than silently settled.

struct Sample {
  uint8_t r = 0, g = 0, b = 0, a = 0;
};

// Every refusal and every saturation is COUNTED. The contract's reasoning:
// "quietly accepting an unknown one is how a content bug becomes a shipped
// picture nobody questions", and the saturation counts "tell content authors
// when a recipe is clipping constantly".
struct Ledger {
  uint32_t refused_unknown_recipe = 0;
  uint32_t refused_missing_sample = 0;
  uint32_t saturated_add = 0;
  uint32_t saturated_mul2x = 0;
};

struct Out {
  uint8_t r = 0, g = 0, b = 0, a = 0;
  uint16_t frag_tag = 0;
  bool refused = false;  // the fragment was malformed; see the Ledger for which
};

// How many samples each recipe REQUIRES. A recipe naming more samples than
// `count` supplied is malformed and refused -- not silently degraded to
// passthrough, which is exactly what the surviving TEXJOIN does today and why a
// wrong material looks plausible.
constexpr uint8_t samples_required(uint8_t recipe) {
  if (recipe == kPassthru) return 1;
  if (recipe == kTerrainDetailLight || recipe == kTerrainDetailMask) return 3;
  return 2;
}

// Product jobs this recipe costs, by §15.3. The combiner's scheduler is sized
// from these and §15.4 requires them counted per recipe at run time, so the
// 80%-capacity question is answered from a trace rather than from arithmetic.
// PASSTHRU and ADD_SAT are bypasses and cost nothing.
constexpr uint8_t product_jobs(uint8_t recipe) {
  switch (recipe) {
    case kPassthru:
    case kAddSat:
      return 0;
    case kMask:
      // ZERO, and it used to say 1 "one alpha product". AUDIT R15.
      //
      // MASK does not multiply anything. Its implementation below is a binary
      // gate -- `if (s1.a != 0)` copy s0 through, else emit transparent -- so
      // there is no product to count. The label described an operation the
      // arithmetic does not perform.
      //
      // Everything else already agreed on zero and only this disagreed: the
      // combine implementation, the RTL (a composed run measures
      // `0 32 32 32 0 0 48 32`, which is zero for MASK across eight fragments),
      // and the composed test's expectation. A cost model that overstates a
      // recipe by one product per fragment feeds S15.4's 80%-capacity argument
      // a number the hardware never issues.
      return 0;
    case kModulate:
    case kModulate2x:
    case kLerp:
      // FOUR, and §15.3's table says three. The table counts RGB only, but the
      // ratified arithmetic multiplies ALPHA too -- so the fourth product
      // exists whatever the table says, and the only question was whether it
      // ran in a lane or in parallel silicon beside them. Computing it at
      // acceptance is what kept the RTL over its DSP budget; as a microjob it
      // shares a lane. §15.4 asks for ACTUAL jobs by recipe, so this reports
      // what the hardware issues and the difference from §15.3 is visible
      // rather than reconciled away.
      return 4;
    case kTerrainDetailMask:
      return 4;  // 3 first-layer RGB + 1 alpha
    case kTerrainDetailLight:
      return 6;  // 3 first-layer RGB + 3 second-layer RGB -- the worst case
    default:
      return 0;
  }
}

namespace detail {

// unit8 saturating add. Saturation is a real event, so it is reported.
constexpr uint8_t add_sat(uint8_t a, uint8_t b, bool* saturated) {
  const uint32_t s = static_cast<uint32_t>(a) + b;
  if (s > 255) {
    *saturated = true;
    return 255;
  }
  return static_cast<uint8_t>(s);
}

// s0*s1*2 with ONE rounding, then saturate. Doubling AFTER the frozen product
// keeps a single rounding per result: `unit_mul` already rounds half-up, and
// rounding twice would drift from the RTL by a least-significant bit on exactly
// the values a directed test is least likely to try.
constexpr uint8_t mul2x(uint8_t a, uint8_t b, bool* saturated) {
  const uint32_t p = static_cast<uint32_t>(unit_mul(unit8{a}, unit8{b})) * 2u;
  if (p > 255) {
    *saturated = true;
    return 255;
  }
  return static_cast<uint8_t>(p);
}

// lerp(a, b, w) in unit8. w == 0 gives a, w == 255 gives *almost* b -- which is
// the unit8 law showing through and is correct: 255 is not 1.0. Written as
// a + (b-a)*w on a SIGNED difference, matching the architecture's
// "3 signed difference x weight products" for LERP, with one rounding.
constexpr uint8_t lerp8(uint8_t a, uint8_t b, uint8_t w) {
  const int32_t d = static_cast<int32_t>(b) - static_cast<int32_t>(a);
  // (|d| * w + 128) >> 8 is unit_mul's law on a magnitude; the sign is
  // reapplied afterwards so the rounding is symmetric about zero rather than
  // biased toward +inf on darkening lerps.
  const uint32_t mag = static_cast<uint32_t>(d < 0 ? -d : d);
  const uint32_t scaled = (mag * static_cast<uint32_t>(w) + 128u) >> 8;
  const int32_t r = static_cast<int32_t>(a) +
                    (d < 0 ? -static_cast<int32_t>(scaled) : static_cast<int32_t>(scaled));
  return static_cast<uint8_t>(r < 0 ? 0 : (r > 255 ? 255 : r));
}

}  // namespace detail

/**
 * The scalar law for TEXTURE.COMBINE.
 *
 * @param recipe     one of the six ratified encodings; anything else is refused
 * @param weight     unit8 blend weight, used by kLerp only
 * @param s          up to three samples, index 0 first
 * @param count      how many of `s` are valid (0..3)
 * @param base       the untextured colour used when `count == 0` (see header)
 * @param frag_tag   rides through UNTOUCHED; retirement order depends on it
 * @param L          optional ledger; refusals and saturations are counted here
 */
inline Out combine(uint8_t recipe, uint8_t weight, const Sample* s, uint8_t count, Sample base,
                   uint16_t frag_tag, Ledger* L = nullptr) {
  Out o;
  o.frag_tag = frag_tag;  // set FIRST, so every early return preserves it

  // An untextured surface is legal and common, and must not require a dummy
  // sample. This is checked before the recipe, because a fragment with no
  // samples has nothing for any recipe to act on.
  if (count == 0) {
    o.r = base.r;
    o.g = base.g;
    o.b = base.b;
    o.a = base.a;
    return o;
  }

  if (recipe >= kRecipeCount) {
    if (L) L->refused_unknown_recipe++;
    o.refused = true;
    return o;
  }

  if (count < samples_required(recipe)) {
    if (L) L->refused_missing_sample++;
    o.refused = true;
    return o;
  }

  const Sample& s0 = s[0];
  const Sample& s1 = (count > 1) ? s[1] : s[0];
  const Sample& s2 = (count > 2) ? s[2] : s0;

  switch (recipe) {
    case kPassthru:
      o.r = s0.r;
      o.g = s0.g;
      o.b = s0.b;
      o.a = s0.a;
      break;

    case kModulate:
      o.r = unit_mul(unit8{s0.r}, unit8{s1.r});
      o.g = unit_mul(unit8{s0.g}, unit8{s1.g});
      o.b = unit_mul(unit8{s0.b}, unit8{s1.b});
      o.a = unit_mul(unit8{s0.a}, unit8{s1.a});
      break;

    case kModulate2x: {
      bool sat = false;
      o.r = detail::mul2x(s0.r, s1.r, &sat);
      o.g = detail::mul2x(s0.g, s1.g, &sat);
      o.b = detail::mul2x(s0.b, s1.b, &sat);
      o.a = detail::mul2x(s0.a, s1.a, &sat);
      if (sat && L) L->saturated_mul2x++;  // once per FRAGMENT, not per channel
      break;
    }

    case kLerp:
      o.r = detail::lerp8(s0.r, s1.r, weight);
      o.g = detail::lerp8(s0.g, s1.g, weight);
      o.b = detail::lerp8(s0.b, s1.b, weight);
      o.a = detail::lerp8(s0.a, s1.a, weight);
      break;

    case kAddSat: {
      bool sat = false;
      o.r = detail::add_sat(s0.r, s1.r, &sat);
      o.g = detail::add_sat(s0.g, s1.g, &sat);
      o.b = detail::add_sat(s0.b, s1.b, &sat);
      o.a = detail::add_sat(s0.a, s1.a, &sat);
      if (sat && L) L->saturated_add++;
      break;
    }

    case kMask:
      // "s0 where s1 passes, else transparent". The pass test is s1's ALPHA
      // being non-zero: MASK exists so a shape's alpha can cut a colour, and
      // testing the RGB instead would make a black-but-opaque mask erase what
      // it was meant to keep.
      if (s1.a != 0) {
        o.r = s0.r;
        o.g = s0.g;
        o.b = s0.b;
        o.a = s0.a;
      } else {
        o.r = o.g = o.b = 0;
        o.a = 0;
      }
      break;

    case kTerrainDetailLight: {
      // ((s0 * s1) * s2) per channel: base, detail layer, then light. Two
      // roundings, because §15.6 C2 rounds the first-layer result before the
      // continuation consumes it -- the intermediate on the wire is 8 bits.
      const uint8_t l0r = unit_mul(unit8{s0.r}, unit8{s1.r});
      const uint8_t l0g = unit_mul(unit8{s0.g}, unit8{s1.g});
      const uint8_t l0b = unit_mul(unit8{s0.b}, unit8{s1.b});
      o.r = unit_mul(unit8{l0r}, unit8{s2.r});
      o.g = unit_mul(unit8{l0g}, unit8{s2.g});
      o.b = unit_mul(unit8{l0b}, unit8{s2.b});
      // No alpha job: this recipe names no mask, so sample 0 owns base alpha.
      o.a = s0.a;
      break;
    }

    case kTerrainDetailMask: {
      // (s0 * s1) on RGB, and the ONE alpha product that makes the third
      // sample a mask. This recipe does name a mask, so §15.1's exception
      // applies and the alpha is a product rather than s0's.
      o.r = unit_mul(unit8{s0.r}, unit8{s1.r});
      o.g = unit_mul(unit8{s0.g}, unit8{s1.g});
      o.b = unit_mul(unit8{s0.b}, unit8{s1.b});
      o.a = unit_mul(unit8{s0.a}, unit8{s2.a});
      break;
    }

    default:
      // Unreachable: the range check above already refused everything else.
      // Present so a future added encoding cannot fall through silently.
      if (L) L->refused_unknown_recipe++;
      o.refused = true;
      break;
  }

  return o;
}

}  // namespace material
}  // namespace zref

#endif  // ZREF_MATERIAL_HPP
