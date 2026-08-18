// lod_dev.hpp — the shared driver for TERRAIN.LOD.
//
// One place that knows the block's two-phase handshake, so the directed lane,
// both random lanes and the composition drive it identically. If the port list
// changes, exactly one file needs editing and every lane stops compiling.

#pragma once

#include <cstdint>
#include <vector>

#include "Vzhao_terrain_lod.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_lod.hpp"

namespace lod_test {

namespace zt = zref::terrain;

constexpr int kNSub = 16;

/** One whole patch job: the governor's targets and sixteen descriptors. */
struct LodJob {
  zt::LodCamera cam[2];
  zt::LodPolicy policy;
  bool dual = false;
  uint8_t edge[4] = {0, 0, 0, 0};  // −z, +z, −x, +x; four 2-bit lanes each
  zt::LodSubpatch sp[kNSub];
  uint16_t src[kNSub] = {0};
};

/** One emitted `lod_target` packet — TERRAIN.TESS's job port, field for field. */
struct LodOut {
  uint8_t ox = 0, oz = 0;
  uint8_t level = 0;
  uint8_t nz = 0, pz = 0, nx = 0, px = 0;
  uint32_t morph = 0;
  uint8_t surface = 0;
  uint8_t dual = 0;
  uint16_t src_id = 0;
  uint8_t hold = 0;
};

inline uint8_t edge_lane(uint8_t e, int k) { return static_cast<uint8_t>((e >> (2 * k)) & 3u); }

/**
 * What the block must emit for one job, straight from `zref::terrain::lod_select`
 * and the neighbour rule.
 */
inline std::vector<LodOut> oracle(const LodJob& job) {
  uint8_t level[kNSub];
  uint32_t morph[kNSub];
  uint8_t hold[kNSub];
  for (int n = 0; n < kNSub; ++n) {
    const zt::LodDecision d = zt::lod_select(job.sp[n], job.cam, job.policy);
    level[n] = static_cast<uint8_t>(d.level);
    morph[n] = static_cast<uint32_t>(d.morph);
    hold[n] = d.hold;
  }
  std::vector<LodOut> out;
  for (int n = 0; n < kNSub; ++n) {
    const int i = n & 3;
    const int j = n >> 2;
    LodOut o;
    o.ox = static_cast<uint8_t>(i * 8);
    o.oz = static_cast<uint8_t>(j * 8);
    o.level = level[n];
    o.nz = (j != 0) ? level[n - 4] : edge_lane(job.edge[0], i);
    o.pz = (j != 3) ? level[n + 4] : edge_lane(job.edge[1], i);
    o.nx = (i != 0) ? level[n - 1] : edge_lane(job.edge[2], j);
    o.px = (i != 3) ? level[n + 1] : edge_lane(job.edge[3], j);
    o.morph = morph[n];
    o.dual = job.dual ? 1 : 0;
    o.src_id = job.src[n];
    o.hold = hold[n];
    o.surface = 0;
    out.push_back(o);
    if (job.dual) {
      o.surface = 1;
      out.push_back(o);
    }
  }
  return out;
}

/** The DUT driver. */
class Dev {
 public:
  explicit Dev(Vzhao_terrain_lod& dut) : dut_(dut) {}

  void reset() {
    dut_.rst_n = 0;
    dut_.sp_valid_i = 0;
    dut_.out_ready_i = 0;
    dut_.cam0_en_i = 0;
    dut_.cam1_en_i = 0;
    dut_.eval();
    for (int i = 0; i < 2; ++i) zhao::tick(dut_);
    dut_.rst_n = 1;
    dut_.eval();
    zhao::tick(dut_);
  }

