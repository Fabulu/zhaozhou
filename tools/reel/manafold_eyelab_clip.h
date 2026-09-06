// MANAFOLD — THE EYE LAB, clip half. LANE-ONLY, SHIPS NOTHING.
//
// manafold_eyelab.h carries the constants and the variant table and is
// included from manafold_model.h (the model needs the star scale and the
// deform opt-in). This half needs `Rig`, so it is included after
// manafold_clips.h. The split is only an include-order fact; the two files
// are one instrument.
//
// ---- THE CHOREOGRAPHY, and why it is shaped like this --------------------
//
// The lane has to answer four questions on ONE clip, because a variant is
// only comparable against another variant that saw the same frames.
//
//   beat 1  REST          the Front.png placement, held still. The control
//                         read: is the eye where the sheet puts it.
//   beat 2  TRAVEL OUT    the arc, eased, to the variant's full angle, and
//                         HELD. The hold is where a placement becomes
//                         judgeable -- the mana lab's lesson, and it applies
//                         to a position exactly as it applied to a shape.
//   beat 3  BODY YAW      the creature turns under the travelled eyes, so the
//                         near eye is dragged through every obliquity from
//                         face-on to edge-on. THIS IS THE NEAR-EYE BAR BEAT.
//                         It is not a separate experiment: §12.4 says the side
//                         extreme of travel hits the bar head-on, so the bar
//                         is measured on the travel clip or it is not measured
//                         under the conditions that matter.
//   beat 4  BLINK         three blinks at the travelled position, then three
//                         back at rest -- a blink has to survive both.
//   beat 5  RETURN        back to the Front placement. An animation, so it
//                         comes back; and the return is where a mechanism
//                         that only looks right at one end gives itself away.
//
// The BREATH runs underneath all five beats, at the shipping amplitude, and
// the travel hold is deliberately long enough to contain several full breath
// cycles -- §7.7 asks for the extremes of the breath, not the rest frame, and
// a hold shorter than a breath cannot show one.

#ifndef ZHAO_REEL_MANAFOLD_EYELAB_CLIP_H
#define ZHAO_REEL_MANAFOLD_EYELAB_CLIP_H

#include "manafold_clips.h"
#include "manafold_eyelab.h"

