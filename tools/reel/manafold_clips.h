// MANAFOLD (creature 02) — clip builders + the maths helpers.
//
// The rotation/curve maths below is the ONE SANCTIONED COPY from
// zixxtrixx.h (pure quat/curve maths, no anatomy). Everything else is this
// creature's own authoring: deterministic integer clip builders, the deform
// sidecar (the constant compression), the hover.
//
// MOTION LAWS OBSERVED (07-MOTION-STYLE):
//  - 30 Hz keys held 2 ticks, presentation interpolation on, hard cuts.
//  - every periodic term completes INTEGER cycles per clip (seamless loops).
//  - the life layer is seasoning and NEVER off (no byte-identical frames).
//  - one thing at a time, each beat >= 8 keys (16 frames) to register.
//  - speed spent on payoffs; wind-ups still >= 8 keys.

#ifndef ZHAO_REEL_MANAFOLD_CLIPS_H
#define ZHAO_REEL_MANAFOLD_CLIPS_H

#include "manafold_art.h"
#include "manafold_rig.h"

namespace u02 {

// ---- rotation helpers (copied verbatim from zixxtrixx.h — pure maths) -----
// The quaternion takes the HALF angle, which is why every amplitude is about
// twice the visible swing.
inline zc::quat16 quat_axis(int32_t ax, int32_t ay, int32_t az, int32_t a) {
  const zref::angle16 h{static_cast<uint16_t>((a >> 1) & 0xFFFF)};
  return zc::quat16_axis_angle(zref::fx16{ax}, zref::fx16{ay}, zref::fx16{az}, zref::fx_sin(h),
                               zref::fx_cos(h));
}
inline zc::quat16 quat_x(int32_t a) { return quat_axis(1 << 16, 0, 0, a); }
inline zc::quat16 quat_y(int32_t a) { return quat_axis(0, 1 << 16, 0, a); }
inline zc::quat16 quat_z(int32_t a) { return quat_axis(0, 0, 1 << 16, a); }

/** Hamilton product of two quat16, S 1.0.14 lanes, ONE rescale(.,14) per lane. */
inline zc::quat16 quat_mul(const zc::quat16& a, const zc::quat16& b) {
  const int64_t aw = a.q[0], ax = a.q[1], ay = a.q[2], az = a.q[3];
  const int64_t bw = b.q[0], bx = b.q[1], by = b.q[2], bz = b.q[3];
  const auto r = [](int64_t v) {
    int64_t q = (v + (1 << 13)) >> 14;
    if (q > zc::kQuatOne) q = zc::kQuatOne;
    if (q < -zc::kQuatOne) q = -zc::kQuatOne;
    return static_cast<int16_t>(q);
  };
  return zc::quat16{{r(aw * bw - ax * bx - ay * by - az * bz),
                     r(aw * bx + ax * bw + ay * bz - az * by),
                     r(aw * by - ax * bz + ay * bw + az * bx),
                     r(aw * bz + ax * by - ay * bx + az * bw)}};
}

/** quaternion conjugate (unit inverse). */
inline zc::quat16 quat_conj(const zc::quat16& q) {
  return zc::quat16{{q.q[0], static_cast<int16_t>(-q.q[1]), static_cast<int16_t>(-q.q[2]),
                     static_cast<int16_t>(-q.q[3])}};
}

/** Rotate an integer vector by a quat16 (S 1.0.14 lanes), integer-only. */
inline void quat_rot_vec(const zc::quat16& q, int32_t vx, int32_t vy, int32_t vz, int32_t& ox,
                         int32_t& oy, int32_t& oz) {
  const int64_t w = q.q[0], x = q.q[1], y = q.q[2], z = q.q[3];  // 2^14 = 1.0
  const int64_t tx = 2 * (y * vz - z * vy);                      // scale 2^14
  const int64_t ty = 2 * (z * vx - x * vz);
  const int64_t tz = 2 * (x * vy - y * vx);
  ox = vx + static_cast<int32_t>((w * tx + y * tz - z * ty) >> 28);
  oy = vy + static_cast<int32_t>((w * ty + z * tx - x * tz) >> 28);
  oz = vz + static_cast<int32_t>((w * tz + x * ty - y * tx) >> 28);
}

/** asin in angle16 by integer bisection on fx_sin. Monotone, deterministic. */
inline int32_t asin16(int32_t dh, int32_t L) {
  const bool neg = dh < 0;
  if (neg) dh = -dh;
  if (dh >= L) return neg ? -16384 : 16384;
  const int64_t target = (static_cast<int64_t>(dh) << 16) / L;
  int32_t lo = 0, hi = 16384;
  for (int i = 0; i < 18; ++i) {
    const int32_t mid = (lo + hi) / 2;
    if (zref::fx_sin(zref::angle16{static_cast<uint16_t>(mid)}).raw < target) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return neg ? -hi : hi;
}

// piecewise-linear keyed curve in thousandths, integer, clamped at both ends
struct Key {
  int f;
  int v;
};
inline int curve(const Key* k, int n, int f) {
  if (f <= k[0].f) return k[0].v;
  for (int i = 0; i + 1 < n; ++i) {
    if (f >= k[i].f && f <= k[i + 1].f) {
      const int span = k[i + 1].f - k[i].f;
      if (span <= 0) return k[i + 1].v;
      return k[i].v + ((k[i + 1].v - k[i].v) * (f - k[i].f) + span / 2) / span;
    }
  }
  return k[n - 1].v;
}
// ---- end of the sanctioned copy -------------------------------------------

/** sin of (f/keys)*cycles turns, Q16.16 — integer cycles keep loops seamless. */
inline int32_t sinp(int f, int keys, int cycles, int32_t phase16 = 0) {
  const uint16_t a = static_cast<uint16_t>(
      ((static_cast<int64_t>(f) * cycles * 65536) / keys + phase16) & 0xFFFF);
  return zref::fx_sin(zref::angle16{a}).raw;
}

/** The per-key quat accumulator (mirrors zixx's Rig; bodies differ). */
struct Rig {
  zc::quat16 q[kBoneCount];
  void reset() {
    for (int b = 0; b < kBoneCount; ++b) q[b] = zc::quat16_identity();
  }
  void write(zc::Clip& c, int f) const {
    for (int b = 0; b < kBoneCount; ++b)
      c.quats[static_cast<size_t>(f) * kBoneCount + b] = q[b];
  }
};

/** Integer square root (64-bit), for the closure aim's vector magnitude. */
inline int64_t isqrt64(int64_t v) {
  if (v <= 0) return 0;
  int64_t r = v;
  int64_t last = 0;
  for (int i = 0; i < 40 && r != last; ++i) {
    last = r;
    r = (r + v / r) / 2;
  }
  return r;
}

/** angle16 of a direction vector under the loop-plane convention
 *  dir(a) = (-sin a, +cos a): returns a with sin(a) = -vx/|v|, cos(a) = vy/|v|. */
inline int32_t angle16_of(int64_t vx, int64_t vy) {
  const int64_t mag = isqrt64(vx * vx + vy * vy);
  if (mag == 0) return 0;
  const int32_t s = asin16(static_cast<int32_t>((-vx * 60000) / mag), 60000);
  return vy >= 0 ? s : 32768 - s;
}

/**
 * The loop pose: the constant drawn-shape fold (all scales 1000), and the
 * articulation vocabulary — per-hinge per-mille scaling of the fold angles
 * at the neck and hinges A..C, plus an out-of-plane tilt. Every clip speaks
 * through these.
 *
 * THE LOOP CLOSES BY CONSTRUCTION (pass 2, R3): hinge D's fold is not an
 * authored angle — it is computed here, per key, so the return arm's last
 * segment always AIMS at the re-entry anchor (kLoopReentry*, = kBLoopBase2's
 * bind). Whatever the fold scales do, the arm plunges into the body: the
 * floating dongle and the punch-through are unrepresentable. This is
 * closed-form pose arithmetic (the root-compensation precedent), not IK.
 * `d_play_a16` adds hinge-play ON TOP of the closure aim (taunts): the aim
 * still lands the arm because the play is bounded and the segment overshoots
 * deep past the anchor.
 */
inline void loop_pose(Rig& g, int32_t neck_pm, int32_t a_pm, int32_t b_pm, int32_t c_pm,
                      int32_t tilt_a16 = 0, int32_t d_play_a16 = 0,
                      int32_t tilt_b_a16 = 0, int32_t tilt_c_a16 = 0) {
  const auto a = [](int32_t base, int32_t pm) {
    return static_cast<int32_t>((static_cast<int64_t>(base) * pm) / 1000);
  };
  const int32_t fn = a(kLoopFoldNeckA16, neck_pm);
  const int32_t fa = a(kLoopFoldAA16, a_pm);
  const int32_t fb = a(kLoopFoldBA16, b_pm);
  const int32_t fc = a(kLoopFoldCA16, c_pm);
  // PASS 4: the drawn kink/lean is a REST attitude on the FRONT JUNCTION
  // bone (the old neck bind — accepted silhouette preserved verbatim).
  // The NEW kBNeck hinge is identity at rest: a pure articulation joint
  // the knead layer drives; the closure walk composes whatever it carries.
  const zc::quat16 loc_junction = quat_mul(quat_y(kNeckRestYawA16), quat_z(fn));
  const zc::quat16 loc_a =
      quat_mul(quat_z(fa), quat_x(kLoopRestTiltA16 + tilt_a16));
  // PASS 6 C.1: B and C gain their out-of-plane axis. They were quat_z ONLY,
  // which is why "each hinge moves up and down separately" was geometrically
  // impossible however hard the amplitudes were pushed.
  const zc::quat16 loc_b = quat_mul(quat_z(fb), quat_x(kLoopRestTiltBA16 + tilt_b_a16));
  const zc::quat16 loc_c = quat_mul(quat_z(fc), quat_x(kLoopRestTiltCA16 + tilt_c_a16));
  g.q[kBJunctionF] = quat_mul(g.q[kBJunctionF], loc_junction);
  g.q[kBHingeA] = quat_mul(g.q[kBHingeA], loc_a);
  g.q[kBHingeB] = quat_mul(g.q[kBHingeB], loc_b);
  g.q[kBHingeC] = quat_mul(g.q[kBHingeC], loc_c);
  // ---- the closure aim, in 3D: quaternion-walk the chain to hinge D
  // exactly as the pose composes it (yaw, tilt AND any knead rotations
  // already sitting on the junction/neck/hinge bones), then choose D's
  // Z-fold so the last segment points at the re-entry anchor. A Z-fold
  // can only aim within D's local XY plane, so the out-of-plane residual is
  // projected away — the anchor is deep enough that the committed closure
  // probe still proves burial across the whole fold-scale range.
  int32_t px = kLoopTubeXMm, py = kLoopNeckExitYMm, pz = 0;
  zc::quat16 Q = g.q[kBJunctionF];
  const zc::quat16 locs[4] = {g.q[kBNeck], g.q[kBHingeA], g.q[kBHingeB],
                              g.q[kBHingeC]};
  for (int i = 0; i < 5; ++i) {
    int32_t dx, dy, dz;
    quat_rot_vec(Q, 0, kLoopArcMm[i], 0, dx, dy, dz);
    px += dx;
    py += dy;
    pz += dz;
    if (i < 4) Q = quat_mul(Q, locs[i]);
  }
  int32_t vx, vy, vz;  // anchor - P_D, taken into C's local frame
  quat_rot_vec(quat_conj(Q), kLoopReentryXMm - px, kLoopReentryYMm - py, -pz, vx, vy, vz);
  // ---- PASS 6 C.4: THE CLOSURE AIM IS NOW 3D ------------------------------
  // It used to be a Z-FOLD ONLY, and the source said so in its own words: "a
  // Z-fold can only aim within D's local XY plane, so the out-of-plane
  // residual is projected away". That was safe while every hinge was quat_z
  // and the residual was ~zero. C.1 gives B and C a real out-of-plane axis,
  // which grows exactly that residual -- and the committed probe caught it
  // immediately: worst arm rim 1539 pm against a 1120 gate, the return arm
  // visibly missing its re-entry. This was risk 3 in the architecture and it
  // fired on the first build, which is why it was worth checking first.
  //
  // The fix is bounded to this function: aim in TWO stages instead of one.
  // First the in-plane swing exactly as before; then re-express the target in
  // the frame that swing produces -- where its x-component is zero by
  // construction -- and lift out of plane by a rotation about D's own local X.
  // No square root and no iteration: quat_rot_vec and angle16_of already do
  // both halves. Composed as quat_z * quat_x, so the lift happens in the
  // swung frame.
  const int32_t aim_z = angle16_of(vx, vy);
  int32_t wx, wy, wz;
  quat_rot_vec(quat_conj(quat_z(aim_z)), vx, vy, vz, wx, wy, wz);
  const int32_t aim_x = angle16_of(-wz, wy);
  g.q[kBHingeD] = quat_mul(g.q[kBHingeD],
                           quat_mul(quat_z(aim_z + d_play_a16), quat_x(aim_x)));
}
inline void loop_rest(Rig& g) { loop_pose(g, 1000, 1000, 1000, 1000); }

/** The face at rest: lenses rolled into their outward V and leaned back. */
inline void face_rest(Rig& g) {
  g.q[kBEyeL] = quat_mul(quat_y(kEyeYawOutA16),
                         quat_mul(quat_x(kEyeVAngleA16), quat_z(-kEyeTiltA16)));
  g.q[kBEyeR] = quat_mul(quat_y(-kEyeYawOutA16),
                         quat_mul(quat_x(-kEyeVAngleA16), quat_z(-kEyeTiltA16)));
}

/** PASS 6 (Direction 5 §5c): "The eye itself can move a bit too."
 *  `pm` is a fraction of kEyeShiftMaxPm; the result is the rotation about the
 *  eye bone's relocated pivot that slides the assembly that far across the
 *  body. Small-angle: a16 ~= (shift_mm / pivot) * 65536 / 2pi, and 10430 is
 *  65536/2pi. Clamped to the owner's cap by construction. */
inline int32_t eye_shift_a16(int32_t pm) {
  // PASS 7 -- THE 916.7 DEGREE FALLBACK. kEyeShiftPivotMm is 0 because pass 6
  // measured the pivot-relocation mechanism unsound against face_rest's rest
  // attitude and declined to ship it. But the guard below divided by *1* in
  // that case rather than returning no shift, so eye_shift_a16(1000) returned
  // 100155 a16 -- 550 degrees per axis, 916.7 degrees composed. Nothing in a
  // clip called it, so nothing shipped bent; but the COMPOSED-EXTREMES GATE
  // calls it, which meant the one 5d gate that is actually enforced was
  // measuring eyes flipped through most of two full turns.
  //
  // An unshipped mechanism must contribute NOTHING, not garbage. While the
  // pivot is 0 this returns 0 and the gate honestly measures the two channels
  // that ARE shipped (roll and gaze). When the pivot is authored the small
  // angle formula below takes over unchanged.
  //
  // DECLARED GAP: Direction 5 5c's "the eye itself can move a bit too" is
  // therefore still NOT SHIPPED at the end of pass 7. It is not silently
  // absent -- kEyeShiftMaxPm keeps its authored 100, and the gate prints the
  // pivot so the state is visible in every probe run.
  if (kEyeShiftPivotMm <= 0) return 0;
  if (pm > 1000) pm = 1000;
  if (pm < -1000) pm = -1000;
  const int32_t shift_mm = 2 * kEyeWideMm * kEyeShiftMaxPm / 1000 * pm / 1000;
  // The `> 0 ? : 1` is unreachable past the early return above; it is here so
  // the compiler does not constant-fold a literal division by zero while the
  // pivot is unshipped. It must never again be the thing that SUPPLIES a
  // value -- that is what produced the 916.7 degree fallback.
  const int32_t pivot = kEyeShiftPivotMm > 0 ? kEyeShiftPivotMm : 1;
  return shift_mm * 10430 / pivot;
}

/** The eye assembly slides on the body. Composed onto face_rest's attitude, so
 *  it must be called after it. Both eyes take the same shift: they are one
 *  face. */
inline void apply_eye_shift(Rig& g, int32_t side_pm, int32_t lift_pm) {
  const int32_t sa = eye_shift_a16(side_pm), la = eye_shift_a16(lift_pm);
  g.q[kBEyeL] = quat_mul(g.q[kBEyeL], quat_mul(quat_y(-sa), quat_z(la)));
  g.q[kBEyeR] = quat_mul(g.q[kBEyeR], quat_mul(quat_y(-sa), quat_z(la)));
}

/** PASS 6 (Direction 5 5d): each eye ROLLS about its own outward axis and
 *  returns. pm is a fraction of kEyeRollMaxA16, per eye, so the two can roll
 *  together (a brow) or against each other (a quizzical tilt). The star unit
 *  rides it -- the pupil bone is a child of this one -- so 5b still holds at
 *  exactly two transforms per eye.
 *
 *  INWARD is the expressive direction AND the collision direction: it is the
 *  sign that carries a rim-pressed star toward the other eye. The composed
 *  extremes are gated in manafold_probe.cpp, not here. */
inline void apply_eye_roll(Rig& g, int32_t left_pm, int32_t right_pm) {
  const auto c = [](int32_t pm) {
    if (pm > 1000) pm = 1000;
    if (pm < -1000) pm = -1000;
    return static_cast<int32_t>((static_cast<int64_t>(kEyeRollMaxA16) * pm) / 1000);
  };
  g.q[kBEyeL] = quat_mul(g.q[kBEyeL], quat_x(c(left_pm)));
  g.q[kBEyeR] = quat_mul(g.q[kBEyeR], quat_x(-c(right_pm)));
}

/** One apparent gaze on both pupil pivots: +side sweeps the stars toward the
 *  creature's left (+z), +lift sweeps them up. The pivot radius is the bulge. */
inline void apply_gaze(Rig& g, int32_t side_a16, int32_t lift_a16) {
  const int32_t side = side_a16 < -kGazeMaxA16   ? -kGazeMaxA16
                       : side_a16 > kGazeMaxA16  ? kGazeMaxA16
                                                 : side_a16;
  const int32_t lift = lift_a16 < -kGazeLiftMaxA16  ? -kGazeLiftMaxA16
                       : lift_a16 > kGazeLiftMaxA16 ? kGazeLiftMaxA16
                                                    : lift_a16;
  g.q[kBPupilL] = quat_mul(quat_y(-side), quat_z(lift));
  g.q[kBPupilR] = quat_mul(quat_y(-side), quat_z(lift));
  // §5c: the eyeball LEADS the star, a little. This is what stops a hard
  // look-direction reading as a sticker sliding on a fixed field, and it costs
  // no clip authoring: every existing gaze schedule drives it already.
  // 5c's eyeball shift is NOT wired here this pass: the pivot mechanism it
  // needs is unsound against face_rest's rest attitude (see manafold_rig.h),
  // and the committed extremes gate is what found that. Reported, not silent.
}

/** PASS 6 B.1 (Direction 5 §5b rule 4): PER-EYE gaze. "Asymmetry is allowed
 *  and wanted -- two independently aimed stars on one apparent point is what
 *  sells a googly eye." Both eyes still converge on one target; this is the
 *  lag, the overshoot and the deliberate cross-eyed taunt. The symmetric
 *  apply_gaze() above stays as the common case, so no existing clip retimes by
 *  this being added. Clamped per eye against the same containment limits. */
inline void apply_gaze_lr(Rig& g, int32_t l_side_a16, int32_t l_lift_a16,
                          int32_t r_side_a16, int32_t r_lift_a16) {
  const auto cs = [](int32_t v) {
    return v < -kGazeMaxA16 ? -kGazeMaxA16 : v > kGazeMaxA16 ? kGazeMaxA16 : v;
  };
  const auto cl = [](int32_t v) {
    return v < -kGazeLiftMaxA16 ? -kGazeLiftMaxA16
           : v > kGazeLiftMaxA16 ? kGazeLiftMaxA16 : v;
  };
  g.q[kBPupilL] = quat_mul(quat_y(-cs(l_side_a16)), quat_z(cl(l_lift_a16)));
  g.q[kBPupilR] = quat_mul(quat_y(-cs(r_side_a16)), quat_z(cl(r_lift_a16)));
}

/** Star twinkle: spin the four-point star about its outward axis. */
inline void apply_twinkle(Rig& g, int32_t spin_a16) {
  g.q[kBPupilL] = quat_mul(g.q[kBPupilL], quat_x(spin_a16));
  g.q[kBPupilR] = quat_mul(g.q[kBPupilR], quat_x(spin_a16));
}

/** Squint 0..1000: the faceted lenses rotate toward edge-on (a shutter).
 *  Negative widens — the lenses roll a little MORE face-on (startle/fall). */
inline void apply_squint(Rig& g, int32_t amount_pm) {
  const int32_t a = static_cast<int32_t>(
      (static_cast<int64_t>(kSquintMaxA16) * amount_pm) / 1000);
  g.q[kBEyeL] = quat_mul(g.q[kBEyeL], quat_y(a));
  g.q[kBEyeR] = quat_mul(g.q[kBEyeR], quat_y(-a));
}

/** Per-eye squint (the wink — taunts). */
inline void apply_squint_lr(Rig& g, int32_t left_pm, int32_t right_pm) {
  g.q[kBEyeL] = quat_mul(g.q[kBEyeL],
      quat_y(static_cast<int32_t>((static_cast<int64_t>(kSquintMaxA16) * left_pm) / 1000)));
  g.q[kBEyeR] = quat_mul(g.q[kBEyeR],
      quat_y(-static_cast<int32_t>((static_cast<int64_t>(kSquintMaxA16) * right_pm) / 1000)));
}

/** The blink floor (§4: the eyes move in EVERY clip): a triangular lid pulse
 *  every kBlinkPeriodKeys, staggered by `offset` so clips never sync. Returns
 *  the squint contribution in pm; add it to the clip's own squint value. */
inline int32_t blink_at(int f, int offset) {
  const int t = (f + offset) % kBlinkPeriodKeys;
  if (t >= kBlinkLenKeys) return 0;
  const int half = kBlinkLenKeys / 2;
  const int tri = t <= half ? t : kBlinkLenKeys - t;  // 0..half..0
  return static_cast<int32_t>(
      (static_cast<int64_t>(kBlinkDepthPm) * tri) / (half > 0 ? half : 1));
}

/** Start a clip: slot, key count, identity quats, root at the hover height,
 *  deform sidecar allocated (identity samples). */
inline zc::Clip clip_shell(uint16_t slot, int keys, int32_t hover_mm) {
  zc::Clip c;
  c.slot_id = slot;
  c.frame_count = static_cast<uint16_t>(keys);
  c.root.assign(static_cast<size_t>(keys) * 3, 0);
  c.quats.assign(static_cast<size_t>(keys) * kBoneCount, zc::quat16_identity());
  c.deform.assign(static_cast<size_t>(keys), zc::DeformSample{});
  for (int f = 0; f < keys; ++f) c.root[static_cast<size_t>(f) * 3 + 1] = fxu(hover_mm);
  c.interpolate = true;
  return c;
}

/** The compression wave sample at key f: flatten = amp * (0.5 + 0.5 sin),
 *  spread the positive-volume partner. amp/period per clip. */
inline zc::DeformSample compress_at(int f, int keys, int cycles, int32_t amp,
                                    int32_t phase16 = 0) {
  const int32_t w = (65536 + sinp(f, keys, cycles, phase16)) / 2;  // 0..65536
  const int32_t flat = static_cast<int32_t>((static_cast<int64_t>(amp) * w) >> 16);
  const int32_t spread = static_cast<int32_t>(
      (static_cast<int64_t>(flat) * kSpreadRatioPm) / 1000);
  return zc::DeformSample{static_cast<uint16_t>(flat), static_cast<uint16_t>(spread)};
}

/** Curve-driven squash for IMPACT clips: the deform follows the clip's own
 *  keys directly (a free-running compression wave decoupled the squash from
 *  the impact frame), with a small breathing wave on top so the deform
 *  channel never flatlines. */
inline zc::DeformSample squash_impact(int f, int K, const Key* sq, int nsq) {
  const int32_t direct = static_cast<int32_t>(
      (static_cast<int64_t>(kCompressAmpPm) * curve(sq, nsq, f)) / 1000);
  const int32_t wave = static_cast<int32_t>(
      (static_cast<int64_t>(kCompressAmpPm / 3) *
       ((65536 + sinp(f, K, K / 25 > 0 ? K / 25 : 1)) / 2)) >> 16);
  int32_t flat = direct + wave;
  if (flat > 60000) flat = 60000;
  const int32_t spread = static_cast<int32_t>(
      (static_cast<int64_t>(flat) * kSpreadRatioPm) / 1000);
  return zc::DeformSample{static_cast<uint16_t>(flat), static_cast<uint16_t>(spread)};
}

/** The hover: base height + two incommensurate bobs (integer cycles). */
inline int32_t hover_at(int f, int keys, int32_t base_mm, int32_t amp_a_mm,
                        int32_t amp_b_mm, int cyc_a, int cyc_b) {
  const int32_t a = static_cast<int32_t>(
      (static_cast<int64_t>(fxu(amp_a_mm)) * sinp(f, keys, cyc_a)) >> 16);
  const int32_t b = static_cast<int32_t>(
      (static_cast<int64_t>(fxu(amp_b_mm)) * sinp(f, keys, cyc_b, 0x3000)) >> 16);
  return fxu(base_mm) + a + b;
}

/** The antenna's living sway: per-hinge fold-scale modulation with cumulative
 *  phase lag (the front leads, the rear follows) + slow out-of-plane tilt +
 *  the sympathetic compression coupling (one amplitude knob: kCompressAmpPm). */
inline void loop_alive(Rig& g, int f, int keys, int cyc, int32_t amp_pm,
                       int32_t compress_amp, int comp_cyc) {
  if (cyc < 1) cyc = 1;
  if (comp_cyc < 1) comp_cyc = 1;
  const int32_t lag = static_cast<int32_t>((65536LL * kAntennaLagKeys * cyc) / keys);
  const int32_t sa = static_cast<int32_t>(
      (static_cast<int64_t>(amp_pm) * sinp(f, keys, cyc)) >> 16);
  const int32_t sb = static_cast<int32_t>(
      (static_cast<int64_t>(amp_pm) * sinp(f, keys, cyc, -lag)) >> 16);
  const int32_t sc = static_cast<int32_t>(
      (static_cast<int64_t>(amp_pm) * sinp(f, keys, cyc, -2 * lag)) >> 16);
  const int32_t couple = static_cast<int32_t>(
      (static_cast<int64_t>(kCompressLoopCouplePm) * compress_amp / kCompressAmpPm *
       sinp(f, keys, comp_cyc)) >>
      16);
  const int32_t tilt = static_cast<int32_t>(
      (static_cast<int64_t>(kAntennaTiltA16) * sinp(f, keys, cyc / 2 > 0 ? cyc / 2 : 1, 0x5000)) >>
      16);
  loop_pose(g, 1000 + couple, 1000 + sa, 1000 + sb, 1000 + sc, tilt);
}

/** THE WHOLE-CREATURE WOBBLE (pass 3, Direction 3 §4), mechanically: a slow
 *  bend STARTS at the loop peak (hinge B, lag station 0), travels down
 *  through C and A (station 1), reaches the neck (station 2), and ARRIVES
 *  IN THE BODY (station 3) as a lean a few keys later — front leads, the
 *  bottom follows. Two incommensurate periods (kWobblePerA/BKeys, the
 *  46/102-frame class) so it never metronomes; root PITCH (up/down
 *  angling — Direction 3 §4) rides the slow wave. The caller's compression
 *  phase supplies the squash half of "lean-plus-squash" (same lag knob).
 *  REPLACES loop_alive where the whole creature should carry the wave. */
inline void whole_wobble(Rig& g, int f, int K, int amp_pm) {
  const int cycA = K / kWobblePerAKeys > 0 ? K / kWobblePerAKeys : 1;
  const int cycB = K / kWobblePerBKeys > 0 ? K / kWobblePerBKeys : 1;
  const int32_t lag = static_cast<int32_t>((65536LL * kWobbleLagKeys * cycA) / K);
  const auto wave = [&](int station) {
    const int32_t a = sinp(f, K, cycA, -lag * station);
    const int32_t b = sinp(f, K, cycB, 0x2800 - lag * station);
    return static_cast<int32_t>((static_cast<int64_t>(amp_pm) * (a + b / 2)) >> 16);
  };
  const int32_t tilt = static_cast<int32_t>(
      (static_cast<int64_t>(kAntennaTiltA16) * sinp(f, K, cycB, 0x5000)) >> 16);
  loop_pose(g, 1000 + wave(2), 1000 + wave(1), 1000 + wave(0), 1000 + wave(1), tilt);
  // the body arrives LAST: lean (the wave, one more lag station) + pitch
  const int32_t leanw = wave(3);
  g.q[kBRoot] = quat_mul(
      g.q[kBRoot],
      quat_x(static_cast<int32_t>((static_cast<int64_t>(kWobbleLeanA16) * leanw) /
                                  (amp_pm > 0 ? amp_pm : 1))));
  g.q[kBRoot] = quat_mul(
      g.q[kBRoot],
      quat_z(static_cast<int32_t>((static_cast<int64_t>(kWobblePitchA16) *
                                   sinp(f, K, cycB, 0x6000)) >> 16)));
}

// ================== THE FOLD-HOLD-KNEAD TIMELINE (pass 4) ==================
//
// One deterministic, hashed, per-clip schedule shared VERBATIM by the
// antenna choreography (key domain, here) and the mote system (frame
// domain, manafold_fx.h; key = frame / 2): GATHER (the joints close, the
// cloud condenses onto stencil k) -> HOLD (the shape stands and READS) ->
// KNEAD (the two hands work; each mote morphs toward stencil k+1) ->
// repeat with the next shape. Durations are 07-band lengths hashed per
// segment (anti-cycle law: never visibly repeating inside a clip), the
// shape order is hashed with next != current, and every clip's tail is a
// RELEASE easing the layer to zero so the loop seam carries no pop.

enum FoldSeg : uint8_t { kSegGather = 0, kSegHold, kSegKnead, kSegRelease };
struct FoldPhase {
  FoldSeg seg;
  int32_t amp_pm;    // grip envelope 0..1000 (gather ramps, hold/knead hold)
  int32_t agit_pm;   // knead-waggle envelope 0..1000 (knead only)
  int32_t morph_pm;  // 0..1000 progress from shape_from to shape_to
  uint8_t shape_from, shape_to;
};

constexpr int kFoldShapeCount = 6;  // ring, star, bar, crescent, triangle, s-curl

/** ease 0..1000 -> 0..1000, smoothstep-ish (integer). */
inline int32_t fold_ease(int32_t t) {
  if (t < 0) t = 0;
  if (t > 1000) t = 1000;
  return t * t / 1000 * (3000 - 2 * t) / 1000;
}

/** The shared schedule. `salt` = the clip slot; `keys` = the clip length;
 *  `kq4` = key position in Q4 (key * 16 + sub-key sixteenths -- the fx lane
 *  passes frame * 8 so 60 Hz frames land between keys). */
inline FoldPhase fold_phase(uint32_t salt, int keys, int32_t kq4) {
  FoldPhase ph{};
  const int32_t release_at = (keys - kReleaseKeys) * 16;
  // the opening shape varies PER CLIP (most clips are shorter than one
  // full cycle, so a fixed opener would make the whole bank read RING);
  // the hover keeps the RING -- the easiest read on the showcase loop
  uint8_t shape = salt == 0 ? 0 : static_cast<uint8_t>(fx_hash(salt, 0xBEEFu, 7u) % kFoldShapeCount);
  uint8_t next_shape = 1;
  int32_t seg_start = 0;
  uint32_t n = 0;
  for (;;) {
    const uint32_t h = fx_hash(0xF01D5EEDu + salt, n, 0x51u);
    int32_t gather = (kGatherKeysBase + static_cast<int32_t>(h % kGatherKeysHash)) * 16;
    int32_t hold = (kHoldKeysBase + static_cast<int32_t>((h >> 8) % kHoldKeysHash)) * 16;
    int32_t knead = (kKneadKeysBase + static_cast<int32_t>((h >> 16) % kKneadKeysHash)) * 16;
    // PASS 5 (QA item 3): SHORT CLIPS MUST STILL KNEAD. `hit` (70 keys) and
    // `startle` (80) never reached the knead segment, and `curious` kneaded
    // 9 keys of 90 -- the hashed first cycle simply did not fit before the
    // release tail, so the owner's "then knead it into new shapes ... going
    // on all the time" was untrue of part of the bank. If the whole first
    // cycle cannot fit, compress its three segments proportionally so
    // gather -> hold -> KNEAD -> release all land inside the clip. A clip
    // whose first cycle already fits takes the same durations as before.
    if (n == 0 && release_at > 3 * 16 && gather + hold + knead > release_at) {
      const int32_t cyc = gather + hold + knead;
      gather = gather * release_at / cyc;
      hold = hold * release_at / cyc;
      if (gather < 16) gather = 16;
      if (hold < 16) hold = 16;
      knead = release_at - gather - hold;  // no rounding gap: the knead
      if (knead < 16) knead = 16;          // hands straight to the release
    }
    next_shape = static_cast<uint8_t>((h >> 24) % kFoldShapeCount);
    if (next_shape == shape)
      next_shape = static_cast<uint8_t>((next_shape + 1 + (h >> 28) % (kFoldShapeCount - 1)) %
                                        kFoldShapeCount);
    const int32_t g_end = seg_start + gather, h_end = g_end + hold, k_end = h_end + knead;
    ph.shape_from = shape;
    ph.shape_to = next_shape;
    if (kq4 >= release_at) {  // the tail: ease everything home
      ph.seg = kSegRelease;
      const int32_t t = (kq4 - release_at) * 1000 / (kReleaseKeys * 16);
      ph.amp_pm = 1000 - fold_ease(t);
      ph.agit_pm = 0;
      ph.morph_pm = 0;
      return ph;
    }
    if (kq4 < g_end) {
      ph.seg = kSegGather;
      const int32_t t = (kq4 - seg_start) * 1000 / gather;
      if (n == 0) {
        // the clip's first gather rises from zero (matches the release the
        // loop seam left behind -- no pop at the wrap)
        ph.amp_pm = fold_ease(t);
      } else {
        // between cycles the hands RELAX briefly (the dough is let go),
        // then re-gather -- continuous with the knead's amp=1000 on both
        // sides, so the envelope never steps
        ph.amp_pm = t < 350 ? 1000 - fold_ease(t * 1000 / 350) * 65 / 100
                            : 350 + fold_ease((t - 350) * 1000 / 650) * 65 / 100;
      }
      ph.agit_pm = 0;
      ph.morph_pm = 0;
      return ph;
    }
    if (kq4 < h_end) {
      ph.seg = kSegHold;
      ph.amp_pm = 1000;
      ph.agit_pm = 0;
      ph.morph_pm = 0;
      return ph;
    }
    if (kq4 < k_end) {
      ph.seg = kSegKnead;
      ph.amp_pm = 1000;
      const int32_t t = (kq4 - h_end) * 1000 / knead;
      // the waggle ramps in and out inside the knead (one thing at a time)
      ph.agit_pm = t < 250 ? fold_ease(t * 4)
                 : t > 750 ? fold_ease((1000 - t) * 4)
                           : 1000;
      ph.morph_pm = fold_ease(t);
      return ph;
    }
    seg_start = k_end;
    shape = next_shape;
    ++n;
    if (n > 64) {  // unreachable guard
      ph.seg = kSegHold;
      ph.amp_pm = 1000;
      return ph;
    }
  }
}

// THE COUPLING'S PROOF IS THE CODE, not a render (pass 5, both gates
// agreeing): every mote position is a fixed-weight sum over the POSED
// anchors (mana_fold / fold_mvc in manafold_fx.h) and no proximity,
// collision or distance term exists anywhere in the mana path -- grep for
// one; finding one is the regression. The old U02_ABLATE_KNEAD gate was
// RETIRED because it zeroed this layer, which moves the BONES it claimed
// to hold fixed, so its A/B could never separate "the mana follows the
// rig" from "the projection moved because the rig moved". The can-fail
// render gate is now U02_FOLD_FREEZE=1 (manafold_fx.h): the bones keep
// animating and only the field's anchor INPUT is frozen at rest -- if the
// mana still tracks the antenna in that render, the coupling is
// decorative and the feature has failed.

/** THE ALWAYS-ON KNEAD LAYER: composed onto the junction/neck/hinge bones
 *  BEFORE the clip's own loop_pose call, so the closure walk accounts for
 *  every knead rotation (the aim still lands the return arm; the committed
 *  closure probe gates the bank). The back-junction ball rides its offset
 *  bind, so kBLoopBase2's rotation SLIDES the ball along the body surface
 *  (Direction 4: a ball that is a joint). */
inline void antenna_knead(Rig& g, uint32_t slot, int keys, int f) {
  // every authored slot reads its own gain (pass 5: the guard was `< 14`,
  // which orphaned index 14 -- the damage clip silently ran at 700, 2.8x
  // its authored 250, and the owner's knob did nothing)
  const int gain = slot < static_cast<uint32_t>(kKneadClipSlots) ? kKneadClipPm[slot] : 700;
  if (gain <= 0) return;
  // PASS 6 C.2: THE SHARED DRIVER IS SPLIT. Every hinge used to read the same
  // `grip` scalar on the same frame, so they were perfectly correlated by
  // construction and the antenna could only open and close as one piece.
  // Each hinge now samples the SAME envelope at its OWN lag, so the grip
  // travels up the antenna as a wave. The lag wraps modulo the clip length,
  // so the clip still loops seamlessly.
  const auto lagged = [&](int lag) {
    const int fl = ((f - lag) % keys + keys) % keys;
    return fold_phase(slot, keys, fl * 16);
  };
  const FoldPhase ph = lagged(kKneadLagJfKeys);
  const FoldPhase ph_neck = lagged(kKneadLagNeckKeys);
  const FoldPhase ph_a = lagged(kKneadLagAKeys);
  const FoldPhase ph_b = lagged(kKneadLagBKeys);
  const FoldPhase ph_c = lagged(kKneadLagCKeys);
  const auto a = [&](int32_t base, int32_t env_pm) {
    return static_cast<int32_t>(static_cast<int64_t>(base) * env_pm / 1000 * gain / 1000);
  };
  // GATHER/HOLD: the grip -- every fold closes a few degrees
  const int32_t grip = ph.amp_pm;
  // HOLD: the small tremor that keeps the grip alive
  const int32_t trem = ph.seg == kSegHold
      ? static_cast<int32_t>((static_cast<int64_t>(kKneadTremorA16) *
                              sinp(f, keys, keys / 9 > 0 ? keys / 9 : 1)) >> 16)
      : 0;
  g.q[kBJunctionF] = quat_mul(g.q[kBJunctionF], quat_z(a(kKneadGripJfA16, grip) + trem));
  g.q[kBNeck] = quat_mul(g.q[kBNeck], quat_z(a(kKneadGripNeckA16, ph_neck.amp_pm) - trem));
  g.q[kBHingeA] = quat_mul(g.q[kBHingeA], quat_z(a(kKneadGripAA16, ph_a.amp_pm)));
  g.q[kBHingeB] =
      quat_mul(g.q[kBHingeB], quat_z(a(kKneadGripBA16, ph_b.amp_pm) + trem / 2));
  g.q[kBHingeC] = quat_mul(g.q[kBHingeC], quat_z(a(kKneadGripCA16, ph_c.amp_pm)));
  // PASS 6 C.1/C.3: THE OUT-OF-PLANE CHANNEL -- the axis that did not exist
  // until this pass. A, B and C swing ACROSS the loop plane on their own
  // period, so "up and down separately" is now something the rig can express.
  // The period is deliberately different from the in-plane wag's, so the two
  // never lock into one apparent motion.
  {
    const int ocyc = keys / kKneadOopPeriodKeys > 0 ? keys / kKneadOopPeriodKeys : 1;
    const auto oop = [&](int32_t base, const FoldPhase& p, uint16_t phase) {
      const int32_t w = sinp(f, keys, ocyc, phase);
      return static_cast<int32_t>(
          (static_cast<int64_t>(a(base, p.amp_pm)) * w) >> 16);
    };
    g.q[kBHingeA] = quat_mul(g.q[kBHingeA], quat_x(oop(kKneadOopAA16, ph_a, 0)));
    g.q[kBHingeB] = quat_mul(g.q[kBHingeB], quat_x(oop(kKneadOopBA16, ph_b, 0x3000)));
    g.q[kBHingeC] = quat_mul(g.q[kBHingeC], quat_x(oop(kKneadOopCA16, ph_c, 0x6800)));
  }
  // KNEAD: the two hands wedge in counter-rotation; the neck stirs
  // out-of-plane; the back ball slides. One consistent period.
  if (ph.agit_pm > 0 || ph_b.agit_pm > 0 || ph_c.agit_pm > 0) {
    const int cyc = keys / kKneadWagPeriodKeys > 0 ? keys / kKneadWagPeriodKeys : 1;
    const int32_t w1 = sinp(f, keys, cyc);
    const int32_t w2 = sinp(f, keys, cyc, 0x4000);
    g.q[kBJunctionF] = quat_mul(
        g.q[kBJunctionF],
        quat_z(static_cast<int32_t>((static_cast<int64_t>(a(kKneadWagJfA16, ph.agit_pm)) * w1) >> 16)));
    g.q[kBHingeC] = quat_mul(
        g.q[kBHingeC],
        quat_z(-static_cast<int32_t>((static_cast<int64_t>(a(kKneadWagCA16, ph_c.agit_pm)) * w1) >> 16)));
    g.q[kBNeck] = quat_mul(
        g.q[kBNeck],
        quat_x(static_cast<int32_t>((static_cast<int64_t>(a(kKneadWagNeckA16, ph_neck.agit_pm)) * w2) >> 16)));
    g.q[kBHingeB] = quat_mul(
        g.q[kBHingeB],
        quat_z(static_cast<int32_t>((static_cast<int64_t>(a(kKneadWagBA16, ph_b.agit_pm)) * w2) >> 16)));
    g.q[kBLoopBase2] = quat_mul(
        g.q[kBLoopBase2],
        quat_z(-static_cast<int32_t>((static_cast<int64_t>(a(kKneadWagB2A16, ph.agit_pm)) * w2) >> 16)));
  }
}

// ---------------------------------------------------------------- clips ----

/** hover-idle, slot 0: the baseline. The hover IS the idle. */
inline zc::Clip build_hover_idle() {
  const int K = kIdleKeys;
  zc::Clip c = clip_shell(0, K, kHoverHeightMm);
  Rig g;
  // the idle glance schedule (thousandths of the gaze clamps)
  static const Key kSide[] = {{0, 0},    {70, 0},   {90, 900},  {150, 900},
                              {170, 0},  {195, 0},  {215, -550}, {245, -550},
                              {265, 0},  {299, 0}};
  static const Key kLift[] = {{0, 0},   {70, 0},   {90, 150},  {150, 150},
                              {170, 0}, {215, -400}, {245, -400}, {265, 0},
                              {299, 0}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 0, K, f);  // pass 4: the always-on fold-hold-knead layer
    // pass 3: the whole creature carries the travelling bend (peak leads,
    // body follows); the squash below lags by the same station clock.
    whole_wobble(g, f, K, kWobbleAmpPm);
    face_rest(g);
    apply_gaze(g,
               static_cast<int32_t>((static_cast<int64_t>(kGazeMaxA16) *
                                     curve(kSide, 10, f)) / 1000),
               static_cast<int32_t>((static_cast<int64_t>(kGazeLiftMaxA16) *
                                     curve(kLift, 9, f)) / 1000));
    apply_squint(g, blink_at(f, 17));
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 1] = hover_at(
        f, K, kHoverHeightMm, kBobAmpAMm, kBobAmpBMm, K / kBobPeriodAKeys, K / kBobPeriodBKeys);
    c.deform[static_cast<size_t>(f)] = compress_at(
        f, K, K / kWobblePerAKeys,  kCompressAmpPm,
        -static_cast<int32_t>((65536LL * 3 * kWobbleLagKeys * (K / kWobblePerAKeys)) / K));
  }
  return c;
}

