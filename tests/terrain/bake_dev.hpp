// bake_dev.hpp — the shared TERRAIN.BAKE device driver.
//
// One place that knows how to run a whole patch-bake through
// `zhao_terrain_bake`: offer the stamp record, answer the block's 33x33 layer
// reads, drain the layer-B stream, answer its 32x32 cell reads and collect the
// §3.4 transitions. The directed suite, both random lanes and the
// BAKE -> PATCH chain all drive the block through THIS, so a handshake mistake
// cannot be made three different ways.
//
// The block owns the sweep (chosen law B1), so the driver is a MEMORY, not a
// scheduler: it reads `vtx_vi_o`/`vtx_vj_o` and `cell_ci_o`/`cell_cj_o` and
// serves the word the block asked for. That is also what makes the scan-order
// check real — nothing here assumes the order, it records what was requested.

#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vzhao_terrain_bake.h"

#include "zhao_sim.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_terrain.hpp"

namespace bdev {

constexpr int kLat = 33;
constexpr int kCells = 32;
constexpr int kVerts = kLat * kLat;          // 1,089
constexpr int kCellCount = kCells * kCells;  // 1,024

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

/** One patch-bake record — the block's `stamp_results` packet. */
struct StampRec {
  uint16_t patch_id = 0;
  int32_t cx = 0, cz = 0;
  int32_t radius = 0;
  int32_t depth_from = 0, depth_to = 0;
  uint16_t src_id = 0;
};

struct BreachEv {
  uint16_t ci = 0, cj = 0;
  uint8_t state = 0;
};

struct BakeOut {
  std::vector<int16_t> scar{};        // layer B out, 1,089, in lattice index order
  std::vector<uint8_t> touched{};     // inside the stencil
  std::vector<uint8_t> meets{};       // base + scar <= bottom
  std::vector<uint8_t> clamped{};     // the no_bake clamp fired
  std::vector<uint8_t> cell_state{};  // layer D out, 1,024 (breach phase only)
  std::vector<BreachEv> events{};
  std::vector<uint32_t> vtx_order{};   // the vertex indices the block asked for
  std::vector<uint32_t> cell_order{};  // the cell indices the block asked for
  bool breach_ran = false;
  bool timed_out = false;
  uint64_t cycles_total = 0;    // command accept -> bake_done
  uint64_t cycles_dig = 0;      // command accept -> dig_done
  uint32_t texels_touched = 0;  // the counter's delta over this bake
  uint32_t breach_events = 0;
  uint32_t saturations = 0;
  uint32_t nobake_clamps = 0;
};

inline void reset_dut(Vzhao_terrain_bake& d) {
  d.rst_n = 0;
  d.frame_start_i = 0;
  d.cmd_valid_i = 0;
  d.vtx_valid_i = 0;
  d.cell_valid_i = 0;
  d.sc_ready_i = 0;
  d.cs_ready_i = 0;
  d.eval();
  for (int i = 0; i < 2; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
  zhao::tick(d);
}

/** The §3.3 no_bake corner shadow of one lattice vertex, from layer D. */
inline bool nobake_shadow(const zref::render::TerrainPatch& p, int i, int j) {
  const int w = p.width, h = p.height;
  if (p.cell_state.size() != static_cast<size_t>(w - 1) * static_cast<size_t>(h - 1)) return false;
  for (int cj = j - 1; cj <= j; ++cj) {
    for (int ci = i - 1; ci <= i; ++ci) {
      if (ci < 0 || cj < 0 || ci >= w - 1 || cj >= h - 1) continue;
      if (p.cell_state[static_cast<size_t>(cj) * (w - 1) + ci] & zref::terrain::kNoBakeBit)
        return true;
    }
  }
  return false;
}

/**
 * Run ONE patch-bake. `p` supplies layers A/B/C/D and the envelope; the result
 * carries the block's layer-B and layer-D answers. `stall_mod` > 1 makes the
 * driver withhold `valid` and drop `ready` on a rolling pattern so every
 * handshake is exercised under backpressure; 0 runs flat out (which is what the
 * throughput measurement uses).
 */
inline BakeOut run_bake(Vzhao_terrain_bake& d, const zref::render::TerrainPatch& p,
                        const StampRec& st, int stall_mod = 0, bool open_window = true) {
  BakeOut o;
  o.scar.assign(kVerts, 0);
  o.touched.assign(kVerts, 0);
  o.meets.assign(kVerts, 0);
  o.clamped.assign(kVerts, 0);
  o.cell_state.assign(kCellCount, 0);

  const bool dual = p.bottom.size() == static_cast<size_t>(kVerts);
  const bool cells = p.cell_state.size() == static_cast<size_t>(kCellCount);
  const uint32_t base_touched = d.surface_texels_touched_o;
  const uint32_t base_events = d.breach_events_o;
  const uint32_t base_sat = d.scar_saturations_o;
  const uint32_t base_clamps = d.nobake_clamps_o;

  // ---- offer the record -------------------------------------------------
  // terrain_rules §9.2 caps a FRAME at BAKE_PATCH_BUDGET acceptances, so a
  // driver that never opens a new window would wedge on the 65th bake. Tests
  // that are exercising the budget itself pass `open_window = false`.
  if (open_window) {
    d.frame_start_i = 1;
    zhao::tick(d);
    d.frame_start_i = 0;
  }
  d.cmd_valid_i = 1;
  d.cmd_patch_id_i = st.patch_id;
  d.cmd_cx_i = st.cx;
  d.cmd_cz_i = st.cz;
  d.cmd_radius_i = st.radius;
  d.cmd_depth_from_i = st.depth_from;
  d.cmd_depth_to_i = st.depth_to;
  d.cmd_env_x0_i = p.env_x0;
  d.cmd_env_z0_i = p.env_z0;
  d.cmd_env_x1_i = p.env_x1;
  d.cmd_env_z1_i = p.env_z1;
  d.cmd_dual_i = dual ? 1 : 0;
  d.cmd_cells_i = cells ? 1 : 0;
  d.cmd_src_id_i = st.src_id;
  d.eval();
  int guard = 0;
  while (!d.cmd_ready_o) {
    zhao::tick(d);
    d.eval();
    if (++guard > 4096) {
      o.timed_out = true;
      d.cmd_valid_i = 0;
      return o;
    }
  }
  zhao::tick(d);
  d.cmd_valid_i = 0;

  // ---- the sweep ---------------------------------------------------------
  int step = 0;
  bool done = false;
  uint64_t cyc = 0;
  const int64_t kMaxCycles = 200000;
  while (!done) {
    d.eval();  // settle the request addresses and the phase flags
    const int vi = d.vtx_vi_o, vj = d.vtx_vj_o;
    const size_t vk = static_cast<size_t>(vj) * kLat + vi;
    const int ci = d.cell_ci_o, cj = d.cell_cj_o;
    const size_t ck = static_cast<size_t>(cj) * kCells + ci;

    const bool hold = stall_mod > 1 && ((step % stall_mod) == 0);
    const bool hold2 = stall_mod > 1 && ((step % stall_mod) == 1);
    ++step;

    d.vtx_base_i = p.heights[vk];
    d.vtx_scar_i = p.scar.empty() ? 0 : p.scar[vk];
    d.vtx_bottom_i = dual ? p.bottom[vk] : 0;
    d.vtx_nobake_i = nobake_shadow(p, vi, vj) ? 1 : 0;
    d.vtx_valid_i = hold ? 0 : 1;
    d.cell_state_i = cells ? p.cell_state[ck] : 0;
    d.cell_valid_i = hold ? 0 : 1;
    d.sc_ready_i = hold2 ? 0 : 1;
    d.cs_ready_i = hold2 ? 0 : 1;
    d.eval();

    if (d.vtx_valid_i && d.vtx_ready_o) o.vtx_order.push_back(static_cast<uint32_t>(vk));
    if (d.cell_valid_i && d.cell_ready_o) o.cell_order.push_back(static_cast<uint32_t>(ck));
    if (d.sc_valid_o && d.sc_ready_i) {
      const size_t k = static_cast<size_t>(d.sc_vj_o) * kLat + d.sc_vi_o;
      if (k < static_cast<size_t>(kVerts)) {
        o.scar[k] = static_cast<int16_t>(d.sc_scar_o);
        o.touched[k] = static_cast<uint8_t>(d.sc_touched_o);
        o.meets[k] = static_cast<uint8_t>(d.sc_meets_o);
        o.clamped[k] = static_cast<uint8_t>(d.sc_clamped_o);
      }
    }
    if (d.cs_valid_o && d.cs_ready_i) {
      const size_t k = static_cast<size_t>(d.cs_cj_o) * kCells + d.cs_ci_o;
      if (k < static_cast<size_t>(kCellCount)) o.cell_state[k] = static_cast<uint8_t>(d.cs_state_o);
      if (d.cs_event_o) {
        o.events.push_back(BreachEv{static_cast<uint16_t>(d.cs_ci_o),
                                    static_cast<uint16_t>(d.cs_cj_o),
                                    static_cast<uint8_t>(d.cs_sub_o)});
      }
    }
    if (d.breach_active_o) o.breach_ran = true;
    if (d.dig_done_o) o.cycles_dig = cyc + 1;
    if (d.bake_done_o) done = true;

    zhao::tick(d);
    ++cyc;
    if (static_cast<int64_t>(cyc) > kMaxCycles) {
      o.timed_out = true;
      break;
    }
  }
  // drain the last published word
  d.vtx_valid_i = 0;
  d.cell_valid_i = 0;
  d.sc_ready_i = 1;
  d.cs_ready_i = 1;
  for (int i = 0; i < 4; ++i) {
    d.eval();
    if (d.sc_valid_o && d.sc_ready_i) {
      const size_t k = static_cast<size_t>(d.sc_vj_o) * kLat + d.sc_vi_o;
      if (k < static_cast<size_t>(kVerts)) {
        o.scar[k] = static_cast<int16_t>(d.sc_scar_o);
        o.touched[k] = static_cast<uint8_t>(d.sc_touched_o);
        o.meets[k] = static_cast<uint8_t>(d.sc_meets_o);
        o.clamped[k] = static_cast<uint8_t>(d.sc_clamped_o);
      }
    }
    if (d.cs_valid_o && d.cs_ready_i) {
      const size_t k = static_cast<size_t>(d.cs_cj_o) * kCells + d.cs_ci_o;
      if (k < static_cast<size_t>(kCellCount)) o.cell_state[k] = static_cast<uint8_t>(d.cs_state_o);
      if (d.cs_event_o) {
        o.events.push_back(BreachEv{static_cast<uint16_t>(d.cs_ci_o),
                                    static_cast<uint16_t>(d.cs_cj_o),
                                    static_cast<uint8_t>(d.cs_sub_o)});
      }
    }
    zhao::tick(d);
    ++cyc;
  }
  d.sc_ready_i = 0;
  d.cs_ready_i = 0;
  o.cycles_total = cyc;
  o.texels_touched = d.surface_texels_touched_o - base_touched;
  o.breach_events = d.breach_events_o - base_events;
  o.saturations = d.scar_saturations_o - base_sat;
  o.nobake_clamps = d.nobake_clamps_o - base_clamps;
  if (!o.breach_ran) o.cell_state.assign(kCellCount, 0);
  return o;
}

/** The oracle: `bake_dig` then `apply_breach_law`, on a copy of the patch. */
inline zref::render::TerrainPatch oracle_bake(const zref::render::TerrainPatch& p,
                                              const StampRec& st,
                                              std::vector<zref::terrain::BreachEvent>* ev_out,
                                              zref::SatLedger* L = nullptr) {
  zref::render::TerrainPatch ref = p;
  if (ref.scar.size() != static_cast<size_t>(kVerts)) ref.scar.assign(kVerts, 0);
  const zref::terrain::DigStamp ds{st.cx, st.cz, st.radius};
  zref::terrain::bake_dig(ref, ds, zref::fx16{st.depth_from}, zref::fx16{st.depth_to}, L);
  const std::vector<zref::terrain::BreachEvent> ev = zref::terrain::apply_breach_law(ref);
  if (ev_out != nullptr) *ev_out = ev;
  return ref;
}

/** A blank 33x33 patch on a 2 m pitch, envelope [x0, x0 + 64 m). */
inline zref::render::TerrainPatch make_patch(int32_t x0, int32_t z0, int32_t span) {
  zref::render::TerrainPatch p;
  p.width = kLat;
  p.height = kLat;
  p.env_x0 = x0;
  p.env_z0 = z0;
  p.env_x1 = x0 + span;
  p.env_z1 = z0 + span;
  p.heights.assign(kVerts, 0);
  p.scar.assign(kVerts, 0);
  return p;
}

}  // namespace bdev
