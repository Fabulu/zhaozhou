// Unnamed02 — clip builders + the maths helpers.
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

#ifndef ZHAO_REEL_UNNAMED02_CLIPS_H
#define ZHAO_REEL_UNNAMED02_CLIPS_H

#include "unnamed02_art.h"
#include "unnamed02_rig.h"

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
                      int32_t tilt_a16 = 0, int32_t d_play_a16 = 0) {
  const auto a = [](int32_t base, int32_t pm) {
    return static_cast<int32_t>((static_cast<int64_t>(base) * pm) / 1000);
  };
  const int32_t fn = a(kLoopFoldNeckA16, neck_pm);
  const int32_t fa = a(kLoopFoldAA16, a_pm);
  const int32_t fb = a(kLoopFoldBA16, b_pm);
  const int32_t fc = a(kLoopFoldCA16, c_pm);
  // the drawn kink/lean is a REST attitude on the neck bone (R8)
  const zc::quat16 loc_neck = quat_mul(quat_y(kNeckRestYawA16), quat_z(fn));
  const zc::quat16 loc_a =
      quat_mul(quat_z(fa), quat_x(kLoopRestTiltA16 + tilt_a16));
  const zc::quat16 loc_b = quat_z(fb);
  const zc::quat16 loc_c = quat_z(fc);
  g.q[kBNeck] = quat_mul(g.q[kBNeck], loc_neck);
  g.q[kBHingeA] = quat_mul(g.q[kBHingeA], loc_a);
  g.q[kBHingeB] = quat_mul(g.q[kBHingeB], loc_b);
  g.q[kBHingeC] = quat_mul(g.q[kBHingeC], loc_c);
  // ---- the closure aim, in 3D: quaternion-walk the chain to hinge D
  // exactly as the pose composes it (yaw and tilt included), then choose
  // D's Z-fold so the last segment points at the re-entry anchor. A Z-fold
  // can only aim within D's local XY plane, so the out-of-plane residual is
  // projected away — the anchor is deep enough that the committed closure
  // probe still proves burial across the whole fold-scale range.
  int32_t px = kLoopTubeXMm, py = kLoopNeckExitYMm, pz = 0;
  zc::quat16 Q = loc_neck;
  const zc::quat16 locs[3] = {loc_a, loc_b, loc_c};
  for (int i = 0; i < 4; ++i) {
    int32_t dx, dy, dz;
    quat_rot_vec(Q, 0, kLoopArcMm[i], 0, dx, dy, dz);
    px += dx;
    py += dy;
    pz += dz;
    if (i < 3) Q = quat_mul(Q, locs[i]);
  }
  int32_t vx, vy, vz;  // anchor - P_D, taken into C's local frame
  quat_rot_vec(quat_conj(Q), kLoopReentryXMm - px, kLoopReentryYMm - py, -pz, vx, vy, vz);
  const int32_t aim = angle16_of(vx, vy);
  g.q[kBHingeD] = quat_mul(g.q[kBHingeD], quat_z(aim + d_play_a16));
}
inline void loop_rest(Rig& g) { loop_pose(g, 1000, 1000, 1000, 1000); }

/** The face at rest: lenses rolled into their outward V and leaned back. */
inline void face_rest(Rig& g) {
  g.q[kBEyeL] = quat_mul(quat_y(kEyeYawOutA16),
                         quat_mul(quat_x(kEyeVAngleA16), quat_z(-kEyeTiltA16)));
  g.q[kBEyeR] = quat_mul(quat_y(-kEyeYawOutA16),
                         quat_mul(quat_x(-kEyeVAngleA16), quat_z(-kEyeTiltA16)));
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
}

/** Star twinkle: spin the four-point star about its outward axis. */
inline void apply_twinkle(Rig& g, int32_t spin_a16) {
  g.q[kBPupilL] = quat_mul(g.q[kBPupilL], quat_x(spin_a16));
  g.q[kBPupilR] = quat_mul(g.q[kBPupilR], quat_x(spin_a16));
}

/** Squint 0..1000: the faceted lenses rotate toward edge-on (a shutter). */
inline void apply_squint(Rig& g, int32_t amount_pm) {
  const int32_t a = static_cast<int32_t>(
      (static_cast<int64_t>(kSquintMaxA16) * amount_pm) / 1000);
  g.q[kBEyeL] = quat_mul(g.q[kBEyeL], quat_y(a));
  g.q[kBEyeR] = quat_mul(g.q[kBEyeR], quat_y(-a));
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
    loop_alive(g, f, K, K / (2 * kBobPeriodAKeys), kAntennaSwayPm, kCompressAmpPm,
               K / kCompressPeriodKeys);
    face_rest(g);
    apply_gaze(g,
               static_cast<int32_t>((static_cast<int64_t>(kGazeMaxA16) *
                                     curve(kSide, 10, f)) / 1000),
               static_cast<int32_t>((static_cast<int64_t>(kGazeLiftMaxA16) *
                                     curve(kLift, 9, f)) / 1000));
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 1] = hover_at(
        f, K, kHoverHeightMm, kBobAmpAMm, kBobAmpBMm, K / kBobPeriodAKeys, K / kBobPeriodBKeys);
    c.deform[static_cast<size_t>(f)] =
        compress_at(f, K, K / kCompressPeriodKeys, kCompressAmpPm);
  }
  return c;
}

