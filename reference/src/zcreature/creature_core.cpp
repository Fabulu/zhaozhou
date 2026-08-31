// creature_core.cpp — formats, skeleton bake, pose decode + cache, skinning,
// ring builder, compile, and the 3->2 clamp gate.
//
// Spec: spec/creature_rules.md 1-3 (law citations in zref_creature.hpp);
// spec/qformats.md 2-4 (single-rounding law, rescale, fx16/angle16);
// charter 29-6/29-7. Integer-only: no host float appears anywhere below.

#include "zref/zref_creature.hpp"
#include "zref/zref_trig.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <map>
#include <tuple>

namespace zref {
namespace creature {

// ------------------------------------------------------------- quat lanes --

namespace {
// fx16 -> S 1.0.14 lane: rescale(.,2) (round-half-up), saturate +-kQuatOne.
inline int16_t quat_lane(int32_t fx_raw) {
  int64_t r = (static_cast<int64_t>(fx_raw) + 2) >> 2;
  if (r > kQuatOne) r = kQuatOne;
  if (r < -kQuatOne) r = -kQuatOne;
  return static_cast<int16_t>(r);
}
}  // namespace

quat16 quat16_quantize(fx16 w, fx16 x, fx16 y, fx16 z) {
  // Hemisphere canonicalization: q and -q are the same rotation; flip when
  // the first nonzero lane (w, x, y, z order) is negative so compression
  // never distinguishes hemispheres.
  if (w.raw < 0 || (w.raw == 0 && x.raw < 0) || (w.raw == 0 && x.raw == 0 && y.raw < 0) ||
      (w.raw == 0 && x.raw == 0 && y.raw == 0 && z.raw < 0)) {
    w.raw = -w.raw;
    x.raw = -x.raw;
    y.raw = -y.raw;
    z.raw = -z.raw;
  }
  return quat16{{quat_lane(w.raw), quat_lane(x.raw), quat_lane(y.raw), quat_lane(z.raw)}};
}

quat16 quat16_axis_angle(fx16 ax, fx16 ay, fx16 az, fx16 half_sin, fx16 half_cos) {
  SatLedger* L = nullptr;
  return quat16_quantize(half_cos, fx_mul(ax, half_sin, L), fx_mul(ay, half_sin, L),
                         fx_mul(az, half_sin, L));
}

quat16 quat16_nlerp(const quat16& a, const quat16& b, int32_t num, int32_t den) {
  if (den <= 0 || num <= 0) return a;
  if (num >= den) return b;
  // Hemisphere: q and -q are the same rotation, so blend toward whichever of
  // b, -b is nearer. Without this a key pair that quantized into opposite
  // hemispheres blends the LONG way round and the joint snaps through half a
  // turn between two frames.
  int64_t d = 0;
  for (int i = 0; i < 4; ++i) d += static_cast<int64_t>(a.q[i]) * b.q[i];
  const int64_t sgn = d < 0 ? -1 : 1;

  int64_t lane[4];
  int64_t mag2 = 0;
  for (int i = 0; i < 4; ++i) {
    // exact in s64: (a*(den-num) + sgn*b*num) / den, ONE rounding
    const int64_t v = static_cast<int64_t>(a.q[i]) * (den - num) + sgn * b.q[i] * num;
    lane[i] = (v + den / 2) / den;
    mag2 += lane[i] * lane[i];
  }
  // RENORMALIZE. A plain lerp shortens the quaternion, and quat16_to_mat3's
  // 9-product formula scales the whole matrix by |q|^2 -- which reads on
  // screen as the creature pulsing in size at every half-key.
  if (mag2 <= 0) return a;
  const int64_t mag = static_cast<int64_t>(isqrt_u64(static_cast<uint64_t>(mag2)));
  if (mag <= 0) return a;
  quat16 out{};
  for (int i = 0; i < 4; ++i) {
    int64_t r = (lane[i] * kQuatOne * 2 + mag) / (mag * 2);  // round-half-up
    if (r > kQuatOne) r = kQuatOne;
    if (r < -kQuatOne) r = -kQuatOne;
    out.q[i] = static_cast<int16_t>(r);
  }
  return out;
}

void quat16_to_mat3(const quat16& q, mat3x4fx& out, SatLedger* L) {
  const int64_t qw = q.q[0], qx = q.q[1], qy = q.q[2], qz = q.q[3];
  // 9-product formula on S 1.0.14 lanes; each element ONE rescale(.,11):
  //   2^16 * (1 - 2(a^2+b^2)) = 65536 - (Qa^2+Qb^2)/2^11
  //   2^16 * 2(ab -+ cd)      = (Qa*Qb -+ Qc*Qd)/2^11
  // products exact in s64 (|sum| < 2^30); NO renormalization (2.2 decision).
  const int64_t xx = qx * qx, yy = qy * qy, zz = qz * qz;
  const int64_t xy = qx * qy, xz = qx * qz, yz = qy * qz;
  const int64_t wz = qw * qz, wy = qw * qy, wx = qw * qx;
  out.m[0] = 65536 - rescale_s32(yy + zz, 11, L);
  out.m[1] = rescale_s32(xy - wz, 11, L);
  out.m[2] = rescale_s32(xz + wy, 11, L);
  out.m[3] = 0;
  out.m[4] = rescale_s32(xy + wz, 11, L);
  out.m[5] = 65536 - rescale_s32(xx + zz, 11, L);
  out.m[6] = rescale_s32(yz - wx, 11, L);
  out.m[7] = 0;
  out.m[8] = rescale_s32(xz - wy, 11, L);
  out.m[9] = rescale_s32(yz + wx, 11, L);
  out.m[10] = 65536 - rescale_s32(xx + yy, 11, L);
  out.m[11] = 0;
}

// ------------------------------------------------------------ mat3x4 ops ---

void mat3x4_mul(const mat3x4fx& a, const mat3x4fx& b, mat3x4fx& out, SatLedger* L) {
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      __int128 p = static_cast<__int128>(a.m[i * 4 + 0]) * b.m[0 * 4 + j] +
                   static_cast<__int128>(a.m[i * 4 + 1]) * b.m[1 * 4 + j] +
                   static_cast<__int128>(a.m[i * 4 + 2]) * b.m[2 * 4 + j];
      out.m[i * 4 + j] = rescale_s32(p, 16, L, &SatLedger::mul);
    }
    __int128 t = static_cast<__int128>(a.m[i * 4 + 0]) * b.m[0 * 4 + 3] +
                 static_cast<__int128>(a.m[i * 4 + 1]) * b.m[1 * 4 + 3] +
                 static_cast<__int128>(a.m[i * 4 + 2]) * b.m[2 * 4 + 3] +
                 (static_cast<__int128>(a.m[i * 4 + 3]) << 16);
    out.m[i * 4 + 3] = rescale_s32(t, 16, L, &SatLedger::mul);
  }
}

void mat3x4_invert_rigid(const mat3x4fx& in, mat3x4fx& out, SatLedger* L) {
  out.m[0] = in.m[0];
  out.m[1] = in.m[4];
  out.m[2] = in.m[8];
  out.m[4] = in.m[1];
  out.m[5] = in.m[5];
  out.m[6] = in.m[9];
  out.m[8] = in.m[2];
  out.m[9] = in.m[6];
  out.m[10] = in.m[10];
  for (int i = 0; i < 3; ++i) {
    const __int128 t = -(static_cast<__int128>(out.m[i * 4 + 0]) * in.m[3] +
                         static_cast<__int128>(out.m[i * 4 + 1]) * in.m[7] +
                         static_cast<__int128>(out.m[i * 4 + 2]) * in.m[11]);
    out.m[i * 4 + 3] = rescale_s32(t, 16, L, &SatLedger::mul);
  }
}

// ------------------------------------------------------------- skeleton ----

bool bake_skeleton(const Skeleton& sk, SkeletonBake& out) {
  if (sk.bone_count == 0 || sk.bone_count > kMaxBones) return false;
  for (int b = 0; b < sk.bone_count; ++b) {
    const Bone& bn = sk.bones[b];
    if (bn.parent > b) return false;  // parent-before-child REQUIRED (1.2)
    if (b == 0 && bn.parent != 0) return false;
    const int32_t px = b == 0 ? 0 : out.world_x[bn.parent];
    const int32_t py = b == 0 ? 0 : out.world_y[bn.parent];
    const int32_t pz = b == 0 ? 0 : out.world_z[bn.parent];
    out.world_x[b] = px + bn.tx;
    out.world_y[b] = py + bn.ty;
    out.world_z[b] = pz + bn.tz;
    // rest rotations are identity -> B_rest^{-1} = translate(-world): EXACT
    out.inv_rest[b] = mat3x4_identity();
    out.inv_rest[b].m[3] = -out.world_x[b];
    out.inv_rest[b].m[7] = -out.world_y[b];
    out.inv_rest[b].m[11] = -out.world_z[b];
  }
  return true;
}

