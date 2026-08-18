// terrain_lod_tess.cpp — the real TERRAIN.LOD drives the real TERRAIN.TESS.
//
// WHY THIS TEST EXISTS. TERRAIN.LOD buffers a whole patch for exactly one
// reason: a subpatch's `lod_target` carries its four NEIGHBOURS' levels, and
// TERRAIN.TESS's crack-safe stitch is built entirely out of those four numbers.
// If the neighbour lookup transposed x and z, or read the +z cell where it
// meant −z, every isolated test would still pass — LOD would emit
// self-consistent packets and TESS would stitch them faithfully — and the
// island would tear along every subpatch boundary.
//
// So the assertion that matters here is not "the numbers match". It is:
//
//   ACROSS EVERY ONE OF THE 24 INTERIOR SUBPATCH BOUNDARIES OF A REAL PATCH,
//   THE TWO SIDES EMIT THE IDENTICAL SET OF VERTICES ON THE SHARED EDGE.
//
// That is a statement about the two blocks' agreement and neither can make it
// alone. It is checked on the geometry the pair actually emitted, not argued.
//
// The two modules are wired port-for-port with no adapter: TERRAIN.LOD's
// `lod_decisions` output IS TERRAIN.TESS's job port. If that stops being true
// this file stops compiling, which is the point.

#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_lod.h"
#include "Vzhao_terrain_tess.h"

#include "lod_dev.hpp"
#include "tess_harness.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_terrain_lod.hpp"
#include "zref/zref_terrain_tess.hpp"

using lod_test::kNSub;
using lod_test::LodJob;
using zhao::check;

namespace zt = zref::terrain;

namespace {

constexpr int32_t kOne = 1 << 16;
constexpr int kSub = 8;  // cells per subpatch side (charter §11.1)

struct Tri {
  int32_t v[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  uint16_t src = 0;
};

/** The two models, wired to each other and to a lattice memory model. */
class Chain {
 public:
  Chain(Vzhao_terrain_lod& lod, Vzhao_terrain_tess& tess, const zt::ComposedLattice& lat)
      : lod_(lod), tess_(tess), lat_(lat) {}

  void reset() {
    lod_.rst_n = 0;
    tess_.rst_n = 0;
    lod_.sp_valid_i = 0;
    lod_.out_ready_i = 0;
    tess_.job_valid_i = 0;
    tess_.tri_ready_i = 0;
    tess_.lat_h_i = 0;
    tess_.lat_wx_i = 0;
    tess_.lat_wz_i = 0;
    tess_.cs_substance_i = 0;
    lod_.eval();
    tess_.eval();
    for (int i = 0; i < 2; ++i) tick();
    lod_.rst_n = 1;
    tess_.rst_n = 1;
    lod_.eval();
    tess_.eval();
    tick();
    lat_pend_ = false;
    cs_pend_ = false;
  }

