// render_sky.cpp — W3.5 sky tests: emit_layers layer census, the §1.1
// cloud vertex-alpha law, tick-exact scroll determinism (T vs T+3840),
// the rotation-only rot_proj validation, and the fallback flat-clear rule.
//
// Law: spec/sky_and_beams.md §1 (assembly, fallback, "no pixel left
// unwritten"), §1.1 (layer table: counts, UV laws, scroll formulas, cloud
// vertex alpha), §6 (the ZRef preview functions); spec/qformats.md §3/§4.

#include "render_helpers.hpp"

#include <algorithm>
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

// Sky rot_proj authoring under the §1.2 perspective law (w = row 2): a
// pitched rotation with focal scale f baked into rows 0-1 only — the FOV is
// the ratio rows0,1 : row2. look_neg_z mirrors the view down the -Z axis.
// All-integer authoring: s64 products, >>16, hand-verified in comments where
// used (charter §29-7 — tests author in integers too).
zhao_abi::ZhMat4fx sky_pitch_mat(int32_t f_raw, int32_t sin_raw, int32_t cos_raw, bool look_neg_z) {
  const auto mul16 = [](int64_t a, int64_t b) { return static_cast<int32_t>((a * b) >> 16); };
  const int32_t zs = look_neg_z ? -1 : 1;
  // y_view = cos*y - zs*sin*z ; z_view = sin*y + zs*cos*z ; screen y-down
  const int32_t m[16] = {f_raw,
                         0,
                         0,
                         0,
                         0,
                         -mul16(f_raw, cos_raw),
                         zs * mul16(f_raw, sin_raw),
                         0,
                         0,
                         sin_raw,
                         zs * cos_raw,
                         0,
                         0,
                         0,
                         0,
                         1 << 16};
  return rtest::mat(m);
}

zref::sky::SkySet test_set() {
  zref::sky::SkySet s;
  s.background = {4, 8, 16};
  s.band_lower_horizon = {90, 120, 200};
  s.band_lower_top = {140, 170, 230};
  s.band_upper_bottom = {140, 170, 230};
  s.band_upper_top = {240, 250, 255};
  s.cap = {250, 252, 255};
  s.under = {20, 30, 40};
  s.cloud = {255, 255, 255};
  s.sun = {255, 240, 200};
  s.cloud_max_alpha = zref::fx16{0xC000};
  s.sun_energy = zref::fx16{1 << 16};
  return s;
}

void test_layer_census() {
  // §1.1 layer table: 2 bands x 48 cols x 16 rows x 2 tris = 3072 (always;
  // kBandRows 8 -> 16, dcb32ff 2026-08-16: the banding fix),
  // + cap 16 + under 128 (8x8 grid since the §1.2 perspective amendment —
  // behind-camera cells must cull without deleting the plane) + cloud 128 +
  // sun 4 (flag-gated)
  const zref::sky::SkySet s = test_set();
  const uint8_t all =
      zref::sky::kLayerUnder | zref::sky::kLayerCloud | zref::sky::kLayerSun | zref::sky::kLayerCap;
  const auto prims = zref::sky::emit_layers(s, 0, zref::angle16{0}, all);
  check(prims.size() == 3072 + 48 + 128 + 128 + 4, "all-layers emission = 3380 primitives");
  const auto bands_only = zref::sky::emit_layers(s, 0, zref::angle16{0}, 0);
  check(bands_only.size() == 3072, "bands are the always-on backdrop (3072)");
  size_t per_layer[6] = {0, 0, 0, 0, 0, 0};
  for (const auto& p : prims) ++per_layer[static_cast<int>(p.layer)];
  check(per_layer[0] == 1536 && per_layer[1] == 1536, "48x16x2 per band");
  check(per_layer[2] == 48 && per_layer[3] == 128 && per_layer[4] == 128 && per_layer[5] == 4,
        "cap 48 (drum-pitch fan, S1.2) / under 128 / cloud 128 / sun 4");

  // §1.2 cap-fan corollary: every cap rim chord must coincide with a drum
  // top edge BIT-EXACTLY (same angle16 raws -> identical vertex words); a
  // 16-gon or unyawed fan leaves background slivers at the join.
  const auto yawed = zref::sky::emit_layers(s, 0, zref::angle16{0x0C00}, zref::sky::kLayerCap);
  size_t cap_rim_matched = 0;
  for (const auto& p : yawed) {
    if (p.layer != zref::sky::SkyLayer::Cap) continue;
    // v[1]/v[2] are the rim pair; find a band-upper top-row vertex equal to v[1]
    for (const auto& q : zref::sky::emit_layers(s, 0, zref::angle16{0x0C00}, 0)) {
      if (q.layer != zref::sky::SkyLayer::BandUpper) continue;
      for (int k = 0; k < 3; ++k)
        if (q.v[k].x.raw == p.v[1].x.raw && q.v[k].z.raw == p.v[1].z.raw &&
            q.v[k].y.raw == p.v[1].y.raw) {
          ++cap_rim_matched;
          k = 3;
        }
      if (cap_rim_matched > 0) break;
    }
    break;  // one chord suffices: the angles derive from the same formula
  }
  check(cap_rim_matched > 0, "cap rim vertex coincides bit-exactly with a drum top-edge vertex");
}

