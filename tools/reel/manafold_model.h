// MANAFOLD (creature 02) — model builders: make_ball(), the body, the hinges.
//
// The consumer (zhao_reel.cpp / the probe) provides `namespace zc =
// zref::creature;` and the zref includes, the same contract zixxtrixx.h uses.
// Everything here is integer-only authoring over the generic ring builder.

#ifndef ZHAO_REEL_MANAFOLD_MODEL_H
#define ZHAO_REEL_MANAFOLD_MODEL_H

#include "manafold_art.h"
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
  const int32_t blend = 145;  // half-width of each fold blend, mm of tube
                              // (max the station spacing admits; wider blend
                              // = rounder corner, and the five-fold pentagon
                              // plus these blends is the sheet's soft ring)
  // the per-station blade taper (piecewise-linear between stations)
  const int32_t stKey[8] = {0, stJF, stNeck, stA, stB, stC, stD, total};
  const auto taper = [&](const int32_t* k, int32_t s) {
    for (int j = 0; j + 1 < 8; ++j) {
      if (s <= stKey[j + 1]) {
        const int32_t span = stKey[j + 1] - stKey[j];
        if (span <= 0) return k[j + 1];
        return k[j] + static_cast<int32_t>(
            (static_cast<int64_t>(k[j + 1] - k[j]) * (s - stKey[j])) / span);
      }
    }
    return k[7];
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
      // u = 1 - (d/half)^2, then w = u^2 -- per-mille throughout
      const int32_t u = 1000 - static_cast<int32_t>(
          (static_cast<int64_t>(d) * d * 1000) /
          (static_cast<int64_t>(kKnuckleSwellHalfMm) * kKnuckleSwellHalfMm));
      const int32_t w = static_cast<int32_t>((static_cast<int64_t>(u) * u) / 1000);
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
    const int32_t tJ = blend_of(stJF), tN = blend_of(stNeck),
                  tA = blend_of(stA), tB = blend_of(stB), tC = blend_of(stC),
                  tD = blend_of(stD);
    if (tN == 0) {  // the buried base and the junction exit: root/junctionF
      rs.b0 = kBRoot;
      rs.b1 = kBJunctionF;
      rs.w0 = static_cast<uint8_t>(64 - tJ);
    } else if (tA == 0) {  // junctionF -> the new neck hinge
      rs.b0 = kBJunctionF;
      rs.b1 = kBNeck;
      rs.w0 = static_cast<uint8_t>(64 - tN);
    } else if (tB == 0) {  // neck -> A
      rs.b0 = kBNeck;
      rs.b1 = kBHingeA;
      rs.w0 = static_cast<uint8_t>(64 - tA);
    } else if (tC == 0) {  // A -> B
      rs.b0 = kBHingeA;
      rs.b1 = kBHingeB;
      rs.w0 = static_cast<uint8_t>(64 - tB);
    } else if (tD == 0) {  // B -> C
      rs.b0 = kBHingeB;
      rs.b1 = kBHingeC;
      rs.w0 = static_cast<uint8_t>(64 - tC);
    } else {  // C -> D and the aimed return arm
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
// The rig is UNCHANGED. kBHingeA/B/C, kBJunctionF and kBLoopBase2 all still
// exist and still drive the chain through the two-bone blend ladder in
// make_loop(); only their rigid ball parts are gone.

/**
 * One almond lens: a flattened ellipsoid swept along +Y (the almond's long
 * axis, counter-stretched like everything vertical), thin in X (the bulge
 * depth off the body), wide in Z. 8 segments facet visibly at 240p.
 */
inline zc::RingPart make_lens(uint8_t bone) {
  // The pass-3 ALMOND (X2's lens): a symmetric flattened ellipsoid. Kept
  // for the Stage E A/B; X1's teardrop below is the expected winner.
  zc::RingPart p;
  p.bone = bone;
  p.cap_base_fix = true;
  p.caps = zc::kCapTop | zc::kCapBot;
  for (int i = 0; i < kEyeRings; ++i) {
    const uint16_t theta = static_cast<uint16_t>(((2 * i + 1) * 32768LL) / (2 * kEyeRings));
    const int32_t sn = zref::fx_sin(zref::angle16{theta}).raw;
    const int32_t cs = zref::fx_cos(zref::angle16{theta}).raw;
    zc::RingSpec rs;
    rs.y = static_cast<int32_t>(((-static_cast<int64_t>(fxu(kEyeLongMm)) * cs) >> 16) *
                                kVStretchPm / 1000);
    rs.radius = 0;
    rs.rx = static_cast<int32_t>((static_cast<int64_t>(fxu(kEyeDeepMm)) * sn) >> 16);
    rs.rz = static_cast<int32_t>((static_cast<int64_t>(fxu(kEyeWideMm)) * sn) >> 16);
    rs.segments = static_cast<uint8_t>(kEyeFacetSegments);
    p.rings.push_back(rs);
  }
  p.r = kLensR;
  p.g = kLensG;
  p.b = kLensB;
  p.page = kPageEyeTile;
  return p;
}

/** X1: the TEARDROP lens (Direction 4 §3: "very pointy at the top, bottom
 *  is more round" -- an asymmetric per-ring profile no two-constant almond
 *  can express). Local +Y = the long axis (tilted into the Λ by the eye
 *  bone's rest); the width AND the dome depth follow kEyeRingWidthPm, and
 *  the apex rings crowd toward the tip (kEyeApexSharpPm). ~10 rings x 8
 *  segments ≈ 176 tris/eye against a 1,9xx-tri creature. */
inline zc::RingPart make_lens_teardrop(uint8_t bone) {
  zc::RingPart p;
  p.bone = bone;
  p.cap_base_fix = true;
  p.caps = zc::kCapTop | zc::kCapBot;
  for (int i = 0; i < kEyeRings2; ++i) {
    // stations run bottom (-Y, round) to top (+Y, pointy); the top interval
    // compresses by kEyeApexSharpPm so the last band is a sharp cone
    int32_t t_pm = 2000 * i / (kEyeRings2 - 1) - 1000;  // -1000..1000
    if (t_pm > 0) t_pm = t_pm * kEyeApexSharpPm / 1000 +
                         t_pm * t_pm / 1000 * (1000 - kEyeApexSharpPm) / 1000;
    zc::RingSpec rs;
    rs.y = static_cast<int32_t>(
        (static_cast<int64_t>(fxu(kEyeLongMm)) * t_pm / 1000) * kVStretchPm / 1000);
    rs.radius = 0;
    const int32_t w = kEyeRingWidthPm[i];
    rs.rx = static_cast<int32_t>((static_cast<int64_t>(fxu(kEyeDeepMm)) * w) / 1000);
    rs.rz = static_cast<int32_t>((static_cast<int64_t>(fxu(kEyeWideMm)) * w) / 1000);
    rs.segments = static_cast<uint8_t>(kEyeFacetSegments);
    p.rings.push_back(rs);
  }
  p.r = kLensR;
  p.g = kLensG;
  p.b = kLensB;
  p.page = kPageEyeTile;
  return p;
}

/** The WHITE ANNULUS (X1 and X2): a flat torus riding the PUPIL bone
 *  between lens face and star, so the white travels with the star through
 *  every gaze -- tracking BY CONSTRUCTION (the page ring mechanically
 *  cannot track; X3's refusal is recorded in the run log). Built as a
 *  closed ring of small circular sections around a circle in the local
 *  Y-Z plane, pushed +X by kWhiteRingOffXMm. The X-Z section is only an
 *  approximation of a true tube frame at the top/bottom stations -- at a
 *  ~1.5 px tube gauge the difference cannot survive 240p. */
inline zc::RingPart make_white_ring(uint8_t bone) {
  zc::RingPart p;
  p.bone = bone;
  p.cap_base_fix = true;
  p.caps = 0;  // a closed loop: first and last stations coincide
  for (int i = 0; i <= kWhiteRingSegs; ++i) {
    const uint16_t a = static_cast<uint16_t>((static_cast<int64_t>(i) * 65536 / kWhiteRingSegs) & 0xFFFF);
    zc::RingSpec rs;
    rs.y = static_cast<int32_t>(
        ((static_cast<int64_t>(fxu(kWhiteRingRMm)) *
          zref::fx_cos(zref::angle16{a}).raw) >> 16) * kVStretchPm / 1000);
    rs.cz = static_cast<int32_t>(
        (static_cast<int64_t>(fxu(kWhiteRingRMm)) * zref::fx_sin(zref::angle16{a}).raw) >> 16);
    rs.cx = fxu(kWhiteRingOffXMm);
    rs.radius = fxu(kWhiteRingTubeMm);
    rs.segments = 6;
    p.rings.push_back(rs);
  }
  p.r = 246;
  p.g = 242;
  p.b = 250;
  p.page = kPageAtlasTile;
  p.v0 = kWhiteV0;
  p.v1 = kWhiteV1;
  return p;
}

/**
 * Half of a pupil star: one thin blade. Two crossed blades per pupil bone
 * (the second quarter-turned) make the four-pointed cyan star — tiny
 * geometry riding its own bone, the zixx pupil lesson at creature-02 scale.
 */
inline zc::RingPart make_star_blade(uint8_t bone, bool crossed) {
  zc::RingPart p;
  p.bone = bone;
  p.cap_base_fix = true;
  p.caps = zc::kCapTop | zc::kCapBot;
  if (crossed) p.pitch_q = 1;  // quarter turn: the horizontal arm of the star
  // PASS 4 (Stage E): PER-AXIS arms -- the long arm rides the lens long
  // axis; the crossed (short) arm must FIT the lens half-width (the
  // reviewer's 185-vs-125 arithmetic ends here).
  const int32_t arm = crossed ? kPupilStarArmShortMm : kPupilStarArmLongMm;
  const int n = 3;
  for (int i = 0; i < n; ++i) {
    const uint16_t theta = static_cast<uint16_t>(((2 * i + 1) * 32768LL) / (2 * n));
    const int32_t sn = zref::fx_sin(zref::angle16{theta}).raw;
    const int32_t cs = zref::fx_cos(zref::angle16{theta}).raw;
    zc::RingSpec rs;
    rs.y = static_cast<int32_t>((-static_cast<int64_t>(fxu(arm)) * cs) >> 16);
    rs.radius = 0;
    rs.rx = static_cast<int32_t>((static_cast<int64_t>(fxu(kPupilStarThinMm)) * sn) >> 16);
    rs.rz = static_cast<int32_t>((static_cast<int64_t>(fxu(kPupilStarWideMm)) * sn) >> 16);
    rs.segments = 4;
    rs.cx = fxu(kEyeBulgeMm);  // proud of the lens; the gaze pivot radius
    p.rings.push_back(rs);
  }
  p.r = kStarR;
  p.g = kStarG;
  p.b = kStarB;
  // The page is REQUIRED, not decoration: an untextured part renders black
  // under celmain (09-ENGINE-GOTCHAS.md §7 — the toon ramp and the pre-lit
  // lanes disagree). The tile is flat cyan; kStar* stay as the fallback.
  p.page = kPageStarTile;
  return p;
}

}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_MODEL_H
