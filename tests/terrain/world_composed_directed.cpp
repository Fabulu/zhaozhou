// world_composed_directed.cpp -- STEP 8: the composed terrain world path.
//
// TERRAIN.SEQ, TERRAIN.RESIDENCY (v2), TERRAIN.PAGELOADER, TERRAIN.WRITEBACK
// and MEM.HPS.ARBITER in one Verilator top, driven by a frame ring and nothing
// else. Every block is the real RTL; the harness plays only HPS DDR, the page
// pool's fabric, the journal doorbell and the engine's ready.
//
// ---------------------------------------------------------------------------
// WHAT THIS TEST IS FOR, AND WHY IT IS EXPECTED TO FIND THINGS
// ---------------------------------------------------------------------------
// `reports/G1D-COMPOSED-ISLAND-20260905.md`: the FIRST composed test of
// individually-verified texture blocks found five real defects that no unit
// suite could see. Today's own evidence is the same shape -- composing
// TERRAIN.VISIBLE with TERRAIN.ISLAND found a dropped answer under
// backpressure that a 21-check differential over a full 15,625-patch sweep
// could not, because every phase held the consumer's ready high.
//
// So this file is written to look for the seams, not to confirm the blocks.
// Every block here already has a green unit suite. What none of them has is a
// neighbour.
//
// ---------------------------------------------------------------------------
// THE ORACLE IS FED THE DIRECTORY'S REAL ANSWERS
// ---------------------------------------------------------------------------
// `zref::terrain::seq::Sequencer` is a pure function of (record, directory
// answer, frame state). In TERRAIN.SEQ's own bench the answers are invented by
// the bench and handed to both sides. Here they come out of the REAL
// directory, are captured as they are given, and are then replayed into the
// reference. So the claim this file makes is exactly: *whatever
// `zhao_terrain_residency_v2` said, `zhao_terrain_seq` did with it what
// `zref::terrain::seq::Sequencer` says to do.*
//
// The directory's own answers are checked separately, against
// `zref::terrain::residency_set_index` -- every claim must land in the set the
// hash names -- so a directory fault cannot be laundered into a sequencing
// verdict.
//
// ---------------------------------------------------------------------------
// WHAT COMPOSING FOUND -- six things, none of them visible to a unit suite
// ---------------------------------------------------------------------------
//  1. CLOSED 2026-09-07. A page loaded into this world layer never became
//     GROUND: a claim sets `mips_stale`, so the loader's completion parked the
//     entry in ST_MIPGEN, and only a SECOND completion moves it to
//     RESIDENT_CLEAN -- the only state a lookup hits on. Nothing in the tree
//     could send it, and the machine as assembled drew no terrain at all.
//
//     THE CAUSE MOVED TWICE BEFORE IT WAS FIXED, which is the part worth
//     keeping. It first read "zhao_terrain_mipgen has no slot, generation,
//     epoch or completion port" -- true when written, false within hours,
//     because those ports landed the same day. It then read "nothing turns a
//     resident page slot into a lattice", which was true and was the real hole.
//     A defect description is a measurement with a date on it, and this one was
//     re-checked twice rather than re-quoted.
//
//     `zhao_terrain_pagestream` reads layers A, B and C out of the slot and
//     emits the 33x33 lattice; `zhao_terrain_mipfeed` runs it twice, one pass
//     per surface, and hands the samples to `zhao_terrain_mipgen`; MIPGEN's
//     `done` is the second completion. Phase C now measures eight pages loaded
//     and EIGHT RESIDENT with the harness playing nothing, and phase C2 takes
//     the chain out again and watches all eight go back to parked -- because a
//     fix whose absence was never re-measured is a claim, not evidence.
//
//     TWO THINGS THE CHAIN LEARNED FROM THE BENCH RATHER THAN FROM A DOCUMENT.
//     MIPGEN's `done_o` pulses while MIPFEED is still two states away from
//     looking at it, so the pulse has to be LATCHED -- the first version hung on
//     page one with 578 mip writes already done. And TERRAIN.RESIDENCY
//     validates the CRC on EVERY completion, not only the loader's, so a mip
//     completion carrying zero is a CRC FAILURE: 16 lattices, 17,424 samples,
//     4,624 mip writes, eight CRC failures, zero resident. Every block had done
//     its job and the page still was not ground.
//
//  2. ONE DIRECTORY MUTATION ON THE LOOKUP CYCLE DEADLOCKS THE FRAME.
//     TERRAIN.SEQ offers its lookup for one cycle with no `ready`;
//     TERRAIN.RESIDENCY drops a lookup that collides with any mutation and
//     never answers it. Neither block raises anything. (phase A)
//
//  3. THE OBVIOUS GLUE FOR (2) ALSO DEADLOCKS -- deferring the loader's
//     completion to the frame boundary starves the loader, which refuses new
//     jobs while holding an unconsumed `fin`, while TERRAIN.SEQ blocks on load
//     acceptance. Measured: two records into an eight-record frame. (bench
//     header, and why the shim holds the lookup instead)
//
//  4. THERE IS NO LOAD QUEUE. TERRAIN.SEQ's "a miss is SKIPPED, never waited
//     on" holds for load COMPLETION and is defeated by load ACCEPTANCE: the
//     loader takes one job and holds `j_ready_o` low for the whole 21,376-byte
//     transfer. Eight misses cost 53,806 clocks of a 1,666,667-clock frame with
//     the sequencer blocked. (phase B, measured)
//
//  5. T4's BARRIER DOES NOT HOLD IN BYTES. The writeback JOB is emitted before
//     the load JOB and that is all TERRAIN.SEQ guarantees; nothing waits for
//     the writeback to finish, so the displacing page overwrites the slot while
//     the sheet is still being read out of it. (phases G and G2)
//
//  6. AND THAT RACE LOSES THE SCAR. TERRAIN.WRITEBACK verifies the evicted
//     page's 64-byte header before journalling its sheet; move the fabric's
//     bandwidth ratio and the loader has already replaced that header, so the
//     writeback refuses -- correctly -- and the deformation is gone with no
//     upstream counter to notice. Same blocks, same commands, one ratio.
//     (phase G3)
//
//  Also found, in this file's own instruments rather than in the RTL: a
//  dropped-lookup tripwire that assumed a one-cycle answer and would have
//  called every healthy lookup a drop; a settle that watched handshakes rather
//  than work and returned with seven of eight pages still in flight; a played
//  engine whose pin mirror leaked silently and made the dirty eviction
//  unreachable while every check still passed; an F-sheet comparison read six
//  bytes out of lane (F_OFF is 10,694 and 10,694 mod 8 is 6) that reported
//  8,155 of 8,192 bytes corrupted on a perfect sheet; and a journal comparison
//  against an entry the writeback had never written. Every one of them read in
//  the direction that made the machine look worse, which is the only reason
//  they were caught.
//
// ---------------------------------------------------------------------------
// THE BARRIER IS CHECKED IN BYTES
// ---------------------------------------------------------------------------
// T4 says a dirty victim's F sheet is journalled before the page that
// displaces it is written. TERRAIN.SEQ's own suite proves it emits the
// writeback JOB before the load JOB and that is a real check -- both counters
// read 1 either way. It is not the barrier. The barrier is whether the sheet
// bytes reached the journal before the loader's bytes reached the slot, and
// only a composed machine can be asked that question.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vtb_terrain_world.h"

#include "zhao_sim.hpp"
#include "zref/zref_sw_stream.hpp"
#include "zref/zref_terrain.hpp"
#include "zref/zref_terrain_page.hpp"
#include "zref/zref_terrain_seq.hpp"

namespace sq = zref::terrain::seq;
namespace ss = zref::swstream;
namespace tp = zref::terrain;

namespace {

int g_checks = 0;
int g_fail = 0;

void ck(bool ok, const char* what, long long want = 1, long long got = 0) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL: %s (expected %lld, got %lld)\n", what, want, got);
    std::fflush(stdout);
  }
}

// A check that is EXPECTED to fail because it is reporting a defect this test
// found. It still counts, it still prints, and it is listed separately so the
// suite's own summary cannot quietly absorb a real finding.
int g_defects = 0;
void defect(bool held, const char* what, const char* evidence) {
  ++g_checks;
  if (!held) {
    ++g_defects;
    std::printf("DEFECT: %s\n        %s\n", what, evidence);
    std::fflush(stdout);
  }
}

// ---------------------------------------------------------------------------
// THE SHAPES, mirrored from the bench and from the written law
// ---------------------------------------------------------------------------
constexpr uint32_t kPageBytes = tp::kPageBytes;       // 21,376
constexpr uint32_t kPageWords = kPageBytes / 8;       // 2,672
constexpr uint32_t kPoolBase = 0x04000000u;
constexpr uint32_t kArenaBase = 0x20000000u;
constexpr uint32_t kStagePages = 64;
constexpr uint32_t kArenaBytes = kStagePages * kPageBytes;
constexpr uint32_t kJournalBase = 0x30000000u;
constexpr uint32_t kJournalEntries = 16;
constexpr uint32_t kFOff = 10694;
constexpr uint32_t kFBytes = 8192;
constexpr uint32_t kJournalBytes = kJournalEntries * kFBytes;
constexpr uint32_t kBenchComposeSlots = 16;

constexpr uint16_t kReq = ss::kFlagRequired;
constexpr uint16_t kPre = ss::kFlagPrefetch;
constexpr uint16_t kDyn = ss::kFlagDynamic;

// ---------------------------------------------------------------------------
// ONE OBSERVED ACTION
// ---------------------------------------------------------------------------
enum class K : uint8_t { kLookup, kClaim, kWriteback, kLoad, kPin, kIssue };

const char* kname(K k) {
  switch (k) {
    case K::kLookup: return "lookup";
    case K::kClaim: return "claim";
    case K::kWriteback: return "writeback";
    case K::kLoad: return "load";
    case K::kPin: return "pin";
    default: return "issue";
  }
}

struct Act {
  K k = K::kLookup;
  uint32_t island = 0;
  int32_t ix = 0, iz = 0;
  uint32_t slot = 0, gen = 0;
  uint32_t src_id = 0;
  uint32_t crc = 0;
  uint64_t addr = 0;
  uint32_t flags = 0, view = 0, prio = 0;
  uint32_t cslot = 0;
  bool cslot_valid = false;
  uint32_t cycle = 0;  // observation only; never compared

  bool operator==(const Act& o) const {
    return k == o.k && island == o.island && ix == o.ix && iz == o.iz && slot == o.slot &&
           gen == o.gen && src_id == o.src_id && crc == o.crc && addr == o.addr &&
           flags == o.flags && view == o.view && prio == o.prio && cslot == o.cslot &&
           cslot_valid == o.cslot_valid;
  }
};

std::string describe(const Act& a) {
  char b[256];
  std::snprintf(b, sizeof b,
                "%s isl=%u (%d,%d) slot=%u gen=%u src=%u crc=%08x addr=%llx "
                "flags=%03x view=%u prio=%u cs=%d/%u",
                kname(a.k), a.island, a.ix, a.iz, a.slot, a.gen, a.src_id, a.crc,
                static_cast<unsigned long long>(a.addr), a.flags, a.view, a.prio,
                static_cast<int>(a.cslot_valid), a.cslot);
  return std::string(b);
}

const char* kCounterNames[14] = {
    "records_consumed", "patches_issued",   "prefetch_resident", "skipped_not_resident",
    "claims_issued",    "claims_refused",   "claims_same",       "loads_issued",
    "loads_deferred",   "writebacks_issued","compose_slots_used","pins_issued",
    "drained",          "frame_faults"};

struct Frame {
  std::vector<Act> acts;
  std::vector<sq::ResAnswer> answers;  // one per record, as the DIRECTORY gave them
  std::vector<uint8_t> answered;       // 1 = lookup answered, 2 = claim answered too
  uint32_t c[14] = {0};
  bool fault = false;
  bool err_stray = false;
  bool done = false;
  uint32_t accepted = 0;
  uint64_t cycles = 0;
};

// ---------------------------------------------------------------------------
// STALL PATTERNS -- ALL DRAWN FROM THE LCG'S HIGH BITS
// ---------------------------------------------------------------------------
// A sibling lane's "randomised" phase over 240 windows turned out to be four
// distinct cases, because the draws came off the low bits of a linear
// congruential generator, whose low bits have periods of 2, 4, 8...
uint32_t lcg(uint32_t& s) {
  s = s * 1664525u + 1013904223u;
  return s;
}
uint32_t hi(uint32_t& s, uint32_t modulus) { return (lcg(s) >> 16) % modulus; }

// 0 always ready; 1 three-in-four (the MOSTLY-READY case, where a sibling
// block lost answers); 2 one-in-eight; 3 one-in-two.
bool ready_draw(uint32_t& s, int pattern) {
  switch (pattern) {
    case 0: return true;
    case 1: return hi(s, 4) != 0;
    case 2: return hi(s, 8) == 0;
    case 3: return hi(s, 2) == 0;
    default: return true;
  }
}
const char* kStallNames[4] = {"always-ready", "3-in-4", "1-in-8", "1-in-2"};

// ===========================================================================
// THE STAGED PAGE
// ===========================================================================
// Built by hand from spec/terrain_rules.md sec 2.1 and CRC'd with the one
// CRC-32C in the tree, so the loader's verdict is a verdict on our page rather
// than on a second implementation of the format.
struct Page {
  std::vector<uint8_t> b;
  uint32_t crc = 0;
};

