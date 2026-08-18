// tess_harness.hpp — the lattice/cell-state memory model TERRAIN.TESS reads
// through, shared by the directed and random lanes (tests only).
//
// The block has two REGISTERED read ports: it presents an address in one cycle
// and the datum must be present the next. This model reproduces exactly that
// and nothing more permissive — a model that answered combinationally would
// hide a whole class of pipeline bug, and this block's throughput claim rests
// on issuing one lattice read per clock.

#pragma once

#include <cstdint>
#include <vector>

#include "Vzhao_terrain_tess.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_tess.hpp"

namespace tess_test {

namespace zt = zref::terrain;

/** Drives one subpatch job through the DUT and collects the emitted mesh. */
class Driver {
 public:
  explicit Driver(Vzhao_terrain_tess& dut) : dut_(dut) {}

  void reset() {
    dut_.rst_n = 0;
    dut_.job_valid_i = 0;
    dut_.tri_ready_i = 0;
    dut_.lat_h_i = 0;
    dut_.lat_wx_i = 0;
    dut_.lat_wz_i = 0;
    dut_.cs_substance_i = 0;
    dut_.eval();
    for (int i = 0; i < 2; ++i) zhao::tick(dut_);
    dut_.rst_n = 1;
    dut_.eval();
    zhao::tick(dut_);
    lat_pend_ = false;
    cs_pend_ = false;
  }

  /**
   * Run one job to completion. `stall_mask` gates `tri_ready_i` on a repeating
   * schedule (0 = never stall) so backpressure rides every lane rather than
   * living in one dedicated case. Returns the emitted triangles in order.
   */
  std::vector<zt::MeshTri> run(const zt::ComposedLattice& lat, const zt::SubpatchJob& job,
                               bool* rejected, uint32_t stall_mask = 0, int max_cycles = 20000) {
    std::vector<zt::MeshTri> out;
    *rejected = false;

    dut_.job_valid_i = 1;
    dut_.job_ox_i = static_cast<uint8_t>(job.ox);
    dut_.job_oz_i = static_cast<uint8_t>(job.oz);
    dut_.job_level_i = static_cast<uint8_t>(job.level);
    dut_.job_lvl_nz_i = static_cast<uint8_t>(job.nlevel[zt::kSideNegZ]);
    dut_.job_lvl_pz_i = static_cast<uint8_t>(job.nlevel[zt::kSidePosZ]);
    dut_.job_lvl_nx_i = static_cast<uint8_t>(job.nlevel[zt::kSideNegX]);
    dut_.job_lvl_px_i = static_cast<uint8_t>(job.nlevel[zt::kSidePosX]);
    dut_.job_morph_i = static_cast<uint32_t>(job.morph < 0 ? 0 : job.morph);
    dut_.job_surface_i = job.surface == zt::Surface::kUnderside ? 1 : 0;
    dut_.job_dual_i = lat.dual ? 1 : 0;
    dut_.job_src_id_i = src_id_;

    bool taken = false;
    int idle_run = 0;
    for (int cycle = 0; cycle < max_cycles; ++cycle) {
      dut_.eval();
      if (!taken && dut_.job_ready_o) taken = true;

      // ---- serve the reads requested LAST cycle (registered ports) --------
      if (lat_pend_) {
        const size_t k = static_cast<size_t>(lat_vj_) * static_cast<size_t>(lat.w) +
                         static_cast<size_t>(lat_vi_);
        dut_.lat_h_i = lat_surf_ ? lat.bottom[k] : lat.top[k];
        dut_.lat_wx_i = lat.wx[static_cast<size_t>(lat_vi_)];
        dut_.lat_wz_i = lat.wz[static_cast<size_t>(lat_vj_)];
      } else {
        // Deliberate poison: a block that reads data it did not request must
        // fail loudly rather than pick up a stale value that happens to be
        // right.
        dut_.lat_h_i = 0x5BADF00D;
        dut_.lat_wx_i = 0x5BADF00D;
        dut_.lat_wz_i = 0x5BADF00D;
      }
      if (cs_pend_) {
        dut_.cs_substance_i =
            static_cast<uint8_t>(lat.substance(static_cast<int>(cs_ci_), static_cast<int>(cs_cj_)));
      } else {
        dut_.cs_substance_i = 3;  // "reserved", never SOLID
      }
      dut_.tri_ready_i = stall_mask == 0 ? 1 : (((stall_mask >> (cycle & 31)) & 1u) ^ 1u);
      dut_.eval();

      // ---- observe ---------------------------------------------------------
      const bool lat_req = dut_.lat_req_o != 0;
      const uint8_t nvi = dut_.lat_vi_o, nvj = dut_.lat_vj_o;
      const bool nsurf = dut_.lat_surface_o != 0;
      const bool cs_req = dut_.cs_req_o != 0;
      const uint8_t nci = dut_.cs_ci_o, ncj = dut_.cs_cj_o;

      if (dut_.tri_valid_o && dut_.tri_ready_i) {
        zt::MeshTri t;
        t.ax = static_cast<int32_t>(dut_.ax_o);
        t.ay = static_cast<int32_t>(dut_.ay_o);
        t.az = static_cast<int32_t>(dut_.az_o);
        t.bx = static_cast<int32_t>(dut_.bx_o);
        t.by = static_cast<int32_t>(dut_.by_o);
        t.bz = static_cast<int32_t>(dut_.bz_o);
        t.cx = static_cast<int32_t>(dut_.cx_o);
        t.cy = static_cast<int32_t>(dut_.cy_o);
        t.cz = static_cast<int32_t>(dut_.cz_o);
        out.push_back(t);
        last_surface_ = dut_.surface_o != 0;
        last_src_ = dut_.src_id_o;
        ++transfers_;
      }
      if (dut_.job_reject_o) *rejected = true;

      const bool done = taken && dut_.idle_o && !dut_.job_valid_i;
      zhao::tick(dut_);
      if (taken) dut_.job_valid_i = 0;
      lat_pend_ = lat_req;
      lat_vi_ = nvi;
      lat_vj_ = nvj;
      lat_surf_ = nsurf;
      cs_pend_ = cs_req;
      cs_ci_ = nci;
      cs_cj_ = ncj;
      ++cycles_;
      if (done) {
        if (++idle_run >= 2) break;
      } else {
        idle_run = 0;
      }
    }
    return out;
  }

