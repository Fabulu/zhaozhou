// render_heightfield.cpp — W3.5 DrawProcedural (heightfield_patch) directed
// test + differential vs a NAIVE per-pixel oracle.
//
// The fixture is a single flat 2x2 patch (one quad) under an ortho top-down
// matrix, so the projected quad is an exact axis-aligned rectangle and the
// oracle can predict every pixel WITHOUT the edge-function machinery:
// containment by bounds + the hand-computed flat lambert colour + the
// resolve dither law. This differentially proves the §8 raster (edge setup,
// top-left bias, plane stepping), the painter sort and the shading path.
//
// Hand computation (all integer, shown per law):
//   vp scale 1/32 (fx raw 2048): ndc = world/32; Z60 view 384x240:
//     px(x) = 192 + x*6        py(z) = 120 + z*(120/32 = 3.75)
//   patch envelope +-8 m -> ndc x,z = +-0.25:
//     x in [192-48, 192+48) = [144,240);  y in [120-30, 120+30) = [90,150)
//   flat cell normal (0,1,0) (up, y-up world; terrain.cpp winding check):
//     shade = nhat . L = kLightY = 53521            (qformats §7.4-style)
//     c = (base*shade + 32768)>>16 per channel      (single rounding)
//   then RGB565 via the resolve law (resolve.cpp):
//     r5 = (r*31 + B*16+8)/255, g6 = (g*63 + B*32+16)/255, b5 as r5
//
// Law: spec/qformats.md §3/§4/§7.2/§8; spec/commands.zidl DrawProcedural;
//      charter §12 (sheet tint — the stamped variant darkens by the sheet).

#include "render_helpers.hpp"
#include "zref/zref_terrain.hpp"

#include <cstdio>
#include <vector>

