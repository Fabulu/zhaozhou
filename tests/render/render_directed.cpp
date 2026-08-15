// render_directed.cpp — W3.5 directed renderer tests: hand-computed
// fixed-point projections (integer math shown in the comments), per-command
// behaviour, resolve/CRC laws, the mode latch, validation gating, and the
// no-float grep-audit of the render path.
//
// Law: spec/qformats.md §2/§3/§4/§8 (projection arithmetic),
//      spec/video_rules.md §1.1/§3/§3.1/§4 (modes, layout, Duo map, CRC),
//      spec/commands.zidl (record layouts via the generated pack helpers),
//      spec/capture_format.md §3.2 (fail-safe validation first).

#include "render_helpers.hpp"
#include "zrender/internal.hpp"  // white-box: project_vertex / WorkSurface

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

fs::path repo_root() {
  fs::path dir = fs::current_path();
  for (int i = 0; i < 5 && !fs::exists(dir / "spec" / "commands.zidl"); ++i)
    dir = dir.parent_path();
  return dir;
}

zref::mat4fx to_zref(const zhao_abi::ZhMat4fx& m) {
  zref::mat4fx r;
  const int32_t* p = &m.m00;
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b) r.m[a][b] = zref::fx16{p[a * 4 + b]};
  return r;
}

// ---- 1. hand-computed projection (qformats §2/§3/§8) -----------------------
//
// vp = [[2,0,0,0],[0,2,0,0],[0,0,1,0],[0,0,1,0]] (fx16), world (0.75,
// -0.1875, 3.0) = raw (49152, -12288, 196608), viewport 256x192 at (0,0):
//
//   clip.x: row0 . v = 2*49152 << 16 >> 16 = 98304            (exact, s128 row sum)
//   clip.w: row3 . v = 1*196608 = 196608                       (exact)
//   ndc.x  = round(98304*65536/196608) = 98304/3 = 32768       (exact /3)
//   ndc.y  = round(-24576*65536/196608) = -8192                (exact /3;
//            y fixture uses -0.1875: clip.y = 2*(-12288) = -24576)
//   screen.x = ndc.x*halfW + cx: (32768*8388608 + 128<<32)>>16 = 192<<16
//            -> S12.8 = 192<<8 = 49152                         (exact)
//   screen.y = -8192*6291456 + 96<<32 = -12*2^32 + 96*2^32 = 84*2^32
//            -> >>16 = 84<<16 -> S12.8 = 84<<8 = 21504         (exact)
//   depth   = round(65536*65536/196608) = round(21845.33) = 21845 (§4 rhu)
void test_projection_hand_computed() {
  const zref::mat4fx vp = to_zref(rtest::persp2x());
  const zref::render::Viewport vpp{0, 0, 256, 192};
  const zref::render::ProjOut p = zref::render::project_vertex(
      vp, vpp, zref::fx16{49152}, zref::fx16{-12288}, zref::fx16{196608}, nullptr);
  check(p.in, "vertex in front of the eye is `in`");
  check(p.s.x == 192 << 8, "screen x = 192 px (S12.8 = 49152)");
  check(p.s.y == 84 << 8, "screen y = 84 px (S12.8 = 21504)");
  check(p.s.d == 21845, "Q16.16 1/w depth = 21845");

  // behind the eye: w = z <= 0 -> culled (Phase-3 near-plane law)
  const zref::render::ProjOut q =
      zref::render::project_vertex(vp, vpp, zref::fx16{0}, zref::fx16{0}, zref::fx16{0}, nullptr);
  check(!q.in, "w = 0 vertex is rejected");

  // guard band (§8): a far-off vertex clamps to +-2048 px, never wraps
  // w = z = 1/16 -> ndc.y = 2*0.75*16 = 24 -> 96 + 24*96 = 2400 px > 2048
  const zref::render::ProjOut g = zref::render::project_vertex(
      vp, vpp, zref::fx16{49152}, zref::fx16{49152}, zref::fx16{1 << 12}, nullptr);
  check(g.in && g.s.y == 2048 << 8, "off-canvas vertex clamps to +2048 px");
}

