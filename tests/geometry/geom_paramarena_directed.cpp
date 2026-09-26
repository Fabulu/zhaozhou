// geom_paramarena_directed.cpp -- GEOM.PARAMBUF's ARENA AND WALKER, THROUGH
// REAL MEMORY.  The owner's completion ruling of 2026-09-22, item 4.
//
// ===========================================================================
// WHAT THIS FILE IS EVIDENCE OF
// ===========================================================================
// Item 4: "Do not pack fields into a byte vector merely to unpack them again
// and count that as external-memory integration."
//
// So the headline case here is not that a decoder decodes.  It is that a
// triangle descriptor's six fields, written as FIELDS into
// `zhao_geom_paramarena`, come back out of `zhao_geom_paramwalk` BIT-IDENTICAL
// after travelling through the real MEM.GUARD's verdict, the real
// MEM.VRAM.ARBITER's credit law, the real MEM.SDRAM controller's burst and a
// behavioural DRAM -- and that the frame directory that names where they live
// made the same trip and still agrees with the producer's own registers
// (`dir_mismatch_o`).
//
// Eleven cases, each of which could FAIL:
//
//   1  THE ROUND TRIP.  Fields in, bytes through DRAM, fields out, compared.
//   2  `dir_mismatch_o` silent on a healthy frame AND FIRED by corrupting the
//      directory in the DRAM behind the block's back.  A detector that has not
//      been seen to move is a claim, not an instrument.
//   3  QUOTA OVERFLOW: the counter, the fault, the source id, and -- the most
//      important assertion in the file -- `publish_*_o` STILL DESCRIBING THE
//      PRIOR COMPLETE FRAME.  That is R7's fallback contract as a mechanism.
//   4  THE FAULTED FRAME STILL ACCEPTS.  `ready` stays high and
//      `records_discarded_o` counts, so the fault does not stall GEOM.ASSEMBLE.
//   5  THE VIEW ALTERNATES and a seal offered with a write outstanding is HELD.
//   6  PUBLICATION WAITS FOR RETIREMENT (`publish_blocked_o` nonzero).
//   7  A STALE CHUNK -- a real one, carried over in the view two frames later,
//      not poked into memory.
//   8  AN ILLEGAL CHUNK -- a count above fourteen, and a `next` at ARENA_CHUNKS.
//   9  `guard_violations` ZERO across every legal case.
//  10  `stray_beat_o` / `short_burst_o` zero on healthy traffic.
//  11  THE SCRATCH HAS ONE OWNER, and release is an act.
//
// ===========================================================================
// CASE 1a IS THE REGRESSION GUARD ON `pb_lease_valid_o`
// ===========================================================================
// Every case in this file runs against the STRAIGHT composition: the guard's
// three PARAMBUF ports driven from the arena's own outputs and nothing else.
// There is no workaround knob in the bench and there never is one again.
//
// Case 1a is a separate, minimal model whose only job is ONE FRAME PUBLISHING.
// It exists because that is what the first version of this composition could
// not do: `pb_lease_valid_o` was `frame_open_q || pub_valid_q` and the
// directory write is issued from the publication arm, which is reachable only
// after `frame_end_i` has cleared `frame_open_q`.  The guard refused it, at
// exactly SCRATCH_BASE, and no frame could ever publish.  Production was
// repaired (`|| pub_pending_q`, plus releasing `scr_mine_q` on the fault path).
//
// It is kept SEPARATE and FIRST so a regression in that one term is reported
// as four failed checks and a banner naming the file, the line and the repair,
// rather than as three hundred failures in ten unrelated cases.
#include <cstdint>
#include <cstdio>
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
                static_cast<unsigned long long>(want), static_cast<unsigned long long>(want),
                static_cast<unsigned long long>(got), static_cast<unsigned long long>(got));
  }
}

