// terrain_tess_modes_directed.cpp — TERRAIN.TESS's VERTEX MODE and REFERENCE
// MODE against the oracle that already defines them (2026-09-10).
//
// reports/PROJECTION-ADOPTION-20260910.md §7 items 1 and 2. The projected-
// vertex arena shell (`zhao_terrain_wcache`, DENSE_SEAL at DEPTH = 81) is
// filled per job with the 81 window vertices in index order, geomorph applied,
// and replayed from index TRIPLES. The differential that proved the shell
// (terrain_wcache_differential) produced those 81 vertices with
// `zref::terrain::detail::vertex_at` — the function `tessellate` calls per
// corner — and the triples with a level-0-only walker. This test is the
// hardware producer against that same oracle, over the SAME case space the
// identity probe (terrain_identity_probe.cpp) measured: 3 lattices x 3 origins
// x 4 own levels x 4^4 neighbour levels x 6 morph factors x 2 surfaces =
// 110,592 jobs.
//
// THREE CHECKS PER JOB, and the third is the one the consumer cares about:
//
//   VTX   the 81 emitted vertices equal `vertex_at` on every vertex of the
//         job's own stride grid, and equal the PLAIN lattice vertex
//         (`vertex_at` with morph 0) on every other window vertex, with
//         `vtx_stride_o` telling the two apart (RTL header, chosen law 6);
//         indices 0..80 in order; surface and src_id ride every one.
//   REF   the emitted triples equal `tessellate`'s triangle stream with every
//         corner inverted to its window index, in order, underside b/c swapped.
//   REF∘VTX  applying the triples to the vertices REBUILDS `tessellate`'s
//         triangles bit for bit. That is the arena's replay, done in software
//         on the two hardware streams: fill + references == the mesh.
//
// The reject verdict (stitched + void) must agree in every mode (law 7); a
// legacy-page underside emits nothing in every mode.
//
// WHY "vertex_at OVER ALL 81" IS NOT THE CRITERION AT LEVEL >= 1, stated here
// because the brief said it was: at level 1 with ox = 0, vertex vi = 1 is
// off the stride grid, `morph_case` calls it a diagonal midpoint, and
// `vertex_at` reads its parent at vi - s = -1 — outside the lattice. The
// reference is undefined on legal input there. Only the stride-grid vertices
// can be corners (the identity probe measured that), so those ARE `vertex_at`
// and the rest are fillers the arena's dense-seal discipline needs. The test
// asserts exactly that, and asserts `vtx_stride_o` separates them.

#include <cstdint>
#include <cstdio>
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
using tess_test::EmitRef;
using tess_test::EmitVert;

