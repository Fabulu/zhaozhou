// terrain_veljoin_directed.cpp -- TERRAIN.VELJOIN, against the oracle.
//
// WHAT THIS FILE IS EVIDENCE FOR
// ---------------------------------------------------------------------------
// Entry I34 in `zhao_console_core.sv` recorded velocity's blocker as
// architectural: "`zhao_terrain_velocity` drives its OWN 33x33 sweep, so
// joining it to the consumer's vertex stream is a scheduler and a composer may
// not write one."
//
// `zhao_terrain_veljoin` is that join, as a named block. This file drives the
// REAL PAIR -- the join and TERRAIN.VELOCITY, wired in C++ exactly as
// `zhao_console_core` wires them -- and checks the lattice they produce against
// `zref::terrain::velocity_vertex`, which is the oracle `design/blocks.yml`
// declares for TERRAIN.VELOCITY.
//
// It follows `tests/terrain/terrain_velocity_chain.cpp`'s pattern: two real
// Verilated blocks, a settle() that re-drives the combinational wires between
// them, and answers checked against the reference rather than against the
// author.
//
// THE COUNTERS ARE FIRED, NOT ASSERTED ZERO. Sections 5, 6 and 7 each drive
// the pair into the fault their counter watches and require the counter to
// MOVE; section 4's negative control runs byte-identical stimulus WITHOUT the
// fault and requires it to stay put. A detector reading zero is a claim, and
// these are the claims the console smoke quotes.
//
// No committed mutant is owed for any of the three (ruling R95): every fault
// here is reachable with LEGAL STIMULUS AT THE JOIN'S OWN PORTS -- a walk
// address that skips a vertex, a word offered with the fork shut, a done pulse
// that does not land on the 1,089th word are all things a port can be driven
// to do. The unreachable-guard case the mutant rule exists for does not arise.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_veljoin.h"
#include "Vzhao_terrain_velocity.h"

#include "zhao_sim.hpp"
#include "zref/zref_terrain_velocity.hpp"

// zhao::check takes (cond, what, expected, actual). Most assertions here are
// structural rather than numeric, so this wrapper supplies 0/0 for those and
// the numeric ones pass their real pair -- a message with a fake number beside
// it is worse than a message with none.
namespace {
void ck(bool cond, const char* what, uint64_t expected = 0, uint64_t actual = 0) {
  zhao::check(cond, what, expected, actual);
}
}  // namespace

