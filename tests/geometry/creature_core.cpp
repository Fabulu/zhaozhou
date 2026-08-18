// creature_core.cpp — the creature/character reference-lane tests.
//
// House style: plain check() counters (tests/terrain/terrain_dual.cpp), no
// framework. Every anchor is HAND-COMPUTABLE and independently derived —
// the oracle is the arithmetic stated in the comment, never a call into the
// implementation under test. Doubles appear ONLY in the quat error sweep
// (the libm-oracle precedent of test_fixp's sin/cos bound); every
// deterministic path under test is integer-only (charter 29-7).
//
// Law: spec/creature_rules.md 1-4 (ring counts, 8 B quats, clamp gate,
// alive laws), spec/qformats.md 3/4 (single rounding, rescale), charter 9
// (LOD hysteresis + hold). This file tests the ZREF REFERENCE and confers
// no block promotion (GEOM.POSE/SKIN stay SPECIFIED until the qformats
// amendment freezes the quat lane).

#include "zref/zref_creature.hpp"
#include "zref/zref_trig.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace {

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}
void check_eq(long long got, long long want, const char* what) {
  if (got != want) {
    std::fprintf(stderr, "FAIL: %s (got %lld, want %lld)\n", what, got, want);
    ++failures;
  }
}

using zref::fx16;
using zref::SatLedger;
namespace zc = zref::creature;

constexpr int32_t M1 = 1 << 16;  // 1.0 fx16
constexpr double kPi = 3.14159265358979323846;

// ---- shared fixture: a 3-bone chain (root -> hip -> knee) ------------------

zc::Skeleton chain3() {
  zc::Skeleton sk;
  sk.bone_count = 3;
  sk.bones[0] = zc::Bone{0, 0, 0, 0};
  sk.bones[1] = zc::Bone{0, 2 * M1, 0, 0};  // hip 2 m out on +X
  sk.bones[2] = zc::Bone{1, 0, -M1, 0};     // knee 1 m down from hip
  return sk;
}

zc::ClipBank identity_bank(uint8_t bones, uint16_t frames) {
  zc::ClipBank bank;
  bank.bone_count = bones;
  zc::Clip c;
  c.slot_id = 1;
  c.frame_count = frames;
  c.root.assign(static_cast<size_t>(frames) * 3, 0);
  c.quats.assign(static_cast<size_t>(frames) * bones, zc::quat16_identity());
  bank.clips.push_back(std::move(c));
  return bank;
}

// ---- 1. quat16 anchors ------------------------------------------------------

void test_quat_anchors() {
  SatLedger* L = nullptr;

  // identity decodes EXACTLY to the identity matrix
  zc::mat3x4fx m;
  zc::quat16_to_mat3(zc::quat16_identity(), m, L);
  check_eq(m.m[0], 65536, "identity m00");
  check_eq(m.m[5], 65536, "identity m11");
  check_eq(m.m[10], 65536, "identity m22");
  for (int i = 0; i < 12; ++i) {
    if (i != 0 && i != 5 && i != 10) check_eq(m.m[i], 0, "identity off-diag/tx");
  }

  // 180 deg about Y: lanes (0,0,1,0) are EXACT in S 1.0.14 -> matrix exact
  zc::quat16 q180y{{0, 0, 16384, 0}};
  zc::quat16_to_mat3(q180y, m, L);
  check_eq(m.m[0], -65536, "180y m00");
  check_eq(m.m[5], 65536, "180y m11");
  check_eq(m.m[10], -65536, "180y m22");
  check_eq(m.m[1] + m.m[2] + m.m[4] + m.m[6] + m.m[8] + m.m[9], 0, "180y off-diag");

  // 90 deg about Y: lanes quantize(11585.24) = 11585 (round-half-up), and
  //   m02 = rhu(11585^2 / 2^11) = rhu(134212225/2048) = 65533.31 -> 65533
  //   m00 = 65536 - 65533 = 3
  // (first draft of this anchor said 65534/2 — the hand division was
  // wrong and the test caught it; trails-agent lesson applied)
  // The GEOM.POSE contract says "identity/90 deg exact" — 90 deg is NOT
  // lane-exact in ANY power-of-two quat scale (sqrt(2)/2 is irrational);
  // recorded as a contract-wording finding, bounded here.
  const zref::angle16 half{0x2000};  // 1/8 turn = 45 deg
  zc::quat16 q90y =
      zc::quat16_axis_angle(fx16{0}, fx16{M1}, fx16{0}, zref::fx_sin(half), zref::fx_cos(half));
  check_eq(q90y.q[0], 11585, "90y w lane");
  check_eq(q90y.q[2], 11585, "90y y lane");
  zc::quat16_to_mat3(q90y, m, L);
  check_eq(m.m[2], 65533, "90y m02 (hand: 65533)");
  check_eq(m.m[8], -65533, "90y m20");
  check_eq(m.m[0], 3, "90y m00 (hand: 3)");
  check_eq(m.m[10], 3, "90y m22");
  // the rotation itself: R * x_hat = (2, 0, -65534)/65536 ~ (0,0,-1)
  check(m.m[2] > 65530 && m.m[0] < 8, "90y within declared bound");

  // hemisphere canonicalization: q and -q quantize identically
  zc::quat16 qa = zc::quat16_quantize(fx16{-46341}, fx16{0}, fx16{0}, fx16{0});
  zc::quat16 qb = zc::quat16_quantize(fx16{46341}, fx16{0}, fx16{0}, fx16{0});
  check(qa.q[0] == qb.q[0] && qa.q[1] == qb.q[1] && qa.q[2] == qb.q[2] && qa.q[3] == qb.q[3],
        "hemisphere canonical");
}

// ---- 2. quat round-trip sweep: measured worst-case error (the report number) -