// ---- 2. validation gates everything (capture_format §3.2) ------------------
// qformats §8 fill rule: two triangles sharing an edge must cover every pixel
// EXACTLY ONCE — no holes, no double-fill. This is the Phase-4/5 SymbiYosys
// property, asserted here on the software raster because the rule it depends
// on is frozen now.
//
// Fixture: the 10x10 pixel square [2,12]x[2,12] split along its MAIN
// DIAGONAL. That split is the load-bearing choice: the diagonal is y = x, so
// the centre of every diagonal pixel (k+0.5, k+0.5) lies EXACTLY on the
// shared edge and E' is exactly 0 there. The top-left bias is supposed to
// hand each such pixel to exactly one of the two triangles.
//
// With the pre-fix strict `E' + bias > 0`, both sides reject it (0 > 0 on the
// top-left owner, -1 > 0 on the other) and the diagonal is a line of 10
// holes; non-top-left edges additionally drop their strictly-interior E' = 1
// rank. `>= 0` is the D3D rule the spec sentence already named.
void test_shared_edge_exactly_once() {
  using namespace zref::render;
  constexpr int32_t kLo = 2, kHi = 12, kDim = 16;
  auto sv = [](int32_t px, int32_t py) {
    ScreenV v;
    v.x = px << 8;  // S 12.8, exactly on the pixel grid
    v.y = py << 8;
    return v;
  };
  const ScreenV v00 = sv(kLo, kLo), v10 = sv(kHi, kLo), v11 = sv(kHi, kHi), v01 = sv(kLo, kHi);

  // coverage mask of one triangle drawn alone (background 0, fill 255)
  auto cover = [&](const ScreenV& a, const ScreenV& b, const ScreenV& c) {
    WorkSurface s;
    s.reset(kDim, kDim, zref::sky::SkyColor{0, 0, 0});
    const Viewport vp{0, 0, kDim, kDim};
    TriMode m;
    m.depth_test = false;
    m.depth_write = false;
    m.use_fixed_depth = true;
    m.fixed_depth = 1;
    raster_tri(s, vp, a, b, c, 255, 255, 255, m);
    std::vector<uint8_t> mask(static_cast<size_t>(kDim) * kDim, 0);
    for (size_t i = 0; i < mask.size(); ++i) mask[i] = s.rgb[i * 3] != 0 ? 1 : 0;
    return mask;
  };

  // both halves keep the same (positive-area) winding, so neither is flipped
  const std::vector<uint8_t> t1 = cover(v00, v10, v11);  // upper-right half
  const std::vector<uint8_t> t2 = cover(v00, v11, v01);  // lower-left half

  uint32_t holes = 0, doubles = 0, covered = 0, outside = 0, diag_covered = 0;
  for (int32_t y = 0; y < kDim; ++y)
    for (int32_t x = 0; x < kDim; ++x) {
      const size_t i = static_cast<size_t>(y) * kDim + x;
      const int n = t1[i] + t2[i];
      // pixel centre (x+0.5, y+0.5) is inside the square iff x,y in [2,11]
      const bool in_square = x >= kLo && x < kHi && y >= kLo && y < kHi;
      if (in_square) {
        if (n == 0) ++holes;
        if (n > 1) ++doubles;
        if (n == 1) ++covered;
        if (x == y && n >= 1) ++diag_covered;
      } else if (n != 0) {
        ++outside;
      }
    }
  std::printf("  shared edge: covered %u holes %u doubles %u diag %u/10\n", covered, holes, doubles,
              diag_covered);
  check(holes == 0, "shared-edge split leaves no holes");
  check(doubles == 0, "shared-edge split double-fills nothing");
  check(covered == 100, "shared-edge split covers all 100 pixels exactly once");
  check(diag_covered == 10, "every pixel centre ON the shared edge is claimed by one side");
  check(outside == 0, "no pixel outside the square is touched");
}

void test_validation_gate() {
  zref::render::SoftwareRenderer rend;
  zref::render::RenderCanvas canvas;
  zref::render::RenderResources res;
  std::vector<uint8_t> bad = rtest::seal_frame(1, [](zhao::ZhaoFrameBuilder&) {});
  bad[0] = 'X';  // corrupt magic
  const zref::render::RenderResult r = rend.render_frame(bad, 0, canvas, res);
  check(r.status == zhao_abi::ZH_ABI_BAD_MAGIC, "bad magic -> BAD_MAGIC");
  check(r.commands_executed == 0 && r.canvas_crc32c == 0, "rejected packet draws nothing");
}