namespace {

constexpr int32_t kOne = 1 << 16;
constexpr int kSide = 9;
constexpr int kDepth = 81;

// ---- the identity probe's lattices, verbatim -------------------------------
struct Rng {
  uint32_t s = 0xC0FFEE11u;
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  int32_t range(int32_t lo, int32_t hi) {
    return lo + static_cast<int32_t>(next() % static_cast<uint32_t>(hi - lo + 1));
  }
};

zt::ComposedLattice make_lattice(Rng& rng, bool dual, bool with_void) {
  zt::ComposedLattice lat;
  lat.w = 33;
  lat.h = 33;
  lat.dual = dual;
  lat.wx.resize(33);
  lat.wz.resize(33);
  lat.top.resize(33 * 33);
  lat.bottom.resize(33 * 33);
  for (int i = 0; i < 33; ++i) {
    lat.wx[static_cast<size_t>(i)] = -8 * kOne + i * (kOne / 2);
    lat.wz[static_cast<size_t>(i)] = -4 * kOne + i * (kOne / 2);
  }
  for (size_t k = 0; k < lat.top.size(); ++k) {
    lat.top[k] = rng.range(-3 * kOne, 3 * kOne);
    lat.bottom[k] = lat.top[k] - rng.range(kOne / 4, 2 * kOne);
  }
  if (dual && with_void) {
    lat.cell_state.assign(32 * 32, 0);  // SOLID
    const int voids[4][2] = {{3, 3}, {12, 20}, {30, 1}, {17, 17}};
    for (auto& v : voids) lat.cell_state[static_cast<size_t>(v[1]) * 32 + static_cast<size_t>(v[0])] = 1;
  }
  return lat;
}

int find_index(const std::vector<int32_t>& axis, int32_t v) {
  for (size_t i = 0; i < axis.size(); ++i)
    if (axis[i] == v) return static_cast<int>(i);
  return -1;
}

// ---- the oracle for one job -------------------------------------------------
struct Want {
  zt::TessResult tess;              // the triangles, verdict, clamp count
  std::vector<zt::detail::TessVert> verts;  // 81, index order
  std::vector<bool> stride;         // 81: on the own stride grid?
  std::vector<int> ref;             // 3 per triangle, window indices, emit order
  bool ref_ok = true;               // every corner inverted to a window index
};

Want oracle(const zt::ComposedLattice& lat, const zt::SubpatchJob& job) {
  Want w;
  w.tess = zt::tessellate(lat, job, nullptr);
  zt::SubpatchJob plain = job;
  plain.morph = 0;
  const int32_t m = job.morph > 65536 ? 65536 : job.morph;  // the block's clamp
  const int s = 1 << job.level;
  w.verts.resize(kDepth);
  w.stride.resize(kDepth);
  for (int k = 0; k < kDepth; ++k) {
    const int vi = job.ox + k % kSide, vj = job.oz + k / kSide;
    const bool on = ((vi - job.ox) % s == 0) && ((vj - job.oz) % s == 0);
    w.stride[static_cast<size_t>(k)] = on;
    w.verts[static_cast<size_t>(k)] = on ? zt::detail::vertex_at(lat, job, vi, vj, m, nullptr)
                                         : zt::detail::vertex_at(lat, plain, vi, vj, 0, nullptr);
  }
  for (const zt::MeshTri& t : w.tess.tris) {
    const int32_t xs[3] = {t.ax, t.bx, t.cx};
    const int32_t zs[3] = {t.az, t.bz, t.cz};
    for (int c = 0; c < 3; ++c) {
      const int vi = find_index(lat.wx, xs[c]);
      const int vj = find_index(lat.wz, zs[c]);
      if (vi < job.ox || vi > job.ox + 8 || vj < job.oz || vj > job.oz + 8) {
        w.ref_ok = false;
        w.ref.push_back(-1);
      } else {
        w.ref.push_back((vj - job.oz) * kSide + (vi - job.ox));
      }
    }
  }
  return w;
}

// ---- comparators: they COUNT, so a positive control can read them ----------
int diff_verts(const std::vector<EmitVert>& got, const Want& want, bool surf, uint16_t src) {
  if (got.size() != static_cast<size_t>(kDepth)) return 1000 + static_cast<int>(got.size());
  int bad = 0;
  for (int k = 0; k < kDepth; ++k) {
    const EmitVert& g = got[static_cast<size_t>(k)];
    const zt::detail::TessVert& v = want.verts[static_cast<size_t>(k)];
    if (g.index != k || g.x != v.x || g.y != v.y || g.z != v.z ||
        g.stride != want.stride[static_cast<size_t>(k)] || g.surface != surf || g.src != src)
      ++bad;
  }
  return bad;
}

int diff_refs(const std::vector<EmitRef>& got, const Want& want, bool surf, uint16_t src) {
  if (got.size() * 3 != want.ref.size()) return 1000 + static_cast<int>(got.size());
  int bad = 0;
  for (size_t t = 0; t < got.size(); ++t) {
    const EmitRef& g = got[t];
    if (g.ia != want.ref[3 * t] || g.ib != want.ref[3 * t + 1] || g.ic != want.ref[3 * t + 2] ||
        g.surface != surf || g.src != src)
      ++bad;
  }
  return bad;
}

bool same_tri(const zt::MeshTri& a, const zt::MeshTri& b) {
  return a.ax == b.ax && a.ay == b.ay && a.az == b.az && a.bx == b.bx && a.by == b.by &&
         a.bz == b.bz && a.cx == b.cx && a.cy == b.cy && a.cz == b.cz;
}

// the arena's replay, in software: triples applied to the fill
int diff_rebuild(const std::vector<EmitVert>& verts, const std::vector<EmitRef>& refs,
                 const std::vector<zt::MeshTri>& want) {
  if (verts.size() != static_cast<size_t>(kDepth) || refs.size() != want.size())
    return 1000 + static_cast<int>(refs.size());
  int bad = 0;
  for (size_t t = 0; t < refs.size(); ++t) {
    const EmitRef& r = refs[t];
    if (r.ia >= kDepth || r.ib >= kDepth || r.ic >= kDepth) {
      ++bad;
      continue;
    }
    zt::MeshTri g;
    g.ax = verts[r.ia].x;
    g.ay = verts[r.ia].y;
    g.az = verts[r.ia].z;
    g.bx = verts[r.ib].x;
    g.by = verts[r.ib].y;
    g.bz = verts[r.ib].z;
    g.cx = verts[r.ic].x;
    g.cy = verts[r.ic].y;
    g.cz = verts[r.ic].z;
    if (!same_tri(g, want[t])) ++bad;
  }
  return bad;
}

int diff_tris(const std::vector<zt::MeshTri>& got, const std::vector<zt::MeshTri>& want) {
  if (got.size() != want.size()) return 1000 + static_cast<int>(got.size());
  int bad = 0;
  for (size_t t = 0; t < got.size(); ++t)
    if (!same_tri(got[t], want[t])) ++bad;
  return bad;
}

// backpressure rides the sweep rather than living in one case
const uint32_t kStalls[7] = {0u, 0u, 0x1u, 0xAAAAAAAAu, 0u, 0x0000FFFFu, 0x7FFFFFFEu};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_tess dut;
  Driver drv(dut);
  drv.reset();
  const uint16_t kSrc = 0x5A17;
  drv.set_src(kSrc);

