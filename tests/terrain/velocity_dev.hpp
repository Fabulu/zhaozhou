// velocity_dev.hpp — the shared TERRAIN.VELOCITY device driver.
//
// One place that knows how to run a whole patch sweep through
// `zhao_terrain_velocity`: start it, answer the address it asks for with that
// vertex's `lanes` velocity words, and drain the height16 lattice stream. The
// directed suite, both random lanes and the throughput measurement all drive
// the block through THIS, so a handshake mistake cannot be made four ways.
//
// The block owns the sweep (chosen law V3), so the driver is a MEMORY, not a
// scheduler: it reads `vtx_vi_o`/`vtx_vj_o` and serves the words for the
// vertex the block asked for. Nothing here assumes the scan order — it RECORDS
// what was requested, which is what makes the z-then-x check a fact.

#pragma once

#include <cstdint>
#include <vector>

#include "Vzhao_terrain_velocity.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_patch.hpp"  // zref::terrain::subpatch_mask (the ONE rule)
#include "zref/zref_terrain_velocity.hpp"

namespace vdev {

constexpr int kLat = 33;
constexpr int kVerts = kLat * kLat;  // 1,089 (terrain_rules §2)

// Deterministic: same sequence every run, on every host. splitmix64.
struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed) {}
  uint64_t next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  int32_t range(int32_t lo, int32_t hi) {
    const uint64_t span = static_cast<uint64_t>(hi - lo) + 1;
    return lo + static_cast<int32_t>(next() % span);
  }
  bool chance(int n) { return (next() % static_cast<uint64_t>(n)) == 0; }
};

/**
 * The lane plane for one patch: `lanes` velocity words and `lanes` covers bits
 * per vertex, indexed `(vj * 33 + vi) * lanes + k`. This is FIELD.SEQ.EARTH's
 * out-lane 1 stream (field-ir §7.1) paired with TERRAIN.PATCH's §9.1 answer.
 */
struct LanePlane {
  int lanes = 0;
  std::vector<int32_t> velocity{};  // fx16 raw
  std::vector<uint8_t> covers{};

  void resize(int n_lanes) {
    lanes = n_lanes;
    velocity.assign(static_cast<size_t>(kVerts) * static_cast<size_t>(n_lanes < 1 ? 1 : n_lanes),
                    0);
    covers.assign(static_cast<size_t>(kVerts) * static_cast<size_t>(n_lanes < 1 ? 1 : n_lanes), 0);
  }
  size_t at(int vi, int vj, int k) const {
    return (static_cast<size_t>(vj) * kLat + static_cast<size_t>(vi)) *
               static_cast<size_t>(lanes < 1 ? 1 : lanes) +
           static_cast<size_t>(k);
  }
};

struct SweepOut {
  std::vector<int16_t> velocity{};  // the §4.2 lattice, 1,089, in lattice order
  std::vector<uint8_t> moving{};
  std::vector<uint8_t> covered{};
  std::vector<uint32_t> vtx_order{};  // the vertex indices the block asked for
  std::vector<uint16_t> src_ids{};
  uint16_t moving_mask = 0;
  uint32_t samples = 0;       // terrain_samples_evaluated delta
  uint32_t add_sats = 0;      // SatLedger::add delta
  uint32_t rescale_sats = 0;  // SatLedger::rescale delta
  uint64_t cycles = 0;        // start accept -> last word retired
  bool timed_out = false;
  bool done_pulsed = false;
};

/** Reset with this block's own port set (zhao::reset assumes a byte stream). */
inline void reset_dut(Vzhao_terrain_velocity& dut) {
  dut.rst_n = 0;
  dut.start_valid_i = 0;
  dut.start_lanes_i = 0;
  dut.start_patch_id_i = 0;
  dut.start_src_id_i = 0;
  dut.lane_valid_i = 0;
  dut.lane_velocity_i = 0;
  dut.lane_covers_i = 0;
  dut.vv_ready_i = 0;
  dut.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);
}

/**
 * Run one whole patch sweep.
 *
 * `stall_lane` / `stall_sink` inject backpressure with the given period
 * (0 = never stall), so the handshake is exercised on both sides rather than
 * only in the free-running case. `sink_always_ready` (both stalls 0) is the
 * configuration the throughput measurement uses, because a ledger rate is a
 * statement about the block, not about a stalling testbench.
 */