// ---- 3. DrawForm: pattern, scale, wall clamp --------------------------------
void test_draw_form() {
  zref::render::FormPattern form;
  for (int i = 0; i < 64; ++i) {
    form.mask[i] = 1;
    form.rgb[i * 3 + 0] = 255;
    form.rgb[i * 3 + 1] = 0;
    form.rgb[i * 3 + 2] = 0;
  }
  form.rgb[(4 * 8 + 4) * 3 + 0] = 255;  // centre texel white
  form.rgb[(4 * 8 + 4) * 3 + 1] = 255;
  form.rgb[(4 * 8 + 4) * 3 + 2] = 255;

  zref::render::RenderResources res;
  res.forms.push_back({7, form});
  // world z = 0 keeps the marker at the view centre row; screen-space half
  // size 32 px (flags b1)
  res.transforms.push_back({8, zref::render::FormTransform{0, 0, 0, 32 << 16}});
  // x = 30 m -> 192 + 180 = 372 px, past the 384-px wall -> clamped to 351
  zref::render::FormTransform off = {30 << 16, 0, 0, 32 << 16};
  res.transforms.push_back({9, off});

  const auto body = [&](zhao::ZhaoFrameBuilder& b) {
    auto spc = zhao_abi::zhao_sample_set_presentation_contract();
    spc.payload.mode = zhao_abi::VIDEO_Z60;
    std::vector<uint8_t> v;
    zhao_abi::zhao_pack_set_presentation_contract(spc, v);
    b.append_record(v);

    auto sv = zhao_abi::zhao_sample_set_view();
    sv.payload.view_id = 0;
    sv.payload.view_projection = rtest::ortho_topdown(2048);
    std::vector<uint8_t> v2;
    zhao_abi::zhao_pack_set_view(sv, v2);
    b.append_record(v2);

    auto df = zhao_abi::zhao_sample_draw_form();
    df.payload.form = 7;
    df.payload.transform = 8;
    df.payload.viewport_mask = 1;
    df.payload.flags = 0x0002;  // screen-space size: half = 32 px
    std::vector<uint8_t> v3;
    zhao_abi::zhao_pack_draw_form(df, v3);
    b.append_record(v3);

    auto df2 = zhao_abi::zhao_sample_draw_form();
    df2.payload.form = 7;
    df2.payload.transform = 9;  // centre 372 px -> wall clamp at 351
    df2.payload.viewport_mask = 1;
    df2.payload.flags = 0x0002;
    std::vector<uint8_t> v4;
    zhao_abi::zhao_pack_draw_form(df2, v4);
    b.append_record(v4);
  };
  const std::vector<uint8_t> pkt = rtest::seal_frame(5, body);

  zref::render::SoftwareRenderer rend;
  zref::render::RenderCanvas canvas;
  const zref::render::RenderResult r = rend.render_frame(pkt, 0, canvas, res);
  check(r.status == zhao_abi::ZH_ABI_OK, "DrawForm frame renders");
  // ortho_topdown: world x -> ndc x/32 -> px = 192 + x*6 (384-wide view);
  // marker centre (0,0) lands at (192,120) (240-high view: cy = 120). Quad
  // [160..224] x [88..152]: (192,120) = centre texel, (170,100) = body.
  const uint16_t red565 = (31u << 11) | (0u << 5) | 0u;  // 255,0,0 exact 565
  const uint16_t white565 = (31u << 11) | (63u << 5) | 31u;
  check(rtest::px(canvas, 0, 192, 120, 384) == white565, "marker centre texel is white");
  check(rtest::px(canvas, 0, 170, 100, 384) == red565, "marker body texel is red");
  check(rtest::px(canvas, 0, 150, 120, 384) != red565, "pixel left of the quad untouched");
  // wall clamp: centre 372 px clamps to 384-1-32 = 351 -> quad [319..383]
  check(rtest::px(canvas, 0, 350, 120, 384) == red565,
        "wall-clamped marker visible at the right wall");
  check(rtest::px(canvas, 0, 318, 120, 384) != red565,
        "wall-clamped marker starts at the clamp edge");
}

