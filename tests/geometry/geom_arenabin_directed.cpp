// geom_arenabin_directed.cpp -- CONSOLE ENTRY I55's PRODUCER ACCEPTANCE:
// the external arena filled with NO on-chip arena anywhere in the circuit.
//
// ===========================================================================
// WHAT THIS FILE IS EVIDENCE FOR
// ===========================================================================
// Entry I55's refusal (WALKSWAP, 2026-09-26) is measured and correct:
//
//   "`u_geom_chunkser` is the ONLY driver of `u_geom_paramarena`'s `ck_*`
//    intake, and its only input is `zhao_geom_binner_v2`'s SERIALISE PASS --
//    a second read walk over the very `tile_ram`/`ref_ram`/`next_ram` the swap
//    would delete. The on-chip arena is what FILLS the external one ... the
//    two are in SERIES, not in parallel."
//
// Owner vacation directive section 4 rules that this state is not closure, and
// WALKSWAP's own closing paragraph names the shape that breaks the series:
//
//   "a producer that fills the external arena WITHOUT the on-chip one -- tile
//    binning performed against arena triangle ids directly."
//
// `zhao_geom_arenabin` is that producer and this file is its acceptance.
//
// ===========================================================================
// THE INDEPENDENCE CLAIM IS STRUCTURAL, NOT A COUNTER AT ZERO
// ===========================================================================
// The `geom_arenabin_directed` ctest elaborates the bench with
// `HAVE_ONCHIP = 0`, and its source list CONTAINS NO `zhao_geom_binner_v2.sv`,
// NO `zhao_geom_chunkser.sv`, NO `zhao_geom_tidq.sv` AND NO
// `zhao_geom_arena.sv`. The chunks that reach SDRAM in that build cannot have
// come from an on-chip reference array, because there is no on-chip reference
// array in the circuit.
//
// THE LIST IS ENFORCED, NOT DESCRIBED. `tests/CMakeLists.txt` splits the
// bench's sources into `ZHAO_PARAMARENA_BASE_SOURCES` and
// `ZHAO_PARAMARENA_ONCHIP_SOURCES` and FAILS THE CONFIGURE if any of the four
// on-chip files appears in the base list. Putting one back does not quietly
// turn this test into the thing it exists to refute; it stops the build and
// names the file.
//
// THAT MATTERS BECAUSE THE OBVIOUS VERSION OF THIS TEST IS VACUOUS. Compiling
// the binner in and asserting its counters read zero proves only that a path
// which exists did not happen to run this time -- and in a build where those
// ports are tied off by a generate, asserting they are zero is asserting a
// constant. So:
//
//   * the DIRECTED build (HAVE_ONCHIP = 0) makes the structural claim and does
//     NOT read the on-chip counters at all, because there they are constants;
//   * the PRICE build (HAVE_ONCHIP = 1, `geom_arenabin_price`) has the whole
//     on-chip path compiled in and LIVE, and it is there that
//     "`bin_tile_references_o > 0` while every arena chunk came from
//     GEOM.ARENABIN" is a real measurement -- the binner ran the same scene,
//     at full rate, and contributed no chunk, because `zhao_geom_arenabin`
//     owns the intake. (It used to read `cs_chunks_o == 0`, the serialiser's
//     counter; ARENACOMPOSE retired the serialiser with the binner's
//     serialise pass, so the surviving on-chip producer is the binner and the
//     assertion is made about IT.)
//
// ===========================================================================
// THE SAME TRAP ENTRY I54 NAMES, AND THE SAME DEFENCE
// ===========================================================================
// Four descriptors are allocated and never binned, so triangle `t` carries
// arena index `t + kDecoy`. A producer that shipped a local slot number
// instead of the arena's own index would decode to the WRONG descriptor for
// every triangle while every range guard still passed. The comparison is
// against the DESCRIPTOR FIELDS returned by the real `zhao_geom_paramwalk`
// after a real SDRAM round trip, not against any counter this chain owns.
//
// THE GROUND TRUTH IS NOT A MODEL OF TILE BINNING. The edge functions are made
// uniformly accepting -- all three slopes zero, a large positive constant --
// so `zhao_raster_fill` accepts every tile and a triangle's tile set is
// EXACTLY its bounding box. The expected reference order is then the scene's
// own submission order per tile, which is arithmetic, not a second binner.
// Whether this block's corner test agrees with `zhao_geom_binner_v2`'s on
// NON-trivial edges is a different question and is NOT claimed here.
//
// ===========================================================================
// WHAT IS DELIBERATELY NOT CLAIMED
// ===========================================================================
//   * That the raster path has been swapped. IT HAS NOT. This file exercises
//     the PRODUCER and the walk; the pixels in the composed console still come
//     from the on-chip path, and entry I55 says so.
//   * That this producer is cheaper. `price_the_producer()` reports the
//     number; it asserts nothing about which is better, in the same spirit as
//     `geom_chunkser_directed`'s `price_the_swap`.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