  Rng rng;
  const zt::ComposedLattice lats[3] = {make_lattice(rng, true, false), make_lattice(rng, true, true),
                                       make_lattice(rng, false, false)};
  const char* lat_names[3] = {"dual", "dual+void", "legacy"};
  const int32_t morphs[6] = {0, 1, 0x4000, 0x8000, 0xFFFF, 0x10000};
  const int origins[3][2] = {{0, 0}, {8, 16}, {24, 24}};

  // =========================================================================
  // 1. the sweep — the identity probe's case space, three checks per job
  // =========================================================================
  long jobs = 0, rejected = 0, empty_underside = 0;
  long vtx_jobs = 0, vtx_verts = 0, vtx_bad = 0, vtx_first_bad = -1;
  long ref_jobs = 0, ref_tris = 0, ref_bad = 0, ref_first_bad = -1;
  long rebuild_bad = 0, rebuild_first_bad = -1;
  long tri_jobs = 0, tri_tris = 0, tri_bad = 0;
  long verdict_bad = 0, oracle_ref_bad = 0;
  long stride_zero_seen = 0, stride_one_seen = 0;
  long per_level_fills[4] = {0, 0, 0, 0}, per_level_reads[4] = {0, 0, 0, 0};
  long per_level_jobs[4] = {0, 0, 0, 0};
  uint32_t stall_i = 0;

