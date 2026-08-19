// terrain_velocity_chain.cpp — the PATCH -> VELOCITY seam, both blocks REAL.
//
// TERRAIN.PATCH owns the terrain_rules §9.1 field list and decides the
// CLOSED-INTERVAL footprint test once (its chosen law 2: "the footprint test
// lives here, not upstream"). TERRAIN.VELOCITY consumes the same per-vertex
// lane stream for earth out-lane 1 and takes that answer on `fld_covers_o`
// rather than holding a second 16-rectangle list. That is a seam, and a seam is
// only proved by running both blocks off ONE list — which is what this file
// does. Nothing here invents a covers bit; the wire carries it.
//
// It also settles, with the real blocks rather than with prose, the argument
// the velocity contract's rejected law V5 rests on:
//
//   TERRAIN.PATCH's `dirty` bit is DISPLACEMENT (`live_top != fx(base)`) and
//   TERRAIN.VELOCITY's `moving` bit is the RATE. A wave's leading edge moves
//   with no displacement yet; its crest is displaced with zero rate. Neither
//   subpatch mask contains the other, so gating a velocity sweep by the dirty
//   mask would drop exactly the leading edge of every wake — and a wake is what
//   this console is being built for (untitled-game DESIGN.md, "Wacko mode").
//
// And it MEASURES the composed pair's sustained rate under the wake workload,
// because a chain's throughput is not either block's throughput and the
// difference is the interesting number.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_patch.h"
#include "Vzhao_terrain_velocity.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_patch.hpp"
#include "zref/zref_terrain_velocity.hpp"

using zhao::check;
namespace zt = zref::terrain;

namespace {

constexpr int kLat = 33;
constexpr int kVerts = kLat * kLat;
constexpr int32_t kPitch = 1 << 16;  // 1 m per lattice step, fx16

struct Both {
  Vzhao_terrain_patch patch;
  Vzhao_terrain_velocity vel;
};

// The seam is COMBINATIONAL: TERRAIN.PATCH's `fld_covers_o` feeds
// TERRAIN.VELOCITY's `lane_covers_i` inside one cycle. It is re-driven after
// every eval of the producer, which is what a wire between two blocks in one
// clock domain does.
void settle(Both& b) {
  b.patch.eval();
  b.vel.lane_covers_i = b.patch.fld_covers_o;
  b.vel.eval();
}

void tick_pair(Both& b) {
  b.patch.clk = 0;
  b.vel.clk = 0;
  settle(b);
  b.patch.clk = 1;
  b.vel.clk = 1;
  settle(b);
  b.patch.clk = 0;
  b.vel.clk = 0;
  settle(b);
}

void reset_pair(Both& b) {
  b.patch.rst_n = 0;
  b.patch.list_clear_i = 0;
  b.patch.patch_id_i = 0;
  b.patch.fld_add_valid_i = 0;
  b.patch.vtx_valid_i = 0;
  b.patch.fld_valid_i = 0;
  b.patch.st_ready_i = 1;
  b.vel.rst_n = 0;
  b.vel.start_valid_i = 0;
  b.vel.start_lanes_i = 0;
  b.vel.start_patch_id_i = 0;
  b.vel.start_src_id_i = 0;
  b.vel.lane_valid_i = 0;
  b.vel.lane_velocity_i = 0;
  b.vel.lane_covers_i = 0;
  b.vel.vv_ready_i = 1;
  settle(b);
  for (int i = 0; i < 2; ++i) tick_pair(b);
  b.patch.rst_n = 1;
  b.vel.rst_n = 1;
  settle(b);
  tick_pair(b);
}

/** One patch, its §9.1 list, its three height16 planes and the two out-lanes. */
struct Wake {
  std::vector<zt::FieldRecord> records;
  std::vector<int16_t> base, scar, bottom;
  std::vector<int32_t> wx, wz;
  std::vector<int32_t> height;    // earth out-lane 0, per (vertex, lane)
  std::vector<int32_t> velocity;  // earth out-lane 1, per (vertex, lane)

