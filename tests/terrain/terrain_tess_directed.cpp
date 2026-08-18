// terrain_tess_directed.cpp — TERRAIN.TESS against its oracle, and the oracle
// against the ratified laws it claims to be a view of.
//
// FOUR LAYERS:
//   1. The oracle's UNSTITCHED path is terrain_rules §4.3's normative
//      pseudocode, restated independently here from the spec text — the fixed
//      i00-i11 diagonal, the emit order (i00,i11,i10) then (i00,i01,i11), the
//      z-then-x scan, and the underside's inverted winding. If this layer is
//      red the oracle is not the law it says it is.
//   2. The oracle's GEOMORPH TARGET is `zref::terrain::column_query` evaluated
//      on the coarse cell. That is what makes "ha + rescale(hb - ha, 1)" a
//      DERIVATION of §4.3 rather than an invention.
//   3. The RTL against the oracle: every stride, both surfaces, all 256
//      neighbour-level combinations at every own level, void cells, geomorph
//      endpoints, backpressure, the counters.
//   4. CRACK-SAFETY read off the RTL's own output: for every level pair, the
//      two subpatches sharing an edge use the IDENTICAL vertex set on it, and
//      that set is exactly the coarser side's stride. This is the contract's
//      stitch invariant, checked on real emitted geometry rather than argued.

#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_tess.h"

#include "tess_harness.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_tess.hpp"

using zhao::check;
namespace zt = zref::terrain;
using tess_test::Driver;
using tess_test::fill_relief;
using tess_test::make_lattice;

