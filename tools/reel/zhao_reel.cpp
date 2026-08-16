// zhao_reel.cpp — deterministic frame-SEQUENCE capture for the deformation
// reel (Tier 1, plan RUN-20260814-2154 PLAN-deformation-reel.md).
//
// The single missing piece between the existing render chain and an animated
// GIF was driving the field programs across successive tick values and
// dumping N frames instead of one. This tool is that loop and nothing more:
// it builds one validated sealed frame packet per frame (exactly the way
// tests/render/*.cpp and the site's zshot.cpp build theirs), renders it
// through zref::render (the reference oracle; the ONE zfield interpreter is
// called inside — never reimplemented, charter §29-6), and writes each
// frame's RGB888 canvas plus the frame set's own palette.
//
// PALETTE LAW (QUEUE-animated-gifs-and-site.md): a shipped GIF must be
// palette-exact — encoded against the frame set's OWN <=256-colour palette
// with dithering off, never palettegen. The scenes here are AUTHORED into
// that budget (flat-band sky, no gradient cloud/sun layers) and this tool
// ENFORCES it: a subject whose frame set exceeds 256 unique colours fails
// the run with exit 3. Never weaken this into a quantisation step.
//
// Determinism: the render path is integer-only (zref); the per-frame
// parameter authoring below is integer/fixed-point arithmetic on constants
// (charter §29-7 — no host floats in the deterministic path; the only
// doubles are inside rtest::bump_patch, the same asset-authoring fixture
// every render test uses). Rendering the same subject twice must produce
// byte-identical frames; the driver script verifies by running twice.
//
// Output per subject, under <out>/<subject>/:
//   %04d.rgb    u32 w LE | u32 h LE | w*h*3 RGB888   (one per frame)
//   palette.rgb u32 count LE | count*3 RGB888        (first-seen order)
//   meta.txt    provenance the site quotes; includes per-frame CRC-32C
//
// Build (standalone, mirrors zhaozhou-site/update.ps1's zshot line):
//   g++ -std=c++17 -O2 -o zhao-reel tools/reel/zhao_reel.cpp \
//       reference/src/zref_frame.cpp reference/src/zref.cpp \
//       reference/src/zref_audio.cpp reference/src/zfield/zfield_decode.cpp \
//       reference/src/zfield/zfield_interpret.cpp \
//       reference/src/zrender/render_frame.cpp reference/src/zrender/rast.cpp \
//       reference/src/zrender/terrain.cpp reference/src/zrender/sprites.cpp \
//       reference/src/zrender/resolve.cpp reference/src/zsky/emit_layers.cpp \
//       -Ireference/include -Iruntime/include -Itests/render \
//       -Icompiler/tests/generated

#include "render_helpers.hpp"  // tests/render (packet/canvas helpers)

#include "impact_wave.hpp"  // compiler/tests/generated (TS-generated)
#include "wave_pool.hpp"    // compiler/tests/generated (TS-generated)
#include "zfield/zfield.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#define ZHAO_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define ZHAO_MKDIR(p) mkdir(p, 0755)
#endif

namespace {

std::string g_out;

// ---------------------------------------------------------------- output ----

bool write_rgb(const std::string& path, uint32_t w, uint32_t h, const std::vector<uint8_t>& rgb) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) {
    std::fprintf(stderr, "cannot open %s\n", path.c_str());
    return false;
  }
  uint8_t hdr[8];
  for (int i = 0; i < 4; ++i) hdr[i] = static_cast<uint8_t>(w >> (8 * i));
  for (int i = 0; i < 4; ++i) hdr[4 + i] = static_cast<uint8_t>(h >> (8 * i));
  fwrite(hdr, 1, 8, f);
  fwrite(rgb.data(), 1, rgb.size(), f);
  fclose(f);
  return true;
}

// RGB565 halfword -> RGB888 (bit replication via the exact rounding zshot
// uses; the honest expansion of what the console stores).
inline void unpack565(uint16_t p, uint8_t* out) {
  const uint32_t r5 = (p >> 11) & 0x1F, g6 = (p >> 5) & 0x3F, b5 = p & 0x1F;
  out[0] = static_cast<uint8_t>((r5 * 255 + 15) / 31);
  out[1] = static_cast<uint8_t>((g6 * 255 + 31) / 63);
  out[2] = static_cast<uint8_t>((b5 * 255 + 15) / 31);
}

