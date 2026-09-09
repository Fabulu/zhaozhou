// manafold-eyecam -- THE COMMITTED EYE-VS-CAMERA PROBE (pass 15, LANE-EYE).
//
// WHY THIS EXISTS. Three passes built an eye-travel channel, measured it in
// its OWN space (a carrier angle in per-mille or degrees), reported it
// working, and the owner said "they don't move left and right at all". Every
// one of those gates was honest and none of them measured the quantity that
// decides the picture:
//
//     WHERE IS THE EYE PLATE POINTING, RELATIVE TO THE CAMERA?
//
// A rotation of 39 degrees is a big number in body space and can be either a
// sweep across the visible face or a slide off the limb, depending ENTIRELY
// on which way the camera is. That is the fault D11 SS2.1 describes, and it
// is not visible from any measurement taken without the camera in it.
//
// So this probe joins the two spaces. For every shipped clip and every 60 Hz
// presentation frame it reports, per eye:
//
//   pos_az      the eye's own azimuth around the body axis, world, degrees
//   nrm_az      the azimuth its PLATE NORMAL points (local +X through the
//               posed bone -- the lens dome and both stars stand along +X,
//               see make_eye_lens/make_star, so +X IS the plate normal)
//   cam_az      the camera's world azimuth for that clip at that frame
//   off         wrap180(nrm_az - cam_az): 0 = the plate faces the camera
//               square; +-90 = edge on and the eye is a blade.
//
// ⚠ THE CONVENTIONS ARE MEASURED HERE, NOT ASSUMED. Every azimuth is
// atan2(x, z) in degrees, the same convention rot_world_yaw uses for the
// camera (zhao_reel.cpp:243 -- the camera sits at world azimuth theta after
// a rot_world_yaw(theta)), so the two are directly comparable. A sign
// argued from a quaternion formula would be exactly the kind of confident
// wrong number this file exists to stop.
//
// THE CAMERA TABLE mirrors subject_u02_clip (zhao_reel.cpp:5641): one shipped
// clip ORBITS (slot 0, `hover`, one exact turn per presentation loop) and
// every other shipped clip is fixed at cam_yaw 0x2000 -- the three-quarter.
// It is a mirror and it can go stale; the SCREEN-PIXEL gate (eyesweep.py) is
// what actually reads the shipped frames, and this probe is the authoring
// instrument that tells you which way to point things.
//
// USAGE
//   manafold-eyecam                 summary for every shipped clip
//   manafold-eyecam --csv <slot>    per-presentation-frame CSV for one slot
//   manafold-eyecam --rest          the REST-NORMAL report (D11 SS2.2 / R3):
//                                   how far each eye's plate normal is from
//                                   its own surface normal, with travel off
//
// Committed because a thrown-away probe is unreproducible (CLAUDE.md).

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <array>
#include <vector>

#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_texture.hpp"

namespace zc = zref::creature;
#include "manafold.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

// atan2(x, z) in degrees -- the azimuth convention rot_world_yaw uses.
double azdeg(double x, double z) { return std::atan2(x, z) * 180.0 / kPi; }

double wrap180(double a) {
  while (a > 180.0) a -= 360.0;
  while (a <= -180.0) a += 360.0;
  return a;
}

// The camera mirror. Returns the camera's world azimuth in degrees for a
// presentation frame `p` of a clip whose presentation length is `frames`.
struct CamSpec {
  bool orbit;
  int32_t yaw_a16;
};

CamSpec cam_for_slot(uint16_t slot) {
  // subject_u02_clip: only `manafold-hover` (slot 0) passes orbit=true among
  // the SHIPPED clips; every other subject takes `cam_yaw = 0x2000`.
  if (slot == 0) return CamSpec{true, 0};
  return CamSpec{false, 0x2000};
}

double cam_az_deg(const CamSpec& c, uint32_t p, uint32_t frames) {
  if (c.orbit) {
    const double th = static_cast<double>(p) * 65536.0 / (frames > 0 ? frames : 1);
    return wrap180(th * 360.0 / 65536.0);
  }
  return wrap180(static_cast<double>(c.yaw_a16) * 360.0 / 65536.0);
}

// The plate normal of a posed bone: local +X carried through the 3x3.
// mat3x4fx is a flat int32_t[12], row r column c at m[r * 4 + c].
void plate_normal(const zc::mat3x4fx& m, double& nx, double& ny, double& nz) {
  nx = static_cast<double>(m.m[0]);
  ny = static_cast<double>(m.m[4]);
  nz = static_cast<double>(m.m[8]);
}

const char* slot_name(uint16_t s) {
  switch (s) {
    case 0: return "hover";
    case 1: return "drift";
    case 2: return "channel";
    case 3: return "curious";
    case 4: return "startle";
    case 5: return "rest";
    case 6: return "pirouette";
    case 7: return "still";
    case 8: return "hasty";
    case 9: return "fall";
    case 10: return "hit";
    case 11: return "taunt";
    case 12: return "taunt2";
    case 13: return "trick";
    case 14: return "damage";
    case 16: return "nodule-solo";
    case 17: return "death-drop";
    case 18: return "death-gutter";
    case 19: return "lasso";
    case 20: return "blown";
    case 21: return "taunt3";
    case 22: return "flight";
    default: return "?";
  }
}

