// zref_terrain_tess.hpp — the TERRAIN.TESS oracle.
//
// PART VIEW, PART DEFINITION, and the file says which is which at every line,
// because getting that wrong is how an invention becomes "the law".
//
// WHAT IS A VIEW ONTO RATIFIED LAW (found, not chosen):
//   * The cell split. Cell (i, j) has corners i00=(i,j), i10=(i+1,j),
//     i01=(i,j+1), i11=(i+1,j+1) on the FIXED i00-i11 diagonal
//     (spec/terrain_rules.md §4.3), and the top surface emits
//     (i00, i11, i10) then (i00, i01, i11) — quoted from §4.3 itself, which
//     names `draw_heightfield`'s emit order as normative.
//   * The winding. reference/src/zrender/terrain.cpp writes it out:
//     "y-up winding: e1 x e2 = +Y for a flat cell — the flat-shade normal
//     points UP, so the island top lights up". TERRAIN.NORMALS already depends
//     on that sign, so this block MUST agree or the two disagree about which
//     way the island faces.
//   * The underside. Same §4.3 diagonal, INVERTED winding — the reference
//     comments that line "inverted relative to the top: normal points DOWN"
//     and labels it "TERRAIN.TESS law" in as many words. It is (i00, i10, i11)
//     then (i00, i11, i01): the top's pair with b and c swapped.
//   * Void cells emit no surface, and the test is guarded by `dual`:
//     `if (dual && lat.substance(i, j) != terrain::kSolid) continue;`. A legacy
//     single-surface page (§3.1 option (a)) has no void concept and no
//     underside at all.
//   * Scan order z-then-x, the cartridge patch order.
//   * The geomorph TARGET height, which is the §4.3 interpolation of the
//     next-coarser cell evaluated at the vertex — see `coarse_height` below,
//     where the reduction from column_query's exact-rational form to one
//     round-half-up halving is derived rather than assumed.
//
// WHAT IS CHOSEN HERE (no ratified law exists; each is argued, each names the
// alternative it beat, and each is repeated in design/contracts/TERRAIN.TESS.md
// and in the RTL header):
//   * The LEVEL SET. Charter §11.1 says a patch is 32x32 cells "divided into
//     sixteen 8x8-cell subpatches; each subpatch selects one of several
//     crack-safe grid resolutions" and never says which. A subpatch edge must
//     land on lattice vertices, so the stride must DIVIDE 8; the divisors of 8
//     are exactly {1, 2, 4, 8}, so the set is forced. What is chosen is only
//     the ENCODING: level L in 0..3, stride = 1 << L. Rejected alternative:
//     arbitrary run counts (non-divisors), which do not close on the lattice
//     and square the stitch case matrix — the same objection terrain_rules
//     §3.1(d) uses to reject column runs.
//   * The EDGE STRIDE RULE: `edge_stride[side] = 1 << max(own_level,
//     neighbour_level[side])`. It is symmetric by construction, so two
//     neighbours independently compute the SAME stride for the edge they
//     share, which is exactly terrain_rules §5's crack law and the contract's
//     "shared-edge vertex sets are identical". Rejected alternative: vertex
//     snapping (the finer side keeps its extra boundary vertices and slides
//     them onto the coarse segment). That leaves T-junctions, and a
//     fixed-point rasterizer cracks at a T-junction by a pixel — the 2026-08-15
//     seam-crack defect class recorded in design/contracts/GEOM.CLIP.md.
//   * The RING SCHEME that realises it (see `tessellate`). Rejected
//     alternative: per-side boundary strips, which is simpler until two
//     adjacent sides are both coarsened and the corner square needs two
//     vertices that neither side's coarse set contains.
//   * The VOID RULE AT STRIDE > 1: a run-cell is emitted iff EVERY one of the
//     stride x stride patch cells it covers is SOLID. Rejected alternative:
//     emit if any is solid, which roofs over a breach — and terrain_rules §3.5
//     says what you see through a breach is sky.
//   * REJECTING a coarsened subpatch that contains a void cell, rather than
//     guessing. See `TessVerdict::kRejectVoidStitch`.
//   * GEOMORPH APPLIES ONLY TO VERTICES STRICTLY INSIDE THE SUBPATCH. See
//     `morph_applies` for the crack argument and for the CONTRACT GAP this
//     works around.
//
// Law, in citation order:
//   spec/terrain_rules.md §4.3 (triangulation, the fixed diagonal, the
//     interpolation form), §3.2/§3.5 (void columns), §5 (the crack law and the
//     inverted-winding underside), §4.1 (one evaluation, every consumer)
//   spec/qformats.md §3 (fx_mul is ONE rescale(.,16)), §4 (round-half-up)
//   charter §11.1 (16 subpatches of 8x8 cells, crack-safe resolutions,
//     precomputed border stitch patterns, geomorph between levels)
//   reference/src/zrender/terrain.cpp `draw_heightfield` — the emit order and
//     both windings, which this file reproduces exactly on the unstitched path
//   design/contracts/TERRAIN.TESS.md