std::vector<uint8_t> canvas_rgb(const zref::render::RenderCanvas& c, uint32_t slot, uint32_t w,
                                uint32_t h) {
  std::vector<uint8_t> rgb(static_cast<size_t>(w) * h * 3);
  for (uint32_t y = 0; y < h; ++y)
    for (uint32_t x = 0; x < w; ++x) {
      const uint16_t p = rtest::px(c, slot, x, y, w);
      unpack565(p, &rgb[(static_cast<size_t>(y) * w + x) * 3]);
    }
  return rgb;
}

// ------------------------------------------------------- palette tracking ---

// Unique colours across a subject's frame set, in first-seen scan order (a
// deterministic order: frames in sequence, pixels in raster order).
struct PaletteSet {
  std::unordered_set<uint32_t> seen;
  std::vector<uint8_t> order;  // 3 bytes per colour, first-seen

  void add_frame(const std::vector<uint8_t>& rgb) {
    for (size_t i = 0; i + 2 < rgb.size(); i += 3) {
      const uint32_t key = (static_cast<uint32_t>(rgb[i]) << 16) |
                           (static_cast<uint32_t>(rgb[i + 1]) << 8) | rgb[i + 2];
      if (seen.insert(key).second) {
        order.push_back(rgb[i]);
        order.push_back(rgb[i + 1]);
        order.push_back(rgb[i + 2]);
      }
    }
  }
  size_t count() const { return seen.size(); }
};

// -------------------------------------------------------- shared fixtures ---

// The flat-band dusk sky: the gallery's dusk identity, authored into a
// handful of exact colours so an animated frame set stays inside the 256-
// colour palette law. Gradient bands, the cloud sheet and the additive sun
// are the palette hogs of the still gallery (sky-dusk alone: 449 unique
// colours) — here each band is ONE colour (gradient endpoints equal) and
// cloud/sun layers are not requested.
zref::sky::SkySet flat_dusk_sky() {
  zref::sky::SkySet s;
  s.background = {24, 26, 70};
  s.band_lower_horizon = {214, 116, 82};  // flat: both endpoints equal
  s.band_lower_top = {214, 116, 82};
  s.band_upper_bottom = {88, 62, 110};
  s.band_upper_top = {88, 62, 110};
  s.cap = {26, 28, 74};
  s.under = {38, 25, 29};
  s.cloud = {255, 216, 196};  // unused (layer not requested)
  s.sun = {255, 206, 130};    // unused (layer not requested)
  s.cloud_max_alpha = zref::fx16{0};
  s.sun_energy = zref::fx16{0};
  s.sun_dir_x = zref::fx16{22938};
  s.sun_dir_z = zref::fx16{61604};
  return s;
}

zhao_abi::ZhMat4fx rot_only(int32_t s, int32_t c, int32_t sn) {
  const int32_t m[16] = {s, 0, 0, 0, 0, -c, sn, 0, 0, sn, c, 0, 0, 0, 0, 1 << 16};
  return rtest::mat(m);
}

