// terrain_tess_normals.cpp — the real TERRAIN.TESS drives the real
// TERRAIN.NORMALS.
//
// WHY THIS TEST EXISTS. Composing GEOM.BINNER with the real rasterizer
// immediately exposed a silent 16x tile-index-versus-pixel error that no
// isolated test could see, because each block was self-consistently wrong about
// what the other meant. The Mantle chain has the same shape of risk and a
// sharper failure mode: TERRAIN.NORMALS computes an UNNORMALISED cross product
// and does not take its absolute value, so if TERRAIN.TESS wound a triangle the
// other way the normal points into the ground and the whole island shades
// black — which is exactly the symptom the rescale-32 defect produced, and
// exactly the symptom nobody would attribute to the tessellator.
//
// So the assertion that matters here is not "the numbers match" (both blocks
// match their own oracles already). It is: EVERY TOP TRIANGLE OF A REAL
// TESSELLATED SUBPATCH PRODUCES A NORMAL WITH ny > 0, AND EVERY UNDERSIDE
// TRIANGLE PRODUCES ny < 0, on real island relief. That is a statement about
// the two blocks' agreement, and it cannot be made by either alone.
//
// The two modules are wired port-for-port with no adapter: TERRAIN.TESS's
// `terrain_mesh` output IS TERRAIN.NORMALS' input packet. If that ever stops
// being true this file stops compiling, which is the point.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_normals.h"
#include "Vzhao_terrain_tess.h"

#include "tess_harness.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_terrain_normals.hpp"
#include "zref/zref_terrain_tess.hpp"

using zhao::check;
namespace zt = zref::terrain;
using tess_test::make_lattice;

namespace {

struct Normal {
  int32_t x = 0, y = 0, z = 0;
  bool degenerate = false;
  uint16_t src = 0;
};

/**
 * Run one subpatch through TESS and feed every emitted triangle straight into
 * NORMALS, on the same clock, with NO reformatting between them.
 */
class Chain {
 public:
  Chain(Vzhao_terrain_tess& tess, Vzhao_terrain_normals& norm) : tess_(tess), norm_(norm) {}

  void reset() {
    tess_.rst_n = 0;
    tess_.job_valid_i = 0;
    tess_.tri_ready_i = 0;
    tess_.lat_h_i = 0;
    tess_.lat_wx_i = 0;
    tess_.lat_wz_i = 0;
    tess_.cs_substance_i = 0;
    norm_.rst_n = 0;
    norm_.tri_valid_i = 0;
    norm_.nrm_ready_i = 0;
    tess_.eval();
    norm_.eval();
    for (int i = 0; i < 2; ++i) {
      zhao::tick(tess_);
      zhao::tick(norm_);
    }
    tess_.rst_n = 1;
    norm_.rst_n = 1;
    tess_.eval();
    norm_.eval();
    zhao::tick(tess_);
    zhao::tick(norm_);
    lat_pend_ = false;
    cs_pend_ = false;
  }

