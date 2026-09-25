// manafold_eyesize.cpp -- Pass 17 eye form/size infrastructure gate.
//
// Proves the identity-default optional track, independent L/R authored values,
// Eye->Pupil hierarchy propagation and exact legacy/current form control before
// any new public art consumes the channel.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_texture.hpp"
#include "render_helpers.hpp"
#include "zrender/internal.hpp"

namespace zc = zref::creature;
#include "manafold.h"

namespace {

constexpr uint16_t kLargeQ15 = 49152;  // 1.5
constexpr uint16_t kSmallQ15 = 24576;  // 0.75

bool ring_equal(const zc::RingSpec& a, const zc::RingSpec& b) {
  if (a.y != b.y || a.radius != b.radius || a.rx != b.rx || a.rz != b.rz ||
      a.cx != b.cx || a.cz != b.cz || a.segments != b.segments ||
      a.b0 != b.b0 || a.b1 != b.b1 || a.w0 != b.w0 ||
      a.deform_role != b.deform_role || a.deform_axis != b.deform_axis ||
      a.deform_strength != b.deform_strength ||
      a.deform_center_x != b.deform_center_x ||
      a.deform_center_y != b.deform_center_y ||
      a.deform_center_z != b.deform_center_z)
    return false;
  for (size_t lane = 0; lane + 1 < zc::kDeformLaneCount; ++lane)
    if (a.deform_strength_ex[lane] != b.deform_strength_ex[lane]) return false;
  return true;
}

bool part_equal(const zc::RingPart& a, const zc::RingPart& b) {
  if (a.bone != b.bone || a.rings.size() != b.rings.size() || a.caps != b.caps ||
      a.align != b.align || a.chain != b.chain ||
      a.micro_keep_rings != b.micro_keep_rings ||
      a.micro_keep_segments != b.micro_keep_segments ||
      a.r != b.r || a.g != b.g || a.b != b.b || a.page != b.page ||
      a.v0 != b.v0 || a.v1 != b.v1 || a.pitch_q != b.pitch_q ||
      a.yaw_q != b.yaw_q || a.cap_base_fix != b.cap_base_fix)
    return false;
  for (size_t i = 0; i < a.rings.size(); ++i)
    if (!ring_equal(a.rings[i], b.rings[i])) return false;
  return true;
}

zc::Clip scale_clip(u02::EyeSizeMute mute, bool wrong_bone = false) {
  u02::g_u02_eye_size_mute = mute;
  zc::Clip c = u02::clip_shell(60, 2, u02::kHoverHeightMm);
  u02::enable_eye_scale_track(c);
  for (int f = 0; f < 2; ++f) {
    u02::Rig g;
    g.reset();
    const int32_t l = f == 0 ? 1000 : 1500;
    const int32_t r = f == 0 ? 1000 : 750;
    if (!g.set_eye_scale_pm(l, r)) {
      std::fprintf(stderr, "meyesize: internal scale tuple rejected\n");
      return {};
    }
    if (wrong_bone && f == 1) {
      g.scale_q15[u02::kBPupilL] = g.scale_q15[u02::kBEyeL];
      g.scale_q15[u02::kBEyeL] = u02::kEyeScaleIdentityQ15;
    }
    u02::face_rest(g);
    g.write(c, f);
  }
  zc::bake_presentation_midpoints(c, u02::kBoneCount);
  u02::g_u02_eye_size_mute = u02::EyeSizeMute::kNone;
  return c;
}

int rigid_vertex_count(const zc::CreatureType& t, uint8_t bone) {
  int n = 0;
  for (const zc::Meshlet& m : t.mesh)
    for (const zc::SkinVertex& v : m.verts)
      if ((v.b0 == bone && v.w0 == 64) || (v.b1 == bone && v.w0 == 0)) ++n;
  return n;
}

}  // namespace