// ---- A1: bake the 60 Hz presentation companion ----------------------------
namespace {
quat16 quat_renorm(int64_t w, int64_t x, int64_t y, int64_t z) {
  const uint64_t n2 = static_cast<uint64_t>(w * w + x * x + y * y + z * z);
  const int64_t n = static_cast<int64_t>(isqrt_u64(n2));
  if (n == 0) return quat16_identity();
  const auto d = [&](int64_t v) {
    return static_cast<int16_t>((v * kQuatOne + (v >= 0 ? n / 2 : -n / 2)) / n);
  };
  return quat16{{d(w), d(x), d(y), d(z)}};
}
}  // namespace

void bake_presentation_midpoints(Clip& c, uint8_t bc) {
  static const std::vector<uint8_t> kNoAuthoredChannels;
  bake_presentation_midpoints(c, bc, kNoAuthoredChannels);
}

void bake_presentation_midpoints(
    Clip& c, uint8_t bc,
    const std::vector<uint8_t>& authored_channels) {
  const int n = c.frame_count;
  const bool has_deform = c.deform.size() == static_cast<size_t>(n);
  // A midpoint deformation channel is meaningless without one valid source
  // sample per key. Clear malformed/stale data even on a non-interpolated clip.
  if (!has_deform) c.mid_deform.clear();
  if (!c.interpolate || n < 2) return;
  const size_t quat_count = static_cast<size_t>(n) * bc;
  const size_t root_count = static_cast<size_t>(n) * 3;

  // Preserve the old storage only as a source for explicitly owned samples.
  // Complete but unowned arrays are generated cache, so key edits invalidate
  // and regenerate them just as surely as missing or partial arrays.
  const std::vector<quat16> old_mid_quats = c.mid_quats;
  const std::vector<int32_t> old_mid_root = c.mid_root;
  const std::vector<DeformSample> old_mid_deform = c.mid_deform;
  c.mid_quats.assign(quat_count, quat16_identity());
  c.mid_root.assign(root_count, 0);
  if (has_deform)
    c.mid_deform.assign(static_cast<size_t>(n), DeformSample{});
  else
    c.mid_deform.clear();
  // event-adjacent segments keep the plain nlerp midpoint (monotone at
  // impact/burial/extraction: no smoothing across a gameplay moment)
  std::vector<bool> plain(n, false);
  for (const ClipEvent& e : c.events) {
    for (int d = -2; d <= 1; ++d) {
      const int k = (static_cast<int>(e.frame) + d + n) % n;
      plain[static_cast<size_t>(k)] = true;
    }
  }
  // hold_last: the final segment clamps instead of wrapping (a one-shot
  // clip's held corpse must not blend back toward key 0)
  const auto wrap = [&](int k) {
    if (c.hold_last) return k < 0 ? 0 : (k >= n ? n - 1 : k);
    return (k + n) % n;
  };
  for (int k = 0; k < n; ++k) {
    const int k0 = wrap(k - 1), k1 = k, k2 = wrap(k + 1), k3 = wrap(k + 2);
    // root: Catmull-Rom at t=1/2, clamped into the segment interval
    for (int i = 0; i < 3; ++i) {
      const int64_t p0 = c.root[static_cast<size_t>(k0) * 3 + i];
      const int64_t p1 = c.root[static_cast<size_t>(k1) * 3 + i];
      const int64_t p2 = c.root[static_cast<size_t>(k2) * 3 + i];
      const int64_t p3 = c.root[static_cast<size_t>(k3) * 3 + i];
      int64_t m = (-p0 + 9 * p1 + 9 * p2 - p3) / 16;
      const int64_t lo = p1 < p2 ? p1 : p2, hi = p1 < p2 ? p2 : p1;
      if (m < lo) m = lo;
      if (m > hi) m = hi;
      c.mid_root[static_cast<size_t>(k) * 3 + i] = static_cast<int32_t>(m);
    }
    if (!c.mid_deform.empty()) {
      const DeformSample& d0 = c.deform[static_cast<size_t>(k0)];
      const DeformSample& d1 = c.deform[static_cast<size_t>(k1)];
      const DeformSample& d2 = c.deform[static_cast<size_t>(k2)];
      const DeformSample& d3 = c.deform[static_cast<size_t>(k3)];
      const auto mid_lane = [&](uint16_t p0, uint16_t p1, uint16_t p2,
                                uint16_t p3) {
        if (p1 == p2) return p1;  // a held deformation stays held
        int64_t m = plain[static_cast<size_t>(k)]
                        ? (static_cast<int64_t>(p1) + p2) / 2
                        : (-static_cast<int64_t>(p0) + 9 * p1 + 9 * p2 - p3) / 16;
        const int64_t lo = p1 < p2 ? p1 : p2;
        const int64_t hi = p1 < p2 ? p2 : p1;
        if (m < lo) m = lo;
        if (m > hi) m = hi;
        return static_cast<uint16_t>(m);
      };
      c.mid_deform[static_cast<size_t>(k)] =
          DeformSample{mid_lane(d0.flatten, d1.flatten, d2.flatten, d3.flatten),
                       mid_lane(d0.spread, d1.spread, d2.spread, d3.spread)};
    }
    for (int b = 0; b < bc; ++b) {
      const quat16& q1 = c.quats[static_cast<size_t>(k1) * bc + b];
      const quat16& q2r = c.quats[static_cast<size_t>(k2) * bc + b];
      // A held authored channel must remain held at presentation cadence.
      // Cubic neighbours are allowed to shape motion *between different keys*,
      // never to pull a midpoint away when both segment endpoints are already
      // bit-identical.  Without this, an attack's constant embedded pose still
      // twitched at 60 Hz from the entry/extraction keys outside the segment.
      if (std::memcmp(&q1, &q2r, sizeof(q1)) == 0) {
        c.mid_quats[static_cast<size_t>(k) * bc + b] = q1;
        continue;
      }
      if (plain[static_cast<size_t>(k)]) {
        c.mid_quats[static_cast<size_t>(k) * bc + b] = quat16_nlerp(q1, q2r, 1, 2);
        continue;
      }
      const quat16& q0r = c.quats[static_cast<size_t>(k0) * bc + b];
      const quat16& q3r = c.quats[static_cast<size_t>(k3) * bc + b];
      // hemisphere-align every neighbour to q1
      const auto align = [&](const quat16& q) {
        const int32_t dot =
            q1.q[0] * q.q[0] + q1.q[1] * q.q[1] + q1.q[2] * q.q[2] + q1.q[3] * q.q[3];
        quat16 r = q;
        if (dot < 0)
          for (int i = 0; i < 4; ++i) r.q[i] = static_cast<int16_t>(-r.q[i]);
        return r;
      };
      const quat16 q0 = align(q0r), q2 = align(q2r), q3 = align(q3r);
      c.mid_quats[static_cast<size_t>(k) * bc + b] =
          quat_renorm(-q0.q[0] + 9 * q1.q[0] + 9 * q2.q[0] - q3.q[0],
                      -q0.q[1] + 9 * q1.q[1] + 9 * q2.q[1] - q3.q[1],
                      -q0.q[2] + 9 * q1.q[2] + 9 * q2.q[2] - q3.q[2],
                      -q0.q[3] + 9 * q1.q[3] + 9 * q2.q[3] - q3.q[3]);
    }

    const uint8_t owned = static_cast<size_t>(k) < authored_channels.size()
                              ? authored_channels[static_cast<size_t>(k)]
                              : 0;
    const size_t qi = static_cast<size_t>(k) * bc;
    const size_t ri = static_cast<size_t>(k) * 3;
    if ((owned & kMidpointQuatsAuthored) != 0 &&
        old_mid_quats.size() >= qi + bc)
      std::copy_n(old_mid_quats.begin() + qi, bc,
                  c.mid_quats.begin() + qi);
    if ((owned & kMidpointRootAuthored) != 0 &&
        old_mid_root.size() >= ri + 3)
      std::copy_n(old_mid_root.begin() + ri, 3,
                  c.mid_root.begin() + ri);
    if (has_deform && (owned & kMidpointDeformAuthored) != 0 &&
        old_mid_deform.size() > static_cast<size_t>(k))
      c.mid_deform[static_cast<size_t>(k)] =
          old_mid_deform[static_cast<size_t>(k)];
  }
}

// ---------------------------------------------------------- pose decode ----