/** drift, slot 1 (PASS 3 REBUILD — Direction 3 §7: "just rotating. That is
 *  not how it works."). Mechanically: a WIND-BLOWN LATERAL GLIDE — the
 *  body BANKS into a sideways slide toward +z, translates across the shot
 *  in a lazy S (fore-aft swing while the lateral travel is constant), the
 *  antenna trails against the travel, and TWICE it over-banks and
 *  recovers (keys ~45 and ~105) — a thing blown on the wind correcting
 *  itself, not a turntable. */
inline zc::Clip build_drift() {
  const int K = kDriftKeys;
  zc::Clip c = clip_shell(1, K, kHoverHeightMm);
  Rig g;
  // the bank: the working lean into the slide, with two over-bank bumps
  // that visibly correct (the "caught by a gust" beats)
  static const Key kBank[] = {{0, 1000},  {38, 1000}, {48, 1520}, {60, 860},
                              {72, 1060}, {98, 1000}, {108, 1460}, {120, 880},
                              {132, 1040}, {149, 1000}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 1, K, f);  // pass 4: the always-on fold-hold-knead layer
    // banked INTO the travel (+z): a roll about the forward axis
    const int32_t bank = static_cast<int32_t>(
        (static_cast<int64_t>(kDriftBankA16) * curve(kBank, 10, f)) / 1000);
    g.q[kBRoot] = quat_mul(g.q[kBRoot], quat_x(bank));
    whole_wobble(g, f, K, kWobbleAmpPm * 3 / 4);
    // the antenna TRAILS against the travel: a standing off-plane lean
    // (pass 4: the pivot is the front junction — the old neck bind)
    g.q[kBJunctionF] = quat_mul(g.q[kBJunctionF], quat_x(-kDriftTrailA16));
    face_rest(g);
    // eyes INTO the travel, one glance back at the second correction
    apply_gaze(g, f >= 104 && f < 122 ? -kGazeMaxA16 / 2 : kGazeMaxA16 / 2,
               kGazeLiftMaxA16 / 5);
    apply_squint(g, blink_at(f, 41));
    g.write(c, f);
    // lateral travel (+z), centred on the shot; the lazy S is the x swing
    c.root[static_cast<size_t>(f) * 3 + 2] =
        fxu(static_cast<int32_t>((f - K / 2) * kDriftSpeedMmPerKey));
    c.root[static_cast<size_t>(f) * 3 + 0] = static_cast<int32_t>(
        (static_cast<int64_t>(fxu(kDriftSCurveMm)) * sinp(f, K, 2, 0x2000)) >> 16);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(f, K, kHoverHeightMm, kBobAmpAMm * 2 / 3, kBobAmpBMm * 2 / 3, K / 10, K / 25);
    c.deform[static_cast<size_t>(f)] =
        compress_at(f, K, K / kDriftCompressPeriodKeys, kCompressAmpPm);
  }
  return c;
}

