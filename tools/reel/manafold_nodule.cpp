// manafold_nodule.cpp -- THE COMMITTED PER-NODULE INDEPENDENCE GATE
// (Manafold pass 12, Owner Direction 9 SS2.)
//
// WHY THIS EXISTS, in the owner's words:
//
//   "the antennae animation still is incredibly awful, almost nonexistent.
//    NONE OF THE BONES I SPECIFICALLY ASKED FOR WERE ADDED. All the nodules
//    should be able to move individually and bring the antennae parts with
//    them. The middle one might go down while the other two swing up.
//    Sideways. Up, down. Any kind of configuration."
//
//   "Do not report this as done again without showing each nodule moving
//    independently, in a plate the owner can look at."
//
// The instruction has been given about six times and four passes reported it
// delivered. This gate exists so a fifth report cannot be made on a mechanism
// alone. It is the numeric half; the PLATE built from the same clip is the
// half the owner actually judges, and neither substitutes for the other.
//
// *** WHAT IT MEASURES, AND WHY THAT IS THE HARD PART ***
//
// It measures the POSED BALL POSITIONS -- not the offsets that were requested,
// not the quaternions that were written, and not a re-derivation of the solver
// in this file. Two gates last pass read a same-named constant instead of the
// renderer's own symbol (gate checklist item 10), and the entire history of
// this creature's antenna is passes reporting a capability that a comment
// described and no frame ever showed.
//
// So the path here is exactly the shipping one:
//     u02::type()  ->  the compiled CreatureType, mesh and bank
//     zc::decode_pose(T, clip, f, pose)   -- the production decoder
//     zc::skin_vertex(pose, sv, ...)      -- the production skinner
// with the ball read as a synthetic vertex rigidly bound to its bone at that
// bone's own bind position, which is how manafold_hinge_traj.cpp and
// manafold_probe.cpp's rest-anchor table already read a joint. If the solver
// were deleted tomorrow this gate would fail, because nothing in it knows how
// the solver works.
//
// *** WHICH BONE IS WHICH NODULE ***
// A station's own rotation never moves its own origin, only its descendants'.
// The ball the owner calls "nodule A" sits at hinge A's pivot, so its posed
// position is hinge A's posed origin -- and it is moved by kBNeck. That
// off-by-one is the reason the solver writes to neck/A/B rather than A/B/C,
// and reading it wrong here would make an honest solver look broken.
//
// *** THE TWO FAILABLE LEGS (checklist item 10) ***
// Run with `--fail-mute <A|B|C>` to mute one nodule's offsets at the point the
// production path consumes them, and with `--fail-ignore` to reproduce the
// pass-11 behaviour where offsets are ignored entirely. Both must make this
// gate report FAIL. They are not simulations of a failure: they remove the
// mechanism from the same code path the pass verdict is taken on.
//
// Usage:
//   manafold-nodule.exe                 the gate; rc 0 pass, rc 1 fail
//   manafold-nodule.exe --fail-mute B   failable leg 1 (expects FAIL)
//   manafold-nodule.exe --fail-ignore   failable leg 2 (expects FAIL)
//   manafold-nodule.exe --csv           per-frame posed ball track, for plots

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <array>
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

// The threshold the plan names: a driven nodule must move at least this far.
// 200 mm is the authored offset ceiling; requiring 150 leaves headroom for the
// span-aim shortfall (a nodule lands ALONG its target direction at span
// length, not AT the target) without letting a dead nodule pass.
constexpr int32_t kDrivenMinMm = 150;
// A nodule that is NOT being driven may still move, because the one that IS
// carries the sections downstream of it -- that is the requirement, not a
// fault. So the independence test is DIRECTIONAL, and this is the whole
// subtlety of the gate: an UPSTREAM nodule must stay put when a DOWNSTREAM one
// moves. A is upstream of B is upstream of C.
constexpr int32_t kUpstreamMaxMm = 12;   // ~1.7 px at native
// The opposing-vertical test's floor: each of the two must clear this, in
// opposite directions, in the SAME pose. 40 mm is about 6 px at native --
// unmistakable on the plate, and far above the 12 mm dither band.
constexpr int32_t kOpposeMinMm = 40;

struct Ball {
  const char* name;
  uint8_t bone;
};
const Ball kBalls[3] = {{"A", u02::kBHingeA}, {"B", u02::kBHingeB}, {"C", u02::kBHingeC}};

