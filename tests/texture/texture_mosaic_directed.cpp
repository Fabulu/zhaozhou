// texture_mosaic_directed.cpp — TEXTURE.MOSAIC reference tests (deep-keel
// wave; spec/terrain_rules.md §6.2 pattern + fold laws, frozen 2026-08-16).
//
// What each lane would catch (the "could have been red" statement):
//   1. mirror_texel directed — hand-computed anchors at the wrap boundaries
//      (first period, the mirror turn at u=1, the period-2 return, negative
//      u). Red on: any off-by-one in the fold, floor vs round drift, wrong
//      modulo for negatives.
//   2. mosaic_pick directed — the frozen hash constants: pinned p values for
//      four (tx,ty) anchors computed BY HAND from the spec formula, plus the
//      weight extremes (0 = pure B, 255 = pure A) and stability (the same
//      world texel always picks the same id). Red on: a changed constant, a
//      swapped candidate order, a non-stable pattern.
//   3. rendered Mosaic — a textured top cell shows BOTH tile families at
//      weight 128 (the zero-blend dither) and exactly ONE at the extremes;
//      the exact modulated pixel value at a probe point matches the
//      hand-computed shade-quantised product through the replicated resolve.
//      Red on: pick not per-texel, wrong tile id, wrong modulation factors,
//      UV off by a cell.

#include "render_helpers.hpp"  // tests/render packet/canvas helpers
#include "zref/zref_terrain.hpp"
#include "zrender/internal.hpp"  // white-box: draw-side structures

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

using zref::fx16;
namespace zt = zref::terrain;
namespace zr = zref::render;

// ---- 1. the mirrored-repeat fold (§6.2, hand anchors) -----------------------

void test_mirror_fold() {
  const auto U = [](double u) { return static_cast<int32_t>(std::lround(u * 65536.0)); };
  // first period, identity: texel index = floor(u*64)
  check(zt::mirror_texel(U(0.0)) == 0, "u=0 -> texel 0");
  check(zt::mirror_texel(U(0.25)) == 16, "u=0.25 -> texel 16 (floor of 16)");
  check(zt::mirror_texel(U(0.75)) == 48, "u=0.75 -> texel 48");
  check(zt::mirror_texel(U(63.0 / 64.0)) == 63, "u just below 1 -> texel 63");
  // the mirror turn: u in [1,2) folds to 127 - floor(u*64)
  check(zt::mirror_texel(U(1.0)) == 63, "u=1.0 exactly -> 63 (127-64, the edge clamp)");
  check(zt::mirror_texel(U(1.25)) == 47, "u=1.25 -> 47 (127-80)");
  check(zt::mirror_texel(U(1.75)) == 15, "u=1.75 -> 15 (127-112)");
  // period 2 returns to identity
  check(zt::mirror_texel(U(2.25)) == 16, "u=2.25 -> 16 (period 2 = two tiles)");
  check(zt::mirror_texel(U(3.25)) == 47, "u=3.25 -> 47 (period 2 of the mirror)");
  // negative u: floored mod keeps the mirror symmetric side
  check(zt::mirror_texel(U(-0.25)) == 15, "u=-0.25 -> 15 (floored mod 128)");
  check(zt::mirror_texel(U(-0.75)) == 47, "u=-0.75 -> 47");
  // the seam law: adjacent same-texture cells meet mirrored, so the texel
  // just left of a cell border and just right of it are MIRROR NEIGHBOURS
  // (63 and 63 at the border itself — no gap, no tear)
  check(zt::mirror_texel(U(1.0) - 1) == 63 && zt::mirror_texel(U(1.0)) == 63,
        "cell-border seam: both sides sample texel 63");
}

// ---- 2. the stable world-space pick (§6.2, frozen constants) ----------------