#include "verilated.h"

#include "Vtb_zhao_geom_paramarena.h"
#include "zhao_sim.hpp"

using Dut = Vtb_zhao_geom_paramarena;

namespace {

int g_fail = 0;
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

// The producer's tile index is `ty * GRID_W + tx` with GRID_W the PARAMETER
// (24), not the active `grid_w_i` -- the same law `zhao_geom_binner_v2` uses,
// and getting it wrong aims the head lookup at another tile.
constexpr int kGridWParam = 24;
constexpr int kChunkIds   = 14;
constexpr int kDecoy      = 4;

// Nineteen references on tile (0,0): more than fourteen, so that tile needs
// TWO chunks, which is what makes the CHAIN PATCH live. A scene where no tile
// exceeds one chunk never patches anything and would be green on a producer
// whose chain is broken.
constexpr int kTile0Tris = 19;

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

// The reference order the producer MUST deliver, derived from the scene by
// arithmetic: bounding box, row major, submission order within a tile.
std::map<int, std::vector<uint32_t> > expected(const std::vector<TriPlan>& s) {
  std::map<int, std::vector<uint32_t> > out;
  for (size_t i = 0; i < s.size(); ++i) {
    const uint32_t id = static_cast<uint32_t>(i) + kDecoy;
    for (int ty = s[i].min_ty; ty <= s[i].max_ty; ++ty)
      for (int tx = s[i].min_tx; tx <= s[i].max_tx; ++tx)
        out[tile_index(tx, ty)].push_back(id);
  }
  return out;
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
  t.lk_valid_i = 0;
  t.lk_index_i = 0;
  t.lk_next_i = 0;
  t.lk_count_i = 0;
  t.walk_valid_i = 0;
  t.walk_head_i = 0;
  t.t_ready_i = 0;
  t.peek_en_i = 0;
  t.peek_waddr_i = 0;

  // THE ON-CHIP HALF TAKES NO TRIANGLE UNLESS A CASE GIVES IT ONE. In the
  // directed build it is not even elaborated.
  t.bin_frame_begin_i = 0;
  t.bin_frame_end_i = 0;
  t.bin_grid_w_i = 4;
  t.bin_grid_h_i = 2;
  t.bin_tri_valid_i = 0;
  t.bin_job_ready_i = 0;
  t.vid_retire_i = 0;
  t.vid_id_ok_i = 0;
  t.vid_id_i = 0;
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

  // GEOM.ARENABIN OWNS THE INTAKE.
  t.ab_enable_i = 1;
  t.ab_geom_done_i = 0;
  t.ab_grid_w_i = 4;
  t.ab_grid_h_i = 2;
  t.ab_tri_valid_i = 0;
  t.ab_arena_id_i = 0;
  t.ab_id_ok_i = 0;
  t.ab_head_tile_i = 0;
  // Uniformly accepting, for the reason in the header.
  t.ab_kx0_i = 0; t.ab_ky0_i = 0; t.ab_kc0_i = (1ull << 40);
  t.ab_kx1_i = 0; t.ab_ky1_i = 0; t.ab_kc1_i = (1ull << 40);
  t.ab_kx2_i = 0; t.ab_ky2_i = 0; t.ab_kc2_i = (1ull << 40);
  t.ab_tl_i = 0;
  t.ab_min_x_i = 0; t.ab_max_x_i = 15;
  t.ab_min_y_i = 0; t.ab_max_y_i = 15;

  t.eval();
  idle(t, 8);
  t.rst_n = 1;
  t.eval();
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

// Offer one triangle to GEOM.ARENABIN. `id_ok` false is a descriptor the arena
// refused: it has no index, so it must be dropped rather than binned.
// THE SAME TRIANGLE, OFFERED TO THE ON-CHIP BINNER. Only the price case uses
// this: everywhere else the binner is deliberately given nothing, so that
// "the on-chip path produced no chunk" is about a block that COULD have.
bool push_bin_tri(Dut& t, const TriPlan& p, uint16_t src_id) {
  t.bin_min_x_i = static_cast<int16_t>(p.min_tx * 16);
  t.bin_max_x_i = static_cast<int16_t>(p.max_tx * 16 + 15);
  t.bin_min_y_i = static_cast<int16_t>(p.min_ty * 16);
  t.bin_max_y_i = static_cast<int16_t>(p.max_ty * 16 + 15);
  t.bin_src_id_i = src_id;
  t.bin_tri_valid_i = 1;
  for (int i = 0; i < 200000; ++i) {
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

// Run the binner's raster drain to completion. `dbg_drain_*` in the bench
// measures the span from the first job handshake to the last.
void drain_the_binner(Dut& t) {
  t.bin_job_ready_i = 1;
  for (int i = 0; i < 400000; ++i) {
    t.eval();
    const bool done = t.bin_drain_done_o != 0;
    zhao::tick(t);
    if (done) break;
  }
  t.bin_job_ready_i = 0;
  t.eval();
}

bool push_tri(Dut& t, const TriPlan& p, uint32_t arena_id, bool id_ok) {
  t.ab_min_x_i = static_cast<int16_t>(p.min_tx * 16);
  t.ab_max_x_i = static_cast<int16_t>(p.max_tx * 16 + 15);
  t.ab_min_y_i = static_cast<int16_t>(p.min_ty * 16);
  t.ab_max_y_i = static_cast<int16_t>(p.max_ty * 16 + 15);
  t.ab_arena_id_i = arena_id;
  t.ab_id_ok_i = id_ok ? 1 : 0;
  t.ab_tri_valid_i = 1;
  for (int i = 0; i < 200000; ++i) {
    t.eval();
    const bool go = t.ab_tri_ready_o != 0;
    zhao::tick(t);
    if (go) {
      t.ab_tri_valid_i = 0;
      t.eval();
      return true;
    }
  }
  t.ab_tri_valid_i = 0;
  t.eval();
  return false;
}

struct Walked {
  bool completed;
  bool failed;
  std::vector<uint32_t> source;
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
  t.ab_head_tile_i = static_cast<uint16_t>(tile);
  t.eval();
  zhao::tick(t);
  t.eval();
  zhao::tick(t);
  t.eval();
  return t.ab_head_chunk_o;
}

// Drive one whole frame through GEOM.ARENABIN. `skip_id_at` is the triangle
// whose descriptor the arena is to be told it refused.
void run_frame(Dut& t, const std::vector<TriPlan>& tris, uint16_t gen,
               int skip_id_at) {
  ckt(seal(t, gen), "the arena sealed a frame");
  // `pa_seal_fire` is GEOM.ARENABIN's frame start and it clears a 576-entry
  // directory one entry per clock. Interrupting that is a producer that
  // inherits last frame's heads.
  idle(t, 700);
  if (getenv("ZHAO_ARENABIN_TRACE"))
    std::printf("TRACE after seal gen=%04X: busy=%u binned=%u refs=%u pub=%u\n",
                gen, (unsigned)t.ab_busy_o, (unsigned)t.ab_tris_binned_o,
                (unsigned)t.ab_refs_binned_o, (unsigned)t.frames_published_o);

  for (int i = 0; i < 3; ++i)
    ckt(push_pv(t, 100 + i), "the three vertices every descriptor names");
  for (int i = 0; i < kDecoy; ++i) ckt(push_td(t, i), "decoy descriptor taken");

  for (size_t i = 0; i < tris.size(); ++i) {
    const uint32_t arena_id = static_cast<uint32_t>(i) + kDecoy;
    ckt(push_td(t, arena_id), "descriptor taken");
    const bool ok = (static_cast<int>(i) != skip_id_at);
    ckt(push_tri(t, tris[i], arena_id, ok), "triangle offered to the producer");
  }

  t.ab_geom_done_i = 1;
  t.eval();
  zhao::tick(t);
  t.ab_geom_done_i = 0;
  t.eval();

  // WAIT FOR *THIS* FRAME, NOT FOR ANY FRAME. `frames_published_o` is
  // cumulative, so `>= 1` breaks instantly on every frame after the first and
  // the assertions then read counters before the work has happened. That was a
  // real bug in the first version of this file and it presented as the RTL
  // failing to bin frame two.
  const uint32_t want_pub = t.frames_published_o + 1;
  for (int i = 0; i < 2000000; ++i) {
    t.eval();
    if (t.frames_published_o >= want_pub) break;
    zhao::tick(t);
  }
  if (getenv("ZHAO_ARENABIN_TRACE"))
    std::printf("TRACE end   gen=%04X: binned=%u refs=%u chunks=%u pub=%u\n",
                gen, (unsigned)t.ab_tris_binned_o, (unsigned)t.ab_refs_binned_o,
                (unsigned)t.ab_chunks_emitted_o, (unsigned)t.frames_published_o);
}

// ---------------------------------------------------------------------------
// CASE 1 -- THE ROUND TRIP, AND THE IDS ARE THE RIGHT IDS.
// ---------------------------------------------------------------------------
void case1_the_external_arena_is_filled_without_the_onchip_one() {
  Dut t;
  bring_up(t);
  const std::vector<TriPlan> s = scene();
  run_frame(t, s, 0x4242, -1);

  cke(1, t.frames_published_o, "the frame published");
  cke(0, t.frame_fault_o, "no frame fault");
  cke(0, t.ab_overflow_o, "the producer did not overflow");
  cke(0, t.ab_chunk_refused_o, "the arena refused no chunk");
  cke(0, t.ab_tris_unnamed_o, "every triangle had an arena identity");
  cke(0, t.ab_flush_cut_o, "the end-of-frame flush ran to completion");
  cke(0, t.link_illegal_o, "no chain patch named an unallocated chunk");

  const std::map<int, std::vector<uint32_t> > want = expected(s);

  // The reference arithmetic, stated so a silent change in the scene cannot
  // quietly make this test trivial.
  size_t want_refs = 0, want_chunks = 0, want_links = 0;
  for (std::map<int, std::vector<uint32_t> >::const_iterator it = want.begin();
       it != want.end(); ++it) {
    const size_t n = it->second.size();
    const size_t c = (n + kChunkIds - 1) / kChunkIds;
    want_refs += n;
    want_chunks += c;
    want_links += (c > 0 ? c - 1 : 0);
  }
  ckt(want_links > 0, "the scene actually exercises the chain patch");

  cke(want_refs, t.ab_refs_binned_o, "references binned");
  cke(want_chunks, t.ab_chunks_emitted_o, "chunks emitted");
  cke(want_links, t.ab_links_patched_o, "chain patches issued");
  cke(want.size(), t.ab_tiles_with_refs_o, "tiles that hold references");
  cke(want_chunks, t.chunks_written_o, "chunks the ARENA wrote to SDRAM");
  cke(want_links, t.links_written_o, "chain patches the ARENA wrote to SDRAM");

  // THE WALK. Every tile the scene touches, through real SDRAM, compared
  // against the descriptor fields rather than against any counter.
  for (std::map<int, std::vector<uint32_t> >::const_iterator it = want.begin();
       it != want.end(); ++it) {
    const uint32_t head = head_of(t, it->first);
    ckt(head != 0xFFFFFFFFu, "the tile has a head chunk");
    if (head == 0xFFFFFFFFu) continue;
    const Walked w = walk(t, head);
    ckt(w.completed, "the walk reached the end of the chain");
    ckt(!w.failed, "the walk did not end badly");
    cke(0, w.illegal, "no descriptor was refused by the record layer");
    cke(it->second.size(), w.source.size(), "references returned by the walk");
    const size_t n = w.source.size() < it->second.size() ? w.source.size()
                                                         : it->second.size();
    for (size_t k = 0; k < n; ++k) {
      // `source` is the descriptor's own field and is unique per triangle, so
      // an id that is IN RANGE but names the wrong record fails here.
      cke(it->second[k], w.source[k],
          "the walk returned the right descriptor, in painter order");
    }
  }

  // THE BYTES, so "the producer wrote the wrong chain" and "the reader
  // followed it wrongly" cannot be confused. Tile 0 needs two chunks; its
  // first must NOT be terminal and must name its successor.
  const uint32_t base = t.publish_chunk_base_o;
  const uint32_t h0 = head_of(t, 0);
  ckt(h0 != 0xFFFFFFFFu, "tile 0 has a head");
  if (h0 != 0xFFFFFFFFu) {
    const uint32_t next0 = peek32(t, base + h0 * 64);
    ckt(next0 != 0xFFFFFFFFu,
        "tile 0's FIRST chunk is not terminal -- the patch landed");
    const uint32_t cnt0 = peek16(t, base + h0 * 64 + 4);
    cke(kChunkIds, cnt0, "tile 0's first chunk is full");
    cke(0x4242, peek16(t, base + h0 * 64 + 6),
        "the ARENA stamped the generation, not the producer");
    if (next0 != 0xFFFFFFFFu) {
      const uint32_t nx = peek32(t, base + next0 * 64);
      cke(0xFFFFFFFFu, nx, "tile 0's SECOND chunk terminates the chain");
      cke(kTile0Tris - kChunkIds, peek16(t, base + next0 * 64 + 4),
          "tile 0's second chunk carries the remainder");
    }
  }

  std::printf("PRODUCER refs=%u chunks=%u links=%u tiles=%u stall=%u maxchunks=%u\n",
              (unsigned)t.ab_refs_binned_o, (unsigned)t.ab_chunks_emitted_o,
              (unsigned)t.ab_links_patched_o, (unsigned)t.ab_tiles_with_refs_o,
              (unsigned)t.ab_intake_stall_o, (unsigned)t.ab_max_tile_chunks_o);
}

// ---------------------------------------------------------------------------
// CASE 2 -- A TRIANGLE WITH NO ARENA IDENTITY IS DROPPED AND COUNTED.
// ---------------------------------------------------------------------------
// Not "the guard passes": the reference must be ABSENT from the tile's list,
// which is checked through the walk, and the count must move.
void case2_a_nameless_triangle_is_not_binned() {
  Dut t;
  bring_up(t);
  const std::vector<TriPlan> s = scene();
  const int skip = 3;
  run_frame(t, s, 0x5151, skip);

  cke(1, t.frames_published_o, "the frame published");
  cke(1, t.ab_tris_unnamed_o, "the nameless triangle was counted");

  std::map<int, std::vector<uint32_t> > want = expected(s);
  const uint32_t gone = static_cast<uint32_t>(skip) + kDecoy;
  for (std::map<int, std::vector<uint32_t> >::iterator it = want.begin();
       it != want.end(); ++it) {
    std::vector<uint32_t> keep;
    for (size_t k = 0; k < it->second.size(); ++k)
      if (it->second[k] != gone) keep.push_back(it->second[k]);
    it->second = keep;
  }

  const uint32_t head = head_of(t, 0);
  ckt(head != 0xFFFFFFFFu, "tile 0 still has a head");
  if (head != 0xFFFFFFFFu) {
    const Walked w = walk(t, head);
    cke(want[0].size(), w.source.size(),
        "tile 0 lost exactly the nameless triangle");
    for (size_t k = 0; k < w.source.size() && k < want[0].size(); ++k)
      cke(want[0][k], w.source[k], "and the survivors kept their order");
  }
}

// ---------------------------------------------------------------------------
// CASE 3 -- `link_illegal_o` FIRES, AND IT IS FIRED AT A PORT.
// ---------------------------------------------------------------------------
// POSITIVE CONTROL FOR THE ARENA'S NEW GUARD, with legal stimulus rather than
// a committed mutant: `lk_index_i` is an INPUT, so a chain patch naming a
// chunk the allocation cursor has not reached is presentable directly. Case 1
// is the negative control -- it asserts the counter is ZERO on a correct run.
void case3_link_illegal_fires() {
  Dut t;
  bring_up(t);
  t.ab_enable_i = 0;          // the bench drives the patch port directly
  t.eval();

  ckt(seal(t, 0x6161), "the arena sealed a frame");
  idle(t, 700);
  for (int i = 0; i < 3; ++i) ckt(push_pv(t, 100 + i), "a vertex");

  cke(0, t.link_illegal_o, "the guard starts silent");

  // No chunk has been allocated this frame, so index 0 is past the cursor.
  t.lk_valid_i = 1;
  t.lk_index_i = 0;
  t.lk_next_i = 1;
  t.lk_count_i = kChunkIds;
  for (int i = 0; i < 20000; ++i) {
    t.eval();
    const bool go = t.lk_ready_o != 0;
    zhao::tick(t);
    if (go) break;
  }
  t.lk_valid_i = 0;
  t.eval();
  idle(t, 64);

  cke(1, t.link_illegal_o, "the guard FIRED on a patch past the cursor");
  cke(0, t.links_written_o, "and the refused patch wrote nothing");
  cke(0, t.frame_fault_o,
      "a bad link does not throw away the frame's good records");
}

// ---------------------------------------------------------------------------
// CASE 4 -- TWO FRAMES: THE DIRECTORY DOES NOT SURVIVE A SEAL.
// ---------------------------------------------------------------------------
// The identity claim in the block's header is that the mapping's lifetime is
// ONE FRAME. A head table that carried over would give frame two a chain that
// starts in frame one's bytes and decodes cleanly.
void case4_the_directory_does_not_outlive_its_frame() {
  Dut t;
  bring_up(t);
  const std::vector<TriPlan> s = scene();
  run_frame(t, s, 0x7001, -1);
  cke(1, t.frames_published_o, "frame one published");

  const uint32_t refs1 = t.ab_refs_binned_o;
  const uint32_t chunks1 = t.ab_chunks_emitted_o;

  // A tile the first frame never touched.
  const int quiet = tile_index(3, 1);
  cke(0xFFFFFFFFu, head_of(t, quiet), "an untouched tile has no head");

  // Frame two: ONE triangle, on that previously quiet tile only.
  std::vector<TriPlan> s2;
  TriPlan q = {3, 3, 1, 1};
  s2.push_back(q);
  run_frame(t, s2, 0x7002, -1);
  cke(2, t.frames_published_o, "frame two published");

  cke(refs1 + 1, t.ab_refs_binned_o, "frame two binned exactly one reference");
  cke(chunks1 + 1, t.ab_chunks_emitted_o, "and emitted exactly one chunk");

  // Tile 0 was covered nineteen times in frame one and NOT AT ALL in frame
  // two. Its head must be gone, not stale.
  cke(0xFFFFFFFFu, head_of(t, 0),
      "frame one's head did not survive the seal");

  const uint32_t head = head_of(t, quiet);
  ckt(head != 0xFFFFFFFFu, "frame two's tile has a head");
  if (head != 0xFFFFFFFFu) {
    const Walked w = walk(t, head);
    ckt(w.completed, "the walk completed");
    cke(1, w.source.size(), "one reference");
    if (w.source.size() == 1) cke(kDecoy, w.source[0], "and it is the right one");
    cke(0x7002, peek16(t, t.publish_chunk_base_o + head * 64 + 6),
        "the chunk carries FRAME TWO's generation");
  }
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// THE PRICE OF ENTRY I55'S RASTER SWAP -- ONE STIMULUS, BOTH CONSUMERS.
// ---------------------------------------------------------------------------
// REBUILT HERE BY ARENACOMPOSE, 2026-09-26. It lived in
// `geom_chunkser_directed.cpp`, which was retired with `zhao_geom_chunkser`.
// The NUMBER it produces is the one entry I55 quotes to refuse the raster
// swap, so throwing the probe away and keeping the prose would have left a
// measurement nobody could reproduce -- CLAUDE.md's ground-contact chapter,
// "commit the probe".
//
// IT MEASURES THE CONSUMER SIDE, not the producer: `zhao_geom_binner_v2`'s
// on-chip job drain (what `zhao_raster_tile_pipe_v2` eats today, and what the
// swap would replace) against `zhao_geom_paramwalk`'s external walk over the
// published SDRAM arena. Both are clocks per tile reference delivered to a
// raster consumer.
//
// NOTE THE TWO DIFFERENCES IN CONVENTION, because BINARENA found them and the
// entry's old "same unit on both sides" was doing more work than it could
// carry: the drain is a SPAN divided by `refs - 1` while the walk is an
// accumulated busy total divided by emitted triangles, and the two cover
// different populations. The `refs - 1` convention inflates the drain figure
// slightly at this scene size -- i.e. in the direction that makes the refusal
// look STRONGER -- so it is left as it was rather than quietly improved, and
// said here instead.
//
// AND IT IS THE PLACE THE ON-CHIP ASSERTION IS NOT VACUOUS. In this build the
// binner is elaborated, fed the same scene and drained for real, so
// "the arena's chunks all came from GEOM.ARENABIN" is a measurement about a
// live competitor rather than about a tied-off generate.
void price_the_swap() {
  Dut t;
  bring_up(t);
  const std::vector<TriPlan> s = scene();

  // ---- open the frame on both producers -----------------------------------
  ckt(seal(t, 0x6666), "the arena sealed a frame");
  t.bin_frame_begin_i = 1;
  t.eval();
  zhao::tick(t);
  t.bin_frame_begin_i = 0;
  t.eval();
  idle(t, 700);   // both directories clear one entry per clock

  for (int i = 0; i < 3; ++i)
    ckt(push_pv(t, 100 + i), "the three vertices every descriptor names");
  for (int i = 0; i < kDecoy; ++i) ckt(push_td(t, i), "decoy descriptor taken");

  // ---- one scene, offered to BOTH ----------------------------------------
  for (size_t i = 0; i < s.size(); ++i) {
    const uint32_t arena_id = static_cast<uint32_t>(i) + kDecoy;
    ckt(push_td(t, arena_id), "descriptor taken");
    ckt(push_tri(t, s[i], arena_id, true), "triangle offered to the producer");
    ckt(push_bin_tri(t, s[i], static_cast<uint16_t>(i)),
        "the same triangle offered to the on-chip binner");
  }

  // ---- end the frame and drain the binner for the picture -----------------
  t.bin_frame_end_i = 1;
  t.ab_geom_done_i = 1;
  t.eval();
  zhao::tick(t);
  t.bin_frame_end_i = 0;
  t.ab_geom_done_i = 0;
  t.eval();

  drain_the_binner(t);

  const uint32_t want_pub = t.frames_published_o + 1;
  for (int i = 0; i < 2000000; ++i) {
    t.eval();
    if (t.frames_published_o >= want_pub) break;
    zhao::tick(t);
  }
  cke(want_pub, t.frames_published_o, "the frame published");

  // THE CONTROL, AND IT IS NOT A CONSTANT. The binner ran the same scene at
  // full rate in this very elaboration.
  ckt(t.bin_tile_references_o > 0,
      "the on-chip binner really binned this scene");
  ckt(t.ab_refs_binned_o > 0, "and so did GEOM.ARENABIN");
  cke(t.ab_chunks_emitted_o, t.chunks_written_o,
      "every chunk the arena holds came from GEOM.ARENABIN");

  // ---- walk the same tile out of SDRAM ------------------------------------
  const uint32_t head = head_of(t, tile_index(0, 0));
  if (head != 0xFFFFFFFFu) {
    walk(t, head);
    t.eval();
  }

  const double drain_per =
      t.dbg_drain_refs_o > 1
          ? static_cast<double>(t.dbg_drain_cycles_o) /
                static_cast<double>(t.dbg_drain_refs_o - 1)
          : 0.0;
  const double walk_per =
      t.tris_emitted_o ? static_cast<double>(t.dbg_walk_cycles_o) /
                             static_cast<double>(t.tris_emitted_o)
                       : 0.0;
  std::printf(
      "PRICE on-chip drain: refs=%u span=%u clocks -> %.2f clocks/ref\n"
      "PRICE external walk: tris=%u busy=%u clocks reqs=%u -> %.2f clocks/tri\n",
      (unsigned)t.dbg_drain_refs_o, (unsigned)t.dbg_drain_cycles_o, drain_per,
      (unsigned)t.tris_emitted_o, (unsigned)t.dbg_walk_cycles_o,
      (unsigned)t.dbg_walk_reqs_o, walk_per);
}

// THE PRICE -- reported, not asserted, and only where both paths exist.
// ---------------------------------------------------------------------------
// Entry I55 carries WALKSWAP's measurement of the CONSUMER side: 4.12
// clocks/ref for the on-chip drain against 29.89 for the external walk. This
// measures the PRODUCER side, in the same unit -- clocks per tile reference --
// so the two halves of the cost can be added rather than confused.
//
// It runs only in the `HAVE_ONCHIP = 1` elaboration, because the comparison
// needs the serialiser compiled in. It also makes the one on-chip assertion
// that is NOT vacuous: with the whole legacy path present and live, it
// produced NOTHING, because GEOM.ARENABIN owns the intake.
void price_the_producer() {
  Dut t;
  bring_up(t);
  const std::vector<TriPlan> s = scene();
  run_frame(t, s, 0x4242, -1);

  cke(1, t.frames_published_o, "the frame published");
  // NOT A CONSTANT HERE. In this build `zhao_geom_binner_v2` is elaborated and
  // clocked. It was offered no triangle in this case, so it binned nothing --
  // `price_the_swap` below is the case that feeds it and measures it.
  cke(0, t.bin_tile_references_o, "the on-chip binner binned NOTHING");
  ckt(t.chunks_written_o > 0, "and the external arena was filled anyway");

  const uint32_t refs_a = t.ab_refs_binned_o;
  const uint32_t busy_a = t.ab_busy_clocks_o;
  const uint32_t stall_a = t.ab_intake_stall_o;
  const double per_a = refs_a ? static_cast<double>(busy_a) / refs_a : 0.0;
  std::printf("PRICE producer, SMALL scene: refs=%u busy=%u clocks chunks=%u"
              " links=%u stall=%u -> %.2f clocks/ref\n",
              (unsigned)refs_a, (unsigned)busy_a,
              (unsigned)t.ab_chunks_emitted_o, (unsigned)t.ab_links_patched_o,
              (unsigned)stall_a, per_a);

  // ONE POINT IS NOT A PRICE, AND AT THIS SCENE SIZE IT IS A LIE IN THE
  // ALARMING DIRECTION. `busy_o` is high for the WHOLE frame, which includes a
  // 576-entry directory CLEAR and a 576-entry end-of-frame FLUSH SWEEP -- both
  // fixed, both independent of how many references the frame holds. Divided by
  // 27 references they dominate the quotient completely and the block reads far
  // more expensive than it is.
  //
  // So: TWO POINTS, and solve for the line. A second frame with a much larger
  // reference count separates the fixed per-frame sweep from the marginal cost
  // of a reference, which is the number that actually scales to R7's giant.
  std::vector<TriPlan> big;
  TriPlan whole = {0, 3, 0, 1};        // every tile of the 4x2 active grid
  for (int i = 0; i < 200; ++i) big.push_back(whole);
  run_frame(t, big, 0x4243, -1);
  cke(2, t.frames_published_o, "the large frame published too");
  cke(0, t.ab_overflow_o, "and did not overflow");

  const uint32_t refs_b = t.ab_refs_binned_o - refs_a;
  const uint32_t busy_b = t.ab_busy_clocks_o - busy_a;
  const double per_b = refs_b ? static_cast<double>(busy_b) / refs_b : 0.0;
  std::printf("PRICE producer, LARGE scene: refs=%u busy=%u clocks"
              " chunks=%u total -> %.2f clocks/ref\n",
              (unsigned)refs_b, (unsigned)busy_b,
              (unsigned)t.ab_chunks_emitted_o, per_b);

  if (refs_b > refs_a) {
    const double marginal =
        static_cast<double>(busy_b - busy_a) /
        static_cast<double>(refs_b - refs_a);
    const double fixed = static_cast<double>(busy_a) - marginal * refs_a;
    std::printf("PRICE producer, SOLVED: fixed=%.0f clocks/frame"
                " marginal=%.2f clocks/ref\n", fixed, marginal);
    std::printf("PRICE producer, AT R7's GIANT (32768 refs): %.2f clocks/ref\n",
                (fixed + marginal * 32768.0) / 32768.0);
    // THE FIXED TERM IS NOT A DEFECT AND NOT FREE. It is two sweeps of the
    // 576-entry directory. It is reported so the next packet can decide
    // whether to spend logic shortening it rather than rediscovering it.
    ckt(fixed > 0.0, "the fixed per-frame sweep is real and measured");
  }
  std::printf("PRICE producer backpressure: small=%u large=%u clocks"
              " (clocks the geometry stream waited on this block)\n",
              (unsigned)stall_a, (unsigned)(t.ab_intake_stall_o - stall_a));
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  const bool price = (argc > 1) && (std::strcmp(argv[1], "price") == 0);

  if (price) {
    price_the_producer();
    price_the_swap();
    std::printf("geom_arenabin_price: %d checks, %d failures\n", g_checks,
                g_fail);
  } else {
    case1_the_external_arena_is_filled_without_the_onchip_one();
    case2_a_nameless_triangle_is_not_binned();
    case3_link_illegal_fires();
    case4_the_directory_does_not_outlive_its_frame();
    std::printf("geom_arenabin_directed: %d checks, %d failures\n", g_checks,
                g_fail);
  }

  zhao::exit_hard(g_fail == 0 ? 0 : 1);
  return 0;
}
