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
#include "zref/zref_creature.hpp"  // creature reference core (creature_rules.md)
#include "zref/zref_star.hpp"     // celestial compositor preview (stars_and_flares.md)
#include "zref/zref_terrain.hpp"  // dual-heightfield bake/breach reference
#include "zrender/internal.hpp"   // compose_lattice for the sim-side tilt taps

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
  zref::star::FlareSlots slots;         // persists across the loop (the fade)
  zref::star::TrailHistory trails[2];   // §15 rings, persist across the loop
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
  uint8_t core16 = 5;  // halo_space (default for space scenes)
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
    case 5:
      cls = 1;  // S01 blue giant
      break;
    case 6:
      cls = 2;  // S02 white dwarf
      break;
    case 7:
      cls = 4;  // S04 orange giant
      break;
    case 8:
      cls = 7;  // S07 blue dwarf
      break;
    case 9:
      cls = 8;  // S08 multiple
      break;
    case 10:
      cls = 9;  // S09 infant star
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
  } else if (celestial == 2 || celestial == 3 || celestial >= 5) {
    // the sweep + ghost lanes cover most of the frame; white glints
    // saturate under additive overlap and add no palette colours
    // celestial >= 5 are the new star classes (5-10)
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
  // camera (defaults = the wave-2 reel constants; island scenes override).
  // cam_pull: lerp cam -> cam2 over [cam_pull0, frames) — the LOD ladder's
  // pull-back shot walks the creature down mesh -> micro -> splat -> glint.
  int32_t cam_k = 127000, cam_eye = 14, cam_dist = 33, cam_bias = 14000;
  bool cam_pull = false;
  uint32_t cam_pull0 = 0;
  int32_t cam2_eye = 0, cam2_dist = 0, cam2_bias = 0;  // (cam2_k == cam_k)
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
  // creature subjects (creature_rules.md lane): 1 = wave-walk (the identity
  // shot: walk + wave tilt + LOD pull-back), 2 = bulk-pop (inflate -> gibs)
  int creature = 0;
  int32_t bump_ext = 12;  // bump_patch half-extent (creature shots: 6 m so the
                          // near camera at dist 8 stands OUTSIDE the envelope —
                          // Phase-3 culls the whole patch when any lattice
                          // vertex lands behind the eye, terrain.cpp 310)
  const char* note = "";
  // --check golden: CRC-32C over all frame RGB bytes in sequence (0 = none).
  // Moves whenever the renderer, the field programs or the authoring here
  // legitimately change — update it in the same commit and say so.
  uint32_t expect_seq_crc = 0;
};