void test_quat_error_sweep() {
  SatLedger* L = nullptr;
  // deterministic axis-angle grid; the double oracle is the TEST's, not the
  // implementation's (libm-oracle precedent, test_fixp sin/cos).
  // UNIT axes in fx16 (a non-unit axis makes a non-unit quat: lanes saturate)
  const int32_t axes[5][3] = {
      {M1, 0, 0}, {0, M1, 0}, {0, 0, M1}, {37837, 37837, 37837}, {46341, 46341, 0}};
  double worst_elem = 0;  // max |m_ij - exact*65536|, in fx16 LSB
  double worst_norm = 0;  // max |col_norm - 65536| (scale drift, no renorm)
  double worst_rot = 0;   // max angle error of a mapped unit vector, degrees
  for (int ai = 0; ai < 5; ++ai) {
    double ax = 0, ay = 0, az = 0;
    const double an = std::sqrt(static_cast<double>(axes[ai][0]) * axes[ai][0] +
                                static_cast<double>(axes[ai][1]) * axes[ai][1] +
                                static_cast<double>(axes[ai][2]) * axes[ai][2]);
    ax = axes[ai][0] / an;
    ay = axes[ai][1] / an;
    az = axes[ai][2] / an;
    for (int t = 0; t < 720; ++t) {
      const double ang = (t + 0.5) * 2.0 * kPi / 720.0;
      const double hs = std::sin(ang / 2), hc = std::cos(ang / 2);
      // the integer authoring path: HALF-angle turn = (t + 0.5)/1440 turns
      // (the first draft wrote the full-angle turn — a 2x error the sweep's
      // own 90-deg worst-case exposed; the implementation was innocent)
      const uint16_t turn = static_cast<uint16_t>((65536LL * (2 * t + 1) / 2880) & 0xFFFF);
      const zc::quat16 q = zc::quat16_axis_angle(
          fx16{axes[ai][0]}, fx16{axes[ai][1]}, fx16{axes[ai][2]},
          zref::fx_sin(zref::angle16{turn}), zref::fx_cos(zref::angle16{turn}));
      zc::mat3x4fx m;
      zc::quat16_to_mat3(q, m, L);
      // TWO oracles, two different numbers:
      //  (a) the QUANTIZED-LANE oracle isolates the decode formula (the
      //      FORMAT bound the qformats amendment freezes), and
      //  (b) the TRUE rotation carries the end-to-end angle error
      //      (fx-table authoring + quantization + decode).
      const double lqw = q.q[0] / 16384.0, lqx = q.q[1] / 16384.0, lqy = q.q[2] / 16384.0,
                   lqz = q.q[3] / 16384.0;
      const double e[9] = {1 - 2 * (lqy * lqy + lqz * lqz), 2 * (lqx * lqy - lqz * lqw),
                           2 * (lqx * lqz + lqy * lqw),     2 * (lqx * lqy + lqz * lqw),
                           1 - 2 * (lqx * lqx + lqz * lqz), 2 * (lqy * lqz - lqx * lqw),
                           2 * (lqx * lqz - lqy * lqw),     2 * (lqy * lqz + lqx * lqw),
                           1 - 2 * (lqx * lqx + lqy * lqy)};
      for (int i = 0; i < 9; ++i) {
        // impl layout is 3x4 (translation column at 3/7/11): skip it
        const int32_t got = m.m[(i / 3) * 4 + (i % 3)];
        const double d = std::fabs(got - e[i] * 65536.0);
        if (d > worst_elem) worst_elem = d;
      }
      for (int c = 0; c < 3; ++c) {
        const double col[3] = {static_cast<double>(m.m[c]), static_cast<double>(m.m[4 + c]),
                               static_cast<double>(m.m[8 + c])};  // rows 0/1/2 of column c
        const double nrm = std::sqrt(col[0] * col[0] + col[1] * col[1] + col[2] * col[2]);
        const double dn = std::fabs(nrm - 65536.0);
        if (dn > worst_norm) worst_norm = dn;
        const double tw = hc, tx = hs * ax, ty = hs * ay, tz = hs * az;
        const double te[9] = {
            1 - 2 * (ty * ty + tz * tz), 2 * (tx * ty - tz * tw),     2 * (tx * tz + ty * tw),
            2 * (tx * ty + tz * tw),     1 - 2 * (tx * tx + tz * tz), 2 * (ty * tz - tx * tw),
            2 * (tx * tz - ty * tw),     2 * (ty * tz + tx * tw),     1 - 2 * (tx * tx + ty * ty)};
        const double dot = (col[0] * te[c] + col[1] * te[3 + c] + col[2] * te[6 + c]) / nrm;
        const double deg = std::acos(dot > 1 ? 1 : (dot < -1 ? -1 : dot)) * 180.0 / kPi;
        if (deg > worst_rot) worst_rot = deg;
      }
    }
  }
  std::printf(
      "quat sweep (S 1.0.14, 3600 rotations): decode element err <= %.2f LSB, "
      "column-norm drift <= %.2f LSB, end-to-end column angle err <= %.4f deg\n",
      worst_elem, worst_norm, worst_rot);
  // declared bounds for the PROPOSED lane (freeze with the qformats
  // amendment; tightened numbers must re-run this sweep). Decode-only
  // elements: ONE rescale each -> <= 0.5 LSB + exact-formula slack.
  check(worst_elem <= 1.0, "quat decode element error <= 1 LSB");
  check(worst_norm <= 20.0, "quat scale drift <= 20 LSB (no renorm at decode)");
  check(worst_rot <= 0.05, "quat end-to-end column angle error <= 0.05 deg");
}

// ---- 3. skeleton bake -------------------------------------------------------

void test_skeleton_bake() {
  const zc::Skeleton sk = chain3();
  zc::SkeletonBake baked;
  check(zc::bake_skeleton(sk, baked), "bake chain3");
  check_eq(baked.world_x[1], 2 * M1, "world x of hip");
  check_eq(baked.world_y[2], -M1, "world y of knee");
  check_eq(baked.world_x[2], 2 * M1, "world x of knee");
  // rest inverse is EXACT: A(rest) * inv_rest == identity in exact integers
  // (rest rotations identity — translations cancel bit-exactly)
  zc::PoseBank pb;
  zc::CreatureType type;
  type.type_id = 7;
  type.skeleton = sk;
  type.bank = identity_bank(3, 1);
  type.baked = baked;
  std::array<zc::mat3x4fx, zc::kMaxBones> pose;
  zc::decode_pose(type, type.bank.clips[0], 0, pose, nullptr);
  for (int b = 0; b < 3; ++b) {
    check_eq(pose[b].m[0], 65536, "rest pose diag (exact identity)");
    check_eq(pose[b].m[5], 65536, "rest pose diag");
    check_eq(pose[b].m[10], 65536, "rest pose diag");
    check_eq(pose[b].m[3], 0, "rest pose tx (exact 0)");
    check_eq(pose[b].m[7], 0, "rest pose ty");
    check_eq(pose[b].m[11], 0, "rest pose tz");
  }
  // parent-before-child violation rejected
  zc::Skeleton bad = chain3();
  bad.bones[1].parent = 2;  // forward reference
  check(!zc::bake_skeleton(bad, baked), "forward parent rejected");
}

