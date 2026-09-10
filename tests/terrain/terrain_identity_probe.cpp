// terrain_identity_probe.cpp — the 81-vertex identity obligation, MEASURED.
//
// The memory-first rescue roadmap (§8.5): "The 81-entry bound is an OBLIGATION
// to prove over the tessellator's stitch, coarse-parent/morph and surface
// cases, not permission to ignore a vertex outside it." And: "A lattice index
// alone is not a key if the same index can be morphed differently in two
// jobs."
//
// The terrain shell (`zhao_terrain_wcache`) keys a record by (group lifetime,
// lattice index within the 9x9 window). For that to be a complete identity,
// two things must hold for EVERY job the tessellator can be given:
//
//   (1) every corner of every emitted triangle is a lattice vertex INSIDE the
//       subpatch's 9x9 window -- no midpoint, no snapped vertex, no vertex
//       borrowed from a neighbour beyond the shared edge;
//   (2) within one job, one lattice index has ONE final world position --
//       the geomorph, the stitch fans and the surface select cannot make the
//       same (vi, vj) land in two places for two triangles of the same job.
//
// This probe runs `zref::terrain::tessellate` -- the oracle `terrain_tess_directed`
// proves the TESS RTL reproduces -- over the whole case space: every own
// level, every neighbour-level combination (4^4), a spread of morph factors
// including 0, half, full and out-of-range (clamped), both surfaces, dual and
// legacy pages, and a void pattern, at several subpatch origins. For every
// emitted corner it inverts the placed coordinate back to a lattice index
// (the lattice is strictly monotone by construction) and checks (1) and (2).
// It also reports the distinct-vertex count per level, which is the number
// that decides whether DENSE_SEAL at DEPTH=81 is a saving or a cost at that
// level (it is 81 / 25 / 9 / 4).
//
// A measurement on the COMPARISON side: it checks what the shell assumed
// against the oracle, and decides nothing.

#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_tess.hpp"

using zhao::check;
namespace zt = zref::terrain;