// §1.1 rows 1-2: the drum bands are a REAL 48-column cylinder. The column
// angle is `drum_yaw + a` where `a` is the circumferential parameter — an
// fx16 turn fraction in [0,65536), which IS the angle16 u16-turns container
// (qformats §2). A conversion that collapses `a` to 0 makes every column
// share one angle: the drum degenerates to a vertical line, every triangle
// has zero screen area, and `raster_tri` drops all 3072 of them — the 360°
// skybox silently ceases to exist while every census/UV/scroll test stays
// green. These assertions pin the geometry itself.
void test_band_geometry() {
  const zref::sky::SkySet s = test_set();
  const auto prims = zref::sky::emit_layers(s, 0, zref::angle16{0}, 0);
  check(prims.size() == 3072, "bands-only emission");

  // 1. the columns occupy MANY distinct world positions, not one.
  // 48 evenly spaced angles + the per-row radius lerp: cos/sin take on well
  // over 24 distinct values each. The collapsed-angle bug yields 9.
  std::vector<int32_t> xs, zs;
  for (const auto& p : prims)
    for (int k = 0; k < 3; ++k) {
      xs.push_back(p.v[k].x.raw);
      zs.push_back(p.v[k].z.raw);
    }
  auto distinct = [](std::vector<int32_t> v) {
    std::sort(v.begin(), v.end());
    return static_cast<size_t>(std::unique(v.begin(), v.end()) - v.begin());
  };
  const size_t nx = distinct(xs);
  const size_t nz = distinct(zs);
  if (nx <= 24 || nz <= 24)
    std::fprintf(stderr, "  band distinct X=%zu Z=%zu (collapsed drum gives 9)\n", nx, nz);
  check(nx > 24, "band columns span many distinct X (drum is a cylinder)");
  check(nz > 24, "band columns span many distinct Z (drum is a cylinder)");

  // 2. no band triangle is degenerate in the horizontal plane: every triangle
  // must have two vertices at DIFFERENT column angles, else it is a vertical
  // sliver that no rasteriser can fill.
  size_t degenerate = 0;
  for (const auto& p : prims) {
    const bool same = p.v[0].x.raw == p.v[1].x.raw && p.v[1].x.raw == p.v[2].x.raw &&
                      p.v[0].z.raw == p.v[1].z.raw && p.v[1].z.raw == p.v[2].z.raw;
    if (same) ++degenerate;
  }
  check(degenerate == 0, "no band triangle collapses to a vertical line");

  // 3. drum_yaw rotates the GEOMETRY while the UVs stay glued to the columns
  // (§1 — the texture visibly rotates with the drum).
  const auto rot = zref::sky::emit_layers(s, 0, zref::angle16{0x4000}, 0);
  size_t moved = 0, uv_changed = 0;
  for (size_t i = 0; i < prims.size(); ++i)
    for (int k = 0; k < 3; ++k) {
      if (prims[i].v[k].x.raw != rot[i].v[k].x.raw || prims[i].v[k].z.raw != rot[i].v[k].z.raw)
        ++moved;
      if (prims[i].v[k].u.raw != rot[i].v[k].u.raw || prims[i].v[k].v.raw != rot[i].v[k].v.raw)
        ++uv_changed;
    }
  check(moved > 0, "drum_yaw rotates band geometry");
  check(uv_changed == 0, "drum_yaw leaves band UVs glued to the columns");

  // 4. a quarter-turn yaw is exactly the 12-column shift of the 48-column
  // drum: column c at yaw 0x4000 sits where column c+12 sits at yaw 0.
  // (kDrumCols/4 == 12; the angles are the same u16 raws, so this is exact.)
  const int cols = zref::sky::kDrumCols;
  const int rows = zref::sky::kBandRows;
  size_t shift_mismatch = 0;
  for (int c = 0; c < cols; ++c) {
    // lower band, row 0, first triangle of the (col,row) pair
    const size_t base = static_cast<size_t>(c) * rows * 2;
    const size_t shifted = static_cast<size_t>((c + cols / 4) % cols) * rows * 2;
    if (rot[base].v[0].x.raw != prims[shifted].v[0].x.raw ||
        rot[base].v[0].z.raw != prims[shifted].v[0].z.raw)
      ++shift_mismatch;
  }
  check(shift_mismatch == 0, "yaw 0x4000 == a 12-column rotation of the 48-column drum");
}