// per-frame celestial compose — the ONE hook target (set on the renderer's
// [phase3-preview] pre-resolve point). Authoring only; every law call goes
// through zref::star / zref::flare / zref::post.
//
// TRAIL AUTHORING (§15, the palette cost stated up front): ghosts draw
// through the LEVEL-CAPPED halo palette, so each ghost's colours are ramp
// entries — NOT ring-colour × alpha products. An alpha-scaled chain would
// cost ~63 ring colours × 8 ghost alphas ≈ 504 entries before the starfield
// greys, far over the 256-colour law (the same multiplication the pulsar
// subject avoids with halo_airless). What trails DO add is additive overlap
// sums along the path (contiguous ghost sums, quickly clamped); the tool's
// palette counter is the arbiter and ghost_r_px is the knob.
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
  L.trail = &ctx.trails[0];  // §15: history persists across the loop; the
                             // static-skip law keeps resting subjects clean
  zref::star::ComposeLight L2;  // the S08 multiple system's companion body
  L2.ramp = a.ramp;
  L2.face = &a.face;
  L2.corona = &a.corona;
  L2.trail = &ctx.trails[1];
  int n_lights = 1;
  // ping-pong drift for the class portraits: +-150 px about the centre,
  // ~9.7 px/frame. The speed is a PALETTE-LAW choice, not just an aesthetic
  // one: ghost radius sits at/below the per-frame spacing, so a trail pixel
  // sums at most ~2 ghost falloffs (contiguous-sum pairs) instead of all 8
  // (the 400+-colour blow-up the tool refuses to ship).
  const int32_t drift_x = 42 + static_cast<int32_t>((300 * ph) / (half - 1));

  switch (sub.celestial) {
    case 1: {  // star-boil: one close S03 giant, the CLUT rotation (colour
               // boil) is the subject. 63 frames × rot step 2 = one full
               // palette revolution. The star is AT REST — no trail (and the
               // §15 static-skip law would render none anyway).
      L.trail = nullptr;
      L.x_px = 192;
      L.y_px = 120;
      L.disc_r_px = 80;  // ENLARGED for legibility
      L.halo_r_px = 160;
      L.d_milli = 3 * zref::star::kGamut[3].ray_milli / 2;
      L.r_milli = zref::star::kGamut[3].ray_milli;
      L.flare_mode = 0;
      L.probe_x = 192;
      L.probe_y = 120;
      break;
    }
    case 2: {  // noctis-flare: S00 sweeping, washed white disc + corona +
               // the full ghost chain; border fade at the sweep ends.
               // TRAILED (§15): the sweep leaves the smear — the moving sun
               // the owner asked to look Noctis.
      L.x_px = 8 + static_cast<int32_t>((368 * ph) / (half - 1));
      L.y_px = 150;
      L.disc_r_px = 8;
      L.halo_r_px = 16;
      L.ghost_r_px = 12;  // at the ~12 px/frame sweep the chain just touches
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
    case 3: {  // pulsar: ENLARGED cyan core for legibility. The duty strobe
               // on flare is now clearly visible at this scale. AT REST — no
               // trail (the strobe is the subject; §15 static-skip).
      L.trail = nullptr;
      L.x_px = 192;
      L.y_px = 120;
      L.disc_r_px = 28;  // ENLARGED from 4 for legibility (was "a dot phasing")
      L.halo_r_px = 80;  // ENLARGED from 14; burst visible against corona
      L.d_milli = 40LL * zref::star::kGamut[11].ray_milli;
      L.r_milli = zref::star::kGamut[11].ray_milli;
      L.flare_mode = 2;
      // lawful spin: rate = SPIN_K·13 = 715 angle16/tick (h2 mod 30 == 12);
      L.spin_phase = static_cast<uint16_t>(tick * 715u);
      L.tint[0] = c8(0);
      L.tint[1] = c8(63);
      L.tint[2] = c8(63);
      L.probe_x = 192;
      L.probe_y = 120;
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
               // NOT trailed: the sun sits at the glint rung (no disc or
               // halo); at this scale the streak IS the smear.
      L.trail = nullptr;
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
    case 5: {  // blue-giant: S01, drift + smear. Close (2.5r) so the disc
               // keeps its colour (SATUR 30, not the white wash of 20r);
               // flare OFF for class portraits (the flare chain is the
               // noctis-flare subject's job) and the disc sized so the
               // 78 px tail extends well past disc + halo.
      L.x_px = drift_x;
      L.y_px = 120;
      L.disc_r_px = 40;
      L.halo_r_px = 26;
      L.ghost_r_px = 15;
      L.d_milli = 5LL * zref::star::kGamut[1].ray_milli / 2;
      L.r_milli = zref::star::kGamut[1].ray_milli;
      L.flare_mode = 0;
      L.probe_x = L.x_px;
      L.probe_y = 120;
      break;
    }
    case 6: {  // white-dwarf: S02 compact hot star. Small disc (the class
               // IS compact), 2r so the white stays graded, flare off, the
               // tail dominates: a small fast star with a long smear.
      L.x_px = drift_x;
      L.y_px = 120;
      L.disc_r_px = 16;
      L.halo_r_px = 14;
      L.ghost_r_px = 11;
      L.d_milli = 2LL * zref::star::kGamut[2].ray_milli;
      L.r_milli = zref::star::kGamut[2].ray_milli;
      L.flare_mode = 0;
      L.probe_x = L.x_px;
      L.probe_y = 120;
      break;
    }
    case 7: {  // orange-giant: S04 warm giant, close (2.5r) so the golden
               // colour fills the disc; flare off; tail past disc + halo.
      L.x_px = drift_x;
      L.y_px = 120;
      L.disc_r_px = 42;
      L.halo_r_px = 28;
      L.ghost_r_px = 15;
      L.d_milli = 5LL * zref::star::kGamut[4].ray_milli / 2;
      L.r_milli = zref::star::kGamut[4].ray_milli;
      L.flare_mode = 0;
      L.probe_x = L.x_px;
      L.probe_y = 120;
      break;
    }
    case 8: {  // blue-dwarf: S07 compact deep-blue star; small disc, 2r,
               // flare off; the deep blue smear is the read.
      L.x_px = drift_x;
      L.y_px = 120;
      L.disc_r_px = 14;
      L.halo_r_px = 12;
      L.ghost_r_px = 10;
      L.d_milli = 2LL * zref::star::kGamut[7].ray_milli;
      L.r_milli = zref::star::kGamut[7].ray_milli;
      L.flare_mode = 0;
      L.probe_x = L.x_px;
      L.probe_y = 120;
      break;
    }
    case 9: {  // multiple: S08 as it actually is — TWO bodies. One class
               // (one ramp, one colour family; a second ramp would double
               // the palette cost), primary and companion orbiting the
               // barycentre once per loop: curved trails, the §15 showpiece.
               // Integer orbit: angle = f·(65536/64) per frame, table cos/sin.
      const int32_t ang = static_cast<int32_t>(f * (65536u / 64u));
      const int32_t co = zref::fx_cos(zref::angle16{static_cast<uint16_t>(ang)}).raw;
      const int32_t si = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ang)}).raw;
      L.x_px = 192 + ((90 * co) >> 16);
      L.y_px = 120 - ((90 * si) >> 16);
      L.disc_r_px = 26;
      L.halo_r_px = 18;
      L.ghost_r_px = 12;
      L.d_milli = 5LL * zref::star::kGamut[8].ray_milli / 2;
      L.r_milli = zref::star::kGamut[8].ray_milli;
      L.flare_mode = 0;
      L.probe_x = L.x_px;
      L.probe_y = L.y_px;
      L2.x_px = 192 - ((90 * co) >> 16);
      L2.y_px = 120 + ((90 * si) >> 16);
      L2.disc_r_px = 16;  // the companion: smaller, no flare of its own
      L2.halo_r_px = 12;
      L2.ghost_r_px = 9;
      L2.d_milli = 15LL * zref::star::kGamut[8].ray_milli;
      L2.r_milli = zref::star::kGamut[8].ray_milli;
      L2.flare_mode = 0;
      L2.probe_x = L2.x_px;
      L2.probe_y = L2.y_px;
      n_lights = 2;
      break;
    }
    case 10: {  // infant: S09 young protostar; 2r keeps the per-identity
                // undertone visible in the disc; flare off.
      L.x_px = drift_x;
      L.y_px = 120;
      L.disc_r_px = 24;
      L.halo_r_px = 18;
      L.ghost_r_px = 12;
      L.d_milli = 2LL * zref::star::kGamut[9].ray_milli;
      L.r_milli = zref::star::kGamut[9].ray_milli;
      L.flare_mode = 0;
      L.probe_x = L.x_px;
      L.probe_y = 120;
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
  zref::star::ComposeLight ls[2] = {L, L2};  // L2 only used by the S08 pair
  zref::star::compose_view(rgb, depth, w, h, 0, 0, w, h, tick, ls, n_lights,
                           pts.empty() ? nullptr : pts.data(), static_cast<int>(pts.size()),
                           ctx.slots, nullptr);
}

