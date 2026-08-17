// terrain_core.cpp — dual-heightfield terrain reference core: the §4.3
// column query, the TERRAIN.BAKE incremental dig stamp and the §3.4 breach
// law. Law citations in zref/zref_terrain.hpp (the single header); nothing
// here re-derives a numeric law — fx16/height16 conversions and rounding are
// zref_fixp.hpp calls (qformats.md is the ONE numeric law).

#include "zref/zref_terrain.hpp"

#include "zref/zref_render.hpp"
#include "zref/zref_trig.hpp"  // isqrt_u64 (qformats §7.2), for keel R

#include <algorithm>

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

// ---- keel default (§3.7) ----------------------------------------------------

KeelProfile keel_profile(const render::TerrainPatch& patch, int32_t heart_x, int32_t heart_z) {
  KeelProfile kp;
  const int w = patch.width, h = patch.height;
  if (w < 2 || h < 2) return kp;
  const size_t n = static_cast<size_t>(w) * h;
  if (patch.heights.size() != n) return kp;
  const size_t cells = static_cast<size_t>(w - 1) * static_cast<size_t>(h - 1);
  if (patch.cell_state.size() != cells) return kp;  // no SOLID mask: no R

  // R: the largest SOLID cell-centre distance from the heart (squared, in
  // (fx16 metres)^2 = m^2 << 32), floored to whole metres via isqrt (§7.2)
  int64_t d2max = 0;
  int32_t peak = INT32_MIN;
  for (size_t k = 0; k < n; ++k)
    if (patch.heights[k] > peak) peak = patch.heights[k];
  for (int cj = 0; cj < h - 1; ++cj) {
    for (int ci = 0; ci < w - 1; ++ci) {
      if ((patch.cell_state[static_cast<size_t>(cj) * (w - 1) + ci] & terrain::kSubstanceMask) !=
          terrain::kSolid)
        continue;
      // cell centre on the envelope lerp (one lattice_lerp + half a cell)
      const int64_t cx =
          (static_cast<int64_t>(lattice_lerp(patch.env_x0, patch.env_x1, ci, w - 1)) +
           lattice_lerp(patch.env_x0, patch.env_x1, ci + 1, w - 1)) /
          2;
      const int64_t cz =
          (static_cast<int64_t>(lattice_lerp(patch.env_z0, patch.env_z1, cj, h - 1)) +
           lattice_lerp(patch.env_z0, patch.env_z1, cj + 1, h - 1)) /
          2;
      const int64_t dx = cx - heart_x, dz = cz - heart_z;
      const int64_t d2 = dx * dx + dz * dz;
      if (d2 > d2max) d2max = d2;
    }
  }
  // d2max is m^2 << 32; isqrt gives metres << 16 (fx16 raw) — floor to m
  kp.radius_m =
      d2max > 0 ? static_cast<int32_t>(zref::isqrt_u64(static_cast<uint64_t>(d2max)) >> 16) : 0;
  kp.peak_m = peak == INT32_MIN ? 0 : peak >> 8;  // height16 -> whole metres (floor)

  // KEEL_DEPTH = min(max(50, R/2), 126 - max(0, peak)), §3.7. The cap can
  // cut BELOW the floor on a tall-spired island (bottom = peak - depth stays
  // on the height16 rails either way - that is what the cap is FOR)
  int32_t depth = kKeelFloorM;
  const int32_t half_r = kp.radius_m / 2;  // floor
  if (half_r > depth) depth = half_r;
  const int32_t head = 126 - (kp.peak_m > 0 ? kp.peak_m : 0);
  if (depth > head) depth = head;
  kp.depth_m = depth;
  kp.depth_raw = depth << 8;
  return kp;
}