// ---- 4. DrawPopulation: point + triangle sprites, depth-tested -------------
void test_draw_population() {
  zref::render::Population pop;
  pop.parts.push_back({0, 0, 0, 64, 0, 255, 0});          // 4x4 px point
  pop.parts.push_back({(6 << 16), 0, 0, 64, 0, 0, 255});  // triangle
  zref::render::RenderResources res;
  res.populations.push_back({3, pop});

  const auto body = [&](zhao::ZhaoFrameBuilder& b) {
    auto sv = zhao_abi::zhao_sample_set_view();
    sv.payload.view_id = 0;
    sv.payload.view_projection = rtest::ortho_topdown(2048);
    std::vector<uint8_t> v;
    zhao_abi::zhao_pack_set_view(sv, v);
    b.append_record(v);
    auto dp = zhao_abi::zhao_sample_draw_population();
    dp.payload.population = 3;
    dp.payload.viewport_mask = 1;
    dp.payload.flags = 0x0003;  // points + triangles
    std::vector<uint8_t> v2;
    zhao_abi::zhao_pack_draw_population(dp, v2);
    b.append_record(v2);
  };
  zref::render::SoftwareRenderer rend;
  zref::render::RenderCanvas canvas;
  const zref::render::RenderResult r =
      rend.render_frame(rtest::seal_frame(6, body), 0, canvas, res);
  check(r.status == zhao_abi::ZH_ABI_OK, "population frame renders");
  // point sprite: centre (192,120) 4x4 -> px [190..193] x [118..121]
  const uint16_t green565 = (0u << 11) | (63u << 5) | 0u;
  check(rtest::px(canvas, 0, 191, 119, 384) == green565, "point sprite pixel green");
  check(rtest::px(canvas, 0, 189, 119, 384) == 0x0000,
        "pixel outside point sprite untouched (black clear)");
  // triangle: centre x = 192 + 6*6 = 228 (1 world m = 6 px in a 384-wide
  // view at scale 1/32); the fan spans +-3 px horizontally
  const uint16_t blue565 = (0u << 11) | (0u << 5) | 31u;
  bool tri_found = false;
  for (int y = 112; y <= 122 && !tri_found; ++y)
    for (int x = 220; x <= 236; ++x)
      if (rtest::px(canvas, 0, static_cast<uint32_t>(x), static_cast<uint32_t>(y), 384) ==
          blue565) {
        tri_found = true;
        break;
      }
  check(tri_found, "triangle sprite pixel blue");
}

// ---- 5. EmitAudioEvent -> trigger list + tone mapping ----------------------
void test_audio_events() {
  const auto body = [&](zhao::ZhaoFrameBuilder& b) {
    for (uint32_t i = 0; i < 2; ++i) {
      auto e = zhao_abi::zhao_sample_emit_audio_event();
      e.payload.event_id = 100 + i;
      e.payload.pan_fx = static_cast<int16_t>(-8192 * (1 + static_cast<int>(i)));
      e.payload.gain = static_cast<uint16_t>(0xC000 + i);
      e.payload.sample_handle = i;
      e.payload.timestamp = 42;
      std::vector<uint8_t> v;
      zhao_abi::zhao_pack_emit_audio_event(e, v);
      b.append_record(v);
    }
  };
  zref::render::SoftwareRenderer rend;
  zref::render::RenderCanvas canvas;
  zref::render::RenderResources res;
  const zref::render::RenderResult r =
      rend.render_frame(rtest::seal_frame(7, body), 0, canvas, res);
  check(r.audio_events.size() == 2, "two audio triggers recorded");
  if (r.audio_events.size() == 2) {
    check(r.audio_events[0].event_id == 100 && r.audio_events[1].event_id == 101,
          "event ids in stream order");
    check(r.audio_events[0].pan_fx == -8192 && r.audio_events[1].pan_fx == -16384,
          "pan lanes carried verbatim");
    check(r.audio_events[0].gain == 0xC000, "gain carried verbatim");
    check(r.audio_events[0].timestamp == 42, "timestamp carried verbatim");
  }
  zref::ToneId t;
  check(zref::render::tone_id_for(0, &t) && t == zref::ToneId::TONE_A4 &&
            zref::render::tone_id_for(1, &t) && t == zref::ToneId::TONE_A5 &&
            zref::render::tone_id_for(2, &t) && t == zref::ToneId::TONE_C4 &&
            !zref::render::tone_id_for(3, &t),
        "sample_index -> frozen tone table (audio_rules §4)");
}

