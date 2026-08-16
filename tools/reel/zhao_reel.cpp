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
// that budget (C0 gradient-row sky, palette-permutation star boil, white
// glints that saturate under additive glow, silhouette materials) and this
// tool ENFORCES it: a subject whose frame set exceeds 256 unique colours
// fails the run with exit 3. Never weaken this into a quantisation step.
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
//       reference/src/zsky/star_gamut.cpp reference/src/zsky/star_bake.cpp \
//       reference/src/zsky/star_flare.cpp reference/src/zsky/star_field.cpp \
//       reference/src/zsky/star_compose.cpp reference/src/zterrain/terrain_core.cpp \
//       -Ireference/include -Iruntime/include -Itests/render \
//       -Icompiler/tests/generated

#include "render_helpers.hpp"  // tests/render (packet/canvas helpers)

#include "impact_wave.hpp"  // compiler/tests/generated (TS-generated)
#include "wave_pool.hpp"    // compiler/tests/generated (TS-generated)
#include "zfield/zfield.hpp"
#include "zref/zref_star.hpp"     // celestial compositor preview (stars_and_flares.md)
#include "zref/zref_terrain.hpp"  // dual-heightfield bake/breach reference

#include <cmath>
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
bool g_write = true;  // --check: render + verify CRCs only, write nothing

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

// The dusk sky under the sky_and_beams §1.2 elevation-ramp continuity law
// (2026-08-16): ONE ramp from warm horizon to deep zenith, C0 across every
// layer join — under == band_lower_horizon, band_lower_top ==
// band_upper_bottom, cap == band_upper_top. The pre-amendment authoring
// (each layer its own flat colour) rendered the cap and under rims as hard
// elliptical outlines. Gradient rows cost 16 flat colours (8 per band), well
// inside the 256-colour palette law; cloud/sun layers stay off (the additive
// sun is superseded by the star compositor subjects).
zref::sky::SkySet dusk_sky(int variant = 0) {
  zref::sky::SkySet s;
  s.background = {24, 26, 70};
  s.under = {214, 116, 82};               // == band_lower_horizon (S1.2 rule 1)
  s.band_lower_horizon = {214, 116, 82};  // warm dusk horizon
  s.band_lower_top = {150, 92, 118};      // == band_upper_bottom (rule 2)
  s.band_upper_bottom = {150, 92, 118};
  s.band_upper_top = {56, 48, 110};  // zenith
  s.cap = {56, 48, 110};             // == band_upper_top (rule 3)
  if (variant == 1) {
    // flat upper band (bottom == top == cap): still C0 under S1.2 — the
    // additive sun/corona/flare then sums against ONE colour, which keeps
    // an animated subject inside the 256-colour palette law
    s.band_upper_top = {150, 92, 118};
    s.cap = {150, 92, 118};
  }
  s.cloud = {255, 216, 196};  // unused (layer not requested)
  s.sun = {255, 206, 130};    // unused (layer not requested)
  s.cloud_max_alpha = zref::fx16{0};
  s.sun_energy = zref::fx16{0};
  s.sun_dir_x = zref::fx16{22938};
  s.sun_dir_z = zref::fx16{61604};
  return s;
}