bool generate_bottom(render::TerrainPatch& patch, int32_t heart_x, int32_t heart_z,
                     int32_t shallow_override_raw) {
  const int w = patch.width, h = patch.height;
  const size_t n = static_cast<size_t>(w) * h;
  if (w < 2 || h < 2 || patch.heights.size() != n) return false;
  if (patch.cell_state.size() != static_cast<size_t>(w - 1) * static_cast<size_t>(h - 1))
    return false;  // no SOLID mask: R is undefined, refuse (write nothing)

  const KeelProfile kp = keel_profile(patch, heart_x, heart_z);
  int32_t depth_raw = kp.depth_raw;
  if (shallow_override_raw > 0 && shallow_override_raw < depth_raw)
    depth_raw = shallow_override_raw;  // the DELIBERATE slab §3.7 permits

  // thickness(v) = K x (4 + 6 x (1 - q)) / 10, q = round((d/R)^2) in Q16,
  // ONE round-half-up division per vertex (§3.7's "one rounding")
  const int64_t r2 = static_cast<int64_t>(kp.radius_m) * kp.radius_m;  // m^2
  patch.bottom.assign(n, 0);
  for (int j = 0; j < h; ++j) {
    const int32_t vz = lattice_lerp(patch.env_z0, patch.env_z1, j, h - 1);
    const int64_t dz = static_cast<int64_t>(vz) - heart_z;
    for (int i = 0; i < w; ++i) {
      const int32_t vx = lattice_lerp(patch.env_x0, patch.env_x1, i, w - 1);
      const int64_t dx = static_cast<int64_t>(vx) - heart_x;
      const size_t k = static_cast<size_t>(j) * w + i;
      int64_t q16 = 65536;  // d >= R (or R == 0): rim
      if (r2 > 0) {
        const int64_t d2 = (dx * dx + dz * dz) >> 32;  // m^2 (exact enough: R is whole metres)
        if (d2 < r2) q16 = div_rhu(static_cast<__int128>(d2) * 65536, r2);
      }
      const __int128 num =
          static_cast<__int128>(depth_raw) * static_cast<int32_t>(2621440 + 60 * (65536 - q16));
      // divisor 100*65536: t = K x (4 + 6(1-q))/10 with the inner scaled x10
      const int32_t t_raw = static_cast<int32_t>(div_rhu(num, 6553600));
      int64_t bot = static_cast<int64_t>(patch.heights[k]) - t_raw;
      if (bot < -32768) bot = -32768;  // height16 rails (defensive; §3.7 headroom)
      if (bot > 32767) bot = 32767;
      patch.bottom[k] = static_cast<int16_t>(bot);
    }
  }
  return true;
}

}  // namespace terrain

// ---- FORGE.CLIFF reference (§5) ---------------------------------------------

