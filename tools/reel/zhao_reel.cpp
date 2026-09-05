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
#include "zref/zref_star.hpp"      // celestial compositor preview (stars_and_flares.md)
#include "zref/zref_terrain.hpp"   // dual-heightfield bake/breach reference
#include "zrender/internal.hpp"    // compose_lattice for the sim-side tilt taps

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
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
  } else if (variant == 2) {
    // FULLY flat: every band, the cap and the under-plane are one colour.
    // Trivially C0 under S1.2 (all joins are equalities).
    //
    // Variant 1 only flattens the UPPER band, which is enough for a subject
    // whose additive chain sits high. It is not enough for a large corona
    // that descends THROUGH the horizon: the halo then crosses the lower
    // band's whole gradient, and since the sequence palette counts the union
    // over every frame, the product is halo levels x sky rows. Measured on
    // the atmosphere pair at 395 and 408 unique colours against a ceiling of
    // 256.
    //
    // The colour is deliberately NEAR-NEUTRAL rather than a warm dusk. These
    // subjects exist to compare a whitening ramp against a reddening one, and
    // a warm sky would flatter the reddening one before the comparison
    // started. A loaded test is worse than no test.
    const zref::sky::SkyColor flat = {110, 96, 104};
    s.background = flat;
    s.under = flat;
    s.band_lower_horizon = flat;
    s.band_lower_top = flat;
    s.band_upper_bottom = flat;
    s.band_upper_top = flat;
    s.cap = flat;
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

// fx16 4x4 matrix product (row-major, points as columns): each element is
// the exact s128 sum of four 32x32 products with ONE >>16 rescale - the
// qformats 2 arithmetic, applied to whole matrices instead of vectors.
zhao_abi::ZhMat4fx mat4_mul(const zhao_abi::ZhMat4fx& a, const zhao_abi::ZhMat4fx& b) {
  const int32_t* A = &a.m00;
  const int32_t* B = &b.m00;
  zhao_abi::ZhMat4fx r;
  int32_t* R = &r.m00;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) {
      __int128 sum = 0;
      for (int k = 0; k < 4; ++k) sum += static_cast<__int128>(A[i * 4 + k]) * B[k * 4 + j];
      R[i * 4 + j] = static_cast<int32_t>(sum >> 16);
    }
  return r;
}

// World yaw for the orbit camera: rotate the world by -theta about Y so the
// pitched camera (which sits at azimuth 0, +D on the sunlit -z side) sees
// the island from azimuth theta. Integer sin/cos from the ONE trig tables.
zhao_abi::ZhMat4fx rot_world_yaw(uint16_t theta_turns) {
  const int32_t c = zref::fx_cos(zref::angle16{theta_turns}).raw;
  const int32_t sn = zref::fx_sin(zref::angle16{theta_turns}).raw;
  const int32_t m[16] = {c, 0, -sn, 0, 0, 1 << 16, 0, 0, sn, 0, c, 0, 0, 0, 0, 1 << 16};
  return rtest::mat(m);
}

// World translation (Q16.16 metres) for the TRACKING camera: composing
// view * T shifts the whole world before the pitched projection, which is a
// true camera translation -- and translation does not move a sky at
// infinity, so the sky matrices are correctly left alone.
zhao_abi::ZhMat4fx mat_world_translate(int32_t tx, int32_t ty, int32_t tz) {
  const int32_t m[16] = {1 << 16, 0, 0, tx, 0, 1 << 16, 0, ty,
                         0, 0, 1 << 16, tz, 0, 0, 0, 1 << 16};
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
  zref::star::Sprite8 face;    // starface for the subject's class
  zref::star::Sprite8 corona;  // corona variant
  uint8_t ramp[64][3];         // built ramp (slew state at targets)
  std::vector<zref::star::GlintPoint> glints;
};

struct SceneSubject;  // defined below (the celestial fns follow it)

