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
  // re-lengthening kPupilStarArmShortMm re-breaks star-in-lens containment.
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
        {"white ring", 246, 242, 250, 0}};
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
