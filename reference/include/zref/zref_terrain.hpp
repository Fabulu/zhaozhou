// zref_terrain.hpp — dual-heightfield island terrain reference core
// (world-identity wave, spec/terrain_rules.md).
//
// This header is the ONE place the composed-lattice law (§4) lives on the
// reference side: the renderer, the sim column query, particle collision and
// every test interpolate the SAME composed lattice arrays on the SAME §4.3
// triangulation (fixed i00–i11 diagonal, ties to triangle A). Nothing may
// evaluate terrain height any other way — that is the anti-drift law the
// donor's Erupt (sim 2.0 vs shader 3.0, S1 §4 / S5 §1.5) exists to enforce.
//
// Law (in citation order):
//   spec/terrain_rules.md §2 (patch layers A/B/C/D), §3.2 (column law),
//     §3.3 (cell state byte), §3.4 (height composition + breach law),
//     §3.7 (keel depth default), §4.2 (composed lattice), §4.3
//     (triangulation + point query), §5 (rim geometry + the frozen degrade
//     order — zref::forge), §6.2 (Mosaic pattern + mirrored fold),
//     §9 (incremental stamp scaling — applyDMapDelta heir)
//   spec/qformats.md §2/§9 (height16 <-> fx16, exact raw << 8), §3
//     (saturating fx16 ops), §4 (round-half-up division)
//   design/contracts/TERRAIN.BAKE.md (breach evaluation, no_bake clamp:
//     composed top stays >= bottom + one height16 LSB on protected cells)
//   charter §29-6 (never implement semantics twice), §29-7 (no host floats
//     in deterministic paths — everything below is integer-only)
//
// Phase-3 reading (recorded): the reference TerrainPatch is the envelope-
// based Phase-3 resource (cartridge kind-4 shape + the Island Patch v1
// layers B/C/D carried in-memory); the kind-6 page byte layout, pitch_log2
// shift addressing and the sparse island directory are Phase-6 loader work.
// Cell fractions here are exact rationals from the placed lattice, which
// REDUCE to the spec's shift form when the envelope is pitch-aligned (the
// canonical 2 m pitch demo/test envelopes are).

#pragma once

#include "zref/zref_fixp.hpp"
#include "zref/zref_sat.hpp"

#include <cstdint>
#include <vector>

namespace zref {
namespace render {
struct TerrainPatch;  // zref/zref_render.hpp
}

namespace terrain {

// ---- cell state byte (terrain_rules.md §3.3, layer D) ----------------------

inline constexpr uint8_t kSolid = 0;         // one interval [bottom, top]
inline constexpr uint8_t kVoidAuthored = 1;  // never ground
inline constexpr uint8_t kVoidBreached = 2;  // destroyed at runtime (bake)
inline constexpr uint8_t kSubstanceMask = 0x03;
inline constexpr uint8_t kNoBakeBit = 0x04;  // bit 2: bakes clamp, never breach

// ---- shared lattice interpolation helper -----------------------------------

// a + (b-a)*num/den with ONE rounding (qformats §4 family; den > 0). The
// SINGLE lattice-coordinate lerp used by the renderer grid build, the bake
// stencil placement and the tests — one implementation, per charter §29-6.
inline int32_t lattice_lerp(int32_t a, int32_t b, int32_t num, int32_t den) {
  const int64_t span = static_cast<int64_t>(b) - a;
  const int64_t v = a + (span * num + den / 2) / den;
  return static_cast<int32_t>(v);
}

// ---- the composed lattice (terrain_rules.md §4.2) --------------------------

/**
 * The per-frame composed lattice of one patch: placed vertex coordinates,
 * live_top per §3.4 (base + scar, clamped at bottom, plus live field lanes,
 * clamped at bottom), the bottom surface, and the cell-state plane. Built
 * ONCE (zref::render::compose_lattice) and consumed by the tessellating
 * renderer AND column_query below — never rebuilt by a consumer.
 *
 * dual == false is the Phase-3 legacy single-surface page (kind 4): every
 * cell SOLID, no modelled underside, no rim emission (terrain_rules §3.1
 * option (a) kept as the degenerate case).
 */
struct ComposedLattice {
  int w = 0, h = 0;  // lattice vertices (w x h), cells (w-1) x (h-1)
  bool dual = false;
  std::vector<int32_t> wx;          // placed x per lattice column (fx16 raw)
  std::vector<int32_t> wz;          // placed z per lattice row (fx16 raw)
  std::vector<int32_t> top;         // live_top (fx16 raw), w*h, z-then-x
  std::vector<int32_t> bottom;      // bottom surface (fx16 raw), dual only
  std::vector<uint8_t> cell_state;  // layer D ((w-1)*(h-1)); empty = SOLID