// A clip is SHIPPED if the bestiary renders it. `still` and `nodule-solo` are
// diagnostics and deliberately do not run antenna_knead, so they carry no
// travel at all -- reporting them beside the bank would be the solo-clip
// mistake D11 SS3 names, one channel over.
bool shipped(uint16_t s) { return s != 7 && s != 16; }

// The READABLE band. A plate at |off| beyond this is presented so obliquely
// that its star cannot be read at 384x240 -- authored by LOOKING at the pin
// ladder, not derived, and it is a reporting threshold only: nothing in the
// creature is generated from it.
constexpr double kReadableDeg = 45.0;

}  // namespace

int main(int argc, char** argv) {
  int csv_slot = -1;
  bool rest_report = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc) csv_slot = std::atoi(argv[++i]);
    else if (std::strcmp(argv[i], "--rest") == 0) rest_report = true;
  }

  const zc::CreatureType& T = u02::type();
  if (T.bank.clips.empty()) {
    std::printf("eyecam: FAIL empty clip bank\n");
    return 1;
  }

  if (rest_report) {
    // ---- THE REST-NORMAL REPORT (D11 SS2.2, pass-14 R3) -------------------
    // The eye BIND is a pure translation (manafold_rig.h), so at rest the
    // plate normal is body +X -- azimuth 90 by this file's convention --
    // while the eye SITS at azimuth atan2(kEyeXMm, +-kEyeZMm). The gap
    // between those two is the orientation degree of freedom the star has
    // never had. face_rest's kEyeYawOutA16 is composed on top; whether it
    // closes the gap or widens it is the question, and it is answered by
    // arithmetic here rather than by reading the constant's name.
    const double pos_l = azdeg(u02::kEyeXMm, u02::kEyeZMm);
    const double pos_r = azdeg(u02::kEyeXMm, -u02::kEyeZMm);
    u02::Rig g;
    g.reset();
    u02::face_rest(g);
    std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
    // Compose the rest rig by hand through the skeleton so no clip is needed.
    zc::Skeleton sk = u02::build_skeleton();
    std::printf("eyecam --rest: the eye's SURFACE normal vs its PLATE normal\n");
    std::printf("  kEyeXMm=%d kEyeZMm=%d kEyeStandoffMm=%d\n",
                u02::kEyeXMm, u02::kEyeZMm, u02::kEyeStandoffMm);
    std::printf("  surface normal azimuth : L %+7.2f   R %+7.2f  (deg, atan2(x,z))\n",
                pos_l, pos_r);
    std::printf("  a bind-identity plate  : L %+7.2f   R %+7.2f  (body +X)\n",
                90.0, 90.0);
    std::printf("  REST-NORMAL ERROR      : L %+7.2f   R %+7.2f\n",
                wrap180(90.0 - pos_l), wrap180(90.0 - pos_r));
    std::printf("  kEyeYawOutA16=%d -> %+.2f deg composed by face_rest\n",
                u02::kEyeYawOutA16,
                static_cast<double>(u02::kEyeYawOutA16) * 360.0 / 65536.0);
    (void)pose; (void)sk;
    // The measured version: run the still clip's first key through decode_pose
    // and read the actual posed normal, so the answer includes face_rest's
    // full composition and not just the yaw term.
    for (const zc::Clip& clip : T.bank.clips) {
      if (clip.slot_id != 7) continue;  // the still diagnostic: no travel
      std::array<zc::mat3x4fx, zc::kMaxBones> p{};
      zc::decode_pose(T, clip, 0, p, nullptr, 0);
      double nx, ny, nz;
      plate_normal(p[u02::kBEyeL], nx, ny, nz);
      const double nl = azdeg(nx, nz);
      plate_normal(p[u02::kBEyeR], nx, ny, nz);
      const double nr = azdeg(nx, nz);
      const double px = static_cast<double>(p[u02::kBEyeL].m[3] - p[u02::kBRoot].m[3]);
      const double pz = static_cast<double>(p[u02::kBEyeL].m[11] - p[u02::kBRoot].m[11]);
      const double qx = static_cast<double>(p[u02::kBEyeR].m[3] - p[u02::kBRoot].m[3]);
      const double qz = static_cast<double>(p[u02::kBEyeR].m[11] - p[u02::kBRoot].m[11]);
      std::printf("  MEASURED on `still` f0 (face_rest fully composed):\n");
      std::printf("    posed position azimuth : L %+7.2f   R %+7.2f\n",
                  azdeg(px, pz), azdeg(qx, qz));
      std::printf("    posed plate   azimuth  : L %+7.2f   R %+7.2f\n", nl, nr);
      std::printf("    PLATE MINUS SURFACE    : L %+7.2f   R %+7.2f"
                  "   <- zero means the eye sits ON the ball\n",
                  wrap180(nl - azdeg(px, pz)), wrap180(nr - azdeg(qx, qz)));
    }
    return 0;
  }

  if (csv_slot >= 0) {
    for (const zc::Clip& clip : T.bank.clips) {
      if (clip.slot_id != static_cast<uint16_t>(csv_slot)) continue;
      const CamSpec cs = cam_for_slot(clip.slot_id);
      const uint32_t frames = static_cast<uint32_t>(clip.frame_count) * 2u;
      std::printf("p,cam_az,l_pos_az,l_nrm_az,l_off,r_pos_az,r_nrm_az,r_off\n");
      for (uint16_t f = 0; f < clip.frame_count; ++f) {
        for (uint8_t sub = 0; sub < 2; ++sub) {
          std::array<zc::mat3x4fx, zc::kMaxBones> p{};
          zc::decode_pose(T, clip, f, p, nullptr, sub);
          const uint32_t pi = static_cast<uint32_t>(f) * 2u + sub;
          const double ca = cam_az_deg(cs, pi, frames);
          double nx, ny, nz;
          plate_normal(p[u02::kBEyeL], nx, ny, nz);
          const double nl = azdeg(nx, nz);
          plate_normal(p[u02::kBEyeR], nx, ny, nz);
          const double nr = azdeg(nx, nz);
          const double lpx = static_cast<double>(p[u02::kBEyeL].m[3] - p[u02::kBRoot].m[3]);
          const double lpz = static_cast<double>(p[u02::kBEyeL].m[11] - p[u02::kBRoot].m[11]);
          const double rpx = static_cast<double>(p[u02::kBEyeR].m[3] - p[u02::kBRoot].m[3]);
          const double rpz = static_cast<double>(p[u02::kBEyeR].m[11] - p[u02::kBRoot].m[11]);
          std::printf("%u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", pi, ca,
                      azdeg(lpx, lpz), nl, wrap180(nl - ca),
                      azdeg(rpx, rpz), nr, wrap180(nr - ca));
        }
      }
      return 0;
    }
    std::printf("eyecam: no clip with slot %d\n", csv_slot);
    return 1;
  }

  // ---- the summary -------------------------------------------------------
  std::printf("manafold-eyecam: the eye plate vs the camera, every shipped clip.\n");
  std::printf("`off` is wrap180(plate normal azimuth - camera azimuth): 0 faces the\n");
  std::printf("camera square, +-90 is edge on. `read%%` is the share of presentation\n");
  std::printf("frames where at least one eye is inside +-%.0f deg.\n\n", kReadableDeg);
  std::printf("%-14s %6s  %8s %8s  %8s %8s  %6s %6s %6s\n", "clip", "frames",
              "L off min", "L off max", "R off min", "R off max",
              "read%", "both%", "sweep");
  for (const zc::Clip& clip : T.bank.clips) {
    if (!shipped(clip.slot_id)) continue;
    const CamSpec cs = cam_for_slot(clip.slot_id);
    const uint32_t frames = static_cast<uint32_t>(clip.frame_count) * 2u;
    double lmin = 1e9, lmax = -1e9, rmin = 1e9, rmax = -1e9;
    double best_l = 1e9;  // closest approach of the near eye, for the sweep
    double worst_l = -1e9;
    uint32_t readable = 0, both = 0, n = 0;
    for (uint16_t f = 0; f < clip.frame_count; ++f) {
      for (uint8_t sub = 0; sub < 2; ++sub) {
        std::array<zc::mat3x4fx, zc::kMaxBones> p{};
        zc::decode_pose(T, clip, f, p, nullptr, sub);
        const uint32_t pi = static_cast<uint32_t>(f) * 2u + sub;
        const double ca = cam_az_deg(cs, pi, frames);
        double nx, ny, nz;
        plate_normal(p[u02::kBEyeL], nx, ny, nz);
        const double ol = wrap180(azdeg(nx, nz) - ca);
        plate_normal(p[u02::kBEyeR], nx, ny, nz);
        const double orr = wrap180(azdeg(nx, nz) - ca);
        if (ol < lmin) lmin = ol;
        if (ol > lmax) lmax = ol;
        if (orr < rmin) rmin = orr;
        if (orr > rmax) rmax = orr;
        if (ol < best_l) best_l = ol;
        if (ol > worst_l) worst_l = ol;
        const bool rl = std::fabs(ol) <= kReadableDeg;
        const bool rr = std::fabs(orr) <= kReadableDeg;
        if (rl || rr) ++readable;
        if (rl && rr) ++both;
        ++n;
      }
    }
    std::printf("%-14s %6u  %+8.1f %+8.1f  %+8.1f %+8.1f  %5.0f%% %5.0f%% %6.1f\n",
                slot_name(clip.slot_id), n, lmin, lmax, rmin, rmax,
                n ? 100.0 * readable / n : 0.0, n ? 100.0 * both / n : 0.0,
                worst_l - best_l);
  }
  std::printf("\n`sweep` is the total swing of the left plate against the camera over\n");
  std::printf("the clip, in degrees. On an ORBITING clip the camera supplies most of\n");
  std::printf("it; on a fixed-camera clip every degree of it is the eye travel.\n");
  return 0;
}