// Pitched perspective camera: at (0, E, −D) looking down by θ (sin/cos in
// Q16.16), plus a raw additive Y-screen offset (screen shake). Rows built
// with exact s64 products, one >>16 rescale each — the same hand-matrix
// authoring every render test uses.
//   xv = x;  yv = c·(y−E) + s·(z+D);  zv = −s·(y−E) + c·(z+D)
//   x' = k·xv,  y' = −k·yv (+shake),  w = zv
// zsign +1: camera at (0,E,−D) looking +Z; zsign −1: camera at (0,E,+D)
// looking −Z (the sunlit side — the renderer's ONE light is (1,2,1)/√6, so
// the −Z viewpoint sees the lit slopes instead of the backlit silhouette).
zhao_abi::ZhMat4fx cam_pitch(int32_t k, int32_t eye_m, int32_t dist_m, int32_t ps, int32_t pc,
                             int32_t bias, int32_t zsign, int32_t shake_raw) {
  const auto mul16 = [](int64_t a, int64_t b) { return static_cast<int32_t>((a * b) >> 16); };
  const int32_t E = eye_m << 16, D = dist_m << 16;
  const int32_t kc = mul16(k, pc), ks = mul16(k, ps) * zsign;
  const int32_t wz = mul16(1 << 16, pc) * zsign;
  const int32_t y_w = mul16(k, mul16(pc, E) - mul16(ps, D)) + shake_raw;
  const int32_t w_w = mul16(ps, E) + mul16(pc, D);
  // screen-centre bias: y' += bias·w shifts the aim by bias in NDC (adding
  // bias × the w row into the y row — still one exact row sum per vertex)
  const int32_t m[16] = {k,
                         0,
                         0,
                         0,
                         0,
                         -kc + mul16(bias, -ps),
                         -ks + mul16(bias, wz),
                         y_w + mul16(bias, w_w),
                         0,
                         -ps,
                         wz,
                         w_w,
                         0,
                         -ps,
                         wz,
                         w_w};
  return rtest::mat(m);
}

// Q16.16 raw from milli-units (integer authoring, charter §29-7)
constexpr int32_t fxm(int64_t milli) {
  return static_cast<int32_t>((milli * 65536 + (milli >= 0 ? 500 : -500)) / 1000);
}

void put_param(uint8_t* blob, int lane, int32_t raw) {
  for (int b = 0; b < 4; ++b) blob[lane * 4 + b] = static_cast<uint8_t>(raw >> (8 * b));
}

// One field application: program handle + centre/params (all Q16.16 raw)
struct FieldSpec {
  uint32_t program;
  int32_t p[8];
  int32_t fx0, fz0, fx1, fz1;  // footprint
  uint32_t start_tick, duration;
};

struct StampSpec {
  uint32_t at_frame;  // stamped exactly once, on this frame
  int32_t cx, cz;     // world centre (fx16 raw)
  int32_t radius, ring_width;
  uint16_t strength;
};

// A debris particle: ballistic from the impact point, integer physics.
struct Debris {
  int32_t x0, z0;      // spawn (fx raw)
  int32_t vx, vy, vz;  // per-frame velocity (fx raw / frame)
  uint8_t size, r, g, b;
};

struct SceneSubject {
  const char* name;
  uint32_t frames;
  uint32_t step;  // ticks per frame
  std::vector<FieldSpec> fields;
  std::vector<StampSpec> stamps;
  // debris: spawned at spawn_frame, integer gravity fxm per frame^2
  uint32_t debris_spawn_frame = 0;
  int32_t debris_gravity = 0;  // fx raw per frame^2 (subtracted from vy)
  std::vector<Debris> debris;
  // screen shake: raw Y offsets per frame, starting at shake_frame
  uint32_t shake_frame = 0;
  std::vector<int32_t> shake;
  const char* note = "";
};

// ------------------------------------------------------------ scene render --