  uint8_t substance(int ci, int cj) const {
    if (cell_state.empty()) return kSolid;
    return static_cast<uint8_t>(
        cell_state[static_cast<size_t>(cj) * (w - 1) + static_cast<size_t>(ci)] & kSubstanceMask);
  }
};

// ---- the column query (terrain_rules.md §4.3) ------------------------------

enum class ColumnClass : uint8_t {
  kOut = 0,    // outside the lattice envelope (absent patch = open sky)
  kVoid = 1,   // authored or breached void column
  kSolid = 2,  // one solid interval [bottom, top]
};

struct ColumnResult {
  ColumnClass cls = ColumnClass::kOut;
  fx16 top{0};     // interpolated live_top (valid for kSolid)
  fx16 bottom{0};  // interpolated bottom (== top for a legacy lattice)
};

/**
 * `zref::terrain::column_query` — THE reference point query (terrain_rules
 * §4.3; the symbol SW.CPUCOLL / PART.COLLIDE / TERRAIN.TESS cite). Locates
 * the cell, picks triangle A (u >= v, ties to A) or B on the fixed i00–i11
 * diagonal, and interpolates top and bottom with ONE round-half-up rounding
 * each — the identical corner set and diagonal the renderer tessellates
 * (physics equals pixels, tests/terrain/).
 *
 * The lattice must be axis-aligned monotone (identity/axis placement —
 * island-datum space, the space §4.3 is written in).
 */
ColumnResult column_query(const ComposedLattice& lat, fx16 wx, fx16 wz);

// ---- TERRAIN.BAKE reference (terrain_rules.md §3.4 + §9) -------------------

/**
 * Radial dig stencil for the bake reference: a paraboloid bowl,
 * stencil(d) = (1 - d^2/R^2) in Q16 for d < R, else 0. Deterministic
 * integer evaluation at lattice vertices; centre/radius in island-datum
 * fx16 metres. (The donor's 33x33 volc.DATA ubyte stencil is the asset-
 * shaped ancestor; a byte-stencil bake lands with the asset lane.)
 */
struct DigStamp {
  int32_t cx = 0, cz = 0;  // fx16 raw, island-datum space
  int32_t radius = 0;      // fx16 raw, > 0
};

/**
 * Incremental stamp bake (the applyDMapDelta heir, terrain_rules §9):
 * writes layer B so the vertex carries scar contribution g(to) - g(from)
 * where g(depth) = bake_back(depth * stencil) — a pure function of the
 * ABSOLUTE depth, so a stepped ramp telescopes bit-exactly to the one-shot
 * bake (deferral identity, §9.2 law 3; asserted by tests/terrain/).
 * Positive depth digs DOWN (scar goes negative). Applies the TERRAIN.BAKE
 * no_bake clamp: on vertices touching a no_bake cell, scar is clamped so
 * base + scar >= bottom + 1 height16 LSB.
 *
 * The caller runs apply_breach_law afterwards — bake writes B, the breach
 * law writes D; the split mirrors the contract's two planes.
 */
void bake_dig(render::TerrainPatch& patch, const DigStamp& st, fx16 depth_from, fx16 depth_to,
              SatLedger* L);

struct BreachEvent {
  uint16_t ci = 0, cj = 0;  // cell coords
  uint8_t state = 0;        // new substance (kVoidBreached or kSolid heal)
};

/**
 * The breach law (terrain_rules §3.4, evaluated only at bake time): a SOLID
 * cell with no_bake = 0 becomes VOID_BREACHED iff compose_top == bottom
 * (exact equality after the clamp, i.e. base + scar <= bottom in height16)
 * at ALL FOUR corner vertices; a VOID_BREACHED cell with any corner strictly
 * above heals to SOLID. VOID_AUTHORED never becomes ground. Returns the
 * transitions in cell scan order (z-then-x) — deterministic, replay-exact.
 */
std::vector<BreachEvent> apply_breach_law(render::TerrainPatch& patch);

// ---- keel default (terrain_rules.md §3.7, frozen 2026-08-16) ----------------

inline constexpr int32_t kKeelFloorM = 50;  // the donor's fixed mapDepth (sacmap.d:106)

struct KeelProfile {
  int32_t radius_m = 0;   // R: max SOLID cell-centre distance, floored
  int32_t peak_m = 0;     // authored top peak
  int32_t depth_m = 0;    // KEEL_DEPTH = min(max(50, R/2), 126 - max(0, peak))
  int32_t depth_raw = 0;  // the same on the height16 grid
};

/** Measure R and the top peak, derive KEEL_DEPTH per §3.7 (pure function). */
KeelProfile keel_profile(const render::TerrainPatch& patch, int32_t heart_x, int32_t heart_z);

/**
 * Write layer C (the bottom surface) from the authored base heights using
 * the §3.7 bitten-apple profile: thickness(v) = KEEL_DEPTH x (0.4 + 0.6 x
 * (1 - (d/R)^2)), one rounding, bottom = base - thickness on the height16
 * grid. Requires cell_state (the SOLID mask defines R); returns false and
 * writes nothing without it. shallow_override_raw (height16, may be 0) is
 * the DELIBERATE authoring choice §3.7 permits: when set and smaller than
 * the lawful depth it replaces the depth (a recorded slab, never a default).
 */
bool generate_bottom(render::TerrainPatch& patch, int32_t heart_x, int32_t heart_z,
                     int32_t shallow_override_raw = 0);

// ---- TEXTURE.MOSAIC reference (terrain_rules.md §6.2, frozen 2026-08-16) ----

/**
 * The mirrored-repeat texel fold (§6.2): u is Q16.16 TILE units; returns the
 * 0..63 texel index. m = floor(u*64) (arithmetic shift), per = floored
 * m mod 128, texel = per < 64 ? per : 127 - per. Adjacent same-texture
 * cells mirror into each other — the donor's seam-free trick in integer law.
 */
inline int32_t mirror_texel(int32_t u_raw) {
  const int32_t m = u_raw >> 10;
  int32_t per = m % 128;
  if (per < 0) per += 128;
  return per < 64 ? per : 127 - per;
}

/**
 * The stable world-space pick (§6.2): tx/ty are the UNFOLDED world texel
 * indices; h = (tx*73856093) ^ (ty*19349663), p = h mod 255, and the winner
 * is matA iff p < weight (0 -> always B, 255 -> always A, 128 dithers
 * ~50/255 — the zero-blend transition). The constants are frozen: changing
 * one changes every capture's pixels.
 */
inline uint8_t mosaic_pick(uint8_t mat_a, uint8_t mat_b, uint8_t weight, int32_t tx, int32_t ty) {
  const uint32_t h = (static_cast<uint32_t>(tx) * 73856093u) ^
                     (static_cast<uint32_t>(ty) * 19349663u);
  return (h % 255u) < static_cast<uint32_t>(weight) ? mat_a : mat_b;
}

}  // namespace terrain

// ---- FORGE.CLIFF reference (terrain_rules.md §5, frozen degrade order) ------

namespace forge {

/** Per-page rim emission budget (terrain_rules §5, provisional 512). */
inline constexpr uint32_t kRimBudgetPerPage = 512;

struct RimEdge {
  uint16_t ci = 0, cj = 0;  // the SOLID cell owning the wall
  uint8_t side = 0;         // 0 = -z, 1 = +z, 2 = -x, 3 = +x
  uint16_t span = 1;        // merged contiguous-collinear continuation
};

struct RimPlan {
  std::vector<RimEdge> edges;  // scan order (z-then-x, side 0..3), per page
  uint32_t merged = 0;         // edges absorbed into spans (degrade step 1)
  uint32_t dropped = 0;        // edges beyond budget after merge (the counter)
};

/**
 * Enumerate the rim edges of a composed lattice (§5): one edge per SOLID
 * cell side facing a void/OUT neighbour, budgeted per 32x32-cell PAGE block
 * (the Phase-3 envelope patch carries many pages; the hardware budget is
 * per Island-Patch page). Degrade order per page (§5, frozen): (1) merge
 * contiguous collinear edges (same side, same lattice line, sharing a
 * vertex — a merge never bridges a notch) longest-run-first until inside
 * budget; (2) still over: keep the edges whose endpoint vdist is GREATEST
 * (vdist = per-vertex nearness, Q16.16 1/w from the renderer; null = keep
 * scan order), ties by scan order, count the drop. THE one rim law —
 * zrender's draw_heightfield emits exactly this plan (charter §29-6).
 */
RimPlan rim_plan(const terrain::ComposedLattice& lat, const int32_t* vdist);

}  // namespace forge
}  // namespace zref