void test_mosaic_pick() {
  // p = ((tx*73856093) ^ (ty*19349663)) mod 255, hand-computed:
  //   (0,0): 0 ^ 0 = 0 -> p = 0
  //   (1,0): 73856093 -> 73856093 mod 255 = 188
  //   (0,1): 19349663 mod 255 = 8
  //   (2,3): (147712186 ^ 58048989) = 187313511 mod 255 = 84
  check(zt::mosaic_pick(10, 20, 255, 0, 0) == 10, "(0,0): p=0 < 255 -> A");
  check(zt::mosaic_pick(10, 20, 128, 1, 0) == 20, "(1,0): p=188 >= 128 -> B");
  check(zt::mosaic_pick(10, 20, 9, 0, 1) == 10, "(0,1): p=8 < 9 -> A");
  check(zt::mosaic_pick(10, 20, 9, 2, 3) == 20, "(2,3): p=84 >= 9 -> B");
  // BOUNDARY weights pin p EXACTLY (the pick is p < weight, so p itself
  // still selects B): one LSB of drift in either frozen constant flips
  // these - a mutated 73856093+1 makes p(1,0) = 189 and both checks invert
  check(zt::mosaic_pick(10, 20, 189, 1, 0) == 10, "p(1,0) == 188 exactly: w=189 picks A");
  check(zt::mosaic_pick(10, 20, 188, 1, 0) == 20, "w=188 flips (1,0) to B (p pinned)");
  check(zt::mosaic_pick(10, 20, 9, 0, 1) == 10, "p(0,1) == 8 exactly: w=9 picks A");
  check(zt::mosaic_pick(10, 20, 8, 0, 1) == 20, "w=8 flips (0,1) to B (p pinned)");
  check(zt::mosaic_pick(10, 20, 85, 2, 3) == 10, "p(2,3) == 84 exactly: w=85 picks A");
  check(zt::mosaic_pick(10, 20, 84, 2, 3) == 20, "w=84 flips (2,3) to B (p pinned)");
  // the extremes: 0 = pure B, 255 = pure A (mod 255 makes p <= 254)
  for (int32_t tx = -3; tx <= 3; ++tx)
    for (int32_t ty = -3; ty <= 3; ++ty) {
      check(zt::mosaic_pick(7, 9, 0, tx, ty) == 9, "weight 0: always B");
      check(zt::mosaic_pick(7, 9, 255, tx, ty) == 7, "weight 255: always A");
    }
  // stability: the pattern is a pure function of the world texel
  bool stable = true;
  for (int32_t tx = 0; tx < 64; ++tx)
    for (int32_t ty = 0; ty < 64; ++ty)
      if (zt::mosaic_pick(1, 2, 128, tx, ty) != zt::mosaic_pick(1, 2, 128, tx, ty)) stable = false;
  check(stable, "pick is deterministic at every texel");
  // and it actually dithers near the half weight (not all-one-side)
  int na = 0;
  for (int32_t tx = 0; tx < 64; ++tx)
    for (int32_t ty = 0; ty < 64; ++ty)
      if (zt::mosaic_pick(1, 2, 128, tx, ty) == 1) ++na;
  check(na > 1500 && na < 2600, "weight 128 dithers ~50/255 (measured 4096-texel census)");
  std::printf("  mosaic census at w=128: %d/4096 texels pick A\n", na);
}

// ---- 3. rendered Mosaic + the exact modulation ------------------------------

// a 2x2-cell SINGLE-SURFACE textured patch: env +-2 m (2 m pitch), flat
// 10 m top. Single-surface on purpose: under the D7 painter law an ortho
// view depth-ties every primitive and undersides (kind 1) paint after
// tops, so exact-pixel probes ride the top-only legacy shape (the dual
// texturing is exercised by the orbit/breach reel subjects).
// Candidates from layer E, tint from layer H, a hand-built 2-entry tileset.
zr::TerrainPatch tex_patch(uint8_t wa, uint16_t tint_v) {
  zr::TerrainPatch p;
  p.width = p.height = 3;
  p.env_x0 = p.env_z0 = -(2 << 16);
  p.env_x1 = p.env_z1 = (2 << 16);
  p.heights.assign(9, 2560);  // flat 10 m top
  p.tileset_id = 77;
  p.mat_a.assign(4, 0);  // tile 0: palette entry 1 everywhere
  p.mat_b.assign(4, 1);  // tile 1: palette entry 2 everywhere
  p.mat_w.assign(4, wa);
  p.tint.assign(9, tint_v);
  return p;
}

// fills a caller-owned tileset (the 1 MiB container must stay off the
// stack - the Windows default stack is 1 MiB, measured the hard way in the
// reel build too)
void two_tone_fill(zr::Tileset& ts) {
  ts.palette[1] = 0xD64C;  // (r5,g6,b5) = (26,50,12) -> 888 (214,202,99)
  ts.palette[2] = 0x2D5F;  // (5,42,31)   -> 888 (41,170,255)
  std::memset(ts.tiles[0], 1, sizeof(ts.tiles[0]));
  std::memset(ts.tiles[1], 2, sizeof(ts.tiles[1]));
}

zref::render::RenderResult render_textured(const zr::TerrainPatch& patch, zr::Tileset& ts,
                                           const zhao_abi::ZhMat4fx& view,
                                           zref::render::RenderCanvas& canvas) {
  zr::Material mat{200, 180, 160};
  zr::RenderResources res;
  res.terrain_patches.push_back({44, &patch});
  res.materials.push_back({45, mat});
  res.tilesets.push_back({77, ts});  // copies onto the vector's heap store
  zr::SoftwareRenderer rend;
  const auto body = [&](zhao::ZhaoFrameBuilder& b) {
    auto sv = zhao_abi::zhao_sample_set_view();
    sv.payload.view_id = 0;
    sv.payload.view_projection = view;
    std::vector<uint8_t> v1;
    zhao_abi::zhao_pack_set_view(sv, v1);
    b.append_record(v1);
    auto dp = zhao_abi::zhao_sample_draw_procedural();
    dp.payload.program = 44;
    dp.payload.material = 45;
    dp.payload.transform = rtest::xform_identity();
    dp.payload.screen_error = 1 << 16;
    dp.payload.kind = zhao_abi::FORGE_HEIGHTFIELD_PATCH;
    std::vector<uint8_t> v2;
    zhao_abi::zhao_pack_draw_procedural(dp, v2);
    b.append_record(v2);
  };
  return rend.render_frame(rtest::seal_frame(1, body), 0, canvas, res);
}