/** channel, slot 2: the conduit at work. Three beats: draw-in (the inhale),
 *  blaze (bolts + twinkle), release (the exhale). One thing at a time. */
inline zc::Clip build_channel() {
  const int K = kChannelKeys;
  zc::Clip c = clip_shell(2, K, kHoverHeightMm);
  Rig g;
  static const Key kDepth[] = {{0, 1000},  {24, 1400}, {56, 2200}, {130, 2200},
                               {150, 2600}, {170, 1300}, {195, 1000}, {209, 1000}};
  static const Key kLoopOpen[] = {{0, 1000}, {30, 1060}, {56, 1085}, {140, 1085},
                                  {158, 940}, {180, 1015}, {200, 1000}, {209, 1000}};
  static const Key kLift2[] = {{0, 0}, {20, -600}, {56, -600}, {140, -350},
                               {170, 300}, {200, 0}, {209, 0}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 2, K, f);  // pass 4: the always-on fold-hold-knead layer
    const int open = curve(kLoopOpen, 8, f);
    loop_pose(g, 1000, open, open, open,
              static_cast<int32_t>((static_cast<int64_t>(kAntennaTiltA16) *
                                    sinp(f, K, 3, 0x5000)) >> 16));
    face_rest(g);
    apply_gaze(g, 0,
               static_cast<int32_t>((static_cast<int64_t>(kGazeLiftMaxA16) *
                                     curve(kLift2, 7, f)) / 1000));
    if (f >= 56 && f < 140) {  // the blaze: the stars spin slowly — dilation-as-motion
      const int32_t spin = static_cast<int32_t>(
          (static_cast<int64_t>(f - 56) * kBlazeTwinkleA16 * 2) / 84);
      apply_twinkle(g, spin);
    }
    apply_squint(g, blink_at(f, 63));
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(f, K, kHoverHeightMm, kBobAmpAMm / 2, kBobAmpBMm / 2, K / 42, K / 70);
    const int32_t amp = static_cast<int32_t>(
        (static_cast<int64_t>(kCompressAmpPm) * curve(kDepth, 8, f)) / 1000);
    c.deform[static_cast<size_t>(f)] = compress_at(f, K, K / kChannelCompressPeriodKeys, amp);
  }
  return c;
}