namespace {

bool same(const zt::MeshTri& a, const zt::MeshTri& b) {
  return a.ax == b.ax && a.ay == b.ay && a.az == b.az && a.bx == b.bx && a.by == b.by &&
         a.bz == b.bz && a.cx == b.cx && a.cy == b.cy && a.cz == b.cz;
}

/** Compare an RTL stream against the oracle's, triangle for triangle. */
int diff(const std::vector<zt::MeshTri>& got, const std::vector<zt::MeshTri>& want) {
  if (got.size() != want.size()) return -1;
  for (size_t i = 0; i < got.size(); ++i)
    if (!same(got[i], want[i])) return static_cast<int>(i);
  return -2;  // identical
}

void expect_job(Driver& drv, const zt::ComposedLattice& lat, const zt::SubpatchJob& job,
                const char* what, uint32_t stall_mask = 0) {
  const zt::TessResult want = zt::tessellate(lat, job);
  bool rejected = false;
  const std::vector<zt::MeshTri> got = drv.run(lat, job, &rejected, stall_mask);
  check(rejected == (want.verdict != zt::TessVerdict::kOk), what,
        want.verdict != zt::TessVerdict::kOk ? 1 : 0, rejected ? 1 : 0);
  const std::vector<zt::MeshTri>& w = want.tris;
  const int d = diff(got, w);
  check(d == -2, what, static_cast<uint32_t>(w.size()), static_cast<uint32_t>(got.size()));
  if (d >= 0) {
    std::fprintf(stderr, "  first mismatch at triangle %d of %u (%s)\n", d,
                 static_cast<uint32_t>(w.size()), what);
    std::fprintf(
        stderr, "    want a(%d,%d,%d) b(%d,%d,%d) c(%d,%d,%d)\n", w[static_cast<size_t>(d)].ax,
        w[static_cast<size_t>(d)].ay, w[static_cast<size_t>(d)].az, w[static_cast<size_t>(d)].bx,
        w[static_cast<size_t>(d)].by, w[static_cast<size_t>(d)].bz, w[static_cast<size_t>(d)].cx,
        w[static_cast<size_t>(d)].cy, w[static_cast<size_t>(d)].cz);
    std::fprintf(stderr, "    got  a(%d,%d,%d) b(%d,%d,%d) c(%d,%d,%d)\n",
                 got[static_cast<size_t>(d)].ax, got[static_cast<size_t>(d)].ay,
                 got[static_cast<size_t>(d)].az, got[static_cast<size_t>(d)].bx,
                 got[static_cast<size_t>(d)].by, got[static_cast<size_t>(d)].bz,
                 got[static_cast<size_t>(d)].cx, got[static_cast<size_t>(d)].cy,
                 got[static_cast<size_t>(d)].cz);
  }
}

/** Signed 2x area in the (x,z) plane, in whole world units. */
int64_t area2(const zt::MeshTri& t) {
  const int64_t ax = t.ax >> 16, az = t.az >> 16;
  const int64_t bx = t.bx >> 16, bz = t.bz >> 16;
  const int64_t cx = t.cx >> 16, cz = t.cz >> 16;
  return (bx - ax) * (cz - az) - (cx - ax) * (bz - az);
}

// ---------------------------------------------------------------------------
// LAYER 1: the unstitched oracle IS terrain_rules §4.3
// ---------------------------------------------------------------------------
void test_oracle_is_section_4_3() {
  zt::ComposedLattice lat = make_lattice(true);
  fill_relief(lat, 0x1234u);

  for (int level = 0; level <= zt::kMaxLevel; ++level) {
    for (int surf = 0; surf < 2; ++surf) {
      zt::SubpatchJob job;
      job.ox = 8;
      job.oz = 16;
      job.level = level;
      for (int k = 0; k < 4; ++k) job.nlevel[k] = level;  // unstitched
      job.surface = surf ? zt::Surface::kUnderside : zt::Surface::kTop;
      const zt::TessResult r = zt::tessellate(lat, job);

      // §4.3 restated from the spec: cell (i,j) has corners i00=(i,j),
      // i10=(i+1,j), i01=(i,j+1), i11=(i+1,j+1) on the fixed i00-i11 diagonal;
      // the emit order is (i00,i11,i10) then (i00,i01,i11); underside is the
      // same with b and c swapped; scan order is z-then-x.
      const int s = 1 << level;
      const int n = 8 / s;
      const std::vector<int32_t>& plane = surf ? lat.bottom : lat.top;
      std::vector<zt::MeshTri> want;
      const auto V = [&](int vi, int vj, int32_t* x, int32_t* y, int32_t* z) {
        *x = lat.wx[static_cast<size_t>(vi)];
        *z = lat.wz[static_cast<size_t>(vj)];
        *y = plane[static_cast<size_t>(vj) * 33 + static_cast<size_t>(vi)];
      };
      const auto push = [&](int i0, int j0, int i1, int j1, int i2, int j2) {
        zt::MeshTri t;
        V(i0, j0, &t.ax, &t.ay, &t.az);
        if (!surf) {
          V(i1, j1, &t.bx, &t.by, &t.bz);
          V(i2, j2, &t.cx, &t.cy, &t.cz);
        } else {
          V(i2, j2, &t.bx, &t.by, &t.bz);
          V(i1, j1, &t.cx, &t.cy, &t.cz);
        }
        want.push_back(t);
      };
      for (int b = 0; b < n; ++b) {
        for (int a = 0; a < n; ++a) {
          const int i0 = job.ox + a * s, j0 = job.oz + b * s;
          push(i0, j0, i0 + s, j0 + s, i0 + s, j0);
          push(i0, j0, i0, j0 + s, i0 + s, j0 + s);
        }
      }
      check(diff(r.tris, want) == -2, "the unstitched oracle is §4.3's emit order verbatim",
            static_cast<uint32_t>(want.size()), static_cast<uint32_t>(r.tris.size()));
      check(r.tris.size() == static_cast<size_t>(2 * n * n),
            "an unstitched subpatch emits 2 triangles per run-cell",
            static_cast<uint32_t>(2 * n * n), static_cast<uint32_t>(r.tris.size()));
    }
  }

  // The winding, stated as a sign rather than trusted: on a FLAT top surface
  // every triangle's (x,z) orientation must be clockwise, which is what makes
  // e1 x e2 point to +Y. TERRAIN.NORMALS depends on that sign.
  zt::ComposedLattice flat = make_lattice(true);
  zt::SubpatchJob j0;
  j0.ox = 0;
  j0.oz = 0;
  const zt::TessResult top = zt::tessellate(flat, j0);
  int wrong = 0;
  for (const zt::MeshTri& t : top.tris)
    if (area2(t) >= 0) ++wrong;
  check(wrong == 0, "every top triangle winds +Y (clockwise in x,z)", 0,
        static_cast<uint32_t>(wrong));
  j0.surface = zt::Surface::kUnderside;
  const zt::TessResult und = zt::tessellate(flat, j0);
  wrong = 0;
  for (const zt::MeshTri& t : und.tris)
    if (area2(t) <= 0) ++wrong;
  check(wrong == 0, "every underside triangle winds -Y (the inverted winding)", 0,
        static_cast<uint32_t>(wrong));
}

// ---------------------------------------------------------------------------
// LAYER 2: the geomorph target IS §4.3's interpolation of the coarse cell
// ---------------------------------------------------------------------------
// `coarse_height(ha, hb)` is claimed to be column_query on the next-coarser
// cell at u = v = 1/2. Prove it by BUILDING that coarse cell as a 2x2
// ComposedLattice and calling the ratified query.
void test_coarse_height_is_column_query() {
  const int32_t hs[] = {0,  1 << 16,  -(1 << 16), 12345,  -12345, 1,
                        -1, 0x7FFF00, -0x7FFF00,  655360, -655360};
  int checked = 0;
  int odd_sum = 0;
  for (int32_t ha : hs) {
    for (int32_t hb : hs) {
      // A 2x2 coarse cell with the query point exactly at its centre. The z
      // corners are duplicated per row so the diagonal case is the one under
      // test; the x- and z-midpoint cases reduce to the same expression by the
      // §4.3 corner identities, which the RTL relies on.
      zt::ComposedLattice c;
      c.w = c.h = 2;
      c.dual = false;
      c.wx = {0, 4 << 16};
      c.wz = {0, 4 << 16};
      c.top = {ha, ha, hb, hb};  // h00 = h10 = ha, h01 = h11 = hb
      c.cell_state.assign(1, zt::kSolid);
      // At the centre of that cell the point is a z-midpoint (u = 0 side), so
      // column_query takes triangle B and returns h00 + fx_mul(1/2, h01 - h00).
      const zt::ColumnResult r = zt::column_query(c, zref::fx16{0}, zref::fx16{2 << 16});
      check(r.cls == zt::ColumnClass::kSolid, "the coarse-cell probe lands on solid ground", 2,
            static_cast<uint32_t>(r.cls));
      check(r.top.raw == zt::coarse_height(ha, hb),
            "coarse_height == column_query on the coarse cell (the §4.3 derivation)",
            static_cast<uint32_t>(r.top.raw), static_cast<uint32_t>(zt::coarse_height(ha, hb)));
      if (((static_cast<int64_t>(hb) - ha) & 1) != 0) ++odd_sum;
      ++checked;
    }
  }
  check(checked == 121, "the derivation was checked over the whole height cross-product", 121,
        static_cast<uint32_t>(checked));
  // The round-half-up only shows itself on an ODD difference; if the sweep
  // never sampled one it proved nothing about the rounding.
  check(odd_sum > 0, "the sweep actually sampled odd differences (where rounding shows)", 1,
        static_cast<uint32_t>(odd_sum));
  // and the two rounding halves, by hand
  check(zt::coarse_height(0, 1) == 1, "a coarse midpoint of (0,1) rounds half UP to 1", 1,
        static_cast<uint32_t>(zt::coarse_height(0, 1)));
  check(zt::coarse_height(0, -1) == 0, "a coarse midpoint of (0,-1) rounds half up to 0, not -1", 0,
        static_cast<uint32_t>(zt::coarse_height(0, -1)));
  // geomorph endpoints are EXACT
  check(zt::morph_height(1000, 2000, 0) == 1000, "morph factor 0 is the own level exactly", 1000,
        static_cast<uint32_t>(zt::morph_height(1000, 2000, 0)));
  check(zt::morph_height(1000, 2000, 65536) == 2000, "morph factor 1.0 is the coarse level exactly",
        2000, static_cast<uint32_t>(zt::morph_height(1000, 2000, 65536)));
}

/** The set of z coordinates the mesh uses on the vertical line x == xline. */
std::set<int32_t> on_x_line(const std::vector<zt::MeshTri>& tris, int32_t xline) {
  std::set<int32_t> s;
  for (const zt::MeshTri& t : tris) {
    if (t.ax == xline) s.insert(t.az);
    if (t.bx == xline) s.insert(t.bz);
    if (t.cx == xline) s.insert(t.cz);
  }
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  test_oracle_is_section_4_3();
  test_coarse_height_is_column_query();

  Vzhao_terrain_tess dut;
  Driver drv(dut);
  drv.reset();

  zt::ComposedLattice lat = make_lattice(true);
  fill_relief(lat, 0xC0FFEEu);

  // =========================================================================
  // 1. every LOD stride the block accepts, both surfaces, unstitched
  // =========================================================================
  {
    const int want_tris[4] = {128, 32, 8, 2};
    for (int level = 0; level <= zt::kMaxLevel; ++level) {
      for (int surf = 0; surf < 2; ++surf) {
        zt::SubpatchJob job;
        job.ox = 8;
        job.oz = 16;
        job.level = level;
        for (int k = 0; k < 4; ++k) job.nlevel[k] = level;
        job.surface = surf ? zt::Surface::kUnderside : zt::Surface::kTop;
        expect_job(drv, lat, job, "an unstitched subpatch at every legal stride");
        const zt::TessResult r = zt::tessellate(lat, job);
        check(r.tris.size() == static_cast<size_t>(want_tris[level]),
              "the triangle count per stride is 2 * (8/stride)^2",
              static_cast<uint32_t>(want_tris[level]), static_cast<uint32_t>(r.tris.size()));
      }
    }
    // level 3 is ONE cell: the degenerate single-cell subpatch.
    zt::SubpatchJob one;
    one.ox = 0;
    one.oz = 0;
    one.level = 3;
    for (int k = 0; k < 4; ++k) one.nlevel[k] = 3;
    expect_job(drv, lat, one, "a single-cell subpatch (level 3) emits exactly two triangles");
  }

  // =========================================================================
  // 2. the lattice edges and corners
  // =========================================================================
  {
    const int corners[4][2] = {{0, 0}, {24, 0}, {0, 24}, {24, 24}};
    for (const auto& c : corners) {
      for (int level = 0; level <= zt::kMaxLevel; ++level) {
        zt::SubpatchJob job;
        job.ox = c[0];
        job.oz = c[1];
        job.level = level;
        for (int k = 0; k < 4; ++k) job.nlevel[k] = level;
        expect_job(drv, lat, job, "a subpatch at a patch corner");
        job.nlevel[zt::kSideNegZ] = 3;  // and stitched there too
        expect_job(drv, lat, job, "a stitched subpatch at a patch corner");
      }
    }
  }

  // =========================================================================
  // 3. EVERY stitch pattern pair: all 256 neighbour combinations x 4 levels
  // =========================================================================
  {
    int stitched = 0, plain = 0;
    for (int level = 0; level <= zt::kMaxLevel; ++level) {
      for (int combo = 0; combo < 256; ++combo) {
        zt::SubpatchJob job;
        job.ox = 8;
        job.oz = 8;
        job.level = level;
        for (int k = 0; k < 4; ++k) job.nlevel[k] = (combo >> (2 * k)) & 3;
        bool st = false;
        for (int k = 0; k < 4; ++k)
          if (job.nlevel[k] > level) st = true;
        if (st)
          ++stitched;
        else
          ++plain;
        expect_job(drv, lat, job, "every neighbour-level combination at every own level");
      }
    }
    check(stitched > 0 && plain > 0, "the sweep covered both the stitched and the unstitched path",
          1, (stitched > 0 && plain > 0) ? 1 : 0);
    std::printf("terrain_tess_directed: stitch sweep %d stitched / %d plain jobs\n", stitched,
                plain);

    // COVERAGE, not just agreement: every job must tile the subpatch exactly
    // and wind consistently. 8 cells at 2 m = 16 m square -> 2x area = 512.
    for (int level = 0; level <= zt::kMaxLevel; ++level) {
      for (int combo = 0; combo < 256; ++combo) {
        zt::SubpatchJob job;
        job.ox = 8;
        job.oz = 8;
        job.level = level;
        for (int k = 0; k < 4; ++k) job.nlevel[k] = (combo >> (2 * k)) & 3;
        const zt::TessResult r = zt::tessellate(lat, job);
        int64_t a = 0;
        int wrong = 0;
        for (const zt::MeshTri& t : r.tris) {
          const int64_t s2 = area2(t);
          a += s2;
          if (s2 >= 0) ++wrong;
        }
        check(a == -512, "the stitched subpatch tiles its 16 m square exactly, no gap or overlap",
              512, static_cast<uint32_t>(-a));
        check(wrong == 0, "every stitched triangle keeps the +Y winding", 0,
              static_cast<uint32_t>(wrong));
      }
    }
  }

  // =========================================================================
  // 4. CRACK-SAFETY, read off the RTL's own emitted geometry
  // =========================================================================
  {
    for (int la = 0; la <= 3; ++la) {
      for (int lb = 0; lb <= 3; ++lb) {
        zt::SubpatchJob A;
        A.ox = 8;
        A.oz = 8;
        A.level = la;
        for (int k = 0; k < 4; ++k) A.nlevel[k] = la;
        A.nlevel[zt::kSidePosX] = lb;
        zt::SubpatchJob B;
        B.ox = 16;
        B.oz = 8;
        B.level = lb;
        for (int k = 0; k < 4; ++k) B.nlevel[k] = lb;
        B.nlevel[zt::kSideNegX] = la;
        bool ra = false, rb = false;
        const std::vector<zt::MeshTri> ta = drv.run(lat, A, &ra);
        const std::vector<zt::MeshTri> tb = drv.run(lat, B, &rb);
        const int32_t xline = lat.wx[16];
        const std::set<int32_t> sa = on_x_line(ta, xline);
        const std::set<int32_t> sb = on_x_line(tb, xline);
        check(sa == sb, "adjacent subpatches use the IDENTICAL vertex set on their shared edge",
              static_cast<uint32_t>(sa.size()), static_cast<uint32_t>(sb.size()));
        const int es = 1 << (la > lb ? la : lb);
        check(sa.size() == static_cast<size_t>(8 / es + 1),
              "and that set is exactly the coarser side's stride",
              static_cast<uint32_t>(8 / es + 1), static_cast<uint32_t>(sa.size()));
      }
    }
  }

  // =========================================================================
  // 5. void columns
  // =========================================================================
  {
    zt::ComposedLattice v = make_lattice(true);
    fill_relief(v, 0xBEEF);
    // one void cell inside the subpatch at (8,8)
    v.cell_state[static_cast<size_t>(10) * 32 + 11] = zt::kVoidAuthored;
    for (int level = 0; level <= zt::kMaxLevel; ++level) {
      zt::SubpatchJob job;
      job.ox = 8;
      job.oz = 8;
      job.level = level;
      for (int k = 0; k < 4; ++k) job.nlevel[k] = level;
      expect_job(drv, v, job, "one void cell removes exactly the run-cells covering it");
      const zt::TessResult r = zt::tessellate(v, job);
      const int n = 8 >> level;
      check(r.tris.size() == static_cast<size_t>(2 * n * n - 2),
            "a void removes exactly ONE run-cell's pair at every stride",
            static_cast<uint32_t>(2 * n * n - 2), static_cast<uint32_t>(r.tris.size()));
    }
    // a BREACHED cell counts the same as an authored one (§3.2: both are void)
    v.cell_state[static_cast<size_t>(10) * 32 + 11] = zt::kVoidBreached;
    zt::SubpatchJob jb;
    jb.ox = 8;
    jb.oz = 8;
    expect_job(drv, v, jb, "a breached cell is void too");

    // an ALL-void subpatch emits nothing at all — legal and common on sparse
    // islands, not an error
    zt::ComposedLattice av = make_lattice(true);
    for (int cj = 8; cj < 16; ++cj)
      for (int ci = 8; ci < 16; ++ci)
        av.cell_state[static_cast<size_t>(cj) * 32 + static_cast<size_t>(ci)] = zt::kVoidAuthored;
    for (int level = 0; level <= zt::kMaxLevel; ++level) {
      zt::SubpatchJob job;
      job.ox = 8;
      job.oz = 8;
      job.level = level;
      for (int k = 0; k < 4; ++k) job.nlevel[k] = level;
      expect_job(drv, av, job, "an all-void subpatch emits nothing");
      check(zt::tessellate(av, job).tris.empty(), "and it is empty, not rejected", 0,
            static_cast<uint32_t>(zt::tessellate(av, job).tris.size()));
    }

    // THE REJECT PATH: coarsened AND carrying a void.
    const uint32_t rej_before = dut.subpatch_rejected_o;
    zt::SubpatchJob rj;
    rj.ox = 8;
    rj.oz = 8;
    rj.level = 1;
    for (int k = 0; k < 4; ++k) rj.nlevel[k] = 1;
    rj.nlevel[zt::kSidePosZ] = 2;  // a coarser neighbour: the annulus engages
    bool rejected = false;
    const std::vector<zt::MeshTri> got = drv.run(v, rj, &rejected);
    check(rejected, "a coarsened subpatch carrying a void is REJECTED, not guessed at", 1,
          rejected ? 1 : 0);
    check(got.empty(), "and it emits nothing", 0, static_cast<uint32_t>(got.size()));
    check(dut.subpatch_rejected_o == rej_before + 1, "subpatch_rejected counts the reject",
          rej_before + 1, dut.subpatch_rejected_o);
    check(zt::tessellate(v, rj).verdict == zt::TessVerdict::kRejectVoidStitch,
          "and the oracle agrees on the verdict", 1,
          zt::tessellate(v, rj).verdict == zt::TessVerdict::kRejectVoidStitch ? 1 : 0);
    // the SAME subpatch unstitched is fine: the reject is about the stitch
    rj.nlevel[zt::kSidePosZ] = 1;
    expect_job(drv, v, rj, "the same void subpatch unstitched tessellates normally");
  }

  // =========================================================================
  // 6. the legacy single-surface page (terrain_rules §3.1 option (a))
  // =========================================================================
  {
    zt::ComposedLattice legacy = make_lattice(false);
    fill_relief(legacy, 0x5150);
    // a legacy page carrying cell state must still draw every cell: the
    // reference's void test is guarded by `dual`
    legacy.cell_state[static_cast<size_t>(9) * 32 + 9] = zt::kVoidAuthored;
    zt::SubpatchJob job;
    job.ox = 8;
    job.oz = 8;
    expect_job(drv, legacy, job, "a legacy page ignores cell state entirely");
    check(zt::tessellate(legacy, job).tris.size() == 128, "and emits every cell of the subpatch",
          128, static_cast<uint32_t>(zt::tessellate(legacy, job).tris.size()));
    job.surface = zt::Surface::kUnderside;
    expect_job(drv, legacy, job, "a legacy page has no underside at all");
    check(zt::tessellate(legacy, job).tris.empty(), "the underside job emits nothing", 0,
          static_cast<uint32_t>(zt::tessellate(legacy, job).tris.size()));

    // A LEGACY PAGE CAN STILL BE STITCHED. LOD levels have nothing to do with
    // whether a page models an underside, and the first version of this block
    // forced the plain path for every non-dual page — emitting the full grid
    // where the annulus was required, and cracking against its coarser
    // neighbour. The randomized lane B found it; this pins it.
    job.surface = zt::Surface::kTop;
    for (int level = 0; level <= zt::kMaxLevel; ++level) {
      for (int combo = 0; combo < 256; ++combo) {
        job.level = level;
        for (int k = 0; k < 4; ++k) job.nlevel[k] = (combo >> (2 * k)) & 3;
        expect_job(drv, legacy, job, "a legacy page obeys the stitch just like a dual one");
      }
    }
  }

  // =========================================================================
  // 7. geomorph
  // =========================================================================
  {
    zt::ComposedLattice g = make_lattice(true);
    fill_relief(g, 0x9911);
    for (int level = 0; level <= zt::kMaxLevel; ++level) {
      for (int32_t m : {0, 1, 16384, 32768, 65535, 65536}) {
        zt::SubpatchJob job;
        job.ox = 8;
        job.oz = 8;
        job.level = level;
        for (int k = 0; k < 4; ++k) job.nlevel[k] = level;
        job.morph = m;
        expect_job(drv, g, job, "geomorph at every factor and every stride");
        job.nlevel[zt::kSideNegX] = 3;  // and composed with the stitch
        expect_job(drv, g, job, "geomorph composed with the annulus");
        job.nlevel[zt::kSideNegX] = level;
        job.surface = zt::Surface::kUnderside;
        expect_job(drv, g, job, "geomorph on the underside plane");
        job.surface = zt::Surface::kTop;
      }
    }

    // ENDPOINT EXACTNESS: factor 0 must be byte-identical to no geomorph.
    zt::SubpatchJob z0;
    z0.ox = 8;
    z0.oz = 8;
    z0.level = 1;
    for (int k = 0; k < 4; ++k) z0.nlevel[k] = 1;
    const zt::TessResult none = zt::tessellate(g, z0);
    z0.morph = 0;
    check(diff(zt::tessellate(g, z0).tris, none.tris) == -2,
          "morph factor 0 is byte-identical to no geomorph at all", 1, 1);

    // BOUNDARY VERTICES NEVER MOVE (chosen law 4): at full morph the subpatch
    // boundary must carry exactly the un-morphed lattice heights, or the crack
    // argument is void.
    z0.morph = 65536;
    const zt::TessResult full = zt::tessellate(g, z0);
    int moved_border = 0, moved_interior = 0;
    const auto probe = [&](int32_t x, int32_t y, int32_t z) {
      // recover the lattice index from the placed coordinate (2 m pitch)
      const int vi = (x >> 16) / 2, vj = (z >> 16) / 2;
      const int32_t h = g.top[static_cast<size_t>(vj) * 33 + static_cast<size_t>(vi)];
      const bool border = vi <= z0.ox || vi >= z0.ox + 8 || vj <= z0.oz || vj >= z0.oz + 8;
      if (y != h) {
        if (border)
          ++moved_border;
        else
          ++moved_interior;
      }
    };
    for (const zt::MeshTri& t : full.tris) {
      probe(t.ax, t.ay, t.az);
      probe(t.bx, t.by, t.bz);
      probe(t.cx, t.cy, t.cz);
    }
    check(moved_border == 0, "no subpatch-boundary vertex is ever moved by geomorph", 0,
          static_cast<uint32_t>(moved_border));
    check(moved_interior > 0, "and interior vertices genuinely DID move at full factor", 1,
          static_cast<uint32_t>(moved_interior));

    // THE ROUNDING BOUNDARY, on the RTL and not just the oracle. The geomorph
    // target is `ha + rescale(hb - ha, 1)`, a round-half-up halving, and the
    // half is only visible when the parent difference is ODD. Build a lattice
    // whose two coarse parents differ by exactly +1 and exactly -1 and read the
    // emitted vertex back: +1 must round UP to 1, and -1 must round to 0, not
    // -1. A truncating shift passes every other case in this file.
    for (int sign = 0; sign < 2; ++sign) {
      zt::ComposedLattice h = make_lattice(true);
      // level 1 (stride 2) at subpatch (8,8): vertex (10,10) is a DIAGONAL
      // midpoint of the coarse cell whose corners are (8,8) and (12,12).
      const int32_t hb = sign ? -1 : 1;
      h.top[static_cast<size_t>(8) * 33 + 8] = 0;
      h.top[static_cast<size_t>(12) * 33 + 12] = hb;
      zt::SubpatchJob j;
      j.ox = 8;
      j.oz = 8;
      j.level = 1;
      for (int k = 0; k < 4; ++k) j.nlevel[k] = 1;
      j.morph = 65536;
      expect_job(drv, h, j, "the geomorph rounding boundary, both halves");
      const int32_t want_y = zt::coarse_height(0, hb);
      check(want_y == (sign ? 0 : 1), "a coarse midpoint of (0,+-1) rounds half UP",
            static_cast<uint32_t>(sign ? 0 : 1), static_cast<uint32_t>(want_y));
      bool rej = false;
      const std::vector<zt::MeshTri> got = drv.run(h, j, &rej);
      const int32_t px = h.wx[10], pz = h.wz[10];
      int seen_probe = 0, wrong = 0;
      const auto probe = [&](int32_t x, int32_t y, int32_t z) {
        if (x != px || z != pz) return;
        ++seen_probe;
        if (y != want_y) ++wrong;
      };
      for (const zt::MeshTri& t : got) {
        probe(t.ax, t.ay, t.az);
        probe(t.bx, t.by, t.bz);
        probe(t.cx, t.cy, t.cz);
      }
      check(seen_probe > 0, "the rounding-boundary vertex was actually emitted", 1,
            static_cast<uint32_t>(seen_probe));
      check(wrong == 0, "and it carries the round-half-up coarse height", 0,
            static_cast<uint32_t>(wrong));
    }

    // the out-of-range morph factor is clamped and counted
    const uint32_t cl_before = dut.lod_clamped_o;
    zt::SubpatchJob big = z0;
    big.morph = 100000;
    bool rej = false;
    const std::vector<zt::MeshTri> got = drv.run(g, big, &rej);
    check(dut.lod_clamped_o == cl_before + 1, "an out-of-range morph factor is counted",
          cl_before + 1, dut.lod_clamped_o);
    check(diff(got, full.tris) == -2, "and clamped to exactly 1.0, not rejected", 1, 1);
    check(zt::tessellate(g, big).lod_clamped == 1, "the oracle counts the clamp too", 1,
          zt::tessellate(g, big).lod_clamped);
  }

  // =========================================================================
  // 8. backpressure
  // =========================================================================
  {
    zt::SubpatchJob job;
    job.ox = 8;
    job.oz = 8;
    job.level = 0;
    for (int k = 0; k < 4; ++k) job.nlevel[k] = 0;
    // several stall schedules, including one that stalls most cycles
    for (uint32_t mask : {0x55555555u, 0xAAAAAAAAu, 0xFFFFFFFEu, 0x0F0F0F0Fu}) {
      expect_job(drv, lat, job, "a stalled consumer loses no triangle and corrupts none", mask);
    }
    job.nlevel[zt::kSidePosX] = 2;
    for (uint32_t mask : {0x55555555u, 0xFFFFFFFEu}) {
      expect_job(drv, lat, job, "and the same on the stitched path", mask);
    }
  }

  // =========================================================================
  // 9. counters, source ids and idle
  // =========================================================================
  {
    drv.reset();
    drv.set_src(0xABCD);
    zt::SubpatchJob job;
    job.ox = 0;
    job.oz = 0;
    job.level = 1;
    for (int k = 0; k < 4; ++k) job.nlevel[k] = 1;
    bool rej = false;
    const std::vector<zt::MeshTri> a = drv.run(lat, job, &rej);
    check(dut.terrain_triangles_emitted_o == 32,
          "terrain_triangles_emitted counts emitted triangles", 32,
          dut.terrain_triangles_emitted_o);
    check(drv.last_src() == 0xABCD, "src_id rides the mesh", 0xABCD, drv.last_src());
    check(!drv.last_surface(), "the surface tag rides the mesh (top)", 0,
          drv.last_surface() ? 1 : 0);
    job.surface = zt::Surface::kUnderside;
    const std::vector<zt::MeshTri> b = drv.run(lat, job, &rej);
    check(dut.terrain_triangles_emitted_o == 64, "and accumulates across jobs", 64,
          dut.terrain_triangles_emitted_o);
    check(drv.last_surface(), "the surface tag rides the mesh (underside)", 1,
          drv.last_surface() ? 1 : 0);
    check(a.size() == 32 && b.size() == 32, "both surfaces emitted their 32 triangles", 32,
          static_cast<uint32_t>(a.size()));
    check(dut.idle_o == 1, "the block reports idle when drained", 1, dut.idle_o);
  }

  // =========================================================================
  // 10. throughput — the ledger's "1 emitted vertex per clock", MEASURED
  // =========================================================================
  {
    drv.reset();
    zt::SubpatchJob job;
    job.ox = 8;
    job.oz = 8;
    job.level = 0;
    for (int k = 0; k < 4; ++k) job.nlevel[k] = 0;
    bool rej = false;
    drv.reset_counters();
    const std::vector<zt::MeshTri> t = drv.run(lat, job, &rej);
    const double cyc_per_tri = static_cast<double>(drv.cycles()) / static_cast<double>(t.size());
    std::printf(
        "terrain_tess_directed: level 0, no morph — %llu cycles for %u triangles "
        "(%.2f cycles/triangle, %.2f vertices/clock)\n",
        static_cast<unsigned long long>(drv.cycles()), static_cast<uint32_t>(t.size()), cyc_per_tri,
        3.0 / cyc_per_tri);
    // 3 lattice reads per triangle at one read per clock is the target rate;
    // the 8x8 cell-state pre-scan is a fixed 65-cycle prologue per subpatch.
    check(drv.cycles() <= 3 * t.size() + 75,
          "the steady-state rate is one lattice read (one vertex) per clock",
          static_cast<uint32_t>(3 * t.size() + 75), static_cast<uint32_t>(drv.cycles()));

    drv.reset_counters();
    job.morph = 32768;
    const std::vector<zt::MeshTri> tm = drv.run(lat, job, &rej);
    const double cyc_m = static_cast<double>(drv.cycles()) / static_cast<double>(tm.size());
    std::printf(
        "terrain_tess_directed: level 0, morph 0.5 — %llu cycles for %u triangles "
        "(%.2f cycles/triangle)\n",
        static_cast<unsigned long long>(drv.cycles()), static_cast<uint32_t>(tm.size()), cyc_m);
  }

  return zhao::report_and_exit("terrain_tess_directed");
}