// ------------------------------------------------------------ creature -----
//
// The creature subjects ride the renderer's pre-resolve hook exactly like
// the celestial compositor preview does: every LAW (ring build, pose decode
// + cache, skinning, tilt, bulk, LOD ladder) lives in zref::creature
// (creature_rules.md — never re-implemented here); this block is pure
// AUTHORING: which creature, where, what camera. All integers (29-7).

namespace zc = zref::creature;

// The demo subject: a watchdog quadruped. Ring parts are rigid per bone
// (donor law); the body lies along Z via an EXACT quarter turn (pitch_q=1).
// 7 bones, 6 parts, authored in integer constants only.
const zc::CreatureType& watchdog_type() {
  static const zc::CreatureType t = [] {
    zc::Skeleton sk;
    sk.bone_count = 7;
    sk.bones[0] = zc::Bone{0, 0, fxm(520), 0};
    sk.bones[1] = zc::Bone{0, 0, fxm(120), fxm(420)};
    sk.bones[2] = zc::Bone{0, -fxm(180), -fxm(50), fxm(260)};
    sk.bones[3] = zc::Bone{0, fxm(180), -fxm(50), fxm(260)};
    sk.bones[4] = zc::Bone{0, -fxm(180), -fxm(50), -fxm(260)};
    sk.bones[5] = zc::Bone{0, fxm(180), -fxm(50), -fxm(260)};

    std::vector<zc::RingPart> parts;
    zc::RingPart body;
    body.rings = {{-fxm(280), fxm(200), 10}, {-fxm(140), fxm(265), 10}, {0, fxm(280), 10},
                  {fxm(140), fxm(265), 10}, {fxm(280), fxm(200), 10}};
    body.caps = zc::kCapTop | zc::kCapBot;
    body.pitch_q = 1;  // exact quarter turn: stack along +Z (the facing axis)
    body.bone = 0;
    body.r = 198; body.g = 108; body.b = 58;
    parts.push_back(body);

    zc::RingPart head;
    head.rings = {{0, fxm(125), 8}, {fxm(150), fxm(115), 8}, {fxm(300), fxm(80), 8}};
    head.caps = zc::kCapTop | zc::kCapBot;
    head.pitch_q = 1;
    head.bone = 1;
    head.r = 232; head.g = 168; head.b = 96;
    parts.push_back(head);

    for (int leg = 0; leg < 4; ++leg) {
      zc::RingPart lp;
      lp.rings = {{0, fxm(60), 6}, {-fxm(250), fxm(55), 6}, {-fxm(500), fxm(45), 6}};
      lp.caps = zc::kCapTop | zc::kCapBot;
      lp.bone = static_cast<uint8_t>(2 + leg);
      lp.r = 122; lp.g = 74; lp.b = 52;
      parts.push_back(lp);
    }

    // clip bank: slot 1 idle (breathing bob), slot 2 walk (16 keys).
    // Authored through the fx trig tables — the integer path.
    zc::ClipBank bank;
    bank.bone_count = 7;
    zc::Clip idle;
    idle.slot_id = 1;
    idle.frame_count = 32;
    idle.root.assign(32 * 3, 0);
    idle.quats.assign(static_cast<size_t>(32) * 7, zc::quat16_identity());
    for (uint16_t f = 0; f < 32; ++f) {
      const zref::angle16 breathe{static_cast<uint16_t>(f * (65536u / 32u))};
      idle.root[f * 3 + 1] = (zref::fx_sin(breathe).raw * 520) >> 16;  // +-8 mm
    }
    bank.clips.push_back(std::move(idle));

    zc::Clip walk;
    walk.slot_id = 2;
    walk.frame_count = 16;
    walk.root.assign(16 * 3, 0);
    walk.quats.assign(static_cast<size_t>(16) * 7, zc::quat16_identity());
    // swing: legs +-0.30 rad about X, diagonal pairs in antiphase; head
    // nods; root bobs at double frequency. Half-angle as angle16 via a
    // linear map of the table sine (all integer).
    for (uint16_t f = 0; f < 16; ++f) {
      const zref::angle16 ph{static_cast<uint16_t>(f * (65536u / 16u))};
      const int32_t s1 = zref::fx_sin(ph).raw;
      const int32_t s2 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph.raw * 2)}).raw;
      for (int b = 2; b <= 5; ++b) {
        const bool diagonalA = (b == 2 || b == 5);
        const int32_t swing = (diagonalA ? s1 : -s1) * 1565 >> 16;  // 0.15 rad half
        const zref::angle16 half{static_cast<uint16_t>(swing & 0xFFFF)};
        walk.quats[static_cast<size_t>(f) * 7 + b] =
            zc::quat16_axis_angle(zref::fx16{1 << 16}, zref::fx16{0}, zref::fx16{0}, zref::fx_sin(half),
                                  zref::fx_cos(half));
      }
      const int32_t nod = (s2 * 400) >> 16;
      const zref::angle16 halfh{static_cast<uint16_t>(nod & 0xFFFF)};
      walk.quats[static_cast<size_t>(f) * 7 + 1] =
          zc::quat16_axis_angle(zref::fx16{1 << 16}, zref::fx16{0}, zref::fx16{0}, zref::fx_sin(halfh),
                                zref::fx_cos(halfh));
      walk.root[f * 3 + 1] = (zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph.raw * 2 + 0x4000)})
                                  .raw *
                              1640) >>
                             16;  // +-25 mm bob
    }
    walk.events = {{0, zc::kEvFoot, 0}, {8, zc::kEvFoot, 1}};
    bank.clips.push_back(std::move(walk));

    zc::CreatureType type;
    type.type_id = 1;
    const char* reason = "";
    if (!zc::compile_creature(sk, bank, parts, type, &reason)) {
      std::fprintf(stderr, "watchdog_type: compile failed: %s\n", reason);
    }
    return type;
  }();
  return t;
}