  for (int li = 0; li < 3; ++li) {
    const zt::ComposedLattice& lat = lats[li];
    for (int oi = 0; oi < 3; ++oi)
      for (int level = 0; level <= zt::kMaxLevel; ++level)
        for (int nl = 0; nl < 256; ++nl)
          for (int mi = 0; mi < 6; ++mi)
            for (int surf = 0; surf < 2; ++surf) {
              zt::SubpatchJob job;
              job.ox = origins[oi][0];
              job.oz = origins[oi][1];
              job.level = level;
              job.nlevel[0] = nl & 3;
              job.nlevel[1] = (nl >> 2) & 3;
              job.nlevel[2] = (nl >> 4) & 3;
              job.nlevel[3] = (nl >> 6) & 3;
              job.morph = morphs[mi];
              job.surface = surf ? zt::Surface::kUnderside : zt::Surface::kTop;
              ++jobs;
              const Want want = oracle(lat, job);
              const bool want_rej = want.tess.verdict != zt::TessVerdict::kOk;
              const bool legacy_under = !lat.dual && surf;
              if (want_rej) ++rejected;
              if (!want_rej && want.tess.tris.empty()) ++empty_underside;
              if (!want.ref_ok) ++oracle_ref_bad;
              const uint32_t stall = kStalls[stall_i++ % 7];

              // ---- VTX ----
              drv.set_mode(1);
              bool rej = false;
              drv.reset_counters();
              (void)drv.run(lat, job, &rej, stall);
              if (rej != want_rej) ++verdict_bad;
              ++vtx_jobs;
              if (want_rej || legacy_under) {
                if (!drv.verts.empty()) ++vtx_bad;
              } else {
                const int d = diff_verts(drv.verts, want, surf != 0, kSrc);
                vtx_verts += static_cast<long>(drv.verts.size());
                if (d != 0) {
                  ++vtx_bad;
                  if (vtx_first_bad < 0) vtx_first_bad = jobs - 1;
                }
                for (const EmitVert& v : drv.verts) (v.stride ? stride_one_seen : stride_zero_seen)++;
                if (stall == 0) {
                  ++per_level_jobs[level];
                  per_level_fills[level] += static_cast<long>(drv.verts.size());
                  per_level_reads[level] += static_cast<long>(drv.cycles());
                }
              }
              const std::vector<EmitVert> verts = drv.verts;

              // ---- REF ----
              drv.set_mode(2);
              rej = false;
              (void)drv.run(lat, job, &rej, stall);
              if (rej != want_rej) ++verdict_bad;
              ++ref_jobs;
              if (want_rej || legacy_under) {
                if (!drv.refs.empty()) ++ref_bad;
              } else {
                const int d = diff_refs(drv.refs, want, surf != 0, kSrc);
                ref_tris += static_cast<long>(drv.refs.size());
                if (d != 0) {
                  ++ref_bad;
                  if (ref_first_bad < 0) ref_first_bad = jobs - 1;
                }
                // ---- REF applied to VTX rebuilds the mesh ----
                const int rb = diff_rebuild(verts, drv.refs, want.tess.tris);
                if (rb != 0) {
                  ++rebuild_bad;
                  if (rebuild_first_bad < 0) rebuild_first_bad = jobs - 1;
                }
              }

              // ---- TRI, every eighth job: mode 0 on this lattice too ----
              if ((jobs & 7) == 0) {
                drv.set_mode(0);
                rej = false;
                const std::vector<zt::MeshTri> got = drv.run(lat, job, &rej, stall);
                if (rej != want_rej) ++verdict_bad;
                ++tri_jobs;
                if (!want_rej) {
                  tri_tris += static_cast<long>(got.size());
                  if (diff_tris(got, want.tess.tris) != 0) ++tri_bad;
                }
              }
            }
    std::printf("terrain_tess_modes_directed: lattice %-10s done (%ld jobs so far)\n", lat_names[li], jobs);
  }