void put16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); }
void put32(uint8_t* p, uint32_t v) { put16(p, uint16_t(v)); put16(p + 2, uint16_t(v >> 16)); }

Page make_page(uint32_t island, int16_t ix, int16_t iz, uint32_t salt, bool corrupt_crc = false,
               uint32_t ident_island_override = 0xFFFFFFFFu) {
  Page pg;
  pg.b.assign(kPageBytes, 0);
  uint8_t* p = pg.b.data();

  // body first: a deterministic fill, so two pages are never accidentally equal
  uint32_t s = 0x9E3779B9u ^ salt ^ (island * 2654435761u) ^ (uint32_t(uint16_t(ix)) << 13) ^
               (uint32_t(uint16_t(iz)) << 3);
  for (uint32_t i = 64; i < tp::kPageBodyEnd; ++i) {
    s = s * 1664525u + 1013904223u;
    p[i] = uint8_t(s >> 24);
  }

  put16(p + 0, 1);                       // format_version
  p[2] = 0;                              // pitch_log2
  p[3] = 0;                              // flags
  put32(p + 4, ident_island_override == 0xFFFFFFFFu ? island : ident_island_override);
  put16(p + 8, uint16_t(ix));
  put16(p + 10, uint16_t(iz));
  put32(p + 12, 7);                      // tileset_id
  pg.crc = tp::page_payload_crc(p);
  put32(p + 32, corrupt_crc ? (pg.crc ^ 0xDEADBEEFu) : pg.crc);
  return pg;
}

// ===========================================================================
// THE WORLD UNDER TEST
// ===========================================================================
struct World {
  Vtb_terrain_world& d;
  uint32_t epoch = 1;

  explicit World(Vtb_terrain_world& dut) : d(dut) {}

  void quiet() {
    d.mw_en = 0;
    d.mw_sel = 0;
    d.mr_sel = 0;
    d.mr_addr = 0;
    d.fr_start = 0;
    d.rec_valid = 0;
    d.is_ready = 1;
    d.wbdone_ready = 1;
    d.dm_valid = 0;
    d.wat_arm = 0;
    d.stat_clear = 0;
  }

  void config() {
    d.cfg_epoch_i = epoch;
    d.cfg_hps_arena_base_i = kArenaBase;
    d.cfg_hps_arena_bytes_i = kArenaBytes;
    d.cfg_journal_base_i = kJournalBase;
    d.cfg_journal_bytes_i = kJournalBytes;
    d.cfg_load_budget_i = 32;
    d.cfg_dir_gate_i = 1;
    // OFF by default: the harness's stand-in is no longer the machine as
    // assembled, because the machine can now do this itself. Phase C2 turns it
    // on with the real chain OFF, which is what keeps the old measurement
    // reproducible instead of merely remembered.
    d.cfg_mipgen_fin_i = 0;
    // ON by default: PAGESTREAM -> MIPFEED -> MIPGEN is real RTL that is really
    // instantiated, so the default is the machine as it now stands.
    d.cfg_mipfeed_i = 1;
    d.cfg_wb_barrier_i = 0;   // OFF by default: likewise
    // ON by default, and that asymmetry is deliberate. The two knobs above
    // stand in for machinery that DOES NOT EXIST in the tree, so their default
    // is off and the suite reports what is missing. `zhao_terrain_loadq` is a
    // real block that is really instantiated, so the default is the machine as
    // it now stands -- and phase L turns it off to show the old cost coming
    // back rather than asking anyone to take the improvement on trust.
    d.cfg_loadq_i = 1;
    d.cfg_loadq_drain_i = 0;
    d.cfg_wat_auto_i = 1;     // the witness arms itself on the victim
    d.cfg_unpin_delay_i = 6;
    d.cfg_ack_delay_i = 4;
    d.cfg_ack_ok_i = 1;
    d.cfg_req_latency_i = 2;
    d.cfg_beat_gap_i = 0;
    d.cfg_grant_hold_i = 0;
    d.cfg_wready_gap_i = 0;
    d.cfg_rd_latency_i = 2;
    d.cfg_rd_gap_i = 0;
  }

  void reset() {
    d.rst_n = 0;
    quiet();
    config();
    d.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(d);
    d.rst_n = 1;
    d.eval();
    // The directory sweeps 256 sets before it will answer anything. A frame
    // started before `ready_o` rises has every lookup dropped, which is the
    // same deadlock the header describes reached by a different road.
    for (int i = 0; i < 400 && !d.res_ready; ++i) zhao::tick(d);
    for (int i = 0; i < 4; ++i) zhao::tick(d);
  }

  void idle(int n) {
    d.rec_valid = 0;
    d.fr_start = 0;
    for (int i = 0; i < n; ++i) zhao::tick(d);
  }

  // ---- memory ------------------------------------------------------------
  void mem_write(int sel, uint32_t word, uint64_t data) {
    d.mw_en = 1;
    d.mw_sel = uint8_t(sel);
    d.mw_addr = word;
    d.mw_data = data;
    zhao::tick(d);
    d.mw_en = 0;
  }

  uint64_t mem_read(int sel, uint32_t word) {
    d.mr_sel = uint8_t(sel);
    d.mr_addr = word;
    zhao::tick(d);   // address captured
    zhao::tick(d);   // datum out
    return d.mr_data;
  }

  void stage_page(uint32_t stage_idx, const Page& pg) {
    const uint32_t base = stage_idx * kPageWords;
    for (uint32_t w = 0; w < kPageWords; ++w) {
      uint64_t v = 0;
      for (int b = 0; b < 8; ++b) v |= uint64_t(pg.b[w * 8 + b]) << (8 * b);
      mem_write(0, base + w, v);
    }
  }

  // Read `n` bytes out of pool slot `slot`, starting at page-relative `off`.
  // `off` and `n` are 8-byte aligned here by every caller.
  std::vector<uint8_t> pool_read(uint32_t slot, uint32_t off, uint32_t n) {
    std::vector<uint8_t> out(n);
    const uint32_t w0 = slot * kPageWords + off / 8;
    for (uint32_t i = 0; i < n / 8; ++i) {
      const uint64_t v = mem_read(1, w0 + i);
      for (int b = 0; b < 8; ++b) out[i * 8 + b] = uint8_t(v >> (8 * b));
    }
    return out;
  }

  // LAYER F IS NOT 8-BYTE ALIGNED IN THE PAGE. F_OFF is 10,694 and 10,694 mod 8
  // is 6, so the sheet starts six bytes into a 64-bit word. TERRAIN.WRITEBACK
  // reads the containing aligned chunks and re-lanes them, and the journal
  // therefore holds the sheet aligned. A comparison that read the pool from
  // word 10,694/8 would be comparing against bytes six positions out and would
  // report ~8,160 of 8,192 differing on a perfectly good sheet -- a broken
  // instrument in the loud direction, which this one was until it was checked.
  std::vector<uint8_t> sheet_of(uint32_t slot) {
    const uint32_t lane = kFOff % 8;
    const uint32_t aligned = kFOff - lane;
    const std::vector<uint8_t> win = pool_read(slot, aligned, kFBytes + 16);
    return std::vector<uint8_t>(win.begin() + lane, win.begin() + lane + kFBytes);
  }

  std::vector<uint8_t> journal_read(uint32_t entry, uint32_t n) {
    std::vector<uint8_t> out(n);
    const uint32_t w0 = entry * (kFBytes / 8);
    for (uint32_t i = 0; i < n / 8; ++i) {
      const uint64_t v = mem_read(2, w0 + i);
      for (int b = 0; b < 8; ++b) out[i * 8 + b] = uint8_t(v >> (8 * b));
    }
    return out;
  }
};

// ===========================================================================
// ONE RECORD AND ITS STAGING
// ===========================================================================
struct Rec {
  ss::PatchRecord r;
  uint32_t stage_idx = 0;
};

Rec mk(uint32_t src, uint32_t island, int16_t ix, int16_t iz, uint16_t flags,
       uint32_t stage_idx, uint32_t crc) {
  Rec c;
  c.r.island_id = island;
  c.r.patch_ix = ix;
  c.r.patch_iz = iz;
  c.r.hps_page_addr = kArenaBase + uint64_t(stage_idx) * kPageBytes;
  c.r.expected_page_crc32c = crc;
  c.r.flags = flags;
  c.r.view_mask = 3;
  c.r.priority = (flags & kReq) ? ss::kPriorityRequiredCurrent : ss::kPriorityRing;
  c.r.source_id = src;
  c.stage_idx = stage_idx;
  return c;
}

// ===========================================================================
// RUN ONE FRAME
// ===========================================================================
// Every consumer that is not another block is stalled on a pattern. The three
// that ARE other blocks -- the directory, the loader and the writeback -- back
// pressure themselves, which is the point: their ready is not a knob here, it
// is a real block's real answer.
Frame run_frame(World& w, const std::vector<Rec>& cs, uint16_t patch_count, uint16_t budget,
                int pattern, uint32_t seed, uint64_t cap = 400000ull,
                int collide_lookup = -1) {
  Vtb_terrain_world& d = w.d;
  Frame O;
  uint32_t s_rec = seed ^ 0xA5A5u, s_is = seed ^ 0xC0DEu, s_wd = seed ^ 0x77A1u;

  uint32_t before[14];
  d.eval();
  before[0] = d.s_records_consumed;   before[1] = d.s_patches_issued;
  before[2] = d.s_prefetch_resident;  before[3] = d.s_skipped_not_resident;
  before[4] = d.s_claims_issued;      before[5] = d.s_claims_refused;
  before[6] = d.s_claims_same;        before[7] = d.s_loads_issued;
  before[8] = d.s_loads_deferred;     before[9] = d.s_writebacks_issued;
  before[10] = d.s_compose_slots_used; before[11] = d.s_pins_issued;
  before[12] = d.s_drained;           before[13] = d.s_frame_faults;

  O.answers.assign(cs.size(), sq::ResAnswer{});
  O.answered.assign(cs.size(), 0u);

  d.cfg_load_budget_i = budget;
  d.fr_epoch = w.epoch;
  d.fr_patch_count = patch_count;
  d.fr_sequence = 0x0100u;
  d.fr_start = 1;
  d.rec_valid = 0;
  d.eval();
  zhao::tick(d);
  d.fr_start = 0;

  std::size_t next_rec = 0;
  std::size_t cur = 0;         // record whose answers are owed
  int collided = 0;

  for (uint64_t cyc = 0; cyc < cap; ++cyc) {
    const bool offer = next_rec < cs.size() && ready_draw(s_rec, pattern);
    d.rec_valid = offer ? 1 : 0;
    if (next_rec < cs.size()) {
      const ss::PatchRecord& r = cs[next_rec].r;
      d.rec_island = r.island_id;
      d.rec_ix = uint16_t(r.patch_ix);
      d.rec_iz = uint16_t(r.patch_iz);
      d.rec_hps_addr = r.hps_page_addr;
      d.rec_crc = r.expected_page_crc32c;
      d.rec_flags = r.flags;
      d.rec_view_mask = r.view_mask;
      d.rec_priority = r.priority;
      d.rec_src_id = r.source_id;
    }
    d.is_ready = ready_draw(s_is, pattern) ? 1 : 0;
    d.wbdone_ready = ready_draw(s_wd, pattern) ? 1 : 0;

    d.eval();

    // THE DELIBERATE COLLISION. One mutation presented on the exact cycle
    // TERRAIN.SEQ offers a lookup. See the bench header: this is the shape a
    // loader `fin` takes when it lands during a frame.
    if (collide_lookup >= 0 && d.lu_valid && collided < collide_lookup) {
      d.dm_valid = 1;
      d.dm_slot = 0;
      d.dm_gen = 0;
      d.dm_epoch = w.epoch;
      d.dm_bd = 1;
      d.dm_f = 0;
      d.dm_mips = 0;
      ++collided;
      d.eval();
    } else {
      d.dm_valid = 0;
      d.eval();
    }

    if (d.rec_valid && d.rec_ready) ++next_rec;

    if (d.lu_valid) {
      Act x; x.k = K::kLookup; x.island = d.lu_island;
      x.ix = int16_t(d.lu_ix); x.iz = int16_t(d.lu_iz); x.cycle = d.cyc;
      O.acts.push_back(x);
    }
    if (d.cl_valid && d.cl_ready) {
      Act x; x.k = K::kClaim; x.island = d.cl_island;
      x.ix = int16_t(d.cl_ix); x.iz = int16_t(d.cl_iz); x.crc = d.cl_expect_crc; x.cycle = d.cyc;
      O.acts.push_back(x);
    }
    if (d.wb_valid && d.wb_ready) {
      Act x; x.k = K::kWriteback; x.island = d.wb_island;
      x.ix = int16_t(d.wb_ix); x.iz = int16_t(d.wb_iz);
      x.slot = d.wb_slot; x.gen = d.wb_gen; x.src_id = d.wb_src_id; x.cycle = d.cyc;
      O.acts.push_back(x);
    }
    if (d.ld_valid && d.ld_ready) {
      Act x; x.k = K::kLoad; x.island = d.ld_island;
      x.ix = int16_t(d.ld_ix); x.iz = int16_t(d.ld_iz);
      x.slot = d.ld_slot; x.gen = d.ld_gen; x.src_id = d.ld_src_id;
      x.crc = d.ld_expect_crc; x.addr = d.ld_hps_addr; x.cycle = d.cyc;
      O.acts.push_back(x);
    }
    if (d.pin_valid && d.pin_ready) {
      Act x; x.k = K::kPin; x.slot = d.pin_slot; x.gen = d.pin_gen; x.cycle = d.cyc;
      O.acts.push_back(x);
    }
    if (d.is_valid && d.is_ready) {
      Act x; x.k = K::kIssue; x.island = d.is_island;
      x.ix = int16_t(d.is_ix); x.iz = int16_t(d.is_iz);
      x.slot = d.is_slot; x.gen = d.is_gen; x.src_id = d.is_src_id;
      x.flags = d.is_flags; x.view = d.is_view_mask; x.prio = d.is_priority;
      x.cslot = d.is_cslot; x.cslot_valid = d.is_cslot_valid != 0; x.cycle = d.cyc;
      O.acts.push_back(x);
    }

    // ---- the directory's answers, captured as it gives them ---------------
    if (next_rec > 0) cur = next_rec - 1;
    if (d.ra_lu_valid && cur < O.answers.size()) {
      sq::ResAnswer& a = O.answers[cur];
      a.hit = d.ra_lu_hit != 0;
      a.slot = uint16_t(d.ra_lu_slot);
      a.gen = uint8_t(d.ra_lu_gen);
      if (O.answered[cur] < 1u) O.answered[cur] = 1u;
    }
    if (d.ra_cl_valid && cur < O.answers.size()) {
      sq::ResAnswer& a = O.answers[cur];
      a.claim_same = d.ra_cl_same != 0;
      a.claim_refused = d.ra_cl_refused != 0;
      a.claim_slot = uint16_t(d.ra_cl_slot);
      a.claim_gen = uint8_t(d.ra_cl_gen);
      a.evicted_dirty = d.ra_cl_ev_dirty != 0;
      a.ev_island = d.ra_cl_ev_island;
      a.ev_ix = int16_t(d.ra_cl_ev_ix);
      a.ev_iz = int16_t(d.ra_cl_ev_iz);
      a.ev_gen = uint8_t(d.ra_cl_ev_gen);
      O.answered[cur] = 2u;
    }

    ++O.cycles;
    const bool fin = d.fr_done;
    zhao::tick(d);
    if (fin) { O.done = true; break; }
  }

  d.rec_valid = 0;
  d.dm_valid = 0;
  d.is_ready = 1;
  d.wbdone_ready = 1;
  d.eval();

  O.c[0] = d.s_records_consumed - before[0];
  O.c[1] = d.s_patches_issued - before[1];
  O.c[2] = d.s_prefetch_resident - before[2];
  O.c[3] = d.s_skipped_not_resident - before[3];
  O.c[4] = d.s_claims_issued - before[4];
  O.c[5] = d.s_claims_refused - before[5];
  O.c[6] = d.s_claims_same - before[6];
  O.c[7] = d.s_loads_issued - before[7];
  O.c[8] = d.s_loads_deferred - before[8];
  O.c[9] = d.s_writebacks_issued - before[9];
  O.c[10] = d.s_compose_slots_used - before[10];
  O.c[11] = d.s_pins_issued - before[11];
  O.c[12] = d.s_drained - before[12];
  O.c[13] = d.s_frame_faults - before[13];
  O.fault = d.seq_frame_fault != 0;
  O.err_stray = d.seq_err_stray_ans != 0;
  O.accepted = uint32_t(next_rec);
  return O;
}