struct CreatureReelCtx {
  zc::CreatureInstance* inst = nullptr;
  zc::PoseBank* poses = nullptr;
  zref::mat4fx vp;
  std::vector<zc::Gib>* gibs = nullptr;
};

void creature_hook(void* vctx, uint8_t* rgb, int32_t* depth, uint32_t w, uint32_t h,
                   uint32_t /*tick*/) {
  CreatureReelCtx& c = *static_cast<CreatureReelCtx*>(vctx);
#ifdef ZHAO_CREATURE_DEBUG
  {
    // telemetry: rung, projected radius, screen bbox of the skinned mesh
    const zc::CreatureType& T = *c.inst->type;
    const zref::vec4fx clip = zref::mat4_vec4(
        c.vp, zref::vec4fx{zref::fx16{c.inst->x}, zref::fx16{c.inst->y}, zref::fx16{c.inst->z},
                           zref::fx16{1 << 16}},
        nullptr);
    const zref::render::Viewport vpp{0, 0, w, h};
    const zref::render::ProjOut pc = zref::render::project_vertex(
        c.vp, vpp, zref::fx16{c.inst->x}, zref::fx16{c.inst->y}, zref::fx16{c.inst->z}, nullptr);
    int32_t minx = 1 << 30, maxx = -(1 << 30), miny = 1 << 30, maxy = -(1 << 30);
    if (pc.in) {
      minx = maxx = pc.s.x;
      miny = maxy = pc.s.y;
    }
    std::fprintf(stderr, "creature: rung=%d vis=%d in=%d centre=(%d,%d) w_raw=%d\n",
                 static_cast<int>(c.inst->lod.rung), c.inst->visible ? 1 : 0, pc.in ? 1 : 0,
                 pc.in ? pc.s.x >> 8 : -1, pc.in ? pc.s.y >> 8 : -1, clip.w.raw);
  }
#endif
  zc::compose_creatures(rgb, depth, w, h, c.vp, &c.inst, 1, *c.poses, nullptr);
  if (c.gibs == nullptr || c.gibs->empty()) return;
  // gib particles: projected squares at fixed depth (the PART.SPAWN burst
  // stand-in; DrawPopulation draws the shipping version)
  zref::render::WorkSurface surf;
  surf.w = w;
  surf.h = h;
  surf.rgb.assign(rgb, rgb + static_cast<size_t>(w) * h * 3);
  surf.depth.assign(depth, depth + static_cast<size_t>(w) * h);
  const zref::render::Viewport vpp{0, 0, w, h};
  for (const zc::Gib& g : *c.gibs) {
    const zref::render::ProjOut pc =
        zref::render::project_vertex(c.vp, vpp, zref::fx16{g.x}, zref::fx16{g.y}, zref::fx16{g.z},
                                     nullptr);
    if (!pc.in) continue;
    const int32_t r8 = static_cast<int32_t>(g.size) * 16;  // U 0.4.4 px -> S12.8
    zref::render::ScreenV a = pc.s, b = pc.s, d = pc.s, e = pc.s;
    a.x = pc.s.x - r8; a.y = pc.s.y - r8;
    b.x = pc.s.x + r8; b.y = pc.s.y - r8;
    d.x = pc.s.x + r8; d.y = pc.s.y + r8;
    e.x = pc.s.x - r8; e.y = pc.s.y + r8;
    zref::render::TriMode tm;
    tm.use_fixed_depth = true;
    tm.fixed_depth = pc.s.d;
    zref::render::raster_tri(surf, vpp, a, b, d, g.r, g.g, g.b, tm);
    zref::render::raster_tri(surf, vpp, a, d, e, g.r, g.g, g.b, tm);
  }
  std::memcpy(rgb, surf.rgb.data(), surf.rgb.size());
  std::memcpy(depth, surf.depth.data(), surf.depth.size() * sizeof(int32_t));
}

// ------------------------------------------------------------ scene render --

