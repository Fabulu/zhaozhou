// geom_tilewalk_directed.cpp -- GEOM.TILEWALK, entry I55's sequencer.
//
// ===========================================================================
// WHAT THIS PROVES, AND WHY EACH CHECK IS THE CHECK
// ===========================================================================
//
// The block sweeps a frame's tiles, asks the head table which chunk each
// tile's list starts at, drives the walk over that chain, and hands every
// triangle to the geometry back end and then to the raster door. The head
// table, the walk, the back end and the door are modelled HERE, because what
// is under test is a SEQUENCE and a sequence is easier to assert from a
// program than from a second state machine in the same language as the first.
//
// THE DISCRIMINATING CHECK IS THE TILE COORDINATE, not the job count.
// `zhao_geom_arenabin` addresses its head table as `ty * GRID_W + tx` with the
// compile-time parameter, so a sequencer that derived the index from a runtime
// grid width would query one tile's list and label it with ANOTHER tile's
// coordinates. The list would be real, the descriptors would decode, every
// range guard downstream would pass, and the picture would be wrong. So every
// job's (tile_x, tile_y) is checked against the tile whose head was OFFERED,
// which is a fact this file recorded at offer time rather than recomputed.
//
// AND THE BRACKETS. `first` CLEARS a tile's accumulator in
// `zhao_raster_tile_pipe_v2` and `last` RESOLVES it (`rs_state_q <= last_q ?
// RS_SWAP : RS_IDLE`). A tile that never sees `last` is never written out --
// it would draw NOTHING while every counter balanced -- so exactly one job per
// tile must carry each, and that is asserted per tile rather than in total.
//
// TWO CTESTS ARE BUILT FROM THIS ONE FILE:
//
//   geom_tilewalk_directed          unmutated production. `overlap_o` MUST
//                                   stay at zero, and every sequencing check
//                                   must pass.
//   geom_tilewalk_overlapmut_fires  -DZHAO_GEOM_TILEWALK_OVERLAP_MUT, against
//                                   tests/mutants/zhao_geom_tilewalk_overlap_mutant.sv,
//                                   and `overlap_o` MUST MOVE. INVERTED
//                                   POLARITY: it passes when the block is
//                                   broken.
//
// The second exists because `overlap_o` is UNREACHABLE with legal stimulus
// while the serial handshake is correct -- `t_ready_o` is high in exactly one
// sub-state -- so no amount of traffic can move it and its zero would be a
// claim forever. CLAUDE.md: such a guard owes a committed mutant.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Vtb_zhao_geom_tilewalk.h"
#include "zhao_sim.hpp"

using Dut = Vtb_zhao_geom_tilewalk;