/** react-curious, slot 3: the gaze snaps FIRST, the body yaws after with lag,
 *  the antenna perks. Settle back. */
inline zc::Clip build_curious() {
  const int K = kCuriousKeys;
  zc::Clip c = clip_shell(3, K, kHoverHeightMm);
  Rig g;
  // pass 3: the DOUBLE-TAKE — look, glance away, snap BACK, then home
  static const Key kSide[] = {{0, 0},    {4, 0},    {8, 1000},  {38, 1000},
                              {44, 120}, {50, 120}, {54, 1000}, {66, 1000},
                              {78, 0},   {89, 0}};
  static const Key kYaw[] = {{0, 0}, {8, 0}, {16, 200}, {36, 1000}, {60, 1000},
                             {78, 0}, {89, 0}};
  static const Key kPerk[] = {{0, 1000}, {12, 1000}, {26, 880}, {58, 880},
                              {76, 1000}, {89, 1000}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 3, K, f);  // pass 4: the always-on fold-hold-knead layer
    const int perk = curve(kPerk, 6, f);
    loop_pose(g, 1000, perk, perk, perk, 0);
    // PASS 2: the yaw is NEGATED — the gaze sweeps the stars toward +z
    // (the creature's left) and quat_y(+) turns the face toward -z, so the
    // shipped clip named for looking at something looked AWAY from it.
    g.q[kBRoot] = quat_mul(
        g.q[kBRoot],
        quat_y(-static_cast<int32_t>((static_cast<int64_t>(kCuriousYawA16) *
                                      curve(kYaw, 7, f)) / 1000)));
    face_rest(g);
    apply_gaze(g,
               static_cast<int32_t>((static_cast<int64_t>(kGazeMaxA16) *
                                     curve(kSide, 10, f)) / 1000),
               kGazeLiftMaxA16 / 4);
    // pass 3 (Direction 3 §4): the body angles UP toward the thing too
    g.q[kBRoot] = quat_mul(
        g.q[kBRoot], quat_z(static_cast<int32_t>(
                         (static_cast<int64_t>(kWobblePitchA16) *
                          curve(kYaw, 7, f)) / 1000)));
    apply_squint(g, blink_at(f, 29));
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(f, K, kHoverHeightMm, kBobAmpAMm * 2 / 3, kBobAmpBMm / 2, K / 30, K / 45);
    c.deform[static_cast<size_t>(f)] = compress_at(f, K, K / 30, kCompressAmpPm);
  }
  return c;
}