  /** Push one patch's descriptors through LOD into TESS; collect the mesh. */
  std::vector<Tri> run(const LodJob& job, uint32_t* rejects) {
    lod_.cam0_x_i = static_cast<uint32_t>(job.cam[0].ex);
    lod_.cam0_y_i = static_cast<uint32_t>(job.cam[0].ey);
    lod_.cam0_z_i = static_cast<uint32_t>(job.cam[0].ez);
    lod_.cam0_scale_i = job.cam[0].scale;
    lod_.cam0_en_i = job.cam[0].enabled ? 1 : 0;
    lod_.cam1_en_i = 0;
    lod_.cam1_x_i = 0;
    lod_.cam1_y_i = 0;
    lod_.cam1_z_i = 0;
    lod_.cam1_scale_i = 256;
    lod_.hyst_i = job.policy.hyst;
    lod_.min_hold_i = job.policy.min_hold;
    lod_.morph_step_i = static_cast<uint32_t>(job.policy.morph_step);
    lod_.dual_i = 0;
    lod_.edge_nz_i = job.edge[0];
    lod_.edge_pz_i = job.edge[1];
    lod_.edge_nx_i = job.edge[2];
    lod_.edge_px_i = job.edge[3];

    std::vector<Tri> out;
    *rejects = 0;
    int pushed = 0;
    int quiet = 0;
    for (int cycle = 0; cycle < 2000000; ++cycle) {
      // ---- descriptors into TERRAIN.LOD ----------------------------------
      if (pushed < kNSub) {
        const zt::LodSubpatch& s = job.sp[pushed];
        lod_.sp_valid_i = 1;
        lod_.sp_cx_i = static_cast<uint32_t>(s.cx);
        lod_.sp_cy_i = static_cast<uint32_t>(s.cy);
        lod_.sp_cz_i = static_cast<uint32_t>(s.cz);
        lod_.sp_dev1_i = s.dev[1] & 0xFFFFFFu;
        lod_.sp_dev2_i = s.dev[2] & 0xFFFFFFu;
        lod_.sp_dev3_i = s.dev[3] & 0xFFFFFFu;
        lod_.sp_prev_level_i = static_cast<uint8_t>(s.level & 3);
        lod_.sp_prev_morph_i = static_cast<uint32_t>(s.morph);
        lod_.sp_hold_i = s.hold;
        lod_.sp_src_id_i = job.src[pushed];
      } else {
        lod_.sp_valid_i = 0;
      }

      // ---- the registered lattice / cell-state ports ---------------------
      if (lat_pend_) {
        const size_t k = static_cast<size_t>(lat_vj_) * static_cast<size_t>(lat_.w) +
                         static_cast<size_t>(lat_vi_);
        tess_.lat_h_i = lat_surf_ ? lat_.bottom[k] : lat_.top[k];
        tess_.lat_wx_i = lat_.wx[static_cast<size_t>(lat_vi_)];
        tess_.lat_wz_i = lat_.wz[static_cast<size_t>(lat_vj_)];
      } else {
        tess_.lat_h_i = 0x5BADF00D;
        tess_.lat_wx_i = 0x5BADF00D;
        tess_.lat_wz_i = 0x5BADF00D;
      }
      if (cs_pend_) {
        tess_.cs_substance_i = static_cast<uint8_t>(
            lat_.substance(static_cast<int>(cs_ci_), static_cast<int>(cs_cj_)));
      } else {
        tess_.cs_substance_i = 3;
      }

      // ---- THE WIRING: lod_decisions IS the tessellator's job port -------
      tess_.tri_ready_i = 1;
      tess_.eval();
      lod_.out_ready_i = tess_.job_ready_o;
      lod_.eval();
      tess_.job_valid_i = lod_.out_valid_o;
      tess_.job_ox_i = lod_.out_ox_o;
      tess_.job_oz_i = lod_.out_oz_o;
      tess_.job_level_i = lod_.out_level_o;
      tess_.job_lvl_nz_i = lod_.out_lvl_nz_o;
      tess_.job_lvl_pz_i = lod_.out_lvl_pz_o;
      tess_.job_lvl_nx_i = lod_.out_lvl_nx_o;
      tess_.job_lvl_px_i = lod_.out_lvl_px_o;
      tess_.job_morph_i = lod_.out_morph_o;
      tess_.job_surface_i = lod_.out_surface_o;
      tess_.job_dual_i = lod_.out_dual_o;
      tess_.job_src_id_i = lod_.out_src_id_o;
      tess_.eval();

      const bool take_sp = (pushed < kNSub) && lod_.sp_ready_o;
      if (tess_.tri_valid_o) {
        Tri t;
        t.v[0] = tess_.ax_o;
        t.v[1] = tess_.ay_o;
        t.v[2] = tess_.az_o;
        t.v[3] = tess_.bx_o;
        t.v[4] = tess_.by_o;
        t.v[5] = tess_.bz_o;
        t.v[6] = tess_.cx_o;
        t.v[7] = tess_.cy_o;
        t.v[8] = tess_.cz_o;
        t.src = static_cast<uint16_t>(tess_.src_id_o);
        out.push_back(t);
      }
      if (tess_.job_reject_o) ++*rejects;

      lat_pend_ = tess_.lat_req_o != 0;
      lat_vi_ = static_cast<uint8_t>(tess_.lat_vi_o);
      lat_vj_ = static_cast<uint8_t>(tess_.lat_vj_o);
      lat_surf_ = tess_.lat_surface_o != 0;
      cs_pend_ = tess_.cs_req_o != 0;
      cs_ci_ = static_cast<uint8_t>(tess_.cs_ci_o);
      cs_cj_ = static_cast<uint8_t>(tess_.cs_cj_o);

      tick();
      if (take_sp) ++pushed;

      if (pushed == kNSub && lod_.idle_o && tess_.idle_o && !lod_.out_valid_o) {
        if (++quiet > 64) break;
      } else {
        quiet = 0;
      }
    }
    lod_.sp_valid_i = 0;
    tess_.job_valid_i = 0;
    lod_.eval();
    tess_.eval();
    return out;
  }