namespace {

constexpr int kLat = 33;
constexpr int kVerts = kLat * kLat;  // 1,089 (terrain_rules 2)

struct Duo {
  Vzhao_terrain_veljoin join;
  Vzhao_terrain_velocity vel;
};

/** One captured lattice word, as the compose cache's velocity plane sees it. */
struct Word {
  int vi = 0;
  int vj = 0;
  int16_t vel = 0;
};

/**
 * The combinational wires between the two blocks, re-driven to a fixed point.
 * There is no loop to worry about and that is worth stating rather than
 * assuming: the join's `p_valid_o` and `v_valid_o` do not depend on the ready
 * they gate against, and TERRAIN.VELOCITY's `lane_ready_o` does not depend on
 * `lane_valid_i`. Four passes is generous.
 */
void settle(Duo& d) {
  for (int i = 0; i < 4; ++i) {
    d.vel.start_valid_i = d.join.v_start_valid_o;
    d.vel.start_lanes_i = d.join.v_start_lanes_o;
    d.vel.start_patch_id_i = d.join.v_start_patch_id_o;
    d.vel.start_src_id_i = d.join.v_start_src_id_o;
    d.vel.abort_i = d.join.v_abort_o;
    d.vel.lane_valid_i = d.join.v_valid_o;
    d.vel.lane_velocity_i = d.join.v_velocity_o;
    d.vel.lane_covers_i = d.join.v_covers_o;
    d.vel.eval();

    d.join.v_start_ready_i = d.vel.start_ready_o;
    d.join.v_ready_i = d.vel.lane_ready_o;
    d.join.v_vtx_vi_i = d.vel.vtx_vi_o;
    d.join.v_vtx_vj_i = d.vel.vtx_vj_o;
    d.join.v_idle_i = d.vel.idle_o;
    d.join.eval();
  }
}

/** One clock on both blocks, draining the lattice word if one is presented. */
void tick(Duo& d, std::vector<Word>* out) {
  // THE PRE-EDGE VALUES ARE THE ONES BOTH BLOCKS MUST SAMPLE, and getting this
  // wrong is why the first version of this file deadlocked. `settle()` copies
  // signals in one direction and then the other; calling it again with the
  // clock already HIGH makes the join sample TERRAIN.VELOCITY's POST-edge
  // `lane_ready_o` at its own edge -- one block a clock ahead of the other.
  // `terrain_velocity_chain.cpp` can settle across the edge safely because its
  // seam is one-directional (patch -> velocity); this seam runs BOTH ways, so
  // the fixed point has to be reached BEFORE the edge and then frozen.
  d.join.clk = 0;
  d.vel.clk = 0;
  settle(d);

  d.join.clk = 1;
  d.vel.clk = 1;
  d.vel.eval();
  d.join.eval();

  // The compose cache accepts every clock (one RAM write), which is the fact
  // law J1 depends on. Captured AFTER the posedge, which is when the block's
  // output register holds the word.
  if (out != nullptr && d.vel.vv_valid_o) {
    Word w;
    w.vi = d.vel.vv_vi_o;
    w.vj = d.vel.vv_vj_o;
    w.vel = static_cast<int16_t>(d.vel.vv_velocity_o);
    out->push_back(w);
  }

  d.join.clk = 0;
  d.vel.clk = 0;
  settle(d);
}

void reset(Duo& d) {
  d.join.rst_n = 0;
  d.join.patch_open_i = 0;
  d.join.patch_id_i = 0;
  d.join.src_id_i = 0;
  d.join.lanes_i = 0;
  d.join.vtx_fire_i = 0;
  d.join.w_vi_i = 0;
  d.join.w_vj_i = 0;
  d.join.a_valid_i = 0;
  d.join.a_velocity_i = 0;
  d.join.a_covers_i = 0;
  d.join.p_ready_i = 1;

  d.vel.rst_n = 0;
  d.vel.vv_ready_i = 1;

  settle(d);
  for (int i = 0; i < 3; ++i) tick(d, nullptr);
  d.join.rst_n = 1;
  d.vel.rst_n = 1;
  settle(d);
  tick(d, nullptr);
}

/** The Earth out-lane-1 plane for one patch: `lanes` words per vertex. */
struct Plane {
  int lanes = 0;
  std::vector<int32_t> velocity;  // fx16 raw, indexed (vj*33 + vi)*lanes + k
  std::vector<uint8_t> covers;

  void build(int n_lanes, uint64_t seed) {
    lanes = n_lanes;
    const size_t n = static_cast<size_t>(kVerts) * static_cast<size_t>(n_lanes < 1 ? 1 : n_lanes);
    velocity.assign(n, 0);
    covers.assign(n, 0);
    uint64_t s = seed;
    auto next = [&s]() {
      uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
      z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
      z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
      return z ^ (z >> 31);
    };
    for (size_t i = 0; i < n; ++i) {
      // A wake's worth of rate: a few metres per tick either way in fx16, so
      // the bake-back lands well inside height16 for most vertices and
      // saturates for none of them. Section 3 covers the saturating end.
      velocity[i] = static_cast<int32_t>(next() % 400001ULL) - 200000;
      covers[i] = static_cast<uint8_t>((next() % 4ULL) != 0);  // ~75% covered
    }
  }