/** react-startle, slot 4. PASS 2 REWORK (the shipped clip was a 12-frame hop
 *  and a 130-frame droop): anticipation dip -> the SNAP payoff (back + up,
 *  eyes flying WIDE) -> overshoot -> two damped settle bounces -> rest.
 *  Mechanically: the body drops and compresses for 8 keys, launches back and
 *  up over 6, overshoots its arc by key 22, bounces at 30 and 44 with
 *  falling amplitude, and is home by 64. The antenna whips one beat late. */
inline zc::Clip build_startle() {
  const int K = kStartleKeys;
  zc::Clip c = clip_shell(4, K, kHoverHeightMm);
  Rig g;
  // pass 3 ("ain't bad, make it better"): the snap lands two keys sooner
  // and overshoots harder before the recoil catches it
  static const Key kBack[] = {{0, 0},    {8, 140},   {12, -1300}, {20, -980},
                              {28, -1080}, {42, -1000}, {52, -960}, {74, -60},
                              {79, 0}};
  static const Key kUp[] = {{0, 0},    {8, -170},  {12, 1300}, {20, 750},
                            {28, 990}, {42, 720},  {52, 640},  {74, 40},
                            {79, 0}};
  static const Key kWhip[] = {{0, 1000},  {8, 1060},  {14, 760},  {22, 1160},
                              {32, 880},  {44, 1080}, {56, 950},  {68, 1020},
                              {79, 1000}};
  static const Key kWide[] = {{0, 0},   {8, 80},   {11, -430}, {36, -430},
                              {52, 0},  {79, 0}};
  static const Key kSquash[] = {{0, 1000}, {8, 1900},  {14, 600},  {22, 2600},
                                {36, 1500}, {52, 1900}, {68, 1100}, {79, 1000}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 4, K, f);  // pass 4: the always-on fold-hold-knead layer
    const int whip = curve(kWhip, 9, f);
    loop_pose(g, 1000, whip, whip, whip, 0);
    face_rest(g);
    apply_squint(g, curve(kWide, 6, f) + blink_at(f, 70));
    apply_gaze(g, 0, f >= 12 && f < 40 ? kGazeLiftMaxA16 / 2 : 0);
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 0] = static_cast<int32_t>(
        (static_cast<int64_t>(fxu(kStartleJumpMm)) * curve(kBack, 9, f)) / 1000);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        fxu(kHoverHeightMm) +
        static_cast<int32_t>((static_cast<int64_t>(fxu(kStartleLiftMm)) * curve(kUp, 9, f)) /
                             1000) +
        static_cast<int32_t>((static_cast<int64_t>(fxu(kBobAmpBMm)) * sinp(f, K, 2)) >> 16);
    c.deform[static_cast<size_t>(f)] = squash_impact(f, K, kSquash, 8);
  }
  return c;
}