// ===========================================================================
// THE ORACLE, over the answers the real directory gave
// ===========================================================================
Frame expect_of(const std::vector<Rec>& cs, const std::vector<sq::ResAnswer>& answers,
                uint16_t patch_count, uint16_t budget, uint32_t epoch) {
  Frame E;
  sq::Sequencer S(kBenchComposeSlots, budget);
  S.begin_frame(epoch);
  const std::size_t n = cs.size() < patch_count ? cs.size() : patch_count;
  for (std::size_t i = 0; i < n; ++i) {
    const ss::PatchRecord& r = cs[i].r;
    const sq::ResAnswer& a = answers[i];
    const sq::Step st = S.step(r, a);

    if (st.did_lookup) {
      Act x; x.k = K::kLookup; x.island = r.island_id; x.ix = r.patch_ix; x.iz = r.patch_iz;
      E.acts.push_back(x);
    }
    if (st.did_claim) {
      Act x; x.k = K::kClaim; x.island = r.island_id; x.ix = r.patch_ix; x.iz = r.patch_iz;
      x.crc = r.expected_page_crc32c;
      E.acts.push_back(x);
    }
    if (st.did_writeback) {
      Act x; x.k = K::kWriteback; x.island = a.ev_island; x.ix = a.ev_ix; x.iz = a.ev_iz;
      x.slot = a.claim_slot; x.gen = a.ev_gen; x.src_id = r.source_id;
      E.acts.push_back(x);
    }
    if (st.did_load) {
      Act x; x.k = K::kLoad; x.island = r.island_id; x.ix = r.patch_ix; x.iz = r.patch_iz;
      x.slot = a.claim_slot; x.gen = a.claim_gen; x.src_id = r.source_id;
      x.crc = r.expected_page_crc32c; x.addr = r.hps_page_addr;
      E.acts.push_back(x);
    }
    if (st.did_pin) {
      Act x; x.k = K::kPin; x.slot = a.slot; x.gen = a.gen;
      E.acts.push_back(x);
    }
    if (st.did_issue) {
      Act x; x.k = K::kIssue; x.island = r.island_id; x.ix = r.patch_ix; x.iz = r.patch_iz;
      x.slot = a.slot; x.gen = a.gen; x.src_id = r.source_id; x.flags = r.flags;
      x.view = r.view_mask; x.prio = r.priority;
      x.cslot = st.compose_slot_valid ? st.compose_slot : 0u;
      x.cslot_valid = st.compose_slot_valid;
      E.acts.push_back(x);
    }
  }
  const sq::Ledger& L = S.ledger();
  E.c[0] = L.records_consumed;      E.c[1] = L.patches_issued;
  E.c[2] = L.prefetch_resident;     E.c[3] = L.skipped_not_resident;
  E.c[4] = L.claims_issued;         E.c[5] = L.claims_refused;
  E.c[6] = L.claims_same;           E.c[7] = L.loads_issued;
  E.c[8] = L.loads_deferred;        E.c[9] = L.writebacks_issued;
  E.c[10] = L.compose_slots_used;   E.c[11] = L.pins_issued;
  E.c[12] = L.drained;              E.c[13] = L.frame_faults;
  E.fault = S.fault().active;
  return E;
}

int compare(const char* label, const Frame& O, const Frame& E) {
  int bad = 0;
  if (O.acts.size() != E.acts.size()) {
    ++bad;
    std::printf("    %s: %zu actions, oracle %zu\n", label, O.acts.size(), E.acts.size());
  }
  const std::size_t n = O.acts.size() < E.acts.size() ? O.acts.size() : E.acts.size();
  int shown = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (!(O.acts[i] == E.acts[i])) {
      ++bad;
      if (shown < 3) {
        std::printf("    %s[%zu]:\n      rtl    %s\n      oracle %s\n", label, i,
                    describe(O.acts[i]).c_str(), describe(E.acts[i]).c_str());
        ++shown;
      }
    }
  }
  for (int i = 0; i < 14; ++i) {
    if (O.c[i] != E.c[i]) {
      ++bad;
      std::printf("    %s: counter %s rtl=%u oracle=%u\n", label, kCounterNames[i], O.c[i],
                  E.c[i]);
    }
  }
  if (O.fault != E.fault) {
    ++bad;
    std::printf("    %s: frame_fault rtl=%d oracle=%d\n", label, int(O.fault), int(E.fault));
  }
  if (!O.done) { ++bad; std::printf("    %s: frame never completed\n", label); }
  // The acceptance invariant: only a witness OUTSIDE the block can see a
  // record taken off the ring and not counted.
  if (O.accepted != O.c[0]) {
    ++bad;
    std::printf("    %s: bench handed over %u records, block counted %u consumed\n", label,
                O.accepted, O.c[0]);
  }
  return bad;
}

// Actions with the cycle stripped, for a determinism comparison: the same
// frame twice must produce the same SEQUENCE, and cycle numbers advance.
bool same_log(const Frame& a, const Frame& b) {
  if (a.acts.size() != b.acts.size()) return false;
  for (std::size_t i = 0; i < a.acts.size(); ++i)
    if (!(a.acts[i] == b.acts[i])) return false;
  for (int i = 0; i < 14; ++i)
    if (a.c[i] != b.c[i]) return false;
  return true;
}