void decode_pose(const CreatureType& type, const Clip& clip, uint16_t frame,
                 std::array<mat3x4fx, kMaxBones>& out, SatLedger* L, uint8_t sub) {
  const Skeleton& sk = type.skeleton;
  const uint8_t bc = type.bank.bone_count;
  const int32_t* disp = clip.root.data() + static_cast<size_t>(frame) * 3;
  int32_t disp_i[3] = {disp[0], disp[1], disp[2]};
  if (clip.interpolate && sub != 0) {
    if (!clip.mid_root.empty()) {
      // A1: the baked companion midpoint (cubic, monotone-clamped)
      const int32_t* dm = clip.mid_root.data() + static_cast<size_t>(frame) * 3;
      for (int i = 0; i < 3; ++i) disp_i[i] = dm[i];
    } else {
      const uint16_t nf = static_cast<uint16_t>(
          frame + 1 >= clip.frame_count ? (clip.hold_last ? frame : 0) : frame + 1);
      const int32_t* d2 = clip.root.data() + static_cast<size_t>(nf) * 3;
      for (int i = 0; i < 3; ++i) disp_i[i] = (disp[i] + d2[i]) >> 1;
    }
  }
  disp = disp_i;
  std::array<mat3x4fx, kMaxBones> a{};  // animated world chains
  for (int b = 0; b < bc; ++b) {
    mat3x4fx r;
    // PRESENTATION INTERPOLATION (section 8): keys are 30 Hz held for two sim
    // ticks, so without this the pose only moves every other tick. Opt-in per
    // clip; the sim clock and every event frame are untouched.
    quat16 q = clip.quats[static_cast<size_t>(frame) * bc + b];
    if (clip.interpolate && sub != 0) {
      if (!clip.mid_quats.empty()) {
        q = clip.mid_quats[static_cast<size_t>(frame) * bc + b];  // A1
      } else {
        const uint16_t nf = static_cast<uint16_t>(
            frame + 1 >= clip.frame_count ? (clip.hold_last ? frame : 0) : frame + 1);
        q = quat16_nlerp(q, clip.quats[static_cast<size_t>(nf) * bc + b], sub, 2);
      }
    }
    quat16_to_mat3(q, r, L);
    mat3x4fx lr = r;  // LR = R with the rest translation (+ root displacement)
    lr.m[3] += sk.bones[b].tx + (b == 0 ? disp[0] : 0);
    lr.m[7] += sk.bones[b].ty + (b == 0 ? disp[1] : 0);
    lr.m[11] += sk.bones[b].tz + (b == 0 ? disp[2] : 0);
    if (b == 0) {
      a[0] = lr;
    } else {
      mat3x4_mul(a[sk.bones[b].parent], lr, a[b], L);
    }
    mat3x4_mul(a[b], type.baked.inv_rest[b], out[b], L);
  }
  for (int b = bc; b < kMaxBones; ++b) out[b] = mat3x4_identity();
}

// PoseBank ------------------------------------------------------------------

namespace {
const std::array<mat3x4fx, kMaxBones> kIdentityPose = [] {
  std::array<mat3x4fx, kMaxBones> p{};
  p.fill(mat3x4_identity());
  return p;
}();
}  // namespace

void PoseBank::begin_frame() {
  for (auto& s : slots_) s.this_frame = false;
}

const mat3x4fx* PoseBank::acquire(const CreatureType& type, uint16_t slot, uint16_t frame,
                                  uint8_t sub) {
  const Clip* clip = nullptr;
  for (const Clip& c : type.bank.clips)
    if (c.slot_id == slot) clip = &c;
  if (clip == nullptr || frame >= clip->frame_count) {
    ++ctr_.bad_ids;
    return kIdentityPose.data();
  }
  for (auto& s : slots_) {
    if (s.valid && s.type == type.type_id && s.clip == slot && s.frame == frame && s.sub == sub) {
      ++ctr_.hits;
      s.this_frame = true;
      s.lru = ++lru_ctr_;
      return s.pose.data();
    }
  }
  ++ctr_.misses;
  size_t victim = kCacheTuples;
  uint64_t best = UINT64_MAX;
  for (size_t i = 0; i < kCacheTuples; ++i) {
    const Slot& s = slots_[i];
    if (!s.valid) {
      victim = i;
      break;
    }
    if (s.this_frame) continue;  // never evicted (bounded by the 128 budget)
    if (s.lru < best) {
      best = s.lru;
      victim = i;
    }
  }
  if (victim == kCacheTuples) {
    // every slot referenced this frame: a content-tier violation — decode
    // without inserting and count it (the deterministic clamp)
    ++ctr_.clamped_inserts;
    decode_pose(type, *clip, frame, scratch_, nullptr, sub);
    return scratch_.data();
  }
  Slot& s = slots_[victim];
  if (!s.valid) ++resident_;
  s.valid = true;
  s.type = type.type_id;
  s.clip = slot;
  s.frame = frame;
  s.sub = sub;
  s.lru = ++lru_ctr_;
  s.this_frame = true;
  decode_pose(type, *clip, frame, s.pose, nullptr, sub);
  return s.pose.data();
}

// ------------------------------------------------------------- skinning ----

DeformSample deformation_sample(const CreatureType& type, uint16_t slot,
                                uint16_t frame, uint8_t sub) {
  const Clip* clip = nullptr;
  for (const Clip& c : type.bank.clips)
    if (c.slot_id == slot) clip = &c;
  if (clip == nullptr || frame >= clip->frame_count || clip->deform.empty())
    return DeformSample{};
  if (clip->deform.size() != clip->frame_count) return DeformSample{};
  if (clip->interpolate && sub != 0) {
    if (clip->mid_deform.size() == clip->frame_count)
      return clip->mid_deform[frame];
    const uint16_t nf = static_cast<uint16_t>(
        frame + 1 >= clip->frame_count ? (clip->hold_last ? frame : 0) : frame + 1);
    const DeformSample& a = clip->deform[frame];
    const DeformSample& b = clip->deform[nf];
    return DeformSample{static_cast<uint16_t>((static_cast<uint32_t>(a.flatten) + b.flatten) / 2),
                        static_cast<uint16_t>((static_cast<uint32_t>(a.spread) + b.spread) / 2)};
  }
  return clip->deform[frame];
}

SkinVertex deform_skin_vertex(const SkinVertex& v, const DeformVertex& meta,
                              const DeformSample& sample) {
  // This branch is the exact-identity contract: no fixed-point round is allowed
  // to touch an ordinary clip, an unauthorised key, or an unmarked vertex.
  if (meta.role == DeformRole::kNone || meta.strength == 0 ||
      (sample.flatten == 0 && sample.spread == 0))
    return v;

  SkinVertex out = v;
  const int32_t flatten = static_cast<int32_t>(
      (static_cast<uint32_t>(sample.flatten) * meta.strength + 127) / 255);
  const int32_t spread = static_cast<int32_t>(
      (static_cast<uint32_t>(sample.spread) * meta.strength + 127) / 255);
  if (flatten == 0 && spread == 0) return v;
  const int32_t squash_scale = 65536 - flatten;  // always positive: flatten is u16
  const int32_t spread_scale = 65536 + spread;
  const uint8_t axis = meta.axis < 3 ? meta.axis : 0;

  int32_t* const dst[3] = {&out.x, &out.y, &out.z};
  const int32_t src[3] = {v.x, v.y, v.z};
  const int32_t center[3] = {meta.center_x, meta.center_y, meta.center_z};
  const int32_t carrier[3] = {meta.carrier_x, meta.carrier_y, meta.carrier_z};
  if (meta.role == DeformRole::kRadial) {
    for (uint8_t lane = 0; lane < 3; ++lane) {
      const int32_t scale = lane == axis ? squash_scale : spread_scale;
      *dst[lane] = center[lane] +
                   rescale_s32(static_cast<int64_t>(src[lane] - center[lane]) * scale,
                               16, nullptr);
    }

    // For diag(t,t,s), inverse-transpose is diag(1/t,1/t,1/s).
    // Multiplying by the common positive factor s*t gives diag(s,s,t),
    // preserving the exact direction without reciprocal division.
    if (v.nx != 0 || v.ny != 0 || v.nz != 0) {
      const int32_t packed[3] = {v.nx, v.ny, v.nz};
      int64_t n[3];
      for (uint8_t lane = 0; lane < 3; ++lane)
        n[lane] = static_cast<int64_t>(packed[lane]) *
                  (lane == axis ? spread_scale : squash_scale);
      const uint64_t mag2 = static_cast<uint64_t>(n[0] * n[0]) +
                            static_cast<uint64_t>(n[1] * n[1]) +
                            static_cast<uint64_t>(n[2] * n[2]);
      if (mag2 != 0) {
        const int64_t mag = static_cast<int64_t>(isqrt_u64(mag2));
        const auto pack = [mag](int64_t c) {
          const int64_t q = (c * 127 + (c >= 0 ? mag / 2 : -mag / 2)) / mag;
          return static_cast<int8_t>(q > 127 ? 127 : (q < -127 ? -127 : q));
        };
        out.nx = pack(n[0]);
        out.ny = pack(n[1]);
        out.nz = pack(n[2]);
      }
    }
  } else if (meta.role == DeformRole::kFollower) {
    // Followers inherit the carrier point's ellipsoidal displacement as a pure
    // translation. Their own dimensions/normals stay rigid; authors place the
    // per-ring centre so only intended attachment offsets participate.
    for (uint8_t lane = 0; lane < 3; ++lane) {
      const int32_t scale = lane == axis ? squash_scale : spread_scale;
      const int32_t moved =
          center[lane] +
          rescale_s32(static_cast<int64_t>(carrier[lane] - center[lane]) * scale,
                      16, nullptr);
      *dst[lane] += moved - carrier[lane];
    }
  }
  return out;
}

