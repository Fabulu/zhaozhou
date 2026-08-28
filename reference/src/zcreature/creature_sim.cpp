// creature_sim.cpp — the alive laws (creature_rules 4.2) and the reference
// compositor preview: hard-cut clip clock + event tags, rotateOnGround
// slope tilt, bulk inflation, tick-skip slow-motion, the LOD ladder, and
// gib-to-particles.
//
// Spec: spec/creature_rules.md 2.1 (2 ticks per key, hard cuts, events),
// 4.2 (tilt/bulk/tick-skip), 7 (LOD mandatory); charter 9 (hysteresis +
// minimum hold), 10; spec/qformats.md 3/4/7.4/7.5. The compositor draws
// through zrender's OWN raster + projection + flat-shade law (internal.hpp
// — charter 29-6: the 8 edge-function law is never reimplemented here).
// Integer-only.

#include "zref/zref_creature.hpp"
#include "zref/zref_trig.hpp"

#include "../zrender/internal.hpp"

#include <algorithm>
#include <cstring>

namespace zref {
namespace creature {

// ------------------------------------------------------------ anim clock ----

void anim_advance(AnimPlayer& a, const ClipBank& bank, const ClipEvent** fired,
                  uint8_t& fired_count) {
  fired_count = 0;
  *fired = nullptr;
  if (a.frozen) return;  // petrify: the clip clock stops (4.2)
  const Clip* clip = nullptr;
  for (const Clip& c : bank.clips)
    if (c.slot_id == a.slot) clip = &c;
  if (clip == nullptr || clip->frame_count == 0) return;
  if (++a.sub < 2) return;  // each key shown 2 sim ticks (2.1)
  a.sub = 0;
  const uint16_t next = static_cast<uint16_t>(a.frame + 1);
  // hold_last (run 0326): a one-shot clip (death) HOLDS its final key
  // instead of wrapping -- the sim previously flashed key 0 on the tick
  // after a corpse finished dying. Loops are untouched.
  a.frame = next >= clip->frame_count
                ? (clip->hold_last ? static_cast<uint16_t>(clip->frame_count - 1) : 0)
                : next;
  // events fire on ENTERING a tagged frame (the wrap into frame 0 included)
  for (const ClipEvent& e : clip->events) {
    if (e.frame != a.frame) continue;
    if (fired_count == 0) *fired = &e;
    ++fired_count;
  }
}

// --------------------------------------------------------- rotateOnGround --

namespace {
// tap the composed lattice once; hold the previous slope contribution when
// the column is absent/void (a creature stepping off the island does not
// snap flat)
struct TapResult {
  bool ok = false;
  int32_t h = 0;
};
TapResult tap(const terrain::ComposedLattice& lat, fx16 x, fx16 z, SatLedger* L) {
  TapResult t;
  const terrain::ColumnResult c = terrain::column_query(lat, x, z);
  if (c.cls == terrain::ColumnClass::kSolid) {
    t.ok = true;
    t.h = c.top.raw;
  }
  (void)L;
  return t;
}
}  // namespace

void ground_tilt_update(GroundTilt& t, TiltMode mode, angle16 facing,
                        const terrain::ComposedLattice& lat, fx16 x, fx16 z, fx16 tap_dist,
                        fx16 max_step) {
  SatLedger* L = nullptr;
  const int32_t c = fx_cos(facing).raw;
  const int32_t s = fx_sin(facing).raw;
  const int32_t d = tap_dist.raw;
  // two taps per axis: +-facing (slope_f), +-side (slope_s). One exact
  // division each (the sim-truth tier; 4.2 "2 taps + a clamp").
  int32_t tgt_f = t.slope_f, tgt_s = t.slope_s;
  {
    const TapResult hf = tap(lat, fx16{x.raw + rescale_s32(static_cast<int64_t>(c) * d, 16, L)},
                             fx16{z.raw + rescale_s32(static_cast<int64_t>(s) * d, 16, L)}, L);
    const TapResult hb = tap(lat, fx16{x.raw - rescale_s32(static_cast<int64_t>(c) * d, 16, L)},
                             fx16{z.raw - rescale_s32(static_cast<int64_t>(s) * d, 16, L)}, L);
    if (hf.ok && hb.ok) {
      tgt_f = fx_div_exact(fx16{hf.h - hb.h}, fx16{2 * d}, L).raw;
    }
  }
  {
    const TapResult hs = tap(lat, fx16{x.raw - rescale_s32(static_cast<int64_t>(s) * d, 16, L)},
                             fx16{z.raw + rescale_s32(static_cast<int64_t>(c) * d, 16, L)}, L);
    const TapResult ho = tap(lat, fx16{x.raw + rescale_s32(static_cast<int64_t>(s) * d, 16, L)},
                             fx16{z.raw - rescale_s32(static_cast<int64_t>(c) * d, 16, L)}, L);
    if (hs.ok && ho.ok) {
      tgt_s = fx_div_exact(fx16{hs.h - ho.h}, fx16{2 * d}, L).raw;
    }
  }
  if (mode == TiltMode::kNone) {
    tgt_f = 0;
    tgt_s = 0;
  } else if (mode == TiltMode::kSideways) {
    tgt_f = 0;  // roll only (bipeds keep pitch upright)
  }
  // rate limit: clamp the per-tick change (the donor's rate-limited slerp
  // in slope space — stated honestly as the integer equivalent)
  const auto step_toward = [&](int32_t cur, int32_t tgt) -> int32_t {
    int32_t d = tgt - cur;
    const int32_t lim = max_step.raw;
    if (d > lim) d = lim;
    if (d < -lim) d = -lim;
    return cur + d;
  };
  t.slope_f = step_toward(t.slope_f, tgt_f);
  t.slope_s = step_toward(t.slope_s, tgt_s);
}

mat3x4fx tilt_matrix(const GroundTilt& t, SatLedger* L) {
  // R = I + [a]x + [a]x^2 / (1 + n.y) with a = y_hat x n (UNnormalized —
  // the standard unnormalized-axis Rodrigues form maps y_hat onto n
  // exactly; derivation in the header). n = normalize(-sf, 1, -ss) via the
  // shared 7.4 normalize.
  const vec3fx n = normalize3_approx(vec3fx{fx16{-t.slope_f}, fx16{1 << 16}, fx16{-t.slope_s}}, L);
  const int32_t ax = n.z.raw, ay = 0, az = -n.x.raw;  // y_hat x n
  const int32_t one_plus_c = (1 << 16) + n.y.raw;
  const int32_t k = one_plus_c >= 64 ? field_rcp(fx16{one_plus_c}, L).raw : 0;  // c ~ -1 pinned 0
  const int64_t a2 = static_cast<int64_t>(ax) * ax + static_cast<int64_t>(az) * az;
  mat3x4fx r = mat3x4_identity();
  // cross term (exact adds) + K*(a_i a_j - |a|^2 delta) (one rounding each)
  const int64_t prod[3][3] = {
      {static_cast<int64_t>(ax) * ax, static_cast<int64_t>(ax) * ay, static_cast<int64_t>(ax) * az},
      {static_cast<int64_t>(ay) * ax, static_cast<int64_t>(ay) * ay, static_cast<int64_t>(ay) * az},
      {static_cast<int64_t>(az) * ax, static_cast<int64_t>(az) * ay,
       static_cast<int64_t>(az) * az}};
  const int32_t cross[3][3] = {{0, -az, ay}, {az, 0, -ax}, {-ay, ax, 0}};
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      const int64_t t_ij = prod[i][j] - (i == j ? a2 : 0);
      // Q-algebra: k is Q16.16 raw, t_ij is a Q32.32 product of fx16 lanes,
      // so k*t_ij is Q48.48 -> rescale(.,32) lands in Q16.16 (the first
      // draft shifted 16 and produced elements 2^16 too large — the test's
      // orthogonality anchor caught it).
      const int32_t rod = rescale_s32(static_cast<int64_t>(k) * t_ij, 32, L, &SatLedger::mul);
      r.m[i * 4 + j] = rod + cross[i][j] + (i == j ? (1 << 16) : 0);
    }
  }
  return r;
}