  std::printf("terrain_tess_modes_directed: %ld jobs (rejected %ld, legacy-underside empty %ld); "
              "VTX %ld jobs / %ld vertices, REF %ld jobs / %ld triples, TRI %ld jobs / %ld triangles\n",
              jobs, rejected, empty_underside, vtx_jobs, vtx_verts, ref_jobs, ref_tris, tri_jobs, tri_tris);
  check(jobs == 110592, "the sweep is the identity probe's case space", 110592, static_cast<uint64_t>(jobs));
  check(rejected > 0, "the void+stitch reject was exercised", 1, static_cast<uint64_t>(rejected));
  check(oracle_ref_bad == 0, "the oracle inverted every corner to a window index", 0,
        static_cast<uint64_t>(oracle_ref_bad));
  check(verdict_bad == 0, "law 7: the reject verdict agrees in every mode", 0, static_cast<uint64_t>(verdict_bad));
  check(vtx_bad == 0, "VTX: 81 vertices == vertex_at (stride grid) / plain (fillers), index order, flags",
        0, static_cast<uint64_t>(vtx_bad));
  if (vtx_first_bad >= 0) std::printf("  first VTX mismatch at sweep job %ld\n", vtx_first_bad);
  check(ref_bad == 0, "REF: triples == tessellate's corners as window indices, in order", 0,
        static_cast<uint64_t>(ref_bad));
  if (ref_first_bad >= 0) std::printf("  first REF mismatch at sweep job %ld\n", ref_first_bad);
  check(rebuild_bad == 0, "REF applied to VTX rebuilds tessellate's triangles bit for bit", 0,
        static_cast<uint64_t>(rebuild_bad));
  if (rebuild_first_bad >= 0) std::printf("  first rebuild mismatch at sweep job %ld\n", rebuild_first_bad);
  check(tri_bad == 0, "TRI (mode 0) on the probe's lattices == tessellate", 0, static_cast<uint64_t>(tri_bad));
  check(stride_one_seen > 0 && stride_zero_seen > 0, "vtx_stride_o was seen at both values", 1,
        (stride_one_seen > 0 && stride_zero_seen > 0) ? 1 : 0);
  // the cost table the adoption report carries forward (§3), now MEASURED on
  // the hardware producer: cycles per fill per level, consumer always ready
  for (int l = 0; l <= 3; ++l)
    if (per_level_jobs[l] > 0)
      std::printf("  level %d: %ld unstalled fills, %ld vertices, %.1f cycles per job (mean, incl. drain; "
                  "stitched jobs carry the 65-cycle scan)\n",
                  l, per_level_jobs[l], per_level_fills[l],
                  static_cast<double>(per_level_reads[l]) / static_cast<double>(per_level_jobs[l]));

  // ---- the counters, against the sweep's own tallies ----
  check(dut.terrain_vertices_emitted_o == static_cast<uint32_t>(vtx_verts),
        "terrain_vertices_emitted_o == vertices the sweep collected", static_cast<uint64_t>(vtx_verts),
        dut.terrain_vertices_emitted_o);
  check(dut.terrain_refs_emitted_o == static_cast<uint32_t>(ref_tris),
        "terrain_refs_emitted_o == triples the sweep collected", static_cast<uint64_t>(ref_tris),
        dut.terrain_refs_emitted_o);
  check(dut.terrain_triangles_emitted_o == static_cast<uint32_t>(tri_tris),
        "terrain_triangles_emitted_o counts ONLY mode-0 triangles", static_cast<uint64_t>(tri_tris),
        dut.terrain_triangles_emitted_o);
  // every rejected job was presented in VTX and REF, and every eighth in TRI
  long rej_presentations = 0;
  {
    // recount: the sweep rejected each rejected job once per presentation
    // (2, or 3 when it was also a TRI job). Rather than track it inline, use
    // the counter's own arithmetic: VTX + REF presentations = 2 x rejected,
    // TRI presentations of rejected jobs are counted by the loop below.
    long tri_rej = 0;
    long j = 0;
    for (int li = 0; li < 3; ++li)
      for (int oi = 0; oi < 3; ++oi)
        for (int level = 0; level <= zt::kMaxLevel; ++level)
          for (int nl = 0; nl < 256; ++nl)
            for (int mi = 0; mi < 6; ++mi)
              for (int surf = 0; surf < 2; ++surf) {
                ++j;
                if ((j & 7) != 0) continue;
                zt::SubpatchJob job;
                job.ox = origins[oi][0];
                job.oz = origins[oi][1];
                job.level = level;
                job.nlevel[0] = nl & 3;
                job.nlevel[1] = (nl >> 2) & 3;
                job.nlevel[2] = (nl >> 4) & 3;
                job.nlevel[3] = (nl >> 6) & 3;
                job.morph = morphs[mi];
                job.surface = surf ? zt::Surface::kUnderside : zt::Surface::kTop;
                if (zt::tessellate(lats[li], job, nullptr).verdict != zt::TessVerdict::kOk) ++tri_rej;
              }
    rej_presentations = 2 * rejected + tri_rej;
  }
  check(dut.subpatch_rejected_o == static_cast<uint32_t>(rej_presentations),
        "subpatch_rejected_o counts PRESENTATIONS (law 7)", static_cast<uint64_t>(rej_presentations),
        dut.subpatch_rejected_o);
  check(dut.mode_invalid_o == 0, "mode_invalid_o is zero after a sweep of legal modes", 0, dut.mode_invalid_o);
  check(dut.lod_clamped_o == 0, "no clamp in the sweep (morph <= 65536)", 0, dut.lod_clamped_o);