void skin_vertex(const mat3x4fx* palette, const SkinVertex& v, int32_t& ox, int32_t& oy,
                 int32_t& oz, SatLedger* L) {
  const mat3x4fx& A = palette[v.b0];
  if (v.b1 == v.b0 || v.w0 == 64) {
    ox =
        rescale_s32(static_cast<__int128>(A.m[0]) * v.x + static_cast<__int128>(A.m[1]) * v.y +
                        static_cast<__int128>(A.m[2]) * v.z + (static_cast<__int128>(A.m[3]) << 16),
                    16, L, &SatLedger::mul);
    oy =
        rescale_s32(static_cast<__int128>(A.m[4]) * v.x + static_cast<__int128>(A.m[5]) * v.y +
                        static_cast<__int128>(A.m[6]) * v.z + (static_cast<__int128>(A.m[7]) << 16),
                    16, L, &SatLedger::mul);
    oz = rescale_s32(static_cast<__int128>(A.m[8]) * v.x + static_cast<__int128>(A.m[9]) * v.y +
                         static_cast<__int128>(A.m[10]) * v.z +
                         (static_cast<__int128>(A.m[11]) << 16),
                     16, L, &SatLedger::mul);
    return;
  }
  // 2-weight: w0*(A v) + w1*(B v) EXACTLY in s128 (the skin product is
  // never rounded before the blend), then ONE rescale(.,22) — 16 matrix
  // fraction bits + 6 weight fraction bits. A3b: no double rounding.
  const mat3x4fx& B = palette[v.b1];
  const int32_t w1 = 64 - v.w0;
  const int32_t rows[3] = {0, 4, 8};
  int32_t* o[3] = {&ox, &oy, &oz};
  for (int i = 0; i < 3; ++i) {
    const int r = rows[i];
    const __int128 pa =
        static_cast<__int128>(A.m[r]) * v.x + static_cast<__int128>(A.m[r + 1]) * v.y +
        static_cast<__int128>(A.m[r + 2]) * v.z + (static_cast<__int128>(A.m[r + 3]) << 16);
    const __int128 pb =
        static_cast<__int128>(B.m[r]) * v.x + static_cast<__int128>(B.m[r + 1]) * v.y +
        static_cast<__int128>(B.m[r + 2]) * v.z + (static_cast<__int128>(B.m[r + 3]) << 16);
    *o[i] = rescale_s32(v.w0 * pa + w1 * pb, 22, L, &SatLedger::mul);
  }
}

int32_t skin_normal_lambert(const mat3x4fx* palette, const SkinVertex& v, int32_t lx, int32_t ly,
                            int32_t lz) {
  if (v.nx == 0 && v.ny == 0 && v.nz == 0) return 0;
  const mat3x4fx& A = palette[v.b0];
  const mat3x4fx& B = palette[v.b1];
  const int32_t w0 = v.w0;
  const int32_t w1 = 64 - w0;
  int64_t n[3] = {};
  for (int row = 0; row < 3; ++row) {
    const int r = row * 4;
    const int64_t na = static_cast<int64_t>(A.m[r]) * v.nx +
                       static_cast<int64_t>(A.m[r + 1]) * v.ny +
                       static_cast<int64_t>(A.m[r + 2]) * v.nz;
    const int64_t nb = static_cast<int64_t>(B.m[r]) * v.nx +
                       static_cast<int64_t>(B.m[r + 1]) * v.ny +
                       static_cast<int64_t>(B.m[r + 2]) * v.nz;
    // Keep the full weighted direction. Its common 1/64 and uniform-bulk
    // factors cancel in the normalisation below, so no pre-normalise rounding
    // is introduced.
    n[row] = static_cast<int64_t>(w0) * na + static_cast<int64_t>(w1) * nb;
  }

  // Range-reduce before squaring. The same shift is applied to every lane, so
  // direction is unchanged; this only protects the fixed-width magnitude.
  int64_t mx =
      std::max({n[0] < 0 ? -n[0] : n[0], n[1] < 0 ? -n[1] : n[1], n[2] < 0 ? -n[2] : n[2]});
  while (mx >= (int64_t{1} << 30)) {
    n[0] >>= 1;
    n[1] >>= 1;
    n[2] >>= 1;
    mx >>= 1;
  }
  const uint64_t mag2 = static_cast<uint64_t>(n[0] * n[0]) + static_cast<uint64_t>(n[1] * n[1]) +
                        static_cast<uint64_t>(n[2] * n[2]);
  if (mag2 == 0) return 0;
  const int64_t mag = static_cast<int64_t>(isqrt_u64(mag2));
  const __int128 dot = static_cast<__int128>(n[0]) * lx + static_cast<__int128>(n[1]) * ly +
                       static_cast<__int128>(n[2]) * lz;
  if (dot <= 0) return 0;
  int64_t lam = static_cast<int64_t>((dot + mag / 2) / mag);
  if (lam > 65536) lam = 65536;
  return static_cast<int32_t>(lam);
}

// ----------------------------------------------------------- ring builder --

namespace {

struct BuiltVert {
  int32_t x, y, z;
  uint8_t u, v;
};

// One ring's vertices: angle_k = (k * 65536 / seg + align * 256) mod 2^16
// turns; U = angle >> 8 (the 8-bit angular alignment -> U law); position via
// the fx trig tables (single-rounded products — compile-time authoring
// arithmetic, deterministic; the donor's all-integer ring construction).
std::vector<BuiltVert> build_ring(const RingSpec& spec, uint8_t align, uint8_t v_lane) {
  std::vector<BuiltVert> out(spec.segments);
  // Elliptical when either per-axis radius is set, circular otherwise. The
  // circular case is bit-identical to what it was, because rx == rz == radius
  // reduces to the same two products.
  const int32_t rx = spec.rx != 0 || spec.rz != 0 ? spec.rx : spec.radius;
  const int32_t rz = spec.rx != 0 || spec.rz != 0 ? spec.rz : spec.radius;
  for (int k = 0; k < spec.segments; ++k) {
    const uint16_t ang = static_cast<uint16_t>((k * 65536 / spec.segments + align * 256) & 0xFFFF);
    out[k].x =
        spec.cx + rescale_s32(static_cast<int64_t>(rx) * fx_cos(angle16{ang}).raw, 16, nullptr);
    out[k].y = spec.y;
    out[k].z =
        spec.cz + rescale_s32(static_cast<int64_t>(rz) * fx_sin(angle16{ang}).raw, 16, nullptr);
    out[k].u = static_cast<uint8_t>(ang >> 8);
    out[k].v = v_lane;
  }
  return out;
}

// ---- SMOOTH VERTEX NORMALS (N2) -------------------------------------------
// Generated at compile time over a finished meshlet set, in creature-global
// BIND space. Area-weighted: the unnormalised cross product of a triangle's
// edges IS 2*area times its unit normal, so summing raw crosses weights each
// face by its area for free. The accumulator is keyed on the EXACT bind
// position (x,y,z), so the textured seam duplicate (u=255), the ring-closing
// wrap vertex and every meshlet-boundary duplicate receive the SAME packed
// normal — a lighting seam cannot open where the surface is closed.
//
// Deterministic integer arithmetic throughout (authoring-time, like
// build_ring's trig): edges are pre-shifted >>8 (quarter-mm units) so a
// cross term fits s64 for any legal extent; the accumulated vector is
// range-reduced before the isqrt so the magnitude square fits u64.
//
// The micro rung calls this on its OWN meshlets — normals recomputed from
// micro topology, never copied from the full mesh (the amendment's rule).
namespace {
void generate_smooth_normals(std::vector<Meshlet>& mesh) {
  std::map<std::tuple<int32_t, int32_t, int32_t>, std::array<int64_t, 3>> acc;
  for (const Meshlet& m : mesh) {
    for (size_t t = 0; t + 2 < m.idx.size(); t += 3) {
      const SkinVertex& a = m.verts[m.idx[t]];
      const SkinVertex& b = m.verts[m.idx[t + 1]];
      const SkinVertex& c = m.verts[m.idx[t + 2]];
      const int64_t e1x = (static_cast<int64_t>(b.x) - a.x) >> 8;
      const int64_t e1y = (static_cast<int64_t>(b.y) - a.y) >> 8;
      const int64_t e1z = (static_cast<int64_t>(b.z) - a.z) >> 8;
      const int64_t e2x = (static_cast<int64_t>(c.x) - a.x) >> 8;
      const int64_t e2y = (static_cast<int64_t>(c.y) - a.y) >> 8;
      const int64_t e2z = (static_cast<int64_t>(c.z) - a.z) >> 8;
      // build_ring_part's ring zipper is OUTWARD wound: for an unrotated
      // +Y-axis tube its first face has e1 along +Y and e2 around the ring,
      // so e1 x e2 points radially away from the centreline. V13's committed
      // synthetic and posed-ring fixture proved the old negation made every
      // packed normal inward (dot(normal,outward) approximately -1).
      const int64_t nx = e1y * e2z - e1z * e2y;
      const int64_t ny = e1z * e2x - e1x * e2z;
      const int64_t nz = e1x * e2y - e1y * e2x;
      for (const uint8_t vi : {m.idx[t], m.idx[t + 1], m.idx[t + 2]}) {
        const SkinVertex& v = m.verts[vi];
        auto& s = acc[{v.x, v.y, v.z}];
        s[0] += nx;
        s[1] += ny;
        s[2] += nz;
      }
    }
  }
  for (Meshlet& m : mesh) {
    for (SkinVertex& v : m.verts) {
      const auto it = acc.find({v.x, v.y, v.z});
      if (it == acc.end()) continue;
      int64_t x = it->second[0], y = it->second[1], z = it->second[2];
      // range-reduce so x^2+y^2+z^2 fits u64 comfortably
      int64_t mx = std::max({x < 0 ? -x : x, y < 0 ? -y : y, z < 0 ? -z : z});
      while (mx >= (int64_t{1} << 30)) {
        x >>= 8;
        y >>= 8;
        z >>= 8;
        mx >>= 8;
      }
      const uint64_t mag2 = static_cast<uint64_t>(x * x) + static_cast<uint64_t>(y * y) +
                            static_cast<uint64_t>(z * z);
      if (mag2 == 0) continue;  // degenerate: leave "no normal" (flat fallback)
      const int64_t norm = static_cast<int64_t>(isqrt_u64(mag2));
      const auto pack = [norm](int64_t c) {
        const int64_t s = (c * 127 + (c >= 0 ? norm / 2 : -norm / 2)) / norm;
        return static_cast<int8_t>(s > 127 ? 127 : (s < -127 ? -127 : s));
      };
      v.nx = pack(x);
      v.ny = pack(y);
      v.nz = pack(z);
    }
  }
}
}  // namespace

}  // namespace