// ------------------------------------------------------------------ bulk ----

void bulk_update(BulkState& b, uint8_t rate_shift) {
  const int32_t d = b.target - b.scale;
  b.scale += d >> rate_shift;  // arithmetic shift (floor) — deterministic
}

// ------------------------------------------------------------------- LOD ----

// ---------------------------------------------------------------------------
// NO QUOTIENT IS EVER MATERIALISED HERE, AND THAT IS A CORRECTNESS FIX.
//
// This code used to compute the two quantities the ladder is described in terms
// of — the rung error and the rung boundary — and compare them. Both were
// computed in __int128 and then NARROWED TO int32, and the boundary
//
//     B_r = round_half_up(thresh * R / e_r)
//
// exceeds int32 for perfectly reachable inputs: a small `micro_error` (an
// accurately decimated rung) with a large threshold is enough. The narrowing
// wrapped it NEGATIVE, and a negative boundary makes the eager-coarsen test
// `10*proj <= 9*B` false for every projected radius — so the ladder REFUSED TO
// COARSEN and the creature stayed pinned at a fine rung forever. Found by the
// random lane of tests/differential/geom_lod_directed.cpp at
// R = 59353, thresh = 40818, e = 1, proj = 339695, and ruled on by the owner on
// 2026-08-22: fix the law, do not bake the wrap into hardware.
//
// WIDENING THE BOUNDARY TO int64 IS NOT ENOUGH, which is the part worth saying
// out loud. The quotient can approach 2^62, and the tests then multiply it by 9
// or 11 — so `bnd * 9` overflows signed 64-bit and the bug comes back wearing a
// wider type.
//
// So the boundary is never formed at all. Every use of a quotient here is a
// COMPARISON against an integer, and for integers N >= 0, e > 0 and integer T:
//
//     floor(N/e) <= T   <=>   N < (T+1)*e
//     floor(N/e) >= T   <=>   N >= T*e
//
// Both are exact identities, so each division becomes a multiplication, and
// every intermediate stays in __int128 where nothing can wrap. This is also the
// same predicate `fpga/rtl/geometry/zhao_geom_lod.sv` evaluates, which is the
// architectural point: the reference and the RTL now implement one mathematical
// statement, rather than the reference dividing and the hardware proving an
// equivalent comparison by a different route.
//
// The identities need a NON-NEGATIVE numerator (C++ integer division truncates
// toward zero, which equals floor only there). That is the domain: a projected
// radius, three error magnitudes, and a bound radius from `isqrt_u64`.
// ---------------------------------------------------------------------------