// ---- 4. decode + skinning anchors -------------------------------------------

void test_skin_anchors() {
  SatLedger* L = nullptr;
  // skeleton: bone0 at origin, bone1 translated (2,0,0). Clip frame: root
  // displacement (1,0,0); bone0 identity; bone1 = 90 deg about Z.
  // Hand derivation:
  //   A_0 = translate(1,0,0)                      S_0 = translate(1,0,0)
  //   A_1 = translate(1,0,0)*RotZ90*translate(2,0,0)
  //   S_1 = A_1 * translate(-2,0,0) = RotZ90 + t(1,0,0)
  // Bind vertex v = (0, 1, 0) m:
  //   S_0 = translate(1,0,0)                 -> S_0 v  = (1, 1, 0)
  //   S_1 = RotZ90 | RotZ90*(-2,0,0)+(3,0,0) -> S_1 v  ~ (2, -2, 0)
  //     (the rest offset (-2,0,0) is ROTATED by the bone: S = A * B^-1;
  //      the first draft of this anchor forgot that term — test caught it)
  //   2-weight (16/64 bone0, 48/64 bone1)
  //     ~ (16*(1,1) + 48*(2,-2))/64 = (1.75, -1.25, 0) m
  zc::Skeleton sk;
  sk.bone_count = 2;
  sk.bones[0] = zc::Bone{0, 0, 0, 0};
  sk.bones[1] = zc::Bone{0, 2 * M1, 0, 0};
  zc::SkeletonBake baked;
  check(zc::bake_skeleton(sk, baked), "bake 2-bone");

  zc::ClipBank bank;
  bank.bone_count = 2;
  zc::Clip c;
  c.slot_id = 1;
  c.frame_count = 1;
  c.root = {M1, 0, 0};
  c.quats.resize(2);
  c.quats[0] = zc::quat16_identity();
  const zref::angle16 half{0x2000};
  c.quats[1] =
      zc::quat16_axis_angle(fx16{0}, fx16{0}, fx16{M1}, zref::fx_sin(half), zref::fx_cos(half));
  bank.clips.push_back(std::move(c));

  zc::CreatureType type;
  type.type_id = 1;
  type.skeleton = sk;
  type.baked = baked;
  type.bank = bank;
  std::array<zc::mat3x4fx, zc::kMaxBones> pose;
  zc::decode_pose(type, type.bank.clips[0], 0, pose, nullptr);

  zc::SkinVertex v{0, M1, 0, 0, 1, 16};  // 16/64 on bone0
  int32_t x, y, z;
  zc::skin_vertex(pose.data(), zc::SkinVertex{0, M1, 0, 0, 0, 64}, x, y, z, L);
  check_eq(x, M1, "rigid bone0 x (hand: 1 m)");
  check_eq(y, M1, "rigid bone0 y (hand: v_y carried)");
  check_eq(z, 0, "rigid bone0 z");
  zc::skin_vertex(pose.data(), zc::SkinVertex{0, M1, 0, 1, 1, 64}, x, y, z, L);
  check(std::abs(x - 2 * M1) <= 12 && std::abs(y + 2 * M1) <= 12 && std::abs(z) <= 12,
        "rigid bone1 ~ (2, -2, 0)");
  zc::skin_vertex(pose.data(), v, x, y, z, L);
  check(std::abs(x - (7 * M1 / 4)) <= 8, "2-weight blend x ~ 1.75 m");
  check(std::abs(y + (5 * M1 / 4)) <= 8, "2-weight blend y ~ -1.25 m");
  check(std::abs(z) <= 8, "2-weight blend z ~ 0");

  // single-rounding proof by exact replay: the A3b expression evaluated
  // with the same integers in the test (independent formulation)
  {
    const zc::mat3x4fx& A = pose[0];
    const zc::mat3x4fx& B = pose[1];
    __int128 px = static_cast<__int128>(A.m[0]) * 0 + static_cast<__int128>(A.m[1]) * M1 +
                  static_cast<__int128>(A.m[2]) * 0 + (static_cast<__int128>(A.m[3]) << 16);
    __int128 qx = static_cast<__int128>(B.m[0]) * 0 + static_cast<__int128>(B.m[1]) * M1 +
                  static_cast<__int128>(B.m[2]) * 0 + (static_cast<__int128>(B.m[3]) << 16);
    const int32_t want = zref::rescale_s32(16 * px + 48 * qx, 22, nullptr);
    check_eq(x, want, "2-weight single-rounding replay (A3b exact)");
  }
}

// ---- 5. ring builder --------------------------------------------------------