std::vector<Meshlet> build_ring_part(const RingPart& part) {
  std::vector<Meshlet> out;
  if (part.rings.size() < 2) return out;
  const int n_rings = static_cast<int>(part.rings.size());

  // exact quarter-turn orientation (entries in {0, +-65536}: no rounding)
  const std::array<int32_t, 9> rot = [pq = part.pitch_q, yq = part.yaw_q] {
    // Ry(yq * 90) * Rx(pq * 90), quarter indices mod 4
    const auto rx = [](int q, int32_t* m) {
      q &= 3;
      const int32_t c[4] = {65536, 0, -65536, 0}, s[4] = {0, 65536, 0, -65536};
      const int32_t t[9] = {65536, 0, 0, 0, c[q], -s[q], 0, s[q], c[q]};
      for (int i = 0; i < 9; ++i) m[i] = t[i];
    };
    const auto ry = [](int q, int32_t* m) {
      q &= 3;
      const int32_t c[4] = {65536, 0, -65536, 0}, s[4] = {0, 65536, 0, -65536};
      const int32_t t[9] = {c[q], 0, s[q], 0, 65536, 0, -s[q], 0, c[q]};
      for (int i = 0; i < 9; ++i) m[i] = t[i];
    };
    int32_t X[9], Y[9];
    rx(pq, X);
    ry(yq, Y);
    std::array<int32_t, 9> R{};
    // Apply pitch first, then yaw: R = Ry * Rx, matching the authored axis
    // convention above. Both inputs are signed permutation matrices, so the
    // product remains exact in {0, +-65536}.
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
        R[i * 3 + j] = Y[i * 3 + 0] / 65536 * X[0 * 3 + j] + Y[i * 3 + 1] / 65536 * X[1 * 3 + j] +
                       Y[i * 3 + 2] / 65536 * X[2 * 3 + j];
    return R;
  }();

  Meshlet cur;
  std::vector<std::vector<BuiltVert>> ring_cache;  // rings emitted into `cur`

  const auto v_lane_of = [&](int ri) {
    // T4: map the ring index into the part's page V RANGE [v0..v1] (defaults
    // 0..255 keep every pre-atlas part bit-identical)
    const int span = part.v1 - part.v0;
    return static_cast<uint8_t>(part.v0 + (static_cast<int64_t>(ri) * span + (n_rings - 1) / 2) /
                                              (n_rings - 1));
  };
  const auto orient = [&](int32_t& x, int32_t& y, int32_t& z) {
    const int32_t nx = rot[0] / 65536 * x + rot[1] / 65536 * y + rot[2] / 65536 * z;
    const int32_t ny = rot[3] / 65536 * x + rot[4] / 65536 * y + rot[5] / 65536 * z;
    const int32_t nz = rot[6] / 65536 * x + rot[7] / 65536 * y + rot[8] / 65536 * z;
    x = nx;
    y = ny;
    z = nz;
  };
  const auto deform_of = [&](const RingSpec& rs) {
    DeformVertex d;
    d.role = rs.deform_role;
    d.strength = rs.deform_strength;
    if (d.role == DeformRole::kNone || d.strength == 0) return d;
    d.center_x = rs.deform_center_x;
    d.center_y = rs.deform_center_y;
    d.center_z = rs.deform_center_z;
    orient(d.center_x, d.center_y, d.center_z);
    d.carrier_x = rs.cx;
    d.carrier_y = rs.y;
    d.carrier_z = rs.cz;
    orient(d.carrier_x, d.carrier_y, d.carrier_z);
    int32_t ax = rs.deform_axis == 0 ? 65536 : 0;
    int32_t ay = rs.deform_axis == 1 ? 65536 : 0;
    int32_t az = rs.deform_axis == 2 ? 65536 : 0;
    orient(ax, ay, az);
    d.axis = ax != 0 ? 0 : (ay != 0 ? 1 : 2);  // sign is irrelevant to scaling
    return d;
  };
  const auto drop_identity_sidecar = [](Meshlet& m) {
    bool active = false;
    for (const DeformVertex& d : m.deform)
      active = active || (d.role != DeformRole::kNone && d.strength != 0);
    if (!active) m.deform.clear();
  };
  // TEXTURE SEAM LAW (2026-08-26). U is periodic (a full turn is 256) but a
  // SkinVertex u is 8-bit, so the ring's closing face used to interpolate
  // u 2xx -> 0 BACKWARD across the whole tile — one face per ring wearing the
  // entire texture as a smeared streak, worst wherever a painted feature
  // (Zixxtrixx's eye) sat near U = 0. The fix is a per-ring DUPLICATE of
  // vertex 0 carrying u = 255 (the true value is 256; the 1/256-turn error is
  // invisible), and the closing face indexes the duplicate. Applied ONLY to
  // textured parts with align == 0 (align != 0 moves the wrap into the ring
  // interior, which nothing authored uses): untextured parts — the watchdog,
  // every pinned CRC — are bit-identical.
  const uint32_t dup = part.page != 255 && part.align == 0 ? 1u : 0u;
  const auto add_ring = [&](int ri) -> uint32_t {
    ring_cache.push_back(build_ring(part.rings[ri], part.align, v_lane_of(ri)));
    for (BuiltVert& bv : ring_cache.back()) orient(bv.x, bv.y, bv.z);
    const uint32_t base = static_cast<uint32_t>(cur.verts.size());
    // CHAIN parts carry their bones per RING, so one continuous surface can
    // span a whole bone chain and blend across each joint. RIGID parts keep
    // the original {bone, bone, 64} exactly, so nothing that existed before
    // changes by a single bit.
    const RingSpec& rs = part.rings[ri];
    const uint8_t vb0 = part.chain ? rs.b0 : part.bone;
    const uint8_t vb1 = part.chain ? rs.b1 : part.bone;
    const uint8_t vw0 = part.chain ? rs.w0 : 64;
    const DeformVertex dm = deform_of(rs);
    for (const BuiltVert& bv : ring_cache.back()) {
      cur.verts.push_back(SkinVertex{bv.x, bv.y, bv.z, vb0, vb1, vw0, bv.u, bv.v});
      cur.deform.push_back(dm);
    }
    if (dup != 0) {
      const BuiltVert& b0 = ring_cache.back()[0];
      cur.verts.push_back(SkinVertex{b0.x, b0.y, b0.z, vb0, vb1, vw0, 255, b0.v});
      cur.deform.push_back(dm);
    }
    return base;
  };

  add_ring(0);
  for (int ri = 0; ri + 1 < n_rings; ++ri) {
    const int n = part.rings[ri].segments;
    const int m = part.rings[ri + 1].segments;
    const bool bottom_cap = ri == 0 && (part.caps & kCapBot) != 0;
    const bool top_cap = ri + 2 == n_rings && (part.caps & kCapTop) != 0;
    const int tris_needed = n + m + (bottom_cap ? n : 0) + (top_cap ? m : 0);
    const int verts_needed = m + static_cast<int>(dup) + (bottom_cap ? 1 : 0) + (top_cap ? 1 : 0);
    if (static_cast<int>(cur.verts.size()) + verts_needed > kMeshletMaxVerts ||
        static_cast<int>(cur.idx.size()) / 3 + tris_needed > kMeshletMaxTris) {
      cur.page = part.page;
      cur.r = part.r;
      cur.g = part.g;
      cur.b = part.b;
      drop_identity_sidecar(cur);
      out.push_back(std::move(cur));
      cur = Meshlet{};
      ring_cache.clear();
      add_ring(ri);  // duplicate the seam ring: the split stays watertight
    }
    const uint32_t hi = add_ring(ri + 1);
    const uint32_t lo = hi - (static_cast<uint32_t>(n) + dup);
    // ring position n is the wrap: the u=255 duplicate when textured, vertex 0
    // otherwise
    const uint32_t lo_wrap = dup != 0 ? static_cast<uint32_t>(n) : 0u;
    const uint32_t hi_wrap = dup != 0 ? static_cast<uint32_t>(m) : 0u;

    // zig-zag zipper (the donor's ring merge): advance the side whose
    // fractional arc position lags — i*m <= j*n compares the arc fractions
    // exactly in integers. Equal counts alternate the quad diagonal.
    int i = 0, j = 0;
    while (i < n || j < m) {
      const bool adv_lo = (j >= m) || (i < n && i * m <= j * n);
      if (adv_lo) {
        cur.idx.push_back(static_cast<uint8_t>(lo + (i == n ? lo_wrap : i)));
        cur.idx.push_back(static_cast<uint8_t>(hi + j));
        cur.idx.push_back(static_cast<uint8_t>(lo + (i + 1 == n ? lo_wrap : i + 1)));
        ++i;
      } else {
        cur.idx.push_back(static_cast<uint8_t>(lo + (i == n ? lo_wrap : i)));
        cur.idx.push_back(static_cast<uint8_t>(hi + j));
        cur.idx.push_back(static_cast<uint8_t>(hi + (j + 1 == m ? hi_wrap : j + 1)));
        ++j;
      }
    }
    if (bottom_cap) {
      const uint32_t apex = static_cast<uint32_t>(cur.verts.size());
      // the apex sits at the ring CENTRE, which offset rings move
      int32_t ax0 = part.rings[0].cx, ay0 = part.rings[0].y, az0 = part.rings[0].cz;
      orient(ax0, ay0, az0);
      // a chain end cap follows its own end ring, not a part-wide bone
      cur.verts.push_back(SkinVertex{ax0, ay0, az0, part.chain ? part.rings[0].b0 : part.bone,
                                     part.chain ? part.rings[0].b1 : part.bone,
                                     part.chain ? part.rings[0].w0 : uint8_t{64},
                                     static_cast<uint8_t>(part.align), 0});
      cur.deform.push_back(deform_of(part.rings[0]));
      for (int k = 0; k < n; ++k) {
        cur.idx.push_back(static_cast<uint8_t>(lo + k));
        cur.idx.push_back(static_cast<uint8_t>(lo + (k + 1 == n ? lo_wrap : k + 1)));
        cur.idx.push_back(static_cast<uint8_t>(apex));
      }
    }
    if (top_cap) {
      const uint32_t apex = static_cast<uint32_t>(cur.verts.size());
      int32_t ax1 = part.rings[n_rings - 1].cx, ay1 = part.rings[n_rings - 1].y,
              az1 = part.rings[n_rings - 1].cz;
      orient(ax1, ay1, az1);
      cur.verts.push_back(SkinVertex{ax1, ay1, az1,
                                     part.chain ? part.rings[n_rings - 1].b0 : part.bone,
                                     part.chain ? part.rings[n_rings - 1].b1 : part.bone,
                                     part.chain ? part.rings[n_rings - 1].w0 : uint8_t{64},
                                     static_cast<uint8_t>(part.align), 255});
      cur.deform.push_back(deform_of(part.rings[n_rings - 1]));
      for (int k = 0; k < m; ++k) {
        cur.idx.push_back(static_cast<uint8_t>(hi + k));
        cur.idx.push_back(static_cast<uint8_t>(hi + (k + 1 == m ? hi_wrap : k + 1)));
        cur.idx.push_back(static_cast<uint8_t>(apex));
      }
    }
  }
  if (!cur.idx.empty()) {
    cur.page = part.page;
    cur.r = part.r;
    cur.g = part.g;
    cur.b = part.b;
    drop_identity_sidecar(cur);
    out.push_back(std::move(cur));
  }
  return out;
}