/** rest, slot 5: the lower hover; almost-sleep. The life clock NEVER stops. */
inline zc::Clip build_rest() {
  const int K = kRestKeys;
  zc::Clip c = clip_shell(5, K, kRestHeightMm);
  Rig g;
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 5, K, f);  // pass 4: the always-on fold-hold-knead layer
    whole_wobble(g, f, K, kWobbleAmpPm / 2);  // pass 3: slower, whole-body
    face_rest(g);
    apply_squint(g, kRestSquintPm + blink_at(f, 77));
    apply_gaze(g,
               static_cast<int32_t>((static_cast<int64_t>(kGazeMaxA16 / 4) *
                                     sinp(f, K, 2)) >> 16),
               -kGazeLiftMaxA16 * 2 / 3);
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(f, K, kRestHeightMm, kBobAmpAMm * 2 / 5, kBobAmpBMm / 2,
                 K / kRestBobPeriodKeys, K / (2 * kRestBobPeriodKeys));
    c.deform[static_cast<size_t>(f)] =
        compress_at(f, K, K / kRestCompressPeriodKeys, kCompressAmpPm * 4 / 5);
  }
  return c;
}

/** pirouette, slot 6 (the stretch): one slow full yaw, antenna flaring under
 *  the turn, the gaze holding then whipping round. */
inline zc::Clip build_pirouette() {
  const int K = kPirouetteKeys;
  zc::Clip c = clip_shell(6, K, kHoverHeightMm);
  Rig g;
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 6, K, f);  // pass 4: the always-on fold-hold-knead layer
    const uint16_t ph = static_cast<uint16_t>((static_cast<int64_t>(f) * 65536) / K);
    g.q[kBRoot] = quat_mul(g.q[kBRoot], quat_y(static_cast<int32_t>(ph)));
    const int32_t flare = 1000 + static_cast<int32_t>(
        (static_cast<int64_t>(kPirouetteFlarePm) *
         ((65536 - zref::fx_cos(zref::angle16{ph}).raw) / 2)) >> 16);
    loop_pose(g, 1000, flare, flare, flare,
              static_cast<int32_t>((static_cast<int64_t>(2 * kAntennaTiltA16) *
                                    sinp(f, K, 2)) >> 16));
    face_rest(g);
    const int32_t counter = static_cast<int32_t>(ph) < 32768
                                ? -static_cast<int32_t>(ph) / 8
                                : (65536 - static_cast<int32_t>(ph)) / 8;
    apply_gaze(g, counter, 0);
    apply_squint(g, blink_at(f, 53));
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(f, K, kHoverHeightMm, kBobAmpAMm * 3 / 4, kBobAmpBMm, K / 24, K / 40);
    c.deform[static_cast<size_t>(f)] = compress_at(f, K, K / 24, kCompressAmpPm);
  }
  return c;
}

/** hasty, slot 8 (PASS 3 — Direction 3 §7: "do not run in a circle. Run
 *  in ONE DIRECTION"). Mechanically: straight-line travel along +x
 *  crossing the fixed shot through its centre (the Zixxtrixx walk staging
 *  precedent — it starts half the travel back), body pitched hard into
 *  the travel and banked, the fishtail yaw wobble kept (never quite
 *  corrected — the clumsy read), bob frequency doubled, the antenna
 *  dragging behind, one mid-flight panic glance sideways. */
inline zc::Clip build_hasty() {
  const int K = kHastyKeys;
  zc::Clip c = clip_shell(8, K, kHoverHeightMm);
  Rig g;
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 8, K, f);  // pass 4: the always-on fold-hold-knead layer
    // pitched into the travel, banked, fishtailing — travel is +x, the
    // rest facing, so no yaw circuit at all
    g.q[kBRoot] = quat_mul(g.q[kBRoot], quat_z(-kHastyPitchA16));
    g.q[kBRoot] = quat_mul(g.q[kBRoot], quat_x(kHastyBankA16));
    g.q[kBRoot] = quat_mul(
        g.q[kBRoot], quat_y(static_cast<int32_t>(
                         (static_cast<int64_t>(kHastyFishtailA16) *
                          sinp(f, K, kHastyFishtailCycles)) >> 16)));
    // the antenna drags: stronger sway, and the whole loop blown back a bit
    loop_alive(g, f, K, K / 15, kAntennaSwayPm * 3, kCompressAmpPm, K / 15);
    face_rest(g);
    // eyes ahead-up; one panic glance sideways mid-flight
    apply_gaze(g, f >= 56 && f < 72 ? kGazeMaxA16 / 2 : 0, kGazeLiftMaxA16 / 3);
    apply_squint(g, 220 + blink_at(f, 11));  // squinting into the wind
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 0] =
        fxu(static_cast<int32_t>((f - K / 2) * kHastySpeedMmPerKey));
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(f, K, kHoverHeightMm, kHastyBobAmpMm, kBobAmpBMm,
                 kHastyBobCycles, K / 20);
    c.deform[static_cast<size_t>(f)] = compress_at(f, K, K / 15, kCompressAmpPm);
  }
  return c;
}

/** fall, slot 9 (§5: blown high into the air). Mechanically: it starts
 *  blown kFallHeightMm up, tumbles one full pitch turn while dropping with
 *  gravity's curve, antenna streaming open above it, eyes flying wide —
 *  then the CATCH at kFallCatchKey: the folds snap home with overshoot, a
 *  deep recovery squash, and it bobs back up to the hover, composed. */
inline zc::Clip build_fall() {
  const int K = kFallKeys;
  zc::Clip c = clip_shell(9, K, kHoverHeightMm);
  Rig g;
  static const Key kStream[] = {{0, 1000}, {14, 720}, {112, 700}, {130, 1120},
                                {146, 940}, {158, 1030}, {169, 1000}};
  static const Key kWide[] = {{0, -430}, {112, -430}, {140, 80}, {158, 0}, {169, 0}};
  static const Key kSquash[] = {{0, 800}, {124, 800}, {132, 2600}, {146, 1400},
                                {158, 1050}, {169, 1000}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 9, K, f);  // pass 4: the always-on fold-hold-knead layer
    // one full pitch tumble over the drop; 65536 wraps to 0 at the catch
    if (f < kFallCatchKey) {
      const int64_t t = (static_cast<int64_t>(f) << 16) / kFallCatchKey;
      const int64_t ease = (t * t) >> 16;  // accelerating spin, like the drop
      g.q[kBRoot] = quat_mul(g.q[kBRoot],
                             quat_z(static_cast<int32_t>((65536 * ease) >> 16)));
      // pass 3: the EXTRA tumble axis — a slow yaw under the pitch spin
      g.q[kBRoot] = quat_mul(
          g.q[kBRoot], quat_y(static_cast<int32_t>(
                           (static_cast<int64_t>(kFallYawTumbleA16) * ease) >> 16)));
    }
    const int stream = curve(kStream, 7, f);
    loop_pose(g, stream, stream, stream, stream, 0);
    face_rest(g);
    apply_squint(g, curve(kWide, 5, f) + blink_at(f, 5));
    apply_gaze(g, 0, kGazeLiftMaxA16 / 2);
    g.write(c, f);
    // the drop: height falls with t^2, lands at the hover by the catch,
    // dips through it, and floats back up
    int32_t y;
    if (f < kFallCatchKey) {
      const int64_t t = (static_cast<int64_t>(f) << 16) / kFallCatchKey;
      const int64_t drop = (t * t) >> 16;  // 0..1
      y = fxu(kHoverHeightMm + kFallHeightMm) -
          static_cast<int32_t>((static_cast<int64_t>(fxu(kFallHeightMm)) * drop) >> 16);
    } else {
      static const Key kCatch[] = {{130, 0}, {140, -180}, {152, 60}, {169, 0}};
      y = fxu(kHoverHeightMm) +
          static_cast<int32_t>((static_cast<int64_t>(fxu(100)) * curve(kCatch, 4, f)) / 1000);
    }
    c.root[static_cast<size_t>(f) * 3 + 1] = y;
    c.deform[static_cast<size_t>(f)] = squash_impact(f, K, kSquash, 6);
  }
  return c;
}