// Sky rot_proj matched to cam_pitch (below): the SAME rotation rows with the
// translation dropped, so the sky horizon sits exactly where the terrain
// camera's horizon sits. Under the §1.2 perspective law the renderer takes
// w from row 2, so these are true camera-direction rows; the screen-centre
// bias folds in as row1 += bias*row2 (still rotation-only: translation
// column and bottom row stay zero).
zhao_abi::ZhMat4fx sky_rot_for_cam(int32_t k, int32_t ps, int32_t pc, int32_t bias, int32_t zsign) {
  const auto mul16 = [](int64_t a, int64_t b) { return static_cast<int32_t>((a * b) >> 16); };
  const int32_t wz = pc * zsign;  // Q16.16 unit-scale z row
  const int32_t m[16] = {k,
                         0,
                         0,
                         0,
                         0,
                         -mul16(k, pc) + mul16(bias, -ps),
                         -mul16(k, ps) * zsign + mul16(bias, wz),
                         0,
                         0,
                         -ps,
                         wz,
                         0,
                         0,
                         0,
                         0,
                         1 << 16};
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

// ------------------------------------------------------------- celestial ---
//
// The star subjects ride the renderer's [phase3-preview] pre-resolve hook:
// every LAW (ramps, boil, corona, flare chain, fade, budget) lives in
// zref::star / zref::flare / zref::post (stars_and_flares.md §12 — never
// re-implemented here); this block is pure AUTHORING: which class, where on
// screen, what distance. All integers (charter §29-7).

// display expansion for tints/glints (stars_and_flares.md §2 D2 law)
constexpr uint8_t c8(uint8_t c6) { return static_cast<uint8_t>((c6 << 2) | (c6 >> 4)); }

struct CelAssets {
  zref::star::Sprite8 face;         // starface for the subject's class
  zref::star::Sprite8 corona;       // corona variant
  uint8_t ramp[64][3];              // built ramp (slew state at targets)
  std::vector<zref::star::GlintPoint> glints;
};

struct SceneSubject;  // defined below (the celestial fns follow it)

struct CelCtx {
  const SceneSubject* sub = nullptr;
  uint32_t frame = 0;
  CelAssets* assets = nullptr;
  zref::star::FlareSlots slots;  // persists across the loop (the fade)
};

// Starfield backdrop: the §7 sector hash over the ±4 cube around sector
// (0,0,0), camera at the origin looking +Z, fixed integer projection
// (f = 300 px). Glints inside `excl` rects are dropped (they would sit
// under a halo/flare and add palette colours; the near star outshines
// them — an authoring cull, not a law).
struct Rect {
  int32_t x0, y0, x1, y1;
};
std::vector<zref::star::GlintPoint> make_glints(bool white, const std::vector<Rect>& excl) {
  std::vector<zref::star::GlintPoint> out;
  for (int kx = -4; kx <= 4; ++kx) {
    for (int ky = -4; ky <= 4; ++ky) {
      for (int kz = -4; kz <= 4; ++kz) {
        if (zref::sky::starfield_rarity_skip(kx, ky, kz)) continue;
        const zref::sky::SectorStar s = zref::sky::starfield(kx, ky, kz);
        if (s.no_star != 0) continue;
        if (s.z < 30000) continue;  // behind / too close to the eye plane
        const int32_t px = 192 + static_cast<int32_t>((300LL * s.x) / s.z);
        const int32_t py = 120 - static_cast<int32_t>((300LL * s.y) / s.z);
        if (px < 2 || px >= 382 || py < 2 || py >= 238) continue;
        bool excluded = false;
        for (const Rect& r : excl)
          if (px >= r.x0 && px < r.x1 && py >= r.y0 && py < r.y1) excluded = true;
        if (excluded) continue;
        const uint8_t i6 = zref::sky::starfield_intensity6(s.z);
        if (i6 == 0) continue;
        zref::star::GlintPoint g;
        g.x_px = px;
        g.y_px = py;
        g.size_px = i6 >= 58 ? 2 : 1;
        g.intensity6 = i6;
        if (white) {
          // flare subjects: saturated white — additive overlap with the
          // glow plane saturates back to white, no new palette colours
          g.rgb[0] = g.rgb[1] = g.rgb[2] = 255;
        } else {
          const uint8_t v = c8(i6);
          g.rgb[0] = g.rgb[1] = g.rgb[2] = v;
        }
        out.push_back(g);
      }
    }
  }
  return out;
}

// identity for a subject: a fixed sector + seed, class forced to the
// subject's star (the identity schedule stays the source of the seeds)
zref::star::StarIdentity cel_identity(uint8_t cls, uint32_t seed) {
  zref::star::StarIdentity id = zref::star::identity(7, 3, -2, seed);
  id.cls = cls;
  const zref::star::StarClass& c = zref::star::kGamut[cls];
  if (cls != 9)
    for (int ch = 0; ch < 3; ++ch) id.under6[ch] = c.under6[ch];
  return id;
}

void cel_build_assets(int celestial, CelAssets& a) {
  uint8_t cls = 0;
  uint8_t core16 = 5;  // halo_space
  switch (celestial) {
    case 1:
      cls = 3;  // S03 red giant, close: the boiling disc
      break;
    case 2:
      cls = 0;  // S00 yellow star: the classic flare
      break;
    case 3:
      cls = 11;   // S11 pulsar
      core16 = 8; // halo_airless: hard core + short skirt — few ring
                  // colours under the strobing burst (palette law), and a
                  // crisp look that suits a compact object
      break;
    case 4:
      cls = 0;
      core16 = 0;  // halo_atmo: surface sun with atmosphere (§4)
      break;
  }
  const zref::star::StarClass& c = zref::star::kGamut[cls];
  const zref::star::StarIdentity id = cel_identity(cls, 0xA11CE5u);
  zref::star::RampState rs;
  zref::star::ramp_retarget(rs, id);  // snap: reel ramps sit at targets
  zref::star::ramp_build(rs.cur, a.ramp);
  a.face = zref::star::starface(id.texture_seed, c.smooth);
  a.corona = zref::star::corona_sprite(core16);
  // glints: space subjects only; exclusion rects sized per subject below
  if (celestial == 1) {
    a.glints = make_glints(false, {{44, 12, 340, 220}});  // giant + halo box
  } else if (celestial == 2 || celestial == 3) {
    // the sweep + ghost lanes cover most of the frame; white glints
    // saturate under additive overlap and add no palette colours
    a.glints = make_glints(true, {});
  }
}

// Q16.16 raw from milli-units (integer authoring, charter §29-7)
constexpr int32_t fxm(int64_t milli) {
  return static_cast<int32_t>((milli * 65536 + (milli >= 0 ? 500 : -500)) / 1000);
}

void put_param(uint8_t* blob, int lane, int32_t raw) {
  for (int b = 0; b < 4; ++b) blob[lane * 4 + b] = static_cast<uint8_t>(raw >> (8 * b));
}

// ---- dual-heightfield island (terrain_rules.md, world-identity wave) -------
//
// A 161x161 lattice over ±160 m: 160x160 cells at the CANONICAL 2.0 m pitch
// (terrain_rules §1.3) — a 320 m island, the area of 25 Island-Patch pages
// (5x5 of 32x32 cells), carried as one Phase-3 envelope patch (the kind-6
// page split / sparse directory is Phase-6 loader work; stated honestly in
// meta.txt). Doubles below are ASSET AUTHORING ONLY (the bump_patch rule);
// heights quantize to 0.25 m steps so the flat-shade palette stays inside
// the 256-colour law. Top: coastal plateau rising to a ~15 m heart with
// gentle swells. Bottom: a keel — 5 m rim thickness growing to ~27 m at the
// centre (the bitten-apple profile the donor could never model). Coastline:
// R(theta) wobbles 116..151 m, cells outside are VOID_AUTHORED.
zref::render::TerrainPatch dual_island_patch() {
  const int W = 161;
  zref::render::TerrainPatch p;
  p.width = p.height = W;
  p.env_x0 = p.env_z0 = -(160 << 16);
  p.env_x1 = p.env_z1 = (160 << 16);
  p.heights.resize(static_cast<size_t>(W) * W);
  p.scar.assign(static_cast<size_t>(W) * W, 0);
  p.bottom.resize(static_cast<size_t>(W) * W);
  p.cell_state.assign(160 * 160, zref::terrain::kVoidAuthored);
  const auto coast_r = [](double x, double z) {
    const double th = std::atan2(z, x);
    return 130.0 + 14.0 * std::sin(3.0 * th + 1.0) + 7.0 * std::sin(7.0 * th + 2.0);
  };
  const auto q025 = [](double m) {  // quantize to 0.25 m height16 steps
    return static_cast<int16_t>(std::lround(m * 4.0) * 64);
  };
  for (int j = 0; j < W; ++j) {
    for (int i = 0; i < W; ++i) {
      const double x = (i - 80) * 2.0, z = (j - 80) * 2.0;
      const double r = std::sqrt(x * x + z * z);
      double t = r / coast_r(x, z);
      if (t > 1.0) t = 1.0;
      const double dome = 1.0 - t * t;
      const double top = 6.0 + 9.0 * dome + 1.5 * std::sin(x * 0.11) * std::cos(z * 0.09);
      const double thick = 5.0 + 22.0 * dome;
      const size_t k = static_cast<size_t>(j) * W + i;
      p.heights[k] = q025(top);
      p.bottom[k] = q025(top - thick);
    }
  }
  for (int cj = 0; cj < 160; ++cj) {
    for (int ci = 0; ci < 160; ++ci) {
      bool solid = true;  // a cell is ground iff ALL FOUR corners are inside
      for (int dz = 0; dz <= 1 && solid; ++dz)
        for (int dx = 0; dx <= 1 && solid; ++dx) {
          const double x = (ci + dx - 80) * 2.0, z = (cj + dz - 80) * 2.0;
          if (std::sqrt(x * x + z * z) > coast_r(x, z)) solid = false;
        }
      if (solid) p.cell_state[static_cast<size_t>(cj) * 160 + ci] = zref::terrain::kSolid;
    }
  }
  return p;
}

// One deterministic bake step: dig depth from->to (milli-metres) at a frame
// (TERRAIN.BAKE reference; the renderer just reads the layers it wrote).
struct BakeStep {
  uint32_t frame;
  int32_t cx, cz, radius;  // fx16 raw
  int32_t from_milli, to_milli;
};

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
  // world-identity wave: dual-heightfield island + deterministic bake ramp
  bool island = false;          // true: dual_island_patch (320 m, 2 m pitch)
  std::vector<BakeStep> bakes;  // applied at frame start, in order
  // camera (defaults = the wave-2 reel constants; island scenes override)
  int32_t cam_k = 127000, cam_eye = 14, cam_dist = 33, cam_bias = 14000;
  // debris: spawned at spawn_frame, integer gravity fxm per frame^2
  uint32_t debris_spawn_frame = 0;
  int32_t debris_gravity = 0;      // fx raw per frame^2 (subtracted from vy)
  int32_t debris_y0 = 2 << 16;     // launch height (fx raw)
  int32_t debris_floor = 1 << 15;  // despawn line (fx raw) — a breach scene
                                   // sets this BELOW the island so debris
                                   // visibly falls through the hole
  std::vector<Debris> debris;
  // screen shake: raw Y offsets per frame, starting at shake_frame
  uint32_t shake_frame = 0;
  std::vector<int32_t> shake;
  // sky-sweep mode: no terrain, camera pitch ping-pongs from pitch0 to
  // pitch1 (angle16 turns, pitch-down positive) across the loop — the §1.2
  // continuity demo: the cap and under rims must cross the frame invisibly
  bool sky_sweep = false;
  int32_t sweep_pitch0 = 0, sweep_pitch1 = 0;
  // terrain material (per-subject: the dusk-silhouette scenes darken it)
  uint8_t mat_r = 104, mat_g = 122, mat_b = 78;
  // celestial subjects (stars_and_flares.md, zref::star compositor preview):
  //   0 none; 1 star-boil (close S03 giant, CLUT boil + corona + glints);
  //   2 noctis-flare (S00 sweep, white-washed disc + corona + the full
  //     ghost chain); 3 pulsar (S11 duty strobe + glints);
  //   4 flare-occlusion (surface dusk: the sun crosses behind the island,
  //     the probe fade Noctis lacked)
  int celestial = 0;
  bool space = false;  // no DrawSky (fallback black), no terrain
  int sky_variant = 0;  // dusk_sky variant (1 = flat upper band, still C0)
  const char* note = "";
  // --check golden: CRC-32C over all frame RGB bytes in sequence (0 = none).
  // Moves whenever the renderer, the field programs or the authoring here
  // legitimately change — update it in the same commit and say so.
  uint32_t expect_seq_crc = 0;
};