void test_cloud_vertex_alpha_law() {
  // §1.1: alpha = (1 - r^2) * max_alpha in fx16, narrowed to u8.
  // hand: r2 = 0.25 (0x4000), max = 0.75 (0xC000):
  //   a = fx_mul(0xC000, 0x4000... (1-r2) = 0xC000) = 0.5625 -> 36864
  //   u8 = (36864 + 128) >> 8 = 144
  check(zref::sky::cloud_vertex_alpha(zref::fx16{0x4000}, zref::fx16{0xC000}, nullptr) == 144,
        "cloud alpha (1-r2)*max -> u8 = 144");
  check(zref::sky::cloud_vertex_alpha(zref::fx16{0}, zref::fx16{0xC000}, nullptr) == 192,
        "sheet centre alpha = max_alpha (0.75 -> 192)");
  check(zref::sky::cloud_vertex_alpha(zref::fx16{1 << 16}, zref::fx16{0xC000}, nullptr) == 0,
        "sheet edge r2 = 1 -> alpha 0");
  check(zref::sky::cloud_vertex_alpha(zref::fx16{5 << 16}, zref::fx16{0xC000}, nullptr) == 0,
        "r2 > 1 clamps to 0 (corner case)");
}

// §1.1 row 6: the sun centre vertex carries alpha = min(3*energy, 1) narrowed
// to u8 through the ONE §2 conversion. A hand-rolled `(a + 128) >> 8` wraps
// 256 -> 0 for every energy >= 1/3 (the default is 1.0), so the additive quad
// contributed nothing and the sun was invisible. Rim vertices are 0 by design,
// so the centre alpha is the whole signal.
void test_sun_centre_alpha() {
  zref::sky::SkySet s = test_set();
  auto centre_alpha = [&](int32_t energy_raw) {
    s.sun_energy = zref::fx16{energy_raw};
    const auto prims = zref::sky::emit_layers(s, 0, zref::angle16{0}, zref::sky::kLayerSun);
    // 4 sun tris, each a fan around the shared centre vertex v[0]
    uint8_t a = 0;
    int seen = 0;
    for (const auto& p : prims)
      if (p.layer == zref::sky::SkyLayer::Sun) {
        a = p.v[0].alpha;
        ++seen;
        check(p.v[1].alpha == 0 && p.v[2].alpha == 0, "sun rim vertices alpha 0 (radial falloff)");
      }
    check(seen == 4, "sun is a 4-triangle fan");
    return a;
  };
  // the energies the review found wrapping to 0 — all must now be full alpha
  check(centre_alpha(1 << 16) == 255, "sun energy 1.0 -> centre alpha 255");
  check(centre_alpha(32768) == 255, "sun energy 0.5 -> 3*e saturates -> 255");
  check(centre_alpha(21846) == 255, "sun energy just above 1/3 -> 255");
  check(centre_alpha(21845) == 255, "sun energy just below 1/3 -> 255 (0xFFFF rounds up)");
  // and the law itself below the saturation knee: alpha = 3*energy
  check(centre_alpha(1 << 14) == 192, "sun energy 0.25 -> 3*e = 0.75 -> 192");
  check(centre_alpha(0) == 0, "sun energy 0 -> alpha 0");
}

// ...and the quad must actually reach the canvas. Render the same frame with
// and without kLayerSun: the additive sun has to change pixels.
void test_sun_visible_on_canvas() {
  const zref::sky::SkySet s = test_set();
  zref::render::RenderResources res;
  res.sky_sets.push_back({2, s});
  // §1.2 perspective authoring: sun_dir defaults to -Z (anchor y 2048,
  // z -4096, elevation 26.6 deg); camera looks -Z pitched up 25 deg
  // (sin 27697, cos 59404), f = 1.4 (91750). Hand check: y_view =
  // 0.9063*2048 + 0.4226*(-4096) = 125, z_view = 0.4226*2048 + 0.9063*4096
  // = 4577 -> ndc_y = -1.4*125/4577 = -0.038 — the sun sits near centre.
  const zhao_abi::ZhMat4fx m = sky_pitch_mat(91750, 27697, 59404, true);

  auto render_with = [&](uint8_t flags, std::vector<uint16_t>& out) {
    zref::render::SoftwareRenderer rend;
    zref::render::RenderCanvas canvas;
    const auto pkt = rtest::seal_frame(9, [&](zhao::ZhaoFrameBuilder& b) {
      auto sv = zhao_abi::zhao_sample_set_view();
      sv.payload.view_id = 0;
      sv.payload.view_projection = rtest::ortho_topdown(2048);
      std::vector<uint8_t> v1;
      zhao_abi::zhao_pack_set_view(sv, v1);
      b.append_record(v1);
      auto sk = zhao_abi::zhao_sample_draw_sky();
      sk.payload.sky_set = 2;
      sk.payload.rot_proj[0] = m;
      sk.payload.rot_proj[1] = m;
      sk.payload.drum_yaw = 0;
      sk.payload.viewport_mask = 1;
      sk.payload.flags = flags;
      std::vector<uint8_t> v2;
      zhao_abi::zhao_pack_draw_sky(sk, v2);
      b.append_record(v2);
    });
    const zref::render::RenderResult r = rend.render_frame(pkt, 0, canvas, res);
    check(r.status == zhao_abi::ZH_ABI_OK, "sun fixture frame renders");
    out.clear();
    for (uint32_t y = 0; y < 240; ++y)
      for (uint32_t x = 0; x < 384; ++x) out.push_back(rtest::px(canvas, 0, x, y, 384));
  };

  const uint8_t base = zref::sky::kLayerUnder | zref::sky::kLayerCloud | zref::sky::kLayerCap;
  std::vector<uint16_t> without, with;
  render_with(base, without);
  render_with(static_cast<uint8_t>(base | zref::sky::kLayerSun), with);
  uint32_t diff = 0;
  for (size_t i = 0; i < with.size(); ++i)
    if (with[i] != without[i]) ++diff;
  std::printf("render_sky: sun pixels on canvas = %u\n", diff);
  check(diff > 0, "the sun quad changes pixels (additive blend reaches the canvas)");
}

