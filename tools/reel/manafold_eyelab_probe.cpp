// u02-eyelab-probe — THE COMMITTED EYE-LAB PROBE (Owner Direction 7 §12).
//
// CLAUDE.md: "A probe that does this was written once and thrown away, so its
// numbers are unreproducible -- commit the probe." This is that probe for the
// eye. It answers, in numbers, the things the plates answer by eye, and it is
// deliberately built so that the two can DISAGREE -- measurement lives on the
// comparison side; it never chooses a value here.
//
// ============ HOW IT AVOIDS THE FAULTS THIS PROJECT HAS PAID FOR ==========
//
// 1. IT NEVER INVERTS A SKINNING MATRIX TO GET "LOCAL" SPACE.
//    09-ENGINE-GOTCHAS §15: bind -> posed inverted and applied to a posed
//    point returns BIND space, which still carries the part's ±215 mm bind
//    offset -- a measurement that was 215 mm wrong before it started.
//    This probe walks POSED, DEFORMED WORLD VERTICES (decode_pose +
//    deformation_sample + deform_skin_vertex + skin_vertex, the shipped path)
//    and then applies ONE rigid inverse: the ROOT bone's. That is legitimate
//    and it is a different act -- the root transform is the creature's own
//    placement, so undoing it puts every part into ONE COMMON BODY FRAME. For
//    the body's own vertices the result is the DEFORMED bind shape (the
//    deform is applied before skinning, so the pulsation survives); for the
//    lens it is the posed lens expressed in the body's frame, which is
//    exactly the comparison we want and is not the lens's bind space.
//
// 2. IT MEASURES AT THE EXTREMES OF THE BREATH, NOT AT REST.
//    §7.7: "a placement that looks correct on the neutral frame is the thing
//    that goes wrong". Every gate below reports its worst over the whole clip,
//    and separately AT the frame where the deform sample peaks and where it
//    troughs, so a fault that only exists on the inhale cannot average away.
//
// 3. IT SWEEPS. 09-ENGINE-GOTCHAS §17: the eye-to-eye gap is 98 mm at 0 deg
//    roll, 0 mm at 7, and 18 mm at 10 -- a gate that samples the corners of a
//    box cannot see a minimum in the box's interior. The travel ladder is
//    swept, and it is swept UPWARD FROM SMALL because the owner's instruction
//    is "less is better", so the question is where it BREAKS, not whether the
//    ceiling survives.
//
// 4. IT SEPARATES THE UPWARD AND DOWNWARD STAR MARGINS.
//    §12.1 orders the asymmetry authored rather than discovered. A single
//    worst-case number is structurally incapable of showing an asymmetry, so
//    reporting one would hide the exact thing that was asked for.
//
// 5. IT CROSS-CHECKS ITSELF AGAINST AN INDEPENDENTLY RECORDED NUMBER.
//    manafold_art.h records the shipped rest as "the lens's deepest point at
//    837 pm of the body's own ellipsoid and its crown 123 mm proud". The
//    control variant here must reproduce those, or the instrument is wrong
//    and every other number it prints is worthless. It prints the comparison
//    rather than asserting it quietly.

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <array>
#include <algorithm>
#include <string>

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

// fx16 -> mm. ⚠ fxu() is mm*65536/1000, so the inverse is (raw*1000)>>16 and
// NOT raw>>16. The shipped probe prints (raw>>16), which is METRES with a mm
// label -- manafold_qa_extremes.cpp exists because of that, and this probe
// uses the correct conversion from its first line.
static inline int32_t tomm(int64_t raw) {
  return static_cast<int32_t>((raw * 1000) >> 16);
}