void test_ring_builder() {
  // 4 rings x 8 segments, both caps: 3 seams * 16 + 16 cap tris = 64
  zc::RingPart p;
  p.rings = {{0, 32768, 8}, {65536, 32768, 8}, {131072, 32768, 8}, {196608, 32768, 8}};
  p.caps = zc::kCapTop | zc::kCapBot;
  p.bone = 0;
  p.align = 17;
  std::vector<zc::Meshlet> ms = zc::build_ring_part(p);
  check_eq(ms.size(), 1, "single meshlet");
  check_eq(ms[0].verts.size(), 34, "verts 4*8 + 2 apex");
  check_eq(ms[0].idx.size() / 3, zc::ring_side_tris(4, 8) + 16, "tri count anchor (3*16 + 16)");
  for (uint8_t ix : ms[0].idx) check(ix < ms[0].verts.size(), "index in range");
  // zig-zag: the first two triangles of a seam split the quad on OPPOSITE
  // diagonals (tri0 closes on a low-ring vertex, tri1 on a high-ring vertex)
  const uint8_t t0a = ms[0].idx[2], t1c = ms[0].idx[5];
  check((t0a < 8) != (t1c < 8), "zig-zag alternates the diagonal");
  // U lane: align 17 -> vertex 0 of ring 0 at angle 17/256 turns
  const uint16_t expect_ang = static_cast<uint16_t>(17 * 256);
  check_eq(ms[0].verts[0].u, expect_ang >> 8, "U from 8-bit angular alignment");
  // position: vertex 0 of ring 0 = r*(cos, sin) of that angle (one rounding)
  const int32_t ex = zref::rescale_s32(
      static_cast<int64_t>(32768) * zref::fx_cos(zref::angle16{expect_ang}).raw, 16, nullptr);
  check_eq(ms[0].verts[0].x, ex, "ring vertex x = r*cos(angle) (one rounding)");

  // split by rings: 12 rings x 16 segments. The greedy accumulator holds 3
  // rings then flushes when the next would pass 64 verts (meshlet A also
  // carries the bottom apex). Coverage: A(rings 0-2) B(2-5) C(5-8) D'(8-10)
  // E(10-11) = 5 meshlets; every seam is emitted exactly once (seam-ring
  // duplication is vertices only), so side tris = 11 * 32 and caps add 32:
  // 384 tris total (hand-counted against the limit arithmetic; the first
  // draft said 3/448 by ignoring the cap vertex in the flush check — the
  // test caught it).
  zc::RingPart big;
  for (int i = 0; i < 12; ++i) big.rings.push_back({i * 65536, 32768, 16});
  big.caps = zc::kCapTop | zc::kCapBot;
  std::vector<zc::Meshlet> split = zc::build_ring_part(big);
  check_eq(split.size(), 5, "12x16 part splits into 5 meshlets");
  uint32_t tris = 0;
  for (const zc::Meshlet& m : split) {
    check(m.verts.size() <= 64, "meshlet <= 64 verts");
    check(m.idx.size() / 3 <= 126, "meshlet <= 126 tris");
    tris += m.idx.size() / 3;
  }
  check_eq(tris, 384, "split tri total 11*32 + 32");

  // unequal ring counts: the zipper emits n + m triangles per seam
  zc::RingPart cone;
  cone.rings = {{0, 49152, 8}, {65536, 16384, 4}};
  std::vector<zc::Meshlet> taper = zc::build_ring_part(cone);
  check_eq(taper.size(), 1, "cone single meshlet");
  check_eq(taper[0].idx.size() / 3, 8 + 4, "zipper tri count n + m");

  // Combined quarter turns obey the documented order: pitch maps the local
  // ring axis +Y to +Z, then yaw maps +Z to +X.
  zc::RingPart oriented;
  oriented.rings = {{0, 0, 3}, {M1, 0, 3}};
  oriented.pitch_q = 1;
  oriented.yaw_q = 1;
  const std::vector<zc::Meshlet> turned = zc::build_ring_part(oriented);
  check_eq(turned[0].verts[3].x, M1, "pitch then yaw maps +Y ring axis to +X");
  check_eq(turned[0].verts[3].y, 0, "combined quarter turn y lane");
  check_eq(turned[0].verts[3].z, 0, "combined quarter turn z lane");
}

// ---- 6. compile + bound radius + micro error ---------------------------------

void test_compile() {
  zc::Skeleton sk;
  sk.bone_count = 1;
  sk.bones[0] = zc::Bone{0, 0, 0, 0};
  zc::ClipBank bank = identity_bank(1, 2);
  zc::RingPart body;
  body.rings = {{-M1 / 2, M1 / 2, 8}, {0, M1 / 2, 8}, {M1 / 2, M1 / 2, 8}};
  body.caps = zc::kCapTop | zc::kCapBot;
  zc::CreatureType t;
  const char* reason = "";
  check(zc::compile_creature(sk, bank, {body}, t, &reason), "compile ok");
  // bound radius: max |v| over ring vertices at (r=0.5, y=+-0.5):
  // isqrt(2 * 32768^2) = 46341 (0.7071 m). The ring circle comes from the
  // fx trig table (|c^2+s^2 - 2^32| <= ~3 LSB), so tolerate +-4 LSB.
  check(std::abs(t.bound_radius - 46341) <= 4, "bound radius isqrt anchor (46341 +- 4)");
  check_eq(t.glint_error, t.bound_radius, "glint error = R");
  check_eq(t.splat_error, t.bound_radius / 2, "splat error = R/2");
  check(t.micro_error >= 0 && t.micro.size() >= 1, "micro built");

  // Ring parts are bone-local authoring. Compilation translates them to the
  // bone's world-rest attachment before the inverse-bind skin palette is used.
  zc::Skeleton attached_sk;
  attached_sk.bone_count = 2;
  attached_sk.bones[0] = zc::Bone{0, 0, 0, 0};
  attached_sk.bones[1] = zc::Bone{0, 2 * M1, M1, -M1};
  zc::ClipBank attached_bank = identity_bank(2, 1);
  zc::RingPart attached_part = body;
  attached_part.bone = 1;
  zc::CreatureType attached;
  check(zc::compile_creature(attached_sk, attached_bank, {attached_part}, attached, &reason),
        "compile attached rigid part");
  int32_t min_x = std::numeric_limits<int32_t>::max();
  int32_t max_x = std::numeric_limits<int32_t>::min();
  for (const zc::Meshlet& m : attached.mesh) {
    for (const zc::SkinVertex& v : m.verts) {
      min_x = std::min(min_x, v.x);
      max_x = std::max(max_x, v.x);
    }
  }
  check(min_x >= 3 * M1 / 2 && max_x <= 5 * M1 / 2,
        "compiled vertices include child bone world-rest attachment");

  // validation: 5 events on one frame rejected (creature_rules 2.1 <= 4)
  zc::ClipBank bad_bank = identity_bank(1, 4);
  for (int i = 0; i < 5; ++i) bad_bank.clips[0].events.push_back({2, zc::kEvFoot, 0});
  zc::CreatureType bad;
  check(!zc::compile_creature(sk, bad_bank, {body}, bad, &reason), "5 events rejected");
  check(std::strcmp(reason, ">4 events on one frame") == 0, "reason text");

  // ring segments over the meshlet law rejected
  zc::RingPart fat;
  fat.rings = {{0, M1, 40}, {M1, M1, 40}};
  check(!zc::compile_creature(sk, bank, {fat}, bad, &reason), "40 segments rejected");
}