#pragma once

#include <cstdint>
#include <vector>

#include "zref/zref_fixp.hpp"
#include "zref/zref_terrain.hpp"

namespace zref {
namespace terrain {

// ---- the level set (charter §11.1; the stride must divide 8) ---------------

inline constexpr int kSubpatchCells = 8;  // charter §11.1: 8x8-cell subpatches
inline constexpr int kMaxLevel = 3;       // stride 1, 2, 4, 8 — the divisors of 8

/** Side order, terrain_rules §6.6's rim order: -z, +z, -x, +x. */
enum Side : int { kSideNegZ = 0, kSidePosZ = 1, kSideNegX = 2, kSidePosX = 3 };

enum class Surface : uint8_t { kTop = 0, kUnderside = 1 };

/** One triangle of `terrain_mesh`, fx16 world units — TERRAIN.NORMALS' input. */
struct MeshTri {
  int32_t ax = 0, ay = 0, az = 0;
  int32_t bx = 0, by = 0, bz = 0;
  int32_t cx = 0, cy = 0, cz = 0;
};

/** One subpatch work item: `patch_state` + `lod_target`, as the block sees it. */
struct SubpatchJob {
  int ox = 0;  // subpatch cell origin, a multiple of 8, 0..24
  int oz = 0;
  int level = 0;                 // own level, 0..3; stride = 1 << level
  int nlevel[4] = {0, 0, 0, 0};  // neighbour levels, indexed by Side
  int32_t morph = 0;             // geomorph factor, Q16, 0..65536
  Surface surface = Surface::kTop;
};

enum class TessVerdict : uint8_t {
  kOk = 0,
  /**
   * A subpatch that is BOTH coarsened by a neighbour AND contains a void cell.
   *
   * CHOSEN, and loud on purpose. The ring scheme's fans are not aligned to
   * run-cells, so honouring a void inside one would mean a conservative
   * bounding-box scan per fan — up to 64 cell-state reads for a fan that emits
   * two triangles — to buy geometry no LOD selector should ever ask for: a
   * breached subpatch is exactly the subpatch a projected-error selector keeps
   * FINE (terrain_rules §4.4, "we recompute exactly where the ground moved").
   * So the block REJECTS the job and counts it, in the spirit of charter
   * §11.4's reject-never-silently-drop, instead of roofing over a hole
   * (terrain_rules §3.5 says a breach shows sky) or guessing.
   *
   * The obligation this creates is stated rather than hidden: TERRAIN.LOD must
   * not coarsen a subpatch's neighbour past a subpatch that carries void cells.
   */
  kRejectVoidStitch = 1,
};

struct TessResult {
  std::vector<MeshTri> tris;
  TessVerdict verdict = TessVerdict::kOk;
  uint32_t lod_clamped = 0;  // morph factors clamped into [0, 65536]
};

// ---- the geomorph target height -------------------------------------------

/**
 * `coarse_height` — the height the NEXT COARSER level would give at a lattice
 * vertex that the coarser level does not carry.
 *
 * DERIVED FROM §4.3, NOT INVENTED. The vertex sits at u = 1/2 (and/or v = 1/2)
 * inside the coarse cell, so `column_query`'s two-MAD form collapses:
 *
 *   x-midpoint (v = 0):     triangle A -> h00 + fx_mul(1/2, h10 - h00)
 *   z-midpoint (u = 0):     triangle B -> h00 + fx_mul(1/2, h01 - h00)
 *   diagonal   (u = v = 1/2, ties to A):
 *        h00 + fx_mul(1/2, h10-h00) + fx_mul(1/2, h11-h10)
 *      = h00 + div_rhu((h10-h00)*un*vd + (h11-h10)*vn*ud, ud*vd)
 *      = h00 + div_rhu((h11 - h00) * ud*vd/2, ud*vd)
 *      = h00 + div_rhu(h11 - h00, 2)
 *
 * — one common denominator, ONE round-half-up, exactly as column_query does it.
 * All three are `ha + div_rhu(hb - ha, 2)`, and `div_rhu(n, 2)` is
 * `rescale_s32(n, 1)`. `tests/terrain/terrain_tess_directed` proves the
 * reduction by evaluating `zref::terrain::column_query` on the coarse cell and
 * requiring it to agree.
 *
 * The reduction assumes un/ud = vn/vd = 1/2 EXACTLY, i.e. a uniform placed
 * pitch. That is not an assumption about content: terrain_rules §2.1 makes the
 * packer ASSERT that a patch envelope equals `origin + coords x 32 x pitch`
 * exactly, and §4.3 is itself written in the shift form ("cx = (wx - env_x0) >>
 * pitch_log2 — shift, no division"). The reference's exact-rational form is the
 * more general one, and its own header records that it "REDUCE[s] to the spec's
 * shift form when the envelope is pitch-aligned".
 */
inline int32_t coarse_height(int32_t ha, int32_t hb, SatLedger* L = nullptr) {
  return fx_add(fx16{ha}, fx16{rescale_s32(static_cast<int64_t>(hb) - ha, 1, L)}, L).raw;
}

/**
 * `morph_height` — blend a vertex's own-level height toward the coarse-level
 * height, ONE rounding (qformats §3), endpoints EXACT.
 *
 * The shape `h + fx_mul(m, hc - h)` is §4.3's own shape: an exact add of a
 * rounded delta, never a rounded sum of two rounded terms. m = 0 gives h and
 * m = 65536 gives hc bit-exactly, which is what "factor 0/1 = exact levels" in
 * design/contracts/TERRAIN.TESS.md means.
 */
inline int32_t morph_height(int32_t h, int32_t hc, int32_t morph, SatLedger* L = nullptr) {
  const int64_t d = static_cast<int64_t>(hc) - h;
  return fx_add(fx16{h}, fx16{rescale_s32(static_cast<int64_t>(morph) * d, 16, L)}, L).raw;
}

/**
 * Which morph case a vertex is in, given the subpatch and its own stride.
 * 0 = none (the coarse level already carries this vertex), 1 = x-midpoint,
 * 2 = z-midpoint, 3 = the coarse cell's diagonal midpoint.
 *
 * GEOMORPH APPLIES ONLY STRICTLY INSIDE THE SUBPATCH — CHOSEN, and the reason
 * is a real gap in `lod_target`. A vertex on a subpatch boundary is shared with
 * a neighbour that has its OWN morph factor; moving it needs both sides to
 * agree, and design/contracts/TERRAIN.TESS.md's `lod_target` packet carries one
 * factor and four neighbour LEVELS — it cannot express the neighbour's factor
 * at all. Leaving boundary vertices unmorphed makes crack-safety hold
 * unconditionally, for every level pair and every factor pair, with no
 * cross-subpatch agreement of any kind.
 *
 * The cost is stated, not buried: during a transition the subpatch interior
 * moves while its border does not, so the border reads as a shallow crease
 * bounded by the level's own height deviation. REJECTED ALTERNATIVE: morphing
 * boundary vertices too, which is the textbook form and is strictly better
 * looking — and which cannot be implemented correctly until `lod_target` gains
 * a per-edge morph factor. That is a contract amendment, not an RTL decision.
 */
inline int morph_case(const SubpatchJob& job, int vi, int vj) {
  if (job.morph == 0) return 0;
  const int s = 1 << job.level;
  if (vi <= job.ox || vi >= job.ox + kSubpatchCells) return 0;
  if (vj <= job.oz || vj >= job.oz + kSubpatchCells) return 0;
  const int sc = s << 1;  // the next coarser level's stride
  const bool xc = (vi & (sc - 1)) == 0;
  const bool zc = (vj & (sc - 1)) == 0;
  if (xc && zc) return 0;  // the coarse level already carries this vertex
  if (zc) return 1;        // on a coarse row: an x-midpoint
  if (xc) return 2;        // on a coarse column: a z-midpoint
  return 3;                // the coarse cell's diagonal midpoint
}

// ---- the LOD deviation, BOTH readings of it --------------------------------
//
// `zref::terrain::LodSubpatch::dev[L]` is "the largest |fine - coarse| height
// deviation this subpatch would suffer at level L", and until 2026-09-07
// NOTHING IN THE REPOSITORY COMPUTED IT -- not the RTL (the only driver of
// `sp_dev1_i` in the tree is an LFSR in `zhao_prod_top.sv`) and not this
// library (every test wrote the three numbers by hand). The quantity had one
// sentence and no implementation.
//
// THE SENTENCE HAS TWO READINGS AND THIS FUNCTION IMPLEMENTS BOTH, because
// choosing between them is an owner ruling and inventing it here would be the
// fault this tree keeps recording. `morph_case` returns 0 on subpatch BOUNDARY
// vertices -- geomorph applies only strictly inside, chosen for unconditional
// crack-safety -- so the two differ exactly on the boundary ring:
//
//   include_boundary = false   the MORPH deviation: how far the interior moves
//                              during a transition. Boundary vertices never
//                              move, so they never deviate.
//   include_boundary = true    the MESH deviation: how far the coarse MESH
//                              departs from the fine one. The coarse mesh drops
//                              those vertices and interpolates across them, so
//                              they deviate like any other. Always the larger
//                              number, and the one a projected-error selector
//                              wants if the question is "will the player see
//                              it".
//
// It is a PARAMETER rather than a default so that both are measurable on a real
// page before anyone has to choose. See
// reports/TERRAIN-LOD-DEVIATION-20260907.md.
//
// NO NEW ARITHMETIC. The coarse height at a midpoint is `coarse_height` of the
// relevant pair and which pair it is comes from `morph_case` -- both ratified,
// both already used by the tessellator, so this is a walk rather than a law.
inline uint32_t lod_deviation(const ComposedLattice& lat, Surface surf, int ox, int oz,
                              int level, bool include_boundary, SatLedger* L = nullptr) {
  if (level <= 0) return 0u;   // dev[0] is zero by definition
  // `detail::plane_at` is declared further down this file, so the same one-line
  // access is written here rather than reordering a header around a helper.
  // It is the same expression: `vj * w + vi`, the tree's lattice addressing.
  const auto plane = [&](int pi, int pj) -> int32_t {
    const std::size_t k =
        static_cast<std::size_t>(pj) * static_cast<std::size_t>(lat.w) +
        static_cast<std::size_t>(pi);
    return surf == Surface::kUnderside ? lat.bottom[k] : lat.top[k];
  };
  const int s = 1 << level;
  const int sc = s << 1;
  uint32_t worst = 0u;

  // The whole subpatch INCLUDING its border ring, so the boundary reading has
  // something to include; the other reading skips them the way `morph_case`
  // does.
  for (int vj = oz; vj <= oz + kSubpatchCells; ++vj) {
    for (int vi = ox; vi <= ox + kSubpatchCells; ++vi) {
      if (vi < 0 || vj < 0 || vi >= lat.w || vj >= lat.h) continue;
      const bool on_border = (vi == ox) || (vi == ox + kSubpatchCells) || (vj == oz) ||
                             (vj == oz + kSubpatchCells);
      if (on_border && !include_boundary) continue;

      const bool xc = (vi & (sc - 1)) == 0;
      const bool zc = (vj & (sc - 1)) == 0;
      if (xc && zc) continue;   // the coarse level already carries this vertex

      const int32_t h = plane(vi, vj);
      int32_t hc = h;
      if (zc) {                 // an x-midpoint
        if (vi - s < 0 || vi + s >= lat.w) continue;
        hc = coarse_height(plane(vi - s, vj),
                           plane(vi + s, vj), L);
      } else if (xc) {          // a z-midpoint
        if (vj - s < 0 || vj + s >= lat.h) continue;
        hc = coarse_height(plane(vi, vj - s),
                           plane(vi, vj + s), L);
      } else {                  // the coarse cell's diagonal midpoint
        if (vi - s < 0 || vi + s >= lat.w || vj - s < 0 || vj + s >= lat.h) continue;
        hc = coarse_height(plane(vi - s, vj - s),
                           plane(vi + s, vj + s), L);
      }

      const int64_t d = static_cast<int64_t>(h) - hc;
      const uint64_t mag = static_cast<uint64_t>(d < 0 ? -d : d);
      // The port is 24 bits (`sp_dev1_i`), so a deviation wider than that
      // SATURATES rather than wrapping -- a wrapped deviation would read as
      // small and pick a level that is far too coarse.
      const uint32_t clipped = mag > 0xFFFFFFull ? 0xFFFFFFu : static_cast<uint32_t>(mag);
      if (clipped > worst) worst = clipped;
    }
  }
  return worst;
}

// ---- the tessellation ------------------------------------------------------

namespace detail {

/** The height plane the job selected, at one lattice vertex. */
inline int32_t plane_at(const ComposedLattice& lat, Surface surf, int vi, int vj) {
  const size_t k = static_cast<size_t>(vj) * static_cast<size_t>(lat.w) + static_cast<size_t>(vi);
  return surf == Surface::kUnderside ? lat.bottom[k] : lat.top[k];
}

/** One emitted vertex: lattice position, geomorphed height. */
struct TessVert {
  int32_t x = 0, y = 0, z = 0;
};

inline TessVert vertex_at(const ComposedLattice& lat, const SubpatchJob& job, int vi, int vj,
                          int32_t morph, SatLedger* L) {
  TessVert v;
  v.x = lat.wx[static_cast<size_t>(vi)];
  v.z = lat.wz[static_cast<size_t>(vj)];
  const int32_t h = plane_at(lat, job.surface, vi, vj);
  const int s = 1 << job.level;
  int ai = vi, aj = vj, bi = vi, bj = vj;
  switch (morph_case(job, vi, vj)) {
    case 1:
      ai = vi - s;
      bi = vi + s;
      break;
    case 2:
      aj = vj - s;
      bj = vj + s;
      break;
    case 3:
      ai = vi - s;
      aj = vj - s;
      bi = vi + s;
      bj = vj + s;
      break;
    default:
      v.y = h;
      return v;
  }
  const int32_t hc =
      coarse_height(plane_at(lat, job.surface, ai, aj), plane_at(lat, job.surface, bi, bj), L);
  v.y = morph_height(h, hc, morph, L);
  return v;
}

}  // namespace detail

/**
 * `tessellate` — one subpatch of one surface into `terrain_mesh` triangles.
 *
 * THE UNSTITCHED PATH IS THE REFERENCE PATH. When no neighbour is coarser
 * (every `edge_stride` equals the own stride) the block walks run-cells in
 * z-then-x scan order and emits the §4.3 pair per cell — so at level 0 over a
 * whole patch the triangle stream is exactly `draw_heightfield`'s cell emission,
 * vertex for vertex. That is the property that makes "RTL == oracle" mean "RTL
 * == the geometry the golden captures already pin", and it is asserted directly
 * by `terrain_tess_directed`.
 *
 * THE STITCHED PATH (any neighbour coarser) is an ANNULUS. The subpatch splits
 * into an inner (n-2) x (n-2) block of run-cells, emitted plain, and a ring one
 * run-cell deep between the subpatch boundary and that block. The ring is
 * triangulated by walking the OUTER boundary — whose vertices are ONLY the
 * per-side coarse ones, which is the entire point — clockwise from the subpatch
 * corner nearest the origin, and fanning each outer segment to the stretch of
 * the inner rectangle's boundary it projects onto.
 *
 * There is no corner case, and that is why this shape was chosen: a subpatch
 * corner is a legal vertex on BOTH its sides' coarse sets (index 0 of each), so
 * a fan anchored at it never needs a vertex the neighbour does not have. The
 * per-side-strip alternative fails exactly there — when two adjacent sides are
 * both coarsened, the corner square's other two corners lie on the two boundary
 * lines and neither is in its side's coarse set.
 *
 * `n == 2` (level 2 next to a level-3 neighbour) degenerates cleanly: the inner
 * rectangle collapses to the single centre vertex and the ring becomes a fan
 * from it. `n == 1` (level 3) can never be stitched at all, because 8 is the
 * coarsest stride, so the plain path always applies.
 */
inline TessResult tessellate(const ComposedLattice& lat, const SubpatchJob& job_in,
                             SatLedger* L = nullptr) {
  TessResult out;
  SubpatchJob job = job_in;
  if (job.morph < 0) {
    job.morph = 0;
    ++out.lod_clamped;
  } else if (job.morph > 65536) {
    job.morph = 65536;
    ++out.lod_clamped;
  }

  const int ox = job.ox;
  const int oz = job.oz;
  const int s = 1 << job.level;
  const int n = kSubpatchCells / s;  // run-cells per side
  const bool underside = job.surface == Surface::kUnderside;

  // A legacy single-surface page has no underside at all (terrain_rules §3.1
  // option (a); the reference's `if (!dual) continue;` for the kind-1 prim).
  if (underside && !lat.dual) return out;

  // edge strides: symmetric by construction, so both sides of a shared edge
  // compute the same one (the crack law, terrain_rules §5)
  int es[4];
  bool stitched = false;
  for (int k = 0; k < 4; ++k) {
    const int lv = job.nlevel[k] > job.level ? job.nlevel[k] : job.level;
    es[k] = 1 << lv;
    if (es[k] != s) stitched = true;
  }

  // The 8x8 solidity of the subpatch, read ONCE. `substance` returns SOLID for
  // an empty cell-state plane, and the void test only applies to a dual page.
  bool solid[kSubpatchCells][kSubpatchCells];
  bool any_void = false;
  for (int cj = 0; cj < kSubpatchCells; ++cj) {
    for (int ci = 0; ci < kSubpatchCells; ++ci) {
      const bool sol = !lat.dual || lat.substance(ox + ci, oz + cj) == kSolid;
      solid[cj][ci] = sol;
      if (!sol) any_void = true;
    }
  }
  if (stitched && any_void) {
    out.verdict = TessVerdict::kRejectVoidStitch;
    return out;
  }

  const auto vert = [&](int vi, int vj) {
    return detail::vertex_at(lat, job, vi, vj, job.morph, L);
  };
  const auto emit = [&](int i0, int j0, int i1, int j1, int i2, int j2) {
    const detail::TessVert a = vert(i0, j0);
    // The underside is the top's pair with b and c swapped — the ONE place the
    // inverted winding lives (terrain_rules §5, and the reference's own note).
    const detail::TessVert b = vert(underside ? i2 : i1, underside ? j2 : j1);
    const detail::TessVert c = vert(underside ? i1 : i2, underside ? j1 : j2);
    out.tris.push_back(MeshTri{a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z});
  };
  // The §4.3 pair for one run-cell, in `draw_heightfield`'s order.
  const auto emit_cell = [&](int i0, int j0) {
    emit(i0, j0, i0 + s, j0 + s, i0 + s, j0);  // (i00, i11, i10)
    emit(i0, j0, i0, j0 + s, i0 + s, j0 + s);  // (i00, i01, i11)
  };
  // A run-cell is emitted iff EVERY patch cell it covers is SOLID.
  const auto run_solid = [&](int a, int b) {
    for (int dj = 0; dj < s; ++dj)
      for (int di = 0; di < s; ++di)
        if (!solid[b * s + dj][a * s + di]) return false;
    return true;
  };

  if (!stitched) {
    for (int b = 0; b < n; ++b)
      for (int a = 0; a < n; ++a)
        if (run_solid(a, b)) emit_cell(ox + a * s, oz + b * s);
    return out;
  }

  // ---- the annulus ---------------------------------------------------------
  const int u = n - 2;              // inner-rectangle vertices per side, minus 1
  const int P = u > 0 ? 4 * u : 1;  // inner ring perimeter in vertices
  const int x_lo = ox + s, x_hi = ox + kSubpatchCells - s;
  const int z_lo = oz + s, z_hi = oz + kSubpatchCells - s;

  for (int b = 1; b + 1 < n; ++b)
    for (int a = 1; a + 1 < n; ++a) emit_cell(ox + a * s, oz + b * s);

  // W[t]: the inner ring, clockwise from the corner nearest the origin.
  const auto inner = [&](int t, int* vi, int* vj) {
    if (u == 0) {
      *vi = x_lo;
      *vj = z_lo;
      return;
    }
    t = ((t % P) + P) % P;
    if (t < u) {
      *vi = x_lo;
      *vj = z_lo + t * s;
    } else if (t < 2 * u) {
      *vi = x_lo + (t - u) * s;
      *vj = z_hi;
    } else if (t < 3 * u) {
      *vi = x_hi;
      *vj = z_hi - (t - 2 * u) * s;
    } else {
      *vi = x_hi - (t - 3 * u) * s;
      *vj = z_lo;
    }
  };

  // The outer ring, clockwise from the same corner: -x up, +z across, +x down,
  // -z back. `m[k]` segments on side k.
  const int order[4] = {kSideNegX, kSidePosZ, kSidePosX, kSideNegZ};
  int m[4];
  for (int k = 0; k < 4; ++k) m[k] = kSubpatchCells / es[order[k]];

  const auto outer = [&](int k, int g, int* vi, int* vj) {
    const int step = es[order[k]] * g;
    switch (k) {
      case 0:  // -x, going +z
        *vi = ox;
        *vj = oz + step;
        break;
      case 1:  // +z, going +x
        *vi = ox + step;
        *vj = oz + kSubpatchCells;
        break;
      case 2:  // +x, going -z
        *vi = ox + kSubpatchCells;
        *vj = oz + kSubpatchCells - step;
        break;
      default:  // -z, going -x
        *vi = ox + kSubpatchCells - step;
        *vj = oz;
        break;
    }
  };
  // Project an outer vertex onto the inner ring: the same position clamped into
  // the inner rectangle. Exact, because every stride divides 8.
  const auto proj = [&](int k, int g) {
    if (u == 0) return 0;
    const int a = es[order[k]] * g / s;
    int c = a - 1;
    if (c < 0) c = 0;
    if (c > u) c = u;
    return (k * u + c) % P;
  };

  for (int k = 0; k < 4; ++k) {
    for (int g = 0; g < m[k]; ++g) {
      const int nk = (g + 1 < m[k]) ? k : (k + 1) % 4;
      const int ng = (g + 1 < m[k]) ? g + 1 : 0;
      int v0i, v0j, v1i, v1j;
      outer(k, g, &v0i, &v0j);
      outer(nk, ng, &v1i, &v1j);
      const int t0 = proj(k, g);
      const int t1 = proj(nk, ng);
      const int steps = u == 0 ? 0 : ((t1 - t0) % P + P) % P;
      int wi, wj;
      inner(t1, &wi, &wj);
      // the segment's own triangle, closing the fan onto the next outer vertex
      emit(v0i, v0j, v1i, v1j, wi, wj);
      // then the fan across the inner stretch it spans
      for (int r = 0; r < steps; ++r) {
        int ai, aj, bi, bj;
        inner(t0 + r + 1, &ai, &aj);
        inner(t0 + r, &bi, &bj);
        emit(v0i, v0j, ai, aj, bi, bj);
      }
    }
  }
  return out;
}

}  // namespace terrain
}  // namespace zref
