// geom_chunkser_directed.cpp -- ENTRY I54's ACCEPTANCE: a chunk whose ids are
// the RIGHT ids, checked against the arena's own descriptors.
//
// ===========================================================================
// WHY THIS TEST IS SHAPED THE WAY IT IS
// ===========================================================================
// Entry I54 exists to prevent one specific failure, and it names it:
//
//   "a binner slot is 0..127, every one of those is far below any plausible
//    sealed `tris`, so `zhao_geom_parambuf`'s `ck_illegal_o` and
//    `td_illegal_o` would both PASS and the walk would return WRONG
//    DESCRIPTORS THAT DECODE CLEANLY. Every counter would balance."
//
// SO "THE GUARDS PASS" IS NOT EVIDENCE HERE, and neither is any counter this
// chain owns. Both range guards are structurally blind to a wrong id that is
// in range; a test built on them would be green on the defect it is for.
//
// Two things make this file able to see it:
//
//   1. THE ARENA INDEX IS DELIBERATELY NOT THE BINNER SLOT. Four descriptors
//      are allocated that are never binned, so triangle `t` occupies binner
//      slot `t` and arena index `t + kDecoy`. A chunk carrying slots instead
//      of arena ids therefore decodes to the WRONG descriptor for every
//      triangle while every range guard still passes. If the two numbers were
//      allowed to coincide -- which they do in the obvious version of this
//      test -- the defect would be invisible and this file would prove
//      nothing.
//
//   2. THE COMPARISON IS AGAINST THE DESCRIPTORS, THROUGH THE REAL WALKER.
//      `zhao_geom_paramwalk` reads the chunk out of real SDRAM, follows the
//      chain, fetches each id's TriangleDescriptor and hands back its FIELDS.
//      Each descriptor carries a unique `source` and a unique vertex triple,
//      so an id that is in range but names the wrong record fails on the field
//      values rather than on a bound.
//
// THE GROUND TRUTH IS THE BINNER'S OWN RASTER DRAIN, not a model of it. The
// drain is captured tile by tile as it happens and the walk must reproduce it
// exactly, in order. There is no second implementation of tile binning here to
// disagree with the first.
//
// EVERY MODULE IN THE CHAIN IS THE PRODUCTION FILE: zhao_geom_binner_v2,
// zhao_geom_tidq, zhao_geom_chunkser, zhao_geom_paramarena, zhao_mem_share_wr,
// zhao_mem_guard, zhao_vram_arbiter, zhao_sdram_ctrl, zhao_geom_paramwalk and
// zhao_geom_parambuf. The metadata image is built at the console's own offset
// (322), so what is exercised is the shipped placement and not a convenient
// one.
//
// THE TWO POSITIVE CONTROLS ARE SEPARATE CASES AND ASSERT THE DETECTOR, NOT
// THE DEFECT. Case 1 asserts the CORRECT behaviour with every counter at zero;
// cases 2 and 3 inject a fault at a PORT and assert the counter FIRES. Neither
// owes a committed mutant, because both faults are presentable as stimulus --
// which is why `ck_alloc_id_i` and the descriptor RETIRE beat are inputs.
//
// ===========================================================================
// A LIVE DEFECT THIS FILE DOES NOT ASSERT, DECLARED RATHER THAN HIDDEN
// ===========================================================================
// RUN THIS BINARY WITH ANY ARGUMENT and it sweeps `kTile0Tris` from 1 to 60
// instead of running the cases. At the time of writing, ONE phase of the sixty
// -- 53 -- never publishes the frame: `frames_published_o` stays 0 and
// `publish_blocked_o` counts every clock of the wait.
//
// That places it exactly in `zhao_geom_paramarena`'s publication arm, in the
// `wr_words_q != 0` branch: a write whose words the socket never retired, with
// the engine idle and nothing left to issue. `retire_underflow_o`,
// `records_unsealed_o`, `records_discarded_o`, `arena_overrun_o`,
// `guard_denied_o`, `frame_fault_o`, `wq_err_o`, `addr_view_bad_o` and every
// `share_*` error counter read ZERO throughout, which is what makes it worth
// writing down: nothing in the tree reports it.
//
// IT IS NOT IN THE SERIALISER, and that was MEASURED rather than argued. In a
// failing run the chunks this block hands the arena were tapped
// (`next=1 count=14 id0=4`, then `next=NULL count=5 id0=18`) and the bytes in
// DRAM were peeked at the published base -- both correct. Its own detectors
// (`chain_break_o`, `head_clash_o`, `pass_truncated_o`, `chunks_sunk_o`) and
// all three `tidq_*` counters were zero. Pushing the identical record counts
// by hand through the bench's `ck_valid_i` with the serialiser disabled
// publishes normally, and so does inserting idle clocks between triangles. So
// what the live chunk intake changed is the TIMING, not the work.
//
// IT MOVES WITH THE FIXTURE, WHICH IS WHY THE SWEEP EXISTS. The phase that
// failed before this file published its three vertices was 20, and 20 is
// healthy now. "The scene I happen to run is fine" is not evidence about a
// one-phase race; walking the phase is.
//
// IT IS REPORTED, NOT WAIVED. It must go green before the console is fitted.
// It is downstream of everything entry I54 owns and belongs to whoever holds
// GEOM.PARAMBUF's socket accounting.

