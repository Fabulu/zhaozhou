// zfield_steps.hpp — the ONE semantic step layer for canonical Field IR ops.
//
// Extracted VERBATIM from zfield_interpret.cpp (Field v3 Phase 2,
// reports/Fieldv3.md): full interpretation, exact uniform PREPARATION on the
// ARM, and the FPLAN vector reference executor must all call the SAME
// semantic step functions, so that "prepare once + execute varying" cannot
// drift from "interpret every point" — they are the same code.
//
// exec_op() executes one canonical operation on FLATTENED operand values:
//   src[] = the operand group members in [A..., B..., C...] order, exactly
//           the shape the generated table (zfield_optable.hpp) declares;
//   dst[] = the destination group (1..3 values).
// Every numeric primitive is a frozen zref:: call (spec/qformats.md); nothing
// here re-derives arithmetic. The charter's grep-audit law still holds: this
// file IS zfield_interpret.cpp's semantics, relocated so three callers can
// share them — it is not a third implementation.

#pragma once

#include <cstdint>

#include "zfield/zfield.hpp"
#include "zref/zref_rcp.hpp"

namespace zfield {
namespace steps {

using zref::fx16;
using zref::SatLedger;

inline fx16 F(int32_t raw) { return fx16{raw}; }

inline int32_t clamp_raw(int32_t v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// §3.10 DOT2/3 finish: exact s128 sum, ONE rescale(·,16), saturate s32.
inline int32_t dot_finish(__int128 p, SatLedger* L) {
  const __int128 r = (p + ((__int128)1 << 15)) >> 16;
  if (r > INT32_MAX) {
    zref::detail::ledger_bump(L, &SatLedger::mul);
    return INT32_MAX;
  }
  if (r < INT32_MIN) {
    zref::detail::ledger_bump(L, &SatLedger::mul);
    return INT32_MIN;
  }
  return (int32_t)r;
}

// §3.7 saturating abs: abs(0x80000000) = 0x7FFFFFFF + SAT
inline int32_t abs_sat(int32_t a, SatLedger* L) {
  if (a == INT32_MIN) {
    zref::detail::ledger_bump(L, &SatLedger::rescale);
    return INT32_MAX;
  }
  return a < 0 ? -a : a;
}

// §3.11 length over lanes: exact u64 sum of squares -> isqrt_u64 -> sat s32
inline int32_t len_of(const int32_t* v, int n, SatLedger* L) {
  uint64_t n2 = 0;
  for (int i = 0; i < n; ++i) n2 += (uint64_t)(int64_t)v[i] * v[i];
  const uint64_t len = zref::isqrt_u64(n2);
  if (len > (uint64_t)INT32_MAX) {
    zref::detail::ledger_bump(L, &SatLedger::rescale);
    return INT32_MAX;
  }
  return (int32_t)len;
}

// §3.12 NORMALIZE2: the normalize3_approx algorithm on two lanes
// (qformats.md §7.4 shape, pinned identically)
inline void normalize2(int32_t x, int32_t y, int32_t& ox, int32_t& oy, SatLedger* L) {
  const uint64_t n2 = (uint64_t)(int64_t)x * x + (uint64_t)(int64_t)y * y;
  if (n2 == 0) {
    zref::detail::ledger_bump(L, &SatLedger::rcp0);
    ox = oy = 0;
    return;
  }
  const uint64_t len = zref::isqrt_u64(n2);
  int e = 0;
  uint64_t m = len;
  while (m < (1ull << 23)) {
    m <<= 1;
    --e;
  }
  while (m >= (1ull << 24)) {
    m >>= 1;
    ++e;
  }
  const uint32_t r = zref::rcp_u24_norm((uint32_t)m);
  ox = zref::rescale_s32((int64_t)x * r, 31 + e, L);
  oy = zref::rescale_s32((int64_t)y * r, 31 + e, L);
}

// §3.15 pinned branchless 6-step compare/select segment search
inline int segment_search(const Table& t, int32_t a) {
  const int n = (int)t.x.size();
  const int32_t clamped = clamp_raw(a, t.x[0], t.x[n - 1]);
  int lo = 0;
  for (int k = 5; k >= 0; --k) {
    const int mid = lo + (1 << k);
    if (mid <= n - 1 && t.x[mid] <= clamped) lo = mid;
  }
  return lo;
}

// §3.15 SPLINE coefficient: exact s64 combination, ONE saturate to s32
inline int32_t sat_s32_i64(int64_t v, SatLedger* L) {
  if (v > INT32_MAX) {
    zref::detail::ledger_bump(L, &SatLedger::rescale);
    return INT32_MAX;
  }
  if (v < INT32_MIN) {
    zref::detail::ledger_bump(L, &SatLedger::rescale);
    return INT32_MIN;
  }
  return (int32_t)v;
}

// §3.17 RING's midpoint: rescale_s32((s64)r0 + r1, 1) — NOT fx_add-then-shift.
// Exposed as its own step because the v3 planner prepares it ONCE when both
// radii are uniform (the prepared-ring lowering, Fieldv3.md §6).
inline int32_t ring_mid(int32_t r0, int32_t r1, SatLedger* L) {
  return zref::rescale_s32((int64_t)r0 + r1, 1, L);
}

// §3.17 RING with UNIFORM radii, prepared once (Fieldv3.md §6 "prepared
// ring"): the midpoint m = ring_mid(r0, r1) and the two smoothstep
// reciprocals rA = field_rcp(fx_sub(m, r0)), rB = field_rcp(fx_sub(r1, m))
// are computed ONCE per field instance by prepare() — their ledger events
// land in the uniform Status, exactly as if every point had computed them.
// The nine separately-rounded varying products below are UNCHANGED from
// zref::smoothstep + the RING finish, so rounding is bit-identical.
inline int32_t ring_prepared(int32_t d, int32_t r0, int32_t m, int32_t rA, int32_t rB,
                             SatLedger* L) {
  // smoothstep(r0, m, d) with its reciprocal prepared:
  fx16 t0 = zref::fx_mul(zref::fx_sub(F(d), F(r0), L), F(rA), L);
  t0 = zref::fx_clamp(t0, F(0), F(1 << 16));
  const fx16 t0sq = zref::fx_mul(t0, t0, L);
  const int32_t s0 =
      zref::fx_mul(t0sq, zref::fx_sub(F(3 << 16), zref::fx_mul(F(2 << 16), t0, L), L), L).raw;
  // smoothstep(m, r1, d) with its reciprocal prepared (r1 enters only via rB):
  fx16 t1 = zref::fx_mul(zref::fx_sub(F(d), F(m), L), F(rB), L);
  t1 = zref::fx_clamp(t1, F(0), F(1 << 16));
  const fx16 t1sq = zref::fx_mul(t1, t1, L);
  const int32_t s1 =
      zref::fx_mul(t1sq, zref::fx_sub(F(3 << 16), zref::fx_mul(F(2 << 16), t1, L), L), L).raw;
  return zref::fx_mul(F(s0), zref::fx_sub(F(1 << 16), F(s1), L), L).raw;
}

/**
 * Execute ONE canonical operation on flattened operand values.
 *
 * `src` holds the operand group members in [A..., B..., C...] order per the
 * generated shape table; `dst` receives the destination group. `tables` is
 * the program's table set (CURVE/DCURVE/SPLINE). OP_END and non-canonical
 * bytes are the CALLER's to reject — this function is only reachable with a
 * validated executable opcode, exactly as zfield::interpret's switch was.
 */
inline void exec_op(uint8_t op, uint32_t imm, const std::vector<Table>& tables, const int32_t* src,
                    int32_t* dst, SatLedger* L) {
  switch (op) {
    case OP_MOV:
      dst[0] = src[0];
      break;
    case OP_LDC:
      dst[0] = (int32_t)imm;
      break;
    case OP_ADD:
      dst[0] = zref::fx_add(F(src[0]), F(src[1]), L).raw;
      break;
    case OP_SUB:
      dst[0] = zref::fx_sub(F(src[0]), F(src[1]), L).raw;
      break;
    case OP_MUL:
      dst[0] = zref::fx_mul(F(src[0]), F(src[1]), L).raw;
      break;
    case OP_MAD:
      dst[0] = zref::fx_mad(F(src[0]), F(src[1]), F(src[2]), L).raw;
      break;
    case OP_MIN:
      dst[0] = zref::fx_min(F(src[0]), F(src[1])).raw;
      break;
    case OP_MAX:
      dst[0] = zref::fx_max(F(src[0]), F(src[1])).raw;
      break;
    case OP_ABS:
      dst[0] = abs_sat(src[0], L);
      break;
    case OP_CLAMP:
      dst[0] = zref::fx_clamp(F(src[0]), F(src[1]), F(src[2])).raw;
      break;
    case OP_SELECT:
      dst[0] = src[2] != 0 ? src[0] : src[1];
      break;
    case OP_CMP: {
      const int32_t a = src[0], b = src[1];
      bool t = false;
      switch (imm) {
        case 0:
          t = a == b;
          break;
        case 1:
          t = a != b;
          break;
        case 2:
          t = a < b;
          break;
        case 3:
          t = a <= b;
          break;
        case 4:
          t = a > b;
          break;
        case 5:
          t = a >= b;
          break;
      }
      dst[0] = t ? 0x10000 : 0;
      break;
    }
    case OP_DOT2: {
      const __int128 p = (__int128)(int64_t)src[0] * src[2] + (__int128)(int64_t)src[1] * src[3];
      dst[0] = dot_finish(p, L);
      break;
    }
    case OP_DOT3: {
      const __int128 p = (__int128)(int64_t)src[0] * src[3] + (__int128)(int64_t)src[1] * src[4] +
                         (__int128)(int64_t)src[2] * src[5];
      dst[0] = dot_finish(p, L);
      break;
    }
    case OP_LEN2:
      dst[0] = len_of(src, 2, L);
      break;
    case OP_LEN3:
      dst[0] = len_of(src, 3, L);
      break;
    case OP_DIST2: {
      const int32_t d[2] = {
          zref::fx_sub(F(src[0]), F(src[2]), L).raw,
          zref::fx_sub(F(src[1]), F(src[3]), L).raw,
      };
      dst[0] = len_of(d, 2, L);
      break;
    }
    case OP_NORMALIZE2:
      normalize2(src[0], src[1], dst[0], dst[1], L);
      break;
    case OP_NORMALIZE3: {
      // zref::normalize3_approx verbatim (qformats.md §7.4)
      const zref::vec3fx v =
          zref::normalize3_approx(zref::vec3fx{F(src[0]), F(src[1]), F(src[2])}, L);
      dst[0] = v.x.raw;
      dst[1] = v.y.raw;
      dst[2] = v.z.raw;
      break;
    }
    case OP_RCP:
      dst[0] = zref::field_rcp(F(src[0]), L).raw;
      break;
    case OP_SIN:
      dst[0] = zref::fx_sin(zref::angle16{(uint16_t)src[0]}).raw;
      break;
    case OP_COS:
      dst[0] = zref::fx_cos(zref::angle16{(uint16_t)src[0]}).raw;
      break;
    case OP_CURVE: {
      const Table& t = tables[imm];
      const int i = segment_search(t, src[0]);
      const int32_t a = clamp_raw(src[0], t.x[0], t.x[(int)t.x.size() - 1]);
      dst[0] = zref::fx_mad(zref::fx_sub(F(a), F(t.x[i]), L), F(t.dy[i]), F(t.y[i]), L).raw;
      break;
    }
    case OP_DCURVE: {
      const Table& t = tables[imm];
      const int i = segment_search(t, src[0]);
      dst[0] = t.dy[i];
      break;
    }
    case OP_SPLINE: {
      const Table& t = tables[imm];
      const int n = (int)t.x.size();
      const int i = segment_search(t, src[0]);
      const int32_t a = clamp_raw(src[0], t.x[0], t.x[n - 1]);
      // t = clamp(rescale((a − x_i)·dy_i, 16), 0, 1)
      const int32_t tt =
          zref::fx_clamp(
              F(zref::rescale_s32(
                  (int64_t)(zref::fx_sub(F(a), F(t.x[i]), L).raw) * (int64_t)t.dy[i], 16, L)),
              F(0), F(1 << 16))
              .raw;
      const int32_t p0 = t.y[i > 0 ? i - 1 : 0];
      const int32_t p1 = t.y[i];
      const int32_t p2 = t.y[i + 1 < n ? i + 1 : n - 1];
      const int32_t p3 = t.y[i + 2 < n ? i + 2 : n - 1];
      const int32_t C1 = sat_s32_i64((int64_t)p2 - p0, L);
      const int32_t C2 = sat_s32_i64((int64_t)2 * p0 - 5 * (int64_t)p1 + 4 * (int64_t)p2 - p3, L);
      const int32_t C3 = sat_s32_i64(-(int64_t)p0 + 3 * (int64_t)p1 - 3 * (int64_t)p2 + p3, L);
      int32_t u = zref::fx_mad(F(tt), F(C3), F(C2), L).raw;  // Horner
      u = zref::fx_mad(F(tt), F(u), F(C1), L).raw;
      const int32_t v = zref::fx_mul(F(tt), F(u), L).raw;
      // §3.15: dst = fx_add(P1, rescale_s32(v, 1)) — the ½ of Catmull-Rom.
      dst[0] = zref::fx_add(F(p1), F(zref::rescale_s32((int64_t)v, 1, L)), L).raw;
      break;
    }
    case OP_NOISE2: {
      const uint32_t ix = (uint32_t)(src[0] >> 16);  // arithmetic floor
      const uint32_t iy = (uint32_t)(src[1] >> 16);
      dst[0] = (int32_t)(zref::noise2_hash(ix, iy, imm, 0) >> 16);
      dst[1] = (int32_t)(zref::noise2_hash(ix, iy, imm, 1) >> 16);
      break;
    }
    case OP_RING: {
      const int32_t d = src[0], r0 = src[1], r1 = src[2];
      const int32_t m = ring_mid(r0, r1, L);
      const int32_t s0 = zref::smoothstep(F(r0), F(m), F(d), L).raw;
      const int32_t s1 = zref::smoothstep(F(m), F(r1), F(d), L).raw;
      dst[0] = zref::fx_mul(F(s0), zref::fx_sub(F(1 << 16), F(s1), L), L).raw;
      break;
    }
    case OP_RIDGE: {
      const uint32_t u =
          zref::noise2_hash((uint32_t)(src[0] >> 16), (uint32_t)(src[1] >> 16), imm, 0) >> 16;
      const int32_t t =
          zref::fx_sub(zref::fx_add(F((int32_t)u), F((int32_t)u), L), F(1 << 16), L).raw;
      dst[0] = zref::fx_sub(F(1 << 16), F(abs_sat(t, L)), L).raw;
      break;
    }
    case OP_ROT2: {
      const int32_t ang = src[2] & 0xFFFF;
      const int32_t c = zref::fx_cos(zref::angle16{(uint16_t)ang}).raw;
      const int32_t s = zref::fx_sin(zref::angle16{(uint16_t)ang}).raw;
      const int32_t x = src[0], y = src[1];
      dst[0] = zref::fx_sub(zref::fx_mul(F(c), F(x), L), zref::fx_mul(F(s), F(y), L), L).raw;
      dst[1] = zref::fx_add(zref::fx_mul(F(s), F(x), L), zref::fx_mul(F(c), F(y), L), L).raw;
      break;
    }
    case OP_ROT3: {
      const int32_t ang = src[3] & 0xFFFF;
      const int32_t c = zref::fx_cos(zref::angle16{(uint16_t)ang}).raw;
      const int32_t s = zref::fx_sin(zref::angle16{(uint16_t)ang}).raw;
      const int32_t x = src[0], y = src[1], z = src[2];
      if (imm == 0) {  // X axis
        dst[1] = zref::fx_sub(zref::fx_mul(F(c), F(y), L), zref::fx_mul(F(s), F(z), L), L).raw;
        dst[2] = zref::fx_add(zref::fx_mul(F(s), F(y), L), zref::fx_mul(F(c), F(z), L), L).raw;
        dst[0] = x;
      } else if (imm == 1) {  // Y axis
        dst[2] = zref::fx_sub(zref::fx_mul(F(c), F(z), L), zref::fx_mul(F(s), F(x), L), L).raw;
        dst[0] = zref::fx_add(zref::fx_mul(F(s), F(z), L), zref::fx_mul(F(c), F(x), L), L).raw;
        dst[1] = y;
      } else {  // Z axis
        dst[0] = zref::fx_sub(zref::fx_mul(F(c), F(x), L), zref::fx_mul(F(s), F(y), L), L).raw;
        dst[1] = zref::fx_add(zref::fx_mul(F(s), F(x), L), zref::fx_mul(F(c), F(y), L), L).raw;
        dst[2] = z;
      }
      break;
    }
    default:
      // unreachable: exec_op only runs on validated executable opcodes
      __builtin_unreachable();
  }
}

}  // namespace steps
}  // namespace zfield