  int lanes() const { return static_cast<int>(records.size()); }
  size_t at(int vi, int vj, int k) const {
    const size_t n = records.empty() ? 1 : records.size();
    return (static_cast<size_t>(vj) * kLat + static_cast<size_t>(vi)) * n + static_cast<size_t>(k);
  }
  void init(int n_lanes) {
    records.assign(static_cast<size_t>(n_lanes), zt::FieldRecord{});
    base.assign(kVerts, 0);
    scar.assign(kVerts, 0);
    bottom.assign(kVerts, static_cast<int16_t>(-8192));
    wx.resize(kLat);
    wz.resize(kLat);
    for (int i = 0; i < kLat; ++i) {
      wx[static_cast<size_t>(i)] = (i - 16) * kPitch;
      wz[static_cast<size_t>(i)] = (i - 16) * kPitch;
    }
    const size_t n = static_cast<size_t>(kVerts) * static_cast<size_t>(n_lanes < 1 ? 1 : n_lanes);
    height.assign(n, 0);
    velocity.assign(n, 0);
  }
};

struct ChainOut {
  std::vector<int32_t> live_top;
  std::vector<uint8_t> dirty;
  std::vector<int16_t> vel_word;
  std::vector<uint8_t> moving;
  std::vector<uint8_t> covered;
  uint16_t dirty_mask = 0;
  uint16_t moving_mask = 0;
  uint64_t cycles = 0;
  int lane_desync = 0;  // a cycle where only one of the two took the word
  bool timed_out = false;
};

/**
 * Drive the pair over one whole patch.
 *
 * TERRAIN.VELOCITY owns the sweep (its chosen law V3), so the driver follows
 * ITS address and hands the same vertex to TERRAIN.PATCH. The two lane streams
 * advance together — a word is offered only when BOTH blocks can take it, which
 * is the only way `fld_covers_o` can be the answer for the lane VELOCITY is
 * folding. Any cycle where exactly one side took a word is counted as a
 * desync and asserted to be zero, so the lockstep is a measurement rather than
 * an assumption.
 */
ChainOut run_chain(Both& b, const Wake& w, uint16_t patch_id) {
  ChainOut out;
  out.live_top.assign(kVerts, 0);
  out.dirty.assign(kVerts, 0);
  out.vel_word.assign(kVerts, 0);
  out.moving.assign(kVerts, 0);
  out.covered.assign(kVerts, 0);

  // ---- load the §9.1 list into TERRAIN.PATCH ------------------------------
  bool first = true;
  for (int a = 0; a < w.lanes(); ++a) {
    b.patch.list_clear_i = first ? 1 : 0;
    b.patch.patch_id_i = patch_id;
    b.patch.fld_add_valid_i = 1;
    b.patch.fld_add_x0_i = w.records[static_cast<size_t>(a)].x0;
    b.patch.fld_add_z0_i = w.records[static_cast<size_t>(a)].z0;
    b.patch.fld_add_x1_i = w.records[static_cast<size_t>(a)].x1;
    b.patch.fld_add_z1_i = w.records[static_cast<size_t>(a)].z1;
    b.patch.fld_add_hash_i = 0;
    b.patch.fld_add_cmd_i = static_cast<uint16_t>(a);
    tick_pair(b);
    b.patch.fld_add_valid_i = 0;
    b.patch.list_clear_i = 0;
    first = false;
  }
  if (first) {
    b.patch.list_clear_i = 1;
    tick_pair(b);
    b.patch.list_clear_i = 0;
  }

  // ---- start the velocity sweep with the list size PATCH actually holds ---
  b.vel.start_valid_i = 1;
  b.vel.start_lanes_i = static_cast<uint8_t>(b.patch.fields_active_o);
  b.vel.start_patch_id_i = patch_id;
  b.vel.start_src_id_i = patch_id;
  settle(b);
  uint64_t guard = 0;
  while (b.vel.start_ready_o == 0 && guard++ < 64) tick_pair(b);
  tick_pair(b);
  b.vel.start_valid_i = 0;

  // ---- the joint walk ------------------------------------------------------
  const int n_lanes = w.lanes();
  int vel_words = 0;     // velocity lattice words retired
  int patch_words = 0;   // patch_state records retired (same sweep order)
  bool holding = false;  // TERRAIN.PATCH has accepted the current vertex
  int vtx_given = 0;     // vertices handed to TERRAIN.PATCH: exactly 1,089
  int lane_k = 0;
  uint64_t cyc = 0;
  while ((vel_words < kVerts || patch_words < kVerts) && cyc < 400000) {
    const int vi = static_cast<int>(b.vel.vtx_vi_o);
    const int vj = static_cast<int>(b.vel.vtx_vj_o);
    const size_t li = static_cast<size_t>(vj) * kLat + static_cast<size_t>(vi);

    // TERRAIN.PATCH spends one cycle ACCEPTING a vertex before it will take
    // that vertex's lane words; with an empty list it composes in the accept
    // cycle itself and never goes busy, so `holding` is a lane-chain state, not
    // a retirement state. Getting that wrong is what a desync counter is for.
    // ...and the patch holds exactly 1,089 vertices. The
    // first version of this driver kept offering the final vertex while
    // draining TERRAIN.PATCH's last record, which left that block BUSY holding
    // a phantom vertex and deadlocked the NEXT patch. Recorded because the
    // symptom (a later sweep making no progress at all) points nowhere near it.
    b.patch.vtx_valid_i = (holding || vtx_given >= kVerts) ? 0 : 1;
    b.patch.base_i = w.base[li];
    b.patch.scar_i = w.scar[li];
    b.patch.bottom_i = w.bottom[li];
    b.patch.dual_i = 1;
    b.patch.wx_i = w.wx[static_cast<size_t>(vi)];
    b.patch.wz_i = w.wz[static_cast<size_t>(vj)];
    b.patch.vi_i = static_cast<uint8_t>(vi);
    b.patch.vj_i = static_cast<uint8_t>(vj);
    b.patch.src_id_i = patch_id;
    b.patch.st_ready_i = 1;
    b.vel.vv_ready_i = 1;

    // The lane word, offered to both only once PATCH is holding the vertex.
    const bool offer = n_lanes > 0 && holding;
    const size_t idx = w.at(vi, vj, lane_k < n_lanes ? lane_k : 0);
    b.patch.fld_valid_i = offer ? 1 : 0;
    b.patch.fld_height_i = offer ? w.height[idx] : 0;
    b.vel.lane_valid_i = offer ? 1 : 0;
    b.vel.lane_velocity_i = offer ? w.velocity[idx] : 0;
    settle(b);

    const bool vtx_taken = b.patch.vtx_valid_i != 0 && b.patch.vtx_ready_o != 0;
    const bool p_took = b.patch.fld_valid_i != 0 && b.patch.fld_ready_o != 0;
    const bool v_took = b.vel.lane_valid_i != 0 && b.vel.lane_ready_o != 0;
    if (p_took != v_took) ++out.lane_desync;

    const bool st_go = b.patch.st_valid_o != 0 && b.patch.st_ready_i != 0;
    const int32_t st_top = b.patch.top_o;
    const uint8_t st_dirty = static_cast<uint8_t>(b.patch.st_dirty_o);
    const bool vv_go = b.vel.vv_valid_o != 0 && b.vel.vv_ready_i != 0;
    const uint32_t vv_at =
        static_cast<uint32_t>(b.vel.vv_vj_o) * kLat + static_cast<uint32_t>(b.vel.vv_vi_o);
    const int16_t vv_word = static_cast<int16_t>(b.vel.vv_velocity_o);
    const uint8_t vv_mov = static_cast<uint8_t>(b.vel.vv_moving_o);
    const uint8_t vv_cov = static_cast<uint8_t>(b.vel.vv_covered_o);

    tick_pair(b);
    ++cyc;

    if (vtx_taken) {
      holding = n_lanes > 0;
      lane_k = 0;
      ++vtx_given;
    }
    if (p_took && v_took) {
      ++lane_k;
      if (lane_k >= n_lanes) {
        lane_k = 0;
        holding = false;  // the chain for this vertex is complete on both sides
      }
    }
    if (st_go) {
      if (patch_words < kVerts) {
        out.live_top[static_cast<size_t>(patch_words)] = st_top;
        out.dirty[static_cast<size_t>(patch_words)] = st_dirty;
      }
      ++patch_words;
    }
    if (vv_go) {
      out.vel_word[vv_at] = vv_word;
      out.moving[vv_at] = vv_mov;
      out.covered[vv_at] = vv_cov;
      ++vel_words;
    }
  }
  b.patch.vtx_valid_i = 0;
  b.patch.fld_valid_i = 0;
  b.vel.lane_valid_i = 0;
  settle(b);
  out.cycles = cyc;
  out.timed_out = vel_words < kVerts || patch_words < kVerts;
  out.dirty_mask = static_cast<uint16_t>(b.patch.subpatch_dirty_o);
  out.moving_mask = static_cast<uint16_t>(b.vel.moving_mask_o);
  return out;
}

/** Both blocks against their own oracles, over the same fixture. */
void compare(const Wake& w, const ChainOut& got, const char* tag) {
  zt::FieldList list;
  for (int a = 0; a < w.lanes(); ++a) list.offer(w.records[static_cast<size_t>(a)], 1);

  int top_bad = 0, dirty_bad = 0, vel_bad = 0, mov_bad = 0, cov_bad = 0;
  uint16_t want_dirty = 0, want_moving = 0;
  for (int vj = 0; vj < kLat; ++vj) {
    for (int vi = 0; vi < kLat; ++vi) {
      const size_t li = static_cast<size_t>(vj) * kLat + static_cast<size_t>(vi);
      zt::ComposeIn in;
      in.base = w.base[li];
      in.scar = w.scar[li];
      in.bottom = w.bottom[li];
      in.dual = true;
      in.wx = w.wx[static_cast<size_t>(vi)];
      in.wz = w.wz[static_cast<size_t>(vj)];
      int32_t fh[zt::kMaxPatchFields] = {};
      int32_t fv[zt::kMaxPatchFields] = {};
      bool cov[zt::kMaxPatchFields] = {};
      for (int a = 0; a < w.lanes(); ++a) {
        fh[a] = w.height[w.at(vi, vj, a)];
        fv[a] = w.velocity[w.at(vi, vj, a)];
        cov[a] = zt::covers(list[a], in.wx, in.wz);
      }
      const zt::ComposeOut c = zt::compose_vertex(in, list, fh);
      const zt::VelocityOut v = zt::velocity_vertex(fv, cov, w.lanes());
      if (got.live_top[li] != c.live_top) ++top_bad;
      if ((got.dirty[li] != 0) != c.dirty) ++dirty_bad;
      if (got.vel_word[li] != v.velocity) ++vel_bad;
      if ((got.moving[li] != 0) != v.moving) ++mov_bad;
      if ((got.covered[li] != 0) != v.covered) ++cov_bad;
      if (c.dirty) want_dirty |= zt::subpatch_mask(vi, vj);
      if (v.moving) want_moving |= zt::subpatch_mask(vi, vj);
    }
  }
  std::printf("--- chain [%s]: %llu clocks, %d desyncs\n", tag,
              static_cast<unsigned long long>(got.cycles), got.lane_desync);
  check(!got.timed_out, "the chain completed both streams", 0, got.timed_out ? 1 : 0);
  check(got.lane_desync == 0, "the two blocks consumed the SAME lane words in lockstep", 0,
        static_cast<uint64_t>(got.lane_desync));
  check(top_bad == 0, "every composed live_top matches compose_vertex", 0,
        static_cast<uint64_t>(top_bad));
  check(dirty_bad == 0, "every dirty bit matches", 0, static_cast<uint64_t>(dirty_bad));
  check(vel_bad == 0, "every velocity lattice word matches velocity_vertex — through the seam", 0,
        static_cast<uint64_t>(vel_bad));
  check(mov_bad == 0, "every moving bit matches", 0, static_cast<uint64_t>(mov_bad));
  check(cov_bad == 0, "every covered bit matches — the covers wire IS the §9.1 test", 0,
        static_cast<uint64_t>(cov_bad));
  check(got.dirty_mask == want_dirty, "the dirty subpatch mask matches", want_dirty,
        got.dirty_mask);
  check(got.moving_mask == want_moving, "the moving subpatch mask matches", want_moving,
        got.moving_mask);
}

// ---------------------------------------------------------------------------
// 1. THE SEAM, ON A FOOTPRINT WHOSE EDGE FALLS EXACTLY ON LATTICE VERTICES
// ---------------------------------------------------------------------------
// The closed-interval rule is only observable where a vertex sits EXACTLY on a
// footprint edge, and a randomly placed rectangle never does. The footprint
// below is placed on the lattice pitch so vertices land on x0 and x1 exactly —
// those vertices are INSIDE, and the seam has to carry that.
void test_seam_closed_interval(Both& b) {
  Wake w;
  w.init(1);
  w.records[0].x0 = -8 * kPitch;  // exactly the vertex at vi = 8
  w.records[0].z0 = -32 * kPitch;
  w.records[0].x1 = 8 * kPitch;  // exactly the vertex at vi = 24
  w.records[0].z1 = 32 * kPitch;
  for (int vj = 0; vj < kLat; ++vj)
    for (int vi = 0; vi < kLat; ++vi) {
      w.base[static_cast<size_t>(vj) * kLat + static_cast<size_t>(vi)] =
          static_cast<int16_t>(256 + vi);
      w.height[w.at(vi, vj, 0)] = (vi - 16) * 4096;
      w.velocity[w.at(vi, vj, 0)] = (vj - 16) * 4096;
    }
  const ChainOut got = run_chain(b, w, 0x301);
  compare(w, got, "closed-interval footprint edge");

  // The boundary is REACHED: the vertices at x0 and x1 are covered, and their
  // outer neighbours are not.
  const int row = 16 * kLat;
  check(got.covered[static_cast<size_t>(row + 8)] == 1,
        "the vertex EXACTLY on the footprint's x0 is inside", 1,
        got.covered[static_cast<size_t>(row + 8)]);
  check(got.covered[static_cast<size_t>(row + 24)] == 1, "and the vertex EXACTLY on x1 is inside",
        1, got.covered[static_cast<size_t>(row + 24)]);
  check(got.covered[static_cast<size_t>(row + 7)] == 0, "one vertex outside x0 is not", 0,
        got.covered[static_cast<size_t>(row + 7)]);
  check(got.covered[static_cast<size_t>(row + 25)] == 0, "one vertex outside x1 is not", 0,
        got.covered[static_cast<size_t>(row + 25)]);
}

// ---------------------------------------------------------------------------
// 2. DISPLACEMENT IS NOT RATE — the measurement law V5 rests on
// ---------------------------------------------------------------------------
void test_dirty_is_not_moving(Both& b) {
  Wake w;
  w.init(1);
  w.records[0].x0 = -32 * kPitch;
  w.records[0].z0 = -32 * kPitch;
  w.records[0].x1 = 32 * kPitch;
  w.records[0].z1 = 32 * kPitch;

  // A travelling wave, sampled at one instant, in two clearly separated
  // subpatch columns:
  //   * the LEADING EDGE (vi 2..5): the ground has a rate but has not moved
  //     yet — velocity non-zero, height lane exactly zero;
  //   * the CREST (vi 18..21): the ground is displaced and instantaneously
  //     still — height lane non-zero, velocity exactly zero.
  // Both live in subpatch ROW 0 (vj 2..5), columns 0 and 2 respectively.
  for (int vj = 2; vj <= 5; ++vj) {
    for (int vi = 2; vi <= 5; ++vi) w.velocity[w.at(vi, vj, 0)] = 2 << 16;
    for (int vi = 18; vi <= 21; ++vi) w.height[w.at(vi, vj, 0)] = 3 << 16;
  }
  const ChainOut got = run_chain(b, w, 0x302);
  compare(w, got, "displacement vs rate");

  const uint16_t crest_bit = static_cast<uint16_t>(1u << 2);  // row 0, col 2
  const uint16_t edge_bit = static_cast<uint16_t>(1u << 0);   // row 0, col 0
  check(got.dirty_mask == crest_bit, "the DIRTY mask holds only the crest's subpatch", crest_bit,
        got.dirty_mask);
  check(got.moving_mask == edge_bit, "the MOVING mask holds only the leading edge's subpatch",
        edge_bit, got.moving_mask);
  check((got.dirty_mask & got.moving_mask) == 0,
        "the two masks are DISJOINT here — neither contains the other", 0,
        static_cast<uint64_t>(got.dirty_mask & got.moving_mask));
  std::printf(
      "[terrain_velocity_chain] dirty mask 0x%04X, moving mask 0x%04X — a dirty-gated velocity"
      " sweep would have dropped the whole leading edge\n",
      got.dirty_mask, got.moving_mask);
}

// ---------------------------------------------------------------------------
// 3. THE COMPOSED PAIR'S SUSTAINED RATE UNDER THE WAKE WORKLOAD
// ---------------------------------------------------------------------------
void test_composed_rate(Both& b) {
  double per[3] = {0, 0, 0};
  const int lanes_cases[3] = {0, 1, 4};
  for (int c = 0; c < 3; ++c) {
    Wake w;
    w.init(lanes_cases[c]);
    for (int a = 0; a < lanes_cases[c]; ++a) {
      w.records[static_cast<size_t>(a)].x0 = -32 * kPitch;
      w.records[static_cast<size_t>(a)].z0 = -32 * kPitch;
      w.records[static_cast<size_t>(a)].x1 = 32 * kPitch;
      w.records[static_cast<size_t>(a)].z1 = 32 * kPitch;
    }
    for (int vj = 0; vj < kLat; ++vj)
      for (int vi = 0; vi < kLat; ++vi)
        for (int a = 0; a < lanes_cases[c]; ++a) {
          w.height[w.at(vi, vj, a)] = (vi + vj) * 512;
          w.velocity[w.at(vi, vj, a)] = (vi - vj) * 1024;
        }
    const ChainOut got = run_chain(b, w, static_cast<uint16_t>(0x310 + c));
    compare(w, got, lanes_cases[c] == 1 ? "the wake, composed" : "rate sweep");
    per[c] = static_cast<double>(got.cycles) / kVerts;
  }
  std::printf(
      "[terrain_velocity_chain] MEASURED composed PATCH+VELOCITY rate:\n"
      "  0 lanes %.3f clocks/vertex\n"
      "  1 lane (THE WAKE) %.3f clocks/vertex\n"
      "  4 lanes %.3f clocks/vertex\n",
      per[0], per[1], per[2]);
  // TERRAIN.PATCH spends a separate cycle ACCEPTING each vertex before it will
  // take that vertex's lane words, so the composed pair costs 1 + lanes clocks
  // per vertex where TERRAIN.VELOCITY alone costs max(1, lanes). The pair, not
  // the velocity block, is what a wake actually runs through, so this is the
  // number the contract quotes for the composed path.
  check(per[0] <= 1.05, "an empty list composes at 1 clock/vertex through the pair", 1,
        per[0] <= 1.05 ? 1 : 0);
  check(per[1] <= 2.05, "the composed wake path holds 2 clocks/vertex", 1, per[1] <= 2.05 ? 1 : 0);
  check(per[2] <= 5.05, "and stays 1 + lanes at four lanes", 1, per[2] <= 5.05 ? 1 : 0);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Both b;
  reset_pair(b);

  test_seam_closed_interval(b);
  test_dirty_is_not_moving(b);
  test_composed_rate(b);

  const int rc = zhao::report_and_exit("terrain_velocity_chain");
  zhao::exit_hard(rc);
}