  // =========================================================================
  // 2. the comparators FIRE: a one-LSB fault in the expectation is seen
  // =========================================================================
  {
    const zt::ComposedLattice& lat = lats[0];
    zt::SubpatchJob job;
    job.ox = 8;
    job.oz = 16;
    job.level = 0;
    job.morph = 0x8000;
    Want want = oracle(lat, job);
    drv.set_mode(1);
    bool rej = false;
    (void)drv.run(lat, job, &rej);
    check(diff_verts(drv.verts, want, false, kSrc) == 0, "positive control baseline: VTX job matches", 0,
          static_cast<uint64_t>(diff_verts(drv.verts, want, false, kSrc)));
    want.verts[40].y += 1;
    check(diff_verts(drv.verts, want, false, kSrc) == 1,
          "POSITIVE CONTROL: the vertex comparator fires on a one-LSB fault in ONE vertex", 1,
          static_cast<uint64_t>(diff_verts(drv.verts, want, false, kSrc)));
    want.verts[40].y -= 1;
    const std::vector<EmitVert> verts = drv.verts;
    drv.set_mode(2);
    (void)drv.run(lat, job, &rej);
    check(diff_refs(drv.refs, want, false, kSrc) == 0, "positive control baseline: REF job matches", 0,
          static_cast<uint64_t>(diff_refs(drv.refs, want, false, kSrc)));
    std::vector<EmitRef> refs = drv.refs;
    refs[77].ib = static_cast<uint8_t>(refs[77].ib ^ 1);
    check(diff_refs(refs, want, false, kSrc) == 1,
          "POSITIVE CONTROL: the triple comparator fires on one flipped index", 1,
          static_cast<uint64_t>(diff_refs(refs, want, false, kSrc)));
    const int rb = diff_rebuild(verts, refs, want.tess.tris);
    check(rb >= 1, "POSITIVE CONTROL: the rebuild comparator fires on one flipped index", 1,
          static_cast<uint64_t>(rb));
  }

  // =========================================================================
  // 3. counters the sweep cannot reach: invalid mode, the clamp in ModeVtx
  // =========================================================================
  {
    const zt::ComposedLattice& lat = lats[0];
    zt::SubpatchJob job;
    job.ox = 8;
    job.oz = 8;
    job.level = 1;
    job.morph = 0x4000;
    const Want want = oracle(lat, job);
    drv.set_mode(3);
    bool rej = false;
    const std::vector<zt::MeshTri> got = drv.run(lat, job, &rej);
    check(dut.mode_invalid_o == 1, "SEEN TO FIRE: mode_invalid_o on job_mode_i == 3", 1, dut.mode_invalid_o);
    check(diff_tris(got, want.tess.tris) == 0, "an invalid mode runs as ModeTri (counted, never silent)", 0,
          static_cast<uint64_t>(diff_tris(got, want.tess.tris)));
    check(drv.verts.empty() && drv.refs.empty(), "an invalid mode emits on no other port", 1,
          (drv.verts.empty() && drv.refs.empty()) ? 1 : 0);

    // the morph clamp counts in ModeVtx exactly as in ModeTri, and the
    // vertices equal vertex_at at 65536
    zt::SubpatchJob over = job;
    over.morph = 0x1FFFF;
    const Want want_over = oracle(lat, over);  // oracle() clamps to 65536
    drv.set_mode(1);
    (void)drv.run(lat, over, &rej);
    check(dut.lod_clamped_o == 1, "lod_clamped_o fires in ModeVtx", 1, dut.lod_clamped_o);
    check(diff_verts(drv.verts, want_over, false, kSrc) == 0, "a clamped morph in ModeVtx == vertex_at at 65536",
          0, static_cast<uint64_t>(diff_verts(drv.verts, want_over, false, kSrc)));
  }