/** Posed position of a ball, ROOT-LOCAL, through the production pose path. */
void posed_ball(const zc::CreatureType& T, const zc::Clip& clip, uint16_t f, uint8_t bone,
                double& x, double& y, double& z) {
  std::array<zc::mat3x4fx, zc::kMaxBones> pose;
  zc::decode_pose(T, clip, f, pose, nullptr, 0);
  zc::SkinVertex sv{T.baked.world_x[bone], T.baked.world_y[bone], T.baked.world_z[bone],
                    bone, bone, 64, 0, 0};
  int32_t wx, wy, wz;
  zc::skin_vertex(pose.data(), sv, wx, wy, wz, nullptr);
  const zc::mat3x4fx& rm = pose[u02::kBRoot];
  const int64_t dx = wx - rm.m[3], dy = wy - rm.m[7], dz = wz - rm.m[11];
  x = static_cast<double>(((rm.m[0] * dx + rm.m[4] * dy + rm.m[8] * dz) >> 16) * 1000 >> 16);
  y = static_cast<double>(((rm.m[1] * dx + rm.m[5] * dy + rm.m[9] * dz) >> 16) * 1000 >> 16);
  z = static_cast<double>(((rm.m[2] * dx + rm.m[6] * dy + rm.m[10] * dz) >> 16) * 1000 >> 16);
}

}  // namespace