// --------------------------------------------------------------- compile ---

bool compile_creature(const Skeleton& sk, const ClipBank& bank,
                      const std::vector<RingPart>& parts, CreatureType& out,
                      const char** reason) {
  static const std::vector<PresentationMidpointAuthorship> kNoAuthorship;
  return compile_creature(sk, bank, parts, out, reason, kNoAuthorship);
}

bool compile_creature(
    const Skeleton& sk, const ClipBank& bank,
    const std::vector<RingPart>& parts, CreatureType& out, const char** reason,
    const std::vector<PresentationMidpointAuthorship>& midpoint_authorship) {
  if (reason) *reason = "ok";
  if (sk.bone_count == 0 || sk.bone_count > kMaxBones) {
    if (reason) *reason = "bone count";
    return false;
  }
  if (bank.bone_count != sk.bone_count) {
    if (reason) *reason = "clip bank bone_count mismatch";
    return false;
  }
  if (!bake_skeleton(sk, out.baked)) {
    if (reason) *reason = "skeleton order (parent-before-child)";
    return false;
  }
  out.skeleton = sk;

  // Authorship is compile-only provenance beside the bank. Validate it before
  // copying: a missing slot, duplicate declaration, wrong sample count or
  // unknown channel bit would otherwise silently preserve the wrong cache.
  for (size_t ai = 0; ai < midpoint_authorship.size(); ++ai) {
    const PresentationMidpointAuthorship& a = midpoint_authorship[ai];
    const Clip* owned_clip = nullptr;
    for (const Clip& c : bank.clips)
      if (c.slot_id == a.slot_id) owned_clip = &c;
    if (owned_clip == nullptr ||
        a.channels.size() != static_cast<size_t>(owned_clip->frame_count)) {
      if (reason) *reason = "midpoint authorship references a missing clip or has wrong count";
      return false;
    }
    for (size_t aj = 0; aj < ai; ++aj)
      if (midpoint_authorship[aj].slot_id == a.slot_id) {
        if (reason) *reason = "duplicate midpoint authorship slot";
        return false;
      }
    for (size_t k = 0; k < a.channels.size(); ++k) {
      const uint8_t mask = a.channels[k];
      if ((mask & ~(kMidpointQuatsAuthored | kMidpointRootAuthored |
                    kMidpointDeformAuthored)) != 0) {
        if (reason) *reason = "unknown midpoint authorship channel";
        return false;
      }
      if (((mask & kMidpointQuatsAuthored) != 0 &&
           owned_clip->mid_quats.size() < (k + 1) * bank.bone_count) ||
          ((mask & kMidpointRootAuthored) != 0 &&
           owned_clip->mid_root.size() < (k + 1) * 3) ||
          ((mask & kMidpointDeformAuthored) != 0 &&
           (owned_clip->deform.size() != owned_clip->frame_count ||
            owned_clip->mid_deform.size() <= k))) {
        if (reason) *reason = "owned midpoint channel is missing or malformed";
        return false;
      }
    }
  }

  out.bank = bank;
  // A1: regenerate every unowned presentation sample from the current keys.
  if (bank.bake60) {
    for (Clip& c : out.bank.clips) {
      const PresentationMidpointAuthorship* ownership = nullptr;
      for (const PresentationMidpointAuthorship& a : midpoint_authorship)
        if (a.slot_id == c.slot_id) ownership = &a;
      if (ownership != nullptr)
        bake_presentation_midpoints(c, bank.bone_count, ownership->channels);
      else
        bake_presentation_midpoints(c, bank.bone_count);
    }
  }

  // ---- C2: enforce the declared phase seams (bit-identical poses) --------
  for (const SeamPair& sp : bank.seams) {
    const Clip* a = nullptr;
    const Clip* b = nullptr;
    for (const Clip& c : bank.clips) {
      if (c.slot_id == sp.slot_a) a = &c;
      if (c.slot_id == sp.slot_b) b = &c;
    }
    if (a == nullptr || b == nullptr || sp.key_a >= a->frame_count || sp.key_b >= b->frame_count) {
      if (reason) *reason = "seam pair references a missing clip/key";
      return false;
    }
    const size_t bc = bank.bone_count;
    bool same = true;
    size_t bad_bone = 999;
    for (size_t k = 0; k < bc && same; ++k) {
      bad_bone = k;
      const quat16& qa = a->quats[static_cast<size_t>(sp.key_a) * bc + k];
      const quat16& qb = b->quats[static_cast<size_t>(sp.key_b) * bc + k];
      same = qa.q[0] == qb.q[0] && qa.q[1] == qb.q[1] && qa.q[2] == qb.q[2] && qa.q[3] == qb.q[3];
    }
    for (int k = 0; k < 3 && same; ++k)
      same = a->root[static_cast<size_t>(sp.key_a) * 3 + k] ==
             b->root[static_cast<size_t>(sp.key_b) * 3 + k];
    if (same) {
      const DeformSample da = a->deform.empty() ? DeformSample{} : a->deform[sp.key_a];
      const DeformSample db = b->deform.empty() ? DeformSample{} : b->deform[sp.key_b];
      same = da.flatten == db.flatten && da.spread == db.spread;
    }
    if (!same) {
      static char msg[96];
      std::snprintf(msg, sizeof(msg),
                    "phase seam mismatch (C2): slot %u key %u != slot %u key %u (bone %u)",
                    sp.slot_a, sp.key_a, sp.slot_b, sp.key_b, static_cast<unsigned>(bad_bone));
      if (reason) *reason = msg;
      return false;
    }
  }

  for (const RingPart& p : parts) {
    if (p.rings.size() < 2) {
      if (reason) *reason = "part needs >= 2 rings";
      return false;
    }
    if (p.bone >= sk.bone_count) {
      if (reason) *reason = "part bone index out of range";
      return false;
    }
    for (const RingSpec& rs : p.rings) {
      if (rs.segments < 3 || rs.segments > 32) {
        if (reason) *reason = "ring segments outside 3..32 (meshlet limit)";
        return false;
      }
      if (static_cast<uint8_t>(rs.deform_role) >
          static_cast<uint8_t>(DeformRole::kFollower)) {
        if (reason) *reason = "unknown deformation role";
        return false;
      }
      if (rs.deform_axis > 2) {
        if (reason) *reason = "deformation axis outside 0..2";
        return false;
      }
      if ((rs.deform_role == DeformRole::kNone) != (rs.deform_strength == 0)) {
        if (reason) *reason = "deformation role/strength mismatch";
        return false;
      }
      // CHAIN parts carry per-ring bones and a 1/64 weight. w1 = 64 - w0 is
      // structural, so weights cannot fail to normalise -- but the indices and
      // the weight range still have to be checked, and a chain part must not
      // also claim a part-wide bone.
      if (p.chain) {
        if (rs.b0 >= sk.bone_count || rs.b1 >= sk.bone_count) {
          if (reason) *reason = "chain ring bone index out of range";
          return false;
        }
        if (rs.w0 > 64) {
          if (reason) *reason = "chain ring w0 above 64 (weights are 1/64 quanta)";
          return false;
        }
      }
    }
    if (p.chain && (p.caps & ~(kCapTop | kCapBot)) != 0) {
      if (reason) *reason = "chain part caps outside {top, bot}";
      return false;
    }
  }

  // clips: array shapes + event validation (<=4 per frame, in range, sorted)
  for (const Clip& c : bank.clips) {
    if (c.quats.size() != static_cast<size_t>(c.frame_count) * bank.bone_count ||
        c.root.size() != static_cast<size_t>(c.frame_count) * 3 ||
        (!c.deform.empty() && c.deform.size() != c.frame_count)) {
      if (reason) *reason = "clip frame arrays";
      return false;
    }
    uint16_t per_frame_count = 0;
    int32_t last = -1;
    for (const ClipEvent& e : c.events) {
      if (e.frame >= c.frame_count) {
        if (reason) *reason = "event frame out of range";
        return false;
      }
      if (static_cast<int32_t>(e.frame) < last) {
        if (reason) *reason = "events not frame-sorted";
        return false;
      }
      if (static_cast<int32_t>(e.frame) != last) per_frame_count = 0;
      last = e.frame;
      if (++per_frame_count > 4) {
        if (reason) *reason = ">4 events on one frame";
        return false;
      }
    }
  }

  // full mesh
  out.mesh.clear();
  int64_t max_r2 = 0;
  for (const RingPart& p : parts) {
    for (Meshlet& m : build_ring_part(p)) {
      // RingPart vertices are authored in bone-local bind space. Store the
      // compiled payload in creature-global bind space so S=A*inv_rest keeps
      // the part attached at that bone in the identity pose instead of piling
      // every rigid part at the root.
      // RIGID parts are authored bone-locally and are lifted into
      // creature-global bind space here. CHAIN parts are ALREADY in
      // creature-global bind space -- they have no single bone to be local to,
      // which is the whole point -- so their offset is zero.
      const int32_t bx = p.chain ? 0 : out.baked.world_x[p.bone];
      const int32_t by = p.chain ? 0 : out.baked.world_y[p.bone];
      const int32_t bz = p.chain ? 0 : out.baked.world_z[p.bone];
      for (SkinVertex& v : m.verts) {
        v.x += bx;
        v.y += by;
        v.z += bz;
      }
      if (!m.deform.empty()) {
        if (m.deform.size() != m.verts.size()) {
          if (reason) *reason = "full deformation metadata count";
          return false;
        }
        for (DeformVertex& d : m.deform) {
          d.center_x += bx;
          d.center_y += by;
          d.center_z += bz;
          d.carrier_x += bx;
          d.carrier_y += by;
          d.carrier_z += bz;
        }
      }
      for (const SkinVertex& v : m.verts) {
        const int64_t r2 = static_cast<int64_t>(v.x) * v.x + static_cast<int64_t>(v.y) * v.y +
                           static_cast<int64_t>(v.z) * v.z;
        if (r2 > max_r2) max_r2 = r2;
      }
      out.mesh.push_back(std::move(m));
    }
  }
  if (out.mesh.empty()) {
    if (reason) *reason = "no meshlets";
    return false;
  }
  out.bound_radius = static_cast<int32_t>(isqrt_u64(static_cast<uint64_t>(max_r2)));

  // N2: smooth vertex normals over the finished full-rung meshlet set
  // (position-keyed across meshlet boundaries — see generate_smooth_normals)
  generate_smooth_normals(out.mesh);

  // micro rung: decimate (every 2nd ring kept — first and last always;
  // segments halved, min 3) and MEASURE the geometric error (charter 9:
  // compiler-generated LOD errors, never artist faces).
  out.micro.clear();
  int32_t micro_err = 0;
  for (const RingPart& p : parts) {
    RingPart d = p;
    d.rings.clear();
    for (size_t ri = 0; ri < p.rings.size(); ++ri) {
      const bool keep = p.micro_keep_rings || (ri % 2 == 0) || ri + 1 == p.rings.size();
      if (!keep) continue;
      RingSpec rs = p.rings[ri];
      if (!p.micro_keep_segments)
        rs.segments = static_cast<uint8_t>(rs.segments / 2 < 3 ? 3 : rs.segments / 2);
      d.rings.push_back(rs);
    }
    for (Meshlet& m : build_ring_part(d)) {
      // RIGID parts are authored bone-locally and are lifted into
      // creature-global bind space here. CHAIN parts are ALREADY in
      // creature-global bind space -- they have no single bone to be local to,
      // which is the whole point -- so their offset is zero.
      const int32_t bx = p.chain ? 0 : out.baked.world_x[p.bone];
      const int32_t by = p.chain ? 0 : out.baked.world_y[p.bone];
      const int32_t bz = p.chain ? 0 : out.baked.world_z[p.bone];
      for (SkinVertex& v : m.verts) {
        v.x += bx;
        v.y += by;
        v.z += bz;
      }
      if (!m.deform.empty()) {
        if (m.deform.size() != m.verts.size()) {
          if (reason) *reason = "micro deformation metadata count";
          return false;
        }
        for (DeformVertex& d : m.deform) {
          d.center_x += bx;
          d.center_y += by;
          d.center_z += bz;
          d.carrier_x += bx;
          d.carrier_y += by;
          d.carrier_z += bz;
        }
      }
      out.micro.push_back(std::move(m));
    }

    // error term 1: dropped rings — radius deviation from the linear blend
    // of the kept neighbours (lattice_lerp: the ONE shared lerp, 29-6)
    if (!p.micro_keep_rings) {
      for (size_t ri = 1; ri + 1 < p.rings.size(); ri += 2) {
        const RingSpec& a = p.rings[ri - 1];
        const RingSpec& b = p.rings[ri + 1];
        const RingSpec& mid = p.rings[ri];
        int32_t dev = 0;
        const int32_t den = b.y - a.y;
        if (den != 0) {
          const int32_t lerp = terrain::lattice_lerp(a.radius, b.radius, mid.y - a.y, den);
          dev = mid.radius > lerp ? mid.radius - lerp : lerp - mid.radius;
        } else {
          dev = mid.radius > a.radius ? mid.radius - a.radius : a.radius - mid.radius;
        }
        if (dev > micro_err) micro_err = dev;
      }
    }
    // error term 2: segment halving — chord vs arc: r * (1 - cos(pi/seg_new))
    if (!p.micro_keep_segments) {
      for (const RingSpec& rs : p.rings) {
        if (rs.segments < 6) continue;  // already coarse
        const uint8_t half = static_cast<uint8_t>(rs.segments / 2 < 3 ? 3 : rs.segments / 2);
        const int32_t c = fx_cos(angle16{static_cast<uint16_t>(0x8000 / half)}).raw;
        const int32_t dev = rescale_s32(static_cast<int64_t>(rs.radius) * (65536 - c), 16, nullptr);
        if (dev > micro_err) micro_err = dev;
      }
    }
  }
  // N2: micro-rung normals recomputed from MICRO topology, never copied
  generate_smooth_normals(out.micro);

  out.micro_error = micro_err;
  out.splat_error = out.bound_radius / 2;
  out.glint_error = out.bound_radius;
  return true;
}