/** hit, slot 10 (§5: hit animations). Mechanically: the impact lands at key
 *  8 — the body is knocked back and DEEP-squashed in one beat, the hinges
 *  recoil a beat later, the eyes slam to a squint — then two damped
 *  recovery bounces carry it home by key 56. */
inline zc::Clip build_hit() {
  const int K = kHitKeys;
  zc::Clip c = clip_shell(10, K, kHoverHeightMm);
  Rig g;
  static const Key kKnock[] = {{0, 0},   {8, 0},    {13, -1000}, {22, -820},
                               {32, -900}, {44, -300}, {58, -40}, {69, 0}};
  static const Key kRecoil[] = {{0, 1000}, {10, 1000}, {16, 1180}, {26, 860},
                                {38, 1080}, {50, 960}, {62, 1010}, {69, 1000}};
  static const Key kSquint[] = {{0, 0}, {8, 0}, {11, 900}, {30, 900}, {46, 250},
                                {58, 0}, {69, 0}};
  static const Key kSquash[] = {{0, 1000}, {8, 1000}, {12, 2600}, {24, 2600},
                                {40, 1600}, {56, 1150}, {69, 1000}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 10, K, f);  // pass 4: the always-on fold-hold-knead layer
    const int rec = curve(kRecoil, 8, f);
    loop_pose(g, 1000, rec, rec, rec, 0);
    face_rest(g);
    apply_squint(g, curve(kSquint, 7, f) + blink_at(f, 33));
    apply_gaze(g, 0, 0);
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 0] = static_cast<int32_t>(
        (static_cast<int64_t>(fxu(kHitKnockMm)) * curve(kKnock, 8, f)) / 1000);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        fxu(kHoverHeightMm) +
        static_cast<int32_t>((static_cast<int64_t>(fxu(kBobAmpBMm)) * sinp(f, K, 2)) >> 16);
    c.deform[static_cast<size_t>(f)] = squash_impact(f, K, kSquash, 7);
  }
  return c;
}

/** taunt, slot 11 (§4: the hinge-play showcase — the balls are hinges and
 *  it PLAYS with them). Mechanically: a cocky double-bob while the loop
 *  waggles — hinges A and C pumping in anti-phase, hinge D swinging its own
 *  play on top of the closure aim — then it leans in and WINKS (left lid
 *  only, keys 78..92), stars twinkling through the waggle. */
inline zc::Clip build_taunt() {
  const int K = kTauntKeys;
  zc::Clip c = clip_shell(11, K, kHoverHeightMm);
  Rig g;
  // PASS 3 (Direction 3 §7: "can be more fun"): comedy is the HOLD.
  // Mechanically: 0..36 a BIG anticipation wind-up (it crouches, compresses
  // and pulls the whole loop back); 36..62 the waggle spins up; 62..86 the
  // waggle FREEZES AT ITS EXTREME for a 24-key readable beat — lean-in and
  // WINK inside it — with only a tiny tremble betraying the effort;
  // 86..139 the smug settle-bob (slow, pleased-with-itself).
  static const Key kWind[] = {{0, 0}, {8, 0}, {24, 1000}, {34, 1000}, {40, 0},
                              {139, 0}};
  static const Key kWagRamp[] = {{0, 0}, {36, 0}, {50, 700}, {62, 1000},
                                 {86, 1000}, {98, 260}, {112, 0}, {139, 0}};
  static const Key kLean[] = {{0, 0}, {40, 0}, {56, 200}, {64, 620}, {88, 620},
                              {102, 0}, {139, 0}};
  static const Key kWinkL[] = {{0, 0}, {64, 0}, {70, 820}, {82, 820}, {88, 0},
                               {139, 0}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 11, K, f);  // pass 4: the always-on fold-hold-knead layer
    // the waggle phase FREEZES during the hold (the frozen extreme is the
    // joke); a 3 pm tremble keeps the life clock honest
    const int fw = f < kTauntHoldStartKey ? f
                   : f < kTauntHoldEndKey ? kTauntHoldStartKey
                                          : f - (kTauntHoldEndKey - kTauntHoldStartKey);
    const int32_t ramp = curve(kWagRamp, 8, f);
    const int32_t tremble =
        f >= kTauntHoldStartKey && f < kTauntHoldEndKey ? sinp(f, K, 35) / 8192 : 0;
    const int32_t wag = static_cast<int32_t>(
        (static_cast<int64_t>(kTauntWagglePm) * ramp / 1000 * sinp(fw, K, 7)) >> 16) +
        tremble;
    const int32_t play = static_cast<int32_t>(
        (static_cast<int64_t>(kTauntPlayA16) * ramp / 1000 * sinp(fw, K, 7, 0x3000)) >> 16);
    // the wind-up pulls the WHOLE loop back (neck scale up = a crouching
    // gather), and the body dips with it
    const int32_t wind = curve(kWind, 6, f);
    loop_pose(g, 1000 + wind / 4, 1000 + wag - wind / 5, 1000 - wind / 6,
              1000 - wag,
              static_cast<int32_t>((static_cast<int64_t>(kAntennaTiltA16) *
                                    sinp(fw, K, 3)) >> 16),
              play);
    g.q[kBRoot] = quat_mul(
        g.q[kBRoot], quat_z(static_cast<int32_t>(
                         (static_cast<int64_t>(-900) * wind) / 1000)));
    // the cocky lean-in toward the viewer for the wink beat
    g.q[kBRoot] = quat_mul(
        g.q[kBRoot], quat_z(static_cast<int32_t>(
                         (static_cast<int64_t>(1400) * curve(kLean, 7, f)) / 1000)));
    face_rest(g);
    apply_twinkle(g, static_cast<int32_t>(
                         (static_cast<int64_t>(kBlazeTwinkleA16) * sinp(f, K, 2)) >> 16));
    apply_gaze(g, 0, kGazeLiftMaxA16 / 3);
    apply_squint_lr(g, curve(kWinkL, 6, f) + blink_at(f, 21), blink_at(f, 21));
    g.write(c, f);
    // wind-up crouch, then the smug settle: slower, bigger bobs after the hold
    const int32_t dip = static_cast<int32_t>(
        (static_cast<int64_t>(fxu(140)) * wind) / 1000);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(f, K, kHoverHeightMm, kBobAmpAMm * 3 / 2, kBobAmpBMm, K / 14, K / 28) - dip;
    c.deform[static_cast<size_t>(f)] = compress_at(f, K, K / 14, kCompressAmpPm +
        static_cast<int32_t>((static_cast<int64_t>(kCompressAmpPm) * wind) / 1500));
  }
  return c;
}

/** taunt-lasso, slot 12 (the second taunt): it tips forward and swings the
 *  whole loop in a circle over its head like a lasso — tilt and fold-scale
 *  in quadrature trace the peak around — bouncing on the spot, eyes
 *  following its own antenna around. */
inline zc::Clip build_taunt2() {
  const int K = kTaunt2Keys;
  zc::Clip c = clip_shell(12, K, kHoverHeightMm);
  Rig g;
  static const Key kRamp[] = {{0, 0}, {16, 0}, {32, 1000}, {88, 1000},
                              {106, 0}, {119, 0}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 12, K, f);  // pass 4: the always-on fold-hold-knead layer
    const int ramp = curve(kRamp, 6, f);  // the lasso spins up and back down
    const int32_t tilt = static_cast<int32_t>(
        (static_cast<int64_t>(kTaunt2LassoA16) * ramp / 1000 * sinp(f, K, 4)) >> 16);
    const int32_t pump = static_cast<int32_t>(
        (static_cast<int64_t>(130) * ramp / 1000 * sinp(f, K, 4, 0x4000)) >> 16);
    loop_pose(g, 1000, 1000 + pump, 1000 + pump / 2, 1000 - pump / 3, tilt);
    // PASS 3 (R10 — the one rebuild): the swing PIVOTS AT THE BODY-SIDE
    // JUNCTION. The neck bone circles (lateral x fore-aft in quadrature),
    // so the whole antenna visibly swings around its base knuckle instead
    // of only rippling — the missing-junction fault the owner named twice.
    g.q[kBJunctionF] = quat_mul(
        g.q[kBJunctionF],
        quat_mul(quat_x(static_cast<int32_t>(
                     (static_cast<int64_t>(2300) * ramp / 1000 * sinp(f, K, 4)) >> 16)),
                 quat_z(static_cast<int32_t>(
                     (static_cast<int64_t>(1500) * ramp / 1000 *
                      sinp(f, K, 4, 0x4000)) >> 16))));
    g.q[kBRoot] = quat_mul(g.q[kBRoot], quat_z(-static_cast<int32_t>(900 * ramp / 1000)));
    face_rest(g);
    // the gaze chases the lasso around
    apply_gaze(g,
               static_cast<int32_t>((static_cast<int64_t>(kGazeMaxA16 * 2 / 3) *
                                     ramp / 1000 * sinp(f, K, 4)) >> 16),
               static_cast<int32_t>((static_cast<int64_t>(kGazeLiftMaxA16 / 2) *
                                     ramp / 1000 * sinp(f, K, 4, 0x4000)) >> 16));
    apply_squint(g, blink_at(f, 47));
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(f, K, kHoverHeightMm, kBobAmpAMm, kBobAmpBMm, K / 12, K / 30);
    c.deform[static_cast<size_t>(f)] = compress_at(f, K, K / 12, kCompressAmpPm);
  }
  return c;
}