// ---- 6. resolve: dither vectors + the two CRC laws -------------------------
void test_resolve_and_crc() {
  // a uniform 128-gray 4x1 strip: channel steps straddle the 565 threshold
  // and the Bayer offsets must alternate the quantized value
  uint8_t rgb[4 * 3];
  for (int i = 0; i < 4; ++i) {
    rgb[i * 3 + 0] = 128;
    rgb[i * 3 + 1] = 128;
    rgb[i * 3 + 2] = 128;
  }
  uint8_t out[8];
  zref::render::resolve_rgb565(rgb, 4, 1, out);
  // hand law (resolve.cpp): r5 = (128*31 + B*16+8)/255.
  //  B=0: (3968+8)/255  = 15 (floor 15.60)  -> g6 B=0: (8064+16)/255 = 31
  //  B=8: (3968+136)/255= 16 (16.07)        -> g6 B=8: (8064+272)/255 = 32
  //  B=2: (3968+40)/255 = 15 (15.72)
  //  B=10: (3968+168)/255 = 16 (16.21)
  const auto r5_of = [](uint8_t B) { return (128 * 31 + B * 16 + 8) / 255; };
  const auto g6_of = [](uint8_t B) { return (128 * 63 + B * 32 + 16) / 255; };
  const uint8_t bayer0[4] = {0, 8, 2, 10};  // row 0 of the matrix
  for (int x = 0; x < 4; ++x) {
    const uint16_t pxv = static_cast<uint16_t>(out[x * 2] | (out[x * 2 + 1] << 8));
    const uint16_t want = static_cast<uint16_t>((r5_of(bayer0[x]) << 11) | (g6_of(bayer0[x]) << 5) |
                                                r5_of(bayer0[x]));
    check(pxv == want, "dithered 565 matches the hand law");
  }

  // CRC laws (video_rules §3/§4): Z60 canvas == displayed; Duo displayed
  // embeds the 48 border rows, canvas does not
  std::vector<uint8_t> slot(zref::render::kSlotBytes, 0);
  for (size_t i = 0; i < 100; ++i) slot[i] = static_cast<uint8_t>(i);
  const uint32_t c_canvas = zref::render::canvas_crc32c(zhao_abi::VIDEO_DUO, slot.data());
  const uint32_t c_disp = zref::render::displayed_crc32c(zhao_abi::VIDEO_DUO, slot.data());
  check(c_canvas != c_disp, "Duo canvas CRC != displayed CRC");
  check(zref::render::canvas_crc32c(zhao_abi::VIDEO_Z60, slot.data()) ==
            zref::render::displayed_crc32c(zhao_abi::VIDEO_Z60, slot.data()),
        "Z60 canvas CRC == displayed CRC");
  // displayed stream is 512*240*2 bytes: border(24) + canvas(192) + border(24)
  // rows; recompute by hand with zhao_crc32c chunks
  uint32_t crc = 0;
  const uint8_t border[512 * 2] = {};
  for (int i = 0; i < 24; ++i) crc = zhao_abi::zhao_crc32c(crc, border, sizeof(border));
  for (uint32_t row = 0; row < 192; ++row)
    crc = zhao_abi::zhao_crc32c(crc, slot.data() + static_cast<size_t>(row) * 512 * 2, 512 * 2);
  for (int i = 0; i < 24; ++i) crc = zhao_abi::zhao_crc32c(crc, border, sizeof(border));
  check(crc == c_disp, "displayed CRC law recomputed by hand matches");
}