  std::vector<Normal> run(const zt::ComposedLattice& lat, const zt::SubpatchJob& job,
                          std::vector<zt::MeshTri>* mesh_out, uint32_t stall_mask = 0) {
    std::vector<Normal> out;
    tess_.job_valid_i = 1;
    tess_.job_ox_i = static_cast<uint8_t>(job.ox);
    tess_.job_oz_i = static_cast<uint8_t>(job.oz);
    tess_.job_level_i = static_cast<uint8_t>(job.level);
    tess_.job_lvl_nz_i = static_cast<uint8_t>(job.nlevel[zt::kSideNegZ]);
    tess_.job_lvl_pz_i = static_cast<uint8_t>(job.nlevel[zt::kSidePosZ]);
    tess_.job_lvl_nx_i = static_cast<uint8_t>(job.nlevel[zt::kSideNegX]);
    tess_.job_lvl_px_i = static_cast<uint8_t>(job.nlevel[zt::kSidePosX]);
    tess_.job_morph_i = static_cast<uint32_t>(job.morph < 0 ? 0 : job.morph);
    tess_.job_surface_i = job.surface == zt::Surface::kUnderside ? 1 : 0;
    tess_.job_dual_i = lat.dual ? 1 : 0;
    tess_.job_src_id_i = 0x7000;

    bool taken = false;
    int idle_run = 0;
    for (int cycle = 0; cycle < 40000; ++cycle) {
      tess_.eval();
      if (!taken && tess_.job_ready_o) taken = true;

      // the registered lattice / cell-state ports
      if (lat_pend_) {
        const size_t k = static_cast<size_t>(lat_vj_) * static_cast<size_t>(lat.w) +
                         static_cast<size_t>(lat_vi_);
        tess_.lat_h_i = lat_surf_ ? lat.bottom[k] : lat.top[k];
        tess_.lat_wx_i = lat.wx[static_cast<size_t>(lat_vi_)];
        tess_.lat_wz_i = lat.wz[static_cast<size_t>(lat_vj_)];
      } else {
        tess_.lat_h_i = 0x5BADF00D;
        tess_.lat_wx_i = 0x5BADF00D;
        tess_.lat_wz_i = 0x5BADF00D;
      }
      if (cs_pend_) {
        tess_.cs_substance_i =
            static_cast<uint8_t>(lat.substance(static_cast<int>(cs_ci_), static_cast<int>(cs_cj_)));
      } else {
        tess_.cs_substance_i = 3;
      }

      // ---- THE WIRING. Nine coordinates and a source id, straight across. --
      norm_.eval();
      tess_.tri_ready_i = norm_.tri_ready_o;
      tess_.eval();
      norm_.tri_valid_i = tess_.tri_valid_o;
      norm_.ax_i = tess_.ax_o;
      norm_.ay_i = tess_.ay_o;
      norm_.az_i = tess_.az_o;
      norm_.bx_i = tess_.bx_o;
      norm_.by_i = tess_.by_o;
      norm_.bz_i = tess_.bz_o;
      norm_.cx_i = tess_.cx_o;
      norm_.cy_i = tess_.cy_o;
      norm_.cz_i = tess_.cz_o;
      norm_.src_id_i = tess_.src_id_o;
      norm_.nrm_ready_i = stall_mask == 0 ? 1 : (((stall_mask >> (cycle & 31)) & 1u) ^ 1u);
      norm_.eval();

      if (tess_.tri_valid_o && tess_.tri_ready_i && mesh_out != nullptr) {
        zt::MeshTri t;
        t.ax = static_cast<int32_t>(tess_.ax_o);
        t.ay = static_cast<int32_t>(tess_.ay_o);
        t.az = static_cast<int32_t>(tess_.az_o);
        t.bx = static_cast<int32_t>(tess_.bx_o);
        t.by = static_cast<int32_t>(tess_.by_o);
        t.bz = static_cast<int32_t>(tess_.bz_o);
        t.cx = static_cast<int32_t>(tess_.cx_o);
        t.cy = static_cast<int32_t>(tess_.cy_o);
        t.cz = static_cast<int32_t>(tess_.cz_o);
        mesh_out->push_back(t);
      }
      if (norm_.nrm_valid_o && norm_.nrm_ready_i) {
        Normal nn;
        nn.x = static_cast<int32_t>(norm_.nx_o);
        nn.y = static_cast<int32_t>(norm_.ny_o);
        nn.z = static_cast<int32_t>(norm_.nz_o);
        nn.degenerate = norm_.degenerate_o != 0;
        nn.src = norm_.src_id_o;
        out.push_back(nn);
      }

      const bool lat_req = tess_.lat_req_o != 0;
      const uint8_t nvi = tess_.lat_vi_o, nvj = tess_.lat_vj_o;
      const bool nsurf = tess_.lat_surface_o != 0;
      const bool cs_req = tess_.cs_req_o != 0;
      const uint8_t nci = tess_.cs_ci_o, ncj = tess_.cs_cj_o;
      const bool drained = taken && tess_.idle_o && norm_.idle_o && !tess_.job_valid_i;

      zhao::tick(tess_);
      zhao::tick(norm_);
      if (taken) tess_.job_valid_i = 0;
      lat_pend_ = lat_req;
      lat_vi_ = nvi;
      lat_vj_ = nvj;
      lat_surf_ = nsurf;
      cs_pend_ = cs_req;
      cs_ci_ = nci;
      cs_cj_ = ncj;
      if (drained) {
        if (++idle_run >= 3) break;
      } else {
        idle_run = 0;
      }
    }
    norm_.tri_valid_i = 0;
    return out;
  }