int render_scene(const SceneSubject& sub) {
  const uint32_t W = 384, H = 240;
  zref::render::TerrainPatch patch =
      sub.island ? dual_island_patch() : rtest::bump_patch(161, 161, sub.bump_ext, 8);
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

  // creature subject state (zref::creature — the laws live there)
  const zc::CreatureType* dog = nullptr;
  zc::CreatureInstance dog_inst;
  zc::PoseBank dog_poses;
  std::vector<zc::Gib> gibs;
  CreatureReelCtx cr_ctx;
  int32_t pop_threshold = 0;
  int32_t gib_gravity = 0;
  if (sub.creature != 0) {
    dog = &watchdog_type();
    dog_inst.type = dog;
    dog_inst.tilt_mode = zc::TiltMode::kCompletely;
    dog_inst.anim.cut(sub.creature == 1 ? 2 : 1);  // walk / idle
    cr_ctx.inst = &dog_inst;
    cr_ctx.poses = &dog_poses;
    cr_ctx.gibs = &gibs;
    pop_threshold = 22 * (1 << 16) / 10;  // bulk 2.2 pops (species constant)
    gib_gravity = fxm(18);                // per frame^2
    rend.set_pre_resolve(&creature_hook, &cr_ctx);
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

    // ---- creature sim (the driver composes the tick cadence; the laws are
    // zref::creature's). One lattice compose per frame feeds the tilt taps —
    // the SAME compose_lattice the renderer's DrawProcedural runs inside
    // (physics equals pixels, terrain_rules 4.1).
    int32_t cam_eye = sub.cam_eye, cam_dist = sub.cam_dist, cam_bias = sub.cam_bias;
    if (dog != nullptr) {
      if (sub.cam_pull && f >= sub.cam_pull0) {
        const int64_t n = sub.frames - sub.cam_pull0;
        const int64_t k = f - sub.cam_pull0;
        cam_eye = static_cast<int32_t>(sub.cam_eye + (k * (sub.cam2_eye - sub.cam_eye) + n / 2) / n);
        cam_dist = static_cast<int32_t>(sub.cam_dist + (k * (sub.cam2_dist - sub.cam_dist) + n / 2) / n);
        cam_bias = static_cast<int32_t>(sub.cam_bias + (k * (sub.cam2_bias - sub.cam_bias) + n / 2) / n);
      }
      std::vector<zref::render::FieldApp> fapps;
      for (const FieldSpec& fs : sub.fields) {
        zref::render::FieldApp fa;
        fa.prog = fs.program == 6 ? &wave.prog : &impact.prog;
        fa.cmd.program = fs.program;
        fa.cmd.footprint.x0 = fs.fx0;
        fa.cmd.footprint.y0 = fs.fz0;
        fa.cmd.footprint.x1 = fs.fx1;
        fa.cmd.footprint.y1 = fs.fz1;
        fa.cmd.start_tick = fs.start_tick;
        fa.cmd.duration_ticks = fs.duration;
        for (int kk = 0; kk < 8; ++kk) put_param(fa.cmd.parameters, kk, fs.p[kk]);
        fapps.push_back(fa);
      }
      const zref::terrain::ComposedLattice lat = zref::render::compose_lattice(
          patch, rtest::xform_identity(), fapps, tick, nullptr, nullptr);

      if (sub.creature == 1) {
        // walk: root motion + ground snap, 8 anim ticks per frame
        dog_inst.x += fxm(22);
        dog_inst.facing = zref::angle16{0};
        for (int t = 0; t < 8; ++t) {
          const zc::ClipEvent* fired = nullptr;
          uint8_t nf = 0;
          zc::anim_advance(dog_inst.anim, dog->bank, &fired, nf);
        }
        zc::ground_tilt_update(dog_inst.tilt, dog_inst.tilt_mode, dog_inst.facing, lat,
                               zref::fx16{dog_inst.x}, zref::fx16{dog_inst.z}, zref::fx16{fxm(40)},
                               zref::fx16{fxm(20)});
      } else {
        for (int t = 0; t < 8; ++t) {
          const zc::ClipEvent* fired = nullptr;
          uint8_t nf = 0;
          zc::anim_advance(dog_inst.anim, dog->bank, &fired, nf);
        }
        // bulk inflation: target ramps from frame 16; crossing the species
        // pop threshold gibs the creature (mesh removed, burst spawned)
        dog_inst.bulk.target = f < 16 ? (1 << 16)
                                   : static_cast<int32_t>(65536 + (f - 16) * (65536 * 14 / 10) / 24);
        for (int t = 0; t < 8; ++t) zc::bulk_update(dog_inst.bulk, 4);
        if (dog_inst.visible && zc::bulk_popped(dog_inst.bulk, pop_threshold)) {
          dog_inst.visible = false;
          std::array<zc::mat3x4fx, zc::kMaxBones> pose;
          const zc::Clip* clip = nullptr;
          for (const zc::Clip& cc : dog->bank.clips)
            if (cc.slot_id == dog_inst.anim.slot) clip = &cc;
          zc::decode_pose(*dog, *clip, dog_inst.anim.frame, pose, nullptr);
          const int32_t bs = dog_inst.bulk.scale;
          for (int b = 0; b < dog->bank.bone_count; ++b) {
            for (int i = 0; i < 3; ++i)
              for (int j = 0; j < 3; ++j)
                pose[b].m[i * 4 + j] = zref::rescale_s32(
                    static_cast<int64_t>(pose[b].m[i * 4 + j]) * bs, 16, nullptr);
          }
          zc::spawn_gibs(*dog, pose.data(), zref::fx16{dog_inst.x}, zref::fx16{dog_inst.y},
                         zref::fx16{dog_inst.z}, 0x600DF00Du, gibs);
        }
      }
      // ground snap (root sits on the leg length; the skeleton carries 0.52)
      const zref::terrain::ColumnResult col =
          zref::terrain::column_query(lat, zref::fx16{dog_inst.x}, zref::fx16{dog_inst.z});
      if (col.cls == zref::terrain::ColumnClass::kSolid) dog_inst.y = col.top.raw;
      // gib ballistics (integer)
      for (auto it = gibs.begin(); it != gibs.end();) {
        it->x += it->vx * 8;
        it->y += it->vy * 8;
        it->z += it->vz * 8;
        it->vy -= gib_gravity;
        if (it->y < dog_inst.y - (2 << 16)) {
          it = gibs.erase(it);
        } else {
          ++it;
        }
      }
      dog_poses.begin_frame();
    }

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
      // are per-subject (the island scenes stand much further back; creature
      // subjects lerp them for the LOD pull-back)
      sv.payload.view_projection = cam_pitch(sub.cam_k, cam_eye, cam_dist, 28732, 58903,
                                             cam_bias, -1, shake_raw);
      if (dog != nullptr) {
        // the creature hook consumes the SAME matrix (16 raws, ABI -> zref)
        const int32_t* mm = &sv.payload.view_projection.m00;
        for (int i = 0; i < 4; ++i)
          for (int j = 0; j < 4; ++j) cr_ctx.vp.m[i][j] = zref::fx16{mm[i * 4 + j]};
      }
      std::vector<uint8_t> v1;
      zhao_abi::zhao_pack_set_view(sv, v1);
      b.append_record(v1);

      // sky rotation: the terrain camera's own rows (pitch 26 deg down,
      // zsign -1) so the sky horizon and the terrain horizon agree; in
      // sky-sweep mode the pitch ping-pongs pitch0 -> pitch1 -> pitch0
      // across the loop (integer lerp on angle16 turns, table sin/cos)
      int32_t sky_ps = 28732, sky_pc = 58903, sky_bias = cam_bias;
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
  s.expect_seq_crc = 0xE89BB76Bu;  // re-pinned 2026-08-16: kBandRows 8->16
  // (dcb32ff, the banding fix). The SHIPPED gif is the pre-fix capture
  // (sequence_crc32c=0x0222090B, provenance in zhaozhou-site/scratch-reel)
  // kept by owner instruction; this constant tracks the current renderer.
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
  s.expect_seq_crc = 0x7E07D08Au;  // re-pinned 2026-08-16: kBandRows fix (dcb32ff); shipped gif is pre-fix (0x4F97AD9B)
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
  s.expect_seq_crc = 0x106B4DE8u;  // re-pinned 2026-08-16: kBandRows fix (dcb32ff); shipped gif is pre-fix (0x86069EA1)
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
  s.expect_seq_crc = 0x839E117Fu;  // re-pinned 2026-08-16: kBandRows fix (dcb32ff); shipped gif is pre-fix (0x47D4D163)
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
  s.expect_seq_crc = 0x074B5DCAu;  // re-pinned 2026-08-16: kBandRows fix (dcb32ff)
  // — this subject IS the banding demo, so the gif was re-shot with the fix
  return s;
}

// 6. star-boil — S03 red giant ENLARGED for legibility. The CLUT rotation
// (color cycling through the ramp) is the animation. 80 px disc radius
// makes the boil visible at gallery scale.
SceneSubject subject_starboil() {
  SceneSubject s;
  s.name = "star-boil";
  s.frames = 63;
  s.step = 6;  // rot = (tick/3) mod 63 = 2f mod 63 -> f=63 wraps to 0
  s.celestial = 1;
  s.space = true;
  s.note =
      "S03 red giant at 1.5 radii; granulation is a 63-entry palette rotation, "
      "zero texels rewritten per frame; ENLARGED to 80 px disc radius for "
      "legibility (was invisible at gallery scale)";
  s.expect_seq_crc = 0xADC6EB7Cu;  // pinned 2026-08-16 (80 px legibility scale;
  // identical before/after trails — the star is at rest, static-skip emits nothing)
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
  s.expect_seq_crc = 0xD20023CDu;  // re-pinned 2026-08-16: §15 trail (the smear
  // the owner asked for; was 0x9448C485 before trails)
  return s;
}

// 8. pulsar — S11: ENLARGED core for legibility. The duty strobe is now
// clearly visible at gallery scale.
SceneSubject subject_pulsar() {
  SceneSubject s;
  s.name = "pulsar";
  s.frames = 64;
  s.step = 8;
  s.celestial = 3;
  s.space = true;
  s.note =
      "S11 pulsar at 40 radii; the flare strobes on the S2 duty law "
      "(spin_phase < 0x4000, one quarter of each rotation); ENLARGED to "
      "28 px disc radius for legibility (was 4 px, read as 'a dot phasing')";
  s.expect_seq_crc = 0x6F2A61FCu;  // pinned 2026-08-16 (28 px legibility scale;
  // §15 static: the strobe is the subject, no trail)
  return s;
}

// 10. blue-giant — S01: large hot blue star, 15k radius, bright white-blue
SceneSubject subject_bluegiant() {
  SceneSubject s;
  s.name = "blue-giant";
  s.frames = 64;
  s.step = 8;
  s.celestial = 5;  // new celestial mode: S01 class
  s.space = true;
  s.note =
      "S01 blue giant at 20 radii; large hot star with bright blue-white "
      "colour (30,50,63 VGA); compact corona and burst flare";
  s.expect_seq_crc = 0xDFAFCD70u;  // pinned 2026-08-16 (first render, trailed)
  return s;
}

// 11. white-dwarf — S02: compact hot star, 300 radius, very fast spin
SceneSubject subject_whitedwarf() {
  SceneSubject s;
  s.name = "white-dwarf";
  s.frames = 64;
  s.step = 8;
  s.celestial = 6;  // S02 class
  s.space = true;
  s.note =
      "S02 white dwarf at 2 radii; compact white star (63,63,63) with rapid "
      "spin; five-pass box-smooth granulation; drifts with a long smear";
  s.expect_seq_crc = 0x048AB345u;  // pinned 2026-08-16 (first render, trailed)
  return s;
}

// 12. orange-giant — S04: warm giant, 15k radius, golden orange
SceneSubject subject_orangegiant() {
  SceneSubject s;
  s.name = "orange-giant";
  s.frames = 64;
  s.step = 8;
  s.celestial = 7;  // S04 class
  s.space = true;
  s.note =
      "S04 orange giant at 2.5 radii; warm giant star with golden orange colour "
      "(63,55,32); drifts with a white-hot smear fading to orange at the fringe";
  s.expect_seq_crc = 0x66299B68u;  // pinned 2026-08-16 (first render, trailed)
  return s;
}

// 13. blue-dwarf — S07: compact hot star, 2k radius, fast spin
SceneSubject subject_bluedwarf() {
  SceneSubject s;
  s.name = "blue-dwarf";
  s.frames = 64;
  s.step = 8;
  s.celestial = 8;  // S07 class
  s.space = true;
  s.note =
      "S07 blue dwarf at 2 radii; compact deep blue star (10,20,63); the drift "
      "smear grades white to deep blue along the tail";
  s.expect_seq_crc = 0xD3355069u;  // pinned 2026-08-16 (first render, trailed)
  return s;
}

// 14. multiple — S08: binary system, 4k radius, orange-yellow primary
SceneSubject subject_multiple() {
  SceneSubject s;
  s.name = "multiple";
  s.frames = 64;
  s.step = 8;
  s.celestial = 9;  // S08 class
  s.space = true;
  s.note =
      "S08 multiple system: two bodies of one class orbiting the barycentre, "
      "one revolution per loop, each with a curved trail (the §15 showpiece)";
  s.expect_seq_crc = 0xCA637ABDu;  // pinned 2026-08-16 (first render, trailed)
  return s;
}

// 15. infant — S09: young protostar, 1.5k radius, variable undertone
SceneSubject subject_infant() {
  SceneSubject s;
  s.name = "infant";
  s.frames = 64;
  s.step = 8;
  s.celestial = 10;  // S09 class
  s.space = true;
  s.note =
      "S09 infant star at 2 radii; young protostar, purple (48,32,63), "
      "per-identity undertone; drifts with a purple smear";
  s.expect_seq_crc = 0x5FBE7C1Bu;  // pinned 2026-08-16 (first render, trailed)
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
  s.expect_seq_crc = 0x4382E5C8u;  // re-pinned 2026-08-16: kBandRows fix (dcb32ff)
  // — re-shot; trails do not apply at the glint rung (see cel_hook)
  return s;
}

// 16. creature-wave-walk — the creature-lane identity shot: the watchdog
// walks the island while a travelling wave passes UNDER it; two
// column_query taps per tick tilt it through the wave (rotateOnGround);
// then the camera pulls back and the LOD ladder walks it down
// mesh -> micro-mesh -> splat -> glint with 10%/15-tick hysteresis.
SceneSubject subject_creaturewalk() {
  SceneSubject s;
  s.name = "creature-wave-walk";
  s.frames = 96;
  s.step = 8;
  s.creature = 1;
  // near phase: camera aims AT the creature (aim y = eye - 0.4877*dist =
  // 8 m, the bump-patch crown it walks on); focal 3.36 puts the ~1 m
  // watchdog at ~70 px (the legibility rule)
  s.bump_ext = 6;
  s.cam_k = 320000;
  s.cam_eye = 12;
  s.cam_dist = 8;
  s.cam_bias = 0;
  // pull-back phase (frames 48..95): dist 8 -> 300 m walks the ladder
  // (micro ~17 m, splat ~155 m, glint ~310 m with hysteresis; the last
  // frames hold the glint rung)
  s.cam_pull = true;
  s.cam_pull0 = 48;
  s.cam2_eye = 150;
  s.cam2_dist = 300;
  s.cam2_bias = 0;
  FieldSpec fs;
  fs.program = 6;        // wave_pool
  fs.p[0] = fxm(500);    // centre x 0.5 m (on the walking line)
  fs.p[1] = fxm(1000);   // centre z 1.0 m
  fs.p[2] = fxm(180);    // k = 0.18 turns/m (wavelength ~5.6 m)
  fs.p[3] = fxm(3000);   // n = 3 cycles per loop
  fs.p[4] = fxm(500);    // amplitude 0.5 m (focal 4.88: +-55 px bob at the
                         // near camera; crest slope 0.56 tilts it ~29 deg)
  fs.p[5] = fxm(5500);   // envelope: full inside 5.5 m
  fs.p[6] = fxm(12200);  // fades to zero at the island rim
  fs.p[7] = 0;
  fs.fx0 = fxm(-12000);
  fs.fz0 = fxm(-12000);
  fs.fx1 = fxm(12000);
  fs.fz1 = fxm(12000);
  fs.start_tick = 0;
  fs.duration = s.frames * s.step;
  s.fields.push_back(fs);
  s.note =
      "watchdog quadruped (7 bones, 6 ring parts) walks east at 0.022 m/frame; "
      "a 1.2 m wave crosses under it and two column_query taps per tick tilt "
      "it through the crest; frames 48+ pull the camera back 9 m -> 320 m and "
      "the LOD ladder walks it down mesh -> micro-mesh -> splat -> glint "
      "(screen-space error, 10% hysteresis, 15-tick hold)";
  s.expect_seq_crc = 0x33782CB8u;  // pinned 2026-08-16 (first render)
  return s;
}

// 17. creature-bulk-pop — bulk inflation then the gib burst: the watchdog
// idles while its root SCALE inflates 1.0 -> ~2.3; crossing the species pop
// threshold (2.2) removes the mesh and spawns the PART.SPAWN burst
// (deterministic noise2 velocities, integer ballistics).
SceneSubject subject_creaturepop() {
  SceneSubject s;
  s.name = "creature-bulk-pop";
  s.frames = 72;
  s.step = 8;
  s.creature = 2;
  // fixed camera on the bump-patch crown (aim y = 8.1 m; the inflated 2.3x
  // creature reaches ~135 px before the pop)
  s.bump_ext = 6;
  s.cam_k = 220000;
  s.cam_eye = 12;
  s.cam_dist = 8;
  s.cam_bias = 0;
  s.note =
      "watchdog idles (32-key breathing clip) while bulk inflates the root "
      "scale 1.0 -> 2.3 (exponential smoothing, one scalar); crossing the "
      "2.2 pop threshold removes the mesh and bursts 64 gibs with "
      "noise2-derived velocities and integer ballistics";
  s.expect_seq_crc = 0x00889F52u;  // pinned 2026-08-16 (first render)
  return s;
}

}  // namespace