int main(int argc, char** argv) {
  // PASS 20 PACKET 5: the shared dip/dent knob parser, so this binary cannot
  // be blind to a knob a ladder is being run against (see apply_knead_dip_env).
  if (!u02::apply_knead_dip_env()) return 2;
  enum class Mode { kNormal, kMuteL, kMuteR, kWrongBone } mode = Mode::kNormal;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--fail-mute") == 0 && i + 1 < argc) {
      const char* v = argv[++i];
      if (std::strcmp(v, "L") == 0) mode = Mode::kMuteL;
      else if (std::strcmp(v, "R") == 0) mode = Mode::kMuteR;
      else {
        std::fprintf(stderr, "meyesize: --fail-mute expects L|R\n");
        return 2;
      }
    } else if (std::strcmp(argv[i], "--fail-wrong-bone") == 0) {
      mode = Mode::kWrongBone;
    } else {
      std::fprintf(stderr, "meyesize: unknown argument %s\n", argv[i]);
      return 2;
    }
  }

  int failures = 0;
  const zc::CreatureType& t = u02::type();
  for (const zc::Clip& c : t.bank.clips) {
    // PASS 26: the list moved to u02::eye_expression_slot() in manafold_clips.h.
    // It used to live here ALONE while the clips each called
    // enable_eye_scale_track() in their own source -- two copies of one fact,
    // and this gate correctly went red the first time one of them moved
    // (hasty, Direction 27). One definition, read by the gate and the clips.
    const bool expression_slot = u02::eye_expression_slot(c.slot_id);
    const size_t expected = static_cast<size_t>(c.frame_count) * u02::kBoneCount;
    if (expression_slot) {
      if (c.uniform_scale_q15.size() != expected ||
          std::all_of(c.uniform_scale_q15.begin(), c.uniform_scale_q15.end(),
                      [](uint16_t v) { return v == u02::kEyeScaleIdentityQ15; })) {
        std::printf("FAIL: expression slot %u lacks an active eye-scale track\n", c.slot_id);
        ++failures;
      }
    } else if (!c.uniform_scale_q15.empty()) {
      std::printf("FAIL: non-expression slot %u unexpectedly allocates eye scale\n", c.slot_id);
      ++failures;
    }
  }

  const u02::EyeForm saved_current = u02::selected_eye_form();
  const zc::RingPart current_l = u02::make_eye_lens(u02::kBEyeL);
  u02::g_u02_eye_form_legacy = true;
  const zc::RingPart legacy_l = u02::make_eye_lens(u02::kBEyeL);
  u02::g_u02_eye_form_legacy = false;
  if (!u02::eye_form_valid(saved_current) || part_equal(current_l, legacy_l)) {
    std::printf("FAIL: selected current form is invalid or did not leave the legacy dagger\n");
    ++failures;
  }

  if (rigid_vertex_count(t, u02::kBEyeL) == 0 ||
      rigid_vertex_count(t, u02::kBEyeR) == 0 ||
      rigid_vertex_count(t, u02::kBPupilL) == 0 ||
      rigid_vertex_count(t, u02::kBPupilR) == 0) {
    std::printf("FAIL: lens/star ownership is missing from compiled visible skin\n");
    ++failures;
  }

  const u02::EyeSizeMute mute =
      mode == Mode::kMuteL ? u02::EyeSizeMute::kLeft
      : mode == Mode::kMuteR ? u02::EyeSizeMute::kRight
                             : u02::EyeSizeMute::kNone;
  zc::Clip c = scale_clip(mute, mode == Mode::kWrongBone);
  const size_t want = static_cast<size_t>(c.frame_count) * u02::kBoneCount;
  if (c.uniform_scale_q15.size() != want || c.mid_uniform_scale_q15.size() != want) {
    std::printf("FAIL: eye scale track shape is wrong\n");
    ++failures;
  }

  const size_t key = u02::kBoneCount;
  const uint16_t got_l = c.uniform_scale_q15[key + u02::kBEyeL];
  const uint16_t got_r = c.uniform_scale_q15[key + u02::kBEyeR];
  const uint16_t want_l = mode == Mode::kMuteL || mode == Mode::kWrongBone
                              ? u02::kEyeScaleIdentityQ15
                              : kLargeQ15;
  const uint16_t want_r = mode == Mode::kMuteR ? u02::kEyeScaleIdentityQ15 : kSmallQ15;
  if (got_l != want_l || got_r != want_r) {
    std::printf("FAIL: independent L/R authored scales got %u/%u want %u/%u\n",
                got_l, got_r, want_l, want_r);
    ++failures;
  }
  for (uint8_t b = 0; b < u02::kBoneCount; ++b)
    if (b != u02::kBEyeL && b != u02::kBEyeR &&
        c.uniform_scale_q15[key + b] != u02::kEyeScaleIdentityQ15 &&
        !(mode == Mode::kWrongBone && b == u02::kBPupilL)) {
      std::printf("FAIL: scale leaked to bone %u\n", b);
      ++failures;
    }

  std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
  zc::decode_pose(t, c, 1, pose, nullptr, 0);
  const int32_t left_basis = pose[u02::kBEyeL].m[0];
  const int32_t right_basis = pose[u02::kBEyeR].m[0];
  constexpr int kBasis[9] = {0, 1, 2, 4, 5, 6, 8, 9, 10};
  const auto same_basis = [&](uint8_t a, uint8_t b) {
    for (int i : kBasis)
      if (pose[a].m[i] != pose[b].m[i]) return false;
    return true;
  };
  const bool left_registered = same_basis(u02::kBEyeL, u02::kBPupilL);
  const bool right_registered = same_basis(u02::kBEyeR, u02::kBPupilR);
  std::printf("eye scale: L/R q15 %u/%u basis %d/%d registration %s/%s\n",
              got_l, got_r, left_basis, right_basis,
              left_registered ? "OK" : "BROKEN",
              right_registered ? "OK" : "BROKEN");

  if (mode == Mode::kWrongBone) {
    if (!left_registered && right_registered) {
      std::printf("RED LEG OK: wrong-bone scale fails only left Eye->Pupil registration\n");
      return 0;
    }
    std::fprintf(stderr, "meyesize: wrong-bone red leg did not isolate registration fault\n");
    return 1;
  }
  if (!left_registered || !right_registered) {
    std::printf("FAIL: Eye scale did not carry both Pupil child bases\n");
    ++failures;
  }

  if (!u02::Rig{}.set_eye_scale_pm(1000, 1000)) {
    std::printf("FAIL: identity per-mille scale rejected\n");
    ++failures;
  }
  u02::Rig invalid;
  invalid.reset();
  if (invalid.set_eye_scale_pm(0, 1000)) {
    std::printf("FAIL: invalid zero per-mille scale accepted\n");
    ++failures;
  }

  if (mode == Mode::kMuteL || mode == Mode::kMuteR) {
    if (failures != 0) return 1;
    std::printf("RED LEG OK: mute %s restores only its eye to identity\n",
                mode == Mode::kMuteL ? "L" : "R");
    return 0;
  }

  std::printf("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}
