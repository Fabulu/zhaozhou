// zref_forge_eval.hpp — the FORGE.PRIM.EVAL position law (the lightning evaluator).
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// zhao_forge_prim owns TOPOLOGY only — its own header says positions come from
// "`params` through the evaluator", and reports/ADDLIGHTNING.md (owner,
// 2026-09-04) names the consequence: "The missing work is chiefly the
// procedural position evaluator and end-to-end Forge integration." This is
// that evaluator's oracle. The owner's law, verbatim from the document:
//
//     P0 = start
//     PN = end
//     Pi = lerp(start, end, i/N)
//        + perpendicular_1 x jitter(seed,   tick_phase, i)
//        + perpendicular_2 x jitter(seed^2, tick_phase, i)
//
// and the bound: "Draw one deterministic ribbon with at most 24 segments and
// at most two bounded branches" — explicitly NOT "any number of branching
// antialiased electrical lines with arbitrary widths".
//
// ---------------------------------------------------------------------------
// THE LAW, MADE EXACT (every choice named; RTL matches this file bit for bit)
// ---------------------------------------------------------------------------
// All positions are fx16 (S15.16, s32) per spec/qformats.md; rescale is the
// one rounding primitive, (x + 32768) >>> 16, round-half-up (qformats §4).
// NOTE: FORGE.PRIM.md says "round-half away from zero"; qformats §4 says
// round-half-up (ties toward +inf). qformats is the machine-wide law and is
// what SIN_Q16 consumers already do (zhao_terrain_normals rescale16), so
// round-half-up is chosen here and the discrepancy is recorded in
// reports/FORGE-PRIM-EVAL-IMPLEMENTATION-20260909.md.
//
//  lerp     off_c   = rhu(D_c * i / N) computed EXACTLY as
//                     floor((2*D_c*i + N) / (2*N)) — an exact rational, no
//                     precision knob, no accumulated error. off_0 = 0 and
//                     off_N = D_c hold identically, which is what makes
//                     P0 == start and PN == end BY ARITHMETIC, not by test.
//  jitter   two xorshift32 streams, one seeded from `seed`, one from
//                     seed2 = low32(seed*seed) ^ kEvalSeed2Salt  (the owner's
//                     literal "seed^2", salted so seed 0/1 still decorrelate).
//                     The stream state advances ONCE PER POINT (i >= 1), so
//                     stalls cannot move it; the low 8 bits index JITTER_Q16,
//                     a 256-entry fx16 table resident in one M10K.
//  scale    jA      = fx_mul(amp, JITTER_Q16[hA & 255])       (one rounding)
//           disp_c  = rescale(perp1_c*jA + perp2_c*jB, 16)    (fused, ONE
//                     rounding — the qformats mad law, exact wide sum first)
//  point    P_c     = sat_s32(S_c + off_c + disp_c)           (one saturation)
//  ribbon   v0_c    = sat_s32(P_c - wvec_c),  v1_c = sat_s32(P_c + wvec_c)
//           wvec_c  = fx_mul(half_width, waxis_c)             (per polyline)
//  ends     i == 0 and i == N take NO jitter: the anchors are exact. The
//           hash still advances at i == N so the stream position is a pure
//           function of i, never of which points happened to be interior.
//
// Branches: branch b is its own polyline whose START is the main polyline's
// jittered centre point at attach index a_b (captured in flight — the caller
// cannot know it, jitter is hardware-side), and whose end is an anchor in the
// params. Its hash streams are re-seeded from (seed ^ kEvalBranchSalt[b]) and
// (seed2 ^ kEvalBranchSalt[b]) so a branch is not a phase-shifted copy of the
// main bolt.
//
// EMISSION ORDER (the order IS the contract, capture-CRC law): polylines in
// order main, branch0, branch1; within a polyline, points i = 0..N; per point
// the ribbon pair (P - wvec) then (P + wvec). That is exactly ring-major
// vidx(s, k, ring=2) = 2s + k, the walk zhao_forge_prim's ribbon indices
// reference — see zref::forge::prim_vertex_index.
//
// Degenerate anchors (start == end) are LEGAL here: D = 0, every base point
// is the anchor, jitter still displaces the interior. FORGE.PRIM refuses
// zero-length primitives at the topology level; the EVALUATOR's domain is the
// full fx16 cube, because a zero-length main bolt with live branches is a
// meaningful effect and refusing it here would be a silent second law.
#pragma once