// Wait until the loader and the writeback are quiet and their completions have
// been consumed by the directory. The gate holds mutations until `fr_busy` is
// low, so this is where a frame's loads actually become residency.
void settle(World& w, uint64_t cycles = 600000) {
  Vtb_terrain_world& d = w.d;
  d.rec_valid = 0;
  d.fr_start = 0;
  d.is_ready = 1;
  d.wbdone_ready = 1;
  // "NOTHING IS ASSERTED" IS NOT "NOTHING IS HAPPENING", and the first version
  // of this function believed it was. TERRAIN.PAGELOADER mid-page raises none
  // of the handshake signals a neighbour can see -- it is talking to the bridge
  // and the guard -- so a settle that watched only the job and completion ports
  // returned after 64 cycles with seven of eight pages still in flight, and
  // every later phase then measured a machine that had not finished. What is
  // watched instead is WORK DONE: burst counts, write-beat counts and retired
  // pages. A machine that is doing something moves one of them.
  uint32_t quiet = 0;
  uint32_t prev[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  for (uint64_t i = 0; i < cycles; ++i) {
    zhao::tick(d);
    d.eval();
    // THE MIP CHAIN IS WORK TOO, and it is invisible in the six signals above:
    // TERRAIN.PAGESTREAM reads through its own played engine, so no arbiter
    // burst and no pool write moves while it runs. A settle that watched only
    // the original six returned with two lattice passes still in flight and
    // every later phase then measured a machine that had not finished -- the
    // same "nothing is asserted is not nothing is happening" failure this
    // function was rewritten for once already, reached through new blocks.
    const uint32_t now[8] = {d.arb_c0_bursts, d.arb_c1_bursts, d.h_pool_writes,
                             d.h_jnl_writes,  d.pl_pages_loaded, d.wb_sheets_written,
                             d.ps_bursts,     d.mf_samples};
    bool moved = false;
    for (int k = 0; k < 8; ++k) { if (now[k] != prev[k]) moved = true; prev[k] = now[k]; }
    // THE QUEUE COUNTS AS BUSY. Without `lq_level` here a settle could return
    // with jobs still queued and no block asserting anything -- the same
    // "nothing is asserted is not nothing is happening" failure this function
    // was rewritten for once already, reached through the new block.
    const bool busy = moved || d.ld_valid || d.pl_fin_valid || d.wb_valid || d.wbrel_valid ||
                      d.wbdone_valid || d.fr_busy || d.lq_inflight != 0;
    quiet = busy ? 0 : (quiet + 1);
    if (quiet > 400) return;
  }
  std::printf("    settle: gave up after %llu cycles still busy\n",
              (unsigned long long)cycles);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vtb_terrain_world* dut = new Vtb_terrain_world;
  World w(*dut);
  Vtb_terrain_world& d = *dut;

  std::printf("== STEP 8: composed terrain world path ==\n");
  std::printf("   real blocks: TERRAIN.SEQ + TERRAIN.RESIDENCY(v2) + TERRAIN.PAGELOADER\n");
  std::printf("                + TERRAIN.WRITEBACK + MEM.HPS.ARBITER + 2x MEM.GUARD observer\n");
  std::printf("   harness:     frame ring, HPS DDR, page-pool fabric, journal doorbell,\n");
  std::printf("                the engine's ready and its unpin\n\n");

  std::printf("   page pool at 0x%08x, %u B per page; HPS arena at 0x%08x; journal at 0x%08x\n\n",
              kPoolBase, kPageBytes, kArenaBase, kJournalBase);

  w.reset();
  ck(d.res_ready != 0, "the directory finished its 256-set init sweep");

  // =========================================================================
  // A -- THE LOOKUP THAT NOBODY ANSWERS
  // =========================================================================
  // TERRAIN.SEQ offers a lookup for one cycle and then waits with no timeout.
  // TERRAIN.RESIDENCY takes a lookup only when no mutation is present. One
  // mutation on that cycle and the frame never ends. Proven by putting exactly
  // one there.
  {
    std::printf("-- A: one mutation on the lookup cycle --\n");
    std::vector<Rec> cs;
    Page p0 = make_page(7, 0, 0, 1);
    w.stage_page(0, p0);
    cs.push_back(mk(100, 7, 0, 0, kReq, 0, p0.crc));
    cs.push_back(mk(101, 7, 1, 0, kReq, 0, p0.crc));

    d.cfg_dir_gate_i = 0;   // the glue removed, so the collision can happen
    d.stat_clear = 1; zhao::tick(d); d.stat_clear = 0;
    const Frame O = run_frame(w, cs, 2, 32, 0, 0xA0u, 8000ull, /*collide_lookup=*/1);
    d.cfg_dir_gate_i = 1;

    // ---- REPAIRED 2026-09-07, and this check is flipped to prove it --------
    // This case used to assert the DEFECT: S_LOOKUP advanced unconditionally
    // while `lu_valid_o` was one cycle wide, and the directory accepts a
    // lookup only while `ev_c == EV_NONE` -- so a lookup that lost the
    // arbitration was never answered, never counted, and the frame waited for
    // ever with no error bit anywhere.
    //
    // The directory's own comment had said a ready was unnecessary because
    // "both callers already tolerate that -- they are queries, not
    // transactions". TERRAIN.SEQ did not tolerate it. The repair gives the
    // query a `lu_ready_o` in the same form the mutation ports already use,
    // and SEQ now holds S_LOOKUP until the offer is taken.
    //
    // The assertion is INVERTED rather than deleted, so this case still fails
    // if the ready is ever removed again -- a defect case that is simply
    // dropped once fixed leaves nothing watching the repair.
    ck(O.done,
       "THE FRAME NOW COMPLETES with a directory mutation landing on the lookup "
       "cycle. This deadlocked before lu_ready_o existed, and the collision is "
       "still injected here -- the case was inverted, not removed",
       1, O.done ? 1 : 0);
    ck(d.h_lu_dropped == 0,
       "and NO lookup went unanswered: the offer is held until the directory "
       "takes it, rather than fired once into an arbitration it can lose",
       0, d.h_lu_dropped);
    ck(d.seq_err_stray_ans == 0,
       "with no stray answer either -- holding the offer must not produce a "
       "second answer for one query",
       0, d.seq_err_stray_ans);
    std::printf("   offers=%u dropped=%u records_consumed=%u fr_busy=%d\n",
                d.h_lu_offers, d.h_lu_dropped, O.c[0], int(d.fr_busy));
  }

  w.reset();

  // =========================================================================
  // The frame the rest of the file uses.
  // =========================================================================
  // Eight patches on one island: four DYNAMIC (they need a composed slot),
  // four static, one prefetch-only among them.
  std::vector<Rec> setA;
  std::vector<Page> pagesA;
  {
    for (int i = 0; i < 8; ++i) {
      const int16_t ix = int16_t(i);
      const int16_t iz = 4;
      Page pg = make_page(7, ix, iz, uint32_t(0x100 + i));
      pagesA.push_back(pg);
      w.stage_page(uint32_t(i), pg);
      uint16_t flags = kReq;
      if (i < 4) flags = uint16_t(flags | kDyn);
      if (i == 7) flags = kPre;   // one prefetch-only record
      setA.push_back(mk(uint32_t(200 + i), 7, ix, iz, flags, uint32_t(i), pg.crc));
    }
  }

  // =========================================================================
  // B -- THE COLD FRAME: nothing resident, nothing drawn, everything asked for
  // =========================================================================
  Frame coldB, warmD;
  {
    std::printf("\n-- B: the cold frame --\n");
    d.stat_clear = 1; zhao::tick(d); d.stat_clear = 0;
    const Frame O = run_frame(w, setA, 8, 32, 0, 0xB0u);
    const Frame E = expect_of(setA, O.answers, 8, 32, w.epoch);
    coldB = O;
    const int bad = compare("B cold", O, E);
    ck(bad == 0, "B the cold frame matches zref::terrain::seq::Sequencer action for action", 0,
       bad);
    ck(O.c[1] == 0, "B nothing is drawn on the frame that discovers the ground is missing", 0,
       O.c[1]);
    ck(O.c[3] == 7, "B seven REQUIRED misses are counted as skipped", 7, O.c[3]);
    ck(O.c[2] == 0, "B the prefetch record is not counted as resident", 0, O.c[2]);
    ck(O.c[4] == 8, "B eight claims", 8, O.c[4]);
    ck(O.c[7] == 8, "B eight loads", 8, O.c[7]);
    ck(O.c[9] == 0, "B no writebacks on a cold directory", 0, O.c[9]);
    ck(O.c[10] == 0, "B no composed slot is spent on a patch that never issues", 0, O.c[10]);
    ck(d.h_lu_dropped == 0, "B the gate kept every lookup answered", 0, d.h_lu_dropped);

    // THE COST OF A MISS, MEASURED. TERRAIN.SEQ's law is that a miss is
    // "SKIPPED and counted, not stalled on -- the frame must never wait on an
    // 80 microsecond page load mid-walk". It does not wait for the load to
    // COMPLETE. It used to wait for the load JOB to be ACCEPTED, because
    // TERRAIN.PAGELOADER takes one job at a time and holds `j_ready_o` low for
    // the whole page, and nothing sat between them. `zhao_terrain_loadq` now
    // does. Reported in cycles, against the 1,666,667-clock frame.
    std::printf("   B cost: %llu cycles for 8 records with 8 misses = %.0f cycles per miss\n",
                (unsigned long long)O.cycles, double(O.cycles) / 8.0);
    std::printf("      that is %.1f%% of a 1,666,667-clock frame for EIGHT pages; T7's\n"
                "      ceiling is 32 per frame.\n",
                100.0 * double(O.cycles) / 1666667.0);
    ck(O.cycles < 20000,
       "B the frame does not wait on load ACCEPTANCE -- eight misses cost well under the "
       "53,806 cycles the un-queued wiring charged (phase L turns the queue off and "
       "collects that number again)",
       1, O.cycles < 20000 ? 1 : 0);

    // THE QUEUE'S OWN ACCOUNT OF THE SAME FRAME. `s_loads_issued` is what the
    // sequencer believes it emitted; `lq_accepted` is what a block outside it
    // saw arrive. Those two disagreeing is the only way a job can be lost
    // between them, and neither block can notice it alone.
    ck(int(d.lq_accepted) == O.c[7],
       "B every load the sequencer counted is a load the queue took off it", O.c[7],
       int(d.lq_accepted));
    ck(d.lq_refused == 0,
       "B and nothing was offered to a full queue -- a non-zero refusal means the "
       "sequencer ignored j_ready_o, not that the queue was too small",
       0, d.lq_refused);
    std::printf("      queue: accepted=%u issued=%u high water=%u of 32\n",
                d.lq_accepted, d.lq_issued, d.lq_high_water);
    ck(d.lq_high_water > 1,
       "B and the queue actually HELD jobs -- a high water of one would mean the loader "
       "was never the bottleneck and this phase proved nothing about queueing",
       1, d.lq_high_water > 1 ? 1 : 0);

    // THE DIRECTORY'S OWN ANSWER, checked against the hash rather than trusted.
    int wrongset = 0, checked = 0;
    for (std::size_t i = 0; i < setA.size(); ++i) {
      // ONLY THE RECORDS THAT WERE ACTUALLY CLAIMED. A default-constructed
      // answer reads as set 0, and comparing those against the hash reports a
      // directory fault for every record the frame never reached -- the loud
      // direction of the broken-instrument failure, but still a broken
      // instrument.
      if (O.answered[i] < 2u) continue;
      ++checked;
      const uint8_t want = tp::residency_set_index(setA[i].r.island_id, setA[i].r.patch_ix,
                                                   setA[i].r.patch_iz, w.epoch);
      const uint32_t got = O.answers[i].claim_slot >> 2;   // slot = {set, way}
      if (got != want) {
        ++wrongset;
        if (wrongset <= 2)
          std::printf("    B claim %zu landed in set %u, hash says %u\n", i, got, want);
      }
    }
    ck(checked == 8, "B all eight claims were answered", 8, checked);
    ck(wrongset == 0, "B every claim landed in the set zref::terrain::residency_set_index names",
       0, wrongset);
  }

  // =========================================================================
  // C -- THE PAGES ACTUALLY ARRIVE
  // =========================================================================
  {
    std::printf("\n-- C: the loads complete and the bytes are the bytes --\n");
    settle(w);
    ck(d.pl_pages_loaded == 8, "C eight pages loaded", 8, d.pl_pages_loaded);
    ck(d.pl_pages_faulted == 0, "C none faulted", 0, d.pl_pages_faulted);
    ck(d.pl_crc_fails == 0, "C no CRC failures", 0, d.pl_crc_fails);
    ck(d.pl_hdr_ident_fails == 0, "C no header identity failures", 0, d.pl_hdr_ident_fails);
    ck(d.pl_guard_denied == 0, "C the guard denied nothing", 0, d.pl_guard_denied);
    ck(d.pl_bridge_errs == 0, "C no bridge errors", 0, d.pl_bridge_errs);
    ck(d.pl_load_bytes == 8u * kPageBytes, "C exactly eight pages of bytes moved",
       8ll * kPageBytes, d.pl_load_bytes);
    ck(d.h_pool_oob == 0, "C nothing was written outside the page pool", 0, d.h_pool_oob);
    ck(d.r_crc_failures == 0, "C the directory saw no CRC failure", 0, d.r_crc_failures);
    ck(d.r_stale_events == 0, "C no stale event reached the directory", 0, d.r_stale_events);

    // ---- THE HEADLINE. ------------------------------------------------
    // Eight pages were fetched, CRC-checked, written into their slots and
    // acknowledged to the directory, and the directory calls NONE of them
    // ground. A claim sets `mips_stale`; the loader's `fin` therefore moves the
    // entry RESERVED -> MIPGEN rather than RESERVED -> RESIDENT_CLEAN; and a
    // lookup hits ONLY on RESIDENT_CLEAN or RESIDENT_DIRTY_F. The second
    // completion that would close MIPGEN cannot be sent by anything in the
    // tree. UPDATED 2026-09-07: `zhao_terrain_mipgen.sv` GAINED
    // job_slot_i/job_gen_i/job_epoch_i and done_slot_o/done_gen_o/done_epoch_o,
    // so a completion can now be attributed -- but nothing drives it. MIPGEN
    // scans a lattice through fine_valid_i/fine_h_i and NOTHING IN fpga/rtl
    // turns a resident page slot into a lattice stream. That streamer is the
    // same one TERRAIN.PATCH needs, and it is a BLOCK, not a wire.
    // identity -- and `zhao_terrain_pageloader.sv` is the only `fin` producer
    // in `fpga/rtl/` and emits exactly one per page.
    ck(d.r_resident == 8,
       "C the composed world layer CALLS A LOADED PAGE RESIDENT -- the machine can turn "
       "eight fetched, CRC-verified pages into ground, which until 2026-09-07 it could not "
       "do at all",
       8, d.r_resident);

    // AND IT WAS THE BLOCKS THAT DID IT, NOT THE BENCH. `h_mipgen_fins` is the
    // harness's stand-in; it must be ZERO here, or "resident" above would be
    // measuring the knob rather than the machine.
    ck(d.h_mipgen_fins == 0,
       "C with no completion played by the harness at all", 0, d.h_mipgen_fins);
    ck(d.mf_pages_mipped == 8,
       "C TERRAIN.MIPFEED completed the mips for all eight pages", 8, d.mf_pages_mipped);
    ck(d.mf_pages_faulted == 0, "C and faulted none", 0, d.mf_pages_faulted);
    ck(d.h_mipreq_drops == 0,
       "C and the bench's mip-request glue lost none -- a drop here would reappear as a "
       "page that never becomes ground, which is the defect this chain removed",
       0, d.h_mipreq_drops);

    // THE SHAPE OF THE WORK, DERIVED. Two passes per page because MIPGEN takes
    // one 16-bit surface at a time and PAGESTREAM emits three planes at once;
    // 1,089 samples per pass; 17x17 and 9x9 selections per surface.
    ck(d.ps_lattices == 16,
       "C TERRAIN.PAGESTREAM streamed the page TWICE per page -- once for each surface",
       16, d.ps_lattices);
    ck(d.mf_samples == 8u * 2u * 1089u,
       "C and every one of the 17,424 fine samples reached MIPGEN",
       8 * 2 * 1089, int(d.mf_samples));
    ck(d.mg_m17_writes == 8u * 2u * 289u,
       "C MIPGEN selected 289 mip17 vertices per surface, ruling T8's 17x17",
       8 * 2 * 289, int(d.mg_m17_writes));
    ck(d.mg_m9_writes == 8u * 2u * 81u,
       "C and 81 mip9 vertices, T8's 9x9", 8 * 2 * 81, int(d.mg_m9_writes));
    ck(d.mg_aborts == 0,
       "C with no scan restarted mid-flight -- a `start` between the two passes would "
       "send surface 1's samples into surface 0's mip and count an abort here",
       0, d.mg_aborts);
    std::printf("   mip chain: %u lattices, %u bursts, %u samples, m17=%u m9=%u aborts=%u\n",
                d.ps_lattices, d.ps_bursts, d.mf_samples, d.mg_m17_writes, d.mg_m9_writes,
                d.mg_aborts);

    std::printf("   eight pages loaded, %u resident\n", d.r_resident);

    // THE REAL GUARD, in the composed setting. TERRAIN.PAGE_POOL is write-only
    // to TERRAIN.BUILD, so every loader write passes.
    ck(d.gobs_wr_ok > 0 && d.gobs_wr_viol == 0,
       "C the real MEM.GUARD passed every page-pool write and refused none", 0, d.gobs_wr_viol);
    std::printf("   guard observer: loader writes ok=%u viol=%u\n", d.gobs_wr_ok,
                d.gobs_wr_viol);

    // The bytes. This is the only end-to-end claim the machine can make today:
    // the page that was staged in HPS DDR is the page that is in the slot.
    int badpages = 0;
    for (std::size_t i = 0; i < setA.size(); ++i) {
      const uint32_t slot = coldB.answers[i].claim_slot;
      const std::vector<uint8_t> got = w.pool_read(slot, 0, kPageBytes);
      if (std::memcmp(got.data(), pagesA[i].b.data(), kPageBytes) != 0) {
        ++badpages;
        if (badpages <= 2) {
          std::size_t first = 0;
          while (first < kPageBytes && got[first] == pagesA[i].b[first]) ++first;
          std::printf("    C page %zu (slot %u) differs first at byte %zu: %02x vs %02x\n", i,
                      slot, first, got[first], pagesA[i].b[first]);
        }
      }
    }
    ck(badpages == 0, "C every loaded page is byte-identical to what was staged", 0, badpages);
  }

  // =========================================================================
  // C2 -- THE SAME FRAME WITH THE MIP CHAIN TAKEN OUT
  // =========================================================================
  // Phase C says the machine turns eight loaded pages into ground. On its own
  // that is not evidence that PAGESTREAM -> MIPFEED -> MIPGEN is what did it:
  // any of a dozen changes since the defect was first measured could have, and
  // a fix whose absence was never re-measured is a claim.
  //
  // So the identical cold frame is replayed twice more on a fresh directory --
  // once with the real chain OFF and the harness's stand-in OFF too, which
  // reproduces the original defect exactly, and once with the stand-in ON,
  // which is what the suite ran against for the day and a half the chain did
  // not exist. Three numbers, one measurement each.
  {
    std::printf("\n-- C2: the same frame without the mip chain --\n");

    // ---- neither: the defect, reproduced -------------------------------
    w.reset();
    d.cfg_mipfeed_i = 0;
    d.cfg_mipgen_fin_i = 0;
    d.eval();
    run_frame(w, setA, 8, 32, 0, 0xB2u);
    settle(w);
    const uint32_t resident_none = d.r_resident;
    ck(resident_none == 0,
       "C2 with the mip chain removed, NOT ONE of the eight loaded pages becomes ground -- "
       "the directory parks every entry in ST_MIPGEN waiting for a second completion "
       "nothing can send. This is the defect this suite carried from 2026-09-06, "
       "reproduced on demand rather than remembered",
       0, resident_none);
    ck(d.mf_pages_mipped == 0,
       "C2 and the chain really is out of the path -- it mipped nothing at all", 0,
       d.mf_pages_mipped);

    // ---- the harness's stand-in: what the suite ran against before -----
    // BOTH KNOBS SET AFTER THE RESET, not before. `World::reset` calls
    // `config()`, which puts `cfg_mipfeed_i` back to its default of 1 -- so a
    // version of this that cleared it first measured the REAL chain and
    // reported zero played completions beside eight resident pages, which is a
    // correct machine and a meaningless phase.
    w.reset();
    d.cfg_mipfeed_i = 0;
    d.cfg_mipgen_fin_i = 1;
    d.eval();
    run_frame(w, setA, 8, 32, 0, 0xB3u);
    settle(w);
    ck(d.r_resident >= 8, "C2 with the completion PLAYED, the pages become ground", 8,
       d.r_resident);
    ck(d.h_mipgen_fins >= 8, "C2 and one completion had to be played per page", 8,
       d.h_mipgen_fins);
    std::printf("   resident: %u with nothing, %u with the harness playing %u completions\n",
                resident_none, d.r_resident, d.h_mipgen_fins);

    // ---- and back to the machine as it now stands ----------------------
    // The rest of the file needs a directory that can call a page ground, and
    // from here it is the BLOCKS that do it.
    w.reset();
    d.cfg_mipgen_fin_i = 0;
    d.cfg_mipfeed_i = 1;
    d.eval();
    coldB = run_frame(w, setA, 8, 32, 0, 0xB1u);
    settle(w);
    ck(d.r_resident >= 8,
       "C2 and with the real chain back in, the machine does it itself", 8, d.r_resident);
  }

  // =========================================================================
  // D -- THE WARM FRAME: the same list, now drawn
  // =========================================================================
  {
    std::printf("\n-- D: the warm frame, and the ordering the whole layer exists for --\n");
    const Frame O = run_frame(w, setA, 8, 32, 0, 0xD0u);
    const Frame E = expect_of(setA, O.answers, 8, 32, w.epoch);
    warmD = O;
    const int bad = compare("D warm", O, E);
    ck(bad == 0, "D the warm frame matches the reference action for action", 0, bad);
    ck(O.c[1] == 7, "D seven required patches issue", 7, O.c[1]);
    ck(O.c[2] == 1, "D the prefetch record is counted resident and not drawn", 1, O.c[2]);
    ck(O.c[3] == 0, "D nothing is skipped once the ground has arrived", 0, O.c[3]);
    ck(O.c[10] == 4, "D exactly the four DYNAMIC patches take a composed slot", 4, O.c[10]);
    ck(O.c[11] == 7, "D one pin per issued patch", 7, O.c[11]);
    ck(O.c[7] == 0, "D a warm frame asks for no loads", 0, O.c[7]);
    ck(d.h_lu_dropped == 0, "D no lookup went unanswered", 0, d.h_lu_dropped);

    // THE ORDERING, not the counts: a page that was missing was LOADED before
    // its patch ISSUED. Frame B issued nothing at all, frame D issued it, and
    // the load's bytes landed in between. The composed cycle numbers say so.
    uint32_t last_load_cycle = 0, first_issue_cycle = 0xFFFFFFFFu;
    for (const Act& a : coldB.acts)
      if (a.k == K::kLoad && a.cycle > last_load_cycle) last_load_cycle = a.cycle;
    for (const Act& a : O.acts)
      if (a.k == K::kIssue && a.cycle < first_issue_cycle) first_issue_cycle = a.cycle;
    ck(last_load_cycle < first_issue_cycle,
       "D every load job preceded every patch issue of the page it fetched", 1,
       last_load_cycle < first_issue_cycle);
    std::printf("   last load job at cycle %u, first patch issue at cycle %u\n",
                last_load_cycle, first_issue_cycle);

    // The compose slots are frame-scoped: the n-th composing record of THIS
    // frame gets slot n, with no memory of the last frame.
    uint32_t expect_cs = 0;
    int csbad = 0;
    for (const Act& a : O.acts) {
      if (a.k != K::kIssue) continue;
      if (a.cslot_valid) {
        if (a.cslot != expect_cs) ++csbad;
        ++expect_cs;
      }
    }
    ck(csbad == 0, "D compose slot n is the n-th composing record of this frame", 0, csbad);
  }

  // =========================================================================
  // E -- DETERMINISM: the same frame twice
  // =========================================================================
  {
    std::printf("\n-- E: the same frame twice --\n");
    const Frame O2 = run_frame(w, setA, 8, 32, 0, 0xD0u);
    ck(same_log(warmD, O2), "E replaying the warm frame produces the identical action sequence");
    if (!same_log(warmD, O2)) {
      const std::size_t n = warmD.acts.size() < O2.acts.size() ? warmD.acts.size()
                                                               : O2.acts.size();
      for (std::size_t i = 0; i < n; ++i)
        if (!(warmD.acts[i] == O2.acts[i])) {
          std::printf("    E[%zu]:\n      run1 %s\n      run2 %s\n", i,
                      describe(warmD.acts[i]).c_str(), describe(O2.acts[i]).c_str());
          break;
        }
    }
  }

  // =========================================================================
  // F -- BACKPRESSURE: four stall patterns, one log
  // =========================================================================
  {
    std::printf("\n-- F: four stall patterns --\n");
    // The blocks' own readies are not knobs; what IS stalled is the record
    // producer, the engine's ready and the writeback completion sink, plus the
    // fabric's own timing. A block that dropped a job under backpressure would
    // produce a shorter, otherwise-correct log.
    const uint8_t lat[4] = {2, 16, 5, 9};
    const uint8_t gap[4] = {0, 1, 3, 0};
    for (int p = 0; p < 4; ++p) {
      d.cfg_req_latency_i = lat[p];
      d.cfg_beat_gap_i = gap[p];
      d.cfg_grant_hold_i = uint8_t(p);
      d.cfg_wready_gap_i = uint8_t(p % 3);
      const Frame O = run_frame(w, setA, 8, 32, p, 0xF0u + uint32_t(p));
      const Frame E = expect_of(setA, O.answers, 8, 32, w.epoch);
      char lab[64];
      std::snprintf(lab, sizeof lab, "F %s", kStallNames[p]);
      const int bad = compare(lab, O, E);
      ck(bad == 0, "F the warm frame matches the reference under this stall pattern", 0, bad);
      ck(same_log(warmD, O), "F the log is identical to the always-ready log");
      if (!same_log(warmD, O))
        std::printf("    %s: %zu actions vs %zu always-ready\n", lab, O.acts.size(),
                    warmD.acts.size());
      ck(d.h_lu_dropped == 0, "F no lookup went unanswered under stall", 0, d.h_lu_dropped);
      ck(d.seq_err_stray_ans == 0, "F no stray directory answer", 0, d.seq_err_stray_ans);
    }
    d.cfg_req_latency_i = 2;
    d.cfg_beat_gap_i = 0;
    d.cfg_grant_hold_i = 0;
    d.cfg_wready_gap_i = 0;
  }

  // =========================================================================
  // G -- T4's BARRIER, IN BYTES
  // =========================================================================
  // WHY THE SET HAS TO BE ALL-DIRTY, and it is not a convenience. T9's
  // replacement order takes any unpinned way that is not dirty BEFORE a dirty
  // one -- rule 3's test is `!s_f(...)`, which a freshly RESERVED way passes.
  // So in a set holding one dirty page and three fresh reservations the dirty
  // page is never the victim, and a dirty eviction cannot be reached at all.
  // It is reached here by filling one whole set with dirty pages, which is the
  // real traversal case: an area the players have fought over.
  {
    std::printf("\n-- G: a dirty victim, and what actually reached the journal --\n");
    settle(w);
    w.idle(200);

    // A set none of the resident patches is in, so the four ways start empty.
    uint8_t target_set = 0;
    {
      bool used[256];
      for (int i = 0; i < 256; ++i) used[i] = false;
      for (const Rec& r : setA)
        used[tp::residency_set_index(r.r.island_id, r.r.patch_ix, r.r.patch_iz, w.epoch)] = true;
      for (int s2 = 0; s2 < 256; ++s2)
        if (!used[s2]) { target_set = uint8_t(s2); break; }
    }

    // Five keys in it: four to fill the ways, one to displace a dirty way.
    std::vector<Rec> setG;
    std::vector<Page> pagesG;
    {
      uint32_t stage = 8;
      for (int32_t ix = 100; ix < 20000 && setG.size() < 5; ++ix) {
        if (tp::residency_set_index(7, int16_t(ix), 9, w.epoch) != target_set) continue;
        Page pg = make_page(7, int16_t(ix), 9, uint32_t(0x900 + setG.size()));
        w.stage_page(stage, pg);
        // PREFETCH-only, so they are claimed and loaded and never PINNED. A
        // pinned way is not evictable at all: rules 3, 4 and 5 all test the
        // pin count first.
        setG.push_back(mk(uint32_t(300 + setG.size()), 7, int16_t(ix), 9, kPre, stage, pg.crc));
        pagesG.push_back(pg);
        ++stage;
      }
    }
    ck(setG.size() == 5, "G five keys were found that hash into one set", 5, int(setG.size()));

    // Fill the set.
    std::vector<Rec> fill(setG.begin(), setG.begin() + 4);
    const Frame F0 = run_frame(w, fill, 4, 32, 0, 0x69u);
    settle(w);
    ck(F0.c[7] == 4, "G four pages were fetched into the four ways of one set", 4, F0.c[7]);

    // Mark every one of them dirty in layer F. SURFACE.STAMP's stand-in.
    const uint32_t stale_before = d.r_stale_events;
    int marks = 0;
    for (int i = 0; i < 4; ++i) {
      d.dm_valid = 1;
      d.dm_slot = uint16_t(F0.answers[i].claim_slot);
      d.dm_gen = uint8_t(F0.answers[i].claim_gen);
      d.dm_epoch = w.epoch;
      d.dm_bd = 0;
      d.dm_f = 1;
      d.dm_mips = 0;
      bool ok = false;
      for (int t = 0; t < 200 && !ok; ++t) {
        d.eval();
        if (d.dm_valid && d.dm_ready) ok = true;
        zhao::tick(d);
      }
      d.dm_valid = 0;
      d.eval();
      if (ok) ++marks;
    }
    ck(marks == 4, "G all four ways were marked dirty in layer F", 4, marks);
    ck(d.r_stale_events == stale_before,
       "G and none of the marks was rejected on identity", 0,
       d.r_stale_events - stale_before);

    // The sheets as they stand. One of these is what the journal must receive.
    std::vector<std::vector<uint8_t> > sheets;
    for (int i = 0; i < 4; ++i)
      sheets.push_back(w.sheet_of(F0.answers[i].claim_slot));

    d.stat_clear = 1; zhao::tick(d); d.stat_clear = 0;

    // The journal entry the next sheet will land in, read BEFORE the frame:
    // the ticket is minted by the job's acceptance and the address is
    // base + (ticket mod 16) * 8,192. Reading the wrong entry would compare
    // the sheet against untouched memory and call the barrier broken for a
    // reason that has nothing to do with the barrier.
    const uint32_t jnl_entry = d.h_wb_ticket % kJournalEntries;

    // The fifth key displaces one of them.
    std::vector<Rec> one(setG.begin() + 4, setG.end());
    const Frame O = run_frame(w, one, 1, 32, 0, 0x6Au);
    const Frame E = expect_of(one, O.answers, 1, 32, w.epoch);
    const int bad = compare("G evict", O, E);
    ck(bad == 0, "G the eviction frame matches the reference action for action", 0, bad);
    ck(d.r_dirty_evictions >= 1, "G the directory reported a dirty eviction", 1,
       d.r_dirty_evictions);
    ck(O.c[9] == 1, "G exactly one writeback job was issued", 1, O.c[9]);
    ck(O.c[7] == 1, "G and the load that displaces it was issued too", 1, O.c[7]);

    // The JOB order -- TERRAIN.SEQ's own law, and what its unit suite pins.
    int wb_i = -1, ld_i = -1;
    uint32_t victim_slot = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < O.acts.size(); ++i) {
      if (O.acts[i].k == K::kWriteback && wb_i < 0) {
        wb_i = int(i);
        victim_slot = O.acts[i].slot;
      }
      if (wb_i >= 0 && O.acts[i].k == K::kLoad && ld_i < 0) ld_i = int(i);
    }
    ck(wb_i >= 0 && ld_i > wb_i,
       "G the writeback JOB is emitted before the load JOB for the same slot", 1,
       (wb_i >= 0 && ld_i > wb_i) ? 1 : 0);

    // Which of the four sheets was it?
    int victim_i = -1;
    for (int i = 0; i < 4; ++i)
      if (F0.answers[i].claim_slot == victim_slot) victim_i = i;
    ck(victim_i >= 0, "G the evicted slot is one of the four that were filled", 1,
       victim_i >= 0 ? 1 : 0);

    // The witness armed itself on the writeback job's own slot, at the cycle
    // the job was accepted -- see `cfg_wat_auto_i`. Arming it from here would
    // start recording after the reads it is meant to time.
    settle(w);
    ck(d.h_wat_slot == victim_slot,
       "G the witness armed on the slot the writeback job named", victim_slot, d.h_wat_slot);

    std::printf("   watched slot %u: loader wrote %u beats (first at cycle %u), "
                "writeback read %u beats (last at cycle %u)\n",
                victim_slot, d.wat_wr_count, d.wat_wr_first, d.wat_rd_count, d.wat_rd_last);
    std::printf("   writeback: sheets written=%u refused=%u faulted=%u hdr ident fails=%u "
                "guard denied=%u acks ok=%u\n",
                d.wb_sheets_written, d.wb_sheets_refused, d.wb_sheets_faulted,
                d.wb_hdr_ident_fails, d.wb_guard_denied, d.wb_acks_ok);

    // ---- THE BARRIER, MEASURED --------------------------------------------
    const bool read_any = (d.wat_rd_count > 0);
    const bool wrote_any = (d.wat_wr_count > 0);
    defect(read_any && wrote_any && (d.wat_wr_first > d.wat_rd_last),
           "T4's writeback-before-load barrier does not hold in BYTES",
           "zhao_terrain_seq.sv:465-466 S_WB -> S_LOAD advances on wb_ready_i, which is "
           "TERRAIN.WRITEBACK ACCEPTING the job -- not completing it. Nothing gates the "
           "loader on the writeback's completion, so the displacing page starts overwriting "
           "the slot while the sheet is still being read out of it. Job order is correct and "
           "byte order is not.");

    // ---- what the journal actually got ------------------------------------
    if (victim_i >= 0) {
      const std::vector<uint8_t> got = w.journal_read(jnl_entry, kFBytes);
      std::size_t diff = 0;
      for (std::size_t i = 0; i < kFBytes; ++i)
        if (got[i] != sheets[victim_i][i]) ++diff;
      // AND THE HONEST RESULT: at THESE speeds the sheet survived anyway. The
      // loader writes the page from byte 0 upward and the sheet lives at
      // 10,688; with the fabric configured as it is here the writeback's read
      // cursor stays ahead of the loader's write cursor for the whole sheet,
      // so the journal gets the right bytes despite the overlap. That is what
      // makes this a LATENT defect rather than a visible one: the outcome is a
      // bandwidth ratio and nothing in the design fixes that ratio. G3 changes
      // it and asks again.
      ck(diff == 0,
         "G at THIS bandwidth ratio the sheet still reached the journal intact", 0, int(diff));
      ck(d.wb_sheets_written == 1, "G one sheet was written", 1, d.wb_sheets_written);
      std::printf("   journal vs the evicted sheet: %zu of %u bytes differ "
                  "-- intact here, and only here\n", diff, kFBytes);
    }

    ck(d.wb_acks_unmatched == 0, "G no acknowledgement went unmatched", 0, d.wb_acks_unmatched);
    ck(d.wb_seq_conflicts == 0, "G no journal ticket collided", 0, d.wb_seq_conflicts);

    // ---- and the slot the loader filled ------------------------------------
    // The victim entered EVICT_PENDING on the claim. The loader's `fin` arrives
    // while the entry is still EVICT_PENDING, and the FIN arm has no case for
    // that state: it does not fault, does not make resident, does not count.
    const Frame R = run_frame(w, one, 1, 32, 0, 0x6Bu);
    settle(w);
    const Frame R2 = run_frame(w, one, 1, 32, 0, 0x6Cu);
    settle(w);
    defect(R2.c[2] == 1,
           "a page loaded into a slot that was in EVICT_PENDING never becomes resident",
           "zhao_terrain_residency_v2.sv: the FIN arm advances only from RESERVED, LOADING "
           "or MIPGEN, so a fin arriving in EVICT_PENDING is not acted on and the slot is "
           "held by a key whose page will never be called ground. NO LONGER SILENT since "
           "2026-09-07: it passes the identity check, so it was not counted stale either -- "
           "a good completion landing where no arm can use it now reaches stale_events_o. "
           "COUNTED RATHER THAN ADVANCED on purpose: whether a load into an evicting slot "
           "should WIN races the writeback this directory itself ordered, and that is "
           "residency policy, not something to invent in the arm that drops things.");
    std::printf("   the displacing key, re-submitted twice: resident=%u then %u\n",
                R.c[2], R2.c[2]);

    // =====================================================================
    // G2 -- THE SAME EVICTION WITH THE BARRIER PUT BACK
    // =====================================================================
    // A check that can only ever fail is not evidence, it is a slogan. The
    // barrier check above is measured again here with the harness holding the
    // load job until TERRAIN.WRITEBACK reports the sheet done -- which is what
    // T4 actually asks for -- and it must now HOLD. Same set, same rules, one
    // wire different.
    {
      std::printf("\n-- G2: the same eviction with the barrier put back --\n");
      // A sixth key in the same set displaces another still-dirty way.
      std::vector<Rec> six;
      {
        uint32_t stage = 14;
        for (int32_t ix = 20000; ix < 60000 && six.empty(); ++ix) {
          if (tp::residency_set_index(7, int16_t(ix), 9, w.epoch) != target_set) continue;
          Page pg = make_page(7, int16_t(ix), 9, 0x9F0u);
          w.stage_page(stage, pg);
          six.push_back(mk(399, 7, int16_t(ix), 9, kPre, stage, pg.crc));
        }
      }
      ck(six.size() == 1, "G2 a sixth key in the same set was found", 1, int(six.size()));

      d.cfg_wb_barrier_i = 1;
      d.stat_clear = 1; zhao::tick(d); d.stat_clear = 0;
      const uint32_t jnl_entry2 = d.h_wb_ticket % kJournalEntries;
      const Frame O2 = run_frame(w, six, 1, 32, 0, 0x6Du);
      const Frame E2 = expect_of(six, O2.answers, 1, 32, w.epoch);
      const int bad2 = compare("G2 evict", O2, E2);
      ck(bad2 == 0, "G2 the frame still matches the reference with the barrier in place", 0,
         bad2);
      ck(O2.c[9] == 1, "G2 one writeback job", 1, O2.c[9]);

      uint32_t vslot2 = 0xFFFFFFFFu;
      for (const Act& a : O2.acts)
        if (a.k == K::kWriteback) vslot2 = a.slot;
      int vi2 = -1;
      for (int i = 0; i < 4; ++i)
        if (F0.answers[i].claim_slot == vslot2) vi2 = i;

      settle(w);
      ck(d.h_wat_slot == vslot2,
         "G2 the witness armed on the slot the writeback job named", vslot2, d.h_wat_slot);

      std::printf("   watched slot %u: loader wrote %u beats (first at cycle %u), "
                  "writeback read %u beats (last at cycle %u), barrier held the load "
                  "%u cycles\n",
                  vslot2, d.wat_wr_count, d.wat_wr_first, d.wat_rd_count, d.wat_rd_last,
                  d.h_barrier_stalls);
      // REPAIRED 2026-09-07. This asserted that the HARNESS barrier held the
      // load back, because TERRAIN.SEQ's own S_WB -> S_LOAD advanced on
      // `wb_ready_i` -- the writeback ACCEPTING the job, not completing it.
      // SEQ now waits in S_WAIT_WB for the completion of the slot it evicted,
      // so the harness barrier has nothing left to hold and reports zero.
      //
      // The byte-order check below is untouched and is the one that matters:
      // it still requires the loader's first write to follow the writeback's
      // last read, and it now passes because of the RTL rather than because of
      // the harness.
      ck(d.h_barrier_stalls == 0,
         "G2 the HARNESS barrier no longer has to hold anything -- TERRAIN.SEQ "
         "holds it in S_WAIT_WB now, so the knob that used to be load-bearing "
         "reports zero while the byte order below still holds",
         0, d.h_barrier_stalls);
      // AND THE BLOCK THAT TOOK THE JOB OVER SAYS SO ITSELF. `h_barrier_stalls`
      // going to zero is consistent with the barrier being enforced elsewhere
      // AND with it not being enforced at all; only TERRAIN.SEQ's own wait
      // counter separates those two. It was wired to a dead local until the
      // lint gate found it, which is exactly how long it proved nothing.
      ck(d.s_wb_wait_cycles > 0,
         "G2 and TERRAIN.SEQ spent real cycles in S_WAIT_WB -- the barrier moved into the "
         "block rather than evaporating",
         1, d.s_wb_wait_cycles > 0 ? 1 : 0);
      std::printf("   TERRAIN.SEQ waited %u cycles in S_WAIT_WB for the writeback it ordered\n",
                  d.s_wb_wait_cycles);
      ck(d.wat_rd_count > 0 && d.wat_wr_count > 0,
         "G2 both the writeback's reads and the loader's writes touched the slot", 1,
         (d.wat_rd_count > 0 && d.wat_wr_count > 0) ? 1 : 0);
      ck(d.wat_wr_first > d.wat_rd_last,
         "G2 WITH THE BARRIER, every writeback read of the slot precedes every loader write",
         1, (d.wat_wr_first > d.wat_rd_last) ? 1 : 0);

      if (vi2 >= 0) {
        const std::vector<uint8_t> got2 = w.journal_read(jnl_entry2, kFBytes);
        std::size_t diff2 = 0;
        for (std::size_t i = 0; i < kFBytes; ++i)
          if (got2[i] != sheets[vi2][i]) ++diff2;
        ck(diff2 == 0,
           "G2 and the journal receives the evicted sheet byte for byte", 0, int(diff2));
        std::printf("   journal vs the evicted sheet: %zu of %u bytes differ\n", diff2,
                    kFBytes);
      }
      d.cfg_wb_barrier_i = 0;
      settle(w);
    }

    // =====================================================================
    // G3 -- THE SAME RACE, WITH THE RATIO MOVED
    // =====================================================================
    // G showed the loader writing the victim slot while the writeback was
    // still reading it, and the sheet surviving anyway. That is a race won,
    // not a race avoided. Here the writeback's read path is slowed and the
    // loader's read path sped up -- both are knobs of the played fabric, not
    // of either block -- and the same eviction is run again with the barrier
    // still off. If the sheet is corrupted now, the ordering defect is not
    // theoretical.
    {
      std::printf("\n-- G3: the same race with the fabric's ratio moved --\n");
      std::vector<Rec> seven;
      {
        uint32_t stage = 15;
        for (int32_t ix = 60000; ix < 200000 && seven.empty(); ++ix) {
          if (tp::residency_set_index(7, int16_t(ix), 9, w.epoch) != target_set) continue;
          Page pg = make_page(7, int16_t(ix), 9, 0x9E0u);
          w.stage_page(stage, pg);
          seven.push_back(mk(398, 7, int16_t(ix), 9, kPre, stage, pg.crc));
        }
      }
      ck(seven.size() == 1, "G3 a seventh key in the same set was found", 1, int(seven.size()));

      d.cfg_wb_barrier_i = 0;
      d.cfg_rd_latency_i = 12;   // the writeback's page-pool reads: slow
      d.cfg_rd_gap_i = 6;
      d.cfg_req_latency_i = 1;   // the loader's HPS reads: as fast as the bridge allows
      d.cfg_beat_gap_i = 0;
      d.stat_clear = 1; zhao::tick(d); d.stat_clear = 0;
      const uint32_t jnl_entry3 = d.h_wb_ticket % kJournalEntries;
      // THE DUT'S COUNTERS ARE CUMULATIVE AND `stat_clear` ONLY ZEROES THE
      // HARNESS'S. G and G2 each journalled a sheet before this phase, so a
      // check written against the raw total would compare 2 with 0 and call a
      // correct machine broken -- the same trap TERRAIN.SEQ's own bench records
      // for its per-frame ledger.
      const uint32_t wr_before = d.wb_sheets_written;
      const uint32_t id_before = d.wb_hdr_ident_fails;
      const uint32_t ft_before = d.wb_sheets_faulted;

      const Frame O3 = run_frame(w, seven, 1, 32, 0, 0x6Eu);
      const Frame E3 = expect_of(seven, O3.answers, 1, 32, w.epoch);
      const int bad3 = compare("G3 evict", O3, E3);
      ck(bad3 == 0, "G3 the frame still matches the reference at this ratio", 0, bad3);

      uint32_t vslot3 = 0xFFFFFFFFu;
      for (const Act& a : O3.acts)
        if (a.k == K::kWriteback) vslot3 = a.slot;
      int vi3 = -1;
      for (int i = 0; i < 4; ++i)
        if (F0.answers[i].claim_slot == vslot3) vi3 = i;
      settle(w);

      std::printf("   watched slot %u: loader wrote %u beats (first at cycle %u), "
                  "writeback read %u beats (last at cycle %u)\n",
                  vslot3, d.wat_wr_count, d.wat_wr_first, d.wat_rd_count, d.wat_rd_last);

      // AND THE ANSWER IS SHARPER THAN CORRUPTION. TERRAIN.WRITEBACK reads the
      // evicted page's 64-byte HEADER FIRST and checks it against the identity
      // the job named -- the redundancy terrain_rules 2.1 calls a corruption
      // check, and the same check `zhao_mem_guard.sv:243` cites when it grants
      // the read window. At this ratio the loader has already replaced that
      // header with the DISPLACING page's identity by the time the writeback
      // reads it, so the writeback refuses the sheet -- correctly -- and the
      // scar is gone. The block is right; the composition is wrong.
      //
      // An earlier, harsher ratio stalled the writeback after fourteen beats
      // and left the journal entry untouched, and an unguarded byte comparison
      // reported 8,155 differences as "corruption". A comparison is evidence
      // only when both machines finished, so what is asserted here is the
      // machines' own verdict counters rather than a difference against
      // silence.
      std::printf("   writeback at this ratio: read %u beats, hdr ident fails=%u, "
                  "faulted=%u, written=%u\n",
                  d.wat_rd_count, d.wb_hdr_ident_fails - id_before,
                  d.wb_sheets_faulted - ft_before, d.wb_sheets_written - wr_before);
      ck(d.wat_wr_count == kPageWords, "G3 the loader wrote the whole displacing page",
         kPageWords, d.wat_wr_count);
      // REPAIRED 2026-09-07, AND THIS IS THE CASE THAT MATTERED MOST.
      //
      // It used to assert the scar was LOST: at this bandwidth ratio the
      // loader's write cursor reached the evicted page's header before
      // TERRAIN.WRITEBACK read it, the header no longer named the evicted
      // page, the sheet was refused, and the player's deformation was gone --
      // with nothing upstream waiting for the verdict, so the frame that lost
      // it reported nothing.
      //
      // TERRAIN.SEQ now holds S_WAIT_WB until the writeback COMPLETES for the
      // slot it evicted, so the loader cannot start writing that page while
      // its header is still being read. The ratio is still shifted here -- the
      // race is still ATTEMPTED -- and the sheet survives it.
      // 1,040 beats: TERRAIN.WRITEBACK's own measured figure -- the 64-byte
      // header burst plus the aligned superset of the 8,192-byte F sheet.
      // Before the barrier this read stopped at 8, the header alone, because
      // the identity check refused and there was nothing left to do.
      ck(d.wat_rd_count == 1040,
         "G3 the writeback read the header AND the whole sheet, at the very ratio "
         "that used to cut it off after the header burst",
         1040, d.wat_rd_count);
      ck(d.wb_hdr_ident_fails - id_before == 0,
         "G3 the header still named the evicted page when the writeback read it -- "
         "the loader had not reached it, because it had not started",
         0, d.wb_hdr_ident_fails - id_before);
      ck(d.wb_sheets_faulted - ft_before == 0,
         "G3 so no sheet faulted at a ratio that used to lose one every time",
         0, d.wb_sheets_faulted - ft_before);
      ck(d.wb_sheets_written - wr_before == 1,
         "G3 AND THE SCAR REACHED THE JOURNAL. This is the check the whole "
         "barrier exists for: same blocks, same commands, same hostile ratio, "
         "and the deformation survives",
         1, d.wb_sheets_written - wr_before);
      (void)vi3;
      (void)jnl_entry3;
      // put the fabric back
      d.cfg_rd_latency_i = 2;
      d.cfg_rd_gap_i = 0;
      d.cfg_req_latency_i = 2;
      d.cfg_beat_gap_i = 0;
      settle(w);
    }
  }

  // =========================================================================
  // H -- THE TRIPWIRES, EVERY ONE OF THEM READ
  // =========================================================================
  {
    std::printf("\n-- H: refusals and tripwires --\n");

    // ---- a page whose CRC does not match ---------------------------------
    {
      const uint32_t before_fail = d.pl_crc_fails;
      const uint32_t before_dirfail = d.r_crc_failures;
      Page bad = make_page(7, 40, 40, 0x4242u);
      const uint32_t honest_crc = bad.crc;
      // Corrupt one body byte AFTER the header's CRC word was written, so the
      // page's own header disagrees with its bytes.
      bad.b[5000] ^= 0xFFu;
      w.stage_page(20, bad);
      std::vector<Rec> cs;
      cs.push_back(mk(400, 7, 40, 40, kReq, 20, honest_crc));
      run_frame(w, cs, 1, 32, 0, 0x8Au);
      settle(w);
      ck(d.pl_crc_fails == before_fail + 1, "H a corrupted page is refused by CRC", 1,
         d.pl_crc_fails - before_fail);
      ck(d.r_crc_failures == before_dirfail + 1,
         "H and the directory records the failure rather than the page", 1,
         d.r_crc_failures - before_dirfail);
      const Frame again = run_frame(w, cs, 1, 32, 0, 0x8Bu);
      ck(again.c[1] == 0, "H a CRC-failed page is never drawn", 0, again.c[1]);
      settle(w);
    }

    // ---- a page whose header names another patch --------------------------
    {
      const uint32_t before_id = d.pl_hdr_ident_fails;
      Page wrong = make_page(7, 41, 41, 0x5151u, false, /*ident_island_override=*/9);
      w.stage_page(21, wrong);
      std::vector<Rec> cs;
      cs.push_back(mk(401, 7, 41, 41, kReq, 21, wrong.crc));
      run_frame(w, cs, 1, 32, 0, 0x8Cu);
      settle(w);
      ck(d.pl_hdr_ident_fails == before_id + 1,
         "H a page whose header names another island is refused", 1,
         d.pl_hdr_ident_fails - before_id);
    }

    // ---- T7: the load budget defers, and does NOT fault -------------------
    {
      std::vector<Rec> cs;
      for (int i = 0; i < 6; ++i) {
        Page pg = make_page(7, int16_t(60 + i), 60, uint32_t(0x600 + i));
        w.stage_page(uint32_t(22 + i), pg);
        cs.push_back(mk(uint32_t(500 + i), 7, int16_t(60 + i), 60, kReq, uint32_t(22 + i),
                        pg.crc));
      }
      const Frame O = run_frame(w, cs, 6, /*budget=*/2, 0, 0x8Du);
      const Frame E = expect_of(cs, O.answers, 6, 2, w.epoch);
      const int bad = compare("H budget", O, E);
      ck(bad == 0, "H the T7 load-budget frame matches the reference", 0, bad);
      ck(O.c[7] == 2, "H only the budgeted loads are issued", 2, O.c[7]);
      ck(O.c[8] == 4, "H the rest are DEFERRED", 4, O.c[8]);
      ck(O.c[13] == 0, "H and a deferred load is not a frame fault", 0, O.c[13]);
      settle(w);
    }

    // ---- T6: more required dynamic patches than composed slots ------------
    {
      // The bench runs COMPOSE_SLOTS = 16; the seventeenth composing patch of
      // one frame is the fault. Every one of them must be RESIDENT to reach
      // the allocator at all, so this reuses the eight that are and adds
      // enough new ones -- which means loading them first.
      // TWENTY DISTINCT ARENA PAGES. Reusing one staging slot for two records
      // would point the second record's job at the first record's bytes, the
      // loader would refuse it on header identity, and the phase would silently
      // stop testing T6 while every check still passed.
      std::vector<Rec> cs;
      for (int i = 0; i < 20; ++i) {
        Page pg = make_page(7, int16_t(80 + i), 80, uint32_t(0x800 + i));
        const uint32_t arena = uint32_t(32 + i);
        w.stage_page(arena, pg);
        cs.push_back(mk(uint32_t(600 + i), 7, int16_t(80 + i), 80, uint16_t(kReq | kDyn),
                        arena, pg.crc));
      }
      // Frame 1: everything misses, everything is claimed and loaded.
      run_frame(w, cs, 20, 64, 0, 0x8Eu);
      settle(w);
      // Frame 2: they are resident now, so all twenty want a composed slot.
      const Frame O = run_frame(w, cs, 20, 64, 0, 0x8Fu);
      const Frame E = expect_of(cs, O.answers, 20, 64, w.epoch);
      const int bad = compare("H T6", O, E);
      ck(bad == 0, "H the T6 overflow frame matches the reference", 0, bad);
      int resident = 0;
      for (const sq::ResAnswer& a : O.answers) if (a.hit) ++resident;
      ck(resident >= 17, "H at least seventeen of the twenty were resident, so T6 is reachable",
         17, resident);
      if (resident >= 17) {
        ck(O.c[13] == 1, "H the seventeenth composing patch faults the frame", 1, O.c[13]);
        ck(O.fault, "H and the fault is latched with the rejected identity");
        ck(O.c[12] > 0, "H the rest of the sealed list is DRAINED, not abandoned", 1, O.c[12]);
        ck(O.c[10] == kBenchComposeSlots, "H exactly COMPOSE_SLOTS slots were handed out",
           kBenchComposeSlots, O.c[10]);
      }
      settle(w);
    }

    // ---- the guard's verdict on the writeback's READ ----------------------
    // MEM.GUARD gives TERRAIN.BUILD a WRITE-ONLY window over the page pool and
    // TERRAIN.WRITEBACK reads it. The composed machine therefore has one client
    // the real guard refuses outright; the played guard admits it so the rest
    // of the chain is reachable, and this is where that substitution is
    // declared rather than hidden.
    std::printf("   guard observer, writeback reads: ok=%u violation=%u\n", d.gobs_rd_ok,
                d.gobs_rd_viol);
    // TERRAIN.WRITEBACK.md recorded that MEM.GUARD gave TERRAIN.BUILD a
    // WRITE-ONLY window over the page pool while this block READS it, and its
    // own bench's evidence for the requested amendment was that the real guard
    // refused every one of its reads. The amendment LANDED on 2026-09-07 --
    // `zhao_mem_guard.sv:161-186` now grants TERRAIN.BUILD the pool in both
    // directions -- and this suite watched the inversion happen between two of
    // its own runs an hour apart. So the check is the inverted one: the real
    // block must now PASS every read the writeback makes and refuse none.
    ck(d.gobs_rd_ok > 0,
       "H the real MEM.GUARD passes the writeback's page-pool READS (the read arm landed)", 1,
       d.gobs_rd_ok);
    ck(d.gobs_rd_viol == 0, "H ...and refuses none of them", 0, d.gobs_rd_viol);
    ck(d.wb_guard_denied == 0, "H and the writeback saw no denial of its own", 0,
       d.wb_guard_denied);

    // ---- MEM.HPS.ARBITER carried both terrain clients ---------------------
    std::printf("   arbiter: loader bursts=%u writeback bursts=%u writeback waited=%u cycles\n",
                d.arb_c0_bursts, d.arb_c1_bursts, d.arb_c1_wait_cycles);
    ck(d.arb_c0_bursts > 0 && d.arb_c1_bursts > 0,
       "H MEM.HPS.ARBITER carried bursts for BOTH terrain clients", 1,
       (d.arb_c0_bursts > 0 && d.arb_c1_bursts > 0) ? 1 : 0);
    ck(d.pl_bridge_errs == 0,
       "H and the loader saw no bridge error, so no request arrived while busy", 0,
       d.pl_bridge_errs);
    ck(d.wb_bridge_errs == 0, "H nor did the writeback", 0, d.wb_bridge_errs);
    ck(d.h_jnl_oob == 0, "H nothing was written outside the journal arena", 0, d.h_jnl_oob);

    // ---- every remaining tripwire, read ----------------------------------
    ck(d.seq_err_stray_ans == 0, "H TERRAIN.SEQ never saw a stray directory answer", 0,
       d.seq_err_stray_ans);
    // ZERO NOW, and it used to be exactly one -- G3's, produced on purpose by
    // racing the loader against the writeback's header read. TERRAIN.SEQ's
    // S_WAIT_WB removed the race, so the whole run faults no sheet at all.
    // Kept as an EXACT count rather than relaxed to "at least none": a
    // tripwire that cannot distinguish zero from three is not a tripwire.
    ck(d.wb_sheets_faulted == 0,
       "H NOT ONE sheet faulted mid-transfer across the whole run -- G3's "
       "deliberate race no longer produces one",
       0, d.wb_sheets_faulted);
    ck(d.wb_acks_after_epoch == 0, "H no acknowledgement arrived after its epoch", 0,
       d.wb_acks_after_epoch);
    ck(d.wb_acks_overdue == 0, "H no acknowledgement went overdue", 0, d.wb_acks_overdue);
    ck(d.wb_acks_nak == 0, "H the journal refused nothing", 0, d.wb_acks_nak);
    ck(d.wb_hdr_ident_fails == 0,
       "H and no header identity refusal either -- every evicted page still "
       "named itself when its sheet was read",
       0, d.wb_hdr_ident_fails);
    ck(d.pl_pages_refused == 0, "H no load was refused before it started", 0, d.pl_pages_refused);
    ck(d.pl_incomplete == 0, "H no load ended incomplete", 0, d.pl_incomplete);
    ck(d.h_pool_oob == 0, "H every write beat landed inside the page pool", 0, d.h_pool_oob);
    ck(d.r_refused_all_pinned == 0,
       "H no claim was refused for an all-pinned set (the engine's unpin kept up)", 0,
       d.r_refused_all_pinned);
    ck(d.h_pin_drops == 0,
       "H the played engine released every pin it was handed -- a leaked pin makes a page "
       "permanently unevictable and the suite would still pass", 0, d.h_pin_drops);
    ck(d.h_pins == d.h_unpins, "H pins and unpins balance", d.h_pins, d.h_unpins);
    std::printf("   directory: hits=%u misses=%u claims=%u evictions=%u dirty=%u resident=%u\n",
                d.r_hits, d.r_misses, d.r_claims, d.r_evictions, d.r_dirty_evictions,
                d.r_resident);
    std::printf("   harness:   pins=%u unpins=%u dropped=%u acks=%u pool writes=%u "
                "journal writes=%u mip completions played=%u\n",
                d.h_pins, d.h_unpins, d.h_pin_drops, d.h_acks_sent, d.h_pool_writes,
                d.h_jnl_writes, d.h_mipgen_fins);
  }

  // =========================================================================
  // I -- THE SEALED LIST IS A LENGTH, NOT A STREAM
  // =========================================================================
  // T5 seals a `patch_count` and TERRAIN.SEQ must stop there. A block that
  // held `rec_ready_o` high past its declared count would take records off the
  // ring and not count them: the producer considers them delivered, the block
  // considers them never to have arrived, and they are gone with every other
  // number still agreeing -- the block's own counter and its own action log
  // would agree with each other about a record neither had seen.
  //
  // ONLY A WITNESS OUTSIDE THE BLOCK CAN SEE IT, which is why `compare()`
  // checks the BENCH's tally of accepted handshakes against `records_consumed`.
  // That check existed before this phase did and was unreachable, because every
  // other phase declares exactly as many records as it offers -- a mutation of
  // `rec_ready_o` ran straight past it. A check no phase can reach is a check
  // that is not there.
  {
    std::printf("\n-- I: eight records offered, five declared --\n");
    settle(w);
    const Frame O = run_frame(w, setA, 5, 32, 0, 0x5Au);
    ck(O.done, "I the frame completed at its declared count");
    ck(O.accepted == 5, "I the bench was allowed to hand over exactly five records", 5,
       O.accepted);
    ck(O.c[0] == 5, "I and the block counted five consumed", 5, O.c[0]);
    const Frame E = expect_of(setA, O.answers, 5, 32, w.epoch);
    const int bad = compare("I short", O, E);
    ck(bad == 0, "I the short frame matches the reference action for action", 0, bad);
    settle(w);
  }


  // =========================================================================
  // L -- THE LOAD QUEUE, MEASURED IN BOTH DIRECTIONS
  // =========================================================================
  // Phase B showed the frame is now cheap. That on its own is not evidence
  // that the QUEUE made it cheap: any of a dozen changes since the 53,806-cycle
  // measurement could have done it, and a fix nobody re-measured the absence of
  // is a claim. So this phase runs the identical cold frame twice on a
  // freshly-reset world, once with `cfg_loadq_i` clear and once set, and puts
  // the two numbers next to each other.
  //
  // It also fires the DRAIN, which nothing in the tree asserts. A port that has
  // never been pulsed is a port nobody has tested, and the T6 fault path is
  // exactly where somebody will one day wire this up and assume it works.
  {
    std::printf("\n-- L: the load queue, with it and without it --\n");
    settle(w);

    uint64_t cyc_off = 0, cyc_on = 0;

    // ---- without ----------------------------------------------------------
    w.reset();                 // config() sets the knob back on...
    d.cfg_loadq_i = 0;         // ...so it is cleared AFTER the reset, not before
    d.eval();
    for (int i = 0; i < 8; ++i) w.stage_page(uint32_t(i), pagesA[i]);
    {
      d.stat_clear = 1; zhao::tick(d); d.stat_clear = 0;
      const Frame O = run_frame(w, setA, 8, 32, 0, 0xE0u);
      cyc_off = O.cycles;
      ck(O.c[7] == 8, "L the un-queued run still issues all eight loads", 8, O.c[7]);
      ck(d.lq_accepted == 0,
         "L and with the knob clear the queue is genuinely out of the path -- it saw "
         "nothing at all, so the number below is the old wiring and not a mixture",
         0, d.lq_accepted);
      settle(w);
    }

    // ---- with -------------------------------------------------------------
    w.reset();
    d.eval();
    for (int i = 0; i < 8; ++i) w.stage_page(uint32_t(i), pagesA[i]);
    {
      d.stat_clear = 1; zhao::tick(d); d.stat_clear = 0;
      const Frame O = run_frame(w, setA, 8, 32, 0, 0xE1u);
      cyc_on = O.cycles;
      ck(O.c[7] == 8, "L the queued run issues the same eight loads", 8, O.c[7]);
      ck(int(d.lq_accepted) == 8, "L and the queue took all eight off the sequencer", 8,
         int(d.lq_accepted));
      settle(w);
      ck(int(d.lq_issued) == 8,
         "L and handed all eight on to the loader -- a queue that accepts more than it "
         "issues is a queue that is eating jobs", 8, int(d.lq_issued));
      ck(d.lq_level == 0, "L and ended empty", 0, d.lq_level);
    }

    std::printf("   L: %llu cycles without the queue, %llu with it -- %.1fx, "
                "%.0f vs %.0f cycles per miss\n",
                (unsigned long long)cyc_off, (unsigned long long)cyc_on,
                cyc_on ? double(cyc_off) / double(cyc_on) : 0.0,
                double(cyc_off) / 8.0, double(cyc_on) / 8.0);

    ck(cyc_off > cyc_on * 4,
       "L the queue is what made the frame cheap: removing it puts the acceptance stall "
       "straight back, at more than four times the cost. This is the check that makes "
       "phase B's number mean something",
       1, (cyc_off > cyc_on * 4) ? 1 : 0);

    // ---- DEPTH AGAINST T7's ACTUAL CEILING -------------------------------
    // Eight misses is a quiet frame and the queue swallowed it whole (high
    // water 7 of 8). T7 permits THIRTY-TWO pages per frame, which is four
    // times the depth, so the interesting question is not whether the queue
    // helps on eight -- it plainly does, 803x above -- but what it does when
    // the frame is legal and full. The answer decides DEPTH, and it is
    // measured here rather than argued in a comment, because the first version
    // of that comment argued 8 was enough on reasoning that this measurement
    // does not support.
    {
      std::printf("\n-- L2: a legal FULL frame, 32 misses against a queue of 32 --\n");
      w.reset();
      d.eval();
      std::vector<Rec> set32;
      for (int i = 0; i < 32; ++i) {
        const int16_t ix = int16_t(i);
        Page pg = make_page(9, ix, 12, uint32_t(0x300 + i));
        w.stage_page(uint32_t(16 + i), pg);
        set32.push_back(mk(uint32_t(700 + i), 9, ix, 12, kReq, uint32_t(16 + i), pg.crc));
      }
      d.stat_clear = 1; zhao::tick(d); d.stat_clear = 0;
      const Frame O = run_frame(w, set32, 32, 32, 0, 0xE2u, 4000000ull);
      ck(O.c[7] == 32, "L2 all thirty-two misses issue a load", 32, O.c[7]);
      std::printf("   L2: %llu cycles for 32 misses = %.0f per miss; queue accepted=%u "
                  "high water=%u refused=%u\n",
                  (unsigned long long)O.cycles, double(O.cycles) / 32.0, d.lq_accepted,
                  d.lq_high_water, d.lq_refused);
      std::printf("   L2: that is %.1f%% of a 1,666,667-clock frame spent waiting on load "
                  "ACCEPTANCE alone\n", 100.0 * double(O.cycles) / 1666667.0);
      // THE DEPTH IS NOW ASSERTED, because the measurement has been taken and
      // it chose 32. At depth 8 this frame cost 176,768 cycles and the
      // sequencer sat on a full queue for 176,509 of them; at T7's own budget
      // the whole miss list fits and the walk finishes in hundreds of cycles.
      // A regression that quietly shrinks the queue would put 10.6% of the
      // frame back, and nothing else in the suite would notice.
      ck(d.lq_refused == 0,
         "L2 a LEGAL FULL frame never blocks the sequencer on a full queue -- T7 permits "
         "32 pages and the queue holds 32, so the walk never waits on acceptance",
         0, d.lq_refused);
      ck(O.cycles < 20000,
         "L2 and the full frame's walk costs the same order as the quiet one, not the "
         "176,768 cycles depth 8 charged",
         1, O.cycles < 20000 ? 1 : 0);
      ck(int(d.lq_accepted) == O.c[7],
         "L2 the queue took exactly the loads the sequencer issued, no more and no fewer",
         O.c[7], int(d.lq_accepted));
      settle(w, 4000000ull);
      ck(int(d.lq_issued) == int(d.lq_accepted),
         "L2 and issued every one of them onward -- depth may be too small, but nothing "
         "is lost when it is",
         int(d.lq_accepted), int(d.lq_issued));
      w.reset();
      for (int i = 0; i < 8; ++i) w.stage_page(uint32_t(i), pagesA[i]);
    }

    // ---- the drain, fired ------------------------------------------------
    // Filled deliberately and then thrown away. The fill is driven by the
    // sequencer's own frame rather than by poking the port, so what gets
    // drained is a real job list and not a bench fiction.
    {
      w.reset();
      d.eval();
      for (int i = 0; i < 8; ++i) w.stage_page(uint32_t(i), pagesA[i]);
      d.stat_clear = 1; zhao::tick(d); d.stat_clear = 0;

      // Start a frame and stop the moment the queue is holding something.
      d.cfg_load_budget_i = 32;
      d.fr_epoch = w.epoch;
      d.fr_patch_count = 8;
      d.fr_sequence = 0x0700u;
      d.fr_start = 1; d.eval(); zhao::tick(d); d.fr_start = 0;

      std::size_t next_rec = 0;
      int held = 0;
      for (int i = 0; i < 40000 && held < 4; ++i) {
        d.rec_valid = (next_rec < setA.size()) ? 1 : 0;
        if (next_rec < setA.size()) {
          const ss::PatchRecord& r = setA[next_rec].r;
          d.rec_island = r.island_id; d.rec_ix = uint16_t(r.patch_ix);
          d.rec_iz = uint16_t(r.patch_iz); d.rec_hps_addr = r.hps_page_addr;
          d.rec_crc = r.expected_page_crc32c; d.rec_flags = r.flags;
          d.rec_view_mask = r.view_mask; d.rec_priority = r.priority;
          d.rec_src_id = r.source_id;
        }
        d.is_ready = 1; d.wbdone_ready = 1;
        d.eval();
        if (d.rec_valid && d.rec_ready) ++next_rec;
        held = int(d.lq_inflight);
        zhao::tick(d);
      }
      d.rec_valid = 0;
      d.eval();

      // INFLIGHT, NOT LEVEL. The drain throws away the job in the write
      // serialiser and the one in the output register as well as the store's,
      // and the first version of this check compared against `lq_level` alone
      // and was off by exactly those two.
      const uint32_t level_before = d.lq_inflight;
      const uint32_t drained_before = d.lq_drained;
      ck(level_before >= 2,
         "L the drain is fired against a queue that is actually holding jobs -- draining "
         "an empty queue would prove the port compiles, nothing more",
         1, level_before >= 2 ? 1 : 0);

      d.cfg_loadq_drain_i = 1;
      d.eval();
      zhao::tick(d);
      d.cfg_loadq_drain_i = 0;
      d.eval();

      ck(d.lq_inflight == 0, "L one drain pulse empties the queue -- store, serialiser "
         "and output register alike", 0, d.lq_inflight);
      ck(d.lq_drained == drained_before + level_before,
         "L and COUNTS what it threw away, exactly the level it held. A drain that "
         "silently empties cannot be told from a queue that was never filled",
         int(drained_before + level_before), int(d.lq_drained));
      std::printf("   L: drained %u queued jobs, counter now %u\n", level_before, d.lq_drained);

      // NO SETTLE HERE, deliberately. The frame above was abandoned mid-walk to
      // get jobs into the queue, so `fr_busy` never falls and `settle` would
      // spin its whole 600,000-cycle budget and then report giving up -- a
      // frightening line in the log that would mean nothing. The reset is the
      // correct way out of a frame nobody intends to finish.
      w.reset();
      for (int i = 0; i < 8; ++i) w.stage_page(uint32_t(i), pagesA[i]);
    }
  }

  std::printf("\n== %d checks, %d failures, %d composition defects ==\n", g_checks, g_fail,
              g_defects);
  if (g_defects > 0)
    std::printf("   The defects above are the deliverable: they are seam faults no unit suite\n"
                "   could see, and every one of them is reproduced by this test on demand.\n");
  std::fflush(stdout);

  const int rc = (g_fail == 0) ? 0 : 1;
  delete dut;
  return rc;
}