LodRung lod_raw(int32_t proj_radius_q8, int32_t thresh_q8, const CreatureType& type) {
  const __int128 R = type.bound_radius;
  const __int128 p = proj_radius_q8;
  // err_r <= thresh  <=>  proj*e_r + R/2 < (thresh + 1) * R
  // e_r == 0 needs no special case: it gives R/2 < (thresh+1)*R, which agrees
  // with the old `err = 0` short-circuit on both signs of thresh.
  const __int128 rhs = (static_cast<__int128>(thresh_q8) + 1) * R;
  const __int128 e[4] = {0, type.micro_error, type.splat_error, type.glint_error};
  // coarsest legal rung (fewest pixels that still meets the error budget)
  for (int r = 3; r >= 1; --r) {
    if (p * e[r] + R / 2 < rhs) return static_cast<LodRung>(r);
  }
  return LodRung::kMesh;
}

LodRung lod_update(LodState& st, int32_t proj_radius_q8, int32_t thresh_q8,
                   const CreatureType& type) {
  const LodRung raw = lod_raw(proj_radius_q8, thresh_q8, type);
  if (raw == st.rung) {
    if (st.hold < 0xFFFF) ++st.hold;
    return st.rung;
  }
  if (st.hold < kLodHoldTicks) {
    ++st.hold;  // minimum hold not elapsed: stay (charter 9)
    return st.rung;
  }
  // The boundary between rung r and its FINER neighbour is the projected radius
  // at which rung r's error equals the threshold, B_r = thresh * R / e_r. It is
  // NEVER FORMED — see the note above; forming it is what wrapped. Each test is
  // cross-multiplied instead, exactly, in __int128.
  const __int128 e[4] = {0, type.micro_error, type.splat_error, type.glint_error};
  const __int128 e_sel = e[static_cast<int>(raw > st.rung ? raw : st.rung)];
  // N = thresh*R + e/2, the boundary's numerator before the divide not taken
  const __int128 N = static_cast<__int128>(thresh_q8) * type.bound_radius + e_sel / 2;
  const __int128 p10 = static_cast<__int128>(proj_radius_q8) * 10;

  bool switch_ok = false;
  if (e_sel == 0) {
    // The boundary is defined as zero when the rung has no error term, so the
    // two tests degenerate to `10*proj <= 0` and `10*proj >= 0`. Kept explicit
    // because the identities above were derived under e > 0.
    switch_ok = (raw > st.rung) ? (proj_radius_q8 <= 0) : true;
  } else if (raw > st.rung) {
    // coarsening: eager — 10% BELOW the boundary of the target rung.
    //   10*proj <= 9*B  <=>  B >= ceil(10*proj/9)  <=>  N >= ceil(10*proj/9)*e
    const __int128 k = (p10 + 8) / 9;  // ceil, valid for a non-negative numerator
    switch_ok = (N >= k * e_sel);
  } else {
    // refining: lazy — 10% ABOVE the boundary of the CURRENT (coarser) rung.
    //   10*proj >= 11*B  <=>  B <= floor(10*proj/11)  <=>  N < (floor+1)*e
    const __int128 m = p10 / 11;
    switch_ok = (N < (m + 1) * e_sel);
  }
  if (switch_ok) {
    st.rung = raw;
    st.hold = 0;
  } else {
    if (st.hold < 0xFFFF) ++st.hold;
  }
  return st.rung;
}

// ------------------------------------------------------------------ gibs ----

void spawn_gibs(const CreatureType& type, const mat3x4fx* palette, fx16 wx, fx16 wy, fx16 wz,
                uint32_t seed, std::vector<Gib>& out) {
  SatLedger* L = nullptr;
  uint32_t emitted = 0;
  for (const Meshlet& m : type.mesh) {
    for (size_t vi = 0; vi < m.verts.size() && emitted < 64; ++vi) {
      const SkinVertex& v = m.verts[vi];
      // world position through the pose in force at the pop
      int32_t px, py, pz;
      skin_vertex(palette, v, px, py, pz, L);
      const uint32_t h0 = noise2_hash(static_cast<uint32_t>(vi), seed, 0xB00B1E5u, 0);
      const uint32_t h1 = noise2_hash(static_cast<uint32_t>(vi), seed, 0xB00B1E5u, 1);
      // direction lanes: (h>>16) as signed 16 -> fx16 in [-1, 1)
      const auto s16lane = [](uint32_t h) -> int32_t {
        return static_cast<int32_t>(static_cast<int16_t>(h >> 16));
      };
      Gib g;
      g.x = px + wx.raw;
      g.y = py + wy.raw;
      g.z = pz + wz.raw;
      g.vx = s16lane(h0) * 2;                 // fx16 raw: |v| < 2.0
      g.vy = (1 << 16) + (s16lane(h1) >> 1);  // up bias 1.0 +- 0.5
      g.vz = s16lane(h1 << 8) * 2;
      g.size = static_cast<uint8_t>(2 + (h1 & 3));  // 2..5 (U 0.4.4 px)
      g.r = static_cast<uint8_t>((m.r * 200 + 128) >> 8);
      g.g = static_cast<uint8_t>((m.g * 200 + 128) >> 8);
      g.b = static_cast<uint8_t>((m.b * 200 + 128) >> 8);
      out.push_back(g);
      ++emitted;
    }
    if (emitted >= 64) break;
  }
}

// ------------------------------------------------------------- sim tick -----

bool creature_update(CreatureInstance& inst, const SimParams& sp,
                     const terrain::ComposedLattice* lat, uint32_t tick) {
  if (!tick_skip_due(tick, sp.skip_shift)) return false;
  const CreatureType& T = *inst.type;
  const ClipEvent* fired = nullptr;
  uint8_t fired_n = 0;
  anim_advance(inst.anim, T.bank, &fired, fired_n);
  (void)fired;
  (void)fired_n;
  if (lat != nullptr) {
    ground_tilt_update(inst.tilt, inst.tilt_mode, inst.facing, *lat, fx16{inst.x}, fx16{inst.z},
                       sp.tap_dist, sp.tilt_step);
  }
  bulk_update(inst.bulk, 4);
  return true;
}