int main(int argc, char** argv) {
  const char* mute = nullptr;
  bool ignore_all = false, csv = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--fail-mute") == 0 && i + 1 < argc) mute = argv[++i];
    else if (std::strcmp(argv[i], "--fail-ignore") == 0) ignore_all = true;
    else if (std::strcmp(argv[i], "--csv") == 0) csv = true;
  }

  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) {
    std::fprintf(stderr, "manafold-nodule: FAIL compile produced no meshlets\n");
    return 1;
  }

  // The solo clip is authored per key here rather than read from the bank, so
  // the failable legs can remove the mechanism from the SAME path the verdict
  // uses. It reproduces build_nodule_solo exactly apart from that -- and the
  // check below proves it does, against the committed clip, so this cannot
  // drift into measuring a private copy of the animation.
  const zc::Clip* banked = nullptr;
  for (const zc::Clip& c : T.bank.clips)
    if (c.slot_id == 16) banked = &c;
  if (!banked) {
    std::fprintf(stderr, "manafold-nodule: FAIL slot 16 (nodule-solo) is not in the bank\n");
    return 1;
  }

  const int S = u02::kNoduleSoloSegKeys;
  const int K = u02::kNoduleSoloKeys;
  zc::Clip c = u02::clip_shell(16, K, u02::kHoverHeightMm);
  for (int f = 0; f < K; ++f) {
    u02::Rig g;
    g.reset();
    const int seg = f / S, lf = f % S;
    const int32_t vert = lf < S / 2
        ? static_cast<int32_t>((static_cast<int64_t>(u02::kNoduleSoloAmpMm) *
                                u02::sinp(lf, S / 2, 1)) >> 16) : 0;
    const int32_t lat = lf >= S / 2
        ? static_cast<int32_t>((static_cast<int64_t>(u02::kNoduleSoloAmpMm) *
                                u02::sinp(lf - S / 2, S / 2, 1)) >> 16) : 0;
    u02::NoduleOffsets n;
    if (seg == 0) { n.ay = vert; n.az = lat; }
    else if (seg == 1) { n.by = vert; n.bz = lat; }
    else if (seg == 2) { n.cy = vert; n.cz = lat; }
    else {
      const int32_t w = static_cast<int32_t>(
          (static_cast<int64_t>(u02::kNoduleSoloAmpMm) * u02::sinp(lf, S, 1)) >> 16);
      n.ay = static_cast<int32_t>((static_cast<int64_t>(w) * u02::kNoduleSoloOutPm) / 1000);
      n.by = -static_cast<int32_t>((static_cast<int64_t>(w) * u02::kNoduleSoloMidPm) / 1000);
      n.cy = static_cast<int32_t>((static_cast<int64_t>(w) * u02::kNoduleSoloOutPm) / 1000);
      n.az = static_cast<int32_t>((static_cast<int64_t>(w) * u02::kNoduleSoloOutPm) / 1000);
    }
    // ---- THE FAILABLE LEGS, applied where the production path consumes the
    // offsets: not by faking a result, but by removing the mechanism.
    if (ignore_all) n = u02::NoduleOffsets{};
    if (mute) {
      if (*mute == 'A') { n.ax = n.ay = n.az = 0; }
      if (*mute == 'B') { n.bx = n.by = n.bz = 0; }
      if (*mute == 'C') { n.cx = n.cy = n.cz = 0; }
    }
    g.nod = n;
    u02::loop_pose(g, 1000, 1000, 1000, 1000);
    u02::face_rest(g);
    g.write(c, f);
  }
  c.interpolate = banked->interpolate;

  // PROOF THAT THIS IS THE SHIPPED ANIMATION: with no leg engaged, every quat
  // must equal the committed clip's. Without this check the gate could pass on
  // a private copy while the bank shipped something else -- which is the exact
  // shape of the "a gate read a same-named constant" failure.
  if (!mute && !ignore_all) {
    if (c.quats.size() != banked->quats.size()) {
      std::fprintf(stderr, "manafold-nodule: FAIL local clip size != banked\n");
      return 1;
    }
    for (size_t i = 0; i < c.quats.size(); ++i) {
      const zc::quat16& a = c.quats[i];
      const zc::quat16& b = banked->quats[i];
      if (a.q[0] != b.q[0] || a.q[1] != b.q[1] || a.q[2] != b.q[2] || a.q[3] != b.q[3]) {
        std::fprintf(stderr,
                     "manafold-nodule: FAIL local clip differs from the banked "
                     "slot 16 at quat %zu -- this gate is not measuring what ships\n", i);
        return 1;
      }
    }
    std::printf("identity: local pose == committed slot 16, %zu quats\n", c.quats.size());
  }

  if (csv) {
    std::printf("frame,seg,ball,x_mm,y_mm,z_mm\n");
    for (int f = 0; f < K; ++f)
      for (const Ball& b : kBalls) {
        double x, y, z;
        posed_ball(T, c, static_cast<uint16_t>(f), b.bone, x, y, z);
        std::printf("%d,%d,%s,%.0f,%.0f,%.0f\n", f, f / S, b.name, x, y, z);
      }
    return 0;
  }

  // Rest reference: frame 0 of every segment is exact rest by construction.
  double rx[3], ry[3], rz[3];
  for (int i = 0; i < 3; ++i) posed_ball(T, c, 0, kBalls[i].bone, rx[i], ry[i], rz[i]);

  std::printf("\nPER-NODULE INDEPENDENCE, posed ball travel from rest (mm), "
              "through decode_pose + skin_vertex\n");
  std::printf("  segment          ball A    ball B    ball C   verdict\n");

  int fails = 0;
  const char* segname[4] = {"0  A alone   ", "1  B alone   ", "2  C alone   ",
                            "3  mid down  "};
  for (int seg = 0; seg < 4; ++seg) {
    double peak[3] = {0, 0, 0};
    for (int f = seg * S; f < (seg + 1) * S; ++f) {
      for (int i = 0; i < 3; ++i) {
        double x, y, z;
        posed_ball(T, c, static_cast<uint16_t>(f), kBalls[i].bone, x, y, z);
        const double d = std::sqrt((x - rx[i]) * (x - rx[i]) + (y - ry[i]) * (y - ry[i]) +
                                   (z - rz[i]) * (z - rz[i]));
        if (d > peak[i]) peak[i] = d;
      }
    }
    const char* verdict = "ok";
    if (seg < 3) {
      // the driven one must MOVE
      if (peak[seg] < kDrivenMinMm) { verdict = "FAIL driven nodule is dead"; ++fails; }
      // every UPSTREAM one must stay put. Downstream ones are CARRIED and are
      // expected to move -- that is the requirement, not a fault.
      for (int i = 0; i < seg; ++i)
        if (peak[i] > kUpstreamMaxMm) {
          verdict = "FAIL an upstream nodule moved";
          ++fails;
        }
    } else {
      for (int i = 0; i < 3; ++i)
        if (peak[i] < kDrivenMinMm) { verdict = "FAIL a nodule is dead"; ++fails; }
    }
    std::printf("  %s  %7.0f   %7.0f   %7.0f   %s\n", segname[seg], peak[0], peak[1],
                peak[2], verdict);
  }

  // ---- THE OPPOSING-VERTICAL TEST -----------------------------------------
  // "the middle one might go down while the other two swing up."
  //
  // A MAGNITUDE test cannot tell "all three swung together" from "the middle
  // went the other way", and the whole content of the owner's sentence is that
  // distinction. So this is a SIGN test on the posed vertical displacements at
  // one instant: the middle must be going DOWN while an outer goes UP, by at
  // least kOpposeMinMm each. This is the claim the pass stands or falls on and
  // it FAILS the gate.
  {
    double best = 0;
    int bestf = -1;
    for (int f = 3 * S; f < 4 * S; ++f) {
      double y[3];
      for (int i = 0; i < 3; ++i) {
        double x, yy, z;
        posed_ball(T, c, static_cast<uint16_t>(f), kBalls[i].bone, x, yy, z);
        y[i] = yy - ry[i];
      }
      const double sep = y[2] - y[1];
      if (sep > best) { best = sep; bestf = f; }
    }
    std::printf("\n  OPPOSING VERTICALS -- \"the middle goes DOWN while the "
                "other swings UP\":\n");
    double y[3] = {0, 0, 0};
    if (bestf >= 0)
      for (int i = 0; i < 3; ++i) {
        double x, yy, z;
        posed_ball(T, c, static_cast<uint16_t>(bestf), kBalls[i].bone, x, yy, z);
        y[i] = yy - ry[i];
      }
    if (bestf < 0 || !(y[1] < -kOpposeMinMm && y[2] > kOpposeMinMm)) {
      std::printf("    frame %d:  A %+.0f   B %+.0f   C %+.0f mm\n", bestf, y[0], y[1], y[2]);
      std::printf("    FAIL the middle and the rear must move in OPPOSITE "
                  "vertical directions, by at least %d mm each\n", (int)kOpposeMinMm);
      ++fails;
    } else {
      std::printf("    frame %d:  A %+.0f   B %+.0f (down)   C %+.0f (up)   "
                  "separation %.0f mm\n", bestf, y[0], y[1], y[2], best);
      std::printf("    ok -- middle down and rear up IN ONE POSE, not a phase "
                  "offset of one curve\n");
    }
  }

  // ---- VERTICAL REACH PER NODULE -- A DECLARED GAP, NOT A WEAKENED GATE ----
  //
  // This block reports and does NOT fail, and the reason has to be on the
  // record or it is exactly the "re-record the band to admit the measurement"
  // failure 07-MOTION-STYLE warns about.
  //
  // Ball A's span leaves the junction pointing STRAIGHT UP -- measured rest
  // geometry: junction (90, 664), ball A (0, 1337, 29), a 679 mm span whose
  // direction is (-0.13, +0.99, +0.04). Moving A up or down therefore means
  // LENGTHENING OR SHORTENING that span, and `rigid_fault_of()` guarantees
  // bones cannot scale. It is not a tuning shortfall and no constant in this
  // tree will change it: a 200 mm vertical request moves ball A by 3 mm, while
  // the same 200 mm SIDEWAYS moves it 198 mm.
  //
  // ⚠ PASS 12 WAVE 2a: D9 SS13's STRETCHY SPANS ARE NOW BUILT, AND THIS BLOCK
  // STILL PRINTS -7..+3. THAT IS NOT A FAILED FIX -- IT IS THIS GATE'S BLIND
  // SPOT, and the number below is honest about bones and silent about skin.
  //
  // `posed_ball()` above skins a SYNTHETIC VERTEX AT THE BONE'S OWN BIND
  // ORIGIN, carrying no deform metadata. The span stretch is a VERTEX effect
  // applied before rigid skinning, so no vertex the bone gate constructs can
  // ever carry it. A bone gate measuring bones is right; it just cannot answer
  // this particular question.
  //
  // THE SKIN ANSWER IS MEASURED, in `manafold_spangate.cpp` (build target
  // `mspan`), through the same production path plus deform_skin_vertex_lanes:
  //
  //     ball A vertical SKIN reach, solo-A segment:  37.8 mm -> 172.3 mm
  //
  // So the reach is delivered and the ball does go where it is told; what stays
  // true is that the BONE barely moves, because bones cannot scale. Read the
  // two gates together. The line below is left printing, and left ungated, for
  // the same reason it always was.
  //
  // It prints loudly, every run, in the same spirit as kEyeShiftPivotMm's
  // declared gap: not silently absent.
  {
    std::printf("\n  VERTICAL REACH per nodule (200 mm requested, solo, "
                "DECLARED GAP -- reported, not gated):\n");
    for (int i = 0; i < 3; ++i) {
      double lo = 1e9, hi = -1e9;
      for (int f = i * S; f < i * S + S / 2; ++f) {
        double x, y2, z;
        posed_ball(T, c, static_cast<uint16_t>(f), kBalls[i].bone, x, y2, z);
        const double d = y2 - ry[i];
        if (d < lo) lo = d;
        if (d > hi) hi = d;
      }
      std::printf("    ball %s  %+6.0f .. %+6.0f mm%s\n", kBalls[i].name, lo, hi,
                  (hi - lo) < 60 ? "   <-- BONE only: the span points along "
                                   "the request. The SKIN answer is mspan's G4"
                                 : "");
    }
  }

  const bool leg = (mute != nullptr) || ignore_all;
  std::printf("\n%s: %d failure(s)%s\n", fails ? "FAIL" : "PASS", fails,
              leg ? "   [FAILABLE LEG ENGAGED -- a failure here is the gate working]" : "");
  if (leg) {
    // A leg that does not fail is a gate that cannot fail, which is worse than
    // no gate. Invert the exit code so the leg run itself is checkable in CI.
    if (fails == 0) {
      std::fprintf(stderr,
                   "manafold-nodule: THE FAILABLE LEG DID NOT FAIL. The gate is "
                   "not measuring the mechanism.\n");
      return 1;
    }
    return 0;
  }
  return fails ? 1 : 0;
}