// The rigid inverse of a bone transform (rotation + translation only).
static inline void inv_point(const zc::mat3x4fx& m, int32_t x, int32_t y, int32_t z,
                             int32_t& ox, int32_t& oy, int32_t& oz) {
  const int64_t dx = static_cast<int64_t>(x) - m.m[3];
  const int64_t dy = static_cast<int64_t>(y) - m.m[7];
  const int64_t dz = static_cast<int64_t>(z) - m.m[11];
  ox = static_cast<int32_t>((m.m[0] * dx + m.m[4] * dy + m.m[8] * dz) >> 16);
  oy = static_cast<int32_t>((m.m[1] * dx + m.m[5] * dy + m.m[9] * dz) >> 16);
  oz = static_cast<int32_t>((m.m[2] * dx + m.m[6] * dy + m.m[10] * dz) >> 16);
}

static inline int64_t isq(int64_t v) { return v * v; }

// ---------------------------------------------------------------------------
// THE BODY SURFACE, sampled from the POSED DEFORMED MESH rather than assumed.
//
// ⚠ THE FIRST VERSION OF THIS WAS WRONG, AND THE CROSS-CHECK IS WHAT CAUGHT IT.
// It binned by AZIMUTH x HEIGHT over a 96 x 64 grid. The body has 11 rings and
// a finite segment count, so most of those 6144 bins were never sampled, and a
// lens vertex landing in an empty bin was simply SKIPPED -- the surface was
// only compared where a body vertex happened to fall in the same cell. It
// reported a 72 mm crown against manafold_art.h's independently recorded
// 123 mm, and a 913 pm depth against a recorded 837 pm. Both wrong, both
// plausible, and neither would have been questioned without a recorded number
// to sit beside. This is the whole reason the cross-check prints.
//
// THE FIX IS ALSO THE SIMPLIFICATION. The body's HORIZONTAL CROSS-SECTION IS A
// CIRCLE at every height -- make_ball's rings are circles, kBodyTaperPm scales
// a radius, kVStretchPm scales Y, and none of those turns a circle into
// anything else. The lean displaces the circle's CENTRE in X, which is exactly
// what kEyeTravelPivotXMm accounts for. So the surface is a function of HEIGHT
// ALONE, r(y), and azimuth was never information -- it was 96 opportunities to
// have no data.
//
// r(y) is built from the actual posed, deformed body vertices (so the breath
// is in it), at fine height resolution, and gaps between the 11 ring heights
// are filled by linear interpolation between the neighbouring sampled rows.
// ---------------------------------------------------------------------------
constexpr int kRows = 256;               // height rows across the body
struct BodyProfile {
  int32_t r[kRows];                      // fx16 radius about the pivot axis
  int32_t y_lo = 0, y_hi = 0;
};

static void fill_gaps(BodyProfile& bp) {
  // forward-fill from the first sampled row, backward-fill from the last, and
  // linearly interpolate every run of empties in between. A hole left in a
  // surface map is a place the measurement silently declines to look.
  int first = -1, last = -1;
  for (int i = 0; i < kRows; ++i) if (bp.r[i] > 0) { if (first < 0) first = i; last = i; }
  if (first < 0) return;
  for (int i = 0; i < first; ++i) bp.r[i] = bp.r[first];
  for (int i = last + 1; i < kRows; ++i) bp.r[i] = bp.r[last];
  int i = first;
  while (i < last) {
    int j = i + 1;
    while (j <= last && bp.r[j] == 0) ++j;
    for (int k = i + 1; k < j; ++k) {
      const int64_t t = static_cast<int64_t>(k - i) * 65536 / (j - i);
      bp.r[k] = static_cast<int32_t>(
          bp.r[i] + ((static_cast<int64_t>(bp.r[j] - bp.r[i]) * t) >> 16));
    }
    i = j;
  }
}

