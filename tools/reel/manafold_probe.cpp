// u02_probe — the COMMITTED hover-clearance probe for creature 02.
//
// THE CONTRACT IS CLEARANCE, NOT PENETRATION (OWNER-DIRECTION-1: the
// creature FLOATS — no legs, no gait, no ground contact; Zixxtrixx's
// authored-penetration law does not apply). For EVERY clip, EVERY key AND
// every 60 Hz presentation midpoint, skin EVERY full-detail vertex through
// the decoded pose and assert
//
//     world_y(vertex) >= kMinClearanceMm
//
// where world y is measured from the instance's terrain-snap plane (the
// reel sets instance y to the column top; the clip root carries the hover).
// Terrain sloping away from the snap column only increases clearance, so
// the flat-plane bound is the conservative one.
//
// The constant is derived from the ACCEPTED motion (a window from named
// constants, never absolute ticks): the rest clip's low hover minus its bob
// leaves ~70 mm; the gate sits below it with honest headroom (the
// gate-fitted-to-its-own-answer trap).
//
// Committed because a thrown-away probe is unreproducible (CLAUDE.md).

#include <cstdio>
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>

#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_texture.hpp"
#include "render_helpers.hpp"  // rtest::bump_patch — the reel's own stage
#include "zrender/internal.hpp"  // zref::render::compose_lattice

namespace zc = zref::creature;
#include "manafold.h"


// PASS 6 (5c): inverse of a rigid bone transform applied to a point. The bone
// matrices here are exactly rotation-plus-translation, so the inverse is the
// transpose of the 3x3 with -R^T t -- no general inversion needed.
static inline void inv_point(const zc::mat3x4fx& m, int32_t x, int32_t y, int32_t z,
                             int32_t& ox, int32_t& oy, int32_t& oz) {
  // mat3x4fx is a FLAT int32_t[12]: row r, column c is m[r * 4 + c].
  const int64_t dx = static_cast<int64_t>(x) - m.m[3];
  const int64_t dy = static_cast<int64_t>(y) - m.m[7];
  const int64_t dz = static_cast<int64_t>(z) - m.m[11];
  ox = static_cast<int32_t>((m.m[0] * dx + m.m[4] * dy + m.m[8] * dz) >> 16);
  oy = static_cast<int32_t>((m.m[1] * dx + m.m[5] * dy + m.m[9] * dz) >> 16);
  oz = static_cast<int32_t>((m.m[2] * dx + m.m[6] * dy + m.m[10] * dz) >> 16);
}