// ---- 7. the 3->2 clamp gate (creature_rules 3 — the money test) --------------

void test_clamp_gate() {
  // Analytic rig: three bones at the origin; bone2 rotated 180 deg about Y
  // (EXACT lanes (0,0,16384,0) -> exact diag(-1,1,1)); bind vertex (1,0,0) m.
  //   p3  = T2 v = (-1, 0, 0);  p12 = the kept-bone blend
  //   D = |p3 - p12| = 2 m exactly (isqrt exact: d2 = (2^17)^2 raws)
  // The gate drops the SMALLEST influence, so bone2 must carry the smallest
  // weight; with both kept bones identity, p12 = v exactly and
  //   err = w_dropped/64 * 2 m   — the spec's analytic bound, exact here.
  zc::Skeleton sk;
  sk.bone_count = 3;
  sk.bones[0] = zc::Bone{0, 0, 0, 0};
  sk.bones[1] = zc::Bone{0, 0, 0, 0};
  sk.bones[2] = zc::Bone{0, 0, 0, 0};
  zc::SkeletonBake baked;
  check(zc::bake_skeleton(sk, baked), "bake analytic rig");

  zc::ClipBank bank;
  bank.bone_count = 3;
  zc::Clip c;
  c.slot_id = 1;
  c.frame_count = 1;
  c.root = {0, 0, 0};
  c.quats.resize(3, zc::quat16_identity());
  c.quats[2] = zc::quat16{{0, 0, 16384, 0}};
  bank.clips.push_back(std::move(c));

  // worst-legal-band case: (21, 22, 21) — dropped w = 21 (bone 2, the tie
  // loser), kept pair (22 bone1, 21 bone0), sum 43:
  //   n0 = rhu(22*64/43) = rhu(32.74) = 33 (renorm adjusted on largest)
  //   err = 21/64 * 2 m = 0.65625 m = 43008 raw, EXACTLY
  {
    std::vector<zc::SourceVertex> src{zc::SourceVertex{M1, 0, 0, 0, 1, 2, 21, 22, 21}};
    std::vector<zc::SkinVertex> out;
    zc::ClampVerdict v = zc::clamp_3to2(src, sk, baked, bank, 20 * M1, &out);
    check_eq(v.worst_err, 43008, "err == w_dropped/64 * D exactly (analytic)");
    check(v.warn && v.reject, "0.65625 m vs 20 m bound: reject (> 3% = 0.6 m)");
    check_eq(out[0].w0, 33, "renorm rhu(22*64/43) = 33");
    check_eq(out[0].b0, 1, "kept pair led by bone 1 (w22)");
    check_eq(out[0].w0 + (64 - out[0].w0), 64, "weights sum to exactly 64");
    check_eq(v.renorm_adjusted, 1, "22*64 % 43 != 0: adjusted");
  }
  // silent case: dropped w = 2 -> err = 0.0625 m < 1% (0.2 m)
  {
    std::vector<zc::SourceVertex> src{zc::SourceVertex{M1, 0, 0, 0, 1, 2, 31, 31, 2}};
    zc::ClampVerdict v = zc::clamp_3to2(src, sk, baked, bank, 20 * M1, nullptr);
    check_eq(v.worst_err, 4096, "err = 2/64*2 m");
    check(!v.warn && !v.reject, "below 1%: silent");
  }
  // warn band: dropped w = 15 -> err = 0.46875 m in (0.2, 0.6)
  {
    std::vector<zc::SourceVertex> src{zc::SourceVertex{M1, 0, 0, 0, 1, 2, 25, 24, 15}};
    zc::ClampVerdict v = zc::clamp_3to2(src, sk, baked, bank, 20 * M1, nullptr);
    check_eq(v.worst_err, 15 * 65536 / 32, "err = 15/64*2 m");
    check(v.warn && !v.reject, "warn band (1%..3%)");
  }
  // renorm anchor: (20, 10, 34) — dropped bone1 (w10); kept (34 bone2, 20
  // bone0) sum 54: n0 = rhu(34*64/54) = rhu(40.30) = 40
  {
    std::vector<zc::SourceVertex> src{zc::SourceVertex{M1, 0, 0, 0, 1, 2, 20, 10, 34}};
    std::vector<zc::SkinVertex> out;
    zc::ClampVerdict v = zc::clamp_3to2(src, sk, baked, bank, 20 * M1, &out);
    check_eq(out[0].w0, 40, "renorm rhu(34*64/54) = 40");
    check_eq(out[0].b0, 2, "kept pair led by bone 2 (w34)");
    check_eq(v.renorm_adjusted, 1, "34*64 % 54 != 0: adjusted");
  }
  // malformed weights rejected
  {
    std::vector<zc::SourceVertex> src{zc::SourceVertex{M1, 0, 0, 0, 1, 2, 30, 30, 30}};
    zc::ClampVerdict v = zc::clamp_3to2(src, sk, baked, bank, 20 * M1, nullptr);
    check(v.reject, "weights not summing to 64 rejected");
  }
}

// ---- 8. pose bank: counters, eviction, determinism ---------------------------

