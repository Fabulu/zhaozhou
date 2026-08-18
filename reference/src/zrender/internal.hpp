// internal.hpp — zrender internal surface (NOT installed; the public API is
// zref/zref_render.hpp). Shared by rast.cpp / terrain.cpp / sprites.cpp /
// render_frame.cpp / resolve.cpp.
//
// Law: spec/qformats.md §2 (mat4fx x vec4 s128 exact row sum + ONE rescale),
// §3 (single-rounding fx16 ops), §4 (round_half_up signed division),
// §8 (screenXY S 12.8, ±2048 px guard band, edge functions E0 s64 setup /
// E' = E0>>8 / top-left bias / exact stepping, depth = 1/z larger-closer);
// charter §8 (24-bit RGB working colour, RGB565 at resolve);
// plan W3.5/D7 (painter's algorithm with Q16.16 1/z depth per view).

#pragma once

#include "zref/zref_render.hpp"
#include "zref/zref_terrain.hpp"

namespace zref {
namespace render {

// generated ABI record types used by the internal draw entry points
using zhao_abi::ZhCmdSurfaceStamp;
using zhao_abi::ZhCmdTerrainField;
using zhao_abi::ZhTransform2fx;

// ---- viewport (canvas-local pixels; Duo map per video_rules.md §3.1) ------

struct Viewport {
  uint32_t x0 = 0, y0 = 0, w = 0, h = 0;
};

/**
 * The viewports of a mode: Duo = two 256x192 view blocks STACKED in the
 * storage raster (video_rules.md §3.1 packed layout, ratified 2026-08-15) —
 * view 0 at rows 0..191 (slot bytes [0,0x18000)), view 1 at rows 192..383
 * (slot bytes [0x18000,0x30000)). NOT side by side: the views are separate
 * contiguous blocks, and the 512-wide displayed row is assembled at scanout.
 * Else one full canvas.
 */
inline uint32_t viewports_of(zhao_abi::video_mode m, Viewport out[2]) {
  if (m == zhao_abi::VIDEO_DUO) {
    out[0] = Viewport{0, 0, 256, 192};
    out[1] = Viewport{0, 192, 256, 192};
    return 2;
  }
  const uint32_t w = canvas_width(m), h = canvas_height(m);
  out[0] = Viewport{0, 0, w, h};
  return 1;
}

// ---- working surface (charter §8: RGB888 working, Q16.16 depth) -----------

struct WorkSurface {
  uint32_t w = 0, h = 0;
  std::vector<uint8_t> rgb;    // w*h*3
  std::vector<int32_t> depth;  // Q16.16 1/w; clear value 0 = far (§8)