int main() {
  constexpr int32_t kMinClearanceMm = 40;
  // PASS 3: the headstand (slot 13) DECLARES ground contact — the loop
  // peak plants at kTrickPlantDepthMm inside keys
  // [kTrickPlantKey, kTrickLiftKey). Inside that window the clearance
  // contract is REPLACED by the contact contract: the deepest vertex must
  // sit in [-kTrickDepthMaxMm, -kTrickDepthMinMm] — really touching,
  // never drowning. Outside the window the float contract holds as ever.
  constexpr uint16_t kTrickSlot = 13;
  constexpr int32_t kTrickDepthMinMm = 5;   // shallower reads as hovering
  constexpr int32_t kTrickDepthMaxMm = 60;  // deeper is a crash, not a plant
  // The APRON: the landing approach and the lift-away, a declared few keys
  // either side of the contact window where the peak is legitimately
  // skimming the dirt — neither the 40 mm float gate nor the must-touch
  // contact gate applies there. Part of the declaration, not a loophole:
  // it is two keys wide and the probe still asserts nothing goes deeper
  // than the crash bound inside it.
  constexpr int kTrickApronKeys = 2;
  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) {
    std::printf("u02-probe: FAIL compile produced no meshlets\n");
    return 1;
  }
  int rc = 0;
  for (const zc::Clip& clip : T.bank.clips) {
    const bool has_window = clip.slot_id == kTrickSlot;
    int32_t worst = INT32_MAX;        // outside any declared window
    int32_t window_worst = INT32_MAX; // inside the declared window
    uint16_t worst_frame = 0;
    uint8_t worst_sub = 0;
    for (uint16_t f = 0; f < clip.frame_count; ++f) {
      const bool in_window =
          has_window && f >= u02::kTrickPlantKey && f < u02::kTrickLiftKey;
      const bool in_apron =
          has_window && !in_window &&
          f >= u02::kTrickPlantKey - kTrickApronKeys &&
          f < u02::kTrickLiftKey + kTrickApronKeys;
      for (uint8_t sub = 0; sub < 2; ++sub) {
        std::array<zc::mat3x4fx, zc::kMaxBones> pose;
        zc::decode_pose(T, clip, f, pose, nullptr, sub);
        const zc::DeformSample d = zc::deformation_sample(T, clip.slot_id, f, sub);
        for (const zc::Meshlet& m : T.mesh) {
          for (size_t vi = 0; vi < m.verts.size(); ++vi) {
            zc::SkinVertex sv = m.verts[vi];
            if (!m.deform.empty()) sv = zc::deform_skin_vertex(sv, m.deform[vi], d);
            int32_t x, y, z;
            zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
            if (in_window || in_apron) {
              if (y < window_worst) window_worst = y;
              continue;
            }
            if (y < worst) {
              worst = y;
              worst_frame = f;
              worst_sub = sub;
            }
          }
        }
      }
    }
    const int32_t worst_mm = static_cast<int32_t>((static_cast<int64_t>(worst) * 1000) >> 16);
    const bool ok = worst_mm >= kMinClearanceMm;
    std::printf("u02-probe: slot %u (%u keys): min clearance %d mm at key %u sub %u — %s\n",
                clip.slot_id, clip.frame_count, worst_mm, worst_frame, worst_sub,
                ok ? "OK" : "FAIL");
    if (!ok) rc = 1;
    if (has_window) {
      const int32_t wmm =
          static_cast<int32_t>((static_cast<int64_t>(window_worst) * 1000) >> 16);
      const bool wok = wmm <= -kTrickDepthMinMm && wmm >= -kTrickDepthMaxMm;
      std::printf(
          "u02-probe: slot %u DECLARED CONTACT keys %d..%d (+%d-key apron): "
          "deepest vertex %d mm (declared -%d, accepted -%d..-%d) — %s\n",
          clip.slot_id, u02::kTrickPlantKey, u02::kTrickLiftKey, kTrickApronKeys,
          wmm, u02::kTrickPlantDepthMm, kTrickDepthMaxMm, kTrickDepthMinMm,
          wok ? "OK" : "FAIL");
      if (!wok) rc = 1;
    }
  }
  std::printf(rc == 0 ? "u02-probe: CLEARANCE CONTRACT HOLDS (>= %d mm everywhere)\n"
                      : "u02-probe: CLEARANCE VIOLATED (< %d mm)\n",
              kMinClearanceMm);

  // ---- PASS 2: the LOOP-CLOSURE probe (committed, per CLAUDE.md) ---------
  //
  // The return arm must stay ENGAGED with the body at every representable
  // fold scale: loop_pose aims hinge D's last segment at the re-entry
  // anchor in closed form, and this probe is the 3D proof (the 2D aim
  // ignores the neck yaw and the rest tilt, so the verdict lives here).
  // Contract: the arm's terminal ring CENTRES sit inside the body
  // ellipsoid at <= kMaxEndEllipPm of the surface — for a synthetic fold
  // sweep 780..1160 (the historical clip range) AND for every key of every
  // shipped clip. The floating dongle and the punch-through both violate
  // this; connection cannot regress silently again.
  {
    constexpr int32_t kMaxEndEllipPm = 920;  // centreline gate (pass 2's)
    // PASS 3: the RIM gate (the reviewer's ask made a metric). The terminal
    // rings' rim samples (±rx/±rz at the terminal blade radii) may GRAZE
    // the reference ellipsoid — the offsets are tangential and the junction
    // knuckle masks the entry — measured worst 1011 pm across sweep + bank
    // on the pass-3 geometry, and the worst key (fall slot 9 key 57) was
    // rendered and LOOKED at: no visible breakout. Gate at measured + honest
    // headroom; past it the arm rim is genuinely leaving the body.
    // PASS 4: the always-on knead layer legitimately moves the return arm
    // (bounded joint gestures on top of the closure aim), and the measured
    // bank worst rose 1039 -> 1087 (taunt2 key 83). The worst key was
    // rendered and LOOKED at (run evidence lasso-166): no visible breakout
    // -- the back-junction ball masks the entry, the pass-3 rationale
    // unchanged. Gate = measured + the same honest headroom class.
    constexpr int32_t kMaxRimEllipPm = 1120;
    const int32_t rx = u02::fxu(u02::kBodyRadiusMm);
    const int32_t ry = u02::fxu(u02::vmm(u02::kBodyRadiusMm));
    // terminal ring stations (mm along the tube from y0), replicated from
    // make_loop's law: the last three rings of the chain.
    const int32_t y0 = u02::kLoopNeckExitYMm - u02::kLoopBuryMm;
    const int32_t total = u02::kLoopBuryMm + u02::kLoopArcMm[0] + u02::kLoopArcMm[1] +
                          u02::kLoopArcMm[2] + u02::kLoopArcMm[3] + u02::kLoopArcMm[4] +
                          u02::kLoopArcMm[5];
    int32_t worst_pm = 0;
    int32_t worst_ellip = 0;
    const auto end_ellip = [&](const std::array<zc::mat3x4fx, zc::kMaxBones>& pose) {
      // Test in the ROOT-LOCAL frame: invert the root's affine (R^T (p - t))
      // before the ellipsoid test. Subtracting only the translation lied
      // twice — first on a jump clip (x/z moved), then on the fall (the
      // TUMBLE rotates the body, and the anchor rotates with it).
      const zc::mat3x4fx& rm = pose[u02::kBRoot];
      int32_t worst = 0;
      // PASS 3 (the reviewer's ask): test the terminal rings' RIM, not only
      // their centrelines — five sample offsets per ring: centre, ±rx (in
      // the loop plane), ±rz (across it), at the terminal blade radii.
      const int32_t rim_rx = u02::kLoopBladeRxMm[7];
      const int32_t rim_rz = u02::kLoopBladeRzMm[7];
      const int32_t offs[5][2] = {
          {0, 0}, {rim_rx, 0}, {-rim_rx, 0}, {0, rim_rz}, {0, -rim_rz}};
      for (int ri = u02::kLoopRings - 3; ri < u02::kLoopRings; ++ri) {
        const int32_t s =
            static_cast<int32_t>((static_cast<int64_t>(total) * ri) / (u02::kLoopRings - 1));
        for (int oi = 0; oi < 5; ++oi) {
        zc::SkinVertex sv{};
        sv.x = u02::fxu(u02::kLoopTubeXMm + offs[oi][0]);
        sv.y = u02::fxu(y0 + s);
        sv.z = u02::fxu(offs[oi][1]);
        sv.b0 = u02::kBHingeD;  // the terminal rings are fully on the arm bone
        sv.b1 = u02::kBHingeD;
        sv.w0 = 64;
        int32_t x, y, z;
        zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
        const int64_t dx = x - rm.m[3], dy = y - rm.m[7], dz = z - rm.m[11];
        const int64_t lx = (rm.m[0] * dx + rm.m[4] * dy + rm.m[8] * dz) >> 16;
        const int64_t ly = (rm.m[1] * dx + rm.m[5] * dy + rm.m[9] * dz) >> 16;
        const int64_t lz = (rm.m[2] * dx + rm.m[6] * dy + rm.m[10] * dz) >> 16;
        const int64_t ex = (lx << 16) / rx;
        const int64_t ey = (ly << 16) / ry;
        const int64_t ez = (lz << 16) / rx;
        const int32_t e = static_cast<int32_t>(
            (u02::isqrt64(ex * ex + ey * ey + ez * ez) * 1000) >> 16);
        if (e > worst) worst = e;
        }
      }
      return worst;
    };
    // (a) the synthetic sweep: one clip, one key per fold scale
    {
      const int kSteps = 24;
      zc::Clip sweep = u02::clip_shell(7, kSteps, u02::kHoverHeightMm);
      for (int i = 0; i < kSteps; ++i) {
        const int32_t pm = 700 + (1160 - 700) * i / (kSteps - 1);
        u02::Rig g;
        g.reset();
        u02::loop_pose(g, pm, pm, pm, pm);
        u02::face_rest(g);
        g.write(sweep, i);
      }
      for (int i = 0; i < kSteps; ++i) {
        std::array<zc::mat3x4fx, zc::kMaxBones> pose;
        zc::decode_pose(T, sweep, static_cast<uint16_t>(i), pose, nullptr, 0);
        const int32_t e = end_ellip(pose);
        const int32_t pm = 700 + (1160 - 700) * i / (kSteps - 1);
        if (e > worst_ellip) {
          worst_ellip = e;
          worst_pm = pm;
        }
        if (e > kMaxRimEllipPm) {
          std::printf("u02-probe: CLOSURE FAIL at fold scale %d: arm rim at %d pm of surface\n",
                      pm, e);
          rc = 1;
        }
      }
      std::printf("u02-probe: closure sweep 700..1160 worst arm RIM %d pm of surface (at pm %d)"
                  " — %s (rim gate %d; centreline gate %d unchanged)\n",
                  worst_ellip, worst_pm, worst_ellip <= kMaxRimEllipPm ? "OK" : "FAIL",
                  kMaxRimEllipPm, kMaxEndEllipPm);
    }
    // (b) every key of every shipped clip
    int32_t bank_worst = 0;
    uint16_t bank_slot = 0, bank_key = 0;
    for (const zc::Clip& clip : T.bank.clips) {
      for (uint16_t f = 0; f < clip.frame_count; ++f) {
        for (uint8_t sub = 0; sub < 2; ++sub) {  // PASS 3: midpoints too
          std::array<zc::mat3x4fx, zc::kMaxBones> pose;
          zc::decode_pose(T, clip, f, pose, nullptr, sub);
          const int32_t e = end_ellip(pose);
          if (e > bank_worst) {
            bank_worst = e;
            bank_slot = clip.slot_id;
            bank_key = f;
          }
        }
      }
    }
    const bool ok = bank_worst <= kMaxRimEllipPm;
    std::printf("u02-probe: closure over the clip bank (both subs): worst arm RIM %d pm"
                " (slot %u key %u) — %s (rim gate %d)\n",
                bank_worst, bank_slot, bank_key, ok ? "OK" : "FAIL", kMaxRimEllipPm);
    if (!ok) rc = 1;
  }

  // ---- PASS 2/5: the EYE-PROTRUSION gate (committed; PER NAMED PART)
  //
  // Owner: the eyes must keep poking out as 3D things — the artist likes
  // it ("abstehendes Auge"). PASS 5 RE-BASELINE, after the pass-4 QA
  // refutation: the old probe took the max over EVERY face vertex, so
  // WHICH part it measured silently changed when the geometry changed
  // (10-GATE-CHECKLIST item 16). Pass 3's "protected 164 mm" was the CYAN
  // STAR poking outside its lens — the exact fault pass 4 was ordered to
  // remove — while the LENS itself never regressed (1218 → 1228 pm). So
  // the protected read is now the LENS crown, measured over the lens's own
  // vertices (matched by part material), and the star/ring are REPORTED so
  // the extremum's identity can never silently move again. Want more
  // stand-off? Raise the lens (kEyeDeepMm/kEyeXMm) — never the star arms:
  // re-lengthening kStarArmSideMm re-breaks star-in-lens containment.
  // Computed in 3D against the body ellipsoid — a rendered-frame
  // measurement would conflate protrusion with perspective.
  {
    // the GATE: the lens crown must stand at least this far out. Derived
    // from the accepted pass-5 lens read minus honest headroom (never
    // re-fit this to a failing number — raise the lens instead).
    constexpr int32_t kLensCrownMinPm = 1215;
    const int32_t rx = u02::fxu(u02::kBodyRadiusMm);
    const int32_t ry = u02::fxu(u02::vmm(u02::kBodyRadiusMm));
    const zc::Clip& still = T.bank.clips[7];  // slot 7: the still pose
    std::array<zc::mat3x4fx, zc::kMaxBones> pose;
    zc::decode_pose(T, still, 0, pose, nullptr, 0);
    const int32_t root_y = still.root[1];
    struct EyePart { const char* name; uint8_t r, g, b; int32_t max_e; };
    EyePart parts[3] = {
        {"purple lens", u02::kLensR, u02::kLensG, u02::kLensB, 0},
        {"cyan star", u02::kStarR, u02::kStarG, u02::kStarB, 0},
        {"white star", 246, 242, 250, 0}};
    for (const zc::Meshlet& m : T.mesh) {
      EyePart* part = nullptr;
      for (auto& p : parts)
        if (m.r == p.r && m.g == p.g && m.b == p.b) part = &p;
      if (!part) continue;
      for (const zc::SkinVertex& sv : m.verts) {
        int32_t x, y, z;
        zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
        const int64_t ex = (static_cast<int64_t>(x) << 16) / rx;
        const int64_t ey = (static_cast<int64_t>(y - root_y) << 16) / ry;
        const int64_t ez = (static_cast<int64_t>(z) << 16) / rx;
        const int32_t e = static_cast<int32_t>(
            (u02::isqrt64(ex * ex + ey * ey + ez * ez) * 1000) >> 16);
        if (e > part->max_e) part->max_e = e;
      }
    }
    for (const auto& p : parts) {
      const int32_t proud_mm = static_cast<int32_t>(
          (static_cast<int64_t>(p.max_e - 1000) * u02::kBodyRadiusMm) / 1000);
      std::printf("u02-probe: eye crown [%s] ellip %d pm — stands %d mm proud of the body\n",
                  p.name, p.max_e, proud_mm);
    }
    const bool lens_ok = parts[0].max_e >= kLensCrownMinPm;
    std::printf("u02-probe: eye-protrusion gate (LENS >= %d pm): %s\n",
                kLensCrownMinPm, lens_ok ? "OK" : "FAIL");
    if (!lens_ok) rc = 1;
  }
  // ---- PASS 6 (Direction 5 5c): THE CONTAINMENT LEASH, AS A GATE ---------
  //
  // The old rule was "the star stays inside the purple", it lived in a COMMENT,
  // and QA found it 34% stale because the white ring's tube gauge moved
  // underneath its arithmetic. The owner has now retired the rule itself:
  //
  //   "Star (and surrounding white) can travel a certain distance outside the\n"
  //    eye. Pick something sensible. The eye itself can move a bit too."
  //
  // The star rides to the rim and OVER it, the way a googly eye's pupil presses
  // against its socket. Three rules replace containment, and all three live
  // HERE rather than in prose, because this eye's containment has now been
  // redesigned three times and every time the rule lived in a comment it went
  // wrong silently.
  //
  //   1. OVERHANG IS CAPPED at kStarOverhangMaxPm of the star's half-width.
  //      It presses at the rim; it does not slide off.
  //   2. AT LEAST 60% OF THE STAR STAYS ON THE PURPLE. Past that it stops
  //      reading as an eye looking somewhere and starts reading as a sticker.
  //   3. THE STAR NEVER CROSSES THE BODY OUTLINE. Overhanging onto the pink is
  //      correct and wanted; overhanging into the SKY is a detached artefact.
  //
  // Measured over the WHOLE CLIP BANK -- every clip, every key, both eyes --
  // because that is what ships, not over a synthetic gaze sweep.
  //
  // HOW RULE 3 IS DONE, AND ITS HONEST LIMITS. It is a SILHOUETTE test, not a
  // distance test: the eye pops out of a curved body, so "outside the body" in
  // 3D is normal and correct, and only the projected outline decides whether a
  // pixel lands on pink or on sky. For an ellipsoid the outline has a closed
  // form -- scale space by the body radii so the ellipsoid becomes the unit
  // sphere, scale the view direction the same way, and a point is inside the
  // outline exactly when its distance from the origin measured PERPENDICULAR
  // to that direction is <= 1. No rasterising and no camera matrix, which also
  // keeps it clear of the documented trap of measuring a silhouette off a
  // rendered frame.
  //
  // Two declared approximations:
  //   * ORTHOGRAPHIC. The perspective outline differs slightly; the creature is
  //     small against its camera distance, so the error sits far below the ~1 px
  //     this rule protects.
  //   * TWO VIEWS, not a full sphere. A full ring would be wrong rather than
  //     conservative: from side-on the eye is outside the body outline BY
  //     DESIGN -- that is the pop-out the artist drew a dedicated study of --
  //     and a star sitting on a lens that is itself legitimately proud is not
  //     the fault this rule looks for. A star vertex passes if it is on the
  //     purple (rule 2's own test) OR inside the body outline: over pink
  //     either way.
  {
    const int32_t bx = u02::fxu(u02::kBodyRadiusMm);
    const int32_t by = u02::fxu(u02::vmm(u02::kBodyRadiusMm));
    // The star's half-width at its waist, in mm, after 5c's scale: the unit
    // rule 1 is expressed in. DERIVED from the shipped constants, never typed.
    const int32_t star_half_mm =
        static_cast<int32_t>((static_cast<int64_t>(u02::kStarArmSideMm) *
                              u02::kStarScalePm) / 1000) + u02::kStarWhiteRimMm;
    const int32_t overhang_cap_mm =
        static_cast<int32_t>((static_cast<int64_t>(star_half_mm) *
                              u02::kStarOverhangMaxPm) / 1000);
    // The two shipping views as direction vectors (Q16.16) at the showcase
    // down-pitch: three-quarter (cam_yaw 0x2000) and front (0x4000).
    struct View { const char* name; int32_t dx, dy, dz; };
    const View views[2] = {{"three-quarter", 46341, -17000, 46341},
                           {"front", 65536, -17000, 0}};
    int32_t worst_overhang_mm = 0;
    int32_t worst_on_purple_pm = 1000;
    int outside_body = 0;
    uint32_t worst_slot = 0, worst_key = 0;
    const int32_t eye_long_fx = u02::fxu(u02::vmm(u02::kEyeLongMm));
    for (const zc::Clip& clip : T.bank.clips) {
      for (uint32_t f = 0; f < clip.frame_count; ++f) {
        std::array<zc::mat3x4fx, zc::kMaxBones> pose;
        zc::decode_pose(T, clip, f, pose, nullptr, 0);
        const int32_t root_y = clip.root[static_cast<size_t>(f) * 3 + 1];
        for (const zc::Meshlet& m : T.mesh) {
          if (!(m.r == 246 && m.g == 242 && m.b == 250)) continue;  // white star
          int on = 0, tot = 0;
          for (const zc::SkinVertex& sv : m.verts) {
            int32_t x, y, z;
            zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
            // Rules 1 and 2: is this vertex over the purple? The lens is an
            // ellipse in ITS OWN bone frame, which is also the only frame in
            // which "the rim" is well defined -- so measure there.
            const uint8_t eb =
                (sv.b0 == u02::kBPupilL || sv.b0 == u02::kBEyeL) ? u02::kBEyeL
                                                                 : u02::kBEyeR;
            const zc::mat3x4fx& em = pose[eb];
            int32_t lx, ly, lz;
            inv_point(em, x, y, z, lx, ly, lz);
            // PASS 7 -- THE SECOND BUG IN THIS BLOCK, found by QA and not in
            // the pass-7 brief's list. inv_point against a SKINNING matrix
            // (world * inv_bind) returns BIND space, NOT eye-bone space. The
            // eye bone's bind is a pure translation off the root, so the raw
            // lz still carries the eye's own +/-kEyeZMm (215 mm) offset. Left
            // uncorrected it inflates every overhang by ~215 mm: with the
            // units fixed but the frame still wrong, rule 1 reads 378 mm where
            // the true figure is 142 mm.
            //
            // TWO bugs were stacked in one measurement, and the first one
            // masked the second -- while everything truncated to zero, a
            // 215 mm frame error was invisible. Fixing only the units would
            // have produced a confident, wrong, and much SCARIER number, and
            // the creature would then have been tuned to satisfy it.
            lx -= u02::fxu(u02::kEyeXMm);
            ly -= u02::fxu(u02::vmm(u02::kEyeYMm));
            lz -= (eb == u02::kBEyeL) ? u02::fxu(u02::kEyeZMm)
                                      : -u02::fxu(u02::kEyeZMm);
            (void)lx;
            int32_t w_pm = 0;
            if (eye_long_fx > 0) {
              const int64_t dy_pm = (static_cast<int64_t>(ly) * 1000) / eye_long_fx;
              if (dy_pm > -1000 && dy_pm < 1000) {
                int32_t t = static_cast<int32_t>((dy_pm + 1000) *
                                                 (u02::kEyeLensRings - 1) / 2000);
                if (t < 0) t = 0;
                if (t > u02::kEyeLensRings - 1) t = u02::kEyeLensRings - 1;
                const int32_t t2 = t + 1 < u02::kEyeLensRings ? t + 1 : t;
                w_pm = (u02::kEyeLensWidthPm[t] + u02::kEyeLensWidthPm[t2]) / 2;
              }
            }
            const int32_t rim_mm = static_cast<int32_t>(
                (static_cast<int64_t>(u02::kEyeWideMm) * w_pm) / 1000);
            // PASS 7 -- THE UNITS BUG THAT KILLED FIVE GATES. fxu(mm) is
            // mm * 65536 / 1000, so a raw fx16 value is METRES x 65536 and the
            // conversion back to mm is (raw * 1000) >> 16. Pass 6 wrote a bare
            // `>> 16` and called the result mm, so EVERY offset under 1000 mm
            // truncated to zero: rule 1's worst overhang was pinned at 0, rule
            // 2's on-purple fraction at 1000 pm, and rule 3 -- which sits
            // behind `if (over > 0)` -- NEVER EXECUTED AT ALL.
            const int32_t off_mm = static_cast<int32_t>(
                ((lz < 0 ? -static_cast<int64_t>(lz) : static_cast<int64_t>(lz)) *
                 1000) >> 16);
            const int32_t over = off_mm - rim_mm;
            ++tot;
            if (over <= 0) ++on;
            if (over > worst_overhang_mm) {
              worst_overhang_mm = over;
              worst_slot = clip.slot_id;
              worst_key = f;
            }
            // Rule 3: a vertex off the purple must still be inside the BODY
            // outline from a shipping view, or it is drawn against sky.
            if (over > 0) {
              for (const View& v : views) {
                const int64_t ux = (static_cast<int64_t>(x) << 16) / bx;
                const int64_t uy = (static_cast<int64_t>(y - root_y) << 16) / by;
                const int64_t uz = (static_cast<int64_t>(z) << 16) / bx;
                const int64_t ex = (static_cast<int64_t>(v.dx) << 16) / bx;
                const int64_t ey = (static_cast<int64_t>(v.dy) << 16) / by;
                const int64_t ez = (static_cast<int64_t>(v.dz) << 16) / bx;
                const int64_t elen = u02::isqrt64(ex * ex + ey * ey + ez * ez);
                if (elen == 0) continue;
                const int64_t dot = (ux * ex + uy * ey + uz * ez) / elen;
                const int64_t px = ux - dot * ex / elen;
                const int64_t py = uy - dot * ey / elen;
                const int64_t pz = uz - dot * ez / elen;
                const int64_t perp = u02::isqrt64(px * px + py * py + pz * pz);
                if (perp > (1LL << 16)) ++outside_body;
              }
            }
          }
          if (tot > 0) {
            const int32_t pm = on * 1000 / tot;
            if (pm < worst_on_purple_pm) worst_on_purple_pm = pm;
          }
        }
      }
    }
    const bool r1 = worst_overhang_mm <= overhang_cap_mm;
    const bool r2 = worst_on_purple_pm >= 600;
    const bool r3 = outside_body == 0;
    std::printf("u02-probe: 5c LEASH rule 1 (overhang <= %d mm = %d pm of a "
                "%d mm star half-width): worst %d mm at slot %u key %u - %s\n",
                overhang_cap_mm, u02::kStarOverhangMaxPm, star_half_mm,
                worst_overhang_mm, worst_slot, worst_key, r1 ? "OK" : "FAIL");
    std::printf("u02-probe: 5c LEASH rule 2 (>= 600 pm of the star on the "
                "purple): worst %d pm - %s\n",
                worst_on_purple_pm, r2 ? "OK" : "FAIL");
    std::printf("u02-probe: 5c LEASH rule 3 (no star vertex outside the BODY "
                "outline; orthographic, 2 shipping views): %d violations - %s\n",
                outside_body, r3 ? "OK" : "FAIL");
    if (!r1 || !r2 || !r3) rc = 1;
  }

  // ---- PASS 6 (Direction 5 5d): THE COMPOSED-EXTREMES GATE ---------------
  //
  // Four things now move on one face: the star's overhang past the rim (5c),
  // the eyeball's shift across the body (5c), the eye's roll (5d), and the
  // head's own motion under all three. EACH CAN PASS ITS OWN LIMIT WHILE THE
  // COMBINATION COLLIDES, so gating them one at a time is a check that cannot
  // fail. This project has already shipped exactly that shape of defect --
  // every automated gate green while a stray triangle sat in a creature's eye.
  //
  // The interaction is concrete rather than hypothetical: ROLLING INWARD MOVES
  // THE STAR'S OVERHANG DIRECTION. A star pressed against the inner rim, on a
  // lens rolled inward, reaches further toward the other eye than either rule
  // alone predicts -- and inward roll is simultaneously the collision case and
  // the most expressive direction, so it gets reached in practice.
  //
  // So this walks the composed CORNERS on the same frame: every sign
  // combination of {roll, gaze side, gaze lift, eyeball shift} at full
  // authored amplitude, both eyes, posed exactly as a clip would pose them.
  // Two prohibitions, both owner-stated, both gates:
  //   A. THE EYES NEVER TOUCH EACH OTHER. On the sheet their tops already
  //      nearly meet at the centre line, so this is a small margin by design.
  //   B. NOTHING CLIPS. The eyes pop out of a CURVED body, so a rolled lens can
  //      dig its FAR end in while its near end still looks fine -- the test is
  //      therefore the minimum depth over lens vertices, against the rest
  //      pose's own minimum, not against zero.
  {
    u02::Rig g;
    zc::Clip ex;
    const int kCorners = 16;
    ex.slot_id = 7;
    ex.frame_count = kCorners;
    ex.quats.assign(static_cast<size_t>(kCorners) * u02::kBoneCount, zc::quat16_identity());
    ex.root.assign(static_cast<size_t>(kCorners) * 3, 0);
    ex.deform.assign(static_cast<size_t>(kCorners), zc::DeformSample{});
    for (int i = 0; i < kCorners; ++i) {
      const int32_t sr = (i & 1) ? 1000 : -1000;   // roll
      const int32_t ss = (i & 2) ? 1000 : -1000;   // gaze side
      const int32_t sl = (i & 4) ? 1000 : -1000;   // gaze lift
      const int32_t sh = (i & 8) ? 1000 : -1000;   // eyeball shift
      g.reset();
      u02::loop_rest(g);
      u02::face_rest(g);
      u02::apply_eye_roll(g, sr, sr);
      u02::apply_gaze(g, u02::kGazeMaxA16 * ss / 1000,
                      u02::kGazeLiftMaxA16 * sl / 1000);
      u02::apply_eye_shift(g, sh, sh);
      g.write(ex, i);
      ex.root[static_cast<size_t>(i) * 3 + 1] = u02::fxu(u02::kHoverHeightMm);
    }
    const int32_t bx = u02::fxu(u02::kBodyRadiusMm);
    const int32_t by = u02::fxu(u02::vmm(u02::kBodyRadiusMm));
    // A: closest approach between the two eye assemblies, over the corners.
    // B: how deep the lens's deepest vertex sits, as a fraction of the body
    //    ellipsoid, against the SAME measure taken at rest.
    int32_t rest_min_ellip = 1 << 30;
    int32_t worst_min_ellip = 1 << 30;
    int64_t closest_mm = 1LL << 40;
    int worst_corner = -1, closest_corner = -1;
    int census_star = 0, census_lens = 0;
    for (int pass = 0; pass < 2; ++pass) {
      const int n = pass == 0 ? 1 : kCorners;
      for (int i = 0; i < n; ++i) {
        std::array<zc::mat3x4fx, zc::kMaxBones> pose;
        if (pass == 0) {
          u02::Rig r0;
          r0.reset();
          u02::loop_rest(r0);
          u02::face_rest(r0);
          zc::Clip rc0 = ex;
          r0.write(rc0, 0);
          zc::decode_pose(T, rc0, 0, pose, nullptr, 0);
        } else {
          zc::decode_pose(T, ex, static_cast<uint32_t>(i), pose, nullptr, 0);
        }
        const int32_t root_y = u02::fxu(u02::kHoverHeightMm);
        std::vector<std::array<int32_t, 3>> left, right;
        for (const zc::Meshlet& m : T.mesh) {
          const bool lens = (m.r == u02::kLensR && m.g == u02::kLensG && m.b == u02::kLensB);
          const bool star = (m.r == 246 && m.g == 242 && m.b == 250) ||
                            (m.r == u02::kStarR && m.g == u02::kStarG && m.b == u02::kStarB);
          if (!lens && !star) continue;
          for (const zc::SkinVertex& sv : m.verts) {
            int32_t x, y, z;
            zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
            if (lens) {
              const int64_t ex2 = (static_cast<int64_t>(x) << 16) / bx;
              const int64_t ey2 = (static_cast<int64_t>(y - root_y) << 16) / by;
              const int64_t ez2 = (static_cast<int64_t>(z) << 16) / bx;
              const int32_t e = static_cast<int32_t>(
                  (u02::isqrt64(ex2 * ex2 + ey2 * ey2 + ez2 * ez2) * 1000) >> 16);
              if (pass == 0) {
                if (e < rest_min_ellip) rest_min_ellip = e;
              } else if (e < worst_min_ellip) {
                worst_min_ellip = e;
                worst_corner = i;
              }
            }
            // WHICH EYE: taken GEOMETRICALLY from the sign of z, not from
            // sv.b0. The bone-id read looked obvious and was wrong -- it put
            // vertices from both eyes into the same bucket, so the gate
            // reported a 0 mm closest approach at every amplitude INCLUDING
            // zero roll, which is what exposed it. kBEyeL binds at +kEyeZMm and
            // no authored motion carries an eye across the centre line.
            if (pass == 1 && star) ++census_star;
            if (pass == 1 && lens) ++census_lens;
            (z > 0 ? left : right).push_back({x, y, z});
          }
        }
        if (pass == 1) {
          for (const auto& a : left)
            for (const auto& b : right) {
              // PASS 7: same units bug. The lenses sit ~130 mm apart, whose
              // raw fx16 difference is 8519 -- and 8519 >> 16 is 0. That, and
              // not the bone-id read, is why gate A reported a 0 mm closest
              // approach at every amplitude INCLUDING zero roll.
              const int64_t dx = ((static_cast<int64_t>(a[0]) - b[0]) * 1000) >> 16;
              const int64_t dy = ((static_cast<int64_t>(a[1]) - b[1]) * 1000) >> 16;
              const int64_t dz = ((static_cast<int64_t>(a[2]) - b[2]) * 1000) >> 16;
              const int64_t d2 = dx * dx + dy * dy + dz * dz;
              if (d2 < closest_mm) {
                closest_mm = d2;
                closest_corner = i;
              }
            }
        }
      }
    }
    const int64_t closest = u02::isqrt64(closest_mm);
    // The margins. Separation is a hard 0 with an honest apron: the sheet draws
    // the tops nearly meeting, so a large gate would be a lie about the design.
    constexpr int64_t kEyeSeparationMinMm = 12;
    // Depth: the composed extremes may not bury the lens meaningfully deeper
    // than the rest pose already does. Derived from the rest measurement in
    // this same run, so it cannot go stale the way a typed number would.
    // ⚠ REFORMULATED after the first version measured the WRONG THING. It
    // gated "no deeper than rest", which fails a roll that merely sinks the
    // lens further into an OPAQUE body -- invisible, and not a clip at all.
    // The lens is deliberately half-buried; what would actually show is the
    // base coming OUT and leaving a gap between lens and body. So the floor is
    // the body SURFACE: the deepest lens vertex must stay inside it. The crown
    // end is covered by the separate eye-protrusion gate above, so the two
    // together bracket the assembly at both ends.
    const int32_t depth_floor = 1000;
    // PASS 7: GATE A IS NOW ENFORCED. Pass 6 reported it and refused to
    // enforce it, on the correct instinct that an instrument returning 0 mm at
    // EVERY amplitude -- including a roll of exactly zero, where the lenses are
    // ~130 mm apart by construction -- was wrong about the geometry rather than
    // finding something. It was right not to tune the creature to satisfy it.
    //
    // The cause was units, not the bone-id read it replaced: the fx16
    // difference for a 130 mm gap is 8519, and a bare `>> 16` of that is 0.
    // (So the original sv.b0 read was correct all along; the geometric z-sign
    // split that replaced it is also correct, and is kept.) With `* 1000`
    // restored the gate reports a varying, sensible closest approach that
    // responds to roll -- so it now FAILS THE BUILD, as the owner's "shouldn't
    // touch each other" always intended.
    const bool sep_ok = closest >= kEyeSeparationMinMm;
    const bool depth_ok = worst_min_ellip < depth_floor;
    std::printf("u02-probe: 5d instrument census: %d white-star, %d lens verts\n",
                census_star, census_lens);
    std::printf("u02-probe: 5d EXTREMES (%d composed corners: roll x gaze-side x "
                "gaze-lift x eyeball-shift, both eyes, all at full amplitude)\n",
                kCorners);
    std::printf("u02-probe: 5d gate A -- eyes never touch: closest approach %lld mm "
                "(corner %d, floor %lld) - %s\n",
                static_cast<long long>(closest), closest_corner,
                static_cast<long long>(kEyeSeparationMinMm),
                sep_ok ? "OK" : "FAIL");
    std::printf("u02-probe: 5d channel state: roll SHIPPED (cap %d a16), gaze "
                "SHIPPED, eyeball-shift NOT SHIPPED (kEyeShiftPivotMm=%d, so "
                "eye_shift_a16(1000)=%d) -- Direction 5 5c's eyeball shift is a "
                "DECLARED GAP, not a silent one\n",
                u02::kEyeRollMaxA16, u02::kEyeShiftPivotMm,
                u02::eye_shift_a16(1000));
    std::printf("u02-probe: 5d gate B -- nothing clips: lens deepest %d pm of the "
                "body (corner %d) vs %d pm at rest, floor %d - %s\n",
                worst_min_ellip, worst_corner, rest_min_ellip, depth_floor,
                depth_ok ? "OK" : "FAIL");
    if (!sep_ok || !depth_ok) rc = 1;
  }

  // ---- PASS 4: the JUNCTION SURFACE-CROSSING report (Stage B) ------------
  //
  // The measurement side of ball siting (the probe finds the crossing; the
  // EYE places the ball): walk the posed loop centreline in the still pose
  // and report where it crosses the body ellipsoid — once near the front
  // junction (going up the lower tube) and once on the return arm. The
  // reported positions are ROOT-LOCAL mm, directly comparable to the bind
  // constants (kLoopNeckExitYMm, kKnuckleReentryOff*).
  {
    const int32_t rx = u02::fxu(u02::kBodyRadiusMm);
    const int32_t ry = u02::fxu(u02::vmm(u02::kBodyRadiusMm));
    const zc::Clip& still = T.bank.clips[7];
    std::array<zc::mat3x4fx, zc::kMaxBones> pose;
    zc::decode_pose(T, still, 0, pose, nullptr, 0);
    const zc::mat3x4fx& rm = pose[u02::kBRoot];
    const int32_t y0 = u02::kLoopNeckExitYMm - u02::kLoopBuryMm;
    const int32_t total = u02::kLoopBuryMm + u02::kLoopArcMm[0] + u02::kLoopArcMm[1] +
                          u02::kLoopArcMm[2] + u02::kLoopArcMm[3] + u02::kLoopArcMm[4] +
                          u02::kLoopArcMm[5];
    // dense centreline walk: 300 samples, blended exactly like make_loop
    int32_t prev_e = -1;
    int32_t prev_lx = 0, prev_ly = 0;
    int crossing = 0;
    for (int i = 0; i <= 300; ++i) {
      const int32_t sarc = static_cast<int32_t>((static_cast<int64_t>(total) * i) / 300);
      // which bone pair carries this station (make_loop's ladder, centreline)
      const int32_t stJF = u02::kLoopBuryMm;
      const int32_t stNeck = stJF + u02::kLoopArcMm[0];
      const int32_t stA = stNeck + u02::kLoopArcMm[1];
      const int32_t stB = stA + u02::kLoopArcMm[2];
      const int32_t stC = stB + u02::kLoopArcMm[3];
      const int32_t stD = stC + u02::kLoopArcMm[4];
      const int32_t blend = 145;
      const auto blend_of = [&](int32_t st) {
        int32_t t = ((sarc - (st - blend)) * 64) / (2 * blend);
        if (t < 0) t = 0;
        if (t > 64) t = 64;
        return t;
      };
      uint8_t b0, b1, w0;
      const int32_t tJ = blend_of(stJF), tN = blend_of(stNeck), tA = blend_of(stA),
                    tB = blend_of(stB), tC = blend_of(stC), tD = blend_of(stD);
      if (tN == 0) { b0 = u02::kBRoot; b1 = u02::kBJunctionF; w0 = static_cast<uint8_t>(64 - tJ); }
      else if (tA == 0) { b0 = u02::kBJunctionF; b1 = u02::kBNeck; w0 = static_cast<uint8_t>(64 - tN); }
      else if (tB == 0) { b0 = u02::kBNeck; b1 = u02::kBHingeA; w0 = static_cast<uint8_t>(64 - tA); }
      else if (tC == 0) { b0 = u02::kBHingeA; b1 = u02::kBHingeB; w0 = static_cast<uint8_t>(64 - tB); }
      else if (tD == 0) { b0 = u02::kBHingeB; b1 = u02::kBHingeC; w0 = static_cast<uint8_t>(64 - tC); }
      else { b0 = u02::kBHingeC; b1 = u02::kBHingeD; w0 = static_cast<uint8_t>(64 - tD); }
      zc::SkinVertex sv{};
      sv.x = u02::fxu(u02::kLoopTubeXMm);
      sv.y = u02::fxu(y0 + sarc);
      sv.z = 0;
      sv.b0 = b0; sv.b1 = b1; sv.w0 = w0;
      int32_t x, y, z;
      zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
      const int64_t dx = x - rm.m[3], dy = y - rm.m[7], dz = z - rm.m[11];
      const int64_t lx = (rm.m[0] * dx + rm.m[4] * dy + rm.m[8] * dz) >> 16;
      const int64_t ly = (rm.m[1] * dx + rm.m[5] * dy + rm.m[9] * dz) >> 16;
      const int64_t lz = (rm.m[2] * dx + rm.m[6] * dy + rm.m[10] * dz) >> 16;
      const int64_t ex = (lx << 16) / rx;
      const int64_t ey = (ly << 16) / ry;
      const int64_t ez = (lz << 16) / rx;
      const int32_t e = static_cast<int32_t>(
          (u02::isqrt64(ex * ex + ey * ey + ez * ez) * 1000) >> 16);
      const int32_t lmx = static_cast<int32_t>((lx * 1000) >> 16);
      const int32_t lmy = static_cast<int32_t>((ly * 1000) >> 16);
      if (prev_e >= 0 && ((prev_e < 1000) != (e < 1000))) {
        ++crossing;
        std::printf(
            "u02-probe: SURFACE CROSSING %d (%s) near root-local (%d, %d) mm "
            "(arc station %d mm; prev sample (%d, %d))\n",
            crossing, prev_e < 1000 ? "exiting" : "entering", lmx, lmy, sarc,
            prev_lx, prev_ly);
      }
      prev_e = e;
      prev_lx = lmx;
      prev_ly = lmy;
    }
  }
  // ---- PASS 4 (Stage T): the TRAVELLING-COLUMN probe ---------------------
  //
  // The reviewer's first ask: the reel ground-snaps the root with ONE
  // column_query at the fixed stage centre, so a clip with real lateral
  // travel inherits whatever the terrain does under its own path — the
  // flat-plane clearance bound is NOT conservative for a travelling clip
  // (it reported 432 mm while drift sank into the hillside). This probe
  // rebuilds the subject's own stage (rtest::bump_patch, the reel's law),
  // re-queries the column along each clip's root path, and asserts
  // clearance-minus-rise stays above the float gate. Travelling clips
  // stage FLAT (bump_ext 18, the walk precedent); everyone else keeps the
  // mound (bump_ext 6) and barely moves.
  {
    constexpr int32_t kMinClearanceMm2 = 40;
    for (const zc::Clip& clip : T.bank.clips) {
      // does this clip travel?  (root x/z span over the whole clip)
      int32_t min_x = INT32_MAX, max_x = INT32_MIN, min_z = INT32_MAX,
              max_z = INT32_MIN;
      for (uint16_t f = 0; f < clip.frame_count; ++f) {
        const int32_t x = clip.root[static_cast<size_t>(f) * 3 + 0];
        const int32_t z = clip.root[static_cast<size_t>(f) * 3 + 2];
        min_x = std::min(min_x, x); max_x = std::max(max_x, x);
        min_z = std::min(min_z, z); max_z = std::max(max_z, z);
      }
      const int32_t span_mm = static_cast<int32_t>(
          ((static_cast<int64_t>(max_x - min_x) + (max_z - min_z)) * 1000) >> 16);
      if (span_mm < 500) continue;  // fixed-position clip: centre column rules
      const bool flat_staged = clip.slot_id == 1 || clip.slot_id == 8;
      const int bump_ext = flat_staged ? 18 : 6;
      zref::render::TerrainPatch patch = rtest::bump_patch(161, 161, bump_ext, 8);
      const zref::terrain::ComposedLattice lat = zref::render::compose_lattice(
          patch, rtest::xform_identity(), {}, 0, nullptr, nullptr);
      const zref::terrain::ColumnResult c0 =
          zref::terrain::column_query(lat, zref::fx16{0}, zref::fx16{0});
      int32_t worst_rise = 0;
      uint16_t worst_key = 0;
      // per-key: min vertex world y (vs the snap plane) and the terrain
      // rise under the travelled root
      int32_t worst_eff = INT32_MAX;
      uint16_t worst_eff_key = 0;
      for (uint16_t f = 0; f < clip.frame_count; ++f) {
        const int32_t rx2 = clip.root[static_cast<size_t>(f) * 3 + 0];
        const int32_t rz2 = clip.root[static_cast<size_t>(f) * 3 + 2];
        const zref::terrain::ColumnResult cr = zref::terrain::column_query(
            lat, zref::fx16{rx2}, zref::fx16{rz2});
        const int32_t rise = cr.cls == zref::terrain::ColumnClass::kSolid &&
                                     c0.cls == zref::terrain::ColumnClass::kSolid
                                 ? cr.top.raw - c0.top.raw
                                 : 0;
        if (rise > worst_rise) {
          worst_rise = rise;
          worst_key = f;
        }
        int32_t key_min = INT32_MAX;
        std::array<zc::mat3x4fx, zc::kMaxBones> pose;
        zc::decode_pose(T, clip, f, pose, nullptr, 0);
        const zc::DeformSample d = zc::deformation_sample(T, clip.slot_id, f, 0);
        for (const zc::Meshlet& m : T.mesh) {
          for (size_t vi = 0; vi < m.verts.size(); ++vi) {
            zc::SkinVertex sv = m.verts[vi];
            if (!m.deform.empty()) sv = zc::deform_skin_vertex(sv, m.deform[vi], d);
            int32_t x, y, z;
            zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
            if (y < key_min) key_min = y;
          }
        }
        const int32_t eff = key_min - rise;
        if (eff < worst_eff) {
          worst_eff = eff;
          worst_eff_key = f;
        }
      }
      const int32_t rise_mm = static_cast<int32_t>((static_cast<int64_t>(worst_rise) * 1000) >> 16);
      const int32_t eff_mm = static_cast<int32_t>((static_cast<int64_t>(worst_eff) * 1000) >> 16);
      const bool ok = eff_mm >= kMinClearanceMm2;
      std::printf(
          "u02-probe: TRAVEL slot %u (span %d mm, bump_ext %d): max terrain rise "
          "%d mm under the path (key %u); worst clearance-minus-rise %d mm "
          "(key %u) — %s\n",
          clip.slot_id, span_mm, bump_ext, rise_mm, worst_key, eff_mm,
          worst_eff_key, ok ? "OK" : "FAIL");
      if (!ok) rc = 1;
    }
  }

  // ---- PASS 4 (Stage FOLD): the REST ANCHOR TABLE ------------------------
  // Root-local posed origins of the six fold anchors in the still pose --
  // the measurement side of kFoldAnchorRestUV (the authored table is the
  // knob; this print is the comparison).
  {
    const zc::Clip& still = T.bank.clips[7];
    std::array<zc::mat3x4fx, zc::kMaxBones> pose;
    zc::decode_pose(T, still, 0, pose, nullptr, 0);
    const zc::mat3x4fx& rm = pose[u02::kBRoot];
    const uint8_t bones[6] = {u02::kBJunctionF, u02::kBNeck, u02::kBHingeA,
                              u02::kBHingeB, u02::kBHingeC, u02::kBLoopBase2};
    const char* names[6] = {"junctionF", "neck", "hingeA", "hingeB", "hingeC",
                            "junctionB"};
    for (int i = 0; i < 6; ++i) {
      // the posed bone ORIGIN = a vertex at the bone's bind position skinned
      // with full weight on that bone (the reel's own FxAnchors law)
      const uint8_t b = bones[i];
      zc::SkinVertex sv{T.baked.world_x[b], T.baked.world_y[b],
                        T.baked.world_z[b], b, b, 64, 0, 0};
      int32_t x, y, z;
      zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
      const int64_t dx = x - rm.m[3], dy = y - rm.m[7], dz = z - rm.m[11];
      const int64_t lx = (rm.m[0] * dx + rm.m[4] * dy + rm.m[8] * dz) >> 16;
      const int64_t ly = (rm.m[1] * dx + rm.m[5] * dy + rm.m[9] * dz) >> 16;
      const int64_t lz = (rm.m[2] * dx + rm.m[6] * dy + rm.m[10] * dz) >> 16;
      std::printf("u02-probe: REST ANCHOR %-9s root-local (%5d, %5d, %5d) mm\n",
                  names[i], static_cast<int32_t>((lx * 1000) >> 16),
                  static_cast<int32_t>((ly * 1000) >> 16),
                  static_cast<int32_t>((lz * 1000) >> 16));
    }
  }
  return rc;
}