// Library catalogue for --list
struct LibraryEntry {
  const char* id;
  const char* name;
  const char* description;
  bool implemented;
};

constexpr LibraryEntry kLibrary[] = {
  {"star-s00-yellow", "Yellow star", "Classic main sequence star with full lens flare chain", true},
  {"star-s03-red-giant", "Red giant", "Large cool star with boiling CLUT rotation", true},
  {"star-s11-pulsar", "Pulsar", "Compact neutron star with duty-cycle strobe", true},
  {"terrain-wave", "Wave pool", "Travelling radial wave, two full cycles per loop", true},
  {"terrain-impact", "Impact wave", "Expanding annular wave with debris and screen shake", true},
  {"terrain-crater", "Crater ring", "Static crater with charred core and cracked ring", true},
  {"terrain-scars", "Scars accumulation", "Three strikes with persistent surface-sheet scars", true},
  {"terrain-breach", "Breach", "Dual-heightfield island with pit through 22 m thickness", true},
  {"celestial-sky-sweep", "Sky sweep", "Camera pitch sweep horizon to zenith", true},
  {"celestial-flare-occlusion", "Flare occlusion", "Sun crosses behind island with 15-frame fade", true},

  // Newly implemented stars (2026-08-16)
  {"star-s01-blue-giant", "Blue giant", "Large hot star 15k radius, bright blue-white", true},
  {"star-s02-white-dwarf", "White dwarf", "Compact hot star 300 radius, fast spin", true},
  {"star-s04-orange-giant", "Orange giant", "Warm giant 15k radius, golden orange", true},
  {"star-s07-blue-dwarf", "Blue dwarf", "Compact hot star 2k radius, fast spin", true},
  {"star-s08-multiple", "Multiple", "Binary star system 4k radius", true},
  {"star-s09-infant", "Infant star", "Young protostar with variable undertone", true},

  // Creature/character lane (spec/creature_rules.md)
  {"creature-wave-walk", "Creature wave walk",
   "Ring-mesh quadruped tilts through a wave, LOD ladder to glints", true},
  {"creature-bulk-pop", "Bulk inflate and pop",
   "Root-scale bulk inflation crossing the pop threshold into gibs", true},

  // Dead classes (no flare capability, stub entries only)
  {"star-s05-brown-dwarf", "Brown dwarf", "Dim substellar object, no flare capability", false},
  {"star-s06-grey-giant", "Grey giant", "Low luminosity giant, no flare", false},
  {"star-s10-runaway", "Runaway", "High-velocity star, no flare capability", false},
  {nullptr, nullptr, nullptr, false}
};