namespace {

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

constexpr uint8_t kBayer4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

// the naive oracle's 565 value of a working-space rgb at (x, y)
uint16_t oracle_px(uint8_t r, uint8_t g, uint8_t b, uint32_t x, uint32_t y) {
  const uint8_t B = kBayer4[y & 3][x & 3];
  const uint32_t r5 = (static_cast<uint32_t>(r) * 31 + B * 16 + 8) / 255;
  const uint32_t g6 = (static_cast<uint32_t>(g) * 63 + B * 32 + 16) / 255;
  const uint32_t b5 = (static_cast<uint32_t>(b) * 31 + B * 16 + 8) / 255;
  return static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
}

zref::render::RenderResult render_island(zref::render::SoftwareRenderer& rend,
                                         zref::render::RenderCanvas& canvas,
                                         const zref::render::RenderResources& res,
                                         uint32_t frame_id, bool with_stamp) {
  const auto body = [&](zhao::ZhaoFrameBuilder& b) {
    auto spc = zhao_abi::zhao_sample_set_presentation_contract();
    spc.payload.mode = zhao_abi::VIDEO_Z60;
    std::vector<uint8_t> v0;
    zhao_abi::zhao_pack_set_presentation_contract(spc, v0);
    b.append_record(v0);

    auto sv = zhao_abi::zhao_sample_set_view();
    sv.payload.view_id = 0;
    sv.payload.view_projection = rtest::ortho_topdown(2048);
    std::vector<uint8_t> v1;
    zhao_abi::zhao_pack_set_view(sv, v1);
    b.append_record(v1);

    if (with_stamp) {
      auto st = zhao_abi::zhao_sample_surface_stamp();
      st.payload.brush = 0;
      st.payload.patch = 44;  // the patch handle (sheet identity)
      st.payload.operation = 0;
      st.payload.tag = 1;
      st.payload.strength = 0xFFFF;  // sheet strength u8 255 -> tint 127
      st.payload.transform = rtest::xform_identity();
      st.payload.radius = 8 << 16;  // covers the whole patch
      st.payload.ring_width = 0;
      std::vector<uint8_t> v2;
      zhao_abi::zhao_pack_surface_stamp(st, v2);
      b.append_record(v2);
    }

    auto dp = zhao_abi::zhao_sample_draw_procedural();
    dp.payload.program = 44;  // terrain-patch page handle
    dp.payload.material = 45;
    dp.payload.transform = rtest::xform_identity();
    dp.payload.screen_error = 1 << 16;
    dp.payload.kind = zhao_abi::FORGE_HEIGHTFIELD_PATCH;
    std::vector<uint8_t> v3;
    zhao_abi::zhao_pack_draw_procedural(dp, v3);
    b.append_record(v3);
  };
  return rend.render_frame(rtest::seal_frame(frame_id, body), 0, canvas, res);
}

void test_flat_quad_oracle() {
  zref::render::TerrainPatch patch;  // 2x2, flat 64 m, +-8 m envelope
  patch.width = 2;
  patch.height = 2;
  patch.env_x0 = -8 * (1 << 16);
  patch.env_z0 = -8 * (1 << 16);
  patch.env_x1 = 8 * (1 << 16);
  patch.env_z1 = 8 * (1 << 16);
  patch.heights.assign(4, 64 * 256);

  zref::render::Material mat{200, 180, 160};
  zref::render::RenderResources res;
  res.terrain_patches.push_back({44, &patch});
  res.materials.push_back({45, mat});

  zref::render::SoftwareRenderer rend;
  zref::render::RenderCanvas canvas;
  const zref::render::RenderResult r = render_island(rend, canvas, res, 1, false);
  check(r.status == zhao_abi::ZH_ABI_OK, "flat quad frame renders");
  check(r.terrain_velocity.empty(), "no TerrainField -> no velocity samples");

  // hand-computed flat colour (see file header): shade = kLightY = 53521
  const int32_t shade = 53521;
  const auto q = [shade](uint8_t base) {
    return static_cast<uint8_t>((static_cast<int32_t>(base) * shade + 32768) >> 16);
  };
  const uint8_t er = q(mat.r), eg = q(mat.g), eb = q(mat.b);

  // differential: EVERY canvas pixel vs the naive containment oracle
  int mismatch = 0;
  for (uint32_t y = 0; y < 240; ++y) {
    for (uint32_t x = 0; x < 384; ++x) {
      const bool inside = x >= 144 && x < 240 && y >= 90 && y < 150;
      // black is NOT 0x0000 everywhere: the dither law can lift a 0 channel
      // to 1 LSB (e.g. g=0 at threshold 8 -> (0+272)/255 = 1)
      const uint16_t want = inside ? oracle_px(er, eg, eb, x, y) : oracle_px(0, 0, 0, x, y);
      const uint16_t got = rtest::px(canvas, 0, x, y, 384);
      if (got != want && mismatch++ < 8) {
        std::fprintf(stderr, "  mismatch at (%u,%u): got %04x want %04x\n", x, y, got, want);
      }
    }
  }
  check(mismatch == 0, "flat quad == naive per-pixel oracle (all 92160 px)");
}

void test_surface_sheet_tint() {
  zref::render::TerrainPatch patch;  // same fixture
  patch.width = 2;
  patch.height = 2;
  patch.env_x0 = -8 * (1 << 16);
  patch.env_z0 = -8 * (1 << 16);
  patch.env_x1 = 8 * (1 << 16);
  patch.env_z1 = 8 * (1 << 16);
  patch.heights.assign(4, 64 * 256);
  zref::render::Material mat{200, 180, 160};
  zref::render::RenderResources res;
  res.terrain_patches.push_back({44, &patch});
  res.materials.push_back({45, mat});

  // frame 1: stamp first (persistent sheet), frame 2: no stamp, sheet stays
  zref::render::SoftwareRenderer rend;
  zref::render::RenderCanvas canvas;
  const zref::render::RenderResult r1 = render_island(rend, canvas, res, 1, true);
  check(r1.status == zhao_abi::ZH_ABI_OK, "stamped frame renders");
  check(rend.sheets().size() == 1 && rend.sheets()[0].first == 44,
        "one persistent sheet keyed by the patch handle");
  // the stamp is a radius-8 m circle: the naive oracle predicts the sheet
  // from texel-centre distances (envelope +-8 m, 64 texels)
  {
    const zref::render::SurfaceSheet& sheet = rend.sheets()[0].second;
    int wrong = 0;
    for (int j = 0; j < 64; ++j)
      for (int i = 0; i < 64; ++i) {
        // texel centre in metres: ((i+0.5)/64 - 0.5) * 16
        const int64_t wx = ((2 * i + 1 - 64) * (int64_t)8 * 65536) / 64;
        const int64_t wz = ((2 * j + 1 - 64) * (int64_t)8 * 65536) / 64;
        const int64_t r2 = (int64_t)(8 << 16) * (8 << 16);
        const bool stamped = wx * wx + wz * wz <= r2;
        const uint8_t want = stamped ? 255 : 0;
        if (sheet.strength[j * 64 + i] != want) ++wrong;
      }
    check(wrong == 0, "circle stamp == naive texel-centre oracle (4096 cells)");
    check(sheet.tag[32 * 64 + 32] == 1, "tag byte stored at the centre");
  }
  // tint law: strength 255 -> tint = 255 - 255/2 = 128 (half-darkening)
  const int32_t shade = 53521;
  const auto lit = [shade](uint8_t base) {
    return static_cast<int32_t>((static_cast<int32_t>(base) * shade + 32768) >> 16);
  };
  const auto tinted = [lit](uint8_t base) {
    return static_cast<uint8_t>((lit(base) * 128 + 128) >> 8);
  };
  // (192,120) is inside the quad [144,240)x[90,150); its cell centre (0,0)
  // samples the sheet centre (strength 255 -> tint 128)
  check(rtest::px(canvas, 0, 192, 120, 384) ==
            oracle_px(tinted(mat.r), tinted(mat.g), tinted(mat.b), 192, 120),
        "stamped pixel darkened by the sheet tint law");
  check(rtest::px(canvas, 0, 192, 120, 384) !=
            oracle_px(static_cast<uint8_t>(lit(mat.r)), static_cast<uint8_t>(lit(mat.g)),
                      static_cast<uint8_t>(lit(mat.b)), 192, 120),
        "tint actually changed the pixel (not a passthrough)");

  // frame 2 without a stamp command: the persistent sheet still tints
  const zref::render::RenderResult r2 = render_island(rend, canvas, res, 2, false);
  check(r2.status == zhao_abi::ZH_ABI_OK && r2.canvas_crc32c == r1.canvas_crc32c,
        "sheet persists across frames (charter §12): identical canvas CRC");
}

// a stamped ANNULUS: ring_width > 0 leaves the disc interior untouched
void test_ring_stamp() {
  zref::render::TerrainPatch patch;
  patch.width = 2;
  patch.height = 2;
  patch.env_x0 = -8 * (1 << 16);
  patch.env_z0 = -8 * (1 << 16);
  patch.env_x1 = 8 * (1 << 16);
  patch.env_z1 = 8 * (1 << 16);
  patch.heights.assign(4, 64 * 256);
  zref::render::RenderResources res;
  res.terrain_patches.push_back({44, &patch});
  res.materials.push_back({45, zref::render::Material{200, 180, 160}});

  zref::render::SoftwareRenderer rend;
  zref::render::RenderCanvas canvas;
  const auto body = [&](zhao::ZhaoFrameBuilder& b) {
    auto sv = zhao_abi::zhao_sample_set_view();
    sv.payload.view_id = 0;
    sv.payload.view_projection = rtest::ortho_topdown(2048);
    std::vector<uint8_t> v1;
    zhao_abi::zhao_pack_set_view(sv, v1);
    b.append_record(v1);
    auto st = zhao_abi::zhao_sample_surface_stamp();
    st.payload.patch = 44;
    st.payload.tag = 2;
    st.payload.strength = 0xFFFF;
    st.payload.transform = rtest::xform_identity();
    st.payload.radius = 8 << 16;      // outer = envelope edge
    st.payload.ring_width = 6 << 16;  // annulus [2 m, 8 m]
    std::vector<uint8_t> v2;
    zhao_abi::zhao_pack_surface_stamp(st, v2);
    b.append_record(v2);
  };
  const zref::render::RenderResult r =
      rend.render_frame(rtest::seal_frame(1, body), 0, canvas, res);
  check(r.status == zhao_abi::ZH_ABI_OK, "ring stamp frame validates");
  const zref::render::SurfaceSheet& s = rend.sheets()[0].second;
  // sheet texel (32,32) = centre -> inside the hole (dist 0 < 2 m)
  check(s.strength[32 * 64 + 32] == 0, "annulus hole: centre texel untouched");
  // texel world x = ((i+0.5) - 32)/4 metres; annulus [2,8]:
  //   i=8  -> x = -5.875 m  -> stamped
  //   i=28 -> x = -0.875 m  -> inside the hole
  check(s.strength[32 * 64 + 8] == 255, "annulus body texel stamped");
  check(s.strength[32 * 64 + 28] == 0, "annulus hole boundary respected");
}

// ---- sub-metre grid spacing (defect fixed 2026-08-15) ----------------------
//
// draw_heightfield built its shading normal with rescale_s32(cross, 32). The
// cross product of two fx16 (Q16.16) edge vectors is Q32.32, so >>32 yields
// Q32.0 — the normal quantised to WHOLE world-units^2. Below ~1 m grid
// spacing every component of a near-flat cell rounds to 0, the nmag2 == 0
// guard fires for every triangle, and the whole patch shades solid black.
// The rescale is 16 now. This test renders a 41x41 bump over +-12 m (0.6 m
// spacing) and demands (a) the patch is NOT a black silhouette and (b) its
// shading actually VARIES across the bump.
void test_submetre_shading() {
  // 0.6 m spacing: 24 m across 40 cells
  const zref::render::TerrainPatch patch = rtest::bump_patch(41, 41, 12, 10);
  zref::render::Material mat{200, 180, 160};
  zref::render::RenderResources res;
  res.terrain_patches.push_back({44, &patch});
  res.materials.push_back({45, mat});

  zref::render::SoftwareRenderer rend;
  zref::render::RenderCanvas canvas;
  const zref::render::RenderResult r = render_island(rend, canvas, res, 1, false);
  check(r.status == zhao_abi::ZH_ABI_OK, "sub-metre patch frame renders");

  // ortho_topdown(2048) => px = 192 + x*6, py = 120 + z*3.75; +-12 m maps to
  // x in [120,264), y in [75,165). Sample the interior conservatively.
  uint32_t lit = 0, total = 0;
  std::vector<uint16_t> distinct;
  for (uint32_t y = 78; y < 162; ++y) {
    for (uint32_t x = 123; x < 261; ++x) {
      ++total;
      const uint16_t p = rtest::px(canvas, 0, x, y, 384);
      const uint32_t r5 = p >> 11, g6 = (p >> 5) & 63, b5 = p & 31;
      if (r5 + g6 + b5 > 6) ++lit;
      // Bayer index is (y&3, x&3): sample only the B == 0 phase so any
      // variation seen is SHADING, never dither.
      if ((x & 3) == 0 && (y & 3) == 0) {
        bool seen = false;
        for (uint16_t v : distinct)
          if (v == p) seen = true;
        if (!seen && distinct.size() < 64) distinct.push_back(p);
      }
    }
  }
  std::printf("  sub-metre patch: %u/%u lit px, %zu distinct undithered values\n", lit, total,
              distinct.size());
  check(lit > (total * 9) / 10, "0.6 m grid spacing is NOT a black silhouette");
  check(distinct.size() >= 3, "sub-metre patch shading varies across the bump");

  // and the same envelope at 1.0 m spacing (25x25) must agree qualitatively —
  // the old code only worked here, which is why nothing caught the defect
  const zref::render::TerrainPatch coarse = rtest::bump_patch(25, 25, 12, 10);
  zref::render::RenderResources res2;
  res2.terrain_patches.push_back({44, &coarse});
  res2.materials.push_back({45, mat});
  zref::render::SoftwareRenderer rend2;
  zref::render::RenderCanvas canvas2;
  check(render_island(rend2, canvas2, res2, 1, false).status == zhao_abi::ZH_ABI_OK,
        "1.0 m spacing control frame renders");
  uint32_t lit2 = 0;
  for (uint32_t y = 78; y < 162; ++y)
    for (uint32_t x = 123; x < 261; ++x) {
      const uint16_t p = rtest::px(canvas2, 0, x, y, 384);
      if ((p >> 11) + ((p >> 5) & 63) + (p & 31) > 6) ++lit2;
    }
  check(lit2 > (total * 9) / 10, "1.0 m spacing control is lit too");
}

// ---- near-plane per-primitive rejection (deep-keel wave) --------------------
//
// The documented Phase-3 clip model is WHOLE-PRIMITIVE near-plane rejection
// (sky_and_beams.md 1.2 projection corollary: the under-plane subdivides
// "so behind-camera cells cull without deleting the plane"). draw_heightfield
// used to reject the whole PATCH when ANY lattice vertex had w <= 0 - a near
// camera inside the envelope erased the island (found by the creature lane,
// terrain.cpp:310). The fix applies the same law at primitive granularity:
// cells (and wall quads) whose corners include a behind-eye vertex drop,
// the rest draw. This test could not have been green before the fix - the
// old code drew NOTHING for this camera.
void test_near_camera_keeps_island() {
  // 9x9 dual slab, env +-8 m, top 4 m, bottom 0
  zref::render::TerrainPatch patch;
  patch.width = patch.height = 9;
  patch.env_x0 = patch.env_z0 = -(8 << 16);
  patch.env_x1 = patch.env_z1 = (8 << 16);
  patch.heights.assign(81, 1024);
  patch.bottom.assign(81, 0);
  patch.scar.assign(81, 0);
  patch.cell_state.assign(64, zref::terrain::kSolid);

  // perspective with w = z: vertices at z < 0 are behind the eye. The
  // island spans z in [-8, 8]: the near half's vertices are behind, the far
  // half's are in front.
  const int32_t m[16] = {2048, 0,     0,       0,  //
                         0,    -2048, 0,       0,  //
                         0,    0,     1 << 16, 0,  //
                         0,    0,     1 << 16, 0};
  zref::render::Material mat{200, 180, 160};
  zref::render::RenderResources res;
  res.terrain_patches.push_back({44, &patch});
  res.materials.push_back({45, mat});
  zref::render::SoftwareRenderer rend;
  zref::render::RenderCanvas canvas;
  const auto shoot = [&](zref::render::RenderCanvas& c) {
    const auto body = [&](zhao::ZhaoFrameBuilder& b) {
      auto sv = zhao_abi::zhao_sample_set_view();
      sv.payload.view_id = 0;
      sv.payload.view_projection = rtest::mat(m);
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
    return rend.render_frame(rtest::seal_frame(1, body), 0, c, res);
  };
  const zref::render::RenderResult r = shoot(canvas);
  check(r.status == 0, "near-camera frame renders");

  // census: the far half (z in (0, 8]) must paint; with w = z the far cells
  // project around screen centre (ndc = (x/32)/z). The old whole-patch law
  // painted NOTHING here.
  uint32_t lit = 0;
  for (uint32_t y = 60; y < 180; ++y)
    for (uint32_t x = 60; x < 320; ++x) {
      const uint16_t p = rtest::px(canvas, 0, x, y, 384);
      if ((p >> 11) + ((p >> 5) & 63) + (p & 31) > 6) ++lit;
    }
  std::printf("  near-camera: %u lit px (the far half of the island)\n", lit);
  check(lit > 200, "the island does NOT vanish: far-half cells draw");
  check(lit < 20000, "the behind-eye half is dropped (not the whole canvas)");

  // determinism: the same shoot twice is byte-identical
  zref::render::RenderCanvas canvas2;
  shoot(canvas2);
  bool same = canvas.slot[0].size() == canvas2.slot[0].size();
  if (same)
    for (size_t k = 0; k < canvas.slot[0].size(); ++k)
      if (canvas.slot[0][k] != canvas2.slot[0][k]) same = false;
  check(same, "near-camera rejection is deterministic");
}

}  // namespace

int main() {
  test_flat_quad_oracle();
  test_surface_sheet_tint();
  test_ring_stamp();
  test_submetre_shading();
  test_near_camera_keeps_island();
  if (failures == 0) std::printf("render_heightfield: all green\n");
  return failures == 0 ? 0 : 1;
}