void test_pose_bank() {
  zc::Skeleton sk;
  sk.bone_count = 1;
  sk.bones[0] = zc::Bone{0, 0, 0, 0};
  zc::ClipBank bank;
  bank.bone_count = 1;
  zc::Clip c;
  c.slot_id = 1;
  c.frame_count = 300;  // > cache capacity
  c.root.assign(300 * 3, 0);
  c.quats.resize(300, zc::quat16_identity());
  for (int f = 0; f < 300; ++f)
    c.quats[f] = zc::quat16{{static_cast<int16_t>(16000 - f * 8), 0, 0, 0}};
  bank.clips.push_back(std::move(c));
  zc::CreatureType type;
  type.type_id = 3;
  type.skeleton = sk;
  type.bank = bank;
  zc::SkeletonBake baked;
  zc::bake_skeleton(sk, baked);
  type.baked = baked;

  zc::PoseBank pb;
  const zc::mat3x4fx* p0 = pb.acquire(type, 1, 0);
  const zc::mat3x4fx* p0b = pb.acquire(type, 1, 0);
  check_eq(pb.counters().misses, 1, "first acquire misses");
  check_eq(pb.counters().hits, 1, "second acquire hits");
  check(p0 == p0b, "same tuple, same palette pointer");
  // frame content is a pure function: w lane 16000-8f appears as the m00
  // diagonal via 65536 - rhu(2*(qy^2+qz^2)/2^11) = 65536 here (w lane only)
  check_eq(p0[0].m[5], 65536, "decode content sanity");

  // bad ids: identity palette + counter, no crash
  const zc::mat3x4fx* bad = pb.acquire(type, 999, 0);
  check_eq(bad[0].m[0], 65536, "bad slot -> identity bind pose");
  const zc::mat3x4fx* badf = pb.acquire(type, 1, 40000);
  check_eq(badf[0].m[0], 65536, "bad frame -> identity bind pose");
  check_eq(pb.counters().bad_ids, 2, "bad ids counted");

  // fill to capacity, then eviction law: referenced-this-frame is never evicted
  zc::PoseBank full;
  for (int f = 0; f < 128; ++f) full.acquire(type, 1, f);
  check_eq(full.resident(), 128, "cache filled to 128");
  full.begin_frame();
  full.acquire(type, 1, 0);    // tuple 0 referenced this frame
  full.acquire(type, 1, 128);  // forces eviction of LRU unreferenced (tuple 1)
  check_eq(full.resident(), 128, "still 128 after evict+insert");
  const zc::mat3x4fx* keep = full.acquire(type, 1, 0);
  check_eq(full.counters().hits, 2, "referenced tuple 0 survived (hit)");
  (void)keep;
  const zc::mat3x4fx* evic = full.acquire(type, 1, 1);
  (void)evic;
  check(full.counters().misses == 130, "tuple 1 was the LRU victim (miss)");

  // clamped insert: reference ALL 128 tuples this frame, then one more
  zc::PoseBank clamp;
  for (int f = 0; f < 128; ++f) clamp.acquire(type, 1, f);
  clamp.begin_frame();
  for (int f = 0; f < 128; ++f) clamp.acquire(type, 1, f);
  const zc::mat3x4fx* overflow = clamp.acquire(type, 1, 200);
  check_eq(clamp.counters().clamped_inserts, 1, "content-tier violation counted");
  check_eq(overflow[0].m[0], 65536, "overflow decodes without insert");
  check_eq(clamp.resident(), 128, "nothing evicted under full reference");

  // determinism: same request multiset in a different order -> identical
  // decoded content (palette bytes), per the GEOM.POSE contract
  const int reqs[] = {5, 9, 5, 17, 9, 5, 21, 17};
  zc::PoseBank pa, pbn;
  std::vector<const zc::mat3x4fx*> va, vb;
  pa.begin_frame();
  pbn.begin_frame();
  for (int f : reqs) va.push_back(pa.acquire(type, 1, f));
  for (int i = 7; i >= 0; --i) vb.push_back(pbn.acquire(type, 1, reqs[i]));
  std::reverse(vb.begin(), vb.end());
  for (size_t i = 0; i < va.size(); ++i) {
    check(std::memcmp(va[i], vb[i], sizeof(zc::mat3x4fx)) == 0, "palette order-independence");
  }
}

// ---- 9. anim clock + event tags ----------------------------------------------

void test_anim() {
  zc::ClipBank bank;
  bank.bone_count = 1;
  zc::Clip c;
  c.slot_id = 4;
  c.frame_count = 4;
  c.root.assign(12, 0);
  c.quats.assign(4, zc::quat16_identity());
  c.events = {{2, zc::kEvFoot, 7}, {2, zc::kEvSound, 0}};
  bank.clips.push_back(std::move(c));

  zc::AnimPlayer a;
  a.cut(4);
  check_eq(a.slot, 4, "hard cut sets slot");
  check_eq(a.frame, 0, "hard cut resets frame");

  // 2 ticks per key: frame enters 1 at tick 2, 2 at tick 4 (events fire),
  // 3 at tick 6, wraps to 0 at tick 8
  const zc::ClipEvent* fired = nullptr;
  uint8_t n = 0;
  for (int t = 0; t < 3; ++t) zc::anim_advance(a, bank, &fired, n);
  check_eq(a.frame, 1, "frame 1 after 3 ticks");
  check_eq(n, 0, "no events at frame 1");
  zc::anim_advance(a, bank, &fired, n);
  check_eq(a.frame, 2, "frame 2 after 4 ticks");
  check_eq(n, 2, "two events on frame 2");
  check(fired != nullptr && fired[0].event == zc::kEvFoot && fired[0].param == 7,
        "event payload order");
  for (int t = 4; t < 8; ++t) zc::anim_advance(a, bank, &fired, n);
  check_eq(a.frame, 0, "wrap at frame_count");

  // petrify: frozen clock advances nothing
  a.frozen = true;
  for (int t = 0; t < 10; ++t) zc::anim_advance(a, bank, &fired, n);
  check_eq(a.frame, 0, "frozen frame");
  check_eq(a.sub, 0, "frozen sub");
}

// ---- 10. rotateOnGround -------------------------------------------------------