int render_scene(const SceneSubject& sub) {
  const uint32_t W = 384, H = 240;
  zref::render::TerrainPatch patch = rtest::bump_patch(161, 161, 12, 8);
  zref::render::Material mat{104, 122, 78};
  zref::sky::SkySet sky = flat_dusk_sky();

  const zfield::DecodeResult wave = zfield::decode(
      zfield_gen::wave_pool::kProgramBytes.data(), zfield_gen::wave_pool::kProgramBytesLen);
  const zfield::DecodeResult impact = zfield::decode(
      zfield_gen::impact_wave::kProgramBytes.data(), zfield_gen::impact_wave::kProgramBytesLen);
  if (wave.error != zfield::DecodeError::kOk || impact.error != zfield::DecodeError::kOk) {
    std::fprintf(stderr, "%s: field program decode failed\n", sub.name);
    return 2;
  }

  zref::render::RenderResources res;
  res.field_programs.push_back({6, &wave.prog});
  res.field_programs.push_back({7, &impact.prog});
  res.terrain_patches.push_back({44, &patch});
  res.materials.push_back({45, mat});
  zref::render::Population debris_pop;  // rebuilt per frame
  res.populations.push_back({3, debris_pop});

  res.sky_sets.push_back({2, sky});

  zref::render::SoftwareRenderer rend;  // ONE renderer: sheets persist
  zref::render::RenderCanvas canvas;
  PaletteSet pal;
  std::vector<uint32_t> frame_crcs;
  uint32_t seq_crc = 0;

  const std::string dir = g_out + "/" + sub.name;
  ZHAO_MKDIR(g_out.c_str());
  ZHAO_MKDIR(dir.c_str());

  for (uint32_t f = 0; f < sub.frames; ++f) {
    const uint32_t tick = f * sub.step;

    // debris population for this frame (integer ballistics)
    zref::render::Population* pop = nullptr;
    for (auto& e : res.populations)
      if (e.first == 3) pop = &e.second;
    pop->parts.clear();
    if (!sub.debris.empty() && f >= sub.debris_spawn_frame) {
      const int32_t t = static_cast<int32_t>(f - sub.debris_spawn_frame);
      for (const Debris& d : sub.debris) {
        // y = vy*t - g*t*(t-1)/2 ; alive while above the ground line
        const int64_t rise = static_cast<int64_t>(d.vy) * t;
        const int64_t fall = static_cast<int64_t>(sub.debris_gravity) * t * (t - 1) / 2;
        const int64_t y = (2 << 16) + rise - fall;  // launch from ~2 m
        if (y < (1 << 15)) continue;                // landed (below 0.5 m)
        pop->parts.push_back({d.x0 + d.vx * t, static_cast<int32_t>(y), d.z0 + d.vz * t, d.size,
                              d.r, d.g, d.b});
      }
    }

    // screen shake offset for this frame
    int32_t shake_raw = 0;
    if (!sub.shake.empty() && f >= sub.shake_frame && f < sub.shake_frame + sub.shake.size())
      shake_raw = sub.shake[f - sub.shake_frame];

    const auto pkt = rtest::seal_frame(tick, [&](zhao::ZhaoFrameBuilder& b) {
      auto spc = zhao_abi::zhao_sample_set_presentation_contract();
      spc.payload.mode = zhao_abi::VIDEO_Z60;
      spc.payload.view_count = 1;
      std::vector<uint8_t> v0;
      zhao_abi::zhao_pack_set_presentation_contract(spc, v0);
      b.append_record(v0);

      auto sv = zhao_abi::zhao_sample_set_view();
      sv.payload.view_id = 0;
      // pitch ~26 deg down: sin 28732, cos 58903 (hand Q16.16 constants);
      // sunlit-side camera (zsign −1), bias +0.25 NDC recentres the island
      sv.payload.view_projection =
          cam_pitch(127000, 14, 33, 28732, 58903, 14000, -1, shake_raw);
      std::vector<uint8_t> v1;
      zhao_abi::zhao_pack_set_view(sv, v1);
      b.append_record(v1);

      auto sk = zhao_abi::zhao_sample_draw_sky();
      sk.payload.sky_set = 2;
      sk.payload.rot_proj[0] = rot_only(14, 13, 5);
      sk.payload.rot_proj[1] = rot_only(14, 13, 5);
      sk.payload.drum_yaw = 0x0C00;
      sk.payload.cloud_scroll_u = 0;
      sk.payload.cloud_scroll_v = 0;
      sk.payload.viewport_mask = 1;
      sk.payload.flags = zref::sky::kLayerUnder | zref::sky::kLayerCap;  // flat bands only
      std::vector<uint8_t> v2;
      zhao_abi::zhao_pack_draw_sky(sk, v2);
      b.append_record(v2);

      for (const FieldSpec& fs : sub.fields) {
        auto tf = zhao_abi::zhao_sample_terrain_field();
        tf.payload.program = fs.program;
        tf.payload.footprint.x0 = fs.fx0;
        tf.payload.footprint.y0 = fs.fz0;
        tf.payload.footprint.x1 = fs.fx1;
        tf.payload.footprint.y1 = fs.fz1;
        tf.payload.start_tick = fs.start_tick;
        tf.payload.duration_ticks = fs.duration;
        std::memset(tf.payload.parameters, 0, 64);
        for (int k = 0; k < 8; ++k) put_param(tf.payload.parameters, k, fs.p[k]);
        std::vector<uint8_t> v3;
        zhao_abi::zhao_pack_terrain_field(tf, v3);
        b.append_record(v3);
      }

      for (const StampSpec& st : sub.stamps) {
        if (st.at_frame != f) continue;  // stamps land ONCE; the sheet persists
        auto s = zhao_abi::zhao_sample_surface_stamp();
        s.payload.patch = 44;
        s.payload.tag = 1;
        s.payload.strength = st.strength;
        s.payload.transform = rtest::xform_identity();
        s.payload.transform.tx = st.cx;
        s.payload.transform.ty = st.cz;
        s.payload.radius = st.radius;
        s.payload.ring_width = st.ring_width;
        std::vector<uint8_t> v4;
        zhao_abi::zhao_pack_surface_stamp(s, v4);
        b.append_record(v4);
      }

      auto dp = zhao_abi::zhao_sample_draw_procedural();
      dp.payload.program = 44;
      dp.payload.material = 45;
      dp.payload.transform = rtest::xform_identity();
      dp.payload.screen_error = 1 << 16;
      dp.payload.kind = zhao_abi::FORGE_HEIGHTFIELD_PATCH;
      std::vector<uint8_t> v5;
      zhao_abi::zhao_pack_draw_procedural(dp, v5);
      b.append_record(v5);

      if (!pop->parts.empty()) {
        auto dpop = zhao_abi::zhao_sample_draw_population();
        dpop.payload.population = 3;
        dpop.payload.viewport_mask = 1;
        dpop.payload.flags = 0x0003;
        std::vector<uint8_t> v6;
        zhao_abi::zhao_pack_draw_population(dpop, v6);
        b.append_record(v6);
      }
    });

    const zref::render::RenderResult r = rend.render_frame(pkt, 0, canvas, res);
    if (r.status != 0 || r.resource_misses != 0) {
      std::fprintf(stderr, "%s frame %u: status=%u resource_misses=%u — refusing to continue\n",
                   sub.name, f, r.status, r.resource_misses);
      return 2;
    }

    const std::vector<uint8_t> rgb = canvas_rgb(canvas, 0, W, H);
    pal.add_frame(rgb);
    frame_crcs.push_back(zhao_abi::zhao_crc32c(0, rgb.data(), rgb.size()));
    seq_crc = zhao_abi::zhao_crc32c(seq_crc, rgb.data(), rgb.size());

    char fp[600];
    std::snprintf(fp, sizeof(fp), "%s/%04u.rgb", dir.c_str(), f);
    if (!write_rgb(fp, W, H, rgb)) return 2;
  }

  // ---- the palette law: a shipped GIF must be palette-exact ----
  if (pal.count() > 256) {
    std::fprintf(stderr,
                 "%s: %zu unique colours (> 256) — the frame set cannot be encoded "
                 "palette-exactly. Re-author the scene; NEVER fall back to palettegen.\n",
                 sub.name, pal.count());
    return 3;
  }

  {
    std::vector<uint8_t> pl(4);
    const uint32_t n = static_cast<uint32_t>(pal.count());
    for (int i = 0; i < 4; ++i) pl[i] = static_cast<uint8_t>(n >> (8 * i));
    pl.insert(pl.end(), pal.order.begin(), pal.order.end());
    FILE* f = fopen((dir + "/palette.rgb").c_str(), "wb");
    if (!f) return 2;
    fwrite(pl.data(), 1, pl.size(), f);
    fclose(f);
  }

  // ---- provenance ----
  std::string meta;
  char line[512];
  std::snprintf(line, sizeof(line),
                "subject=%s\nmode=Z60 384x240 single view, reference oracle (zref::render)\n"
                "frames=%u step=%u ticks (frame f: tick = f*step)\n"
                "programs: wave_pool hash=0x%08X (%zu instrs), impact_wave hash=0x%08X (%zu "
                "instrs)\n"
                "field_apps=%zu stamps=%zu debris=%zu shake_frames=%zu\n"
                "palette=%zu unique colours (palette-exact by construction, <=256 enforced)\n"
                "%s\n"
                "sequence_crc32c=0x%08X\n",
                sub.name, sub.frames, sub.step, wave.prog.program_hash, wave.prog.instrs.size(),
                impact.prog.program_hash, impact.prog.instrs.size(), sub.fields.size(),
                sub.stamps.size(), sub.debris.size(), sub.shake.size(), pal.count(), sub.note,
                seq_crc);
  meta += line;
  for (uint32_t f = 0; f < frame_crcs.size(); ++f) {
    std::snprintf(line, sizeof(line), "frame_%04u_crc32c=0x%08X\n", f, frame_crcs[f]);
    meta += line;
  }
  FILE* mf = fopen((dir + "/meta.txt").c_str(), "wb");
  if (!mf) return 2;
  fwrite(meta.data(), 1, meta.size(), mf);
  fclose(mf);

  std::printf("%s: %u frames, %zu unique colours, sequence_crc32c=0x%08X\n", sub.name, sub.frames,
              pal.count(), seq_crc);
  return 0;
}