// per-frame celestial compose — the ONE hook target (set on the renderer's
// [phase3-preview] pre-resolve point). Authoring only; every law call goes
// through zref::star / zref::flare / zref::post.
void cel_hook(void* vctx, uint8_t* rgb, int32_t* depth, uint32_t w, uint32_t h, uint32_t tick) {
  CelCtx& ctx = *static_cast<CelCtx*>(vctx);
  const SceneSubject& sub = *ctx.sub;
  CelAssets& a = *ctx.assets;
  const uint32_t f = ctx.frame;
  const uint32_t half = sub.frames / 2;
  const uint32_t ph = f < half ? f : (sub.frames - 1 - f);  // ping-pong

  zref::star::ComposeLight L;
  L.ramp = a.ramp;
  L.face = &a.face;
  L.corona = &a.corona;
  int n_lights = 1;

  switch (sub.celestial) {
    case 1: {  // star-boil: fixed close S03 giant; the CLUT rotation IS the
               // animation (63 frames × rot step 2 = one full revolution)
      L.x_px = 192;
      L.y_px = 116;
      L.disc_r_px = 48;
      L.halo_r_px = 120;
      L.d_milli = 3 * zref::star::kGamut[3].ray_milli / 2;  // d = 1.5r
      L.r_milli = zref::star::kGamut[3].ray_milli;
      L.flare_mode = 0;  // d < 5r: outside the §5 window — no flare
      L.probe_x = 192;
      L.probe_y = 116;
      break;
    }
    case 2: {  // noctis-flare: S00 sweeping, washed white disc + corona +
               // the full ghost chain; border fade at the sweep ends
      L.x_px = 8 + static_cast<int32_t>((368 * ph) / (half - 1));
      L.y_px = 150;
      L.disc_r_px = 8;
      L.halo_r_px = 16;
      L.d_milli = 40LL * zref::star::kGamut[0].ray_milli;  // k = 40, burst12
      L.r_milli = zref::star::kGamut[0].ray_milli;
      L.flare_mode = 1;
      L.tint[0] = c8(63);
      L.tint[1] = c8(58);
      L.tint[2] = c8(40);  // the class colour as display tint (D2 law)
      L.probe_x = L.x_px;
      L.probe_y = L.y_px;
      break;
    }
    case 3: {  // pulsar: tiny cyan core, §2 duty strobe on the flare
      L.x_px = 240;
      L.y_px = 110;
      L.disc_r_px = 4;
      L.halo_r_px = 14;  // small: the burst overlaps the halo, and every
                         // distinct halo ring colour times every glow level
                         // is a palette entry (395 colours at r 36)
      L.d_milli = 40LL * zref::star::kGamut[11].ray_milli;
      L.r_milli = zref::star::kGamut[11].ray_milli;
      L.flare_mode = 2;
      // lawful spin: rate = SPIN_K·13 = 715 angle16/tick (h2 mod 30 == 12);
      // the loop restart is the sequence replaying (scars precedent)
      L.spin_phase = static_cast<uint16_t>(tick * 715u);
      L.tint[0] = c8(0);
      L.tint[1] = c8(63);
      L.tint[2] = c8(63);
      L.probe_x = 240;
      L.probe_y = 110;
      break;
    }
    case 4: {  // flare-occlusion: a DISTANT sun (d = 600r) crosses behind
               // the island — the §6 far-glint rung plus the §5 b=7 streak,
               // the iconic anamorphic line. The terrain depth kills the
               // probe and the streak FADES over 15 frames (±1/frame, the
               // pop Noctis had and this spec removes). A near sun's full
               // burst was tried first and is unpublishable under the
               // palette law (every glow level × every lambert shade of the
               // island; 257–309 colours at k 12–30) — the far streak is
               // both the signature look and the one that fits.
      L.x_px = 24 + static_cast<int32_t>((336 * ph) / (half - 1));
      L.y_px = 102;     // grazes the island's raised heart: occluded only
                        // mid-sweep, so the fade has room to breathe
      L.disc_r_px = 0;  // <1.5 px projected: glint rung (below)
      L.halo_r_px = 0;
      L.d_milli = 600LL * zref::star::kGamut[0].ray_milli;  // streak, k=384
      L.r_milli = zref::star::kGamut[0].ray_milli;
      L.flare_mode = 1;
      L.tint[0] = c8(63);
      L.tint[1] = c8(58);
      L.tint[2] = c8(40);
      L.probe_x = L.x_px;
      L.probe_y = L.y_px;
      break;
    }
    default:
      n_lights = 0;
      break;
  }

  // the light's own far glint (§6 rung: the star IS a 2–3 px point when the
  // disc rung is off) — white saturates under the additive streak, and it
  // carries the glow tag the occlusion probe latches
  std::vector<zref::star::GlintPoint> pts(a.glints);
  if (n_lights == 1 && L.disc_r_px == 0 && L.halo_r_px == 0) {
    zref::star::GlintPoint g;
    g.x_px = L.x_px;
    g.y_px = L.y_px;
    g.size_px = 3;
    g.intensity6 = zref::star::glint_intensity6(L.d_milli, L.r_milli);
    g.rgb[0] = g.rgb[1] = g.rgb[2] = 255;
    pts.push_back(g);
  }
  zref::star::compose_view(rgb, depth, w, h, 0, 0, w, h, tick, &L, n_lights,
                           pts.empty() ? nullptr : pts.data(), static_cast<int>(pts.size()),
                           ctx.slots, nullptr);
}