namespace forge {
namespace {

// rim-edge predicate: cell (ci,cj) SOLID, neighbour across `side` not
inline bool is_rim_edge(const terrain::ComposedLattice& lat, int ci, int cj, int side) {
  static const int noff[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
  if (lat.substance(ci, cj) != terrain::kSolid) return false;
  const int ni = ci + noff[side][0], nj = cj + noff[side][1];
  if (ni < 0 || nj < 0 || ni >= lat.w - 1 || nj >= lat.h - 1) return true;  // OUT
  return lat.substance(ni, nj) != terrain::kSolid;
}

// page-scope working edge (terrain_rules §5 frozen degrade order)
struct WorkEdge {
  forge::RimEdge e;
  bool alive = true;
};

// maximal contiguous-collinear runs among live edges, scan order: same
// side, same lattice line, each next edge one span-step past the previous
// (they share a vertex — a merge never bridges a notch). Sides 0/1 run
// along +x (advance ci); sides 2/3 along +z (advance cj).
void build_runs(const std::vector<WorkEdge>& es, size_t begin, size_t end,
                std::vector<std::pair<size_t, uint32_t>>& runs) {  // (start, len)
  runs.clear();
  size_t i = begin;
  while (i < end) {
    if (!es[i].alive) {
      ++i;
      continue;
    }
    size_t j = i + 1;
    while (j < end) {
      const WorkEdge& prev = es[j - 1];
      const WorkEdge& cur = es[j];
      if (!cur.alive || cur.e.side != es[i].e.side) break;
      const bool along_x = es[i].e.side < 2;
      const bool cont = along_x ? (cur.e.cj == es[i].e.cj && cur.e.ci == prev.e.ci + prev.e.span)
                                : (cur.e.ci == es[i].e.ci && cur.e.cj == prev.e.cj + prev.e.span);
      if (!cont) break;
      ++j;
    }
    if (j - i >= 2) runs.push_back({i, static_cast<uint32_t>(j - i)});
    i = j;
  }
}

}  // namespace

RimPlan rim_plan(const terrain::ComposedLattice& lat, const int32_t* vdist) {
  forge::RimPlan plan;
  if (lat.w < 2 || lat.h < 2) return plan;
  const int cw = lat.w - 1, ch = lat.h - 1;

  for (int pj = 0; pj < ch; pj += 32) {    // 32x32-cell PAGE blocks (the
    for (int pi = 0; pi < cw; pi += 32) {  // hardware budget is per page)
      std::vector<WorkEdge> es;
      for (int cj = pj; cj < pj + 32 && cj < ch; ++cj) {
        for (int ci = pi; ci < pi + 32 && ci < cw; ++ci) {
          for (int side = 0; side < 4; ++side)
            if (is_rim_edge(lat, ci, cj, side))
              es.push_back(
                  WorkEdge{forge::RimEdge{static_cast<uint16_t>(ci), static_cast<uint16_t>(cj),
                                          static_cast<uint8_t>(side), 1},
                           true});
        }
      }
      const size_t count = es.size();
      if (count > forge::kRimBudgetPerPage) {
        // degrade (1): merge contiguous collinear runs, longest first
        // (ties: earliest start), until inside budget
        size_t need = count - forge::kRimBudgetPerPage;
        std::vector<std::pair<size_t, uint32_t>> runs;
        while (need > 0) {
          build_runs(es, 0, count, runs);
          // longest run (ties earliest — stable scan order keeps them)
          size_t best = SIZE_MAX;
          uint32_t best_len = 1;
          for (const auto& r : runs)
            if (r.second > best_len) {
              best_len = r.second;
              best = r.first;
            }
          if (best == SIZE_MAX) break;  // nothing mergeable left
          uint32_t take = best_len;
          if (take - 1 > need) take = static_cast<uint32_t>(need) + 1;  // partial prefix
          es[best].e.span = static_cast<uint16_t>(take);
          for (uint32_t k = 1; k < take; ++k) es[best + k].alive = false;
          need -= take - 1;
          plan.merged += take - 1;
        }
        size_t alive = 0;
        for (const WorkEdge& e : es)
          if (e.alive) ++alive;
        if (alive > forge::kRimBudgetPerPage) {
          // degrade (2): keep the greatest endpoint nearness (Q16.16 1/w
          // from the renderer; null = scan order), ties by scan order
          // endpoint nearness with the SAME side->vertex mapping the
          // renderer emits (sides 0/1 run along +x, 2/3 along +z)
          const auto prio = [&](size_t idx) -> int64_t {
            if (vdist == nullptr) return 0;
            const RimEdge& e = es[idx].e;
            const size_t base = static_cast<size_t>(e.cj) * lat.w + e.ci;
            size_t va, vb;
            switch (e.side) {
              case 0:
                va = base;
                vb = base + e.span;
                break;
              case 1:
                va = base + lat.w + e.span;
                vb = base + lat.w;
                break;
              case 2:
                va = base + static_cast<size_t>(e.span) * lat.w;
                vb = base;
                break;
              default:
                va = base + 1;
                vb = base + static_cast<size_t>(e.span) * lat.w + 1;
                break;
            }
            int64_t p = vdist[va];
            if (vdist[vb] > p) p = vdist[vb];
            return p;
          };
          std::vector<size_t> order;
          order.reserve(alive);
          for (size_t k = 0; k < count; ++k)
            if (es[k].alive) order.push_back(k);
          std::stable_sort(order.begin(), order.end(),
                           [&](size_t a, size_t b) { return prio(a) > prio(b); });
          // dropped counts BODIES (a dropped merged span takes its whole
          // span with it), so emitted-bodies + dropped == enumerated, always
          for (size_t k = forge::kRimBudgetPerPage; k < order.size(); ++k) {
            plan.dropped += es[order[k]].e.span;
            es[order[k]].alive = false;
          }
        }
      }
      for (const WorkEdge& e : es)
        if (e.alive) plan.edges.push_back(e.e);  // scan order preserved
    }
  }
  return plan;
}

}  // namespace forge
}  // namespace zref