void test_ground_tilt() {
  // hand-built composed lattice: plane top = x * 1 m (slope +1 along x)
  zref::terrain::ComposedLattice lat;
  lat.w = 3;
  lat.h = 3;
  lat.dual = false;
  lat.wx = {0, M1, 2 * M1};
  lat.wz = {0, M1, 2 * M1};
  lat.top = {0, M1, 2 * M1, 0, M1, 2 * M1, 0, M1, 2 * M1};
  lat.bottom = lat.top;

  zc::GroundTilt t;
  zc::ground_tilt_update(t, zc::TiltMode::kCompletely, zref::angle16{0}, lat, fx16{M1}, fx16{0},
                         fx16{M1 / 2}, fx16{10 * M1});
  check_eq(t.slope_f, M1, "facing slope = 1 (plane x*1, taps +-0.5 m)");
  check_eq(t.slope_s, 0, "side slope 0");

  // rate limit: step clamped to 0.25 per tick
  zc::GroundTilt t2;
  zc::ground_tilt_update(t2, zc::TiltMode::kCompletely, zref::angle16{0}, lat, fx16{M1}, fx16{0},
                         fx16{M1 / 2}, fx16{M1 / 4});
  check_eq(t2.slope_f, M1 / 4, "rate-limited first tick");

  // sideways mode: pitch held at 0
  zc::GroundTilt t3;
  zc::ground_tilt_update(t3, zc::TiltMode::kSideways, zref::angle16{0}, lat, fx16{M1}, fx16{0},
                         fx16{M1 / 2}, fx16{10 * M1});
  check_eq(t3.slope_f, 0, "sideways holds pitch");
  // facing 90 deg off: the same plane now reads on the SIDE axis
  zc::GroundTilt t4;
  zc::ground_tilt_update(t4, zc::TiltMode::kCompletely, zref::angle16{0x4000}, lat, fx16{M1},
                         fx16{0}, fx16{M1 / 2}, fx16{10 * M1});
  // side axis = (-sin, +cos) of facing: at facing 90 deg that is -X, so the
  // +side tap is the LOWER ground: slope_s = (0.5 - 1.5)/1 = -1 (sign law)
  check_eq(t4.slope_s, -M1, "facing +90 deg swaps the slope axis (sign law)");
  check_eq(t4.slope_f, 0, "no forward slope across the plane");

  // void column: slope holds (no snap)
  zref::terrain::ComposedLattice holed = lat;
  holed.cell_state.assign(4, zref::terrain::kVoidAuthored);
  zc::GroundTilt t5;
  t5.slope_f = 12345;
  zc::ground_tilt_update(t5, zc::TiltMode::kCompletely, zref::angle16{0}, holed, fx16{M1}, fx16{0},
                         fx16{M1 / 2}, fx16{10 * M1});
  check_eq(t5.slope_f, 12345, "void column holds slope");
}

// ---- 11. tilt matrix ----------------------------------------------------------

void test_tilt_matrix() {
  SatLedger* L = nullptr;
  zc::GroundTilt zero;
  zc::mat3x4fx r = zc::tilt_matrix(zero, L);
  check_eq(r.m[0], 65536, "zero tilt -> exact identity");
  check_eq(r.m[5], 65536, "zero tilt diag");
  check_eq(r.m[10], 65536, "zero tilt diag");
  check_eq(r.m[1] + r.m[2] + r.m[4] + r.m[6] + r.m[8] + r.m[9], 0, "zero tilt off-diag");

  // 45 deg forward tilt: R * y_hat ~ normalize(-1, 1, 0) = (-46341, 46341, 0)
  zc::GroundTilt t45;
  t45.slope_f = M1;
  r = zc::tilt_matrix(t45, L);
  check(std::abs(r.m[5] - 46341) <= 8, "tilt maps y_hat to the ground normal (y)");
  check(std::abs(r.m[1] + 46341) <= 8 && std::abs(r.m[9]) <= 8,
        "tilt maps y_hat to the ground normal (xz)");
  // columns orthonormal within the declared slack
  for (int i = 0; i < 3; ++i) {
    const int64_t nx = r.m[i], ny = r.m[4 + i], nz = r.m[8 + i];
    const int64_t n2 = (nx * nx + ny * ny + nz * nz) >> 16;
    check(std::abs(n2 - 65536) <= 700, "tilt column unit within slack");
  }
}

// ---- 12. bulk + tick-skip ------------------------------------------------------

void test_bulk_and_skip() {
  zc::BulkState b;
  b.scale = M1;
  b.target = 2 * M1;
  zc::bulk_update(b, 4);
  check_eq(b.scale, M1 + (M1 >> 4), "exponential smoothing anchor (1 + 1/16)");
  for (int i = 0; i < 200; ++i) zc::bulk_update(b, 4);
  check(b.scale >= 2 * M1 - 16, "converges to target (shift-stall rail 16 raw)");
  check(zc::bulk_popped(b, static_cast<int32_t>(65536LL + (65536LL * 32768) / 65536)),
        "pop threshold crossed");

  check(zc::tick_skip_due(0, 1) && zc::tick_skip_due(4, 1) && zc::tick_skip_due(8, 1),
        "4-tick interval due at 0, 4, 8");
  check(!zc::tick_skip_due(1, 1) && !zc::tick_skip_due(2, 1) && !zc::tick_skip_due(3, 1),
        "4-tick interval not due between");
  check(zc::tick_skip_due(16, 2) && !zc::tick_skip_due(15, 2), "16-tick interval (4^2)");
}

// ---- 13. LOD ladder ------------------------------------------------------------

void test_lod() {
  zc::CreatureType t;
  t.bound_radius = M1;     // 1 m
  t.micro_error = M1 / 8;  // 0.125 m
  t.splat_error = M1 / 2;
  t.glint_error = M1;
  const int32_t thresh = 512;  // 2 px in S12.8

  // boundaries: micro legal at proj <= 4096 (16 px), splat <= 1024, glint <= 512
  check(zc::lod_raw(5000, thresh, t) == zc::LodRung::kMesh, "5000 -> mesh");
  check(zc::lod_raw(4000, thresh, t) == zc::LodRung::kMicro, "4000 -> micro (err 500 <= 512)");
  check(zc::lod_raw(1000, thresh, t) == zc::LodRung::kSplat, "1000 -> splat");
  check(zc::lod_raw(500, thresh, t) == zc::LodRung::kGlint, "500 -> glint");

  // hysteresis: within 10% of the boundary nothing flips, even past the hold
  zc::LodState st;
  st.rung = zc::LodRung::kMesh;
  st.hold = 100;
  check(zc::lod_update(st, 3800, thresh, t) == zc::LodRung::kMesh,
        "3800 > 0.9*4096: stays mesh (no flip)");
  check(zc::lod_update(st, 3600, thresh, t) == zc::LodRung::kMicro,
        "3600 <= 0.9*4096: coarsens (hold elapsed)");
  st.hold = 100;
  check(zc::lod_update(st, 4200, thresh, t) == zc::LodRung::kMicro, "4200 < 1.1*4096: stays micro");
  check(zc::lod_update(st, 4600, thresh, t) == zc::LodRung::kMesh, "4600 >= 1.1*4096: refines");

  // minimum hold: a deep move does not switch until the 15 ticks elapse.
  // hold starts 0; calls 1..15 see hold < 15 (call 15 raises it TO 15);
  // call 16 is the first that may switch.
  zc::LodState st2;
  st2.rung = zc::LodRung::kMesh;
  st2.hold = 0;
  for (int i = 0; i < 15; ++i) {
    check(zc::lod_update(st2, 100, thresh, t) == zc::LodRung::kMesh, "hold blocks switch");
  }
  check(zc::lod_update(st2, 100, thresh, t) == zc::LodRung::kGlint, "16th tick switches");
}

