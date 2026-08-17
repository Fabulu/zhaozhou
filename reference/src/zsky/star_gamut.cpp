// star_gamut.cpp — the 12-class star gamut: identity, ramp, palettes, LOD,
// glints, capture state (spec/stars_and_flares.md §2/§3/§6/§7/§8, §12).
//
// Law (citation order):
//   stars_and_flares.md §2  the class table (verbatim compiled defaults; RGB
//                           original VGA 6-bit), SPIN_K = 55, identity
//                           schedule inputs, radius_milli law, infant
//                           undertone, dead-class flare suppression
//                       §3  ramp build (control points ×4 into the s16
//                           pre-clamp domain — the file's 2026-08-16
//                           clarification; segment law + round_half_up;
//                           slew ±1/tick), boil rotation, SATUR washout
//                       §6  LOD ladder + hysteresis + glint clamp
//                       §7  identity schedule (PCG draws via the ONE
//                           noise2_hash, qformats §7.5)
//                       §8  celestial_state layout (fixed little-endian)
//   qformats.md §4          round_half_up — via zref::detail::div_rhu_s64,
//                           the ONE signed-division rounding (§29-6)
//
// Wide-integer law (stars_and_flares §14 risk 4): every d/r input is s64
// milli-units. fx16 cannot hold interstellar distances; none is used here.

#include "zref/zref_star.hpp"