struct CelCtx {
  const SceneSubject* sub = nullptr;
  uint32_t frame = 0;
  CelAssets* assets = nullptr;
  zref::star::FlareSlots slots;        // persists across the loop (the fade)
  zref::star::TrailHistory trails[2];  // §15 rings, persist across the loop
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

// ===========================================================================
// PLANETSIDE SUNS — Noctis's actual technique, not a sprite over a gradient
// ===========================================================================
//
// The owner asked for the sun as seen from a planet's SURFACE, and the first
// attempt (atmo-sun-donor / atmo-sun-thick) got it wrong in a way worth
// stating: it drew a coloured disc with a halo over an RGB sky. On a world with
// real air there IS no disc. There is a formless bloom near the horizon
// bleeding into a mottled sky, and it cannot be produced by compositing a
// coloured sprite over a background.
//
// Read out of the donor's own surface renderer (noctis-1.cpp / noctis-0.cpp;
// the study is in untitled-game/docs/NOCTIS-SURFACE-NOTES.md), the mechanism is:
//
//   1. THE SKY IS A SIX-BIT INTENSITY PLANE, not RGB. `s_background` holds
//      0..63 per pixel over a panoramic strip and is coloured through a palette
//      only at the very end.
//   2. THE SUN IS ADDED INTO THAT PLANE. `white_sun` rasterises an additive,
//      linearly-falling radial splat straight into the sky's intensity and
//      saturates at 63 — BEFORE anything is coloured.
//   3. SO THE SUN HAS NO COLOUR OF ITS OWN. It saturates to 63, and 63 is the
//      sky palette's own peak entry. "Different planet, different sky,
//      different sun" is ONE mechanism, not two.
//   4. ATMOSPHERE IS ONE NUMBER. With air the splat has no flat core at all
//      (fgm_factor 0); without air it keeps a hard saturated core and reads as
//      a disc with a skirt.
//   5. THE FORMLESS LOOK IS EMERGENT from additive-plus-clamp. Where the sky
//      under the splat is already bright, the sum rails over a wide area IN THE
//      SKY'S OWN COLOUR, so the bloom has no edge anywhere. Alpha-blend a
//      sprite instead and the effect cannot happen at any radius.
//
// This is the technique, reimplemented in integer arithmetic; no donor asset or
// data is used. It also fixes the palette problem that forced the first attempt
// onto a flat sky: sky and sun together select at most 64 colours however large
// the bloom gets, because they share one plane and one ramp.
//
// The vertical ramp (brightest at the horizon, falling toward the zenith) is
// the donor's `crcy = s_background[p] * cpos / bk_lines_to_horizon`. The
// mottling is our own PCG value noise smoothed twice, standing in for
// `nebular_sky`'s middle-square fill plus its smoothers — the donor's own note
// is that the character comes from the smoothing, not the generator.

struct PlanetSky {
  uint8_t ramp[64][3] = {};  // intensity -> RGB. ramp[63] IS the sun's colour.
  int32_t horizon_y = 150;   // screen row the ground meets the sky
  int32_t sun_x = 192, sun_y = 150;
  int32_t sun_mag = 120;   // splat radius, px
  int32_t sun_core = 0;    // flat saturated core, px. 0 = has an atmosphere
  // A SECOND sun. The donor carries one too (`secondarysun`, with its own
  // pri_x/pri_y/pri_z), and it costs nothing here because both splats add into
  // the same plane and the clamp resolves the overlap: where two blooms meet
  // the sum simply rails, in the sky's own peak colour, exactly as one bloom
  // does against itself. Two suns therefore add ZERO palette entries.
  int32_t sun2_mag = 0;  // 0 = single star system
  int32_t sun2_core = 0;
  int32_t sun2_x = 0, sun2_y = 0;
  uint8_t base = 40;       // sky_brightness before the ramp, 0..63
  uint8_t noise_amp = 10;  // mottling, 0 disables
  uint32_t seed = 1;
};

// PCG value noise on an 8x8 lattice, smoothed — deterministic, integer only.
uint8_t sky_noise(int32_t x, int32_t y, uint32_t seed) {
  const auto h = [&](int32_t cx, int32_t cy) -> uint32_t {
    uint32_t v = static_cast<uint32_t>(cx) * 0x9E3779B9u ^ static_cast<uint32_t>(cy) * 0x85EBCA6Bu ^
                 seed * 0xC2B2AE35u;
    v ^= v >> 15;
    v *= 0x2545F491u;
    v ^= v >> 13;
    return v;
  };
  const int32_t gx = x >> 3, gy = y >> 3;
  const int32_t fx = x & 7, fy = y & 7;
  // bilinear over the four lattice corners: the two smoothing passes the donor
  // runs, folded into the interpolation rather than done as separate sweeps
  const uint32_t a = (h(gx, gy) >> 24) & 63u;
  const uint32_t b = (h(gx + 1, gy) >> 24) & 63u;
  const uint32_t c = (h(gx, gy + 1) >> 24) & 63u;
  const uint32_t d = (h(gx + 1, gy + 1) >> 24) & 63u;
  const uint32_t top = (a * (8 - fx) + b * fx) >> 3;
  const uint32_t bot = (c * (8 - fx) + d * fx) >> 3;
  return static_cast<uint8_t>((top * (8 - fy) + bot * fy) >> 3);
}

/** Build a 64-entry ramp from three control colours: the deep sky at 0, the
 *  mid body, and the peak at 63 — which is what the sun becomes. */
void planet_ramp(uint8_t out[64][3], const uint8_t lo[3], const uint8_t mid[3],
                 const uint8_t hi[3]) {
  for (int i = 0; i < 64; ++i) {
    for (int c = 0; c < 3; ++c) {
      int32_t v;
      if (i < 40) {
        v = lo[c] + (static_cast<int32_t>(mid[c] - lo[c]) * i + 20) / 40;
      } else {
        v = mid[c] + (static_cast<int32_t>(hi[c] - mid[c]) * (i - 40) + 12) / 24;
      }
      out[i][c] = static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
    }
  }
}

/** The pre-resolve hook: paint every sky pixel from the intensity plane.
 *  Depth is Q16.16 1/w with larger = closer and the sky backdrop at 0, so a
 *  pixel with depth 0 is sky and everything else is a real surface we leave
 *  alone. */
void planet_sky_hook(void* vctx, uint8_t* rgb, int32_t* depth, uint32_t w, uint32_t h,
                     uint32_t tick) {
  (void)tick;
  const PlanetSky& p = *static_cast<const PlanetSky*>(vctx);
  const int32_t hy = p.horizon_y > 1 ? p.horizon_y : 1;
  for (uint32_t y = 0; y < h; ++y) {
    for (uint32_t x = 0; x < w; ++x) {
      const size_t i = static_cast<size_t>(y) * w + x;
      if (depth[i] != 0) continue;  // a real surface: not ours to paint

      // 1. The vertical ramp. The donor brightens toward the horizon and stops
      //    there because you are standing on ground. Here the world is a
      //    floating island with open sky BELOW it too, so the ramp falls away
      //    again under the horizon rather than holding flat -- the horizon
      //    becomes a band of light with dark above and dark below, which is
      //    what an island hanging in air should look like.
      int32_t v;
      if (static_cast<int32_t>(y) <= hy) {
        v = (static_cast<int32_t>(p.base) * static_cast<int32_t>(y)) / hy;
      } else {
        const int32_t below = static_cast<int32_t>(h) - hy;
        const int32_t d = static_cast<int32_t>(y) - hy;
        v = static_cast<int32_t>(p.base) - (static_cast<int32_t>(p.base) * d) /
                                               (below > 0 ? below : 1);
      }

      // 2. the mottling
      if (p.noise_amp > 0) {
        // Two octaves. One alone showed its 8 px lattice as visible squares;
        // the coarse octave carries the cloud masses and the fine one breaks
        // the edges up, which is the job the donor's repeated smoothing passes
        // do to its middle-square fill.
        const int32_t xi = static_cast<int32_t>(x), yi = static_cast<int32_t>(y);
        const int32_t n0 = static_cast<int32_t>(sky_noise(xi >> 1, yi >> 1, p.seed));
        const int32_t n1 = static_cast<int32_t>(sky_noise(xi << 1, yi << 1, p.seed ^ 0x5A17u));
        const int32_t n = ((n0 - 32) * 3 + (n1 - 32)) / 4;
        v += (n * p.noise_amp) / 32;
      }

      // 3. THE SUN, added into the plane and saturating there
      const int32_t dx = static_cast<int32_t>(x) - p.sun_x;
      const int32_t dy = static_cast<int32_t>(y) - p.sun_y;
      const int32_t d2 = dx * dx + dy * dy;
      if (d2 < p.sun_mag * p.sun_mag) {
        const int32_t d = static_cast<int32_t>(zref::isqrt_u32(static_cast<uint32_t>(d2)));
        int32_t add;
        if (d <= p.sun_core) {
          add = 63;
        } else {
          const int32_t span = p.sun_mag - p.sun_core;
          add = (63 * (p.sun_mag - d) + span / 2) / (span > 0 ? span : 1);
        }
        v += add;
      }

      // 3b. the companion, if this system has one
      if (p.sun2_mag > 0) {
        const int32_t dx2 = static_cast<int32_t>(x) - p.sun2_x;
        const int32_t dy2 = static_cast<int32_t>(y) - p.sun2_y;
        const int32_t dd2 = dx2 * dx2 + dy2 * dy2;
        if (dd2 < p.sun2_mag * p.sun2_mag) {
          const int32_t d = static_cast<int32_t>(zref::isqrt_u32(static_cast<uint32_t>(dd2)));
          if (d <= p.sun2_core) {
            v += 63;
          } else {
            const int32_t span = p.sun2_mag - p.sun2_core;
            v += (63 * (p.sun2_mag - d) + span / 2) / (span > 0 ? span : 1);
          }
        }
      }

      if (v < 0) v = 0;
      if (v > 63) v = 63;  // the clamp that makes the bloom formless
      rgb[i * 3 + 0] = p.ramp[v][0];
      rgb[i * 3 + 1] = p.ramp[v][1];
      rgb[i * 3 + 2] = p.ramp[v][2];
    }
  }
}

// The planet table. Each row is one world: the deep sky, the mid body, and the
// PEAK — and the peak is the sun, because the sun saturates the plane to 63 and
// 63 is this entry. Changing a row changes the sky and the sun together, which
// is the point: one island, one sky, one sun, one set of numbers.
//
// `core` is the atmosphere in a single value. 0 means real air and no
// resolvable disc at all. A nonzero core is a thin or absent atmosphere, where
// the splat keeps a hard saturated centre and reads as a disc with a skirt.
struct PlanetDef {
  const char* name;
  uint8_t lo[3], mid[3], hi[3];
  uint8_t base;       // sky brightness at the horizon before the sun
  uint8_t noise;      // mottling amplitude
  int32_t mag;        // splat radius px
  int32_t core;       // saturated core px: 0 = thick atmosphere
};

const PlanetDef kPlanets[] = {
    // 1. the thick violet world: the look the owner pointed at. A wide bloom
    //    with no core, sitting low, bleeding up through a mottled indigo sky.
    {"violet-thick", {14, 10, 46}, {70, 44, 132}, {255, 214, 240}, 38, 12, 190, 0},
    // 2. a breathable blue world. Thinner air: less mottle, a tighter bloom.
    {"terran-blue", {18, 34, 78}, {96, 140, 196}, {255, 246, 214}, 34, 6, 150, 0},
    // 3. a dust world. The atmosphere itself is the colour, so the bloom is
    //    barely distinguishable from the sky it sits in.
    {"dust-ochre", {40, 22, 10}, {150, 88, 34}, {255, 226, 168}, 44, 14, 210, 0},
    // 4. a methane world, and the reason the ramp is three points rather than
    //    two: the mid body carries the identity, the peak stays near-white.
    {"methane-teal", {8, 28, 30}, {48, 132, 110}, {214, 255, 240}, 30, 10, 170, 0},
    // 5. NO ATMOSPHERE. Same machinery, core nonzero: a hard white disc with a
    //    short skirt against an almost black sky. This is the control that
    //    shows the other four are doing something.
    {"airless-grey", {2, 2, 4}, {26, 26, 32}, {255, 255, 255}, 8, 3, 56, 30},
    // 6. a red dwarf's world: dim, deep, and the sun never gets past orange
    //    because the peak entry itself is orange.
    {"ember-red", {20, 4, 6}, {112, 26, 18}, {255, 168, 96}, 26, 11, 200, 0},
    // ---- the big ones. A star close enough to fill the sky is the whole
    //      reason this technique is worth having: a 420 px splat on a 384x240
    //      frame saturates most of the visible dome, and because the sun IS
    //      the sky's peak entry that costs no extra colours at all. Try the
    //      same picture with a sprite and the palette is gone.
    // 7. an amber giant seen from close in: the bloom is larger than the frame
    //    and the sky never gets dark anywhere.
    {"giant-amber", {60, 26, 8}, {186, 110, 30}, {255, 240, 200}, 46, 9, 420, 0},
    // 8. a blue supergiant. Big, brutal, and the ramp's peak is near-white, so
    //    the core reads as glare rather than as a coloured object.
    {"supergiant-blue", {10, 20, 60}, {74, 132, 214}, {244, 250, 255}, 40, 7, 340, 0},
    // 9. a swollen red giant filling the sky of a dim world. The peak stays
    //    orange, so even at full saturation nothing on this world is ever white.
    {"redgiant-swollen", {28, 6, 4}, {150, 44, 20}, {255, 150, 70}, 42, 13, 480, 0},
    // 10. a pale close star through thin haze: big but weak, so the bloom is
    //     broad and never rails except at its very centre.
    {"pale-close", {26, 26, 34}, {120, 124, 140}, {255, 252, 240}, 24, 8, 300, 0},
    // 11. a binary's world, and it needed its OWN row. Pointing the two-sun
    //     subject at `pale-close` railed the entire frame to white: two 300 px
    //     and 150 px blooms over a base of 24 leave nowhere unsaturated, and
    //     two suns you cannot tell apart are worse than one. A darker sky and
    //     two smaller, well-separated stars keep both blooms readable AND the
    //     dark band between them, which is the thing worth showing.
    {"binary-pair", {6, 8, 22}, {58, 74, 128}, {255, 244, 226}, 16, 9, 130, 0},
};
constexpr int kPlanetCount = static_cast<int>(sizeof(kPlanets) / sizeof(kPlanets[0]));

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
      cls = 11;    // S11 pulsar
      core16 = 8;  // halo_airless: hard core + short skirt — few ring
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
    case 11:
    case 12:
      cls = 0;     // S00 yellow star, the sun over a world
      core16 = 0;  // halo_atmo: surface sun w/ atmosphere (§4)
      break;
  }
  const zref::star::StarClass& c = zref::star::kGamut[cls];
  const zref::star::StarIdentity id = cel_identity(cls, 0xA11CE5u);
  zref::star::RampState rs;
  zref::star::ramp_retarget(rs, id);  // snap: reel ramps sit at targets
  if (celestial == 12) {
    // THE THICK-ATMOSPHERE CORRECTION — a transmission filter on the ramp's
    // control points, applied before the ramp is built.
    //
    // FIRST ATTEMPT, AND WHY IT FAILED, because the failure is the useful part.
    // §3 freezes the ramp's top control point at P3 = (256, 280, 304): every
    // channel over-ranges the 0..255 clamp, so the top of the ramp whitens by
    // deliberate early saturation. That is wrong for thick air, so the obvious
    // correction is to redden P3 alone. Rendered, it is INVISIBLE — the pair
    // was indistinguishable at every frame.
    //
    // Two reasons, both worth keeping:
    //   1. §4's corona falls off LINEARLY in radius, so ramp index is roughly
    //      linear in distance from the centre. On a 104 px halo the top of the
    //      ramp covers a few pixels and the mid and low entries carry the
    //      entire visible glow. P3 governs [40..64) — a quarter of the ramp
    //      that is almost none of the picture.
    //   2. The corona composites ADDITIVELY (§4 star_halo_additive,
    //      dst = sat(dst + src)). Near the core every channel rails no matter
    //      what colour went in, so the brightest region is white either way.
    //      Additive saturation eats exactly the correction P3 was making.
    //
    // The physically-suggestive model is also the one that survives both:
    // thick air is a per-wavelength TRANSMISSION, so it attenuates the star's
    // colour at every intensity, not only at the top. Applying it to the
    // control points leaves §3's segment law and its single rounding entirely
    // untouched — only the inputs change — and it costs three multiplies at
    // bake time, once, for the whole ramp.
    //
    // (1.0, 0.60, 0.25): red passes, green is halved, blue is mostly gone.
    // P0 stays black; black is black through any depth of air.
    static const int32_t kTrans[3] = {256, 154, 64};  // /256
    for (int pt = 1; pt < 4; ++pt)
      for (int ch = 0; ch < 3; ++ch) {
        const int idx = pt * 3 + ch;
        const int32_t v = (static_cast<int32_t>(rs.cur[idx]) * kTrans[ch] + 128) / 256;
        rs.cur[idx] = rs.tgt[idx] = static_cast<int16_t>(v);
      }
  }
  zref::star::ramp_build(rs.cur, a.ramp);
  if (celestial == 2) {
    // The flare subject shares one global GIF palette with its ramp smear.
    // Pair adjacent entries here, at authoring time, so the exact six-bit
    // reconstruction remains intact while the published reel stays <= 256.
    for (uint32_t i = 1; i < 64; i += 2)
      for (int ch = 0; ch < 3; ++ch) a.ramp[i][ch] = a.ramp[i - 1][ch];
  }
  a.face = zref::star::starface(id.texture_seed, c.smooth);
  a.corona = zref::star::corona_sprite(core16);
  // glints: space subjects only; exclusion rects sized per subject below
  if (celestial == 1) {
    a.glints = make_glints(false, {{44, 12, 340, 220}});  // giant + halo box
  } else if (celestial == 2 || celestial == 3 || (celestial >= 5 && celestial <= 10)) {
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

// ---- dual-heightfield island (terrain_rules.md; deep-keel wave) -----------
//
// A 161x161 lattice over +-160 m: 160x160 cells at the CANONICAL 2.0 m pitch
// (terrain_rules 1.3) - a 320 m island, the area of 25 Island-Patch pages
// (5x5 of 32x32 cells), carried as one Phase-3 envelope patch (the kind-6
// page split / sparse directory is Phase-6 loader work; stated honestly in
// meta.txt). Doubles below are ASSET AUTHORING ONLY (the bump_patch rule);
// heights quantize to 0.25 m steps so shading ladders stay inside the
// 256-colour law.
//
// [deep-keel wave] The bottom is no longer authored inline: the 3.7 keel
// default writes it (R = 151 m -> KEEL_DEPTH = 75 m at the heart, 30 m at
// the rim - the donor 50 m curtain was the FLOOR, not the target). The
// island also carries its TEXTURE layers now: layer E candidates (grass
// heart, rock coast, dithered weight ring, sand lip) and layer H tint (one
// warm variant on the sunward half - the LMAP heir at Phase-3 flat scope),
// plus the tileset id. Top: coastal plateau rising to a ~15 m heart with
// gentle swells. Coastline: R(theta) wobbles 116..151 m, outside =
// VOID_AUTHORED.

// The island tileset (terrain_rules 6.1): 256 CLUT8 64x64 tiles + one RGB565
// palette, generated with integer LCG noise - the "simple automatically
// made texture that looked like rock" the owner asked for, no authored art.
// Layout: 0..7 grass speckle, 16..23 rock, 32 sand shore, 240 STRATA (rim
// wall banding, 6.6 frozen id), 241 UNDERSIDE blotch (6.6 frozen id).
// returns a HEAP tileset: the 1 MiB container must never touch the stack
// (the Windows default stack is 1 MiB - measured the hard way)
std::unique_ptr<zref::render::Tileset> island_tileset() {
  auto tsp = std::make_unique<zref::render::Tileset>();
  zref::render::Tileset& ts = *tsp;
  // palette: 17 authored RGB565 entries (grass 0..3, rock 4..7, sand 8..9,
  // strata 10..14, underside 15..16) - small on purpose: the palette law
  // counts texel x shade x tint products, and 256 is the ceiling
  const uint8_t rgb888[][3] = {
      {86, 138, 60},   {104, 156, 70},  {122, 172, 78},  {140, 186, 88},                   // grass
      {128, 116, 98},  {110, 99, 84},   {146, 133, 112}, {96, 86, 73},                     // rock
      {176, 160, 118}, {192, 176, 132},                                                    // sand
      {122, 100, 78},  {104, 84, 64},   {140, 118, 92},  {88, 70, 54},   {150, 130, 104},  // strata
      {92, 80, 66},    {76, 66, 54},                                                       // under
  };
  const int NPAL = static_cast<int>(sizeof(rgb888) / sizeof(rgb888[0]));
  for (int k = 0; k < NPAL; ++k) {
    const uint8_t r = rgb888[k][0], g = rgb888[k][1], b = rgb888[k][2];
    ts.palette[k] = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
  }
  // integer LCG (deterministic; the constants are authoring, not law)
  uint32_t rng = 0x5EED01u;
  const auto next7 = [&rng]() {
    rng = rng * 1103515245u + 12345u;
    return (rng >> 13) & 127u;
  };
  // grass/rock/sand tiles: two-tone speckle at two scales (clumps + grain)
  for (int t = 0; t < 8; ++t) {
    const uint8_t base = static_cast<uint8_t>(t & 3);           // grass family
    const uint8_t alt = static_cast<uint8_t>((t & 3) + 1 & 3);  // neighbour tone
    for (int ty = 0; ty < 64; ++ty)
      for (int tx = 0; tx < 64; ++tx) {
        const uint32_t n = next7();
        const bool grain = n < 40;
        const bool clump = ((tx >> 3) + (ty >> 3) + t) % 3 == 0;
        ts.tiles[t][ty * 64 + tx] = (grain != clump) ? alt : base;
      }
  }
  for (int t = 16; t < 24; ++t) {
    const uint8_t base = static_cast<uint8_t>(4 + (t & 3));
    const uint8_t alt = static_cast<uint8_t>(4 + ((t & 3) + 1 & 3));
    for (int ty = 0; ty < 64; ++ty)
      for (int tx = 0; tx < 64; ++tx) {
        const uint32_t n = next7();
        ts.tiles[t][ty * 64 + tx] = (n < 52) ? alt : base;
      }
  }
  for (int ty = 0; ty < 64; ++ty)
    for (int tx = 0; tx < 64; ++tx) ts.tiles[32][ty * 64 + tx] = (next7() < 48) ? 9 : 8;
  // 240 STRATA: horizontal rock bands with per-column wobble - mirrored
  // repeat stacks the bands into geology on the wall V axis (terrain_rules 5)
  {
    int wobble[64];
    for (int tx = 0; tx < 64; ++tx) wobble[tx] = static_cast<int>(next7() % 5) - 2;
    for (int ty = 0; ty < 64; ++ty)
      for (int tx = 0; tx < 64; ++tx) {
        const int band = ((ty + wobble[tx]) >> 3) % 5;  // 8-texel bands
        ts.tiles[240][ty * 64 + tx] = static_cast<uint8_t>(10 + band);
      }
  }
  // 241 UNDERSIDE: coarse blotches of the two under tones
  for (int ty = 0; ty < 64; ++ty)
    for (int tx = 0; tx < 64; ++tx) {
      const uint32_t n = next7();
      ts.tiles[241][ty * 64 + tx] = (n < 30) ? 16 : 15;
    }
  return tsp;
}

zref::render::TerrainPatch dual_island_patch() {
  const int W = 161;
  zref::render::TerrainPatch p;
  p.width = p.height = W;
  p.env_x0 = p.env_z0 = -(160 << 16);
  p.env_x1 = p.env_z1 = (160 << 16);
  p.heights.resize(static_cast<size_t>(W) * W);
  p.scar.assign(static_cast<size_t>(W) * W, 0);
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
      const size_t k = static_cast<size_t>(j) * W + i;
      p.heights[k] = q025(top);
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
  // THE keel default (terrain_rules 3.7): the generator writes layer C.
  // 320 m island, R = 151 -> KEEL_DEPTH = 75 m heart / 30 m rim.
  zref::terrain::generate_bottom(p, 0, 0);

  // ---- texture layers: E candidates + H tint (asset authoring) ----
  p.tileset_id = 90;
  p.mat_a.assign(160 * 160, 0);
  p.mat_b.assign(160 * 160, 0);
  p.mat_w.assign(160 * 160, 0);
  uint32_t rng = 0xC0FFEEu;
  const auto next15 = [&rng]() {
    rng = rng * 1103515245u + 12345u;
    return (rng >> 17) & 15u;
  };
  for (int cj = 0; cj < 160; ++cj) {
    for (int ci = 0; ci < 160; ++ci) {
      const size_t c = static_cast<size_t>(cj) * 160 + ci;
      if ((p.cell_state[c] & zref::terrain::kSubstanceMask) != zref::terrain::kSolid) continue;
      const double x = (ci - 79.5) * 2.0, z = (cj - 79.5) * 2.0;
      const double r = std::sqrt(x * x + z * z);
      const double t = r / coast_r(x, z);  // 0 heart .. 1 rim
      // candidates: grass family in the heart, rock family at the coast;
      // the weight dithers the ring between them (Mosaic, 6.2)
      p.mat_a[c] = static_cast<uint8_t>(next15() & 7);         // grass 0..7
      p.mat_b[c] = static_cast<uint8_t>(16 + (next15() & 7));  // rock 16..23
      int w = 0;
      if (t < 0.62)
        w = 255;
      else if (t < 0.82)
        w = 160;
      else if (t < 0.92)
        w = 96;
      else if (t < 0.985)
        w = 32;
      p.mat_w[c] = static_cast<uint8_t>(w);
      // sand collar right at the lip (the beach reads the coastline)
      if (t >= 0.92 && next15() < 3) {
        p.mat_a[c] = 32;
        p.mat_b[c] = 32;
        p.mat_w[c] = 255;
      }
    }
  }
  // layer H tint: authored UNITY in this fixture, deliberately - a second
  // tint family multiplies every texel x shade product past the 256-colour
  // capture law (measured: 267 unique with a warm half; 5-bit-tint maths is
  // pinned instead by tests/texture/texture_mosaic_directed.cpp). The lane
  // is wired end to end; a Gouraud ride with locality lands in Phase 4/5.
  p.tint.assign(static_cast<size_t>(W) * W, 0xFFFF);
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

// Direction 29 clip suns: the full spec block lives beside the moving-light
// authoring below (it needs the zc alias); SceneSubject only holds a pointer.
struct ZixxSunSpec;

// S2 (creature 02): WHICH creature a subject stages. The old selector was the
// binary expression `sub.creature >= 3` (watchdog below, Zixxtrixx above),
// which a third creature cannot extend. kAuto preserves that legacy rule
// bit-for-bit for every existing subject (none of them set the field);
// creature-02 subjects set kUnnamed02 explicitly. `creature` itself stays
// the clip slot + 2.
enum class Species : uint8_t { kAuto = 0, kWatchdog, kZixxtrixx, kUnnamed02 };

struct SceneSubject {
  const char* name;
  uint32_t frames;
  uint32_t step;  // ticks per frame
  std::vector<FieldSpec> fields;
  std::vector<StampSpec> stamps;
  // world-identity wave: dual-heightfield island + deterministic bake ramp
  bool island = false;          // true: dual_island_patch (320 m, 2 m pitch)
  bool island_flat = false;     // strip the texture layers (tileset_id 0):
                                // flare-stack subjects exceed the palette
                                // law when the island texture lane is also
                                // live - their subject is the flare, not
                                // the rock (measured: 325 unique otherwise)
  std::vector<BakeStep> bakes;  // applied at frame start, in order
  // camera (defaults = the wave-2 reel constants; island scenes override).
  // cam_pull: lerp cam -> cam2 over [cam_pull0, frames) — the LOD ladder's
  // pull-back shot walks the creature down mesh -> micro -> splat -> glint.
  int32_t cam_k = 127000, cam_eye = 14, cam_dist = 33, cam_bias = 14000;
  // per-subject pitch (defaults = the wave-2 reel constants 26 deg down);
  // low-pitch island shots read the keel against the sky below the rim
  int32_t cam_ps = 28732, cam_pc = 58903;
  // orbit: yaw the world by f * (65536/frames) per frame - one exact turn
  // per loop when frames divides 65536 (the integer step keeps the loop
  // seamless); the sky rotates with it so the world-fixed sun sweeps round
  bool orbit = false;
  // constant world yaw (angle16): a FIXED three-quarter or front camera for
  // the diagnostic subjects -- same matrix as the orbit, held still. Applied
  // to the view and the sky exactly like the orbit's theta.
  int32_t cam_yaw = 0;
  bool cam_pull = false;
  uint32_t cam_pull0 = 0;
  int32_t cam2_eye = 0, cam2_dist = 0, cam2_bias = 0;  // (cam2_k == cam_k)
  int32_t cam_k_end = 0;  // Wave E LOD sweep: lerp cam_k -> cam_k_end over the subject
  // debris: spawned at spawn_frame, integer gravity fxm per frame^2
  uint32_t debris_spawn_frame = 0;
  int32_t debris_gravity = 0;      // fx raw per frame^2 (subtracted from vy)
  int32_t debris_y0 = 2 << 16;     // launch height (fx raw)
  int32_t debris_floor = 1 << 15;  // despawn line (fx raw) — a breach scene
                                   // sets this BELOW the island so debris
                                   // visibly falls through the hole
  std::vector<Debris> debris;
  // S5 (creature 02): draw the shared-ramp centre glow for this subject.
  bool u02_glow = false;
  // creature 02: run the ten-kind mana emitter tables + the bolt.
  int u02_mana = 0;  // pass 2: the mana-menu candidate (1..6; 0 = none)
  // the S5 spike's three-glow staging (diagnostic only)
  bool u02_glow_trio = false;
  // the fx tour: cycle the ten kinds solo, 60 frames each
  // the ADDLIGHTNING crackle variant: bolts + sparks every frame
  // three phase-offset conduits sharing one stage (the budget proof)
  bool u02_trio = false;
  // S3 (creature 02): DRAW_POPULATION flags. b0 points, b1 tris, b2 additive
  // (RASTER.FRAGMENT ADD). Default is the historical 0x0003 -- byte-identical.
  uint16_t pop_flags = 0x0003;
  // screen shake: raw Y offsets per frame, starting at shake_frame.
  // SCALE NOTE (2026-08-27): these are added to the projection's y row
  // CONSTANT, so the on-screen shift is shake/w NDC -- and w for the
  // creature subjects is ~10.6 m = ~693000 raw. The first Zixxtrixx jolt was
  // authored at 2100, which is 0.003 NDC: a third of a PIXEL. Mathematically
  // present, visually absent -- the crayon-grain failure mode again. A
  // visible hit needs ~0.1 NDC, i.e. tens of thousands raw at this w.
  uint32_t shake_frame = 0;
  std::vector<int32_t> shake;
  // tracking camera (creature subjects): follow the authored flight path
  bool cam_track = false;
  int32_t cam_track_num = 850;  // 1/1000 of the lift the camera follows
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
  bool space = false;   // no DrawSky (fallback black), no terrain
  int sky_variant = 0;  // dusk_sky variant (1 = flat upper band, still C0)
  // planetside sky: 0 = off (the RGB dome). >0 selects a planet from kPlanets,
  // and the six-bit intensity plane replaces the dome entirely.
  int planet = 0;
  int32_t planet_sun_x = 192, planet_sun_y0 = 150, planet_sun_y1 = 150;
  int32_t planet_sun2_mag = 0;  // >0: a binary system, companion at these px
  int32_t planet_sun2_x = 0, planet_sun2_y = 0;
  // DIAGNOSTIC shade mode for creature subjects (P2): 0 off, 1 unlit
  // (fullbright/texture-only), 2 normal viz, 3 wireframe. Never set on a
  // pinned subject.
  int creature_shade = 0;
  // creature subjects (creature_rules.md lane): 1 = wave-walk (the identity
  // shot: walk + wave tilt + LOD pull-back), 2 = bulk-pop (inflate -> gibs)
  int creature = 0;
  // S2: the species selector; kAuto = the legacy binary rule on `creature`.
  Species species = Species::kAuto;
  // Diagnostic-only override: pin the existing compiled micro mesh at readable
  // framing. It never changes the creature asset or runtime LOD law.
  bool creature_force_micro = false;
  // V12 inspection-only camera orbit: hold the selected creature key and bulk
  // exactly while the view turns around it. This is reel presentation state,
  // never an animation/asset change.
  bool creature_hold = false;
  // Final Zixxtrixx inspection clip: one deterministic world-space descriptor
  // drives BOTH genuine spatial point lighting and its depth-tested marker.
  // All ordinary subjects leave this false and retain their selected rig.
  bool creature_moving_light = false;
  // Direction 29: the clip's SUN -- one named, positioned, emitting point
  // source above and to one side, tracking the creature. Null = no sun (the
  // moving-light subjects keep their own four-source rigs).
  const ZixxSunSpec* sun = nullptr;
  // FULL-COLOUR LANE (MODELINGGUIDE section 5). The 256-colour rule is a
  // GIF-EXPORT constraint, and it was allowed to redesign a creature: it
  // deleted an eye colour, a mouth and a throat transition, and it forced a
  // one-scalar shading model that turned the concept's pastels grey. A subject
  // that sets this is exempt from the palette gate and ships full-colour
  // frames; GIF becomes an optional degraded fallback for it, never a content
  // gate. Terrain and sky subjects keep the gate -- their GIFs are the
  // deliverable.
  bool full_colour = false;
  int32_t bump_ext = 12;  // bump_patch half-extent (creature shots: 6 m so the
                          // near camera at dist 8 stands OUTSIDE the envelope —
                          // Phase-3 culls the whole patch when any lattice
                          // vertex lands behind the eye, terrain.cpp 310)
  const char* note = "";
  // run 0326: scripted clip cuts for chained creature subjects -- at frame
  // .first the instance cuts to clip slot .second, exactly the game's own
  // hard-cut state change (no blend exists anywhere). Empty = single clip.
  std::vector<std::pair<uint32_t, uint16_t>> clip_cuts;
  // run 0326 salto variations: a TARGET DUMMY -- the watchdog quadruped
  // (the owner's pick) -- standing or hovering in the shot. Reel-only:
  // never part of the shipped creature or its site card.
  bool dummy = false;
  int32_t dummy_x_mm = 0, dummy_y_mm = 0;
  // Canonical one-shot evidence starts on authored key 0, includes each real
  // midpoint once, and ends on the final authored key: 2*(keys-1)+1 samples.
  // Legacy looping/pinned subjects keep their historical pre-advance cadence.
  bool canonical_creature_timeline = false;
  // --check golden: CRC-32C over all frame RGB bytes in sequence (0 = none).
  // Moves whenever the renderer, the field programs or the authoring here
  // legitimately change — update it in the same commit and say so.
  uint32_t expect_seq_crc = 0;
};

// per-frame celestial compose — the ONE hook target (set on the renderer's
// [phase3-preview] pre-resolve point). Authoring only; every law call goes
// through zref::star / zref::flare / zref::post.
//
// TRAIL AUTHORING (§15, palette cost stated up front): the compositor rebuilds
// one six-bit intensity plane from the captured positions, applies Noctis's
// subtract-8 and twice-smoothed source step per age, then performs one class
// ramp lookup. ghost_r_px scales the reconstructed corona and disc silhouette.
// The palette counter remains the arbiter for each complete reel sequence.
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
  L.trail = &ctx.trails[0];     // §15: history persists across the loop; the
                                // static-skip law keeps resting subjects clean
  zref::star::ComposeLight L2;  // the S08 multiple system's companion body
  L2.ramp = a.ramp;
  L2.face = &a.face;
  L2.corona = &a.corona;
  L2.trail = &ctx.trails[1];
  int n_lights = 1;
  // ping-pong drift for the class portraits: +-150 px about the centre,
  // ~9.7 px/frame.
  //
  // ghost_r_px is the ghost's HALO radius, and trail_disc_radius derives the
  // ghost's DISC from it as disc_r * ghost_r / halo_r. So setting it to
  // halo_r_px is what makes a ghost exactly the sun's own silhouette: the
  // derived disc comes back as disc_r_px on the nose. Each sun therefore
  // smears at its own size, 12 px for the blue dwarf up to 28 for the orange
  // giant, instead of the old uniform 9..15 band that made every sun leave
  // the same footprint.
  //
  // Do NOT set this to disc_r_px. That reads as "the smear should be as big
  // as the sun" and is wrong twice: it feeds the ratio, so the orange giant
  // derived a 63 px ghost disc against a real 42 px one, and a ghost larger
  // than its sun bleeds out in FRONT of it and above and below, which is not
  // a trail. Ask for the halo and the disc follows.
  //
  // Overlap costs no palette: the reconstruction plane is six-bit, so however
  // many falloffs a pixel sums, one class-ramp lookup still yields at most 64
  // trail colours. The per-sequence palette counter stays the arbiter.
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
    case 2: {  // noctis-flare: S00 sweep, washed disc, corona, flare,
               // and the bounded retained-frame reconstruction.
               // TRAILED (§15): the moving sun carries Noctis decay and
               // asymmetric diffusion reconstructed from captured positions.
      L.x_px = 8 + static_cast<int32_t>((368 * ph) / (half - 1));
      L.y_px = 150;
      L.disc_r_px = 8;
      L.halo_r_px = 16;
      L.ghost_r_px = 16;  // == halo_r_px (see the note above the drift)
      // k = 20, not 40. Two reasons, and they point the same way. The owner
      // wants the flare STRONGER, and a closer sun gives a bigger burst. And
      // correcting the resolve dither on 2026-08-18 un-collapsed greens that
      // the doubled amplitude had been merging, which pushed this subject to
      // 284 of a permitted 256: the flare chain alone measures 245 and the
      // trail needs about 39 more. Shrinking the trail does not fix that (a
      // 10 px ghost still lands at 270); the chain is the cost. At k = 20 the
      // whole subject fits at 240 with a larger burst than before.
      //
      // The real headroom is the docketed one: route the flare chain through a
      // single six-bit plane with one ramp lookup at the end, the way the trail
      // already is, and its cost drops to about the ramp size. Until then this
      // subject is the ceiling case and every change to it is measured.
      L.d_milli = 20LL * zref::star::kGamut[0].ray_milli;  // k = 20, burst12
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
      L.ghost_r_px = 26;  // == halo_r_px
      // 3r/2, not 5r/2: SATUR = min(63, 12d/r) is the FLOOR boil_index clamps
      // every palette entry up to, so at 5r/2 it was 30 and the bottom half of
      // the ramp collapsed flat, leaving the disc almost static. At 3r/2 the
      // floor is 18, the same as star-boil, and the class's own CLUT rotation
      // becomes a visible sheen. Moving closer also REDUCES the white wash
      // (that grows with distance, 20r being the washed case), so the class
      // colour this subject exists to show is better preserved, not worse.
      L.d_milli = 3LL * zref::star::kGamut[1].ray_milli / 2;
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
      L.ghost_r_px = 14;  // == halo_r_px
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
      L.ghost_r_px = 28;  // == halo_r_px
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
      L.ghost_r_px = 12;  // == halo_r_px
      // 3r/2 for the same reason as the blue giant: SATUR 24 -> 18, so the
      // compact blue disc boils visibly instead of sitting flat.
      L.d_milli = 3LL * zref::star::kGamut[7].ray_milli / 2;
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
      L.ghost_r_px = 18;  // == halo_r_px
      L.d_milli = 5LL * zref::star::kGamut[8].ray_milli / 2;
      L.r_milli = zref::star::kGamut[8].ray_milli;
      L.flare_mode = 0;
      L.probe_x = L.x_px;
      L.probe_y = L.y_px;
      L2.x_px = 192 - ((90 * co) >> 16);
      L2.y_px = 120 + ((90 * si) >> 16);
      L2.disc_r_px = 16;  // the companion: smaller, no flare of its own
      L2.halo_r_px = 12;
      L2.ghost_r_px = 12;  // == the companion's own halo_r_px
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
      L.ghost_r_px = 18;  // == halo_r_px
      L.d_milli = 2LL * zref::star::kGamut[9].ray_milli;
      L.r_milli = zref::star::kGamut[9].ray_milli;
      L.flare_mode = 0;
      L.probe_x = L.x_px;
      L.probe_y = 120;
      break;
    }
    case 11:    // atmo-sun-donor  — §4 halo_atmo, §3 ramp exactly as ported
    case 12: {  // atmo-sun-thick  — the same, with P3 retargeted (see above)
      // The two subjects are IDENTICAL here on purpose. Every geometric and
      // photometric parameter below is shared; the only difference between the
      // published pair is the one ramp control point set in cel_build_assets.
      // A comparison that moves two things proves nothing about either.
      //
      // No trail. The sun descends rather than crossing, and §15's smear is
      // not what this subject is about — the atmosphere is. Leaving the trail
      // live would also put ghost falloff levels on top of the halo's, which
      // is palette the halo needs.
      L.trail = nullptr;
      L.x_px = 192;
      // A setting sun: 96 px of descent over the loop, ending low. Thick air
      // is a PATH LENGTH effect, so the look belongs near the horizon where
      // the path is longest — a zenith sun in the same atmosphere is nearly
      // white and would not show what this pair exists to show.
      L.y_px = 56 + static_cast<int32_t>((96 * ph) / (half - 1));
      L.disc_r_px = 26;
      // 4x the disc, per §4's own note against halo_atmo ("pure glow ball,
      // 4xR"). kHaloRMaxZ60Px (225) still bounds it; 104 is well inside.
      L.halo_r_px = 104;
      // 3r/2 -> SATUR = min(63, 12d/r) = 18, the same floor star-boil uses.
      // Distance washout is the WRONG lever here: it whitens, and whitening
      // is the thing the corrected variant exists to argue against. Keeping
      // the sun close leaves the ramp's own colour in charge of the look.
      L.d_milli = 3LL * zref::star::kGamut[0].ray_milli / 2;
      L.r_milli = zref::star::kGamut[0].ray_milli;
      L.flare_mode = 0;  // no lens chain: the subject is the atmosphere
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

// Zixxtrixx (Upheaval's first creature) is authored in its own header so every
// knob Fabian might turn sits in one findable place.
#include "zixxtrixx.h"
#include "unnamed02.h"

// The demo subject: a watchdog quadruped. Ring parts are rigid per bone
// (donor law). Its authored forward axis is +X: pitch maps each body/head ring
// stack +Y -> +Z, then yaw maps +Z -> +X. Six bones/parts: torso, head, and
// four independently animated legs.
const zc::CreatureType& watchdog_type() {
  static const zc::CreatureType t = [] {
    zc::Skeleton sk;
    sk.bone_count = 6;
    sk.bones[0] = zc::Bone{0, 0, fxm(520), 0};
    sk.bones[1] = zc::Bone{0, fxm(390), fxm(80), 0};
    sk.bones[2] = zc::Bone{0, fxm(300), -fxm(20), -fxm(180)};
    sk.bones[3] = zc::Bone{0, fxm(300), -fxm(20), fxm(180)};
    sk.bones[4] = zc::Bone{0, -fxm(300), -fxm(20), -fxm(180)};
    sk.bones[5] = zc::Bone{0, -fxm(300), -fxm(20), fxm(180)};

    std::vector<zc::RingPart> parts;
    zc::RingPart body;
    body.rings = {{-fxm(450), fxm(145), 10},
                  {-fxm(300), fxm(185), 10},
                  {0, fxm(205), 10},
                  {fxm(300), fxm(185), 10},
                  {fxm(450), fxm(145), 10}};
    body.caps = zc::kCapTop | zc::kCapBot;
    body.pitch_q = 1;
    body.yaw_q = 1;
    body.bone = 0;
    body.r = 198;
    body.g = 108;
    body.b = 58;
    parts.push_back(body);

    zc::RingPart head;
    head.rings = {{-fxm(80), fxm(135), 8}, {fxm(110), fxm(120), 8}, {fxm(300), fxm(75), 8}};
    head.caps = zc::kCapTop | zc::kCapBot;
    head.pitch_q = 1;
    head.yaw_q = 1;
    head.bone = 1;
    head.r = 232;
    head.g = 168;
    head.b = 96;
    parts.push_back(head);

    for (int leg = 0; leg < 4; ++leg) {
      zc::RingPart lp;
      lp.rings = {{0, fxm(55), 6}, {-fxm(260), fxm(48), 6}, {-fxm(520), fxm(38), 6}};
      lp.caps = zc::kCapTop | zc::kCapBot;
      lp.bone = static_cast<uint8_t>(2 + leg);
      lp.r = 122;
      lp.g = 74;
      lp.b = 52;
      parts.push_back(lp);
    }

    // clip bank: slot 1 idle (breathing bob), slot 2 walk (16 keys).
    // Authored through the fx trig tables — the integer path.
    zc::ClipBank bank;
    bank.bone_count = 6;
    zc::Clip idle;
    idle.slot_id = 1;
    idle.frame_count = 32;
    idle.root.assign(32 * 3, 0);
    idle.quats.assign(static_cast<size_t>(32) * 6, zc::quat16_identity());
    for (uint16_t f = 0; f < 32; ++f) {
      const zref::angle16 breathe{static_cast<uint16_t>(f * (65536u / 32u))};
      idle.root[f * 3 + 1] = (zref::fx_sin(breathe).raw * 520) >> 16;  // +-8 mm
    }
    bank.clips.push_back(std::move(idle));

    zc::Clip walk;
    walk.slot_id = 2;
    walk.frame_count = 16;
    walk.root.assign(16 * 3, 0);
    walk.quats.assign(static_cast<size_t>(16) * 6, zc::quat16_identity());
    // Stride: legs swing +-0.30 rad about Z, so the upright legs move along
    // the authored +X forward axis. Diagonal pairs run in antiphase; the root
    // lowers by the lost vertical reach so at least one pair stays planted.
    for (uint16_t f = 0; f < 16; ++f) {
      const zref::angle16 ph{static_cast<uint16_t>(f * (65536u / 16u))};
      const int32_t s1 = zref::fx_sin(ph).raw;
      const int32_t s2 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph.raw * 2)}).raw;
      for (int b = 2; b <= 5; ++b) {
        const bool diagonal_a = (b == 2 || b == 5);
        const int32_t swing = (diagonal_a ? s1 : -s1) * 1565 >> 16;
        const zref::angle16 half{static_cast<uint16_t>(swing & 0xFFFF)};
        walk.quats[static_cast<size_t>(f) * 6 + b] =
            zc::quat16_axis_angle(zref::fx16{0}, zref::fx16{0}, zref::fx16{1 << 16},
                                  zref::fx_sin(half), zref::fx_cos(half));
      }
      const int32_t nod = (s2 * 400) >> 16;
      const zref::angle16 halfh{static_cast<uint16_t>(nod & 0xFFFF)};
      walk.quats[static_cast<size_t>(f) * 6 + 1] =
          zc::quat16_axis_angle(zref::fx16{0}, zref::fx16{0}, zref::fx16{1 << 16},
                                zref::fx_sin(halfh), zref::fx_cos(halfh));
      walk.root[f * 3 + 1] =
          -static_cast<int32_t>((static_cast<int64_t>(std::abs(s1)) * fxm(23)) >> 16);
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

// RUN 1939 (owner: "Would be cool if the flying salto target had wings").
// The FLYING target dummy only: the hovering watchdog read as a ground
// animal suspended in air. Same six bones and parts plus two membrane
// wings on their own bones, and a slow flap baked into its idle clip so
// the prop is not a static mount. A reel prop, not a creature: never on
// the site's creature pages, never authored to the sheet standard.
const zc::CreatureType& winged_dummy_type() {
  static const zc::CreatureType t = [] {
    zc::Skeleton sk;
    sk.bone_count = 8;
    sk.bones[0] = zc::Bone{0, 0, fxm(520), 0};
    sk.bones[1] = zc::Bone{0, fxm(390), fxm(80), 0};
    sk.bones[2] = zc::Bone{0, fxm(300), -fxm(20), -fxm(180)};
    sk.bones[3] = zc::Bone{0, fxm(300), -fxm(20), fxm(180)};
    sk.bones[4] = zc::Bone{0, -fxm(300), -fxm(20), -fxm(180)};
    sk.bones[5] = zc::Bone{0, -fxm(300), -fxm(20), fxm(180)};
    sk.bones[6] = zc::Bone{0, fxm(60), fxm(150), -fxm(200)};  // wing roots
    sk.bones[7] = zc::Bone{0, fxm(60), fxm(150), fxm(200)};

    std::vector<zc::RingPart> parts;
    zc::RingPart body;
    body.rings = {{-fxm(450), fxm(145), 10},
                  {-fxm(300), fxm(185), 10},
                  {0, fxm(205), 10},
                  {fxm(300), fxm(185), 10},
                  {fxm(450), fxm(145), 10}};
    body.caps = zc::kCapTop | zc::kCapBot;
    body.pitch_q = 1;
    body.yaw_q = 1;
    body.bone = 0;
    body.r = 198;
    body.g = 108;
    body.b = 58;
    parts.push_back(body);

    zc::RingPart head;
    head.rings = {{-fxm(80), fxm(135), 8}, {fxm(110), fxm(120), 8}, {fxm(300), fxm(75), 8}};
    head.caps = zc::kCapTop | zc::kCapBot;
    head.pitch_q = 1;
    head.yaw_q = 1;
    head.bone = 1;
    head.r = 232;
    head.g = 168;
    head.b = 96;
    parts.push_back(head);

    for (int leg = 0; leg < 4; ++leg) {
      zc::RingPart lp;
      lp.rings = {{0, fxm(55), 6}, {-fxm(260), fxm(48), 6}, {-fxm(520), fxm(38), 6}};
      lp.caps = zc::kCapTop | zc::kCapBot;
      lp.bone = static_cast<uint8_t>(2 + leg);
      lp.r = 122;
      lp.g = 74;
      lp.b = 52;
      parts.push_back(lp);
    }

    // the wings: flat elliptical membranes spanning +-Z off the shoulders
    // (rings stack +Y, pitch_q 1 maps +Y -> +Z; rx = chord along X, rz =
    // membrane thickness). Sized to read at the salto-fly camera.
    for (int w = 0; w < 2; ++w) {
      zc::RingPart wp;
      const int32_t sgn = w == 0 ? -1 : 1;
      for (int i = 0; i < 4; ++i) {
        zc::RingSpec rs;
        rs.y = sgn * fxm(60 + i * 230);
        rs.radius = fxm(200 - i * 45);
        rs.segments = 6;
        rs.rx = fxm(260 - i * 55);  // chord
        rs.rz = fxm(20);            // membrane
        wp.rings.push_back(rs);
      }
      wp.caps = zc::kCapTop | zc::kCapBot;
      wp.pitch_q = 1;
      wp.bone = static_cast<uint8_t>(6 + w);
      wp.r = 214;
      wp.g = 150;
      wp.b = 88;
      parts.push_back(wp);
    }

    zc::ClipBank bank;
    bank.bone_count = 8;
    zc::Clip idle;
    idle.slot_id = 1;
    idle.interpolate = true;
    idle.frame_count = 32;
    idle.root.assign(32 * 3, 0);
    idle.quats.assign(static_cast<size_t>(32) * 8, zc::quat16_identity());
    for (uint16_t f = 0; f < 32; ++f) {
      const zref::angle16 breathe{static_cast<uint16_t>(f * (65536u / 32u))};
      idle.root[f * 3 + 1] = (zref::fx_sin(breathe).raw * 520) >> 16;  // +-8 mm
      // the flap: two slow cycles per loop, +-16 deg about X (the span
      // tilts up and down), antiphased a touch so the hover reads alive
      const int32_t fl =
          zref::fx_sin(zref::angle16{static_cast<uint16_t>((f * 4096) & 0xFFFF)}).raw;
      const int32_t a = (fl * 2900) >> 16;
      for (int wb = 6; wb <= 7; ++wb) {
        const int32_t aa = wb == 6 ? a : -a;  // mirrored: both tips rise together
        const zref::angle16 half{static_cast<uint16_t>((aa >> 1) & 0xFFFF)};
        idle.quats[static_cast<size_t>(f) * 8 + wb] =
            zc::quat16_axis_angle(zref::fx16{1 << 16}, zref::fx16{0}, zref::fx16{0},
                                  zref::fx_sin(half), zref::fx_cos(half));
      }
    }
    bank.clips.push_back(std::move(idle));

    zc::CreatureType type;
    type.type_id = 4;
    const char* reason = "";
    if (!zc::compile_creature(sk, bank, parts, type, &reason)) {
      std::fprintf(stderr, "winged_dummy_type: compile failed: %s\n", reason);
    }
    return type;
  }();
  return t;
}

// One descriptor is the source of truth for both the rendered prop and the
// committed blade-vs-victim triangle gate.  It intentionally names an actual
// creature mesh and idle clip rather than reducing the victim to a sphere.
struct ZixxTargetDescriptor {
  const zc::CreatureType* type = nullptr;
  uint16_t clip_slot = 1;
  zref::angle16 facing{uint16_t{0x8000}};
  int32_t x_mm = 0;
  int32_t y_mm = 0;
};

ZixxTargetDescriptor zixx_target_descriptor(uint16_t attack_slot) {
  constexpr int32_t kTargetTorsoCentreMm = 520;
  const zc::AttackPlan p = zixx::zixx_variant_plan(attack_slot);
  ZixxTargetDescriptor d;
  d.type = attack_slot == zixx::kSlotAtkFly ? &winged_dummy_type()
                                             : &watchdog_type();
  d.x_mm = p.intercept_x_mm;
  // Ground watchdogs stand on the shared terrain; their attack intercept is a
  // point within the lower torso, not an instruction to lift the whole animal.
  // The flying victim is instead placed so its authored 520 mm torso centre is
  // exactly at the planned aerial intercept.  The old descriptor added the
  // intercept as a root height in both cases, making grounded dogs hover and
  // steering the blade underneath both actual posed meshes.
  d.y_mm = attack_slot == zixx::kSlotAtkFly
               ? p.intercept_y_mm - kTargetTorsoCentreMm
               : 0;
  return d;
}

int zixx_target_freeze_tick(uint16_t attack_slot) {
  const zc::AttackPlan p = zixx::zixx_variant_plan(attack_slot);
  return 2 * zixx::zixx_attack_variant_phases(p, true).impact;
}

const zc::Clip* reel_find_clip(const zc::CreatureType& type, uint16_t slot) {
  for (const zc::Clip& c : type.bank.clips)
    if (c.slot_id == slot) return &c;
  return nullptr;
}

struct ProbeVec3 {
  int64_t x = 0, y = 0, z = 0;  // integer millimetres
};

ProbeVec3 probe_cross(const ProbeVec3& a, const ProbeVec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
          a.x * b.y - a.y * b.x};
}
ProbeVec3 probe_sub(const ProbeVec3& a, const ProbeVec3& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
__int128 probe_dot(const ProbeVec3& a, const ProbeVec3& b) {
  return static_cast<__int128>(a.x) * b.x +
         static_cast<__int128>(a.y) * b.y +
         static_cast<__int128>(a.z) * b.z;
}

// Exact integer Moller-Trumbore predicates.  All comparisons are homogeneous,
// so no quotient or floating tolerance can turn a grazing frame into a
// platform-dependent hit.
bool segment_hits_triangle(const ProbeVec3& p, const ProbeVec3& q,
                           const ProbeVec3& a, const ProbeVec3& b,
                           const ProbeVec3& c) {
  const ProbeVec3 d = probe_sub(q, p);
  const ProbeVec3 e1 = probe_sub(b, a);
  const ProbeVec3 e2 = probe_sub(c, a);
  const ProbeVec3 h = probe_cross(d, e2);
  __int128 det = probe_dot(e1, h);
  if (det == 0) return false;
  const ProbeVec3 s = probe_sub(p, a);
  __int128 u = probe_dot(s, h);
  const ProbeVec3 r = probe_cross(s, e1);
  __int128 v = probe_dot(d, r);
  __int128 t = probe_dot(e2, r);
  if (det < 0) {
    det = -det;
    u = -u;
    v = -v;
    t = -t;
  }
  return u >= 0 && v >= 0 && u + v <= det && t >= 0 && t <= det;
}

ProbeVec3 skinned_probe_point(
    const std::array<zc::mat3x4fx, zc::kMaxBones>& pose,
    const zc::SkinVertex& v) {
  int32_t x = 0, y = 0, z = 0;
  zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
  return {static_cast<int64_t>(x) * 1000 >> 16,
          static_cast<int64_t>(y) * 1000 >> 16,
          static_cast<int64_t>(z) * 1000 >> 16};
}

zc::mat3x4fx probe_instance_world(zref::angle16 facing,
                                  const zc::GroundTilt& ground_tilt,
                                  int32_t x, int32_t y, int32_t z) {
  const int32_t fc = zref::fx_cos(facing).raw;
  const int32_t fs = zref::fx_sin(facing).raw;
  const zc::mat3x4fx roty{{fc, 0, fs, 0,
                           0, 1 << 16, 0, 0,
                           -fs, 0, fc, 0}};
  const zc::mat3x4fx tilt = zc::tilt_matrix(ground_tilt, nullptr);
  zc::mat3x4fx world{};
  zc::mat3x4_mul(roty, tilt, world, nullptr);
  world.m[3] = x;
  world.m[7] = y;
  world.m[11] = z;
  return world;
}

std::array<zc::mat3x4fx, zc::kMaxBones> probe_world_pose(
    const zc::CreatureType& type,
    const std::array<zc::mat3x4fx, zc::kMaxBones>& pose,
    const zc::mat3x4fx& world) {
  std::array<zc::mat3x4fx, zc::kMaxBones> out{};
  for (int b = 0; b < type.bank.bone_count; ++b)
    zc::mat3x4_mul(world, pose[b], out[b], nullptr);
  return out;
}

bool blade_segment_hits_target(uint16_t attack_slot, int presentation_tick,
                               const zc::mat3x4fx& attack_world,
                               const zc::mat3x4fx& target_world,
                               int& triangle_hits) {
  triangle_hits = 0;
  const zc::CreatureType& attacker = zixx::type();
  const zc::Clip* attack = reel_find_clip(attacker, attack_slot);
  if (!attack) return false;
  const uint16_t attack_key =
      static_cast<uint16_t>(presentation_tick / 2);
  const uint8_t attack_sub = static_cast<uint8_t>(presentation_tick & 1);
  std::array<zc::mat3x4fx, zc::kMaxBones> attack_local;
  zc::decode_pose(attacker, *attack, attack_key, attack_local, nullptr,
                  attack_sub);
  const std::array<zc::mat3x4fx, zc::kMaxBones> attack_pose =
      probe_world_pose(attacker, attack_local, attack_world);

  const zixx::Bind fork_bind = zixx::station_bind(56);
  const zc::SkinVertex fork_v{-fxm(zixx::station_x(56)), fxm(zixx::kBodyY),
                              0, fork_bind.b0, fork_bind.b1, fork_bind.w0,
                              0, 0};
  const zc::SkinVertex left_tip{
      -fxm(zixx::station_x(56) + zixx::kBladeLen), fxm(zixx::kBodyY), 0,
      zixx::kBBladeL2, zixx::kBBladeL2, 64, 0, 0};
  const zc::SkinVertex right_tip{
      -fxm(zixx::station_x(56) + zixx::kBladeLen), fxm(zixx::kBodyY), 0,
      zixx::kBBladeR2, zixx::kBBladeR2, 64, 0, 0};
  const ProbeVec3 blade_base = skinned_probe_point(attack_pose, fork_v);
  const ProbeVec3 blade_tip[2] = {
      skinned_probe_point(attack_pose, left_tip),
      skinned_probe_point(attack_pose, right_tip)};

  const ZixxTargetDescriptor target = zixx_target_descriptor(attack_slot);
  if (!target.type) return false;
  const zc::Clip* idle = reel_find_clip(*target.type, target.clip_slot);
  if (!idle || idle->frame_count == 0) return false;
  const int target_period = 2 * static_cast<int>(idle->frame_count);
  // The victim idles until contact, then its posed body becomes the stable
  // embedded surface.  Advancing an unrelated idle loop under a stopped blade
  // made the target leave and re-enter the weapon during the committed hold.
  // This is the same freeze law the reel applies to its rendered target.
  const int frozen_tick = zixx_target_freeze_tick(attack_slot);
  const int target_tick =
      std::min(presentation_tick, frozen_tick) % target_period;
  const uint16_t target_key = static_cast<uint16_t>(target_tick / 2);
  const uint8_t target_sub = static_cast<uint8_t>(target_tick & 1);
  std::array<zc::mat3x4fx, zc::kMaxBones> target_local;
  zc::decode_pose(*target.type, *idle, target_key, target_local, nullptr,
                  target_sub);
  const std::array<zc::mat3x4fx, zc::kMaxBones> target_pose =
      probe_world_pose(*target.type, target_local, target_world);

  for (const zc::Meshlet& m : target.type->mesh) {
    std::vector<ProbeVec3> world(m.verts.size());
    for (size_t i = 0; i < m.verts.size(); ++i)
      world[i] = skinned_probe_point(target_pose, m.verts[i]);
    for (size_t i = 0; i + 2 < m.idx.size(); i += 3) {
      const ProbeVec3& a = world[m.idx[i]];
      const ProbeVec3& b = world[m.idx[i + 1]];
      const ProbeVec3& c = world[m.idx[i + 2]];
      for (const ProbeVec3& tip : blade_tip) {
        if (segment_hits_triangle(blade_base, tip, a, b, c))
          ++triangle_hits;
      }
    }
  }
  return triangle_hits > 0;
}

int validate_zixx_target_interactions() {
  int failures = 0;
  // Reproduce the exact static reel terrain and per-frame instance transforms.
  // A flat local-space test can pass while the rendered attacker and victim sit
  // on different terrain columns (slot 48 differs by more than half a metre).
  // The gate therefore advances the same ground-tilt smoothing from frame zero,
  // ground-snaps each root to its own column, and skins both meshes in world
  // space before running the integer segment/triangle predicate.
  const zref::render::TerrainPatch patch = rtest::bump_patch(161, 161, 18, 8);
  const std::vector<zref::render::FieldApp> no_fields;
  const zref::terrain::ComposedLattice lat = zref::render::compose_lattice(
      patch, rtest::xform_identity(), no_fields, 0, nullptr, nullptr);
  for (uint16_t slot : {zixx::kSlotAtkDummy, zixx::kSlotAtkFly,
                        zixx::kSlotAtkNine}) {
    const zc::AttackPlan p = zixx::zixx_variant_plan(slot);
    const zixx::AttackVariantPhases ph =
        zixx::zixx_attack_variant_phases(p, true);
    const int begin_tick = 2 * ph.unroll_end;
    const int end_tick = 2 * (ph.recoil_begin - 1);
    const ZixxTargetDescriptor target = zixx_target_descriptor(slot);
    const int32_t attack_x = fxm(zixx::kStageCentreMm);
    const int32_t target_x = fxm(zixx::kStageCentreMm + target.x_mm);
    const zref::terrain::ColumnResult attack_col = zref::terrain::column_query(
        lat, zref::fx16{attack_x}, zref::fx16{0});
    const zref::terrain::ColumnResult target_col = zref::terrain::column_query(
        lat, zref::fx16{target_x}, zref::fx16{0});
    const int32_t attack_y =
        attack_col.cls == zref::terrain::ColumnClass::kSolid
            ? attack_col.top.raw
            : 0;
    const int32_t target_ground_y =
        target_col.cls == zref::terrain::ColumnClass::kSolid
            ? target_col.top.raw
            : 0;
    const int32_t target_y = target_ground_y + fxm(target.y_mm);
    zc::GroundTilt attack_tilt{};
    zc::GroundTilt target_tilt{};
    const zc::Clip* target_idle =
        target.type ? reel_find_clip(*target.type, target.clip_slot) : nullptr;
    const int freeze_tick = zixx_target_freeze_tick(slot);
    std::array<zc::mat3x4fx, zc::kMaxBones> frozen_target_local{};
    zc::mat3x4fx frozen_target_world{};
    bool have_frozen_target = false;
    bool victim_pose_stable = target_idle != nullptr;
    int first_victim_twitch = -1;
    std::vector<uint8_t> intersects;
    std::vector<int> counts;
    intersects.reserve(end_tick - begin_tick + 1);
    counts.reserve(end_tick - begin_tick + 1);
    for (int tick = 0; tick <= end_tick; ++tick) {
      zc::ground_tilt_update(attack_tilt, zc::TiltMode::kSideways,
                             zref::angle16{uint16_t{0}}, lat,
                             zref::fx16{attack_x}, zref::fx16{0},
                             zref::fx16{fxm(40)}, zref::fx16{fxm(20)});
      zc::ground_tilt_update(target_tilt, zc::TiltMode::kCompletely,
                             target.facing, lat, zref::fx16{target_x},
                             zref::fx16{0}, zref::fx16{fxm(40)},
                             zref::fx16{fxm(20)});
      if (tick < begin_tick) continue;
      const zc::mat3x4fx attack_world = probe_instance_world(
          zref::angle16{uint16_t{0}}, attack_tilt, attack_x, attack_y, 0);
      const zc::mat3x4fx target_world = probe_instance_world(
          target.facing, target_tilt, target_x, target_y, 0);
      // No-victim-twitch law: from the first embedded sample onward both the
      // decoded victim palette and its rendered world transform are bit-exact.
      // Checking only the idle key is insufficient because a still local pose
      // can visibly creep while the terrain-tilt smoother continues moving.
      if (tick >= freeze_tick && target_idle) {
        const int target_period = 2 * static_cast<int>(target_idle->frame_count);
        const int target_tick = std::min(tick, freeze_tick) % target_period;
        std::array<zc::mat3x4fx, zc::kMaxBones> target_local;
        zc::decode_pose(*target.type, *target_idle,
                        static_cast<uint16_t>(target_tick / 2), target_local,
                        nullptr, static_cast<uint8_t>(target_tick & 1));
        if (!have_frozen_target) {
          frozen_target_local = target_local;
          frozen_target_world = target_world;
          have_frozen_target = true;
        } else if (std::memcmp(target_local.data(), frozen_target_local.data(),
                               sizeof(zc::mat3x4fx) * target.type->bank.bone_count) != 0 ||
                   std::memcmp(&target_world, &frozen_target_world,
                               sizeof(target_world)) != 0) {
          victim_pose_stable = false;
          if (first_victim_twitch < 0) first_victim_twitch = tick;
        }
      }
      int hits = 0;
      const bool hit = blade_segment_hits_target(
          slot, tick, attack_world, target_world, hits);
      intersects.push_back(hit ? 1 : 0);
      counts.push_back(hits);
    }
    const auto at = [&](int tick) -> bool {
      return intersects[static_cast<size_t>(tick - begin_tick)] != 0;
    };
    int entries = 0;
    bool prior = false;
    int first_entry = -1;
    int first_clear = -1;
    bool reentry_after_clear = false;
    for (int tick = begin_tick; tick <= end_tick; ++tick) {
      const bool hit = at(tick);
      if (hit && !prior) {
        ++entries;
        if (first_entry < 0) first_entry = tick;
        if (first_clear >= 0) reentry_after_clear = true;
      }
      if (!hit && prior && tick >= 2 * ph.extract_begin && first_clear < 0)
        first_clear = tick;
      prior = hit;
    }
    bool held = true;
    int hold_min_hits = INT32_MAX;
    int first_hold_miss = -1;
    for (int tick = 2 * ph.impact; tick <= 2 * ph.extract_begin; ++tick) {
      held = held && at(tick);
      if (!at(tick) && first_hold_miss < 0) first_hold_miss = tick;
      hold_min_hits = std::min(
          hold_min_hits, counts[static_cast<size_t>(tick - begin_tick)]);
    }
    const bool clear_before_recoil = !at(end_tick);
    if (!have_frozen_target || !victim_pose_stable) {
      ++failures;
      std::fprintf(stderr,
                   "target-check slot %u: victim pose/world transform twitched "
                   "after embedding (first %d%s)\n",
                   slot, first_victim_twitch >= 0 ? first_victim_twitch / 2 : -1,
                   first_victim_twitch >= 0 && (first_victim_twitch & 1) ? ".5" : "");
    }
    if (entries != 1) {
      ++failures;
      std::fprintf(stderr,
                   "target-check slot %u: expected one entry, observed %d\n",
                   slot, entries);
    }
    if (!held) {
      ++failures;
      std::fprintf(stderr,
                   "target-check slot %u: blade left actual victim mesh "
                   "during committed hold\n",
                   slot);
    }
    if (!clear_before_recoil || reentry_after_clear || first_clear < 0) {
      ++failures;
      std::fprintf(stderr,
                   "target-check slot %u: extraction did not clear once and "
                   "stay clear before recoil\n",
                   slot);
    }
    if (entries != 1 || !held || !clear_before_recoil ||
        reentry_after_clear || first_clear < 0) {
      std::printf("  target diagnostic slot %u: hit ranges", slot);
      bool in_range = false;
      int range_begin = -1;
      for (int tick = begin_tick; tick <= end_tick + 1; ++tick) {
        const bool hit = tick <= end_tick ? at(tick) : false;
        if (hit && !in_range) {
          in_range = true;
          range_begin = tick;
        }
        if (!hit && in_range) {
          const int range_end = tick - 1;
          std::printf(" %d%s..%d%s", range_begin / 2,
                      (range_begin & 1) ? ".5" : "", range_end / 2,
                      (range_end & 1) ? ".5" : "");
          in_range = false;
        }
      }
      std::printf("; first hold miss %d%s\n",
                  first_hold_miss >= 0 ? first_hold_miss / 2 : -1,
                  first_hold_miss >= 0 && (first_hold_miss & 1) ? ".5" : "");
    }
    std::printf(
        "target-check slot %u: victim=%s, entry=%d%s, hold %d..%d "
        "(%d+ triangle hits/sample), clear=%d%s, recoil=%d, victim frozen "
        "pose+world exact — %s\n",
        slot, slot == zixx::kSlotAtkFly ? "winged-watchdog"
                                        : "watchdog",
        first_entry >= 0 ? first_entry / 2 : -1,
        first_entry >= 0 && (first_entry & 1) ? ".5" : "",
        ph.impact, ph.extract_begin, hold_min_hits,
        first_clear >= 0 ? first_clear / 2 : -1,
        first_clear >= 0 && (first_clear & 1) ? ".5" : "",
        ph.recoil_begin,
        have_frozen_target && victim_pose_stable && entries == 1 && held &&
                clear_before_recoil && !reentry_after_clear && first_clear >= 0
            ? "PASS"
            : "FAIL");
  }
  std::printf(failures == 0
                  ? "TARGET INTERACTION: PASS — rendered world-space posed "
                    "victim triangles, exact frozen victim pose/world, "
                    "entry/hold/extraction at every key + midpoint\n"
                  : "TARGET INTERACTION: FAIL — %d assertion(s)\n",
              failures);
  return failures == 0 ? 0 : 1;
}

struct ReelGibPiece {
  int32_t x = 0, y = 0, z = 0;
  int32_t vx = 0, vy = 0, vz = 0;
  int32_t half_extent = 0;
  uint16_t yaw = 0, pitch = 0;
  uint16_t yaw_step = 0, pitch_step = 0;
  uint16_t age = 0, lifetime = 0;
  uint8_t r = 0, g = 0, b = 0;
};

void spawn_reel_gibs(const zc::CreatureType& type, const zc::mat3x4fx* pose, int32_t wx, int32_t wy,
                     int32_t wz, int32_t ground, std::vector<ReelGibPiece>& out) {
  std::vector<zc::Gib> points;
  zc::spawn_gibs(type, pose, zref::fx16{wx}, zref::fx16{wy}, zref::fx16{wz}, 0x600DF00Du, points);
  out.clear();
  for (size_t i = 0; i < points.size() && out.size() < 18; i += 3) {
    const zc::Gib& src = points[i];
    ReelGibPiece p;
    p.x = src.x;
    p.y = src.y;
    p.z = src.z;
    p.vx = src.vx / 4 + (src.x >= wx ? fxm(55) : -fxm(55));
    p.vy = std::max(fxm(150), src.vy / 5);
    p.vz = src.vz / 4 + (src.z >= wz ? fxm(45) : -fxm(45));
    p.half_extent = fxm(55 + static_cast<int32_t>(i % 4) * 10);
    if (p.y < ground + p.half_extent) p.y = ground + p.half_extent;
    p.yaw = static_cast<uint16_t>(0x0713u * (i + 1));
    p.pitch = static_cast<uint16_t>(0x0B47u * (i + 3));
    p.yaw_step = static_cast<uint16_t>(0x0311u + (i % 5) * 0x0097u);
    p.pitch_step = static_cast<uint16_t>(0x021Du + (i % 7) * 0x0061u);
    p.lifetime = static_cast<uint16_t>(28 + i % 5);
    p.r = src.r;
    p.g = src.g;
    p.b = src.b;
    out.push_back(p);
  }
}

void advance_reel_gibs(std::vector<ReelGibPiece>& pieces, int32_t ground, int32_t gravity) {
  for (auto it = pieces.begin(); it != pieces.end();) {
    ReelGibPiece& p = *it;
    ++p.age;
    if (p.age >= p.lifetime) {
      it = pieces.erase(it);
      continue;
    }
    p.x += p.vx;
    p.y += p.vy;
    p.z += p.vz;
    p.vy -= gravity;
    const int32_t floor = ground + p.half_extent;
    if (p.y < floor) {
      p.y = floor;
      if (p.vy < 0) p.vy = -p.vy / 3;
      p.vx = p.vx * 3 / 4;
      p.vz = p.vz * 3 / 4;
    }
    p.yaw = static_cast<uint16_t>(p.yaw + p.yaw_step);
    p.pitch = static_cast<uint16_t>(p.pitch + p.pitch_step);
    ++it;
  }
}

// ---- Owner Direction 29: A SUN IN EVERY CLIP -------------------------------
// Twenty-one of the twenty-two bank clips carried no point source at all, so
// the approved additive emission had nothing to emit. Each clip now declares
// ONE named sun: a positioned point source that TRACKS the creature instance
// at a fixed millimetre offset -- always ABOVE and to one side (the owner has
// rejected lighting from below or behind more than once) -- with the whole
// animal inside the sun's INNER radius by construction, so attenuation is 1
// everywhere on the body and the source is a pure directional lambert light:
// the never-enters-its-own-inner-radius trap (08-LIGHTING) cannot occur.
// The accepted Cool Cross rig stays underneath untouched; the sun is ADDED
// light. The additive emission (add_*) rides the SAME lambert*attenuation
// response as the multiplicative gain (never a flat lift) and carries the
// visible colour on pigments that lack the hue; each multiplicative gain
// SUPPRESSES its complement (a red sun must not pump green -- the three
// warming/amber/gold failures of RUN-2144). Everything accumulates and
// saturates once at the end, in the existing shade path. Every value below
// is an owner-editable named constant, per clip.
// ZIXX_SUNS=off is the demonstrable revert path: with it set, every clip
// renders byte-identically to the sunless bank (the g_creature_additive_light
// gate itself stays default-OFF and is only raised subject-scoped).
struct ZixxSunSpec {
  int32_t x_mm, y_mm, z_mm;        // offset from the tracked staged centre
  int32_t inner_mm, outer_mm;      // whole body INSIDE inner -> attenuation 1
  int32_t gain_r, gain_g, gain_b;  // multiplicative, Q16.16 (fxm thousandths)
  int32_t add_r, add_g, add_b;     // additive emission, Q16.16 of 255 scale
};
// Shared geometry: a sun is FAR AWAY. 50 m up and 22 m to the side puts the
// stage at ~66 deg elevation and the nine-salto's authored 24 m root apex --
// the highest thing this creature ever does -- still ~50 deg BELOW the sun,
// so no clip is ever lit from below or behind, and the 65 m inner radius
// keeps every body vertex, every flight and the salto target dummy inside
// attenuation 1: direction is the only thing the distance changes, which is
// exactly a sun. (The first authoring at 8.6 m would have put the six- and
// nine-salto apexes ABOVE the sun at their peaks -- caught before render.)
constexpr int32_t kZixxSunHeightMm = 50000;
constexpr int32_t kZixxSunInnerMm = 65000;
constexpr int32_t kZixxSunOuterMm = 78000;
// Per-clip suns. Azimuth is authored per camera (yaw 0 / 45 / 67.5 deg
// subjects) so the sun always sits high on the camera side of the sky, offset
// to one flank -- never behind the animal relative to the shot. Colour is the
// clip's MOOD; dominant channel strong, complement suppressed, judged by eye
// at native 384x240 (art law: these numbers are knobs, not derivations).
// Direction 30 calmed the whole table: the first authoring lifted the animal
// +60..+85 mean counts with near-equal R/G on warm suns (the mult gains rode
// the green-rich pigment and diluted every hue toward white -- the owner saw
// "one super bright normal sun, no color"). Adds are cut to ~40% and mults to
// ~50% of Direction 29, complements near zero, so the ADD carries a nameable
// hue and pigment/form dominate again (reference: Archive Generation 18).
// Creature 02 clip suns (the same far-sun law; a cool mana-violet family
// authored CALM per Direction 30 -- the pink pigment is an amplifier).
constexpr ZixxSunSpec kU02SunHover   {14000, kZixxSunHeightMm, 16000, kZixxSunInnerMm, kZixxSunOuterMm, fxm(150), fxm(40), fxm(200), fxm(80), fxm(25), fxm(130)};
constexpr ZixxSunSpec kU02SunDrift   {-13000, kZixxSunHeightMm, 17000, kZixxSunInnerMm, kZixxSunOuterMm, fxm(120), fxm(55), fxm(170), fxm(55), fxm(35), fxm(110)};
constexpr ZixxSunSpec kU02SunChannel {12000, kZixxSunHeightMm, 14000, kZixxSunInnerMm, kZixxSunOuterMm, fxm(180), fxm(35), fxm(220), fxm(100), fxm(20), fxm(150)};
constexpr ZixxSunSpec kU02SunCalm    {15000, kZixxSunHeightMm, 15000, kZixxSunInnerMm, kZixxSunOuterMm, fxm(130), fxm(45), fxm(160), fxm(60), fxm(28), fxm(95)};
// Pass 3 (Stage L, Direction 3 §1): the four-light rig was a REGRESSION at
// bank scale -- every clip returns to ONE directional sun, Zixxtrixx-style,
// so the missing clips get their own named suns here. Same far-sun law
// (50 m up: even the fall's ~3.7 m apex stays ~50 deg below the sun), same
// calm cool mana-violet family, azimuth spread per clip mood. Every value
// is an owner knob, judged by eye at native.
constexpr ZixxSunSpec kU02SunCurious   {16000, kZixxSunHeightMm, 13000, kZixxSunInnerMm, kZixxSunOuterMm, fxm(140), fxm(50), fxm(180), fxm(70), fxm(30), fxm(115)};
constexpr ZixxSunSpec kU02SunStartle   {4000,  kZixxSunHeightMm, 21000, kZixxSunInnerMm, kZixxSunOuterMm, fxm(100), fxm(70), fxm(220), fxm(45), fxm(40), fxm(140)};  // cold shock blue
constexpr ZixxSunSpec kU02SunPirouette {-15000, kZixxSunHeightMm, 15000, kZixxSunInnerMm, kZixxSunOuterMm, fxm(160), fxm(45), fxm(190), fxm(85), fxm(28), fxm(120)};
constexpr ZixxSunSpec kU02SunHasty     {13000, kZixxSunHeightMm, 17000, kZixxSunInnerMm, kZixxSunOuterMm, fxm(170), fxm(60), fxm(150), fxm(90), fxm(35), fxm(85)};   // warmer rose: effort
constexpr ZixxSunSpec kU02SunFall      {-14000, kZixxSunHeightMm, 16800, kZixxSunInnerMm, kZixxSunOuterMm, fxm(90),  fxm(75), fxm(230), fxm(40), fxm(38), fxm(145)};  // ice violet
constexpr ZixxSunSpec kU02SunHit       {3800,  kZixxSunHeightMm, 21600, kZixxSunInnerMm, kZixxSunOuterMm, fxm(110), fxm(65), fxm(210), fxm(50), fxm(33), fxm(130)};  // bruise
constexpr ZixxSunSpec kU02SunTaunt     {21600, kZixxSunHeightMm, -3800, kZixxSunInnerMm, kZixxSunOuterMm, fxm(190), fxm(35), fxm(200), fxm(105), fxm(18), fxm(120)}; // magenta show-off
constexpr ZixxSunSpec kU02SunTaunt2    {11000, kZixxSunHeightMm, 19000, kZixxSunInnerMm, kZixxSunOuterMm, fxm(150), fxm(55), fxm(175), fxm(80), fxm(30), fxm(105)};  // rose dusk
constexpr ZixxSunSpec kZixxSunIdle       {15556,  kZixxSunHeightMm, 15556,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(240), fxm(70), fxm(12), fxm(150), fxm(65), fxm(5)};  // golden morning
constexpr ZixxSunSpec kZixxSunWalk       {-12619,  kZixxSunHeightMm, 18022,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(18), fxm(80), fxm(280), fxm(10), fxm(35), fxm(135)};  // azure day
constexpr ZixxSunSpec kZixxSunRun        {12619,  kZixxSunHeightMm, 18022,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(280), fxm(90), fxm(10), fxm(150), fxm(42), fxm(3)};  // hot orange
constexpr ZixxSunSpec kZixxSunLook       {12619,  kZixxSunHeightMm, 18022,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(160), fxm(25), fxm(260), fxm(85), fxm(10), fxm(140)};  // violet
constexpr ZixxSunSpec kZixxSunBalance    {14142,  kZixxSunHeightMm, 16853,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(18), fxm(250), fxm(240), fxm(8), fxm(115), fxm(140)};  // teal
constexpr ZixxSunSpec kZixxSunTaunt      {21666,  kZixxSunHeightMm, -3820,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(250), fxm(18), fxm(210), fxm(140), fxm(6), fxm(95)};  // magenta
constexpr ZixxSunSpec kZixxSunSlowTaunt  {11000,  kZixxSunHeightMm, 19053,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(230), fxm(60), fxm(105), fxm(110), fxm(27), fxm(50)};  // rose dusk
constexpr ZixxSunSpec kZixxSunJumpOne    {13544,  kZixxSunHeightMm, 17336,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(105), fxm(260), fxm(18), fxm(48), fxm(125), fxm(9)};  // spring lime
constexpr ZixxSunSpec kZixxSunJumpMulti  {-13544,  kZixxSunHeightMm, 17336,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(290), fxm(60), fxm(8), fxm(155), fxm(33), fxm(3)};  // sunset red-orange
constexpr ZixxSunSpec kZixxSunAttack     {14142,  kZixxSunHeightMm, 16853,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(300), fxm(18), fxm(12), fxm(160), fxm(6), fxm(6)};  // crimson
constexpr ZixxSunSpec kZixxSunHit        {3820,  kZixxSunHeightMm, 21666,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(28), fxm(100), fxm(250), fxm(12), fxm(45), fxm(125)};  // steel blue
constexpr ZixxSunSpec kZixxSunDamage     {21666,  kZixxSunHeightMm, 3820,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(260), fxm(115), fxm(12), fxm(130), fxm(55), fxm(3)};  // ember amber
constexpr ZixxSunSpec kZixxSunKnockdown  {4574,  kZixxSunHeightMm, 21518,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(130), fxm(20), fxm(240), fxm(63), fxm(9), fxm(125)};  // bruise violet
constexpr ZixxSunSpec kZixxSunFall       {-14142,  kZixxSunHeightMm, 16853,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(55), fxm(150), fxm(270), fxm(27), fxm(70), fxm(135)};  // ice blue
constexpr ZixxSunSpec kZixxSunHitFloor   {21518,  kZixxSunHeightMm, 4574,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(250), fxm(125), fxm(20), fxm(125), fxm(57), fxm(6)};  // dust orange
constexpr ZixxSunSpec kZixxSunSaltoDummy {11000,  kZixxSunHeightMm, 19053,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(200), fxm(220), fxm(15), fxm(90), fxm(100), fxm(6)};  // chartreuse gold
constexpr ZixxSunSpec kZixxSunSaltoFly   {-11000,  kZixxSunHeightMm, 19053,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(12), fxm(63), fxm(290), fxm(6), fxm(27), fxm(155)};  // deep azure
constexpr ZixxSunSpec kZixxSunSaltoSix   {16853,  kZixxSunHeightMm, 14142,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(260), fxm(185), fxm(18), fxm(135), fxm(78), fxm(6)};  // gold
constexpr ZixxSunSpec kZixxSunSaltoNine  {-16853,  kZixxSunHeightMm, 14142,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(270), fxm(27), fxm(140), fxm(145), fxm(11), fxm(72)};  // hot pink
constexpr ZixxSunSpec kZixxSunDeath      {21916,  kZixxSunHeightMm, 1917,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(280), fxm(12), fxm(8), fxm(150), fxm(4), fxm(4)};  // deep red
constexpr ZixxSunSpec kZixxSunDeath2     {3062,  kZixxSunHeightMm, 21785,  kZixxSunInnerMm, kZixxSunOuterMm, fxm(105), fxm(115), fxm(260), fxm(48), fxm(51), fxm(130)};  // moonlight

// Direction 29/30 revert switch (reel-side; the silicon-side revert is the
// g_creature_additive_light gate plus the CREATURE.LIGHT parameters).
// ZIXX_SUNS=off restores the PRE-SUNS bank byte-identically: the clip suns go
// dark AND additive-normal mode drops, so the moving-light clip renders its
// original multiplicative-only form (proof: evidence-gateoff-identity in the
// suns-calmed run).
bool g_zixx_suns_enabled = true;
// Direction 30: the additive term is the NORMAL lighting mode for every
// animation -- not a per-subject experiment a future pass must remember to
// raise. The library gate zc::g_creature_additive_light itself stays
// default-OFF (the silicon-cost revert path, pending the ALM/M10K sweep);
// the reel raises it around every creature compose that carries point
// sources. Cleared together with the suns by ZIXX_SUNS=off.
bool g_zixx_additive_normal = true;

void sample_zixx_clip_sun(const ZixxSunSpec& sun, const zc::CreatureInstance& inst,
                          zc::CreaturePointLight& out) {
  const int32_t staged_centre_x = inst.x - fxm(zixx::kStageCentreMm);
  out.world_x = staged_centre_x + fxm(sun.x_mm);
  out.world_y = inst.y + fxm(sun.y_mm);
  out.world_z = inst.z + fxm(sun.z_mm);
  out.inner_radius = fxm(sun.inner_mm);
  out.outer_radius = fxm(sun.outer_mm);
  out.gain_r = sun.gain_r;
  out.gain_g = sun.gain_g;
  out.gain_b = sun.gain_b;
  out.add_r = sun.add_r;  // consumed under the subject-scoped additive gate
  out.add_g = sun.add_g;
  out.add_b = sun.add_b;
}

struct CreatureReelCtx {
  zc::CreatureInstance* inst = nullptr;
  zc::CreatureInstance* dummy = nullptr;  // run 0326: the target dummy
  zc::PoseBank* poses = nullptr;
  zref::mat4fx vp;
  std::vector<ReelGibPiece>* gibs = nullptr;
  uint32_t gibs_in_view = 0;
  bool force_micro = false;
  bool moving_light = false;
  // Pass 2 (creature 02, R6): the moving-light base rig is per-subject —
  // Zixxtrixx keeps the inspection rig; u02 carries its own lower-ambient
  // variant (the gas rim is thickened through the LIGHT, R4).
  const zc::CreatureLightRig* moving_rig = &zc::kCreatureLightMovingInspection;
  // Direction 29 clip sun: when true, moving_sources[0] holds the subject's
  // one sun and the additive gate is raised for this subject's compose only.
  bool sun_light = false;
  // Pass 3 (Stage L): the u02-owned base rig under a SUN subject (null = the
  // process-selected rig — Zixxtrixx's path, untouched), and the marker-orb
  // switch: u02 subjects draw NO orbs (Direction 3 §1, "remove the light
  // dot"), Zixxtrixx keeps its own.
  const zc::CreatureLightRig* sun_rig = nullptr;
  bool moving_markers = true;
  // Direction 26: the moving-light inspection carries FOUR world-space
  // sources -- the original warm lamp plus blue, orange and green. They are
  // one contiguous array so the compositor's point-light globals can name
  // them directly.
  zc::CreaturePointLight moving_sources[zc::kCreatureMaxPointLights]{};
  // S5 (creature 02): splat the centre glow before the compose
  bool u02_glow = false;
  bool u02_glow_trio = false;         // the S5 spike's phantom neighbours
  int32_t u02_body[3] = {0, 0, 0};    // posed body-centre world position
  int32_t u02_glow_gain_pm = 1000;    // pulses with the compression wave
  // the mana populations, drawn INSIDE this hook (after the chained sky /
  // celestial passes, which repaint depth==0 and would otherwise eat every
  // particle over sky — particles never write depth, charter pass 7)
  zref::render::Population u02_add_pop;  // additive points (flags 0x0005)
  zref::render::Population u02_tri_pop;  // additive tris (0x0006: glints, bolts)
  zref::render::Population u02_opq_pop;  // opaque points (0x0001: droplets)
  // the trio: two extra phase-offset instances composed with the primary
  zc::CreatureInstance* u02_extra[2] = {nullptr, nullptr};
  int32_t u02_glow_centres[3][3] = {};  // one glow per composed conduit
  int u02_glow_count = 0;
  // pass 2: the mana menu — world-space splat list filled per frame by
  // u02::mana_fill, composed by the hook (pre splats before the creature,
  // post splats after, each depth-tested at its own projected 1/w)
  int u02_mana = 0;
  uint32_t u02_frame = 0;
  std::vector<u02::ManaSplat> u02_mana_splats;
};
constexpr uint32_t kZixxMovingSourceCount = 4;
constexpr uint32_t kZixxMovingSourceWarm = 0;
constexpr uint32_t kZixxMovingSourceBlue = 1;
constexpr uint32_t kZixxMovingSourceOrange = 2;
constexpr uint32_t kZixxMovingSourceGreen = 3;
static_assert(kZixxMovingSourceCount <= zc::kCreatureMaxPointLights,
              "moving-light sources exceed the compositor's array bound");

// Four slow, continuous world-space passes: along the camera-side flank, over
// the whole animal, back along the far flank, then over and around to the start.
// The path is authored in millimetres around the staged signature-S, never from
// camera coordinates. Each leg eases to rest so its direction change reads as a
// deliberate inspection rather than a bouncing light.
void sample_zixx_moving_source(uint32_t frame, uint32_t frames,
                               const zc::CreatureInstance& inst,
                               zc::CreaturePointLight& out) {
  constexpr int32_t kPathHalfMm = 1750;
  constexpr int32_t kPathSideMm = 850;
  constexpr int32_t kPathHeightMm = 1400;
  constexpr int32_t kHighArchMm = 1200;
  constexpr int32_t kReturnArchMm = 900;
  // A broad, forceful inspection lamp rather than a small travelling glint.
  // The core covers a substantial body phrase and the long shoulder keeps the
  // source legible across the far flank; gains may exceed one because the
  // compositor clamps only the final material response, preserving pigment.
  // Direction 27: the previous 2.25/1.70/1.10 at inner radius 2300 put ALL
  // THREE channels over quant_shade's 1.0 ceiling across the entire pool --
  // the lamp read as a hue-neutral whiteout that flooded the animal and left
  // the coloured sources nothing to read against (phase-1 evidence,
  // RUN-20260903-2017). Tamed so its own warmth survives the clamp and its
  // pool is a pool rather than the whole scene.
  constexpr int32_t kLightInnerRadiusMm = 1200;
  constexpr int32_t kLightOuterRadiusMm = 4500;
  constexpr int32_t kLightGainR = 68813;   // 1.05 -- warm core, only R may clamp
  constexpr int32_t kLightGainG = 47186;   // 0.72
  constexpr int32_t kLightGainB = 19661;   // 0.30
  // Direction 28: the warm inspection lamp carries NO additive term even in
  // the additive prototype clip -- it already reads as light, and restraint
  // is part of the shippable settings being demonstrated.
  constexpr int32_t kLightAddR = 0;
  constexpr int32_t kLightAddG = 0;
  constexpr int32_t kLightAddB = 0;
  const uint32_t leg_frames = std::max<uint32_t>(1, frames / 4);
  const uint32_t leg = std::min<uint32_t>(3, frame / leg_frames);
  const uint32_t local = frame - leg * leg_frames;
  const int32_t linear = static_cast<int32_t>(
      (static_cast<uint64_t>(local) * 1000u) / leg_frames);
  const int32_t ease = static_cast<int32_t>(
      (static_cast<int64_t>(linear) * linear * (3000 - 2 * linear)) /
      1000000);
  const int32_t arch = zref::fx_sin(zref::angle16{
      static_cast<uint16_t>((static_cast<uint32_t>(linear) * 32768u) / 1000u)})
                           .raw;
  const auto lerp_mm = [ease](int32_t a, int32_t b) {
    return a + static_cast<int32_t>(
                   (static_cast<int64_t>(b - a) * ease + 500) / 1000);
  };

  int32_t x_mm = 0, y_mm = kPathHeightMm, z_mm = 0;
  switch (leg) {
    case 0:  // near-side longitudinal pass
      x_mm = lerp_mm(-kPathHalfMm, kPathHalfMm);
      z_mm = -kPathSideMm;
      break;
    case 1:  // high diagonal arch over the complete S
      x_mm = lerp_mm(kPathHalfMm, -kPathHalfMm);
      z_mm = lerp_mm(-kPathSideMm, kPathSideMm);
      y_mm += static_cast<int32_t>(
          (static_cast<int64_t>(kHighArchMm) * arch + 32768) >> 16);
      break;
    case 2:  // far-side longitudinal return
      x_mm = lerp_mm(-kPathHalfMm, kPathHalfMm);
      z_mm = kPathSideMm;
      break;
    default:  // lower crossover completes the around-and-over loop
      x_mm = lerp_mm(kPathHalfMm, -kPathHalfMm);
      z_mm = lerp_mm(kPathSideMm, -kPathSideMm);
      y_mm += static_cast<int32_t>(
          (static_cast<int64_t>(kReturnArchMm) * arch + 32768) >> 16);
      break;
  }

  const int32_t staged_centre_x = inst.x - fxm(zixx::kStageCentreMm);
  out.world_x = staged_centre_x + fxm(x_mm);
  out.world_y = inst.y + fxm(y_mm);
  out.world_z = inst.z + fxm(z_mm);
  out.inner_radius = fxm(kLightInnerRadiusMm);
  out.outer_radius = fxm(kLightOuterRadiusMm);
  out.gain_r = kLightGainR;  // warm gummy-lamp source
  out.gain_g = kLightGainG;
  out.gain_b = kLightGainB;
  out.add_r = kLightAddR;  // consumed only under the Direction 28 gate
  out.add_g = kLightAddG;
  out.add_b = kLightAddB;
}

// ---- Direction 26: three more coloured moving sources ----------------------
// "the render with the moving light, there should be 3 more different color
// lights. Blue, orange, and green. They should sometimes intersect and mix
// colors." Each source moves on its own authored millimetre path about the
// staged signature-S; every colour, radius, gain, path extent and period below
// is a named owner knob. The pools are deliberately TIGHTER than the warm
// lamp's so an overlap is an event that reads as mixed colour on the animal,
// not a permanent wash. All three paths close exactly over the clip's frame
// count, so the loop stays clean.
//
// Direction 27 rebalance: the shipped orbits never entered their own 350mm
// inner radius (closest approach ~1000-1200mm against outer 1500/1700, so
// attenuation topped out near 0.4) and the peak colour event was +10..35
// counts on dark-band pixels -- mathematically present, visually absent.
// Each path now passes genuinely CLOSE, each inner radius covers that pass,
// each outer radius makes the pool a readable body-phrase, and each gain
// vector is PURE (one dominant channel; a clamped dominant keeps hue, three
// clamped channels are white).
//
// BLUE: a level counter-clockwise orbit around the whole animal, two laps.
constexpr int32_t kBlueOrbitXMm = 2050;      // orbit half-width along the body
constexpr int32_t kBlueOrbitZMm = 1050;      // orbit half-depth across it
constexpr int32_t kBlueHeightMm = 800;       // constant carry height
constexpr uint32_t kBlueOrbitTurns = 2;      // laps per clip
constexpr int32_t kBlueInnerRadiusMm = 900;
constexpr int32_t kBlueOuterRadiusMm = 2600;
constexpr int32_t kBlueGainR = 5243;        // 0.08
constexpr int32_t kBlueGainG = 19661;       // 0.30
constexpr int32_t kBlueGainB = 111411;      // 1.70
// Direction 28 additive colour (Q16.16 of the 255 pixel scale; consumed only
// in the additive prototype subject): a restrained true-blue emission so the
// pool reads blue even on pigments with little blue.
constexpr int32_t kBlueAddR = 0;
constexpr int32_t kBlueAddG = 1966;         // 0.03
constexpr int32_t kBlueAddB = 13107;        // 0.20
// ORANGE: a clockwise counter-orbit, three laps, bobbing as it goes.
constexpr int32_t kOrangeOrbitXMm = 1550;
constexpr int32_t kOrangeOrbitZMm = 820;
constexpr int32_t kOrangeHeightMm = 640;
constexpr int32_t kOrangeBobMm = 300;        // vertical bob amplitude
constexpr uint32_t kOrangeOrbitTurns = 3;
constexpr uint16_t kOrangePhaseA16 = 16384;  // quarter-turn start offset
constexpr int32_t kOrangeInnerRadiusMm = 900;
constexpr int32_t kOrangeOuterRadiusMm = 2600;
constexpr int32_t kOrangeGainR = 131072;     // 2.00
constexpr int32_t kOrangeGainG = 45875;      // 0.70 -- amber, not olive, on the body green
constexpr int32_t kOrangeGainB = 2621;       // 0.04
// Direction 28: THIS IS THE RED-ON-GREEN CASE. In the additive prototype the
// third slot is not the orange lamp any more -- it is a RED lamp, the source
// the owner asked for in the cancelled Direction-27 follow-up ("make one of
// the orange ones a strong red") that multiplicative transport could not show
// (red*green = olive). Two authored lessons from tuning by eye at native:
//   * v1-v3 kept the orange MULTIPLICATIVE gains (G 0.70) and only added red;
//     the pool's own green amplification folded every add into GOLD. A red
//     emitter that also amplifies green is not a red lamp; under the additive
//     model the shippable red source barely multiplies green at all.
//   * The additive term carries the red READ on the green body (pigment has
//     no red for a multiply to find); the multiplicative red still lights the
//     red-rich eye, crown and pink stripe, which is exactly right.
// Direction 30: additive is normal, so the moving-light clip carries the
// red source; ZIXX_SUNS=off reverts it to kOrange* (the pre-suns clip).
constexpr int32_t kRedGainR = 104858;        // 1.60 -- blazes the eye/pink/crown
constexpr int32_t kRedGainG = 7864;          // 0.12 -- a red lamp must NOT pump green
constexpr int32_t kRedGainB = 1966;          // 0.03
constexpr int32_t kRedAddR = 36045;          // 0.55 true-red emission
constexpr int32_t kRedAddG = 1311;           // 0.02
constexpr int32_t kRedAddB = 0;
// GREEN: a low near-side longitudinal shuttle, three round trips, with a
// small depth ellipse so the travel stays alive at the turnarounds.
constexpr int32_t kGreenSweepMm = 1700;      // longitudinal half-travel
constexpr int32_t kGreenSideMm = -800;       // carried on the camera side
constexpr int32_t kGreenSideDriftMm = 250;   // depth ellipse half-extent
constexpr int32_t kGreenHeightMm = 520;      // low, raking the flank
constexpr uint32_t kGreenSweepTurns = 4;  // de-phased from orange's 3 laps so their meetings drift
constexpr int32_t kGreenInnerRadiusMm = 900;
constexpr int32_t kGreenOuterRadiusMm = 2600;
constexpr int32_t kGreenGainR = 6554;        // 0.10
constexpr int32_t kGreenGainG = 98304;       // 1.50
constexpr int32_t kGreenGainB = 11796;       // 0.18
// Direction 28 additive colour: modest true-green emission.
constexpr int32_t kGreenAddR = 1311;         // 0.02
constexpr int32_t kGreenAddG = 10486;        // 0.16
constexpr int32_t kGreenAddB = 1966;         // 0.03

void sample_zixx_moving_colour_sources(uint32_t frame, uint32_t frames,
                                       const zc::CreatureInstance& inst,
                                       zc::CreaturePointLight* out,
                                       bool additive) {
  const int32_t staged_centre_x = inst.x - fxm(zixx::kStageCentreMm);
  // angle16 that completes `turns` exact revolutions over the clip
  const auto path_angle = [frame, frames](uint32_t turns, uint16_t phase) {
    return zref::angle16{static_cast<uint16_t>(
        (static_cast<uint64_t>(frame) * turns * 65536u) / frames + phase)};
  };
  const auto scaled = [](int32_t mm, int32_t trig_raw) {
    return static_cast<int32_t>(
        (static_cast<int64_t>(mm) * trig_raw + 32768) >> 16);
  };
  const auto place = [&](zc::CreaturePointLight& s, int32_t x_mm, int32_t y_mm,
                         int32_t z_mm, int32_t inner_mm, int32_t outer_mm,
                         int32_t gr, int32_t gg, int32_t gb, int32_t adr,
                         int32_t adg, int32_t adb) {
    s.world_x = staged_centre_x + fxm(x_mm);
    s.world_y = inst.y + fxm(y_mm);
    s.world_z = inst.z + fxm(z_mm);
    s.inner_radius = fxm(inner_mm);
    s.outer_radius = fxm(outer_mm);
    s.gain_r = gr;
    s.gain_g = gg;
    s.gain_b = gb;
    s.add_r = adr;  // consumed only under the Direction 28 gate
    s.add_g = adg;
    s.add_b = adb;
  };

  const zref::angle16 ab = path_angle(kBlueOrbitTurns, 0);
  place(out[kZixxMovingSourceBlue], scaled(kBlueOrbitXMm, zref::fx_cos(ab).raw),
        kBlueHeightMm, scaled(kBlueOrbitZMm, zref::fx_sin(ab).raw),
        kBlueInnerRadiusMm, kBlueOuterRadiusMm, kBlueGainR, kBlueGainG,
        kBlueGainB, kBlueAddR, kBlueAddG, kBlueAddB);

  const zref::angle16 ao = path_angle(kOrangeOrbitTurns, kOrangePhaseA16);
  const zref::angle16 ao2 = path_angle(kOrangeOrbitTurns * 2, 0);
  place(out[kZixxMovingSourceOrange],
        scaled(kOrangeOrbitXMm, zref::fx_cos(ao).raw),
        kOrangeHeightMm + scaled(kOrangeBobMm, zref::fx_sin(ao2).raw),
        -scaled(kOrangeOrbitZMm, zref::fx_sin(ao).raw),  // reversed: clockwise
        kOrangeInnerRadiusMm, kOrangeOuterRadiusMm,
        additive ? kRedGainR : kOrangeGainR, additive ? kRedGainG : kOrangeGainG,
        additive ? kRedGainB : kOrangeGainB, additive ? kRedAddR : 0,
        additive ? kRedAddG : 0, additive ? kRedAddB : 0);

  const zref::angle16 ag = path_angle(kGreenSweepTurns, 0);
  place(out[kZixxMovingSourceGreen], scaled(kGreenSweepMm, zref::fx_sin(ag).raw),
        kGreenHeightMm, kGreenSideMm + scaled(kGreenSideDriftMm, zref::fx_cos(ag).raw),
        kGreenInnerRadiusMm, kGreenOuterRadiusMm, kGreenGainR, kGreenGainG,
        kGreenGainB, kGreenAddR, kGreenAddG, kGreenAddB);
}

// ---- Creature 02: the many-coloured moving rig, u02-scaled (pass 2, R6) ----
//
// Direction 2 §6: the many-coloured lighting IS the standard now. The
// Zixxtrixx paths orbit a ~4 m staged signature-S; the conduit's body ball
// is 0.9 m hovering at ~1.25 m, so the placement code is reused with
// u02-OWNED extents — otherwise the coloured pools never land on the animal
// (08-LIGHTING: check the closest approach against the inner radius).
//
// The base rig is a u02 variant of the inspection rig (R4: the gas rim is
// the dark toon band at grazing incidence, and its width is governed by
// AMBIENT — lowering ambient grows the rim evenly around the silhouette,
// while the kept key/fill opposition stops it collapsing into a one-sided
// terminator). Ambient below even the inspection rig's, fill slightly UP,
// tuned by looking at the rim at native 384x240.
const zc::CreatureLightRig kU02MovingRig{
    43000,  36000, 35000,   // the honest top diagonal (Cool Cross's key)
    -45000, 35000, -32000,  // the opposing crossfill direction (kept: R4)
    9175,   9830,  12452,   // LOW ambient: .14, .15, .19 — the rim knob
    21627,                  // dim white key: .33
    8520,   11141, 15729};  // blue-leaning crossfill: .13, .17, .24

// Pass 3 (Stage L/M, R5): the u02-owned BASE RIG for every SUN subject.
// Starts as kCreatureLightDiagonalCoolCross VERBATIM (the rig v1's liked
// mist was judged under); the AMBIENT rungs are the mist knob — lower is a
// wider but darker rim — re-authored by the Stage M ladder, by eye, for
// "thicker than v1 AND the pink glows through". Key/fill opposition kept.
const zc::CreatureLightRig kU02SunRig{
    43000,  36000, 35000,   // top diagonal from +X/+Z (Cool Cross)
    -45000, 35000, -32000,  // strong opposing top-diagonal crossfill
    26214,  29491, 36700,   // ambient .40/.45/.56 — THE MIST KNOB (Stage M)
    38011,                  // white key: .58
    16384,  24904, 36045};  // blue crossfill: .25, .38, .55

// Path extents around the hovering conduit (world mm, instance-relative).
constexpr int32_t kU02WarmOrbitXMm = 1500, kU02WarmOrbitZMm = 1500;
constexpr int32_t kU02WarmHeightMm = 2500;   // above the loop peak
constexpr uint32_t kU02WarmTurns = 1;
constexpr int32_t kU02WarmInnerMm = 900, kU02WarmOuterMm = 3600;
constexpr int32_t kU02BlueOrbitXMm = 1150, kU02BlueOrbitZMm = 700;
constexpr int32_t kU02BlueHeightMm = 1250;   // level with the body centre
constexpr uint32_t kU02BlueTurns = 2;
constexpr int32_t kU02BlueInnerMm = 520, kU02BlueOuterMm = 1700;
constexpr int32_t kU02RedOrbitXMm = 950, kU02RedOrbitZMm = 580;
constexpr int32_t kU02RedHeightMm = 850;
constexpr int32_t kU02RedBobMm = 250;
constexpr uint32_t kU02RedTurns = 3;
constexpr uint16_t kU02RedPhaseA16 = 16384;
constexpr int32_t kU02RedInnerMm = 520, kU02RedOuterMm = 1600;
// The red source's gains are u02-owned: the Zixxtrixx red (mult R 1.60) was
// authored against a GREEN body; on the conduit's R-rich pink it pegs the
// red channel across the whole pool and reads neon (the channel-ceiling
// law). Tamed by eye at native.
constexpr int32_t kU02RedGainR = 58982;   // 0.90
constexpr int32_t kU02RedGainG = 6554;    // 0.10
constexpr int32_t kU02RedGainB = 1966;    // 0.03
constexpr int32_t kU02RedAddR = 19661;    // 0.30 true-red emission
constexpr int32_t kU02RedAddG = 1311;     // 0.02
constexpr int32_t kU02RedAddB = 0;
constexpr int32_t kU02GreenSweepMm = 1100;
constexpr int32_t kU02GreenSideMm = -600;
constexpr int32_t kU02GreenSideDriftMm = 200;
constexpr int32_t kU02GreenHeightMm = 550;   // low, raking the belly
constexpr uint32_t kU02GreenTurns = 4;
constexpr int32_t kU02GreenInnerMm = 520, kU02GreenOuterMm = 1600;

void sample_u02_moving_sources(uint32_t frame, uint32_t frames,
                               const zc::CreatureInstance& inst,
                               zc::CreaturePointLight* out) {
  const int32_t centre_x = inst.x - fxm(u02::kStageCentreMm);
  const auto path_angle = [frame, frames](uint32_t turns, uint16_t phase) {
    return zref::angle16{static_cast<uint16_t>(
        (static_cast<uint64_t>(frame) * turns * 65536u) / frames + phase)};
  };
  const auto scaled = [](int32_t mm, int32_t trig_raw) {
    return static_cast<int32_t>(
        (static_cast<int64_t>(mm) * trig_raw + 32768) >> 16);
  };
  const auto place = [&](zc::CreaturePointLight& s, int32_t x_mm, int32_t y_mm,
                         int32_t z_mm, int32_t inner_mm, int32_t outer_mm,
                         int32_t gr, int32_t gg, int32_t gb, int32_t adr,
                         int32_t adg, int32_t adb) {
    s.world_x = centre_x + fxm(x_mm);
    s.world_y = inst.y + fxm(y_mm);
    s.world_z = inst.z + fxm(z_mm);
    s.inner_radius = fxm(inner_mm);
    s.outer_radius = fxm(outer_mm);
    s.gain_r = gr;
    s.gain_g = gg;
    s.gain_b = gb;
    s.add_r = adr;
    s.add_g = adg;
    s.add_b = adb;
  };
  // Colour gains and additive emissions reuse the shipped Zixxtrixx source
  // values (they are per-channel responses, independent of stage scale);
  // only the PATHS are re-authored to the conduit's size. The warm lamp
  // still emits nothing — restraint is part of the standard look.
  const zref::angle16 aw = path_angle(kU02WarmTurns, 0);
  place(out[kZixxMovingSourceWarm], scaled(kU02WarmOrbitXMm, zref::fx_cos(aw).raw),
        kU02WarmHeightMm, scaled(kU02WarmOrbitZMm, zref::fx_sin(aw).raw),
        kU02WarmInnerMm, kU02WarmOuterMm, 68813, 47186, 19661, 0, 0, 0);
  const zref::angle16 ab = path_angle(kU02BlueTurns, 0);
  place(out[kZixxMovingSourceBlue], scaled(kU02BlueOrbitXMm, zref::fx_cos(ab).raw),
        kU02BlueHeightMm, scaled(kU02BlueOrbitZMm, zref::fx_sin(ab).raw),
        kU02BlueInnerMm, kU02BlueOuterMm, kBlueGainR, kBlueGainG, kBlueGainB,
        kBlueAddR, kBlueAddG, kBlueAddB);
  const zref::angle16 ar = path_angle(kU02RedTurns, kU02RedPhaseA16);
  const zref::angle16 ar2 = path_angle(kU02RedTurns * 2, 0);
  place(out[kZixxMovingSourceOrange],
        scaled(kU02RedOrbitXMm, zref::fx_cos(ar).raw),
        kU02RedHeightMm + scaled(kU02RedBobMm, zref::fx_sin(ar2).raw),
        -scaled(kU02RedOrbitZMm, zref::fx_sin(ar).raw),
        kU02RedInnerMm, kU02RedOuterMm, kU02RedGainR, kU02RedGainG,
        kU02RedGainB, kU02RedAddR, kU02RedAddG, kU02RedAddB);
  const zref::angle16 ag = path_angle(kU02GreenTurns, 0);
  place(out[kZixxMovingSourceGreen], scaled(kU02GreenSweepMm, zref::fx_sin(ag).raw),
        kU02GreenHeightMm,
        kU02GreenSideMm + scaled(kU02GreenSideDriftMm, zref::fx_cos(ag).raw),
        kU02GreenInnerMm, kU02GreenOuterMm, kGreenGainR, kGreenGainG,
        kGreenGainB, kGreenAddR, kGreenAddG, kGreenAddB);
}

// ---- Direction 27 committed diagnostic: solo one moving source ------------
// The owner could not see three of the four pools, and this creature has
// already shipped one instrument-invisible light, so the proof tool is
// committed rather than improvised. ZIXX_ML_SOLO=warm|blue|orange|green
// isolates one source by zeroing every other source's gains;
// ZIXX_ML_SOLO=none zeroes all four (the dark plate a solo render is
// differenced against -- markers still draw, so they cancel in the diff).
// ZIXX_ML_BOOST=<n> multiplies the solo source's gains n-fold for the
// extreme-gain existence proof. Unset (the shipping case) leaves every byte
// of every subject identical.
int g_zixx_ml_solo = -2;      // -2 unset, -1 none, else source index
int32_t g_zixx_ml_boost = 1;  // gain multiplier for the solo source

void apply_zixx_ml_solo(zc::CreaturePointLight* s) {
  if (g_zixx_ml_solo == -2) return;
  for (uint32_t si = 0; si < kZixxMovingSourceCount; ++si) {
    if (static_cast<int>(si) == g_zixx_ml_solo) {
      s[si].gain_r *= g_zixx_ml_boost;
      s[si].gain_g *= g_zixx_ml_boost;
      s[si].gain_b *= g_zixx_ml_boost;
      s[si].add_r *= g_zixx_ml_boost;
      s[si].add_g *= g_zixx_ml_boost;
      s[si].add_b *= g_zixx_ml_boost;
    } else {
      s[si].gain_r = s[si].gain_g = s[si].gain_b = 0;
      s[si].add_r = s[si].add_g = s[si].add_b = 0;
    }
  }
}

// Per-source visible-orb tints (owner knobs): {core r,g,b, halo r,g,b}. The
// core is the lamp itself; the halo blends toward its colour so each orb names
// which pool it drives even when two sit close together.
constexpr uint8_t kZixxMovingSourceMarker[kZixxMovingSourceCount][6] = {
    {255, 232, 170, 255, 184, 92},   // warm (unchanged)
    {190, 220, 255, 110, 160, 255},  // blue
    {255, 220, 180, 255, 150, 60},   // orange
    {200, 255, 190, 110, 235, 110},  // green
};
// Direction 28: in the additive prototype the orange orbit EMITS true red, so
// its visible orb must say so -- a marker whose colour disagrees with its
// light is its own small lie (08-LIGHTING). Other slots keep their tints.
constexpr uint8_t kZixxMovingSourceMarkerAdditive[kZixxMovingSourceCount][6] = {
    {255, 232, 170, 255, 184, 92},   // warm (unchanged)
    {190, 220, 255, 110, 160, 255},  // blue (unchanged)
    {255, 120, 90, 255, 60, 40},     // RED: the additive red-on-green source
    {200, 255, 190, 110, 235, 110},  // green (unchanged)
};

void draw_zixx_moving_source_markers(const CreatureReelCtx& c, uint8_t* rgb,
                                     int32_t* depth, uint32_t w, uint32_t h) {
  const zref::render::Viewport viewport{0, 0, w, h};
  for (uint32_t si = 0; si < kZixxMovingSourceCount; ++si) {
    const zc::CreaturePointLight& src = c.moving_sources[si];
    // Direction 30: additive is the normal mode, so the orbs wear the
    // additive palette (the red orb for the red-emitting orbit) unless the
    // whole look is reverted to the pre-suns bank.
    const uint8_t* tint = g_zixx_additive_normal ? kZixxMovingSourceMarkerAdditive[si]
                                                 : kZixxMovingSourceMarker[si];
    const zref::render::ProjOut p = zref::render::project_vertex(
        c.vp, viewport, zref::fx16{src.world_x}, zref::fx16{src.world_y},
        zref::fx16{src.world_z}, nullptr);
    if (!p.in) continue;
    const int32_t cx = p.s.x >> 8;
    const int32_t cy = p.s.y >> 8;
    constexpr int32_t kHaloRadiusPx = 7;
    constexpr int32_t kCoreRadiusSq = 4;
    for (int32_t dy = -kHaloRadiusPx; dy <= kHaloRadiusPx; ++dy) {
      for (int32_t dx = -kHaloRadiusPx; dx <= kHaloRadiusPx; ++dx) {
        const int32_t d2 = dx * dx + dy * dy;
        if (d2 > kHaloRadiusPx * kHaloRadiusPx) continue;
        const int32_t x = cx + dx;
        const int32_t y = cy + dy;
        if (x < 0 || y < 0 || x >= static_cast<int32_t>(w) ||
            y >= static_cast<int32_t>(h))
          continue;
        const size_t i = static_cast<size_t>(y) * w + x;
        // Q16.16 1/w: larger is closer. The orb therefore disappears honestly
        // behind the creature or terrain instead of becoming an unrelated HUD
        // dot.
        if (p.s.d < depth[i]) continue;
        uint8_t* px = &rgb[i * 3];
        if (d2 <= kCoreRadiusSq) {
          px[0] = tint[0];
          px[1] = tint[1];
          px[2] = tint[2];
          depth[i] = p.s.d;
        } else {
          const int32_t a = ((kHaloRadiusPx * kHaloRadiusPx - d2) * 150) /
                            (kHaloRadiusPx * kHaloRadiusPx);
          px[0] = static_cast<uint8_t>(px[0] + ((tint[3] - px[0]) * a + 127) / 255);
          px[1] = static_cast<uint8_t>(px[1] + ((tint[4] - px[1]) * a + 127) / 255);
          px[2] = static_cast<uint8_t>(px[2] + ((tint[5] - px[2]) * a + 127) / 255);
        }
      }
    }
  }
}

// RUN 1939/2234 texture-experiment lane: reel-side post effects, env-gated.
// Default off: the normal render never enters these branches and stays
// byte-identical. The radius is a SCREEN-SPACE authoring knob so a thick line
// stays thick through distance and mips instead of being baked into the atlas.
int g_exp_contour = 0;         // ink the creature's silhouette (the sheets' line)
int g_exp_contour_radius = 2;  // pixels, inward from the rendered silhouette
int g_exp_boil = 0;            // hand-drawn shimmer: a chunky deterministic
                        // per-4-frame displacement of the creature's pixels
                        // (a function of x, y and FRAME -- never wall-clock)
uint32_t g_exp_frame = 0;

// V9 main cel presentation. This is deliberately separate from the archived
// inward experiment contour above: it draws only into border-connected
// background and scales by projected creature size at the actual camera.
int g_cel_main = 0;
constexpr int32_t kCelInkFarRadiusQ8 = 120 * 256;
constexpr int32_t kCelInkMidRadiusQ8 = 200 * 256;
constexpr int32_t kCelInkCloseRadiusQ8 = 360 * 256;
constexpr int kCelInkFarWidthPx = 1;
constexpr int kCelInkMidWidthPx = 2;
constexpr int kCelInkCloseWidthPx = 4;
constexpr uint8_t kCelInkR = 26;
constexpr uint8_t kCelInkG = 24;
constexpr uint8_t kCelInkB = 22;

int cel_main_ink_width(int32_t radius_q8) {
  const auto blend = [](int32_t x, int32_t x0, int32_t x1,
                        int y0, int y1) {
    const int64_t num = static_cast<int64_t>(x - x0) * (y1 - y0);
    const int64_t den = x1 - x0;
    return y0 + static_cast<int>((num + den / 2) / den);
  };
  if (radius_q8 <= kCelInkFarRadiusQ8) return kCelInkFarWidthPx;
  if (radius_q8 < kCelInkMidRadiusQ8)
    return blend(radius_q8, kCelInkFarRadiusQ8, kCelInkMidRadiusQ8,
                 kCelInkFarWidthPx, kCelInkMidWidthPx);
  if (radius_q8 < kCelInkCloseRadiusQ8)
    return blend(radius_q8, kCelInkMidRadiusQ8, kCelInkCloseRadiusQ8,
                 kCelInkMidWidthPx, kCelInkCloseWidthPx);
  return kCelInkCloseWidthPx;
}

void creature_hook(void* vctx, uint8_t* rgb, int32_t* depth, uint32_t w, uint32_t h,
                   uint32_t /*tick*/) {
  CreatureReelCtx& c = *static_cast<CreatureReelCtx*>(vctx);
  static std::vector<uint8_t> pre;
  static std::vector<int32_t> pre_depth;
  // (pass 2: the snapshot moved BELOW the u02 pre-compose glow/mana splats,
  // so a light pool never enters the cel-ink mask — the pulsar's halo was
  // getting a black outline as if it were creature surface. Zixxtrixx
  // subjects draw no pre-compose splats, so their bytes cannot move.)
  int32_t primary_radius_q8 = 0;
  int32_t dummy_radius_q8 = 0;
  bool primary_radius_visible = false;
  bool dummy_radius_visible = false;
  if (g_cel_main) {
    primary_radius_visible = zc::projected_bound_radius_q8(
        c.vp, c.inst->x, c.inst->y, c.inst->z, c.inst->type->bound_radius,
        w, primary_radius_q8, nullptr);
    if (c.dummy != nullptr) {
      dummy_radius_visible = zc::projected_bound_radius_q8(
          c.vp, c.dummy->x, c.dummy->y, c.dummy->z,
          c.dummy->type->bound_radius, w, dummy_radius_q8, nullptr);
    }
    std::fprintf(stderr,
                 "celmain projected_radius_q8 primary=%d primary_visible=%d "
                 "dummy=%d dummy_visible=%d ink_width=%d\n",
                 primary_radius_q8, primary_radius_visible ? 1 : 0,
                 dummy_radius_q8, dummy_radius_visible ? 1 : 0,
                 cel_main_ink_width(primary_radius_q8));
  }
#ifdef ZHAO_CREATURE_DEBUG
  {
    // telemetry: rung, projected radius, screen bbox of the skinned mesh
    const zc::CreatureType& T = *c.inst->type;
    const zref::vec4fx clip =
        zref::mat4_vec4(c.vp,
                        zref::vec4fx{zref::fx16{c.inst->x}, zref::fx16{c.inst->y},
                                     zref::fx16{c.inst->z}, zref::fx16{1 << 16}},
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
  // ---- S5: the creature-02 centre glow, BEFORE the compose so the body
  // occludes its own core (the light lives in the belly). One baked sprite
  // per process, ONE ramp per frame, one cheap splat per conduit; the
  // splat depth-tests against the conduit centre's own 1/w so it paints
  // over the terrain behind the creature but never over anything nearer.
  int32_t cr_glow_px[3][2];
  int cr_glow_n = 0;
  static u02::GlowAssets s_glow_assets;
  u02::GlowFrame s_glow_frame;
  if (c.u02_glow) {
    u02::glow_bake(s_glow_assets);
    u02::GlowFrame& gf = s_glow_frame;  // the one per-frame ramp build
    const int32_t gain = static_cast<int32_t>(
        (static_cast<int64_t>(u02::kCentreGlowGainPm) * c.u02_glow_gain_pm) / 1000);
    u02::glow_build_ramp(gf, u02::kGlowLo, u02::kGlowMid, u02::kGlowHi, gain);
    const zref::render::Viewport vpp_g{0, 0, w, h};
    // one glow per composed conduit (the trio subject), or the S5 spike's
    // phantom staging when u02_glow_trio without real extras
    const int n = c.u02_glow_count > 0 ? c.u02_glow_count : 1;
    const int phantoms = c.u02_glow_trio && c.u02_extra[0] == nullptr ? 3 : n;
    for (int gi = 0; gi < phantoms; ++gi) {
      int32_t gx, gy, gz;
      if (gi < n && c.u02_glow_count > 0) {
        gx = c.u02_glow_centres[gi][0];
        gy = c.u02_glow_centres[gi][1];
        gz = c.u02_glow_centres[gi][2];
      } else {
        gx = c.u02_body[0] + fxm(u02::kS5PhantomOffsMm[gi][0]);
        gy = c.u02_body[1];
        gz = c.u02_body[2] + fxm(u02::kS5PhantomOffsMm[gi][1]);
      }
      if (c.u02_glow_trio && c.u02_extra[0] == nullptr) {
        gx = c.u02_body[0] + fxm(u02::kS5PhantomOffsMm[gi][0]);
        gy = c.u02_body[1];
        gz = c.u02_body[2] + fxm(u02::kS5PhantomOffsMm[gi][1]);
      }
      const zref::render::ProjOut pc = zref::render::project_vertex(
          c.vp, vpp_g, zref::fx16{gx}, zref::fx16{gy}, zref::fx16{gz}, nullptr);
      if (!pc.in) continue;
      u02::glow_splat(rgb, depth, w, h, s_glow_assets, gf, pc.s.x >> 8, pc.s.y >> 8,
                      u02::kCentreGlowRadiusPx, pc.s.d);
      cr_glow_px[cr_glow_n][0] = pc.s.x >> 8;
      cr_glow_px[cr_glow_n][1] = pc.s.y >> 8;
      if (cr_glow_n < 2) ++cr_glow_n;
    }
  }
  // ---- pass 2: the mana menu, PRE-compose layer — the pools the creature
  // and its arms occlude (the caged pulsar's halo, the plasma blobs, the
  // boil's outer). Each splat depth-tests at its own projected 1/w.
  static u02::GlowFrame s_mana_ramps[u02::kRampCount];
  if (c.u02_mana != 0 && !c.u02_mana_splats.empty()) {
    u02::glow_bake(s_glow_assets);
    u02::mana_build_ramps(s_mana_ramps, c.u02_frame);
    const zref::render::Viewport vpp_m{0, 0, w, h};
    for (const u02::ManaSplat& ms : c.u02_mana_splats) {
      if (!ms.pre) continue;
      const zref::render::ProjOut pm = zref::render::project_vertex(
          c.vp, vpp_m, zref::fx16{ms.x}, zref::fx16{ms.y}, zref::fx16{ms.z}, nullptr);
      if (!pm.in) continue;
      u02::GlowFrame gf2 = s_mana_ramps[ms.ramp];
      if (ms.gain_pm != 1000) {
        for (int i = 0; i < 64; ++i)
          for (int ch = 0; ch < 3; ++ch)
            gf2.pal[i][ch] = static_cast<uint8_t>(gf2.pal[i][ch] * ms.gain_pm / 1000);
      }
      u02::glow_splat(rgb, depth, w, h, s_glow_assets, gf2, pm.s.x >> 8, pm.s.y >> 8,
                      ms.r_px, pm.s.d, ms.depth_test, /*bloom=*/true, ms.opaque);
    }
  }
  if (g_exp_contour || g_exp_boil || g_cel_main)
    pre.assign(rgb, rgb + static_cast<size_t>(w) * h * 3);
  if (g_cel_main)
    pre_depth.assign(depth, depth + static_cast<size_t>(w) * h);
  {
    zc::CreatureInstance* insts[4] = {c.inst, c.dummy, c.u02_extra[0], c.u02_extra[1]};
    if (c.force_micro) {
      c.inst->lod.rung = zc::LodRung::kMicro;
      c.inst->lod.hold = 0;  // compose cannot refine during this diagnostic frame
    }
    const zc::CreatureLightRig* const saved_rig = zc::g_creature_light_rig;
    const zc::CreaturePointLight* const saved_points = zc::g_creature_point_lights;
    const uint32_t saved_point_count = zc::g_creature_point_light_count;
    const bool saved_additive = zc::g_creature_additive_light;
    if (c.moving_light) {
      zc::g_creature_light_rig = c.moving_rig;
      zc::g_creature_point_lights = c.moving_sources;
      zc::g_creature_point_light_count = kZixxMovingSourceCount;
      zc::g_creature_additive_light = g_zixx_additive_normal;  // Direction 30
    } else if (c.sun_light) {
      // Direction 29: the clip's sun joins the selected rig (Cool Cross stays
      // underneath); emission is ON through the same gate, scoped and restored.
      // Pass 3: a u02 sun subject composes over its OWN base rig (kU02SunRig,
      // the Stage M mist knob), subject-scoped exactly like moving_rig.
      if (c.sun_rig != nullptr) zc::g_creature_light_rig = c.sun_rig;
      zc::g_creature_point_lights = c.moving_sources;
      zc::g_creature_point_light_count = 1;
      zc::g_creature_additive_light = g_zixx_additive_normal;
    }
    size_t inst_n = 0;
    zc::CreatureInstance* live[4];
    for (zc::CreatureInstance* ip : insts)
      if (ip != nullptr) live[inst_n++] = ip;
    zc::compose_creatures(rgb, depth, w, h, c.vp, live, inst_n, *c.poses, nullptr);
    // Subject-scoped by construction: a moving-light render cannot tint the
    // next requested subject in this one-binary catalogue process.
    zc::g_creature_light_rig = saved_rig;
    zc::g_creature_point_lights = saved_points;
    zc::g_creature_point_light_count = saved_point_count;
    zc::g_creature_additive_light = saved_additive;
  }
  // ---- RUN 1939 experiment post-pass (env-gated, default off). Placed
  // HERE because the hook returns early when there are no gibs -- the
  // first cut sat after that return and never ran on the idle. ----------
  if (g_exp_contour || g_exp_boil || g_cel_main) {
    const size_t n = static_cast<size_t>(w) * h;
    std::vector<uint8_t> mask(n, 0);
    for (size_t i = 0; i < n; ++i) {
      const size_t b3 = i * 3;
      const bool rgb_changed =
          rgb[b3] != pre[b3] || rgb[b3 + 1] != pre[b3 + 1] ||
          rgb[b3 + 2] != pre[b3 + 2];
      if (rgb_changed || (g_cel_main && depth[i] != pre_depth[i]))
        mask[i] = 1;
    }
    if (g_exp_boil) {
      // the redrawn-every-frame look: displace the creature's pixels by a
      // chunky 6x6-cell field that re-rolls every 4 frames. Deterministic
      // integer hash of (cell, frame) -- replay-exact by construction.
      const std::vector<uint8_t> src(rgb, rgb + n * 3);
      const uint32_t fq = g_exp_frame / 4;
      for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
          const size_t i = static_cast<size_t>(y) * w + x;
          if (!mask[i]) continue;
          uint32_t hsh = (x / 6) * 73856093u ^ (y / 6) * 19349663u ^ fq * 83492791u;
          hsh ^= hsh >> 13;
          const int dx = static_cast<int>((hsh >> 3) % 3) - 1;
          const int dy = static_cast<int>((hsh >> 7) % 3) - 1;
          const int sx = static_cast<int>(x) + dx, sy = static_cast<int>(y) + dy;
          if (sx < 0 || sy < 0 || sx >= static_cast<int>(w) || sy >= static_cast<int>(h))
            continue;
          const size_t j = static_cast<size_t>(sy) * w + sx;
          if (!mask[j]) continue;
          rgb[i * 3] = src[j * 3];
          rgb[i * 3 + 1] = src[j * 3 + 1];
          rgb[i * 3 + 2] = src[j * 3 + 2];
        }
      }
    }
    if (std::getenv("ZIXX_EXP_DEBUG")) {
      size_t mc = 0;
      for (size_t i = 0; i < n; ++i) mc += mask[i];
      std::fprintf(stderr, "exp mask %zu of %zu" "\n", mc, n);
    }
    if (g_exp_contour) {
      // The sheets' bold ink line around the whole creature. The ordinary
      // contour is 2 px (1 px vanished into the creature's own dark edges at
      // 240p); RUN 2234's deliberately thick variants select 5 px. With boil
      // active the line inherits the boil's wobble via the mask.
      std::vector<uint8_t> edge(n, 0);
      for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
          const size_t i = static_cast<size_t>(y) * w + x;
          if (!mask[i]) continue;
          bool e = false;
          for (int r = 1; r <= g_exp_contour_radius && !e; ++r) {
            e = (static_cast<int>(x) - r < 0 || !mask[i - r]) ||
                (x + r >= w || !mask[i + r]) ||
                (static_cast<int>(y) - r < 0 || !mask[i - static_cast<size_t>(r) * w]) ||
                (y + r >= h || !mask[i + static_cast<size_t>(r) * w]);
          }
          edge[i] = e ? 1 : 0;
        }
      }
      for (size_t i = 0; i < n; ++i) {
        if (!edge[i]) continue;
        rgb[i * 3] = 26;
        rgb[i * 3 + 1] = 24;
        rgb[i * 3 + 2] = 22;
      }
    }
    if (g_cel_main) {
      // Exterior-only contour. Four-neighbour flood fill identifies only the
      // background connected to the viewport border; enclosed holes therefore
      // remain uninked. The ring then grows with eight-neighbour connectivity
      // through that exterior and never overwrites a creature pixel.
      std::vector<uint8_t> exterior(n, 0);
      std::vector<size_t> queue;
      queue.reserve(n);
      const auto admit_exterior = [&](uint32_t x, uint32_t y) {
        const size_t i = static_cast<size_t>(y) * w + x;
        if (mask[i] || exterior[i]) return;
        exterior[i] = 1;
        queue.push_back(i);
      };
      for (uint32_t x = 0; x < w; ++x) {
        admit_exterior(x, 0);
        if (h > 1) admit_exterior(x, h - 1);
      }
      for (uint32_t y = 1; y + 1 < h; ++y) {
        admit_exterior(0, y);
        if (w > 1) admit_exterior(w - 1, y);
      }
      for (size_t q = 0; q < queue.size(); ++q) {
        const size_t i = queue[q];
        const uint32_t x = static_cast<uint32_t>(i % w);
        const uint32_t y = static_cast<uint32_t>(i / w);
        if (x > 0) admit_exterior(x - 1, y);
        if (x + 1 < w) admit_exterior(x + 1, y);
        if (y > 0) admit_exterior(x, y - 1);
        if (y + 1 < h) admit_exterior(x, y + 1);
      }

      std::vector<uint8_t> edge(n, 0);
      std::vector<size_t> frontier;
      frontier.reserve(n);
      for (size_t i = 0; i < n; ++i)
        if (mask[i]) frontier.push_back(i);
      const int ink_width = cel_main_ink_width(primary_radius_q8);
      for (int r = 0; r < ink_width && !frontier.empty(); ++r) {
        std::vector<size_t> next;
        next.reserve(frontier.size() * 2);
        for (const size_t i : frontier) {
          const int x = static_cast<int>(i % w);
          const int y = static_cast<int>(i / w);
          for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
              if (dx == 0 && dy == 0) continue;
              const int nx = x + dx;
              const int ny = y + dy;
              if (nx < 0 || ny < 0 || nx >= static_cast<int>(w) ||
                  ny >= static_cast<int>(h))
                continue;
              const size_t j = static_cast<size_t>(ny) * w +
                               static_cast<uint32_t>(nx);
              if (!exterior[j] || edge[j]) continue;
              edge[j] = 1;
              next.push_back(j);
            }
          }
        }
        frontier.swap(next);
      }
      for (size_t i = 0; i < n; ++i) {
        if (!edge[i]) continue;
        rgb[i * 3] = kCelInkR;
        rgb[i * 3 + 1] = kCelInkG;
        rgb[i * 3 + 2] = kCelInkB;
      }
    }
    ++g_exp_frame;
  }
  if (c.moving_light && c.moving_markers)
    draw_zixx_moving_source_markers(c, rgb, depth, w, h);
  // creature 02: the mana and the belly core draw AFTER the cel ink pass —
  // effects glow over the drawn creature, never inside its ink mask (each
  // 2 px mote was getting its own black outline ring).
  // The mana populations: drawn HERE so they land after the chained sky and
  // celestial passes (which repaint depth==0). Depth-tested, never writing —
  // the particle law — through the same draw_population the hardware previews.
  if (!c.u02_add_pop.parts.empty() || !c.u02_tri_pop.parts.empty() ||
      !c.u02_opq_pop.parts.empty()) {
    zref::render::WorkSurface fsurf;
    fsurf.w = w;
    fsurf.h = h;
    fsurf.rgb.assign(rgb, rgb + static_cast<size_t>(w) * h * 3);
    fsurf.depth.assign(depth, depth + static_cast<size_t>(w) * h);
    const zref::render::Viewport fvp{0, 0, w, h};
    if (!c.u02_opq_pop.parts.empty())
      zref::render::draw_population(fsurf, fvp, c.vp, c.u02_opq_pop, 0x0001, nullptr);
    if (!c.u02_add_pop.parts.empty())
      zref::render::draw_population(fsurf, fvp, c.vp, c.u02_add_pop, 0x0005, nullptr);
    if (!c.u02_tri_pop.parts.empty())
      zref::render::draw_population(fsurf, fvp, c.vp, c.u02_tri_pop, 0x0006, nullptr);
    std::memcpy(rgb, fsurf.rgb.data(), fsurf.rgb.size());
  }
  // S5 layer two: the small core drawn OVER the composed body, no depth
  // test -- the belly-light shining through the skin. Same frame ramp,
  // scaled by its own calm gain.
  // ---- pass 2: the mana menu, POST-compose layer — bullets, bolts, the
  // drip, the caged core (depth-tested at each splat's own 1/w so the body
  // occludes honestly; the caged core sets depth_test=false and shines
  // through the blade).
  if (c.u02_mana != 0 && !c.u02_mana_splats.empty()) {
    const zref::render::Viewport vpp_m{0, 0, w, h};
    for (const u02::ManaSplat& ms : c.u02_mana_splats) {
      if (ms.pre) continue;
      const zref::render::ProjOut pm = zref::render::project_vertex(
          c.vp, vpp_m, zref::fx16{ms.x}, zref::fx16{ms.y}, zref::fx16{ms.z}, nullptr);
      if (!pm.in) continue;
      u02::GlowFrame gf2 = s_mana_ramps[ms.ramp];
      if (ms.gain_pm != 1000) {
        for (int i = 0; i < 64; ++i)
          for (int ch = 0; ch < 3; ++ch)
            gf2.pal[i][ch] = static_cast<uint8_t>(gf2.pal[i][ch] * ms.gain_pm / 1000);
      }
      u02::glow_splat(rgb, depth, w, h, s_glow_assets, gf2, pm.s.x >> 8, pm.s.y >> 8,
                      ms.r_px, pm.s.d, ms.depth_test, /*bloom=*/true, ms.opaque);
    }
  }
  if (c.u02_glow && cr_glow_n > 0) {
    u02::GlowFrame core;
    u02::glow_build_ramp(core, u02::kGlowLo, u02::kGlowMid, u02::kGlowHi,
                         u02::kCentreGlowCoreGainPm);
    for (int gi = 0; gi < cr_glow_n; ++gi)
      u02::glow_splat(rgb, depth, w, h, s_glow_assets, core, cr_glow_px[gi][0],
                      cr_glow_px[gi][1], u02::kCentreGlowCorePx, 0, false);
  }

  c.gibs_in_view = 0;
  if (c.gibs == nullptr || c.gibs->empty()) return;

  // Reel-only detached chunks: each piece is a separately translated and
  // rotating cube, depth-tested through zrender's raster. This is deliberately
  // a fixed fragment list, not a new particle or rigging framework.
  zref::render::WorkSurface surf;
  surf.w = w;
  surf.h = h;
  surf.rgb.assign(rgb, rgb + static_cast<size_t>(w) * h * 3);
  surf.depth.assign(depth, depth + static_cast<size_t>(w) * h);
  const zref::render::Viewport vpp{0, 0, w, h};
  constexpr uint8_t kTri[12][3] = {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7},
                                   {0, 1, 5}, {0, 5, 4}, {3, 7, 6}, {3, 6, 2},
                                   {0, 4, 7}, {0, 7, 3}, {1, 2, 6}, {1, 6, 5}};
  constexpr uint16_t kFaceShade[6] = {176, 224, 192, 240, 160, 208};
  for (const ReelGibPiece& g : *c.gibs) {
    const zref::render::ProjOut centre = zref::render::project_vertex(
        c.vp, vpp, zref::fx16{g.x}, zref::fx16{g.y}, zref::fx16{g.z}, nullptr);
    if (!centre.in) continue;
    ++c.gibs_in_view;

    const uint16_t remaining = static_cast<uint16_t>(g.lifetime - g.age);
    const int32_t half =
        remaining < 6 ? static_cast<int32_t>((static_cast<int64_t>(g.half_extent) * remaining) / 6)
                      : g.half_extent;
    const int32_t cy = zref::fx_cos(zref::angle16{g.yaw}).raw;
    const int32_t sy = zref::fx_sin(zref::angle16{g.yaw}).raw;
    const int32_t cp = zref::fx_cos(zref::angle16{g.pitch}).raw;
    const int32_t sp = zref::fx_sin(zref::angle16{g.pitch}).raw;
    std::array<zref::render::ProjOut, 8> pv;
    for (int vi = 0; vi < 8; ++vi) {
      const int32_t lx = (vi & 1) != 0 ? half : -half;
      const int32_t ly = (vi & 2) != 0 ? half : -half;
      const int32_t lz = (vi & 4) != 0 ? half : -half;
      const int32_t x1 = zref::rescale_s32(
          static_cast<int64_t>(cy) * lx + static_cast<int64_t>(sy) * lz, 16, nullptr);
      const int32_t z1 = zref::rescale_s32(
          -static_cast<int64_t>(sy) * lx + static_cast<int64_t>(cy) * lz, 16, nullptr);
      const int32_t y2 = zref::rescale_s32(
          static_cast<int64_t>(cp) * ly - static_cast<int64_t>(sp) * z1, 16, nullptr);
      const int32_t z2 = zref::rescale_s32(
          static_cast<int64_t>(sp) * ly + static_cast<int64_t>(cp) * z1, 16, nullptr);
      pv[vi] = zref::render::project_vertex(c.vp, vpp, zref::fx16{g.x + x1}, zref::fx16{g.y + y2},
                                            zref::fx16{g.z + z2}, nullptr);
    }
    for (int ti = 0; ti < 12; ++ti) {
      const zref::render::ProjOut& a = pv[kTri[ti][0]];
      const zref::render::ProjOut& b = pv[kTri[ti][1]];
      const zref::render::ProjOut& d = pv[kTri[ti][2]];
      if (!a.in || !b.in || !d.in) continue;
      const uint16_t shade = kFaceShade[ti / 2];
      zref::render::TriMode tm;
      zref::render::raster_tri(surf, vpp, a.s, b.s, d.s,
                               static_cast<uint8_t>((g.r * shade + 128) >> 8),
                               static_cast<uint8_t>((g.g * shade + 128) >> 8),
                               static_cast<uint8_t>((g.b * shade + 128) >> 8), tm);
    }
  }
  std::memcpy(rgb, surf.rgb.data(), surf.rgb.size());
  std::memcpy(depth, surf.depth.data(), surf.depth.size() * sizeof(int32_t));
}