// ----------------------------------------------------- compositor preview --

namespace {

// quantize a Q16.16 lambert weight to 16 levels (palette law: a creature
// contributes <= 17 shades per material; the tool counts and enforces)
inline int32_t quant_shade(int32_t shade) {
  int32_t q = (shade + 0x800) >> 12 << 12;
  if (q < 0) q = 0;
  if (q > 0x10000) q = 0x10000;
  return q;
}

// ---------------------------------------------------------------------------
// THE CREATURE LIGHT RIG (rewritten 2026-08-26)
//
// It used to be one white key light and a grey scalar ambient:
//     s = 0.25 + 0.75 * lambert;   pixel_c = authored_c * s
//
// That is a MULTIPLY-ONLY model with a single scalar applied to R, G and B
// alike, and it has two measured consequences (FINDINGS-R1 D.3):
//
//   1. On a body of revolution lit from one side, SIX OF TWELVE FACES -- a
//      full half of the surface -- land on the identical ambient floor. They
//      are not shaded; they are one flat dark colour with no form at all.
//
//   2. A scalar multiply preserves the hue RATIO exactly while collapsing
//      ABSOLUTE chroma toward zero. The concept's dorsal pink has a channel
//      spread of 23 counts at full light and SIX at the floor. Six of 255 is
//      not a colour, it is grey -- and that is precisely the "grey helmet"
//      the first Zixxtrixx pass reported, which was then "fixed" by pushing
//      the artwork's saturation. The artwork was never the problem.
//
// So: a white KEY, a cool FILL from the opposite side, and a small per-channel
// AMBIENT. The fill is what gives the shadow side form; the fill and ambient
// being COOL and PER-CHANNEL is what stops a pastel from going grey, because
// the shadow now ADDS blue instead of only subtracting everything equally.
//
// MODELINGGUIDE:249 asked for exactly this -- "Do not solve pastel colours
// becoming muddy merely by pushing saturation harder. First correct the
// material response, lighting and texture."
//
// Still deliberately cheap: two dot products and three multiply-adds per
// triangle, flat per face, no specular, no shadows. Nothing here needs
// silicon; this is the reference renderer's material response.
// ---------------------------------------------------------------------------

// The rig below was SOLVED, not chosen: a 120,000-point sweep over fill
// direction, key strength, ambient tint and fill tint, scored on (a) no face
// darker than the shadow floor, (b) worst-case chroma spread of the concept's
// dorsal pink, (c) distinct face values across a body of revolution, and
// (d) a near-neutral highlight. `tools/tune_creature_light.py` reproduces it.
//
// Measured against the old one-light model, over 36 faces of a horizontal
// cylinder, with the concept's raw dorsal pink (233,188,206):
//
//                          OLD      NEW
//   darkest face gain      0.250 -> 0.375   (the shadow side stops being a
//                                            flat silhouette)
//   distinct face values   7     -> 21      (form instead of six identical
//                                            faces)
//   worst chroma spread    11    -> 17      (+55%; the pastel survives)
//   brightest face         0.938 -> 1.000   (unity was unreachable before)
//   highlight neutrality   n/a   -> exact   (a white key reads white)
//
// The fill came out as a WARM bounce from BELOW under a cool neutral ambient.
// That is ground light off the ochre terrain plus sky -- the sweep was not
// told to prefer it; it scored best. Physical coherence for free.
inline constexpr int32_t kFillX = -14301;  // -0.21822
inline constexpr int32_t kFillY = -57205;  // -0.87287
inline constexpr int32_t kFillZ = -28602;  // -0.43644

inline constexpr int32_t kAmbR = 22282, kAmbG = 23265, kAmbB = 24248;     // .34 .355 .37
inline constexpr int32_t kKey = 48497;                                    // .74, white
inline constexpr int32_t kFillR = 19661, kFillG = 15073, kFillB = 10486;  // .30 .23 .16

struct Shade3 {
  int32_t r, g, b;  // Q16.16 gain per channel
};

// The smooth/face mix, in 1/1024 (N3, owner direction: keep a blend knob —
// 100% smooth normals erase the hand-cut low-poly read entirely). 819/1024
// = 0.8 of the per-vertex smooth Lambert + 0.2 of the face Lambert at each
// corner, applied to key and fill alike, BEFORE the rig composes gains.
inline constexpr int32_t kSmoothMixNum = 819;

// Compose the rig into a per-channel gain. Each channel is quantised on the
// same 1/16 ladder the palette tool counts, so the shade COUNT per material is
// unchanged -- what changes is that the three channels no longer move
// together, which is the whole point.
inline Shade3 creature_light(int32_t lam_key, int32_t lam_fill) {
  const auto mix = [](int32_t amb, int32_t fill, int32_t lk, int32_t lf) {
    const int64_t k = (static_cast<int64_t>(kKey) * lk) >> 16;
    const int64_t f = (static_cast<int64_t>(fill) * lf) >> 16;
    return quant_shade(static_cast<int32_t>(amb + k + f));
  };
  return Shade3{mix(kAmbR, kFillR, lam_key, lam_fill), mix(kAmbG, kFillG, lam_key, lam_fill),
                mix(kAmbB, kFillB, lam_key, lam_fill)};
}

// the ambient floor of the dual-terrain walls (0.25 + 0.75*lambert) -- kept
// because the gib/debris path still uses the single-scalar form
inline int32_t ambient_floor(int32_t shade) {
  return 16384 + static_cast<int32_t>((static_cast<int64_t>(shade) * 49152 + 32768) >> 16);
}

inline uint8_t sat_u8(int32_t v) { return static_cast<uint8_t>(v > 255 ? 255 : (v < 0 ? 0 : v)); }

}  // namespace