namespace {

int g_checks = 0;
int g_fail = 0;

void cke(uint64_t want, uint64_t got, const char* what) {
  ++g_checks;
  if (want != got) {
    ++g_fail;
    std::printf("FAIL: %s -- expected %llu, got %llu\n", what,
                static_cast<unsigned long long>(want),
                static_cast<unsigned long long>(got));
  }
}

void ckt(bool cond, const char* what) {
  ++g_checks;
  if (!cond) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

// The bench's parameters. Small on purpose: the sweep is over the WHOLE index
// space by design, so a 576-tile grid would spend the run querying empty
// tiles without testing anything a 8-tile one does not.
constexpr unsigned TILES = 8;
constexpr unsigned GRID_W = 4;

// The scene. Tile index -> how many references its list holds. A tile with 0
// has no head at all, which is the `head_valid_i` low case.
struct TileScene {
  unsigned tile;
  unsigned refs;
  uint32_t head;
};

// Deliberately NOT tiles 0..N: the gaps are what exercise `tiles_empty_o`, and
// a scene that fills every tile could not tell a sequencer that skips from one
// that counts. Tile 6 is a SINGLE-reference tile, where `first` and `last`
// must both land on the same job -- the case a first/last pair computed with
// an off-by-one gets wrong.
const TileScene kScene[] = {
    {1, 3, 0x0000'0011u},
    {2, 1, 0x0000'0022u},
    {5, 4, 0x0000'0055u},
    {6, 1, 0x0000'0066u},
};
constexpr unsigned kSceneN = sizeof(kScene) / sizeof(kScene[0]);

unsigned refs_of(unsigned tile) {
  for (unsigned i = 0; i < kSceneN; ++i)
    if (kScene[i].tile == tile) return kScene[i].refs;
  return 0;
}

uint32_t head_of(unsigned tile) {
  for (unsigned i = 0; i < kSceneN; ++i)
    if (kScene[i].tile == tile) return kScene[i].head;
  return 0xFFFF'FFFFu;
}

struct JobSeen {
  unsigned tx, ty;
  bool first, last;
  unsigned offered_tile;  // the tile whose head this file offered
};

}  // namespace

int main() {
  Dut t;

  t.rst_n = 0;
  t.start_i = 0;
  t.head_chunk_i = 0;
  t.head_valid_i = 0;
  t.walk_ready_i = 0;
  t.walk_done_i = 0;
  t.walk_failed_i = 0;
  t.t_valid_i = 0;
  t.t_first_i = 0;
  t.t_last_i = 0;
  t.be_ready_i = 0;
  t.be_out_valid_i = 0;
  t.job_ready_i = 0;
  for (int i = 0; i < 8; ++i) zhao::tick(t);
  t.rst_n = 1;
  for (int i = 0; i < 4; ++i) zhao::tick(t);

  // ---- the models --------------------------------------------------------
  // The head table answers ONE CLOCK LATE, exactly as `zhao_geom_arenabin`
  // does (`hq_chunk_q <= head_ram[head_tile_i]`). Answering combinationally
  // would let a sequencer with no wait state pass here and hang in the
  // console, which is the difference between a bench and a model.
  unsigned head_addr_prev = 0;

  // The walk.
  bool walk_running = false;
  unsigned walk_tile = 0;      // the tile whose head we accepted
  unsigned walk_left = 0;      // references still to emit
  unsigned walk_total = 0;

  // The back end: a fixed latency, so a triangle cannot come out on the clock
  // it went in and the sideband really has to be held across something.
  constexpr int kBeLatency = 5;
  int be_countdown = -1;
  bool be_out = false;

  std::vector<JobSeen> jobs;
  bool saw_frame_done = false;

  t.start_i = 1;  // a LEVEL; the block takes its own rising edge

  for (int c = 0; c < 20000; ++c) {
    // Present the head table's registered answer for the address seen last
    // clock.
    t.head_chunk_i = head_of(head_addr_prev);
    t.head_valid_i = (refs_of(head_addr_prev) > 0) ? 1 : 0;

    // The walk: accept an offer, then emit `refs` triangles, then pulse done.
    t.walk_ready_i = walk_running ? 0 : 1;
    t.walk_done_i = 0;
    if (walk_running && walk_left > 0) {
      t.t_valid_i = 1;
      t.t_first_i = (walk_left == refs_of(walk_tile)) ? 1 : 0;
      t.t_last_i = (walk_left == 1) ? 1 : 0;
    } else {
      t.t_valid_i = 0;
      t.t_first_i = 0;
      t.t_last_i = 0;
    }

    // The back end.
    t.be_ready_i = (be_countdown < 0 && !be_out) ? 1 : 0;
    t.be_out_valid_i = be_out ? 1 : 0;

    // The raster door, always ready. Backpressure is exercised by the console
    // smoke against the real tile pipe; here the sequence is the subject.
    t.job_ready_i = 1;

    t.eval();

    // ---- observe, before the edge ----------------------------------------
    const unsigned head_addr_now = t.head_tile_o;

    const bool walk_take = (t.walk_valid_o && t.walk_ready_i);
    const bool tri_take = (t.t_valid_i && t.t_ready_o);
    const bool be_take = (t.be_valid_o && t.be_ready_i);
    const bool job_take = (t.job_valid_o && t.job_ready_i);
    const uint32_t offered_head = t.walk_head_o;

    if (job_take) {
      JobSeen j;
      j.tx = static_cast<unsigned>(t.job_tile_x_o & 0xFFF);
      j.ty = static_cast<unsigned>(t.job_tile_y_o & 0xFFF);
      j.first = t.job_first_o != 0;
      j.last = t.job_last_o != 0;
      j.offered_tile = walk_tile;
      jobs.push_back(j);
    }
    if (t.frame_done_o) saw_frame_done = true;

    zhao::tick(t);

    // ---- advance the models ----------------------------------------------
    head_addr_prev = head_addr_now;

    if (walk_take) {
      // The head the block offered names the tile it is about to walk. This
      // file finds the tile FROM the head rather than assuming the block's
      // cursor, so a sequencer offering the wrong tile's head is caught.
      walk_tile = TILES;  // an impossible value until matched
      for (unsigned i = 0; i < kSceneN; ++i)
        if (kScene[i].head == offered_head) walk_tile = kScene[i].tile;
      ckt(walk_tile < TILES,
          "the head offered is one this scene actually placed");
      walk_left = (walk_tile < TILES) ? refs_of(walk_tile) : 0;
      walk_total += walk_left;
      walk_running = true;
    }
    if (tri_take && walk_left > 0) --walk_left;
    if (walk_running && walk_left == 0 && !tri_take) {
      // The chain has ended. `walk_done_o` in the real block pulses AFTER the
      // last emit was accepted, which is the ordering that makes a latched
      // `done_pend_q` necessary, so it is modelled that way.
      t.walk_done_i = 1;
      t.walk_failed_i = 0;
      zhao::tick(t);
      t.walk_done_i = 0;
      walk_running = false;
    }

    if (be_take) be_countdown = kBeLatency;
    if (be_countdown > 0) --be_countdown;
    else if (be_countdown == 0) {
      be_out = true;
      be_countdown = -1;
    }
    if (be_out && t.be_out_ready_o) be_out = false;

    if (saw_frame_done) break;
  }

  // =======================================================================
  // THE SWEEP
  //
  // THE SEQUENCING CHECKS BELOW ARE THE PRODUCTION BUILD's. The `_fires`
  // build runs a block whose handshake is deliberately broken, so it drops and
  // reorders triangles BY CONSTRUCTION -- requiring its sequence to be correct
  // would assert nothing about the counter and would bury the one line that
  // matters under a dozen failures that are the mutation working.
  // =======================================================================
#ifndef ZHAO_GEOM_TILEWALK_OVERLAP_MUT
  std::printf("\n=== geom_tilewalk: the sweep\n");
  ckt(saw_frame_done, "the sweep reached frame_done_o");
  cke(kSceneN, t.tiles_walked_o, "every tile with a head was walked");
  cke(TILES - kSceneN, t.tiles_empty_o,
      "every tile WITHOUT a head was counted empty -- the sweep covers the"
      " whole index space, which is what agrees with GEOM.ARENABIN");
  cke(walk_total, t.jobs_issued_o, "one job per reference the walk emitted");
  cke(walk_total, static_cast<uint32_t>(jobs.size()),
      "and the raster door took every one of them");
  cke(0, t.walks_failed_o, "no walk ended badly");

  // =======================================================================
  // THE COORDINATES, AND THIS IS THE CHECK THAT CATCHES A WRONG STRIDE
  // =======================================================================
  std::printf("=== geom_tilewalk: the tile coordinates\n");
  {
    size_t k = 0;
    for (unsigned i = 0; i < kSceneN; ++i) {
      const unsigned tile = kScene[i].tile;
      // THE UNITS ARE PIXELS. `zhao_geom_binner_v2` drives this port as
      // `$signed({2'd0, d_jx_r, 4'd0})` -- the tile index shifted LEFT by
      // four -- and its header states it: "`job_tile_x_o` is the tile's
      // top-left PIXEL". `zhao_geom_bin_pipe_v2` agrees from the other end,
      // deriving the raster's tile index as `job_tile_x_w[9:4]`.
      //
      // THIS EXPECTATION WAS WRITTEN IN TILE INDICES AND FAILED LOUDLY,
      // which is why it is transcribed here from the producer rather than
      // read back off the block under test.
      const unsigned want_tx = (tile % GRID_W) * 16u;
      const unsigned want_ty = (tile / GRID_W) * 16u;
      unsigned firsts = 0, lasts = 0;
      for (unsigned r = 0; r < kScene[i].refs && k < jobs.size(); ++r, ++k) {
        cke(want_tx, jobs[k].tx, "the job's tile_x is the tile that was walked");
        cke(want_ty, jobs[k].ty, "the job's tile_y is the tile that was walked");
        cke(tile, jobs[k].offered_tile,
            "and the job belongs to the walk this file offered a head for");
        if (jobs[k].first) ++firsts;
        if (jobs[k].last) ++lasts;
      }
      // EXACTLY ONE OF EACH, PER TILE. `first` clears the tile and `last`
      // resolves it, so two of either draws the tile twice and none of the
      // second never writes it out at all.
      cke(1, firsts, "exactly one job in this tile carried `first`");
      cke(1, lasts, "exactly one job in this tile carried `last`");
    }
    // The single-reference tile is the one an off-by-one gets wrong, so it is
    // asserted by name rather than left to the loop above.
    bool single_ok = false;
    size_t base = 0;
    for (unsigned i = 0; i < kSceneN; ++i) {
      if (kScene[i].tile == 6 && base < jobs.size())
        single_ok = jobs[base].first && jobs[base].last;
      base += kScene[i].refs;
    }
    ckt(single_ok,
        "a tile holding ONE reference carries `first` AND `last` on that same"
        " job -- the case an off-by-one bracket gets wrong");
  }

#endif  // !ZHAO_GEOM_TILEWALK_OVERLAP_MUT

  // =======================================================================
  // THE OVERLAP TRIPWIRE
  // =======================================================================
#ifdef ZHAO_GEOM_TILEWALK_OVERLAP_MUT
  std::printf("=== geom_tilewalk: MUTANT (zhao_geom_tilewalk_overlap_mutant)\n");
  ckt(t.overlap_o > 0,
      "MUTANT: overlap_o FIRED -- a triangle was taken while one was still in"
      " flight, which is the fault the counter exists for");
  std::printf("  overlap_o = %u (INVERTED POLARITY: nonzero is the pass)\n",
              t.overlap_o);
#else
  std::printf("=== geom_tilewalk: PRODUCTION\n");
  cke(0, t.overlap_o,
      "PRODUCTION: overlap_o is SILENT -- the serial handshake admits one"
      " triangle at a time");
#endif

  std::printf(
      "  tiles=%u empty=%u jobs=%u failed=%u stall=%u overlap=%u\n",
      t.tiles_walked_o, t.tiles_empty_o, t.jobs_issued_o, t.walks_failed_o,
      t.be_stall_clocks_o, t.overlap_o);
  std::printf("geom_tilewalk_directed: %d/%d checks failed\n", g_fail, g_checks);
  zhao::exit_hard(g_fail == 0 ? 0 : 1);
}