  // =========================================================================
  // 4. throughput and the scan — MEASURED, and the numbers the report carries
  // =========================================================================
  {
    const zt::ComposedLattice& lat = lats[0];  // dual, no void
    zt::SubpatchJob job;
    job.ox = 8;
    job.oz = 8;
    job.level = 0;
    bool rej = false;

    drv.set_mode(1);
    drv.reset_counters();
    (void)drv.run(lat, job, &rej);
    const uint64_t c_vtx0 = drv.cycles();
    std::printf("terrain_tess_modes_directed: VTX level 0, no morph: %llu cycles for %u vertices "
                "(%.2f vertices/clock; no cell-state scan, law 7)\n",
                static_cast<unsigned long long>(c_vtx0), static_cast<uint32_t>(drv.verts.size()),
                81.0 / static_cast<double>(c_vtx0));
    check(drv.verts.size() == 81 && c_vtx0 <= 81 + 12, "VTX unstitched level 0: one vertex per clock, no scan",
          81 + 12, c_vtx0);

    job.morph = 0x8000;
    drv.reset_counters();
    (void)drv.run(lat, job, &rej);
    const uint64_t c_vtx_m = drv.cycles();
    std::printf("terrain_tess_modes_directed: VTX level 0, morph 0.5: %llu cycles for 81 vertices "
                "(40 morphing x 3 reads + 41 x 1 = 161 reads)\n",
                static_cast<unsigned long long>(c_vtx_m));
    check(c_vtx_m <= 161 + 12, "VTX level 0 with morph: one lattice read per clock", 161 + 12, c_vtx_m);

    job.morph = 0;
    job.level = 1;
    drv.reset_counters();
    (void)drv.run(lat, job, &rej);
    std::printf("terrain_tess_modes_directed: VTX level 1, no morph: %llu cycles for 81 vertices "
                "(25 on the stride grid, 56 fillers)\n",
                static_cast<unsigned long long>(drv.cycles()));
    check(drv.cycles() <= 81 + 12, "VTX level 1: fillers cost one read each, no parent reads", 81 + 12,
          drv.cycles());

    // stitched: the scan runs (law 7), so the job is >= 65 cycles longer
    job.level = 0;
    job.nlevel[0] = 1;
    drv.reset_counters();
    (void)drv.run(lat, job, &rej);
    std::printf("terrain_tess_modes_directed: VTX level 0 STITCHED: %llu cycles (the 65-cycle scan runs "
                "for the reject decision)\n",
                static_cast<unsigned long long>(drv.cycles()));
    check(drv.cycles() >= 65 + 81, "VTX stitched on a dual page runs the cell-state scan (law 7)", 65 + 81,
          drv.cycles());
    check(drv.verts.size() == 81, "VTX stitched still emits all 81", 81, drv.verts.size());
    job.nlevel[0] = 0;

    drv.set_mode(2);
    drv.reset_counters();
    (void)drv.run(lat, job, &rej);
    const uint64_t c_ref = drv.cycles();
    std::printf("terrain_tess_modes_directed: REF level 0: %llu cycles for %u triples "
                "(65-cycle scan + one triple per clock + drain)\n",
                static_cast<unsigned long long>(c_ref), static_cast<uint32_t>(drv.refs.size()));
    check(drv.refs.size() == 128 && c_ref <= 128 + 65 + 12, "REF level 0: one triple per clock after the scan",
          128 + 65 + 12, c_ref);

    // and mode 0 on the same job is what the directed suite measures
    drv.set_mode(0);
    drv.reset_counters();
    const std::vector<zt::MeshTri> t = drv.run(lat, job, &rej);
    std::printf("terrain_tess_modes_directed: TRI level 0: %llu cycles for %u triangles (unchanged law)\n",
                static_cast<unsigned long long>(drv.cycles()), static_cast<uint32_t>(t.size()));
    check(drv.cycles() <= 3 * t.size() + 75, "TRI: the directed suite's own bound still holds",
          static_cast<uint64_t>(3 * t.size() + 75), drv.cycles());
    check(dut.idle_o == 1, "the block reports idle when drained", 1, dut.idle_o);
  }

  return zhao::report_and_exit("terrain_tess_modes_directed");
}
