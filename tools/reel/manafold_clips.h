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

#include <cstdlib>

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

/** PASS 12 (Direction 9 SS2) -- THE PER-NODULE OFFSETS: where each ball GOES.
 *
 *  Millimetres, creature-relative, one 3D offset per nodule. All zero is the
 *  identity and is BIT-IDENTICAL to the pass-11 pose -- the solve inside
 *  loop_pose early-outs on all-zero, so that guarantee is structural rather
 *  than a rounding hope. +y is up, +x is forward (the face's side), +z is the
 *  creature's left.
 */
struct NoduleOffsets {
  int32_t ax = 0, ay = 0, az = 0;  // nodule A -- the lower-front ball
  int32_t bx = 0, by = 0, bz = 0;  // nodule B -- the peak
  int32_t cx = 0, cy = 0, cz = 0;  // nodule C -- the upper-rear ball
};

/** The per-key quat accumulator (mirrors zixx's Rig; bodies differ). */
struct Rig {
  zc::quat16 q[kBoneCount];
  // PASS 12: the nodule targets ride the rig rather than loop_pose's argument
  // list. antenna_knead sets them and loop_pose consumes them, which is the
  // ordering every clip already uses -- so not one call site changes, and a
  // clip that never touches them poses exactly as it did before.
  NoduleOffsets nod;
  /** PASS 12 (Direction 9 SS13.3 item 2) -- THE POSE-DERIVED SPAN STRETCH.
   *
   *  Per-mille of extra length each inter-nodule span needs THIS KEY, written
   *  by the nodule solve and read by nothing else. It is the SHORTFALL the
   *  solve would otherwise swallow: `nodule_aim` lands the ball along the
   *  direction of its target at exactly the bind arc length, so a target
   *  further away than that simply does not get reached. This is how far it
   *  fell short, and the deform lanes are what pay it.
   *
   *  ⚠ IT IS COMPUTED FROM THE POSED CHAIN, NEVER FROM BIND. The solve walks
   *  the chain forward in world millimetres (09-ENGINE-GOTCHAS SS15: inverting
   *  a SKINNING matrix returns bind space, which would report the rest pose's
   *  shortfall -- zero -- on every key). No matrix is inverted anywhere here.
   */
  int32_t span_pm[3] = {0, 0, 0};
  void reset() {
    for (int b = 0; b < kBoneCount; ++b) q[b] = zc::quat16_identity();
    nod = NoduleOffsets{};
    span_pm[0] = span_pm[1] = span_pm[2] = 0;
  }
  void write(zc::Clip& c, int f) const {
    for (int b = 0; b < kBoneCount; ++b)
      c.quats[static_cast<size_t>(f) * kBoneCount + b] = q[b];
    write_span_lanes(c, f);
  }
  /** Emit this key's span stretch onto deform lanes 1..3.
   *
   *  It rides `write` -- the ONE place every clip already commits a key -- so
   *  no clip builder has to remember it and no clip can carry a pose whose
   *  spans disagree with its quats. A clip whose extra track was never
   *  allocated writes nothing, which is exact identity. */
  void write_span_lanes(zc::Clip& c, int f) const {
    const size_t ex = static_cast<size_t>(zc::kDeformLaneCount) - 1u;
    if (c.deform_ex.size() != static_cast<size_t>(c.frame_count) * ex) return;
    for (int i = 0; i < 3; ++i) {
      int32_t pm = span_pm[i];
      if (pm < 0) pm = 0;  // see kSpanStretchMaxPm: the sidecar has no sign
      if (pm > kSpanStretchMaxPm) pm = kSpanStretchMaxPm;
      // spread EXPANDS the two lanes perpendicular to the named axis -- y (the
      // arc, so the span lengthens) and z. flatten CONTRACTS the named axis x,
      // the blade's broad in-plane half-width: the band thins as it stretches.
      const int32_t spread = static_cast<int32_t>(
          (static_cast<int64_t>(pm) * 65536) / 1000);
      const int32_t flat = static_cast<int32_t>(
          (static_cast<int64_t>(spread) * kSpanThinRatioPm) / 1000);
      c.deform_ex[static_cast<size_t>(f) * ex + static_cast<size_t>(i)] =
          zc::DeformSample{static_cast<uint16_t>(flat), static_cast<uint16_t>(spread)};
    }
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
// DIRECTION 7 §1: every station's out-of-plane tilt (about X) AND yaw (about
// Y), individually, on top of its fold (about Z). The argument order is
// append-only so every existing call site keeps its exact meaning and no clip
// retimes; the defaults are all zero, so a caller that does not ask for the new
// axes gets pass-7 behaviour bit for bit.
struct HingePlay {
  int32_t tilt_neck = 0, yaw_neck = 0;
  int32_t tilt_a = 0, yaw_a = 0;
  int32_t tilt_b = 0, yaw_b = 0;
  int32_t tilt_c = 0, yaw_c = 0;
};

/** Turn the span leaving `local`'s bone so it points at `target` instead of
 *  where it points now, and advance `p` to the span's new END POINT.
 *
 *  THIS IS THE CLOSURE'S OWN AIM PRIMITIVE, reused verbatim at three more
 *  stations -- which is the whole reason the nodule solve is closed-form and
 *  the loop still closes by construction. `Q` is the span's WORLD orientation
 *  before the correction, `p` its start; the span runs `len` along Q's own +y.
 *  Because Q = Q_parent * local, post-multiplying `local` by the correction
 *  gives exactly Q' = Q_parent * local * C, so the fix is local to ONE bone and
 *  every ancestor is untouched.
 *
 *  Two stages, exactly as the pass-6 C.4 closure fix: the in-plane swing about
 *  z, then the out-of-plane lift about x in the frame that swing produced,
 *  where the target's x component is zero by construction. No iteration, no IK.
 *
 *  WARNING -- THE SPAN DOES NOT STRETCH. The end lands along the direction of
 *  the target at exactly `len`. See kNoduleOffsetMaxMm for what that costs. */
inline void nodule_aim(zc::quat16& local, zc::quat16& Q, int32_t len,
                       int32_t& px, int32_t& py, int32_t& pz,
                       int32_t tx, int32_t ty, int32_t tz,
                       int32_t* short_pm = nullptr) {
  // PASS 12 (D9 SS13.3 item 2): HOW FAR SHORT THIS AIM LANDS, in per-mille of
  // the bind arc. The warning above is the whole point -- the end lands along
  // the target's direction at exactly `len`, so a target 1.2 arc-lengths away
  // is missed by 20% and nothing downstream ever knew by how much. It does now,
  // and the deform lanes spend it as span stretch.
  //
  // Measured HERE, on the forward-walked posed chain in world millimetres,
  // which is the only place it is honest: px/py/pz have already been carried by
  // every solved station above, so this is the POSED gap, not a bind one.
  if (short_pm != nullptr) {
    const int64_t dx = tx - px, dy = ty - py, dz = tz - pz;
    const int64_t want = isqrt64(dx * dx + dy * dy + dz * dz);
    *short_pm = len > 0 ? static_cast<int32_t>(((want - len) * 1000) / len) : 0;
  }
  int32_t vx, vy, vz;
  quat_rot_vec(quat_conj(Q), tx - px, ty - py, tz - pz, vx, vy, vz);
  const int32_t aim_z = angle16_of(vx, vy);
  int32_t wx, wy, wz;
  quat_rot_vec(quat_conj(quat_z(aim_z)), vx, vy, vz, wx, wy, wz);
  (void)wx;
  const int32_t aim_x = angle16_of(-wz, wy);
  const zc::quat16 corr = quat_mul(quat_z(aim_z), quat_x(aim_x));
  local = quat_mul(local, corr);
  Q = quat_mul(Q, corr);
  int32_t dx, dy, dz;
  quat_rot_vec(Q, 0, len, 0, dx, dy, dz);
  px += dx;
  py += dy;
  pz += dz;
}

inline void loop_pose(Rig& g, int32_t neck_pm, int32_t a_pm, int32_t b_pm, int32_t c_pm,
                      int32_t tilt_a16 = 0, int32_t d_play_a16 = 0,
                      int32_t tilt_b_a16 = 0, int32_t tilt_c_a16 = 0,
                      const HingePlay* play = nullptr) {
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
  // DIRECTION 7 §1: fold (Z) -> tilt (X) -> yaw (Y), in that fixed order at
  // every station. The ORDER is what makes it read as a hinge rather than as a
  // free ball joint, so it is written the same way four times on purpose.
  const int32_t pt_n = play ? play->tilt_neck : 0, py_n = play ? play->yaw_neck : 0;
  const int32_t pt_a = play ? play->tilt_a : 0, py_a = play ? play->yaw_a : 0;
  const int32_t pt_b = play ? play->tilt_b : 0, py_b = play ? play->yaw_b : 0;
  const int32_t pt_c = play ? play->tilt_c : 0, py_c = play ? play->yaw_c : 0;
  const zc::quat16 loc_a = quat_mul(
      quat_mul(quat_z(fa), quat_x(kLoopRestTiltA16 + tilt_a16 + pt_a)), quat_y(py_a));
  // PASS 6 C.1: B and C gain their out-of-plane axis. They were quat_z ONLY,
  // which is why "each hinge moves up and down separately" was geometrically
  // impossible however hard the amplitudes were pushed.
  const zc::quat16 loc_b = quat_mul(
      quat_mul(quat_z(fb), quat_x(kLoopRestTiltBA16 + tilt_b_a16 + pt_b)), quat_y(py_b));
  const zc::quat16 loc_c = quat_mul(
      quat_mul(quat_z(fc), quat_x(kLoopRestTiltCA16 + tilt_c_a16 + pt_c)), quat_y(py_c));
  // kBNeck was driven by NOTHING before Direction 7 §1 -- it existed as an
  // articulation joint and no shipped layer ever moved it. It moves now.
  if (pt_n != 0 || py_n != 0)
    g.q[kBNeck] = quat_mul(g.q[kBNeck], quat_mul(quat_x(pt_n), quat_y(py_n)));
  g.q[kBJunctionF] = quat_mul(g.q[kBJunctionF], loc_junction);
  g.q[kBHingeA] = quat_mul(g.q[kBHingeA], loc_a);
  g.q[kBHingeB] = quat_mul(g.q[kBHingeB], loc_b);
  g.q[kBHingeC] = quat_mul(g.q[kBHingeC], loc_c);
  // ---- PASS 12: THE NODULE SOLVE (Direction 9 SS2) ------------------------
  // Runs BEFORE the closure walk, so the closure sees the nodule rotations and
  // still lands the return arm -- the same ordering rule antenna_knead already
  // depends on, for the same reason.
  //
  // Each nodule's TARGET is its CARRIED position plus its offset: solve A, and
  // B's position has already moved because A brought its section with it. That
  // is the owner's sentence made arithmetic -- "all the nodules should be able
  // to move individually AND BRING THE ANTENNAE PARTS WITH THEM" -- and it is
  // why this walks down the chain once rather than treating the three as
  // independent of each other's carry.
  //
  // WHICH BONE CARRIES WHICH NODULE, and it is not the obvious one:
  //   nodule A is moved by kBNeck   -- P_A = P_JF + (Q_JF*Q_neck)*(0,arc1,0)
  //   nodule B is moved by kBHingeA -- the span A->B leaves A's frame
  //   nodule C is moved by kBHingeB -- the span B->C leaves B's frame
  // A station's own rotation never moves its own ball; it moves the NEXT one.
  // (kLoopArcMm[0] is 0, so kBJunctionF and kBNeck share a pivot and the span
  // to A is a single 680 mm run in Q_JF*Q_neck's frame.)
  //
  // The all-zero early-out is the bit-identity guarantee: a clip that authors
  // no offsets takes not one extra arithmetic operation, so pass 11's accepted
  // antenna pose is reproduced exactly rather than approximately.
  {
    const NoduleOffsets& nd = g.nod;
    if ((nd.ax | nd.ay | nd.az | nd.bx | nd.by | nd.bz | nd.cx | nd.cy | nd.cz) !=
        0) {
      const auto cl = [](int32_t v, int32_t lim) {
        return v > lim ? lim : (v < -lim ? -lim : v);
      };
      int32_t nx = kLoopTubeXMm, ny = kLoopNeckExitYMm, nz = 0;
      zc::quat16 NQ = quat_mul(g.q[kBJunctionF], g.q[kBNeck]);
      int32_t ex, ey, ez;
      // span 1: junction+neck -> nodule A
      quat_rot_vec(NQ, 0, kLoopArcMm[1], 0, ex, ey, ez);
      nodule_aim(g.q[kBNeck], NQ, kLoopArcMm[1], nx, ny, nz,
                 nx + ex + cl(nd.ax, kNoduleOffsetMaxMm[0]),
                 ny + ey + cl(nd.ay, kNoduleOffsetMaxMm[1]),
                 nz + ez + cl(nd.az, kNoduleOffsetMaxMm[2]),
                 &g.span_pm[0]);
      // span 2: nodule A -> nodule B
      NQ = quat_mul(NQ, g.q[kBHingeA]);
      quat_rot_vec(NQ, 0, kLoopArcMm[2], 0, ex, ey, ez);
      nodule_aim(g.q[kBHingeA], NQ, kLoopArcMm[2], nx, ny, nz,
                 nx + ex + cl(nd.bx, kNoduleOffsetMaxMm[0]),
                 ny + ey + cl(nd.by, kNoduleOffsetMaxMm[1]),
                 nz + ez + cl(nd.bz, kNoduleOffsetMaxMm[2]),
                 &g.span_pm[1]);
      // span 3: nodule B -> nodule C
      NQ = quat_mul(NQ, g.q[kBHingeB]);
      quat_rot_vec(NQ, 0, kLoopArcMm[3], 0, ex, ey, ez);
      nodule_aim(g.q[kBHingeB], NQ, kLoopArcMm[3], nx, ny, nz,
                 nx + ex + cl(nd.cx, kNoduleOffsetMaxMm[0]),
                 ny + ey + cl(nd.cy, kNoduleOffsetMaxMm[1]),
                 nz + ez + cl(nd.cz, kNoduleOffsetMaxMm[2]),
                 &g.span_pm[2]);
    }
  }

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
  // ---- PASS 10 C.1: AIM AT THE POSED ANCHOR, NOT THE BIND ONE -------------
  // kBLoopBase2 was dead twice over. It skins nothing (nothing is bound to it;
  // its ball part went when the knuckles moved into the chain's own skin at
  // pass 6), AND its authored rotation could not move anything either, because
  // the closure read kLoopReentryXMm/YMm -- BIND constants -- as its target.
  // kKneadWagB2A16 has been driving it 900 a16 every frame since pass 4 and
  // moving precisely nothing, while three separate comments said otherwise.
  //
  // The anchor is a point on the body, kLoopReentry* from the body centre, so
  // carrying it by kBLoopBase2's own rotation SLIDES IT ALONG THE BODY SURFACE
  // -- it stays at the same radius by construction. The whole return arm then
  // visibly re-aims as the knead wags that bone, which is one of the owner's
  // two named junctions (Direction 7 section 9.1, "the two spots where the
  // antennae meet the creature") coming alive at ZERO skinning cost.
  //
  // The free-floating-dongle fault stays structurally excluded: the aim still
  // lands on a point inside the body, so the arm still plunges into it.
  //
  // At rest this is bit-identical -- loop_rest() poses a fresh rig where
  // kBLoopBase2 is identity, so the rotated offset IS the bind offset.
  int32_t rax, ray, raz;
  quat_rot_vec(g.q[kBLoopBase2], kLoopReentryXMm, kLoopReentryYMm, 0, rax, ray, raz);
  int32_t vx, vy, vz;  // anchor - P_D, taken into C's local frame
  quat_rot_vec(quat_conj(Q), rax - px, ray - py, raz - pz, vx, vy, vz);
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

/** PASS 12 WAVE 2a (Direction 9 SS6/SS6.1/SS12.3) -- THE EYE TRAVEL.
 *
 *  The eye assembly RIDES AROUND THE BODY on its carrier bone, tracing the
 *  surface. `pm` is a signed fraction of kEyeTravelMaxA16, per eye.
 *
 *  ⚠ IT MUST BE CALLED BEFORE face_rest's attitude is composed onto the EYE
 *  bones -- it writes the CARRIER, which is their parent, so the order between
 *  them does not actually matter. It is written first anyway, because the
 *  reading is "the eye goes there, and then it is an eye".
 *
 *  ⚠ ONE ANGLE FOR BOTH EYES, AND THAT IS A MEASURED DECISION, not tidiness.
 *  The signature took a per-eye pm first, and putting travel into the composed
 *  sweep failed gate A immediately: at corner 1696 -- MIRRORED travel at one
 *  eighth of the range, 5.6 degrees, with the roll at a quarter of its own cap
 *  -- the two eye assemblies closed to 0 mm. Mirrored travel carries them
 *  through each other, and the zero sits deep MID-RAMP with both endpoints
 *  clean, which is gotcha SS17 exactly and is the same shape as the eye lab's
 *  own travel sign bug.
 *
 *  So the mirrored case is now UNREPRESENTABLE rather than merely discouraged:
 *  there is one parameter and both carriers take it. That is also the right
 *  READ -- both eyes taking the same world rotation is a head turn, one eye
 *  coming toward the viewer while the other goes away, which is precisely the
 *  near-eye case SS12.2 is about. And their separation is preserved EXACTLY,
 *  because a rigid rotation about a shared axis preserves every distance.
 *  (`manafold-probe --fail-mirror` reinstates the mirrored pose on the carriers
 *  and is the witnessed failing leg for this.)
 *
 *  SS12.3 IS SATISFIED HERE WITHOUT A SECOND MECHANISM. The carrier is the
 *  eye's PARENT, so its rotation turns the lens and both stars through the same
 *  angle: they arrive facing outward instead of edge-on. Nothing slides against
 *  anything, so SS5a/SS5b's one-rigid-unit rule is structural rather than
 *  maintained. */
inline int32_t eye_travel_a16(int32_t pm) {
  if (pm > 1000) pm = 1000;
  if (pm < -1000) pm = -1000;
  return static_cast<int32_t>((static_cast<int64_t>(kEyeTravelMaxA16) * pm) / 1000);
}

inline void apply_eye_travel(Rig& g, int32_t pm) {
  const int32_t a = eye_travel_a16(pm);
  if (a == 0) return;
  g.q[kBEyeTravelL] = quat_mul(g.q[kBEyeTravelL], quat_y(a));
  g.q[kBEyeTravelR] = quat_mul(g.q[kBEyeTravelR], quat_y(a));
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

/** Star twinkle: spin the four-point star about its outward axis.
 *
 *  PASS 7: CLAMPED. This was the 5c leash's actual violator -- 142 mm of
 *  overhang against a 24 mm cap and 220 pm of the star on the purple against a
 *  600 pm floor, while the gaze (25 mm at full amplitude) sat comfortably
 *  inside. Every clip's schedule is left exactly as authored; the clamp is the
 *  structural guarantee that no future schedule can walk the star off the eye
 *  again, the way tuning a constant alone would allow. */
inline void apply_twinkle(Rig& g, int32_t spin_a16) {
  const int32_t s = spin_a16 > kStarTwinkleMaxA16    ? kStarTwinkleMaxA16
                    : spin_a16 < -kStarTwinkleMaxA16 ? -kStarTwinkleMaxA16
                                                     : spin_a16;
  g.q[kBPupilL] = quat_mul(g.q[kBPupilL], quat_x(s));
  g.q[kBPupilR] = quat_mul(g.q[kBPupilR], quat_x(s));
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
  // PASS 12: lanes 1..4. Allocated identity for EVERY clip, because the nodule
  // schedule is always on (D9 SS2) and therefore so is the span stretch --
  // there is no such thing as a Manafold clip whose spans never move. Identity
  // samples cost nothing: deform_skin_vertex_lanes skips a zero term before it
  // rounds anything.
  c.deform_ex.assign(static_cast<size_t>(keys) * (zc::kDeformLaneCount - 1u),
                     zc::DeformSample{});
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

/** DIRECTION 7 §1: the per-station hinge play, shared by every layer that
 *  poses the loop, so the two of them cannot drift apart.
 *
 *  Station 0 is the neck, 1..3 are hinges A, B and C. Each gets its own scale
 *  (kHingeAxisScalePm), its own phase (kHingePhaseStepA16 per station, so the
 *  motion TRAVELS along the antenna instead of every ball wobbling together),
 *  and two axes running at rates that do not divide into each other or into the
 *  fold's -- that is what keeps a bounded motion from reading as a metronome
 *  while still reading as guided.
 *
 *  Deterministic and closed-form: no state, no hash, no per-clip table. Same
 *  frame in, same pose out. */
inline void hinge_play(HingePlay& hp, int f, int keys, int cyc) {
  if (cyc < 1) cyc = 1;
  // PASS 12 (D4): the rates are PERIODS IN KEYS now, not multipliers of the
  // caller's cycle count. `cyc` is still taken -- it keeps the signature and
  // every call site unchanged -- but it no longer sets the rate, because
  // multiplying it by 3 and 5 is what put a 3.9 Hz tilt and a 6.5 Hz yaw on
  // every station of the idle. See kHingeTiltPerKeys.
  //
  // Integer cycle counts across `keys` are what makes the layer loop
  // seamlessly, so the division is deliberate and the floor of 1 is the
  // short-clip case (curious at 90 keys gets one yaw cycle, not a fraction
  // of one, which would step at the loop point).
  const int tcyc = keys / kHingeTiltPerKeys > 0 ? keys / kHingeTiltPerKeys : 1;
  const int ycyc = keys / kHingeYawPerKeys > 0 ? keys / kHingeYawPerKeys : 1;
  (void)cyc;
  int32_t t[4], y[4];
  for (int st = 0; st < 4; ++st) {
    const int32_t ph = -kHingePhaseStepA16 * st;
    t[st] = static_cast<int32_t>(
        (static_cast<int64_t>(kHingeTiltAmpA16) * kHingeAxisScalePm[st] / 1000 *
         sinp(f, keys, tcyc, ph)) >> 16);
    y[st] = static_cast<int32_t>(
        (static_cast<int64_t>(kHingeYawAmpA16) * kHingeAxisScalePm[st] / 1000 *
         sinp(f, keys, ycyc, ph + 0x4000)) >> 16);  // quarter-cycle off its tilt
  }
  hp.tilt_neck = t[0]; hp.yaw_neck = y[0];
  hp.tilt_a = t[1];    hp.yaw_a = y[1];
  hp.tilt_b = t[2];    hp.yaw_b = y[2];
  hp.tilt_c = t[3];    hp.yaw_c = y[3];
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
  HingePlay hp;
  hinge_play(hp, f, keys, cyc);  // DIRECTION 7 §1
  loop_pose(g, 1000 + couple, 1000 + sa, 1000 + sb, 1000 + sc, tilt, 0, 0, 0, &hp);
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
  HingePlay hp;
  hinge_play(hp, f, K, cycA);  // DIRECTION 7 §1
  loop_pose(g, 1000 + wave(2), 1000 + wave(1), 1000 + wave(0), 1000 + wave(1), tilt,
            0, 0, 0, &hp);
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

// DIRECTION 7 §3: kSegDrift is new and it is FIRST in every cycle. In it the
// hands are open, the loop returns toward its rest area, and the mote cloud's
// coherence -- which is derived from that area, in manafold_fx.h -- falls back
// to the channel look on its own. That is the "standard" the owner asks for,
// and the fold becomes punctuation between drifts rather than a permanent
// state (Direction 4's "going on all the time" is superseded).
enum FoldSeg : uint8_t { kSegDrift = 0, kSegGather, kSegHold, kSegKnead, kSegRelease };
struct FoldPhase {
  FoldSeg seg;
  int32_t amp_pm;    // grip envelope 0..1000 (gather ramps, hold/knead hold)
  int32_t agit_pm;   // knead-waggle envelope 0..1000 (knead only)
  int32_t morph_pm;  // 0..1000 progress from shape_from to shape_to
  uint8_t shape_from, shape_to;
};

// MUST equal u02::kFoldStencilCount in manafold_fx.h. It cannot be *derived*
// from it -- this header is included first, so that symbol does not exist yet --
// so the equality is enforced by a static_assert in fx.h instead, and that
// assert is `==`, not `<=`: drift in EITHER direction fails the build.
// Pass 12 added BOLT, COIL and CROSS and this still said 6, so three figures
// were authored and unreachable. Same fault class as the table bounds that
// shipped off-by-one three passes running.
constexpr int kFoldShapeCount = 9;  // ring, star, bar, crescent, triangle,
                                    // s-curl, BOLT, COIL, CROSS

/** ease 0..1000 -> 0..1000, smoothstep-ish (integer). */
inline int32_t fold_ease(int32_t t) {
  if (t < 0) t = 0;
  if (t > 1000) t = 1000;
  return t * t / 1000 * (3000 - 2 * t) / 1000;
}

/** PASS 13 (R3) -- THE ATTACK EASE. Fast out, decelerating in: 1 - (1-t)^3.
 *
 *  `fold_ease` is a smoothstep. It leaves slowly AND arrives slowly, which is
 *  correct for a breath, a sway or a lean and is exactly wrong for a beat. The
 *  whole of `build_taunt3` was authored through it, so every gesture drifted
 *  into place and nothing ever SNAPPED -- which is the entire content of the
 *  standing "taunt3 is not funny" verdict.
 *
 *  This is the other half of the vocabulary. It reaches 27% in the first tenth
 *  of its span and 70% in the first third, so a three-key attack is a three-key
 *  attack; and because it arrives on a flat tangent it still settles rather
 *  than slamming, which keeps 07-MOTION-STYLE's "nothing twitches".
 *
 *  Amplitude goes up and reversal density does not: this changes WHEN the
 *  motion happens, never how many times it turns around. */
inline int32_t punch_ease(int32_t t) {
  if (t < 0) t = 0;
  if (t > 1000) t = 1000;
  const int32_t u = 1000 - t;
  return 1000 - static_cast<int32_t>((static_cast<int64_t>(u) * u / 1000) * u / 1000);
}

/** PASS 11 F.3 -- THE PRESS-RECOVER WAVE, which replaces sinp on the knead wag.
 *
 *  Same contract as sinp: returns -65536..65536, integer cycles per clip so the
 *  loop is seamless. What differs is the SHAPE. A sine spends equal time going
 *  each way and never dwells; this rises fast, DWELLS at the extreme, then eases
 *  home slowly -- press, hold, release. The speed is on the payoff.
 *
 *  ⚠ IT RISES ONCE AND FALLS ONCE PER CYCLE, at every parameter value. The
 *  rejected spazz reads were reversal DENSITY, not amplitude, so a waveform that
 *  cannot express a reversal is the guard, rather than a band someone has to
 *  remember to check. Both limbs are smoothstepped, so there is no velocity step
 *  at the plateau edges either -- a hold that snaps in is a different fault.
 *
 *  `phase_pm` is a phase offset in thousandths of a cycle (the wag's second
 *  waveform used sinp's 0x4000, a quarter cycle: 250 here).
 */
inline int32_t press_wave(int f, int keys, int cycles, int32_t phase_pm = 0) {
  if (cycles <= 0 || keys <= 0) return 0;
  int64_t t = (static_cast<int64_t>(f) * cycles * 1000) / keys + phase_pm;
  t %= 1000;
  if (t < 0) t += 1000;
  const int32_t rise = kKneadPressRisePm;
  const int32_t hold = kKneadPressHoldPm;
  const int32_t fall = 1000 - rise - hold;
  int32_t u;  // 0..1000, the press's own progress
  if (t < rise) {
    u = fold_ease(static_cast<int32_t>(t * 1000 / (rise > 0 ? rise : 1)));
  } else if (t < rise + hold) {
    u = 1000;  // THE HOLD. This is the beat the review said was missing.
  } else {
    const int32_t v = static_cast<int32_t>((t - rise - hold) * 1000 /
                                           (fall > 0 ? fall : 1));
    u = 1000 - fold_ease(v);
  }
  // 0..1000 -> -65536..65536: the press swings through neutral, as the sine did
  return static_cast<int32_t>((static_cast<int64_t>(u) * 2 - 1000) * 65536 / 1000);
}

/** PASS 11 F.3 -- the per-cycle accent, and the leading ball.
 *
 *  `station` is 0..4 (Jf, Neck, A, B, C). Returns a per-mille gain for THIS
 *  cycle at THIS station. Hashed off fold_phase's own stream, keyed on the
 *  cycle index, so it loops with the clip and never visibly repeats.
 *
 *  It scales a monotone envelope and therefore cannot introduce a reversal --
 *  see press_wave. That is deliberate: the accents had to be safe by
 *  construction, not by a band someone checks afterwards.
 */
inline int32_t knead_accent_pm(uint32_t slot, int f, int keys, int cycles,
                               int station) {
  if (cycles <= 0 || keys <= 0) return 1000;
  const uint32_t n = static_cast<uint32_t>(
      (static_cast<int64_t>(f) * cycles) / keys);
  const uint32_t h = fx_hash(0xACCE7Fu + slot, n, 0x2Bu);
  const int32_t span = kKneadAccentHiPm - kKneadAccentLoPm;
  int32_t g = kKneadAccentLoPm + static_cast<int32_t>(h % static_cast<uint32_t>(span + 1));
  // ...and one station leads this press.
  const uint32_t lead = (h >> 19) % 5u;
  if (static_cast<uint32_t>(station) == lead)
    g = static_cast<int32_t>((static_cast<int64_t>(g) * kKneadLeadBoostPm) / 1000);
  return g;
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
    int32_t drift = (kDriftKeysBase + static_cast<int32_t>((h >> 4) % kDriftKeysHash)) * 16;
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
    // DIRECTION 7 §3: the drift joins the compression, and it is compressed
    // HARDER than the rest -- a short clip that spent most of itself drifting
    // would never show a shape at all, which trades one fault (the fold ran
    // permanently) for its mirror image.
    if (n == 0 && release_at > 4 * 16 && drift + gather + hold + knead > release_at) {
      const int32_t cyc = drift + gather + hold + knead;
      drift = drift * release_at / cyc / 2;
      gather = gather * release_at / cyc;
      hold = hold * release_at / cyc;
      if (drift < 16) drift = 16;
      if (gather < 16) gather = 16;
      if (hold < 16) hold = 16;
      knead = release_at - drift - gather - hold;  // no rounding gap: the knead
      if (knead < 16) knead = 16;                  // hands straight to release
    }
    next_shape = static_cast<uint8_t>((h >> 24) % kFoldShapeCount);
    if (next_shape == shape)
      next_shape = static_cast<uint8_t>((next_shape + 1 + (h >> 28) % (kFoldShapeCount - 1)) %
                                        kFoldShapeCount);
    const int32_t d_end = seg_start + drift;
    const int32_t g_end = d_end + gather, h_end = g_end + hold, k_end = h_end + knead;
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
    if (kq4 < d_end) {
      // DIRECTION 7 §3: the DRIFT. The hands ease open to a floor and hold
      // there; they do NOT go slack, or the next gather reads as a snap.
      ph.seg = kSegDrift;
      const int32_t t = (kq4 - seg_start) * 1000 / drift;
      const int32_t ease_keys = 250;  // per-mille of the drift spent easing
      int32_t e;
      if (t < ease_keys) e = 1000 - fold_ease(t * 1000 / ease_keys);
      else if (t > 1000 - ease_keys) e = fold_ease((1000 - t) * 1000 / ease_keys);
      else e = 0;
      // the clip's FIRST drift starts from zero, matching the release the loop
      // seam left behind, exactly as the first gather used to
      if (n == 0) e = 0;
      ph.amp_pm = kDriftAmpFloorPm + (1000 - kDriftAmpFloorPm) * e / 1000;
      ph.agit_pm = 0;
      ph.morph_pm = 0;
      ph.shape_to = shape;  // nothing is being formed: do not advertise a shape
      return ph;
    }
    if (kq4 < g_end) {
      ph.seg = kSegGather;
      const int32_t t = (kq4 - d_end) * 1000 / gather;
      if (n == 0) {
        // the clip's first gather rises from the drift's floor (DIRECTION 7
        // §3; it used to rise from zero, which is now the drift's job)
        ph.amp_pm = kDriftAmpFloorPm +
                    (1000 - kDriftAmpFloorPm) * fold_ease(t) / 1000;
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
 *  closure probe gates the bank). That ordering is now load-bearing in a
 *  second way: pass 10 C.1 has loop_pose read kBLoopBase2's POSED anchor, and
 *  this function is what poses it.
 *
 *  PASS 10 C.4 — a FOURTH false comment, which QA did not catch: this said
 *  "the back-junction ball rides its offset bind, so kBLoopBase2's rotation
 *  SLIDES the ball along the body surface". There is no back-junction ball —
 *  the rigid ball parts went at pass 6 — and until C.1 the rotation slid
 *  nothing at all, because the closure aimed at bind constants. What is true
 *  now: kBLoopBase2's rotation slides the closure's ANCHOR POINT along the
 *  body surface, and the return arm re-aims at it. No geometry is skinned to
 *  the bone; the effect is entirely through the aim. */
/** PASS 12 (Direction 9 SS2) -- the always-on nodule schedule.
 *
 *  NINE INDEPENDENT TRACKS: three nodules, three axes, each on its own period
 *  from kNodulePerKeys and its own amplitude from kNoduleAmpMm. Because the
 *  periods are mutually prime, the configuration the owner named -- "the middle
 *  one might go down while the other two swing up" -- arises on its own, along
 *  with every other configuration, instead of being one authored pose that
 *  would read as a loop.
 *
 *  This is the difference from every previous pass: those drove one envelope
 *  through per-station LAGS, which makes three copies of one curve. Nine
 *  clocks make three independent nodules.
 *
 *  Integer cycle counts across `keys` keep the loop seamless; the floor of 1 is
 *  the short-clip case. */
inline NoduleOffsets nodule_schedule(uint32_t slot, int keys, int f) {
  NoduleOffsets n;
  const int32_t gain =
      slot < static_cast<uint32_t>(kNoduleClipSlots) ? kNoduleClipPm[slot] : 800;
  if (gain <= 0) return n;
  const auto trk = [&](int nod, int axis) {
    const int per = kNodulePerKeys[nod][axis];
    const int cyc = keys / per > 0 ? keys / per : 1;
    const int32_t amp = kNoduleAmpMm[nod][axis] * gain / 1000;
    return static_cast<int32_t>(
        (static_cast<int64_t>(amp) *
         sinp(f, keys, cyc, static_cast<int32_t>(0x1000 * (nod * 3 + axis)))) >>
        16);
  };
  n.ax = trk(0, 0); n.ay = trk(0, 1); n.az = trk(0, 2);
  n.bx = trk(1, 0); n.by = trk(1, 1); n.bz = trk(1, 2);
  n.cx = trk(2, 0); n.cy = trk(2, 1); n.cz = trk(2, 2);
  return n;
}

/** PASS 13 R1(c) -- THE EYE TRAVEL IS A SCHEDULE: DWELL, THEN GLANCE.
 *
 *  D9 SS12.1: "They should always be centered unless they DECIDE to move."
 *  Pass 12 drove this channel from two always-on sines, so the eyes were at an
 *  arbitrary angle on every key of every clip and never once at centre. The
 *  main idle then spent most of its loop looking somewhere the camera was not.
 *
 *  Now: the eye rests at centre with a small living drift, and takes at most
 *  kEyeGlanceCount deliberate looks per loop -- eased out, HELD, eased back.
 *  The first still reaches the full 45 deg the owner asked for; it is an event
 *  with a decision in front of it, not a resting place.
 *
 *  PERIODIC BY CONSTRUCTION. Every window is evaluated modulo `keys`, so the
 *  loop seam needs no arithmetic luck and no integer cycle count -- which also
 *  retires the pass-12 defect where two `keys/divisor` counts collapsed to the
 *  same frequency on any clip under 122 keys.
 *
 *  THE SUM CANNOT RIDE THE CLAMP. The dwell drift is scaled down by how far
 *  out the glance is, so |glance| + |drift| <= 1000 by construction. Pass 12's
 *  700+300 could reach 1000 and sit there (44% of the idle, a column of
 *  identical 45.00 readings in the gate); that is now unrepresentable, not
 *  merely avoided.
 *
 *  Rides antenna_knead for the same reason the nodule schedule does -- it is
 *  the one layer every performing clip already calls. `eye_pm` at the call
 *  site is untouched, so the deaths' travel fade still works exactly as the
 *  pass-12 fix wave left it. */
/** PASS 13 R1(b): THE COMMITTED TRAVEL PIN, and it exists because D9 SS12.2
 *  asks for a PICTURE. "Plates showing the star and its outline surviving 45
 *  degrees" cannot be made from the bank: the shipping cameras orbit, so every
 *  frame at a different travel angle is also at a different camera, and a
 *  ladder built that way measures the camera. `U02_EYE_TRAVEL_PIN=<pm>` holds
 *  the channel at one angle for a whole render, so the SAME frame index of the
 *  SAME subject can be rendered at 0 / 333 / 667 / 1000 pm and the four tiles
 *  differ in nothing but the travel.
 *
 *  Unset it and this is identity -- the schedule below runs untouched -- which
 *  is the same shape as U02_FOLD_FREEZE and kept for the same reason: a
 *  diagnostic that lives in a run folder is orphaned by the next pass. */
inline int32_t eye_travel_pin_pm(bool& pinned) {
  static const int32_t v = [] {
    const char* e = std::getenv("U02_EYE_TRAVEL_PIN");
    return e && *e ? std::atoi(e) : 0x7FFFFFFF;
  }();
  pinned = v != 0x7FFFFFFF;
  return pinned ? (v > 1000 ? 1000 : v < -1000 ? -1000 : v) : 0;
}

inline int32_t eye_travel_life_pm(uint32_t slot, int keys, int f) {
  bool pinned = false;
  const int32_t pin = eye_travel_pin_pm(pinned);
  if (pinned) return pin;
  const int span = keys > 0 ? keys : 1;
  // The ramps have a FLOOR IN KEYS. A fraction of a short clip is a snap, and
  // a snap on this channel is what QA Q2's 8 deg/key step check exists to
  // catch (it is how a switched-off carrier looks). Held to at most a sixth of
  // the clip each so the floor can never swallow the loop.
  const auto win_keys = [&](int32_t pm, int floor_k) {
    int k = static_cast<int>((static_cast<int64_t>(span) * pm) / 1000);
    if (k < floor_k) k = floor_k;
    if (k > span / 6) k = span / 6;
    if (k < 1) k = 1;
    return k;
  };
  const int rise = win_keys(kEyeGlanceRisePm, kEyeGlanceMinRampKeys);
  const int hold = win_keys(kEyeGlanceHoldPm, 1);
  const int fall = win_keys(kEyeGlanceFallPm, kEyeGlanceMinRampKeys);
  const int win = rise + hold + fall;
  // Drop glances until the schedule fits with real dwell between them. A short
  // clip gets ONE good look rather than three crowded ones.
  int n = kEyeGlanceCount;
  while (n > 1 && n * (win + kEyeGlanceMinDwellKeys) > span) --n;
  const int32_t phase = static_cast<int32_t>(
      (static_cast<int64_t>(span) *
       (kEyeGlancePhasePm + static_cast<int32_t>(slot) * kEyeGlanceSlotSkewPm)) / 1000);
  int32_t glance = 0;
  for (int i = 0; i < n; ++i) {
    const int32_t skew =
        static_cast<int32_t>((static_cast<int64_t>(span) * kEyeGlanceSkewPm[i]) / 1000);
    const int start = i * span / n + phase + skew;
    int d = (f - start) % span;
    if (d < 0) d += span;
    int32_t env = 0;
    if (d < rise) {
      env = fold_ease(d * 1000 / rise);
    } else if (d < rise + hold) {
      env = 1000;
    } else if (d < win) {
      env = 1000 - fold_ease((d - rise - hold) * 1000 / fall);
    }
    if (env == 0) continue;
    glance += static_cast<int32_t>(
        (static_cast<int64_t>(kEyeGlanceOutPm[i]) * env) / 1000);
  }
  if (glance > 1000) glance = 1000;
  if (glance < -1000) glance = -1000;
  // THE DWELL IS NOT A FREEZE -- a small drift keeps the centred eye alive,
  // faded out under a glance so the sum can never reach the clamp.
  const int32_t room = 1000 - (glance < 0 ? -glance : glance);
  const int cyc = span / kEyeDwellPeriodKeys > 0 ? span / kEyeDwellPeriodKeys : 1;
  const int32_t drift = static_cast<int32_t>(
      (static_cast<int64_t>(kEyeDwellDriftPm) * room / 1000 *
       sinp(f, span, cyc, static_cast<int32_t>((slot * 9973u) & 0xFFFF))) >> 16);
  int32_t pm = glance + drift;
  if (pm > 1000) pm = 1000;
  if (pm < -1000) pm = -1000;
  return pm;
}

/** `eye_pm` is the per-clip eye-travel gain in per-mille, 1000 by default so
 *  no existing call site changes. It exists for the same reason kKneadClipPm
 *  and kNoduleClipPm do -- a clip that needs the eyes to stop must be able to
 *  say so -- and the two deaths are why it was needed on the first day: see
 *  the fade at their call sites. */
inline void antenna_knead(Rig& g, uint32_t slot, int keys, int f,
                          int32_t eye_pm = 1000) {
  // The eye travel rides here because this is the one layer every PERFORMING
  // clip calls (build_still and build_nodule_solo deliberately do not). It
  // writes the carrier bones, which nothing else in this function touches.
  apply_eye_travel(g, static_cast<int32_t>(
      (static_cast<int64_t>(eye_travel_life_pm(slot, keys, f)) * eye_pm) / 1000));
  // PASS 12: the nodule targets for this key. Set here because antenna_knead
  // already runs before every clip's loop_pose call, which is where they are
  // consumed. A clip whose kNoduleClipPm entry is 0 gets all-zero offsets and
  // therefore the exact pass-11 pose.
  g.nod = nodule_schedule(slot, keys, f);
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
  // out-of-plane; the rear junction's closure ANCHOR slides. One consistent
  // period. (PASS 11, QA §7.4: this said "the back ball slides" -- the
  // identical claim pass 10 corrected 60 lines above, inside this same
  // function, and missed here. There is no back ball; the rigid ball parts
  // went at pass 6. What kBLoopBase2 slides is loop_pose's aim target.)
  if (ph.agit_pm > 0 || ph_b.agit_pm > 0 || ph_c.agit_pm > 0) {
    const int cyc = keys / kKneadWagPeriodKeys > 0 ? keys / kKneadWagPeriodKeys : 1;
    // PASS 11 F.3: press-recover, not a sine. Same amplitude, different phrasing.
    const int32_t w1 = press_wave(f, keys, cyc);
    const int32_t w2 = press_wave(f, keys, cyc, 250);  // a quarter cycle, as 0x4000 was
    // ...and this cycle is not like the last one. Station indices: Jf 0, Neck 1,
    // A 2, B 3, C 4 -- one of them leads each press.
    const auto acc = [&](int station) {
      return knead_accent_pm(slot, f, keys, cyc, station);
    };
    const auto ax = [&](int32_t v, int station) {
      return static_cast<int32_t>((static_cast<int64_t>(v) * acc(station)) / 1000);
    };
    g.q[kBJunctionF] = quat_mul(
        g.q[kBJunctionF],
        quat_z(ax(static_cast<int32_t>((static_cast<int64_t>(a(kKneadWagJfA16, ph.agit_pm)) * w1) >> 16), 0)));
    g.q[kBHingeC] = quat_mul(
        g.q[kBHingeC],
        quat_z(-ax(static_cast<int32_t>((static_cast<int64_t>(a(kKneadWagCA16, ph_c.agit_pm)) * w1) >> 16), 4)));
    g.q[kBNeck] = quat_mul(
        g.q[kBNeck],
        quat_x(ax(static_cast<int32_t>((static_cast<int64_t>(a(kKneadWagNeckA16, ph_neck.agit_pm)) * w2) >> 16), 1)));
    g.q[kBHingeB] = quat_mul(
        g.q[kBHingeB],
        quat_z(ax(static_cast<int32_t>((static_cast<int64_t>(a(kKneadWagBA16, ph_b.agit_pm)) * w2) >> 16), 3)));
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
    // PASS 14 / R2(c) -- THE THIRTEEN-CYCLE BREATH, PLUS TWO BIG SLOW ONES.
    // The fast breath is untouched (it is the life layer and it lags the
    // wobble by a station clock, which is why the phase term is what it is);
    // what is new is a swell underneath it at kIdleSwellCycles, so the body's
    // ENVELOPE changes across the clip instead of only its surface. See the
    // note beside kIdleSwellPm for why raising the fast breath could not do
    // this: thirteen cycles sampled eight times is 1.63 cycles per tile.
    {
      const int cyc = K / kWobblePerAKeys;
      const zc::DeformSample fast = compress_at(
          f, K, cyc, kCompressAmpPm * kIdleFastBreathPm / 1000,
          -static_cast<int32_t>((65536LL * 3 * kWobbleLagKeys * cyc) / K));
      const int32_t sw = (65536 + press_wave(f, K, kIdleSwellCycles)) / 2;  // 0..65536
      int32_t flat = static_cast<int32_t>(fast.flatten) +
                     static_cast<int32_t>(
                         (static_cast<int64_t>(kCompressAmpPm) * kIdleSwellPm / 1000 * sw) >> 16);
      if (flat > 60000) flat = 60000;  // the ceiling; see kCompressAmpPm
      const int32_t spread =
          static_cast<int32_t>((static_cast<int64_t>(flat) * kSpreadRatioPm) / 1000);
      c.deform[static_cast<size_t>(f)] =
          zc::DeformSample{static_cast<uint16_t>(flat), static_cast<uint16_t>(spread)};
    }
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
  // ⚠ R2 (pass 13): THIS CLIP TRAVELS, so its wrap partner must carry the
  // traverse instead of folding back across it. With the flag off the last
  // key's sub-frame blends toward key 0 and the root wraps the WHOLE journey in
  // half a key -- an enormous fake velocity that teleports the pose and paints
  // grey speed-smear ghosts beside it (`drift`: near-grey pixels 295 -> 729 over
  // its last frames, then 59 at f0). `zc::Clip::wrap_root_delta` is default-OFF
  // so Zixxtrixx stays bit-identical; these four clips opt in. See
  // PASS-13-FINDINGS-C SS2 and tools/reel/wrapseam.py.
  c.wrap_root_delta = true;
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
  // PASS 7: the brow, keyed against the SAME beats as kSide so the tilt reads
  // as part of the look rather than as a separate wobble. Negative draws the
  // tops together (intent); it eases off on the glance away at 44.
  static const Key kBrow[] = {{0, 0},    {8, -250}, {16, -900}, {38, -900},
                              {44, 150}, {50, 150}, {54, -1000}, {66, -900},
                              {78, 0},   {89, 0}};
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
    // PASS 7 (Direction 5 5d): THE BROW. apply_eye_roll() shipped with ZERO
    // CALLERS -- the owner asked for it explicitly ("eyes should also be able
    // to rotate and rotate back... just for expressiveness") and nothing in the
    // bank used it, so 0 of 2204 frames intersected only because the feature
    // was inert. On a face drawn as a LAMBDA the roll changes the lambda angle,
    // which is the only brow this animal has: no mouth, no nose, nothing else.
    //
    // Here it tracks the double-take: the tops draw TOGETHER as it fixes on
    // the thing (intent), release on the glance away, and snap back harder on
    // the second look. Same sign on both eyes = a symmetric brow.
    apply_eye_roll(g, curve(kBrow, 9, f), curve(kBrow, 9, f));
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
  //
  // PASS 14 / R4's cheap rider (B4 has no delivery on record). The snap was
  // real and there was nothing after it: the curve reached its extreme at key
  // 12 and immediately started ringing, -980, -1080, -1000, -960, so the
  // startle read as a wobble that began violently. Same fault as taunt3's, at
  // a twentieth of the size -- an arrival nobody is given a frame to see.
  //
  // The attack is one key sharper (8 -> 11, three keys) and the extreme is now
  // HELD for ten keys, which is twenty frames on screen and clears
  // 07-MOTION-STYLE's sixteen. The ring that follows is untouched: it is the
  // recoil, it is correct, and it now has something to be a recoil FROM.
  // No new reversals and no new keys -- the same nine entries, re-placed.
  static const Key kBack[] = {{0, 0},    {8, 140},   {11, -1300}, {21, -1300},
                              {30, -980}, {42, -1080}, {52, -960}, {74, -60},
                              {79, 0}};
  static const Key kUp[] = {{0, 0},    {8, -170},  {11, 1300}, {21, 1300},
                            {30, 750}, {42, 990},  {52, 640},  {74, 40},
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
    // PASS 12 A6 (Direction 3 SS7's third debt): THE SPLAY. antenna_knead has
    // already filled g.nod with the always-on schedule; this ADDS to it rather
    // than replacing it, so the startle still breathes and simply flings its
    // nodules apart on the snap. Signs oppose -- outers one way, middle the
    // other -- which is what makes it a splay and not a lean. It rides kWide,
    // the recoil curve the clip already owns, so no new schedule and no new
    // reversals.
    {
      const int32_t sp = static_cast<int32_t>(
          (static_cast<int64_t>(kStartleSplayMm) * -curve(kWide, 6, f)) / 1000);
      g.nod.az += sp;
      g.nod.bz -= sp;
      g.nod.cz += sp;
    }
    const int whip = curve(kWhip, 9, f);
    loop_pose(g, 1000, whip, whip, whip, 0);
    face_rest(g);
    apply_squint(g, curve(kWide, 6, f) + blink_at(f, 70));
    apply_gaze(g, 0, f >= 12 && f < 40 ? kGazeLiftMaxA16 / 2 : 0);
    // PASS 7 (Direction 5 5d): "tops rolling apart reads surprised or soft".
    // The startle's whole payoff is the eyes flying wide, so the brow goes the
    // OPPOSITE way from curious's -- positive, tops apart -- on the same curve
    // the widen already uses, which costs no new schedule.
    apply_eye_roll(g, curve(kWide, 6, f), curve(kWide, 6, f));
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
  // ⚠ R2 (pass 13): THIS CLIP TRAVELS, so its wrap partner must carry the
  // traverse instead of folding back across it. With the flag off the last
  // key's sub-frame blends toward key 0 and the root wraps the WHOLE journey in
  // half a key -- an enormous fake velocity that teleports the pose and paints
  // grey speed-smear ghosts beside it (`drift`: near-grey pixels 295 -> 729 over
  // its last frames, then 59 at f0). `zc::Clip::wrap_root_delta` is default-OFF
  // so Zixxtrixx stays bit-identical; these four clips opt in. See
  // PASS-13-FINDINGS-C SS2 and tools/reel/wrapseam.py.
  c.wrap_root_delta = true;
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
  // ⚠ R2 (pass 13): THIS CLIP TRAVELS, so its wrap partner must carry the
  // traverse instead of folding back across it. With the flag off the last
  // key's sub-frame blends toward key 0 and the root wraps the WHOLE journey in
  // half a key -- an enormous fake velocity that teleports the pose and paints
  // grey speed-smear ghosts beside it (`drift`: near-grey pixels 295 -> 729 over
  // its last frames, then 59 at f0). `zc::Clip::wrap_root_delta` is default-OFF
  // so Zixxtrixx stays bit-identical; these four clips opt in. See
  // PASS-13-FINDINGS-C SS2 and tools/reel/wrapseam.py.
  c.wrap_root_delta = true;
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
  // PASS 7: the lopsided brow -- the left top drives in hard through the hold,
  // the right only drifts. That asymmetry IS the smirk.
  static const Key kBrowL[] = {{0, 0}, {40, 0}, {58, -400}, {66, -1000},
                               {88, -1000}, {139, 0}};
  static const Key kBrowR[] = {{0, 0}, {44, 0}, {62, -120}, {86, -260},
                               {104, 0}, {139, 0}};
  // and the cross-eyed beat, held inside the frozen hold and released with it
  static const Key kCross[] = {{0, 0}, {60, 0}, {68, 900}, {86, 900}, {96, 0}};
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
    // PASS 7 (Direction 5 5d + 5b rule 4): the taunt gets an ASYMMETRIC brow
    // and the cross-eyed beat, and both were dead code before this pass.
    //
    // The brow is deliberately lopsided -- one top drawn in hard, the other
    // barely -- which is the cocky smirk this clip's whole joke is built on,
    // and it lands on the same 62..86 frozen hold that carries the wink.
    apply_eye_roll(g, curve(kBrowL, 6, f), curve(kBrowR, 6, f));
    // apply_gaze_lr() ALSO shipped with zero callers. Direction 5 5b rule 4:
    // "Asymmetry is allowed and wanted -- two independently aimed stars on one
    // apparent point is what sells a googly eye... cross-eyed is a CHOICE for a
    // taunt, not a default." This is that choice, and the only place in the
    // bank that takes it: through the hold the two stars converge inward on
    // each other. Everywhere else the symmetric apply_gaze() still runs, so no
    // other clip changes by this existing.
    {
      const int32_t cross = curve(kCross, 5, f);
      const int32_t side = static_cast<int32_t>(
          (static_cast<int64_t>(kGazeMaxA16) * cross) / 1000);
      apply_gaze_lr(g, -side, kGazeLiftMaxA16 / 3, side, kGazeLiftMaxA16 / 3);
    }
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

/** PASS 12 -- THE NODULE-SOLO DIAGNOSTIC CLIP (Direction 9 SS2), slot 16.
 *
 *  This clip IS the acceptance evidence for the headline item, and it exists
 *  because four passes reported antenna motion and the owner said the bones
 *  were never added. It shows one thing per segment, at the offset ceiling,
 *  with everything else at exact rest:
 *
 *    seg 0  nodule A alone: a vertical arc, then a lateral one
 *    seg 1  nodule B alone: the same two arcs
 *    seg 2  nodule C alone: the same two arcs
 *    seg 3  THE OWNER'S CONFIGURATION -- the middle nodule DOWN while the
 *           outer two swing UP, which is the sentence this whole mechanism
 *           was built to be able to express.
 *
 *  It deliberately runs NO knead layer and NO wobble: the antenna is at rest
 *  apart from the nodule under test, so anything that moves is the nodule and
 *  the section it carries. A diagnostic that also breathes proves nothing.
 *
 *  `sinp(local, seg, 1)` runs 0 -> +1 -> 0 -> -1 -> 0 across a segment, so
 *  every segment boundary is exact rest and the clip loops seamlessly. */
inline zc::Clip build_nodule_solo() {
  zc::Clip c = clip_shell(16, kNoduleSoloKeys, kHoverHeightMm);
  Rig g;
  const int S = kNoduleSoloSegKeys;
  for (int f = 0; f < kNoduleSoloKeys; ++f) {
    g.reset();
    const int seg = f / S;
    const int lf = f % S;
    // first half of a segment is the VERTICAL arc, second half the LATERAL --
    // "Sideways. Up, down." named separately because they are separate axes
    // and a single diagonal swing would show neither cleanly.
    const int32_t vert = lf < S / 2
        ? static_cast<int32_t>((static_cast<int64_t>(kNoduleSoloAmpMm) *
                                sinp(lf, S / 2, 1)) >> 16)
        : 0;
    const int32_t lat = lf >= S / 2
        ? static_cast<int32_t>((static_cast<int64_t>(kNoduleSoloAmpMm) *
                                sinp(lf - S / 2, S / 2, 1)) >> 16)
        : 0;
    NoduleOffsets n;
    if (seg == 0) {
      n.ay = vert; n.az = lat;
    } else if (seg == 1) {
      n.by = vert; n.bz = lat;
    } else if (seg == 2) {
      n.cy = vert; n.cz = lat;
    } else {
      // "the middle one might go down while the other two swing up".
      // The mix is NOT 1:1 and kNoduleSoloMidPm explains why: offsets are
      // relative to the CARRIED position, so a middle that drops carries the
      // rear down with it by more than it drops itself. The middle therefore
      // takes the smaller share. Ball A's rise is small and that is geometry,
      // not a bug -- see kNoduleSoloMidPm's note.
      const int32_t w = static_cast<int32_t>(
          (static_cast<int64_t>(kNoduleSoloAmpMm) * sinp(lf, S, 1)) >> 16);
      n.ay = static_cast<int32_t>((static_cast<int64_t>(w) * kNoduleSoloOutPm) / 1000);
      n.by = -static_cast<int32_t>((static_cast<int64_t>(w) * kNoduleSoloMidPm) / 1000);
      n.cy = static_cast<int32_t>((static_cast<int64_t>(w) * kNoduleSoloOutPm) / 1000);
      // ...and A gets its sideways swing too, because a vertical request moves
      // it 3 mm and the segment must still show three nodules doing three
      // different things.
      n.az = static_cast<int32_t>((static_cast<int64_t>(w) * kNoduleSoloOutPm) / 1000);
    }
    g.nod = n;
    loop_pose(g, 1000, 1000, 1000, 1000);
    face_rest(g);
    g.write(c, f);
  }
  return c;
}

// ================= PASS 12 / WAVE 2b — THE THEATRICAL CLIPS =================
//
// Direction 9 §11 (fall, taunts, DEATHS) and §15 (the mana lasso).
//
// WHY THESE ARE POSSIBLE ONLY NOW. Every one of them is a PERFORMANCE PLAYED
// ON AN INSTRUMENT, and the instrument was finished this pass: each nodule
// moves individually and CARRIES ITS ANTENNA SECTION (§2, proved by slot 16),
// the body is round and bouncy (§5/§9), and the motion has a deliberate,
// low-reversal-density quality (§9.2) that a throw and a death both need.
// Authoring any of this on the pass-11 rig would have meant authoring it twice.
//
// THE STANDARD, and the trap inside it. "Expressive and theatrical" — but
// ⚠ "careful not to spazz out — it needs to look deliberate." 07-MOTION-STYLE:
// REVERSAL DENSITY is the spazz signature, NOT amplitude. Pass 12 cut antenna
// churn from 94.7 to 32.7 and this lane does not give that back, so every beat
// below is a PRESS AND A HOLD: the nodules go somewhere, arrive, and STAY
// there long enough to read, and the schedule oscillator that would have run
// underneath is switched off per clip (kNoduleClipPm 17..21 are all zero).
//
// SLOTS: 17 death-drop · 18 death-gutter · 19 lasso · 20 blown · 21 taunt3.
// All ADDITIVE. No existing builder is restructured (implementer 2A is in this
// file on the eyes at the same time).

/** ⚠ THE DEATH GATE'S FAILABLE LEGS (checklist §0: "every gate proved failable
 *  through the real code path, with a witnessed failing leg"). Each leg
 *  REMOVES the mechanism from the same builder the verdict is taken on -- it
 *  is not a separate broken copy of the clip, which is the trick that lets a
 *  gate look proven while it is measuring something it can never fail on.
 *
 *    1  the corpse KEEPS BREATHING   -- the tail's deform is left running
 *    2  the corpse RESTS AT ZERO     -- the settle root goes to the surface,
 *                                       which is the "reads as hovering" fault
 *                                       the ground-contact law names by name
 *    5  the eyes SNAP DEAD           -- the eye-travel fade is removed, so the
 *                                     carrier drops from up to 45 deg to
 *                                     identity in ONE key at the settle
 *    4  the corpse WRAPS TO ALIVE  -- hold_last is left off, so the final
 *                                     key's sub-frame blends toward key 0 and
 *                                     the corpse stands up for two frames on a
 *                                     clip that loops (review item 1)
 *    3  a strike DROWNS the creature  -- one impact drives the root far below
 *                                       the settle, which is the crash bound
 *                                       the airborne gate exists to catch
 *
 *  ⚠ LEG 3 WAS SOMETHING ELSE FIRST, AND IT COULD NOT FAIL. It removed the
 *  impact dips so "the strikes never touch" -- but with the dips gone the
 *  strikes sit at exactly kDeathRestRootMm, which is the settle height, which
 *  is 25 mm INSIDE the dirt and therefore passes. Run, watched, and it printed
 *  all-OK: a leg that cannot fail is the exact thing a failable leg exists to
 *  rule out, and it was only visible because the leg was actually run rather
 *  than reasoned about. Leg 2 already covers the not-touching fault.
 *
 *  Set before the first call to u02::type() -- the bank is a function-local
 *  static, so a leg is one process, not a toggle. Default 0 ships. */
inline int g_u02_death_fail = 0;

/** PASS 14 / R2(c) -- THE CORPSE'S HELD SAG, shared by both deaths.
 *
 *  One function so the two death performances cannot drift to different
 *  corpses, and so the QA gate has exactly one value to compare against. It
 *  takes no frame argument ON PURPOSE: a corpse that could depend on `f` is a
 *  corpse that could breathe, and the type is the guard. */
inline constexpr int32_t kCorpseFlatMax = kCompressAmpPm * kDeathCorpseFlatPm / 1000;
inline zc::DeformSample corpse_sample() {
  return zc::DeformSample{
      static_cast<uint16_t>(kCorpseFlatMax),
      static_cast<uint16_t>(static_cast<int64_t>(kCorpseFlatMax) * kSpreadRatioPm / 1000)};
}

constexpr uint16_t kDeathSlot = 17;
constexpr uint16_t kDeathBSlot = 18;
constexpr uint16_t kLassoSlot = 19;
constexpr uint16_t kBlownSlot = 20;
constexpr uint16_t kTaunt3Slot = 21;
// PASS 12 / WAVE 3: the plain FLIGHT clip (D5 SS7's first line). Appended,
// so every existing slot id and every existing clip stays bit-identical.
constexpr uint16_t kFlightSlot = 22;

/** WHICH CLIPS STAGE ON FLAT GROUND -- ONE definition, TWO consumers.
 *
 *  ⚠ THIS EXISTS BECAUSE THE RULE WAS DUPLICATED AND WENT STALE THE MOMENT A
 *  SLOT WAS ADDED. `subject_u02_clip` in zhao_reel.cpp set `bump_ext = 18` for
 *  slots 1 and 8, and `manafold_probe.cpp`'s travelling-column probe carried
 *  its own hand-copied `slot_id == 1 || slot_id == 8`. Adding the flight clip
 *  made the probe measure a DIFFERENT STAGE from the one the renderer builds --
 *  it reported "slot 22 ... bump_ext 6" against a renderer staging it flat.
 *  The numbers happened to agree this time. That is luck, not a check, and it
 *  is gate checklist item 10 wearing the other creature's clothes.
 *
 *  A travelling clip stages FLAT (the Zixxtrixx walk precedent): the reel
 *  ground-snaps the root with ONE column query at the stage centre, so a mound
 *  under a moving path is terrain the creature walks into. Everything else
 *  keeps the mound and barely moves. */
inline constexpr bool flat_staged_slot(uint16_t slot) {
  return slot == 1 || slot == 8 || slot == kFlightSlot;
}

/** Sum of a key-interval table. constexpr so the clip length is a compile-time
 *  constant the reel subject can name, and so retiming a bounce in the table
 *  retimes the clip rather than leaving a stale total behind. */
constexpr int sum_keys(const int* t, int n) {
  int s = 0;
  for (int i = 0; i < n; ++i) s += t[i];
  return s;
}
constexpr int kDeathKeys = kDeathFailKey + kDeathDropKeys +
                           sum_keys(kDeathIntervalKeys, kDeathBounces) +
                           kDeathTailKeys;
constexpr int kDeathBKeys = kDeathBLetGoKey + kDeathBDropKeys +
                            sum_keys(kDeathBIntervalKeys, kDeathBBounces) +
                            kDeathBTailKeys;

/** THE DEATH'S SCHEDULE, derived once from the tables and shared by the clip
 *  builder AND the committed ground-contact probe. One source: a probe that
 *  re-derives a declared window from its own copy of the arithmetic can bless
 *  a window the clip does not actually have. */
struct DeathBeats {
  int impact[kDeathBounces];  // the key each strike lands on
  int dipk[kDeathBounces];    // keys the body spends IN the dirt after it
  int settle;                 // eternal rest begins
  int keys;
};
inline DeathBeats death_beats() {
  DeathBeats b{};
  int k = kDeathFailKey + kDeathDropKeys;
  for (int i = 0; i < kDeathBounces; ++i) {
    b.impact[i] = k;
    const int iv = kDeathIntervalKeys[i];
    // the contact beat can never eat more than half a short interval, or the
    // last two bounces would be contact with no flight between them
    b.dipk[i] = kDeathImpactDipKeys < iv / 2 ? kDeathImpactDipKeys
                                             : (iv / 2 > 0 ? iv / 2 : 1);
    k += iv;
  }
  b.settle = k;
  b.keys = k + kDeathTailKeys;
  return b;
}
inline DeathBeats deathb_beats() {
  DeathBeats b{};
  int k = kDeathBLetGoKey + kDeathBDropKeys;
  for (int i = 0; i < kDeathBBounces; ++i) {
    b.impact[i] = k;
    const int iv = kDeathBIntervalKeys[i];
    b.dipk[i] = kDeathImpactDipKeys < iv / 2 ? kDeathImpactDipKeys
                                             : (iv / 2 > 0 ? iv / 2 : 1);
    k += iv;
  }
  b.settle = k;
  b.keys = k + kDeathBTailKeys;
  return b;
}

/** The root height of a bouncing corpse at key f, in mm.
 *
 *  Three regimes, and the middle one is the whole point:
 *    f <  fail          it still floats (the caller adds the last stutter)
 *    f <  impact[0]     the DROP: t-squared, the float simply gone
 *    per bounce i       dipk keys of CONTACT (driven BELOW the settle height
 *                       by the strike, easing back), then a parabolic flight
 *                       to kDeathApexMm[i] and back down to the next strike
 *    f >= settle        eternal rest, exactly kDeathRestRootMm, forever
 *
 *  The dip exists because of the ground-contact law: a ball that stops at
 *  exactly zero reads as HOVERING, and a corpse that touches the dirt without
 *  entering it reads as weightless. The penetration is declared and probed.
 *
 *  `apex` and `iv` come from the tables, so the decay is authored per bounce
 *  rather than generated by a restitution coefficient. */
inline int32_t death_root_at(const DeathBeats& B, const int32_t* apex,
                             const int* ivt, int nb, int fail_key, int drop_keys,
                             const int32_t* dip, int f) {
  // FAILABLE LEG 2: the corpse settles at the surface instead of IN it. The
  // 740 mm is the rest pose's own lowest-vertex offset, so this parks the
  // creature at exactly zero -- which reads as hovering, which is the fault.
  const int32_t rest = g_u02_death_fail == 2 ? 740 : kDeathRestRootMm;
  if (f <= fail_key) return kHoverHeightMm;
  if (f < B.impact[0]) {
    const int64_t t = (static_cast<int64_t>(f - fail_key) << 16) / drop_keys;
    const int64_t g = (t * t) >> 16;  // gravity's curve, not a ramp
    return kHoverHeightMm -
           static_cast<int32_t>((static_cast<int64_t>(kHoverHeightMm - rest) * g) >> 16);
  }
  for (int i = 0; i < nb; ++i) {
    const int a = B.impact[i], iv = ivt[i];
    if (f < a || f >= a + iv) continue;
    const int lf = f - a, dk = B.dipk[i];
    if (lf < dk) {  // IN the dirt: the strike, and the climb back out of it
      // FAILABLE LEG 3: no dip -- the strikes stop AT the surface. A spear
      // that stops at the surface reads weightless, and so does a corpse.
      // FAILABLE LEG 3: one strike DROWNS it -- 5x the authored dip, which is
      // the crash the airborne/contact bound is there to refuse.
      const int32_t d = g_u02_death_fail == 3 ? dip[i] * 5 : dip[i];
      return rest - d * (dk - lf) / dk;
    }
    const int fl = iv - dk > 0 ? iv - dk : 1;
    const int32_t u = (lf - dk) * 1000 / fl;  // 0..1000 across the flight
    // 4*A*u*(1-u): the parabola with its apex at exactly A, mid-flight
    return rest + static_cast<int32_t>((4LL * apex[i] * u * (1000 - u)) / 1000000);
  }
  return rest;
}

/** ⚠ THE MANA MUST RESPOND — "a dead conduit should not keep folding shapes."
 *
 *  0 = dead, 1000 = alive. Read by manafold_fx.h, which scales the fold's
 *  coherence, its agitation and its mote count by it and cuts the lightning
 *  strand outright below kFoldLightningCutPm. Every other slot returns 1000,
 *  so no existing clip's mana changes by one splat.
 *
 *  The two deaths gutter DIFFERENTLY, which is half of what makes them two
 *  deaths: the drop keeps folding until the float fails and then loses it
 *  across the bounces; the gutter loses the mana FIRST and the body follows. */
inline int32_t fold_life_pm(uint32_t slot, int keys, int32_t kq4) {
  const int f = kq4 / 16;
  if (slot == kDeathSlot) {
    if (f <= kDeathFailKey) return 1000;
    const DeathBeats B = death_beats();
    if (f >= B.settle) return 0;  // eternal rest is DARK
    const int span = B.settle - kDeathFailKey;
    return 1000 - 1000 * (f - kDeathFailKey) / (span > 0 ? span : 1);
  }
  if (slot == kDeathBSlot) {
    // out by the time the third nodule goes limp -- 20 keys before the body
    // even lets go. The light goes out, THEN the creature falls.
    const int out = kDeathBLimpKey[2];
    if (f >= out) return 0;
    return 1000 - 1000 * f / (out > 0 ? out : 1);
  }
  (void)keys;
  return 1000;
}

/** THE MANA LASSO's flight (Direction 9 §15), shared with manafold_fx.h.
 *
 *  ⚠ IT IS NOT A NEW PRIMITIVE. `active` pins the fold's figure to stencil 0,
 *  THE RING — "a loop is a shape the fold vocabulary already knows" — and the
 *  three fields below then translate, scale and spin that existing shape. The
 *  substance, the edge, the motes and the knead are the ones the fold already
 *  ships, which is what makes it read as the creature's own mana.
 *
 *  ⚠ THE RETURN IS AUTHORED, because §7.8 says "leaving stuff hanging in space
 *  just looks like a glitch". The ring flies out and OPENS, snags at the catch,
 *  is REELED BACK IN — cinching tight as it comes, the way a rope does — and
 *  arrives home at the pocket at exactly its normal size and zero offset, so
 *  the hand-back to the ordinary fold has no step in it. */
struct LassoState {
  bool active = false;
  int32_t off_mm[3] = {0, 0, 0};
  int32_t scale_pm = 1000;
  int32_t spin_a16 = 0;
};
inline LassoState lasso_at(uint32_t slot, int keys, int32_t kq4) {
  LassoState L;
  if (slot != kLassoSlot) return L;
  const int f = kq4 / 16;
  if (f < kLassoReleaseKey || f >= kLassoHomeKey) return L;
  L.active = true;
  int32_t t;  // 0..1000 along the throw
  if (f < kLassoCatchKey) {
    t = (f - kLassoReleaseKey) * 1000 / (kLassoCatchKey - kLassoReleaseKey);
  } else if (f < kLassoReelKey) {
    t = 1000;  // SNAGGED: it hangs on the target while the antennae take the jerk
  } else {
    t = 1000 - (f - kLassoReelKey) * 1000 / (kLassoHomeKey - kLassoReelKey);
  }
  const int32_t e = fold_ease(t);
  for (int k = 0; k < 3; ++k)
    L.off_mm[k] = static_cast<int32_t>((static_cast<int64_t>(kLassoThrowMm[k]) * e) / 1000);
  // the LOFT: a thrown loop arcs, it does not slide along a rail
  L.off_mm[1] += static_cast<int32_t>((4LL * kLassoArcMm * e * (1000 - e)) / 1000000);
  if (f < kLassoReelKey) {
    L.scale_pm = 1000 + static_cast<int32_t>(
                            (static_cast<int64_t>(kLassoOutScalePm - 1000) * e) / 1000);
  } else {
    // reeled home: it CINCHES to kLassoHomeScalePm over the first 70% of the
    // return, then relaxes back to normal so the hand-back is seamless
    const int32_t r = (f - kLassoReelKey) * 1000 / (kLassoHomeKey - kLassoReelKey);
    L.scale_pm = r < 700
        ? kLassoOutScalePm +
              (kLassoHomeScalePm - kLassoOutScalePm) * fold_ease(r * 1000 / 700) / 1000
        : kLassoHomeScalePm + (1000 - kLassoHomeScalePm) *
                                  fold_ease((r - 700) * 1000 / 300) / 1000;
  }
  L.spin_a16 = static_cast<int32_t>(
      (static_cast<int64_t>(kLassoSpinA16) * (f - kLassoReleaseKey)) & 0xFFFF);
  (void)keys;
  return L;
}

/** DEATH ONE — "the drop", slot 17. THE OWNER'S OWN MECHANISM, built as
 *  written: "multiple bounces on the ground before coming to eternal rest."
 *
 *  IT FLOATS, SO LOSING THE FLOAT IS THE DEATH. There is no flail and no
 *  clutch — the one beat before the fall is the float STUTTERING (two failing
 *  catches, keys 6..18), and then it simply stops holding itself up.
 *
 *  ⚠ AUTHORED, DECLARED EXCEPTION TO 07-MOTION-STYLE'S NEVER-OFF LIFE LAYER.
 *  "A corpse that keeps breathing is not dead" (D9 §11.2). From the settle key
 *  the deform sample is BIT-ZERO, the blink is off, the twinkle is off, the
 *  gaze is frozen and the nodules hold one authored droop. This is the one
 *  clip in the bank permitted byte-identical frames, and the tail is 108 keys
 *  of them on purpose: that stillness IS the eternal rest. Every other clip's
 *  life floor is untouched.
 *
 *  ⚠ DECLARED GROUND CONTACT (the law): the body's underside enters the dirt
 *  at five strikes — keys death_beats().impact[i], for dipk[i] keys each, by
 *  up to kDeathImpactDipMm[i] of root drive — and rests from the settle key to
 *  the end at kDeathSettleDepthMm. manafold_probe.cpp walks every posed vertex
 *  of every key against terrain height and fails on anything outside that.
 *
 *  THE MANA RESPONDS: fold_life_pm() above takes the fold from 1000 at the
 *  stutter to 0 at the settle. The conduit goes out as the creature does.
 *
 *  LOOP HONESTY: the site loops clips. The decay plus the long dark tail is
 *  what keeps the wrap from reading as a resurrection — but it IS a wrap, and
 *  the caption says so rather than pretending otherwise. */
inline zc::Clip build_death_drop() {
  const DeathBeats B = death_beats();
  const int K = B.keys;
  zc::Clip c = clip_shell(kDeathSlot, K, kHoverHeightMm);
  Rig g;
  // the float's last two failing catches, before it gives up entirely
  static const Key kStutter[] = {{0, 0}, {6, 0}, {9, -170}, {12, -40},
                                 {15, -210}, {18, 0}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    const bool dead = f >= B.settle;
    const bool falling = f > kDeathFailKey;
    // how far through the dying we are, 0..1000 -- one signal, many users
    const int32_t gone = !falling ? 0
                       : dead     ? 1000
                                  : 1000 * (f - kDeathFailKey) /
                                        (B.settle - kDeathFailKey);
    // ⚠ THE EYES STOP LOOKING AROUND AS IT DIES, and they stop CONTINUOUSLY.
    // antenna_knead is not called once dead, so the travel carrier snaps back
    // to identity in one key -- up to 45 deg on both eyes, at the exact instant
    // the corpse is supposed to go still. The nodules were protected from this
    // (their droop eases in on fold_ease(gone)); the travel arrived with no
    // equivalent, which is the half of the wave-2a edit that never got written.
    // The gain reaches 0 exactly at the settle key, so the dead branch's
    // identity carrier is where the fade was already going.
    if (!dead) antenna_knead(g, kDeathSlot, K, f,
                              g_u02_death_fail == 5 ? 1000 : 1000 - fold_ease(gone));
    // THE NODULES HANG. The schedule is off for this slot (kNoduleClipPm[17]
    // is 0), so the only nodule motion is this: one authored droop that
    // arrives across the bounces and then never moves again. A living
    // oscillator under a corpse is precisely the fault D9 §11.2 names.
    {
      NoduleOffsets n;
      const int32_t d = fold_ease(gone);
      const auto s = [&](int32_t mm) {
        return static_cast<int32_t>((static_cast<int64_t>(mm) * d) / 1000);
      };
      // ...and each strike WHIPS the antenna before the droop wins. The whip
      // is one press per impact, not a vibration: it decays with the bounce.
      int32_t whip = 0;
      for (int i = 0; i < kDeathBounces; ++i) {
        const int lf = f - B.impact[i];
        if (lf >= 0 && lf < 2 * B.dipk[i])
          whip = kDeathImpactDipMm[i] * (2 * B.dipk[i] - lf) / (2 * B.dipk[i]);
      }
      n.ax = s(kDeathDroopMm[0][0]); n.ay = s(kDeathDroopMm[0][1]) - whip / 3;
      n.az = s(kDeathDroopMm[0][2]);
      n.bx = s(kDeathDroopMm[1][0]); n.by = s(kDeathDroopMm[1][1]) + whip;
      n.bz = s(kDeathDroopMm[1][2]);
      n.cx = s(kDeathDroopMm[2][0]); n.cy = s(kDeathDroopMm[2][1]) + whip * 2 / 3;
      n.cz = s(kDeathDroopMm[2][2]);
      g.nod = n;
    }
    // the antenna goes SLACK: the fold scale opens past rest and stops there
    const int32_t slack = 1000 + (kDeathSlackPm - 1000) * fold_ease(gone) / 1000;
    loop_pose(g, slack, slack, slack, slack, 0);
    // the corpse's final attitude -- it does not park itself level
    g.q[kBRoot] = quat_mul(
        g.q[kBRoot],
        quat_mul(quat_z(static_cast<int32_t>(
                     (static_cast<int64_t>(kDeathRestPitchA16) * fold_ease(gone)) / 1000)),
                 quat_x(static_cast<int32_t>(
                     (static_cast<int64_t>(kDeathRestRollA16) * fold_ease(gone)) / 1000))));
    face_rest(g);
    // THE EYES: wide on the stutter (it knows), then closing across the
    // bounces, then SHUT and still. No blink after the settle -- that is the
    // life layer, and this is the clip where the life layer is over.
    const int32_t shut = fold_ease(gone);
    apply_squint(g, dead ? 1000
                         : (falling ? shut : -300) + (falling ? 0 : blink_at(f, 11)));
    apply_gaze(g, 0, dead ? -kGazeLiftMaxA16 : -kGazeLiftMaxA16 * shut / 1000);
    g.write(c, f);
    // the last seconds of the float, then the trajectory takes over entirely
    const int32_t y =
        falling ? fxu(death_root_at(B, kDeathApexMm, kDeathIntervalKeys,
                                    kDeathBounces, kDeathFailKey, kDeathDropKeys,
                                    kDeathImpactDipMm, f))
                : hover_at(f, K, kHoverHeightMm, kBobAmpAMm, kBobAmpBMm, 1, 1) +
                      fxu(curve(kStutter, 6, f));
    c.root[static_cast<size_t>(f) * 3 + 1] = y;
    // ⚠ THE DEFORM STOPS. Ramping to exactly zero over kDeathDeformFadeKeys
    // after the last strike, and bit-zero from the settle key onward.
    // FAILABLE LEG 1: the tail's deform is left running -- the corpse keeps
    // breathing, which is the fault D9 §11.2 names in those words.
    if (dead && g_u02_death_fail != 1) {
      // ⚠ NOT BIT-ZERO ANY MORE, AND THAT IS THE POINT. Zero flatten is the
      // round bind ellipsoid, so the old corpse was the ROUNDEST shape in the
      // clip. This is a HELD, non-zero sag: still to the count, and flatter
      // than any living inhale. See kDeathCorpseFlatPm.
      c.deform[static_cast<size_t>(f)] = corpse_sample();
    } else {
      int32_t flat = 0;
      // each strike squashes, decaying with the bounce; between strikes the
      // body is a round thing in the air and holds its shape
      for (int i = 0; i < kDeathBounces; ++i) {
        const int lf = f - B.impact[i];
        const int win = 2 * B.dipk[i];
        if (lf >= 0 && lf < win) {
          const int32_t sq = kDeathImpactSquashPm * kDeathApexMm[i] / kDeathApexMm[0];
          const int32_t v = kCompressAmpPm * sq / 1000 * (win - lf) / win;
          if (v > flat) flat = v;
        }
      }
      // the last breath, fading out entirely before the settle
      const int keys_left = B.settle - f;
      const int32_t life = keys_left >= kDeathDeformFadeKeys
                               ? 1000
                               : 1000 * keys_left / kDeathDeformFadeKeys;
      const int32_t breath = static_cast<int32_t>(
          (static_cast<int64_t>(kCompressAmpPm) * life / 1000 *
           ((65536 + sinp(f, K, K / 40 > 0 ? K / 40 : 1)) / 2)) >> 16);
      // PASS 14 / R2(c): the sag fades IN on the same ramp the life fades OUT
      // on, so the corpse's flatten is already at its held value by the settle
      // key and nothing snaps. The body goes flat as it dies.
      flat = flat * life / 1000 + breath + kCorpseFlatMax * (1000 - life) / 1000;
      // THE OPENING HOLD: exactly zero at key 0, so the loop seam cannot
      // interpolate a breath back onto the corpse. See kDeathOpenKeys.
      if (f < kDeathOpenKeys) flat = flat * fold_ease(f * 1000 / kDeathOpenKeys) / 1000;
      if (flat > 60000) flat = 60000;
      const int32_t spread =
          static_cast<int32_t>((static_cast<int64_t>(flat) * kSpreadRatioPm) / 1000);
      c.deform[static_cast<size_t>(f)] =
          zc::DeformSample{static_cast<uint16_t>(flat), static_cast<uint16_t>(spread)};
    }
  }
  // ⚠ ETERNAL REST IS A HOLD, NOT A WRAP (D9 §11; pass-12 review item 1).
  // Without this the presentation interpolator takes its sub-frame partner as
  // `frame + 1 >= frame_count ? 0 : frame + 1` (creature_core.cpp, four sites)
  // and the LAST key blends toward key 0 -- the alive hover pose. The corpse
  // stood up for the final two frames of a clip that loops: an 11 px jump and
  // ~6,500 changed pixels, at the end of the clip the owner watches twice.
  //
  // The tail keys were never the knob. kDeathOpenKeys was added when this same
  // seam was found in the DEFORM stream and it closed that stream only; root
  // and quats kept wrapping. 09-ENGINE-GOTCHAS §18: when careful tuning keeps
  // failing, the knob is not the thing. Zixxtrixx has had the real one since
  // run 0326 (zixxtrixx.h:4550, "one-shot: the corpse holds; no wrap-to-stance
  // flash") -- the fix simply never travelled to this creature.
  //
  // FAILABLE LEG 4 removes it from THIS builder -- the same one the verdict is
  // taken on -- rather than from a separate copy of the clip, which is the
  // trick that lets a seam gate look proven while measuring something it can
  // never fail on.
  if (g_u02_death_fail != 4) c.hold_last = true;
  return c;
}

/** DEATH TWO — "the gutter", slot 18. A DISTINCT APPROACH, sharing the corpse
 *  contract (Zixxtrixx's two deaths are the reference for the STANDARD, not
 *  the motion — this takes the "two distinct approaches" idea and none of the
 *  poses; Manafold has no legs, no spine and no jaw to reuse anyway).
 *
 *  DEATH ONE is mechanical: the float fails and physics finishes it.
 *  DEATH TWO is a FAILURE OF THE CONDUIT: the mana goes out FIRST (fold_life_pm
 *  takes it to zero by key 112, twenty keys before the body lets go), the
 *  antenna's three nodules go limp ONE AT A TIME in order, and the creature
 *  SAGS in three stations — twice it hauls itself back up, and the third time
 *  it does not. Only then does it drop, heavily, with two small bounces.
 *
 *  Same rest root, same declared penetration, same bit-zero deform, same
 *  eternal rest. Different performance.
 *
 *  ⚠ Same declared exception to the never-off life layer, same declared ground
 *  contact, same probe. */
inline zc::Clip build_death_gutter() {
  const DeathBeats B = deathb_beats();
  const int K = B.keys;
  zc::Clip c = clip_shell(kDeathBSlot, K, kHoverHeightMm);
  Rig g;
  // the three sags: down, HAUL BACK UP, down further, haul up less, down and
  // this time it keeps going. Authored as a curve so the two failed recoveries
  // are visible in the table rather than emergent.
  // per-mille of kDeathBSagMaxMm, so the three stations really are three
  // different depths and the recoveries really are partial
  static const Key kSag[] = {
      {0, 0},
      {kDeathBSagKeys[0], kDeathBSagMm[0] * 1000 / kDeathBSagMaxMm},   // sag 1
      {kDeathBSagKeys[0] + 16, kDeathBSagMm[0] * 200 / kDeathBSagMaxMm},  // up
      {kDeathBSagKeys[1], kDeathBSagMm[1] * 1000 / kDeathBSagMaxMm},   // sag 2
      {kDeathBSagKeys[1] + 18, kDeathBSagMm[1] * 520 / kDeathBSagMaxMm},  // up, less
      {kDeathBSagKeys[2], kDeathBSagMm[2] * 1000 / kDeathBSagMaxMm},   // sag 3
      {kDeathBLetGoKey, 1000}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    const bool dead = f >= B.settle;
    const bool falling = f > kDeathBLetGoKey;
    const int32_t gone = !falling ? 0
                       : dead     ? 1000
                                  : 1000 * (f - kDeathBLetGoKey) /
                                        (B.settle - kDeathBLetGoKey);
    // ⚠ THE EYES STOP LOOKING AROUND AS IT DIES, and they stop CONTINUOUSLY.
    // antenna_knead is not called once dead, so the travel carrier snaps back
    // to identity in one key -- up to 45 deg on both eyes, at the exact instant
    // the corpse is supposed to go still. The nodules were protected from this
    // (their droop eases in on fold_ease(gone)); the travel arrived with no
    // equivalent, which is the half of the wave-2a edit that never got written.
    // The gain reaches 0 exactly at the settle key, so the dead branch's
    // identity carrier is where the fade was already going.
    if (!dead) antenna_knead(g, kDeathBSlot, K, f,
                              g_u02_death_fail == 5 ? 1000 : 1000 - fold_ease(gone));
    // THE NODULES DIE IN ORDER. Each one, at its own key, stops whatever it
    // was doing and hangs — and because a nodule CARRIES ITS SECTION (§2),
    // one going limp visibly drops a third of the antenna while the other two
    // are still working. That is a read the pass-11 rig could not produce.
    {
      NoduleOffsets n;
      int32_t limp[3];
      for (int i = 0; i < 3; ++i) {
        const int lk = kDeathBLimpKey[i];
        limp[i] = f <= lk ? 0
                          : (f - lk >= 20 ? 1000 : fold_ease((f - lk) * 1000 / 20));
        if (falling) limp[i] = 1000;
      }
      const auto s = [](int32_t mm, int32_t t) {
        return static_cast<int32_t>((static_cast<int64_t>(mm) * t) / 1000);
      };
      // before a nodule goes limp it is still working -- one slow press each,
      // the labour of a conduit that is losing its grip on the mana
      const auto work = [&](int i, int32_t t) {
        return t >= 1000 ? 0
                         : static_cast<int32_t>(
                               (static_cast<int64_t>(kNoduleAmpMm[i][1]) *
                                press_wave(f, K, 3, 120 * i) * (1000 - t)) >>
                               16) / 1000;
      };
      n.ax = s(kDeathDroopMm[0][0], limp[0]);
      n.ay = s(kDeathDroopMm[0][1], limp[0]) + work(0, limp[0]);
      n.az = s(kDeathDroopMm[0][2], limp[0]);
      n.bx = s(kDeathDroopMm[1][0], limp[1]);
      n.by = s(kDeathDroopMm[1][1], limp[1]) + work(1, limp[1]);
      n.bz = s(kDeathDroopMm[1][2], limp[1]);
      n.cx = s(kDeathDroopMm[2][0], limp[2]);
      n.cy = s(kDeathDroopMm[2][1], limp[2]) + work(2, limp[2]);
      n.cz = s(kDeathDroopMm[2][2], limp[2]);
      g.nod = n;
    }
    const int32_t sag = curve(kSag, 7, f);
    // R5: the two branches now MEET. The sag branch reaches
    // kDeathSagSlackSharePm of the way to full slack by the let-go; the falling
    // branch starts from there instead of from zero. It used to restart at
    // zero, so the body snapped stiff on the same key the root popped -- two
    // discontinuities dressed as one beat.
    const int32_t slack_t =
        falling ? kDeathSagSlackSharePm + (1000 - kDeathSagSlackSharePm) *
                                              fold_ease(gone) / 1000
                : fold_ease(sag) * kDeathSagSlackSharePm / 1000;
    const int32_t slack = 1000 + (kDeathSlackPm - 1000) * slack_t / 1000;
    loop_pose(g, slack, slack, slack, slack, 0);
    g.q[kBRoot] = quat_mul(
        g.q[kBRoot],
        quat_mul(quat_z(static_cast<int32_t>(
                     (static_cast<int64_t>(-kDeathRestPitchA16) * fold_ease(gone)) / 1000)),
                 quat_x(static_cast<int32_t>(
                     (static_cast<int64_t>(-kDeathRestRollA16) * fold_ease(gone)) / 1000))));
    face_rest(g);
    // the eyes go out with the mana, not with the body: they are already
    // half-lidded through the sags, shut by the time it lands
    const int32_t lid = falling ? 400 + 600 * fold_ease(gone) / 1000
                                : 700 * fold_ease(sag) / 1000;
    apply_squint(g, dead ? 1000 : lid + (falling ? 0 : blink_at(f, 29)));
    apply_gaze(g, 0, -kGazeLiftMaxA16 * (dead ? 1000 : lid) / 1000);
    g.write(c, f);
    int32_t y;
    if (!falling) {
      // the sag: it is still nominally floating, just failing at it
      y = fxu(kHoverHeightMm) -
          static_cast<int32_t>((static_cast<int64_t>(fxu(kDeathBSagMaxMm)) * sag) / 1000) +
          static_cast<int32_t>((static_cast<int64_t>(fxu(kBobAmpBMm)) *
                                sinp(f, K, 2) * (1000 - sag)) >> 16) / 1000;
    } else {
      // ⚠ R5 -- THE SAG IS CARRIED INTO THE FALL. `death_root_at` starts from
      // kHoverHeightMm, which is where the OTHER death is when its float dies.
      // This one has sagged kDeathBSagMaxMm below that and is still holding the
      // sag on the previous key, so taking the shared trajectory raw teleported
      // the root 240 mm UP in one key (QA 6.3; the largest interior root step
      // in the bank, and a ~16 px pop at the dramatic beat).
      //
      // FAILABLE LEG 6 collapses the carry to a single key, which reproduces
      // the old constant's behaviour exactly -- the witnessed-fail leg for
      // `mqa`'s Q3 bound, taken from THIS builder rather than from a copy.
      const int carry_keys =
          g_u02_death_fail == 6 ? 1
                                : (kDeathBSagCarryKeys > 0 ? kDeathBSagCarryKeys : 1);
      const int lf = f - kDeathBLetGoKey;
      const int32_t carry =
          lf >= carry_keys ? 0 : 1000 - fold_ease(lf * 1000 / carry_keys);
      y = fxu(death_root_at(B, kDeathBApexMm, kDeathBIntervalKeys, kDeathBBounces,
                            kDeathBLetGoKey, kDeathBDropKeys, kDeathBImpactDipMm, f)) -
          static_cast<int32_t>((static_cast<int64_t>(fxu(kDeathBSagMaxMm)) * carry) / 1000);
    }
    c.root[static_cast<size_t>(f) * 3 + 1] = y;
    if (dead && g_u02_death_fail != 1) {  // ⚠ it STOPS (leg 1 removes that)
      // ...at a HELD sag, not at bit-zero: see kDeathCorpseFlatPm. Both deaths
      // settle into the same flattened corpse; what stays distinct between
      // them is how they get there, which is the protected part.
      c.deform[static_cast<size_t>(f)] = corpse_sample();
    } else {
      int32_t flat = 0;
      for (int i = 0; i < kDeathBBounces; ++i) {
        const int lf = f - B.impact[i];
        const int win = 2 * B.dipk[i];
        if (lf >= 0 && lf < win) {
          const int32_t sq = kDeathImpactSquashPm * kDeathBApexMm[i] / kDeathBApexMm[0];
          const int32_t v = kCompressAmpPm * sq / 1000 * (win - lf) / win;
          if (v > flat) flat = v;
        }
      }
      const int keys_left = B.settle - f;
      const int32_t life = keys_left >= kDeathDeformFadeKeys
                               ? 1000
                               : 1000 * keys_left / kDeathDeformFadeKeys;
      // the breath gets SHALLOWER and SLOWER through the sags before it stops,
      // which is the beat the fold's own guttering is set against
      const int32_t breath = static_cast<int32_t>(
          (static_cast<int64_t>(kCompressAmpPm) * (1000 - sag * 7 / 10) / 1000 *
           life / 1000 * ((65536 + sinp(f, K, K / 52 > 0 ? K / 52 : 1)) / 2)) >> 16);
      // PASS 14 / R2(c): as above -- the sag arrives as the life leaves.
      flat = flat * life / 1000 + breath + kCorpseFlatMax * (1000 - life) / 1000;
      if (f < kDeathOpenKeys) flat = flat * fold_ease(f * 1000 / kDeathOpenKeys) / 1000;
      if (flat > 60000) flat = 60000;
      const int32_t spread =
          static_cast<int32_t>((static_cast<int64_t>(flat) * kSpreadRatioPm) / 1000);
      c.deform[static_cast<size_t>(f)] =
          zc::DeformSample{static_cast<uint16_t>(flat), static_cast<uint16_t>(spread)};
    }
  }
  // ⚠ ETERNAL REST IS A HOLD, NOT A WRAP (D9 §11; pass-12 review item 1).
  // Without this the presentation interpolator takes its sub-frame partner as
  // `frame + 1 >= frame_count ? 0 : frame + 1` (creature_core.cpp, four sites)
  // and the LAST key blends toward key 0 -- the alive hover pose. The corpse
  // stood up for the final two frames of a clip that loops: an 11 px jump and
  // ~6,500 changed pixels, at the end of the clip the owner watches twice.
  //
  // The tail keys were never the knob. kDeathOpenKeys was added when this same
  // seam was found in the DEFORM stream and it closed that stream only; root
  // and quats kept wrapping. 09-ENGINE-GOTCHAS §18: when careful tuning keeps
  // failing, the knob is not the thing. Zixxtrixx has had the real one since
  // run 0326 (zixxtrixx.h:4550, "one-shot: the corpse holds; no wrap-to-stance
  // flash") -- the fix simply never travelled to this creature.
  //
  // FAILABLE LEG 4 removes it from THIS builder -- the same one the verdict is
  // taken on -- rather than from a separate copy of the clip, which is the
  // trick that lets a seam gate look proven while measuring something it can
  // never fail on.
  if (g_u02_death_fail != 4) c.hold_last = true;
  return c;
}

/** THE MANA LASSO, slot 19 (Direction 9 §15) — the creature's whole toolkit
 *  in one gesture.
 *
 *  ⚠ THE ANTENNAE THROW IT. The mana does not fly off on its own: the ring
 *  leaves at kLassoReleaseKey because the nodules WHIP forward at
 *  kLassoReleaseKey, and it is hauled home at kLassoReelKey because the
 *  nodules haul back at kLassoReelKey. The rig is the cause and the mana is
 *  the effect, which is §2's "with that ability, the creature folds the mana"
 *  applied to a throw.
 *
 *  Five beats, each long enough to read (07-MOTION-STYLE §4):
 *    10.. 44  THE WIND-UP. All three nodules haul back and down together
 *             (kLassoWindMm), the body leans away, the gaze locks on target.
 *             One press, held — the wind-up is 34 keys and never reverses.
 *     44      THE RELEASE. The nodules snap through to kLassoWhipMm and the
 *             ring leaves the pocket on the same key.
 *    44..104  THE FLIGHT. lasso_at() carries the RING STENCIL out along a
 *             lofted arc, opening as it goes. The body follows it round.
 *   104..118  THE CATCH. It snags; the jerk comes back down the antenna and
 *             yanks all three nodules toward the target.
 *   118..146  THE RETURN, hand over hand: the nodules haul back in sequence
 *             (C, then B, then A — a rope comes in from the far end) while
 *             the ring cinches and shrinks home.
 *   146..168  composed again.
 *
 *  The mana is the same substance as every other fold in the bank: same
 *  stencil machinery, same edge, same motes, same knead. */
inline zc::Clip build_lasso() {
  const int K = kLassoKeys;
  zc::Clip c = clip_shell(kLassoSlot, K, kHoverHeightMm);
  Rig g;
  // the throw envelope: -1000 fully wound up, +1000 fully whipped through
  static const Key kThrow[] = {{0, 0},
                               {kLassoWindStartKey, 0},
                               {kLassoReleaseKey - 4, -1000},
                               {kLassoReleaseKey + 6, 1000},
                               {kLassoCatchKey, 620},
                               {kLassoCatchKey + 8, -1000},   // the jerk
                               {kLassoReelKey + 14, -300},
                               {kLassoHomeKey, 0},
                               {K - 1, 0}};
  static const Key kLean[] = {{0, 0}, {kLassoWindStartKey, 0},
                              {kLassoReleaseKey - 4, -1000}, {kLassoReleaseKey + 8, 900},
                              {kLassoCatchKey + 8, -700}, {kLassoHomeKey, 0}, {K - 1, 0}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, kLassoSlot, K, f);
    const int32_t th = curve(kThrow, 9, f);
    {
      NoduleOffsets n;
      // ONE table for the wind-up and one for the whip; the envelope picks
      // which side of rest each nodule is on. Sign, not two schedules.
      // THE RETURN IS HAND OVER HAND: after the reel key each nodule leads by
      // its own lag, C first (the far end of the rope comes in first).
      const auto lag = [&](int i) {
        if (f < kLassoReelKey) return f;
        const int l = (2 - i) * 6;  // C 0, B 6, A 12 keys behind
        const int lf = f - l;
        return lf < kLassoReelKey ? kLassoReelKey : lf;
      };
      const auto mixl = [&](int i, int ax) {
        const int32_t t = curve(kThrow, 9, lag(i));
        return t < 0 ? -kLassoWindMm[i][ax] * t / 1000 : kLassoWhipMm[i][ax] * t / 1000;
      };
      n.ax = mixl(0, 0); n.ay = mixl(0, 1); n.az = mixl(0, 2);
      n.bx = mixl(1, 0); n.by = mixl(1, 1); n.bz = mixl(1, 2);
      n.cx = mixl(2, 0); n.cy = mixl(2, 1); n.cz = mixl(2, 2);
      // the CATCH's jerk: the rope goes taut and pulls every nodule at once
      if (f >= kLassoCatchKey && f < kLassoCatchKey + 10) {
        const int32_t j = kLassoJerkMm * (10 - (f - kLassoCatchKey)) / 10;
        n.ax += j; n.bx += j * 3 / 2; n.cx += j * 5 / 4;
      }
      g.nod = n;
    }
    // the spans open on the throw and close on the haul -- the antenna
    // REACHES for the throw, which is §13's stretchy spans doing their job
    const int32_t reach = th > 0 ? 1000 + th / 12 : 1000 + th / 20;
    loop_pose(g, reach, reach, reach, reach, 0);
    g.q[kBRoot] = quat_mul(
        g.q[kBRoot],
        quat_z(static_cast<int32_t>(
            (static_cast<int64_t>(kLassoLeanA16) * curve(kLean, 7, f)) / 1000)));
    face_rest(g);
    // the gaze is ON THE TARGET the whole way -- it aims, it throws, it
    // watches the ring fly, it tracks it home. One continuous look, no darting.
    {
      const bool flying = f >= kLassoReleaseKey && f < kLassoHomeKey;
      const LassoState L = lasso_at(kLassoSlot, K, f * 16);
      const int32_t out = flying ? L.off_mm[0] : 0;
      apply_gaze(g,
                 static_cast<int32_t>((static_cast<int64_t>(kGazeMaxA16) * out) /
                                      (kLassoThrowMm[0] > 0 ? kLassoThrowMm[0] : 1)),
                 kGazeLiftMaxA16 / 3);
    }
    apply_squint(g, blink_at(f, 63) + (th < -600 ? 320 : 0));  // the aiming squint
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(f, K, kHoverHeightMm, kBobAmpAMm, kBobAmpBMm, K / 24, K / 56) -
        static_cast<int32_t>((static_cast<int64_t>(fxu(120)) * (th < 0 ? -th : 0)) / 1000);
    c.deform[static_cast<size_t>(f)] = compress_at(f, K, K / 24, kCompressAmpPm);
  }
  return c;
}

/** BLOWN HIGH UP IN THE AIR, slot 20 (Direction 5 §7, asked twice; D9 §11).
 *
 *  ⚠ DISTINCT FROM `fall` (slot 9). That clip begins already 3.6 m up and
 *  drops; the owner asked to be "blown high up in the air", and a launch you
 *  never see is not a launch. This one starts AT THE HOVER, on screen, and the
 *  blast happens in frame.
 *
 *    0.. 22  ANTICIPATION. It gathers: compresses and SINKS. The tell.
 *   22.. 30  THE BLAST. Eight keys, all of the speed in the clip.
 *   30.. 96  THE RISE, decelerating into the apex, tumbling two thirds of a
 *            turn with a slow yaw under it — not a full spin, which reads as
 *            a wheel rather than as something thrown.
 *   96..164  THE DROP, accelerating, the antenna STREAMING behind it: the
 *            nodules trail against the direction of travel, in proportion to
 *            speed. This is the new mechanism's show moment — three balls
 *            lagging the body by different amounts because they are three
 *            independent things, not one hose.
 *  164..196  THE CATCH: the float takes hold with a deep squash and one
 *            overshoot, and it composes itself. */
inline zc::Clip build_blown() {
  const int K = kBlownKeys;
  zc::Clip c = clip_shell(kBlownSlot, K, kHoverHeightMm);
  Rig g;
  static const Key kGather[] = {{0, 0}, {6, 0}, {kBlownAnticipKey, 1000},
                                {kBlownLaunchKey, 0}, {K - 1, 0}};
  static const Key kSquash[] = {{0, 1000}, {14, 1000}, {kBlownAnticipKey, 2400},
                                {kBlownLaunchKey, 700}, {kBlownApexKey, 900},
                                {kBlownCatchKey, 800},
                                {kBlownCatchKey + 8, kBlownCatchSquashPm},
                                {kBlownCatchKey + 22, 1400}, {K - 1, 1000}};
  static const Key kWide[] = {{0, 0}, {kBlownAnticipKey, 260}, {kBlownLaunchKey + 4, -900},
                              {kBlownApexKey, -700}, {kBlownCatchKey, -400},
                              {kBlownCatchKey + 10, 700}, {K - 1, 0}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, kBlownSlot, K, f);
    // the height, and its own derivative -- the stream is driven by SPEED, so
    // it is computed from the trajectory rather than guessed at with a curve
    const auto height_mm = [&](int q) -> int32_t {
      if (q <= kBlownAnticipKey) {
        const int32_t t = q * 1000 / (kBlownAnticipKey > 0 ? kBlownAnticipKey : 1);
        return kHoverHeightMm - kBlownSinkMm * fold_ease(t) / 1000;
      }
      if (q < kBlownCatchKey) {
        // PASS 13 / R4 -- THREE BEATS, NOT ONE PARABOLA.
        //
        // The old shape was a single arc from the gather to the catch: 74 keys
        // up, 68 down, and because a parabola is flattest at its top it spent
        // about 2.8 s of a 6.5 s clip at effectively constant height. That is a
        // float, and this clip is called `blown`.
        //
        // Now: a BLAST (fast off the ground, decelerating -- 1-(1-t)^2), a
        // short declared HANG at the top, and an accelerating FALL (t^2). The
        // hang is what makes the blast and the fall read as separate events;
        // without it the apex is just the place two curves meet.
        const int hang_in = kBlownApexKey;
        const int hang_out = kBlownApexKey + kBlownHangKeys;
        const int span_up = hang_in - kBlownAnticipKey;
        const int span_dn = kBlownCatchKey - hang_out;
        int32_t h;
        if (q <= hang_in) {
          const int32_t t = (q - kBlownAnticipKey) * 1000 / (span_up > 0 ? span_up : 1);
          // 1-(1-t)^2 : fast off the ground, decelerating into the apex
          const int32_t u = 1000 - t;
          h = static_cast<int32_t>(
              (static_cast<int64_t>(kBlownHeightMm) * (1000000 - u * u)) / 1000000);
        } else if (q <= hang_out) {
          h = kBlownHeightMm;  // THE HANG. Brief, and the only still moment.
        } else {
          const int32_t t = (q - hang_out) * 1000 / (span_dn > 0 ? span_dn : 1);
          h = static_cast<int32_t>(
              (static_cast<int64_t>(kBlownHeightMm) * (1000000 - t * t)) / 1000000);
        }
        return kHoverHeightMm - kBlownSinkMm + h;
      }
      static const Key kCatch[] = {{kBlownCatchKey, 0}, {kBlownCatchKey + 9, -190},
                                   {kBlownCatchKey + 20, 70}, {K - 1, 0}};
      return kHoverHeightMm + curve(kCatch, 4, q) * 100 / 1000;
    };
    const int32_t h_now = height_mm(f);
    const int32_t vel = h_now - height_mm(f > 0 ? f - 1 : 0);  // mm per key
    // THE ANTENNA STREAMS. Each nodule trails by its own fraction — B (the
    // peak, the loosest) most, A (held by the neck) least — so the streaming
    // reads as three balls on three leashes rather than one rigid fin.
    {
      NoduleOffsets n;
      const int32_t s = -vel * kBlownStreamMm / 200;  // 200 mm/key ~ full stream
      const auto cl = [](int32_t v, int32_t lim) {
        return v > lim ? lim : v < -lim ? -lim : v;
      };
      n.ay = cl(s * 6 / 10, kBlownStreamMm);
      n.by = cl(s, kBlownStreamMm * 3 / 2);
      n.cy = cl(s * 8 / 10, kBlownStreamMm);
      // ...and they trail BACKWARD too, against the tumble
      n.ax = cl(-s * 3 / 10, kBlownStreamMm);
      n.bx = cl(-s * 5 / 10, kBlownStreamMm);
      n.cx = cl(-s * 4 / 10, kBlownStreamMm);
      g.nod = n;
    }
    const int32_t gather = curve(kGather, 5, f);
    const int32_t stream = 1000 + (vel > 0 ? vel / 3 : -vel / 5) - gather / 5;
    loop_pose(g, stream, stream, stream, stream, 0);
    if (f > kBlownAnticipKey && f < kBlownCatchKey) {
      // ===== PASS 14 / R7 -- THE TUMBLE IS DECOUPLED FROM THE HEIGHT ========
      //
      // The apex is not DARK. The review measured `blown` at 3.5% brightness
      // range end to end and the plan's warm-lamp theory died there. The apex
      // is EMPTY: nothing happens on it. And the reason nothing happens is in
      // the table this replaces -- the tumble's plateau was authored to follow
      // the height's, deliberately, so that "the one still moment in the clip
      // is still in rotation as well as in height". Height held, rotation held,
      // and the result is fifty-eight frames within 6% of peak height with a
      // parked pose on them. The intent was a beat; what it produced is the one
      // stretch of the clip where the creature is doing nothing at all.
      //
      // So the rotation runs MONOTONE across the whole flight now, and the
      // table IS the shape -- `fold_ease` is gone from it, because a smoothstep
      // exists to ease into and out of a plateau and there is no plateau left.
      // It spins up out of the blast, holds a steady rate through the apex, and
      // decelerates into the catch as the float takes hold.
      //
      // ⚠ THE CATCH POSE IS BIT-FOR-BIT WHAT IT WAS, AND THAT IS STRUCTURAL,
      // NOT A COINCIDENCE. Pass 12 fixed a real fault here -- a one-way
      // 148-degree tumble left the creature nose-down at the catch and drove
      // the antenna 27 mm into the dirt -- and pass 13 fixed it by returning
      // the curve to zero. Returning a curve to zero is a beat spent on
      // bookkeeping. This lands the same pose by going ALL THE WAY ROUND:
      // kBlownTumbleA16 is a full revolution, and quat_axis takes the half
      // angle as `(a >> 1) & 0xFFFF`, so e = 1000 is 32768 of half-angle --
      // exactly -identity, the same rotation as identity. The creature is
      // right-side up at the catch because it completed a turn, not because it
      // rewound one.
      //
      // 390 at the apex is the by-eye finding kBlownTumbleA16's own note
      // records, preserved: 148 degrees still reads as something knocked
      // flying and keeps the antenna off the far side of the body, while 241
      // hides it. What changes is that the creature PASSES THROUGH that angle
      // instead of parking on it. (The old comment here cited "key 163" -- a
      // number from the 196-key version of this clip, left standing three keys
      // above kBlownKeys = 146. Gate checklist 8, and it is corrected rather
      // than carried.)
      static const Key kTumble[] = {{0, 0}, {kBlownAnticipKey, 0},
                                    {kBlownLaunchKey, 40},
                                    {kBlownApexKey, 390},
                                    {kBlownCatchKey - 10, 940},
                                    {kBlownCatchKey, 1000},
                                    {kBlownKeys - 1, 1000}};
      // ...and the YAW keeps the old rise-and-return, on its own curve. It is
      // not part of the revolution: a yaw that ended at 6000 would rotate the
      // creature at the catch and move the very contact the probe gates.
      static const Key kYawArc[] = {{0, 0}, {kBlownAnticipKey, 0},
                                    {kBlownApexKey, 1000},
                                    {kBlownApexKey + kBlownHangKeys, 1000},
                                    {kBlownCatchKey, 0},
                                    {kBlownKeys - 1, 0}};
      const int32_t e = curve(kTumble, 7, f);
      const int32_t ey = fold_ease(curve(kYawArc, 6, f));
      g.q[kBRoot] = quat_mul(
          g.q[kBRoot],
          quat_mul(quat_z(static_cast<int32_t>(
                       (static_cast<int64_t>(kBlownTumbleA16) * e) / 1000)),
                   quat_y(static_cast<int32_t>(
                       (static_cast<int64_t>(kBlownYawA16) * ey) / 1000))));
    }
    face_rest(g);
    apply_squint(g, curve(kWide, 7, f) + blink_at(f, 71));
    apply_gaze(g, 0, vel > 0 ? kGazeLiftMaxA16 / 2 : -kGazeLiftMaxA16 / 2);
    g.write(c, f);
    c.root[static_cast<size_t>(f) * 3 + 1] = fxu(h_now);
    c.deform[static_cast<size_t>(f)] = squash_impact(f, K, kSquash, 9);
  }
  return c;
}

/** THE NODULE TAUNT, slot 21 (Direction 9 §11, "more fun", the third ask).
 *
 *  THE FIRST SHIPPED CLIP WHOSE PERFORMANCE *IS* THE PER-NODULE VOCABULARY.
 *  Slot 16 proves the nodules move; this one uses them to act. The standing
 *  verdict on the old taunts is "an oscillator rather than an animal working
 *  at something", so every beat here is a thing the creature DOES and then
 *  HOLDS — press, arrive, hold — and the ambient nodule schedule is switched
 *  off (kNoduleClipPm[21] = 0) so nothing wobbles under the gesture.
 *
 *  PASS 13 / R3 RE-TIMED IT. Pass 12's beats were right and its EASING was
 *  wrong: every one of them was a 26-to-32-key `fold_ease` ramp, so the clip
 *  read as one slow sway and the standing verdict was "not funny". The beats
 *  below are the same four gestures; what is new is an anticipation, two real
 *  attacks, two real holds, and a loop that closes.
 *
 *    4.. 14  THE ANTICIPATION. Outers dip, middle lifts, the body sinks and
 *            squashes — the shrug's exact opposite, so the shrug is a RELEASE.
 *   14.. 24  THE SHRUG, in ten keys, through `punch_ease`. The owner's own
 *            configuration — "the middle one might go down while the other two
 *            swing up" — used as a GESTURE, and now it arrives.
 *   24.. 52  HELD, 28 keys. The joke is the hold, exactly as slot 11 learned
 *            at pass 3; a hold nobody can tell you arrived is not one.
 *   60..100  THE LEAN. Slow on purpose — this beat is the one that should
 *            drift — tipping toward the viewer while the shrug decays.
 *  100..146  THE SHIMMY. Three balls, in turn, one press each: A, then B,
 *            then C. A wave with three reversals in 46 keys, not a vibration.
 *  146..149  THE DISMISSAL, in THREE keys. The whole antenna is thrown away.
 *  149..173  HELD. The punchline frame is in here, and you can point at it.
 *  173..183  Released to the rest pose, because the last key must equal the
 *            first or the loop shows a seam (QA 6.3b: it was 110.7 mm). */
inline zc::Clip build_taunt3() {
  const int K = kTaunt3Keys;
  zc::Clip c = clip_shell(kTaunt3Slot, K, kHoverHeightMm);
  Rig g;
  // PASS 13 / R3. The tables ARE the performance; read them beside the beat
  // list in manafold_art.h. Two things to know before changing one:
  //
  //  * WHICH EASE a curve is read through is the beat. `punch_ease` snaps,
  //    `fold_ease` drifts. The shrug and the dismissal snap; the anticipation
  //    and the lean drift. That single choice is what pass 12 got wrong on all
  //    four beats at once.
  //  * kFlick RETURNS TO ZERO at K-1. It used to end at 820, and since the
  //    presentation blends the last key toward key 0 that left a 110.7 mm loop
  //    seam (QA 6.3b) -- visible as a grey smear on the last frame. A held pose
  //    that is still held at the last key cannot loop; it is held for
  //    kTaunt3FlickHoldKeys and then released.
  static const Key kAntic[] = {{0, 0}, {4, 0}, {kTaunt3AnticKey, 1000},
                               {kTaunt3ShrugKey, 0}, {K - 1, 0}};
  // PASS 14 / R4: the last leg used to run {94, 220} -> {183, 0}, i.e. the
  // shrug was still decaying THROUGH the punchline hold. A tail is motion. It
  // reaches zero at kTaunt3Hold2Key - 12 now and is flat from there, so the
  // held pose is actually held.
  static const Key kShrug[] = {{0, 0}, {kTaunt3ShrugKey, 0},
                               {kTaunt3ShrugKey + kTaunt3ShrugAttackKeys, 1000},
                               {kTaunt3ShrugHoldKey, 1000},
                               {kTaunt3LeanKey + 14, 420},
                               {kTaunt3ShimmyKey - 6, 220},
                               {kTaunt3Hold2Key - 12, 0}, {K - 1, 0}};
  // PASS 14 / R4: same fault, same fix -- {118, 150} -> {183, 0} was a lean
  // still unwinding under the punchline.
  static const Key kLean[] = {{0, 0}, {kTaunt3LeanKey, 0},
                              {kTaunt3LeanKey + kTaunt3LeanAttackKeys, 1000},
                              {kTaunt3ShimmyKey, 1000}, {kTaunt3ShimmyKey + 18, 150},
                              {kTaunt3Hold2Key - 10, 0}, {K - 1, 0}};
  static const Key kFlick[] = {{0, 0}, {kTaunt3FlickKey, 0},
                               {kTaunt3FlickKey + kTaunt3FlickAttackKeys, 1000},
                               {kTaunt3FlickKey + kTaunt3FlickAttackKeys +
                                    kTaunt3FlickHoldKeys, 1000},
                               {K - 1, 0}};
  // PASS 14 / R4 -- THE AMBIENT CLOCK, WHICH STOPS. See the long note beside
  // kTaunt3Hold1Key in manafold_art.h. This is a TIME WARP, not a fade: it is
  // monotone, it plateaus across both holds, it runs ~1.45x through the lean
  // and the shimmy to pay the parked keys back, and it arrives at K-1 so the
  // loop seam is exactly what it was. Everything ambient reads its time from
  // here and nothing else does -- the beats keep the real key, because a beat
  // that slowed down along with the float would be the "slower, not stiller"
  // failure wearing the fix's clothes.
  static const Key kClock[] = {{0, 0},
                               {kTaunt3Hold1Key, kTaunt3Hold1Key},
                               {kTaunt3Hold1EndKey, kTaunt3Hold1Key},
                               {kTaunt3Hold2Key, kTaunt3ClockAtHold2},
                               {kTaunt3Hold2EndKey, kTaunt3ClockAtHold2},
                               {K - 1, K - 1}};
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, kTaunt3Slot, K, f);  // gain 0: nothing runs under the gesture
    const int fc = curve(kClock, 6, f);   // ambient time, which holds still
    const int32_t antic = fold_ease(curve(kAntic, 5, f));
    const int32_t shrug = punch_ease(curve(kShrug, 8, f));
    const int32_t lean = fold_ease(curve(kLean, 7, f));
    const int32_t flick = punch_ease(curve(kFlick, 5, f));
    // THE BODY FOLLOWS THE ANTENNA, it does not move with it. Same table, read
    // kTaunt3FlickBodyLagKeys later and through the SOFT ease -- so the crown
    // is thrown, and then the shoulder and the body come round after it.
    // Overlapping action; it costs one line and it is most of why a snap reads
    // as a gesture rather than as a jump cut.
    const int32_t flick_body =
        fold_ease(curve(kFlick, 5, f - kTaunt3FlickBodyLagKeys));
    {
      NoduleOffsets n;
      // THE SHRUG: outers up, middle DOWN. Not 1:1 — a middle that drops
      // carries the rear down with it, so it takes the smaller share, the
      // same asymmetry slot 16's segment 3 measured and kNoduleSoloMidPm
      // records.
      //
      // PASS 12 / WAVE 3: NODULE A RISES. This block used to read "nodule A's
      // rise is sideways because a vertical request moves it 3 mm" — true when
      // it was written, FALSE since wave 2a made the spans stretch, and left
      // standing it is the false-structural-comment fault (gate checklist 8).
      // A's vertical is now the main term and the sideways swing is a named
      // third (kTaunt3ShrugLeanPm), so the owner's sentence — "the middle one
      // might go down while the other two swing UP" — is what the clip does.
      // `mspan`'s G5 gates it on THIS clip, not on the diagnostic.
      n.ay = kTaunt3ShrugMm * shrug / 1000;
      n.az = static_cast<int32_t>(
          (static_cast<int64_t>(kTaunt3ShrugMm) * shrug * kTaunt3ShrugLeanPm) / 1000000);
      n.by = -kTaunt3ShrugMm * 6 / 10 * shrug / 1000;
      n.cy = kTaunt3ShrugMm * shrug / 1000;
      // PASS 13 / R3 -- THE ANTICIPATION. The outers dip and the middle lifts,
      // i.e. the exact opposite of the shrug, for ten keys before it. This is
      // the one thing the clip had none of: a gesture with no wind-up cannot
      // have a payoff, because there is nothing for the payoff to be a release
      // FROM. It is small on purpose (kTaunt3AnticMm is a third of the shrug) --
      // an anticipation the size of its own beat is just a second beat.
      n.ay -= kTaunt3AnticMm * antic / 1000;
      n.by += kTaunt3AnticMm * 6 / 10 * antic / 1000;
      n.cy -= kTaunt3AnticMm * antic / 1000;
      // THE SHIMMY: one press per ball, in sequence. Each press is 14 keys
      // wide and rises and falls exactly once, so three balls moving in turn
      // costs three reversals, not thirty.
      if (f >= kTaunt3ShimmyKey && f < kTaunt3FlickKey) {
        for (int i = 0; i < 3; ++i) {
          const int a = kTaunt3ShimmyKey + i * 14;
          const int lf = f - a;
          if (lf < 0 || lf >= 28) continue;
          // 4t(1-t): up and down once, arriving and leaving at exactly zero
          const int32_t t = lf * 1000 / 28;
          const int32_t v = static_cast<int32_t>(
              (4LL * kTaunt3ShimmyMm * t * (1000 - t)) / 1000000);
          // WAVE 3: A's press is VERTICAL now, like B's and C's. It was
          // lateral only because the vertical did not move (see the shrug).
          if (i == 0) {
            n.ay += v;
            n.az += static_cast<int32_t>(
                (static_cast<int64_t>(v) * kTaunt3ShimmyLeanPm) / 1000);
          }
          else if (i == 1) { n.by += v; n.bz += v / 3; }
          else { n.cy += v; n.cz -= v / 3; }
        }
      }
      // THE DISMISSAL: the whole antenna is flung UP AND BACK OVER THE
      // SHOULDER, and all three go together — a gesture, not a ripple.
      //
      // PASS 14 / R4. This block used to be three pure -z pushes, and that is
      // what made the punchline a balloon: pushing every nodule the same way
      // along ONE horizontal axis while the base stays put tips the LOOP'S
      // PLANE, and a tipped loop presents as a line. The share that is UP
      // cannot do that at any body yaw, which is why it is the main term now.
      // The lateral remainder keeps the gesture pointed somewhere.
      const int32_t fl_up = static_cast<int32_t>(
          (static_cast<int64_t>(kTaunt3FlickMm) * kTaunt3FlickLiftPm * flick) / 1000000);
      const int32_t fl_back = static_cast<int32_t>(
          (static_cast<int64_t>(kTaunt3FlickMm) * kTaunt3FlickBackPm * flick) / 1000000);
      const int32_t fl_side = static_cast<int32_t>(
          (static_cast<int64_t>(kTaunt3FlickMm) * kTaunt3FlickSidePm * flick) / 1000000);
      n.ay += fl_up;
      n.by += fl_up;
      n.cy += fl_up * 8 / 10;
      n.ax -= fl_back;
      n.bx -= fl_back;
      n.cx -= fl_back * 8 / 10;
      n.az -= fl_side;
      n.bz -= fl_side;
      n.cz -= fl_side * 8 / 10;
      g.nod = n;
    }
    loop_pose(g, 1000 + shrug / 14, 1000 + shrug / 10, 1000 - shrug / 12,
              1000 + flick / 10, 0);
    // the lean-in, and then the shoulder turned on the dismissal -- the turn
    // rides flick_body, so the crown snaps away first and the body follows.
    // PASS 14 / R4: the dismissal's whole-body component is a yaw AND a roll
    // now. 8b's requirement is that the beat turns the body; it does not
    // require that the turn END front-on, which is where 41.7 degrees of yaw
    // on its own parked it for fifty frames.
    g.q[kBRoot] = quat_mul(
        g.q[kBRoot],
        quat_mul(quat_z(static_cast<int32_t>(
                     (static_cast<int64_t>(kTaunt3LeanA16) * lean) / 1000 -
                     (static_cast<int64_t>(kTaunt3ShrugRollA16) * shrug) / 1000 -
                     (static_cast<int64_t>(kTaunt3FlickRollA16) * flick_body) / 1000)),
                 quat_y(static_cast<int32_t>(
                     (static_cast<int64_t>(kTaunt3FlickYawA16) * flick_body) / 1000))));
    face_rest(g);
    // the eyes: a slow travelling look down the lean, a lopsided brow through
    // the shimmy, and both lids at half on the dismissal (bored)
    apply_eye_roll(g, -900 * lean / 1000, -200 * lean / 1000);
    apply_gaze(g,
               static_cast<int32_t>((static_cast<int64_t>(kGazeMaxA16 * 3 / 4) * lean) / 1000),
               static_cast<int32_t>((static_cast<int64_t>(kGazeLiftMaxA16 / 2) * (1000 - lean)) / 1000) -
                   static_cast<int32_t>((static_cast<int64_t>(kGazeLiftMaxA16 / 2) * flick) / 1000));
    apply_twinkle(g, static_cast<int32_t>(
                         (static_cast<int64_t>(kBlazeTwinkleA16) * sinp(fc, K, 2)) >> 16));
    apply_squint_lr(g, 620 * flick / 1000 + blink_at(f, 83),
                    420 * flick / 1000 + blink_at(f, 83));
    g.write(c, f);
    // the body rides the gesture: it rises INTO the shrug (insolent) and
    // drops on the dismissal
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(fc, K, kHoverHeightMm, kBobAmpAMm * 3 / 2, kBobAmpBMm, K / 46, K / 92) +
        static_cast<int32_t>((static_cast<int64_t>(fxu(kTaunt3ShrugLiftMm)) * shrug) / 1000) -
        static_cast<int32_t>((static_cast<int64_t>(fxu(kTaunt3AnticDipMm)) * antic) / 1000) -
        static_cast<int32_t>(
            (static_cast<int64_t>(fxu(kTaunt3FlickDropMm)) * flick_body) / 1000);
    // ...and it SQUASHES on the anticipation, which is the half of a wind-up a
    // root height cannot express: the body compresses before it rises.
    // PASS 14 / R4 -- AND THE PUNCHLINE IS A DIFFERENT SHAPE. The breath is
    // built by hand here rather than through compress_at, for one reason: the
    // dismissal's flatten must NOT ride the breath's sine. The ambient clock is
    // parked through the hold, so that sine is frozen at whatever phase the
    // hold began on, and a punchline whose depth depends on where the clock
    // stopped is not authored, it is rolled for. Same maths as compress_at
    // otherwise -- see its body; this is the third caller of that pattern
    // (the two deaths are the others) and it stays in step with them.
    {
      const int32_t breath_amp = kCompressAmpPm + kCompressAmpPm * shrug / 3000 +
                                 kCompressAmpPm * antic / 1500;
      const int32_t w = (65536 + sinp(fc, K, K / 46)) / 2;  // 0..65536
      int32_t flat = static_cast<int32_t>((static_cast<int64_t>(breath_amp) * w) >> 16);
      flat += static_cast<int32_t>(
          (static_cast<int64_t>(kCompressAmpPm) * kTaunt3FlickSquashPm * flick) / 1000000);
      if (flat > 60000) flat = 60000;  // the ceiling; see kCompressAmpPm
      const int32_t spread =
          static_cast<int32_t>((static_cast<int64_t>(flat) * kSpreadRatioPm) / 1000);
      c.deform[static_cast<size_t>(f)] =
          zc::DeformSample{static_cast<uint16_t>(flat), static_cast<uint16_t>(spread)};
    }
  }
  return c;
}

/** flight, slot 22 — D5 §7's FIRST LINE, deferred in five passes and the last
 *  un-attempted item of the original clip inventory:
 *
 *      "while the creature doesn't walk, it does move. So have flying movement
 *       with it bobbing up and down"
 *
 *  ⚠ WHY THIS IS NOT A DUPLICATE OF `hover`. `hover` (slot 0) carries a
 *  132+50 mm bob and does not go anywhere — it is the idle, and the owner's
 *  sentence is about the creature MOVING. `drift` (slot 1) travels but is D3
 *  §7's wind-blown glide: banked, passive, over-banking and correcting, a thing
 *  BLOWN rather than a thing flying. `hasty` (slot 8) is the same sentence's
 *  second half — accelerated and clumsy on purpose. Flight is the unhurried
 *  first half, and nothing in the bank was it.
 *
 *  Mechanically: a straight, calm traverse along +x crossing the shot through
 *  its centre (the Zixxtrixx walk staging precedent, the same one hasty uses —
 *  start half the travel back), with ONE clock at kFlightBobPeriodKeys driving
 *  the height, the pitch, the breath and the antenna's hang-back at fixed phase
 *  to each other. The pitch is the bob's own DERIVATIVE — nose up while
 *  climbing, nose down while sinking — which is why this reads as a body
 *  bouncing rather than as a creature with a sine added to its altitude.
 */
inline zc::Clip build_flight() {
  const int K = kFlightKeys;
  zc::Clip c = clip_shell(kFlightSlot, K, kHoverHeightMm);
  // ⚠ R2 (pass 13): THIS CLIP TRAVELS, so its wrap partner must carry the
  // traverse instead of folding back across it. With the flag off the last
  // key's sub-frame blends toward key 0 and the root wraps the WHOLE journey in
  // half a key -- an enormous fake velocity that teleports the pose and paints
  // grey speed-smear ghosts beside it (`drift`: near-grey pixels 295 -> 729 over
  // its last frames, then 59 at f0). `zc::Clip::wrap_root_delta` is default-OFF
  // so Zixxtrixx stays bit-identical; these four clips opt in. See
  // PASS-13-FINDINGS-C SS2 and tools/reel/wrapseam.py.
  c.wrap_root_delta = true;
  Rig g;
  // Integer cycles across the clip: the loop seam is exact by construction,
  // which is the same rule every other layer in this file obeys.
  const int cyc = K / kFlightBobPeriodKeys > 0 ? K / kFlightBobPeriodKeys : 1;
  const int32_t breath = kCompressAmpPm * kFlightBreathGainPm / 1000;
  for (int f = 0; f < K; ++f) {
    g.reset();
    antenna_knead(g, kFlightSlot, K, f);  // fills g.nod from the schedule
    // ---- THE ONE CLOCK ---------------------------------------------------
    const int32_t bob = sinp(f, K, cyc);              // the height
    const int32_t rise = sinp(f, K, cyc, 0x4000);     // d(height)/dt
    // PITCH rides the derivative: nose UP on the way up. A standing lean into
    // the travel sits under it, small — this thing is flying, not diving.
    g.q[kBRoot] = quat_mul(
        g.q[kBRoot],
        quat_z(static_cast<int32_t>(
                   (static_cast<int64_t>(kFlightPitchA16) * rise) >> 16) -
               kFlightPitchLeanA16));
    // a lazy roll, a quarter of the bob out of phase with the pitch so the two
    // never arrive together and the motion never has a single beat
    g.q[kBRoot] = quat_mul(
        g.q[kBRoot],
        quat_x(static_cast<int32_t>(
            (static_cast<int64_t>(kFlightBankA16) * sinp(f, K, cyc, 0x2000)) >> 16)));
    // ---- THE NODULE TRAIL: the antenna arrives AFTER the body -------------
    // Each ball hangs back on its own lag, so the bounce TRAVELS out along the
    // antenna instead of the three of them pumping in unison. Additive, so a
    // clip-level schedule (kNoduleClipPm[22]) could ride underneath if it were
    // ever turned on. A's vertical share is only worth anything because the
    // spans stretch — D9 §13; before wave 2a this term moved ball A by 3 mm.
    {
      const int32_t ty[3] = {
          static_cast<int32_t>((static_cast<int64_t>(kFlightTrailMm[0]) *
                                sinp(f, K, cyc, -kFlightTrailLag16[0])) >> 16),
          static_cast<int32_t>((static_cast<int64_t>(kFlightTrailMm[1]) *
                                sinp(f, K, cyc, -kFlightTrailLag16[1])) >> 16),
          static_cast<int32_t>((static_cast<int64_t>(kFlightTrailMm[2]) *
                                sinp(f, K, cyc, -kFlightTrailLag16[2])) >> 16)};
      // NEGATED: when the body is high the balls have not caught up yet.
      g.nod.ay -= ty[0];
      g.nod.by -= ty[1];
      g.nod.cy -= ty[2];
      // and a little of the same lag sideways, so the trail is a swim rather
      // than a piston
      g.nod.az -= ty[0] / 3;
      g.nod.cz += ty[2] / 3;
    }
    loop_alive(g, f, K, cyc, kFlightSwayPm, breath, cyc);
    face_rest(g);
    // the eyes look where it is going, and lift with the climb
    apply_gaze(g, kGazeMaxA16 / 3,
               static_cast<int32_t>(
                   (static_cast<int64_t>(kGazeLiftMaxA16 / 2) * rise) >> 16));
    apply_squint(g, blink_at(f, 53));
    g.write(c, f);
    // the traverse: start half the travel back, cross through centre
    c.root[static_cast<size_t>(f) * 3 + 0] =
        fxu(static_cast<int32_t>((f - K / 2) * kFlightSpeedMmPerKey));
    // THE BOB. Written from the same `bob` the pitch differentiated, not from a
    // second call to hover_at — one clock means one expression.
    c.root[static_cast<size_t>(f) * 3 + 1] =
        fxu(kHoverHeightMm) +
        static_cast<int32_t>((static_cast<int64_t>(fxu(kFlightBobAmpMm)) * bob) >> 16);
    // THE BREATH, on the same clock and phased to squash at the BOTTOM of the
    // arc: the bounce and the inhale are one motion, which is D5 §6's
    // "it's bouncy, its body stretches, inhales, exhales" read as one thing.
    c.deform[static_cast<size_t>(f)] =
        compress_at(f, K, cyc, breath, kFlightBreathPhase16);
  }
  return c;
}

}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_CLIPS_H