 private:
  void tick() {
    lod_.clk = 0;
    tess_.clk = 0;
    lod_.eval();
    tess_.eval();
    lod_.clk = 1;
    tess_.clk = 1;
    lod_.eval();
    tess_.eval();
    lod_.clk = 0;
    tess_.clk = 0;
    lod_.eval();
    tess_.eval();
  }

  Vzhao_terrain_lod& lod_;
  Vzhao_terrain_tess& tess_;
  const zt::ComposedLattice& lat_;
  bool lat_pend_ = false;
  bool cs_pend_ = false;
  uint8_t lat_vi_ = 0, lat_vj_ = 0, cs_ci_ = 0, cs_cj_ = 0;
  bool lat_surf_ = false;
};

/** Doubled signed area of one triangle in the (x, z) plane. */
int64_t area2_xz(const Tri& t) {
  const int64_t ax = t.v[0], az = t.v[2];
  const int64_t bx = t.v[3], bz = t.v[5];
  const int64_t cx = t.v[6], cz = t.v[8];
  return (bx - ax) * (cz - az) - (bz - az) * (cx - ax);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_lod lod;
  Vzhao_terrain_tess tess;

  zt::ComposedLattice lat = tess_test::make_lattice(false, 2);
  tess_test::fill_relief(lat, 0xC0FFEEu);

  Chain ch(lod, tess, lat);

  // Three cameras placed so the patch gets a MIXTURE of levels — a uniform
  // patch would make the stitch trivially satisfied and prove nothing.
  const int32_t eyes[3][3] = {
      {8 * kOne, 2 * kOne, 8 * kOne}, {32 * kOne, 20 * kOne, 32 * kOne}, {64 * kOne, 5 * kOne, 0}};
  const char* names[3] = {"camera at the -x -z corner", "camera above the patch centre",
                          "camera at the +x -z corner"};

  for (int scene = 0; scene < 3; ++scene) {
    ch.reset();

    LodJob job;
    job.cam[0].ex = eyes[scene][0];
    job.cam[0].ey = eyes[scene][1];
    job.cam[0].ez = eyes[scene][2];
    job.cam[0].scale = 256;
    job.cam[0].enabled = true;
    job.cam[1].enabled = false;
    job.policy.hyst = 256;
    job.policy.min_hold = 0;
    job.policy.morph_step = 0;  // snap: the stitch law is about levels, not morph
    job.dual = false;
    for (int e = 0; e < 4; ++e) job.edge[e] = 0;
    for (int n = 0; n < kNSub; ++n) {
      const int i = n & 3;
      const int j = n >> 2;
      // Subpatch centres on the real lattice: 8 cells at 2 m each = 16 m.
      job.sp[n].cx = (i * 16 + 8) * kOne;
      job.sp[n].cz = (j * 16 + 8) * kOne;
      job.sp[n].cy = 10 * kOne;
      // Deviations that straddle the eye-to-subpatch distances this patch
      // produces (8 m to 70 m), so the ladder gives DIFFERENT levels across the
      // patch. A uniform patch would satisfy the stitch law trivially.
      job.sp[n].dev[1] = static_cast<uint32_t>(8 * kOne);
      job.sp[n].dev[2] = static_cast<uint32_t>(20 * kOne);
      job.sp[n].dev[3] = static_cast<uint32_t>(35 * kOne);
      job.sp[n].level = 0;
      job.sp[n].morph = 0;
      job.sp[n].hold = 255;
      job.src[n] = static_cast<uint16_t>(n);
    }

    // Walk the patch to its RESTING levels before the measured frame. The
    // ladder moves one rung per frame by construction (a geomorph can only
    // blend adjacent levels), so a single frame from level 0 would only ever
    // show levels 0 and 1 and the stitch cases would go untested.
    for (int f = 0; f < 8; ++f) {
      for (int n = 0; n < kNSub; ++n) {
        const zt::LodDecision d = zt::lod_select(job.sp[n], job.cam, job.policy);
        job.sp[n].level = d.level;
        job.sp[n].morph = d.morph;
        job.sp[n].hold = d.hold;
      }
    }

    uint32_t rejects = 0;
    const std::vector<Tri> mesh = ch.run(job, &rejects);
    check(rejects == 0, "no subpatch was rejected", 0, rejects);
    check(!mesh.empty(), "the pair emitted a mesh", 1, mesh.size());

    // ---- what levels did LOD actually choose? --------------------------
    // Recovered from the emitted geometry's source ids: a level-L subpatch
    // emits 2·(8>>L)² triangles when nothing is stitched, and fewer when it is,
    // so the COUNT is not the level — the level comes from the oracle and the
    // MIX is what this checks.
    const std::vector<lod_test::LodOut> want = lod_test::oracle(job);
    std::set<int> levels;
    for (const lod_test::LodOut& o : want) levels.insert(o.level);
    std::printf("[terrain_lod_tess] %s: %u triangles, %u distinct levels\n", names[scene],
                static_cast<unsigned>(mesh.size()), static_cast<unsigned>(levels.size()));
    check(levels.size() >= 2, "the scene really does mix levels across the patch", 2,
          levels.size());

    // ---- THE CRACK INVARIANT, on the emitted geometry ------------------
    // For every interior boundary, collect from each side every vertex that
    // lies exactly on it, and require the two sets to be identical. A vertex
    // one side emits and the other does not IS the crack.
    std::map<uint16_t, std::vector<Tri>> by_sub;
    for (const Tri& t : mesh) by_sub[t.src].push_back(t);
    check(by_sub.size() == 16, "every subpatch emitted something", 16, by_sub.size());

    uint32_t boundaries = 0;
    uint32_t mismatches = 0;
    for (int j = 0; j < 4; ++j) {
      for (int i = 0; i < 4; ++i) {
        const int n = j * 4 + i;
        for (int dir = 0; dir < 2; ++dir) {  // 0 = +x boundary, 1 = +z boundary
          const int ni = i + (dir == 0 ? 1 : 0);
          const int nj = j + (dir == 0 ? 0 : 1);
          if (ni > 3 || nj > 3) continue;
          const int m = nj * 4 + ni;
          const int32_t seam = (dir == 0) ? lat.wx[static_cast<size_t>((i + 1) * kSub)]
                                          : lat.wz[static_cast<size_t>((j + 1) * kSub)];
          std::set<std::pair<int32_t, int32_t>> sa;
          std::set<std::pair<int32_t, int32_t>> sb;
          for (int side = 0; side < 2; ++side) {
            const std::vector<Tri>& src = by_sub[static_cast<uint16_t>(side == 0 ? n : m)];
            std::set<std::pair<int32_t, int32_t>>& dst = (side == 0) ? sa : sb;
            for (const Tri& t : src) {
              for (int k = 0; k < 3; ++k) {
                const int32_t vx = t.v[k * 3 + 0];
                const int32_t vy = t.v[k * 3 + 1];
                const int32_t vz = t.v[k * 3 + 2];
                if (dir == 0 && vx == seam) dst.insert({vz, vy});
                if (dir == 1 && vz == seam) dst.insert({vx, vy});
              }
            }
          }
          ++boundaries;
          if (sa != sb) {
            ++mismatches;
            if (mismatches == 1) {
              char buf[192];
              std::snprintf(buf, sizeof(buf),
                            "%s: subpatch %d and %d disagree on their shared edge — %u vs %u "
                            "vertices",
                            names[scene], n, m, static_cast<unsigned>(sa.size()),
                            static_cast<unsigned>(sb.size()));
              check(false, buf, sa.size(), sb.size());
            }
          }
          check(!sa.empty(), "a shared edge carries vertices at all", 1, sa.size());
        }
      }
    }
    check(boundaries == 24, "all 24 interior subpatch boundaries were examined", 24, boundaries);
    check(mismatches == 0,
          "CRACK-FREE — every interior boundary's two sides emit the identical vertex set", 0,
          mismatches);

    // ---- and the patch is tiled exactly, no gaps and no overlaps -------
    // Every top triangle is clockwise in (x, z), so the doubled signed areas
    // sum to −2·A over the whole 64 m × 64 m patch.
    int64_t total = 0;
    for (const Tri& t : mesh) total += area2_xz(t);
    const int64_t side = static_cast<int64_t>(lat.wx[32]) - lat.wx[0];
    check(total == -2 * side * side, "the patch is tiled exactly — no gaps, no overlaps",
          static_cast<uint64_t>(-2 * side * side), static_cast<uint64_t>(total));
  }

  return zhao::report_and_exit("terrain_lod_tess");
}