int main(int argc, char** argv) {
  // --list: enumerate the library catalogue
  if (argc > 1 && std::strcmp(argv[1], "--list") == 0) {
    std::printf("=== Zhaozhou Effects Library ===\n\n");
    std::printf("%-25s %-20s %-50s %s\n", "ID", "Name", "Description", "Status");
    std::printf("%s\n", std::string(100, '-').c_str());
    for (const LibraryEntry* e = kLibrary; e->id; ++e) {
      std::printf("%-25s %-20s %-50s %s\n", e->id, e->name, e->description,
                  e->implemented ? "READY" : "TODO");
    }
    std::printf("\nRender with: zhao-reel <output-dir> <subject-id>\n");
    std::printf("CRC check: zhao-reel --check\n");
    return 0;
  }

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
    // New star classes (CRCs not yet pinned - will fail until first render)
    rc |= render_scene(subject_bluegiant());
    rc |= render_scene(subject_whitedwarf());
    rc |= render_scene(subject_orangegiant());
    rc |= render_scene(subject_bluedwarf());
    rc |= render_scene(subject_multiple());
    rc |= render_scene(subject_infant());
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
  // New star classes
  if (wanted("blue-giant")) rc |= render_scene(subject_bluegiant());
  if (wanted("white-dwarf")) rc |= render_scene(subject_whitedwarf());
  if (wanted("orange-giant")) rc |= render_scene(subject_orangegiant());
  if (wanted("blue-dwarf")) rc |= render_scene(subject_bluedwarf());
  if (wanted("multiple")) rc |= render_scene(subject_multiple());
  if (wanted("infant")) rc |= render_scene(subject_infant());
  if (wanted("creature-wave-walk")) rc |= render_scene(subject_creaturewalk());
  if (wanted("creature-bulk-pop")) rc |= render_scene(subject_creaturepop());
  return rc;
}
