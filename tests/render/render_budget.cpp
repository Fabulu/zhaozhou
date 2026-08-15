// render_budget.cpp — W3.5 runtime budget: a 600-frame island sequence
// (sky + TerrainField + stamp + 33x33 heightfield + forms + population,
// Duo two views) must complete in < 60 s wall clock (plan W3.5 / risk R4:
// "explicit runtime budget test; failure = optimize the integer path, never
// loosen exactness"). W3.7 supplies the real demo; this synthetic height-
// field stands in until then. Wall-clock is TEST-side measurement only —
// the render path itself stays wall-clock-free (charter determinism).

#include "render_helpers.hpp"

#include "zfield/zfield.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>

namespace {

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

constexpr uint32_t kFrames = 600;

}  // namespace

int main() {
  zref::render::TerrainPatch patch = rtest::bump_patch(33, 33, 8, 12);
  zref::render::FormPattern wizard;
  for (int i = 0; i < 64; ++i) {
    wizard.mask[i] = static_cast<uint8_t>((i / 8) % 2 == (i % 8) % 2 ? 1 : 0);
    wizard.rgb[i * 3 + 0] = 240;
    wizard.rgb[i * 3 + 1] = 200;
    wizard.rgb[i * 3 + 2] = 60;
  }
  zref::render::Population sparks;
  for (int i = 0; i < 16; ++i)
    sparks.parts.push_back({(i - 8) << 14, (3 + (i % 4)) << 16, ((i % 5) - 2) << 16, 96, 255,
                            static_cast<uint8_t>(80 + i), 80});
  zref::sky::SkySet sky;
  sky.background = {6, 10, 20};
  sky.band_lower_horizon = {96, 128, 208};
  sky.band_lower_top = {150, 176, 236};
  sky.band_upper_bottom = {150, 176, 236};
  sky.band_upper_top = {236, 246, 255};
  sky.cap = {248, 250, 255};
  sky.under = {18, 28, 40};
  sky.cloud = {252, 252, 252};
  sky.sun = {255, 238, 196};
  sky.sun_dir_z = zref::fx16{-1 * 32768};

  const std::vector<uint8_t> earth_bytes = rtest::make_earth_prog();
  const zfield::DecodeResult prog = zfield::decode(earth_bytes.data(), earth_bytes.size());
  if (prog.error != zfield::DecodeError::kOk) {
    std::fprintf(stderr, "FAIL: earth program does not decode\n");
    return 1;
  }

  zref::render::RenderResources res;
  res.field_programs.push_back({5, &prog.prog});
  res.terrain_patches.push_back({44, &patch});
  res.materials.push_back({45, zref::render::Material{96, 128, 72}});
  res.forms.push_back({7, wizard});
  res.transforms.push_back({8, zref::render::FormTransform{-3 << 16, 2 << 16, -4 << 16, 12 << 16}});
  res.transforms.push_back({9, zref::render::FormTransform{3 << 16, 2 << 16, -4 << 16, 12 << 16}});
  res.populations.push_back({3, sparks});
  res.sky_sets.push_back({2, sky});

  const int32_t m0[16] = {2 << 16, 0, 0,       0, 0, -2 << 16, 0,       24 << 16,
                          0,       0, 1 << 16, 0, 0, 0,        1 << 16, 32 << 16};
  const int32_t m1[16] = {2 << 16, 0, 0,       0, 0, -2 << 16, 0,       28 << 16,
                          0,       0, 1 << 16, 0, 0, 0,        1 << 16, 40 << 16};
  const int32_t rs0[16] = {12, 0, 0, 0, 0, 9, -9, 0, 0, 9, 9, 0, 0, 0, 0, 1 << 16};
  const int32_t rs1[16] = {13, 0, 0, 0, 0, 8, -8, 0, 0, 8, 8, 0, 0, 0, 0, 1 << 16};

  zref::render::SoftwareRenderer rend;
  zref::render::RenderCanvas canvas;

  // per-frame packet (tick varies: age/phase + cloud scroll advance)
  const auto frame_packet = [&](uint32_t tick) {
    return rtest::seal_frame(tick, [&](zhao::ZhaoFrameBuilder& b) {
      auto spc = zhao_abi::zhao_sample_set_presentation_contract();
      spc.payload.mode = zhao_abi::VIDEO_DUO;
      std::vector<uint8_t> v0;
      zhao_abi::zhao_pack_set_presentation_contract(spc, v0);
      b.append_record(v0);
      auto sv0 = zhao_abi::zhao_sample_set_view();
      sv0.payload.view_id = 0;
      sv0.payload.view_projection = rtest::mat(m0);
      std::vector<uint8_t> v1;
      zhao_abi::zhao_pack_set_view(sv0, v1);
      b.append_record(v1);
      auto sv1 = zhao_abi::zhao_sample_set_view();
      sv1.payload.view_id = 1;
      sv1.payload.view_projection = rtest::mat(m1);
      std::vector<uint8_t> v2;
      zhao_abi::zhao_pack_set_view(sv1, v2);
      b.append_record(v2);
      auto sk = zhao_abi::zhao_sample_draw_sky();
      sk.payload.sky_set = 2;
      sk.payload.rot_proj[0] = rtest::mat(rs0);
      sk.payload.rot_proj[1] = rtest::mat(rs1);
      sk.payload.drum_yaw = static_cast<uint16_t>(tick & 0xFFFF);
      sk.payload.viewport_mask = 3;
      sk.payload.flags = 0x1F;
      std::vector<uint8_t> v3;
      zhao_abi::zhao_pack_draw_sky(sk, v3);
      b.append_record(v3);
      auto tf = zhao_abi::zhao_sample_terrain_field();
      tf.payload.program = 5;
      tf.payload.footprint.x0 = -6 * (1 << 16);
      tf.payload.footprint.y0 = -6 * (1 << 16);
      tf.payload.footprint.x1 = 6 * (1 << 16);
      tf.payload.footprint.y1 = 6 * (1 << 16);
      tf.payload.start_tick = 100;
      tf.payload.duration_ticks = 600;
      std::memset(tf.payload.parameters, 0, 64);
      tf.payload.parameters[2] = 2;  // p0 = 2.0 m (LE Q16.16)
      std::vector<uint8_t> v4;
      zhao_abi::zhao_pack_terrain_field(tf, v4);
      b.append_record(v4);
      auto dp = zhao_abi::zhao_sample_draw_procedural();
      dp.payload.program = 44;
      dp.payload.material = 45;
      dp.payload.transform = rtest::xform_identity();
      dp.payload.kind = zhao_abi::FORGE_HEIGHTFIELD_PATCH;
      std::vector<uint8_t> v5;
      zhao_abi::zhao_pack_draw_procedural(dp, v5);
      b.append_record(v5);
      auto df0 = zhao_abi::zhao_sample_draw_form();
      df0.payload.form = 7;
      df0.payload.transform = 8;
      df0.payload.viewport_mask = 1;
      df0.payload.flags = 2;
      std::vector<uint8_t> v6;
      zhao_abi::zhao_pack_draw_form(df0, v6);
      b.append_record(v6);
      auto df1 = zhao_abi::zhao_sample_draw_form();
      df1.payload.form = 7;
      df1.payload.transform = 9;
      df1.payload.viewport_mask = 2;
      df1.payload.flags = 2;
      std::vector<uint8_t> v7;
      zhao_abi::zhao_pack_draw_form(df1, v7);
      b.append_record(v7);
      auto dpop = zhao_abi::zhao_sample_draw_population();
      dpop.payload.population = 3;
      dpop.payload.viewport_mask = 3;
      dpop.payload.flags = 3;
      std::vector<uint8_t> v8;
      zhao_abi::zhao_pack_draw_population(dpop, v8);
      b.append_record(v8);
    });
  };

  const auto t0 = std::chrono::steady_clock::now();
  uint32_t crc_chain = 0;
  for (uint32_t f = 0; f < kFrames; ++f) {
    const uint32_t tick = 100 + f;
    const zref::render::RenderResult r = rend.render_frame(frame_packet(tick), 0, canvas, res);
    if (r.status != zhao_abi::ZH_ABI_OK) {
      std::fprintf(stderr, "FAIL: frame %u status %u\n", f, r.status);
      return 1;
    }
    crc_chain = zhao_abi::zhao_crc32c(crc_chain, &r.displayed_crc32c, 4);
  }
  const auto t1 = std::chrono::steady_clock::now();
  const double secs = std::chrono::duration<double>(t1 - t0).count();
  std::printf(
      "render_budget: %u frames in %.2f s (%.2f ms/frame), "
      "crc chain %08X\n",
      kFrames, secs, secs * 1000.0 / kFrames, crc_chain);
  check(secs < 60.0, "600-frame island sequence under 60 s");
  check(crc_chain != 0, "crc chain accumulated");
  if (failures == 0) std::printf("render_budget: all green\n");
  return failures == 0 ? 0 : 1;
}