void ckt(bool cond, const char* what) {
  ++g_checks;
  if (!cond) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

// ------------------------------------------------------------ the map -----
// Mirrors zhao_pkg's 5c constants and zhao_geom_paramarena's derived layout.
// Written out rather than read from a header so a silent change to either side
// shows up as a failing address, which is the direction that gets noticed.
constexpr uint32_t VIEW0_BASE   = 0x06000000u;
constexpr uint32_t VIEW1_BASE   = 0x06400000u;
constexpr uint32_t SCRATCH_BASE = 0x06800000u;

constexpr uint32_t PV_B = 24;
constexpr uint32_t TD_B = 16;
constexpr uint32_t CK_B = 64;

// THE BURST-ALIGNMENT QUANTUM AND THE VERTEX ALLOCATION STRIDE, 2026-09-23.
// `PV_B` is the RECORD (R7 freezes it at 24 bytes).  `PV_SLOT_B` is what the
// allocator ADVANCES by, and it is 32 because 24 is not a multiple of 16 and
// a 24-byte stride puts every ODD vertex eight bytes into an aligned
// eight-column block -- where a JEDEC BL8 SEQUENTIAL burst wraps.
//
// These mirror `zhao_geom_paramarena`'s `PV_SLOT_B` and `LAYOUT_ALIGN_B`.  A
// mirrored constant is a frozen copy of yesterday's agreement, so the bench
// does not merely restate them: the alignment ASSERTION at the end of this
// file re-derives the property from the addresses the RTL actually issued,
// through `req_unaligned_o`, which is the bench's OWN observer on the guard
// request and not the block's counter.
constexpr uint32_t BURST_ALIGN_B = 16;
constexpr uint32_t PV_SLOT_B     = 32;

constexpr uint32_t MAX_VERTS  = 65535;
constexpr uint32_t MAX_TRIS   = 16384;
constexpr uint32_t ARENA_CHUNKS = 16384;

constexpr uint32_t align_up(uint32_t v) {
  return ((v + BURST_ALIGN_B - 1u) / BURST_ALIGN_B) * BURST_ALIGN_B;
}

constexpr uint32_t TRI_OFF_B   = align_up(MAX_VERTS * PV_SLOT_B);          // 2,097,120
constexpr uint32_t CHUNK_OFF_B = align_up(TRI_OFF_B + MAX_TRIS * TD_B);    // 2,359,264

uint32_t view_base(int view) { return view ? VIEW1_BASE : VIEW0_BASE; }

// ------------------------------------------------------ DRAM backdoors -----
// The peek port is combinational, so it costs no cycles and disturbs nothing.
uint16_t peek16(Dut& t, uint32_t byte_addr) {
  t.peek_en_i = 1;
  t.peek_waddr_i = byte_addr >> 1;
  t.eval();
  const uint16_t v = t.peek_data_o;
  t.peek_en_i = 0;
  t.eval();
  return v;
}

uint32_t peek32(Dut& t, uint32_t byte_addr) {
  return static_cast<uint32_t>(peek16(t, byte_addr))
       | (static_cast<uint32_t>(peek16(t, byte_addr + 2)) << 16);
}

// The poke lands on a clock edge: it is a backdoor, not a bus master, and it
// exists so a test can construct a fault NO LEGAL REQUEST CAN PRODUCE.
void poke16(Dut& t, uint32_t byte_addr, uint16_t data) {
  t.poke_en_i = 1;
  t.poke_waddr_i = byte_addr >> 1;
  t.poke_data_i = data;
  zhao::tick(t);
  t.poke_en_i = 0;
  t.eval();
}

// ------------------------------------------------------------- stimulus ---
void idle_cycles(Dut& t, int n) {
  for (int i = 0; i < n; ++i) zhao::tick(t);
}

struct Vertex {
  int32_t  x, y;
  uint32_t invw;
  uint8_t  status;
  int32_t  uow, vow;
  uint32_t rgba;
};

struct Descriptor {
  uint16_t v0, v1, v2, material;
  uint32_t raster, source;
};

// Offer a record and hold it until the arena takes it.  TRUE ready/valid: the
// arena's `ready` falls while its one-op engine is busy, so a bench that fired
// and forgot would lose every second record and the round trip would compare
// against a frame that was never written.
bool push_pv(Dut& t, const Vertex& v, int max_wait = 20000) {
  t.pv_valid_i = 1;
  t.pv_x_i = static_cast<uint32_t>(v.x);
  t.pv_y_i = static_cast<uint32_t>(v.y);
  t.pv_invw_i = v.invw;
  t.pv_status_i = v.status;
  t.pv_uow_i = static_cast<uint32_t>(v.uow);
  t.pv_vow_i = static_cast<uint32_t>(v.vow);
  t.pv_rgba_i = v.rgba;
  for (int i = 0; i < max_wait; ++i) {
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

bool push_td(Dut& t, const Descriptor& d, int max_wait = 20000) {
  t.td_valid_i = 1;
  t.td_v0_i = d.v0;
  t.td_v1_i = d.v1;
  t.td_v2_i = d.v2;
  t.td_material_i = d.material;
  t.td_raster_i = d.raster;
  t.td_source_i = d.source;
  for (int i = 0; i < max_wait; ++i) {
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

bool push_ck(Dut& t, uint32_t next, uint16_t count, const uint32_t ids[14],
             int max_wait = 20000) {
  t.ck_valid_i = 1;
  t.ck_next_i = next;
  t.ck_count_i = count;
  for (int k = 0; k < 14; ++k) t.ck_ids_i[k] = ids[k];
  for (int i = 0; i < max_wait; ++i) {
    t.eval();
    const bool go = t.ck_ready_o != 0;
    zhao::tick(t);
    if (go) {
      t.ck_valid_i = 0;
      t.eval();
      return true;
    }
  }
  t.ck_valid_i = 0;
  t.eval();
  return false;
}

// A seal is a REQUEST, not a command: held pending until the drain
// precondition is met.  Returns the number of cycles it was held.
int seal(Dut& t, uint32_t verts, uint32_t tris, uint32_t chunks, uint16_t gen,
         int max_wait = 20000) {
  t.seal_valid_i = 1;
  t.seal_verts_i = verts;
  t.seal_tris_i = tris;
  t.seal_chunks_i = chunks;
  t.frame_gen_i = gen;
  int waited = 0;
  for (int i = 0; i < max_wait; ++i) {
    t.eval();
    const bool go = t.seal_ready_o != 0;
    zhao::tick(t);
    if (go) {
      t.seal_valid_i = 0;
      t.eval();
      return waited;
    }
    ++waited;
  }
  t.seal_valid_i = 0;
  t.eval();
  return -1;
}

// WAIT FOR THE ENGINE TO DRAIN.  `seal_ready_o` IS the drain indicator: the
// arena drives it from `drained_c && !reader_busy_i && !pub_pending_q`, so it
// is high exactly when nothing this block issued is outstanding.  Used before
// `frame_end_i` so a denial can be attributed to the DIRECTORY write rather
// than to a record write that was still in flight -- two different faults with
// the same root cause, and a test that conflated them would report one.
bool wait_drained(Dut& t, int max_wait = 400000) {
  for (int i = 0; i < max_wait; ++i) {
    t.eval();
    if (t.seal_ready_o) return true;
    zhao::tick(t);
  }
  return false;
}

// HOW MANY CLOCKS THE SCRATCH TAKES TO FALL, measured rather than assumed.
// Returns the number of clocks after the call at which `pb_scratch_valid_o`
// first reads 0, or -1 if it never does.
//
// WHY THIS IS A MEASURED BOUND AND NOT `cke(0, pb_scratch_valid_o)`.  The
// first version of this check sampled the bit the instant `frames_published_o`
// moved, which pinned the release to ZERO LATENCY -- a property nobody ever
// asked for.  When the release became one statement keyed on `!pub_pending_q`
// (so that the faulted path and the published path take the identical exit,
// rather than a second per-exit-path copy that no legal stimulus could reach),
// the release moved one clock later and the check went red for a reason that
// was not a defect.
//
// The requirement is "release is an ACT" -- the scratch comes down promptly and
// is not held for the rest of the run.  So the check measures the latency and
// bounds it, which is STRICTLY STRONGER than the equality it replaces: a leak
// times out and fails, and a release that started taking twenty clocks would
// fail too, where `idle for a while, then look` would have hidden both.
int scratch_release_clocks(Dut& t, int max_wait = 500) {
  for (int i = 0; i < max_wait; ++i) {
    t.eval();
    if (!t.pb_scratch_valid_o) return i;
    zhao::tick(t);
  }
  return -1;
}

void end_frame(Dut& t) {
  t.frame_end_i = 1;
  t.eval();
  zhao::tick(t);
  t.frame_end_i = 0;
  t.eval();
}

// Wait for `frames_published_o` to reach `want`.  Bounded: a publication that
// never lands must be reported as a stuck machine, not waited on forever.
bool wait_publish(Dut& t, uint32_t want, int max_wait = 400000) {
  for (int i = 0; i < max_wait; ++i) {
    t.eval();
    if (t.frames_published_o >= want) return true;
    zhao::tick(t);
  }
  return false;
}

struct WalkResult {
  bool completed = false;
  bool failed = false;
  std::vector<Descriptor> tris;
  std::vector<uint8_t> illegal;
};

// Run one walk of `head` and collect every triangle it emits.
WalkResult walk(Dut& t, uint32_t head, int max_wait = 400000) {
  WalkResult r;
  t.walk_head_i = head;
  t.t_ready_i = 1;
  // W_IDLE takes the walk on `walk_valid_i && pub_valid_i`; hold valid for
  // exactly the cycle that starts it, or the walker re-enters from W_END and
  // runs the chain a second time.
  t.walk_valid_i = 1;
  t.eval();
  zhao::tick(t);
  t.walk_valid_i = 0;
  t.eval();

  for (int i = 0; i < max_wait; ++i) {
    t.eval();
    if (t.t_valid_o && t.t_ready_i) {
      Descriptor d;
      d.v0 = t.t_v0_o;
      d.v1 = t.t_v1_o;
      d.v2 = t.t_v2_o;
      d.material = t.t_material_o;
      d.raster = t.t_raster_o;
      d.source = t.t_source_o;
      r.tris.push_back(d);
      r.illegal.push_back(static_cast<uint8_t>(t.t_illegal_o));
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
  t.pv_x_i = 0;
  t.pv_y_i = 0;
  t.pv_invw_i = 0;
  t.pv_status_i = 0;
  t.pv_uow_i = 0;
  t.pv_vow_i = 0;
  t.pv_rgba_i = 0;
  t.td_valid_i = 0;
  t.td_v0_i = 0;
  t.td_v1_i = 0;
  t.td_v2_i = 0;
  t.td_material_i = 0;
  t.td_raster_i = 0;
  t.td_source_i = 0;
  t.ck_valid_i = 0;
  t.ck_next_i = 0;
  t.ck_count_i = 0;
  for (int k = 0; k < 14; ++k) t.ck_ids_i[k] = 0;
  t.walk_valid_i = 0;
  t.walk_head_i = 0;
  t.t_ready_i = 0;
  t.peek_en_i = 0;
  t.peek_waddr_i = 0;
  t.wcfg_chunk_base_bump_i = 0;
  t.poke_en_i = 0;
  t.poke_waddr_i = 0;
  t.poke_data_i = 0;
  t.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();
  // The controller's init sequence (PRECHARGE-ALL + two AUTO_REFRESH + MRS)
  // must complete before any client traffic, or every burst is a protocol
  // error in the model and the failure looks like the DUT's.
  for (int i = 0; i < 400 && !t.init_done_o; ++i) zhao::tick(t);
}

// The frame directory, as the arena writes it and the walker reads it.
struct Directory {
  uint32_t vert_base, tri_base, chunk_base;
  uint16_t gen;
  uint16_t view_valid;
  uint32_t verts, tris, chunks;
};

Directory read_directory(Dut& t) {
  Directory d;
  d.vert_base  = peek32(t, SCRATCH_BASE + 0);
  d.tri_base   = peek32(t, SCRATCH_BASE + 4);
  d.chunk_base = peek32(t, SCRATCH_BASE + 8);
  d.gen        = peek16(t, SCRATCH_BASE + 12);
  d.view_valid = peek16(t, SCRATCH_BASE + 14);
  d.verts      = peek32(t, SCRATCH_BASE + 16);
  d.tris       = peek32(t, SCRATCH_BASE + 20);
  d.chunks     = peek32(t, SCRATCH_BASE + 24);
  return d;
}

// ===========================================================================
// CASE 1a -- THE STRAIGHT COMPOSITION
// ===========================================================================
// The guard's PARAMBUF lease driven from the arena's own outputs and nothing
// else.  One frame, sealed, filled, ended.  The CORRECT outcome is one
// published frame and zero guard violations; that is what is asserted.
void case1a_straight_composition() {
  std::printf("\n=== case 1a: the first frame PUBLISHES "
              "(the regression guard on pb_lease_valid_o)\n");
  Dut t;
  bring_up(t);

  const uint32_t gen = 0x1234;
  const int nv = 4, nt = 2;
  ckt(seal(t, 64, 8, 4, gen) >= 0, "1a: the first seal takes effect");

  for (int i = 0; i < nv; ++i) {
    Vertex v{100 + i, 200 + i, 0x000300u + static_cast<uint32_t>(i),
             static_cast<uint8_t>(i), 1000 + i, 2000 + i, 0x11223344u + static_cast<uint32_t>(i)};
    ckt(push_pv(t, v), "1a: a vertex is accepted");
  }
  for (int i = 0; i < nt; ++i) {
    Descriptor d{static_cast<uint16_t>(i), static_cast<uint16_t>(i + 1),
                 static_cast<uint16_t>(i + 2), static_cast<uint16_t>(0xA000 + i),
                 0xDEAD0000u + static_cast<uint32_t>(i), 0xBEEF0000u + static_cast<uint32_t>(i)};
    ckt(push_td(t, d), "1a: a descriptor is accepted");
  }
  uint32_t ids[14] = {0};
  ids[0] = 0;
  ids[1] = 1;
  ckt(push_ck(t, 0xFFFFFFFFu, 2, ids), "1a: a chunk is accepted");

  // DRAIN FIRST.  Without this the last record's write is still in flight when
  // `frame_end_i` clears `frame_open_q`, and the guard refuses THAT -- a second
  // instance of the same root cause that would mask the directory one.
  ckt(wait_drained(t), "1a: the engine drained before the frame ended");
  const uint32_t viol_before_end = t.guard_violations_o;
  cke(0, viol_before_end, "1a: nothing was refused while the frame was OPEN");

  end_frame(t);
  const bool published = wait_publish(t, 1, 200000);

  // THE ASSERTION IS THE CORRECT BEHAVIOUR, not the bug.  A repaired block
  // publishes exactly one frame here and the guard refuses nothing.
  cke(1, t.frames_published_o, "1a: the first frame is PUBLISHED");
  cke(0, t.guard_violations_o, "1a: the guard refuses nothing the arena asks for");
  cke(0, t.arena_guard_denied_o, "1a: the arena sees no denial");

  // THE SCRATCH IS RELEASED once the frame has landed.  Before the lease repair
  // it was held for the rest of the run: `scr_mine_q` is raised when the
  // directory write issues, so the denial left it set and the walker could
  // never be granted the scratch.
  //
  // MEASURED AND BOUNDED, not sampled once -- see `scratch_release_clocks`.
  // The bound is four clocks because the release is ONE REGISTER, not a
  // sequence: anything slower means it has started waiting for something, and
  // that is a change worth failing on.
  {
    const int lat = scratch_release_clocks(t);
    ckt(lat >= 0, "1a: the scratch is RELEASED after the frame publishes (not leaked)");
    ckt(lat >= 0 && lat <= 4,
        "1a: and the release is prompt -- within four clocks of publication");
    std::printf("1a: the scratch fell %d clock(s) after frames_published_o moved\n", lat);
  }

  if (!published || t.guard_violations_o != 0) {
    std::printf(
        "\n"
        "***************************************************************\n"
        "* REGRESSION in fpga/rtl/geometry/zhao_geom_paramarena.sv      *\n"
        "***************************************************************\n"
        "  The repaired line is\n"
        "    pb_lease_valid_o = frame_open_q || pub_pending_q || pub_valid_q;\n"
        "  If `pub_pending_q` has been dropped from it again, this is what\n"
        "  happens, and it is what this case exists to catch:\n"
        "\n"
        "  The frame DIRECTORY write is issued from the publication arm,\n"
        "  which is reachable only after `frame_end_i` has cleared\n"
        "  `frame_open_q`.  On the FIRST frame `pub_valid_q` is still 0,\n"
        "  so the lease is LOW at the exact cycle the block asks MEM.GUARD\n"
        "  to write the directory into the scratch.  Every PARAMBUF arm of\n"
        "  the guard (pb_rd_ok / pb_wr_ok / pb_scr_ok) requires\n"
        "  pb_lease_valid, so the write is REFUSED.\n"
        "\n"
        "  Measured here:\n"
        "    guard_violations      = %u\n"
        "    arena guard_denied_o  = %u\n"
        "    refused request       = addr 0x%08X len %u write %u\n"
        "    frames_published_o    = %u\n"
        "    frame_fault_o         = %u\n"
        "    pb_scratch_valid_o    = %u  (if this reads 1, `scr_mine_q` has\n"
        "                                 leaked too: it is set when the\n"
        "                                 directory write is issued and must be\n"
        "                                 cleared on the FAULT path as well as\n"
        "                                 the success one, or the walker can\n"
        "                                 never be granted the scratch again)\n"
        "\n"
        "  CONSEQUENCE: no frame can EVER be published.  The denial sets\n"
        "  frame_fault_q, the publication arm drops pub_pending_q, pub_valid_q\n"
        "  stays 0, and the next frame meets the same low lease.  The walker's\n"
        "  `walk_ready_o` is gated on pub_valid_i, so the reader never runs\n"
        "  either.\n"
        "\n"
        "  THE REPAIR (one term):\n"
        "    assign pb_lease_valid_o = frame_open_q || pub_pending_q\n"
        "                           || pub_valid_q;\n"
        "  A frame that has ended but not yet published still owns the arena --\n"
        "  that is exactly what pub_pending_q means -- and the scratch write is\n"
        "  the last act of owning it.  There is NO WORKAROUND KNOB in this\n"
        "  bench; fix the RTL.\n"
        "***************************************************************\n\n",
        t.guard_violations_o, t.arena_guard_denied_o,
        static_cast<unsigned>(t.guard_viol_addr_o), static_cast<unsigned>(t.guard_viol_len_o),
        static_cast<unsigned>(t.guard_viol_write_o), t.frames_published_o,
        static_cast<unsigned>(t.frame_fault_o), static_cast<unsigned>(t.pb_scratch_valid_o));
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  case1a_straight_composition();

  // =========================================================================
  // THE REST OF THE SUITE -- the same straight composition, no workaround.
  // =========================================================================
  std::printf("\n=== cases 1..11, straight composition, no workaround\n");
  Dut t;
  bring_up(t);
  ckt(t.init_done_o != 0, "the SDRAM controller finished its init sequence");

  // -----------------------------------------------------------------------
  // PRELUDE -- THE INTAKE IS A FREE SINK WITH NO FRAME OPEN.
  // Not one of the eleven, but it is the composition requirement the arena's
  // header calls a DEADLOCK rather than a throughput regression: this block
  // taps a live production stream, so a `ready` that is low before the first
  // seal stalls GEOM.ASSEMBLE forever.
  // -----------------------------------------------------------------------
  {
    Descriptor d{0, 1, 2, 0x5555, 0x01020304u, 0x0000AAAAu};
    ckt(push_td(t, d), "prelude: a descriptor with no frame open is ACCEPTED");
    Vertex v{1, 2, 3, 4, 5, 6, 7};
    ckt(push_pv(t, v), "prelude: a vertex with no frame open is ACCEPTED");
    cke(2, t.records_unsealed_o, "prelude: both are counted at records_unsealed_o");
    cke(0, t.verts_written_o, "prelude: nothing was written");
    cke(0, t.tris_written_o, "prelude: nothing was written");
    cke(0, t.pb_lease_valid_o, "prelude: with no frame and no publication the lease is SHUT");
  }

  // =======================================================================
  // FRAME A -- view 1, gen 0x1234.  Cases 1, 2, 6, 9, 10, 11.
  // =======================================================================
  const uint16_t GEN_A = 0x1234;
  const int NV = 8;   // vertex ids 0..7, which is what the descriptors index
  const int NT = 6;
  ckt(seal(t, 64, 16, 4, GEN_A) >= 0, "A: the seal takes effect");
  // THE VIEW FLIPS AT SEAL AND NOWHERE ELSE.  Reset leaves view_q at 0, so the
  // first sealed frame builds in VIEW 1.
  cke(1, t.pb_wr_view_o, "A: the first seal flips the build view to 1");

  std::vector<Vertex> verts;
  for (int i = 0; i < NV; ++i) {
    Vertex v;
    v.x = -1000 - i;                       // negative, to exercise the sign
    v.y = 1000 + i;
    v.invw = 0x00ABC0u + static_cast<uint32_t>(i);
    v.status = static_cast<uint8_t>(0x80 + i);
    v.uow = 0x00010000 + i;
    v.vow = -(0x00020000 + i);
    v.rgba = 0xC0FFEE00u + static_cast<uint32_t>(i);
    verts.push_back(v);
    ckt(push_pv(t, v), "A: a vertex is accepted");
  }

  std::vector<Descriptor> tris;
  for (int i = 0; i < NT; ++i) {
    Descriptor d;
    d.v0 = static_cast<uint16_t>(i);
    d.v1 = static_cast<uint16_t>(i + 1);
    d.v2 = static_cast<uint16_t>(i + 2);
    d.material = static_cast<uint16_t>(0xA100 + i);
    d.raster = 0x5AA50000u + static_cast<uint32_t>(i * 7 + 1);
    d.source = 0x0BAD0000u + static_cast<uint32_t>(i * 13 + 3);
    tris.push_back(d);
    ckt(push_td(t, d), "A: a descriptor is accepted");
  }

  // TWO chunks.  The second is what case 7 comes back for two frames later:
  // frame C writes only chunk 0, so chunk 1 in view 1 still carries frame A's
  // generation -- a real carried-over chunk, not one poked into memory.
  {
    uint32_t ids[14] = {0};
    for (int k = 0; k < NT; ++k) ids[k] = static_cast<uint32_t>(k);
    ckt(push_ck(t, 0xFFFFFFFFu, static_cast<uint16_t>(NT), ids),
        "A: chunk 0 is accepted");
    uint32_t ids2[14] = {0};
    ids2[0] = 1;
    ckt(push_ck(t, 0xFFFFFFFFu, 1, ids2), "A: chunk 1 is accepted");
  }

  // THE COUNTERS ARE READ AFTER THE DRAIN, NOT AFTER THE PUSH.  `push_*`
  // returns when the RECORD IS ACCEPTED; the write it starts takes tens of
  // clocks to reach DRAM, so a count read at the handshake measures the queue
  // depth rather than the traffic.  (Measured: reading them at the push made
  // `chunks_written_o` read 1 of 2 -- a test artefact that looks exactly like a
  // dropped record.)
  ckt(wait_drained(t), "A: the engine drained");
  cke(static_cast<uint32_t>(NV), t.verts_written_o, "A: every vertex reached memory");
  cke(static_cast<uint32_t>(NT), t.tris_written_o, "A: every descriptor reached memory");
  cke(2, t.chunks_written_o, "A: both chunks reached memory");

  const uint32_t pub_blocked_before_A = t.publish_blocked_o;
  end_frame(t);
  ckt(wait_publish(t, 1), "A: the frame publishes");
  cke(1, t.frames_published_o, "A: exactly one frame published");
  cke(GEN_A, t.publish_gen_o, "A: the published generation");
  cke(1, t.publish_view_o, "A: the published view");
  cke(static_cast<uint32_t>(NV), t.publish_verts_o, "A: the published vertex count");
  cke(static_cast<uint32_t>(NT), t.publish_tris_o, "A: the published triangle count");
  cke(2, t.publish_chunks_o, "A: the published chunk count");
  // The published bases are 27-bit, and 0x0600_0000 fits in 27 bits exactly,
  // so these compare whole addresses with no masking.
  cke(VIEW1_BASE, t.publish_vert_base_o, "A: the published vertex base");
  cke(VIEW1_BASE + TRI_OFF_B, t.publish_tri_base_o, "A: the published triangle base");
  cke(VIEW1_BASE + CHUNK_OFF_B, t.publish_chunk_base_o, "A: the published chunk base");

  // -----------------------------------------------------------------------
  // CASE 6 -- PUBLICATION WAITS FOR RETIREMENT.
  // `publish_blocked_o` counts every clock the retire gate held publication.
  // If this is zero the gate is a term nobody can show ever did anything, and
  // that is worth saying loudly rather than deleting the check.
  // -----------------------------------------------------------------------
  {
    const uint32_t blocked = t.publish_blocked_o - pub_blocked_before_A;
    ckt(blocked > 0, "case 6: publish_blocked_o moved -- the retire gate DELAYED publication");
    std::printf("case 6: publish_blocked_o = %u clocks across frame A\n", blocked);
    if (blocked == 0) {
      std::printf(
          "WARNING: the retire gate never held publication.  Either the write\n"
          "         path retires before frame_end_i is seen, or `wr_words_q` is\n"
          "         not counting what it claims to.  Reported rather than\n"
          "         deleted: a gate nobody can show ever did work is not a\n"
          "         protection.\n");
    }
  }

  // -----------------------------------------------------------------------
  // THE BYTES, AS THEY ACTUALLY LIE IN DRAM.
  // An independent second opinion on the round trip: the walker's decode could
  // agree with the arena's pack while BOTH disagreed with R7's layout.  This
  // reads R7's byte offsets straight out of the model.
  // -----------------------------------------------------------------------
  {
    const uint32_t vbase = VIEW1_BASE;
    for (int i = 0; i < 2; ++i) {
      // THE SLOT, not the record: the allocator advances by PV_SLOT_B.
      const uint32_t a = vbase + static_cast<uint32_t>(i) * PV_SLOT_B;
      cke(static_cast<uint32_t>(verts[i].x), peek32(t, a + 0), "A: PV byte 0..3 is screen_x");
      cke(static_cast<uint32_t>(verts[i].y), peek32(t, a + 4), "A: PV byte 4..7 is screen_y");
      const uint32_t w2 = peek32(t, a + 8);
      cke(verts[i].invw, w2 & 0x00FFFFFFu, "A: PV byte 8..10 is invw24");
      cke(verts[i].status, (w2 >> 24) & 0xFFu, "A: PV byte 11 is the status byte");
      cke(static_cast<uint32_t>(verts[i].uow), peek32(t, a + 12), "A: PV byte 12..15 is u/w");
      cke(static_cast<uint32_t>(verts[i].vow), peek32(t, a + 16), "A: PV byte 16..19 is v/w");
      cke(verts[i].rgba, peek32(t, a + 20), "A: PV byte 20..23 is rgba");
    }
    const uint32_t tbase = VIEW1_BASE + TRI_OFF_B;
    for (int i = 0; i < NT; ++i) {
      const uint32_t a = tbase + static_cast<uint32_t>(i) * TD_B;
      cke(tris[i].v0, peek16(t, a + 0), "A: TD byte 0..1 is vertex_id[0]");
      cke(tris[i].v1, peek16(t, a + 2), "A: TD byte 2..3 is vertex_id[1]");
      cke(tris[i].v2, peek16(t, a + 4), "A: TD byte 4..5 is vertex_id[2]");
      cke(tris[i].material, peek16(t, a + 6), "A: TD byte 6..7 is material_id");
      cke(tris[i].raster, peek32(t, a + 8), "A: TD byte 8..11 is raster_state");
      cke(tris[i].source, peek32(t, a + 12), "A: TD byte 12..15 is source_id");
    }
    const uint32_t cbase = VIEW1_BASE + CHUNK_OFF_B;
    cke(0xFFFFFFFFu, peek32(t, cbase + 0), "A: chunk byte 0..3 is next_chunk");
    cke(static_cast<uint32_t>(NT), peek16(t, cbase + 4), "A: chunk byte 4..5 is count");
    // THE GENERATION IS STAMPED BY THE PRODUCER, not supplied by the caller.
    cke(GEN_A, peek16(t, cbase + 6), "A: chunk byte 6..7 is the PRODUCER's frame_generation");
    for (int k = 0; k < NT; ++k)
      cke(static_cast<uint32_t>(k), peek32(t, cbase + 8 + 4 * static_cast<uint32_t>(k)),
          "A: chunk byte 8.. holds the fourteen triangle ids");

    const Directory d = read_directory(t);
    cke(VIEW1_BASE, d.vert_base, "A: the directory's vertex base");
    cke(VIEW1_BASE + TRI_OFF_B, d.tri_base, "A: the directory's triangle base");
    cke(VIEW1_BASE + CHUNK_OFF_B, d.chunk_base, "A: the directory's chunk base");
    cke(GEN_A, d.gen, "A: the directory's generation");
    cke(0x0003, d.view_valid, "A: the directory's {view,valid} word (view 1, valid)");
    cke(static_cast<uint32_t>(NV), d.verts, "A: the directory's vertex count");
    cke(static_cast<uint32_t>(NT), d.tris, "A: the directory's triangle count");
    cke(2, d.chunks, "A: the directory's chunk count");
  }

  // -----------------------------------------------------------------------
  // CASE 1 -- THE ROUND TRIP.
  // -----------------------------------------------------------------------
  {
    std::printf("\n=== case 1: the round trip\n");
    const WalkResult r = walk(t, 0);
    ckt(r.completed, "case 1: the walk completed");
    ckt(!r.failed, "case 1: the walk did not fail");
    cke(static_cast<uint32_t>(NT), static_cast<uint32_t>(r.tris.size()),
        "case 1: every descriptor the chunk names came back");
    for (size_t i = 0; i < r.tris.size() && i < tris.size(); ++i) {
      cke(tris[i].v0, r.tris[i].v0, "case 1: vertex_id[0] is bit-identical");
      cke(tris[i].v1, r.tris[i].v1, "case 1: vertex_id[1] is bit-identical");
      cke(tris[i].v2, r.tris[i].v2, "case 1: vertex_id[2] is bit-identical");
      cke(tris[i].material, r.tris[i].material, "case 1: material_id is bit-identical");
      cke(tris[i].raster, r.tris[i].raster, "case 1: raster_state is bit-identical");
      cke(tris[i].source, r.tris[i].source, "case 1: source_id is bit-identical");
      cke(0, r.illegal[i], "case 1: the descriptor is inside the seal");
    }
    cke(1, t.dirs_read_o, "case 1: the directory was read once");
    cke(0, t.dir_mismatch_o, "case 1 / case 2a: dir_mismatch_o is SILENT on a healthy frame");
    cke(1, t.chunks_walked_o, "case 1: one chunk was walked");
    cke(static_cast<uint32_t>(NT), t.tris_emitted_o, "case 1: the emitted count");
    cke(0, t.tris_illegal_o, "case 1: no descriptor was refused");
    cke(0, t.chunks_stale_o, "case 1: no chunk was stale");
    cke(0, t.chunks_illegal_o, "case 1: no chunk was malformed");
    cke(0, t.walk_cut_o, "case 1: the chain was not cut");
    cke(0, t.gen_race_o, "case 1: the frame did not move under the walk");
  }

  // -----------------------------------------------------------------------
  // CASE 10 (first reading) and CASE 9 -- healthy traffic is clean.
  // -----------------------------------------------------------------------
  cke(0, t.stray_beat_o, "case 10: stray_beat_o is zero on healthy traffic");
  cke(0, t.short_burst_o, "case 10: short_burst_o is zero on healthy traffic");
  cke(0, t.guard_violations_o, "case 9: the guard refused nothing");
  cke(0, t.arena_guard_denied_o, "case 9: the arena saw no denial");
  cke(0, t.walk_guard_denied_o, "case 9: the walker saw no denial");
  cke(0, t.wr_elsewhere_o, "case 9: no arena write left the authorised map");
  cke(0, t.model_error_o, "the DRAM model saw no protocol or timing violation");
  cke(0, t.wq_err_o, "the write-data queue never underflowed");
  cke(0, t.share_err_short_o, "the share saw no short return");
  cke(0, t.share_err_long_o, "the share saw no overlong return");
  cke(0, t.share_err_unowned_o, "the share saw no unowned beat");
  cke(0, t.share_retire_unowned_o, "the share saw no unowned credit");
  cke(0, t.share_wbeat_unowned_o, "the share saw no unowned write beat");
  cke(0, t.share_denied_o, "the share saw no denial");
  cke(0, t.retire_underflow_o, "the arena was never credited words it did not owe");
  cke(0, t.addr_view_bad_o,
      "the held address never lay outside the view the lease names (production)");

  // -----------------------------------------------------------------------
  // CASE 11 -- THE SCRATCH HAS ONE OWNER, AND RELEASE IS AN ACT.
  // -----------------------------------------------------------------------
  {
    std::printf("\n=== case 11: the scratch has one owner\n");
    idle_cycles(t, 20);
    t.eval();
    cke(0, t.pb_scratch_valid_o,
        "case 11: pb_scratch_valid_o is LOW between uses -- release is an act");
    cke(0, t.scr_grant_o, "case 11: no owner holds the scratch at rest");
    cke(0, t.scr_overlap_o,
        "case 11: the walker never held the scratch while the directory was being written");
    cke(0, t.scr_contend_o, "case 11: the walker never asked while the arena held it");
    ckt(t.scratch_open_clocks_o > 0,
        "case 11: the scratch WAS opened -- a window never opened proves nothing");
    std::printf("case 11: the scratch was mapped for %u clocks in total\n",
                t.scratch_open_clocks_o);
  }

  // -----------------------------------------------------------------------
  // CASE 2b -- FIRE `dir_mismatch_o`.
  // The directory is corrupted IN THE DRAM, behind the block's back, which is
  // a fault no legal request can produce.  Without this the counter is a claim.
  // -----------------------------------------------------------------------
  {
    std::printf("\n=== case 2b: dir_mismatch_o FIRED\n");
    const uint16_t saved_gen = peek16(t, SCRATCH_BASE + 12);
    poke16(t, SCRATCH_BASE + 12, static_cast<uint16_t>(saved_gen ^ 0x0F0F));
    cke(static_cast<uint16_t>(saved_gen ^ 0x0F0F), peek16(t, SCRATCH_BASE + 12),
        "case 2b: the backdoor really changed the directory");

    const WalkResult r = walk(t, 0);
    ckt(r.completed, "case 2b: the walk completed");
    ckt(r.failed, "case 2b: the walk FAILED -- a corrupt directory is not followed");
    cke(1, t.dir_mismatch_o, "case 2b: dir_mismatch_o FIRED exactly once");
    cke(2, t.dirs_read_o, "case 2b: the directory was read a second time");
    cke(1, t.chunks_walked_o, "case 2b: no chunk was walked under the corrupt directory");
    cke(static_cast<uint32_t>(NT), t.tris_emitted_o,
        "case 2b: no triangle was emitted under the corrupt directory");

    poke16(t, SCRATCH_BASE + 12, saved_gen);
    cke(saved_gen, peek16(t, SCRATCH_BASE + 12), "case 2b: the directory is restored");
    const WalkResult r2 = walk(t, 0);
    ckt(r2.completed && !r2.failed, "case 2b: the restored directory walks again");
    cke(1, t.dir_mismatch_o, "case 2b: dir_mismatch_o did not move on the restored frame");
  }

  // =======================================================================
  // FRAME B -- view 0, gen 0x5555.  CASE 5.
  // =======================================================================
  std::printf("\n=== case 5: the view alternates and the drain holds\n");
  const uint16_t GEN_B = 0x5555;
  const uint32_t v1_before = t.wr_in_view1_o;
  const uint32_t v0_before = t.wr_in_view0_o;
  const uint32_t scr_before = t.wr_in_scratch_o;
  const uint32_t flipblk_before = t.view_flip_blocked_o;

  ckt(seal(t, 32, 8, 4, GEN_B) >= 0, "B: the seal takes effect");
  cke(0, t.pb_wr_view_o, "case 5: the second seal flips the build view BACK to 0");

  const int NVB = 4, NTB = 3;
  std::vector<Descriptor> trisB;
  for (int i = 0; i < NVB; ++i) {
    Vertex v{2000 + i, -2000 - i, 0x001234u, static_cast<uint8_t>(i),
             7000 + i, 8000 + i, 0xFACE0000u + static_cast<uint32_t>(i)};
    ckt(push_pv(t, v), "B: a vertex is accepted");
  }

  // -------------------------------------------------------------------
  // CASE 5b -- A SEAL OFFERED WITH A WRITE OUTSTANDING IS HELD.
  // Offered immediately after a record, so the engine is mid-op and
  // `wr_words_q` is nonzero.  Dropped the instant `seal_ready_o` rises, or it
  // would take effect and destroy frame B.
  // -------------------------------------------------------------------
  {
    Vertex v{31337, -31337, 0x00BEEFu, 0x7F, 1, 2, 0x99999999u};
    t.pv_valid_i = 1;
    t.pv_x_i = static_cast<uint32_t>(v.x);
    t.pv_y_i = static_cast<uint32_t>(v.y);
    t.pv_invw_i = v.invw;
    t.pv_status_i = v.status;
    t.pv_uow_i = static_cast<uint32_t>(v.uow);
    t.pv_vow_i = static_cast<uint32_t>(v.vow);
    t.pv_rgba_i = v.rgba;
    // consume it
    bool taken = false;
    for (int i = 0; i < 20000 && !taken; ++i) {
      t.eval();
      taken = t.pv_ready_o != 0;
      zhao::tick(t);
    }
    t.pv_valid_i = 0;
    t.eval();
    ckt(taken, "case 5b: the record that makes a write outstanding was accepted");

    // Now offer a seal.  It must be REFUSED while the write is in flight.
    t.seal_valid_i = 1;
    t.seal_verts_i = 8;
    t.seal_tris_i = 8;
    t.seal_chunks_i = 8;
    t.frame_gen_i = 0xDEAD;
    t.eval();
    cke(0, t.seal_ready_o,
        "case 5b: seal_ready_o is LOW while a write this block issued has not retired");
    int blocked_cycles = 0;
    for (int i = 0; i < 4000; ++i) {
      t.eval();
      if (t.seal_ready_o) break;
      ++blocked_cycles;
      zhao::tick(t);
    }
    t.seal_valid_i = 0;
    t.eval();
    ckt(blocked_cycles > 0, "case 5b: the seal was held for at least one clock");
    const uint32_t flipblk = t.view_flip_blocked_o - flipblk_before;
    ckt(flipblk > 0, "case 5b: view_flip_blocked_o counted the held seal");
    cke(0, t.pb_wr_view_o, "case 5b: the view did NOT flip while the seal was held");
    std::printf("case 5b: the seal was held for %d clocks, view_flip_blocked_o += %u\n",
                blocked_cycles, flipblk);
  }

  for (int i = 0; i < NTB; ++i) {
    Descriptor d;
    d.v0 = static_cast<uint16_t>(i);
    d.v1 = static_cast<uint16_t>(i + 1);
    d.v2 = static_cast<uint16_t>(i + 2);
    d.material = static_cast<uint16_t>(0xB200 + i);
    d.raster = 0x12340000u + static_cast<uint32_t>(i);
    d.source = 0x56780000u + static_cast<uint32_t>(i);
    trisB.push_back(d);
    ckt(push_td(t, d), "B: a descriptor is accepted");
  }
  {
    uint32_t ids[14] = {0};
    for (int k = 0; k < NTB; ++k) ids[k] = static_cast<uint32_t>(k);
    ckt(push_ck(t, 0xFFFFFFFFu, static_cast<uint16_t>(NTB), ids), "B: a chunk is accepted");
  }
  end_frame(t);
  ckt(wait_publish(t, 2), "B: the frame publishes");
  cke(2, t.frames_published_o, "B: two frames published");
  cke(0, t.publish_view_o, "case 5: the published view alternated to 0");
  cke(GEN_B, t.publish_gen_o, "B: the published generation");

  // EVERY WRITE OF FRAME B LANDED IN VIEW 0.  Counted at the request, and the
  // memory peek below is the independent second opinion.
  {
    const uint32_t v0_delta = t.wr_in_view0_o - v0_before;
    const uint32_t v1_delta = t.wr_in_view1_o - v1_before;
    const uint32_t scr_delta = t.wr_in_scratch_o - scr_before;
    cke(static_cast<uint32_t>(NVB + 1 + NTB + 1), v0_delta,
        "case 5: every frame-B record write landed in VIEW 0's range");
    cke(0, v1_delta, "case 5: not one frame-B write touched view 1");
    cke(1, scr_delta, "case 5: exactly one scratch write -- the frame directory");
    // Frame A's bytes are STILL THERE, untouched, in view 1.  The two views are
    // disjoint for the same reason the two FB slots are.
    cke(tris[0].source, peek32(t, VIEW1_BASE + TRI_OFF_B + 12),
        "case 5: frame A's bytes in view 1 were not overwritten by frame B");
    cke(trisB[0].source, peek32(t, VIEW0_BASE + TRI_OFF_B + 12),
        "case 5: frame B's bytes are in view 0");
    const Directory d = read_directory(t);
    cke(VIEW0_BASE, d.vert_base, "case 5: the directory now names view 0");
    cke(0x0001, d.view_valid, "case 5: the directory's {view,valid} word (view 0, valid)");
  }
  {
    const WalkResult r = walk(t, 0);
    ckt(r.completed && !r.failed, "case 5: frame B walks");
    cke(static_cast<uint32_t>(NTB), static_cast<uint32_t>(r.tris.size()),
        "case 5: frame B's chunk names three descriptors");
    for (size_t i = 0; i < r.tris.size() && i < trisB.size(); ++i) {
      cke(trisB[i].source, r.tris[i].source, "case 5: frame B's source_id survived");
      cke(trisB[i].raster, r.tris[i].raster, "case 5: frame B's raster_state survived");
    }
  }

  // =======================================================================
  // FRAME C -- view 1, gen 0x7777.  CASE 7 (a REAL stale chunk).
  // =======================================================================
  std::printf("\n=== case 7: a stale chunk is refused\n");
  const uint16_t GEN_C = 0x7777;
  ckt(seal(t, 32, 8, 4, GEN_C) >= 0, "C: the seal takes effect");
  cke(1, t.pb_wr_view_o, "C: the view alternated back to 1");
  {
    for (int i = 0; i < 4; ++i) {
      Vertex v{i, i, 0x000111u, 0, i, i, 0x0u};
      ckt(push_pv(t, v), "C: a vertex is accepted");
    }
    Descriptor d{0, 1, 2, 0xC300, 0xAAAA0000u, 0xBBBB0000u};
    ckt(push_td(t, d), "C: a descriptor is accepted");
    // ONE chunk only.  Chunk index 1 in view 1 still holds FRAME A's chunk,
    // stamped GEN_A -- a chunk carried over in a view that is being rebuilt,
    // which R7 says "reads as a valid chunk in every other respect".
    uint32_t ids[14] = {0};
    ids[0] = 0;
    ckt(push_ck(t, 0xFFFFFFFFu, 1, ids), "C: chunk 0 is accepted");
  }
  end_frame(t);
  ckt(wait_publish(t, 3), "C: the frame publishes");
  cke(GEN_C, t.publish_gen_o, "C: the published generation");
  // The carried-over chunk is genuinely still there with the OLD stamp.
  cke(GEN_A, peek16(t, VIEW1_BASE + CHUNK_OFF_B + CK_B + 6),
      "case 7: chunk 1 in view 1 still carries frame A's generation");

  {
    const uint32_t stale_before = t.chunks_stale_o;
    const uint32_t emitted_before = t.tris_emitted_o;
    const WalkResult r = walk(t, 1);   // the carried-over chunk
    ckt(r.completed, "case 7: the walk completed");
    ckt(r.failed, "case 7: the walk FAILED on the stale chunk");
    cke(stale_before + 1, t.chunks_stale_o, "case 7: chunks_stale_o FIRED");
    cke(emitted_before, t.tris_emitted_o,
        "case 7: not one triangle was emitted from the stale chunk");
    cke(0, static_cast<uint32_t>(r.tris.size()), "case 7: the stale chunk was not followed");
  }

  // =======================================================================
  // FRAME D -- view 0, gen 0x9999.  CASE 8 (illegal chunks).
  // =======================================================================
  std::printf("\n=== case 8: an illegal chunk is refused\n");
  const uint16_t GEN_D = 0x9999;
  ckt(seal(t, 32, 8, 8, GEN_D) >= 0, "D: the seal takes effect");
  {
    for (int i = 0; i < 4; ++i) {
      Vertex v{i, i, 0x000222u, 0, i, i, 0x0u};
      ckt(push_pv(t, v), "D: a vertex is accepted");
    }
    Descriptor d{0, 1, 2, 0xD400, 0xCCCC0000u, 0xDDDD0000u};
    ckt(push_td(t, d), "D: a descriptor is accepted");
    uint32_t ids[14] = {0};
    // chunk 0: a COUNT ABOVE FOURTEEN.  The arena does not police it -- the
    // record layer owns that rule, and this is the test that the rule is
    // reached through real memory rather than at a port.
    ckt(push_ck(t, 0xFFFFFFFFu, 20, ids), "D: chunk 0 (count = 20) is accepted");
    // chunk 1: a `next` AT ARENA_CHUNKS, which is one past the last legal one.
    ckt(push_ck(t, ARENA_CHUNKS, 1, ids), "D: chunk 1 (next = ARENA_CHUNKS) is accepted");
  }
  end_frame(t);
  ckt(wait_publish(t, 4), "D: the frame publishes");

  {
    const uint32_t illegal_before = t.chunks_illegal_o;
    const WalkResult r0 = walk(t, 0);
    ckt(r0.completed && r0.failed, "case 8: the walk of the over-count chunk FAILED");
    cke(illegal_before + 1, t.chunks_illegal_o, "case 8: chunks_illegal_o FIRED on count > 14");
    const WalkResult r1 = walk(t, 1);
    ckt(r1.completed && r1.failed, "case 8: the walk of the out-of-range `next` FAILED");
    cke(illegal_before + 2, t.chunks_illegal_o,
        "case 8: chunks_illegal_o FIRED on next >= ARENA_CHUNKS");
    cke(0, static_cast<uint32_t>(r0.tris.size() + r1.tris.size()),
        "case 8: neither malformed chunk was followed");
  }

  // =======================================================================
  // FRAME E -- CASES 3 and 4.  A frame that overruns its quota.
  // =======================================================================
  std::printf("\n=== case 3: quota overflow, and the PRIOR COMPLETE FRAME stands\n");
  const uint32_t pub_gen_before = t.publish_gen_o;
  const uint32_t pub_view_before = t.publish_view_o;
  const uint32_t pub_tris_before = t.publish_tris_o;
  const uint32_t pub_tri_base_before = t.publish_tri_base_o;
  const uint32_t frames_before = t.frames_published_o;
  const uint32_t quota_before = t.quota_overflow_o;

  ckt(seal(t, 8, 2, 2, 0xABCD) >= 0, "E: the small-quota seal takes effect");
  {
    for (int i = 0; i < 2; ++i) {
      Descriptor d{0, 1, 2, static_cast<uint16_t>(0xE500 + i),
                   0x11110000u + static_cast<uint32_t>(i), 0x22220000u + static_cast<uint32_t>(i)};
      ckt(push_td(t, d), "E: a descriptor inside the quota is accepted");
    }
    cke(0, t.frame_fault_o, "E: two descriptors against a quota of two is not a fault");
    // THE THIRD ONE OVERRUNS.  R7: "report the source IDs" -- the descriptor
    // carries one, so the fault names the draw that overran.
    Descriptor over{0, 1, 2, 0xE5FF, 0x33330000u, 0x00007733u};
    ckt(push_td(t, over), "E: the overrunning descriptor is still ACCEPTED (never refused)");
    cke(quota_before + 1, t.quota_overflow_o, "case 3: quota_overflow_o FIRED");
    cke(1, t.frame_fault_o, "case 3: frame_fault_o rose");
    cke(0x7733, t.fault_source_o, "case 3: fault_source_o names the draw that overran");
  }

  // -------------------------------------------------------------------
  // CASE 4 -- THE FAULTED FRAME STILL ACCEPTS.
  // A block that stopped accepting would stall GEOM.ASSEMBLE; a block that
  // kept writing would publish a frame with an arbitrary missing tail.
  // -------------------------------------------------------------------
  std::printf("\n=== case 4: the faulted frame still accepts\n");
  {
    const uint32_t disc_before = t.records_discarded_o;
    const uint32_t tw_before = t.tris_written_o;
    const uint32_t vw_before = t.verts_written_o;
    const uint32_t cw_before = t.chunks_written_o;
    t.eval();
    cke(1, t.pv_ready_o, "case 4: pv_ready_o stays HIGH after the fault");
    for (int i = 0; i < 3; ++i) {
      Descriptor d{0, 1, 2, 0xFFFF, 0xEEEE0000u, 0x0000BEEFu};
      ckt(push_td(t, d), "case 4: a descriptor after the fault is accepted");
    }
    Vertex v{9, 9, 9, 9, 9, 9, 9};
    ckt(push_pv(t, v), "case 4: a vertex after the fault is accepted");
    uint32_t ids[14] = {0};
    ckt(push_ck(t, 0xFFFFFFFFu, 1, ids), "case 4: a chunk after the fault is accepted");
    cke(disc_before + 5, t.records_discarded_o, "case 4: records_discarded_o counted all five");
    cke(tw_before, t.tris_written_o, "case 4: nothing more was written");
    cke(vw_before, t.verts_written_o, "case 4: nothing more was written");
    cke(cw_before, t.chunks_written_o, "case 4: nothing more was written");
  }

  // THE FALLBACK CONTRACT.  This is the most important assertion in the file:
  // a faulted frame drops `pub_pending_q` without touching a single `pub_*`
  // register, so the published description goes on naming the PRIOR COMPLETE
  // FRAME and the faulted bytes are simply never pointed at.
  end_frame(t);
  idle_cycles(t, 2000);
  t.eval();
  cke(frames_before, t.frames_published_o, "case 3: the faulted frame was NEVER published");
  cke(pub_gen_before, t.publish_gen_o, "case 3: publish_gen_o still names the PRIOR frame");
  cke(pub_view_before, t.publish_view_o, "case 3: publish_view_o still names the PRIOR frame");
  cke(pub_tris_before, t.publish_tris_o, "case 3: publish_tris_o still names the PRIOR frame");
  cke(pub_tri_base_before, t.publish_tri_base_o,
      "case 3: publish_tri_base_o still names the PRIOR frame");
  cke(1, t.publish_valid_o, "case 3: the prior frame is still valid");

  // -------------------------------------------------------------------
  // CASE 11, SECOND HALF -- A FAULTED FRAME DOES NOT LEAK THE SCRATCH.
  // `scr_mine_q` is raised when the directory write issues and must be
  // cleared on the FAULT path as well as the success one.  A release that
  // only happens on the success path is not a release, it is a leak with a
  // good day, and the symptom is the walker never being granted the scratch
  // again -- so the check is not "the bit is low" alone, it is THE WALKER
  // STILL GETS IT, end to end, through the guard.
  //
  // STATED PLAINLY: this frame faulted on QUOTA, which happens before the
  // directory write is ever issued, so `scr_mine_q` was never taken and the
  // release branch is not reached by this stimulus.  The pair below is
  // therefore a REGRESSION GUARD on the ownership protocol, not a positive
  // control for that branch -- with the lease repaired there is no legal way
  // to fault a frame AFTER it has taken the scratch, so that branch owes a
  // mutant or an explicit note, and this is the note.
  {
    t.eval();
    cke(0, t.pb_scratch_valid_o,
        "case 11: the scratch is released after a FAULTED frame too");
    cke(0, t.scr_grant_o, "case 11: nobody holds the scratch after a faulted frame");
    const uint32_t dirs_before = t.dirs_read_o;
    // And the walk STILL WORKS, against the prior frame's bytes.
    const WalkResult r = walk(t, 1);
    ckt(r.completed, "case 3: a walk after the faulted frame still completes");
    ckt(r.failed, "case 3: (frame D's chunk 1 is still the malformed one)");
    cke(dirs_before + 1, t.dirs_read_o,
        "case 11: the walker WAS GRANTED the scratch after the faulted frame");
    t.eval();
    cke(0, t.pb_scratch_valid_o, "case 11: and released it again afterwards");
  }

  // =======================================================================
  // FRAME F -- CASE 3b.  THE **CHUNK** ARM OF `quota_overflow_o`.
  //
  // WHY THIS EXISTS, added 2026-09-26 by GIANTQUOTA.  `quota_overflow_o` is
  // incremented at THREE independent sites in production -- the vertex arm
  // (`!pv_fits_c`), the descriptor arm (`!td_fits_c`) and the chunk arm
  // (`!ck_fits_c`).  Case 3 above fires the DESCRIPTOR arm and was the only
  // positive control this counter had.  A counter with three producers and one
  // exercised producer is two-thirds an assumption: the other two arms could
  // be mis-wired, transposed, or comparing the wrong cursor, and every test in
  // this tree would still pass while the counter read a plausible number.
  //
  // THE CHUNK ARM SPECIFICALLY, and this is not an arbitrary choice of the
  // three.  `ck_fits_c = (n_chunks_q < q_chunks_q)` is the EXACT expression a
  // giant's reservation has to modify -- R7 reserves 32,768 TILE REFERENCES,
  // which is ceil(32768/14) = 2,341 CHUNKS, and console entry I56 is open
  // against precisely that change.  Whoever makes it needs to know the arm
  // fired BEFORE they touched it, or a broken reservation and a broken counter
  // are indistinguishable afterwards.
  //
  // AND IT IS UNREACHABLE IN THE COMPOSED CONSOLE, which is why a directed
  // frame is the only place it can be shown.  `zhao_console_core` seals at the
  // arena's own capacity (`seal_chunks_i = 18'(GEOM_PA_MAX_CHUNKS)` = 16,384)
  // while the console smoke's frame allocates TEN chunks, so `ck_fits_c` can
  // only fail there on a physical overflow no stimulus produces.  Sealing a
  // small quota here is the legal stimulus that the composition cannot offer.
  // =======================================================================
  std::printf("\n=== case 3b: the CHUNK arm of quota_overflow_o\n");
  {
    const uint32_t quota_before_ck = t.quota_overflow_o;
    const uint32_t frames_before_ck = t.frames_published_o;
    const uint32_t cw_before_ck = t.chunks_written_o;
    const uint32_t overrun_before_ck = t.arena_overrun_o;

    // A quota of TWO CHUNKS, with vertices and descriptors left generous so
    // that the only bound this frame can reach is the chunk one.
    ckt(seal(t, 64, 64, 2, 0xF00D) >= 0, "F: the small-CHUNK-quota seal takes effect");
    cke(0, t.fault_source_o, "F: the seal cleared fault_source_o");

    uint32_t ids[14] = {0};
    for (int i = 0; i < 2; ++i) {
      ids[0] = 0x00000100u + static_cast<uint32_t>(i);
      ckt(push_ck(t, 0xFFFFFFFFu, 1, ids), "F: a chunk inside the quota is accepted");
    }
    cke(0, t.frame_fault_o, "case 3b: two chunks against a quota of two is NOT a fault");
    cke(quota_before_ck, t.quota_overflow_o, "case 3b: and the counter has not moved yet");

    // THE THIRD ONE OVERRUNS.  Like the descriptor arm, it is ACCEPTED and
    // discarded rather than refused -- a block that stopped accepting would
    // stall GEOM.CHUNKSER's serialise pass mid-tile.
    ids[0] = 0x000001FFu;
    ckt(push_ck(t, 0xFFFFFFFFu, 1, ids),
        "F: the overrunning chunk is still ACCEPTED (never refused)");
    cke(quota_before_ck + 1, t.quota_overflow_o,
        "case 3b: quota_overflow_o FIRED on the CHUNK arm");
    cke(1, t.frame_fault_o, "case 3b: frame_fault_o rose on the chunk overrun");
    cke(overrun_before_ck, t.arena_overrun_o,
        "case 3b: this was a QUOTA fault, NOT a view overrun -- the two are "
        "different questions and only one of them fired");

    // THE ASYMMETRY, ASSERTED RATHER THAN LEFT TO BE REDISCOVERED.  The
    // descriptor arm writes `fault_source_o <= td_source_i[15:0]` because a
    // TriangleDescriptor carries the draw that made it.  The chunk arm writes
    // NOTHING, because a chunk is a tile-major slice of a per-tile reference
    // list and has no owning draw to name -- it can hold references from two
    // instances at once.  So R7's "report the source IDs" is satisfiable for a
    // descriptor overrun and structurally is not for a chunk overrun.
    cke(0, t.fault_source_o,
        "case 3b: the CHUNK arm names NO source -- a chunk has no owning draw");

    end_frame(t);
    idle_cycles(t, 2000);
    t.eval();
    cke(cw_before_ck + 2, t.chunks_written_o,
        "case 3b: exactly the two chunks inside the quota were written");
    cke(frames_before_ck, t.frames_published_o,
        "case 3b: the chunk-faulted frame was NEVER published");
    cke(1, t.publish_valid_o, "case 3b: the prior complete frame still stands");
  }
  // =======================================================================
  // THE CLOSING SWEEP -- cases 9, 10, 11 across everything that ran.
  // =======================================================================
  std::printf("\n=== cases 9/10/11: the closing sweep\n");
  idle_cycles(t, 50);
  t.eval();
  cke(0, t.guard_violations_o,
      "case 9: guard_violations stayed ZERO across every legal case");
  cke(0, t.arena_guard_denied_o, "case 9: the arena never met a denial");
  cke(0, t.walk_guard_denied_o, "case 9: the walker never met a denial");
  cke(0, t.wr_elsewhere_o, "case 9: not one arena write left the authorised map");
  cke(0, t.arena_overrun_o, "case 9: no allocation reached past the view");
  cke(0, t.stray_beat_o, "case 10: stray_beat_o stayed zero");
  cke(0, t.short_burst_o, "case 10: short_burst_o stayed zero");
  cke(0, t.scr_overlap_o, "case 11: the scratch never had two owners at once");
  cke(0, t.pb_scratch_valid_o, "case 11: the scratch is released at rest");
  cke(0, t.model_error_o, "the DRAM model saw no protocol or timing violation");
  cke(0, t.wq_err_o, "the write-data queue never underflowed");
  cke(0, t.share_retire_unowned_o, "the share attributed every credit to an owner");
  cke(0, t.retire_underflow_o, "the arena was never credited words it did not owe");
  cke(0, t.addr_view_bad_o,
      "PRODUCTION: addr_view_bad_o is ZERO -- see geom_paramarena_drainmut for its "
      "positive control");
  cke(0, t.gen_race_o, "gen_race_o is zero -- the reader's busy blocks the seal");

  // -----------------------------------------------------------------------
  // ASSERTED AS OF 2026-09-23, AND IT WAS "REPORTED, NOT ASSERTED" BEFORE.
  // `zhao_sdram_ctrl` puts `req.addr[26:1]` straight into the COLUMN field of
  // a JEDEC BL8 burst.  A real SDR SDRAM's BL8 SEQUENTIAL burst wraps within
  // its eight-column block, so a burst whose start column is not a multiple of
  // eight returns / writes its words ROTATED.
  //
  // WHEN THIS WAS WRITTEN the sentence continued: "`zhao_sdram_model.sv` walks
  // the column LINEARLY, so THIS BENCH CANNOT FAIL ON THE CONSEQUENCE -- which
  // is exactly why the CAUSE is asserted instead."  THE MODEL NO LONGER WALKS
  // LINEARLY (owner ruling R243 / D-SDRAM-A, the same day): it holds col[10:3]
  // and advances col[2:0], so the consequence IS now reachable by a functional
  // test.  The cause is still asserted here, because a counter that names the
  // fault is a better diagnosis than a corrupted word that does not -- but it
  // is no longer the ONLY thing that could fail.
  //
  // WHAT CHANGED.  This block used to print a warning and assert nothing,
  // on the grounds that "spec/memory_rules.md states no alignment rule for the
  // local SDRAM client ports and the model cannot settle the question. The
  // ruling is the owner's."  The rule is now written down -- memory_rules.md
  // section 5c, THE BURST-ALIGNMENT LAW -- and the first of the three levers
  // that paragraph named ("round the sub-region bases up to a 16-byte
  // boundary") is what `zhao_geom_paramarena` now does.
  //
  // THE SECOND LEVER WAS THEN TAKEN BY THE OWNER (R243), against the note that
  // used to stand here -- "`zhao_vram_arbiter` was NOT touched: the second
  // lever moves a bound `mem_vram_arbiter_liveness` asserts is exact".  It
  // does not move it: the arbiter now clamps every burst to the aligned
  // eight-column block, and the bound was RE-PROVEN at 34/52 with both
  // `bmc_tight_*` tasks still failing at bound-1.  So this block's alignment
  // is now a PERFORMANCE property (one burst fewer per request), not a
  // correctness obligation.
  //
  // `req_unaligned_o` IS THE BENCH'S OWN OBSERVER, watching the guard request
  // at the wire, and is NOT `arena_burst_unaligned_o`.  Both are checked, and
  // that is deliberate: a block's own counter agreeing with itself is one
  // measurement, and a counter that had been wired to the wrong expression
  // would agree with itself perfectly.
  // SNAPSHOT, because case 12 below FIRES the walker's counter on purpose and
  // the bench's observer sees that request too. The banner and the summary
  // must report the HEALTHY run's number, not the control's -- quoting a
  // deliberately-broken tail as though it were the measurement is how an
  // alarm gets learned-past.
  const uint32_t unaligned_healthy = t.req_unaligned_o;
  ckt(unaligned_healthy == 0,
     "every guard request the HEALTHY part of this run issued started on a"
     " 16-byte boundary (the bench's own observer)");
  ckt(t.arena_burst_unaligned_o == 0,
     "the arena's own burst_unaligned_o agrees with the bench's observer");
  ckt(t.walk_burst_unaligned_o == 0,
     "the walker's burst_unaligned_o is silent -- its bases came in aligned");
  ckt(TRI_OFF_B % BURST_ALIGN_B == 0,
     "the derived triangle base is burst-aligned");
  ckt(CHUNK_OFF_B % BURST_ALIGN_B == 0,
     "the derived chunk base is burst-aligned");
  ckt(PV_SLOT_B % BURST_ALIGN_B == 0,
     "the vertex allocation stride is burst-aligned");

  // -----------------------------------------------------------------------
  // CASE 12 -- FIRE `walk_burst_unaligned_o`.
  // RUN LAST, AND AFTER the assertions above, deliberately: those require the
  // counter SILENT across the whole healthy run, and this one requires it to
  // MOVE.  A control that fired earlier would make every silence above
  // unreadable.
  //
  // WHY THIS COUNTER OWES A STIMULUS AND NOT A MUTANT.  The arena's
  // `burst_unaligned_o` is unreachable while its allocator is correct, so it
  // has a committed mutant.  The WALKER's is different in kind: it does not
  // compute its bases, it is TOLD them on `pub_*_base_i`.  Its job is to catch
  // a PRODUCER whose layout drifted, and that is a legal input, not a broken
  // block.  Saying "reachable, therefore no mutant is owed" and never firing
  // it would be the claim, not the evidence.
  //
  // THE STIMULUS IS TWO-PART, AND THE SECOND PART IS WHY.  Bumping the chunk
  // base the walker sees is caught ONE STATE EARLIER: `dir_agrees_c` compares
  // the directory that travelled through SDRAM against `pub_chunk_base_i`, so
  // a bumped base fails at W_DIR_CHECK with `dir_mismatch_o` and never reaches
  // a chunk read.  The directory is therefore poked to MATCH -- which is what
  // a producer with a drifted layout would actually have written -- and only
  // then is the base offered misaligned.
  {
    std::printf("\n=== case 12: walk_burst_unaligned_o FIRED\n");
    const uint32_t dirmiss_before = t.dir_mismatch_o;
    const uint32_t saved_cbase = peek32(t, SCRATCH_BASE + 8);
    ckt((saved_cbase % BURST_ALIGN_B) == 0,
        "case 12: the published chunk base was aligned before the bump");

    poke16(t, SCRATCH_BASE + 8, static_cast<uint16_t>((saved_cbase + 8u) & 0xFFFFu));
    poke16(t, SCRATCH_BASE + 10, static_cast<uint16_t>(((saved_cbase + 8u) >> 16) & 0xFFFFu));
    cke(saved_cbase + 8u, peek32(t, SCRATCH_BASE + 8),
        "case 12: the directory really carries the drifted chunk base");
    t.wcfg_chunk_base_bump_i = 8;
    t.eval();

    const WalkResult r = walk(t, 0);
    ckt(r.completed, "case 12: the walk completed");
    cke(dirmiss_before, t.dir_mismatch_o,
        "case 12: the directory still AGREES -- the fault under test is ALIGNMENT,"
        " not a round-trip mismatch");
    ckt(t.walk_burst_unaligned_o > 0,
        "case 12: walk_burst_unaligned_o FIRED on a producer whose chunk base"
        " is not burst-aligned");
    ckt(t.guard_violations_o == 0,
        "case 12: the guard refused nothing -- a misaligned address inside the"
        " view is LEGAL, which is exactly why only this counter can see it");
    std::printf("case 12: walk_burst_unaligned_o = %u (was 0)\n",
                t.walk_burst_unaligned_o);

    // Put it back, and prove the counter STOPS. A control that fires and then
    // keeps firing on healthy input is a stuck bit, not a detector.
    const uint32_t fired = t.walk_burst_unaligned_o;
    poke16(t, SCRATCH_BASE + 8, static_cast<uint16_t>(saved_cbase & 0xFFFFu));
    poke16(t, SCRATCH_BASE + 10, static_cast<uint16_t>((saved_cbase >> 16) & 0xFFFFu));
    t.wcfg_chunk_base_bump_i = 0;
    t.eval();
    cke(saved_cbase, peek32(t, SCRATCH_BASE + 8),
        "case 12: the directory is restored");
    const WalkResult r2 = walk(t, 0);
    ckt(r2.completed, "case 12: the restored frame walks again");
    cke(fired, t.walk_burst_unaligned_o,
        "case 12: the counter did NOT move again once the base was aligned");
  }
  if (unaligned_healthy != 0) {
    std::printf(
        "\n"
        "***************************************************************\n"
        "* %u of this run's guard requests start at an address that is\n"
        "* NOT 16-BYTE (8-WORD) ALIGNED.\n"
        "***************************************************************\n"
        "  derived sub-region bases were\n"
        "    TRI_OFF_B   = %u   (mod 16 = %u)\n"
        "    CHUNK_OFF_B = %u   (mod 16 = %u)\n"
        "  EVERY OTHER CHECK IN THIS FILE CAN STILL PASS, because the\n"
        "  behavioural model walks rd_col + rd_beat linearly and has no\n"
        "  eight-column wrap to get wrong.  That is the shape of a broken\n"
        "  instrument: the model reads BETTER than the silicon.\n"
        "***************************************************************\n\n",
        unaligned_healthy, TRI_OFF_B, TRI_OFF_B % 16u, CHUNK_OFF_B,
        CHUNK_OFF_B % 16u);
  }

  std::printf(
      "\n--- measured counters -------------------------------------------\n"
      "  frames_published_o   = %u\n"
      "  verts/tris/chunks    = %u / %u / %u\n"
      "  publish_blocked_o    = %u\n"
      "  view_flip_blocked_o  = %u\n"
      "  quota_overflow_o     = %u   records_discarded_o = %u\n"
      "  records_unsealed_o   = %u   arena_overrun_o     = %u\n"
      "  dirs_read_o          = %u   dir_mismatch_o      = %u\n"
      "  chunks_walked_o      = %u   chunks_stale_o      = %u\n"
      "  chunks_illegal_o     = %u   tris_emitted_o      = %u\n"
      "  walk_depth_max_o     = %u   walk_cut_o          = %u\n"
      "  guard_violations     = %u   addr_view_bad_o     = %u\n"
      "  writes v0/v1/scratch = %u / %u / %u\n"
      "  share ledger_full    = %u\n"
      "  req_unaligned_o      = %u  (the HEALTHY run; ASSERTED ZERO since 2026-09-23)\n"
      "  walk_unaligned_o     = %u  (NON-ZERO BY DESIGN: case 12 fires it LAST)\n"
      "-----------------------------------------------------------------\n",
      t.frames_published_o, t.verts_written_o, t.tris_written_o, t.chunks_written_o,
      t.publish_blocked_o, t.view_flip_blocked_o, t.quota_overflow_o, t.records_discarded_o,
      t.records_unsealed_o, t.arena_overrun_o, t.dirs_read_o, t.dir_mismatch_o,
      t.chunks_walked_o, t.chunks_stale_o, t.chunks_illegal_o, t.tris_emitted_o,
      static_cast<unsigned>(t.walk_depth_max_o), t.walk_cut_o, t.guard_violations_o,
      t.addr_view_bad_o, t.wr_in_view0_o, t.wr_in_view1_o, t.wr_in_scratch_o,
      t.share_ledger_full_o, unaligned_healthy, t.walk_burst_unaligned_o);

  std::printf("geom_paramarena_directed: %d/%d checks failed\n", g_fail, g_checks);
  zhao::exit_hard(g_fail == 0 ? 0 : 1);
}