#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vtb_zhao_geom_paramarena.h"
#include "zhao_sim.hpp"

using Dut = Vtb_zhao_geom_paramarena;

namespace {

int g_fail = 0;
std::map<int, std::vector<uint32_t> > g_pass;
int g_checks = 0;

void cke(uint64_t want, uint64_t got, const char* what) {
  ++g_checks;
  if (want != got) {
    ++g_fail;
    std::printf("FAIL: %s -- expected %llu (0x%llX), got %llu (0x%llX)\n", what,
                static_cast<unsigned long long>(want),
                static_cast<unsigned long long>(want),
                static_cast<unsigned long long>(got),
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

// The binner's tile index is `ty * GRID_W + tx` with GRID_W the PARAMETER (24),
// not the active `grid_w_i`. Getting this wrong aims the head lookup at another
// tile, so it is written once, here.
constexpr int kGridWParam = 24;
constexpr int kChunkIds   = 14;

// Descriptors allocated and never binned. See the header: this is what
// separates the arena index from the binner slot, and without it this whole
// file is green on the defect it exists to catch.
constexpr int kDecoy = 4;

// Nineteen triangles on tile (0,0) -- more than fourteen, so that tile needs
// TWO chunks and both the chain link and its sentinel are exercised. See the
// header for what happens at twenty.
int kTile0Tris = 19;

int tile_index(int tx, int ty) { return ty * kGridWParam + tx; }

struct TriPlan {
  int min_tx, max_tx, min_ty, max_ty;
};

std::vector<TriPlan> scene() {
  std::vector<TriPlan> v;
  TriPlan a = {0, 0, 0, 0};
  TriPlan b = {1, 2, 0, 0};
  for (int i = 0; i < kTile0Tris; ++i) v.push_back(a);
  for (int i = 0; i < 4; ++i) v.push_back(b);
  return v;
}

void idle(Dut& t, int n) {
  for (int i = 0; i < n; ++i) zhao::tick(t);
}

void bring_up(Dut& t) {
  t.clk = 0;
  t.rst_n = 0;
  t.seal_valid_i = 0;
  t.seal_verts_i = 0;
  t.seal_tris_i = 0;
  t.seal_chunks_i = 0;
  t.frame_gen_i = 0;
  t.frame_end_i = 0;
  t.pv_valid_i = 0;
  t.td_valid_i = 0;
  t.ck_valid_i = 0;
  t.walk_valid_i = 0;
  t.walk_head_i = 0;
  t.t_ready_i = 0;
  t.peek_en_i = 0;
  t.peek_waddr_i = 0;

  t.cs_enable_i = 1;
  t.bin_frame_begin_i = 0;
  t.bin_frame_end_i = 0;
  t.bin_grid_w_i = 4;
  t.bin_grid_h_i = 2;
  t.bin_tri_valid_i = 0;
  t.bin_job_ready_i = 0;
  t.vid_retire_i = 0;
  t.vid_id_ok_i = 0;
  t.vid_id_i = 0;
  t.cs_head_tile_i = 0;
  t.cs_alloc_skew_i = 0;

  // THE EDGE FUNCTIONS ARE MADE UNIFORMLY ACCEPTING ON PURPOSE. With all three
  // slopes zero and a large positive constant, `zhao_raster_fill` accepts every
  // tile, so the tile set a triangle lands in is EXACTLY its bounding box. That
  // keeps a second implementation of the corner test out of this file: the
  // predicate is what `geom_binner_directed` already asserts against
  // `zref::EdgeWalk`, and what is under test here is the serialiser.
  t.bin_kx0_i = 0; t.bin_ky0_i = 0; t.bin_kc0_i = (1ull << 40);
  t.bin_kx1_i = 0; t.bin_ky1_i = 0; t.bin_kc1_i = (1ull << 40);
  t.bin_kx2_i = 0; t.bin_ky2_i = 0; t.bin_kc2_i = (1ull << 40);
  t.bin_tl_i = 0;
  t.bin_ax_i = 0; t.bin_ay_i = 0;
  t.bin_bx_i = 0; t.bin_by_i = 0;
  t.bin_cx_i = 0; t.bin_cy_i = 0;
  t.bin_min_x_i = 0; t.bin_max_x_i = 15;
  t.bin_min_y_i = 0; t.bin_max_y_i = 15;
  t.bin_src_id_i = 0;
  t.eval();
  idle(t, 8);
  t.rst_n = 1;
  t.eval();
  // The serialiser clears its 576-entry head table out of reset and the binner
  // clears its tile RAM. Neither may be interrupted.
  idle(t, 700);
}

bool seal(Dut& t, uint16_t gen) {
  t.seal_valid_i = 1;
  t.seal_verts_i = 256;
  t.seal_tris_i = 256;
  t.seal_chunks_i = 1024;
  t.frame_gen_i = gen;
  for (int i = 0; i < 20000; ++i) {
    t.eval();
    const bool go = t.seal_ready_o != 0;
    zhao::tick(t);
    if (go) {
      t.seal_valid_i = 0;
      t.eval();
      return true;
    }
  }
  t.seal_valid_i = 0;
  t.eval();
  return false;
}

bool push_pv(Dut& t, int32_t x) {
  t.pv_valid_i = 1;
  t.pv_x_i = static_cast<uint32_t>(x);
  t.pv_y_i = 0;
  t.pv_invw_i = 0x010000;
  t.pv_status_i = 0;
  t.pv_uow_i = 0;
  t.pv_vow_i = 0;
  t.pv_rgba_i = 0xFFFFFFFFu;
  for (int i = 0; i < 20000; ++i) {
    t.eval();
    const bool go = t.pv_ready_o != 0;
    zhao::tick(t);
    if (go) {
      t.pv_valid_i = 0;
      t.eval();
      return true;
    }
  }
  t.pv_valid_i = 0;
  t.eval();
  return false;
}

bool push_td(Dut& t, uint32_t arena_id) {
  t.td_valid_i = 1;
  // EVERY DESCRIPTOR NAMES THE SAME THREE VERTICES, and the frame publishes
  // exactly those three. A descriptor whose vertex ids are past the sealed
  // count is refused by `td_illegal_o` -- correctly -- so a fixture that never
  // publishes a vertex makes every triangle illegal and drowns the signal this
  // file is about. Per-triangle identity comes from `source` and `material`,
  // which are descriptor fields too and equally unique.
  t.td_v0_i = 0;
  t.td_v1_i = 1;
  t.td_v2_i = 2;
  t.td_material_i = static_cast<uint16_t>(0x200 + arena_id);
  t.td_raster_i = 0;
  t.td_source_i = arena_id;
  for (int i = 0; i < 20000; ++i) {
    t.eval();
    const bool go = t.td_ready_o != 0;
    zhao::tick(t);
    if (go) {
      t.td_valid_i = 0;
      t.eval();
      return true;
    }
  }
  t.td_valid_i = 0;
  t.eval();
  return false;
}

// One descriptor-step RETIRE, as GEOM.VERTID produces it. `ok` false is a
// descriptor the arena refused -- the case the queue exists to survive.
void push_id(Dut& t, uint32_t id, bool ok) {
  t.vid_retire_i = 1;
  t.vid_id_ok_i = ok ? 1 : 0;
  t.vid_id_i = id;
  t.eval();
  zhao::tick(t);
  t.vid_retire_i = 0;
  t.vid_id_ok_i = 0;
  t.eval();
}

bool push_tri(Dut& t, const TriPlan& p, uint16_t src_id) {
  t.bin_min_x_i = static_cast<int16_t>(p.min_tx * 16);
  t.bin_max_x_i = static_cast<int16_t>(p.max_tx * 16 + 15);
  t.bin_min_y_i = static_cast<int16_t>(p.min_ty * 16);
  t.bin_max_y_i = static_cast<int16_t>(p.max_ty * 16 + 15);
  t.bin_src_id_i = src_id;
  t.bin_tri_valid_i = 1;
  for (int i = 0; i < 20000; ++i) {
    t.eval();
    const bool go = t.bin_tri_ready_o != 0;
    zhao::tick(t);
    if (go) {
      t.bin_tri_valid_i = 0;
      t.eval();
      return true;
    }
  }
  t.bin_tri_valid_i = 0;
  t.eval();
  return false;
}

// Run the binner's raster drain and capture it. THIS IS THE GROUND TRUTH: the
// per-tile reference order, straight out of the block that owns it.
std::map<int, std::vector<uint16_t> > drain(Dut& t) {
  std::map<int, std::vector<uint16_t> > out;
  t.bin_job_ready_i = 1;
  for (int i = 0; i < 400000; ++i) {
    t.eval();
    if (t.bin_job_valid_o && t.bin_job_ready_i) {
      const int tx = static_cast<int16_t>(t.bin_job_tile_x_o) / 16;
      const int ty = static_cast<int16_t>(t.bin_job_tile_y_o) / 16;
      out[tile_index(tx, ty)].push_back(
          static_cast<uint16_t>(t.bin_job_src_id_o));
    }
    const bool done = t.bin_drain_done_o != 0;
    zhao::tick(t);
    if (done) break;
  }
  t.bin_job_ready_i = 0;
  t.eval();
  return out;
}

struct Walked {
  bool completed;
  bool failed;
  std::vector<uint32_t> source;
  std::vector<uint16_t> v0;
  int illegal;
  Walked() : completed(false), failed(false), illegal(0) {}
};

Walked walk(Dut& t, uint32_t head) {
  Walked r;
  t.walk_head_i = head;
  t.t_ready_i = 1;
  t.walk_valid_i = 1;
  t.eval();
  zhao::tick(t);
  t.walk_valid_i = 0;
  t.eval();
  for (int i = 0; i < 400000; ++i) {
    t.eval();
    if (t.t_valid_o && t.t_ready_i) {
      r.source.push_back(t.t_source_o);
      r.v0.push_back(static_cast<uint16_t>(t.t_material_o));
      if (t.t_illegal_o) ++r.illegal;
    }
    if (t.walk_done_o) {
      r.completed = true;
      r.failed = t.walk_failed_o != 0;
      zhao::tick(t);
      t.t_ready_i = 0;
      t.eval();
      return r;
    }
    zhao::tick(t);
  }
  t.t_ready_i = 0;
  t.eval();
  return r;
}


// Read one 16-bit word straight out of the behavioural DRAM. This is what
// separates "the producer wrote the wrong bytes" from "the reader returned the
// wrong fields" -- two faults that look identical in a walk.
uint16_t peek16(Dut& t, uint32_t byte_addr) {
  t.peek_en_i = 1;
  t.peek_waddr_i = byte_addr >> 1;
  t.eval();
  const uint16_t d = t.peek_data_o;
  t.peek_en_i = 0;
  t.eval();
  return d;
}

uint32_t peek32(Dut& t, uint32_t byte_addr) {
  return static_cast<uint32_t>(peek16(t, byte_addr)) |
         (static_cast<uint32_t>(peek16(t, byte_addr + 2)) << 16);
}

uint32_t head_of(Dut& t, int tile) {
  t.cs_head_tile_i = static_cast<uint16_t>(tile);
  t.eval();
  zhao::tick(t);
  t.eval();
  zhao::tick(t);
  t.eval();
  return t.cs_head_chunk_o;
}

// Drive one whole frame through the chain. Returns the binner's own per-tile
// reference order.
std::map<int, std::vector<uint16_t> > run_frame(
    Dut& t, const std::vector<TriPlan>& tris, int skip_id_at, int skew_after) {
  t.bin_frame_begin_i = 1;
  t.eval();
  zhao::tick(t);
  t.bin_frame_begin_i = 0;
  t.eval();
  idle(t, 700);   // the binner clears its tile RAM entry by entry

  for (int i = 0; i < 3; ++i)
    ckt(push_pv(t, 100 + i), "the three vertices every descriptor names");
  for (int i = 0; i < kDecoy; ++i) ckt(push_td(t, i), "decoy descriptor taken");

  for (size_t i = 0; i < tris.size(); ++i) {
    const uint32_t arena_id = static_cast<uint32_t>(i) + kDecoy;
    ckt(push_td(t, arena_id), "descriptor taken");
    // The RETIRE beat, with its acceptance. Skipping it is exactly a
    // descriptor the arena refused -- see case 3.
    if (static_cast<int>(i) != skip_id_at) push_id(t, arena_id, true);
    ckt(push_tri(t, tris[i], static_cast<uint16_t>(i)), "triangle binned");
  }

  // Geometry is done. This both closes the bin phase and arms the serialise
  // pass; the request has to be standing before the binner reaches D_DONE,
  // which is the one clock it samples it on.
  t.bin_frame_end_i = 1;
  t.eval();
  zhao::tick(t);
  t.bin_frame_end_i = 0;
  t.eval();

  std::map<int, std::vector<uint16_t> > truth = drain(t);

  if (skew_after >= 0) {
    for (int i = 0; i < 400000; ++i) {
      t.eval();
      if (static_cast<int>(t.cs_chunks_o) >= skew_after) break;
      zhao::tick(t);
    }
    t.cs_alloc_skew_i = 1;
    t.eval();
  }

  // CAPTURE THE SERIALISE PASS ITSELF. The binner walks its tile lists TWICE
  // -- once for the picture, once for the chunks -- and the two walks must be
  // the same references in the same order. Comparing them is what makes the
  // second walk evidence rather than an assumption, and it is a comparison
  // between two streams out of the SAME block rather than against a model.
  g_pass.clear();
  for (int i = 0; i < 800000; ++i) {
    t.eval();
    if (t.ser_tap_valid_o)
      g_pass[(int)t.ser_tap_tile_o].push_back((uint32_t)t.ser_tap_id_o);
    if (t.frames_published_o >= 1) break;
    zhao::tick(t);
  }
  return truth;
}

// ---------------------------------------------------------------------------
// CASE 1 -- THE CHUNK EVIDENCE.
// ---------------------------------------------------------------------------
void case1_ids_are_the_right_ids() {
  Dut t;
  bring_up(t);
  ckt(seal(t, 0x1234), "case1: frame sealed");
  idle(t, 700);   // the head-table clear the seal fire starts

  const std::vector<TriPlan> tris = scene();
  std::map<int, std::vector<uint16_t> > truth = run_frame(t, tris, -1, -1);

  t.eval();
  cke(1, t.frames_published_o, "case1: the frame published");

  // ------------------------------------------------------------------------
  // THE BYTES IN MEMORY, READ WITHOUT THE WALKER. This is the half that does
  // not depend on the reader being correct: the chunk layout as R7 freezes it,
  // straight out of the behavioural DRAM at the base the producer published.
  // It is here because the reader was NOT correct -- see the walker repair in
  // this same commit -- and a test that can only see through a broken decoder
  // cannot tell you which side is broken.
  // ------------------------------------------------------------------------
  {
    const uint32_t cb = t.publish_chunk_base_o;
    cke(1, peek32(t, cb + 0), "case1: chunk 0's next names chunk 1");
    cke(kChunkIds, peek16(t, cb + 4), "case1: chunk 0 is full");
    cke(0x1234, peek16(t, cb + 6),
        "case1: the ARENA stamped this frame's generation, not the producer");
    cke(kDecoy, peek32(t, cb + 8), "case1: chunk 0's first id, in memory");

    const uint32_t c1 = cb + 64u;
    cke(0xFFFFFFFFu, peek32(t, c1 + 0),
        "case1: chunk 1 ends the tile's chain with the sentinel");
    cke(kTile0Tris - kChunkIds, peek16(t, c1 + 4),
        "case1: chunk 1 holds the tile's remaining references");
    cke(kChunkIds + kDecoy, peek32(t, c1 + 8),
        "case1: chunk 1's first id continues where chunk 0 stopped");
  }
  cke(3, truth.size(), "case1: three tiles hold references");
  cke(kTile0Tris, truth[tile_index(0, 0)].size(),
      "case1: tile (0,0) holds every triangle that covers it");
  cke(4, truth[tile_index(1, 0)].size(), "case1: tile (1,0) holds 4 refs");
  cke(4, truth[tile_index(2, 0)].size(), "case1: tile (2,0) holds 4 refs");

  size_t total_refs = 0;
  size_t want_chunks = 0;
  for (std::map<int, std::vector<uint16_t> >::const_iterator it = truth.begin();
       it != truth.end(); ++it) {
    total_refs += it->second.size();
    want_chunks += (it->second.size() + kChunkIds - 1) / kChunkIds;
  }
  cke(total_refs, t.cs_refs_o, "case1: every reference was serialised");
  cke(want_chunks, t.cs_chunks_o, "case1: one chunk per fourteen, rounded up");
  cke(truth.size(), t.cs_tiles_o, "case1: a head was opened per occupied tile");
  cke(t.cs_chunks_o, t.chunks_written_o, "case1: the arena wrote every chunk");

  // The serialiser's own detectors, at zero, on a frame that is correct. Two
  // of them are fired deliberately in the cases below, so this zero is a
  // measurement and not a claim.
  cke(0, t.cs_chain_break_o, "case1: the chain is sequential");
  cke(0, t.cs_head_clash_o, "case1: no tile was opened twice");
  cke(0, t.cs_truncated_o, "case1: the pass ended on a chunk boundary");
  cke(0, t.cs_sunk_o, "case1: no chunk was taken and discarded");
  cke(0, t.tidq_underflow_o, "case1: the id stream kept up");
  cke(0, t.tidq_overflow_o, "case1: the id queue never overflowed");
  cke(0, t.tidq_unnamed_o, "case1: every triangle carried a named id");
  cke(0, t.quota_overflow_o, "case1: the quota was not exceeded");
  cke(0, t.frame_fault_o, "case1: the frame did not fault");

  // ------------------------------------------------------------------------
  // AND NOW THE THING THIS FILE IS FOR. Walk each tile out of real memory and
  // compare against the binner's own order -- by the DESCRIPTOR each id names,
  // not by the id.
  // ------------------------------------------------------------------------
  for (std::map<int, std::vector<uint16_t> >::const_iterator it = truth.begin();
       it != truth.end(); ++it) {
    const int tile = it->first;
    const std::vector<uint16_t>& want = it->second;

    const uint32_t head = head_of(t, tile);
    ckt(head != 0xFFFFFFFFu, "case1: an occupied tile has a head chunk");
    ckt(t.cs_head_valid_o != 0, "case1: the head is marked valid");

    Walked w = walk(t, head);
    ckt(w.completed, "case1: the walk completed");
    ckt(!w.failed, "case1: the walk did not end badly");
    cke(0, w.illegal, "case1: no walked descriptor was illegal");
    cke(want.size(), w.source.size(),
        "case1: the walk returned the tile's reference count");

    // The serialise pass saw the same references, in the same order, as the
    // raster drain.
    cke(want.size(), g_pass[tile].size(),
        "case1: the serialise pass saw the tile's whole reference list");
    if (g_pass[tile].size() == want.size()) {
      for (size_t k = 0; k < want.size(); ++k)
        cke((uint32_t)want[k] + (uint32_t)kDecoy, g_pass[tile][k],
            "case1: and in the raster drain's order");
    }

    if (w.source.size() == want.size()) {
      for (size_t k = 0; k < want.size(); ++k) {
        // `want[k]` is the binner's src_id, which this bench set to the
        // triangle's submission index. The ARENA index is that plus kDecoy --
        // and that gap is the whole point: a chunk built from binner SLOTS
        // would return the descriptor kDecoy records earlier, in range and
        // decoding cleanly, and this comparison is what refuses it.
        const uint32_t want_arena_id =
            static_cast<uint32_t>(want[k]) + static_cast<uint32_t>(kDecoy);
        cke(want_arena_id, w.source[k],
            "case1: the walked descriptor is the triangle the tile references");
        cke(static_cast<uint16_t>(0x200 + want_arena_id), w.v0[k],
            "case1: and its material is that descriptor's own");
      }
    }
  }

  // The decoder's own guards, quoted only AFTER the field comparison above --
  // corroboration, never the evidence, because both are blind to a wrong id
  // that is in range.
  cke(0, t.chunks_stale_o, "case1: no chunk read as stale");
  cke(0, t.chunks_illegal_o, "case1: no chunk read as malformed");
  cke(0, t.tris_illegal_o, "case1: no triangle id was out of range");
  cke(0, t.dir_mismatch_o, "case1: the directory agrees with the producer");
  cke(0, t.walk_cut_o, "case1: no chain was cut for length");

  // A NEGATIVE CONTROL ON THE TEST ITSELF. If the arena index and the binner
  // slot were equal, every comparison above would pass on a serialiser that
  // emitted slots. Assert they are NOT equal, so a future edit that removes
  // the decoy descriptors fails here rather than silently disarming the file.
  ckt(kDecoy > 0, "case1: the arena index is offset from the binner slot");
}

// ---------------------------------------------------------------------------
// CASE 2 -- `chain_break_o` FIRES, on legal stimulus.
// ---------------------------------------------------------------------------
// The counter differences a value HELD from one acceptance against the arena's
// LIVE cursor on the next, so the two sides are one acceptance apart. A cursor
// that stops being sequential after the first chunk is therefore visible to it
// -- and to nothing else in this chain, which is why it exists.
void case2_chain_break_fires() {
  Dut t;
  bring_up(t);
  ckt(seal(t, 0x2222), "case2: frame sealed");
  idle(t, 700);
  run_frame(t, scene(), -1, 1);
  t.eval();
  ckt(t.cs_chain_break_o > 0,
      "case2: a non-sequential arena cursor fires chain_break_o");
}

// ---------------------------------------------------------------------------
// CASE 3 -- A MISSING IDENTITY IS MADE LOUD, NOT SILENT.
// ---------------------------------------------------------------------------
// One descriptor retires WITHOUT being pushed into the queue, which is what a
// descriptor the arena refused looks like from here. The queue then runs short,
// reports it, and hands out ALL-ONES rather than zero. All-ones is above the
// sealed `tris`, so the parambuf's range guard REFUSES it and counts it; zero
// would have been accepted and would have named a real triangle.
void case3_missing_id_is_loud() {
  Dut t;
  bring_up(t);
  ckt(seal(t, 0x3333), "case3: frame sealed");
  idle(t, 700);
  run_frame(t, scene(), 0, -1);
  t.eval();
  ckt(t.tidq_underflow_o > 0,
      "case3: a missing identity fires the queue's underflow counter");

  cke(0, t.tidq_overflow_o, "case3: the queue did not overflow");

  const uint32_t head = head_of(t, tile_index(0, 0));
  if (head != 0xFFFFFFFFu && t.frames_published_o >= 1) {
    Walked w = walk(t, head);
    t.eval();
    // The poisoned reference must NOT come back as a legitimate triangle. It
    // is all-ones, so it addresses outside the view and the REAL guard refuses
    // it, or it decodes to a record that was never written and the vertex-seal
    // rule refuses that. Either way the console can SEE it, which is the whole
    // property: a missing identity is loud.
    ckt((t.tris_illegal_o + t.arena_guard_denied_o + (w.failed ? 1u : 0u)) > 0,
        "case3: the poisoned id is refused somewhere, never silently accepted");
  }
}


// THE PRICE OF ENTRY I55'S SWAP, MEASURED ON ONE STIMULUS.
//
// Both producers run in this bench on the same scene: `zhao_geom_binner_v2`'s
// on-chip job drain, which is what `zhao_raster_tile_pipe_v2` eats today, and
// `zhao_geom_paramwalk`'s external walk over the published SDRAM arena. This
// prints clocks per triangle for each. It asserts nothing about which is
// better -- it reports the number entry I55 says must be reported.
void price_the_swap() {
  kTile0Tris = 19;
  Dut t;
  bring_up(t);
  seal(t, 0x6666);
  idle(t, 700);
  run_frame(t, scene(), -1, -1);
  t.eval();
  const uint32_t head = head_of(t, tile_index(0, 0));
  if (head != 0xFFFFFFFFu && t.frames_published_o >= 1) {
    walk(t, head);
    t.eval();
  }
  const double drain_per =
      t.dbg_drain_refs_o > 1
          ? (double)t.dbg_drain_cycles_o / (double)(t.dbg_drain_refs_o - 1)
          : 0.0;
  const double walk_per =
      t.tris_emitted_o ? (double)t.dbg_walk_cycles_o / (double)t.tris_emitted_o
                       : 0.0;
  std::printf(
      "PRICE on-chip drain: refs=%u span=%u clocks -> %.2f clocks/ref\n"
      "PRICE external walk: tris=%u busy=%u clocks reqs=%u -> %.2f clocks/tri\n",
      (unsigned)t.dbg_drain_refs_o, (unsigned)t.dbg_drain_cycles_o, drain_per,
      (unsigned)t.tris_emitted_o, (unsigned)t.dbg_walk_cycles_o,
      (unsigned)t.dbg_walk_reqs_o, walk_per);
}

// CASE 4 -- THE FRAME PUBLISHES ON THE PHASE THAT USED TO WEDGE.
//
// `kTile0Tris = 53` is the one scene of sixty on which a retirement and a
// guard acceptance land on the SAME CLOCK in `zhao_geom_paramarena`. That
// block used to assign `wr_words_q` from two places in one always_ff -- a
// retire arm and the M_VERD accept arm -- so the later non-blocking
// assignment overwrote the earlier and the retired words were discarded. The
// frame then waited forever in the publication arm for words that had already
// come back, with the socket's own issued/credited/retired totals balancing
// exactly and every error counter in the arena, the share and the queue
// reading zero.
//
// THIS CASE ASSERTS THE CORRECT BEHAVIOUR, NOT THE DEFECT. `dbg_collide_o`
// is checked to be NON-ZERO because the coincidence must still be REACHED --
// a repair that moved the fixture off the colliding clock would pass a test
// written against the wedge while proving nothing. So the collision is
// required to happen AND the frame is required to publish under it, and the
// outstanding-word ledger is required to have drained to zero.
void case4_publishes_when_retire_and_accept_collide() {
  kTile0Tris = 53;
  Dut t;
  bring_up(t);
  ckt(seal(t, 0x5555), "case4: frame sealed");
  idle(t, 700);
  run_frame(t, scene(), -1, -1);
  t.eval();

  ckt(t.dbg_collide_o > 0,
      "case4: a retirement and an acceptance DO land on one clock here");
  cke(1, t.frames_published_o,
      "case4: the frame publishes despite the collision");
  cke(t.dbg_issued_words_o, t.dbg_retired_words_o,
      "case4: every word the arena owed the socket retired");
  cke(t.dbg_issued_words_o, t.dbg_client_credits_o,
      "case4: the arena's share of the client credits is all of them");
  cke(0, t.retire_underflow_o, "case4: no retirement went unowed");
  cke(0, t.share_retire_unowned_o, "case4: the share attributed every credit");
  cke(0, t.wq_err_o, "case4: the write queue was never popped empty");
  kTile0Tris = 19;
}

// A PUBLICATION SWEEP, run only with an argument. It is not a ctest case: it
// exists because an earlier fixture hit a ONE-PHASE stall in which the frame
// never published, and the only honest way to say whether that is gone is to
// walk the phase rather than to observe that one scene is healthy.
void sweep(int lo, int hi) {
  for (int n = lo; n <= hi; ++n) {
    kTile0Tris = n;
    Dut t;
    bring_up(t);
    seal(t, 0x4444);
    idle(t, 700);
    run_frame(t, scene(), -1, -1);
    t.eval();
    std::printf("sweep tris=%d published=%u chunks=%u refs=%u blocked=%u occ=%u owed=%u issued=%u credited=%u retired=%u unowned=%u ledgerfull=%u wqerr=%u underflow=%u collide=%u lost=%u\n", n,
                (unsigned)t.frames_published_o, (unsigned)t.cs_chunks_o,
                (unsigned)t.cs_refs_o, (unsigned)t.publish_blocked_o,
                (unsigned)t.dbg_wq_occ_o, (unsigned)t.dbg_wq_owed_o,
                (unsigned)t.dbg_issued_words_o,
                (unsigned)t.dbg_client_credits_o,
                (unsigned)t.dbg_retired_words_o,
                (unsigned)t.share_retire_unowned_o,
                (unsigned)t.share_ledger_full_o,
                (unsigned)t.wq_err_o,
                (unsigned)t.retire_underflow_o,
                (unsigned)t.dbg_collide_o, (unsigned)t.dbg_collide_words_o);
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  if (argc > 1) {
    sweep(1, 60);
    return 0;
  }

  case1_ids_are_the_right_ids();
  case2_chain_break_fires();
  case3_missing_id_is_loud();
  case4_publishes_when_retire_and_accept_collide();
  price_the_swap();

  std::printf("geom_chunkser_directed: %d checks, %d failures\n", g_checks,
              g_fail);
  zhao::exit_hard(g_fail == 0 ? 0 : 1);
  return 0;
}
