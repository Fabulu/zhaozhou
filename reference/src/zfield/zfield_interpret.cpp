// zfield_interpret.cpp — the ONE generic Field IR interpreter (field-ir.md
// §3; §Grep-audit-law). Every numeric primitive is a frozen zref:: call
// (spec/qformats.md) — nothing here re-derives arithmetic. The TS interpreter
// (compiler/src/field_ir/interpret.ts) mirrors this file; golden .zvec are
// C++-owned and TS must replay them byte-identically (Csmith differential).

#include "zfield/zfield.hpp"

#include "zref/zref_rcp.hpp"

namespace zfield {

namespace {

using zref::fx16;
using zref::SatLedger;

fx16 F(int32_t raw) { return fx16{raw}; }

int32_t clamp_raw(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

// §3.10 DOT2/3 finish: exact s128 sum, ONE rescale(·,16), saturate s32.
int32_t dot_finish(__int128 p, SatLedger* L) {
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
int32_t abs_sat(int32_t a, SatLedger* L) {
  if (a == INT32_MIN) {
    zref::detail::ledger_bump(L, &SatLedger::rescale);
    return INT32_MAX;
  }
  return a < 0 ? -a : a;
}

// §3.11 length over lanes: exact u64 sum of squares -> isqrt_u64 -> sat s32
int32_t len_of(const int32_t* v, int n, SatLedger* L) {
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
void normalize2(int32_t x, int32_t y, int32_t& ox, int32_t& oy, SatLedger* L) {
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
int segment_search(const Table& t, int32_t a) {
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
int32_t sat_s32_i64(int64_t v, SatLedger* L) {
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

}  // namespace

Status interpret(const Decoded& prog, const int32_t* in, size_t n_in, int32_t* out, size_t n_out) {
  int32_t reg[REG_COUNT] = {0};
  for (size_t i = 0; i < prog.in_lanes.size() && i < n_in; ++i) {
    reg[prog.in_lanes[i].reg] = in[i];
  }
  SatLedger L = {};

  for (const Instr& ins : prog.instrs) {
    switch (ins.op) {
      case OP_END:
        goto done;
      case OP_MOV:
        reg[ins.dst] = reg[ins.a];
        break;
      case OP_LDC:
        reg[ins.dst] = (int32_t)ins.imm;
        break;
      case OP_ADD:
        reg[ins.dst] = zref::fx_add(F(reg[ins.a]), F(reg[ins.b]), &L).raw;
        break;
      case OP_SUB:
        reg[ins.dst] = zref::fx_sub(F(reg[ins.a]), F(reg[ins.b]), &L).raw;
        break;
      case OP_MUL:
        reg[ins.dst] = zref::fx_mul(F(reg[ins.a]), F(reg[ins.b]), &L).raw;
        break;
      case OP_MAD:
        reg[ins.dst] = zref::fx_mad(F(reg[ins.a]), F(reg[ins.b]), F(reg[ins.c]), &L).raw;
        break;
      case OP_MIN:
        reg[ins.dst] = zref::fx_min(F(reg[ins.a]), F(reg[ins.b])).raw;
        break;
      case OP_MAX:
        reg[ins.dst] = zref::fx_max(F(reg[ins.a]), F(reg[ins.b])).raw;
        break;
      case OP_ABS:
        reg[ins.dst] = abs_sat(reg[ins.a], &L);
        break;
      case OP_CLAMP:
        reg[ins.dst] = zref::fx_clamp(F(reg[ins.a]), F(reg[ins.b]), F(reg[ins.c])).raw;
        break;
      case OP_SELECT:
        reg[ins.dst] = reg[ins.c] != 0 ? reg[ins.a] : reg[ins.b];
        break;
      case OP_CMP: {
        const int32_t a = reg[ins.a], b = reg[ins.b];
        bool t = false;
        switch (ins.imm) {
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
        reg[ins.dst] = t ? 0x10000 : 0;
        break;
      }
      case OP_DOT2: {
        const __int128 p = (__int128)(int64_t)reg[ins.a] * reg[ins.b] +
                           (__int128)(int64_t)reg[ins.a + 1] * reg[ins.b + 1];
        reg[ins.dst] = dot_finish(p, &L);
        break;
      }
      case OP_DOT3: {
        const __int128 p = (__int128)(int64_t)reg[ins.a] * reg[ins.b] +
                           (__int128)(int64_t)reg[ins.a + 1] * reg[ins.b + 1] +
                           (__int128)(int64_t)reg[ins.a + 2] * reg[ins.b + 2];
        reg[ins.dst] = dot_finish(p, &L);
        break;
      }
      case OP_LEN2:
        reg[ins.dst] = len_of(&reg[ins.a], 2, &L);
        break;
      case OP_LEN3:
        reg[ins.dst] = len_of(&reg[ins.a], 3, &L);
        break;
      case OP_DIST2: {
        const int32_t d[2] = {
            zref::fx_sub(F(reg[ins.a]), F(reg[ins.b]), &L).raw,
            zref::fx_sub(F(reg[ins.a + 1]), F(reg[ins.b + 1]), &L).raw,
        };
        reg[ins.dst] = len_of(d, 2, &L);
        break;
      }
      case OP_NORMALIZE2:
        normalize2(reg[ins.a], reg[ins.a + 1], reg[ins.dst], reg[ins.dst + 1], &L);
        break;
      case OP_NORMALIZE3: {
        // zref::normalize3_approx verbatim (qformats.md §7.4)
        const zref::vec3fx v = zref::normalize3_approx(
            zref::vec3fx{F(reg[ins.a]), F(reg[ins.a + 1]), F(reg[ins.a + 2])}, &L);
        reg[ins.dst] = v.x.raw;
        reg[ins.dst + 1] = v.y.raw;
        reg[ins.dst + 2] = v.z.raw;
        break;
      }
      case OP_RCP:
        reg[ins.dst] = zref::field_rcp(F(reg[ins.a]), &L).raw;
        break;
      case OP_SIN:
        reg[ins.dst] = zref::fx_sin(zref::angle16{(uint16_t)reg[ins.a]}).raw;
        break;
      case OP_COS:
        reg[ins.dst] = zref::fx_cos(zref::angle16{(uint16_t)reg[ins.a]}).raw;
        break;
      case OP_CURVE: {
        const Table& t = prog.tables[ins.imm];
        const int i = segment_search(t, reg[ins.a]);
        const int32_t a = clamp_raw(reg[ins.a], t.x[0], t.x[(int)t.x.size() - 1]);
        reg[ins.dst] =
            zref::fx_mad(zref::fx_sub(F(a), F(t.x[i]), &L), F(t.dy[i]), F(t.y[i]), &L).raw;
        break;
      }
      case OP_DCURVE: {
        const Table& t = prog.tables[ins.imm];
        const int i = segment_search(t, reg[ins.a]);
        reg[ins.dst] = t.dy[i];
        break;
      }
      case OP_SPLINE: {
        const Table& t = prog.tables[ins.imm];
        const int n = (int)t.x.size();
        const int i = segment_search(t, reg[ins.a]);
        const int32_t a = clamp_raw(reg[ins.a], t.x[0], t.x[n - 1]);
        // t = clamp(rescale((a − x_i)·dy_i, 16), 0, 1)
        const int32_t tt =
            zref::fx_clamp(
                F(zref::rescale_s32(
                    (int64_t)(zref::fx_sub(F(a), F(t.x[i]), &L).raw) * (int64_t)t.dy[i], 16, &L)),
                F(0), F(1 << 16))
                .raw;
        const int32_t p0 = t.y[i > 0 ? i - 1 : 0];
        const int32_t p1 = t.y[i];
        const int32_t p2 = t.y[i + 1 < n ? i + 1 : n - 1];
        const int32_t p3 = t.y[i + 2 < n ? i + 2 : n - 1];
        const int32_t C1 = sat_s32_i64((int64_t)p2 - p0, &L);
        const int32_t C2 =
            sat_s32_i64((int64_t)2 * p0 - 5 * (int64_t)p1 + 4 * (int64_t)p2 - p3, &L);
        const int32_t C3 = sat_s32_i64(-(int64_t)p0 + 3 * (int64_t)p1 - 3 * (int64_t)p2 + p3, &L);
        int32_t u = zref::fx_mad(F(tt), F(C3), F(C2), &L).raw;  // Horner
        u = zref::fx_mad(F(tt), F(u), F(C1), &L).raw;
        const int32_t v = zref::fx_mul(F(tt), F(u), &L).raw;
        // §3.15: dst = fx_add(P1, rescale_s32(v, 1)) — the ½ of Catmull-Rom.
        // rescale the RAW v by 1; the pre-fix `v << 16` here amplified the
        // term by 2^16 (review C1, RUN-20260814-1912 wave-1).
        reg[ins.dst] = zref::fx_add(F(p1), F(zref::rescale_s32((int64_t)v, 1, &L)), &L).raw;
        break;
      }
      case OP_NOISE2: {
        const uint32_t ix = (uint32_t)(reg[ins.a] >> 16);  // arithmetic floor
        const uint32_t iy = (uint32_t)(reg[ins.a + 1] >> 16);
        reg[ins.dst] = (int32_t)(zref::noise2_hash(ix, iy, ins.imm, 0) >> 16);
        reg[ins.dst + 1] = (int32_t)(zref::noise2_hash(ix, iy, ins.imm, 1) >> 16);
        break;
      }
      case OP_RING: {
        const int32_t d = reg[ins.a], r0 = reg[ins.b], r1 = reg[ins.c];
        const int32_t m = zref::rescale_s32((int64_t)r0 + r1, 1, &L);
        const int32_t s0 = zref::smoothstep(F(r0), F(m), F(d), &L).raw;
        const int32_t s1 = zref::smoothstep(F(m), F(r1), F(d), &L).raw;
        reg[ins.dst] = zref::fx_mul(F(s0), zref::fx_sub(F(1 << 16), F(s1), &L), &L).raw;
        break;
      }
      case OP_RIDGE: {
        const uint32_t u = zref::noise2_hash((uint32_t)(reg[ins.a] >> 16),
                                             (uint32_t)(reg[ins.b] >> 16), ins.imm, 0) >>
                           16;
        const int32_t t =
            zref::fx_sub(zref::fx_add(F((int32_t)u), F((int32_t)u), &L), F(1 << 16), &L).raw;
        reg[ins.dst] = zref::fx_sub(F(1 << 16), F(abs_sat(t, &L)), &L).raw;
        break;
      }
      case OP_ROT2: {
        const int32_t ang = reg[ins.b] & 0xFFFF;
        const int32_t c = zref::fx_cos(zref::angle16{(uint16_t)ang}).raw;
        const int32_t s = zref::fx_sin(zref::angle16{(uint16_t)ang}).raw;
        const int32_t x = reg[ins.a], y = reg[ins.a + 1];
        reg[ins.dst] =
            zref::fx_sub(zref::fx_mul(F(c), F(x), &L), zref::fx_mul(F(s), F(y), &L), &L).raw;
        reg[ins.dst + 1] =
            zref::fx_add(zref::fx_mul(F(s), F(x), &L), zref::fx_mul(F(c), F(y), &L), &L).raw;
        break;
      }
      case OP_ROT3: {
        const int32_t ang = reg[ins.b] & 0xFFFF;
        const int32_t c = zref::fx_cos(zref::angle16{(uint16_t)ang}).raw;
        const int32_t s = zref::fx_sin(zref::angle16{(uint16_t)ang}).raw;
        const int32_t x = reg[ins.a], y = reg[ins.a + 1], z = reg[ins.a + 2];
        if (ins.imm == 0) {  // X axis
          reg[ins.dst + 1] =
              zref::fx_sub(zref::fx_mul(F(c), F(y), &L), zref::fx_mul(F(s), F(z), &L), &L).raw;
          reg[ins.dst + 2] =
              zref::fx_add(zref::fx_mul(F(s), F(y), &L), zref::fx_mul(F(c), F(z), &L), &L).raw;
          reg[ins.dst] = x;
        } else if (ins.imm == 1) {  // Y axis
          reg[ins.dst + 2] =
              zref::fx_sub(zref::fx_mul(F(c), F(z), &L), zref::fx_mul(F(s), F(x), &L), &L).raw;
          reg[ins.dst] =
              zref::fx_add(zref::fx_mul(F(s), F(z), &L), zref::fx_mul(F(c), F(x), &L), &L).raw;
          reg[ins.dst + 1] = y;
        } else {  // Z axis
          reg[ins.dst] =
              zref::fx_sub(zref::fx_mul(F(c), F(x), &L), zref::fx_mul(F(s), F(y), &L), &L).raw;
          reg[ins.dst + 1] =
              zref::fx_add(zref::fx_mul(F(s), F(x), &L), zref::fx_mul(F(c), F(y), &L), &L).raw;
          reg[ins.dst + 2] = z;
        }
        break;
      }
      default:
        // unreachable: interpret only runs on decoded (validated) programs
        __builtin_unreachable();
    }
  }
done:
  for (size_t i = 0; i < prog.out_lanes.size() && i < n_out; ++i) {
    out[i] = reg[prog.out_lanes[i].reg];
  }
  return Status{L.add || L.mul || L.rescale || L.unit || L.rcp, L.rcp0 != 0};
}

}  // namespace zfield