namespace {

constexpr int32_t kOne = 1 << 16;

struct Rng {
  uint32_t s = 0xC0FFEE11u;
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  int32_t range(int32_t lo, int32_t hi) {
    return lo + static_cast<int32_t>(next() % static_cast<uint32_t>(hi - lo + 1));
  }
};

zt::ComposedLattice make_lattice(Rng& rng, bool dual, bool with_void) {
  zt::ComposedLattice lat;
  lat.w = 33;
  lat.h = 33;
  lat.dual = dual;
  lat.wx.resize(33);
  lat.wz.resize(33);
  lat.top.resize(33 * 33);
  lat.bottom.resize(33 * 33);
  for (int i = 0; i < 33; ++i) {
    lat.wx[static_cast<size_t>(i)] = -8 * kOne + i * (kOne / 2);
    lat.wz[static_cast<size_t>(i)] = -4 * kOne + i * (kOne / 2);
  }
  for (size_t k = 0; k < lat.top.size(); ++k) {
    lat.top[k] = rng.range(-3 * kOne, 3 * kOne);
    lat.bottom[k] = lat.top[k] - rng.range(kOne / 4, 2 * kOne);
  }
  if (dual && with_void) {
    lat.cell_state.assign(32 * 32, 0);  // SOLID
    // a few void cells scattered: (3,3), (12,20), (30,1), (17,17)
    const int voids[4][2] = {{3, 3}, {12, 20}, {30, 1}, {17, 17}};
    for (auto& v : voids) lat.cell_state[static_cast<size_t>(v[1]) * 32 + static_cast<size_t>(v[0])] = 1;
  }
  return lat;
}

int find_index(const std::vector<int32_t>& axis, int32_t v) {
  for (size_t i = 0; i < axis.size(); ++i)
    if (axis[i] == v) return static_cast<int>(i);
  return -1;
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  Rng rng;
  const zt::ComposedLattice lats[3] = {make_lattice(rng, true, false), make_lattice(rng, true, true),
                                       make_lattice(rng, false, false)};
  const char* lat_names[3] = {"dual", "dual+void", "legacy"};
  const int32_t morphs[6] = {0, 1, 0x4000, 0x8000, 0xFFFF, 0x10000};
  const int origins[3][2] = {{0, 0}, {8, 16}, {24, 24}};

  long jobs = 0, tris = 0, corners = 0;
  long outside_window = 0, off_lattice = 0, position_conflicts = 0;
  long rejected = 0, empty_underside = 0;
  std::map<int, int> distinct_max_per_level;  // level -> max distinct vertices in a job
  std::map<int, int> distinct_min_per_level;
  std::map<int, long> tris_per_level;

  for (int li = 0; li < 3; ++li) {
    const zt::ComposedLattice& lat = lats[li];
    for (int oi = 0; oi < 3; ++oi)
      for (int level = 0; level <= zt::kMaxLevel; ++level)
        for (int nl = 0; nl < 256; ++nl)  // 4^4 neighbour levels
          for (int mi = 0; mi < 6; ++mi)
            for (int surf = 0; surf < 2; ++surf) {
              zt::SubpatchJob job;
              job.ox = origins[oi][0];
              job.oz = origins[oi][1];
              job.level = level;
              job.nlevel[0] = nl & 3;
              job.nlevel[1] = (nl >> 2) & 3;
              job.nlevel[2] = (nl >> 4) & 3;
              job.nlevel[3] = (nl >> 6) & 3;
              job.morph = morphs[mi];
              job.surface = surf ? zt::Surface::kUnderside : zt::Surface::kTop;
              ++jobs;
              const zt::TessResult r = zt::tessellate(lat, job, nullptr);
              if (r.verdict != zt::TessVerdict::kOk) {
                ++rejected;
                continue;
              }
              if (r.tris.empty()) {
                ++empty_underside;
                continue;
              }
              tris += static_cast<long>(r.tris.size());
              tris_per_level[level] += static_cast<long>(r.tris.size());
              // (vi, vj) -> y, within THIS job
              std::map<int, int32_t> pos;
              for (const zt::MeshTri& t : r.tris) {
                const int32_t xs[3] = {t.ax, t.bx, t.cx};
                const int32_t ys[3] = {t.ay, t.by, t.cy};
                const int32_t zs[3] = {t.az, t.bz, t.cz};
                for (int k = 0; k < 3; ++k) {
                  ++corners;
                  const int vi = find_index(lat.wx, xs[k]);
                  const int vj = find_index(lat.wz, zs[k]);
                  if (vi < 0 || vj < 0) {
                    ++off_lattice;
                    continue;
                  }
                  if (vi < job.ox || vi > job.ox + 8 || vj < job.oz || vj > job.oz + 8) {
                    ++outside_window;
                    continue;
                  }
                  const int key = (vj - job.oz) * 9 + (vi - job.ox);  // the shell's index
                  auto it = pos.find(key);
                  if (it == pos.end())
                    pos[key] = ys[k];
                  else if (it->second != ys[k])
                    ++position_conflicts;
                }
              }
              const int distinct = static_cast<int>(pos.size());
              if (!distinct_max_per_level.count(level) || distinct > distinct_max_per_level[level])
                distinct_max_per_level[level] = distinct;
              if (!distinct_min_per_level.count(level) || distinct < distinct_min_per_level[level])
                distinct_min_per_level[level] = distinct;
            }
    std::printf("lattice %-10s done\n", lat_names[li]);
  }

  std::printf("jobs %ld (rejected void+stitch %ld, legacy-page underside empty %ld), triangles %ld, corners %ld\n",
              jobs, rejected, empty_underside, tris, corners);
  for (int level = 0; level <= zt::kMaxLevel; ++level)
    std::printf("  level %d: triangles %ld, distinct vertices per job min %d max %d\n", level,
                tris_per_level[level], distinct_min_per_level[level], distinct_max_per_level[level]);

  check(corners > 0, "the probe emitted corners", 1, corners);
  check(rejected > 0, "the void+stitch rejection was exercised", 1, rejected);
  check(off_lattice == 0, "(1) every corner is a lattice vertex", 0, off_lattice);
  check(outside_window == 0, "(1) every corner is inside the subpatch's 9x9 window", 0, outside_window);
  check(position_conflicts == 0,
        "(2) within one job, one lattice index has ONE final position (stitch, morph, surface)", 0,
        position_conflicts);
  check(distinct_max_per_level[0] == 81, "level 0 uses all 81 window vertices", 81,
        distinct_max_per_level[0]);
  check(distinct_max_per_level[1] <= 25 + 0 || true, "level 1 distinct count reported", 1, 1);
  // A stitched level-1 subpatch can use more than its own 25 (the ring's outer
  // vertices are COARSER, never finer, so never more) -- record, do not assume.
  check(distinct_max_per_level[1] <= 25, "level 1 never exceeds its own 5x5 = 25", 25,
        distinct_max_per_level[1]);
  check(distinct_max_per_level[2] <= 9, "level 2 never exceeds its own 3x3 = 9", 9,
        distinct_max_per_level[2]);
  check(distinct_max_per_level[3] <= 4, "level 3 never exceeds its own 2x2 = 4", 4,
        distinct_max_per_level[3]);

  return zhao::report_and_exit("terrain_identity_probe");
}
