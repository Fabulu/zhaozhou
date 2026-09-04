// Unnamed02 — model builders: make_ball(), the body, the hinges.
//
// The consumer (zhao_reel.cpp / the probe) provides `namespace zc =
// zref::creature;` and the zref includes, the same contract zixxtrixx.h uses.
// Everything here is integer-only authoring over the generic ring builder.

#ifndef ZHAO_REEL_UNNAMED02_MODEL_H
#define ZHAO_REEL_UNNAMED02_MODEL_H

#include "unnamed02_art.h"
#include "unnamed02_rig.h"

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
  const int32_t total =
      kLoopBuryMm + kLoopArcMm[0] + kLoopArcMm[1] + kLoopArcMm[2] + kLoopArcMm[3] + kLoopBuryMm;
  // hinge stations along the tube, in mm from y0
  const int32_t stA = kLoopBuryMm + kLoopArcMm[0];
  const int32_t stB = stA + kLoopArcMm[1];
  const int32_t stC = stB + kLoopArcMm[2];
  const int32_t blend = 260;  // half-width of each hinge blend, mm of tube
  for (int i = 0; i < kLoopRings; ++i) {
    const int32_t s = static_cast<int32_t>((static_cast<int64_t>(total) * i) / (kLoopRings - 1));
    zc::RingSpec rs;
    rs.y = fxu(y0 + s);
    rs.radius = 0;
    rs.rx = fxu(kLoopBladeRxMm);
    rs.rz = fxu(kLoopBladeRzMm);
    rs.segments = static_cast<uint8_t>(kLoopSegments);
    // weights: root below A-blend; blend root->A across stA; A->B across stB;
    // B->C across stC; C above.
    const auto blend_of = [&](int32_t st) {
      int32_t t = ((s - (st - blend)) * 64) / (2 * blend);
      if (t < 0) t = 0;
      if (t > 64) t = 64;
      return t;  // 0 = fully lower bone, 64 = fully upper bone
    };
    const int32_t tA = blend_of(stA), tB = blend_of(stB), tC = blend_of(stC);
    if (tB == 0) {  // below the B corner: root/A
      rs.b0 = kBRoot;
      rs.b1 = kBHingeA;
      rs.w0 = static_cast<uint8_t>(64 - tA);
    } else if (tC == 0) {  // between: A/B
      rs.b0 = kBHingeA;
      rs.b1 = kBHingeB;
      rs.w0 = static_cast<uint8_t>(64 - tB);
    } else {  // top: B/C
      rs.b0 = kBHingeB;
      rs.b1 = kBHingeC;
      rs.w0 = static_cast<uint8_t>(64 - tC);
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

/** One rigid hinge ball on its own bone (bind translation AT the ball centre). */
inline zc::RingPart make_hinge(uint8_t bone) {
  zc::RingPart p =
      make_ball(kHingeRadiusMm, kHingeRings, kHingeSegments, kHingePoleSegments, bone);
  p.r = kHingeGreyR;
  p.g = kHingeGreyG;
  p.b = kHingeGreyB;
  p.page = kPageAtlasTile;
  p.v0 = kHingeV0;
  p.v1 = kHingeV1;
  return p;
}

/**
 * One almond lens: a flattened ellipsoid swept along +Y (the almond's long
 * axis, counter-stretched like everything vertical), thin in X (the bulge
 * depth off the body), wide in Z. 8 segments facet visibly at 240p.
 */
inline zc::RingPart make_lens(uint8_t bone) {
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
  const int n = 3;
  for (int i = 0; i < n; ++i) {
    const uint16_t theta = static_cast<uint16_t>(((2 * i + 1) * 32768LL) / (2 * n));
    const int32_t sn = zref::fx_sin(zref::angle16{theta}).raw;
    const int32_t cs = zref::fx_cos(zref::angle16{theta}).raw;
    zc::RingSpec rs;
    rs.y = static_cast<int32_t>((-static_cast<int64_t>(fxu(kPupilStarArmMm)) * cs) >> 16);
    rs.radius = 0;
    rs.rx = static_cast<int32_t>((static_cast<int64_t>(fxu(kPupilStarThinMm)) * sn) >> 16);
    rs.rz = static_cast<int32_t>((static_cast<int64_t>(fxu(kPupilStarWideMm)) * sn) >> 16);
    rs.segments = 4;
    p.rings.push_back(rs);
  }
  p.r = kStarR;
  p.g = kStarG;
  p.b = kStarB;
  return p;
}

}  // namespace u02

#endif  // ZHAO_REEL_UNNAMED02_MODEL_H