  size_t at(int vi, int vj, int k) const {
    const size_t l = static_cast<size_t>(lanes < 1 ? 1 : lanes);
    return (static_cast<size_t>(vj) * kLat + static_cast<size_t>(vi)) * l +
           static_cast<size_t>(k);
  }
};

/** The oracle's lattice for a plane: `velocity_vertex` at every vertex. */
std::vector<int16_t> oracle_lattice(const Plane& p) {
  std::vector<int16_t> out(kVerts, 0);
  std::vector<int32_t> lane(static_cast<size_t>(p.lanes < 1 ? 1 : p.lanes), 0);
  std::vector<bool> cov(static_cast<size_t>(p.lanes < 1 ? 1 : p.lanes), false);
  for (int vj = 0; vj < kLat; ++vj) {
    for (int vi = 0; vi < kLat; ++vi) {
      if (p.lanes == 0) {
        out[static_cast<size_t>(vj) * kLat + vi] = 0;  // law V2
        continue;
      }
      std::vector<bool> c(static_cast<size_t>(p.lanes));
      for (int k = 0; k < p.lanes; ++k) {
        lane[static_cast<size_t>(k)] = p.velocity[p.at(vi, vj, k)];
        c[static_cast<size_t>(k)] = p.covers[p.at(vi, vj, k)] != 0;
      }
      // zref takes a bool array; build one contiguously.
      std::vector<char> cb(static_cast<size_t>(p.lanes));
      for (int k = 0; k < p.lanes; ++k) cb[static_cast<size_t>(k)] = c[static_cast<size_t>(k)] ? 1 : 0;
      const bool* cp = reinterpret_cast<const bool*>(cb.data());
      const zref::terrain::VelocityOut vo =
          zref::terrain::velocity_vertex(lane.data(), cp, p.lanes, nullptr);
      out[static_cast<size_t>(vj) * kLat + vi] = vo.velocity;
      (void)cov;
    }
  }
  return out;
}

/**
 * Run one whole patch through the pair, exactly as the composer drives it.
 *
 * `skip_vertex` >= 0 offers the walk address for that vertex index TWICE and
 * never offers the next one -- a walk that has lost a vertex. TERRAIN.VELOCITY
 * has no way to know, because it counts LANE WORDS; the join's interlock is
 * the only thing that can see it, and that is section 5.
 */
struct RunOut {
  std::vector<Word> words;
  uint32_t mismatch = 0;
  uint32_t joined = 0;
  uint32_t started = 0;
  uint32_t aborted = 0;
  uint32_t arm_stall = 0;
};

RunOut run_patch(Duo& d, const Plane& p, int skip_vertex = -1, int abort_after = -1) {
  RunOut r;

  // The patch opens. This is `tce_job_take` in the composer.
  d.join.patch_open_i = 1;
  d.join.patch_id_i = 7;
  d.join.src_id_i = 0x1234;
  d.join.lanes_i = static_cast<uint8_t>(p.lanes);
  tick(d, &r.words);
  d.join.patch_open_i = 0;

  int produced = 0;
  for (int v = 0; v < kVerts; ++v) {
    const int vi = v % kLat;  // column-fast, then row -- the pagestream's order
    const int vj = v / kLat;

    // The walk address the composer presents, skewed if asked.
    int wv = v;
    if (skip_vertex >= 0 && v > skip_vertex) wv = v + 1;
    const int wvi = wv % kLat;
    const int wvj = (wv / kLat) % kLat;

    // The vertex fire. The Earth adapter latches here and so does the join.
    d.join.vtx_fire_i = 1;
    d.join.w_vi_i = static_cast<uint8_t>(wvi);
    d.join.w_vj_i = static_cast<uint8_t>(wvj);
    tick(d, &r.words);
    d.join.vtx_fire_i = 0;

    if (abort_after >= 0 && v == abort_after) {
      // A new patch opens while the sweep is live -- law J4.
      d.join.patch_open_i = 1;
      d.join.lanes_i = static_cast<uint8_t>(p.lanes);
      tick(d, &r.words);
      d.join.patch_open_i = 0;
      r.aborted = d.join.sweeps_aborted_o;
      d.join.eval();
      break;
    }

    // The lane words for this vertex, in list order.
    for (int k = 0; k < p.lanes; ++k) {
      d.join.a_valid_i = 1;
      d.join.a_velocity_i = p.velocity[p.at(vi, vj, k)];
      d.join.a_covers_i = p.covers[p.at(vi, vj, k)];
      // Settle before SAMPLING the ready. `a_ready_o` does not depend on
      // `a_valid_i` -- law J1 gates it on the two consumers only -- so the
      // stale value would in fact be right; it is settled anyway, because
      // "it happens to be correct" is how a reader later concludes the
      // handshake is level-insensitive when it is not.
      settle(d);
      // Hold the offer until the fork takes it.
      int guard = 0;
      while (!d.join.a_ready_o && guard < 64) {
        tick(d, &r.words);
        ++guard;
      }
      if (guard >= 64) {
        // HARD, not a counted failure. zhao::check prints and continues, and
        // continuing past a stuck fork means indexing a short word vector --
        // so the run ends in an access violation and the FIRST failure, which
        // is the informative one, scrolls away behind a thousand copies.
        std::printf("veljoin: the fork never took a lane word at v=%d k=%d\n", v, k);
        zhao::exit_hard(1);
      }
      tick(d, &r.words);
      d.join.a_valid_i = 0;
    }
    ++produced;
  }
  (void)produced;

  // Drain whatever is still in flight.
  for (int i = 0; i < 64; ++i) tick(d, &r.words);

  d.join.eval();
  r.mismatch = d.join.vtx_mismatch_o;
  r.joined = d.join.lanes_joined_o;
  r.started = d.join.sweeps_started_o;
  r.arm_stall = d.join.arm_stall_clocks_o;
  if (abort_after < 0) r.aborted = d.join.sweeps_aborted_o;
  return r;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  int checks = 0;

  // =====================================================================
  // 1. THE LATTICE AGREES WITH THE ORACLE, over a real two-lane plane.
  // =====================================================================
  {
    Duo d;
    reset(d);
    Plane p;
    p.build(2, 0xC0FFEEULL);
    const RunOut r = run_patch(d, p);
    const std::vector<int16_t> want = oracle_lattice(p);

    ck(r.words.size() == static_cast<size_t>(kVerts),
          "veljoin s1: the sweep did not produce exactly 1,089 lattice words",
          static_cast<uint64_t>(kVerts), static_cast<uint64_t>(r.words.size()));
    ++checks;
    if (r.words.size() != static_cast<size_t>(kVerts)) zhao::exit_hard(1);

    // The order is the pagestream's: column fast, then row. Law V3 makes this
    // a structural fact and this is the assertion that says so.
    int order_bad = 0;
    int value_bad = 0;
    for (int v = 0; v < kVerts; ++v) {
      const Word& w = r.words[static_cast<size_t>(v)];
      if (w.vi != v % kLat || w.vj != v / kLat) ++order_bad;
      if (w.vel != want[static_cast<size_t>(v)]) ++value_bad;
    }
    ck(order_bad == 0, "veljoin s1: the sweep order is not column-fast then row");
    ++checks;
    ck(value_bad == 0, "veljoin s1: a lattice word disagrees with zref::terrain::velocity_vertex");
    ++checks;

    ck(r.started == 1u, "veljoin s1: exactly one sweep should have started");
    ++checks;
    ck(r.joined == static_cast<uint32_t>(kVerts * 2),
          "veljoin s1: every lane word of every vertex must have been forked");
    ++checks;
    ck(r.mismatch == 0u, "veljoin s1: the interlock fired on a correct walk");
    ++checks;
    ck(r.aborted == 0u, "veljoin s1: nothing should have been aborted");
    ++checks;

    std::printf("veljoin s1: 1089 words, order ok, oracle ok, joined=%u arm_stall=%u\n",
                r.joined, r.arm_stall);
  }

  // =====================================================================
  // 2. lanes == 0 STILL SWEEPS (law J3): the V2 zero word at every vertex.
  //    A patch with no live field must still publish a plane, or the
  //    compose cache serves the PREVIOUS patch's rates beside this
  //    patch's heights with every counter agreeing.
  // =====================================================================
  {
    Duo d;
    reset(d);
    Plane p;
    p.build(0, 1ULL);
    const RunOut r = run_patch(d, p);

    ck(r.words.size() == static_cast<size_t>(kVerts),
          "veljoin s2: a zero-lane patch must still produce 1,089 words");
    ++checks;
    int nonzero = 0;
    for (const Word& w : r.words) {
      if (w.vel != 0) ++nonzero;
    }
    ck(nonzero == 0, "veljoin s2: a vertex no lane covers must be exactly zero (law V2)");
    ++checks;
    ck(r.started == 1u, "veljoin s2: the zero-lane sweep must still start");
    ++checks;
    std::printf("veljoin s2: zero-lane patch produced 1089 zero words\n");
  }

  // =====================================================================
  // 3. THE FORK IS READY-JOINED (law J1). With the patch's ready LOW, the
  //    adapter must not be able to hand a word to either consumer -- a
  //    word taken by velocity alone would be a lattice that is right and a
  //    height composition that has lost a term.
  // =====================================================================
  {
    Duo d;
    reset(d);
    Plane p;
    p.build(1, 5ULL);

    d.join.patch_open_i = 1;
    d.join.lanes_i = 1;
    tick(d, nullptr);
    d.join.patch_open_i = 0;
    d.join.vtx_fire_i = 1;
    d.join.w_vi_i = 0;
    d.join.w_vj_i = 0;
    tick(d, nullptr);
    d.join.vtx_fire_i = 0;

    d.join.p_ready_i = 0;  // TERRAIN.PATCH cannot take a word
    d.join.a_valid_i = 1;
    d.join.a_velocity_i = 1 << 16;
    d.join.a_covers_i = 1;
    settle(d);

    ck(d.join.a_ready_o == 0, "veljoin s3: the fork took a word with the patch's ready low");
    ++checks;
    ck(d.join.v_valid_o == 0,
          "veljoin s3: velocity was offered a word the patch could not take");
    ++checks;

    const uint32_t before = d.join.lanes_joined_o;
    for (int i = 0; i < 8; ++i) tick(d, nullptr);
    d.join.eval();
    ck(d.join.lanes_joined_o == before,
          "veljoin s3: a word was joined while the fork was shut");
    ++checks;

    d.join.p_ready_i = 1;
    d.join.a_valid_i = 0;
    settle(d);
    std::printf("veljoin s3: the fork is ready-joined; nothing moved with the patch shut\n");
  }

  // =====================================================================
  // 4. THE INTERLOCK'S NEGATIVE CONTROL. Byte-identical stimulus to
  //    section 5 but for the skip: the counter must stay at zero. Written
  //    FIRST so that section 5's firing is a measurement and not a
  //    tautology -- this is the pairing TERRAINAUX got backwards.
  // =====================================================================
  uint32_t control_mismatch = 0;
  {
    Duo d;
    reset(d);
    Plane p;
    p.build(1, 99ULL);
    const RunOut r = run_patch(d, p, /*skip_vertex=*/-1);
    control_mismatch = r.mismatch;
    ck(control_mismatch == 0u,
          "veljoin s4: the interlock fired on a walk with nothing wrong with it");
    ++checks;
    std::printf("veljoin s4: negative control -- vtx_mismatch=%u on a clean walk\n",
                control_mismatch);
  }

  // =====================================================================
  // 5. THE INTERLOCK FIRES. The walk loses a vertex; TERRAIN.VELOCITY
  //    cannot notice, because it counts LANE WORDS and the count is
  //    unchanged. The join's two operands are loaded by different things,
  //    which is the whole reason this is visible at all.
  // =====================================================================
  {
    Duo d;
    reset(d);
    Plane p;
    p.build(1, 99ULL);  // the SAME seed as section 4
    const RunOut r = run_patch(d, p, /*skip_vertex=*/100);

    ck(r.mismatch > 0u,
          "veljoin s5: the walk skipped a vertex and vtx_mismatch_o did not move");
    ++checks;
    ck(r.mismatch > control_mismatch,
          "veljoin s5: the fault did not raise the counter above its own control");
    ++checks;
    std::printf("veljoin s5: FIRED -- vtx_mismatch=%u against control %u on identical stimulus\n",
                r.mismatch, control_mismatch);
  }

  // =====================================================================
  // 6. ABORT (law J4). A patch opening on a live sweep drops the partial
  //    lattice and is COUNTED, and the sweep that follows is complete and
  //    correct. This is the case that would otherwise deadlock the HEIGHT
  //    lane, because the join's ready-join gives velocity a hold on it.
  // =====================================================================
  {
    Duo d;
    reset(d);
    Plane p;
    p.build(1, 7ULL);
    const RunOut r1 = run_patch(d, p, /*skip_vertex=*/-1, /*abort_after=*/40);
    ck(r1.aborted >= 1u, "veljoin s6: a patch opened on a live sweep and nothing was counted");
    ++checks;

    // And the machine is not wedged: the next patch runs to completion and
    // agrees with the oracle. THIS is the assertion that matters -- section 6
    // must not assert the bug, it must assert that the repair holds.
    Plane p2;
    p2.build(2, 8ULL);
    const RunOut r2 = run_patch(d, p2);
    const std::vector<int16_t> want2 = oracle_lattice(p2);
    ck(r2.words.size() == static_cast<size_t>(kVerts),
          "veljoin s6: the sweep after an abort did not complete");
    ++checks;
    int bad = 0;
    for (int v = 0; v < kVerts; ++v) {
      if (r2.words[static_cast<size_t>(v)].vel != want2[static_cast<size_t>(v)]) ++bad;
    }
    ck(bad == 0, "veljoin s6: the sweep after an abort disagrees with the oracle");
    ++checks;
    std::printf("veljoin s6: FIRED -- sweeps_aborted=%u, and the next sweep is oracle-clean\n",
                r1.aborted);
  }

  std::printf("terrain_veljoin_directed: %d checks, 0 failures\n", checks);
  zhao::exit_hard(0);
}