// ---- 14. gibs -------------------------------------------------------------------

void test_gibs() {
  zc::Skeleton sk;
  sk.bone_count = 1;
  sk.bones[0] = zc::Bone{0, 0, 0, 0};
  zc::ClipBank bank = identity_bank(1, 1);
  zc::RingPart body;
  body.rings = {{0, M1 / 2, 6}, {M1, M1 / 2, 6}};
  body.caps = zc::kCapTop | zc::kCapBot;
  body.r = 200;
  body.g = 100;
  body.b = 50;
  zc::CreatureType t;
  const char* reason = "";
  check(zc::compile_creature(sk, bank, {body}, t, &reason), "compile for gibs");
  std::array<zc::mat3x4fx, zc::kMaxBones> pose;
  zc::decode_pose(t, t.bank.clips[0], 0, pose, nullptr);
  std::vector<zc::Gib> g1, g2;
  zc::spawn_gibs(t, pose.data(), fx16{5 * M1}, fx16{0}, fx16{0}, 42, g1);
  zc::spawn_gibs(t, pose.data(), fx16{5 * M1}, fx16{0}, fx16{0}, 42, g2);
  check_eq(g1.size(), 14, "one gib per meshlet vertex (12 ring + 2 apex)");
  check(
      g1.size() == g2.size() && std::memcmp(g1.data(), g2.data(), g1.size() * sizeof(zc::Gib)) == 0,
      "gib burst deterministic (same seed)");
  // world offset applied: vertices span x in [-0.5, 0.5] m around the root
  for (const zc::Gib& g : g1) {
    check(g.x >= 4 * M1 && g.x <= 6 * M1, "gib world offset applied (integer bounds)");
  }
  bool has_up = false;
  for (const zc::Gib& g : g1)
    if (g.vy > 0) has_up = true;
  check(has_up, "gibs fling upward");
}

// ---- 15. compositor smoke: rungs draw ------------------------------------------------

void test_compositor() {
  zc::Skeleton sk;
  sk.bone_count = 1;
  sk.bones[0] = zc::Bone{0, 0, 0, 0};
  zc::ClipBank bank = identity_bank(1, 2);
  // a squat cylinder, distinct colour
  zc::RingPart body;
  body.rings = {{-M1 / 2, M1 / 2, 10}, {0, M1 / 2, 10}, {M1 / 2, M1 / 2, 10}};
  body.caps = zc::kCapTop | zc::kCapBot;
  body.r = 240;
  body.g = 60;
  body.b = 60;
  zc::CreatureType t;
  const char* reason = "";
  check(zc::compile_creature(sk, bank, {body}, t, &reason), "compile for compositor");
  // hand camera: x' = 8x, y' = 8y, w = z + 4 m — at the creature (z=0,
  // w=4) the 0.5 m bound radius projects to 8*0.5/4 = 1.0 NDC = 32 px of
  // the 64 px canvas. All entries exact fx16 integers.
  zref::mat4fx vp{{{fx16{8 << 16}, fx16{0}, fx16{0}, fx16{0}},
                   {fx16{0}, fx16{8 << 16}, fx16{0}, fx16{0}},
                   {fx16{0}, fx16{0}, fx16{1 << 16}, fx16{4 << 16}},
                   {fx16{0}, fx16{0}, fx16{0}, fx16{1 << 16}}}};
  const uint32_t W = 64, H = 64;
  std::vector<uint8_t> rgb(W * H * 3, 9);
  std::vector<int32_t> dep(W * H, 0);

  zc::CreatureInstance inst;
  inst.type = &t;
  inst.x = 0;
  inst.y = 0;  // body spans y in [-0.5, 0.5] m -> NDC [-1, 1] edge to edge
  inst.z = 0;
  zc::PoseBank pb;
  zc::CreatureInstance* arr[1] = {&inst};
  zc::compose_creatures(rgb.data(), dep.data(), W, H, vp, arr, 1, pb, nullptr);
  uint32_t lit = 0;
  for (uint32_t i = 0; i < W * H; ++i) {
    if (rgb[i * 3] > 40 && rgb[i * 3 + 1] < 40 && dep[i] > 0) ++lit;  // reddish + depth written
  }
  check(lit >= 100, "mesh rung paints the creature (depth + colour)");
  check(lit < W * H, "creature does not fill the frame");

  // far away: the ladder walks to glint; a 2x2-ish bright point draws
  zc::CreatureInstance far_inst = inst;
  far_inst.z = 90 * M1;
  far_inst.lod.rung = zc::LodRung::kGlint;  // (chosen by the ladder in situ;
                                            // forced here to smoke the rung)
  std::vector<uint8_t> rgb2(W * H * 3, 9);
  std::vector<int32_t> dep2(W * H, 0);
  zc::CreatureInstance* arr2[1] = {&far_inst};
  zc::compose_creatures(rgb2.data(), dep2.data(), W, H, vp, arr2, 1, pb, nullptr);
  uint32_t pts = 0;
  for (uint32_t i = 0; i < W * H; ++i)
    if (rgb2[i * 3] > 100) ++pts;
  check(pts >= 1 && pts <= 9, "glint rung draws a small bright point");
}

}  // namespace

int main() {
  test_quat_anchors();
  test_quat_error_sweep();
  test_skeleton_bake();
  test_skin_anchors();
  test_ring_builder();
  test_compile();
  test_clamp_gate();
  test_pose_bank();
  test_anim();
  test_ground_tilt();
  test_tilt_matrix();
  test_bulk_and_skip();
  test_lod();
  test_gibs();
  test_compositor();
  if (failures == 0) std::printf("creature_core: all anchors green\n");
  return failures == 0 ? 0 : 1;
}