// ------------------------------------------------------------ subjects ------

// 1. terrain-wave — the membrane. A travelling radial wave crossing the
// island, exactly periodic in the captured loop (integer cycle count).
SceneSubject subject_wave() {
  SceneSubject s;
  s.name = "terrain-wave";
  s.frames = 64;
  s.step = 8;  // duration = frames*step so phase = f/frames
  FieldSpec fs;
  fs.program = 6;  // wave_pool
  fs.p[0] = fxm(-1500);  // centre x  -1.5 m (off-centre reads better)
  fs.p[1] = fxm(1000);   // centre z   1.0 m
  fs.p[2] = fxm(180);    // k = 0.18 turns/m  (wavelength ~5.6 m)
  fs.p[3] = fxm(2000);   // n = 2 cycles per loop (INTEGER -> seamless wrap)
  fs.p[4] = fxm(1900);   // amplitude 1.9 m
  fs.p[5] = fxm(7500);   // envelope: full inside 7.5 m
  fs.p[6] = fxm(12200);  // fades to zero at the island rim
  fs.p[7] = 0;
  fs.fx0 = fxm(-12000);
  fs.fz0 = fxm(-12000);
  fs.fx1 = fxm(12000);
  fs.fz1 = fxm(12000);
  fs.start_tick = 0;
  fs.duration = s.frames * s.step;
  s.fields.push_back(fs);
  s.note = "travelling radial wave, n=2 integer cycles: the loop closes seamlessly";
  return s;
}