#include <cstdint>
#include <vector>

#include "zref/generated/zref_forge_jitter_table.hpp"

namespace zref {
namespace forge {

// ---- the owner's bounds, as named constants -------------------------------
constexpr int kEvalMaxMainSegments = 24;   // "at most 24 segments"
constexpr int kEvalMaxBranches = 2;        // "at most two bounded branches"
constexpr int kEvalMaxBranchSegments = 8;  // "each <= 8 segments"

// ---- the named constants of the jitter law --------------------------------
constexpr uint32_t kEvalHashGolden = 0x9E3779B9u;   // stream-A init salt
constexpr uint32_t kEvalHashSaltB = 0x7F4A7C15u;    // stream-B init salt
constexpr uint32_t kEvalSeed2Salt = 0x85EBCA6Bu;    // decorrelates seed 0/1
constexpr uint32_t kEvalBranchSalt[2] = {0x243F6A88u, 0x452821E6u};

struct EvalVec3 {
  int32_t x, y, z;  // fx16
};

struct EvalBranch {
  int attach;    // 0..segments — main-polyline point index the branch grows from
  int segments;  // 1..kEvalMaxBranchSegments
  EvalVec3 end;  // fx16 anchor
};

struct EvalParams {
  EvalVec3 start, end;    // bolt anchors
  EvalVec3 perp1, perp2;  // jitter axes (caller-normalised; used as-is)
  EvalVec3 waxis;         // ribbon width axis (caller-normalised; used as-is)
  int32_t half_width;     // fx16, main ribbon half width
  int32_t branch_half_width;  // fx16
  int32_t amp;                // fx16, main jitter amplitude
  int32_t branch_amp;         // fx16
  uint32_t seed;
  uint16_t tick_phase;
  int segments;      // 1..kEvalMaxMainSegments
  int branch_count;  // 0..kEvalMaxBranches
  EvalBranch br[2];  // br[k] inspected only for k < branch_count
  int view_mask;     // 2 bits, same semantics as zhao_forge_prim
};

enum EvalVerdict : int { kEvalAccept = 0, kEvalRefusedLimit = 1, kEvalSkippedView = 2 };

// The refusal taxonomy, in the order the block applies it. Caps are REFUSED,
// never clamped — a clamped cap is a wrong number that ships quietly.
inline EvalVerdict eval_verdict(const EvalParams& p, int view_sel) {
  bool bad = p.segments < 1 || p.segments > kEvalMaxMainSegments ||
             p.branch_count < 0 || p.branch_count > kEvalMaxBranches;
  for (int b = 0; !bad && b < p.branch_count; ++b) {
    bad = p.br[b].segments < 1 || p.br[b].segments > kEvalMaxBranchSegments ||
          p.br[b].attach < 0 || p.br[b].attach > p.segments;
  }
  if (bad) return kEvalRefusedLimit;
  if ((p.view_mask & view_sel) == 0) return kEvalSkippedView;
  return kEvalAccept;
}

struct EvalCounts {
  uint64_t points = 0;
  uint64_t vertices = 0;
  uint64_t sat_events = 0;  // one per saturating component operation
};

// ---- primitive ops (spec/qformats.md §3/§4) -------------------------------
inline uint32_t eval_xorshift32(uint32_t h) {
  h ^= h << 13;
  h ^= h >> 17;
  h ^= h << 5;
  return h;
}

inline uint32_t eval_hash_init(uint32_t base, uint16_t tick, uint32_t salt) {
  uint32_t h = base ^ (uint32_t(tick) << 16) ^ salt;
  if (h == 0) h = kEvalHashGolden;  // xorshift32 has a fixed point at 0
  return h;
}

inline uint32_t eval_seed2(uint32_t seed) {
  return uint32_t(seed * seed) ^ kEvalSeed2Salt;  // low 32 bits of seed^2, salted
}

inline int32_t eval_sat_s32(int64_t x, EvalCounts* c) {
  if (x > int64_t(INT32_MAX)) {
    if (c) c->sat_events++;
    return INT32_MAX;
  }
  if (x < int64_t(INT32_MIN)) {
    if (c) c->sat_events++;
    return INT32_MIN;
  }
  return int32_t(x);
}

// rescale by 16 (qformats §4: (x + 2^15) >> 16, arithmetic shift = round-half-
// up) then saturate. Wide input: a fused perp1*jA + perp2*jB sum needs 66 bits,
// which the reference carries as __int128 the same way qformats reserves s128
// for reference-side fx24 — the RTL carries it as a 66-bit accumulator.
inline int32_t eval_rescale16(__int128 x, EvalCounts* c) {
  __int128 r = (x + 32768) >> 16;
  if (r > __int128(INT32_MAX)) {
    if (c) c->sat_events++;
    return INT32_MAX;
  }
  if (r < __int128(INT32_MIN)) {
    if (c) c->sat_events++;
    return INT32_MIN;
  }
  return int32_t(r);
}

// off = rhu(D*i/N) computed exactly: floor((2*D*i + N) / (2*N)).
// |D| <= 2^32, i <= N <= 24: numerator < 2^39, so int64 is exact.
inline int64_t eval_lerp_off(int64_t d, int i, int n) {
  int64_t num = 2 * d * int64_t(i) + int64_t(n);
  int64_t den = 2 * int64_t(n);
  int64_t q = num / den;                    // C++ truncates toward zero
  if (num % den != 0 && num < 0) q -= 1;    // make it floor
  return q;
}

inline int32_t eval_fx_mul(int32_t a, int32_t b, EvalCounts* c) {
  return eval_rescale16(__int128(int64_t(a) * int64_t(b)), c);
}

// ---- one polyline ---------------------------------------------------------
// Emits 2*(n+1) ribbon vertices for the polyline S -> E and, when `cap` is
// non-null, captures the jittered CENTRE point at the two attach indices.
inline void eval_polyline(const EvalVec3& s, const EvalVec3& e, int n, uint32_t h_a,
                          uint32_t h_b, int32_t amp, const EvalVec3& perp1,
                          const EvalVec3& perp2, const EvalVec3& wvec,
                          const int attach[2], int attach_count, EvalVec3 cap[2],
                          std::vector<EvalVec3>& out, EvalCounts& c) {
  const int64_t dx = int64_t(e.x) - s.x;
  const int64_t dy = int64_t(e.y) - s.y;
  const int64_t dz = int64_t(e.z) - s.z;
  for (int i = 0; i <= n; ++i) {
    if (i >= 1) {
      h_a = eval_xorshift32(h_a);  // once per point — never per cycle, so a
      h_b = eval_xorshift32(h_b);  // stall pattern cannot reach the stream
    }
    EvalVec3 p;
    if (i == 0) {
      p = s;  // anchor-exact, by assignment
    } else if (i == n) {
      p = e;  // anchor-exact, by assignment
    } else {
      const int32_t ta = kEvalJitterTable[h_a & 0xFF];
      const int32_t tb = kEvalJitterTable[h_b & 0xFF];
      const int32_t ja = eval_fx_mul(amp, ta, &c);
      const int32_t jb = eval_fx_mul(amp, tb, &c);
      const int32_t pp1[3] = {perp1.x, perp1.y, perp1.z};
      const int32_t pp2[3] = {perp2.x, perp2.y, perp2.z};
      const int32_t ss[3] = {s.x, s.y, s.z};
      const int64_t dd[3] = {dx, dy, dz};
      int32_t pc[3];
      for (int k = 0; k < 3; ++k) {
        // Fused: exact wide sum, ONE rounding (qformats single-rounding law).
        const __int128 wide = __int128(int64_t(pp1[k]) * ja) + __int128(int64_t(pp2[k]) * jb);
        const int32_t disp = eval_rescale16(wide, &c);
        const int64_t off = eval_lerp_off(dd[k], i, n);
        pc[k] = eval_sat_s32(int64_t(ss[k]) + off + disp, &c);
      }
      p = {pc[0], pc[1], pc[2]};
    }
    c.points++;
    for (int b = 0; b < attach_count; ++b) {
      if (attach[b] == i) cap[b] = p;
    }
    EvalVec3 v0 = {eval_sat_s32(int64_t(p.x) - wvec.x, &c),
                   eval_sat_s32(int64_t(p.y) - wvec.y, &c),
                   eval_sat_s32(int64_t(p.z) - wvec.z, &c)};
    EvalVec3 v1 = {eval_sat_s32(int64_t(p.x) + wvec.x, &c),
                   eval_sat_s32(int64_t(p.y) + wvec.y, &c),
                   eval_sat_s32(int64_t(p.z) + wvec.z, &c)};
    out.push_back(v0);
    out.push_back(v1);
    c.vertices += 2;
  }
}

// ---- the whole job --------------------------------------------------------
// Vertex stream for an ACCEPTED job, in the declared order. Call eval_verdict
// first; this function assumes the caps hold.
inline void eval_job(const EvalParams& p, std::vector<EvalVec3>& out, EvalCounts& c) {
  const uint32_t s2 = eval_seed2(p.seed);
  const EvalVec3 wvec_main = {eval_fx_mul(p.half_width, p.waxis.x, &c),
                              eval_fx_mul(p.half_width, p.waxis.y, &c),
                              eval_fx_mul(p.half_width, p.waxis.z, &c)};
  int attach[2] = {-1, -1};
  for (int b = 0; b < p.branch_count; ++b) attach[b] = p.br[b].attach;
  EvalVec3 cap[2] = {{0, 0, 0}, {0, 0, 0}};

  eval_polyline(p.start, p.end, p.segments, eval_hash_init(p.seed, p.tick_phase, kEvalHashGolden),
                eval_hash_init(s2, p.tick_phase, kEvalHashSaltB), p.amp, p.perp1, p.perp2,
                wvec_main, attach, p.branch_count, cap, out, c);

  for (int b = 0; b < p.branch_count; ++b) {
    const EvalVec3 wvec_br = {eval_fx_mul(p.branch_half_width, p.waxis.x, &c),
                              eval_fx_mul(p.branch_half_width, p.waxis.y, &c),
                              eval_fx_mul(p.branch_half_width, p.waxis.z, &c)};
    const int none[2] = {-1, -1};
    eval_polyline(cap[b], p.br[b].end, p.br[b].segments,
                  eval_hash_init(p.seed ^ kEvalBranchSalt[b], p.tick_phase, kEvalHashGolden),
                  eval_hash_init(s2 ^ kEvalBranchSalt[b], p.tick_phase, kEvalHashSaltB),
                  p.branch_amp, p.perp1, p.perp2, wvec_br, none, 0, nullptr, out, c);
  }
}

// Total vertices an accepted job emits — the bound the caller sizes against.
// Worst legal case: 2*(24+1) + 2*2*(8+1) = 86 vertices, 84 triangles through
// zhao_forge_prim — inside the owner's "roughly 64-128 triangles" envelope.
inline int eval_vertex_count(const EvalParams& p) {
  int n = 2 * (p.segments + 1);
  for (int b = 0; b < p.branch_count; ++b) n += 2 * (p.br[b].segments + 1);
  return n;
}

}  // namespace forge
}  // namespace zref