  void reset(uint32_t width, uint32_t height, sky::SkyColor bg) {
    w = width;
    h = height;
    rgb.assign(static_cast<size_t>(width) * height * 3, 0);
    for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
      rgb[i * 3 + 0] = bg.r;
      rgb[i * 3 + 1] = bg.g;
      rgb[i * 3 + 2] = bg.b;
    }
    depth.assign(static_cast<size_t>(width) * height, 0);
  }
};

// ---- projection (qformats.md §2/§3/§8) --------------------------------------

struct ScreenV {
  int32_t x = 0, y = 0;  // S 12.8 canvas pixels (§8)
  int32_t d = 0;         // Q16.16 1/w depth (D7)
  int32_t a = 0;         // Q16.16 interpolated attribute (vertex alpha)
  int32_t u = 0, v = 0;  // Q16.16 TILE units (terrain texturing, §6.2);
                         // read only when raster_tri carries a TextureSpan
};

struct ProjOut {
  bool in = false;  // false: w <= 0 (behind the eye) — Phase-3 near-plane
                    // rejection culls the whole primitive (documented)
  ScreenV s;
};

/**
 * World vertex -> canvas screen vertex. Pipeline (every step cited):
 *   clip   = mat4_vec4(vp, {x,y,z,1})        qformats §2 (s128 row sum)
 *   ndc    = fx_div_exact(clip, w)           §3 (one rounding)
 *   screen = fx_mad(ndc, half_extent, c) + to_screen_xy  §3 + §8
 *            (+Y NDC maps to +Y canvas row — pixel (0,0) is top-left,
 *             video_rules.md §2; the game authors its matrix y-down)
 *   depth  = fx_div_exact(1, w)              Q16.16 1/z, D7
 */
ProjOut project_vertex(const mat4fx& vp, const Viewport& vp_px, fx16 x, fx16 y, fx16 z,
                       SatLedger* L);

// round_half_up signed division on s128 (qformats §4) — the ONE rounding of
// the barycentric plane-equation setup below.
int32_t div_rhu_s128(__int128 n, __int128 d);

// ---- raster (qformats.md §8 edge-function law) ------------------------------

enum class BlendMode { kOpaque, kAlpha, kAdditive };

struct TriMode {
  bool depth_test = true;
  bool depth_write = true;
  bool use_fixed_depth = false;  // sky layer depths (§1.1 layer table)
  int32_t fixed_depth = 0;       // Q16.16
  BlendMode blend = BlendMode::kOpaque;
  bool interp_alpha = false;  // blend with v[i].a (Q16.16)
};

/**
 * The terrain texturing span (terrain_rules §6 + charter §15 "texture x
 * vertex light"): one primary CLUT8 sample per fragment, modulated by a
 * per-primitive Q16.16 colour factor (flat shade x layer-H tint x sheet
 * strength — composed ONCE per primitive, one rounding, before the raster).
 * mosaic = true picks between tile_a/tile_b per texel with the stable
 * world-space pattern (zref::terrain::mosaic_pick, §6.2 frozen constants);
 * the sample index uses the mirrored-repeat fold (zref::terrain::
 * mirror_texel). mod_* are Q16.16 in [0, 0x10000]; 0x10000 is EXACT unity
 * (no darkening at full shade/tint — the sheet-tint lesson, terrain.cpp).
 */
struct TextureSpan {
  const Tileset* ts = nullptr;
  uint8_t tile_a = 0, tile_b = 0, weight = 0;
  bool mosaic = false;
  int32_t mod_r = 1 << 16, mod_g = 1 << 16, mod_b = 1 << 16;
};

/**
 * Rasterize one triangle with the §8 law: s64 edge setup (subpixel^2),
 * E' = E0 >> 8 at pixel centres, D3D top-left bias, exact incremental
 * stepping, affine plane-equation interpolation of depth/alpha (ONE
 * round_half_up at setup, then exact s32 steps — the §8 "stepped edge"
 * model). Double-sided (Phase-3: terrain quads and the sky drum are emitted
 * with recorded windings but the software raster shades both — the RTL
 * backface-culling freeze is Phase 4/5; deviation noted in render_frame.cpp).
 * Scissored to `vp`.
 *
 * PAINTER/DEPTH LAW (D7 "painter's algorithm with Q16.16 1/z depth per
 * view", frozen here): terrain cells rasterize with depth_test = OFF and
 * depth_write = ON — the painter sort IS the ordering between terrain
 * cells, because a constant-w (orthographic) view-projection makes 1/z
 * constant per triangle, so a strict-greater depth test would reject every
 * later cell against the first writer and leave a single quad standing.
 * Depth is still WRITTEN per pixel so every later pass that depth-tests
 * (sky pass-6 sun/cloud, DrawForm markers, DrawPopulation sprites) resolves
 * against the terrain's 1/z exactly as qformats §8 prescribes. Under a
 * perspective matrix the sort and the depth lane agree, so the law is a
 * no-op refinement there; under ortho it is the only correct reading.
 */
void raster_tri(WorkSurface& s, const Viewport& vp, const ScreenV& A, const ScreenV& B,
                const ScreenV& C, uint8_t r, uint8_t g, uint8_t b, const TriMode& m,
                const TextureSpan* tex = nullptr);

/**
 * The §8 SCAN BOX: the whole-pixel range whose CENTRES can lie in triangle
 * ABC, scissored to `vp`. `empty` is exactly raster_tri's own early return.
 *
 * Extracted from raster_tri (2026-08-18, GEOM.CLIP increment) so the bbox law
 * has ONE site: GEOM.CLIP's viewport test is the same decision the software
 * raster makes, and `zref::Clip` reaches it by calling this rather than by
 * restating `(v_min + 127) >> 8`. Pure extract-function — raster_tri calls it
 * and the arithmetic is byte-identical; the golden capture CRCs are the proof.
 * Permutation-invariant in A/B/C (it is a min/max over the three), so it does
 * not matter whether it is asked before or after the winding flip.
 */
struct ScanBox {
  int32_t min_x = 0, max_x = 0, min_y = 0, max_y = 0;
  bool empty = true;  // min_x > max_x || min_y > max_y — nothing to scan
};

ScanBox scan_bbox(const ScreenV& A, const ScreenV& B, const ScreenV& C, const Viewport& vp);

// ---- shared constants -------------------------------------------------------

// Flat-shading light: unit (1,2,1)/sqrt(6), hand-normalized to Q16.16
// (0.40825 -> 26758, 0.81650 -> 53521). The renderer's ONE light; W3.7's
// look is tuned against it (change = golden-CRC regen).
inline constexpr int32_t kLightX = 26758;
inline constexpr int32_t kLightY = 53521;
inline constexpr int32_t kLightZ = 26758;

/**
 * The ONE flat-shade law (terrain top-surface verbatim; the creature lane
 * composes against the same light): exact s64 cross product of the fx16
 * edge vectors -> ONE rescale(.,16) per lane -> single-rounded lambert dot
 * via div_rhu_s128 over isqrt_u64 of the squared norm. Returns the Q16.16
 * weight in [0, 0x10000]; a zero-area triangle returns 0. Defined once in
 * zrender/terrain.cpp, shared per charter 29-6.
 */
int32_t shade_flat_tri(int32_t ax, int32_t ay, int32_t az, int32_t bx, int32_t by, int32_t bz,
                       int32_t cx, int32_t cy, int32_t cz, SatLedger* L);

// ---- terrain.cpp ------------------------------------------------------------

/** SurfaceStamp into a sheet over the patch envelope (charter §12). */
void stamp_surface(SurfaceSheet& sheet, const TerrainPatch& patch, const ZhCmdSurfaceStamp& st);

/** Sheet strength at a world position (charter §12 Phase-3 tint input). */
uint8_t sample_sheet(const SurfaceSheet& sheet, const TerrainPatch& patch, fx16 wx, fx16 wz);

struct FieldApp {
  const zfield::Decoded* prog;  // nullptr = resource miss (counted at walk)
  ZhCmdTerrainField cmd;
};

/**
 * THE one per-frame lattice evaluation (terrain_rules.md §4.1/§4.2): placed
 * grid (envelope lerp + transform2fx), composed top per §3.4 (base + scar
 * clamped at bottom, live TerrainField lanes in command order via the ONE
 * zfield interpreter, clamped at bottom), the bottom plane and the cell-state
 * copy. draw_heightfield tessellates it; zref::terrain::column_query
 * interpolates it; tests difference the two — nothing evaluates twice.
 */
terrain::ComposedLattice compose_lattice(const TerrainPatch& patch, const ZhTransform2fx& xform,
                                         const std::vector<FieldApp>& fields, uint32_t frame_tick,
                                         std::vector<TerrainVelocitySample>* velocity_out,
                                         SatLedger* L);

/**
 * DrawProcedural (forge_kind heightfield_patch): compose the lattice (above),
 * project, painter-sort far-to-near by centroid Q16.16 1/z (D7), flat-shade
 * (exact fx16 cross products -> §7.4-style normalize -> single-rounded
 * lambert dot), tint by the surface sheet, raster with depth. Dual patches
 * (terrain_rules §3) additionally emit the underside (bottom lattice, same
 * §4.3 diagonal, inverted winding, SOLID cells only) and rim walls (the
 * zref::forge::rim_plan quads: top edge at the composed top, bottom edge at
 * the MODELLED bottom — true local thickness, not the donor's fixed -50 m
 * curtain); void cells emit no surface at all (the breach shows sky).
 *
 * [deep-keel wave] Near-plane rejection is PER PRIMITIVE: a cell (or wall
 * quad) whose corner vertices include one behind the eye is dropped, the
 * rest of the patch still draws — the documented Phase-3 clip model
 * ("whole-primitive near-plane rejection", sky_and_beams.md §1.2 projection
 * corollary); the old whole-PATCH abort made a near camera erase the whole
 * island. A patch with a resolvable tileset AND layer E textures every
 * emitted polygon: tops via per-texel Mosaic picks, walls strata (tile
 * 240), underside tile 241 (terrain_rules §5/§6); the modulation is flat
 * shade x layer-H tint x sheet strength, quantised to the palette ladder.
 */
void draw_heightfield(WorkSurface& surf, const Viewport& vpp, const mat4fx& vp,
                      const TerrainPatch& patch, const ZhTransform2fx& xform, const Material& mat,
                      const SurfaceSheet* sheet, const std::vector<FieldApp>& fields,
                      uint32_t frame_tick, std::vector<TerrainVelocitySample>* velocity_out,
                      SatLedger* L, const Tileset* tileset = nullptr);

// ---- sprites.cpp ------------------------------------------------------------

/**
 * DrawForm: the marker/billboard quad — the fixed 8x8 pattern scaled by
 * `half_px` (screen-space when flags b1, else the world size perspective-
 * divided at projection scale 1), centred on the projected transform
 * position, WALL-CLAMPED to the canvas (the marker slides along the wall,
 * never vanishes — the wave-2 Duo marker demo law lineage). Opaque, depth
 * test + write.
 */
void draw_form_marker(WorkSurface& surf, const Viewport& vpp, const mat4fx& vp,
                      const FormPattern& form, const FormTransform& xf, uint16_t flags,
                      SatLedger* L);

/**
 * DrawPopulation: point (flags b0) / triangle (flags b1) particle sprites
 * from the pool snapshot, depth-tested, no depth write (charter §8 pass 7 —
 * particles never occlude). Draw order = snapshot order (deterministic).
 */
void draw_population(WorkSurface& surf, const Viewport& vpp, const mat4fx& vp,
                     const Population& pop, uint16_t flags, SatLedger* L);

}  // namespace render
}  // namespace zref