/** THE HEADSTAND TRICK, slot 13 (PASS 3 — owner-suggested: "stand on its
 *  head using the antenna"; uncuttable among the tricks). Mechanically:
 *  0..30 anticipation (gaze drops to the ground, the body gathers and
 *  compresses); 30..42 it rises slightly (the gymnast's breath);
 *  42..78 the pitch-over — root rotates a half turn about the pitch axis
 *  while climbing so the LOOP PEAK arrives at the dirt exactly at the
 *  plant key; 78..148 PLANTED — declared, authored ground contact
 *  (kTrickPlantDepthMm at the loop peak; the committed probe asserts the
 *  window and depth), body wobbling above as an inverted pendulum, the
 *  antenna flexing at the junction hinges, a slow show-off yaw so the
 *  upside-down face passes the camera; 148..186 it rights itself WITH
 *  OVERSHOOT and floats back up; then a pleased settle. */
inline zc::Clip build_trick() {
  const int K = kTrickKeys;
  zc::Clip c = clip_shell(13, K, kHoverHeightMm);
  Rig g;
  // the pitch-over in thousandths of a half turn (32768); overshoot past
  // zero on the way home, then settle
  static const Key kFlip[] = {{0, 0},   {30, 0},   {42, 0},    {56, -420},
                              {70, -840}, {78, -1000}, {148, -1000},
                              {166, 80},  {178, -40}, {186, 0}, {199, 0}};
  // root height: hover -> gather dip -> climb through the flip -> planted
  // at kTrickPlantRootMm -> lift back -> home with a small bounce
  // the approach ARRIVES at the dirt exactly at the plant key (a touch at
  // an interpolated midpoint before the declared window is the probe's
  // fault to catch — and it did); the plant height is the named constant.
  static const Key kRootY[] = {{0, 1250},  {22, 1130}, {34, 1290}, {50, 1560},
                               {66, 1790}, {78, kTrickPlantRootMm},
                               {148, kTrickPlantRootMm},
                               {162, 1420}, {174, 1180}, {186, 1290}, {199, 1250}};
  static const Key kGazeDown[] = {{0, 0}, {8, -800}, {30, -800}, {46, -300},
                                  {78, 200}, {148, 200}, {170, 500}, {186, 0},
                                  {199, 0}};
  static const Key kSquash[] = {{0, 1000}, {22, 1900}, {36, 900}, {78, 1400},
                                {100, 1100}, {148, 1200}, {166, 1900},
                                {182, 1150}, {199, 1000}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 13, K, f);  // pass 4: the always-on fold-hold-knead layer
    const int32_t flip = static_cast<int32_t>(
        (static_cast<int64_t>(32768) * curve(kFlip, 11, f)) / 1000);
    g.q[kBRoot] = quat_mul(g.q[kBRoot], quat_z(flip));
    // the balance layer FADES over the first keys of the righting instead
    // of cutting (a step in the quats is a one-frame snap)
    static const Key kBalFade[] = {{0, 1000}, {148, 1000}, {158, 0}, {199, 0}};
    const int32_t bal = f >= kTrickPlantKey && f < 158 ? curve(kBalFade, 4, f) : 0;
    const bool planted = bal > 0;
    if (planted) {
      // the inverted-pendulum balance: sway about the plant, never still
      g.q[kBRoot] = quat_mul(
          g.q[kBRoot],
          quat_z(static_cast<int32_t>(
              (static_cast<int64_t>(kTrickBalanceWobbleA16) * bal / 1000 *
               sinp(f, K, 6)) >> 16)));
      g.q[kBRoot] = quat_mul(
          g.q[kBRoot],
          quat_x(static_cast<int32_t>(
              (static_cast<int64_t>(kTrickBalanceWobbleA16 / 2) * bal / 1000 *
               sinp(f, K, 4, 0x4000)) >> 16)));
      // the slow show-off yaw: the upside-down face sweeps the camera
      g.q[kBRoot] = quat_mul(
          g.q[kBRoot],
          quat_y(static_cast<int32_t>(
              (static_cast<int64_t>(3000) * bal / 1000 *
               sinp(f, K, 2, 0x6000)) >> 16)));
      // the antenna flexes at the junction hinges while it balances
      const int32_t flex = static_cast<int32_t>(
          (static_cast<int64_t>(160) * bal / 1000 * sinp(f, K, 8)) >> 16);
      loop_pose(g, 1000 + flex, 1000 - flex / 2, 1000 + flex / 3, 1000 - flex / 4, 0);
      g.q[kBJunctionF] = quat_mul(
          g.q[kBJunctionF], quat_x(static_cast<int32_t>(
                           (static_cast<int64_t>(1100) * bal / 1000 *
                            sinp(f, K, 8, 0x3000)) >> 16)));
    } else {
      loop_rest(g);
    }
    face_rest(g);
    apply_gaze(g, 0, static_cast<int32_t>(
                     (static_cast<int64_t>(kGazeLiftMaxA16) *
                      curve(kGazeDown, 9, f)) / 1000));
    // eyes wide through the balance (effort + delight), blinks never stop
    apply_squint(g, (planted ? -280 : 0) + blink_at(f, 61));
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 1] = fxu(curve(kRootY, 11, f));
    c.deform[static_cast<size_t>(f)] = squash_impact(f, K, kSquash, 9);
  }
  return c;
}

/** DIRECTIONAL DAMAGE, slot 14 (pass 4, Direction 4 §3b). Four blows in
 *  sequence at the named contact stations. Mechanically, per blow: the
 *  struck side leads -- the root displaces AWAY from the blow over 3 keys,
 *  overshoots, and settles IN AIR with two damped bounces (it floats: no
 *  stagger, no ground brace); the antenna whips OPPOSITE through the
 *  junction hinges kDamageWhipLagKeys later and rings down; the eyes
 *  wince (squint spike + gaze snapped toward the blow); the deform squash
 *  spikes on the impact key. The LOOP-PEAK blow inverts the ratio: the
 *  antenna takes the hit (deep whip), the body follows late and less. */
inline zc::Clip build_damage() {
  const int K = kDamageKeys;
  zc::Clip c = clip_shell(14, K, kHoverHeightMm);
  Rig g;
  // per-station blow directions (unit-ish, the blow ARRIVES from this way;
  // displacement is opposite): {x, z}
  static const int32_t kBlowDir[4][2] = {{1000, 0}, {0, 1000}, {-1000, 0}, {300, 0}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, 14, K, f);  // the always-on layer (low gain)
    int32_t dx = 0, dz = 0;      // root displacement this key
    int32_t whip = 0;            // signed fold-scale whip (pm)
    int32_t wince = 0;
    int32_t gaze_side = 0;
    int32_t squash = 1000;
    for (int h = 0; h < 4; ++h) {
      const int t = f - kDamageHitKeys[h];
      if (t < 0 || t >= 56) continue;
      const bool peak = h == 3;
      const int32_t knock = peak ? kDamagePeakKnockMm : kDamageKnockMm;
      // displacement: sharp out (3 keys), overshoot, two damped bounces,
      // home by ~key 48 -- all in the air
      static const Key kD[] = {{0, 0},   {3, -1000}, {10, -780}, {16, -880},
                               {26, -420}, {34, -180}, {44, -40}, {55, 0}};
      const int32_t d = curve(kD, 8, t);
      dx += static_cast<int32_t>(static_cast<int64_t>(fxu(knock)) * d / 1000 *
                                 kBlowDir[h][0] / 1000);
      dz += static_cast<int32_t>(static_cast<int64_t>(fxu(knock)) * d / 1000 *
                                 kBlowDir[h][1] / 1000);
      // the whip: opposite, lagged, ringing down
      const int tw = t - kDamageWhipLagKeys;
      if (tw >= 0) {
        static const Key kW[] = {{0, 0},  {3, 1000}, {9, -560}, {16, 340},
                                 {24, -180}, {34, 80}, {46, 0}, {55, 0}};
        whip += static_cast<int32_t>(
            static_cast<int64_t>(peak ? kDamagePeakWhipPm : kDamageWhipPm) *
            curve(kW, 8, tw) / 1000);
      }
      // the wince: squint spike + gaze snapped toward the blow
      static const Key kWc[] = {{0, 0}, {2, 1000}, {18, 1000}, {30, 250}, {42, 0}, {55, 0}};
      wince = std::max(wince, static_cast<int32_t>(
          static_cast<int64_t>(kDamageWinceSquintPm) * curve(kWc, 6, t) / 1000));
      if (t < 26) gaze_side = kBlowDir[h][1] != 0 ? kGazeMaxA16 * 3 / 4
                                                  : (kBlowDir[h][0] > 0 ? 0 : 0);
      // the squash spikes on impact
      static const Key kSq[] = {{0, 1000}, {2, 1000}, {5, 2600}, {14, 1700},
                                {28, 1250}, {44, 1050}, {55, 1000}};
      squash = std::max(squash, static_cast<int32_t>(
          static_cast<int64_t>(kDamageSquashPm) * curve(kSq, 7, t) / 1000));
    }
    // the whip rides the junction + hinges (the same instrument as the
    // folding, used for impact)
    loop_pose(g, 1000 + whip / 3, 1000 + whip, 1000 - whip / 2, 1000 + whip / 2,
              0);
    face_rest(g);
    apply_gaze(g, gaze_side, kGazeLiftMaxA16 / 5);
    apply_squint(g, wince + blink_at(f, 13));
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 0] = dx;
    c.root[static_cast<size_t>(f) * 3 + 2] = dz;
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(f, K, kHoverHeightMm, kBobAmpAMm * 2 / 3, kBobAmpBMm, K / 30, K / 58);
    static const Key kSqBase[] = {{0, 1000}, {231, 1000}};
    (void)kSqBase;
    c.deform[static_cast<size_t>(f)] = compress_at(
        f, K, K / 30, static_cast<int32_t>(
            static_cast<int64_t>(kCompressAmpPm) * squash / 1000));
  }
  return c;
}

/** the still hover, slot 7: the fixed-camera form diagnostic pose. */
inline zc::Clip build_still() {
  zc::Clip c = clip_shell(7, 2, kHoverHeightMm);
  Rig g;
  for (int f = 0; f < 2; ++f) {
    g.reset();
    loop_rest(g);
    face_rest(g);
    g.write(c, f);
  }
  return c;
}

}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_CLIPS_H
