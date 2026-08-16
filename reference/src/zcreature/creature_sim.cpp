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
  a.frame = next >= clip->frame_count ? 0 : next;
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
  const int64_t prod[3][3] = {{static_cast<int64_t>(ax) * ax, static_cast<int64_t>(ax) * ay,
                               static_cast<int64_t>(ax) * az},
                              {static_cast<int64_t>(ay) * ax, static_cast<int64_t>(ay) * ay,
                               static_cast<int64_t>(ay) * az},
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

namespace {
// err_px (S12.8) = rhu(proj_q8 * e_r, R) — the Measure's screen-error law
int32_t rung_err_q8(int32_t proj_q8, int32_t e_r, int32_t bound_r) {
  if (e_r == 0) return 0;
  const __int128 num = static_cast<__int128>(proj_q8) * e_r;
  const __int128 den = bound_r;
  return static_cast<int32_t>((num + den / 2) / den);
}
}  // namespace

LodRung lod_raw(int32_t proj_radius_q8, int32_t thresh_q8, const CreatureType& type) {
  const int32_t err[4] = {0, rung_err_q8(proj_radius_q8, type.micro_error, type.bound_radius),
                          rung_err_q8(proj_radius_q8, type.splat_error, type.bound_radius),
                          rung_err_q8(proj_radius_q8, type.glint_error, type.bound_radius)};
  // coarsest legal rung (fewest pixels that still meets the error budget)
  for (int r = 3; r >= 1; --r) {
    if (err[r] <= thresh_q8) return static_cast<LodRung>(r);
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
  // boundary between rung r and its FINER neighbour: the projected radius
  // at which rung r's error equals the threshold (S12.8).
  const auto boundary_q8 = [&](int r) -> int32_t {
    const int32_t e[4] = {0, type.micro_error, type.splat_error, type.glint_error};
    if (e[r] == 0) return 0;
    // B = thresh * R / e_r
    const __int128 num = static_cast<__int128>(thresh_q8) * type.bound_radius;
    return static_cast<int32_t>((num + e[r] / 2) / e[r]);
  };
  bool switch_ok = false;
  if (raw > st.rung) {
    // coarsening: eager — 10% BELOW the boundary of the target rung
    const int32_t bnd = boundary_q8(static_cast<int>(raw));
    switch_ok = static_cast<int64_t>(proj_radius_q8) * 10 <= static_cast<int64_t>(bnd) * 9;
  } else {
    // refining: lazy — 10% ABOVE the boundary of the CURRENT (coarser) rung
    const int32_t bnd = boundary_q8(static_cast<int>(st.rung));
    switch_ok = static_cast<int64_t>(proj_radius_q8) * 10 >= static_cast<int64_t>(bnd) * 11;
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
      g.vx = s16lane(h0) * 2;                       // fx16 raw: |v| < 2.0
      g.vy = (1 << 16) + (s16lane(h1) >> 1);       // up bias 1.0 +- 0.5
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

// the ambient floor of the dual-terrain walls (0.25 + 0.75*lambert) — the
// same Phase-3 stand-in, so a creature's underside is not pitch black
inline int32_t ambient_floor(int32_t shade) {
  return 16384 + static_cast<int32_t>((static_cast<int64_t>(shade) * 49152 + 32768) >> 16);
}

inline uint8_t sat_u8(int32_t v) { return static_cast<uint8_t>(v > 255 ? 255 : (v < 0 ? 0 : v)); }

}  // namespace

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

    // ---- world transform: T(x,y,z) * RotY(facing) * tilt * bulk-scale
    const int32_t fc = fx_cos(ci.facing).raw;
    const int32_t fs = fx_sin(ci.facing).raw;
    mat3x4fx roty{{fc, 0, fs, 0, 0, 1 << 16, 0, 0, -fs, 0, fc, 0}};
    const mat3x4fx tilt = tilt_matrix(ci.tilt, L);
    mat3x4fx local{};
    mat3x4_mul(roty, tilt, local, L);
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
                             : 128;                           // 1 px half-extent
      const int32_t x0 = pc.s.x - r8, x1 = pc.s.x + r8;
      const int32_t y0 = pc.s.y - r8, y1 = pc.s.y + r8;
      a.x = x0; a.y = y0;
      b.x = x1; b.y = y0;
      c.x = x1; c.y = y1;
      d.x = x0; d.y = y1;
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
    const mat3x4fx* pose = poses.acquire(T, ci.anim.slot, ci.anim.frame);
    std::array<mat3x4fx, kMaxBones> worldm{};
    for (int b = 0; b < T.bank.bone_count; ++b) {
      mat3x4_mul(world, pose[b], worldm[b], L);
    }
    const std::vector<Meshlet>& mset =
        ci.lod.rung == LodRung::kMicro ? T.micro : T.mesh;
    for (const Meshlet& m : mset) {
      struct PV {
        render::ScreenV s;
        bool in;
        int32_t wx, wy, wz;
      };
      std::vector<PV> pvs(m.verts.size());
      for (size_t vi = 0; vi < m.verts.size(); ++vi) {
        skin_vertex(worldm.data(), m.verts[vi], pvs[vi].wx, pvs[vi].wy, pvs[vi].wz, L);
        const render::ProjOut po =
            render::project_vertex(vp, vpp, fx16{pvs[vi].wx}, fx16{pvs[vi].wy}, fx16{pvs[vi].wz}, L);
        pvs[vi].s = po.s;
        pvs[vi].in = po.in;
      }
      for (size_t ti = 0; ti + 2 < m.idx.size(); ti += 3) {
        const PV& a = pvs[m.idx[ti]];
        const PV& b = pvs[m.idx[ti + 1]];
        const PV& c = pvs[m.idx[ti + 2]];
        if (!a.in || !b.in || !c.in) continue;  // Phase-3 near-plane law
        const int32_t shade =
            quant_shade(ambient_floor(render::shade_flat_tri(a.wx, a.wy, a.wz, b.wx, b.wy, b.wz,
                                                              c.wx, c.wy, c.wz, L)));
        render::TriMode tm;  // opaque: depth test + write
        render::raster_tri(surf, vpp, a.s, b.s, c.s, sat_u8((m.r * shade + 32768) >> 16),
                           sat_u8((m.g * shade + 32768) >> 16), sat_u8((m.b * shade + 32768) >> 16),
                           tm);
      }
    }
  }

  std::memcpy(rgb, surf.rgb.data(), surf.rgb.size());
  std::memcpy(depth, surf.depth.data(), surf.depth.size() * sizeof(int32_t));
}

}  // namespace creature
}  // namespace zref
