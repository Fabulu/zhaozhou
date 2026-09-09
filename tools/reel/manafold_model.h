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
  // PASS 11 F.1: the fold blend is now the named PER-STATION table
  // kFoldBlendMm (manafold_art.h), not a local literal. It was a literal for
  // three passes -- an unnamed number deciding whether the antenna reads as a
  // chain or a hose, which is precisely the kind of value CLAUDE.md says must
  // be a named, editable knob. See that table for why it is a table.
  //
  // (Pass 8's history, kept because it is the risk this change runs in reverse:
  // 145 -> 165 was raised to fix "mitred corners". A fold blend is the corner's
  // turning radius, and at 145 mm on a band 126 mm wide the corner radius was
  // about one band width -- which is what "almost right-angled" describes. But
  // that was the KNUCKLE-LESS strap. With knuckles the tight blend reads as a
  // joint, checked by rendering it and looking.)
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
    // PASS 11 F.4: the half-width is now PER STATION (kKnuckleSwellHalfMm[5]),
    // so a body junction can be a long low thickening while a knuckle stays a
    // short proud lump. Same profile function; only the window changes.
    const auto swell = [&](int32_t at, int32_t half, int32_t rx_mm, int32_t rz_mm,
                           int32_t& best_x, int32_t& best_z) {
      const int32_t d = s > at ? s - at : at - s;
      if (d >= half) return;
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
          (static_cast<int64_t>(half) * half));
      const int32_t w = static_cast<int32_t>(
          (static_cast<int64_t>(u) * u * (3000 - 2 * u)) / 1000000);
      const int32_t ax = static_cast<int32_t>((static_cast<int64_t>(rx_mm) * w) / 1000);
      const int32_t az = static_cast<int32_t>((static_cast<int64_t>(rz_mm) * w) / 1000);
      if (ax > best_x) best_x = ax;
      if (az > best_z) best_z = az;
    };
    int32_t sw_x = 0, sw_z = 0;
    swell(kKnuckleAtJfMm, kKnuckleSwellHalfMm[0],
          kKnuckleSwellJfRxMm, kKnuckleSwellJfRzMm, sw_x, sw_z);
    swell(kKnuckleAtAMm, kKnuckleSwellHalfMm[1],
          kKnuckleSwellARxMm, kKnuckleSwellARzMm, sw_x, sw_z);
    swell(kKnuckleAtBMm, kKnuckleSwellHalfMm[2],
          kKnuckleSwellBRxMm, kKnuckleSwellBRzMm, sw_x, sw_z);
    swell(kKnuckleAtCMm, kKnuckleSwellHalfMm[3],
          kKnuckleSwellCRxMm, kKnuckleSwellCRzMm, sw_x, sw_z);
    swell(kKnuckleAtEndMm, kKnuckleSwellHalfMm[4],
          kKnuckleSwellEndRxMm, kKnuckleSwellEndRzMm, sw_x, sw_z);
    rs.rx = fxu(taper(kLoopBladeRxMm, s) + sw_x);
    rs.rz = fxu(taper(kLoopBladeRzMm, s) + sw_z);
    rs.segments = static_cast<uint8_t>(kLoopSegments);
    // weights: a ladder of two-bone blends across the five fold stations
    const auto blend_of = [&](int32_t st, int32_t bl) {
      int32_t t = ((s - (st - bl)) * 64) / (2 * bl);
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
    // The continuity condition this ladder depends on: a branch flips at
    // (station - blend), and the previous station's weight must already have
    // SATURATED by then, which needs consecutive stations >= 2*blend apart. It
    // is ASSERTED in the committed probe from kFoldBlendMm and kLoopArcMm, not
    // stated in a comment that the next station move would rot (checklist 19).
    const int32_t tN = blend_of(stNeck, kFoldBlendMm[0]),
                  tA = blend_of(stA, kFoldBlendMm[1]),
                  tB = blend_of(stB, kFoldBlendMm[2]),
                  tC = blend_of(stC, kFoldBlendMm[3]),
                  tD = blend_of(stD, kFoldBlendMm[4]);
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
    // ---- PASS 12: THE SPAN STRETCHES (Direction 9 SS13) -------------------
    // kRadial about the tube's BASE, on axis x: x contracts, y and z expand.
    // y is the arc, so the chain LENGTHENS and the balls move apart, while the
    // blade's broad in-plane half-width narrows -- an elastic band, not a
    // balloon. See kLoopStretchStrength for why the axis cannot be y, and for
    // the half of SS13 the single-sample sidecar cannot carry.
    //
    // THE CENTRE IS THE TUBE'S BASE, not the part's middle, and that is the
    // load-bearing choice: scaling about y0 leaves the buried base and the
    // front junction exactly where they are, so the antenna grows OUT of the
    // head instead of sliding out of it. A centre in the middle of the chain
    // would push the front junction into the body and pull the return arm out
    // of it -- the free-floating dongle, re-created by a deform.
    // ⚠ ROLE AND STRENGTH MUST AGREE. `compile_creature` rejects the whole
    // creature with "deformation role/strength mismatch" if a role is set and
    // the strength is 0 -- and a rejected compile renders NOTHING, not a
    // creature without the effect. Setting kLoopStretchStrength to 0 to turn
    // this off is the obvious thing to try, so the obvious thing must work:
    // the role goes to kNone with it. (Found by trying exactly that, as an
    // ablation, and getting an empty frame.)
    //
    // ⚠ THE STRENGTH RAMPS TO ZERO ALONG THE BURIED RETURN ARM, and this is
    // the whole reason the effect can ship at a visible amount.
    //
    // At full strength everywhere, the chain lengthens by ~7% -- which moves
    // the ARM'S BURIED END ~220 mm further along its own direction, straight
    // out through the far side of the body. The render showed it as a tube
    // stub poking from the ball's lower right, and the committed closure probe
    // reported nothing, because that probe measures BONE geometry and this is a
    // VERTEX effect. A gate blind spot, named here so the next person does not
    // trust silence.
    //
    // Capping the strength instead was tried: at 60 the stub is gone and the
    // stretch is also invisible -- ~1 px on a 340 mm span, which is exactly the
    // crayon-grain failure in CLAUDE.md (mathematically present, visually
    // absent). So the amount stays and the REACH changes: full strength over
    // the visible spans between the nodules, ramping to zero by the arm's end,
    // so the tip does not move at all.
    //
    // Displacement is strength*(y - y0), so a linear ramp keeps the mapping
    // continuous AND monotone: d/dy of [y + s(y)(y-y0)] = 1 + s'(y)(y-y0) +
    // s(y), which at the worst point here is 1 - 0.145 + 0.07 = 0.93 > 0. The
    // skin stretches; it never folds back on itself.
    //
    // It is also what SS13 actually asked for -- "the antennae parts BETWEEN
    // THE BLOBS", not the buried return.
    //
    // ==== WAVE 2a: THE THREE SPANS GET THEIR OWN LANES ====================
    //
    // Wave 1's single strength read the body's breath, so every span stretched
    // together by the same amount whatever the nodules were doing. Lanes 1..3
    // carry ONE SPAN EACH, driven by that span's own posed shortfall, so the
    // three are independent -- SS2's "any kind of configuration" reaching the
    // skin instead of stopping at the bones.
    //
    // THE AUTHORITIES ARE GEOMETRY, and this is the part that has to be right
    // or the antenna tears. A ring must carry not only ITS OWN span's stretch
    // but the accumulated stretch of every span BELOW it, or the chain parts
    // company at each span boundary. Since the deform displaces a vertex by
    //     disp = (spread/65536) * (strength/255) * (y - y0)
    // and lane i's sample IS its span's stretch fraction k_i, the authority a
    // ring at station s needs on lane i is
    //     strength_i(s) = 255 * clamp(s - start_i, 0, L_i) / s
    // -- the full span length once the ring is past it, a partial run while
    // the ring is inside it, zero below. That makes
    //     disp(s) = sum_i k_i * clamp(s - start_i, 0, L_i)
    // which is continuous at every boundary and monotone by construction
    // BELOW hinge C (each term's slope is 0 or 1, so dy/ds >= 1).
    //
    // ⚠ Being a SUM is why lanes had to be summed rather than selected. A ring
    // in span 3 needs k_1, k_2 AND k_3 at once; a per-vertex "which lane do I
    // read" would have had to pick one and would have torn the other two.
    //
    // THE RAMP PAST HINGE C survives verbatim from wave 1, and its reason is
    // unchanged: at full authority the accumulated growth pushes the buried
    // arm's END out through the far side of the body -- a tube stub poking
    // from the ball's lower right, which the committed closure probe cannot
    // see because it measures BONES and this is a VERTEX effect. Every lane's
    // authority ramps linearly to zero from stC to the tip, so the arm rides
    // along and its end does not move at all. This is the ONLY region where
    // dy/ds can go negative; kSpanStretchMaxPm is bounded so it cannot.
    //
    // ⚠ ROLE AND STRENGTH MUST STILL AGREE, now across every lane:
    // `compile_creature` rejects the whole creature -- rendering NOTHING, not
    // a creature without the effect -- if any lane has authority and the role
    // is kNone. So the role is set from whether ANY lane wants this ring.
    const int32_t span_start[3] = {stNeck, stA, stB};
    const int32_t span_len[3] = {kLoopArcMm[1], kLoopArcMm[2], kLoopArcMm[3]};
    int32_t ramp_num = 1, ramp_den = 1;
    if (s > stC) {
      ramp_num = total - s;
      ramp_den = total - stC;
      if (ramp_num < 0) ramp_num = 0;
      if (ramp_den <= 0) ramp_den = 1;
    }
    int32_t lane_st[3] = {0, 0, 0};
    for (int L = 0; L < 3; ++L) {
      int32_t run = s - span_start[L];
      if (run < 0) run = 0;
      if (run > span_len[L]) run = span_len[L];
      const int32_t st = s > 0 ? static_cast<int32_t>(
                                     (static_cast<int64_t>(run) * 255 * ramp_num) /
                                     (static_cast<int64_t>(s) * ramp_den))
                               : 0;
      lane_st[L] = st < 0 ? 0 : (st > 255 ? 255 : st);
    }
    // lane 0: the wave-1 breath coupling, kept as a live knob and OFF by
    // default (kLoopStretchStrength). Same ramp, same reason.
    int32_t stretch = kLoopStretchStrength;
    if (s > stC)
      stretch = static_cast<int32_t>(
          (static_cast<int64_t>(kLoopStretchStrength) * ramp_num) / ramp_den);
    if (stretch < 0) stretch = 0;
    const bool any_lane =
        stretch > 0 || lane_st[0] > 0 || lane_st[1] > 0 || lane_st[2] > 0;
    rs.deform_role = any_lane ? zc::DeformRole::kRadial : zc::DeformRole::kNone;
    rs.deform_axis = 0;
    rs.deform_strength = static_cast<uint8_t>(stretch);
    for (int L = 0; L < 3; ++L)
      rs.deform_strength_ex[L] = static_cast<uint8_t>(any_lane ? lane_st[L] : 0);
    rs.deform_center_x = fxu(kLoopTubeXMm);
    rs.deform_center_y = fxu(y0);
    rs.deform_center_z = 0;
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
// The rig is UNCHANGED: kBHingeA/B/C all still exist and still drive the chain
// through the two-bone blend ladder in make_loop(); only their rigid ball parts
// are gone.
//
// PASS 11 (QA §7.5): kBJunctionF WAS IN THAT LIST AND DOES NOT BELONG THERE
// EITHER -- the same fault as kBLoopBase2 below, in the same sentence. It is a
// pure PARENT: it shares kBNeck's pivot exactly (kLoopArcMm[0] == 0), so
// weighting a ring to it would weight the same point twice, and line ~190 of
// make_loop says so in as many words. A rotation on it still bends the whole
// antenna, because every loop bone descends from it -- which is exactly why
// the false version read as true.
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
/**
 * PASS 15 (D11 SS2.3) -- THE EYES RIDE THE BOUNCING SURFACE.
 *
 *   "when the bouncy body expands, as it should, they clip into it. We said
 *    they should be attached to the bouncy part. Well, they're probably
 *    attached higher up and the lower part is what bounces."
 *
 * His diagnosis is the right SHAPE with one refinement, and the refinement is
 * why four passes of moving the eyes could not fix it: THE EYES ARE ON BONES
 * AND THE BOUNCE IS A VERTEX DEFORM. `deform_skin_vertex_lanes` scales marked
 * VERTICES radially about the ball centre (creature_core.cpp); no bone in the
 * rig ever reads that sample, so no amount of re-parenting or re-placing an
 * eye could couple it. There is no bone to attach to -- that is the whole
 * fault. `kEyeStandoffMm` is the static answer that has been standing in for
 * it, and a static standoff can only trade "floating off the face" against
 * "eaten by the breath": at kCompressAmpPm 16500 the surface at eye height
 * travels further than any standoff anyone would accept at rest.
 *
 * THE FIX IS THE FOLLOWER ROLE, WHICH ALREADY EXISTED. A follower vertex
 * inherits the ellipsoidal displacement of its own ring's CARRIER POINT as a
 * pure TRANSLATION -- so the eye assembly is carried by the surface under it
 * while staying rigid. It does NOT inflate with the breath, which was the
 * risk worth naming: the eyes travel with the skin, they do not swell.
 *
 * ⚠ AND THE AUTHORITY IS THE BODY'S OWN, AT THE EYE'S OWN HEIGHT. The number
 * is not chosen, it is read off make_body's equator-peaked ramp at the ring's
 * height and scaled by ONE knob. An independently authored strength here is a
 * second copy of the body's breath profile, and a second copy drifts the first
 * time somebody re-proportions the ball.
 *
 * ⚠ IT ALSO HAS ITS OWN KNOWN-NEGATIVE, and it is exact rather than
 * approximate: kEyeDeformFollowPm = 0 leaves deform_role kNone, no sidecar is
 * emitted at all, and the eye vertices take the identity early-out in
 * deform_skin_vertex_lanes. Pass-14's frames come back byte for byte.
 *
 * ⚠ WHAT THIS DOES NOT DO. A hard hit still exceeds any arrangement -- the
 * squash at f28 of `hit` moves the surface further than the eye is proud --
 * and that residual is DECLARED, in the sense the ground-contact law means:
 * an authored, stated intersection, whose remaining job is to read as the eye
 * entering gas rather than as a plate cut by a line. That half is the fog, it
 * is LANE-FX's, and this comment is the handover.
 */
inline bool eye_bone_is_left(uint8_t bone) { return bone == kBEyeL || bone == kBPupilL; }

inline void eye_deform_follow(zc::RingPart& p) {
  const int32_t bind_y_fx = fxu(vmm(kEyeYMm));
  if (kEyeDeformFollowPm <= 0) return;
  const int32_t bx = fxu(eye_bind_x_mm());
  const int32_t bz = fxu(eye_bind_z_mm());
  const int32_t R = fxu(kBodyRadiusMm);
  const int32_t half_h = static_cast<int32_t>(static_cast<int64_t>(R) * kVStretchPm / 1000);
  for (zc::RingSpec& rs : p.rings) {
    // make_body's own strength ramp, sampled at THIS ring's bind height.
    const int32_t wy = bind_y_fx + rs.y;
    const int64_t a = wy < 0 ? -wy : wy;
    int32_t s = static_cast<int32_t>(255 - (a * 255) / (half_h > 0 ? half_h : 1));
    if (s < 0) s = 0;
    s = static_cast<int32_t>((static_cast<int64_t>(s) * kEyeDeformFollowPm) / 1000);
    if (s < 1) s = 1;   // the compile rejects role-set-with-zero-strength
    if (s > 255) s = 255;
    rs.deform_role = zc::DeformRole::kFollower;
    rs.deform_axis = 1;  // vertical, the same axis the body breathes on
    rs.deform_strength = static_cast<uint8_t>(s);
    // The centre the displacement is measured from is the BALL'S centre, and
    // the compiler adds this part's bone bind offset to whatever is authored
    // here -- so the authored value is the NEGATIVE of that offset. Getting
    // this wrong does not fail a compile; it moves the eye toward a point
    // that is not the body, which is why it is written out rather than
    // hidden in a constant.
    rs.deform_center_x = -bx;
    rs.deform_center_y = -bind_y_fx;
    rs.deform_center_z = eye_bone_is_left(p.bone) ? -bz : bz;
  }
}

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
    p.rings.push_back(rs);
  }
  p.r = kLensR;
  p.g = kLensG;
  p.b = kLensB;
  p.page = kPageEyeTile;
  eye_deform_follow(p);   // D11 SS2.3: ride the bouncing surface
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
  const auto sc = [](int32_t v) {
    return static_cast<int32_t>((static_cast<int64_t>(v) * kStarScalePm) / 1000);
  };
  const int32_t rim = white ? kStarWhiteRimMm : 0;
  const int32_t bot = sc(kStarArmBottomMm) + rim;
  const int32_t top = sc(kStarArmTopMm) + rim;
  // ⚠ THE RIM IS A DILATION IN THE PICTURE PLANE ONLY -- it must NOT thicken
  // the white in DEPTH. Authored the other way first and it cost a render to
  // find: a white slab 2*(thin+rim) deep centred on the pupil swallowed the
  // thinner cyan whole, so the star drew as a white splinter with no cyan
  // anywhere, from every angle. Both stars are the same thickness; only their
  // outlines differ, which is what the sheet draws.
  // PASS 11 E.2: each star states its own depth -- the cyan is a form, the
  // white is an outline. Splitting this is what retires the near-eye bar.
  const int32_t thin = white ? kStarWhiteThinMm : kStarCyanThinMm;
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
    // PASS 12 WAVE 2a (D9 SS12.1/SS12.2): the star finally gets the vertical
    // pre-stretch the lens has always had, and a registration term that puts
    // its asymmetric drawn MASS on the lens centre rather than its origin. See
    // kStarCentreYMm for why "kStarOffsetYMm is already 0" was not the same
    // thing as "the star is centred".
    rs.y = static_cast<int32_t>(
        ((static_cast<int64_t>(fxu(arm)) * yp / 1000) + fxu(kStarOffsetYMm) +
         fxu(kStarCentreYMm)) *
        kStarVStretchPm / 1000);
    rs.radius = 0;
    rs.rx = fxu(thin);
    rs.rz = fxu(w);
    rs.segments = 6;
    // proud of the lens along +X: the pupil bone pivots at the lens centre, so
    // this stand-off IS the gaze pivot radius. The cyan rides a hair further
    // out than its white, so it sits ON it and never sinks into it.
    // Depth ordering: white front = kEyeBulgeMm + kStarWhiteThinMm; cyan front
    // = kEyeBulgeMm + kStarCyanProudMm + kStarCyanThinMm. ASSERTED IN THE PROBE
    // (pass 11 E.2), not merely stated -- an inverted pair shipped once.
    rs.cx = fxu(kEyeBulgeMm + (white ? 0 : kStarCyanProudMm));
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
  // D11 SS2.3: the star rides the same surface as its lens. It MUST take the
  // same authority as make_eye_lens, or the star slides against the lens it
  // sits in -- the one property SS5b says these two parts may never lose.
  eye_deform_follow(p);
  return p;
}

}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_MODEL_H