/** drift, slot 1: the locomotion-analogue — a slow circling drift. */
inline zc::Clip build_drift() {
  const int K = kDriftKeys;
  zc::Clip c = clip_shell(1, K, kHoverHeightMm);
  Rig g;
  for (int f = 0; f < K; ++f) {
    g.reset();
    const uint16_t ph = static_cast<uint16_t>((static_cast<int64_t>(f) * 65536) / K);
    g.q[kBRoot] = quat_mul(g.q[kBRoot], quat_y(static_cast<int32_t>(ph)));
    g.q[kBRoot] = quat_mul(g.q[kBRoot], quat_x(kDriftLeanA16));
    loop_alive(g, f, K, K / kDriftSwayPeriodKeys, kAntennaSwayPm * 3 / 2, kCompressAmpPm,
               K / kDriftCompressPeriodKeys);
    face_rest(g);
    apply_gaze(g, 0, 0);  // eyes forward: it follows something
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 0] = static_cast<int32_t>(
        (static_cast<int64_t>(fxu(kDriftRadiusMm)) *
         zref::fx_cos(zref::angle16{ph}).raw) >> 16);
    c.root[static_cast<size_t>(f) * 3 + 2] = static_cast<int32_t>(
        (static_cast<int64_t>(fxu(kDriftRadiusMm)) * zref::fx_sin(zref::angle16{ph}).raw) >>
        16);
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
  static const Key kSide[] = {{0, 0}, {4, 0}, {8, 1000}, {58, 1000}, {72, 0}, {89, 0}};
  static const Key kYaw[] = {{0, 0}, {8, 0}, {16, 200}, {36, 1000}, {60, 1000},
                             {78, 0}, {89, 0}};
  static const Key kPerk[] = {{0, 1000}, {12, 1000}, {26, 880}, {58, 880},
                              {76, 1000}, {89, 1000}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    const int perk = curve(kPerk, 6, f);
    loop_pose(g, 1000, perk, perk, perk, 0);
    g.q[kBRoot] = quat_mul(
        g.q[kBRoot],
        quat_y(static_cast<int32_t>((static_cast<int64_t>(kCuriousYawA16) *
                                     curve(kYaw, 7, f)) / 1000)));
    face_rest(g);
    apply_gaze(g,
               static_cast<int32_t>((static_cast<int64_t>(kGazeMaxA16) *
                                     curve(kSide, 6, f)) / 1000),
               kGazeLiftMaxA16 / 4);
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(f, K, kHoverHeightMm, kBobAmpAMm * 2 / 3, kBobAmpBMm / 2, K / 30, K / 45);
    c.deform[static_cast<size_t>(f)] = compress_at(f, K, K / 30, kCompressAmpPm);
  }
  return c;
}

/** react-startle, slot 4: wind-up dip, the recoil payoff, deep squash on the
 *  re-entry, antenna whip with a damped settle, hard squint reopening. */
inline zc::Clip build_startle() {
  const int K = kStartleKeys;
  zc::Clip c = clip_shell(4, K, kHoverHeightMm);
  Rig g;
  static const Key kBack[] = {{0, 0},  {8, 80},  {12, -1000}, {26, -1000},
                              {48, -700}, {79, 0}};
  static const Key kUp[] = {{0, 0}, {8, -60}, {13, 1000}, {24, 500},
                            {40, 150}, {60, 0}, {79, 0}};
  static const Key kWhip[] = {{0, 1000},  {8, 1030},  {14, 780},  {22, 1160},
                              {30, 920},  {40, 1050}, {52, 980},  {64, 1000},
                              {79, 1000}};
  static const Key kSquint[] = {{0, 0}, {8, 0}, {12, 850}, {30, 850}, {48, 200},
                                {62, 0}, {79, 0}};
  static const Key kSquash[] = {{0, 1000}, {12, 1000}, {16, 2600}, {30, 2600},
                                {52, 1400}, {70, 1000}, {79, 1000}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    const int whip = curve(kWhip, 9, f);
    loop_pose(g, 1000, whip, whip, whip, 0);
    face_rest(g);
    apply_squint(g, curve(kSquint, 7, f));
    apply_gaze(g, 0, 0);
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 0] = static_cast<int32_t>(
        (static_cast<int64_t>(fxu(kStartleJumpMm)) * curve(kBack, 6, f)) / 1000);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        fxu(kHoverHeightMm) +
        static_cast<int32_t>((static_cast<int64_t>(fxu(kStartleLiftMm)) * curve(kUp, 7, f)) /
                             1000) +
        static_cast<int32_t>((static_cast<int64_t>(fxu(kBobAmpBMm)) * sinp(f, K, 2)) >> 16);
    const int32_t amp = static_cast<int32_t>(
        (static_cast<int64_t>(kCompressAmpPm) * curve(kSquash, 7, f)) / 1000);
    c.deform[static_cast<size_t>(f)] = compress_at(f, K, K / 20, amp);
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
    loop_alive(g, f, K, K / kRestSwayPeriodKeys, kAntennaSwayPm / 2, kCompressAmpPm,
               K / kRestCompressPeriodKeys);
    face_rest(g);
    apply_squint(g, kRestSquintPm);
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
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(f, K, kHoverHeightMm, kBobAmpAMm * 3 / 4, kBobAmpBMm, K / 24, K / 40);
    c.deform[static_cast<size_t>(f)] = compress_at(f, K, K / 24, kCompressAmpPm);
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

#endif  // ZHAO_REEL_UNNAMED02_CLIPS_H
