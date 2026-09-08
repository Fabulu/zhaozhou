// manafold_express.cpp -- THE EXPRESSIVENESS PLATE, Manafold against Zixxtrixx.
// (Manafold pass 12 wave 3, Owner Direction 5 SS6 / Direction 2 SS4.)
//
// WHY THIS EXISTS. The owner wrote one sentence and it has been repeated as a
// design goal in six passes without ever being checked:
//
//   "the creature is expressive. It uses the ball hinges and plays around with
//    them, which also shapes the mana within. It's bouncy, its body stretches,
//    inhales, exhales, EVEN MORESO THAN ZIXXTRIXX. It's such a simple creature
//    it must make up for it in expressiveness."
//
// OWNER-INVENTORY B6 records the state exactly: "The machinery exists (deform
// sidecar, whole_wobble, antenna_knead), but the comparison has NEVER BEEN
// MADE." Every pass has asserted the claim; no pass has shown it. This tool is
// not a feature and it is not a gate on new work -- it is the VERIFICATION the
// owner asked for, and it is written so that it can REFUTE the claim as easily
// as support it. If Manafold loses, the number says so and the findings say so.
//
// *** WHAT IT MEASURES, AND WHY IT IS THE THING RATHER THAN A PROJECTION ***
//
// "Its body stretches, inhales, exhales" is the DEFORM channel: both creatures
// carry a deform sidecar per clip and both bake radial deform metadata onto
// their body rings. So the measurement is the deform stage's own output, in
// bind-space millimetres, read through the production entry point:
//
//     zc::deformation_frame(type, slot, frame)      the production lane resolve
//     zc::deform_skin_vertex_lanes(v, meta, frame)  the production deform
//
// and NOT through the pose. That is deliberate: including the pose would let a
// creature that merely swings a lot score as one that breathes a lot, and the
// owner's sentence is specifically about the BODY stretching. CLAUDE.md's rule
// 3 -- "measure things that ARE the thing, never a projection of them" -- is
// the reason this reads the deform in millimetres instead of counting silhouette
// pixels in a render, which would conflate breath with rotation and with
// perspective. The rendered pair is built alongside it, for the eye, because
// rule 7 says a passing number is not likeness evidence.
//
// The extent is taken along ALL THREE bind axes and the WIDEST swing wins, so
// nothing here has to know which axis a given creature chose to squash along.
// (Manafold authors deform_axis 1, Zixxtrixx authors 2 and 1 -- if that were
// hardcoded here it would be a same-named-constant read, checklist item 10.)
//
// "Bouncy" is the second half and it is a different quantity: the root's own
// vertical travel across the clip, normalised by the creature's rest body
// height so a big animal and a small one are comparable.
//
// *** WHAT IT DELIBERATELY DOES NOT DO ***
// It does not pick a favourable clip. It walks EVERY clip in both banks and
// reports the median and the best, because "cherry-pick the one clip where we
// win" is the shape this comparison would fail in.
//
// Usage:
//   manafold-express.exe          the plate table + the verdict
//   manafold-express.exe --csv    per-frame body-extent traces, both creatures

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
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
constexpr int32_t fxm(int64_t milli) {
  return static_cast<int32_t>((milli * 65536 + (milli >= 0 ? 500 : -500)) / 1000);
}
#include "zixxtrixx.h"
#include "manafold.h"

