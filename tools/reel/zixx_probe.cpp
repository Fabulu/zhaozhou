// zixx_probe — the COMMITTED pose probe for the Zixxtrixx model.
//
// Ground contact must be authored, never accidental (CLAUDE.md), and it must
// be MEASURED with a 3D pose probe, not read off a rendered frame: a 2D frame
// cannot separate "in front of the dirt" from "inside it". This probe decodes
// every key of every clip, skins EVERY mesh vertex, and reports world-Y
// statistics against the flat ground plane the reel's root ground-snap
// establishes (root at terrain top, so ground = 0).
//
// Per clip it prints per-key min/max Y (sampled), the worst penetration and
// where it happens, and clip-specific numbers:
//   - idle/walk: belly excursion across the loop (the authored few-mm sink);
//   - attack:    blade-tip minimum per key around contact (the authored bite);
//   - fall:      whole-loop clearance (must NEVER touch).
//
// Build (from zhaozhou/, same flags as the reel — no cmake, see CLAUDE.md):
//   G="C:/Programmieren/dsstuff/mingw64/bin/g++.exe"
//   "$G" -Itests/render -Icompiler/tests/generated -Ireference/src \
//        -Ireference/include -Iruntime/include -O2 -DNDEBUG -std=c++17 \
//        tools/reel/zixx_probe.cpp build/reference/libzhao_zref.a \
//        -o build/tools/zixx-probe.exe
#include "zref/zref_creature.hpp"
#include "zrender/internal.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace zc = zref::creature;
constexpr int32_t fxm(int64_t milli) {
  return static_cast<int32_t>((milli * 65536 + (milli >= 0 ? 500 : -500)) / 1000);
}
#include "zixxtrixx.h"

namespace {
int32_t to_mm(int64_t fx) { return static_cast<int32_t>(fx * 1000 >> 16); }
}  // namespace

// ---- THE SELF-INTERSECTION PROBE (2026-08-27 head-only run) ---------------
// Headache.md: "compare non-neighbouring ring-centre distances against their
// radii. It does not need triangle-exact collision -- it needs to SHOUT when
// the skull overlaps the neck or trunk." So: every body/head station's ring
// CENTRE is skinned through the same pose the mesh uses (bind centre at
// (-station_x, kBodyY, 0), the station's own bind), its radius is the ring's
// LATERAL half-width (the larger axis, eye bulge included), and every pair
// of stations that is genuinely non-neighbouring -- bind-pose centre
// separation > 1.15 * (r_i + r_j), so the tube's own continuity can never
// trigger it -- is checked posed: centre distance < r_i + r_j is an overlap,
// printed with its depth. Run over EVERY key of EVERY clip.
//
// TWO HONESTY RULES, both learned from the first run of this probe:
//   - the radius used is the VERTICAL one (station_r), not the lateral
//     half-width: the S lives in the X-Y plane and the ring's in-plane
//     extent IS rz -- the googly-eye bulge sticks out in +-Z where there is
//     nothing to hit, and counting it flagged phantom overlaps ~150 mm deep;
//   - pairs closer than 8 stations along the body are skipped outright: the
//     walk's hump legitimately BUNCHES the grounded run, so stations 6 apart
//     can approach through the surface without any clipping (station 38 vs
//     44 fired at "180 mm" on the approved walk).
namespace probe {
struct Station {
  zc::SkinVertex v;   // bind centre + bind, for skin_vertex
  int32_t r_mm;       // vertical (in-plane) half-thickness in mm
};
inline std::vector<Station> stations() {
  std::vector<Station> s;
  for (int i = 0; i < zixx::kProfileStations; ++i) {
    const zixx::Bind bd =
        i <= zixx::kHeadEnd ? zixx::head_station_bind(i) : zixx::station_bind(i);
    Station st;
    st.v = zc::SkinVertex{-fxm(zixx::station_x(i)), fxm(zixx::kBodyY), 0,
                          bd.b0, bd.b1, bd.w0, 0, 0};
    // the in-plane (vertical) half-extent of the ring ACTUALLY BUILT: for
    // head stations that is the ball-swollen rz (head_ring), not the bare
    // taper radius -- the probe must measure the surface the mesh has.
    if (i <= zixx::kHeadEnd) {
      int32_t rx_mm, rz_mm;
      zixx::head_ring(i, rx_mm, rz_mm);
      st.r_mm = rz_mm;
    } else {
      st.r_mm = zixx::station_r(i);
    }
    s.push_back(st);
  }
  return s;
}
}  // namespace probe