namespace zref {
namespace star {

// ---- §2 the gamut (every value restated from a table row, never invented) --

const StarClass kGamut[kStarClasses] = {
    // name           class_rgb     undertone     ray    rayvar dfs smo spin fl
    {"Yellow star", {63, 58, 40}, {64, 54, 28}, 5000, 2000, 64, 2, 0, 1},
    {"Blue giant", {30, 50, 63}, {36, 50, 64}, 15000, 10000, 96, 2, 0, 1},
    {"White dwarf", {63, 63, 63}, {24, 32, 48}, 300, 200, 32, 5, 4, 1},
    {"Red giant", {63, 30, 20}, {64, 24, 12}, 20000, 15000, 51, 2, 0, 1},
    {"Orange giant", {63, 55, 32}, {64, 40, 32}, 15000, 5000, 77, 2, 0, 1},
    {"Brown dwarf", {32, 16, 10}, {28, 20, 12}, 1000, 1000, 6, 2, 0, 0},
    {"Grey giant", {32, 28, 24}, {32, 32, 32}, 3000, 3000, 6, 2, 0, 0},
    {"Blue dwarf", {10, 20, 63}, {32, 44, 64}, 2000, 500, 26, 5, 12, 1},
    {"Multiple", {63, 32, 16}, {64, 60, 32}, 4000, 5000, 58, 2, 0, 1},
    {"Infant star", {48, 32, 63}, {0, 0, 0}, 1500, 10000, 83, 2, 0, 1},  // undertone per-identity
    {"Runaway", {40, 10, 10}, {32, 26, 22}, 30000, 1000, 32, 2, 0, 0},
    {"Pulsar", {0, 63, 63}, {36, 48, 64}, 250, 10, 13, 5, 30, 2},
};

// §7 CLASS_PICK[32]: S00×7 S01×2 S02×2 S03×4 S04×4 S05×4 S06×2 S07×2 S08×2
// S09×1 S10×1 S11×1 (sums to 32).
const uint8_t kClassPick[32] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 3, 3,  4,
                                4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 7, 8, 8, 9, 10, 11};

// ---- §7 identity schedule ---------------------------------------------------

StarIdentity identity(int32_t sx, int32_t sy, int32_t sz, uint32_t galaxy_seed) {
  const uint32_t ux = static_cast<uint32_t>(sx);
  const uint32_t uy = static_cast<uint32_t>(sy);
  const uint32_t uz = static_cast<uint32_t>(sz);
  const uint32_t h0 = noise2_hash(ux, uy, uz ^ galaxy_seed, 0);
  const uint32_t h1 = noise2_hash(ux, uy, uz ^ galaxy_seed, 1);
  const uint32_t h2 = noise2_hash(h0, h1, galaxy_seed, 0);
  const uint32_t h3 = noise2_hash(h0, h1, galaxy_seed, 1);

  StarIdentity id;
  id.cls = kClassPick[h0 >> 27];
  const StarClass& c = kGamut[id.cls];
  id.radius_milli = c.ray_milli + static_cast<int32_t>(h1 % static_cast<uint32_t>(c.rayvar_milli));
  id.spin_rate = c.spin_mod == 0
                     ? 0
                     : static_cast<uint16_t>(kSpinK * (1 + static_cast<int32_t>(h2 % c.spin_mod)));
  id.spin_draw = h2;
  id.texture_seed = h3;
  if (id.cls == 9) {
    // §2 infant undertone: under6[c] = 24 + ((h3 >> (5·c)) & 31)
    for (int ch = 0; ch < 3; ++ch)
      id.under6[ch] = static_cast<uint8_t>(24 + ((h3 >> (5 * ch)) & 31));
  } else {
    for (int ch = 0; ch < 3; ++ch) id.under6[ch] = c.under6[ch];
  }
  return id;
}

// ---- §3 ramp ---------------------------------------------------------------

void ramp_points(const StarIdentity& id, int16_t out[12]) {
  const StarClass& c = kGamut[id.cls];
  // P0 = (0,0,0); P1 = undertone6×4; P2 = class6×4; P3 = (256,280,304)
  // (all ×4 into the s16 pre-clamp domain — file clarification 2026-08-16)
  const int16_t p3[3] = {256, 280, 304};
  for (int ch = 0; ch < 3; ++ch) {
    out[0 + ch] = 0;
    out[3 + ch] = static_cast<int16_t>(id.under6[ch] * 4);
    out[6 + ch] = static_cast<int16_t>(c.rgb6[ch] * 4);
    out[9 + ch] = p3[ch];
  }
}

void ramp_retarget(RampState& st, const StarIdentity& id) {
  ramp_points(id, st.tgt);
  if (!st.init) {
    for (int i = 0; i < 12; ++i) st.cur[i] = st.tgt[i];
    st.init = 1;
  }
}

void ramp_slew_step(RampState& st) {
  // §3: ±1/tick per channel toward the target — palette changes never pop
  for (int i = 0; i < 12; ++i) {
    if (st.cur[i] < st.tgt[i])
      ++st.cur[i];
    else if (st.cur[i] > st.tgt[i])
      --st.cur[i];
  }
}

void ramp_build(const int16_t pts[12], uint8_t out[64][3]) {
  // §3 segments: [0..24) P0→P1, [24..40) P1→P2, [40..64) P2→P3;
  // ramp[i] = base + round_half_up((tgt−base)·(i−a), n), clamp [0,255]
  struct Seg {
    int a, n, base, tgt;  // base/tgt = control point indices (×3 channels)
  };
  const Seg segs[3] = {{0, 24, 0, 3}, {24, 16, 3, 6}, {40, 24, 6, 9}};
  for (const Seg& s : segs) {
    for (int i = s.a; i < s.a + s.n; ++i) {
      for (int ch = 0; ch < 3; ++ch) {
        const int32_t base = pts[s.base + ch];
        const int32_t tgt = pts[s.tgt + ch];
        const int64_t v =
            base + detail::div_rhu_s64(static_cast<int64_t>(tgt - base) * (i - s.a), s.n);
        out[i][ch] = static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
      }
    }
  }
}

// ---- §3 disc palette: boil + washout ---------------------------------------

uint8_t satur_of(int64_t d_milli, int64_t r_milli) {
  if (r_milli <= 0) return 63;
  if (d_milli < 0) d_milli = 0;
  const int64_t s = (12 * d_milli) / r_milli;
  return static_cast<uint8_t>(s > 63 ? 63 : s);
}

uint8_t boil_index(uint8_t e, uint8_t rot, uint8_t satur) {
  if (e == 0) return 0;  // entry 0 stays transparent (callers skip it)
  const uint8_t idx = static_cast<uint8_t>(1 + ((e - 1 + rot) % 63));
  return idx > satur ? idx : satur;
}

void palette_disc(const uint8_t ramp[64][3], uint32_t tick, int64_t d_milli, int64_t r_milli,
                  uint8_t out[64][3]) {
  const uint8_t rot = boil_rot(tick);
  const uint8_t sat = satur_of(d_milli, r_milli);
  out[0][0] = out[0][1] = out[0][2] = 0;
  for (int e = 1; e < 64; ++e) {
    const uint8_t idx = boil_index(static_cast<uint8_t>(e), rot, sat);
    for (int ch = 0; ch < 3; ++ch) out[e][ch] = ramp[idx][ch];
  }
}

void palette_halo(const uint8_t ramp[64][3], uint8_t out[64][3]) {
  // §4: the ramp un-rotated, un-floored; [0] = black = the additive identity
  out[0][0] = out[0][1] = out[0][2] = 0;
  for (int e = 1; e < 64; ++e)
    for (int ch = 0; ch < 3; ++ch) out[e][ch] = ramp[e][ch];
}

// ---- §6 LOD ladder ---------------------------------------------------------

namespace {
// Rung from thresholds t0/t1 (S12.8 px) and the glint window bound (d vs
// bound·r) — the raw law and its hysteresis-shifted variants share this.
LodRung rung_of(int32_t radius_q8, int64_t d_milli, int64_t r_milli, int32_t t0_q8, int32_t t1_q8,
                int64_t glint_bound) {
  if (radius_q8 >= t0_q8) return LodRung::kDisc;
  if (radius_q8 >= t1_q8) return LodRung::kCorona;
  if (d_milli <= glint_bound * r_milli) return LodRung::kGlint;
  return LodRung::kPoint;
}
}  // namespace

LodRung lod_rung_raw(int32_t proj_radius_q8, int64_t d_milli, int64_t r_milli) {
  // §6: ≥6 px disc, 1.5–6 px corona, <1.5 px glint while d ≤ 1550r, beyond
  // that the starfield point. S12.8: 6 px = 1536, 1.5 px = 384.
  return rung_of(proj_radius_q8, d_milli, r_milli, 1536, 384, 1550);
}

LodRung lod_select(LodState& st, int32_t proj_radius_q8, int64_t d_milli, int64_t r_milli) {
  if (!st.init) {
    st.rung = lod_rung_raw(proj_radius_q8, d_milli, r_milli);
    st.hold = 0;
    st.init = 1;
    return st.rung;
  }
  // §6 hysteresis: each boundary shifts 10% AWAY from the current rung (a
  // switch must cross by 10%), and a switch requires the 15-tick hold.
  const int cur = static_cast<int>(st.rung);
  const int32_t t0 = cur <= static_cast<int>(LodRung::kDisc) ? 1536 - 154 : 1536 + 154;
  const int32_t t1 = cur <= static_cast<int>(LodRung::kCorona) ? 384 - 38 : 384 + 38;
  const int64_t gb = cur <= static_cast<int>(LodRung::kGlint) ? 1550 + 155 : 1550 - 155;
  const LodRung cand = rung_of(proj_radius_q8, d_milli, r_milli, t0, t1, gb);
  if (cand != st.rung && st.hold >= 15) {
    st.rung = cand;
    st.hold = 0;
  } else if (st.hold < 0xFFFF) {
    ++st.hold;
  }
  return st.rung;
}

uint8_t glint_intensity6(int64_t d_milli, int64_t r_milli) {
  // §6: 48 + clamp((1600r − d)/(100r), 0, 15) — never dimmer than 75%
  if (r_milli <= 0) return 48;
  int64_t t = (1600 * r_milli - d_milli) / (100 * r_milli);
  if (t < 0) t = 0;
  if (t > 15) t = 15;
  return static_cast<uint8_t>(48 + t);
}

// ---- §8 celestial_state (fixed little-endian layout, header doc) -----------

namespace {
void put16(uint8_t*& p, uint16_t v) {
  *p++ = static_cast<uint8_t>(v);
  *p++ = static_cast<uint8_t>(v >> 8);
}
void put32(uint8_t*& p, uint32_t v) {
  for (int i = 0; i < 4; ++i) *p++ = static_cast<uint8_t>(v >> (8 * i));
}
uint16_t take16(const uint8_t*& p) {
  const uint16_t v = static_cast<uint16_t>(p[0] | (p[1] << 8));
  p += 2;
  return v;
}
uint32_t take32(const uint8_t*& p) {
  const uint32_t v = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                     (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
  p += 4;
  return v;
}
}  // namespace

void celestial_state_serialize(const CelestialState& st, uint8_t out[kCelestialStateBytes]) {
  uint8_t* p = out;
  for (int r = 0; r < 2; ++r) {  // 56 B per ramp
    for (int i = 0; i < 12; ++i) put16(p, static_cast<uint16_t>(st.ramp[r].cur[i]));
    for (int i = 0; i < 12; ++i) put16(p, static_cast<uint16_t>(st.ramp[r].tgt[i]));
    *p++ = st.ramp[r].init;
    for (int i = 0; i < 7; ++i) *p++ = 0;  // reserved
  }
  for (int i = 0; i < 4; ++i) {  // 4 B per flare slot
    put16(p, st.slots.light_id[i]);
    *p++ = st.slots.fade_ctr[i];
    *p++ = st.slots.latched_tag[i];
  }
  for (int i = 0; i < 2; ++i) {  // 12 B per star slot
    *p++ = st.stars[i].cls;
    *p++ = 0;  // pad
    put16(p, st.stars[i].spin_phase);
    put32(p, static_cast<uint32_t>(st.stars[i].radius_milli));
    put32(p, st.stars[i].texture_seed);
  }
  put32(p, st.galaxy_seed);
  for (int i = 0; i < 3; ++i) put32(p, static_cast<uint32_t>(st.cam_sector[i]));
  for (int i = 0; i < 2; ++i) {  // §15 trail rings, 34 B each (appended; the
    // v1 168 B prefix is unchanged so old captures parse to the same prefix)
    for (uint32_t k = 0; k < kTrailN; ++k) put16(p, st.trails[i].x_px[k]);
    for (uint32_t k = 0; k < kTrailN; ++k) put16(p, st.trails[i].y_px[k]);
    *p++ = st.trails[i].head;
    *p++ = st.trails[i].length;
  }
}

void celestial_state_deserialize(const uint8_t in[kCelestialStateBytes], CelestialState& st) {
  const uint8_t* p = in;
  for (int r = 0; r < 2; ++r) {
    for (int i = 0; i < 12; ++i) st.ramp[r].cur[i] = static_cast<int16_t>(take16(p));
    for (int i = 0; i < 12; ++i) st.ramp[r].tgt[i] = static_cast<int16_t>(take16(p));
    st.ramp[r].init = *p++;
    p += 7;  // reserved
  }
  for (int i = 0; i < 4; ++i) {
    st.slots.light_id[i] = take16(p);
    st.slots.fade_ctr[i] = *p++;
    st.slots.latched_tag[i] = *p++;
  }
  for (int i = 0; i < 2; ++i) {
    st.stars[i].cls = *p++;
    ++p;  // pad
    st.stars[i].spin_phase = take16(p);
    st.stars[i].radius_milli = static_cast<int32_t>(take32(p));
    st.stars[i].texture_seed = take32(p);
  }
  st.galaxy_seed = take32(p);
  for (int i = 0; i < 3; ++i) st.cam_sector[i] = static_cast<int32_t>(take32(p));
  for (int i = 0; i < 2; ++i) {  // §15 trail rings (v1.1 amendment tail)
    for (uint32_t k = 0; k < kTrailN; ++k) st.trails[i].x_px[k] = take16(p);
    for (uint32_t k = 0; k < kTrailN; ++k) st.trails[i].y_px[k] = take16(p);
    st.trails[i].head = *p++;
    st.trails[i].length = *p++;
  }
}

}  // namespace star
}  // namespace zref