namespace {

double mm(int32_t fx) { return static_cast<double>(fx) * 1000.0 / 65536.0; }

struct ClipScore {
  uint16_t slot = 0;
  int frames = 0;
  double stretch_pct = 0;   // widest bind-axis extent swing, % of rest extent
  double bob_mm = 0;        // root vertical travel over the clip
  double bob_pct = 0;       // ...as a % of rest body height
};

/** Rest extent of the deforming body along each bind axis, and the count. */
void rest_extent(const zc::CreatureType& T, double ext[3], int& n) {
  double lo[3] = {1e18, 1e18, 1e18}, hi[3] = {-1e18, -1e18, -1e18};
  n = 0;
  for (const zc::Meshlet& m : T.mesh) {
    if (m.deform.empty()) continue;
    for (size_t vi = 0; vi < m.verts.size(); ++vi) {
      if (m.deform[vi].role == zc::DeformRole::kNone) continue;
      const double p[3] = {mm(m.verts[vi].x), mm(m.verts[vi].y), mm(m.verts[vi].z)};
      for (int a = 0; a < 3; ++a) {
        if (p[a] < lo[a]) lo[a] = p[a];
        if (p[a] > hi[a]) hi[a] = p[a];
      }
      ++n;
    }
  }
  for (int a = 0; a < 3; ++a) ext[a] = (n > 0) ? hi[a] - lo[a] : 0.0;
}

/** Deform-only extent of the body at one frame, along each bind axis. */
void frame_extent(const zc::CreatureType& T, const zc::Clip& c, uint16_t f, double ext[3]) {
  const zc::DeformFrame fr = zc::deformation_frame(T, c.slot_id, f, 0);
  double lo[3] = {1e18, 1e18, 1e18}, hi[3] = {-1e18, -1e18, -1e18};
  bool any = false;
  for (const zc::Meshlet& m : T.mesh) {
    if (m.deform.empty()) continue;
    for (size_t vi = 0; vi < m.verts.size(); ++vi) {
      if (m.deform[vi].role == zc::DeformRole::kNone) continue;
      const zc::SkinVertex d = zc::deform_skin_vertex_lanes(m.verts[vi], m.deform[vi], fr);
      const double p[3] = {mm(d.x), mm(d.y), mm(d.z)};
      for (int a = 0; a < 3; ++a) {
        if (p[a] < lo[a]) lo[a] = p[a];
        if (p[a] > hi[a]) hi[a] = p[a];
      }
      any = true;
    }
  }
  for (int a = 0; a < 3; ++a) ext[a] = any ? hi[a] - lo[a] : 0.0;
}

/** The rest bounding height of the WHOLE creature, to normalise the bob. */
double body_height_mm(const zc::CreatureType& T) {
  double lo[3] = {1e18, 1e18, 1e18}, hi[3] = {-1e18, -1e18, -1e18};
  for (const zc::Meshlet& m : T.mesh)
    for (const zc::SkinVertex& v : m.verts) {
      const double p[3] = {mm(v.x), mm(v.y), mm(v.z)};
      for (int a = 0; a < 3; ++a) {
        if (p[a] < lo[a]) lo[a] = p[a];
        if (p[a] > hi[a]) hi[a] = p[a];
      }
    }
  // The creatures are authored on different up-axes, so "height" is the LONGEST
  // rest dimension: a scale for normalising, not a claim about anatomy.
  double best = 0;
  for (int a = 0; a < 3; ++a)
    if (hi[a] - lo[a] > best) best = hi[a] - lo[a];
  return best;
}

std::vector<ClipScore> score(const zc::CreatureType& T, bool csv, const char* who) {
  double rest[3];
  int n = 0;
  rest_extent(T, rest, n);
  const double h = body_height_mm(T);
  std::vector<ClipScore> out;
  if (csv) std::printf("creature,slot,frame,ex,ey,ez\n");
  for (const zc::Clip& c : T.bank.clips) {
    if (c.frame_count < 2) continue;  // 2-key form diagnostics are not motion
    ClipScore s;
    s.slot = c.slot_id;
    s.frames = c.frame_count;
    double lo[3] = {1e18, 1e18, 1e18}, hi[3] = {-1e18, -1e18, -1e18};
    for (int f = 0; f < c.frame_count; ++f) {
      double e[3];
      frame_extent(T, c, static_cast<uint16_t>(f), e);
      for (int a = 0; a < 3; ++a) {
        if (e[a] < lo[a]) lo[a] = e[a];
        if (e[a] > hi[a]) hi[a] = e[a];
      }
      if (csv)
        std::printf("%s,%u,%d,%.2f,%.2f,%.2f\n", who, c.slot_id, f, e[0], e[1], e[2]);
    }
    for (int a = 0; a < 3; ++a) {
      if (rest[a] <= 0.0) continue;
      const double pct = (hi[a] - lo[a]) * 100.0 / rest[a];
      if (pct > s.stretch_pct) s.stretch_pct = pct;
    }
    // the bob: the root's own vertical travel, from the clip's own root track
    if (c.root.size() >= static_cast<size_t>(c.frame_count) * 3) {
      double rlo = 1e18, rhi = -1e18;
      for (int f = 0; f < c.frame_count; ++f) {
        // ROOT COMPONENT 1 IS THE VERTICAL IN BOTH RIGS -- the root track is
        // world translation applied after the pose, not a creature-local axis,
        // so this one really is up for Manafold and Zixxtrixx alike. (The BIND
        // extents above cannot assume that, which is why they take the widest
        // of three; do not "tidy" the two into one rule.)
        const double v = mm(c.root[static_cast<size_t>(f) * 3 + 1]);
        if (v < rlo) rlo = v;
        if (v > rhi) rhi = v;
      }
      if (rhi > rlo) s.bob_mm = rhi - rlo;
    }
    s.bob_pct = h > 0 ? s.bob_mm * 100.0 / h : 0.0;
    out.push_back(s);
  }
  std::printf("  %-10s deform vertices %4d   rest extents %.0f x %.0f x %.0f mm"
              "   rest span %.0f mm   clips %zu\n",
              who, n, rest[0], rest[1], rest[2], h, out.size());
  return out;
}

double median_of(std::vector<double> v) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

/** MEASURE B -- THE POSED BODY'S OWN SIZE, root-local.
 *
 *  ⚠ MEASURE A ALONE IS BIASED AND THE BIAS RUNS OUR WAY. It reads the DEFORM
 *  SIDECAR, and Zixxtrixx does not breathe through the sidecar: `build_idle`
 *  computes a `breath` term and spends it on `apply_stance`, deepening the arch
 *  through the SPINE POSE. Its deform channel is reserved for the whole-body
 *  spring, so 30-odd of its clips score a flat 0.0% on A while the animal is
 *  visibly inhaling. Shipping A on its own would be the exact CLAUDE.md failure
 *  this file's header quotes: a number that measures a proxy, arrives first,
 *  and is believed because it is a number.
 *
 *  So B measures what a viewer actually sees change: the posed body's own
 *  extent, in ROOT-LOCAL millimetres, through decode_pose + the deform + the
 *  production skinner. Root-local means a creature that merely TURNS scores
 *  zero -- rotation is not expression. A body that squashes scores; a spine
 *  that curls also scores, and that is deliberate, because a curling spine IS
 *  the body changing shape to the eye.
 *
 *  It is reported per DEFORM AXIS GROUP, with vertex counts, because each
 *  creature authored its own axes (Manafold's body ball on axis 1 and its
 *  antenna loop on axis 0; Zixxtrixx's body chain on 2 and its stripe on 1) and
 *  a single blended number would quietly compare a body against an antenna.
 *  Nothing here picks the winning group: both are printed.
 *
 *  Root bone: index 0 in both rigs (u02::kBRoot, zixx::kBSpine0). Asserted
 *  below rather than assumed, because "bone 0 is the root" is precisely the
 *  kind of true-today fact that this file is about.
 */
struct GroupScore {
  int axis = -1;
  int verts = 0;
  double median_pct = 0;
  double best_pct = 0;
};

/** Root-local posed position of one already-deformed skin vertex. */
void posed_local(const std::array<zc::mat3x4fx, zc::kMaxBones>& pose, const zc::SkinVertex& v,
                 double& x, double& y, double& z) {
  int32_t wx, wy, wz;
  zc::skin_vertex(pose.data(), v, wx, wy, wz, nullptr);
  const zc::mat3x4fx& rm = pose[0];  // the root; asserted by the caller
  const int64_t dx = wx - rm.m[3], dy = wy - rm.m[7], dz = wz - rm.m[11];
  x = static_cast<double>(((rm.m[0] * dx + rm.m[4] * dy + rm.m[8] * dz) >> 16) * 1000 >> 16);
  y = static_cast<double>(((rm.m[1] * dx + rm.m[5] * dy + rm.m[9] * dz) >> 16) * 1000 >> 16);
  z = static_cast<double>(((rm.m[2] * dx + rm.m[6] * dy + rm.m[10] * dz) >> 16) * 1000 >> 16);
}

std::vector<GroupScore> posed_size_swing(const zc::CreatureType& T, const char* who) {
  // which deform axes did this creature actually author?
  int count[4] = {0, 0, 0, 0};
  for (const zc::Meshlet& m : T.mesh) {
    if (m.deform.empty()) continue;
    for (const zc::DeformVertex& d : m.deform)
      if (d.role != zc::DeformRole::kNone && d.axis < 4) ++count[d.axis];
  }
  std::vector<GroupScore> groups;
  for (int a = 0; a < 4; ++a)
    if (count[a] > 0) {
      GroupScore g;
      g.axis = a;
      g.verts = count[a];
      groups.push_back(g);
    }

  std::vector<std::vector<double>> per_group(groups.size());
  for (const zc::Clip& c : T.bank.clips) {
    if (c.frame_count < 2) continue;
    std::vector<double> lo(groups.size(), 1e18), hi(groups.size(), -1e18), sum(groups.size(), 0.0);
    int frames = 0;
    for (int f = 0; f < c.frame_count; ++f) {
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, c, static_cast<uint16_t>(f), pose, nullptr, 0);
      const zc::DeformFrame fr = zc::deformation_frame(T, c.slot_id, static_cast<uint16_t>(f), 0);
      std::vector<double> blo(groups.size() * 3, 1e18), bhi(groups.size() * 3, -1e18);
      for (const zc::Meshlet& m : T.mesh) {
        if (m.deform.empty()) continue;
        for (size_t vi = 0; vi < m.verts.size(); ++vi) {
          const zc::DeformVertex& dv = m.deform[vi];
          if (dv.role == zc::DeformRole::kNone) continue;
          size_t gi = groups.size();
          for (size_t k = 0; k < groups.size(); ++k)
            if (groups[k].axis == dv.axis) gi = k;
          if (gi == groups.size()) continue;
          const zc::SkinVertex d = zc::deform_skin_vertex_lanes(m.verts[vi], dv, fr);
          double p[3];
          posed_local(pose, d, p[0], p[1], p[2]);
          for (int a = 0; a < 3; ++a) {
            if (p[a] < blo[gi * 3 + a]) blo[gi * 3 + a] = p[a];
            if (p[a] > bhi[gi * 3 + a]) bhi[gi * 3 + a] = p[a];
          }
        }
      }
      for (size_t k = 0; k < groups.size(); ++k) {
        double vol = 1.0;
        for (int a = 0; a < 3; ++a) {
          double e = bhi[k * 3 + a] - blo[k * 3 + a];
          if (e < 1.0) e = 1.0;  // a flat group would make the cube root zero
          vol *= e;
        }
        const double sz = std::cbrt(vol);
        if (sz < lo[k]) lo[k] = sz;
        if (sz > hi[k]) hi[k] = sz;
        sum[k] += sz;
      }
      ++frames;
    }
    for (size_t k = 0; k < groups.size(); ++k) {
      const double mean = frames > 0 ? sum[k] / frames : 0.0;
      if (mean > 0.0) per_group[k].push_back((hi[k] - lo[k]) * 100.0 / mean);
    }
  }
  for (size_t k = 0; k < groups.size(); ++k) {
    groups[k].median_pct = median_of(per_group[k]);
    for (double v : per_group[k])
      if (v > groups[k].best_pct) groups[k].best_pct = v;
    std::printf("    %-10s deform axis %d  (%4d verts)   posed size swing: "
                "median %5.1f%%   best clip %5.1f%%\n",
                who, groups[k].axis, groups[k].verts, groups[k].median_pct,
                groups[k].best_pct);
  }
  return groups;
}

}  // namespace