  /** Run one whole patch job and collect every emitted packet. */
  std::vector<LodOut> run(const LodJob& job, uint32_t stall_mask = 0) {
    dut_.cam0_x_i = static_cast<uint32_t>(job.cam[0].ex);
    dut_.cam0_y_i = static_cast<uint32_t>(job.cam[0].ey);
    dut_.cam0_z_i = static_cast<uint32_t>(job.cam[0].ez);
    dut_.cam0_scale_i = job.cam[0].scale;
    dut_.cam0_en_i = job.cam[0].enabled ? 1 : 0;
    dut_.cam1_x_i = static_cast<uint32_t>(job.cam[1].ex);
    dut_.cam1_y_i = static_cast<uint32_t>(job.cam[1].ey);
    dut_.cam1_z_i = static_cast<uint32_t>(job.cam[1].ez);
    dut_.cam1_scale_i = job.cam[1].scale;
    dut_.cam1_en_i = job.cam[1].enabled ? 1 : 0;
    dut_.hyst_i = job.policy.hyst;
    dut_.min_hold_i = job.policy.min_hold;
    dut_.morph_step_i = static_cast<uint32_t>(job.policy.morph_step);
    dut_.dual_i = job.dual ? 1 : 0;
    dut_.edge_nz_i = job.edge[0];
    dut_.edge_pz_i = job.edge[1];
    dut_.edge_nx_i = job.edge[2];
    dut_.edge_px_i = job.edge[3];

    const size_t want = job.dual ? 32u : 16u;
    std::vector<LodOut> out;
    int pushed = 0;
    for (int cycle = 0; cycle < 40000 && out.size() < want; ++cycle) {
      const bool ready = (stall_mask == 0) || (((stall_mask >> (cycle & 31)) & 1u) == 0);
      dut_.out_ready_i = ready ? 1 : 0;

      if (pushed < kNSub) {
        const zt::LodSubpatch& s = job.sp[pushed];
        dut_.sp_valid_i = 1;
        dut_.sp_cx_i = static_cast<uint32_t>(s.cx);
        dut_.sp_cy_i = static_cast<uint32_t>(s.cy);
        dut_.sp_cz_i = static_cast<uint32_t>(s.cz);
        dut_.sp_dev1_i = s.dev[1] & 0xFFFFFFu;
        dut_.sp_dev2_i = s.dev[2] & 0xFFFFFFu;
        dut_.sp_dev3_i = s.dev[3] & 0xFFFFFFu;
        dut_.sp_prev_level_i = static_cast<uint8_t>(s.level & 3);
        dut_.sp_prev_morph_i = static_cast<uint32_t>(s.morph);
        dut_.sp_hold_i = s.hold;
        dut_.sp_src_id_i = job.src[pushed];
      } else {
        dut_.sp_valid_i = 0;
        // Poison: a block that latches a descriptor it was not offered shows up
        // as a wrong decision rather than as nothing at all.
        dut_.sp_cx_i = 0x5BADF00Du;
        dut_.sp_dev1_i = 0xBADF00u;
      }

      dut_.eval();
      const bool take = (pushed < kNSub) && dut_.sp_ready_o;
      if (dut_.out_valid_o && ready) {
        LodOut o;
        o.ox = static_cast<uint8_t>(dut_.out_ox_o);
        o.oz = static_cast<uint8_t>(dut_.out_oz_o);
        o.level = static_cast<uint8_t>(dut_.out_level_o);
        o.nz = static_cast<uint8_t>(dut_.out_lvl_nz_o);
        o.pz = static_cast<uint8_t>(dut_.out_lvl_pz_o);
        o.nx = static_cast<uint8_t>(dut_.out_lvl_nx_o);
        o.px = static_cast<uint8_t>(dut_.out_lvl_px_o);
        o.morph = static_cast<uint32_t>(dut_.out_morph_o);
        o.surface = static_cast<uint8_t>(dut_.out_surface_o);
        o.dual = static_cast<uint8_t>(dut_.out_dual_o);
        o.src_id = static_cast<uint16_t>(dut_.out_src_id_o);
        o.hold = static_cast<uint8_t>(dut_.out_hold_o);
        out.push_back(o);
      }
      zhao::tick(dut_);
      if (take) ++pushed;
    }
    dut_.sp_valid_i = 0;
    dut_.out_ready_i = 0;
    dut_.eval();
    return out;
  }

 private:
  Vzhao_terrain_lod& dut_;
};

/** A flat job: one camera, no hysteresis, no hold, no morph, all sixteen alike. */
inline LodJob plain_job(int32_t eye_z, uint16_t scale) {
  LodJob j;
  j.cam[0].ex = 0;
  j.cam[0].ey = 0;
  j.cam[0].ez = eye_z;
  j.cam[0].scale = scale;
  j.cam[0].enabled = true;
  j.cam[1].enabled = false;
  j.policy.hyst = 256;
  j.policy.min_hold = 0;
  j.policy.morph_step = 0;
  for (int n = 0; n < kNSub; ++n) {
    j.sp[n].cx = 0;
    j.sp[n].cy = 0;
    j.sp[n].cz = 0;
    j.sp[n].dev[1] = 0;
    j.sp[n].dev[2] = 0;
    j.sp[n].dev[3] = 0;
    j.sp[n].level = 0;
    j.sp[n].morph = 0;
    j.sp[n].hold = 255;  // any hold gate is already satisfied
    j.src[n] = static_cast<uint16_t>(0x100 + n);
  }
  return j;
}

}  // namespace lod_test
