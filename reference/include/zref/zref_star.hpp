// zref_star.hpp — the Noctis star gamut, coronas, lens flares and the
// procedural starfield: the ZRef preview surface of spec/stars_and_flares.md
// (RATIFIED v1, RUN-20260815-2307), §12 "ZRef preview functions".
//
// Law (in citation order):
//   spec/stars_and_flares.md
//     §1  CLUT ramp discipline (intensity×ramp as a texture/material
//         discipline; effect tag (channel<<6)|strength, GLOW = 0b01)
//     §2  the 12-class gamut table (verbatim compiled defaults), SPIN_K = 55,
//         pulsar duty spin_phase < 0x4000, infant undertone law,
//         radius_milli = class_ray + (h1 mod class_rayvar)
//     §3  star disc: starface bake law (256×64 PCG grid → 3×3 box smooth →
//         QT-VR orthographic resample into the 120-half-texel disc),
//         boil rotation rot = (tick/3) mod 63, SATUR = min(63, 12d/r),
//         ramp build (×4 s16 pre-clamp domain — see the file's 2026-08-16
//         implementation clarification — segment law + round_half_up,
//         slew ±1/tick), DISC_RMAX = 112 px
//     §4  corona bake law (linear falloff, core16 variants 0/5/16 5/16 8/16),
//         halo through the un-rotated un-floored ramp, HALO_RMAX 225/160
//     §5  lens flare: baked sprites (burst12/burst4/streak, SPOKE_LEN_SEQ),
//         per-light law k = clamp(d/r,5,384), b = clamp(floor(log2(d/r))−2,
//         0,7), the ghost chain (−26/−77/−230 Q8.8, sizes 26/102/410 Q8.8),
//         glow += rescale_u(sprite·a, 8), flare_texels ≤ 16384,
//         probe latch / ±1 fade counter (fade_alpha = ctr·17) / border fade
//     §6  distance LOD ladder + glint minimum-brightness clamp
//     §7  procedural starfield: the harness-oracle sector hash transliterated
//         verbatim (existence/position), PCG identity schedule, rarity gate,
//         intensity6 = 63 − (rz>>13)
//     §8  celestial_state capture chunk (serialize/deserialize round-trip)
//     §9  placement: bakes are ARM/compile-time, splats are bounded POST —
//         nothing here needs a fragment shader, render-to-texture or a
//         second TMU (charter §26 refusals untouched)
//     §15 motion trails (amendment v1.2): the TrailHistory ring, graded
//         corona and disc intensity replay, subtract-8 decay, the exact
//         asymmetric smoother twice per source-age step, static-skip and
//         halo-skip laws, capture-in-state
//   spec/qformats.md §3/§4 single-rounding arithmetic + round_half_up,
//         §7.1 the ONE 257-entry sin table (asin16 below is its inverse by
//         binary search — no second trig law), §7.2 isqrt, §7.5 noise2_hash
//   spec/sky_and_beams.md §3 (the glow-buffer/composite precedent the flare
//         splat mode sits beside: 96×60 Z60 quarter-res, saturating u16)
//   charter §8 (24-bit working tile + 8-bit effect tag), §15 (masked /
//         additive material recipes), §26 (refusals), §29-6/§29-7
//
// Phase-3 scope ([phase3-preview], mirrors the sky's [w3.5-software]):
//   * These are the ARM/asset-side laws plus a software compositor preview.
//     RTL is Phase 11 (spec §11). The `SetCelestials 0x0320` command surface
//     is an ABI RESERVATION only — it is not in spec/commands.zidl yet, so
//     the preview rides the renderer's pre-resolve hook (zref_render.hpp)
//     with screen-space parameters computed caller-side in wide integers
//     (spec §9: "only screen-space quantities cross to the FPGA").
//   * Textures are baked level-0 CLUT8; the +mips of §3 are an asset-pack
//     product (W3.6 lineage) and the preview samples nearest from level 0.
//   * Interstellar distances exceed fx16 BY LAW (spec §14 risk 4): every
//     d/r input below is a WIDE INTEGER (s64 milli-units), never fx16.