int main(int argc, char** argv) {
  bool csv = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--csv") == 0) csv = true;

  std::printf("THE EXPRESSIVENESS PLATE -- D5 SS6, \"even moreso than Zixxtrixx\"\n");
  std::printf("  deform-stage output in bind-space mm, through deformation_frame +\n");
  std::printf("  deform_skin_vertex_lanes. No pose, no camera, no pixels.\n\n");

  const zc::CreatureType& M = u02::type();
  const zc::CreatureType& Z = zixx::type();
  if (M.mesh.empty() || Z.mesh.empty()) {
    std::fprintf(stderr, "express: a creature compiled to no meshlets\n");
    return 1;
  }

  const std::vector<ClipScore> ms = score(M, csv, "manafold");
  const std::vector<ClipScore> zs = score(Z, csv, "zixxtrixx");
  if (csv) return 0;

  std::printf("\n  BODY STRETCH per clip -- widest bind-axis extent swing, "
              "%% of the rest extent\n");
  std::printf("  (this is the inhale/exhale: how much the body's own size "
              "changes across the clip)\n\n");
  std::printf("      MANAFOLD                          ZIXXTRIXX\n");
  const size_t rows = ms.size() > zs.size() ? ms.size() : zs.size();
  for (size_t i = 0; i < rows; ++i) {
    char a[64] = "                                ";
    char b[64] = "";
    if (i < ms.size())
      std::snprintf(a, sizeof a, "slot %2u  %4d f  stretch %5.1f%%  bob %4.0f mm",
                    ms[i].slot, ms[i].frames, ms[i].stretch_pct, ms[i].bob_mm);
    if (i < zs.size())
      std::snprintf(b, sizeof b, "slot %2u  %4d f  stretch %5.1f%%  bob %4.0f mm",
                    zs[i].slot, zs[i].frames, zs[i].stretch_pct, zs[i].bob_mm);
    std::printf("      %-33s %s\n", a, b);
  }

  std::vector<double> msv, zsv, mbv, zbv;
  double mbest = 0, zbest = 0;
  for (const ClipScore& s : ms) {
    msv.push_back(s.stretch_pct);
    mbv.push_back(s.bob_pct);
    if (s.stretch_pct > mbest) mbest = s.stretch_pct;
  }
  for (const ClipScore& s : zs) {
    zsv.push_back(s.stretch_pct);
    zbv.push_back(s.bob_pct);
    if (s.stretch_pct > zbest) zbest = s.stretch_pct;
  }

  // ---- MEASURE B, and the reason A alone must not be shipped -------------
  {
    int m_zero = 0, z_zero = 0;
    for (const ClipScore& s : ms) if (s.stretch_pct < 0.05) ++m_zero;
    for (const ClipScore& s : zs) if (s.stretch_pct < 0.05) ++z_zero;
    std::printf("\n  ⚠ READ THIS BEFORE THE VERDICT. Clips whose DEFORM CHANNEL "
                "never moves:\n"
                "      manafold %2d of %2d      zixxtrixx %2d of %2d\n",
                m_zero, static_cast<int>(ms.size()), z_zero,
                static_cast<int>(zs.size()));
    std::printf("    Zixxtrixx's idle breath is NOT in the deform sidecar -- "
                "`build_idle` spends its\n    `breath` term on apply_stance, "
                "through the SPINE POSE. Measure A cannot see that,\n    so A is "
                "biased in Manafold's favour and A alone would be a proxy "
                "believed because\n    it is a number. Measure B exists for "
                "exactly this reason.\n");

    std::printf("\n  MEASURE B -- POSED body size swing, root-local, per deform "
                "axis group\n  (rotation scores zero; a squash scores; a curling "
                "spine also scores, deliberately)\n\n");
    const std::vector<GroupScore> mg = posed_size_swing(M, "manafold");
    const std::vector<GroupScore> zg = posed_size_swing(Z, "zixxtrixx");

    std::printf("\n  ------------------------------------------------------------\n");
    std::printf("  A  body stretch, deform channel, median : manafold %5.1f%%   "
                "zixxtrixx %5.1f%%\n", median_of(msv), median_of(zsv));
    std::printf("  A  body stretch, deform channel, best   : manafold %5.1f%%   "
                "zixxtrixx %5.1f%%\n", mbest, zbest);
    std::printf("  bob, median %% of rest span             : manafold %5.1f%%   "
                "zixxtrixx %5.1f%%\n", median_of(mbv), median_of(zbv));
    double mb = 0, zb = 0;
    for (const GroupScore& g : mg) if (g.median_pct > mb) mb = g.median_pct;
    for (const GroupScore& g : zg) if (g.median_pct > zb) zb = g.median_pct;
    std::printf("  B  posed size swing, best group, median : manafold %5.1f%%   "
                "zixxtrixx %5.1f%%\n", mb, zb);
    std::printf("  ------------------------------------------------------------\n");

    std::printf("\n  VERDICT on \"even moreso than Zixxtrixx\" (D5 SS6):\n");
    const double a_m = median_of(msv), a_z = median_of(zsv);
    std::printf("    Measure A (deform channel): %s -- but see the bias note "
                "above; A is not evidence on its own.\n",
                a_m > a_z ? "Manafold ahead" : "Zixxtrixx ahead");
    if (mb > zb)
      std::printf("    Measure B (what a viewer sees): SUPPORTED -- Manafold "
                  "%5.1f%% against Zixxtrixx %5.1f%%, a factor of %.2f.\n",
                  mb, zb, zb > 0.0 ? mb / zb : 0.0);
    else
      std::printf("    Measure B (what a viewer sees): ⚠ REFUTED -- Zixxtrixx "
                  "%5.1f%% against Manafold %5.1f%%, a factor of %.2f. The claim "
                  "has been asserted for six passes and it is WRONG.\n",
                  zb, mb, mb > 0.0 ? zb / mb : 0.0);
    // ⚠ AND B IS BIASED THE OTHER WAY, which has to be printed here or the
    // number gets quoted without it. Zixxtrixx's deform group is its whole
    // nose-to-tail spine chain, so a CURL moves its root-local box a long way;
    // Manafold's body group is a compact ball whose box only moves when it
    // actually squashes. A rewards the sidecar, B rewards bending, and the two
    // creatures are built to lean on opposite ones. That is why this tool
    // refuses to hand back a single winner.
    std::printf("    ⚠ B's own bias runs the OTHER way: Zixxtrixx's deform group "
                "is the whole nose-to-tail\n    spine, so a curl scores; "
                "Manafold's body group is a compact ball that only scores when "
                "it\n    squashes. Body-group against body-group Manafold is "
                "%5.1f%% -- worse still, not better.\n",
                [&mg]() {
                  // the SMALLEST group is Manafold's body ball (257 verts)
                  // against its 612-vertex antenna loop; named by size rather
                  // than by a hardcoded axis index, and printed with its
                  // vertex count above so the choice is visible.
                  double v = 0;
                  int smallest = 1 << 30;
                  for (const GroupScore& g : mg)
                    if (g.verts < smallest) { smallest = g.verts; v = g.median_pct; }
                  return v;
                }());
    std::printf("    Neither number settles an art claim. The rendered pair "
                "plate is built beside this\n    table and the verdict is taken "
                "by LOOKING at it. (CLAUDE.md rules 1 and 7.)\n");
  }
  return 0;
}