// ------------------------------------------------------------ scene render --

int render_scene(const SceneSubject& sub) {
  const uint32_t W = 384, H = 240;
  zref::render::TerrainPatch patch =
      sub.island ? dual_island_patch() : rtest::bump_patch(161, 161, 12, 8);
  zref::render::Material mat{sub.mat_r, sub.mat_g, sub.mat_b};
  zref::sky::SkySet sky = dusk_sky(sub.sky_variant);

  const zfield::DecodeResult wave = zfield::decode(zfield_gen::wave_pool::kProgramBytes.data(),
                                                   zfield_gen::wave_pool::kProgramBytesLen);
  const zfield::DecodeResult impact = zfield::decode(zfield_gen::impact_wave::kProgramBytes.data(),
                                                     zfield_gen::impact_wave::kProgramBytesLen);
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

  // celestial subjects: assets baked once, fade slots persist across the
  // loop, the compositor rides the pre-resolve hook (stars_and_flares.md)
  CelAssets cel_assets;
  CelCtx cel_ctx;
  if (sub.celestial != 0) {
    cel_build_assets(sub.celestial, cel_assets);
    cel_ctx.sub = &sub;
    cel_ctx.assets = &cel_assets;
    // the pulsar subject starts with the fade already up: its subject IS
    // the §2 duty strobe, and the 15-frame fade-in would multiply every
    // glow level by 15 alpha steps (the palette law; the fade-in itself is
    // the flare-occlusion subject's show)
    if (sub.celestial == 3 || sub.celestial == 4) cel_ctx.slots.fade_ctr[0] = 15;
    rend.set_pre_resolve(&cel_hook, &cel_ctx);
  }

  const std::string dir = g_out + "/" + sub.name;
  if (g_write) {
    ZHAO_MKDIR(g_out.c_str());
    ZHAO_MKDIR(dir.c_str());
  }

  uint32_t breach_total = 0;
  for (uint32_t f = 0; f < sub.frames; ++f) {
    const uint32_t tick = f * sub.step;
    cel_ctx.frame = f;  // the hook reads the authored per-frame positions

    // deterministic bake steps for this frame (TERRAIN.BAKE reference:
    // bake writes scar layer B, the breach law flips cell-state layer D;
    // integer-only — the doubles above were authoring, this is the run)
    for (const BakeStep& bs : sub.bakes) {
      if (bs.frame != f) continue;
      const zref::terrain::DigStamp dig{bs.cx, bs.cz, bs.radius};
      zref::terrain::bake_dig(patch, dig, zref::fx16{fxm(bs.from_milli)},
                              zref::fx16{fxm(bs.to_milli)}, nullptr);
      const std::vector<zref::terrain::BreachEvent> ev = zref::terrain::apply_breach_law(patch);
      if (!ev.empty()) {
        breach_total += static_cast<uint32_t>(ev.size());
        std::printf("%s frame %u: %zu cell(s) breached (total %u)\n", sub.name, f, ev.size(),
                    breach_total);
      }
    }

    // debris population for this frame (integer ballistics)
    zref::render::Population* pop = nullptr;
    for (auto& e : res.populations)
      if (e.first == 3) pop = &e.second;
    pop->parts.clear();
    if (!sub.debris.empty() && f >= sub.debris_spawn_frame) {
      const int32_t t = static_cast<int32_t>(f - sub.debris_spawn_frame);
      for (const Debris& d : sub.debris) {
        // y = vy*t - g*t*(t-1)/2 ; alive while above the despawn line
        const int64_t rise = static_cast<int64_t>(d.vy) * t;
        const int64_t fall = static_cast<int64_t>(sub.debris_gravity) * t * (t - 1) / 2;
        const int64_t y = sub.debris_y0 + rise - fall;
        if (y < sub.debris_floor) continue;  // landed / fell out of the world
        pop->parts.push_back(
            {d.x0 + d.vx * t, static_cast<int32_t>(y), d.z0 + d.vz * t, d.size, d.r, d.g, d.b});
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
      // sunlit-side camera (zsign −1), bias recentres the island; k/eye/dist
      // are per-subject (the island scenes stand much further back)
      sv.payload.view_projection = cam_pitch(sub.cam_k, sub.cam_eye, sub.cam_dist, 28732, 58903,
                                             sub.cam_bias, -1, shake_raw);
      std::vector<uint8_t> v1;
      zhao_abi::zhao_pack_set_view(sv, v1);
      b.append_record(v1);

      // sky rotation: the terrain camera's own rows (pitch 26 deg down,
      // zsign -1) so the sky horizon and the terrain horizon agree; in
      // sky-sweep mode the pitch ping-pongs pitch0 -> pitch1 -> pitch0
      // across the loop (integer lerp on angle16 turns, table sin/cos)
      int32_t sky_ps = 28732, sky_pc = 58903, sky_bias = sub.cam_bias;
      if (sub.sky_sweep) {
        const uint32_t half = sub.frames / 2;
        const uint32_t ph = f < half ? f : (sub.frames - 1 - f);
        const int32_t th =
            sub.sweep_pitch0 + static_cast<int32_t>((static_cast<int64_t>(sub.sweep_pitch1 -
                                                                          sub.sweep_pitch0) *
                                                     ph) /
                                                    (half - 1));
        sky_ps = zref::fx_sin(zref::angle16{static_cast<uint16_t>(th)}).raw;
        sky_pc = zref::fx_cos(zref::angle16{static_cast<uint16_t>(th)}).raw;
        sky_bias = 0;
      }
      if (!sub.space) {  // space subjects: fallback black clear, no sky
        auto sk = zhao_abi::zhao_sample_draw_sky();
        sk.payload.sky_set = 2;
        sk.payload.rot_proj[0] = sky_rot_for_cam(sub.cam_k, sky_ps, sky_pc, sky_bias, -1);
        sk.payload.rot_proj[1] = sk.payload.rot_proj[0];
        sk.payload.drum_yaw = 0x0C00;
        sk.payload.cloud_scroll_u = 0;
        sk.payload.cloud_scroll_v = 0;
        sk.payload.viewport_mask = 1;
        sk.payload.flags = zref::sky::kLayerUnder | zref::sky::kLayerCap;
        std::vector<uint8_t> v2;
        zhao_abi::zhao_pack_draw_sky(sk, v2);
        b.append_record(v2);
      }

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

      if (!sub.sky_sweep && !sub.space) {
        auto dp = zhao_abi::zhao_sample_draw_procedural();
        dp.payload.program = 44;
        dp.payload.material = 45;
        dp.payload.transform = rtest::xform_identity();
        dp.payload.screen_error = 1 << 16;
        dp.payload.kind = zhao_abi::FORGE_HEIGHTFIELD_PATCH;
        std::vector<uint8_t> v5;
        zhao_abi::zhao_pack_draw_procedural(dp, v5);
        b.append_record(v5);
      }

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

    if (g_write) {
      char fp[600];
      std::snprintf(fp, sizeof(fp), "%s/%04u.rgb", dir.c_str(), f);
      if (!write_rgb(fp, W, H, rgb)) return 2;
    }
  }

  // ---- the palette law: a shipped GIF must be palette-exact ----
  if (pal.count() > 256) {
    std::fprintf(stderr,
                 "%s: %zu unique colours (> 256) — the frame set cannot be encoded "
                 "palette-exactly. Re-author the scene; NEVER fall back to palettegen.\n",
                 sub.name, pal.count());
    return 3;
  }

  if (g_write) {
    std::vector<uint8_t> pl(4);
    const uint32_t n = static_cast<uint32_t>(pal.count());
    for (int i = 0; i < 4; ++i) pl[i] = static_cast<uint8_t>(n >> (8 * i));
    pl.insert(pl.end(), pal.order.begin(), pal.order.end());
    FILE* f = fopen((dir + "/palette.rgb").c_str(), "wb");
    if (!f) return 2;
    fwrite(pl.data(), 1, pl.size(), f);
    fclose(f);
  }

  if (!g_write) {
    std::printf("%s: %u frames, %zu unique colours, sequence_crc32c=0x%08X\n", sub.name, sub.frames,
                pal.count(), seq_crc);
    if (sub.expect_seq_crc != 0 && seq_crc != sub.expect_seq_crc) {
      std::fprintf(stderr,
                   "%s: sequence_crc32c 0x%08X != expected 0x%08X — the reel drifted. Either a "
                   "renderer/field change moved it legitimately (regenerate the reel, update the "
                   "constant, and say so) or something is nondeterministic (report it loudly).\n",
                   sub.name, seq_crc, sub.expect_seq_crc);
      return 4;
    }
    return 0;
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
  fs.program = 6;        // wave_pool
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
  // note text is published on the site; it must pass the copy gate
  // (zhaozhou-site/tools/copycheck.py: no em dashes, no banned phrases)
  s.note = "travelling radial wave, n=2 whole cycles per loop: frame 63 steps straight back to frame 0";
  s.expect_seq_crc = 0x0222090Bu;  // re-pinned 2026-08-16: sky S1.2 amendment (perspective + C0 dusk ramp)
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
  fs.program = 7;        // impact_wave
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
  s.debris_spawn_frame = 4;    // the strike lands at phase ~0.05
  s.debris_gravity = fxm(45);  // per frame^2
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
    s.debris.push_back(
        {cx + d.dx, cz + d.dz, d.vx, d.vy, d.vz, d.sz, static_cast<uint8_t>(charred ? 54 : 154),
         static_cast<uint8_t>(charred ? 44 : 122), static_cast<uint8_t>(charred ? 40 : 78)});
  }
  s.shake_frame = 4;
  s.shake = {26000, -42000, 34000, -26000, 18000, -12000, 8000, -4000, 2000, 0};
  s.note =
      "strike -> expanding annular wave -> centre rebound -> settle; "
      "debris + screen shake (erupt-style garnish); starts and ends at rest";
  s.expect_seq_crc = 0x4F97AD9Bu;  // re-pinned 2026-08-16: sky S1.2 amendment
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
  s.note =
      "three strikes in sequence; surface-sheet scars persist and accrue "
      "(the loop restart is the sequence replaying)";
  s.expect_seq_crc = 0x86069EA1u;  // re-pinned 2026-08-16: sky S1.2 amendment
  return s;
}

// 4. terrain-breach — the world-identity capture: a 320 m dual-heightfield
// island (2.0 m pitch, modelled keel) holds for a beat, then a bake ramp
// digs a 30 m pit through ~22 m of local thickness; cells cross the §3.4
// breach law corner-first (neighbours sag toward the hole before dropping —
// the S5 corner-coupling look), the rim clothes itself down to the MODELLED
// bottom, and the dusk sky shows clean through the island. Debris + shake
// land at the first breach frame (the bake event the impact FX masks, §3.4).
SceneSubject subject_breach() {
  SceneSubject s;
  s.name = "terrain-breach";
  s.frames = 64;
  s.step = 8;
  s.island = true;
  // camera: far shot — eye 140 m, dist 300 m, same 26° pitch, sunlit side
  s.cam_k = 112000;
  s.cam_eye = 140;
  s.cam_dist = 300;
  s.cam_bias = 5243;  // +0.08 NDC
  // the dig: centre (28, 52) m — 59 m from the island heart, local thickness
  // ~22 m, top ~13 m; radius 34 m, final depth 30 m -> a ~17 m-radius hole
  // punched clean through. Ramp: frames 8..43 in 36 equal bake steps
  // (incremental from->to per frame — the applyDMapDelta cadence).
  const int32_t cx = fxm(28000), cz = fxm(52000), rad = fxm(34000);
  for (uint32_t f = 8; f <= 43; ++f) {
    const int32_t from = static_cast<int32_t>((static_cast<int64_t>(f - 8) * 30000) / 36);
    const int32_t to = static_cast<int32_t>((static_cast<int64_t>(f - 7) * 30000) / 36);
    s.bakes.push_back(BakeStep{f, cx, cz, rad, from, to});
  }
  // debris + screen shake at the first breach frame (frame 32 in the
  // printed deterministic breach schedule; re-pin if the ramp is re-authored)
  const uint32_t first_breach = 32;
  s.debris_spawn_frame = first_breach;
  s.debris_gravity = fxm(380);
  s.debris_y0 = fxm(11000);      // launch near the failing surface (~11 m)
  s.debris_floor = -fxm(80000);  // fall THROUGH the breach, out of the world
  struct D0 {
    int32_t dx, dz, vx, vy, vz;
    uint8_t sz;
  };
  const D0 d0[] = {
      {fxm(2000), fxm(-1500), fxm(900), fxm(1600), fxm(-700), 96},
      {fxm(-2600), fxm(900), fxm(-1100), fxm(2200), fxm(500), 128},
      {fxm(700), fxm(2800), fxm(300), fxm(1200), fxm(1200), 80},
      {fxm(-1400), fxm(-2400), fxm(-500), fxm(1900), fxm(-1000), 96},
      {fxm(3100), fxm(1700), fxm(1300), fxm(900), fxm(600), 80},
      {fxm(-700), fxm(-700), fxm(-300), fxm(2400), fxm(-300), 128},
      {fxm(1700), fxm(-3100), fxm(600), fxm(1400), fxm(-1300), 80},
      {fxm(-2400), fxm(2400), fxm(-900), fxm(1100), fxm(900), 96},
  };
  int i = 0;
  for (const D0& d : d0) {
    const bool charred = (i++ % 2) == 0;
    s.debris.push_back(
        {cx + d.dx, cz + d.dz, d.vx, d.vy, d.vz, d.sz, static_cast<uint8_t>(charred ? 54 : 154),
         static_cast<uint8_t>(charred ? 44 : 122), static_cast<uint8_t>(charred ? 40 : 78)});
  }
  s.shake_frame = first_breach;
  s.shake = {200000, -340000, 270000, -200000, 140000, -90000, 60000, -30000, 15000, 0};
  s.note =
      "320 m dual-heightfield island (2.0 m pitch = 25 Island-Patch pages of ground, "
      "one Phase-3 envelope patch), modelled keel 5..27 m thick; 36-step bake ramp digs "
      "a 30 m pit; cells breach corner-coupled; rim walls run to the MODELLED bottom; "
      "sky visible through the island; debris falls through the hole";
  s.expect_seq_crc = 0x47D4D163u;  // re-pinned 2026-08-16: sky S1.2 amendment (was 482E273C)
  return s;
}

// 5. sky-sweep — the §1.2 continuity demo: no terrain, the camera pitch
// ping-pongs from 2 deg above the horizon to the zenith and back. The cap
// rim (elevation ~26.6 deg) and the band gradient cross the whole frame;
// under the elevation-ramp law no layer join is visible at any pitch. The
// pre-amendment sky failed this exact motion (hard elliptical rims).
SceneSubject subject_skysweep() {
  SceneSubject s;
  s.name = "sky-sweep";
  s.frames = 64;
  s.step = 8;
  s.sky_sweep = true;
  // pitch-down angle16 turns: -2 deg = -364 (2 deg above horizon, the
  // under-plane rim sits at the bottom frame edge), -90 deg = -16384
  // (zenith). Ping-pong over 32 frames each way.
  s.sweep_pitch0 = -364;
  s.sweep_pitch1 = -16384;
  s.note =
      "camera pitch sweep, horizon to zenith and back; one elevation ramp "
      "drives under-plane, both drum bands and the zenith cap, so the layer "
      "joins cross the frame without a visible edge";
  s.expect_seq_crc = 0x740FEBB2u;  // pinned 2026-08-16 (first render, 48-seg cap fan)
  return s;
}

// 6. star-boil — a red giant at 1.5 radii: the boiling CLUT rotation, the
// space corona, and a graded starfield behind it. 63 frames at rot step 2
// per frame = exactly one full palette revolution, so the boil loops with
// no restart jump. No flare: d < 5r is outside the S5 window (the law, not
// an omission).
SceneSubject subject_starboil() {
  SceneSubject s;
  s.name = "star-boil";
  s.frames = 63;
  s.step = 6;  // rot = (tick/3) mod 63 = 2f mod 63 -> f=63 wraps to 0
  s.celestial = 1;
  s.space = true;
  s.note =
      "S03 red giant at 1.5 radii; granulation is a 63-entry palette "
      "rotation, zero texels rewritten per frame; corona reads through the "
      "same ramp; starfield from the sector hash";
  s.expect_seq_crc = 0xAF10A2D1u;  // re-pinned 2026-08-16: resolve white-rail fix
  return s;
}

// 7. noctis-flare — the S00 yellow star at 40 radii sweeping the frame:
// washed-white disc, corona, and the full lens chain (12-spoke burst +
// three mirrored ghosts on the lens axis), border fade at the edges.
SceneSubject subject_noctisflare() {
  SceneSubject s;
  s.name = "noctis-flare";
  s.frames = 64;
  s.step = 8;
  s.celestial = 2;
  s.space = true;
  s.note =
      "S00 at 40 radii; burst at the light, ghosts at -26/-77/-230 Q8.8 of "
      "the axis, quarter-res glow splats, class-colour tint; the flare dims "
      "over the outer 16 px instead of cutting";
  s.expect_seq_crc = 0x9448C485u;  // re-pinned 2026-08-16: resolve white-rail fix
  return s;
}

// 8. pulsar — S11: 250 km-class core, spin 715 angle16/tick, flare only
// while spin_phase < 0x4000 (the exact quarter-turn duty law).
SceneSubject subject_pulsar() {
  SceneSubject s;
  s.name = "pulsar";
  s.frames = 64;
  s.step = 8;
  s.celestial = 3;
  s.space = true;
  s.note =
      "S11 pulsar at 40 radii; the flare strobes on the S2 duty law "
      "(spin_phase < 0x4000, one quarter of each rotation); fade counters "
      "keep the visibility transitions stepped at 17 alpha per frame";
  s.expect_seq_crc = 0x6E58C05Cu;  // re-pinned 2026-08-16: resolve white-rail fix
  return s;
}

// 9. flare-occlusion — the surface case: a dusk sun crosses behind the
// island; the 1-byte probe stops reading GLOW and the flare fades out over
// 15 frames, then back in on the far side. Noctis popped here; the fade
// counter is the improvement the spec adds.
SceneSubject subject_flareocclusion() {
  SceneSubject s;
  s.name = "flare-occlusion";
  s.frames = 64;
  s.step = 8;
  s.celestial = 4;
  s.sky_variant = 1;  // flat upper band (still C0): additive headroom
  s.island = true;    // the dual-heightfield island, framed from afar
  s.cam_k = 112000;
  s.cam_eye = 140;
  s.cam_dist = 300;
  s.cam_bias = 5243;
  // dusk silhouette: a dark material compresses the lambert range (the
  // island against the sunset) and with it the glow-over-terrain sums
  s.mat_r = 44;
  s.mat_g = 48;
  s.mat_b = 42;
  s.note =
      "S00 sun at 30 radii crossing behind the island; the effect-tag probe "
      "gates the flare and a 4-bit counter fades it 15 frames each way; "
      "halo_atmo variant (atmosphere = one bake parameter)";
  s.expect_seq_crc = 0x9AB5AC21u;  // re-pinned 2026-08-16: resolve white-rail fix
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  // --check: the animation-stability regression (PLAN Tier 1 secondary
  // value): render every subject, write nothing, fail on any sequence-CRC
  // drift. Wired into ctest as reel_sequence_crc.
  if (argc > 1 && std::strcmp(argv[1], "--check") == 0) {
    g_write = false;
    int rc = 0;
    rc |= render_scene(subject_wave());
    rc |= render_scene(subject_impact());
    rc |= render_scene(subject_scars());
    rc |= render_scene(subject_breach());
    rc |= render_scene(subject_skysweep());
    rc |= render_scene(subject_starboil());
    rc |= render_scene(subject_noctisflare());
    rc |= render_scene(subject_pulsar());
    rc |= render_scene(subject_flareocclusion());
    std::printf(rc == 0 ? "reel --check: all sequence CRCs match\n" : "reel --check: FAILED\n");
    return rc;
  }
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
  if (wanted("terrain-breach")) rc |= render_scene(subject_breach());
  if (wanted("sky-sweep")) rc |= render_scene(subject_skysweep());
  if (wanted("star-boil")) rc |= render_scene(subject_starboil());
  if (wanted("noctis-flare")) rc |= render_scene(subject_noctisflare());
  if (wanted("pulsar")) rc |= render_scene(subject_pulsar());
  if (wanted("flare-occlusion")) rc |= render_scene(subject_flareocclusion());
  return rc;
}