#pragma once

#include "zref/zref_fixp.hpp"
#include "zref/zref_trig.hpp"

#include <cstdint>
#include <vector>

namespace zref {
namespace star {

// ---- §1 effect-tag convention (frozen) -------------------------------------

inline constexpr uint8_t kTagChannelGlow = 0b01;  // GLOW channel id
inline constexpr uint8_t glow_tag(uint8_t strength6) {
  return static_cast<uint8_t>((kTagChannelGlow << 6) | (strength6 & 0x3F));
}
inline constexpr bool tag_is_glow(uint8_t tag) { return (tag >> 6) == kTagChannelGlow; }

// ---- §2 the star gamut table (verbatim compiled spec-frozen defaults) ------

struct StarClass {
  const char* name;
  uint8_t rgb6[3];    // class colour, VGA 6-bit (0..63)
  uint8_t under6[3];  // undertone, original 0..64 domain; S09: per-identity
  int32_t ray_milli;  // class_ray
  int32_t rayvar_milli;
  uint8_t dfs;       // U2.6 surface daylight factor (consumed by sky grading)
  uint8_t smooth;    // starface box-smooth passes (2, compact classes 5)
  uint8_t spin_mod;  // 0 = no spin; else spin_rate = SPIN_K*(1 + h2 % spin_mod)
  uint8_t flare;     // 0 = never (dead classes), 1 = yes, 2 = pulsar duty
};

inline constexpr int kStarClasses = 12;
extern const StarClass kGamut[kStarClasses];

// SPIN_K: angle16 units/tick per original degree/frame (§2: 65536/360 ÷
// (60/18.2) = 55.16 → 55, error 0.4%).
inline constexpr int32_t kSpinK = 55;

// §2 pulsar duty (frozen): active ⟺ spin_phase < 0x4000 (exact quarter turn).
inline constexpr bool pulsar_active(uint16_t spin_phase) { return spin_phase < 0x4000; }
// §2 pulse_gain U3.5: 160 (=5.0) during flash, 32 (=1.0) off.
inline constexpr uint8_t pulse_gain_u35(uint16_t spin_phase) {
  return pulsar_active(spin_phase) ? 160 : 32;
}

// ---- §7 identity schedule (frozen, replaces Borland rand) ------------------

// CLASS_PICK[32]: S00×7 S01×2 S02×2 S03×4 S04×4 S05×4 S06×2 S07×2 S08×2
// S09×1 S10×1 S11×1 — indexed by h0 >> 27.
extern const uint8_t kClassPick[32];

struct StarIdentity {
  uint8_t cls = 0;             // gamut row
  int32_t radius_milli = 0;    // class_ray + (h1 mod class_rayvar)
  uint16_t spin_rate = 0;      // angle16 units/tick; 0 = non-compact (§2:
                               // wall-clock rotation DROPPED, boil carries it)
  uint32_t spin_draw = 0;      // h2 (kept for capture §8)
  uint32_t texture_seed = 0;   // h3
  uint8_t under6[3] = {0, 0, 0};  // resolved undertone (S09: 24 + ((h3>>(5c))&31))
};

/** §7 identity: sector coords + galaxy seed → everything about the star. */
StarIdentity identity(int32_t sx, int32_t sy, int32_t sz, uint32_t galaxy_seed);

// ---- §3 the ramp (build + slew) --------------------------------------------
//
// Control points in the s16 PRE-CLAMP domain (×4 expansion; file
// clarification 2026-08-16): P0 = (0,0,0), P1 = undertone6×4, P2 = rgb6×4,
// P3 = (256,280,304). Segments [0..24) P0→P1, [24..40) P1→P2, [40..64)
// P2→P3; within [a,a+n): ramp[i] = base + round_half_up((tgt−base)·(i−a), n),
// clamp [0,255]. Slew: every control-point channel walks ±1/tick toward its
// target — palette changes never pop (§3).

struct RampState {
  int16_t cur[12] = {};  // P0..P3 × RGB, s16 pre-clamp domain (current)
  int16_t tgt[12] = {};  // targets
  uint8_t init = 0;      // 0: first retarget snaps cur = tgt
};

/** §3 the class control points (s16 pre-clamp domain), P0..P3 × RGB. */
void ramp_points(const StarIdentity& id, int16_t out[12]);
/** Retarget toward `id`; first call snaps (no slew from nothing). */
void ramp_retarget(RampState& st, const StarIdentity& id);
/** One ±1/tick slew step per channel (§3). */
void ramp_slew_step(RampState& st);
/** Build the 64-entry ramp from control points (segment law + clamp). */
void ramp_build(const int16_t pts[12], uint8_t out[64][3]);

// ---- §3 disc palette: boil + distance washout ------------------------------

/** §3 SATUR = min(63, (12·d)/r) — wide-integer inputs (spec §14 risk 4). */
uint8_t satur_of(int64_t d_milli, int64_t r_milli);
/** §3 rot = (tick / BOIL_DIV) mod 63, BOIL_DIV = 3. */
inline constexpr uint32_t kBoilDiv = 3;
inline constexpr uint8_t boil_rot(uint32_t tick) {
  return static_cast<uint8_t>((tick / kBoilDiv) % 63u);
}
/** §3 disc palette index law for entry e = 1..63 (0 stays transparent):
 *  index = max(1 + ((e−1+rot) mod 63), SATUR). */
uint8_t boil_index(uint8_t e, uint8_t rot, uint8_t satur);
/** §3 pal_d: full disc palette (entry 0 = transparent, callers skip it). */
void palette_disc(const uint8_t ramp[64][3], uint32_t tick, int64_t d_milli, int64_t r_milli,
                  uint8_t out[64][3]);
/** §4 pal_h: halo palette = the ramp un-rotated, un-floored; [0] = black
 *  (the additive identity). */
void palette_halo(const uint8_t ramp[64][3], uint8_t out[64][3]);

// ---- §3/§4/§5 baked sprites ------------------------------------------------

struct Sprite8 {
  uint16_t w = 0, h = 0;
  std::vector<uint8_t> pix;  // row-major intensity (CLUT index / u8 alpha)
};

/**
 * asin16(y): inverse of the frozen §7.1 sin table by binary search — the
 * largest a in [0, 0x4000] with fx_sin(a).raw ≤ |y|, sign-mirrored. Input
 * fx16 raw in [−65536, 65536] (clamped), result signed angle16 units in
 * [−0x4000, 0x4000]. Integer-only; NO second trig approximation exists
 * (charter §29-6) — this is a search over the one table.
 */
int32_t asin16(int32_t y_fx_raw);

/** §3 starface bake: 128×128 CLUT8, index 0 transparent outside the
 *  120-half-texel disc, intensity 1..63 inside (1 + smoothed grid value).
 *  smooth_passes comes from the class table (2, compact 5). */
Sprite8 starface(uint32_t texture_seed, uint8_t smooth_passes);

/** §4 corona bake: 128×128 radial, linear falloff, core16 ∈ {0,5,8}
 *  (halo_atmo / halo_space / halo_airless). */
Sprite8 corona_sprite(uint8_t core16);

// §3/§4 screen clamps (Z60; Duo passes its own smaller halo clamp).
inline constexpr int32_t kDiscRMaxPx = 112;
inline constexpr int32_t kHaloRMaxZ60Px = 225;
inline constexpr int32_t kHaloRMaxDuoPx = 160;

// STAR_DEPTH (§3): sky-prefill far + 1 — beats the sky backdrop (depth 0),
// loses to every real surface (Q16.16 1/w, larger = closer, qformats §8).
inline constexpr int32_t kStarDepth = 1;

// ---- §6 distance LOD ladder ------------------------------------------------

enum class LodRung : uint8_t {
  kDisc = 0,    // ≥ 6 px: textured disc + corona + flare
  kCorona = 1,  // 1.5–6 px: corona sprite + flare
  kGlint = 2,   // < 1.5 px, d ≤ 1550r: far glint (min-brightness clamp)
  kPoint = 3,   // beyond: procedural starfield point only
};

/** §6 raw rung from projected disc radius (S12.8 px) + wide d/r. */
LodRung lod_rung_raw(int32_t proj_radius_q8, int64_t d_milli, int64_t r_milli);

struct LodState {
  LodRung rung = LodRung::kPoint;
  uint16_t hold = 0;  // ticks at the current rung
  uint8_t init = 0;
};

/** §6 rung with §9-style hysteresis: 15-tick minimum hold + 10% threshold
 *  hysteresis (a switch requires crossing the boundary by 10% in the
 *  direction of change, and the hold to have elapsed). */
LodRung lod_select(LodState& st, int32_t proj_radius_q8, int64_t d_milli, int64_t r_milli);

/** §6 far glint: intensity6 = 48 + clamp((1600r − d)/(100r), 0, 15) —
 *  never dimmer than 75% of full. */
uint8_t glint_intensity6(int64_t d_milli, int64_t r_milli);

}  // namespace star

// ---- §5 lens flare (a Mirror Gate mode: frozen-table additive splats) ------

namespace flare {

// SPOKE_LEN_SEQ (§5): the ×1.5 zig-zag frozen as Q4.4 sixteenths; half-length
// L_i = R_canvas · SEQ[i mod 8] / 54.
extern const uint8_t kSpokeLenSeq[8];

/** The three baked sprites (§5): burst12/burst4 64×64, streak 96×16.
 *  Baked once per process from the frozen law (canvas 512² / 768×128,
 *  1-px additive Bresenham lines value 32 saturating, 8× box downsample);
 *  deterministic — byte-identical on every call. */
struct Sprites {
  star::Sprite8 burst12, burst4, streak;
};
const Sprites& sprites();

/** §5 per-light law (wide-int inputs): k = clamp(d/r, 5, 384) px;
 *  b = clamp(floor(log2(d/r)) − 2, 0, 7); sprite by bucket. */
struct LightLaw {
  int32_t k = 0;
  uint8_t bucket = 0;
  uint8_t sprite = 0;  // 0 burst12 (b≤3), 1 burst4 (b≤6), 2 streak
};
LightLaw light_law(int64_t d_milli, int64_t r_milli);

/** One splat: full-res screen px (the glow buffer is ¼ res — the splatter
 *  shifts by 2). half_y differs from half_x only for the streak (aspect). */
struct Splat {
  int32_t cx_px = 0, cy_px = 0;
  int32_t half_x_px = 0, half_y_px = 0;
  uint8_t alpha = 0;   // burst 255, ghosts 64 (§5 table)
  uint8_t sprite = 0;  // 0/1/2 as LightLaw::sprite (ghosts reuse the burst)
};

// §5 ghost table (Q8.8 of the light position relative to the view centre /
// of k): positions −26/−77/−230, half-sizes 26/102/410, alpha 64.
extern const int16_t kGhostPos[3];
extern const int16_t kGhostSize[3];

/** §5 emit: burst at the light, 3 ghosts mirrored through the view centre
 *  (position = centre + rescale(g·(L−C), 8), one round_half_up per axis).
 *  Returns the number of splats written (4; zero-size ghosts dropped). */
int emit(int32_t lx_px, int32_t ly_px, int32_t cx_px, int32_t cy_px, const LightLaw& law,
         Splat out[4]);

/** §5 fade counter: ±1/frame toward target 15 (visible) / 0. */
inline constexpr uint8_t fade_step(uint8_t ctr, bool target_visible) {
  const uint8_t tgt = target_visible ? 15 : 0;
  return static_cast<uint8_t>(ctr < tgt ? ctr + 1 : (ctr > tgt ? ctr - 1 : ctr));
}
/** §5 fade_alpha = ctr·17 (15 → 255; no step exceeds 17/frame). */
inline constexpr uint8_t fade_alpha(uint8_t ctr) {
  return static_cast<uint8_t>((ctr > 15 ? 15 : ctr) * 17);
}
/** §5 border fade: min(255, clamp(edge_dist,0,16)·16) — replaces the hard
 *  off-screen cut (edge_dist = signed px distance to the nearest viewport
 *  edge; ≤ 0 = at/outside the edge). */
inline constexpr uint8_t border_alpha(int32_t edge_dist_px) {
  const int32_t c = edge_dist_px < 0 ? 0 : (edge_dist_px > 16 ? 16 : edge_dist_px);
  const int32_t a = c * 16;
  return static_cast<uint8_t>(a > 255 ? 255 : a);
}

// §5 bound: flare_texels ≤ 16384 per view per frame. A splat that would
// exceed the remaining budget is dropped WHOLE (deterministic degradation —
// the far sun loses its big ghost first).
inline constexpr uint32_t kFlareTexelBudget = 16384;

}  // namespace flare

// ---- §5 POST: the glow-buffer splat + composite ----------------------------

namespace post {

/** Quarter-res glow plane (sky_and_beams §3 precedent: 96×60 Z60). */
struct GlowBuffer {
  uint16_t w = 0, h = 0;
  std::vector<uint16_t> v;  // saturating u16 accumulate
  void reset(uint16_t width, uint16_t height) {
    w = width;
    h = height;
    v.assign(static_cast<size_t>(width) * height, 0);
  }
};

/**
 * §5 flare_splat: one sprite splat into the glow buffer, nearest-sampled,
 * saturating u16: glow += rescale_u(sprite_u8 · alpha, 8). Coordinates in
 * GLOW-BUFFER texels (callers shift full-res px right by 2). Returns the
 * clipped texel count accumulated (0 = empty), or −1 when that count would
 * exceed `budget_left`: the splat is then dropped WHOLE (the §5 bound —
 * deterministic degradation, the far sun loses its big ghost first).
 */
int32_t flare_splat(GlowBuffer& g, const star::Sprite8& sprite, int32_t cx, int32_t cy,
                    int32_t half_x, int32_t half_y, uint8_t alpha, uint32_t budget_left);

/**
 * §5 tint at the upscale-composite: 4× nearest upscale of the glow plane,
 * per-channel additive into the RGB888 working canvas:
 * rgb_c = sat(rgb_c + rescale_u(min(glow,255) · tint_c, 8)).
 */
void glow_composite(uint8_t* rgb888, uint32_t w, uint32_t h, uint32_t vx0, uint32_t vy0,
                    uint32_t vw, uint32_t vh, const GlowBuffer& g, uint8_t tint_r, uint8_t tint_g,
                    uint8_t tint_b);

}  // namespace post

// ---- §7 procedural starfield -----------------------------------------------

namespace sky {

// Frozen transliteration of the harness oracle (tests/golden/starfield/):
// 100,000-unit sectors, ≤1 star each, signed 32×32→64 multiply-fold
// (hi+lo), & 0x1FFFF, per-axis −50000; a coordinate that lands EXACTLY on
// the 50000 sentinel before subtraction ⇒ no star (flag bit set).
inline constexpr int32_t kSectorUnits = 100000;
inline constexpr int32_t kStarCutoff = 50000;

struct SectorStar {
  int32_t x = 0, y = 0, z = 0;  // in-sector star position (units, oracle law)
  uint32_t netpos = 0;          // temp_x + temp_y + temp_z (oracle record)
  uint8_t no_star = 0;          // sentinel flags b0/b1/b2 per axis; ≠0 ⇒ none
};

/** §7 existence/position hash for sector index (kx,ky,kz) — byte-exact
 *  against tests/golden/starfield/oracle.bin BEFORE anything renders. */
SectorStar starfield(int32_t kx, int32_t ky, int32_t kz);

/** §7 rarity gate (float removed), SECTOR-INDEX domain:
 *  e = min(15, (|kx| + 30·|ky| + |kz|)/4000); skip if
 *  (kx+ky+kz) & ((1<<e)−1). The ×30 y-crush is the milky-way disc. */
bool starfield_rarity_skip(int32_t kx, int32_t ky, int32_t kz);

/** §7 magnitude: intensity6 = 63 − (rz >> 13); 0 ⇒ caller skips. rz =
 *  camera-space depth in galaxy units (wide int, clamped below at 0). */
uint8_t starfield_intensity6(int64_t rz);

}  // namespace sky

// ---- the compositor preview ([phase3-preview]) ------------------------------

namespace star {

// ---- §15 motion trails (D12, amendment v1.1) — state + laws ----------------
// (declared here, ahead of ComposeLight: a light carries its ring)

/** Ring of the last N light screen positions (§15). head = next-write index;
 *  the entry of age g (1 = most recent past position) lives at slot
 *  (head − g + N) mod N. 34 B, serialized into celestial_state. */
inline constexpr uint32_t kTrailN = 8;

struct TrailHistory {
  uint16_t x_px[kTrailN] = {};
  uint16_t y_px[kTrailN] = {};
  uint8_t head = 0;    // next-write slot
  uint8_t length = 0;  // valid entries, 0..kTrailN
};

/** §15 one authoritative source-age fade step. Noctis first removes palette
 *  bank bits, then subtracts 8 from the six-bit intensity with saturation. */
inline constexpr uint8_t trail_fade(uint8_t intensity6) {
  return intensity6 > 8 ? static_cast<uint8_t>(intensity6 - 8) : 0;
}

/** §15 push: ghosts render BEFORE the push; this records the current
 *  position as the newest history entry (overwrite evicts past N). */
inline void trail_push(TrailHistory& t, uint16_t x, uint16_t y) {
  t.x_px[t.head] = x;
  t.y_px[t.head] = y;
  t.head = static_cast<uint8_t>((t.head + 1) % kTrailN);
  if (t.length < kTrailN) ++t.length;
}

/** §15 position of the entry of age g (1..length). */
inline void trail_at(const TrailHistory& t, uint32_t g, uint16_t& x, uint16_t& y) {
  const uint32_t idx = (t.head + kTrailN - (g % kTrailN ? g % kTrailN : kTrailN)) % kTrailN;
  x = t.x_px[idx];
  y = t.y_px[idx];
}

/** A starfield glint / far glint, screen-space (caller projects wide-int). */
struct GlintPoint {
  int32_t x_px = 0, y_px = 0;
  uint8_t size_px = 1;  // 1..3 (PART.SOFT glint rung, §7)
  uint8_t intensity6 = 63;
  uint8_t rgb[3] = {255, 255, 255};  // additive colour (tint × intensity,
                                     // caller-computed via the c8 expansion)
};

/** One near star, screen-space quantities only (spec §9: ARM computes the
 *  projection in wide integers; only these cross). */
struct ComposeLight {
  int32_t x_px = 0, y_px = 0;   // light centre (canvas px, integer)
  int32_t disc_r_px = 0;        // projected disc radius (0 = no disc)
  int32_t halo_r_px = 0;        // halo half-size (0 = no halo)
  TrailHistory* trail = nullptr;   // §15: ring of past positions (persistent
                                   // across frames; pushed by compose_view)
  int32_t ghost_r_px = 0;          // §15: reconstructed corona radius (0 = off)
  uint8_t halo_core16 = 5;      // §4 variant: 0 atmo / 5 space / 8 airless
  int64_t d_milli = 0;          // distance (wide int)
  int64_t r_milli = 1;          // star radius (wide int)
  const uint8_t (*ramp)[3] = nullptr;  // 64-entry built ramp (caller slews)
  const Sprite8* face = nullptr;       // starface (disc texture)
  const Sprite8* corona = nullptr;     // corona sprite (halo texture)
  uint8_t flare_mode = 0;              // 0 none / 1 flare / 2 pulsar duty
  uint16_t spin_phase = 0;             // for the duty gate
  uint8_t tint[3] = {255, 255, 255};   // flare tint (upscale-composite)
  int32_t probe_x = 0, probe_y = 0;    // occlusion probe pixel (canvas px)
  uint8_t in_window = 1;               // §5 gating: 5r ≤ d ≤ 1000r (caller)
  uint8_t in_front = 1;                // w > wmin guard (caller)
  int32_t halo_r_max_px = kHaloRMaxZ60Px;  // Duo passes 160
};

/** §8 temporal state: flare slots (fade counter + latched tag). Ramp slew
 *  state lives in RampState; both serialize into the celestial_state chunk. */
struct FlareSlots {
  uint8_t fade_ctr[4] = {0, 0, 0, 0};
  uint8_t latched_tag[4] = {0, 0, 0, 0};
  uint16_t light_id[4] = {0, 0, 0, 0};
};

struct ComposeStats {
  uint32_t star_fragments = 0;  // disc+halo+glint pixels written (§11 budget)
  uint32_t flare_texels = 0;    // glow texels accumulated (§5 bound)
  uint32_t splats_dropped = 0;  // whole splats dropped by the budget
};

/**
 * The ZRef celestial preview: draw glints, halos (additive through pal_h),
 * discs (masked through pal_d), latch the occlusion probes, step the fade
 * counters, splat the flare chain into a ¼-res glow plane and composite it
 * tinted — into the renderer's RGB888 working canvas + Q16.16 depth plane
 * (charter §8 tile store), inside viewport (vx0,vy0,vw,vh).
 *
 * Depth law: celestial pixels test kStarDepth > depth[p] (strict greater =
 * closer, qformats §8) and never write depth (§3: Z-test on / Z-write off).
 * The effect-tag plane is transient per call (charter §8's 8-bit tag lane,
 * preview-local): glow writers set glow_tag(strength); the probe reads the
 * latched tag, never the framebuffer (§5).
 */
void compose_view(uint8_t* rgb888, int32_t* depth, uint32_t w, uint32_t h, uint32_t vx0,
                  uint32_t vy0, uint32_t vw, uint32_t vh, uint32_t tick,
                  const ComposeLight* lights, int n_lights, const GlintPoint* glints, int n_glints,
                  FlareSlots& slots, ComposeStats* stats);

// ---- §8 celestial_state capture chunk ---------------------------------------

/** The four temporal-state families of §8, fixed little-endian layout:
 *  2 ramps × 56 B (12×s16 cur + 12×s16 tgt + init + 7 reserved) +
 *  4 flare slots × 4 B (light_id u16, fade_ctr u8, latched_tag u8) +
 *  2 stars × 12 B (class u8, pad u8, spin_phase u16, radius_milli s32,
 *  texture_seed u32) + galaxy_seed u32 + camera sector 3×s32 = 168 B
 *  (the v1 layout) + 2 trail rings × 34 B (8×u16 x, 8×u16 y, head, length)
 *  = 236 B since amendment v1.1 (§15). */
struct CelestialState {
  RampState ramp[2];
  FlareSlots slots;
  struct StarSlot {
    uint8_t cls = 0;
    uint16_t spin_phase = 0;
    int32_t radius_milli = 0;
    uint32_t texture_seed = 0;
  } stars[2];
  uint32_t galaxy_seed = 0;
  int32_t cam_sector[3] = {0, 0, 0};
  TrailHistory trails[2];  // §15 motion-trail rings (per near star)
};

inline constexpr size_t kCelestialStateBytes =
    2 * 56 + 4 * 4 + 2 * 12 + 4 + 12 + 2 * 34;  // 236 (168 + 68 since v1.1)

void celestial_state_serialize(const CelestialState& st, uint8_t out[kCelestialStateBytes]);
void celestial_state_deserialize(const uint8_t in[kCelestialStateBytes], CelestialState& st);

}  // namespace star
}  // namespace zref