// ---------------------------------------------------- chained pre-resolve --
// S1 (creature 02): the renderer stores ONE pre-resolve (fn, ctx); the reel
// used to overwrite it, which made sky / celestial / creature hooks mutually
// exclusive claimants -- a creature subject could never carry a sun effect.
// This dispatcher chains up to three hooks in the fixed compositing order
// sky -> celestial -> creature (the sky splat touches only depth == 0 pixels,
// the star compose depth-tests, the creature compose writes depth). With
// exactly one hook registered it invokes that hook with the very arguments
// the renderer would have handed it directly -- identity by construction,
// CRC-proven against the existing bank at the S1 spike.
struct HookChain {
  enum Slot { kSky = 0, kCelestial = 1, kCreature = 2 };
  zref::render::SoftwareRenderer::PreResolveFn fn[3] = {nullptr, nullptr, nullptr};
  void* ctx[3] = {nullptr, nullptr, nullptr};
  void install(int slot, zref::render::SoftwareRenderer::PreResolveFn f, void* c) {
    fn[slot] = f;
    ctx[slot] = c;
  }
  bool any() const { return fn[0] != nullptr || fn[1] != nullptr || fn[2] != nullptr; }
};

void chained_pre_resolve(void* vctx, uint8_t* rgb, int32_t* depth, uint32_t w, uint32_t h,
                         uint32_t tick) {
  HookChain* c = static_cast<HookChain*>(vctx);
  for (int i = 0; i < 3; ++i)
    if (c->fn[i] != nullptr) c->fn[i](c->ctx[i], rgb, depth, w, h, tick);
}