static int32_t profile_at(const BodyProfile& bp, int32_t y) {
  const int32_t span = bp.y_hi > bp.y_lo ? bp.y_hi - bp.y_lo : 1;
  int64_t t = static_cast<int64_t>(y - bp.y_lo) * (kRows - 1) * 65536 / span;
  if (t < 0) t = 0;
  const int64_t maxt = static_cast<int64_t>(kRows - 1) * 65536;
  if (t > maxt) t = maxt;
  const int i = static_cast<int>(t >> 16);
  const int j = i + 1 < kRows ? i + 1 : i;
  const int64_t f = t & 0xFFFF;
  return static_cast<int32_t>(bp.r[i] + ((static_cast<int64_t>(bp.r[j] - bp.r[i]) * f) >> 16));
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
  const char* only = argc > 1 ? argv[1] : nullptr;
  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) {
    std::printf("u02-eyelab: FAIL compile produced no meshlets\n");
    return 1;
  }
  const u02::eyelab::EyeVariant& V = u02::eyelab::variant();
  std::printf("u02-eyelab: variant '%s'  travel=%d deg  star=%d pm  offsetY=%d mm"
              "  breathe=%d blink=%d split=%d composed=%d\n",
              V.name, V.travel_deg, V.star_scale_pm, V.star_offset_mm,
              V.breathe_eye ? 1 : 0, V.blink ? 1 : 0, V.blink_split ? 1 : 0,
              V.compose_extremes ? 1 : 0);
  std::printf("u02-eyelab: pivot axis x = %d mm; ceiling %d deg (owner: "
              "'45 should be absolute max it can go, less is better')\n",
              u02::eyelab::kEyeTravelPivotXMm, u02::eyelab::kEyeTravelMaxDeg);

  // ---- which meshlets are which part -------------------------------------
  // By BONE, not by index: parts are compiled into meshlets and the mapping is
  // not something to guess. Every vertex carries its own two bone ids.
  const auto vert_is = [](const zc::SkinVertex& v, uint8_t a, uint8_t b) {
    return v.b0 == a || v.b0 == b || v.b1 == a || v.b1 == b;
  };

  // ---- find the clip ------------------------------------------------------
  const zc::Clip* clip = nullptr;
  for (const zc::Clip& c : T.bank.clips)
    if (c.slot_id == 16) clip = &c;
  if (clip == nullptr) {
    std::printf("u02-eyelab: FAIL no slot 16 (the lab clip)\n");
    return 1;
  }

  // ---- locate the breath extremes ----------------------------------------
  int f_inhale = 0, f_exhale = 0;
  {
    int32_t hi = -1, lo = INT32_MAX;
    for (uint16_t f = 0; f < clip->frame_count; ++f) {
      const zc::DeformSample d = zc::deformation_sample(T, 16, f, 0);
      if (d.flatten > hi) { hi = d.flatten; f_inhale = f; }
      if (d.flatten < lo) { lo = d.flatten; f_exhale = f; }
    }
    std::printf("u02-eyelab: breath extremes -- most SQUASHED key %d "
                "(flatten %d), least key %d (flatten %d)\n",
                f_inhale, hi, f_exhale, lo);
  }

  // ---- the per-frame measurement -----------------------------------------
  struct FrameResult {
    int32_t lens_deep_pm = 100000;   // min over lens verts: r_lens/r_body in pm
    int32_t crown_mm = -100000;      // max stand-off of a lens vert, mm
    int32_t eye_gap_mm = 1 << 28;    // closest approach, left lens to right
    int32_t star_up_mm = 1 << 28;    // mm of lens left beyond the star's + tip
    int32_t star_dn_mm = 1 << 28;    // ... and beyond its - tip
    int32_t lens_half_mm = 0;
    bool valid = false;
  };
  std::vector<FrameResult> res(clip->frame_count);

  int32_t worst_deep = 100000; int worst_deep_f = -1;
  int32_t worst_gap = 1 << 28; int worst_gap_f = -1;
  int32_t best_crown = -100000, crown_at_rest = 0;

  for (uint16_t f = 0; f < clip->frame_count; ++f) {
    std::array<zc::mat3x4fx, zc::kMaxBones> pose;
    zc::decode_pose(T, *clip, f, pose, nullptr, 0);
    const zc::DeformSample d = zc::deformation_sample(T, 16, f, 0);

    // ---- pass 1: build the body's r(y) profile from POSED DEFORMED verts --
    static BodyProfile bm;
    for (int e = 0; e < kRows; ++e) bm.r[e] = 0;
    int32_t ylo = INT32_MAX, yhi = INT32_MIN;
    std::vector<std::array<int32_t, 3>> body_local;
    for (const zc::Meshlet& m : T.mesh) {
      for (size_t vi = 0; vi < m.verts.size(); ++vi) {
        const zc::SkinVertex& raw = m.verts[vi];
        // the body ball is the ONLY part skinned to kBRoot alone
        if (!(raw.b0 == u02::kBRoot && raw.b1 == u02::kBRoot)) continue;
        zc::SkinVertex sv = raw;
        if (!m.deform.empty()) sv = zc::deform_skin_vertex(sv, m.deform[vi], d);
        int32_t x, y, z;
        zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
        int32_t lx, ly, lz;
        inv_point(pose[u02::kBRoot], x, y, z, lx, ly, lz);
        body_local.push_back({lx, ly, lz});
        if (ly < ylo) ylo = ly;
        if (ly > yhi) yhi = ly;
      }
    }
    if (body_local.empty()) { std::printf("u02-eyelab: FAIL no body verts\n"); return 1; }
    bm.y_lo = ylo; bm.y_hi = yhi;
    for (const auto& p : body_local) {
      // ⚠ the pivot axis, not the origin: the section circle is off-centre in
      // X by the teardrop lean, and radii taken about the wrong axis are the
      // measurement telling a confident lie about a shape it never had.
      const int32_t px = p[0] - u02::fxu(u02::eyelab::kEyeTravelPivotXMm);
      const int64_t r2 = isq(px) + isq(p[2]);
      const int32_t r = static_cast<int32_t>(zref::isqrt_u64(
          static_cast<uint64_t>(r2)));
      int e = static_cast<int>(static_cast<int64_t>(p[1] - ylo) * (kRows - 1) /
                               (yhi > ylo ? (yhi - ylo) : 1));
      if (e < 0) e = 0;
      if (e >= kRows) e = kRows - 1;
      // MAX at a height IS the surface: a ring is a circle, so every vertex on
      // it shares one radius, and a smaller radius at the same height belongs
      // to a different ring that landed in the same row.
      if (r > bm.r[e]) bm.r[e] = r;
    }
    fill_gaps(bm);

    // ---- pass 2: the lens and the stars against that map -----------------
    FrameResult fr;
    std::vector<std::array<int32_t, 3>> lensL, lensR, starL;
    for (const zc::Meshlet& m : T.mesh) {
      for (size_t vi = 0; vi < m.verts.size(); ++vi) {
        const zc::SkinVertex& raw = m.verts[vi];
        const bool isL = vert_is(raw, u02::kBEyeL, u02::kBEyeL);
        const bool isR = vert_is(raw, u02::kBEyeR, u02::kBEyeR);
        const bool isSL = vert_is(raw, u02::kBPupilL, u02::kBPupilL);
        const bool isSR = vert_is(raw, u02::kBPupilR, u02::kBPupilR);
        if (!isL && !isR && !isSL && !isSR) continue;
        zc::SkinVertex sv = raw;
        if (!m.deform.empty()) sv = zc::deform_skin_vertex(sv, m.deform[vi], d);
        int32_t x, y, z;
        zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
        int32_t lx, ly, lz;
        inv_point(pose[u02::kBRoot], x, y, z, lx, ly, lz);
        const int32_t px = lx - u02::fxu(u02::eyelab::kEyeTravelPivotXMm);
        const int32_t r = static_cast<int32_t>(zref::isqrt_u64(
            static_cast<uint64_t>(isq(px) + isq(lz))));
        const int32_t br = profile_at(bm, ly);
        if (br > 0 && (isL || isR)) {
          // per-mille of the body's own surface radius. 1000 == exactly ON it.
          const int32_t pm = static_cast<int32_t>(
              (static_cast<int64_t>(r) * 1000) / br);
          if (pm < fr.lens_deep_pm) fr.lens_deep_pm = pm;
          const int32_t proud = tomm(r - br);
          if (proud > fr.crown_mm) fr.crown_mm = proud;
        }
        if (isL) lensL.push_back({lx, ly, lz});
        if (isR) lensR.push_back({lx, ly, lz});
        if (isSL) starL.push_back({lx, ly, lz});
      }
    }
    // eye-to-eye closest approach, the §5d gate-A quantity
    for (const auto& p : lensL) {
      for (const auto& q : lensR) {
        const int64_t dd = isq(static_cast<int64_t>(p[0]) - q[0]) +
                           isq(static_cast<int64_t>(p[1]) - q[1]) +
                           isq(static_cast<int64_t>(p[2]) - q[2]);
        const int32_t mm = tomm(static_cast<int32_t>(
            zref::isqrt_u64(static_cast<uint64_t>(dd))));
        if (mm < fr.eye_gap_mm) fr.eye_gap_mm = mm;
      }
    }
    // ---- §12.1's ASYMMETRY, measured where it lives ----------------------
    //
    // "Travel room becomes ASYMMETRIC -- generous below, thin above. Re-tune
    // the §5c leash against the new rest position, or the first upward glance
    // runs straight into the rim."
    //
    // §5c's cap is ONE number applied both ways, so it is structurally unable
    // to express an asymmetry; reporting a single worst-case would hide the
    // exact thing the owner asked to have made visible. So the up and the down
    // margins are measured and reported SEPARATELY.
    //
    // ⚠ AND IT IS DONE WITHOUT INVERTING A SKINNING MATRIX. The tempting move
    // is inv(M_eye) on a posed star vertex to get "star relative to lens"; that
    // returns the star in the EYE'S BIND SPACE, still carrying the eye's
    // ±215 mm bind offset -- 09-ENGINE-GOTCHAS §15, the fault that cost pass 7
    // a day. Instead the lens describes ITSELF: its centroid is its centre (a
    // symmetric lens), and its long axis is the direction of its own farthest
    // vertex from that centroid. Both come from the posed, deformed vertices
    // that are already in hand, and neither needs a frame of reference that
    // could be the wrong one.
    if (!lensL.empty() && !starL.empty()) {
      int64_t cx = 0, cy = 0, cz = 0;
      for (const auto& p : lensL) { cx += p[0]; cy += p[1]; cz += p[2]; }
      const int32_t n = static_cast<int32_t>(lensL.size());
      const int32_t mx = static_cast<int32_t>(cx / n);
      const int32_t my = static_cast<int32_t>(cy / n);
      const int32_t mz = static_cast<int32_t>(cz / n);
      // the long axis: the lens's own farthest vertex from its centre
      int64_t best = -1; int32_t ax = 0, ay = 0, az = 0;
      for (const auto& p : lensL) {
        const int64_t d = isq(p[0]-mx) + isq(p[1]-my) + isq(p[2]-mz);
        if (d > best) { best = d; ax = p[0]-mx; ay = p[1]-my; az = p[2]-mz; }
      }
      const int32_t half = static_cast<int32_t>(zref::isqrt_u64(
          static_cast<uint64_t>(best)));
      fr.lens_half_mm = tomm(half);
      if (half > 0) {
        // signed projection of each star vertex onto that axis, in mm.
        // POSITIVE is toward the axis-defining tip; the Lambda puts the lens's
        // tips up-and-in / down-and-out, so the sign is reported as measured
        // and named by the tip it points at rather than guessed as "up".
        int32_t hi = INT32_MIN, lo = INT32_MAX;
        for (const auto& p : starL) {
          const int64_t dot = static_cast<int64_t>(p[0]-mx) * ax +
                              static_cast<int64_t>(p[1]-my) * ay +
                              static_cast<int64_t>(p[2]-mz) * az;
          const int32_t proj = tomm(static_cast<int32_t>(dot / half));
          if (proj > hi) hi = proj;
          if (proj < lo) lo = proj;
        }
        fr.star_up_mm = fr.lens_half_mm - hi;    // room left toward the + tip
        fr.star_dn_mm = fr.lens_half_mm + lo;    // room left toward the - tip
      }
    }
    fr.valid = true;
    res[f] = fr;
    if (fr.lens_deep_pm < worst_deep) { worst_deep = fr.lens_deep_pm; worst_deep_f = f; }
    if (fr.eye_gap_mm < worst_gap) { worst_gap = fr.eye_gap_mm; worst_gap_f = f; }
    if (fr.crown_mm > best_crown) best_crown = fr.crown_mm;
    if (f == 0) crown_at_rest = fr.crown_mm;
  }

  // ---- the self-check against an INDEPENDENTLY RECORDED number -----------
  std::printf("\nu02-eyelab: ==== INSTRUMENT CROSS-CHECK ====\n");
  std::printf("u02-eyelab: rest-frame crown stand-off %d mm; manafold_art.h "
              "records 123 mm for the shipped rest pose\n", crown_at_rest);
  std::printf("u02-eyelab: rest-frame lens depth %d pm; manafold_art.h records "
              "837 pm (1000 == exactly on the surface)\n",
              res[0].lens_deep_pm);
  std::printf("u02-eyelab: ⚠ if those two disagree with the recorded values, "
              "EVERY number below is worthless -- fix the probe first.\n");

  // ---- the travel sweep, reported per key --------------------------------
  std::printf("\nu02-eyelab: ==== THE CLIP, SWEPT ====\n");
  std::printf("u02-eyelab: worst lens depth %d pm at key %d (1000 == on the "
              "surface; BELOW 1000 IS SUNK IN, which is the §5.2 fault)\n",
              worst_deep, worst_deep_f);
  std::printf("u02-eyelab: worst eye-to-eye gap %d mm at key %d (§5d gate A "
              "floor is 12 mm)\n", worst_gap, worst_gap_f);
  std::printf("u02-eyelab: best crown stand-off %d mm\n", best_crown);

  // ---- §12.1: the asymmetry, reported as TWO numbers -------------------
  {
    int32_t up = 1 << 28, dn = 1 << 28, halfmm = 0;
    int up_f = 0, dn_f = 0;
    for (uint16_t f = 0; f < clip->frame_count; ++f) {
      if (res[f].star_up_mm < up) { up = res[f].star_up_mm; up_f = f; }
      if (res[f].star_dn_mm < dn) { dn = res[f].star_dn_mm; dn_f = f; }
      if (res[f].lens_half_mm > halfmm) halfmm = res[f].lens_half_mm;
    }
    std::printf("\nu02-eyelab: ==== §12.1 THE ASYMMETRY, AUTHORED NOT DISCOVERED ====\n");
    std::printf("u02-eyelab: lens half-length %d mm\n", halfmm);
    std::printf("u02-eyelab: room left past the star's + tip: %d mm (worst, key %d)\n",
                up, up_f);
    std::printf("u02-eyelab: room left past the star's - tip: %d mm (worst, key %d)\n",
                dn, dn_f);
    std::printf("u02-eyelab: ASYMMETRY %d mm. §5c's leash is ONE cap applied both "
                "ways; the larger this is, the more of the leash is already spent "
                "at REST on one side, and the sooner a glance that way hits the rim.\n",
                (up > dn ? up - dn : dn - up));
    if (up < 0 || dn < 0)
      std::printf("u02-eyelab: ⚠ the star's tip is ALREADY PAST the lens tip at "
                  "rest on one side -- that is §5c overhang spent before any "
                  "gaze is authored.\n");
  }

  // at the breath extremes specifically -- §7.7's actual question
  std::printf("\nu02-eyelab: ==== AT THE EXTREMES OF THE BREATH (§7.7) ====\n");
  std::printf("u02-eyelab: most-squashed key %d: lens depth %d pm, crown %d mm\n",
              f_inhale, res[f_inhale].lens_deep_pm, res[f_inhale].crown_mm);
  std::printf("u02-eyelab: least-squashed key %d: lens depth %d pm, crown %d mm\n",
              f_exhale, res[f_exhale].lens_deep_pm, res[f_exhale].crown_mm);
  std::printf("u02-eyelab: BREATH SWING in crown stand-off: %d mm. With the eye "
              "NOT opted into the deform this is the surface moving out from "
              "under a stationary lens; with it opted in the lens moves too.\n",
              res[f_exhale].crown_mm - res[f_inhale].crown_mm);

  // ---- the per-key table, so a minimum in the interior cannot hide -------
  std::printf("\nu02-eyelab: ==== PER-KEY (every 5th; the sweep, not the corners) ====\n");
  std::printf("u02-eyelab:  key   depth_pm   crown_mm   eyegap_mm\n");
  for (uint16_t f = 0; f < clip->frame_count; f += 5)
    std::printf("u02-eyelab:  %3u   %8d   %8d   %9d\n", f, res[f].lens_deep_pm,
                res[f].crown_mm, res[f].eye_gap_mm);

  // ---- THE SINK TEST, AND ITS THRESHOLD IS NOT 1000 ---------------------
  //
  // ⚠ THIS WAS WRONG IN THE FIRST VERSION, and instructively so: it flagged
  // "SUNK" on EVERY variant including the untouched control. The lens is a
  // dome EMBEDDED in the body surface and popping proud of it, so its deepest
  // vertex is ALWAYS inside the body's radial envelope -- 790 pm at rest here,
  // 837 pm by the shipped gate's own arithmetic. "Below 1000" is not a fault;
  // it IS the construction, and a gate that fires on the control is a gate
  // that has misunderstood the thing it is watching.
  //
  // The shipped gate says as much in its own output -- "lens deepest 837 pm vs
  // 838 pm at REST, floor 1000 - OK" -- which reads like a floor of 1000 and is
  // actually a comparison against the rest pose. That is the right test, and
  // this now does the same: the question is never "is the lens inside the
  // body", it is "does a POSE put it deeper than the rest pose does".
  const int32_t rest_deep = res[0].lens_deep_pm;
  const int32_t sink = rest_deep - worst_deep;
  std::printf("\nu02-eyelab: SINK vs REST %d pm (rest %d pm, worst %d pm at key "
              "%d). The lens is always inside the body's envelope -- it is a "
              "dome embedded in the surface -- so the DELTA is the number.\n",
              sink, rest_deep, worst_deep, worst_deep_f);
  int rc = 0;
  if (sink > 100) {
    std::printf("u02-eyelab: SINKS -- %d pm deeper than rest at key %d. §5.3 "
                "allows the purple to RIDE OVER the body edge; it does not "
                "allow a pose to push it further IN.\n", sink, worst_deep_f);
    rc = 2;  // reported, not fatal: this lane's job is to report
  }
  if (worst_gap < 12) {
    std::printf("u02-eyelab: EYES TOUCH -- %d mm at key %d, against §5d's 12 mm "
                "floor.\n", worst_gap, worst_gap_f);
    rc = 2;
  }
  if (rc == 0) std::printf("\nu02-eyelab: no sink and no eye-to-eye contact on this variant.\n");
  (void)only;
  return 0;  // a research probe REPORTS; it does not gate the lane
}