// ---- 7. mode latch law (video_rules §1.1) ----------------------------------
void test_mode_latch() {
  zref::render::FormPattern form;
  for (int i = 0; i < 64; ++i) {
    form.mask[i] = 1;
    form.rgb[i * 3 + 0] = 255;
  }
  zref::render::RenderResources res;
  res.forms.push_back({7, form});
  res.transforms.push_back({8, zref::render::FormTransform{0, 0, 0, 16 << 16}});
  const auto body = [&](zhao::ZhaoFrameBuilder& b) {
    auto spc = zhao_abi::zhao_sample_set_presentation_contract();
    spc.payload.mode = zhao_abi::VIDEO_DUO;
    std::vector<uint8_t> v;
    zhao_abi::zhao_pack_set_presentation_contract(spc, v);
    b.append_record(v);
    auto sv = zhao_abi::zhao_sample_set_view();
    sv.payload.view_id = 0;
    sv.payload.view_projection = rtest::ortho_topdown(2048);
    std::vector<uint8_t> v2;
    zhao_abi::zhao_pack_set_view(sv, v2);
    b.append_record(v2);
    auto df = zhao_abi::zhao_sample_draw_form();
    df.payload.form = 7;
    df.payload.transform = 8;
    df.payload.viewport_mask = 1;
    df.payload.flags = 0x0002;
    std::vector<uint8_t> v3;
    zhao_abi::zhao_pack_draw_form(df, v3);
    b.append_record(v3);
  };

  zref::render::SoftwareRenderer rend;
  zref::render::RenderCanvas canvas;
  const zref::render::RenderResult f1 =
      rend.render_frame(rtest::seal_frame(1, body), 0, canvas, res);
  // frame 1 still renders the RESET mode Z60: 384x240 canvas, marker at
  // (192,120); a Duo render would have written a 512x192 canvas instead
  const uint16_t red565 = 31u << 11;
  check(f1.status == zhao_abi::ZH_ABI_OK && rend.latched_mode() == zhao_abi::VIDEO_DUO,
        "contract latched for the NEXT frame");
  check(rtest::px(canvas, 0, 192, 120, 384) == red565,
        "frame 1 rendered under the reset mode (Z60 geometry)");
  const zref::render::RenderResult f2 =
      rend.render_frame(rtest::seal_frame(2, body), 0, canvas, res);
  check(f2.status == zhao_abi::ZH_ABI_OK, "frame 2 renders");
  // frame 2 is Duo: view 0 is 256x192, so the marker centre is (128,96)
  // (the Z60 frame put it at (192,120) in its 384x240 viewport)
  check(rtest::px(canvas, 0, 128, 96, 512) == red565,
        "frame 2 rendered Duo geometry (marker at 128,96)");
  check(f2.displayed_crc32c != f2.canvas_crc32c, "Duo displayed CRC carries the border rows");
}

// ---- 8. the no-float audit (plan W3.5) --------------------------------------
//
// Scans every render-path source (zrender/, zsky/, the two public headers)
// for the tokens `float`/`double` OUTSIDE line comments. The __int128 /
// fixed-point law (qformats.md) admits no host float in the render path.
void test_no_float_audit() {
  const fs::path root = repo_root();
  const std::vector<fs::path> files = {
      root / "reference" / "include" / "zref" / "zref_render.hpp",
      root / "reference" / "include" / "zref" / "zref_sky.hpp",
      root / "reference" / "src" / "zrender" / "internal.hpp",
      root / "reference" / "src" / "zrender" / "rast.cpp",
      root / "reference" / "src" / "zrender" / "render_frame.cpp",
      root / "reference" / "src" / "zrender" / "terrain.cpp",
      root / "reference" / "src" / "zrender" / "sprites.cpp",
      root / "reference" / "src" / "zrender" / "resolve.cpp",
      root / "reference" / "src" / "zsky" / "emit_layers.cpp",
  };
  for (const fs::path& f : files) {
    std::ifstream in(f);
    if (!in) {
      check(false, ("audit: missing source " + f.string()).c_str());
      continue;
    }
    std::string line;
    int lineno = 0;
    while (std::getline(in, line)) {
      ++lineno;
      const size_t comment = line.find("//");
      const std::string code = comment == std::string::npos ? line : line.substr(0, comment);
      if (code.find("float") != std::string::npos || code.find("double") != std::string::npos) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "float token in %s:%d", f.filename().string().c_str(),
                      lineno);
        check(false, buf);
      }
    }
  }
}

}  // namespace

int main() {
  test_projection_hand_computed();
  test_shared_edge_exactly_once();
  test_validation_gate();
  test_draw_form();
  test_draw_population();
  test_audio_events();
  test_resolve_and_crc();
  test_mode_latch();
  test_no_float_audit();
  if (failures == 0) std::printf("render_directed: all green\n");
  return failures == 0 ? 0 : 1;
}