int main() {
  const zc::CreatureType& T = zixx::type();
  size_t nv = 0, nt = 0;
  for (const auto& m : T.mesh) {
    nv += m.verts.size();
    nt += m.idx.size() / 3;
  }
  size_t mnt = 0;
  for (const auto& m : T.micro) mnt += m.idx.size() / 3;
  std::printf("bones=%d meshlets=%zu verts=%zu tris=%zu | micro: %zu meshlets "
              "%zu tris, compiler micro_error=%d (a MEASURED result, never an "
              "art target)\n",
              (int)T.bank.bone_count, T.mesh.size(), nv, nt, T.micro.size(), mnt,
              (int)T.micro_error);

  for (const zc::Clip& clip : T.bank.clips) {
    int32_t worst_min = INT32_MAX, worst_max = INT32_MIN;
    int worst_f = -1;
    int worst_b0 = -1, worst_b1 = -1;  // the min vertex's bind at the worst key
    int32_t belly_lo = INT32_MAX, belly_hi = INT32_MIN;  // per-loop belly band
    std::printf("clip slot %d (%d keys)\n", clip.slot_id, clip.frame_count);
    for (uint16_t f = 0; f < clip.frame_count; ++f) {
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, clip, f, pose, nullptr, 0);
      int32_t mn = INT32_MAX, mx = INT32_MIN;
      // blade-tip minimum: the last two blade bones' meshlets carry the tips;
      // cheaper and robust to just track the min over vertices bound to the
      // blade bones.
      int32_t blade_mn = INT32_MAX;
      int mnb0 = -1, mnb1 = -1;
      for (const auto& m : T.mesh) {
        for (const auto& v : m.verts) {
          int32_t x, y, z;
          zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
          if (y < mn) { mnb0 = v.b0; mnb1 = v.b1; }
          mn = std::min(mn, y);
          mx = std::max(mx, y);
          if (v.b0 >= zixx::kBBladeL && v.b0 <= zixx::kBBladeR2)
            blade_mn = std::min(blade_mn, y);
        }
      }
      if (mn < worst_min) {
        worst_min = mn;
        worst_f = f;
        worst_b0 = mnb0;
        worst_b1 = mnb1;
      }
      worst_max = std::max(worst_max, mx);
      belly_lo = std::min(belly_lo, mn);
      belly_hi = std::max(belly_hi, mn);
      const bool attack = clip.slot_id == 3;
      if ((attack && f >= 44) || f % 8 == 0 || mn < -30)
        std::printf("  k%3d  minY %6d  maxY %6d  bladeMin %6d  (mm)\n", f, to_mm(mn), to_mm(mx),
                    to_mm(blade_mn));
    }
    std::printf("  WORST minY %d mm at key %d (bones %d/%d); apex %d mm; "
                "belly band [%d..%d] mm\n",
                to_mm(worst_min), worst_f, worst_b0, worst_b1, to_mm(worst_max),
                to_mm(belly_lo), to_mm(belly_hi));
  }

  // ---- non-adjacent ring overlap, every key of every clip -----------------
  const std::vector<probe::Station> sts = probe::stations();
  const int n = static_cast<int>(sts.size());
  // pairs to check: bind separation strictly beyond tube continuity
  std::vector<std::pair<int, int>> pairs;
  for (int i = 0; i < n; ++i) {
    for (int j = i + 8; j < n; ++j) {
      const int64_t bind_mm = zixx::station_x(j) - zixx::station_x(i);
      if (bind_mm * 100 > static_cast<int64_t>(sts[i].r_mm + sts[j].r_mm) * 115)
        pairs.push_back({i, j});
    }
  }
  // AUTHORED NESTING ALLOWANCES, per clip slot -- the same doctrine as
  // ground contact: declared overlap is design, anything beyond it is the
  // fault. The concept NESTS the head inside the S's hook, so ring centres
  // legitimately come within radii of each other.
  // RE-AUTHORED 2026-08-28 (run 2339): the head is now the CULMINATION of
  // the one tube (kTaper peak ~218 mm) and looks UP (-6000 attitude, owner
  // order) -- a bigger head riding higher inside the same approved hook
  // nests deeper by construction, exactly as Side.png merges the head
  // against the descending stroke. Each figure was re-judged on the worst
  // key's RENDER before being re-authored (run 2339 scratch r10/r11):
  //   idle 250:  breath extreme k81 presses the crown ~239 mm into the
  //              deepened fold (render: head tucked into the coil, eye and
  //              face fully clean -- the sheet's nesting, transient);
  //   walk 165:  gait fold k16 ~154 mm, head in front of the arch, eye
  //              clean; the grounded-run bunching is the approved walk;
  //   attack 210: the REST pose itself nests the culminating head ~199 mm
  //              against the dive stroke (k0 render = the approved S read);
  //              the coil's own closing stays under this;
  //   fall 40:   TIGHTENED from 200: the 2026-08-28 S-authority relax
  //              (owner: "relax by a ton") took the flail from 10,768
  //              hits/307 mm to 67 hits/18 mm -- the loose fall genuinely
  //              stopped folding through itself, so the allowance follows
  //              the evidence DOWN, not up.
  // A regression that digs DEEPER than these prints and exits 1.
  // The 2026-08-28 vocabulary (each judged on its worst-key render,
  // run 2339): hit 215 (the recoil presses the head into the hook, the
  // rest-nesting family); death 265 (the shudder's deepen at k5 presses
  // like the idle breath extreme; the keeled corpse itself sits at -44);
  // balance 210 (the rest nesting; the flop's -191 ground bite is the
  // declared impact, not an overlap); look 250 (the full right turn swings
  // the skull closest to the hook, k72 render: clean overlap layering).
  const auto allow_mm = [](int slot) -> int32_t {
    switch (slot) {
      case 1: return 250;
      case 2: return 165;
      case 3: return 210;
      case 4: return 40;
      case 5: return 215;
      case 6: return 265;
      case 7: return 210;
      case 8: return 250;
      // the C2 phase clips are SLICES of the attack (slots 10..17): they
      // inherit its authored nesting wholesale (the coil's nose-to-tail
      // wheel closure, the rest pose's hook nesting)
      case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
        return 210;
      default: return 0;
    }
  };
  int total_overlaps = 0;
  int violations = 0;
  for (const zc::Clip& clip : T.bank.clips) {
    int clip_overlaps = 0;
    int32_t worst_depth = 0;
    int worst_key = -1, worst_i = -1, worst_j = -1;
    for (uint16_t f = 0; f < clip.frame_count; ++f) {
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, clip, f, pose, nullptr, 0);
      std::vector<int64_t> cx(n), cy(n), cz(n);
      for (int i = 0; i < n; ++i) {
        int32_t x, y, z;
        zc::skin_vertex(pose.data(), sts[i].v, x, y, z, nullptr);
        cx[i] = to_mm(x);
        cy[i] = to_mm(y);
        cz[i] = to_mm(z);
      }
      for (const auto& pr : pairs) {
        const int i = pr.first, j = pr.second;
        const int64_t dx = cx[i] - cx[j], dy = cy[i] - cy[j], dz = cz[i] - cz[j];
        const int64_t d2 = dx * dx + dy * dy + dz * dz;
        const int64_t rr = sts[i].r_mm + sts[j].r_mm;
        if (d2 < rr * rr) {
          const int32_t depth =
              static_cast<int32_t>(rr - static_cast<int64_t>(zref::isqrt_u64(static_cast<uint64_t>(d2))));
          ++clip_overlaps;
          if (depth > worst_depth) {
            worst_depth = depth;
            worst_key = f;
            worst_i = i;
            worst_j = j;
          }
        }
      }
    }
    total_overlaps += clip_overlaps;
    const int32_t allow = allow_mm(clip.slot_id);
    if (worst_depth > allow) ++violations;
    if (clip_overlaps > 0) {
      std::printf(
          "clip slot %d OVERLAP: %d station-pair hits; worst %d mm deep (allowance %d), "
          "key %d, stations %d vs %d%s\n",
          clip.slot_id, clip_overlaps, worst_depth, allow, worst_key, worst_i, worst_j,
          worst_depth > allow ? "  ** BEYOND ALLOWANCE **" : "");
    } else {
      std::printf("clip slot %d overlap: none\n", clip.slot_id);
    }
  }
  if (violations == 0) {
    std::printf("OVERLAP PROBE: %d hits, all within authored allowances\n", total_overlaps);
    return 0;
  }
  std::printf("OVERLAP PROBE: %d clip(s) beyond allowance -- SHOUTING\n", violations);
  return 1;
}
