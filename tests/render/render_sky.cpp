// render_sky.cpp — W3.5 sky tests: emit_layers layer census, the §1.1
// cloud vertex-alpha law, tick-exact scroll determinism (T vs T+3840),
// the rotation-only rot_proj validation, and the fallback flat-clear rule.
//
// Law: spec/sky_and_beams.md §1 (assembly, fallback, "no pixel left
// unwritten"), §1.1 (layer table: counts, UV laws, scroll formulas, cloud
// vertex alpha), §6 (the ZRef preview functions); spec/qformats.md §3/§4.

#include "render_helpers.hpp"

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
  // §1.1 layer table: 2 bands x 48 cols x 8 rows x 2 tris = 1536 (always),
  // + cap 16 + under 2 + cloud 128 + sun 4 (flag-gated)
  const zref::sky::SkySet s = test_set();
  const uint8_t all =
      zref::sky::kLayerUnder | zref::sky::kLayerCloud | zref::sky::kLayerSun | zref::sky::kLayerCap;
  const auto prims = zref::sky::emit_layers(s, 0, zref::angle16{0}, all);
  check(prims.size() == 1536 + 16 + 2 + 128 + 4, "all-layers emission = 1686 primitives");
  const auto bands_only = zref::sky::emit_layers(s, 0, zref::angle16{0}, 0);
  check(bands_only.size() == 1536, "bands are the always-on backdrop (1536)");
  size_t per_layer[6] = {0, 0, 0, 0, 0, 0};
  for (const auto& p : prims) ++per_layer[static_cast<int>(p.layer)];
  check(per_layer[0] == 768 && per_layer[1] == 768, "48x8x2 per band");
  check(per_layer[2] == 16 && per_layer[3] == 2 && per_layer[4] == 128 && per_layer[5] == 4,
        "cap 16 / under 2 / cloud 128 / sun 4");
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

  // a rotation-only projection that maps the drum into view: pitch 45 deg
  // about X composed with scale 1/5120 (the validation law checks the ZERO
  // pattern — translation column and bottom row — scale stays legal). A
  // pure uniform scale would put the horizontal cap/under planes edge-on
  // (degenerate), so the tilt is load-bearing for the fixture.
  // raw entries: value 12 ~ 65536/5120, value 9 ~ 12*0.7071 (one 45-deg
  // factor folded in, hand-rounded to integer raws)
  const int32_t m[16] = {12, 0, 0, 0, 0, 9, -9, 0, 0, 9, 9, 0, 0, 0, 0, 1 << 16};

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
    zhao_abi::ZhMat4fx bad = rtest::mat(m);
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
    const zref::render::RenderResult r =
        rend.render_frame(sky_pkt(rtest::mat(m), 2), 0, canvas, res);
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
  }
}

}  // namespace

int main() {
  test_layer_census();
  test_cloud_vertex_alpha_law();
  test_scroll_determinism();
  test_rot_proj_validation();
  test_fallback_clear();
  if (failures == 0) std::printf("render_sky: all green\n");
  return failures == 0 ? 0 : 1;
}