// ---- WHICH WAY IS UP (latent defect pinned 2026-08-15) ---------------------
//
// Screen Y grows DOWNWARD (video_rules.md §2: pixel (0,0) is top-left, and
// project_vertex maps +Y NDC to +Y canvas row). World Y grows UP. So a
// rotation-only rot_proj authored with a naively POSITIVE y coefficient puts
// the zenith cap at the BOTTOM of the frame and the under-plane at the top —
// an exactly upside-down sky. Every other sky test passes on it: they check
// the layer census, the UV laws, scroll determinism and "some cap pixels
// exist", never which way is up.
//
// The fixture below is the canonically-authored §1.2 perspective matrix — a
// 5-degree upward pitch (sin 5714, cos 65287), f = 1.4 (91750), looking +Z.
// Hand checks (w = z_view = sin*y + cos*z):
//   cap front rim (y +2560, z +5120): ndc_y = -1.4*(0.9962*2560 -
//     0.0872*5120)/(0.0872*2560 + 0.9962*5120) = -1.4*2104/5323 = -0.55 ->
//     row ~54: strictly the UPPER half (the fan centre projects far above).
//   under-plane far edge (y -2560, z +5120): ndc_y = +1.4*2996/4877 = +0.86
//     -> row ~223: strictly the LOWER half.
// Flip any sign in that chain — the matrix, the viewport map, or the sky's
// Y constants — and this test fires.
void test_sky_orientation() {
  const zref::sky::SkySet s = test_set();
  zref::render::RenderResources res;
  res.sky_sets.push_back({2, s});
  const zhao_abi::ZhMat4fx m = sky_pitch_mat(91750, 5714, 65287, false);
  {
    zref::mat4fx rp;
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b) rp.m[a][b] = zref::fx16{(&m.m00)[a * 4 + b]};
    check(zref::sky::rot_proj_is_rotation_only(rp), "orientation fixture is rotation-only (§1)");
  }

  auto render_with = [&](uint8_t flags, std::vector<uint16_t>& out) {
    zref::render::SoftwareRenderer rend;
    zref::render::RenderCanvas canvas;
    const auto pkt = rtest::seal_frame(11, [&](zhao::ZhaoFrameBuilder& b) {
      auto sv = zhao_abi::zhao_sample_set_view();
      sv.payload.view_id = 0;
      sv.payload.view_projection = rtest::ortho_topdown(2048);
      std::vector<uint8_t> v1;
      zhao_abi::zhao_pack_set_view(sv, v1);
      b.append_record(v1);
      auto sk = zhao_abi::zhao_sample_draw_sky();
      sk.payload.sky_set = 2;
      sk.payload.rot_proj[0] = m;
      sk.payload.rot_proj[1] = m;
      sk.payload.drum_yaw = 0;
      sk.payload.viewport_mask = 1;
      sk.payload.flags = flags;
      std::vector<uint8_t> v2;
      zhao_abi::zhao_pack_draw_sky(sk, v2);
      b.append_record(v2);
    });
    check(rend.render_frame(pkt, 0, canvas, res).status == zhao_abi::ZH_ABI_OK,
          "orientation fixture frame renders");
    out.clear();
    for (uint32_t y = 0; y < 240; ++y)
      for (uint32_t x = 0; x < 384; ++x) out.push_back(rtest::px(canvas, 0, x, y, 384));
  };

  // bands-only baseline; the pixels a layer CHANGES are that layer's pixels
  std::vector<uint16_t> bands_only, with_cap, with_under;
  render_with(0, bands_only);
  render_with(zref::sky::kLayerCap, with_cap);
  render_with(zref::sky::kLayerUnder, with_under);

  uint32_t cap_px = 0, cap_lower = 0, under_px = 0, under_upper = 0;
  for (uint32_t y = 0; y < 240; ++y) {
    for (uint32_t x = 0; x < 384; ++x) {
      const size_t i = static_cast<size_t>(y) * 384 + x;
      if (with_cap[i] != bands_only[i]) {
        ++cap_px;
        if (y >= 120) ++cap_lower;
      }
      if (with_under[i] != bands_only[i]) {
        ++under_px;
        if (y < 120) ++under_upper;
      }
    }
  }
  std::printf("render_sky: orientation — cap %u px (%u below the midline), under %u px (%u above)\n",
              cap_px, cap_lower, under_px, under_upper);
  check(cap_px > 0, "the zenith cap reaches the canvas at all");
  check(under_px > 0, "the under-plane reaches the canvas at all");
  check(cap_lower == 0, "ZENITH cap pixels land in the UPPER half of the frame");
  check(under_upper == 0, "UNDER-plane pixels land in the LOWER half of the frame");
}

