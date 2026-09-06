// MANAFOLD — THE EYE LAB (Owner Direction 7 §12). LANE-ONLY, SHIPS NOTHING.
//
// The eye CONSTRUCTION is settled and protected (§12 preamble: "I was quite
// happy with the modelling of the eyes"). Nothing here re-opens the symmetric
// pointed lens, the star unit, the white-from-cyan derivation or the deep
// purple. This file is about PLACEMENT, PROPORTION and LIFE:
//
//   1. TRAVEL   the whole eye slides from the Front.png placement toward the
//               flanks and back. kEyeTravelMaxDeg, 45 deg ABSOLUTE MAX, and
//               the owner has pre-emptively called the larger range
//               overzealous -- so this sweeps UPWARD FROM SMALL.
//   2. BLINK    the lens squashes vertically and springs back, star riding it
//               (§12.3), through the deform sidecar.
//   3. STAR     high in the lens as the sheet draws it (§12.1) and larger
//               (§12.2).
//
// ============================ THE TRAVEL MECHANISM =========================
//
// ⚠ TRAVEL IS NOT ROLL AND IT IS NOT THE UNSHIPPED SHIFT. Three different
// things, and two of them are already named in degrees:
//
//   kEyeRollMaxA16       4.9 deg   HOW THE EYE IS TURNED (§5d; a swept gate
//                                  found the lenses closing to under 1 mm at
//                                  7 deg, so this ceiling is measured, not
//                                  chosen). Untouched here.
//   kEyeShiftMaxPm       10% of the eye's own width. NOT SHIPPED.
//   kEyeTravelMaxDeg     WHERE THE EYE SITS. This file.
//
// WHY THE SHIPPED SHIFT COULD NOT SIMPLY BE SCALED UP -- and it is worse than
// "the pivot is unsound", which is how it is recorded today. The mechanism is
// HALF-WRITTEN, and the missing half is in the skeleton:
//
//   * make_eye_lens sets   rs.cx = fxu(kEyeShiftPivotMm)   -- the lens
//     geometry is pushed back OUT by the pivot radius. Present.
//   * build_skeleton gives the PUPIL bone a matching  {kEyeShiftPivotMm,0,0}
//     bind so the gaze pivot stays at the lens centre. Present.
//   * build_skeleton was supposed to pull the EYE BONE'S OWN BIND INWARD by
//     the same radius. IT DOES NOT: the bind is a flat
//     {kEyeXMm, vmm(kEyeYMm), +-kEyeZMm} with no subtraction anywhere.
//
// So setting kEyeShiftPivotMm non-zero today does not relocate a pivot at
// all; it TRANSLATES THE WHOLE EYE ASSEMBLY OUTWARD by that many millimetres
// and leaves the pivot exactly where it was. The reason nobody saw it is that
// the constant is 0, and at 0 both halves are identity. This is recorded here
// rather than fixed, because fixing it is a shipping act and this lane ships
// nothing -- but any pass that reaches for kEyeShiftPivotMm must fix the
// skeleton in the same edit or it will move the eye and call it a pivot.
//
// ---- WHAT IS BUILT INSTEAD: AN ARC ABOUT THE BODY'S OWN AXIS --------------
//
// The rig authors ROTATIONS ONLY (only the root carries translation), and a
// bone rotates about ITS OWN origin. The eye bone's origin IS the lens centre,
// so every rotation available on it spins the lens on the spot. Travel
// therefore needs a pivot that is NOT at the eye -- so the lab adds two inert
// parent bones, kBEyeTravelL/R, bound at the body's own axis. Rotating one
// carries its whole eye assembly (lens, both stars, and their orientation)
// around the head on an arc. Identity at rest, so the rest pose is
// bit-identical.
//
// AND THE ARC CONFORMS TO THE SURFACE FOR FREE. This is the one genuinely
// lucky fact in the whole lane, and it is worth stating plainly because it is
// what makes travel cheap:
//
//   make_ball builds CIRCULAR rings. kBodyTaperPm scales a ring's radius and
//   kVStretchPm stretches Y -- neither turns a circle into anything else. So
//   the body's HORIZONTAL CROSS-SECTION IS A CIRCLE at every height. It is
//   only OFF-CENTRE IN X, by kBodyLeanXMm at that height (the teardrop lean).
//
// A rotation about the vertical axis through that circle's centre therefore
// holds the eye at EXACTLY CONSTANT distance from the body surface, at every
// travel angle, with no conforming maths at all. Travel does not need to
// solve "stay on a curved surface" -- it needs to pick the right axis, once.
//
// ⚠ THE AXIS IS NOT x = 0. Putting it at the body centre would swing the eye
// through 33 mm of the lean, which at these radii is a third of the crown's
// whole stand-off: the eye would sink on the way out. kEyeTravelPivotXMm is
// the knob, and it is a knob and not a derivation -- see its own comment.
//
// ---- WHAT THE ARC DOES *NOT* SOLVE: THE BREATH ---------------------------
//
// §7.7's hard part 1 asks for the eye to be resolved against the POSED,
// DEFORMED surface every frame, and warns via 09-ENGINE-GOTCHAS §15 that a
// skinning-matrix inverse returns BIND space. Both warnings are live, and the
// second is not the interesting one here. The interesting one is this:
//
//   THE EYES DO NOT BREATHE AT ALL, TODAY. DeformSample is a per-frame
//   global; a part opts in through RingSpec::deform_role. In manafold_model.h
//   ONLY make_body opts in. make_eye_lens and make_star declare kNone, so the
//   lens and the stars are RIGID while the body inflates and deflates
//   underneath them.
//
// So the eye is not "computed once against the neutral shape" -- it is never
// computed against the shape at all. The pulsation moves the surface out from
// under a stationary lens. That is measurable rather than arguable, and the
// committed probe (manafold_eyelab.cpp) measures it at the extremes of the
// breath rather than at rest, which is where §7.7 says the fault lives.
//
// kEyeLabBreatheEye is the lane's answer to it, and it is OPTIONAL per
// variant so the cost of NOT having it stays visible in the same table.