constexpr uint8_t kBayer4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

// the resolve law replicated for one pixel (resolve.cpp formula, house
// precedent: terrain_dual.cpp bg_px)
uint16_t resolve_px(const uint8_t rgb[3], uint32_t x, uint32_t y) {
  const uint8_t b = kBayer4[y & 3][x & 3];
  const uint32_t r5 = (rgb[0] * 31 + b * 16 + 8) / 255;
  const uint32_t g6 = (rgb[1] * 63 + b * 32 + 16) / 255;
  const uint32_t b5 = (rgb[2] * 31 + b * 16 + 8) / 255;
  return static_cast<uint16_t>((r5 > 31 ? 31 : r5) << 11 | ((g6 > 63 ? 63 : g6) << 5) |
                               (b5 > 31 ? 31 : b5));
}

void test_rendered_mosaic() {
  std::vector<zr::Tileset> heap_ts(1);  // heap: 1 MiB must not touch the stack
  two_tone_fill(heap_ts[0]);
  zr::Tileset& ts = heap_ts[0];
  const zhao_abi::ZhMat4fx view = rtest::ortho_topdown(2048);

  // weight 255 = pure A: every top pixel is the entry-1 family, and the
  // exact centre pixel matches the hand-computed product:
  //   flat top -> lambert = kLightY = 53521 -> shade_q = 3 -> 49152
  //   unity tint 0xFFFF avg -> 65536; no sheet -> 65536
  //   mod = rhu(49152*65536*65536 / 2^32) = 49152
  //   texel (214,202,99) -> (161,152,74) -> resolve at the probe
  zref::render::RenderCanvas c1;
  check(render_textured(tex_patch(255, 0xFFFF), ts, view, c1).status == 0, "pure-A cell renders");
  const uint16_t want_mid = resolve_px((const uint8_t[]){161, 152, 74}, 192, 120);
  check(rtest::px(c1, 0, 192, 120, 384) == want_mid,
        "exact modulated pixel: shade ladder x texel through the resolve");
  // the same point with weight 0 = pure B (entry 2):
  //   (41,170,255) x 49152 -> (31,128,191)
  zref::render::RenderCanvas c2;
  check(render_textured(tex_patch(0, 0xFFFF), ts, view, c2).status == 0, "pure-B cell renders");
  const uint16_t want_b = resolve_px((const uint8_t[]){31, 128, 191}, 192, 120);
  check(rtest::px(c2, 0, 192, 120, 384) == want_b, "weight 0 renders candidate B exactly");

  // weight 128: the dither — both families present in ONE cell (the
  // zero-blend transition the TMU budget buys)
  zref::render::RenderCanvas c3;
  check(render_textured(tex_patch(128, 0xFFFF), ts, view, c3).status == 0, "dither cell renders");
  const uint16_t fa = resolve_px((const uint8_t[]){161, 152, 74}, 0, 0);
  const uint16_t fb = resolve_px((const uint8_t[]){31, 128, 191}, 0, 0);
  // the dithered 565 values may each resolve to 1-2 variants per position;
  // census the cell rectangle (px 176..208 x py 104..136 at 1/32 scale)
  int na = 0, nb = 0, other = 0;
  for (uint32_t y = 100; y < 140; ++y)
    for (uint32_t x = 170; x < 215; ++x) {
      const uint16_t q = rtest::px(c3, 0, x, y, 384);
      if (q == resolve_px((const uint8_t[]){161, 152, 74}, x, y))
        ++na;
      else if (q == resolve_px((const uint8_t[]){31, 128, 191}, x, y))
        ++nb;
      else if (q != resolve_px((const uint8_t[]){0, 0, 0}, x, y))
        ++other;  // bg dither
    }
  std::printf("  dither census: A=%d B=%d other=%d\n", na, nb, other);
  check(na > 50 && nb > 50, "weight 128: BOTH tile families visible in one cell");
  check(other == 0, "no colour outside the two families (zero-blend law)");

  // a non-unity layer-H tint multiplies: tint 0xF7DE -> (r5,g6,b5) =
  // (30,62,30); 4-corner sums (120,248,120) -> factors
  //   r = rhu(120*65536, 124) = 63422, g = rhu(248*65536, 252) = 64496,
  //   b = 63422; mod = rhu(49152*factor, 2^32) -> (47567, 48372, 47567)
  //   texel (214,202,99) -> (155,149,72)
  zref::render::RenderCanvas c4;
  check(render_textured(tex_patch(255, 0xF7DE), ts, view, c4).status == 0, "tinted cell renders");
  const uint16_t want_t = resolve_px((const uint8_t[]){155, 149, 72}, 192, 120);
  check(rtest::px(c4, 0, 192, 120, 384) == want_t,
        "layer-H tint modulates the texel (hand-computed 5-bit factor)");
  (void)fa;
  (void)fb;
}

}  // namespace

int main() {
  test_mirror_fold();
  test_mosaic_pick();
  test_rendered_mosaic();
  if (failures == 0) std::printf("texture_mosaic_directed: all green\n");
  return failures == 0 ? 0 : 1;
}