void test_scroll_determinism() {
  // §1.1 tick-exact scroll: T vs T+3840 byte-equal UVs (the 1-tile/64 s law)
  const zref::sky::SkySet s = test_set();
  const uint8_t all =
      zref::sky::kLayerUnder | zref::sky::kLayerCloud | zref::sky::kLayerSun | zref::sky::kLayerCap;
  const auto a = zref::sky::emit_layers(s, 777, zref::angle16{0x1234}, all);
  const auto b = zref::sky::emit_layers(s, 777 + 3840, zref::angle16{0x1234}, all);
  check(a.size() == b.size(), "same primitive count at T and T+3840");
  // compare UV bytes AND vertex alphas of every primitive
  int diffs = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    for (int k = 0; k < 3; ++k) {
      if (a[i].v[k].u.raw != b[i].v[k].u.raw || a[i].v[k].v.raw != b[i].v[k].v.raw ||
          a[i].v[k].alpha != b[i].v[k].alpha)
        ++diffs;
    }
  }
  check(diffs == 0, "tick T vs T+3840: byte-equal UVs + alphas");
  // and the scroll law itself: ((T % 3840) << 16)/3840 floor
  check(zref::sky::cloud_scroll_u(777).raw == static_cast<int32_t>(((777u % 3840u) << 16) / 3840u),
        "scroll_u floor law");
  check(zref::sky::cloud_scroll_v(777).raw == -zref::sky::cloud_scroll_u(777).raw,
        "scroll_v = -scroll_u");
  // different ticks differ (scroll actually moves)
  const auto c = zref::sky::emit_layers(s, 778, zref::angle16{0x1234}, all);
  check(c[0].v[0].u.raw == a[0].v[0].u.raw, "bands ignore the cloud scroll");
  int cloud_diff = 0;
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i].layer == zref::sky::SkyLayer::Cloud && a[i].v[0].u.raw != c[i].v[0].u.raw)
      ++cloud_diff;
  check(cloud_diff > 0, "cloud UVs advance with the tick");
}

void test_rot_proj_validation() {
  zref::mat4fx m{};
  const int32_t one = 1 << 16;
  m.m[0][0] = zref::fx16{one};
  m.m[1][1] = zref::fx16{one};
  m.m[2][2] = zref::fx16{one};
  m.m[3][3] = zref::fx16{one};
  check(zref::sky::rot_proj_is_rotation_only(m), "identity is rotation-only");
  m.m[0][3] = zref::fx16{one};  // translation in the column
  check(!zref::sky::rot_proj_is_rotation_only(m), "nonzero translation column rejected");
  m.m[0][3] = zref::fx16{0};
  m.m[3][0] = zref::fx16{one};  // perspective bottom row
  check(!zref::sky::rot_proj_is_rotation_only(m), "nonzero bottom row rejected");
}