inline SweepOut run_sweep(Vzhao_terrain_velocity& dut, const LanePlane& plane, uint16_t patch_id,
                          uint16_t src_id, int stall_lane = 0, int stall_sink = 0,
                          uint64_t max_cycles = 400000) {
  SweepOut out;
  out.velocity.assign(kVerts, 0);
  out.moving.assign(kVerts, 0);
  out.covered.assign(kVerts, 0);
  out.src_ids.assign(kVerts, 0);
  const uint32_t base_samples = dut.terrain_samples_evaluated_o;
  const uint32_t base_add = dut.velocity_add_sats_o;
  const uint32_t base_res = dut.velocity_rescale_sats_o;

  // ---- offer the start record until it is accepted -----------------------
  dut.start_valid_i = 1;
  dut.start_lanes_i = static_cast<uint8_t>(plane.lanes);
  dut.start_patch_id_i = patch_id;
  dut.start_src_id_i = src_id;
  dut.lane_valid_i = 0;
  dut.vv_ready_i = 1;
  dut.eval();
  uint64_t guard = 0;
  while (dut.start_ready_o == 0 && guard++ < 64) zhao::tick(dut);
  zhao::tick(dut);  // the accept cycle
  dut.start_valid_i = 0;

  // ---- serve the sweep ----------------------------------------------------
  int emitted = 0;
  int lane_k = 0;
  uint64_t cyc = 0;
  while (emitted < kVerts && cyc < max_cycles) {
    const int vi = static_cast<int>(dut.vtx_vi_o);
    const int vj = static_cast<int>(dut.vtx_vj_o);
    const bool lane_stalled = stall_lane > 0 && (cyc % static_cast<uint64_t>(stall_lane)) == 0;
    const bool sink_stalled = stall_sink > 0 && (cyc % static_cast<uint64_t>(stall_sink)) == 0;

    if (plane.lanes > 0 && !lane_stalled) {
      const size_t idx = plane.at(vi, vj, lane_k);
      dut.lane_valid_i = 1;
      dut.lane_velocity_i = plane.velocity[idx];
      dut.lane_covers_i = plane.covers[idx] != 0 ? 1 : 0;
    } else {
      dut.lane_valid_i = 0;
      dut.lane_velocity_i = 0;
      dut.lane_covers_i = 0;
    }
    dut.vv_ready_i = sink_stalled ? 0 : 1;
    dut.eval();

    const bool lane_taken = dut.lane_valid_i != 0 && dut.lane_ready_o != 0;
    const bool word_taken = dut.vv_valid_o != 0 && dut.vv_ready_i != 0;
    uint32_t take_idx = 0;
    int16_t take_vel = 0;
    uint8_t take_mov = 0, take_cov = 0;
    uint16_t take_src = 0;
    if (word_taken) {
      take_idx = static_cast<uint32_t>(dut.vv_vj_o) * kLat + static_cast<uint32_t>(dut.vv_vi_o);
      take_vel = static_cast<int16_t>(dut.vv_velocity_o);
      take_mov = static_cast<uint8_t>(dut.vv_moving_o);
      take_cov = static_cast<uint8_t>(dut.vv_covered_o);
      take_src = static_cast<uint16_t>(dut.vv_src_id_o);
    }
    if (lane_taken && lane_k == 0)
      out.vtx_order.push_back(static_cast<uint32_t>(vj) * kLat + static_cast<uint32_t>(vi));
    if (plane.lanes == 0 && word_taken) out.vtx_order.push_back(take_idx);

    zhao::tick(dut);
    ++cyc;
    if (dut.patch_done_o != 0) out.done_pulsed = true;

    if (lane_taken) {
      ++lane_k;
      if (lane_k >= plane.lanes) lane_k = 0;
    }
    if (word_taken) {
      out.velocity[take_idx] = take_vel;
      out.moving[take_idx] = take_mov;
      out.covered[take_idx] = take_cov;
      out.src_ids[take_idx] = take_src;
      ++emitted;
    }
  }
  dut.lane_valid_i = 0;
  dut.vv_ready_i = 0;
  dut.eval();
  out.cycles = cyc;
  out.timed_out = emitted < kVerts;
  out.moving_mask = static_cast<uint16_t>(dut.moving_mask_o);
  out.samples = dut.terrain_samples_evaluated_o - base_samples;
  out.add_sats = dut.velocity_add_sats_o - base_add;
  out.rescale_sats = dut.velocity_rescale_sats_o - base_res;
  return out;
}

/** The oracle's whole-patch answer for the same lane plane. */
inline SweepOut oracle_sweep(const LanePlane& plane, uint16_t src_id, zref::SatLedger* L) {
  SweepOut out;
  out.velocity.assign(kVerts, 0);
  out.moving.assign(kVerts, 0);
  out.covered.assign(kVerts, 0);
  out.src_ids.assign(kVerts, src_id);
  std::vector<int32_t> lane(static_cast<size_t>(plane.lanes < 1 ? 1 : plane.lanes), 0);
  std::vector<bool> cov(static_cast<size_t>(plane.lanes < 1 ? 1 : plane.lanes), false);
  for (int vj = 0; vj < kLat; ++vj) {
    for (int vi = 0; vi < kLat; ++vi) {
      for (int k = 0; k < plane.lanes; ++k) {
        const size_t idx = plane.at(vi, vj, k);
        lane[static_cast<size_t>(k)] = plane.velocity[idx];
        cov[static_cast<size_t>(k)] = plane.covers[idx] != 0;
      }
      // std::vector<bool> has no contiguous storage, so the covers array is
      // materialised for the call rather than passed by data() (which does not
      // exist for it) — the oracle takes a plain `const bool*`.
      bool cbuf[16] = {};
      for (int k = 0; k < plane.lanes && k < 16; ++k) cbuf[k] = cov[static_cast<size_t>(k)];
      const zref::terrain::VelocityOut v =
          zref::terrain::velocity_vertex(lane.data(), cbuf, plane.lanes, L);
      const size_t li = static_cast<size_t>(vj) * kLat + static_cast<size_t>(vi);
      out.velocity[li] = v.velocity;
      out.moving[li] = v.moving ? 1 : 0;
      out.covered[li] = v.covered ? 1 : 0;
      if (v.moving) out.moving_mask |= zref::terrain::subpatch_mask(vi, vj);
      ++out.samples;
    }
  }
  return out;
}

}  // namespace vdev