  void set_src(uint16_t s) { src_id_ = s; }
  uint16_t last_src() const { return last_src_; }
  bool last_surface() const { return last_surface_; }
  uint64_t cycles() const { return cycles_; }
  uint64_t transfers() const { return transfers_; }
  void reset_counters() {
    cycles_ = 0;
    transfers_ = 0;
  }

 private:
  Vzhao_terrain_tess& dut_;
  bool lat_pend_ = false;
  uint8_t lat_vi_ = 0, lat_vj_ = 0;
  bool lat_surf_ = false;
  bool cs_pend_ = false;
  uint8_t cs_ci_ = 0, cs_cj_ = 0;
  uint16_t src_id_ = 0x1234;
  uint16_t last_src_ = 0;
  bool last_surface_ = false;
  uint64_t cycles_ = 0;
  uint64_t transfers_ = 0;
};

/** A 33x33 composed lattice with a power-of-two pitch (terrain_rules §1.3). */
inline zt::ComposedLattice make_lattice(bool dual, int pitch_m = 2) {
  zt::ComposedLattice lat;
  lat.w = lat.h = 33;
  lat.dual = dual;
  lat.wx.resize(33);
  lat.wz.resize(33);
  for (int i = 0; i < 33; ++i) {
    lat.wx[static_cast<size_t>(i)] = (i * pitch_m) << 16;
    lat.wz[static_cast<size_t>(i)] = (i * pitch_m) << 16;
  }
  lat.top.assign(33 * 33, 0);
  if (dual) lat.bottom.assign(33 * 33, -(50 << 16));
  lat.cell_state.assign(32 * 32, zt::kSolid);
  return lat;
}

/** Deterministic relief on both planes: the height16 grid, exactly << 8. */
inline void fill_relief(zt::ComposedLattice& lat, uint32_t seed) {
  uint32_t s = seed;
  const auto next = [&s]() {
    s = s * 1103515245u + 12345u;
    return s >> 16;
  };
  for (int j = 0; j < lat.h; ++j) {
    for (int i = 0; i < lat.w; ++i) {
      const size_t k = static_cast<size_t>(j) * static_cast<size_t>(lat.w) + static_cast<size_t>(i);
      const int32_t base = static_cast<int16_t>(2560 + static_cast<int>(next() % 2048) - 1024);
      lat.top[k] = base << 8;
      if (lat.dual) {
        const int32_t bot = static_cast<int16_t>(-12800 + static_cast<int>(next() % 512));
        lat.bottom[k] = bot << 8;
      }
    }
  }
}

}  // namespace tess_test
