// terrain_core.cpp — dual-heightfield terrain reference core: the §4.3
// column query, the TERRAIN.BAKE incremental dig stamp and the §3.4 breach
// law. Law citations in zref/zref_terrain.hpp (the single header); nothing
// here re-derives a numeric law — fx16/height16 conversions and rounding are
// zref_fixp.hpp calls (qformats.md is the ONE numeric law).

#include "zref/zref_terrain.hpp"

#include "zref/zref_render.hpp"

namespace zref {
namespace terrain {
namespace {

// round-half-up signed division, den > 0 (qformats §4: floor((2n + d)/(2d))).
inline int64_t div_rhu(__int128 n, __int128 d) {
  const __int128 num = 2 * n + d;
  const __int128 den = 2 * d;
  __int128 q = num / den;
  if (num % den != 0 && num < 0) --q;
  return static_cast<int64_t>(q);
}

// cell interval along one monotone placed axis: largest i with c[i] <= x,
// clamped to [0, n-2] so x == c[n-1] lands in the last cell with fraction 1.
// false = outside the envelope (OUT, terrain_rules §3.2).
inline bool locate(const std::vector<int32_t>& c, int32_t x, int* out_i) {
  const int n = static_cast<int>(c.size());
  if (x < c.front() || x > c.back()) return false;
  int lo = 0, hi = n - 1;  // invariant: c[lo] <= x
  while (hi - lo > 1) {
    const int mid = lo + (hi - lo) / 2;
    if (c[static_cast<size_t>(mid)] <= x)
      lo = mid;
    else
      hi = mid;
  }
  *out_i = lo > n - 2 ? n - 2 : lo;
  return true;
}

}  // namespace

ColumnResult column_query(const ComposedLattice& lat, fx16 wxq, fx16 wzq) {
  ColumnResult r;
  if (lat.w < 2 || lat.h < 2) return r;
  int ci = 0, cj = 0;
  if (!locate(lat.wx, wxq.raw, &ci) || !locate(lat.wz, wzq.raw, &cj)) return r;  // kOut
  if (lat.substance(ci, cj) != kSolid) {
    r.cls = ColumnClass::kVoid;
    return r;
  }
  // exact rational fractions from the placed lattice (see header note): the
  // spec's u/v with numerator un/vn over denominator ud/vd, no rounding yet
  const int64_t un = static_cast<int64_t>(wxq.raw) - lat.wx[static_cast<size_t>(ci)];
  const int64_t ud =
      static_cast<int64_t>(lat.wx[static_cast<size_t>(ci) + 1]) - lat.wx[static_cast<size_t>(ci)];
  const int64_t vn = static_cast<int64_t>(wzq.raw) - lat.wz[static_cast<size_t>(cj)];
  const int64_t vd =
      static_cast<int64_t>(lat.wz[static_cast<size_t>(cj) + 1]) - lat.wz[static_cast<size_t>(cj)];
  if (ud <= 0 || vd <= 0) return r;  // degenerate placed cell: OUT (defensive)
  const size_t i00 = static_cast<size_t>(cj) * lat.w + ci;
  const size_t i10 = i00 + 1;
  const size_t i01 = i00 + static_cast<size_t>(lat.w);
  const size_t i11 = i01 + 1;
  // triangle pick on the fixed i00–i11 diagonal: A iff u >= v, ties to A
  // (§4.3) — cross-multiplied so the compare is exact
  const bool tri_a = un * vd >= vn * ud;
  // §4.3 two-MAD forms with ONE rounding: common denominator ud*vd, one
  // round-half-up division (the corner identities and the single-valued
  // diagonal are asserted by tests/terrain/terrain_dual.cpp)
  const auto interp = [&](const std::vector<int32_t>& hgt) -> int32_t {
    const int64_t h00 = hgt[i00], h10 = hgt[i10], h01 = hgt[i01], h11 = hgt[i11];
    __int128 num;
    if (tri_a)
      num = static_cast<__int128>(h10 - h00) * un * vd + static_cast<__int128>(h11 - h10) * vn * ud;
    else
      num = static_cast<__int128>(h11 - h01) * un * vd + static_cast<__int128>(h01 - h00) * vn * ud;
    const __int128 den = static_cast<__int128>(ud) * vd;
    return static_cast<int32_t>(h00 + div_rhu(num, den));
  };
  r.cls = ColumnClass::kSolid;
  r.top = fx16{interp(lat.top)};
  r.bottom = lat.dual ? fx16{interp(lat.bottom)} : r.top;
  return r;
}

void bake_dig(render::TerrainPatch& patch, const DigStamp& st, fx16 depth_from, fx16 depth_to,
              SatLedger* L) {
  const int w = patch.width, h = patch.height;
  const size_t n = static_cast<size_t>(w) * h;
  if (w < 2 || h < 2 || patch.heights.size() != n || st.radius <= 0) return;
  if (patch.scar.size() != n) patch.scar.assign(n, 0);
  const bool dual = patch.bottom.size() == n;
  const bool has_cells =
      patch.cell_state.size() == static_cast<size_t>(w - 1) * static_cast<size_t>(h - 1);
  const int64_t r2 = static_cast<int64_t>(st.radius) * st.radius;
  for (int j = 0; j < h; ++j) {
    const int32_t vz = lattice_lerp(patch.env_z0, patch.env_z1, j, h - 1);
    const int64_t dz = static_cast<int64_t>(vz) - st.cz;
    for (int i = 0; i < w; ++i) {
      const int32_t vx = lattice_lerp(patch.env_x0, patch.env_x1, i, w - 1);
      const int64_t dx = static_cast<int64_t>(vx) - st.cx;
      const int64_t d2 = dx * dx + dz * dz;
      if (d2 >= r2) continue;
      // paraboloid stencil in Q16, one rounding: s = ((r2-d2)<<16 + r2/2)/r2
      const __int128 sn = (static_cast<__int128>(r2 - d2) << 16) + r2 / 2;
      const int64_t s = static_cast<int64_t>(sn / r2);  // 0 < s <= 65536
      // absolute per-depth contribution g(depth) = bake_back(depth * s) —
      // a pure function of the absolute depth, so stepped ramps telescope
      // bit-exactly (the §9.2 deferral identity; see header)
      const auto g = [&](fx16 depth) -> int32_t {
        const int32_t contrib_fx = rescale_s32(static_cast<int64_t>(depth.raw) * s, 16, L);
        return rescale_s32(contrib_fx, 8, L);  // fx16 -> height16 bake-back (§9)
      };
      const int32_t delta16 = g(depth_from) - g(depth_to);  // positive depth digs DOWN
      const size_t k = static_cast<size_t>(j) * w + i;
      int64_t scar = static_cast<int64_t>(patch.scar[k]) + delta16;
      // no_bake clamp (TERRAIN.BAKE contract): a vertex touching a protected
      // cell keeps base + scar >= bottom + 1 height16 LSB — the cell can
      // never satisfy the breach equality
      if (dual && has_cells) {
        bool guarded = false;
        for (int cj = j - 1; cj <= j && !guarded; ++cj) {
          for (int ci = i - 1; ci <= i && !guarded; ++ci) {
            if (ci < 0 || cj < 0 || ci >= w - 1 || cj >= h - 1) continue;
            if (patch.cell_state[static_cast<size_t>(cj) * (w - 1) + ci] & kNoBakeBit)
              guarded = true;
          }
        }
        if (guarded) {
          const int64_t min_scar = static_cast<int64_t>(patch.bottom[k]) + 1 - patch.heights[k];
          if (scar < min_scar) scar = min_scar;
        }
      }
      if (scar > 32767) scar = 32767;  // height16 rails (defensive, qformats §2)
      if (scar < -32768) scar = -32768;
      patch.scar[k] = static_cast<int16_t>(scar);
    }
  }
}

std::vector<BreachEvent> apply_breach_law(render::TerrainPatch& patch) {
  std::vector<BreachEvent> ev;
  const int w = patch.width, h = patch.height;
  const size_t n = static_cast<size_t>(w) * h;
  if (w < 2 || h < 2 || patch.heights.size() != n) return ev;
  if (patch.bottom.size() != n) return ev;  // legacy page: nothing can breach
  if (patch.cell_state.size() != static_cast<size_t>(w - 1) * static_cast<size_t>(h - 1)) return ev;
  const bool has_scar = patch.scar.size() == n;
  // compose_top == bottom after the §3.4 clamp  <=>  base + scar <= bottom
  // (exact height16 compare — the fx16 forms are these values << 8)
  const auto meets_bottom = [&](int i, int j) {
    const size_t k = static_cast<size_t>(j) * w + i;
    const int32_t composed =
        static_cast<int32_t>(patch.heights[k]) + (has_scar ? patch.scar[k] : 0);
    return composed <= patch.bottom[k];
  };
  for (int cj = 0; cj < h - 1; ++cj) {
    for (int ci = 0; ci < w - 1; ++ci) {
      uint8_t& stc = patch.cell_state[static_cast<size_t>(cj) * (w - 1) + ci];
      const uint8_t sub = stc & kSubstanceMask;
      if (sub == kVoidAuthored) continue;  // never becomes ground (§3.4)
      const bool all4 = meets_bottom(ci, cj) && meets_bottom(ci + 1, cj) &&
                        meets_bottom(ci, cj + 1) && meets_bottom(ci + 1, cj + 1);
      if (sub == kSolid && all4 && !(stc & kNoBakeBit)) {
        stc = static_cast<uint8_t>((stc & ~kSubstanceMask) | kVoidBreached);
        ev.push_back(
            BreachEvent{static_cast<uint16_t>(ci), static_cast<uint16_t>(cj), kVoidBreached});
      } else if (sub == kVoidBreached && !all4) {
        stc = static_cast<uint8_t>((stc & ~kSubstanceMask) | kSolid);  // heal
        ev.push_back(BreachEvent{static_cast<uint16_t>(ci), static_cast<uint16_t>(cj), kSolid});
      }
    }
  }
  return ev;
}

}  // namespace terrain
}  // namespace zref