#ifndef ZHAO_REEL_MANAFOLD_EYELAB_H
#define ZHAO_REEL_MANAFOLD_EYELAB_H

#include <cstdlib>
#include <cstdio>
#include <string>

#include "manafold_art.h"

namespace u02 {
namespace eyelab {

// ========================= THE TRAVEL AXIS ================================
//
// The body's horizontal section at the eye's height is a circle centred at
// x = kBodyLeanXMm(that height). The eye centre sits at y = vmm(kEyeYMm) =
// 149 fx-mm; the ball's stretched half-height is vmm(kBodyRadiusMm) = 747, so
// the eye is 0.20 of the way up from the equator, which lands between body
// rings 5 (lean 20 mm) and 6 (lean 40 mm) -- about 33 mm of lean.
//
// ⚠ MEASUREMENT CHOSE NOTHING HERE. 33 is where the body's own construction
// puts the section centre; it is the STARTING POINT, and the probe reports
// the crown stand-off and the sink depth as the axis moves, so the value can
// be judged by what it does to the read rather than by its provenance. If the
// eye sinks on the way out, this is the first knob, not the travel angle.
constexpr int32_t kEyeTravelPivotXMm = 33;

// THE CEILING, and the owner set it twice -- once at 45-90 deg and then
// explicitly halved:
//   "But I was way overzealous with 45-90. 45 should be absolute max it can
//    go, less is better"
// So 45 is a HARD CLAMP that the ladder is not expected to reach, and the
// ladder below deliberately starts far under it.
constexpr int32_t kEyeTravelMaxDeg = 45;
constexpr int32_t kEyeTravelMaxA16 = kEyeTravelMaxDeg * 65536 / 360;

// The swept ladder. NOT evenly spaced past the middle: the interesting
// question is where the read BREAKS, and the pass-7 roll lesson is that a
// minimum can sit in the INTERIOR of a range, so the low end is sampled
// finely enough to catch one.
constexpr int kTravelLadder[] = {0, 6, 10, 14, 18, 22, 27, 32, 38, 45};
constexpr int kTravelLadderN = static_cast<int>(sizeof(kTravelLadder) / sizeof(int));

/** Travel in DEGREES -> the a16 rotation, clamped at the owner's ceiling. */
constexpr int32_t travel_a16(int32_t deg) {
  const int32_t d = deg > kEyeTravelMaxDeg    ? kEyeTravelMaxDeg
                    : deg < -kEyeTravelMaxDeg ? -kEyeTravelMaxDeg
                                              : deg;
  return d * 65536 / 360;
}

// ========================= THE STAR (§12.1, §12.2) ========================
//
// §12.1 REVERSES §5.1. The shipped rest is kStarOffsetYMm = 0 (concentric),
// recorded there as a registration fix. Offered the explicit choice between
// that and the drawing, the owner picked the drawing: the star sits HIGH.
//
// Both readings are consistent once you see what he was objecting to -- the
// star that annoyed him was LOW AND OUTBOARD, which is misregistration, not
// placement. High is a choice; low-and-outboard was a mistake. Different axis.
//
// ⚠ THE COST §12.1 ORDERS AUTHORED RATHER THAN DISCOVERED: travel room goes
// ASYMMETRIC. Generous below, thin above. The §5c leash is a SYMMETRIC cap
// (kStarOverhangMaxPm, one number both ways), so a high rest position spends
// part of the upward half before the gaze even starts. The probe reports the
// upward and downward margins SEPARATELY for exactly this reason -- a single
// worst-case number cannot show an asymmetry, and this one has to be seen to
// be tuned.
constexpr int32_t kStarHighOffsetYMm = 42;   // the sheet's high placement
// §12.2: "currently noticeably smaller than drawn". Ships at 950 pm of
// drawn-flush. The ladder walks it up; 1000 IS drawn-flush (the white star's
// side tip reaches the lens rim exactly), and past that the star is bigger
// than the drawing.
constexpr int kStarScaleLadder[] = {950, 1000, 1050, 1100};
constexpr int kStarScaleLadderN =
    static_cast<int>(sizeof(kStarScaleLadder) / sizeof(int));

// ========================= THE BLINK (§12.3) ==============================
//
// "The whole lens squashes vertically and springs back, with the star riding
// it." The chosen mechanism is the deform channel, and the channel supports
// exactly this shape of thing: RingSpec::deform_role kRadial squashes a part
// about its own centre on a chosen local axis, and kFollower TRANSLATES an
// attachment by its carrier point's contraction without crushing it. The star
// as a kFollower on the lens's centre is §5b's "one unit" enforced by
// construction -- it CANNOT slide against the purple, because it is not
// squashed at all, it is carried.
//
// ⚠ AND HERE IS THE MECHANISM'S ONE HARD LIMIT, found by reading rather than
// by rendering, so it is stated before any plate:
//
//   THE DEFORM SAMPLE IS ONE GLOBAL {flatten, spread} PER FRAME. It is shared
//   by every part that opts in. `sub` is the 60 Hz half-tick interpolation
//   rung (creature_core.cpp: it selects clip->mid_deform), NOT a second
//   channel. So a blink authored on this channel and the body's breath
//   authored on this channel ARE THE SAME SIGNAL.
//
// The body already uses it: make_body opts in at strength 255 on the equator.
// So "reuse the deform channel that drives the bounce" cannot mean an
// independent blink schedule -- driving a fast blink spike into the sample
// squashes the whole creature on the same spike.
//
// Two ways out, and the lane plates both rather than picking by argument:
//   kBlinkRideBreath   the blink IS the breath's own trough, deepened. One
//                      signal, honest, and the body moves with the eye.
//   kBlinkStrengthSplit  the sample carries the BLINK waveform and the body's
//                      deform_strength is cut hard, so the same signal moves
//                      the eye a lot and the body a little. The breath is then
//                      the blink's own slow component.
constexpr int32_t kBlinkSquashPm = 720;    // how far the lens flattens
constexpr int kBlinkFallKeys = 2;          // down: fast
constexpr int kBlinkRiseKeys = 4;          // up: the spring, slower
constexpr int kBlinkHoldKeys = 1;          // closed
// The body's strength under the split variant. 255 today; this is what the
// body keeps when the sample is carrying a blink.
constexpr int kBlinkBodyStrengthSplit = 28;

/** The blink envelope in per-mille, 0 at rest and 1000 fully squashed.
 *  Fast down, hold, slower spring back -- 07-MOTION-STYLE's asymmetry. What
 *  reads as broken is REVERSAL DENSITY, so this has exactly two reversals. */
inline int32_t blink_env_pm(int t) {
  if (t < 0) return 0;
  if (t < kBlinkFallKeys)
    return 1000 * (t + 1) / kBlinkFallKeys;
  if (t < kBlinkFallKeys + kBlinkHoldKeys) return 1000;
  const int u = t - kBlinkFallKeys - kBlinkHoldKeys;
  if (u < kBlinkRiseKeys) return 1000 - 1000 * (u + 1) / kBlinkRiseKeys;
  return 0;
}
constexpr int kBlinkLenKeysLab =
    kBlinkFallKeys + kBlinkHoldKeys + kBlinkRiseKeys;

// ========================= THE VARIANT TABLE ==============================
//
// Named by MECHANISM, never by number (the mana lab's rule, and it is a good
// one -- the owner picks a direction, not a row).
struct EyeVariant {
  const char* name;
  const char* mechanism;
  int32_t travel_deg;      // arc, front -> flank
  int32_t star_scale_pm;   // §12.2
  int32_t star_offset_mm;  // §12.1: 0 = shipped concentric, >0 = high
  bool breathe_eye;        // lens+star opt into the deform (the §7.7 fix)
  bool blink;              // run the blink schedule
  bool blink_split;        // blink via strength split rather than riding breath
  bool compose_extremes;   // stack roll + gaze + twinkle on the travel extreme
  int32_t star_thin_mm;    // the star's half-DEPTH (shipped kStarThinMm = 16)
  int32_t dome_drop_mm;    // THE NEAR-EYE BAR CANDIDATE -- see below
};

// ===================== THE NEAR-EYE BAR: A MECHANISM =======================
//
// The bar has been named for three passes -- "the near eye collapses into a
// BAR, a chrome scratch, not an eye" on 96% of taunt's frames -- and read each
// time as a gaze or rendering fault, then as the star's own drawn PROPORTION
// (pass 7 measured the sheet and widened the star from 2.82 to 1.70
// major/minor). Widening it helped and did not solve it.
//
// LOOKING AT THE LANE'S OWN ZOOMED PLATE gives a third answer, and it is a
// geometric one that neither previous reading could have reached from a
// number:
//
//   THE LENS HAS DEPTH AND THE STAR HAS ALMOST NONE.
//
//   the lens   2 * kEyeDeepMm  = 180 mm of extent along the eye's OUTWARD axis
//   the star   2 * kStarThinMm =  32 mm
//
// At the obliquity where the eye is viewed along its own WIDTH axis -- which
// is most of a three-quarter camera's range, and all of the side view -- the
// picture-plane silhouette of each part is carried by its extent along the
// OUTWARD axis, not by its drawn width. The lens still reads as a lens because
// its dome gives it 180 mm to project. The star collapses to its own 32 mm.
// That is a 5.6:1 loss, and it is why the star specifically -- not the lens --
// becomes the scratch.
//
// So the bar is NOT the star being edge-on to the camera. It is the star being
// FLAT while the thing it sits on is ROUND. A flat plate on a dome, which is
// what the fault was called before anyone knew why.
//
// ⚠ THE OBVIOUS FIX IS THE ONE THAT ALREADY FAILED ONCE. Simply raising
// kStarThinMm makes both stars thicker, and make_star's own comment records
// what that cost: "a white slab 2*(thin+rim) deep centred on the pupil
// swallowed the thinner cyan whole, so the star drew as a white splinter with
// no cyan anywhere, from every angle." `bar-thicker` is in the table to put
// that on the record with a plate rather than a memory.
//
// THE PROPOSAL IS TO DOME THE STAR INSTEAD. Each ring's outward stand-off
// falls away toward the star's tips, so the star WRAPS the lens's dome rather
// than floating flat over it. Two things follow, and only the first is
// obvious:
//   * the star gains outward extent -- 32 mm becomes 32 + 2*dome_drop -- so it
//     has a silhouette to project when seen along the width axis;
//   * and it CURVES, so the cel ramp can put more than one band across it. A
//     flat plate under a single key light is one flat colour, which is the
//     other half of why it reads as a painted scratch rather than a form.
//
// ⚠ AND THE FACE-ON SILHOUETTE IS UNCHANGED. The drawn outline is the (y, z)
// profile table and this touches only x. The protected construction -- the
// star's shape, its asymmetric arms, the white generated from the cyan by one
// rim -- is not re-opened. This is PLACEMENT, which is what this lane is for.
constexpr int32_t kStarDomeDropMm = 62;   // authored by eye against the lens dome

// The ladder the owner asked for: SMALL UPWARD, not down from the ceiling.
constexpr EyeVariant kEyeVariants[] = {
    {"control-shipped",
     "CONTROL: shipped eye verbatim -- no travel, star concentric at 950 pm",
     0, kStarScalePm, 0, false, false, false, false, kStarThinMm, 0},

    {"travel-06", "TRAVEL: 6 deg of arc -- the smallest move that is a move",
     6, kStarScalePm, 0, false, false, false, false, kStarThinMm, 0},
    {"travel-14", "TRAVEL: 14 deg of arc", 14, kStarScalePm, 0, false, false, false, false, kStarThinMm, 0},
    {"travel-22", "TRAVEL: 22 deg of arc -- half the owner's ceiling",
     22, kStarScalePm, 0, false, false, false, false, kStarThinMm, 0},
    {"travel-32", "TRAVEL: 32 deg of arc", 32, kStarScalePm, 0, false, false, false, false, kStarThinMm, 0},
    {"travel-45", "TRAVEL: 45 deg -- the owner's ABSOLUTE MAXIMUM, for the record",
     45, kStarScalePm, 0, false, false, false, false, kStarThinMm, 0},

    {"travel-22-breathe",
     "TRAVEL + the eye opts into the deform, so it rides the pulsation",
     22, kStarScalePm, 0, true, false, false, false, kStarThinMm, 0},
    {"travel-45-breathe",
     "TRAVEL at the ceiling + the eye rides the pulsation",
     45, kStarScalePm, 0, true, false, false, false, kStarThinMm, 0},

    {"star-high", "STAR: 12.1 -- high in the lens as the sheet draws it",
     0, kStarScalePm, kStarHighOffsetYMm, false, false, false, false, kStarThinMm, 0},
    {"star-big", "STAR: 12.2 -- grown to drawn-flush (1000 pm)",
     0, 1000, 0, false, false, false, false, kStarThinMm, 0},
    {"star-high-big", "STAR: 12.1 + 12.2 together -- high AND drawn-flush",
     0, 1000, kStarHighOffsetYMm, false, false, false, false, kStarThinMm, 0},
    {"star-high-bigger", "STAR: high, and PAST drawn-flush at 1100 pm",
     0, 1100, kStarHighOffsetYMm, false, false, false, false, kStarThinMm, 0},

    {"blink-ride-breath",
     "BLINK: the lens squashes on the breath's own signal (one channel, honest)",
     0, kStarScalePm, 0, true, true, false, false, kStarThinMm, 0},
    {"blink-strength-split",
     "BLINK: the sample carries the blink; the body's strength is cut to 28/255",
     0, kStarScalePm, 0, true, true, true, false, kStarThinMm, 0},

    {"composed-extreme",
     "GATE: travel 45 + roll + gaze + twinkle + breath, all on one frame",
     45, 1000, kStarHighOffsetYMm, true, true, false, true, kStarThinMm, 0},
    {"composed-recommended",
     "GATE: the ranked recommendation, composed the same way",
     14, 1000, kStarHighOffsetYMm, true, false, false, true, kStarThinMm, 0},

    // ---- THE NEAR-EYE BAR, three answers and one of them is the control ----
    {"bar-control",
     "BAR: the shipped flat star, so the plate has something to be beside",
     0, kStarScalePm, 0, false, false, false, false, kStarThinMm, 0},
    {"bar-thicker",
     "BAR: the naive fix -- kStarThinMm 16 -> 46. Expected to reproduce pass "
     "6's white slab swallowing the cyan; on the record either way",
     0, kStarScalePm, 0, false, false, false, false, 46, 0},
    {"bar-domed",
     "BAR: the star DOMES to wrap the lens instead of floating flat on it",
     0, kStarScalePm, 0, false, false, false, false, kStarThinMm, kStarDomeDropMm},
    {"bar-domed-high-big",
     "BAR: domed + 12.1 high + 12.2 drawn-flush -- the three eye changes together",
     0, 1000, kStarHighOffsetYMm, false, false, false, false,
     kStarThinMm, kStarDomeDropMm},
    {"bar-domed-travel14",
     "BAR: domed, at the travel angle the plates rank first",
     14, 1000, kStarHighOffsetYMm, true, false, false, false,
     kStarThinMm, kStarDomeDropMm},
};
constexpr int kEyeVariantCount =
    static_cast<int>(sizeof(kEyeVariants) / sizeof(kEyeVariants[0]));

/** Select by name from U02_EYELAB; index 0 (the control) when unset. */
inline int variant_index() {
  static int idx = [] {
    const char* v = std::getenv("U02_EYELAB");
    if (v == nullptr || v[0] == 0) return 0;
    for (int i = 0; i < kEyeVariantCount; ++i)
      if (std::string(v) == kEyeVariants[i].name) return i;
    std::fprintf(stderr, "u02 eyelab: unknown variant, using control\n");
    return 0;
  }();
  return idx;
}
inline const EyeVariant& variant() { return kEyeVariants[variant_index()]; }

}  // namespace eyelab
}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_EYELAB_H