// 2. terrain-impact — the strike: annular wave expanding outward, centre
// rebound, debris, screen shake, settle. Starts and ends at rest.
SceneSubject subject_impact() {
  SceneSubject s;
  s.name = "terrain-impact";
  s.frames = 80;
  s.step = 8;
  FieldSpec fs;
  fs.program = 7;  // impact_wave
  fs.p[0] = fxm(-500);   // centre x
  fs.p[1] = fxm(1500);   // centre z (sunlit camera side)
  fs.p[2] = fxm(14000);  // wave speed: front reaches 14 m at phase 1
  fs.p[3] = fxm(4500);   // rebound dome radius 4.5 m
  fs.p[4] = fxm(3000);   // amplitude 3.0 m
  fs.p[5] = fs.p[6] = fs.p[7] = 0;
  fs.fx0 = fxm(-12000);
  fs.fz0 = fxm(-12000);
  fs.fx1 = fxm(12000);
  fs.fz1 = fxm(12000);
  fs.start_tick = 0;
  fs.duration = s.frames * s.step;
  s.fields.push_back(fs);

  // erupt-style garnish: debris flung from the impact, screen shake at the
  // strike. Two flat colours; opaque point/tri sprites (palette-cheap).
  s.debris_spawn_frame = 4;             // the strike lands at phase ~0.05
  s.debris_gravity = fxm(45);           // per frame^2
  const int32_t cx = fxm(-500), cz = fxm(1500);
  struct D0 {
    int32_t dx, dz, vx, vy, vz;
    uint8_t sz;
  };
  const D0 d0[] = {
      {fxm(300), fxm(-200), fxm(140), fxm(760), fxm(-110), 96},
      {fxm(-400), fxm(100), fxm(-170), fxm(900), fxm(60), 128},
      {fxm(100), fxm(400), fxm(40), fxm(660), fxm(180), 80},
      {fxm(-200), fxm(-350), fxm(-80), fxm(820), fxm(-160), 96},
      {fxm(450), fxm(250), fxm(200), fxm(560), fxm(90), 80},
      {fxm(-100), fxm(-100), fxm(-40), fxm(1000), fxm(-40), 128},
      {fxm(250), fxm(-450), fxm(90), fxm(700), fxm(-200), 80},
      {fxm(-350), fxm(350), fxm(-140), fxm(620), fxm(140), 96},
  };
  int i = 0;
  for (const D0& d : d0) {
    const bool charred = (i++ % 2) == 0;
    s.debris.push_back({cx + d.dx, cz + d.dz, d.vx, d.vy, d.vz, d.sz,
                        static_cast<uint8_t>(charred ? 54 : 154),
                        static_cast<uint8_t>(charred ? 44 : 122),
                        static_cast<uint8_t>(charred ? 40 : 78)});
  }
  s.shake_frame = 4;
  s.shake = {26000, -42000, 34000, -26000, 18000, -12000, 8000, -4000, 2000, 0};
  s.note = "strike -> expanding annular wave -> centre rebound -> settle; "
           "debris + screen shake (erupt-style garnish); starts and ends at rest";
  return s;
}

