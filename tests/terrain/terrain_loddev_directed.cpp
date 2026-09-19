// terrain_loddev_directed.cpp -- TERRAIN.LOD's deviations (owner ruling R8),
// differentially against zref::terrain::lod_deviation.
//
// Built TWICE from this one file:
//   test_terrain_loddev_directed       the block at its DEFAULT selector, which
//                                      must equal zref::terrain::
//                                      kLodDevIncludeBoundary -- the owner's pick
//                                      is one line in each language and this
//                                      build FAILS if only one of them moves;
//   test_terrain_loddev_mesh_directed  -GDEV_INCLUDE_BOUNDARY=1, against the
//                                      mesh reading explicitly.
//
// WHAT IT GUARDS, beyond "the numbers match":
//   * the two readings must DIFFER on the lattices used, or the selector check
//     is vacuous -- asserted before it is relied on;
//   * the block must measure EXACTLY the vertices zref measures: its
//     `vertices_measured_o` is compared with a count taken by the same loop, and
//     `lattice_reads_o` must be exactly three per measured vertex. An extra or
//     missing vertex whose deviation happened not to be the maximum would pass a
//     value-only check;
//   * `clipped_o` FIRES on a lattice at the rails (|d| wider than 24 bits),
//     which is reachable with legal stimulus, so no mutant is owed;
//   * a record is HELD while `dev_ready_i` is low.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#include "verilated.h"

#ifdef LODDEV_MESH
#include "Vzhao_terrain_loddev_mesh.h"
using Dut = Vzhao_terrain_loddev_mesh;
constexpr bool kBuildIncludesBoundary = true;
#else
#include "Vzhao_terrain_loddev.h"
using Dut = Vzhao_terrain_loddev;
constexpr bool kBuildIncludesBoundary = false;
#endif

#include "zhao_sim.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_tess.hpp"

namespace zt = zref::terrain;