// tiny local Bayer accessor (same matrix as resolve.cpp)
uint8_t kBayerRow(uint32_t y, uint32_t x) {
  constexpr uint8_t kB[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  return kB[y & 3][x & 3];
}

// dithered-black 565 value (the resolve law on rgb 0,0,0)
uint16_t oracle_black(uint32_t x, uint32_t y) {
  const uint8_t B = kBayerRow(y, x);
  const uint32_t r5 = (B * 16 + 8) / 255;
  const uint32_t g6 = (B * 32 + 16) / 255;
  const uint32_t b5 = (B * 16 + 8) / 255;
  return static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
}

// full-frame fallback behaviour through the renderer
void test_fallback_clear() {
  const zref::sky::SkySet s = test_set();
  zref::render::RenderResources res;
  res.sky_sets.push_back({2, s});

  // §1.2 perspective fixture: the orientation matrix (pitch up 5 deg,
  // f = 1.4) — cap, under-plane AND band rows all reach the canvas under it
  // (hand projections at test_sky_orientation).
  const zhao_abi::ZhMat4fx m = sky_pitch_mat(91750, 5714, 65287, false);

  const auto sky_pkt = [&](zhao_abi::ZhMat4fx rot, uint32_t set_handle) {
    return rtest::seal_frame(9, [&](zhao::ZhaoFrameBuilder& b) {
      auto sv = zhao_abi::zhao_sample_set_view();
      sv.payload.view_id = 0;
      sv.payload.view_projection = rtest::ortho_topdown(2048);
      std::vector<uint8_t> v1;
      zhao_abi::zhao_pack_set_view(sv, v1);
      b.append_record(v1);
      auto sk = zhao_abi::zhao_sample_draw_sky();
      sk.payload.sky_set = set_handle;
      sk.payload.rot_proj[0] = rot;
      sk.payload.rot_proj[1] = rot;
      sk.payload.drum_yaw = 0;
      sk.payload.viewport_mask = 1;
      sk.payload.flags = zref::sky::kLayerUnder | zref::sky::kLayerCloud | zref::sky::kLayerSun |
                         zref::sky::kLayerCap;
      std::vector<uint8_t> v2;
      zhao_abi::zhao_pack_draw_sky(sk, v2);
      b.append_record(v2);
    });
  };

  // 1. no DrawSky at all: every pixel black (the fallback clear)
  {
    zref::render::SoftwareRenderer rend;
    zref::render::RenderCanvas canvas;
    const auto pkt = rtest::seal_frame(9, [](zhao::ZhaoFrameBuilder& b) {
      auto sv = zhao_abi::zhao_sample_set_view();
      sv.payload.view_id = 0;
      sv.payload.view_projection = rtest::ortho_topdown(2048);
      std::vector<uint8_t> v1;
      zhao_abi::zhao_pack_set_view(sv, v1);
      b.append_record(v1);
    });
    const zref::render::RenderResult r = rend.render_frame(pkt, 0, canvas, res);
    check(r.status == zhao_abi::ZH_ABI_OK, "no-sky frame renders");
    // black through the dither law (NOT constant 0x0000: thresholds can
    // lift a channel to 1 LSB — resolve.cpp law)
    int bad = 0;
    for (uint32_t y = 0; y < 240; ++y)
      for (uint32_t x = 0; x < 384; ++x)
        if (rtest::px(canvas, 0, x, y, 384) != oracle_black(x, y)) ++bad;
    check(bad == 0, "DrawSky absent -> flat black clear (no pixel unwritten)");
  }

  // 2. a translated rot_proj violates §1 -> fallback to the set background
  {
    zhao_abi::ZhMat4fx bad = m;
    bad.m03 = 1 << 16;  // translation sneaks in
    zref::render::SoftwareRenderer rend;
    zref::render::RenderCanvas canvas;
    const zref::render::RenderResult r = rend.render_frame(sky_pkt(bad, 2), 0, canvas, res);
    check(r.status == zhao_abi::ZH_ABI_OK, "bad-rot frame renders");
    // the whole canvas must match the dither law applied to the set
    // background colour, per pixel
    constexpr uint8_t kB[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    int mism = 0;
    for (uint32_t y = 0; y < 240; ++y)
      for (uint32_t x = 0; x < 384; ++x) {
        const uint8_t Bv = kB[y & 3][x & 3];
        const uint32_t r5 = (4u * 31 + Bv * 16 + 8) / 255;
        const uint32_t g6 = (8u * 63 + Bv * 32 + 16) / 255;
        const uint32_t b5 = (16u * 31 + Bv * 16 + 8) / 255;
        const uint16_t w = static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
        if (rtest::px(canvas, 0, x, y, 384) != w) ++mism;
      }
    check(mism == 0, "invalid rot_proj -> flat clear in the set background");
  }

  // 3. a legal rot_proj renders: the cap (flat colour) and under-plane
  // (flat colour, depth 25 beats the bands' 0) must appear somewhere
  {
    zref::render::SoftwareRenderer rend;
    zref::render::RenderCanvas canvas;
    const zref::render::RenderResult r = rend.render_frame(sky_pkt(m, 2), 0, canvas, res);
    check(r.status == zhao_abi::ZH_ABI_OK, "legal sky frame renders");
    const auto flat565 = [](uint8_t cr, uint8_t cg, uint8_t cb, uint8_t B) {
      const uint32_t r5 = (static_cast<uint32_t>(cr) * 31 + B * 16 + 8) / 255;
      const uint32_t g6 = (static_cast<uint32_t>(cg) * 63 + B * 32 + 16) / 255;
      const uint32_t b5 = (static_cast<uint32_t>(cb) * 31 + B * 16 + 8) / 255;
      return static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
    };
    auto count_colour = [&](uint8_t cr, uint8_t cg, uint8_t cb) {
      uint32_t n = 0;
      for (uint32_t y = 0; y < 240; ++y)
        for (uint32_t x = 0; x < 384; ++x) {
          const uint8_t B = kBayerRow(y, x);
          if (rtest::px(canvas, 0, x, y, 384) == flat565(cr, cg, cb, B)) ++n;
        }
      return n;
    };
    check(count_colour(s.cap.r, s.cap.g, s.cap.b) > 0, "zenith cap flat colour visible");
    check(count_colour(s.under.r, s.under.g, s.under.b) > 0,
          "under-plane flat colour visible (real-depth pass 3)");

    // ...and the BANDS themselves — the backdrop that makes the drum a
    // skybox. Each band row carries lerp8(bottom, top, row, kBandRows) (the
    // render_frame.cpp gradient law); count pixels matching ANY band row
    // colour. A degenerate drum rasterises zero band pixels and this is the
    // only assertion that notices.
    const auto lerp8 = [](uint8_t a, uint8_t b, int num, int den) {
      return static_cast<uint8_t>(
          (static_cast<int32_t>(a) * (den - num) + static_cast<int32_t>(b) * num + den / 2) / den);
    };
    uint32_t band_px = 0;
    for (int band = 0; band < 2; ++band) {
      const zref::sky::SkyColor lo = band == 0 ? s.band_lower_horizon : s.band_upper_bottom;
      const zref::sky::SkyColor hi = band == 0 ? s.band_lower_top : s.band_upper_top;
      for (int row = 0; row < zref::sky::kBandRows; ++row)
        band_px += count_colour(lerp8(lo.r, hi.r, row, zref::sky::kBandRows),
                                lerp8(lo.g, hi.g, row, zref::sky::kBandRows),
                                lerp8(lo.b, hi.b, row, zref::sky::kBandRows));
    }
    // measured 371 with this fixture (the cap and under-plane cover the rest);
    // the collapsed-drum bug scores exactly 0, so the margin is the whole
    // distance between "the skybox exists" and "it does not".
    std::printf("render_sky: band pixels on canvas = %u\n", band_px);
    check(band_px > 100, "drum band pixels reach the canvas (the 360 deg backdrop exists)");
  }
}

// ---- §1.2 elevation-ramp continuity (spec amendment, 2026-08-16) -----------
//
// The defect this pins: a sky whose layers carry unrelated colours renders
// the three layer joins — under rim, band equator, cap rim — as hard
// elliptical outlines ("an oval on top, an oval at the bottom"; the owner's
// report). §1.2 makes C0-across-joins a property of a LEGAL sky set:
//   under == band_lower_horizon,  band_lower_top == band_upper_bottom,
//   cap == band_upper_top.
// The test renders a pitch-spanning frame twice — once with a conforming
// set, once with a deliberately violating one — and walks every canvas
// column vertically measuring the largest adjacent-pixel colour step
// (RGB888-expanded, max channel). A conforming sky's largest step must stay
// within the gradient banding quantum (+ dither allowance); the violating
// sky MUST exceed it — proving this assertion can fire at all.
void test_seam_continuity() {
  // conforming: one dusk elevation ramp sampled at the join elevations
  zref::sky::SkySet good;
  good.background = {24, 26, 70};
  good.under = {228, 130, 88};              // == band_lower_horizon (§1.2 rule 1)
  good.band_lower_horizon = {228, 130, 88}; // warm horizon
  good.band_lower_top = {150, 92, 118};     // == band_upper_bottom (rule 2)
  good.band_upper_bottom = {150, 92, 118};
  good.band_upper_top = {56, 48, 110};      // zenith
  good.cap = {56, 48, 110};  // == band_upper_top (rule 3)
  good.cloud = {255, 255, 255};  // unused (layers not requested below)
  good.sun = {255, 255, 255};
  good.cloud_max_alpha = zref::fx16{0};
  good.sun_energy = zref::fx16{0};

  // violating: the pre-amendment flat-oval authoring (each layer its own)
  zref::sky::SkySet bad = good;
  bad.band_lower_horizon = bad.band_lower_top = {214, 116, 82};
  bad.band_upper_bottom = bad.band_upper_top = {88, 62, 110};
  bad.cap = {26, 28, 74};
  bad.under = {38, 25, 29};

  zref::render::RenderResources res;
  res.sky_sets.push_back({2, good});
  res.sky_sets.push_back({3, bad});
  // the orientation fixture's pitch matrix: cap in the upper half, under in
  // the lower half — one column crosses all three joins
  const int32_t m[16] = {13, 0, 0, 0, 0, -26, 10, 0, 0, 0, 13, 0, 0, 0, 0, 1 << 16};

  auto max_vstep = [&](uint32_t set_handle) {
    zref::render::SoftwareRenderer rend;
    zref::render::RenderCanvas canvas;
    const auto pkt = rtest::seal_frame(13, [&](zhao::ZhaoFrameBuilder& b) {
      auto sv = zhao_abi::zhao_sample_set_view();
      sv.payload.view_id = 0;
      sv.payload.view_projection = rtest::ortho_topdown(2048);
      std::vector<uint8_t> v1;
      zhao_abi::zhao_pack_set_view(sv, v1);
      b.append_record(v1);
      auto sk = zhao_abi::zhao_sample_draw_sky();
      sk.payload.sky_set = set_handle;
      sk.payload.rot_proj[0] = rtest::mat(m);
      sk.payload.rot_proj[1] = rtest::mat(m);
      sk.payload.drum_yaw = 0;
      sk.payload.viewport_mask = 1;
      sk.payload.flags = zref::sky::kLayerUnder | zref::sky::kLayerCap;
      std::vector<uint8_t> v2;
      zhao_abi::zhao_pack_draw_sky(sk, v2);
      b.append_record(v2);
    });
    check(rend.render_frame(pkt, 0, canvas, res).status == zhao_abi::ZH_ABI_OK,
          "continuity fixture frame renders");
    // expand 565 back to 888 (the reel's unpack law) and measure vertical steps
    auto ch = [&](uint32_t x, uint32_t y, int c) {
      const uint16_t p = rtest::px(canvas, 0, x, y, 384);
      const uint32_t r5 = (p >> 11) & 0x1F, g6 = (p >> 5) & 0x3F, b5 = p & 0x1F;
      const uint32_t v[3] = {(r5 * 255 + 15) / 31, (g6 * 255 + 31) / 63, (b5 * 255 + 15) / 31};
      return static_cast<int32_t>(v[c]);
    };
    // §1: an uncovered direction legally falls back to the flat background
    // clear — steps against the background are the fallback boundary, not a
    // layer join. Exclude pixel pairs touching the dithered background.
    const zref::sky::SkySet& set = set_handle == 2 ? good : bad;
    auto is_bg = [&](uint32_t x, uint32_t y) {
      const uint8_t B = kBayerRow(y, x);
      const uint32_t r5 = (static_cast<uint32_t>(set.background.r) * 31 + B * 16 + 8) / 255;
      const uint32_t g6 = (static_cast<uint32_t>(set.background.g) * 63 + B * 32 + 16) / 255;
      const uint32_t b5 = (static_cast<uint32_t>(set.background.b) * 31 + B * 16 + 8) / 255;
      return rtest::px(canvas, 0, x, y, 384) == static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
    };
    int32_t worst = 0;
    uint32_t wx = 0, wy = 0;
    for (uint32_t x = 0; x < 384; x += 8) {
      for (uint32_t y = 1; y < 240; ++y) {
        if (is_bg(x, y) || is_bg(x, y - 1)) continue;
        for (int c = 0; c < 3; ++c) {
          int32_t d = ch(x, y, c) - ch(x, y - 1, c);
          if (d < 0) d = -d;
          if (d > worst) {
            worst = d;
            wx = x;
            wy = y;
          }
        }
      }
    }
    std::printf("render_sky: seam scan set %u worst %d at (%u,%u)\n", set_handle, worst, wx, wy);
    return worst;
  };

  const int32_t good_step = max_vstep(2);
  const int32_t bad_step = max_vstep(3);
  std::printf("render_sky: seam continuity — conforming max step %d, violating %d\n", good_step,
              bad_step);
  // banding quantum of the conforming ramp: <= ceil(78/8) = 10 per row step in
  // 888, plus the 565 quantisation + ordered dither allowance (~17 for a 5-bit
  // channel). 32 is the ceiling that separates "gradient banding" from "seam".
  check(good_step <= 32, "conforming sky: no join step exceeds the banding quantum (S1.2)");
  check(bad_step > 32, "violating sky: the oval seam IS detected (the assertion can fire)");
}

}  // namespace

int main() {
  test_layer_census();
  test_band_geometry();
  test_cloud_vertex_alpha_law();
  test_sun_centre_alpha();
  test_sun_visible_on_canvas();
  test_sky_orientation();
  test_scroll_determinism();
  test_rot_proj_validation();
  test_fallback_clear();
  test_seam_continuity();
  if (failures == 0) std::printf("render_sky: all green\n");
  return failures == 0 ? 0 : 1;
}