// ------------------------------------------------------------ scene render --

int render_scene(const SceneSubject& sub) {
  const uint32_t W = 384, H = 240;
  // S2: resolve the species once for the whole subject. kAuto reproduces the
  // pre-enum binary rule exactly, so every existing subject selects as it
  // always did; creature-02 subjects set kUnnamed02 explicitly.
  const Species species = sub.species != Species::kAuto
                              ? sub.species
                              : (sub.creature >= 3 ? Species::kZixxtrixx : Species::kWatchdog);
  zref::render::TerrainPatch patch =
      sub.island ? dual_island_patch() : rtest::bump_patch(161, 161, sub.bump_ext, 8);
  if (sub.island_flat) {  // keep the deep keel, drop the texture lane
    patch.tileset_id = 0;
    patch.mat_a.clear();
    patch.mat_b.clear();
    patch.mat_w.clear();
    patch.tint.clear();
  }
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
  // the island tileset (terrain_rules 6; the patch carries tileset_id 90)
  if (sub.island) res.tilesets.push_back({90, *island_tileset()});
  zref::render::Population debris_pop;  // rebuilt per frame
  res.populations.push_back({3, debris_pop});

  res.sky_sets.push_back({2, sky});

  zref::render::SoftwareRenderer rend;  // ONE renderer: sheets persist
  HookChain hook_chain;  // S1: outlives the frame loop; installed below if any slot fills
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
    hook_chain.install(HookChain::kCelestial, &cel_hook, &cel_ctx);
  }

  // Planetside sky: the six-bit intensity plane replaces the RGB dome outright,
  // so it is its own hook and is exclusive with the celestial compositor. The
  // sun is IN the plane, so there is no star sprite to compose.
  PlanetSky psky;
  if (sub.planet > 0) {
    const PlanetDef& pd = kPlanets[(sub.planet - 1) % kPlanetCount];
    planet_ramp(psky.ramp, pd.lo, pd.mid, pd.hi);
    psky.base = pd.base;
    psky.noise_amp = pd.noise;
    psky.sun_mag = pd.mag;
    psky.sun_core = pd.core;
    psky.seed = static_cast<uint32_t>(sub.planet) * 2654435761u;
    psky.sun_x = sub.planet_sun_x;
    psky.sun_y = sub.planet_sun_y0;
    psky.sun2_mag = sub.planet_sun2_mag;
    psky.sun2_x = sub.planet_sun2_x;
    psky.sun2_y = sub.planet_sun2_y;
    psky.horizon_y = 150;
    hook_chain.install(HookChain::kSky, &planet_sky_hook, &psky);
  }

  const std::string dir = g_out + "/" + sub.name;
  if (g_write) {
    ZHAO_MKDIR(g_out.c_str());
    ZHAO_MKDIR(dir.c_str());
  }

  // creature subject state (zref::creature — the laws live there)
  const zc::CreatureType* dog = nullptr;
  zc::CreatureInstance dog_inst;
  zc::CreatureInstance dummy_inst;  // run 0326: the salto target dummy
  zc::CreatureInstance u02_extra_inst[2];  // the trio's phase-offset pair
  ZixxTargetDescriptor target_desc;
  zc::PoseBank dog_poses;
  std::vector<ReelGibPiece> gibs;
  CreatureReelCtx cr_ctx;
  int32_t pop_threshold = 0;
  int32_t gib_gravity = 0;
  uint32_t gib_spawn_frame = sub.frames;
  uint32_t gib_spawn_count = 0;
  uint32_t gib_visible_frames = 0;
  int32_t gib_initial_span = 0;
  int32_t gib_max_span = 0;
  if (sub.creature != 0) {
    // 1,2 = the watchdog (wave-walk, bulk-pop). 3,4 = Zixxtrixx (slither,
    // tail-strike) — the Upheaval bestiary lane.
    zc::g_debug_shade = static_cast<zc::DebugShade>(sub.creature_shade);
    const bool zixx_subject = species == Species::kZixxtrixx;
    dog = zixx_subject ? &zixx::type()
                       : (species == Species::kUnnamed02 ? &u02::type() : &watchdog_type());
    dog_inst.type = dog;
    // A quadruped pitches and rolls with the ground under its feet. A 3.9 m
    // serpent lying ON the ground does not: tilting the whole animal off one
    // column sample lifted its tail a metre into the air over a crest. Roll
    // only for Zixxtrixx.
    dog_inst.tilt_mode = species == Species::kUnnamed02
                             ? zc::TiltMode::kNone  // it FLOATS; terrain never tilts it
                             : (zixx_subject ? zc::TiltMode::kSideways : zc::TiltMode::kCompletely);
    // Zixxtrixx is shot on an orbit, so its facing only has to look right at
    // frame 0; the watchdog keeps its authored front-quarter read.
    // Facing 0 is already side-on to the orbit camera at frame 0 -- a quarter
    // turn puts us END-ON, which is worse. Kept at 0; the salto's dive reading
    // small is NOT a staging problem, see the run log.
    dog_inst.facing = zref::angle16{
        (zixx_subject || species == Species::kUnnamed02) ? uint16_t{0} : uint16_t{0x1000}};
    // Zixxtrixx clip slots: 3 = idle, 4 = caterpillar walk, 5 = triple salto,
    // 6 = falling flail. The watchdog keeps its own two.
    dog_inst.anim.cut((zixx_subject || species == Species::kUnnamed02)
                          ? static_cast<uint16_t>(sub.creature - 2)
                          : (sub.creature == 1 ? 2 : 1));
    dog_inst.anim.frozen = sub.creature_hold;
    // Stand the animal on its OWN centre, not its root bone, or the orbit
    // swings its body out of frame. The walk starts half its travel back
    // from there so it crosses through the middle of the shot.
    if (zixx_subject) dog_inst.x = fxm(zixx::kStageCentreMm);
    if (species == Species::kUnnamed02) dog_inst.x = fxm(u02::kStageCentreMm);
    if (zixx_subject && sub.creature == 4)
      dog_inst.x -= fxm(zixx::kWalkSpeed * static_cast<int32_t>(sub.frames)) / 2;
    if (zixx_subject && sub.creature == 29)
      dog_inst.x -= fxm(zixx::kRunSpeed * static_cast<int32_t>(sub.frames)) / 2;
    cr_ctx.inst = &dog_inst;
    cr_ctx.poses = &dog_poses;
    cr_ctx.force_micro = sub.creature_force_micro;
    cr_ctx.moving_light = sub.creature_moving_light;
    if (species == Species::kUnnamed02) {
      cr_ctx.moving_rig = &kU02MovingRig;
      cr_ctx.sun_rig = &kU02SunRig;         // Stage M's mist knob rides here
      cr_ctx.moving_markers = false;        // Direction 3 §1: no light dot
    }
    cr_ctx.sun_light =
        sub.sun != nullptr && g_zixx_suns_enabled && !sub.creature_moving_light;
    if (sub.dummy) {
      const uint16_t attack_slot = static_cast<uint16_t>(sub.creature - 2);
      target_desc = zixx_target_descriptor(attack_slot);
      dummy_inst.type = target_desc.type;
      dummy_inst.tilt_mode = zc::TiltMode::kCompletely;
      dummy_inst.facing = target_desc.facing;
      dummy_inst.anim.cut(target_desc.clip_slot);
      dummy_inst.x = fxm(zixx::kStageCentreMm + target_desc.x_mm);
      dummy_inst.y = fxm(target_desc.y_mm);
      cr_ctx.dummy = &dummy_inst;
    }
    cr_ctx.gibs = &gibs;
    cr_ctx.u02_glow = sub.u02_glow;
    cr_ctx.u02_glow_trio = sub.u02_glow_trio;
    if (species == Species::kUnnamed02 && sub.u02_trio) {
      for (int e = 0; e < 2; ++e) {
        u02_extra_inst[e].type = dog;
        u02_extra_inst[e].tilt_mode = zc::TiltMode::kNone;
        u02_extra_inst[e].facing = zref::angle16{0};
        u02_extra_inst[e].anim.cut(static_cast<uint16_t>(sub.creature - 2));
        // phase-offset a third of the loop each: variety costs pose decode,
        // which is exactly what the trio subject exists to demonstrate
        const zc::Clip* cl0 = nullptr;
        for (const zc::Clip& cc : dog->bank.clips)
          if (cc.slot_id == dog_inst.anim.slot) cl0 = &cc;
        if (cl0 != nullptr)
          u02_extra_inst[e].anim.frame = static_cast<uint16_t>(
              (cl0->frame_count / 3) * (e + 1) % cl0->frame_count);
        u02_extra_inst[e].x = dog_inst.x + fxm(e == 0 ? -2600 : 2000);
        u02_extra_inst[e].z = dog_inst.z + fxm(e == 0 ? 1600 : -2200);
        cr_ctx.u02_extra[e] = &u02_extra_inst[e];
      }
    }
    pop_threshold = 22 * (1 << 16) / 10;  // bulk 2.2 pops (species constant)
    gib_gravity = fxm(18);                // per frame^2
    hook_chain.install(HookChain::kCreature, &creature_hook, &cr_ctx);
  }

  // S1: one dispatcher serves every registered hook, in compositing order.
  if (hook_chain.any()) rend.set_pre_resolve(&chained_pre_resolve, &hook_chain);

  uint32_t breach_total = 0;
  for (uint32_t f = 0; f < sub.frames; ++f) {
    const uint32_t tick = f * sub.step;
    cel_ctx.frame = f;  // the hook reads the authored per-frame positions

    // The sun rises and sets. Its height drives the whole look on its own,
    // because the vertical ramp is brightest at the horizon: low down, the
    // splat lands on already-bright sky and rails over a wide area, so the
    // bloom spreads and loses every edge. High up it lands on dim sky and
    // stays compact. That interaction IS the elevation behaviour -- the donor
    // has no air-mass or path-length term anywhere, and neither does this.
    if (sub.planet > 0) {
      const uint32_t phh = f < (sub.frames / 2) ? f : (sub.frames - 1 - f);
      const uint32_t denom = (sub.frames / 2) > 1 ? (sub.frames / 2) - 1 : 1;
      psky.sun_y = sub.planet_sun_y0 + static_cast<int32_t>(
          (static_cast<int64_t>(sub.planet_sun_y1 - sub.planet_sun_y0) * phh) / denom);
    }

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

    // TRACKING CAMERA (Fabian, 2026-08-27: "It is very important the camera
    // follow it, you did not do that"): follow the salto's AUTHORED flight
    // path -- lift and forward drive from the same file-scope curves the
    // clip is built from, evaluated at reel-frame resolution. Not the
    // decoded root, which also carries the coil re-pivot wobble.
    int32_t trk_x = 0, trk_y = 0;
    if (sub.cam_track && sub.creature == 5) {
      // AIM AT THE SPEAR, NOT THE NOSE (Fabian, 2026-08-27 pass 3: the shot
      // missed "the most important thing, which is the ground hit where the
      // tail actually buries"). The root is the NOSE, and at impact the nose
      // hangs 2.4 m above the ground while the tail tip is 3.3 m below it
      // and 1.9 m ahead -- so a nose-framed camera puts the burial at the
      // bottom edge or off it. kAtkAim blends the tracked point from the
      // nose to the SPEAR'S MIDPOINT as the javelin forms, and holds it
      // there through the dive, the impact and the whole five-second stick.
      const int32_t aim = zixx::attack_aim_mille(static_cast<int>(f));
      trk_x = fxm(zixx::attack_fwd_mm(static_cast<int>(f)) +
                  (aim * (zixx::kAtkTipFwd / 2)) / 1000);
      trk_y = fxm((zixx::attack_lift_mm(static_cast<int>(f)) * sub.cam_track_num) / 1000 -
                  (aim * (zixx::kAtkTipDrop / 2)) / 1000);
    } else if (sub.cam_track &&
               ((sub.creature >= 35 && sub.creature <= 37) ||
                sub.creature == zixx::kSlotAtkNine + 2)) {
      // RUN 1939 (owner: "Salto camera is too jittery"): the variant camera
      // used to follow the clip's BAKED ROOT, which carries the coil
      // re-pivot orbit -- six somersaults put six 485 mm camera orbits into
      // the shot. It now follows the PLAN's smooth trajectory
      // (zixx_variant_track): where the animal is going, not how it is
      // oriented -- the same lesson the golden attack's camera already
      // carries. Frame -> key at the 2-frames-per-key law, lerped at the
      // half key so the track cannot step.
      const uint16_t slot = static_cast<uint16_t>(sub.creature - 2);
      int32_t x0, y0, x1, y1;
      const int key = static_cast<int>(f / 2);
      zixx::zixx_variant_track(slot, key, x0, y0);
      zixx::zixx_variant_track(slot, key + 1, x1, y1);
      const int32_t xm = (f & 1) ? (x0 + x1) / 2 : x0;
      const int32_t ym = (f & 1) ? (y0 + y1) / 2 : y0;
      trk_x = fxm(xm);
      trk_y = static_cast<int32_t>(
          (static_cast<int64_t>(fxm(ym)) * sub.cam_track_num) / 1000);
    } else if (sub.cam_track &&
               (sub.creature == zixx::kSlotJumpOne + 2 ||
                sub.creature == zixx::kSlotJumpMulti + 2)) {
      // Immediate jumps share one smooth vertical trajectory sample. Follow
      // lift and spring descent, never the wheel's c-R(c) re-pivot orbit.
      const uint16_t slot = static_cast<uint16_t>(sub.creature - 2);
      const int32_t count = slot == zixx::kSlotJumpOne ? 1 : 3;
      const zixx::JumpPlan p = zixx::zixx_jump_plan(slot, count);
      const int key = static_cast<int>(f / 2);
      int32_t x0, y0, x1, y1;
      zixx::zixx_jump_track(p, key, x0, y0);
      zixx::zixx_jump_track(p, key + 1, x1, y1);
      const int32_t xm = (f & 1) ? (x0 + x1) / 2 : x0;
      const int32_t ym = (f & 1) ? (y0 + y1) / 2 : y0;
      trk_x = fxm(xm);
      trk_y = static_cast<int32_t>(
          (static_cast<int64_t>(fxm(ym)) * sub.cam_track_num) / 1000);
    }

    // ---- creature sim (the driver composes the tick cadence; the laws are
    // zref::creature's). One lattice compose per frame feeds the tilt taps —
    // the SAME compose_lattice the renderer's DrawProcedural runs inside
    // (physics equals pixels, terrain_rules 4.1).
    int32_t cam_eye = sub.cam_eye, cam_dist = sub.cam_dist, cam_bias = sub.cam_bias;
    if (dog != nullptr) {
      if (!gibs.empty() && f > gib_spawn_frame) {
        advance_reel_gibs(gibs, dog_inst.y, gib_gravity);
      }
      if (sub.cam_pull && f >= sub.cam_pull0) {
        const int64_t n = sub.frames - sub.cam_pull0;
        const int64_t k = f - sub.cam_pull0;
        cam_eye =
            static_cast<int32_t>(sub.cam_eye + (k * (sub.cam2_eye - sub.cam_eye) + n / 2) / n);
        cam_dist =
            static_cast<int32_t>(sub.cam_dist + (k * (sub.cam2_dist - sub.cam_dist) + n / 2) / n);
        cam_bias =
            static_cast<int32_t>(sub.cam_bias + (k * (sub.cam2_bias - sub.cam_bias) + n / 2) / n);
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

      if (species == Species::kUnnamed02) {
        // creature 02 FLOATS: advance the clip on the sim clock and nothing
        // else -- no ground tilt (kNone), no travel (drift authors its own
        // root XZ in the clip), no gibs, no bulk.
        for (uint32_t t = 0; t < sub.step; ++t) {
          const zc::ClipEvent* fired = nullptr;
          uint8_t nf = 0;
          zc::anim_advance(dog_inst.anim, dog->bank, &fired, nf);
          for (int e = 0; e < 2; ++e)
            if (cr_ctx.u02_extra[e] != nullptr)
              zc::anim_advance(cr_ctx.u02_extra[e]->anim, dog->bank, &fired, nf);
        }
      } else if (sub.creature >= 3) {
        // run 0326: scripted state cuts (knockdown -> getUp chains, the
        // directional-damage tour) -- the game's own hard cut, on a frame
        for (const auto& cc : sub.clip_cuts)
          if (f == cc.first) dog_inst.anim.cut(cc.second);
        // Zixxtrixx. Only the gaits travel; the idle, the salto and the
        // fall stay put, because each of those shots is about the animal
        // rather than about it going somewhere. No gibs on this lane.
        if (sub.creature == 29) {  // the run travels at its own speed
          const int32_t fc = zref::fx_cos(dog_inst.facing).raw;
          const int32_t fs = zref::fx_sin(dog_inst.facing).raw;
          dog_inst.x += zref::rescale_s32(
              static_cast<int64_t>(fxm(zixx::kRunSpeed)) * fc, 16, nullptr);
          dog_inst.z -= zref::rescale_s32(
              static_cast<int64_t>(fxm(zixx::kRunSpeed)) * fs, 16, nullptr);
        }
        if (sub.creature == 4) {
          const int32_t fc = zref::fx_cos(dog_inst.facing).raw;
          const int32_t fs = zref::fx_sin(dog_inst.facing).raw;
          dog_inst.x += zref::rescale_s32(
              static_cast<int64_t>(fxm(zixx::kWalkSpeed)) * fc, 16, nullptr);
          dog_inst.z -= zref::rescale_s32(
              static_cast<int64_t>(fxm(zixx::kWalkSpeed)) * fs, 16, nullptr);
        }
        // THE IDLE BREATHES IN GIRTH. A clip carries rotations and a root
        // offset, not scale, so the swell rides the instance bulk. Its phase
        // belongs to the IDLE CLIP, never to the presentation length: the site
        // orbit is three clip loops long, and dividing by sub.frames made one
        // slow girth cycle fight three real breaths (fat on exhale, thin on
        // inhale). Fixed side happened to hide the bug because it is one loop.
        if (sub.creature == 3 && !sub.creature_hold) {
          constexpr uint32_t kIdlePresentationFrames = zixx::kIdleKeys * 2u;
          const uint32_t idle_frame =
              static_cast<uint32_t>((static_cast<uint64_t>(f) * sub.step) %
                                    kIdlePresentationFrames);
          const uint16_t gph = static_cast<uint16_t>(
              (static_cast<uint64_t>(idle_frame) * 65536u) /
              kIdlePresentationFrames);
          const int32_t gsn = zref::fx_sin(zref::angle16{gph}).raw;
          dog_inst.bulk.scale = (1 << 16) + static_cast<int32_t>(
              (static_cast<int64_t>(gsn) * zixx::kIdleGirth) / 1000);
          dog_inst.bulk.target = dog_inst.bulk.scale;
        }
        const bool advance_creature =
            !sub.canonical_creature_timeline || f > 0;
        if (advance_creature) {
          for (uint32_t t = 0; t < sub.step; ++t) {
            const zc::ClipEvent* fired = nullptr;
            uint8_t nf = 0;
            zc::anim_advance(dog_inst.anim, dog->bank, &fired, nf);
            if (sub.dummy) {
              const zc::ClipEvent* df = nullptr;
              uint8_t dn = 0;
              zc::anim_advance(dummy_inst.anim, dummy_inst.type->bank, &df, dn);
            }
          }
        }
        if (sub.dummy) {
          const uint16_t attack_slot =
              static_cast<uint16_t>(sub.creature - 2);
          const uint64_t shown_tick = sub.canonical_creature_timeline
                                          ? static_cast<uint64_t>(f) * sub.step
                                          : f;
          if (shown_tick >= static_cast<uint64_t>(
                                zixx_target_freeze_tick(attack_slot)))
            dummy_inst.anim.frozen = true;
        }
        zc::ground_tilt_update(dog_inst.tilt, dog_inst.tilt_mode, dog_inst.facing, lat,
                               zref::fx16{dog_inst.x}, zref::fx16{dog_inst.z}, zref::fx16{fxm(40)},
                               zref::fx16{fxm(20)});
        if (sub.dummy)
          zc::ground_tilt_update(dummy_inst.tilt, dummy_inst.tilt_mode, dummy_inst.facing, lat,
                                 zref::fx16{dummy_inst.x}, zref::fx16{dummy_inst.z},
                                 zref::fx16{fxm(40)}, zref::fx16{fxm(20)});
      } else if (sub.creature == 1) {
        // walk: root motion follows the authored +X forward axis at a
        // front-quarter yaw; four independent legs carry the 16-key stride.
        // The clip clock runs on the sim clock (sub.step ticks per frame), so
        // the 32-tick stride cycle covers 0.70 m of root motion -- about one
        // body length per cycle, which is what keeps the feet planted instead
        // of skating a 16-key cycle through 0.088 m.
        const int32_t fc = zref::fx_cos(dog_inst.facing).raw;
        const int32_t fs = zref::fx_sin(dog_inst.facing).raw;
        dog_inst.x += zref::rescale_s32(static_cast<int64_t>(fxm(22)) * fc, 16, nullptr);
        dog_inst.z -= zref::rescale_s32(static_cast<int64_t>(fxm(22)) * fs, 16, nullptr);
        for (uint32_t t = 0; t < sub.step; ++t) {
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
        dog_inst.bulk.target =
            f < 16 ? (1 << 16) : static_cast<int32_t>(65536 + (f - 16) * (65536 * 14 / 10) / 24);
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
                pose[b].m[i * 4 + j] =
                    zref::rescale_s32(static_cast<int64_t>(pose[b].m[i * 4 + j]) * bs, 16, nullptr);
          }
          spawn_reel_gibs(*dog, pose.data(), dog_inst.x, dog_inst.y, dog_inst.z, dog_inst.y, gibs);
          gib_spawn_frame = f;
          gib_spawn_count = static_cast<uint32_t>(gibs.size());
          if (gib_spawn_count < 12) {
            std::fprintf(stderr, "%s frame %u: only %u detached pieces spawned (need >=12)\n",
                         sub.name, f, gib_spawn_count);
            return 5;
          }
          int32_t min_x = gibs.front().x, max_x = gibs.front().x;
          int32_t min_y = gibs.front().y, max_y = gibs.front().y;
          int32_t min_z = gibs.front().z, max_z = gibs.front().z;
          for (const ReelGibPiece& g : gibs) {
            min_x = std::min(min_x, g.x);
            max_x = std::max(max_x, g.x);
            min_y = std::min(min_y, g.y);
            max_y = std::max(max_y, g.y);
            min_z = std::min(min_z, g.z);
            max_z = std::max(max_z, g.z);
            if (g.y < dog_inst.y + g.half_extent) {
              std::fprintf(stderr, "%s frame %u: gib spawned below ground\n", sub.name, f);
              return 5;
            }
          }
          gib_initial_span = std::max({max_x - min_x, max_y - min_y, max_z - min_z});
          gib_max_span = gib_initial_span;
        }
      }
      // ground snap (root sits on the leg length; the skeleton carries 0.52)
      const zref::terrain::ColumnResult col =
          zref::terrain::column_query(lat, zref::fx16{dog_inst.x}, zref::fx16{dog_inst.z});
      if (col.cls == zref::terrain::ColumnClass::kSolid) dog_inst.y = col.top.raw;
      for (int e = 0; e < 2; ++e) {
        if (cr_ctx.u02_extra[e] == nullptr) continue;
        const zref::terrain::ColumnResult ec = zref::terrain::column_query(
            lat, zref::fx16{cr_ctx.u02_extra[e]->x}, zref::fx16{cr_ctx.u02_extra[e]->z});
        if (ec.cls == zref::terrain::ColumnClass::kSolid) cr_ctx.u02_extra[e]->y = ec.top.raw;
      }
      // ---- creature 02: posed anchors + the mana emitter tables ----------
      if (species == Species::kUnnamed02 && (sub.u02_mana != 0 || sub.u02_glow)) {
        const zc::Clip* cl = nullptr;
        for (const zc::Clip& cc : dog->bank.clips)
          if (cc.slot_id == dog_inst.anim.slot) cl = &cc;
        if (cl != nullptr) {
          cr_ctx.u02_add_pop.parts.clear();
          cr_ctx.u02_tri_pop.parts.clear();
          cr_ctx.u02_opq_pop.parts.clear();
          cr_ctx.u02_mana_splats.clear();
          cr_ctx.u02_glow_count = 0;
          zc::CreatureInstance* all[3] = {&dog_inst, cr_ctx.u02_extra[0],
                                          cr_ctx.u02_extra[1]};
          for (int ii = 0; ii < 3; ++ii) {
            zc::CreatureInstance* ip = all[ii];
            if (ip == nullptr) continue;
            std::array<zc::mat3x4fx, zc::kMaxBones> pose;
            zc::decode_pose(*dog, *cl, ip->anim.frame, pose, nullptr, ip->anim.sub);
            const auto anchor = [&](uint8_t b, int32_t out[3]) {
              zc::SkinVertex sv{dog->baked.world_x[b], dog->baked.world_y[b],
                                dog->baked.world_z[b], b, b, 64, 0, 0};
              int32_t x, y, z;
              zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
              out[0] = ip->x + x;
              out[1] = ip->y + y;
              out[2] = ip->z + z;
            };
            u02::FxAnchors fa;
            anchor(u02::kBRoot, fa.body);
            anchor(u02::kBHingeA, fa.hinge_a);
            anchor(u02::kBHingeB, fa.hinge_b);
            anchor(u02::kBHingeC, fa.hinge_c);
            fa.crown[0] = fa.body[0];
            fa.crown[1] = fa.body[1] + u02::fxu(u02::vmm(u02::kBodyRadiusMm));
            fa.crown[2] = fa.body[2];
            // the ring-pocket centre: the hinge centroid, so hinge play
            // moves the mana (Direction 2 §4 — one performance)
            for (int k = 0; k < 3; ++k)
              fa.ring[k] = (fa.hinge_a[k] + fa.hinge_b[k] + fa.hinge_c[k]) / 3;
            cr_ctx.u02_glow_centres[cr_ctx.u02_glow_count][0] = fa.body[0];
            cr_ctx.u02_glow_centres[cr_ctx.u02_glow_count][1] = fa.body[1];
            cr_ctx.u02_glow_centres[cr_ctx.u02_glow_count][2] = fa.body[2];
            ++cr_ctx.u02_glow_count;
            if (ii == 0) {
              cr_ctx.u02_body[0] = fa.body[0];
              cr_ctx.u02_body[1] = fa.body[1];
              cr_ctx.u02_body[2] = fa.body[2];
              const zc::DeformSample ds = zc::deformation_sample(
                  *dog, dog_inst.anim.slot, dog_inst.anim.frame, dog_inst.anim.sub);
              cr_ctx.u02_glow_gain_pm =
                  850 +
                  static_cast<int32_t>((static_cast<int64_t>(ds.flatten) * 500) /
                                       (u02::kCompressAmpPm > 0 ? u02::kCompressAmpPm : 1));
            }
            if (sub.u02_mana != 0 && ii == 0) {
              cr_ctx.u02_mana = sub.u02_mana;
              cr_ctx.u02_frame = f;
              u02::mana_fill(sub.u02_mana, f, fa, cr_ctx.u02_mana_splats);
            }
          }
        }
      }
      // the salto target dummy stands (or hovers) relative to the SAME
      // terrain surface -- run 0326: at y=0 it stood eight metres under
      // the crown and never rendered
      if (sub.dummy) {
        const zref::terrain::ColumnResult dcol = zref::terrain::column_query(
            lat, zref::fx16{dummy_inst.x}, zref::fx16{dummy_inst.z});
        if (dcol.cls == zref::terrain::ColumnClass::kSolid)
          dummy_inst.y = dcol.top.raw + fxm(target_desc.y_mm);
      }
      // Sample exactly once, only after terrain snap. The resulting named
      // world-space coordinates live in cr_ctx.moving_sources and are consumed
      // unchanged by both the compositor and the depth-tested visible markers.
      if (sub.creature_moving_light) {
        if (species == Species::kUnnamed02) {
          sample_u02_moving_sources(f, sub.frames, dog_inst, cr_ctx.moving_sources);
        } else {
          sample_zixx_moving_source(f, sub.frames, dog_inst,
                                    cr_ctx.moving_sources[kZixxMovingSourceWarm]);
          sample_zixx_moving_colour_sources(f, sub.frames, dog_inst,
                                            cr_ctx.moving_sources,
                                            g_zixx_additive_normal);
        }
        apply_zixx_ml_solo(cr_ctx.moving_sources);
      }
      // Direction 29: the clip sun tracks the terrain-snapped instance, so
      // its direction on the animal is constant over the whole clip -- a sun.
      if (cr_ctx.sun_light)
        sample_zixx_clip_sun(*sub.sun, dog_inst, cr_ctx.moving_sources[0]);
      // Detached-chunk ballistics advance at the start of subsequent frames,
      // so the exact authored breakup pose is visible for one full frame.
      if (!gibs.empty()) {
        int32_t min_x = gibs.front().x, max_x = gibs.front().x;
        int32_t min_y = gibs.front().y, max_y = gibs.front().y;
        int32_t min_z = gibs.front().z, max_z = gibs.front().z;
        for (const ReelGibPiece& g : gibs) {
          min_x = std::min(min_x, g.x);
          max_x = std::max(max_x, g.x);
          min_y = std::min(min_y, g.y);
          max_y = std::max(max_y, g.y);
          min_z = std::min(min_z, g.z);
          max_z = std::max(max_z, g.z);
        }
        gib_max_span =
            std::max(gib_max_span, std::max({max_x - min_x, max_y - min_y, max_z - min_z}));
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
      // Wave E: an optional camera-distance SWEEP (cam_k_end != 0 lerps k
      // over the subject) -- the LOD-ladder acceptance ride
      int32_t k_now = sub.cam_k;
      if (sub.cam_k_end != 0 && sub.frames > 1) {
        k_now = sub.cam_k + static_cast<int32_t>(
            (static_cast<int64_t>(sub.cam_k_end - sub.cam_k) * f) / (sub.frames - 1));
      }
      sv.payload.view_projection =
          cam_pitch(k_now, cam_eye, cam_dist, sub.cam_ps, sub.cam_pc, cam_bias, -1, shake_raw);
      if (sub.orbit) {
        // one exact turn per loop: theta = f * 65536 / frames (integer)
        const uint16_t theta = static_cast<uint16_t>((static_cast<uint64_t>(f) * 65536u) /
                                                     (sub.frames > 0 ? sub.frames : 1));
        sv.payload.view_projection = mat4_mul(sv.payload.view_projection, rot_world_yaw(theta));
      }
      if (sub.cam_yaw != 0) {
        sv.payload.view_projection = mat4_mul(
            sv.payload.view_projection,
            rot_world_yaw(static_cast<uint16_t>(sub.cam_yaw & 0xFFFF)));
      }
      if (trk_x != 0 || trk_y != 0) {
        // the tracking camera: a true world translation, so the creature
        // stays in frame while the GROUND moves -- the sky, at infinity,
        // correctly does not (no sky matrix touched)
        sv.payload.view_projection =
            mat4_mul(sv.payload.view_projection, mat_world_translate(-trk_x, -trk_y, 0));
      }
      if (dog != nullptr) {
        cr_ctx.vp = rtest::to_zref(sv.payload.view_projection);
      }
      std::vector<uint8_t> v1;
      zhao_abi::zhao_pack_set_view(sv, v1);
      b.append_record(v1);

      // sky rotation: the terrain camera's own rows (pitch 26 deg down,
      // zsign -1) so the sky horizon and the terrain horizon agree; in
      // sky-sweep mode the pitch ping-pongs pitch0 -> pitch1 -> pitch0
      // across the loop (integer lerp on angle16 turns, table sin/cos)
      int32_t sky_ps = sub.cam_ps, sky_pc = sub.cam_pc, sky_bias = cam_bias;
      if (sub.sky_sweep) {
        const uint32_t half = sub.frames / 2;
        const uint32_t ph = f < half ? f : (sub.frames - 1 - f);
        const int32_t th =
            sub.sweep_pitch0 +
            static_cast<int32_t>((static_cast<int64_t>(sub.sweep_pitch1 - sub.sweep_pitch0) * ph) /
                                 (half - 1));
        sky_ps = zref::fx_sin(zref::angle16{static_cast<uint16_t>(th)}).raw;
        sky_pc = zref::fx_cos(zref::angle16{static_cast<uint16_t>(th)}).raw;
        sky_bias = 0;
      }
      // A planet subject paints its own sky from the intensity plane, so the
      // RGB dome must not be drawn at all. Emitting it anyway left the
      // under-plane on screen as real geometry with a depth, and the hook
      // correctly skipped it -- a flat salmon band under the island that had
      // nothing to do with the planet's ramp.
      if (!sub.space && sub.planet == 0) {
        auto sk = zhao_abi::zhao_sample_draw_sky();
        sk.payload.sky_set = 2;
        sk.payload.rot_proj[0] = sky_rot_for_cam(sub.cam_k, sky_ps, sky_pc, sky_bias, -1);
        if (sub.orbit) {
          const uint16_t theta = static_cast<uint16_t>((static_cast<uint64_t>(f) * 65536u) /
                                                       (sub.frames > 0 ? sub.frames : 1));
          sk.payload.rot_proj[0] = mat4_mul(sk.payload.rot_proj[0], rot_world_yaw(theta));
        }
        if (sub.cam_yaw != 0) {
          sk.payload.rot_proj[0] = mat4_mul(
              sk.payload.rot_proj[0], rot_world_yaw(static_cast<uint16_t>(sub.cam_yaw & 0xFFFF)));
        }
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
        dpop.payload.flags = sub.pop_flags;
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

    if (sub.creature == 2 && f >= gib_spawn_frame && f < gib_spawn_frame + 12 &&
        cr_ctx.gibs_in_view >= 10) {
      ++gib_visible_frames;
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

  if (species == Species::kWatchdog && sub.creature == 2) {
    const int32_t required_growth = fxm(500);
    if (gib_spawn_count < 12 || gib_visible_frames < 8 ||
        gib_max_span < gib_initial_span + required_growth) {
      std::fprintf(stderr,
                   "%s: detached-piece invariant failed: spawned=%u visible_frames=%u "
                   "span=%d->%d raw\n",
                   sub.name, gib_spawn_count, gib_visible_frames, gib_initial_span, gib_max_span);
      return 5;
    }
    std::printf(
        "%s: detached-piece invariant: %u spawned, %u early frames with >=10 in "
        "view, span %d -> %d raw\n",
        sub.name, gib_spawn_count, gib_visible_frames, gib_initial_span, gib_max_span);
  }

  // ---- the palette law: a shipped GIF must be palette-exact ----
  // ...but only where a GIF is what ships. See SceneSubject::full_colour.
  if (sub.full_colour && pal.count() > 256) {
    std::printf("%s: %zu unique colours — full-colour lane, palette gate not applied\n",
                sub.name, pal.count());
  } else if (pal.count() > 256) {
    std::fprintf(stderr,
                 "%s: %zu unique colours (> 256) — the frame set cannot be encoded "
                 "palette-exactly. Re-author the scene; NEVER fall back to palettegen.\n",
                 sub.name, pal.count());
    return 3;
  }

  if (g_write && pal.count() <= 256) {
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
  char line[2048];
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
  if (sub.creature == 2) {
    std::snprintf(line, sizeof(line),
                  "detached_pieces=%u spawn_frame=%u visible_early_frames=%u "
                  "initial_span_raw=%d max_span_raw=%d\n",
                  gib_spawn_count, gib_spawn_frame, gib_visible_frames, gib_initial_span,
                  gib_max_span);
    meta += line;
  }
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
  s.note =
      "travelling radial wave, n=2 whole cycles per loop: frame 63 steps straight back to frame 0";
  s.expect_seq_crc = 0x31159F59u;  // re-pinned 2026-08-16: kBandRows 8->16
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
  s.expect_seq_crc = 0x12427B5Fu;  // re-pinned 2026-08-16: kBandRows fix (dcb32ff); shipped gif is
                                   // pre-fix (0x4F97AD9B)
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
  s.expect_seq_crc = 0x3C186D25u;  // re-pinned 2026-08-16: kBandRows fix (dcb32ff); shipped gif is
                                   // pre-fix (0x86069EA1)
  return s;
}

// 3b. terrain-orbit — THE deep-keel payoff: one exact 360 deg orbit of the
// textured island at a low sunward pitch. The camera circles OUTSIDE the
// envelope (340 m vs the 160 m half-width) so no cell sits behind the eye;
// the per-primitive near-plane rejection would keep the island on screen
// even if it did (terrain.cpp, sky 1.2 precedent). Reads: textured top
// (Mosaic dither grass/rock/sand), strata banded walls, the 75 m modelled
// keel, dusk sky below the rim, and the world-fixed sun sweeping round as
// the sky rotates with the yaw.
SceneSubject subject_orbit() {
  SceneSubject s;
  s.name = "terrain-orbit";
  s.frames = 64;  // 64 x 1024 angle16 = one exact turn, loop-seamless
  s.step = 8;
  s.island = true;
  s.orbit = true;
  // low pitch (10 deg down) from a raised eye: the keel dome and the rim
  // walls carry the frame; the under-sky shows below the silhouette
  s.cam_ps = zref::fx_sin(zref::angle16{0x071C}).raw;  // 10.0 deg (1820 turns)
  s.cam_pc = zref::fx_cos(zref::angle16{0x071C}).raw;
  s.cam_k = 96000;
  s.cam_eye = 34;
  s.cam_dist = 340;
  s.cam_bias = 5900;  // +0.09 NDC, keeps the whole island in frame
  s.note =
      "one exact 360-degree orbit per loop: textured top (per-texel Mosaic dither of "
      "grass/rock/sand), strata-banded rim walls, 75 m modelled keel (3.7 default: "
      "R/2 over the 50 m donor floor), dusk sky below the rim, sun sweeping with "
      "the world-fixed sky";
  s.expect_seq_crc = 0xAB956041u;  // pinned 2026-08-17 (first textured render)
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
  // camera: far shot — eye 120 m, dist 320 m, 26° pitch, sunlit side
  s.cam_k = 112000;
  s.cam_eye = 120;
  s.cam_dist = 320;
  s.cam_bias = 5243;  // +0.08 NDC
  // the dig: centre (28, 52) m — 59 m from the island heart. Under the 3.7
  // keel default the local thickness there is ~65 m (q = (59/151)^2 = 0.153
  // -> 75 x (0.4 + 0.6 x 0.847)); radius 36 m, final depth 84 m -> a hole
  // ~15 m in radius punched clean THROUGH the deep keel. Ramp: frames 8..43
  // in 36 equal bake steps (incremental from->to per frame — the
  // applyDMapDelta cadence).
  const int32_t cx = fxm(28000), cz = fxm(52000), rad = fxm(36000);
  for (uint32_t f = 8; f <= 43; ++f) {
    const int32_t from = static_cast<int32_t>((static_cast<int64_t>(f - 8) * 84000) / 36);
    const int32_t to = static_cast<int32_t>((static_cast<int64_t>(f - 7) * 84000) / 36);
    s.bakes.push_back(BakeStep{f, cx, cz, rad, from, to});
  }
  // debris + screen shake at the first breach frame (frame 32 in the
  // printed deterministic breach schedule; re-pin if the ramp is re-authored)
  const uint32_t first_breach = 32;
  s.debris_spawn_frame = first_breach;
  s.debris_gravity = fxm(380);
  s.debris_y0 = fxm(11000);       // launch near the failing surface (~11 m)
  s.debris_floor = -fxm(120000);  // fall THROUGH the deep-keel breach, out of the world
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
      "320 m textured dual-heightfield island (2.0 m pitch, 3.7 deep-keel default: "
      "75 m heart / 30 m rim); 36-step bake ramp digs an 84 m pit THROUGH the keel; "
      "cells breach corner-coupled; strata rim walls run to the MODELLED bottom; "
      "sky visible through the island; debris falls through the world";
  s.expect_seq_crc = 0x54B0C505u;  // RE-PINNED 2026-08-17, loudly: the deep
  // keel + texturing changed every pixel. Lineage: pre-kBandRows 0x47D4D163
  // (the shipped gif), post-fix flat 0x839E117F, now the deep-keel textured
  // island with the 84 m dig
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
  s.expect_seq_crc = 0x99A3E1A0u;  // re-pinned 2026-08-16: kBandRows fix (dcb32ff)
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
  s.expect_seq_crc = 0xDF05E21Eu;  // pinned 2026-08-16 (80 px legibility scale;
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
      "S00 at 40 radii; burst and three lens ghosts over a graded, connected "
      "motion smear rebuilt with subtract-8 decay and asymmetric diffusion; "
      "the flare dims over the outer 16 px instead of cutting";
  s.expect_seq_crc = 0x347B72F6u;  // re-pinned 2026-08-18: v1.3 trail + the resolve green-amplitude fix
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
  s.expect_seq_crc = 0x69F44CA3u;  // pinned 2026-08-16 (28 px legibility scale;
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
  s.expect_seq_crc = 0xE1CB9DA8u;  // re-pinned 2026-08-18: v1.3 trail + the resolve green-amplitude fix
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
  s.expect_seq_crc = 0x227942AEu;  // re-pinned 2026-08-18: v1.3 trail + the resolve green-amplitude fix
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
  s.expect_seq_crc = 0xF76001F9u;  // re-pinned 2026-08-18: v1.3 trail + the resolve green-amplitude fix
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
  s.expect_seq_crc = 0xEBCA7820u;  // re-pinned 2026-08-18: v1.3 trail + the resolve green-amplitude fix
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
  s.expect_seq_crc = 0xCC14D30Au;  // re-pinned 2026-08-18: v1.3 trail + the resolve green-amplitude fix
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
  s.expect_seq_crc = 0x91F8EB1Au;  // re-pinned 2026-08-18: v1.3 trail + the resolve green-amplitude fix
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
  s.sky_variant = 1;     // flat upper band (still C0): additive headroom
  s.island = true;       // the dual-heightfield island, framed from afar
  s.island_flat = true;  // palette law: the flare chain owns the colour budget
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
  s.expect_seq_crc = 0x47C98B90u;  // re-pinned 2026-08-17: the deep keel
  // changed the island silhouette the sun crosses behind (was 0x4382E5C8
  // after the kBandRows fix; flat island by the palette law - the flare
  // chain owns that subject's colour budget)
  // — re-shot; trails do not apply at the glint rung (see cel_hook)
  return s;
}

// 16. creature-wave-walk — the creature-lane identity shot: the six-part
// watchdog walks the island while a travelling wave passes UNDER it; two
// column_query taps per tick tilt it through the wave (rotateOnGround);
// then the camera pulls back and the LOD ladder walks it down
// mesh -> micro-mesh -> splat -> glint with 10%/15-tick hysteresis.
// 19/20. atmo-sun-donor / atmo-sun-thick — THE PAIR.
//
// A sun setting through a thick atmosphere, over the dual-heightfield island.
// Published as two subjects that differ in exactly one number, so the argument
// can be checked rather than taken on trust:
//
//   atmo-sun-donor  §3's ramp as ported: P3 = (256, 280, 304), a top that
//                   whitens by deliberate early per-channel saturation.
//   atmo-sun-thick  the same subject with P3 = (300, 150, 40) — red railing,
//                   green mid, blue scattered out of the path.
//
// The island is FLAT-SHADED here (island_flat), and that is not a concession.
// The halo composites additively (§4 star_halo_additive, dst = sat(dst+src)),
// so every halo level meets every terrain shade and the product is what the
// 256-colour sequence law counts. flare-occlusion hit the same wall and
// measured 325 unique colours with the texture lane live. It is also the right
// picture: a dark land under a vast glowing sky is the look being argued for,
// not a compromise forced by the budget.
SceneSubject atmo_common(const char* name, int celestial) {
  SceneSubject s;
  s.name = name;
  s.frames = 64;
  s.step = 8;
  s.celestial = celestial;
  s.sky_variant = 2;     // FULLY flat sky: the corona descends through the
                         // horizon, so the upper-band-only flattening is not
                         // enough (see dusk_sky). Near-neutral on purpose.
  s.island = true;
  s.island_flat = true;  // see the note above — additive halo x terrain shade
  s.cam_k = 112000;
  s.cam_eye = 140;
  s.cam_dist = 300;
  s.cam_bias = 5243;
  // A silhouette material. Thick air does not light the land facing away from
  // the sun; compressing the lambert range is also what keeps the halo sums
  // inside the palette law.
  s.mat_r = 44;
  s.mat_g = 48;
  s.mat_b = 42;
  return s;
}

SceneSubject subject_atmosundonor() {
  SceneSubject s = atmo_common("atmo-sun-donor", 11);
  s.note =
      "S00 sun setting through a thick atmosphere over the island; halo_atmo "
      "corona at 4x the disc radius, no lens chain. The ramp is the ported "
      "one: its top control point over-ranges the clamp on every channel, so "
      "the core whitens as it brightens. Pair this with atmo-sun-thick, which "
      "applies an atmospheric transmission to the same ramp and changes "
      "nothing else about the scene";
  s.expect_seq_crc = 0xD16723F6u;  // pinned 2026-08-19 (first render)
  return s;
}

SceneSubject subject_atmosunthick() {
  SceneSubject s = atmo_common("atmo-sun-thick", 12);
  s.note =
      "the same sun, the same island, the same camera, with one number "
      "changed: a per-channel transmission (1.0, 0.60, 0.25) is applied to "
      "the ramp control points before the ramp is built, so red passes, green "
      "is halved and blue is mostly gone at EVERY intensity. Reddening only "
      "the ramp top was tried first and is invisible: the corona falls off "
      "linearly so the top governs a few pixels, and additive compositing "
      "rails every channel near the core regardless. The corona is built from "
      "the same ramp and follows without a second knob";
  s.expect_seq_crc = 0x08B5A606u;  // pinned 2026-08-19 (first render)
  return s;
}

// 21+. planet-sun-<world> — the planetside sun over the island, one subject per
// world. Every one of them is the SAME code with a different row of kPlanets:
// the sky and the sun come from one ramp, so a world's identity is six colours
// and four numbers rather than a sky asset plus a star asset that have to be
// kept in agreement.
SceneSubject planet_subject(const char* name, int planet, uint32_t crc, const char* note) {
  SceneSubject s;
  s.name = name;
  s.frames = 48;
  s.step = 8;
  s.planet = planet;
  s.island = true;
  s.island_flat = true;  // the sky owns the palette; the land is a silhouette
  s.planet_sun_x = 192;
  s.planet_sun_y0 = 96;   // high
  s.planet_sun_y1 = 176;  // set, below the horizon line
  s.cam_k = 112000;
  s.cam_eye = 140;
  s.cam_dist = 300;
  s.cam_bias = 5243;
  s.mat_r = 30;
  s.mat_g = 32;
  s.mat_b = 30;
  s.note = note;
  s.expect_seq_crc = crc;  // pinned 2026-08-20 (first render)
  return s;
}

SceneSubject subject_planet_violet() {
  return planet_subject("planet-sun-violet", 1, 0x7DFDD03Cu,
      "a thick-atmosphere world: the sun has NO disc at all, only a bloom that "
      "spreads as it sets. Sky and sun are one six-bit intensity plane coloured "
      "through one ramp, so the sun's colour IS the sky's peak entry and the "
      "whole frame costs 64 colours however large the bloom grows");
}
SceneSubject subject_planet_terran() {
  return planet_subject("planet-sun-terran", 2, 0x503D3FB9u,
      "thinner air over a blue world: less mottling, a tighter bloom, and a sun "
      "that stays closer to white because the ramp's peak does");
}
SceneSubject subject_planet_dust() {
  return planet_subject("planet-sun-dust", 3, 0x6FFC26E5u,
      "a dust world where the atmosphere is the colour: the bloom is barely "
      "separable from the sky it sits in, which is the point rather than a "
      "failure to render it");
}
SceneSubject subject_planet_methane() {
  return planet_subject("planet-sun-methane", 4, 0xFB3E3581u,
      "a methane sky. The ramp carries three control points, not two, so the "
      "world's identity lives in the mid body while the peak stays near white");
}
SceneSubject subject_planet_airless() {
  return planet_subject("planet-sun-airless", 5, 0x921A069Fu,
      "NO atmosphere, same machinery: one number gives the splat a hard "
      "saturated core and the sun becomes a disc with a short skirt against an "
      "almost black sky. The control that shows the others are doing something");
}
SceneSubject subject_planet_ember() {
  return planet_subject("planet-sun-ember", 6, 0xE4A93123u,
      "a red dwarf's world: dim and deep, and the sun never gets past orange "
      "because the ramp's peak entry is orange. Nothing tints the sun; there is "
      "nothing to tint");
}

SceneSubject subject_planet_giant() {
  SceneSubject s = planet_subject("planet-sun-giant", 7, 0xF4C0DB5Du,
      "an amber giant close enough that the bloom is larger than the frame. The "
      "sky never goes dark anywhere, and because the sun IS the sky ramp's peak "
      "entry this costs no colours at all. The same picture built from a sprite "
      "over a gradient would not fit the palette law");
  s.planet_sun_y0 = 60;
  s.planet_sun_y1 = 200;
  return s;
}
SceneSubject subject_planet_supergiant() {
  SceneSubject s = planet_subject("planet-sun-supergiant", 8, 0x2514843Bu,
      "a blue supergiant. The ramp's peak is near white, so the core reads as "
      "glare rather than as a coloured object, while the sky it saturates into "
      "stays unmistakably blue");
  s.planet_sun_x = 250;
  s.planet_sun_y0 = 70;
  s.planet_sun_y1 = 190;
  return s;
}
SceneSubject subject_planet_redgiant() {
  SceneSubject s = planet_subject("planet-sun-redgiant", 9, 0xFDC57719u,
      "a swollen red giant over a dim world, the largest sun in the set at 480 "
      "px. The peak entry is orange, so nothing on this world is ever white -- "
      "not the sun, not its glare, not the ground it lights");
  s.planet_sun_y0 = 40;
  s.planet_sun_y1 = 210;
  return s;
}
SceneSubject subject_planet_binary() {
  SceneSubject s = planet_subject("planet-sun-binary", 11, 0xA9D2C999u,
      "TWO suns. Both splats add into the same plane and the clamp resolves the "
      "overlap, so where the blooms meet the sum simply rails in the sky's own "
      "peak colour exactly as one bloom does against itself. A second star adds "
      "zero palette entries");
  s.planet_sun_x = 104;
  s.planet_sun_y0 = 78;
  s.planet_sun_y1 = 158;
  s.planet_sun2_mag = 96;
  s.planet_sun2_x = 292;
  s.planet_sun2_y = 104;
  return s;
}

// ---- S1 spike (creature 02): the chained pre-resolve proof --------------
// One stage, three subjects: the planet bloom alone, the creature alone, and
// all three hooks chained (sky bloom + celestial star/corona + creature) in a
// single clip -- the composition the one-slot renderer could never draw.
// mode: 0 = bloom only, 1 = creature only, 2 = chained.
SceneSubject s1_chain_subject(int mode) {
  SceneSubject s;
  s.name = mode == 0 ? "u02-s1-bloom" : mode == 1 ? "u02-s1-creature" : "u02-s1-chained";
  s.frames = 48;
  s.step = 1;
  s.full_colour = true;
  s.bump_ext = 6;
  s.cam_k = 320000;  // the creature-wave-walk near camera: watchdog ~70 px
  s.cam_eye = 12;
  s.cam_dist = 8;
  s.cam_bias = 0;
  if (mode != 1) {
    s.planet = 1;  // violet-thick: no disc, pure bloom (the skybox-bloom read)
    s.planet_sun_x = 192;
    s.planet_sun_y0 = 60;
    s.planet_sun_y1 = 60;  // parked high: the spike judges compositing, not a sunset
  }
  if (mode == 2) s.celestial = 5;  // S01 blue giant: disc + corona, depth-tested
  if (mode != 0) s.creature = 1;   // the watchdog walk, unchanged
  s.note =
      "S1 spike subject: proves the chained pre-resolve dispatcher composites "
      "sky bloom (depth==0 only), celestial corona (depth-tested) and creature "
      "in order in one clip. Throwaway diagnostics; no CRC pin.";
  return s;
}

// ---- creature 02 diagnostic stage ----------------------------------------
// Fixed cameras, no orbit. The creature floats at kHoverHeightMm over the
// same bump crown the zixx stage uses; the camera aims at the whole form
// (body + loop), not the crown.
void u02_common(SceneSubject& s) {
  s.species = Species::kUnnamed02;
  s.step = 1;
  s.full_colour = true;
  s.bump_ext = 6;
  s.cam_k = 240000;
  s.cam_ps = 16962;  // ~15 deg down, the showcase pitch
  s.cam_pc = 63313;
  s.cam_eye = 12;
  s.cam_dist = 8;
  s.cam_bias = 0;
  s.mat_r = 104;  // the dry-ground stage material (the zixx lesson: green
  s.mat_g = 78;   // creature on green ground vanished; grey on brown reads)
  s.mat_b = 58;
  // Pass 3 (gotcha §12): EVERY u02 subject — the s4 diagnostics included —
  // defaults to the SHIPPING presentation (a sun over kU02SunRig), so no
  // future by-eye plate can be judged under a rig the creature does not
  // ship with. Clips override with their own named suns; only
  // unnamed02-inspect clears this and raises the four-light rig.
  s.sun = &kU02SunCalm;
}

SceneSubject subject_u02_s4(int view) {
  SceneSubject s;
  s.name = view == 0   ? "u02-s4-side"
           : view == 1 ? "u02-s4-front"
           : view == 2 ? "u02-s4-tq"
           : view == 3 ? "u02-s4-wire"
           : view == 4 ? "u02-s4-ids"
           : view == 5 ? "u02-s4-stage"
                       : "u02-s4-unlit";
  s.creature = 9;  // slot 7: the still form-diagnostic pose
  s.frames = 4;
  s.orbit = false;
  u02_common(s);
  if (view == 1 || view == 4) s.cam_yaw = 0x4000;  // quarter turn: the front view
  if (view == 2 || view == 3 || view == 5 || view == 6) s.cam_yaw = 0x2000;
  if (view == 3) s.creature_shade = 3;  // wireframe diagnostic
  if (view == 4) s.creature_shade = 4;  // triangle-id diagnostic
  if (view == 5) s.creature = 0;  // stage only: isolates non-creature artifacts
  if (view == 6) s.creature_shade = 1;  // unlit fullbright
  s.note = "S4 spike: the grey ball(s) from a fixed diagnostic camera";
  return s;
}

// ---- creature 02 showcase subjects: one per clip -------------------------
SceneSubject subject_u02_clip(int slot, const char* name, uint32_t keys, bool orbit,
                              const ZixxSunSpec* sun) {
  SceneSubject s;
  s.name = name;
  s.creature = slot + 2;
  s.frames = keys * 2;  // one full loop at presentation rate
  s.orbit = orbit;
  u02_common(s);
  if (!orbit) s.cam_yaw = 0x2000;  // three-quarter: the face and the loop both read
  s.u02_glow = true;
  // Pass 2 (Direction 2 §3): the ten-emitter set is AXED. The channel —
  // the conduit at work — carries the LIGHTNING mana (the Description
  // sheet says discharging bolts through the antenna IS this creature);
  // the owner picks the rest of the stack from the mana menu subjects.
  s.u02_mana = slot == 2 ? 4 : 0;
  // Pass 3 (Direction 3 §1): the four-light-everywhere look was a
  // REGRESSION — "It should look just like Zixx' lighting with the
  // directional light from the sun." Every clip ships under its own named
  // sun over kU02SunRig; only unnamed02-inspect raises the moving rig.
  s.sun = sun;
  // the skybox bloom serves the showcase clips where the sky is the backdrop
  // (S1 made a creature + planet bloom lawful in one clip)
  if (slot == 2) {  // fixed-camera subjects only: the bloom is painted in
    // SCREEN space and must not sit frozen while an orbit spins the world
    s.planet = 1;  // violet-thick: pure formless bloom, the mana mood
    // the bloom sits OFF to the side: additive effects vanish over its
    // near-white core (the S3 ceiling lesson at scene scale), so the loop
    // window keeps dark violet sky behind it for the bolt to blaze against
    s.planet_sun_x = 58;
    s.planet_sun_y0 = 96;
    s.planet_sun_y1 = 132;
  }
  return s;
}

// ---- S5 spike (creature 02): the shared-ramp centre glow -----------------
SceneSubject subject_u02_s5() {
  SceneSubject s;
  s.name = "u02-s5-glow";
  s.creature = 2;
  s.frames = 4;
  s.orbit = false;
  u02_common(s);
  s.u02_glow = true;
  s.u02_glow_trio = true;
  s.note =
      "S5 spike: three conduit centre glows (one real body + two phantom "
      "centres) through ONE per-frame ramp build; occlusion by the body and "
      "by near terrain judged by eye";
  return s;
}

SceneSubject subject_creaturewalk() {
  SceneSubject s;
  s.name = "creature-wave-walk";
  s.frames = 96;
  // one sim tick per frame: a key is held 2 ticks (creature 2.1), so the
  // 16-key stride is a 32-frame cycle and the 0.022 m/frame root motion
  // lays down ~0.70 m per cycle -- a planted walk, not a 4-frame scramble.
  s.step = 1;
  s.creature = 1;
  // The creature light rig became per-channel on 2026-08-26 and this
  // subject's shade count went 229 -> 348. It is a CREATURE subject, and
  // the 256-colour rule is a GIF-export constraint that must not shape a
  // creature -- the same law that deleted Zixxtrixx's orange. Full-colour
  // lane. NOTE: zhaozhou-site's shipped GIF for this subject is now stale
  // and needs regenerating through the full-colour path.
  s.full_colour = true;
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
      "watchdog quadruped (6 bones, 6 rigid ring parts: long torso, pale head, "
      "4 separate legs) walks along its authored +X axis at 0.022 m/frame; the "
      "16-key stride is held on the sim clock, so one full gait cycle spans 32 "
      "frames and lays down 0.70 m, about one body length per cycle; a "
      "0.5 m-amplitude wave crosses under it and two column_query taps per tick "
      "tilt it through the crest; frames 48+ pull the camera back 8 m -> 300 m "
      "and the LOD ladder walks it down mesh -> micro-mesh -> splat -> glint "
      "(screen-space error, 10% hysteresis, 15-tick hold)";
  s.expect_seq_crc = 0x1C1A15BAu;  // NOTE 2026-09-04 (creature 02, S4): the
  // build_ring_part bottom-cap band bug (RingPart::cap_base_fix) puts a
  // stray wedge at this watchdog's rear hip. The fix exists but is OPT-IN;
  // the legacy mesh (and this pin) stay frozen because Zixxtrixx's approved
  // bank shares the buggy layout. Evidence pair in the S4 run.
  // RE-PINNED 2026-08-31: the creature lane
  // moved and the constant did not follow. TWENTY commits touched the creature
  // reference between the Gouraud pin and today -- among them "Correct creature
  // normal orientation" (5aff7ab), "Correct Zixxtrixx whole-body spring"
  // (3ba4131), "Add rigid-safe deformation sidecar" (40c5136) and "Synchronize
  // Zixxtrixx idle and inspection light" (a5a175f). Any one of those moves
  // creature pixels; all of them together certainly do. CI has been red on this
  // since, which is the cost of re-pinning late rather than in the commit that
  // moved it: RE-PIN IN THE COMMIT THAT CAUSES THE DRIFT.
  //
  // Not taken on trust. Two independent process runs both produced
  // 0x1C1A15BA, so it is deterministic and not a nondeterminism report. And a
  // 96-frame contact sheet was LOOKED AT (tools/capture/rgb_contact_sheet.py):
  // the walk reads cleanly, the camera pull-back at frame 48 walks the LOD
  // ladder mesh -> micro-mesh -> splat -> glint without a pop or a dropped
  // frame, and no stray geometry appears. What was NOT done is a pixel diff
  // against the pre-drift render, so this says "coherent and explained", not
  // "identical except for the intended change".
  // Previous 0xF46B3B4A (2026-08-27, Gouraud: smooth vertex normals +
  // per-vertex lighting), and 0x4B8730D6 before that (per-channel light rig).
  return s;
}

// Zixxtrixx, the first Upheaval creature. Four presentation subjects.
//
// MODELINGGUIDE section 10: the previous orbit performed a whole revolution
// during a very short clip, so the camera was the dominant visible movement
// and the animation hid behind it. Every subject here runs SEVERAL animation
// cycles per revolution, and the beauty orbit takes 12-16 seconds.
//
// All four are full-colour: the 256-colour rule is a GIF-export constraint and
// it already cost this creature an eye colour, a mouth and a throat blend.
namespace {

void zixx_common(SceneSubject& s) {
  s.step = 1;
  s.full_colour = true;
  s.bump_ext = 6;
  // ZOOMED IN 2026-08-26. At 210000 the animal was about a fifth of the
  // frame -- too small to judge, let alone enjoy. A creature showcase should
  // fill its frame; the orbit keeps it centred.
  s.cam_k = 400000;
  // SHALLOWER CAMERA for the showcase. The default 26-degree-down look shows
  // mostly the creature's BACK, so the dorsal band dominated and the S was
  // foreshortened into a half-bend. The concept sheets are a SIDE view -- get
  // closer to that. sin/cos of ~15 degrees.
  s.cam_ps = 16962;
  s.cam_pc = 63313;
  // 11, not 13: the aim point is eye - tan(pitch)*dist, so flattening the
  // pitch RAISES the aim. Left at 13 the shot looked at empty sky above the
  // animal. tan(15) = 0.268, so 11 - 2.14 lands back on the terrain crown.
  s.cam_eye = 11;
  s.cam_dist = 8;
  s.cam_bias = 0;
  // dry ground: the flank green comes off the concept sheet and the default
  // terrain material is a green of almost the same value, which the animal
  // vanished into. Changing the ground is cheaper than repainting a creature
  // whose colours are the point.
  s.mat_r = 104;
  s.mat_g = 78;
  s.mat_b = 50;
  s.sky_variant = 1;
}

}  // namespace

// The IDLE, on a slow beauty orbit. 96 keys is 192 frames; three of them per
// revolution puts the orbit at 576 frames -- 9.6 s at 60 Hz, and longer still
// at the 20 fps the site plays. The camera is never the fastest thing moving.
SceneSubject subject_zixx_idle() {
  SceneSubject s;
  s.name = "zixxtrixx-idle";
  s.sun = &kZixxSunIdle;  // Direction 29
  s.creature = 3;
  s.frames = zixx::kIdleKeys * 2 * 3;
  s.orbit = true;
  zixx_common(s);
  s.note =
      "Zixxtrixx at rest in the canonical S (rebuilt 2026-08-26 to the "
      "sheet's full letter: head carried high, gentle neck rise to an apex "
      "just above the skull, then the dive PAST vertical back under itself, "
      "a short grounded run sunk an authored few mm, and the tail rising "
      "steeply into the blade fan). The 3.2 s breath deepens the dive while "
      "the root rises to match, so the head and arch bob while the belly "
      "stays planted; girth swells through the instance bulk; the tail "
      "sways lazily on its own period. Three breaths per revolution";
  return s;
}

// ---- S3 spike (creature 02): additive motes over pink pigment -----------
// The same Zixxtrixx idle stage twice: a ring of static cyan 6 px motes
// hung at flank height, once opaque (the confetti read) and once additive
// (the mana read). Unpinned diagnostics; the acceptance question is answered
// by LOOKING at the pair at native 384x240.
SceneSubject s3_mote_subject(bool additive) {
  SceneSubject s;
  s.name = additive ? "u02-s3-motes-add" : "u02-s3-motes";
  s.sun = &kZixxSunIdle;
  s.creature = 3;  // the idle clip; unpinned copy of the showcase stage
  s.frames = 48;
  s.orbit = false;
  zixx_common(s);
  s.debris_spawn_frame = 0;
  s.debris_gravity = 0;
  // the zixx stage sits ON the bump-patch crown at ~8.8 m -- a mote authored
  // near y=0 is INSIDE the mountain and depth-fails every pixel (found the
  // hard way: the first pair rendered identical CRCs because no mote ever won
  // a fragment). Park a 5x2 grid straight over the mid-body flank, just in
  // front of the animal's plane, so the pair answers the actual question:
  // cyan over PINK PIGMENT, opaque vs additive, at native 384x240.
  s.debris_y0 = fxm(8900);  // rows offset via vy read at frame 12
  s.debris_floor = fxm(8000);
  // Placed with a two-fan diagnostic (see run log): the idle stage maps the
  // body arch top to roughly world x -700..+100 at y ~8.2 m from this fixed
  // camera. One row rides the dorsal pink stripe, one the green flank, one
  // hangs in the sky as the control. Heights are encoded via vy so frame 12
  // is the judging frame (y = y0 + vy*12).
  for (int i = 0; i < 13; ++i) {
    Debris d;
    int32_t x_mm, y_mm;
    if (i < 5) {          // stripe row
      x_mm = -750 + i * 200;
      y_mm = 8150;
    } else if (i < 10) {  // flank row
      x_mm = -650 + (i - 5) * 200;
      y_mm = 7950;
    } else {              // sky control row
      x_mm = -500 + (i - 10) * 400;
      y_mm = 9300;
    }
    d.x0 = fxm(x_mm);
    d.z0 = fxm(-900);  // the S curves in x-z; clear the bow toward camera
    d.vx = 0;
    d.vy = fxm(y_mm - 8900) / 12;
    d.vz = 0;
    d.size = 96;  // U0.4.4: 6 px
    d.r = 40;
    d.g = 230;
    d.b = 255;
    s.debris.push_back(d);
  }
  s.pop_flags = additive ? 0x0007 : 0x0003;
  s.note = "S3 spike: cyan motes over the pink stripe -- opaque vs additive pair";
  return s;
}

// The CATERPILLAR WALK, fixed camera. MODELINGGUIDE section 6 is explicit that
// a fixed side or three-quarter render must show the gait clearly BEFORE any
// orbit is applied, so this subject has no orbit at all.
SceneSubject subject_zixx_walk() {
  SceneSubject s;
  s.name = "zixxtrixx-walk";
  s.sun = &kZixxSunWalk;  // Direction 29
  s.creature = 4;
  s.frames = zixx::kWalkKeys * 2 * 2;  // two full gait cycles
  s.orbit = false;
  zixx_common(s);
  // Framing on the flat ground: the camera pitch is fixed, so past ~330000
  // the horizon leaves the top of the frame entirely and the gait floats in
  // a brown void. 310000 keeps the skyline in shot while the animal fills
  // over half the frame width -- and the CLOSE shot is what lets the new
  // breath-bob and front wave read at all (a 2026-08-27 pull-back to 260000
  // shrank a 50 mm head bob to 4 px). The traverse fits because the shot
  // now runs TWO gait cycles, not three: kWalkSpeed grew 11 -> 13.
  s.cam_k = 310000;
  // FLAT GROUND for the gait shot (2026-08-26). At bump_ext 6 the mound's
  // curvature under the 3 m body swallowed the belly along most of the
  // traverse -- the "massive sink" Fabian rejected was the TERRAIN, not the
  // clip: the reel snaps the root to one column and the ground rose under
  // the rest of the animal. The clip's own authored sink is a few mm.
  s.bump_ext = 18;
  s.note =
      "Caterpillar locomotion, fixed three-quarter camera so the gait is "
      "legible before any orbit is applied. The S holds throughout: head "
      "glides high, and ONLY the grounded run carries the travelling hump -- "
      "authored as a height field turned into joint pitches by second "
      "difference, so it arches up off the ground and can never reach below "
      "it (authored sink: a few mm). Flat ground so one column's snap speaks "
      "for the whole body. Three complete cycles";
  return s;
}

// The TRIPLE SALTO MORTALE, fixed three-quarter camera.
SceneSubject subject_zixx_attack() {
  SceneSubject s;
  s.name = "zixxtrixx-attack";
  s.sun = &kZixxSunAttack;  // Direction 29
  s.creature = 5;
  s.frames = zixx::kAttackKeys * 2;
  s.orbit = false;
  zixx_common(s);
  // 235000: at 300000 the dive left the frame entirely for nine frames --
  // the animal is 3.5 m of spear and it travels, so this shot needs room the
  // others do not. Judged from a contact sheet, not from the rest pose.
  // FLATTER GROUND for this shot only. The default mound has a crest, and a
  // 3.5 m spear diving to ground level went BEHIND it -- nine frames where all
  // you could see was the blue head poking over a hill. The animal is the
  // subject; the terrain is not allowed to occlude the impact.
  s.bump_ext = 18;
  s.cam_k = 235000;

  // THE TRACKING CAMERA (2026-08-27). Fabian: "keep the camera on it ... It
  // is very important the camera follow it, you did not do that." The view
  // follows the authored flight path; the creature never leaves frame.
  s.cam_track = true;
  // 1000, was 850: at a 12 m apex, following only 85% of the lift parks the
  // creature 1.8 m above frame centre -- add the spear-midpoint aim shift
  // and it clipped the top of the frame. Full lift tracking keeps the
  // javelin centred the whole flight; the climb still reads because the
  // GROUND rushes away (a world translation does not move the sky).
  s.cam_track_num = 1000;

  // SCREEN SHAKE ON IMPACT -- showcase only, at Fabian's request, and
  // deliberately NOT a general feature: it lives on this presentation
  // subject, not in the creature and not in the sim. Contact is clip key
  // kAtkImpactKey, reel frame 2*kAtkImpactKey (keys are held two ticks; the
  // reel runs one tick per frame). A hard first jolt then a decaying
  // alternation, so it reads as one heavy blow rather than a wobble.
  //
  // STRONGER AND LONGER, 2026-08-27 pass 3 (Fabian, third ask: "we have no
  // screenshake at point of impact I can see, so make that strong"). The
  // previous 9-frame burst decayed inside a third of a second, so a video
  // resampling 60 -> 20 fps kept at most two visibly-shaken frames. Now the
  // first jolt is ~0.25 NDC (~30 px of a 240 px frame) and the alternation
  // runs 16 frames, so even a 20 fps resample carries five-plus frames of
  // it. VERIFIED on rendered frames (diffed around contact), not from these
  // constants.
  s.shake_frame = static_cast<uint32_t>(zixx::kAtkImpactKey) * 2;
  {
    static const int32_t kJolt[] = {-170000, 132000, -101000, 78000, -60000,
                                    46000,   -34000, 25000,   -18000, 13000,
                                    -9000,   6000,   -4000,   2500,   -1400,
                                    0};
    for (int32_t v : kJolt) s.shake.push_back(v);
  }
  s.note =
      "The attack, third pass 2026-08-27. Zixxtrixx rolls up into a wheel, "
      "somersaults three times while climbing to a ~12.5 m apex, unrolls to "
      "a rigid spear at the top, hangs a beat, and PLUNGES ~11 m in ONE "
      "STRAIGHT SHOT along the spear's own 30-deg-from-vertical line (root "
      "drops 9.6 m while driving 5.6 m forward -- the flight path IS the "
      "javelin axis). The tip bites 420 mm into the ground -- the authorised "
      "clipping exception -- and STICKS, dead straight, for 150 keys = 300 "
      "frames = 5.0 s, then pulls out and the loop closes. The camera "
      "TRACKS the flight AND aims at the spear midpoint through the dive "
      "and stick so the ground hit is framed; strong 16-frame screen shake "
      "at contact (reel frame 112)";
  return s;
}

// DIAGNOSTIC: exact fixed-side comparison required by owner direction #9.
// The short subject ends at the clean wheel key, keeping all anticipation
// frames large enough to judge at native resolution.
// Through the whole arming, hold and release to the coil pose -- derived so
// a retimed spring keeps its diagnostic coverage (it was a literal 30, which
// silently cropped the Direction-23 arming to its first quarter).
constexpr uint32_t kZixxSpringDiagnosticKeys =
    static_cast<uint32_t>(zixx::kSaltoCoilPoseKey) + 1;
constexpr uint32_t kZixxSpringDiagnosticSamples =
    2 * (kZixxSpringDiagnosticKeys - 1) + 1;
SceneSubject subject_zixx_spring_side() {
  SceneSubject s;
  s.name = "zixxtrixx-spring-side";
  s.creature = 5;  // slot 3, the theatrical attack
  s.frames = kZixxSpringDiagnosticSamples;  // key 0, each midpoint, key 29
  s.canonical_creature_timeline = true;
  s.orbit = false;
  zixx_common(s);
  s.cam_k = 150000;  // keep exact rest, broad squash and release wheel in frame
  s.note = "DIAGNOSTIC: every frame of shared whole-body spring anticipation, "
           "fixed true-side camera; exact rest through compression hold and "
           "release";
  return s;
}

// DIAGNOSTIC: the complete shared spring from exact rest through release,
// seen from high three-quarter so its broad lateral concertina cannot hide in
// the beauty camera's side projection. Every theatrical salto and immediate
// jump calls the same authored pose function; this shot is the fast art loop.
SceneSubject subject_zixx_spring_top() {
  SceneSubject s;
  s.name = "zixxtrixx-spring-top";
  s.creature = 5;  // slot 3, the theatrical attack
  s.frames = kZixxSpringDiagnosticSamples;  // key 0, each midpoint, key 29
  s.canonical_creature_timeline = true;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 8192;   // three-quarter around the animal
  s.cam_ps = 53684;   // 55 degrees down
  s.cam_pc = 37590;
  s.cam_eye = 20;     // eye - tan(55 deg)*8 m aims at the 8 m stage crown
  s.cam_k = 280000;  // keep broad planform and release wheel in frame
  s.note = "DIAGNOSTIC: every frame of the shared whole-body spring, fixed "
           "high three-quarter camera; exact rest through compression hold "
           "and release";
  return s;
}

// Same fixed-side presentation cadence and framing, but explicitly draw the
// compiled micro mesh. This diagnostic is not in the shipped/site library.
SceneSubject subject_zixx_spring_micro() {
  SceneSubject s = subject_zixx_spring_side();
  s.name = "zixxtrixx-spring-micro";
  s.creature_force_micro = true;
  s.note = "DIAGNOSTIC: every frame of the shared whole-body spring on the "
           "compiled micro mesh, fixed true-side camera";
  return s;
}

// DIAGNOSTIC (deliberately NOT in kLibrary, so it never ships to the site):
// a near-LEVEL orbit for the Front.png acceptance test -- "a straight-on
// view must look like the frontal sketch". The showcase camera looks down
// 15 deg and therefore photographs the CROWN of even a lifted head; the
// sketch's frontal is level with the face, so the comparison must be too.
SceneSubject subject_zixx_front() {
  SceneSubject s;
  s.name = "zixxtrixx-front";
  s.creature = 3;
  s.frames = zixx::kIdleKeys * 2 * 3;
  s.orbit = true;
  zixx_common(s);
  s.cam_ps = 4571;   // sin ~4 deg
  s.cam_pc = 65377;  // cos ~4 deg
  // THE STAGE IS THE MOUND CROWN AT ~8 m (the bump patch's hill -- see the
  // bulk-pop subject's "aim y = 8.1 m"), so a level camera must ride just
  // above it: eye 10, aim = 10 - tan(4 deg)*8 ~ 9.4 m, the lifted head.
  // eye 3 put the camera INSIDE the hill and photographed sky.
  s.cam_eye = 10;
  s.note =
      "DIAGNOSTIC: near-level orbit at head height so a head-on frame can "
      "be compared against Concept/Front.png (the 15-deg showcase pitch "
      "shows the crown, not the face)";
  return s;
}

// DIAGNOSTIC ACCEPTANCE-GATE CAMERAS (2026-08-27 head-only run; none in
// kLibrary, none in --check). Headache.md: one curated frontal portrait let a
// side droop, a snout, a self-intersection and a 101-degree mouth all ship.
// The gate is now fixed SIDE + fixed FRONT + fixed THREE-QUARTER + the slow
// orbit + max idle/walk bend + the salto anticipation + the overlap probe.
SceneSubject subject_zixx_side() {
  SceneSubject s;
  s.name = "zixxtrixx-side";
  s.creature = 3;
  s.frames = zixx::kIdleKeys * 2;  // one full idle loop: max idle bend is IN here
  s.orbit = false;
  zixx_common(s);
  s.note =
      "DIAGNOSTIC: fixed true-side camera over one full idle loop -- the view "
      "that caught the droop the frontal portrait missed. Also the max-idle-"
      "bend evidence: every key of the loop passes this camera";
  return s;
}

SceneSubject subject_zixx_side_game() {
  SceneSubject s = subject_zixx_side();
  s.name = "zixxtrixx-side-game";
  s.cam_k = 160000;
  s.note = "DIAGNOSTIC: full idle loop, fixed true-side gameplay-distance pupil check";
  return s;
}

SceneSubject subject_zixx_tq() {
  SceneSubject s;
  s.name = "zixxtrixx-tq";
  s.creature = 3;
  s.frames = zixx::kIdleKeys * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 8192;  // 45 deg: the fixed three-quarter
  s.note = "DIAGNOSTIC: fixed three-quarter camera, one full idle loop";
  return s;
}

SceneSubject subject_zixx_frontfix() {
  SceneSubject s;
  s.name = "zixxtrixx-frontfix";
  s.creature = 3;
  s.frames = zixx::kIdleKeys * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 16384;  // quarter turn: nose-on
  // near-level, like zixxtrixx-front: the face must be judged at face height
  s.cam_ps = 4571;
  s.cam_pc = 65377;
  s.cam_eye = 10;
  s.note = "DIAGNOSTIC: fixed head-on camera at face height, one full idle loop";
  return s;
}

SceneSubject subject_zixx_frontfix_game() {
  SceneSubject s = subject_zixx_frontfix();
  s.name = "zixxtrixx-frontfix-game";
  s.cam_k = 160000;
  s.note = "DIAGNOSTIC: full idle loop, fixed head-on gameplay-distance two-eye check";
  return s;
}

// One STILL frame, fixed side camera: the orientation-sweep unit. Rebuilt
// once per candidate kHeadAttitude (-DZIXX_ATTITUDE=...), rendered once,
// and the nine stills go on ONE contact sheet to be judged by eye.
SceneSubject subject_zixx_still() {
  SceneSubject s;
  s.name = "zixxtrixx-still";
  s.creature = 3;
  s.frames = 1;
  s.orbit = false;
  zixx_common(s);
  s.note = "DIAGNOSTIC: single idle key 0 frame, fixed side camera, for the "
           "head-attitude orientation sweep";
  return s;
}

// V11 lighting options: one deliberately short fixed-camera idle excerpt. It
// is not a catalogue subject and changes no animation data; it exists only so
// four identical 24-frame renders can be assembled into ONE bounded owner-
// choice motion comparison.
SceneSubject subject_zixx_light_choice() {
  SceneSubject s = subject_zixx_still();
  s.name = "zixxtrixx-light-choice";
  s.frames = 24;
  s.note = "DIAGNOSTIC: v11 lighting choice, first 24 idle ticks, fixed side camera";
  return s;
}

SceneSubject subject_zixx_moving_light() {
  SceneSubject s = subject_zixx_still();
  s.name = "zixxtrixx-moving-light";
  s.frames = 600;
  s.creature_hold = true;
  s.creature_moving_light = true;
  s.cam_yaw = 8192;  // fixed three-quarter view; the SOURCE moves, not the eye
  s.cam_k = 280000;
  s.cam_dist = 9;
  s.note = "FINAL INSPECTION: held signature-S under dim sunlight; four visible "
           "world-space local sources -- the warm inspection lamp plus blue, red "
           "and green -- move on their own authored paths, drive the real "
           "posed-vertex light with the normal per-channel additive emission "
           "(Direction 30), and mix where their pools intersect";
  return s;
}

// V12 overhead-lighting inspection: the same authored idle key is held exactly
// while the camera makes one continuous world-yaw turn. Ten seconds at 60 Hz
// felt slow enough to read each side and the dorsal bands without becoming a
// static turntable. The light rig remains fixed in compose_creatures world
// space; only the view matrix changes, explicitly exposing any accidental
// camera-relative-light defect.
SceneSubject subject_zixx_overhead_orbit() {
  SceneSubject s = subject_zixx_still();
  s.name = "zixxtrixx-overhead-orbit";
  s.frames = 600;
  s.orbit = true;
  s.creature_hold = true;
  s.note = "EXPERIMENTAL: v12 overhead-lighting choice, held signature-S pose, "
           "one slow continuous 10-second world-space inspection orbit";
  return s;
}

// V13's one-and-only post-correction animation. It deliberately inherits the
// identical held key, framing, 600-frame duration and view-only orbit above;
// only its subject name and selected generic world-space rig differ.
SceneSubject subject_zixx_corrected_toplight_1() {
  SceneSubject s = subject_zixx_still();
  s.name = "zixxtrixx-corrected-toplight-1";
  s.frames = 600;
  s.orbit = true;
  s.creature_hold = true;
  s.note = "EXPERIMENTAL: v13 Corrected Toplight 1 after outward-normal repair; "
           "held signature-S, slow 10-second orbit, fixed world-space light";
  return s;
}

// Same pose, light and true-side camera as zixxtrixx-still, pulled back to a
// real gameplay-scale read. Kept as a committed counterpart so smooth toon
// topology comparisons cannot substitute an enlarged close crop for distance.
SceneSubject subject_zixx_still_game() {
  SceneSubject s = subject_zixx_still();
  s.name = "zixxtrixx-still-game";
  s.cam_k = 160000;
  s.note = "DIAGNOSTIC: idle key 0 at fixed gameplay distance; same-pose "
           "normal/faceted/smooth-toon comparison";
  return s;
}

// One STILL frame, head-on at face height: the fast unit of the Front.png
// comparison loop (P2). The full frontfix subject renders a whole idle loop;
// authoring a face by eye needs a seconds-cheap render per knob turn.
SceneSubject subject_zixx_still_front() {
  SceneSubject s;
  s.name = "zixxtrixx-still-front";
  s.creature = 3;
  s.frames = 1;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 16384;  // quarter turn: nose-on
  s.cam_ps = 4571;    // near-level, like zixxtrixx-front
  s.cam_pc = 65377;
  s.cam_eye = 10;
  s.note = "DIAGNOSTIC: single idle key 0 frame, fixed head-on camera at "
           "face height, for the Front.png authoring loop";
  return s;
}

#ifdef ZIXX_SWEEP
// The nine-attitude orientation sweep (see build_sweep in zixxtrixx.h):
// 18 frames = 9 keys held twice, fixed side camera, no orbit. Exists only
// in the -DZIXX_SWEEP diagnostic build.
SceneSubject subject_zixx_sweep() {
  SceneSubject s;
  s.name = "zixxtrixx-sweep";
  s.creature = 11;  // clip slot 9 (after the 2026-08-28 vocabulary)
  s.frames = 18;
  s.orbit = false;
  zixx_common(s);
  s.note = "DIAGNOSTIC: head-attitude sweep, one key each";
  return s;
}
#endif

// DIAGNOSTIC: the falling loop on a FIXED side camera (P2; V2 section 10's
// "fixed-camera falling loop"). Camera rotation plus body rotation makes
// the flail impossible to diagnose on the orbiting site subject.
SceneSubject subject_zixx_fall_side() {
  SceneSubject s;
  s.name = "zixxtrixx-fall-side";
  s.creature = 6;
  s.frames = zixx::kFallKeys * 2;
  s.orbit = false;
  zixx_common(s);
  // Match the accepted site shot's pullback/bias. The diagnostic only helps if
  // every travelling bend stays visible instead of leaving the frame.
  s.cam_k = 140000;
  s.cam_bias = 16000;
  s.note = "DIAGNOSTIC: one full falling loop, fixed side camera, no orbit";
  return s;
}


// The HIT flinch, fixed three-quarter, repeated three times so the site
// loop is watchable (one flinch is 0.6 s).
SceneSubject subject_zixx_hit() {
  SceneSubject s;
  s.name = "zixxtrixx-hit";
  s.sun = &kZixxSunHit;  // Direction 29
  s.creature = 7;  // clip slot 5
  s.frames = zixx::kHitKeys * 2 * 3;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 8192;
  s.cam_k = 300000;
  s.bump_ext = 18;
  s.note =
      "The damage flinch (2026-08-28 vocabulary): the S snaps tighter, the "
      "head whips back-up and aside with the first neck joints carrying a "
      "third of it, one damped overshoot, and the canonical S is back "
      "inside 0.6 s -- both ends sit on the rest pose so hard cuts land "
      "clean. Shown three times per loop";
  return s;
}

// The DEATH, fixed three-quarter. Once through; the last keys are still.
SceneSubject subject_zixx_death() {
  SceneSubject s;
  s.name = "zixxtrixx-death";
  s.sun = &kZixxSunDeath;  // Direction 29
  s.creature = 8;  // clip slot 6
  s.frames = zixx::kDeathKeys * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 8192;
  s.cam_k = 280000;
  s.bump_ext = 18;
  s.note =
      "Death (2026-08-28 vocabulary): two slow shudders, the S drains out "
      "of the body as the carried front sinks, it keels onto its flank "
      "about a ground line under the S's centre, the tail curls once and "
      "releases, and the last second is deliberately almost still -- "
      "stillness after motion is what reads as death";
  return s;
}

// The TAIL-BALANCE stunt, fixed side so the almost-spear reads in profile.
SceneSubject subject_zixx_balance() {
  SceneSubject s;
  s.name = "zixxtrixx-balance";
  s.sun = &kZixxSunBalance;  // Direction 29
  s.creature = 9;  // clip slot 7
  s.frames = 2 * (zixx::kBalKeys - 1) + 1;
  s.canonical_creature_timeline = true;
  s.orbit = false;
  zixx_common(s);
  s.cam_k = 150000;  // the nose reaches ~3 m: this shot needs headroom
  s.bump_ext = 18;
  s.note =
      "The tail-balance idle stunt (2026-08-28): Zixx gathers onto its "
      "planted tail, stretches up toward vertical -- ALMOST the attack's "
      "rigid spear, but effortful, wobbling, fins flared for balance, "
      "never straight -- loses the fight, topples flat forward with an "
      "authored ground bite, and gets back up into the canonical S. The "
      "root is computed from the tail-tip ground constraint, so the stunt "
      "pivots on the tail like a real balance";
  return s;
}

// The LOOK-AROUND, near-level three-quarter so the gaze reads.
SceneSubject subject_zixx_look() {
  SceneSubject s;
  s.name = "zixxtrixx-look";
  s.sun = &kZixxSunLook;  // Direction 29
  s.creature = 10;  // clip slot 8
  s.frames = zixx::kLookKeys * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 12288;  // between three-quarter and head-on
  s.cam_k = 300000;
  s.bump_ext = 18;
  s.note =
      "The look-around idle (2026-08-28): the head-aim rig performed. The "
      "body idles on a quiet half-breath while the head turns left, "
      "glances up, turns right, dips down and comes home -- eased, with "
      "tiny arrival overshoots, the first two neck joints following "
      "softly. The head is bone 0 (the ROOT), so the aim lives on the "
      "dedicated skull bone: the body stays planted while the head turns, "
      "which is the rig capability the sim will later drive";
  return s;
}

SceneSubject subject_zixx_look_game() {
  SceneSubject s = subject_zixx_look();
  s.name = "zixxtrixx-look-game";
  s.cam_k = 160000;
  s.note = "DIAGNOSTIC: full target-led look clip at gameplay distance for pupil motion";
  return s;
}

// Direction #8 acceptance unit: a completely static head while the coordinated
// pupil/stripe system visits both vertical and diagonal extrema, holds, reverses
// and settles. Side and head-on cameras are paired at close and gameplay scale.
SceneSubject subject_zixx_pupil_proof(bool front, bool game) {
  SceneSubject s;
  s.name = front ? (game ? "zixxtrixx-pupil-proof-front-game"
                         : "zixxtrixx-pupil-proof-front")
                 : (game ? "zixxtrixx-pupil-proof-game"
                         : "zixxtrixx-pupil-proof");
  s.creature = 47;  // diagnostic clip slot 45
  s.frames = zixx::kPupilProofKeys * 2;
  s.orbit = false;
  zixx_common(s);
  if (front) {
    s.cam_yaw = 16384;
    s.cam_ps = 4571;
    s.cam_pc = 65377;
    s.cam_eye = 10;
  }
  if (game) s.cam_k = 160000;
  s.note = "DIAGNOSTIC: static-head pupil and elastic stripe boundary-following sweep";
  return s;
}

SceneSubject subject_zixx_pupil_proof_other(bool game) {
  SceneSubject s = subject_zixx_pupil_proof(false, game);
  s.name = game ? "zixxtrixx-pupil-proof-other-game"
                : "zixxtrixx-pupil-proof-other";
  s.cam_yaw = 32768;  // opposite true side: exposes the second eye directly
  s.note = "DIAGNOSTIC: opposite-flank static-head pupil/stripe sweep";
  return s;
}

// P2 DIAGNOSTIC SHADE SUBJECTS: the acceptance-gate views. One idle key,
// fixed side + head-on pairs, in unlit / normal-viz / wireframe; plus the
// full walk loop unlit (W1's first diagnostic: does the edge survive in
// the unlit silhouette?). None in kLibrary, none in --check.
SceneSubject subject_zixx_mode(const char* name, int shade, bool front, int frames) {
  SceneSubject s;
  s.name = name;
  s.creature = 3;
  s.frames = frames;
  s.orbit = false;
  zixx_common(s);
  s.creature_shade = shade;
  if (front) {
    s.cam_yaw = 16384;
    s.cam_ps = 4571;
    s.cam_pc = 65377;
    s.cam_eye = 10;
  }
  s.note = "DIAGNOSTIC: acceptance-gate shade mode";
  return s;
}
SceneSubject subject_zixx_walk_unlit() {
  SceneSubject s;
  s.name = "zixxtrixx-walk-unlit";
  s.creature = 4;
  s.frames = zixx::kWalkKeys * 2 * 2;
  s.orbit = false;
  zixx_common(s);
  s.creature_shade = 1;
  s.cam_k = 310000;
  s.bump_ext = 18;
  s.note =
      "DIAGNOSTIC (W1): the golden walk with the light rig OUT -- if the "
      "perceived edge survives in this unlit silhouette it is animation; "
      "if not it was shading. The default verdict is NO walk change.";
  return s;
}

// DIAGNOSTIC: the F2 BAKED spring-chain fall (slot 19), fixed side camera,
// beside the shipped F1 relax for the owner's eye. Not in the library.
#ifdef ZIXX_F2_PREVIEW
SceneSubject subject_zixx_fall_baked() {
  SceneSubject s;
  s.name = "zixxtrixx-fall-baked";
  s.creature = 21;  // clip slot 19
  s.frames = zixx::kFallKeys * 2;
  s.orbit = false;
  zixx_common(s);
  s.note =
      "DIAGNOSTIC (F2): the offline deterministic spring-chain fall -- "
      "per-joint springs to a weak rest S, neighbour coupling, root "
      "inertia, slow aero forcing, damping; 60 Hz fixed-point, baked, no "
      "runtime physics. An ALTERNATE beside the owner-directed F1 relax "
      "(slot 4): promotion is his call, made by eye.";
  return s;
}
#endif

// WAVE E: the LOD-ladder distance sweep -- the camera pulls back from the
// showcase distance to the horizon over one idle loop, riding the ladder
// mesh -> micro -> splat -> glint. The gate: the pink crown survives, the
// silhouette holds, no rung pops. Diagnostic; not in the library.
SceneSubject subject_zixx_lodsweep() {
  SceneSubject s;
  s.name = "zixxtrixx-lodsweep";
  s.creature = 3;
  s.frames = zixx::kIdleKeys * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_k = 310000;
  s.cam_k_end = 26000;
  s.bump_ext = 18;
  s.note = "DIAGNOSTIC (Wave E): distance sweep over the hardware LOD ladder";
  return s;
}

// ---- run 0326: the vocabulary close-out subjects --------------------------
// The KNOCKDOWN CHAIN: knocked flat, held down a beat, gets back up -- the
// donor's knocked2Floor -> getUp state pair as one watchable shot.
SceneSubject subject_zixx_knockdown() {
  SceneSubject s;
  s.name = "zixxtrixx-knockdown";
  s.sun = &kZixxSunKnockdown;  // Direction 29
  s.creature = 22;  // clip slot 20
  s.frames = zixx::kKnockKeys * 2 + 28 + zixx::kGetUpKeys * 2 + 24;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 8192;
  s.cam_k = 280000;
  s.bump_ext = 18;
  s.clip_cuts = {{static_cast<uint32_t>(zixx::kKnockKeys * 2 + 28), zixx::kSlotGetUp}};
  s.note =
      "The knockdown chain (run 0326 vocabulary): the blow lands on key 0 -- "
      "head snap, the body drains onto its flank with one small rebound and "
      "HOLDS; a beat later getUp cuts in on the byte-identical knocked pose "
      "(compiler-enforced seam): the head wakes first, the roll rights "
      "itself, and the S gathers back with its belly kissing the dirt";
  return s;
}

// The HITFLOOR chain: the landing that ends `falling`, then the get-up.
// V10 structural eye-artifact diagnostic: the named knockdown reproduction
// through the exact same camera and clip, changing only the renderer shade
// mode. This keeps geometry-vs-light-vs-topology comparisons pose-identical.
SceneSubject subject_zixx_knockdown_mode(const char* name, int shade) {
  SceneSubject s = subject_zixx_knockdown();
  s.name = name;
  s.creature_shade = shade;
  s.note = "DIAGNOSTIC: knockdown eye-artifact root isolation";
  return s;
}

SceneSubject subject_zixx_hitfloor() {
  SceneSubject s;
  s.name = "zixxtrixx-hitfloor";
  s.sun = &kZixxSunHitFloor;  // Direction 29
  s.creature = 24;  // clip slot 22
  s.frames = zixx::kHitFloorKeys * 2 + 20 + zixx::kGetUpKeys * 2 + 24;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 8192;
  s.cam_k = 260000;
  s.bump_ext = 18;
  s.clip_cuts = {{static_cast<uint32_t>(zixx::kHitFloorKeys * 2 + 20), zixx::kSlotGetUp}};
  s.note =
      "The landing (run 0326 vocabulary, donor hitFloor): the falling body "
      "flattens as it drops, slams with an authored bite, one small rebound "
      "into the shared knocked pose, then the same getUp";
  return s;
}

// The DIRECTIONAL DAMAGE TOUR: right, back, left, top, one flinch each.
SceneSubject subject_zixx_damage() {
  SceneSubject s;
  s.name = "zixxtrixx-damage";
  s.sun = &kZixxSunDamage;  // Direction 29
  s.creature = 25;  // starts on slot 23 (damageRight)
  s.frames = zixx::kDmgKeys * 2 * 4;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 8192;
  s.cam_k = 300000;
  s.bump_ext = 18;
  s.clip_cuts = {{static_cast<uint32_t>(zixx::kDmgKeys * 2), zixx::kSlotDmgBack},
                 {static_cast<uint32_t>(zixx::kDmgKeys * 4), zixx::kSlotDmgLeft},
                 {static_cast<uint32_t>(zixx::kDmgKeys * 6), zixx::kSlotDmgTop}};
  s.note =
      "The directional damage set (run 0326 vocabulary): the donor picks the "
      "flinch by the direction of the blow, and a missing slot means NO "
      "reaction at all -- so all four are filled. Right, back, left, top, "
      "in that order: lateral whiplash off the blow, a forward shove, the "
      "mirror, and a crush that flattens the arch while the head ducks";
  return s;
}

// The RUN, fixed camera like the walk, travelling.
SceneSubject subject_zixx_run() {
  SceneSubject s;
  s.name = "zixxtrixx-run";
  s.sun = &kZixxSunRun;  // Direction 29
  s.creature = 29;  // clip slot 27
  s.frames = zixx::kRunKeys * 2 * 2;  // 2 loops: 3 walked it out of frame
  s.orbit = false;
  zixx_common(s);
  s.cam_k = 310000;
  s.bump_ext = 18;
  s.note =
      "The run (run 0326 vocabulary): the walk's caterpillar law at a hungry "
      "cadence -- the hump travels once per 0.8 s loop, taller, the front "
      "wave deeper, the nose leaning in, the travel half again the walk's";
  return s;
}

// The SECOND DEATH, same staging as the first.
SceneSubject subject_zixx_death2() {
  SceneSubject s;
  s.name = "zixxtrixx-death2";
  s.sun = &kZixxSunDeath2;  // Direction 29
  s.creature = 30;  // clip slot 28
  s.frames = zixx::kDeath1Keys * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 8192;
  s.cam_k = 280000;
  s.bump_ext = 18;
  s.note =
      "Death, the second way (run 0326 vocabulary; the donor picks at random "
      "among the filled deaths): agony rears the front up, the collapse runs "
      "forward into a mostly-prone corpse, two decaying tail slaps, "
      "stillness";
  return s;
}

// item 13 diagnostic: the death2 subject re-shot in a debug shade mode.
SceneSubject subject_zixx_death2_mode(const char* name, int shade) {
  SceneSubject s = subject_zixx_death2();
  s.name = name;
  s.creature_shade = shade;
  return s;
}

// The TAUNT, near-level three-quarter so the wag reads.
SceneSubject subject_zixx_taunt() {
  SceneSubject s;
  s.name = "zixxtrixx-taunt";
  s.sun = &kZixxSunTaunt;  // Direction 29
  s.creature = 32;  // clip slot 30
  s.frames = zixx::kTauntKeys * 2 * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 12288;
  s.cam_k = 300000;
  s.bump_ext = 18;
  s.note =
      "The taunt (run 0326 vocabulary; the owner's ask, the donor's laugh "
      "slot): the front lobe rears up proud, the blades flare wide, and the "
      "head wags side to side -- slow and eased, the neck following, the "
      "mid-body counter-swaying a whisper";
  return s;
}

// The separate slow taunt, fixed three-quarter so the neck's lateral curve and
// the delayed ear-to-shoulder head tilt remain readable together.
SceneSubject subject_zixx_slow_taunt() {
  SceneSubject s;
  s.name = "zixxtrixx-slow-taunt";
  s.sun = &kZixxSunSlowTaunt;  // Direction 29
  s.creature = 46;  // clip slot 44
  s.frames = 2 * (zixx::kSlowTauntKeys - 1) + 1;
  s.canonical_creature_timeline = true;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 12288;
  s.cam_k = 300000;
  s.bump_ext = 18;
  s.note =
      "A separate slow neck-led taunt: the lower neck bends left and right, "
      "the skull follows with a delayed funny tilt, and the loose body keeps "
      "breathing underneath";
  return s;
}

// DIAGNOSTIC: the corpse loop (death's final key + a barely-there blade
// settle). Not in the library -- the site shows death; the corpse is the
// engine's resting state for the dead.
SceneSubject subject_zixx_corpse() {
  SceneSubject s;
  s.name = "zixxtrixx-corpse";
  s.creature = 33;  // clip slot 31
  s.frames = zixx::kCorpseKeys * 2 * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 8192;
  s.cam_k = 280000;
  s.bump_ext = 18;
  s.note = "DIAGNOSTIC: the corpse loop -- stillness with a sub-degree blade stir";
  return s;
}

// ---- run 0326: the salto variations (attack1/attack2, owner's ask) --------
// Wide fixed side shots -- the whole flight must stay in frame, and the
// target dummy (the watchdog quadruped) stands where the plan aims.
uint32_t zixx_one_shot_presentation_frames(int keys) {
  return keys > 0 ? static_cast<uint32_t>(2 * (keys - 1) + 1) : 0;
}

SceneSubject subject_zixx_salto_dummy() {
  SceneSubject s;
  s.name = "zixxtrixx-salto-dummy";
  s.sun = &kZixxSunSaltoDummy;  // Direction 29
  s.creature = 35;  // clip slot 33
  s.frames = zixx_one_shot_presentation_frames(
      zixx::zixx_attack_variant_key_count(
          zixx::zixx_variant_plan(zixx::kSlotAtkDummy), true));
  s.canonical_creature_timeline = true;
  s.orbit = false;
  zixx_common(s);
  s.cam_k = 180000;
  s.cam_eye = 13;
  s.cam_dist = 11;
  s.bump_ext = 18;
  s.cam_track = true;
  s.cam_track_num = 780;
  s.dummy = true;
  s.dummy_x_mm = 4600;
  s.note =
      "Salto variation (run 0326; the donor's attack1): the planner aims "
      "the committed spear at the grounded target dummy 4.6 m out -- the "
      "watchdog quadruped from the console demos -- and the strike is a "
      "MID-AIR HIT: the spear meets the body, recoils off it, and the "
      "snake falls out of the sky, gathers, and settles back into its S. "
      "Projectile, not missile: the vector locks at the apex";
  return s;
}
SceneSubject subject_zixx_salto_fly() {
  SceneSubject s;
  s.name = "zixxtrixx-salto-fly";
  s.sun = &kZixxSunSaltoFly;  // Direction 29
  s.creature = 36;  // clip slot 34
  s.frames = zixx_one_shot_presentation_frames(
      zixx::zixx_attack_variant_key_count(
          zixx::zixx_variant_plan(zixx::kSlotAtkFly), true));
  s.canonical_creature_timeline = true;
  s.orbit = false;
  zixx_common(s);
  s.cam_k = 170000;
  s.cam_eye = 14;
  s.cam_dist = 12;
  s.bump_ext = 18;
  s.cam_track = true;
  s.cam_track_num = 780;
  s.dummy = true;
  s.dummy_x_mm = 3800;
  s.dummy_y_mm = 3200;
  s.note =
      "Salto variation (run 0326): the FLYING target dummy hovers at "
      "3.2 m and the plan intercepts it there -- a shorter climb, the "
      "spear locked upward-forward, the recoil and the long fall back to "
      "the dirt. The planner sized apex, spin and plunge from the "
      "intercept alone";
  return s;
}
SceneSubject subject_zixx_salto_six() {
  SceneSubject s;
  s.name = "zixxtrixx-salto-six";
  s.sun = &kZixxSunSaltoSix;  // Direction 29
  s.creature = 37;  // clip slot 35
  s.frames = zixx_one_shot_presentation_frames(
      zixx::zixx_attack_variant_key_count(
          zixx::zixx_variant_plan(zixx::kSlotAtkSix), false));
  s.canonical_creature_timeline = true;
  s.orbit = false;
  zixx_common(s);
  s.cam_k = 150000;
  s.cam_eye = 15;
  s.cam_dist = 13;
  s.bump_ext = 18;
  s.cam_track = true;
  s.cam_track_num = 780;
  s.note =
      "Salto variation (run 0326; 'One with 6 saltos'): the full showcase "
      "apex earns SIX somersaults on a 44-key flight before the committed "
      "dive onto a far ground mark -- the spin law spends them all on the "
      "way up and the diagonal spear arrives exactly oriented";
  return s;
}

// Direction #9: immediate ground-jump examples from one programmable builder.
// Both use the same camera, timing and apex; only the authored whole-turn count
// differs, so a landing-phase drift cannot hide behind staging differences.
SceneSubject subject_zixx_jump_one() {
  SceneSubject s;
  const zixx::JumpPlan p =
      zixx::zixx_jump_plan(zixx::kSlotJumpOne, 1);
  s.name = "zixxtrixx-jump-one";
  s.sun = &kZixxSunJumpOne;  // Direction 29
  s.creature = zixx::kSlotJumpOne + 2;
  s.frames = zixx_one_shot_presentation_frames(zixx::zixx_jump_key_count(p));
  s.canonical_creature_timeline = true;
  s.orbit = false;
  zixx_common(s);
  s.cam_k = 210000;
  s.bump_ext = 18;
  s.cam_track = true;
  s.cam_track_num = 1000;
  s.note = "Immediate spring jump: short readable whole-body squash, one "
           "complete salto, authored landing bite, absorption and exact "
           "signature-S settle";
  return s;
}

SceneSubject subject_zixx_jump_multi() {
  SceneSubject s = subject_zixx_jump_one();
  const zixx::JumpPlan p =
      zixx::zixx_jump_plan(zixx::kSlotJumpMulti, 3);
  s.name = "zixxtrixx-jump-multi";
  s.sun = &kZixxSunJumpMulti;  // Direction 29
  s.creature = zixx::kSlotJumpMulti + 2;
  s.frames = zixx_one_shot_presentation_frames(zixx::zixx_jump_key_count(p));
  s.canonical_creature_timeline = true;
  s.note = "The same immediate spring-jump builder and trajectory with three "
           "complete saltos; wheel shape and upright landing phase are shared";
  return s;
}

// Direction #9's deliberate system-limit attack: same target planner/baker as
// the preserved six-salto baseline, nine whole wheel turns and exactly 24 m
// root apex. The tracked fixed-side shot retains the full arc and the grounded
// victim through entry, stable hold, extraction, recoil, landing and recovery.
SceneSubject subject_zixx_salto_nine() {
  SceneSubject s;
  const zc::AttackPlan p =
      zixx::zixx_variant_plan(zixx::kSlotAtkNine);
  s.name = "zixxtrixx-salto-nine";
  s.sun = &kZixxSunSaltoNine;  // Direction 29
  s.creature = zixx::kSlotAtkNine + 2;
  s.frames = zixx_one_shot_presentation_frames(
      zixx::zixx_attack_variant_key_count(p, true));
  s.canonical_creature_timeline = true;
  s.orbit = false;
  zixx_common(s);
  s.cam_k = 145000;
  s.cam_eye = 15;
  s.cam_dist = 13;
  s.bump_ext = 18;
  s.cam_track = true;
  s.cam_track_num = 1000;
  s.dummy = true;
  s.dummy_x_mm = 8500;
  s.dummy_y_mm = 350;
  s.note = "Nine-salto target attack / fixed-point limit probe: shared real "
           "spring, nine coherent wheel turns, exact 24 m root apex (twice "
           "the preserved six-salto 12 m apex), target entry, bit-constant "
           "embedded hold, extraction, delayed recoil and stable recovery";
  return s;
}

// The FALLING FLAIL. Use the accepted fixed-side diagnostic framing as the
// actual presentation: an orbit competing with the body's three-axis tumble hid
// the propagation, and the closer legacy shot cropped the most useful bends.
SceneSubject subject_zixx_fall() {
  SceneSubject s = subject_zixx_fall_side();
  s.name = "zixxtrixx-fall";
  s.sun = &kZixxSunFall;  // Direction 29
  s.note =
      "One complete 4.8-second loose falling loop on the accepted fixed-side "
      "camera: nonuniform tumble and low stance authority let the signature S "
      "recur rather than remain rigid, while neck loll, lateral writhe and a "
      "strong two-cycle accumulated bend propagate through straighter, deep-C "
      "and S silhouettes. Faster response over slow broad terms, never twitch";
  return s;
}

// ---- RUN 0757: the vocabulary close-out subjects --------------------------
SceneSubject subject_zixx_stance2() {
  SceneSubject s;
  s.name = "zixxtrixx-stance2";
  s.creature = 34;  // clip slot 32
  s.frames = zixx::kStance2Keys * 2 * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_k = 300000;
  s.bump_ext = 18;
  s.note =
      "The damaged stance (run 0757 vocabulary, donor stance2): the proud S "
      "sags, the head hangs below its line, the breath weakens with a slow "
      "second tremor riding it, the fins droop -- wounded with no HP bar";
  return s;
}
SceneSubject subject_zixx_tumble() {
  SceneSubject s;
  s.name = "zixxtrixx-tumble";
  s.creature = 38;  // clip slot 36
  s.frames = zixx::kTumbleKeys * 2 * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_k = 270000;
  s.bump_ext = 18;
  s.note =
      "The tumble (donor's thrown-through-the-air state): the body bunches "
      "around itself and turns end over end, once per 2.4 s -- the "
      "somersault hesitates and tips rather than turntabling, a slow roll "
      "wobble rides along, and the head looks into the turn";
  return s;
}
SceneSubject subject_zixx_death3() {
  SceneSubject s;
  s.name = "zixxtrixx-death3";
  s.creature = 31;  // clip slot 29 (death2 in donor terms; our third death)
  s.frames = zixx::kDeath2Keys * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 8192;
  s.cam_k = 280000;
  s.bump_ext = 18;
  s.note =
      "The third death, the UNWINDING: after two slow head-shakes the S "
      "itself dies -- the letter unrolls forward into a stretched line, "
      "belly down, the nose sliding out along the dirt as the coils pay "
      "out; two decaying struggle-waves; the fork folds; one small late "
      "tail beat, then stillness";
  return s;
}
SceneSubject subject_zixx_stretch() {
  SceneSubject s;
  s.name = "zixxtrixx-stretch";
  s.creature = 39;  // clip slot 37 (idle flourish 3)
  s.frames = zixx::kIdle2Keys * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 4096;  // near-side: the lift must read in profile
  s.cam_k = 300000;
  s.bump_ext = 18;
  s.note =
      "Idle flourish 3, the STRETCH: once per loop a slow luxurious wave "
      "travels up the front -- each segment lifts a beat after the one "
      "behind it, the head pitches up and back, the fins flare, and it all "
      "releases even slower than it rose";
  return s;
}
SceneSubject subject_zixx_forkwatch() {
  SceneSubject s;
  s.name = "zixxtrixx-forkwatch";
  s.creature = 40;  // clip slot 38 (idle flourish 4)
  s.frames = zixx::kIdle3Keys * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 4096;  // near-side: head-turn AND fork in one legible line
  s.cam_k = 290000;
  s.bump_ext = 18;
  s.note =
      "Idle flourish 4, FORK-WATCH: the head turns back over its shoulder "
      "and watches its own tail while the fork fans open and shut twice "
      "and the middle spike wiggles; two interested head-tilts, then home";
  return s;
}
SceneSubject subject_zixx_strike() {
  SceneSubject s;
  s.name = "zixxtrixx-strike";
  s.creature = 41;  // clip slot 39 (the quick melee, donor attack1's role)
  s.frames = zixx::kStrikeKeys * 2 * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_k = 290000;
  s.bump_ext = 18;
  s.note =
      "The quick strike (the salto is the spectacle; a battle needs a fast "
      "bite): the hook cocks back with the head rising, SNAPS open along "
      "the strike line with the body shoved forward, and two slow decaying "
      "overshoots ring it back into the S";
  return s;
}
SceneSubject subject_zixx_notify() {
  SceneSubject s;
  s.name = "zixxtrixx-notify";
  s.creature = 42;  // clip slot 40
  s.frames = zixx::kNotifyKeys * 2 * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 12288;
  s.cam_k = 300000;
  s.bump_ext = 18;
  s.note =
      "The alert (donor notify): one sharp rise -- head up, fins flared -- "
      "then the tell is STILLNESS: the breath nearly stops while the head "
      "makes two slow scanning turns, and it stands down just as slowly";
  return s;
}
SceneSubject subject_zixx_bow() {
  SceneSubject s;
  s.name = "zixxtrixx-bow";
  s.creature = 43;  // clip slot 41
  s.frames = zixx::kBowKeys * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 8192;
  s.cam_k = 290000;
  s.bump_ext = 18;
  s.note =
      "The bow: the front lowers in one stately ease, the head dips to "
      "just above the dirt, the fins fold flat along the tail; a held "
      "breath of a beat, then back up with a small arrival overshoot";
  return s;
}
SceneSubject subject_zixx_talk() {
  SceneSubject s;
  s.name = "zixxtrixx-talk";
  s.creature = 44;  // clip slot 42
  s.frames = zixx::kTalkKeys * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 12288;
  s.cam_k = 300000;
  s.bump_ext = 18;
  s.note =
      "Talk: the living idle body with the head doing the talking -- nod "
      "accents of varying depth at irregular intervals, small yaw shifts "
      "between phrases, one emphatic dip with a blade flick";
  return s;
}
SceneSubject subject_zixx_sorrow() {
  SceneSubject s;
  s.name = "zixxtrixx-sorrow";
  s.creature = 45;  // clip slot 43
  s.frames = zixx::kSorrowKeys * 2;
  s.orbit = false;
  zixx_common(s);
  s.cam_yaw = 10240;
  s.cam_k = 300000;
  s.bump_ext = 18;
  s.note =
      "Sorrow: the damaged stance pushed all the way down -- the head "
      "hangs near the dirt, one slow heavy sigh per loop, the tail-tip "
      "dragging a slow half-arc. Mourning at the pace of weather";
  return s;
}

// 17. creature-bulk-pop — bulk inflation then a detached-geometry burst: the
// watchdog idles while its root SCALE inflates 1.0 -> ~2.3; crossing the species
// pop threshold (2.2) removes the mesh. Eighteen deterministic donor samples
// become independently translated and rotating chunks with integer ballistics.
SceneSubject subject_creaturepop() {
  SceneSubject s;
  s.name = "creature-bulk-pop";
  s.frames = 72;
  s.step = 8;
  s.creature = 2;
  // The creature light rig became per-channel on 2026-08-26 and this
  // subject's shade count went 229 -> 348. It is a CREATURE subject, and
  // the 256-colour rule is a GIF-export constraint that must not shape a
  // creature -- the same law that deleted Zixxtrixx's orange. Full-colour
  // lane. NOTE: zhaozhou-site's shipped GIF for this subject is now stale
  // and needs regenerating through the full-colour path.
  s.full_colour = true;
  // fixed camera on the bump-patch crown (aim y = 8.1 m; the inflated 2.3x
  // creature reaches ~135 px before the pop)
  s.bump_ext = 6;
  s.cam_k = 220000;
  s.cam_eye = 12;
  s.cam_dist = 8;
  s.cam_bias = 0;
  s.note =
      "six-part watchdog idles (32-key breathing clip) while bulk inflates the "
      "root scale 1.0 -> 2.3 (exponential smoothing, one scalar); crossing the "
      "2.2 pop threshold removes the mesh and releases 18 detached rotating "
      "chunks, deterministically sampled from donor gibs, with integer "
      "ballistics, gravity, and damped ground bounce";
  s.expect_seq_crc = 0x8554FF23u;  // NOTE 2026-09-04: see the wave-walk pin --
  // the legacy cap layout stays frozen (cap_base_fix is opt-in).
  // RE-PINNED 2026-08-31: same cause as
  // creature-wave-walk -- see the note there for the commits and the evidence.
  // Deterministic across two runs; the 72-frame contact sheet shows the
  // approach, the pop at frame 27 and the gib ballistics reading correctly,
  // and this subject's OWN detached-piece invariant still passes (18 spawned,
  // 12 early frames with >=10 in view, span 133018 -> 980452 raw), which is a
  // stronger check than the eye for exactly the thing this clip is for.
  // Previous 0x3D259C7E (2026-08-27, Gouraud), 0xEDBA0DD2 before that.
  return s;
}

// Task #12 camera/LOD/outline limit gate.  This evaluates the exact nine-salto
// subject camera at every key and midpoint, projects every posed LOD0 vertex at
// native 384x240, and checks the same projected-radius/ink-width law used by
// creature_hook.  It catches clipping and silent splat/glint demotion; visual
// likeness remains the every-frame sheet's job.
int validate_zixx_nine_camera() {
  constexpr uint32_t kW = 384, kH = 240;
  const SceneSubject sub = subject_zixx_salto_nine();
  const zc::CreatureType& type = zixx::type();
  const zc::Clip* clip = reel_find_clip(type, zixx::kSlotAtkNine);
  if (!clip) {
    std::fprintf(stderr, "limit-camera: slot 48 missing\n");
    return 1;
  }
  const zc::AttackPlan plan = zixx::zixx_variant_plan(zixx::kSlotAtkNine);
  const zixx::AttackVariantPhases phase =
      zixx::zixx_attack_variant_phases(plan, true);
  const int samples = 2 * (static_cast<int>(clip->frame_count) - 1) + 1;

  zref::render::TerrainPatch patch =
      rtest::bump_patch(161, 161, sub.bump_ext, 8);
  const std::vector<zref::render::FieldApp> no_fields;
  const zref::terrain::ComposedLattice lat = zref::render::compose_lattice(
      patch, rtest::xform_identity(), no_fields, 0, nullptr, nullptr);
  const int32_t dog_x = fxm(zixx::kStageCentreMm);
  const zref::terrain::ColumnResult dog_col = zref::terrain::column_query(
      lat, zref::fx16{dog_x}, zref::fx16{0});
  const int32_t dog_y = dog_col.cls == zref::terrain::ColumnClass::kSolid
                            ? dog_col.top.raw
                            : 0;

  int failures = 0;
  int32_t min_radius_q8 = INT32_MAX, max_radius_q8 = 0;
  int min_ink = INT32_MAX, max_ink = 0;
  int max_lod = 0;
  int global_min_x = INT32_MAX, global_max_x = INT32_MIN;
  int global_min_y = INT32_MAX, global_max_y = INT32_MIN;
  int worst_margin = INT32_MAX;
  uint64_t saturation = 0;
  bool target_visible = true;
  const zref::render::Viewport viewport{0, 0, kW, kH};
  const ZixxTargetDescriptor target =
      zixx_target_descriptor(zixx::kSlotAtkNine);
  const int32_t target_x = fxm(zixx::kStageCentreMm + target.x_mm);
  const zref::terrain::ColumnResult target_col = zref::terrain::column_query(
      lat, zref::fx16{target_x}, zref::fx16{0});
  const int32_t target_y =
      (target_col.cls == zref::terrain::ColumnClass::kSolid
           ? target_col.top.raw
           : 0) +
      fxm(target.y_mm);
  struct LimitTargetPoint {
    int32_t x = 0, y = 0, z = 0;
  };
  std::vector<LimitTargetPoint> target_points;
  const zc::Clip* target_clip =
      target.type ? reel_find_clip(*target.type, target.clip_slot) : nullptr;
  if (!target_clip || target_clip->frame_count == 0) {
    ++failures;
  } else {
    const int target_period = 2 * static_cast<int>(target_clip->frame_count);
    const int target_tick =
        zixx_target_freeze_tick(zixx::kSlotAtkNine) % target_period;
    std::array<zc::mat3x4fx, zc::kMaxBones> target_pose;
    zc::decode_pose(*target.type, *target_clip,
                    static_cast<uint16_t>(target_tick / 2), target_pose,
                    nullptr, static_cast<uint8_t>(target_tick & 1));
    const int32_t fc = zref::fx_cos(target.facing).raw;
    const int32_t fs = zref::fx_sin(target.facing).raw;
    for (const zc::Meshlet& m : target.type->mesh) {
      for (const zc::SkinVertex& v : m.verts) {
        int32_t x = 0, y = 0, z = 0;
        zc::skin_vertex(target_pose.data(), v, x, y, z, nullptr);
        target_points.push_back(
            {target_x + zref::rescale_s32(
                            static_cast<int64_t>(fc) * x +
                                static_cast<int64_t>(fs) * z,
                            16, nullptr),
             target_y + y,
             zref::rescale_s32(-static_cast<int64_t>(fs) * x +
                                   static_cast<int64_t>(fc) * z,
                               16, nullptr)});
      }
    }
  }

  for (int tick = 0; tick < samples; ++tick) {
    const int key = tick / 2;
    int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    zixx::zixx_variant_track(zixx::kSlotAtkNine, key, x0, y0);
    zixx::zixx_variant_track(zixx::kSlotAtkNine, key + 1, x1, y1);
    const int32_t track_x_mm = (tick & 1) ? (x0 + x1) / 2 : x0;
    const int32_t track_y_mm = (tick & 1) ? (y0 + y1) / 2 : y0;
    zhao_abi::ZhMat4fx camera = cam_pitch(
        sub.cam_k, sub.cam_eye, sub.cam_dist, sub.cam_ps, sub.cam_pc,
        sub.cam_bias, -1, 0);
    camera = mat4_mul(
        camera,
        mat_world_translate(-fxm(track_x_mm), -fxm(track_y_mm), 0));
    const zref::mat4fx vp = rtest::to_zref(camera);

    zref::SatLedger ledger;
    int32_t radius_q8 = 0;
    const bool radius_visible = zc::projected_bound_radius_q8(
        vp, dog_x, dog_y, 0, type.bound_radius, kW, radius_q8, &ledger);
    if (!radius_visible || radius_q8 <= 0) ++failures;
    min_radius_q8 = std::min(min_radius_q8, radius_q8);
    max_radius_q8 = std::max(max_radius_q8, radius_q8);
    const int ink = cel_main_ink_width(radius_q8);
    min_ink = std::min(min_ink, ink);
    max_ink = std::max(max_ink, ink);
    const zc::LodRung rung = zc::lod_raw(radius_q8, 2 * 256, type);
    max_lod = std::max(max_lod, static_cast<int>(rung));

    std::array<zc::mat3x4fx, zc::kMaxBones> pose;
    zc::decode_pose(type, *clip, static_cast<uint16_t>(key), pose, &ledger,
                    static_cast<uint8_t>(tick & 1));
    int frame_min_x = INT32_MAX, frame_max_x = INT32_MIN;
    int frame_min_y = INT32_MAX, frame_max_y = INT32_MIN;
    bool any = false;
    for (const zc::Meshlet& m : type.mesh) {
      for (const zc::SkinVertex& v : m.verts) {
        int32_t x = 0, y = 0, z = 0;
        zc::skin_vertex(pose.data(), v, x, y, z, &ledger);
        const zref::render::ProjOut po = zref::render::project_vertex(
            vp, viewport, zref::fx16{x + dog_x}, zref::fx16{y + dog_y},
            zref::fx16{z}, &ledger);
        if (!po.in) continue;
        any = true;
        const int px = po.s.x >> 8;
        const int py = po.s.y >> 8;
        frame_min_x = std::min(frame_min_x, px);
        frame_max_x = std::max(frame_max_x, px);
        frame_min_y = std::min(frame_min_y, py);
        frame_max_y = std::max(frame_max_y, py);
      }
    }
    if (!any) {
      ++failures;
    } else {
      global_min_x = std::min(global_min_x, frame_min_x);
      global_max_x = std::max(global_max_x, frame_max_x);
      global_min_y = std::min(global_min_y, frame_min_y);
      global_max_y = std::max(global_max_y, frame_max_y);
      const int margin = std::min(
          {frame_min_x, static_cast<int>(kW) - 1 - frame_max_x,
           frame_min_y, static_cast<int>(kH) - 1 - frame_max_y});
      worst_margin = std::min(worst_margin, margin);
      if (margin < 0) ++failures;
    }

    if (tick >= 2 * phase.impact && tick <= 2 * phase.recoil_begin) {
      bool target_mesh_visible = false;
      for (const LimitTargetPoint& p : target_points) {
        const zref::render::ProjOut tp = zref::render::project_vertex(
            vp, viewport, zref::fx16{p.x}, zref::fx16{p.y},
            zref::fx16{p.z}, &ledger);
        const int px = tp.s.x >> 8, py = tp.s.y >> 8;
        if (tp.in && px >= 0 && px < static_cast<int>(kW) && py >= 0 &&
            py < static_cast<int>(kH)) {
          target_mesh_visible = true;
          break;
        }
      }
      if (!target_mesh_visible) target_visible = false;
    }
    saturation += ledger.total();
  }

  if (clip->frame_count != phase.frame_count) ++failures;
  if (min_ink < 1 || max_ink > kCelInkCloseWidthPx || min_ink != 1)
    ++failures;
  if (max_lod > static_cast<int>(zc::LodRung::kMicro)) ++failures;
  if (!target_visible) ++failures;
  if (saturation != 0) ++failures;
  std::printf(
      "limit-camera slot 48: %d keys/%d samples, bbox [%d..%d]x[%d..%d], "
      "worst native margin %d px, projected radius %.2f..%.2f px, ink "
      "%d..%d px, coarsest LOD %d, target_visible=%d, saturation=%llu\n",
      clip->frame_count, samples, global_min_x, global_max_x, global_min_y,
      global_max_y, worst_margin, min_radius_q8 / 256.0,
      max_radius_q8 / 256.0, min_ink, max_ink, max_lod,
      target_visible ? 1 : 0,
      static_cast<unsigned long long>(saturation));
  if (failures == 0) {
    std::printf("LIMIT CAMERA: PASS — native camera, LOD, one-pixel far "
                "outline, target and fixed-point range\n");
    return 0;
  }
  std::printf("LIMIT CAMERA: FAIL — %d assertion(s)\n", failures);
  return 1;
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
    {"star-s00-yellow", "Yellow star", "Classic main sequence star with full lens flare chain",
     true},
    {"star-s03-red-giant", "Red giant", "Large cool star with boiling CLUT rotation", true},
    {"star-s11-pulsar", "Pulsar", "Compact neutron star with duty-cycle strobe", true},
    {"terrain-wave", "Wave pool", "Travelling radial wave, two full cycles per loop", true},
    {"terrain-impact", "Impact wave", "Expanding annular wave with debris and screen shake", true},
    {"terrain-crater", "Crater ring", "Static crater with charred core and cracked ring", true},
    {"terrain-scars", "Scars accumulation", "Three strikes with persistent surface-sheet scars",
     true},
    {"terrain-orbit", "Orbit", "Textured deep-keel island, one 360-degree orbit", true},
    {"terrain-breach", "Breach", "Textured deep-keel island, 84 m pit punched through", true},
    {"celestial-sky-sweep", "Sky sweep", "Camera pitch sweep horizon to zenith", true},
    {"celestial-flare-occlusion", "Flare occlusion", "Sun crosses behind island with 15-frame fade",
     true},

    // Newly implemented stars (2026-08-16)
    {"star-s01-blue-giant", "Blue giant", "Large hot star 15k radius, bright blue-white", true},
    {"star-s02-white-dwarf", "White dwarf", "Compact hot star 300 radius, fast spin", true},
    {"star-s04-orange-giant", "Orange giant", "Warm giant 15k radius, golden orange", true},
    {"star-s07-blue-dwarf", "Blue dwarf", "Compact hot star 2k radius, fast spin", true},
    {"star-s08-multiple", "Multiple", "Binary star system 4k radius", true},
    {"star-s09-infant", "Infant star", "Young protostar with variable undertone", true},

    // Creature/character lane (spec/creature_rules.md)
    {"creature-wave-walk", "Creature wave walk",
     "Six-part quadruped strides through a wave, LOD ladder to glint", true},
    {"creature-bulk-pop", "Bulk inflate and pop",
     "Root-scale inflation releases 18 detached rotating chunks", true},

    // Upheaval bestiary lane (Upheaval/creature/) -- creature subjects shot
    // for the creature site: one camera orbit per loop, performed on ground
    {"zixxtrixx-idle", "Zixxtrixx idle",
     "Relaxed S stance: breathing, bobbing, girth swell, lazy tail sway", true},
    {"zixxtrixx-moving-light", "Zixxtrixx moving-light inspection",
     "Held signature-S under one visible world-space source, around and over", true},
    {"zixxtrixx-walk", "Zixxtrixx walk",
     "Caterpillar gait, vertical and longitudinal, fixed camera", true},
    {"zixxtrixx-attack", "Zixxtrixx triple salto",
     "Three somersaults, a diagonal javelin strike, 5 s planted; tracked", true},
    {"zixxtrixx-fall", "Zixxtrixx falling flail",
     "One fixed-side loose loop through straight, deep-C and S bends", true},
    {"zixxtrixx-hit", "Zixxtrixx hit flinch",
     "Damage recoil: the S snaps tighter, head whips, damped return", true},
    {"zixxtrixx-death", "Zixxtrixx death",
     "Shudder, the S drains, keels onto its flank, one last tail curl", true},
    {"zixxtrixx-balance", "Zixxtrixx tail-balance",
     "Rears up on its tail toward an almost-spear, topples, gets back up", true},
    {"zixxtrixx-look", "Zixxtrixx look-around",
     "Curious head-aim idle: left, up, right, down, home", true},
    {"zixxtrixx-knockdown", "Zixxtrixx knockdown",
     "Slammed onto its flank, held down, wakes and gathers back up", true},
    {"zixxtrixx-hitfloor", "Zixxtrixx landing",
     "The fall's ending: flattens, slams with a bite, gets back up", true},
    {"zixxtrixx-damage", "Zixxtrixx directional hits",
     "Four flinches: off the blow right, back, left, and crushed from above", true},
    {"zixxtrixx-run", "Zixxtrixx run",
     "The caterpillar gait at a hungry cadence, taller hump, nose in", true},
    {"zixxtrixx-death2", "Zixxtrixx death, second way",
     "Agony rear-up, forward collapse to prone, two tail slaps, still", true},
    {"zixxtrixx-taunt", "Zixxtrixx quick taunt",
     "Preserved fast cheeky head bobble over a loose body", true},
    {"zixxtrixx-slow-taunt", "Zixxtrixx slow taunt",
     "Neck-led left/right bends with a delayed funny head tilt", true},
    {"zixxtrixx-jump-one", "Zixxtrixx spring jump",
     "Immediate whole-S squash, one programmed salto, stable landing", true},
    {"zixxtrixx-jump-multi", "Zixxtrixx three-turn jump",
     "The same deterministic spring jump with three complete turns", true},
    {"zixxtrixx-salto-dummy", "Zixxtrixx salto: grounded target",
     "The planner aims the spear at a standing dummy; mid-air hit, recoil", true},
    {"zixxtrixx-salto-fly", "Zixxtrixx salto: flying target",
     "The committed spear intercepts a hovering dummy at 3.2 m", true},
    {"zixxtrixx-salto-six", "Zixxtrixx salto: six somersaults",
     "The preserved 12 m baseline spends six coherent wheel turns", true},
    {"zixxtrixx-salto-nine", "Zixxtrixx salto: nine-turn limit",
     "Nine turns at 24 m, target entry, exact hold, extraction and recovery", true},
    {"zixxtrixx-stance2", "Zixxtrixx damaged stance",
     "Wounded with no HP bar: the S sags, weak breath, drooping fins", true},
    {"zixxtrixx-tumble", "Zixxtrixx tumble",
     "Thrown: bunched around itself, end over end, hesitating as it tips", true},
    {"zixxtrixx-death3", "Zixxtrixx death, third way",
     "The S itself dies: unrolls forward into a line, nose sliding out", true},
    {"zixxtrixx-stretch", "Zixxtrixx stretch",
     "A slow luxurious wave climbs the front; head back, fins flared", true},
    {"zixxtrixx-forkwatch", "Zixxtrixx fork-watch",
     "Looks back and watches its own tail fan open and shut, twice", true},
    {"zixxtrixx-strike", "Zixxtrixx quick strike",
     "The fast bite beside the salto: cock, snap forward, ring out", true},
    {"zixxtrixx-notify", "Zixxtrixx alert",
     "Head up, fins flared, then listening stillness with slow scans", true},
    {"zixxtrixx-bow", "Zixxtrixx bow",
     "One stately dip to the dirt, fins folded, and back up", true},
    {"zixxtrixx-talk", "Zixxtrixx talk",
     "The head talks: irregular nods, phrase-turns, one emphatic dip", true},
    {"zixxtrixx-sorrow", "Zixxtrixx sorrow",
     "The front sinks, one heavy sigh per loop, the tail-tip drags", true},

    // Dead classes (no flare capability, stub entries only)
    {"star-s05-brown-dwarf", "Brown dwarf", "Dim substellar object, no flare capability", false},
    {"star-s06-grey-giant", "Grey giant", "Low luminosity giant, no flare", false},
    {"star-s10-runaway", "Runaway", "High-velocity star, no flare capability", false},
    {nullptr, nullptr, nullptr, false}};

int main(int argc, char** argv) {
  if (argc > 1 && std::strcmp(argv[1], "--zixx-target-check") == 0)
    return validate_zixx_target_interactions();
  if (argc > 1 && std::strcmp(argv[1], "--zixx-limit-check") == 0)
    return validate_zixx_nine_camera();

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
    rc |= render_scene(subject_atmosundonor());
    rc |= render_scene(subject_atmosunthick());
    rc |= render_scene(subject_planet_violet());
    rc |= render_scene(subject_planet_terran());
    rc |= render_scene(subject_planet_dust());
    rc |= render_scene(subject_planet_methane());
    rc |= render_scene(subject_planet_airless());
    rc |= render_scene(subject_planet_ember());
    rc |= render_scene(subject_planet_giant());
    rc |= render_scene(subject_planet_supergiant());
    rc |= render_scene(subject_planet_redgiant());
    rc |= render_scene(subject_planet_binary());
    rc |= render_scene(subject_creaturewalk());
    rc |= render_scene(subject_creaturepop());
    std::printf(rc == 0 ? "reel --check: all sequence CRCs match\n" : "reel --check: FAILED\n");
    return rc;
  }
  g_out = argc > 1 ? argv[1] : ".";
  // RUN 1939/2234 experiment gate. Unset (the normal case) leaves every
  // render byte-identical. Faceted cel holds a face at one band; smoothcel3
  // thresholds interpolated light per fragment. Thick modes use the contour
  // atlas for interior ink and add a five-pixel render-side silhouette below.
  if (const char* ex = std::getenv("ZIXX_EXP")) {
    const std::string e = ex;
    if (e == "cel2") zc::g_cel_bands = 2;
    else if (e == "cel3") zc::g_cel_bands = 3;
    else if (e == "smoothcel3") zc::g_smooth_toon_bands = 3;
    else if (e == "celmain") {
      zc::g_smooth_toon_bands = 3;
      g_cel_main = 1;
    }
    else if (e == "contour") g_exp_contour = 1;
    else if (e == "celcontour") { zc::g_cel_bands = 3; g_exp_contour = 1; }
    else if (e == "thick") { g_exp_contour = 1; g_exp_contour_radius = 5; }
    else if (e == "celthick") {
      zc::g_cel_bands = 3;
      g_exp_contour = 1;
      g_exp_contour_radius = 5;
    }
    else if (e == "smoothcelthick") {
      zc::g_smooth_toon_bands = 3;
      g_exp_contour = 1;
      g_exp_contour_radius = 5;
    }
    else if (e == "boil") g_exp_boil = 1;
    if (!e.empty())
      std::fprintf(stderr, "ZIXX_EXP=%s (experimental lane)\n", e.c_str());
  }
  // Direction 27 solo-source diagnostic (committed proof tool; default off).
  if (const char* solo = std::getenv("ZIXX_ML_SOLO")) {
    const std::string s = solo;
    if (s == "none") g_zixx_ml_solo = -1;
    else if (s == "warm") g_zixx_ml_solo = kZixxMovingSourceWarm;
    else if (s == "blue") g_zixx_ml_solo = kZixxMovingSourceBlue;
    else if (s == "orange") g_zixx_ml_solo = kZixxMovingSourceOrange;
    else if (s == "green") g_zixx_ml_solo = kZixxMovingSourceGreen;
    else {
      std::fprintf(stderr, "ZIXX_ML_SOLO=%s unknown (none|warm|blue|orange|green)\n", solo);
      return 2;
    }
    if (const char* boost = std::getenv("ZIXX_ML_BOOST"))
      g_zixx_ml_boost = std::max(1, std::atoi(boost));
    std::fprintf(stderr, "ZIXX_ML_SOLO=%s boost=%d (Direction 27 diagnostic lane)\n", solo,
                 g_zixx_ml_boost);
  }
  // Direction 29 revert path: ZIXX_SUNS=off renders the sunless bank
  // byte-identically (proven by CRC in the suns run). Unset or any other
  // value leaves the clip suns on -- the shipping default.
  if (const char* suns = std::getenv("ZIXX_SUNS")) {
    if (std::string(suns) == "off") {
      g_zixx_suns_enabled = false;
      g_zixx_additive_normal = false;  // Direction 30: one switch, whole revert
    }
    std::fprintf(stderr,
                 "ZIXX_SUNS=%s (Direction 29/30 clip suns + additive-normal %s)\n",
                 suns, g_zixx_suns_enabled ? "on" : "OFF");
  }
  // V11/V12 bounded owner-choice lane. The selector changes only the generic
  // creature preview rig; baseline is still the default when the variable is
  // absent. All alternatives share identical art and toon ramps. V12's ten
  // overhead options also share one held-pose world-space inspection orbit.
  if (const char* light = std::getenv("ZIXX_LIGHT")) {
    const std::string name = light;
    if (name == "baseline")
      zc::g_creature_light_rig = &zc::kCreatureLightBaseline;
    else if (name == "front-soft")
      zc::g_creature_light_rig = &zc::kCreatureLightFrontSoft;
    else if (name == "high-open")
      zc::g_creature_light_rig = &zc::kCreatureLightHighOpen;
    else if (name == "crossfill")
      zc::g_creature_light_rig = &zc::kCreatureLightCrossfill;
    else if (name == "zenith-sun")
      zc::g_creature_light_rig = &zc::kCreatureLightZenithSun;
    else if (name == "morning-crown")
      zc::g_creature_light_rig = &zc::kCreatureLightMorningCrown;
    else if (name == "evening-crown")
      zc::g_creature_light_rig = &zc::kCreatureLightEveningCrown;
    else if (name == "north-skylight")
      zc::g_creature_light_rig = &zc::kCreatureLightNorthSkylight;
    else if (name == "south-skylight")
      zc::g_creature_light_rig = &zc::kCreatureLightSouthSkylight;
    else if (name == "open-overcast")
      zc::g_creature_light_rig = &zc::kCreatureLightOpenOvercast;
    else if (name == "hard-noon")
      zc::g_creature_light_rig = &zc::kCreatureLightHardNoon;
    else if (name == "veiled-sun")
      zc::g_creature_light_rig = &zc::kCreatureLightVeiledSun;
    else if (name == "silver-moon")
      zc::g_creature_light_rig = &zc::kCreatureLightSilverMoon;
    else if (name == "cloudbreak")
      zc::g_creature_light_rig = &zc::kCreatureLightCloudbreak;
    else if (name == "corrected-toplight-1")
      zc::g_creature_light_rig = &zc::kCreatureLightCorrectedToplight1;
    else if (name == "diagonal-daylight")
      zc::g_creature_light_rig = &zc::kCreatureLightDiagonalDaylight;
    else if (name == "diagonal-open-sky")
      zc::g_creature_light_rig = &zc::kCreatureLightDiagonalOpenSky;
    else if (name == "diagonal-warm-cross")
      zc::g_creature_light_rig = &zc::kCreatureLightDiagonalWarmCross;
    else if (name == "diagonal-cool-cross")
      zc::g_creature_light_rig = &zc::kCreatureLightDiagonalCoolCross;
    else if (name == "diagonal-soft-cloud")
      zc::g_creature_light_rig = &zc::kCreatureLightDiagonalSoftCloud;
    else if (name == "diagonal-hard-sun")
      zc::g_creature_light_rig = &zc::kCreatureLightDiagonalHardSun;
    else if (name == "diagonal-cloudbreak")
      zc::g_creature_light_rig = &zc::kCreatureLightDiagonalCloudbreak;
    else if (name == "diagonal-silver-moon")
      zc::g_creature_light_rig = &zc::kCreatureLightDiagonalSilverMoon;
    else if (name == "diagonal-broad-bounce")
      zc::g_creature_light_rig = &zc::kCreatureLightDiagonalBroadBounce;
    else if (name == "diagonal-rose-dusk")
      zc::g_creature_light_rig = &zc::kCreatureLightDiagonalRoseDusk;
    else {
      std::fprintf(stderr,
                   "unknown ZIXX_LIGHT=%s (expected baseline, front-soft, high-open, crossfill, "
                   "zenith-sun, morning-crown, evening-crown, north-skylight, south-skylight, "
                   "open-overcast, hard-noon, veiled-sun, silver-moon, cloudbreak, "
                   "corrected-toplight-1, diagonal-daylight, diagonal-open-sky, "
                   "diagonal-warm-cross, diagonal-cool-cross, diagonal-soft-cloud, "
                   "diagonal-hard-sun, diagonal-cloudbreak, diagonal-silver-moon, "
                   "diagonal-broad-bounce, diagonal-rose-dusk)\n",
                   light);
      return 2;
    }
    std::fprintf(stderr, "ZIXX_LIGHT=%s (lighting choice lane)\n", light);
  }
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
  if (wanted("terrain-orbit")) rc |= render_scene(subject_orbit());
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
  if (wanted("atmo-sun-donor")) rc |= render_scene(subject_atmosundonor());
  if (wanted("atmo-sun-thick")) rc |= render_scene(subject_atmosunthick());
  if (wanted("planet-sun-violet")) rc |= render_scene(subject_planet_violet());
  if (wanted("planet-sun-terran")) rc |= render_scene(subject_planet_terran());
  if (wanted("planet-sun-dust")) rc |= render_scene(subject_planet_dust());
  if (wanted("planet-sun-methane")) rc |= render_scene(subject_planet_methane());
  if (wanted("planet-sun-airless")) rc |= render_scene(subject_planet_airless());
  if (wanted("planet-sun-ember")) rc |= render_scene(subject_planet_ember());
  if (wanted("planet-sun-giant")) rc |= render_scene(subject_planet_giant());
  if (wanted("planet-sun-supergiant")) rc |= render_scene(subject_planet_supergiant());
  if (wanted("planet-sun-redgiant")) rc |= render_scene(subject_planet_redgiant());
  if (wanted("planet-sun-binary")) rc |= render_scene(subject_planet_binary());
  if (wanted("creature-wave-walk")) rc |= render_scene(subject_creaturewalk());
  if (wanted("unnamed02-hover")) rc |= render_scene(subject_u02_clip(0, "unnamed02-hover", u02::kIdleKeys, true, &kU02SunHover));
  if (wanted("unnamed02-inspect")) {
    // Direction 3 §1: EXACTLY ONE subject carries the four coloured moving
    // lights — this inspection showcase. Hover keys, orbit camera, the u02
    // moving rig and source paths from pass 2, and NO marker orbs
    // (moving_markers is cleared for the whole species).
    SceneSubject s = subject_u02_clip(0, "unnamed02-inspect", u02::kIdleKeys, true, nullptr);
    s.creature_moving_light = true;
    rc |= render_scene(s);
  }
  if (wanted("unnamed02-drift")) rc |= render_scene(subject_u02_clip(1, "unnamed02-drift", u02::kDriftKeys, false, &kU02SunDrift));
  if (wanted("unnamed02-channel")) rc |= render_scene(subject_u02_clip(2, "unnamed02-channel", u02::kChannelKeys, false, &kU02SunChannel));
  if (wanted("unnamed02-curious")) rc |= render_scene(subject_u02_clip(3, "unnamed02-curious", u02::kCuriousKeys, false, &kU02SunCurious));
  if (wanted("unnamed02-startle")) rc |= render_scene(subject_u02_clip(4, "unnamed02-startle", u02::kStartleKeys, false, &kU02SunStartle));
  if (wanted("unnamed02-rest")) rc |= render_scene(subject_u02_clip(5, "unnamed02-rest", u02::kRestKeys, false, &kU02SunCalm));
  if (wanted("unnamed02-pirouette")) rc |= render_scene(subject_u02_clip(6, "unnamed02-pirouette", u02::kPirouetteKeys, false, &kU02SunPirouette));
  if (wanted("unnamed02-hasty")) rc |= render_scene(subject_u02_clip(8, "unnamed02-hasty", u02::kHastyKeys, false, &kU02SunHasty));
  if (wanted("unnamed02-fall")) rc |= render_scene(subject_u02_clip(9, "unnamed02-fall", u02::kFallKeys, false, &kU02SunFall));
  if (wanted("unnamed02-hit")) rc |= render_scene(subject_u02_clip(10, "unnamed02-hit", u02::kHitKeys, false, &kU02SunHit));
  if (wanted("unnamed02-taunt")) rc |= render_scene(subject_u02_clip(11, "unnamed02-taunt", u02::kTauntKeys, false, &kU02SunTaunt));
  if (wanted("unnamed02-taunt2")) rc |= render_scene(subject_u02_clip(12, "unnamed02-taunt2", u02::kTaunt2Keys, false, &kU02SunTaunt2));
  if (wanted("unnamed02-crackle")) {
    SceneSubject s = subject_u02_clip(0, "unnamed02-crackle", u02::kIdleKeys, false, &kU02SunChannel);
    s.u02_mana = 4;  // pass 2: the crackle IS the lightning candidate
    s.planet = 1;  // fixed camera: the bloom may stage the crackle
    s.planet_sun_x = 58;
    s.planet_sun_y0 = 96;
    s.planet_sun_y1 = 132;
    s.note = "the ADDLIGHTNING variant: the conduit crackles continuously";
    rc |= render_scene(s);
  }
  // Pass 2 (Direction 2 §3): THE MANA MENU — six named candidates, each a
  // short clip on the reworked hover, for the OWNER to pick from by eye.
  {
    static const struct { const char* name; int cand; } kManaMenu[] = {
        {"unnamed02-mana-pulsar", 1},   {"unnamed02-mana-plasma", 2},
        {"unnamed02-mana-bullets", 3},  {"unnamed02-mana-lightning", 4},
        {"unnamed02-mana-boil", 5},     {"unnamed02-mana-drip", 6},
    };
    for (const auto& m : kManaMenu) {
      if (!wanted(m.name)) continue;
      SceneSubject s = subject_u02_clip(0, m.name, u02::kIdleKeys, false, &kU02SunHover);
      s.u02_mana = m.cand;
      s.note = "mana-menu candidate: the owner picks with his eyes";
      rc |= render_scene(s);
    }
  }
  if (wanted("u02-trio")) {
    SceneSubject s = subject_u02_clip(2, "u02-trio", u02::kChannelKeys, false, &kU02SunChannel);
    s.u02_trio = true;
    s.cam_k = 170000;  // pull back: three conduits share the frame
    s.note = "THREE phase-offset conduits, one scene sun + bloom, all effects";
    rc |= render_scene(s);
  }
  if (wanted("u02-s5-glow")) rc |= render_scene(subject_u02_s5());
  if (wanted("u02-s4-side")) rc |= render_scene(subject_u02_s4(0));
  if (wanted("u02-s4-front")) rc |= render_scene(subject_u02_s4(1));
  if (wanted("u02-s4-tq")) rc |= render_scene(subject_u02_s4(2));
  if (wanted("u02-s4-wire")) rc |= render_scene(subject_u02_s4(3));
  if (wanted("u02-s4-ids")) rc |= render_scene(subject_u02_s4(4));
  if (wanted("u02-s4-stage")) rc |= render_scene(subject_u02_s4(5));
  if (wanted("u02-s4-unlit")) rc |= render_scene(subject_u02_s4(6));
  if (wanted("u02-s3-motes")) rc |= render_scene(s3_mote_subject(false));
  if (wanted("u02-s3-motes-add")) rc |= render_scene(s3_mote_subject(true));
  if (wanted("u02-s1-bloom")) rc |= render_scene(s1_chain_subject(0));
  if (wanted("u02-s1-creature")) rc |= render_scene(s1_chain_subject(1));
  if (wanted("u02-s1-chained")) rc |= render_scene(s1_chain_subject(2));
  if (wanted("creature-bulk-pop")) rc |= render_scene(subject_creaturepop());
  if (wanted("zixxtrixx-idle")) rc |= render_scene(subject_zixx_idle());
  if (wanted("zixxtrixx-moving-light"))
    rc |= render_scene(subject_zixx_moving_light());
  if (wanted("zixxtrixx-walk")) rc |= render_scene(subject_zixx_walk());
  if (wanted("zixxtrixx-attack")) rc |= render_scene(subject_zixx_attack());
  if (wanted("zixxtrixx-spring-side")) rc |= render_scene(subject_zixx_spring_side());
  if (wanted("zixxtrixx-spring-top")) rc |= render_scene(subject_zixx_spring_top());
  if (wanted("zixxtrixx-spring-micro")) rc |= render_scene(subject_zixx_spring_micro());
  if (wanted("zixxtrixx-fall")) rc |= render_scene(subject_zixx_fall());
  if (wanted("zixxtrixx-front")) rc |= render_scene(subject_zixx_front());
  if (wanted("zixxtrixx-side")) rc |= render_scene(subject_zixx_side());
  if (wanted("zixxtrixx-side-game")) rc |= render_scene(subject_zixx_side_game());
  if (wanted("zixxtrixx-tq")) rc |= render_scene(subject_zixx_tq());
  if (wanted("zixxtrixx-frontfix")) rc |= render_scene(subject_zixx_frontfix());
  if (wanted("zixxtrixx-frontfix-game")) rc |= render_scene(subject_zixx_frontfix_game());
  if (wanted("zixxtrixx-still")) rc |= render_scene(subject_zixx_still());
  if (wanted("zixxtrixx-light-choice")) rc |= render_scene(subject_zixx_light_choice());
  if (wanted("zixxtrixx-overhead-orbit")) rc |= render_scene(subject_zixx_overhead_orbit());
  if (wanted("zixxtrixx-corrected-toplight-1"))
    rc |= render_scene(subject_zixx_corrected_toplight_1());
  if (wanted("zixxtrixx-still-game")) rc |= render_scene(subject_zixx_still_game());
  if (wanted("zixxtrixx-still-front")) rc |= render_scene(subject_zixx_still_front());
  if (wanted("zixxtrixx-fall-side")) rc |= render_scene(subject_zixx_fall_side());
  if (wanted("zixxtrixx-hit")) rc |= render_scene(subject_zixx_hit());
  if (wanted("zixxtrixx-death")) rc |= render_scene(subject_zixx_death());
  if (wanted("zixxtrixx-balance")) rc |= render_scene(subject_zixx_balance());
  if (wanted("zixxtrixx-look")) rc |= render_scene(subject_zixx_look());
  if (wanted("zixxtrixx-look-game")) rc |= render_scene(subject_zixx_look_game());
  if (wanted("zixxtrixx-pupil-proof")) rc |= render_scene(subject_zixx_pupil_proof(false, false));
  if (wanted("zixxtrixx-pupil-proof-game")) rc |= render_scene(subject_zixx_pupil_proof(false, true));
  if (wanted("zixxtrixx-pupil-proof-front")) rc |= render_scene(subject_zixx_pupil_proof(true, false));
  if (wanted("zixxtrixx-pupil-proof-front-game")) rc |= render_scene(subject_zixx_pupil_proof(true, true));
  if (wanted("zixxtrixx-pupil-proof-other")) rc |= render_scene(subject_zixx_pupil_proof_other(false));
  if (wanted("zixxtrixx-pupil-proof-other-game")) rc |= render_scene(subject_zixx_pupil_proof_other(true));
  if (wanted("zixxtrixx-knockdown")) rc |= render_scene(subject_zixx_knockdown());
  if (wanted("zixxtrixx-knockdown-unlit")) rc |= render_scene(subject_zixx_knockdown_mode("zixxtrixx-knockdown-unlit", 1));
  if (wanted("zixxtrixx-knockdown-normviz")) rc |= render_scene(subject_zixx_knockdown_mode("zixxtrixx-knockdown-normviz", 2));
  if (wanted("zixxtrixx-knockdown-wire")) rc |= render_scene(subject_zixx_knockdown_mode("zixxtrixx-knockdown-wire", 3));
  if (wanted("zixxtrixx-knockdown-triids")) rc |= render_scene(subject_zixx_knockdown_mode("zixxtrixx-knockdown-triids", 4));
  if (wanted("zixxtrixx-hitfloor")) rc |= render_scene(subject_zixx_hitfloor());
  if (wanted("zixxtrixx-damage")) rc |= render_scene(subject_zixx_damage());
  if (wanted("zixxtrixx-run")) rc |= render_scene(subject_zixx_run());
  if (wanted("zixxtrixx-death2")) rc |= render_scene(subject_zixx_death2());
  if (wanted("zixxtrixx-taunt")) rc |= render_scene(subject_zixx_taunt());
  if (wanted("zixxtrixx-slow-taunt")) rc |= render_scene(subject_zixx_slow_taunt());
  if (wanted("zixxtrixx-corpse")) rc |= render_scene(subject_zixx_corpse());
  if (wanted("zixxtrixx-salto-dummy")) rc |= render_scene(subject_zixx_salto_dummy());
  if (wanted("zixxtrixx-salto-fly")) rc |= render_scene(subject_zixx_salto_fly());
  if (wanted("zixxtrixx-salto-six")) rc |= render_scene(subject_zixx_salto_six());
  if (wanted("zixxtrixx-jump-one")) rc |= render_scene(subject_zixx_jump_one());
  if (wanted("zixxtrixx-jump-multi")) rc |= render_scene(subject_zixx_jump_multi());
  if (wanted("zixxtrixx-salto-nine")) rc |= render_scene(subject_zixx_salto_nine());
  if (wanted("zixxtrixx-stance2")) rc |= render_scene(subject_zixx_stance2());
  if (wanted("zixxtrixx-tumble")) rc |= render_scene(subject_zixx_tumble());
  if (wanted("zixxtrixx-death3")) rc |= render_scene(subject_zixx_death3());
  if (wanted("zixxtrixx-stretch")) rc |= render_scene(subject_zixx_stretch());
  if (wanted("zixxtrixx-forkwatch")) rc |= render_scene(subject_zixx_forkwatch());
  if (wanted("zixxtrixx-strike")) rc |= render_scene(subject_zixx_strike());
  if (wanted("zixxtrixx-notify")) rc |= render_scene(subject_zixx_notify());
  if (wanted("zixxtrixx-bow")) rc |= render_scene(subject_zixx_bow());
  if (wanted("zixxtrixx-talk")) rc |= render_scene(subject_zixx_talk());
  if (wanted("zixxtrixx-sorrow")) rc |= render_scene(subject_zixx_sorrow());
  if (wanted("zixxtrixx-unlit")) rc |= render_scene(subject_zixx_mode("zixxtrixx-unlit", 1, false, 1));
  if (wanted("zixxtrixx-unlit-front")) rc |= render_scene(subject_zixx_mode("zixxtrixx-unlit-front", 1, true, 1));
  if (wanted("zixxtrixx-normviz")) rc |= render_scene(subject_zixx_mode("zixxtrixx-normviz", 2, false, 1));
  // item 13 (RUN 1939): the death2 stray-eye-triangle hunt -- the same
  // death2 subject in unlit / normal-viz / wireframe so a defect can be
  // separated into "geometry in the wrong place" vs "shaded wrong".
  if (wanted("zixxtrixx-death2-unlit")) rc |= render_scene(subject_zixx_death2_mode("zixxtrixx-death2-unlit", 1));
  if (wanted("zixxtrixx-death2-normviz")) rc |= render_scene(subject_zixx_death2_mode("zixxtrixx-death2-normviz", 2));
  if (wanted("zixxtrixx-death2-wire")) rc |= render_scene(subject_zixx_death2_mode("zixxtrixx-death2-wire", 3));
  if (wanted("zixxtrixx-wire")) rc |= render_scene(subject_zixx_mode("zixxtrixx-wire", 3, false, 1));
  if (wanted("zixxtrixx-walk-unlit")) rc |= render_scene(subject_zixx_walk_unlit());
  if (wanted("zixxtrixx-lodsweep")) rc |= render_scene(subject_zixx_lodsweep());
#ifdef ZIXX_F2_PREVIEW
  if (wanted("zixxtrixx-fall-baked")) rc |= render_scene(subject_zixx_fall_baked());
#endif
#ifdef ZIXX_SWEEP
  if (wanted("zixxtrixx-sweep")) rc |= render_scene(subject_zixx_sweep());
#endif
  return rc;
}