namespace {

int g_fail = 0;
int g_checks = 0;

void cke(uint64_t want, uint64_t got, const char* what) {
  ++g_checks;
  if (want != got) {
    ++g_fail;
    std::printf("FAIL: %s -- expected %llu, got %llu\n", what,
                static_cast<unsigned long long>(want), static_cast<unsigned long long>(got));
  }
}

constexpr int kW = 33;

zt::ComposedLattice make_lattice(int kind, uint32_t seed) {
  zt::ComposedLattice lat;
  lat.w = kW;
  lat.h = kW;
  lat.dual = true;
  lat.top.resize(kW * kW);
  lat.bottom.resize(kW * kW);
  std::mt19937 rng(seed);
  for (int vj = 0; vj < kW; ++vj) {
    for (int vi = 0; vi < kW; ++vi) {
      int32_t t = 0, b = 0;
      switch (kind) {
        case 0: {  // smooth rolling ground, a few metres of relief (fx16)
          const double x = vi / 32.0, z = vj / 32.0;
          t = static_cast<int32_t>(65536.0 * (3.0 * std::sin(6.1 * x + 1.3) * std::cos(4.7 * z) +
                                              1.5 * std::sin(13.0 * x * z)));
          b = t - static_cast<int32_t>(65536.0 * (4.0 + std::cos(9.0 * x)));
          break;
        }
        case 1: {  // rough: independent noise, +-2^20
          t = static_cast<int32_t>(rng() & 0x1FFFFF) - (1 << 20);
          b = static_cast<int32_t>(rng() & 0x1FFFFF) - (1 << 20);
          break;
        }
        default: {  // at the RAILS: saturating halves, and |d| past 24 bits
          const uint32_t r = rng();
          t = (r & 1) ? INT32_MAX - static_cast<int32_t>(r & 0xFF)
                      : INT32_MIN + static_cast<int32_t>((r >> 8) & 0xFF);
          b = (r & 2) ? INT32_MIN : INT32_MAX;
          break;
        }
      }
      lat.top[static_cast<size_t>(vj) * kW + vi] = t;
      lat.bottom[static_cast<size_t>(vj) * kW + vi] = b;
    }
  }
  return lat;
}

// zref's own loop, counting the vertices it MEASURES (not skips).
int measured_count(bool include_boundary) {
  int n = 0;
  for (int sp = 0; sp < 16; ++sp) {
    const int ox = (sp & 3) * 8, oz = (sp >> 2) * 8;
    for (int level = 1; level <= 3; ++level) {
      const int s = 1 << level, sc = s << 1;
      for (int vj = oz; vj <= oz + 8; ++vj)
        for (int vi = ox; vi <= ox + 8; ++vi) {
          const bool on_border = vi == ox || vi == ox + 8 || vj == oz || vj == oz + 8;
          if (on_border && !include_boundary) continue;
          const bool xc = (vi & (sc - 1)) == 0, zc = (vj & (sc - 1)) == 0;
          if (xc && zc) continue;
          if (zc) {
            if (vi - s < 0 || vi + s >= kW) continue;
          } else if (xc) {
            if (vj - s < 0 || vj + s >= kW) continue;
          } else if (vi - s < 0 || vi + s >= kW || vj - s < 0 || vj + s >= kW) {
            continue;
          }
          ++n;
        }
    }
  }
  return n;
}

struct Rec {
  uint32_t sp, surf, d1, d2, d3;
};

long long g_cycles = 0;

std::vector<Rec> run(Dut& t, const zt::ComposedLattice& lat, int surf, uint32_t stall_seed) {
  std::vector<Rec> out;
  std::mt19937 rng(stall_seed);
  const auto h_at = [&](int vi, int vj, int s) -> int32_t {
    const size_t k = static_cast<size_t>(vj) * kW + vi;
    return s ? lat.bottom[k] : lat.top[k];
  };
  t.start_valid_i = 1;
  t.start_surface_i = surf;
  t.start_src_id_i = 0x5A00 + surf;
  t.eval();
  zhao::tick(t);
  t.start_valid_i = 0;
  t.eval();
  long long guard = 0;
  bool held_checked = false;
  while (out.size() < 16 && ++guard < 400000) {
    const bool req = t.lat_req_o;
    const int vi = t.lat_vi_o, vj = t.lat_vj_o, s = t.lat_surface_o;
    // A record is offered: sometimes refuse it for a few cycles and check it holds.
    bool take = false;
    const bool offered = t.dev_valid_o;
    Rec r{};
    if (offered) {
      r = Rec{t.dev_sp_o, t.dev_surface_o, t.dev1_o, t.dev2_o, t.dev3_o};
      take = (rng() % 3) != 0;
    }
    t.dev_ready_i = take;
    t.eval();
    zhao::tick(t);
    ++g_cycles;
    if (req) t.lat_h_i = static_cast<uint32_t>(h_at(vi, vj, s));
    t.dev_ready_i = 0;
    t.eval();
    if (take) {
      out.push_back(r);
    } else if (offered) {
      // refused: the same record must still be standing
      if (!held_checked) {
        cke(1, t.dev_valid_o, "a refused record stays offered");
        cke(r.sp, t.dev_sp_o, "a refused record keeps its subpatch");
        cke(r.d3, t.dev3_o, "a refused record keeps its deviation");
        held_checked = true;
      }
    }
  }
  for (int k = 0; k < 4 && !t.done_o; ++k) { zhao::tick(t); ++g_cycles; t.eval(); }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut t;
  t.start_valid_i = 0;
  t.dev_ready_i = 0;
  t.lat_h_i = 0;
  t.rst_n = 0;
  for (int i = 0; i < 3; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();

  // THE SELECTOR, checked first. The default build's reading IS the zref
  // constant's; if either default moved alone, this compares a block reading
  // one law against an oracle reading the other and fails below on the
  // "readings differ" lattice.
#ifndef LODDEV_MESH
  const bool oracle_reading = zt::kLodDevIncludeBoundary;
  std::printf("selector: RTL default DEV_INCLUDE_BOUNDARY=%d, zref kLodDevIncludeBoundary=%d\n",
              kBuildIncludesBoundary ? 1 : 0, oracle_reading ? 1 : 0);
#else
  const bool oracle_reading = true;
#endif

  int differ = 0;
  uint32_t prev_measured = t.vertices_measured_o, prev_reads = t.lattice_reads_o;
  uint32_t prev_records = t.records_o;
  for (int kind = 0; kind < 3; ++kind) {
    for (uint32_t seed = 1; seed <= (kind == 0 ? 1u : 3u); ++seed) {
      const zt::ComposedLattice lat = make_lattice(kind, seed * 77u + kind);
      for (int surf = 0; surf < 2; ++surf) {
        const std::vector<Rec> got = run(t, lat, surf, seed * 13u + surf);
        cke(16, got.size(), "sixteen records per surface");
        for (size_t i = 0; i < got.size(); ++i) {
          const int ox = (static_cast<int>(i) & 3) * 8, oz = (static_cast<int>(i) >> 2) * 8;
          const zt::Surface sf = surf ? zt::Surface::kUnderside : zt::Surface::kTop;
          cke(i, got[i].sp, "records come in subpatch order");
          cke(static_cast<uint64_t>(surf), got[i].surf, "the record names its surface");
          const uint32_t want[3] = {zt::lod_deviation(lat, sf, ox, oz, 1, oracle_reading),
                                    zt::lod_deviation(lat, sf, ox, oz, 2, oracle_reading),
                                    zt::lod_deviation(lat, sf, ox, oz, 3, oracle_reading)};
          cke(want[0], got[i].d1, "dev1 == zref::terrain::lod_deviation(level 1)");
          cke(want[1], got[i].d2, "dev2 == zref::terrain::lod_deviation(level 2)");
          cke(want[2], got[i].d3, "dev3 == zref::terrain::lod_deviation(level 3)");
          for (int L = 1; L <= 3; ++L)
            if (zt::lod_deviation(lat, sf, ox, oz, L, false) !=
                zt::lod_deviation(lat, sf, ox, oz, L, true))
              ++differ;
        }
        const uint32_t m = t.vertices_measured_o - prev_measured;
        const uint32_t rd = t.lattice_reads_o - prev_reads;
        cke(static_cast<uint64_t>(measured_count(kBuildIncludesBoundary)), m,
            "the block measured exactly the vertices zref measures");
        cke(3ull * m, rd, "exactly three lattice reads per measured vertex");
        cke(16, t.records_o - prev_records, "records_o counted sixteen");
        prev_measured = t.vertices_measured_o;
        prev_reads = t.lattice_reads_o;
        prev_records = t.records_o;
      }
    }
  }
  std::printf("the two readings differ on %d (subpatch, level, surface, lattice) cases\n", differ);
  cke(1, differ > 0 ? 1 : 0, "the lattices DISTINGUISH the readings (else the selector check is vacuous)");
  cke(1, t.clipped_o > 0 ? 1 : 0, "clipped_o FIRED on the rail lattice");
  std::printf("cycles: %lld over %d surface walks (%lld per walk)\n", g_cycles, 14,
              g_cycles / 14);

  std::printf("terrain_loddev_directed (%s reading): %d checks, %d failures\n",
              kBuildIncludesBoundary ? "mesh" : "morph", g_checks, g_fail);
  zhao::exit_hard(g_fail == 0 ? 0 : 1);
}