// ------------------------------------------------------- the clamp gate ----

// ---------------------------------------------------------------------------
// THE CREATURE EXTENT LAW (owner ruling 2026-08-24 item 3)
// ---------------------------------------------------------------------------

RigidFault rigid_fault_of(const mat3x4fx& m, uint32_t tol_q16) {
  // Rows of the 3x3, in fx16. A rotation has unit rows that are mutually
  // perpendicular; scale breaks the first, shear the second, and the two are
  // reported apart because they are different notes to send an author.
  for (int r = 0; r < 3; ++r) {
    const int64_t a0 = m.m[r * 4 + 0], a1 = m.m[r * 4 + 1], a2 = m.m[r * 4 + 2];
    const int64_t n2 = a0 * a0 + a1 * a1 + a2 * a2;  // Q32.32
    const int64_t one = static_cast<int64_t>(1) << 32;
    // |n2 - 1| compared in Q32.32 against a tolerance expressed in Q16 units of
    // the SQUARED norm, which is what the ratified drift bound is stated in.
    const int64_t d = n2 > one ? n2 - one : one - n2;
    if (d > static_cast<int64_t>(tol_q16) << 16) return RigidFault::kRowNormNotUnit;
  }
  for (int r = 0; r < 3; ++r) {
    const int q = (r + 1) % 3;
    const int64_t d01 = static_cast<int64_t>(m.m[r * 4 + 0]) * m.m[q * 4 + 0] +
                        static_cast<int64_t>(m.m[r * 4 + 1]) * m.m[q * 4 + 1] +
                        static_cast<int64_t>(m.m[r * 4 + 2]) * m.m[q * 4 + 2];
    const int64_t ad = d01 < 0 ? -d01 : d01;
    if (ad > static_cast<int64_t>(tol_q16) << 16) return RigidFault::kRowsNotOrthogonal;
  }
  return RigidFault::kNone;
}