// 3. terrain-scars — accumulating damage: three impacts in sequence, each
// leaving a persistent surface-sheet scar (charter §12); ends held so the
// accrued scarring reads. The composed lattice: apps in command order.
SceneSubject subject_scars() {
  SceneSubject s;
  s.name = "terrain-scars";
  s.frames = 96;
  s.step = 8;
  struct Hit {
    uint32_t frame;
    int32_t cx, cz;
    int32_t amp;
  };
  // all three strikes on the sunlit (camera) side so the membrane response
  // and the scars read; spread so the scar accrual is legible
  const Hit hits[] = {
      {4, fxm(-5500), fxm(2500), fxm(2600)},
      {32, fxm(5000), fxm(2000), fxm(2800)},
      {60, fxm(0), fxm(6500), fxm(2400)},
  };
  for (const Hit& h : hits) {
    FieldSpec fs;
    fs.program = 7;  // impact_wave
    fs.p[0] = h.cx;
    fs.p[1] = h.cz;
    fs.p[2] = fxm(12000);  // speed
    fs.p[3] = fxm(3500);   // dome radius
    fs.p[4] = h.amp;
    fs.p[5] = fs.p[6] = fs.p[7] = 0;
    fs.fx0 = fxm(-12000);
    fs.fz0 = fxm(-12000);
    fs.fx1 = fxm(12000);
    fs.fz1 = fxm(12000);
    fs.start_tick = h.frame * s.step;
    fs.duration = 240;  // 30 frames: the wave passes, the membrane settles
    s.fields.push_back(fs);
    // the scar: a crack ring + charred core, landing at the strike frame
    s.stamps.push_back({h.frame, h.cx, h.cz, fxm(3600), fxm(1800), 0xC000});
    s.stamps.push_back({h.frame, h.cx, h.cz, fxm(1300), 0, 0xC000});
  }
  s.note = "three strikes in sequence; surface-sheet scars persist and accrue "
           "(the loop restart is the sequence replaying)";
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  g_out = argc > 1 ? argv[1] : ".";
  std::vector<std::string> want;
  for (int i = 2; i < argc; ++i) want.push_back(argv[i]);
  const auto wanted = [&](const char* n) {
    if (want.empty()) return true;
    for (const auto& w : want)
      if (w == n) return true;
    return false;
  };

  std::printf("zhao-reel: deterministic frame sequences -> %s\n", g_out.c_str());
  int rc = 0;
  if (wanted("terrain-wave")) rc |= render_scene(subject_wave());
  if (wanted("terrain-impact")) rc |= render_scene(subject_impact());
  if (wanted("terrain-scars")) rc |= render_scene(subject_scars());
  return rc;
}