namespace u02 {
namespace eyelab {

// Keys; frames on screen = 2x. 260 keys = 520 frames = 8.7 s at 60 fps.
constexpr int kEyeLabKeys = 260;
constexpr int kBeatRestEnd = 30;      // beat 1 ends
constexpr int kBeatOutEnd = 60;       // the arc has finished travelling
constexpr int kBeatYawEnd = 150;      // the body has turned under the eyes
constexpr int kBeatBlinkEnd = 215;    // the blinks are done
                                      // 215..260 is the return

// The body yaw across beat 3. A quarter turn takes the near eye from the
// three-quarter the camera already frames to fully edge-on and back past it,
// which is the whole obliquity range the bar lives in.
constexpr int32_t kEyeLabYawA16 = 16384;   // 90 deg
// The breath. The shipping idle uses compress_at over kIdleKeys; this is the
// same wave at a period chosen so the travel hold spans three full cycles.
constexpr int kEyeLabBreathCycles = 9;
// The blink schedule inside beat 4: two at the travelled position, two at rest
// -- a blink mechanism that only survives one of those is not a blink.
constexpr int kBlinkKeys[] = {160, 178, 194, 206};
constexpr int kBlinkKeysN = static_cast<int>(sizeof(kBlinkKeys) / sizeof(int));

/** THE TRAVEL POSE. Positive `pm` carries each eye OUTWARD toward its own
 *  flank -- MIRRORED, not common. A single shared rotation would take one eye
 *  to the flank and the other toward the nose, which is a HEAD TURN, not eye
 *  travel; the sheets show both eyes out on their own sides. */
inline void apply_eye_travel_deg(Rig& g, int32_t deg, int32_t pm) {
  if (pm > 1000) pm = 1000;
  if (pm < -1000) pm = -1000;
  const int32_t a = static_cast<int32_t>(
      (static_cast<int64_t>(travel_a16(deg)) * pm) / 1000);
  // ⚠ THE SIGN WAS WRONG IN THE FIRST BUILD, AND THE PROBE IS WHAT FOUND IT.
  // Authored as quat_y(+a) on the left, it carried both eyes INWARD across the
  // midline instead of outward to the flanks: the committed probe reported the
  // eye-to-eye gap reaching 0 mm at EVERY travel angle from 6 deg upward, and
  // the plates showed the Lambda inverting into an X.
  //
  // AND THE COLLISION SAT IN THE INTERIOR OF THE RANGE -- key 37 to 59, on the
  // ramp, not at the extreme. At the full 45 deg the eyes had already passed
  // THROUGH each other and come out the far side with a comfortable gap, so a
  // gate that checked only the travel extreme would have reported it clean.
  // That is 09-ENGINE-GOTCHAS 17 exactly (roll: 98 mm at 0 deg, 0 mm at 7,
  // 18 mm at 10), reproduced on a different channel within one afternoon.
  g.q[kBEyeTravelL] = quat_y(-a);
  g.q[kBEyeTravelR] = quat_y(a);
}
inline void apply_eye_travel(Rig& g, int32_t pm) {
  apply_eye_travel_deg(g, variant().travel_deg, pm);
}

/** The blink's contribution to the deform sample at key f, in per-mille.
 *  0 outside every blink window. */
inline int32_t blink_pm_at(int f) {
  for (int i = 0; i < kBlinkKeysN; ++i) {
    const int t = f - kBlinkKeys[i];
    if (t >= 0 && t < kBlinkLenKeysLab) return blink_env_pm(t);
  }
  return 0;
}

/** Ease 0..1000 over [a,b) -- smoothstep-ish, integer, no reversals. */
inline int32_t ease_pm(int f, int a, int b) {
  if (f <= a) return 0;
  if (f >= b) return 1000;
  const int64_t t = static_cast<int64_t>(f - a) * 1000 / (b - a);
  return static_cast<int32_t>((t * t * (3000 - 2 * t)) / 1000000);
}

/** THE LAB CLIP (slot 16). One choreography, every variant. */
inline zc::Clip build_eyelab() {
  const int K = kEyeLabKeys;
  zc::Clip c = clip_shell(16, K, kHoverHeightMm);
  const EyeVariant& V = variant();
  for (int f = 0; f < K; ++f) {
    Rig g;
    g.reset();
    antenna_knead(g, 0, K, f);  // the always-on fold layer, as every clip runs it
    loop_rest(g);
    face_rest(g);

    // ---- beat 2/5: the arc out, the hold, and the return ----------------
    int32_t travel_pm = 0;
    if (f < kBeatRestEnd) travel_pm = 0;
    else if (f < kBeatOutEnd) travel_pm = ease_pm(f, kBeatRestEnd, kBeatOutEnd);
    else if (f < kBeatBlinkEnd) travel_pm = 1000;
    else travel_pm = 1000 - ease_pm(f, kBeatBlinkEnd, K - 1);
    apply_eye_travel(g, travel_pm);

    // ---- beat 3: the body turns UNDER the travelled eyes -----------------
    // Out and back, so the near eye sweeps through edge-on twice and the
    // return shows whether the read recovers or the eye stayed broken.
    if (f >= kBeatOutEnd && f < kBeatYawEnd) {
      const int span = kBeatYawEnd - kBeatOutEnd;
      const int32_t w = sinp(f - kBeatOutEnd, span, 1, 0);  // -65536..65536
      const int32_t yaw = static_cast<int32_t>(
          (static_cast<int64_t>(kEyeLabYawA16) * w) >> 16);
      g.q[kBRoot] = quat_mul(g.q[kBRoot], quat_y(yaw));
    }

    // ---- the composed-extremes gate variants (§5d) -----------------------
    // Travel + roll + gaze + twinkle on the SAME frames, not one at a time.
    // "Each can pass its own limit while the combination collides."
    if (V.compose_extremes && f >= kBeatOutEnd && f < kBeatYawEnd) {
      apply_eye_roll(g, 1000, 1000);          // full INWARD roll: the collision sign
      apply_gaze(g, kGazeMaxA16, kGazeLiftMaxA16);
      apply_twinkle(g, kStarTwinkleMaxA16);
    } else {
      // an ordinary alive gaze so the eyes are never dead in a plate
      apply_gaze(g, static_cast<int32_t>(
                        (static_cast<int64_t>(kGazeMaxA16) * sinp(f, K, 3, 0)) >> 16) / 2,
                 kGazeLiftMaxA16 / 4);
    }

    g.write(c, f);

    // ---- the deform sidecar: the breath, and the blink on the same lane --
    zc::DeformSample d = compress_at(f, K, kEyeLabBreathCycles, kCompressAmpPm);
    if (V.blink) {
      const int32_t b = blink_pm_at(f);
      if (b > 0) {
        // ONE CHANNEL. The blink cannot be scheduled independently of the
        // breath (see manafold_eyelab.h): all it can do is dominate the
        // sample while it lasts. `blink_split` is the variant that then cuts
        // the BODY's own deform_strength so the same signal reads as an eye
        // event rather than a whole-creature squash.
        const int32_t flat = static_cast<int32_t>(
            (static_cast<int64_t>(kBlinkSquashPm) * b) / 1000 * 65536 / 1000);
        if (flat > d.flatten) {
          d.flatten = static_cast<uint16_t>(flat > 60000 ? 60000 : flat);
          d.spread = static_cast<uint16_t>(
              (static_cast<int64_t>(d.flatten) * kSpreadRatioPm) / 1000);
        }
      }
    }
    c.deform[static_cast<size_t>(f)] = d;
  }
  return c;
}

}  // namespace eyelab
}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_EYELAB_CLIP_H