ExtentVerdict validate_extent(const std::vector<SourceVertex>& src, const Skeleton& sk,
                              const SkeletonBake& baked, const ClipBank& bank,
                              int32_t cumulative_scale) {
  ExtentVerdict v;
  (void)baked;

  // The scale bound is (0, 4]: a zero or negative scale is not a small
  // creature, it is a degenerate or mirrored one.
  v.worst_scale = cumulative_scale;
  if (cumulative_scale <= 0 || cumulative_scale > kMaxCumulativeScale) v.scale_reject = true;

  CreatureType probe{};
  probe.skeleton = sk;
  probe.bank = bank;
  if (!bake_skeleton(sk, probe.baked)) {
    v.radius_reject = true;
    return v;
  }

  SatLedger* L = nullptr;
  for (const Clip& c : bank.clips) {
    for (uint16_t f = 0; f < c.frame_count; ++f) {
      std::array<mat3x4fx, kMaxBones> pose{};
      decode_pose(probe, c, f, pose, nullptr);

      // Every bone that any vertex rides must be rigid. Checked per frame
      // because decode_pose composes the hierarchy: a rig can be rigid at bind
      // and acquire scale through a parent three frames in.
      for (uint16_t b = 0; b < sk.bone_count && v.rigid_fault == RigidFault::kNone; ++b) {
        const RigidFault rf = rigid_fault_of(pose[b]);
        if (rf != RigidFault::kNone) {
          v.rigid_fault = rf;
          v.rigid_fault_bone = b;
          v.worst_frame = f;
        }
      }

      for (uint32_t vi = 0; vi < src.size(); ++vi) {
        const SourceVertex& s = src[vi];
        int32_t x, y, z;
        skin_vertex(pose.data(), SkinVertex{s.x, s.y, s.z, s.b0, s.b1, s.w0}, x, y, z, L);
        const int64_t dx = x, dy = y, dz = z;  // relative to the root at origin
        const uint64_t d2 = static_cast<uint64_t>(dx * dx + dy * dy + dz * dz);
        const int32_t r = static_cast<int32_t>(isqrt_u64(d2));
        if (r > v.worst_radius) {
          v.worst_radius = r;
          v.worst_vertex = vi;
          v.worst_frame = f;
        }
      }
    }
  }

  if (v.worst_radius > kCreatureLocalRadius) v.radius_reject = true;
  return v;
}

ClampVerdict clamp_3to2(const std::vector<SourceVertex>& src, const Skeleton& sk,
                        const SkeletonBake& baked, const ClipBank& bank, int32_t bound_radius,
                        std::vector<SkinVertex>* out) {
  // Pre-decode every pose ONCE (the gate is compile-time; per-vertex loops
  // then cost only the skin evaluations). The caller's bake is reused when
  // valid, re-derived otherwise (same integers either way — it is exact).
  ClampVerdict v;
  if (out != nullptr) out->clear();

  CreatureType probe{};
  probe.skeleton = sk;
  probe.bank = bank;
  if (!bake_skeleton(sk, probe.baked)) {
    v.reject = true;
    return v;
  }
  (void)baked;

  struct PoseRef {
    const Clip* clip;
    uint16_t frame;
    std::array<mat3x4fx, kMaxBones> pose;
  };
  std::vector<PoseRef> poses;
  for (const Clip& c : bank.clips) {
    for (uint16_t f = 0; f < c.frame_count; ++f) {
      PoseRef pr;
      pr.clip = &c;
      pr.frame = f;
      decode_pose(probe, c, f, pr.pose, nullptr);
      poses.push_back(std::move(pr));
    }
  }

  SatLedger* L = nullptr;
  for (uint32_t vi = 0; vi < src.size(); ++vi) {
    const SourceVertex& s = src[vi];
    if (s.w0 + s.w1 + s.w2 != 64) {
      v.reject = true;  // malformed rig: weights not 1/64 quanta summing to 64
      continue;
    }
    // order by weight desc (ties keep declared order — deterministic);
    // the SMALLEST influence is the dropped one (creature_rules 3)
    uint8_t bi[3] = {s.b0, s.b1, s.b2};
    uint8_t wi[3] = {s.w0, s.w1, s.w2};
    for (int a = 0; a < 2; ++a)
      for (int b = a + 1; b < 3; ++b)
        if (wi[b] > wi[a]) {
          uint8_t t = wi[a];
          wi[a] = wi[b];
          wi[b] = t;
          t = bi[a];
          bi[a] = bi[b];
          bi[b] = t;
        }
    const int32_t sum2 = wi[0] + wi[1];
    if (sum2 == 0) {
      v.reject = true;  // degenerate: everything rides the dropped bone
      continue;
    }
    // renormalize: round-half-up, sum forced to exactly 64 on the LARGEST
    const int32_t n0 = static_cast<int32_t>((static_cast<int64_t>(wi[0]) * 64 + sum2 / 2) / sum2);
    const int32_t n1 = 64 - n0;
    if (static_cast<int64_t>(wi[0]) * 64 % sum2 != 0) ++v.renorm_adjusted;

    if (out != nullptr) {
      SkinVertex cv{s.x, s.y, s.z, bi[0], bi[1], static_cast<uint8_t>(n0), 0, 0};
      if (n1 == 0) cv.b1 = cv.b0;  // fully collapsed onto the largest
      out->push_back(cv);
    }

    // exact per-vertex drop error over every frame: w2 * |p3(f) - p12(f)|
    for (uint32_t pi = 0; pi < poses.size(); ++pi) {
      const mat3x4fx* pose = poses[pi].pose.data();
      int32_t x3, y3, z3, x12, y12, z12;
      skin_vertex(pose, SkinVertex{s.x, s.y, s.z, bi[2], bi[2], 64}, x3, y3, z3, L);
      skin_vertex(pose, SkinVertex{s.x, s.y, s.z, bi[0], bi[1], static_cast<uint8_t>(n0)}, x12, y12,
                  z12, L);
      const int64_t dx = x3 - x12, dy = y3 - y12, dz = z3 - z12;
      const uint64_t d2 = static_cast<uint64_t>(dx * dx + dy * dy + dz * dz);
      const int32_t err = rescale_s32(static_cast<int64_t>(wi[2]) * isqrt_u64(d2), 6, L);
      if (err > v.worst_err) {
        v.worst_err = err;
        v.worst_vertex = vi;
        v.worst_frame = poses[pi].frame;
      }
      (void)n1;
    }
  }
  const int64_t wr = v.worst_err;
  v.warn = wr * 100 > static_cast<int64_t>(bound_radius);
  v.reject = v.reject || wr * 100 > 3 * static_cast<int64_t>(bound_radius);
  return v;
}

}  // namespace creature
}  // namespace zref
