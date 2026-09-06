// MANAFOLD (creature 02) — model builders: make_ball(), the body, the hinges.
//
// The consumer (zhao_reel.cpp / the probe) provides `namespace zc =
// zref::creature;` and the zref includes, the same contract zixxtrixx.h uses.
// Everything here is integer-only authoring over the generic ring builder.

#ifndef ZHAO_REEL_MANAFOLD_MODEL_H
#define ZHAO_REEL_MANAFOLD_MODEL_H

#include "manafold_art.h"
#include "manafold_eyelab.h"  // LANE-ONLY (Direction 7 12): the eye-lab variant table
#include "manafold_rig.h"

namespace u02 {

/**
 * A UV-sphere as one rigid RingPart: `rings` latitude bands at half-step
 * cosine spacing (theta_i = (2i+1)·half_turn / 2·rings), so the outermost
 * rings sit close to the poles and the cap fans close tiny, nearly-flat
 * discs no camera at 240p can see. Segments taper with sin(theta) down to
 * `pole_seg` — the integer zipper walk stitches unequal counts. Radius and
 * height are exact integer sin/cos of the same tables the ring builder uses,
 * so the surface is watertight and the position-keyed generated normals come
 * out seamless. Rings stack along local +Y = world up (no quarter turns).
 */
inline zc::RingPart make_ball(int32_t radius_mm, int rings, int max_seg, int pole_seg,
                              uint8_t bone) {
  zc::RingPart p;
  p.bone = bone;
  p.caps = zc::kCapTop | zc::kCapBot;
  p.cap_base_fix = true;  // the corrected band stitching (opt-in; see rig header)
  const int32_t R = fxu(radius_mm);
  for (int i = 0; i < rings; ++i) {
    const uint16_t theta =
        static_cast<uint16_t>(((2 * i + 1) * 32768LL) / (2 * rings));  // (0, half turn)
    const int32_t sn = zref::fx_sin(zref::angle16{theta}).raw;         // Q16.16, >= 0 here
    const int32_t cs = zref::fx_cos(zref::angle16{theta}).raw;
    zc::RingSpec rs;
    // vertical counter-stretch: the ring HEIGHTS stretch, the radii do not,
    // so the ball projects round through the anisotropic viewport
    rs.y = static_cast<int32_t>(
        ((-static_cast<int64_t>(R) * cs) >> 16) * kVStretchPm / 1000);
    rs.radius = static_cast<int32_t>((static_cast<int64_t>(R) * sn) >> 16);
    int seg = static_cast<int>((static_cast<int64_t>(max_seg) * sn + (1 << 15)) >> 16);
    if (seg < pole_seg) seg = pole_seg;
    if (seg > max_seg) seg = max_seg;
    rs.segments = static_cast<uint8_t>(seg);
    p.rings.push_back(rs);
  }
  return p;
}

/**
 * The body ball: make_ball reshaped by the teardrop knobs — per-ring radius
 * multiplier (Pm) and forward lean, both identity by default. Deform sidecar:
 * kRadial about the ball centre, vertical axis, strength peaking at the
 * equator (the constant compression lives on these vertices).
 */
inline zc::RingPart make_body(uint8_t bone) {
  zc::RingPart p = make_ball(kBodyRadiusMm, kBodyRings, kBodySegments, kBodyPoleSegments, bone);
  const int32_t R = fxu(kBodyRadiusMm);
  for (int i = 0; i < kBodyRings; ++i) {
    zc::RingSpec& rs = p.rings[static_cast<size_t>(i)];
    rs.radius = static_cast<int32_t>((static_cast<int64_t>(rs.radius) * kBodyTaperPm[i]) / 1000);
    rs.cx = fxu(kBodyLeanXMm[i]);
    rs.deform_role = zc::DeformRole::kRadial;
    rs.deform_axis = 1;  // vertical: flatten squashes up-down, spread bulges out
    // strength peaks at the equator, eases toward the poles (|y| over the
    // STRETCHED half-height; the pole rings keep a floor of 1 so the
    // authored role stays valid everywhere on the ball)
    const int32_t half_h = static_cast<int32_t>(static_cast<int64_t>(R) * kVStretchPm / 1000);
    const int64_t a = static_cast<int64_t>(rs.y < 0 ? -rs.y : rs.y);
    int32_t s = static_cast<int32_t>(255 - (a * 255) / (half_h > 0 ? half_h : 1));
    if (s < 1) s = 1;
    if (s > 255) s = 255;
    // ---- EYE LAB, LANE-ONLY: the blink-vs-breath channel collision --------
    // There is ONE DeformSample per frame and every opted-in part shares it
    // (see manafold_eyelab.h). So a blink spike on the sample squashes the
    // BODY too. `blink-strength-split` is the variant that answers it by
    // cutting the body's own authority hard, leaving the same signal reading
    // as an eye event with a faint body settle under it. It is a real trade
    // and the plates are supposed to show what it costs the bounce.
    if (eyelab::variant().blink_split) {
      s = s * eyelab::kBlinkBodyStrengthSplit / 255;
      if (s < 1) s = 1;
    }
    rs.deform_strength = static_cast<uint8_t>(s);
    rs.deform_center_x = 0;
    rs.deform_center_y = 0;
    rs.deform_center_z = 0;
  }
  p.r = kGreyR;
  p.g = kGreyG;
  p.b = kGreyB;
  p.page = kPageAtlasTile;
  p.v0 = kBodyV0;
  p.v1 = kBodyV1;
  return p;
}

/**
 * The antenna loop: ONE chain part bound STRAIGHT along +Y from inside the
 * body up past hinge C, blade-elliptical (broad rx in the loop plane, thin
 * rz across). Per-ring {b0,b1,w0} blends across each hinge over a few rings
 * so the posed folds bend smoothly. The drawn loop shape is a POSE.
 */
inline zc::RingPart make_loop() {
  zc::RingPart p;
  p.chain = true;
  p.cap_base_fix = true;
  p.caps = zc::kCapTop | zc::kCapBot;  // both ends are buried in the body, but
                                       // a hole is a hole: cap them closed
  const int32_t y0 = kLoopNeckExitYMm - kLoopBuryMm;
  // stations along the tube, mm from y0: the front junction (the surface
  // exit — the old neck), the NEW neck, hinges A..D, arm end (pass 4)
  const int32_t stJF = kLoopBuryMm;
  const int32_t stNeck = stJF + kLoopArcMm[0];
  const int32_t stA = stNeck + kLoopArcMm[1];
  const int32_t stB = stA + kLoopArcMm[2];
  const int32_t stC = stB + kLoopArcMm[3];
  const int32_t stD = stC + kLoopArcMm[4];
  const int32_t total = stD + kLoopArcMm[5];
  // PASS 8 (pass-7 by-eye fault 1, the MITRE half): 145 -> 165. A fold blend
  // is the corner's turning radius; at 145 mm on a band 126 mm wide the corner
  // radius was about one band width, which is what "almost right-angled" and
  // "mitred" describe. The hard ceiling is half the SHORTEST station spacing
  // (336/2 = 168) -- past that two blends overlap and the two-bone ladder
  // below cannot express the ring, because a ring picks ONE bone pair.
  const int32_t blend = 165;  // half-width of each fold blend, mm of tube
                              // (max the station spacing admits; wider blend
                              // = rounder corner, and the five-fold pentagon
                              // plus these blends is the sheet's soft ring)
  // the per-station blade taper (piecewise-linear between stations)
  // PASS 9: SEVEN keys. stNeck is now coincident with stJF (Direction 7 §9.1 --
  // the joints move onto the balls and the junctions), so a key for it would be
  // a zero-width span returning a different value one millimetre later: a ledge
  // in the skin, not a taper. See kLoopBladeRxMm.
  const int32_t stKey[7] = {0, stJF, stA, stB, stC, stD, total};
  const auto taper = [&](const int32_t* k, int32_t s) {
    for (int j = 0; j + 1 < 7; ++j) {
      if (s <= stKey[j + 1]) {
        const int32_t span = stKey[j + 1] - stKey[j];
        if (span <= 0) return k[j + 1];
        return k[j] + static_cast<int32_t>(
            (static_cast<int64_t>(k[j + 1] - k[j]) * (s - stKey[j])) / span);
      }
    }
    return k[6];
  };
  for (int i = 0; i < kLoopRings; ++i) {
    const int32_t s = static_cast<int32_t>((static_cast<int64_t>(total) * i) / (kLoopRings - 1));
    zc::RingSpec rs;
    rs.y = fxu(y0 + s);
    rs.radius = 0;
    // PASS 6 B.2: the KNUCKLES, in the skin. The band is the accepted taper
    // above, untouched; each knuckle adds a flat-topped bump that meets the
    // band with zero slope, so there is no crease and no waist. MAX, not sum:
    // two overlapping swells cannot stack into a lump.
    const auto swell = [&](int32_t at, int32_t rx_mm, int32_t rz_mm,
                           int32_t& best_x, int32_t& best_z) {
      const int32_t d = s > at ? s - at : at - s;
      if (d >= kKnuckleSwellHalfMm) return;
      // u = 1 - (d/half)^2, then w = SMOOTHSTEP(u) -- per-mille throughout.
      //
      // PASS 8: w was u^2, which is zero-slope at the rim (good, no crease) but
      // also very peaked -- it reaches full height only AT the station and is
      // already down to 56% at half-width. That is a soft hump, and a soft hump
      // on a band reads as the band wobbling, not as a joint. The Side sheet
      // draws ROUND LUMPS: flat over the top, falling away near the rim.
      // Smoothstep u*u*(3-2u) is 84% at half-width instead of 56%, and is STILL
      // zero-slope at u=0, so pass 6's no-crease property is preserved exactly.
      // (A true circular profile, sqrt(1-(d/h)^2), would be rounder still and
      // would put a visible crease ring at every rim -- that is the bead fault
      // returning, so it is deliberately not used.)
      const int32_t u = 1000 - static_cast<int32_t>(
          (static_cast<int64_t>(d) * d * 1000) /
          (static_cast<int64_t>(kKnuckleSwellHalfMm) * kKnuckleSwellHalfMm));
      const int32_t w = static_cast<int32_t>(
          (static_cast<int64_t>(u) * u * (3000 - 2 * u)) / 1000000);
      const int32_t ax = static_cast<int32_t>((static_cast<int64_t>(rx_mm) * w) / 1000);
      const int32_t az = static_cast<int32_t>((static_cast<int64_t>(rz_mm) * w) / 1000);
      if (ax > best_x) best_x = ax;
      if (az > best_z) best_z = az;
    };
    int32_t sw_x = 0, sw_z = 0;
    swell(kKnuckleAtJfMm, kKnuckleSwellJfRxMm, kKnuckleSwellJfRzMm, sw_x, sw_z);
    swell(kKnuckleAtAMm, kKnuckleSwellARxMm, kKnuckleSwellARzMm, sw_x, sw_z);
    swell(kKnuckleAtBMm, kKnuckleSwellBRxMm, kKnuckleSwellBRzMm, sw_x, sw_z);
    swell(kKnuckleAtCMm, kKnuckleSwellCRxMm, kKnuckleSwellCRzMm, sw_x, sw_z);
    swell(kKnuckleAtEndMm, kKnuckleSwellEndRxMm, kKnuckleSwellEndRzMm, sw_x, sw_z);
    rs.rx = fxu(taper(kLoopBladeRxMm, s) + sw_x);
    rs.rz = fxu(taper(kLoopBladeRzMm, s) + sw_z);
    rs.segments = static_cast<uint8_t>(kLoopSegments);
    // weights: a ladder of two-bone blends across the five fold stations
    const auto blend_of = [&](int32_t st) {
      int32_t t = ((s - (st - blend)) * 64) / (2 * blend);
      if (t < 0) t = 0;
      if (t > 64) t = 64;
      return t;  // 0 = fully lower bone, 64 = fully upper bone
    };
    // PASS 9 (Direction 7 §9.1): FIVE rungs, one per articulation station, and
    // every station is now a BALL or a BODY JUNCTION. kBJunctionF drops out of
    // the ladder and becomes a pure parent: it shares kBNeck's pivot exactly
    // (kLoopArcMm[0] == 0), so weighting a ring to it would be weighting to the
    // same point twice, while a rotation on it still bends the whole antenna by
    // construction because every loop bone descends from it.
    //
    // The continuity condition this ladder depends on, stated so the next
    // station move does not have to rediscover it: a branch flips at
    // (station - blend), and the previous station's weight must already have
    // SATURATED by then, which needs consecutive stations >= 2*blend apart.
    // At blend = 165 that is 330 mm; the closest pair here is A->B at 340.
    const int32_t tN = blend_of(stNeck), tA = blend_of(stA), tB = blend_of(stB),
                  tC = blend_of(stC), tD = blend_of(stD);
    if (tA == 0) {  // the buried base -> the front junction
      rs.b0 = kBRoot;
      rs.b1 = kBNeck;
      rs.w0 = static_cast<uint8_t>(64 - tN);
    } else if (tB == 0) {  // the front junction -> ball A
      rs.b0 = kBNeck;
      rs.b1 = kBHingeA;
      rs.w0 = static_cast<uint8_t>(64 - tA);
    } else if (tC == 0) {  // ball A -> ball B
      rs.b0 = kBHingeA;
      rs.b1 = kBHingeB;
      rs.w0 = static_cast<uint8_t>(64 - tB);
    } else if (tD == 0) {  // ball B -> ball C
      rs.b0 = kBHingeB;
      rs.b1 = kBHingeC;
      rs.w0 = static_cast<uint8_t>(64 - tC);
    } else {  // ball C -> the RE-ENTRY BALL (the second body junction) and the
              // buried return arm past it
      rs.b0 = kBHingeC;
      rs.b1 = kBHingeD;
      rs.w0 = static_cast<uint8_t>(64 - tD);
    }
    // chain rings are creature-global: carry the tube x
    rs.cx = fxu(kLoopTubeXMm);
    p.rings.push_back(rs);
  }
  p.r = kGreyR;
  p.g = kGreyG;
  p.b = kGreyB;
  p.page = kPageAtlasTile;
  p.v0 = kLoopV0;
  p.v1 = kLoopV1;
  return p;
}

// PASS 6 B.2: make_hinge() and make_knuckle() are DELETED, and with them the
// five separate closed spheres -- three hinge balls and the front-junction and
// re-entry knuckles. Direction 5 §2b: "smooth skin not visible balls."
//
// Deleting them does three things at once, which is why the recon preferred it
// to re-skinning: it removes the seam and the pinch at every join; it removes
// the sphere-slides-off-the-centreline artefact at large hinge swings; and it
// removes the ONE thing the deform sidecar genuinely cannot do, which is
// stitch two separate closed surfaces into one continuous skin.
//
// It also resolves the free-floating dongle (Direction 5 §1) STRUCTURALLY
// rather than by re-attaching anything. The re-entry knuckle was
// make_knuckle(kBLoopBase2, ...) parented to the BODY -- that parenting was
// the whole fault. As a swell on the chain it is skinned to the arm and
// travels with it by construction: there is nothing left to detach.
//
// The rig is UNCHANGED: kBHingeA/B/C and kBJunctionF all still exist and still
// drive the chain through the two-bone blend ladder in make_loop(); only their
// rigid ball parts are gone.
//
// PASS 10 C.4 — kBLoopBase2 WAS IN THAT LIST AND DID NOT BELONG THERE. It is
// not in the blend ladder and drives no vertex: make_loop() never names it, and
// nothing else binds to it either. The sentence read as finished code while the
// bone it named moved nothing, which is the dangerous shape of a false comment
// — an owner instruction was reported delivered and was inert.
// What it DOES do, since pass 10 C.1: loop_pose aims the closure at its POSED
// anchor, so its rotation slides the re-entry point along the body surface and
// re-aims the return arm. That is a real effect with no skinning behind it.

/**
 * PASS 6 B.1 -- THE LENS. A SYMMETRIC lens, pointed at BOTH ends, ~3.2:1.
 *
 * This replaces make_lens_teardrop() (Direction 4's "pointy at the top,
 * rounder at the bottom") and make_lens() (the pass-3 round-ended ellipsoid
 * A/B). The sheets show neither: they show a symmetric lens that comes to a
 * point at each end, and Direction 5 §5 makes the front sheet the authority.
 * The U02_EYE=x2 A/B branch retires with them -- the question it existed to
 * answer has been decided by the owner.
 *
 * Local +Y is the long axis (tilted into the Λ by the eye bone's rest);
 * width AND dome depth follow kEyeLensWidthPm, which is symmetric by
 * construction, so the two ends cannot drift apart the way two separately
 * authored ends did.
 */
inline zc::RingPart make_eye_lens(uint8_t bone) {
  zc::RingPart p;
  p.bone = bone;
  p.cap_base_fix = true;
  p.caps = zc::kCapTop | zc::kCapBot;
  for (int i = 0; i < kEyeLensRings; ++i) {
    const int32_t t_pm = 2000 * i / (kEyeLensRings - 1) - 1000;  // -1000..1000
    const int32_t w = kEyeLensWidthPm[i];
    zc::RingSpec rs;
    rs.y = static_cast<int32_t>(
        (static_cast<int64_t>(fxu(kEyeLongMm)) * t_pm / 1000) * kVStretchPm / 1000);
    rs.radius = 0;
    rs.rx = static_cast<int32_t>((static_cast<int64_t>(fxu(kEyeDeepMm)) * w) / 1000);
    rs.rz = static_cast<int32_t>((static_cast<int64_t>(fxu(kEyeWideMm)) * w) / 1000);
    // §5c: the lens rides back out to its authored place over the eye bone's
    // relocated pivot, so the REST POSE IS UNCHANGED and a rotation on that
    // bone sweeps the eye across the body instead of spinning it in place.
    rs.cx = fxu(kEyeShiftPivotMm);
    rs.segments = static_cast<uint8_t>(kEyeFacetSegments);
    // ---- EYE LAB, LANE-ONLY: the lens opts into the deform sidecar --------
    // Two jobs at once, and they are the same mechanism:
    //   12.3  THE BLINK. kRadial on the lens's LOCAL LONG AXIS (+Y is the long
    //         axis here) contracts the lens tip-to-tip about its own centre --
    //         which on a lens tilted into the Lambda IS the vertical squash the
    //         owner chose. No new geometry, exactly as 12.3 asks.
    //   7.7   THE PULSATION. Today make_body is the ONLY part that opts in, so
    //         the lens is RIGID while the surface under it inflates. Opting in
    //         makes the lens breathe WITH the body instead of being left behind
    //         by it, which is what "resolve against the posed, deformed
    //         surface" has to mean for a part that is skinned rather than
    //         projected.
    // deform_center is LOCAL to the part's bone and the compiler adds the bind
    // translation, so (0,0,0) is exactly the lens centre.
    if (eyelab::variant().breathe_eye) {
      rs.deform_role = zc::DeformRole::kRadial;
      rs.deform_axis = 1;
      rs.deform_strength = 255;
      rs.deform_center_x = 0;
      rs.deform_center_y = 0;
      rs.deform_center_z = 0;
    }
    p.rings.push_back(rs);
  }
  p.r = kLensR;
  p.g = kLensG;
  p.b = kLensB;
  p.page = kPageEyeTile;
  return p;
}

/**
 * PASS 6 B.1 -- THE STAR UNIT. Direction 5 §5a/§5b.
 *
 * ONE call builds either the cyan inner star or the white outer star, from
 * THE SAME PROFILE TABLE, so the white is literally a dilation of the cyan:
 * same points, same aim, offset outward by kStarWhiteRimMm. The white cannot
 * disagree with the star it rings, because it is generated from it. That
 * retires the pass-5 defect class rather than re-fixing it -- a star could
 * escape a separately authored ring whose tube gauge moved underneath it, and
 * now there is no separately authored ring.
 *
 * SHAPE: 4-pointed, CONCAVE curved edges drawn out into soft spikes, arms
 * unequal (bottom long, top medium, sides short), sitting HIGH in the lens.
 * The old star was two crossed CONVEX blades -- that is why it read as a blob,
 * and it is why replacing it is a shape fix and not a colour fix. Expressing
 * the concave scoop needs a width-along-the-axis profile, which is exactly
 * what a ring stack is: rings march up the long axis and each one is as wide
 * as the star is at that height, so the silhouette IS the drawn outline.
 *
 * ⚠ BOTH PARTS CARRY A PAGE. An untextured part does not render grey under
 * celmain, it renders BLACK (09-ENGINE-GOTCHAS §0/§7), and this exact star has
 * already shipped black once.
 *
 * ANIMATION CONTRACT (§5b): both parts ride the SAME pupil bone, so a pose
 * cannot slide the white against the blue -- they are one rigid unit with one
 * transform, which is the property the owner asked for. (The architecture asked
 * for literally one mesh; a RingPart carries one material and one page, so one
 * mesh cannot be two colours without a bespoke UV scheme fighting the ring
 * builder. Two parts on ONE bone satisfies every stated requirement -- two
 * transforms per eye, no sliding, one containment rule -- and is recorded here
 * as a deliberate, reversible deviation rather than a silent one.)
 */
inline zc::RingPart make_star(uint8_t bone, bool white) {
  // §5c: the arms are authored DRAWN-FLUSH and scaled back by kStarScalePm.
  // EYE LAB (12.2): the lane's scale, defaulting to the shipped kStarScalePm
  // on the control variant. One knob, exactly as the owner left it.
  const int32_t scale_pm = eyelab::variant().star_scale_pm;
  const auto sc = [scale_pm](int32_t v) {
    return static_cast<int32_t>((static_cast<int64_t>(v) * scale_pm) / 1000);
  };
  // EYE LAB (12.1): "the star sits HIGH, as drawn". This SUPERSEDES 5.1's
  // concentric rest. 0 on the control variant, so the control really is the
  // shipped eye.
  const int32_t star_off_mm = eyelab::variant().star_offset_mm;
  const int32_t rim = white ? kStarWhiteRimMm : 0;
  const int32_t bot = sc(kStarArmBottomMm) + rim;
  const int32_t top = sc(kStarArmTopMm) + rim;
  // ⚠ THE RIM IS A DILATION IN THE PICTURE PLANE ONLY -- it must NOT thicken
  // the white in DEPTH. Authored the other way first and it cost a render to
  // find: a white slab 2*(thin+rim) deep centred on the pupil swallowed the
  // thinner cyan whole, so the star drew as a white splinter with no cyan
  // anywhere, from every angle. Both stars are the same thickness; only their
  // outlines differ, which is what the sheet draws.
  // EYE LAB: the star's half-depth, and the DOME DROP that is the lane's
  // proposed answer to the near-eye bar. Both default to the shipped values on
  // every non-bar variant, so nothing else in the table moves because of them.
  const int32_t thin = eyelab::variant().star_thin_mm;
  const int32_t dome_drop = eyelab::variant().dome_drop_mm;
  zc::RingPart p;
  p.bone = bone;
  p.cap_base_fix = true;
  p.caps = zc::kCapTop | zc::kCapBot;
  for (int i = 0; i < kStarRings; ++i) {
    const int32_t yp = kStarProfileYPm[i];
    const int32_t arm = yp < 0 ? bot : top;
    // the WIDTH is a true outward offset (+rim), so the white traces the cyan
    // at a constant remove instead of being a scaled copy that thickens at the
    // tips; the tips themselves get a rim-wide rounded cap, which is what a
    // dilation of a point is.
    const int32_t w = static_cast<int32_t>(
        (static_cast<int64_t>(sc(kStarArmSideMm)) * kStarProfileWPm[i]) / 1000) + rim;
    zc::RingSpec rs;
    rs.y = static_cast<int32_t>(
        (static_cast<int64_t>(fxu(arm)) * yp / 1000) + fxu(star_off_mm));
    rs.radius = 0;
    rs.rx = fxu(thin);
    rs.rz = fxu(w);
    rs.segments = 6;
    // proud of the lens along +X: the pupil bone pivots at the lens centre, so
    // this stand-off IS the gaze pivot radius. The cyan rides a hair further
    // out than its white, so it sits ON it and never sinks into it.
    // Depth ordering, stated as arithmetic so it cannot silently invert again:
    // white front face = kEyeBulgeMm + thin; cyan front face = that + proud.
    // ---- EYE LAB: THE DOME ------------------------------------------------
    // The stand-off falls away toward the star's tips as the SQUARE of the
    // normalised arm position, which is the parabola that best matches a
    // shallow spherical cap over this span and needs no trig. At the star's
    // centre the stand-off is exactly the shipped kEyeBulgeMm, so the gaze
    // pivot radius -- and therefore every existing gaze, roll and twinkle
    // amplitude -- is untouched at the one point that defines them.
    //
    // The white and the cyan share yp, so they dome IDENTICALLY and cannot
    // separate. 5b holds by construction, as it does for the follower role.
    const int64_t ynorm = static_cast<int64_t>(yp) * yp / 1000;  // 0..1000
    const int32_t drop = static_cast<int32_t>(
        (static_cast<int64_t>(dome_drop) * ynorm) / 1000);
    rs.cx = fxu(kEyeBulgeMm - drop + (white ? 0 : kStarCyanProudMm));
    // ---- EYE LAB, LANE-ONLY: the star RIDES the lens's squash -------------
    // 12.3: "the star rides it. It must not slide against the purple during
    // the blink." kFollower is that guarantee expressed as CONSTRUCTION rather
    // than as a schedule: a follower takes its carrier point's displacement as
    // a PURE TRANSLATION and keeps its own dimensions and normals rigid. The
    // star is carried by the squash; it is never squashed by it, and it cannot
    // drift relative to the lens because it is not independently animated.
    //
    // The white and the cyan take the SAME profile table and therefore the
    // SAME carrier at every ring, so 5b's "never animate the white separately
    // from the blue" is preserved by the same fact.
    if (eyelab::variant().breathe_eye) {
      rs.deform_role = zc::DeformRole::kFollower;
      rs.deform_axis = 1;
      rs.deform_strength = 255;
      rs.deform_center_x = 0;
      rs.deform_center_y = 0;
      rs.deform_center_z = 0;
    }
    p.rings.push_back(rs);
  }
  if (white) {
    p.r = 246;
    p.g = 242;
    p.b = 250;
    p.page = kPageAtlasTile;
    p.v0 = kWhiteV0;
    p.v1 = kWhiteV1;
  } else {
    p.r = kStarR;
    p.g = kStarG;
    p.b = kStarB;
    p.page = kPageStarTile;
  }
  return p;
}

}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_MODEL_H