DebugShade g_debug_shade = DebugShade::kOff;

void compose_creatures(uint8_t* rgb, int32_t* depth, uint32_t w, uint32_t h, const mat4fx& vp,
                       CreatureInstance* const* instances, size_t count, PoseBank& poses,
                       SatLedger* L) {
  if (count == 0) return;
  // deterministic order: sort the pointers (the ABI order is caller truth;
  // the compositor must not depend on it)
  std::vector<CreatureInstance*> inst(instances, instances + count);
  std::sort(inst.begin(), inst.end());

  render::WorkSurface surf;
  surf.w = w;
  surf.h = h;
  surf.rgb.assign(rgb, rgb + static_cast<size_t>(w) * h * 3);
  surf.depth.assign(depth, depth + static_cast<size_t>(w) * h);
  const render::Viewport vpp{0, 0, w, h};

  // the per-camera pixel-error threshold (The Measure input; 2 px S12.8 —
  // the reference constant, a Phase-8 contract field when GEOM freezes it)
  const int32_t thresh_q8 = 2 * 256;

  for (CreatureInstance* ip : inst) {
    CreatureInstance& ci = *ip;
    if (!ci.visible || ci.type == nullptr) continue;
    const CreatureType& T = *ci.type;

    // ---- projected bound radius (S12.8 px): clip.x = kx*x/2^16, so
    // ndc_r = kx*R/(2^16*w); the viewport maps ndc -> px with half-extent
    // W/2 (project_vertex, rast.cpp), so
    //   radius_q8 = kx*R*W*128 / (w << 16)   — ONE round_half_up division.
    const vec4fx clip = mat4_vec4(vp, vec4fx{fx16{ci.x}, fx16{ci.y}, fx16{ci.z}, fx16{1 << 16}}, L);
    if (clip.w.raw <= 0) continue;  // behind the eye: whole creature
    const int32_t kx = std::max(std::max(std::abs(vp.m[0][0].raw), std::abs(vp.m[0][1].raw)),
                                std::abs(vp.m[0][2].raw));
    const __int128 rnum = static_cast<__int128>(kx) * T.bound_radius * w * 128;
    const __int128 rden = static_cast<__int128>(clip.w.raw) << 16;
    const int32_t radius_q8 = static_cast<int32_t>((rnum + rden / 2) / rden);

    lod_update(ci.lod, radius_q8, thresh_q8, T);

    // ---- world transform: T(x,y,z) * RotY(facing) * tilt * bulk-scale —
    // or, with the ChoreoRoot armed, T(x,y,z) * R(orient) * bulk-scale:
    // the full-3D per-instance orientation that owns trajectory and spin
    // while the clips keep local body shape (C1; see CreatureInstance).
    mat3x4fx local{};
    if (ci.choreo) {
      quat16_to_mat3(ci.orient, local, L);
    } else {
      const int32_t fc = fx_cos(ci.facing).raw;
      const int32_t fs = fx_sin(ci.facing).raw;
      mat3x4fx roty{{fc, 0, fs, 0, 0, 1 << 16, 0, 0, -fs, 0, fc, 0}};
      const mat3x4fx tilt = tilt_matrix(ci.tilt, L);
      mat3x4_mul(roty, tilt, local, L);
    }
    const int32_t sc = ci.bulk.scale;
    mat3x4fx world = local;
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        world.m[i * 4 + j] =
            rescale_s32(static_cast<int64_t>(local.m[i * 4 + j]) * sc, 16, L, &SatLedger::mul);
      }
    }
    world.m[3] = ci.x;
    world.m[7] = ci.y;
    world.m[11] = ci.z;

    if (ci.lod.rung == LodRung::kGlint || ci.lod.rung == LodRung::kSplat) {
      // project the centre; draw a fixed-depth billboard (splat) or point
      const render::ProjOut pc =
          render::project_vertex(vp, vpp, fx16{ci.x}, fx16{ci.y}, fx16{ci.z}, L);
      if (!pc.in) continue;
      render::ScreenV a = pc.s, b = pc.s, c = pc.s, d = pc.s;
      render::TriMode tm;  // depth-tested against terrain, writes depth
      const int32_t r8 = ci.lod.rung == LodRung::kSplat
                             ? std::max(radius_q8, 2 * 256)  // >= 2 px reads
                             : 128;                          // 1 px half-extent
      const int32_t x0 = pc.s.x - r8, x1 = pc.s.x + r8;
      const int32_t y0 = pc.s.y - r8, y1 = pc.s.y + r8;
      a.x = x0;
      a.y = y0;
      b.x = x1;
      b.y = y0;
      c.x = x1;
      c.y = y1;
      d.x = x0;
      d.y = y1;
      uint8_t cr, cg, cb;
      if (ci.lod.rung == LodRung::kSplat && !T.mesh.empty()) {
        const Meshlet& m0 = T.mesh.front();
        cr = static_cast<uint8_t>((m0.r * 150 + 128) >> 8);
        cg = static_cast<uint8_t>((m0.g * 150 + 128) >> 8);
        cb = static_cast<uint8_t>((m0.b * 150 + 128) >> 8);
      } else {
        cr = cg = cb = 235;  // the faction glint: bright neutral point
      }
      render::raster_tri(surf, vpp, a, b, c, cr, cg, cb, tm);
      render::raster_tri(surf, vpp, a, c, d, cr, cg, cb, tm);
      continue;
    }

    // ---- mesh / micro: pose -> world palette -> skin -> project -> raster
    const mat3x4fx* pose = poses.acquire(T, ci.anim.slot, ci.anim.frame, ci.anim.sub);
    std::array<mat3x4fx, kMaxBones> worldm{};
    for (int b = 0; b < T.bank.bone_count; ++b) {
      mat3x4_mul(world, pose[b], worldm[b], L);
    }

    // ---- PER-VERTEX LIGHTING (N3): pull each light back into BIND space,
    // once per bone, instead of rotating every normal into the world.
    //   N · (R_b^T L) == (R_b N) · L, and worldm's rotation block is the
    // pose rotation TIMES the uniform bulk scale, so each pulled-back
    // component is divided by the scale to restore a unit direction — one
    // §4 round-half-up division per component, 6 per bone, per light.
    // The per-vertex Lambert is then the SKIN-WEIGHT BLEND OF THE TWO
    // BONES' CLAMPED RESPONSES (the N5 probe's option (b), chosen as the
    // law: no renormalisation, no normal lane through the skin datapath —
    // the cheap form the silicon increment would build).
    struct BoneLight {
      int32_t kx, ky, kz;  // key dir in bone bind space, Q16.16
      int32_t fx, fy, fz;  // fill dir likewise
    };
    std::array<BoneLight, kMaxBones> blight{};
    const int32_t bulk = ci.bulk.scale > 0 ? ci.bulk.scale : 1 << 16;
    for (int b = 0; b < T.bank.bone_count; ++b) {
      const mat3x4fx& M = worldm[b];
      const auto pull = [&](int32_t lx, int32_t ly, int32_t lz, int col) {
        const __int128 p = static_cast<__int128>(M.m[col]) * lx +
                           static_cast<__int128>(M.m[col + 4]) * ly +
                           static_cast<__int128>(M.m[col + 8]) * lz;
        // rescale(.,16) puts the transpose product back in Q16.16 (times the
        // bulk scale); the division removes the scale. Two roundings total,
        // each a §4 round-half-up.
        const int32_t scaled = rescale_s32(p, 16, L, &SatLedger::mul);
        return render::div_rhu_s128(static_cast<__int128>(scaled) << 16, bulk);
      };
      blight[b].kx = pull(render::kLightX, render::kLightY, render::kLightZ, 0);
      blight[b].ky = pull(render::kLightX, render::kLightY, render::kLightZ, 1);
      blight[b].kz = pull(render::kLightX, render::kLightY, render::kLightZ, 2);
      blight[b].fx = pull(kFillX, kFillY, kFillZ, 0);
      blight[b].fy = pull(kFillX, kFillY, kFillZ, 1);
      blight[b].fz = pull(kFillX, kFillY, kFillZ, 2);
    }
    // one bone's clamped Lambert of a packed S1.7 normal: dot in s64,
    // /127 with ONE round-half-up, clamp to [0, 1<<16]
    const auto bone_lambert = [](const int8_t nx, const int8_t ny, const int8_t nz,
                                 const int32_t lx, const int32_t ly, const int32_t lz) {
      const int64_t dot = static_cast<int64_t>(nx) * lx + static_cast<int64_t>(ny) * ly +
                          static_cast<int64_t>(nz) * lz;
      if (dot <= 0) return 0;
      int32_t lam = static_cast<int32_t>((dot + 63) / 127);
      return lam > 65536 ? 65536 : lam;
    };
    const std::vector<Meshlet>& mset = ci.lod.rung == LodRung::kMicro ? T.micro : T.mesh;
    for (const Meshlet& m : mset) {
      struct PV {
        render::ScreenV s;
        bool in;
        int32_t wx, wy, wz;
        int32_t lam_k, lam_f;  // per-vertex clamped Lamberts (Q16.16)
        bool lit;              // vertex carries a compiled normal
        int8_t nx, ny, nz;     // the packed bind normal (diagnostic viz)
      };
      std::vector<PV> pvs(m.verts.size());
      for (size_t vi = 0; vi < m.verts.size(); ++vi) {
        const SkinVertex& sv = m.verts[vi];
        skin_vertex(worldm.data(), sv, pvs[vi].wx, pvs[vi].wy, pvs[vi].wz, L);
        const render::ProjOut po = render::project_vertex(vp, vpp, fx16{pvs[vi].wx},
                                                          fx16{pvs[vi].wy}, fx16{pvs[vi].wz}, L);
        pvs[vi].s = po.s;
        // UVs: SkinVertex carries u/v as 0..255, ScreenV wants Q16.16 TILE
        // units, so u8 << 8 puts one full wrap across exactly one tile.
        pvs[vi].s.u = static_cast<int32_t>(sv.u) << 8;
        pvs[vi].s.v = static_cast<int32_t>(sv.v) << 8;
        pvs[vi].in = po.in;
        // N3: per-vertex Lambert = skin-weight blend of the two bones'
        // clamped responses (see the law at blight above); (0,0,0) = the
        // vertex predates normals -> flat fallback for its triangles
        pvs[vi].lit = sv.nx != 0 || sv.ny != 0 || sv.nz != 0;
        pvs[vi].nx = sv.nx;
        pvs[vi].ny = sv.ny;
        pvs[vi].nz = sv.nz;
        if (pvs[vi].lit) {
          const BoneLight& A = blight[sv.b0];
          const BoneLight& B = blight[sv.b1];
          const int32_t w0 = sv.w0, w1 = 64 - sv.w0;
          pvs[vi].lam_k = static_cast<int32_t>(
              (static_cast<int64_t>(w0) * bone_lambert(sv.nx, sv.ny, sv.nz, A.kx, A.ky, A.kz) +
               static_cast<int64_t>(w1) * bone_lambert(sv.nx, sv.ny, sv.nz, B.kx, B.ky, B.kz) +
               32) >>
              6);
          pvs[vi].lam_f = static_cast<int32_t>(
              (static_cast<int64_t>(w0) * bone_lambert(sv.nx, sv.ny, sv.nz, A.fx, A.fy, A.fz) +
               static_cast<int64_t>(w1) * bone_lambert(sv.nx, sv.ny, sv.nz, B.fx, B.fy, B.fz) +
               32) >>
              6);
        } else {
          pvs[vi].lam_k = pvs[vi].lam_f = 0;
        }
      }
      for (size_t ti = 0; ti + 2 < m.idx.size(); ti += 3) {
        const PV& a = pvs[m.idx[ti]];
        const PV& b = pvs[m.idx[ti + 1]];
        const PV& c = pvs[m.idx[ti + 2]];
        if (!a.in || !b.in || !c.in) continue;  // Phase-3 near-plane law
        const int32_t lam_key =
            render::shade_flat_tri(a.wx, a.wy, a.wz, b.wx, b.wy, b.wz, c.wx, c.wy, c.wz, L);
        const int32_t lam_fill = render::shade_flat_tri_dir(
            a.wx, a.wy, a.wz, b.wx, b.wy, b.wz, c.wx, c.wy, c.wz, kFillX, kFillY, kFillZ, L);
        render::TriMode tm;  // opaque: depth test + write
        // GOURAUD (N3): when the compiled mesh carries normals, each corner
        // gets its own Lambert — kSmoothMixNum parts the per-vertex smooth
        // response, the rest the face response (the owner's smooth/face
        // blend knob: 100% smooth erases the hand-cut read entirely) — and
        // the rig's per-channel gains ride the interpolated colour lanes.
        // A mesh with no normals takes the flat path bit-identically.
        const bool gouraud = a.lit && b.lit && c.lit;
        Shade3 shc[3];
        if (gouraud) {
          const PV* corner[3] = {&a, &b, &c};
          for (int k = 0; k < 3; ++k) {
            const int32_t lk =
                static_cast<int32_t>((static_cast<int64_t>(kSmoothMixNum) * corner[k]->lam_k +
                                      static_cast<int64_t>(1024 - kSmoothMixNum) * lam_key + 512) >>
                                     10);
            const int32_t lf = static_cast<int32_t>(
                (static_cast<int64_t>(kSmoothMixNum) * corner[k]->lam_f +
                 static_cast<int64_t>(1024 - kSmoothMixNum) * lam_fill + 512) >>
                10);
            shc[k] = creature_light(lk, lf);
          }
          tm.gouraud = true;
        } else {
          shc[0] = shc[1] = shc[2] = creature_light(lam_key, lam_fill);
        }
        // ---- DIAGNOSTIC SHADE MODES (P2; reel tooling, default off) ----
        if (g_debug_shade == DebugShade::kUnlit) {
          // fullbright / texture-only: unit gain everywhere -- what the
          // surface (or the flat material) looks like with the rig out
          shc[0] = shc[1] = shc[2] = Shade3{65536, 65536, 65536};
        } else if (g_debug_shade == DebugShade::kNormals) {
          // packed-normal visualisation: gain encodes the bind normal
          // (x,y,z) -> (r,g,b), 0 -> 128. Rides the same gouraud lanes.
          const PV* corner[3] = {&a, &b, &c};
          for (int k = 0; k < 3; ++k) {
            shc[k] =
                Shade3{(corner[k]->nx + 128) * 65536 / 255, (corner[k]->ny + 128) * 65536 / 255,
                       (corner[k]->nz + 128) * 65536 / 255};
          }
          tm.gouraud = gouraud;
        } else if (g_debug_shade == DebugShade::kWire) {
          // wireframe: the triangle's three edges, Bresenham, no fill --
          // the mesh structure itself, see-through on purpose
          const auto line = [&](int32_t x0q, int32_t y0q, int32_t x1q, int32_t y1q) {
            int x0 = x0q >> 8, y0 = y0q >> 8, x1 = x1q >> 8, y1 = y1q >> 8;
            const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
            const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;
            for (;;) {
              if (x0 >= 0 && y0 >= 0 && x0 < static_cast<int>(w) && y0 < static_cast<int>(h)) {
                uint8_t* px = &surf.rgb[(static_cast<size_t>(y0) * w + x0) * 3];
                px[0] = px[1] = px[2] = 235;
              }
              if (x0 == x1 && y0 == y1) break;
              const int e2 = 2 * err;
              if (e2 >= dy) {
                err += dy;
                x0 += sx;
              }
              if (e2 <= dx) {
                err += dx;
                y0 += sy;
              }
            }
          };
          line(a.s.x, a.s.y, b.s.x, b.s.y);
          line(b.s.x, b.s.y, c.s.x, c.s.y);
          line(c.s.x, c.s.y, a.s.x, a.s.y);
          continue;  // no fill
        }
        const Shade3& sh = shc[0];
        // TEXTURED PATH. raster_tri has always been able to sample CLUT8
        // through a TextureSpan; nothing on the creature side ever built one.
        // The light gain rides mod_r/g/b (flat) or the interpolated colour
        // lanes (Gouraud): texel colour TIMES the per-channel rig, one
        // multiply, no second shading model.
        const bool want_tex = (T.page_direct != nullptr || T.page_set != nullptr) &&
                              m.page != 255 && g_debug_shade != DebugShade::kNormals;
        const uint8_t em_r = g_debug_shade == DebugShade::kNormals ? 255 : m.r;
        const uint8_t em_g = g_debug_shade == DebugShade::kNormals ? 255 : m.g;
        const uint8_t em_b = g_debug_shade == DebugShade::kNormals ? 255 : m.b;
        if (want_tex) {
          render::TextureSpan tex;
          tex.ts = T.page_set;
          tex.tile_a = m.page;
          tex.tile_b = m.page;
          tex.mosaic = false;
          tex.mod_r = sh.r;
          tex.mod_g = sh.g;
          tex.mod_b = sh.b;
          if (T.page_direct != nullptr) {
            // T1/T2: the direct page wins; the per-triangle req_lod is the
            // Measure the TMU contract says arrives from upstream. Texel
            // density: |uv cross| in texels^2 over |screen cross| in px^2,
            // level = floor(log2(ratio))/2 (area is length squared), U4.4
            // with zero fraction (the TMU truncates level = lod >> 4).
            //   uv lanes are Q16.16 TILE units; one tile edge is 64 texels,
            //   so texels^2 = cross_uv * 4096 / 2^32. Screen lanes are
            //   S12.8: px^2 = cross_px / 2^16.
            const int64_t e1u = b.s.u - a.s.u, e1v = b.s.v - a.s.v;
            const int64_t e2u = c.s.u - a.s.u, e2v = c.s.v - a.s.v;
            const int64_t e1x = b.s.x - a.s.x, e1y = b.s.y - a.s.y;
            const int64_t e2x = c.s.x - a.s.x, e2y = c.s.y - a.s.y;
            int64_t cuv = e1u * e2v - e1v * e2u;
            if (cuv < 0) cuv = -cuv;
            int64_t cpx = e1x * e2y - e1y * e2x;
            if (cpx < 0) cpx = -cpx;
            // ratio in texel^2/px^2 = cuv * (W*H) * 2^16 / (2^32 * cpx)
            //                       = cuv / (2^(16-log2w-log2h) * cpx), floored
            // (generalised 2026-08-28 for T4: pages are no longer always
            // 64x64 -- the body atlas is 128x256 -- so the texel area and
            // the level cap come from the tile's own mode word)
            const Tmu::Mode md = Tmu::Mode::unpack(T.page_direct->mode_of(m.page));
            const int sh = 16 - md.log2w - md.log2h;
            const uint8_t cap = std::min<uint8_t>(md.max_level, std::min(md.log2w, md.log2h));
            uint8_t level = 0;
            if (cpx > 0) {
              uint64_t ratio =
                  sh >= 0 ? (static_cast<uint64_t>(cuv) >> sh) / static_cast<uint64_t>(cpx)
                          : (static_cast<uint64_t>(cuv) << -sh) / static_cast<uint64_t>(cpx);
              while (ratio >= 4 && level < cap) {  // each mip level is 4x area
                ratio >>= 2;
                ++level;
              }
            }
            tex.direct = T.page_direct;
            tex.lod = static_cast<uint8_t>(level << 4);
          }
          render::ScreenV sa = a.s, sb = b.s, sc = c.s;
          if (gouraud) {
            sa.cr = shc[0].r;
            sa.cg = shc[0].g;
            sa.cb = shc[0].b;
            sb.cr = shc[1].r;
            sb.cg = shc[1].g;
            sb.cb = shc[1].b;
            sc.cr = shc[2].r;
            sc.cg = shc[2].g;
            sc.cb = shc[2].b;
          }
          render::raster_tri(surf, vpp, sa, sb, sc, 255, 255, 255, tm, &tex);
        } else {
          render::ScreenV sa = a.s, sb = b.s, sc = c.s;
          if (tm.gouraud) {
            // pre-lit colour on the 255 scale per corner, ONE rounding at
            // the multiply (the raster's write rounds the interpolant once)
            sa.cr = static_cast<int32_t>((static_cast<int64_t>(em_r) * shc[0].r));
            sa.cg = static_cast<int32_t>((static_cast<int64_t>(em_g) * shc[0].g));
            sa.cb = static_cast<int32_t>((static_cast<int64_t>(em_b) * shc[0].b));
            sb.cr = static_cast<int32_t>((static_cast<int64_t>(em_r) * shc[1].r));
            sb.cg = static_cast<int32_t>((static_cast<int64_t>(em_g) * shc[1].g));
            sb.cb = static_cast<int32_t>((static_cast<int64_t>(em_b) * shc[1].b));
            sc.cr = static_cast<int32_t>((static_cast<int64_t>(em_r) * shc[2].r));
            sc.cg = static_cast<int32_t>((static_cast<int64_t>(em_g) * shc[2].g));
            sc.cb = static_cast<int32_t>((static_cast<int64_t>(em_b) * shc[2].b));
          }
          render::raster_tri(surf, vpp, sa, sb, sc, sat_u8((em_r * sh.r + 32768) >> 16),
                             sat_u8((em_g * sh.g + 32768) >> 16),
                             sat_u8((em_b * sh.b + 32768) >> 16), tm);
        }
      }
    }
  }

  std::memcpy(rgb, surf.rgb.data(), surf.rgb.size());
  std::memcpy(depth, surf.depth.data(), surf.depth.size() * sizeof(int32_t));
}

}  // namespace creature
}  // namespace zref