 private:
  Vzhao_terrain_tess& tess_;
  Vzhao_terrain_normals& norm_;
  bool lat_pend_ = false;
  uint8_t lat_vi_ = 0, lat_vj_ = 0;
  bool lat_surf_ = false;
  bool cs_pend_ = false;
  uint8_t cs_ci_ = 0, cs_cj_ = 0;
};

/** Island relief with real slope: a dome, so no triangle is exactly flat. */
zt::ComposedLattice make_island() {
  zt::ComposedLattice lat = make_lattice(true);
  for (int j = 0; j < 33; ++j) {
    for (int i = 0; i < 33; ++i) {
      const int dx = i - 16, dz = j - 16;
      const int d2 = dx * dx + dz * dz;
      // a dome of ~10 m falling to ~1 m at the rim, on the height16 grid
      const int16_t h = static_cast<int16_t>(2560 - d2 * 4);
      const int16_t b = static_cast<int16_t>(-12800 + d2 * 3);
      const size_t k = static_cast<size_t>(j) * 33 + static_cast<size_t>(i);
      lat.top[k] = static_cast<int32_t>(h) << 8;
      lat.bottom[k] = static_cast<int32_t>(b) << 8;
    }
  }
  return lat;
}

/**
 * The same island with VOID CELLS PUNCHED IN IT, and it exists because a fire
 * test proved this suite could not see them.
 *
 * `make_lattice` fills `cell_state` with `kSolid` everywhere, so in every one
 * of this file's 41,731 original checks TERRAIN.TESS's solidity window was
 * asked a question whose answer was always yes. Mutating the window -- the
 * expression that decides whether a run-cell is skipped -- changed nothing
 * here, while `terrain_tess_directed` caught it in five checks. A suite with
 * six times the checks was blind to the logic, which is CLAUDE.md's
 * "component checks passing is not likeness evidence" one level down:
 * 41,731 is a number, not coverage.
 *
 * THE VOIDS ARE CHOSEN TO STRADDLE THE RUN-CELL GRID, not sprinkled. TESS
 * walks run-cells of stride 1/2/4/8 by level, and skips one only when EVERY
 * patch cell under it is solid. A pattern that never fills a whole run-cell
 * exercises the "partially void" arm at every level and never the "wholly
 * void" one, so both are built deliberately:
 *
 *   - a solid 4x4 block of void at (8..11, 8..11), which is a WHOLE run-cell
 *     at levels 0-2 and part of one at level 3;
 *   - a diagonal of single void cells, which is never a whole run-cell at any
 *     level above 0 and so always lands on the partial arm.
 *
 * The oracle reads the same lattice, so the differential is unchanged in
 * kind -- this is not a new expectation, it is the same expectation asked
 * somewhere it was never asked before.
 */
zt::ComposedLattice make_island_voids() {
  zt::ComposedLattice lat = make_island();
  for (int cj = 8; cj < 12; ++cj)
    for (int ci = 8; ci < 12; ++ci)
      lat.cell_state[static_cast<size_t>(cj) * 32 + static_cast<size_t>(ci)] =
          zt::kVoidAuthored;
  for (int d = 0; d < 32; d += 3)
    lat.cell_state[static_cast<size_t>(d) * 32 + static_cast<size_t>(d)] =
        zt::kVoidAuthored;
  return lat;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  Vzhao_terrain_tess tess;
  Vzhao_terrain_normals norm;
  Chain chain(tess, norm);
  chain.reset();

  const zt::ComposedLattice island = make_island();

  uint32_t top_tris = 0, und_tris = 0, sloped = 0;

  // Every subpatch of the island, every level, both surfaces, stitched and not.
  for (int oz = 0; oz < 32; oz += 8) {
    for (int ox = 0; ox < 32; ox += 8) {
      for (int level = 0; level <= zt::kMaxLevel; ++level) {
        for (int stitch = 0; stitch < 2; ++stitch) {
          for (int surf = 0; surf < 2; ++surf) {
            zt::SubpatchJob job;
            job.ox = ox;
            job.oz = oz;
            job.level = level;
            for (int k = 0; k < 4; ++k) job.nlevel[k] = level;
            if (stitch) {
              job.nlevel[zt::kSideNegZ] = 3;
              job.nlevel[zt::kSidePosX] = level < 3 ? level + 1 : 3;
            }
            job.surface = surf ? zt::Surface::kUnderside : zt::Surface::kTop;
            job.morph = ((ox + oz) & 8) ? 32768 : 0;

            std::vector<zt::MeshTri> mesh;
            const std::vector<Normal> ns = chain.run(island, job, &mesh, surf ? 0u : 0x11111111u);

            const zt::TessResult want = zt::tessellate(island, job);
            check(mesh.size() == want.tris.size(),
                  "the composed chain moves every tessellated triangle across",
                  static_cast<uint32_t>(want.tris.size()), static_cast<uint32_t>(mesh.size()));
            check(ns.size() == mesh.size(), "and NORMALS answers exactly once per triangle",
                  static_cast<uint32_t>(mesh.size()), static_cast<uint32_t>(ns.size()));

            const size_t nn = ns.size() < mesh.size() ? ns.size() : mesh.size();
            for (size_t i = 0; i < nn; ++i) {
              // 1. the normal is the ORACLE's normal of the triangle TESS
              //    actually emitted — the two blocks in one statement
              const zt::NormalVertex a{mesh[i].ax, mesh[i].ay, mesh[i].az};
              const zt::NormalVertex b{mesh[i].bx, mesh[i].by, mesh[i].bz};
              const zt::NormalVertex c{mesh[i].cx, mesh[i].cy, mesh[i].cz};
              const zt::FaceNormal w = zt::face_normal(a, b, c);
              check(ns[i].x == w.x && ns[i].y == w.y && ns[i].z == w.z,
                    "the composed normal is the oracle normal of the emitted triangle",
                    static_cast<uint32_t>(w.y), static_cast<uint32_t>(ns[i].y));
              check(ns[i].degenerate == w.degenerate, "and the degeneracy verdict agrees",
                    w.degenerate ? 1 : 0, ns[i].degenerate ? 1 : 0);
              check(ns[i].src == 0x7000, "the source id survives the whole chain", 0x7000,
                    ns[i].src);

              // 2. THE ASSERTION NEITHER BLOCK CAN MAKE ALONE: which way the
              //    island faces. A flipped winding anywhere in TERRAIN.TESS
              //    lands here and nowhere else.
              if (!surf) {
                check(ns[i].y > 0, "every TOP triangle's normal points UP", 1, ns[i].y > 0 ? 1 : 0);
                ++top_tris;
              } else {
                check(ns[i].y < 0, "every UNDERSIDE triangle's normal points DOWN", 1,
                      ns[i].y < 0 ? 1 : 0);
                ++und_tris;
              }
              if (ns[i].x != 0 || ns[i].z != 0) ++sloped;
            }
          }
        }
      }
    }
  }

  // =========================================================================
  // THE VOID SWEEP -- the same chain over a lattice that is not all solid
  // =========================================================================
  // Added after a fire test showed the sweep above cannot see TERRAIN.TESS's
  // solidity window at all: every cell it uses is kSolid, so the window's
  // answer is always yes and mutating it changes nothing.
  {
    const zt::ComposedLattice holed = make_island_voids();
    uint32_t void_tris = 0;
    for (int oz = 0; oz < 32; oz += 8) {
      for (int ox = 0; ox < 32; ox += 8) {
        for (int level = 0; level <= zt::kMaxLevel; ++level) {
          for (int surf = 0; surf < 2; ++surf) {
            zt::SubpatchJob job;
            job.ox = ox;
            job.oz = oz;
            job.level = level;
            for (int k = 0; k < 4; ++k) job.nlevel[k] = level;
            job.surface = surf ? zt::Surface::kUnderside : zt::Surface::kTop;
            job.morph = 0;

            std::vector<zt::MeshTri> mesh;
            const std::vector<Normal> ns = chain.run(holed, job, &mesh, 0x11111111u);
            const zt::TessResult want = zt::tessellate(holed, job);

            check(mesh.size() == want.tris.size(),
                  "void sweep: the hardware emits exactly the oracle's triangle count",
                  want.tris.size(), mesh.size());
            check(ns.size() == mesh.size(),
                  "void sweep: NORMALS answers once per triangle", mesh.size(), ns.size());
            for (size_t i = 0; i < mesh.size() && i < want.tris.size(); ++i) {
              const zt::MeshTri& w = want.tris[i];
              check(mesh[i].ax == w.ax && mesh[i].ay == w.ay && mesh[i].az == w.az &&
                        mesh[i].bx == w.bx && mesh[i].by == w.by && mesh[i].bz == w.bz &&
                        mesh[i].cx == w.cx && mesh[i].cy == w.cy && mesh[i].cz == w.cz,
                    "void sweep: every vertex matches the oracle", 1, 1);
            }
            void_tris += static_cast<uint32_t>(mesh.size());
          }
        }
      }
    }

    // THE CHECK THAT KEEPS THIS FROM BEING VACUOUS. If the voids did not
    // actually remove geometry, the sweep re-tests the solid case in a new
    // costume and the window is still never asked a real question.
    uint32_t solid_tris = 0;
    for (int oz = 0; oz < 32; oz += 8)
      for (int ox = 0; ox < 32; ox += 8)
        for (int level = 0; level <= zt::kMaxLevel; ++level)
          for (int surf = 0; surf < 2; ++surf) {
            zt::SubpatchJob job;
            job.ox = ox;
            job.oz = oz;
            job.level = level;
            for (int k = 0; k < 4; ++k) job.nlevel[k] = level;
            job.surface = surf ? zt::Surface::kUnderside : zt::Surface::kTop;
            job.morph = 0;
            solid_tris += static_cast<uint32_t>(zt::tessellate(island, job).tris.size());
          }
    check(void_tris < solid_tris,
          "void sweep: the voids REMOVED geometry, so the solidity window was "
          "genuinely exercised",
          1, void_tris < solid_tris ? 1 : 0);
    check(void_tris > 0, "void sweep: and did not remove all of it", 1,
          void_tris > 0 ? 1 : 0);
    std::printf(
        "terrain_tess_normals: void sweep %u triangles against %u solid -- %u removed\n",
        void_tris, solid_tris, solid_tris - void_tris);
  }

  // The sweep must have carried real slope, or "ny > 0" would be a statement
  // about a plane rather than about a deformed island.
  check(top_tris > 1000, "the sweep tessellated a real number of top triangles", 1, top_tris);
  check(und_tris > 1000, "and a real number of underside triangles", 1, und_tris);
  check(sloped > top_tris / 2, "and most of them were genuinely sloped, not flat", 1,
        sloped > top_tris / 2 ? 1 : 0);

  std::printf(
      "terrain_tess_normals: %u top + %u underside triangles composed through both "
      "blocks, %u sloped\n",
      top_tris, und_tris, sloped);

  return zhao::report_and_exit("terrain_tess_normals");
}
